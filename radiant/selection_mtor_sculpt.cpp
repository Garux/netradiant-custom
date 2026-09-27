/*
   This file is part of NetRadiant.

   NetRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   NetRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with NetRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "selection_mtor_sculpt.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "brush.h"
#include "grid.h"
#include "mainframe.h"
#include "camwindow.h"
#include "xywindow.h"
#include "map.h"
#include <QWidget>
#include <QWheelEvent>
#include "stream/stringstream.h"

#include <array>
#include <deque>
#include <limits>
#include <map>
#include <optional>


namespace
{

std::optional<DoubleVector3> sculpt_testSelect_point( const View& view ){
	if( !view.fill() && vector3_max_abs_component_index( view.getViewDir() ) != 2 )
		return {};

	SelectionVolume test( view );
	ScenePointSelector selector;
	Scene_forEachVisible_testselect_scene_point_selected_brushes( view, selector, test );
	if( !selector.isSelected() )
		return {};
	test.BeginMesh( g_matrix4_identity, true );
	return DoubleVector3( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, selector.best().depth(), 1 ) ) ) );
}

inline double sculpt_falloff( double t ){
	return 1 - t * t * ( 3 - 2 * t );
}

// rounds away from zero, so any vertex inside the radius moves at least one grid step
inline double sculpt_steps( double offset, double snap ){
	const double steps = offset / snap;
	return steps > 0? std::ceil( steps - 1e-6 ) : std::floor( steps + 1e-6 );
}

constexpr double c_sculpt_spacing = 0.1;

constexpr float c_sculpt_radius_min = 8;
constexpr float c_sculpt_radius_max = 4096;

inline double distance_xy( const DoubleVector3& a, const DoubleVector3& b ){
	return std::hypot( a.x() - b.x(), a.y() - b.y() );
}

struct SculptColumn
{
	DoubleVector3 top;
	double bottom;
	std::size_t count;
};

void brush_gather_columns( const BrushInstance& brush, std::vector<SculptColumn>& columns ){
	std::vector<DoubleVector3> vertices;
	brush.gather_vertices( vertices );
	columns.clear();
	for( const auto& v : vertices ){
		auto column = std::ranges::find_if( columns, [&v]( const SculptColumn& c ){ return distance_xy( c.top, v ) < 0.05; } );
		if( column == columns.end() ){
			columns.push_back( SculptColumn{ v, v.z(), 1 } );
		}
		else{
			if( v.z() > column->top.z() )
				column->top = v;
			column->bottom = std::min( column->bottom, v.z() );
			++column->count;
		}
	}
	std::erase_if( columns, []( const SculptColumn& c ){ return c.count < 2; } );
}

class SculptStroke
{
public:
	struct Vertex
	{
		double zOrig;
		double zCurrent;
		double zTarget;
		double zMin;
		double offset;
		std::size_t dab;
	};
private:
	using Key = std::pair<long long, long long>;
	std::map<Key, std::deque<Vertex>> m_vertices;
	static Key key( const DoubleVector3& v ){
		return { std::llround( v.x() * 16 ), std::llround( v.y() * 16 ) };
	}
public:
	Vertex* find( const DoubleVector3& v ){
		if( auto it = m_vertices.find( key( v ) ); it != m_vertices.end() )
			for( auto& sv : it->second )
				if( std::fabs( sv.zCurrent - v.z() ) < 0.1 )
					return &sv;
		return nullptr;
	}
	Vertex& insert( const DoubleVector3& v ){
		if( Vertex* sv = find( v ) )
			return *sv;
		return m_vertices[key( v )].emplace_back( Vertex{ v.z(), v.z(), v.z(), std::numeric_limits<double>::lowest(), 0, 0 } );
	}
	void clear(){
		m_vertices.clear();
	}
};


class SculptManipulatorImpl final : public SculptManipulator, public Manipulatable
{
	bool m_isSelected = false;
	bool m_lower = false;
	std::size_t m_dab = 0;
	DoubleVector3 m_lastDab;
	SculptStroke m_stroke;

	bool m_visible = false;
	bool m_lowerHighlight = false;
	DoubleVector3 m_center;
	float m_renderedRadius = 0;

	struct RenderableCircle : public OpenGLRenderable
	{
		std::array<PointVertex, 64> m_vertices;
		void render( RenderStateFlags state ) const override {
			gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_vertices.data()->colour );
			gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_vertices.data()->vertex );
			gl().glDrawArrays( GL_LINE_LOOP, 0, GLsizei( m_vertices.size() ) );
		}
	};
	RenderableCircle m_circle;
	RenderablePoint m_point;

	void updateRenderables(){
		const Colour4b colour = m_lowerHighlight? Colour4b( 255, 96, 64, 255 ) : g_colour_screen;
		for( std::size_t i = 0; i < m_circle.m_vertices.size(); ++i ){
			const double angle = c_2pi * i / m_circle.m_vertices.size();
			m_circle.m_vertices[i] = PointVertex( vertex3f_for_vector3( m_center + DoubleVector3( std::cos( angle ), std::sin( angle ), 0 ) * g_sculpt_radius ), colour );
		}
		m_point.m_point = PointVertex( vertex3f_for_vector3( m_center ), colour );
		m_renderedRadius = g_sculpt_radius;
	}
	void setCircle( const std::optional<DoubleVector3>& point, bool lower ){
		const bool visible = point.has_value();
		if( visible != m_visible
		 || lower != m_lowerHighlight
		 || m_renderedRadius != g_sculpt_radius
		 || ( visible && *point != m_center ) ){
			m_visible = visible;
			m_lowerHighlight = lower;
			if( visible )
				m_center = *point;
			updateRenderables();
			SceneChangeNotify();
		}
	}

	void dab( const DoubleVector3& center ){
		const double radius = g_sculpt_radius;
		const double snap = GetSnapGridSize();
		const double amount = ( m_lower? -GetGridSize() : GetGridSize() ) * c_sculpt_spacing / 0.25;
		++m_dab;
		m_lastDab = center;

		std::vector<BrushInstance*> brushes;
		std::vector<SculptStroke::Vertex*> touched;
		std::vector<SculptColumn> columns;
		Scene_forEachVisibleSelectedBrush( [&]( BrushInstance& brush ){
			const AABB& aabb = brush.worldAABB();
			if( std::fabs( aabb.origin.x() - center.x() ) > aabb.extents.x() + radius
			 || std::fabs( aabb.origin.y() - center.y() ) > aabb.extents.y() + radius )
				return;
			brush_gather_columns( brush, columns );
			bool affected = false;
			for( const auto& column : columns ){
				const double dist = distance_xy( column.top, center );
				if( dist >= radius )
					continue;
				SculptStroke::Vertex& sv = m_stroke.insert( column.top );
				sv.zMin = std::max( sv.zMin, column.bottom + 1 );
				if( sv.dab != m_dab ){
					sv.dab = m_dab;
					sv.offset += amount * sculpt_falloff( dist / radius );
					touched.push_back( &sv );
				}
				affected = true;
			}
			if( affected )
				brushes.push_back( &brush );
		} );

		bool changed = false;
		for( SculptStroke::Vertex *sv : touched ){
			double z = sv->zOrig + ( snap > 0? sculpt_steps( sv->offset, snap ) * snap : sv->offset );
			if( z < sv->zMin ){
				const double lowest = snap > 0? sv->zOrig + std::ceil( ( sv->zMin - sv->zOrig ) / snap ) * snap : sv->zMin;
				z = std::min( lowest, sv->zCurrent );
			}
			else if( z > g_MaxWorldCoord ){
				const double highest = snap > 0? sv->zOrig + std::floor( ( g_MaxWorldCoord - sv->zOrig ) / snap ) * snap : g_MaxWorldCoord;
				z = std::max( highest, sv->zCurrent );
			}
			sv->zTarget = z;
			changed |= ( z != sv->zCurrent );
		}
		if( !changed )
			return;

		for( BrushInstance* brush : brushes ){
			brush->sculpt_vertices( [this]( DoubleVector3& v ){
				if( SculptStroke::Vertex *sv = m_stroke.find( v ); sv != nullptr && sv->dab == m_dab && sv->zTarget != sv->zCurrent ){
					v.z() = sv->zTarget;
					return true;
				}
				return false;
			} );
		}
		for( SculptStroke::Vertex *sv : touched )
			sv->zCurrent = sv->zTarget;

		SceneChangeNotify();
	}

public:
	SculptManipulatorImpl(){
		m_point.setColour( g_colour_screen );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		if( !m_visible )
			return;
		if( m_renderedRadius != g_sculpt_radius )
			updateRenderables();
		renderer.SetState( m_state_line, Renderer::eWireframeOnly );
		renderer.SetState( m_state_line, Renderer::eFullMaterials );
		renderer.addRenderable( m_circle, g_matrix4_identity );
		renderer.SetState( m_state_point, Renderer::eWireframeOnly );
		renderer.SetState( m_state_point, Renderer::eFullMaterials );
		renderer.addRenderable( m_point, g_matrix4_identity );
	}
	void highlight( const View& view, const Matrix4& pivot2world ) override {
		setCircle( sculpt_testSelect_point( view ), g_modifiers.ctrl() );
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		m_isSelected = false;
		if( g_modifiers == c_modifierNone || g_modifiers == c_modifierControl ){
			const auto point = sculpt_testSelect_point( view );
			setCircle( point, g_modifiers.ctrl() );
			m_isSelected = point.has_value();
		}
	}

	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_lower = g_modifiers.ctrl();
		m_stroke.clear();
		if( m_visible )
			dab( m_center );
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		View scissored( *m_view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, m_device_epsilon ) );
		const auto point = sculpt_testSelect_point( scissored );
		setCircle( point, m_lower );
		if( point && distance_xy( *point, m_lastDab ) >= g_sculpt_radius * c_sculpt_spacing )
			dab( *point );
	}

	Manipulatable* GetManipulatable() override {
		return this;
	}

	void setSelected( bool select ) override {
		m_isSelected = select;
	}
	bool isSelected() const override {
		return m_isSelected;
	}
};

} // namespace


SculptManipulator* New_SculptManipulator(){
	return new SculptManipulatorImpl;
}

void Sculpt_setRadius( float radius ){
	g_sculpt_radius = std::clamp( radius, c_sculpt_radius_min, c_sculpt_radius_max );
	Sys_Status( StringStream<64>( "Sculpt radius: ", int( g_sculpt_radius ) ) );
	SceneChangeNotify();
}

bool Sculpt_wheelEvent( const QWheelEvent& event ){
	if( GlobalSelectionSystem().ManipulatorMode() != SelectionSystem::eSculpt || !( event.modifiers() & Qt::KeyboardModifier::AltModifier ) )
		return false;
	const int delta = std::abs( event.angleDelta().y() ) > std::abs( event.angleDelta().x() ) // y() goes to x() with ALT pressed
	                  ? event.angleDelta().y()
	                  : event.angleDelta().x();
	if( delta != 0 )
		Sculpt_setRadius( g_sculpt_radius * std::pow( 1.1, delta / 120.0 ) );
	return true;
}

void Sculpt_modeChanged( bool isSculpt ){
	if( g_pParentWnd ){
		g_pParentWnd->forEachXYWnd( [isSculpt]( XYWnd* xywnd ){
			isSculpt? xywnd->GetWidget()->setCursor( Qt::CursorShape::CrossCursor ) : xywnd->GetWidget()->unsetCursor();
		} );
		if( g_pParentWnd->GetCamWnd() )
			isSculpt? CamWnd_getWidget( *g_pParentWnd->GetCamWnd() )->setCursor( Qt::CursorShape::CrossCursor ) : CamWnd_getWidget( *g_pParentWnd->GetCamWnd() )->unsetCursor();
	}
}

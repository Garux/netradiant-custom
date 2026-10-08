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
#include "patch.h"
#include "grid.h"
#include "map.h"
#include "mainframe.h"
#include "camwindow.h"
#include "xywindow.h"
#include "stream/stringstream.h"
#include <QWidget>
#include <QWheelEvent>

#include <array>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <tuple>


namespace
{

constexpr double c_sculpt_spacing = 0.1;

constexpr float c_sculpt_radius_min = 8;
constexpr float c_sculpt_radius_max = 4096;

struct SculptHit
{
	DoubleVector3 point;
	DoubleVector3 normal;
	const BrushInstance* brush = nullptr;
};

DoubleVector3 patch_normal_at( const Patch& patch, const DoubleVector3& point, const Vector3& viewDir ){
	const ArbitraryMeshVertex* nearest = nullptr;
	double nearestDist = std::numeric_limits<double>::max();
	for( const auto& v : patch.getTesselation().m_vertices ){
		const double dist = vector3_length_squared( DoubleVector3( vertex3f_to_vector3( v.vertex ) ) - point );
		if( dist < nearestDist ){
			nearestDist = dist;
			nearest = &v;
		}
	}
	if( nearest == nullptr )
		return -DoubleVector3( viewDir );
	const DoubleVector3 normal( normal3f_to_vector3( nearest->normal ) );
	return vector3_dot( normal, DoubleVector3( viewDir ) ) > 0? -normal : normal; // patches are two-sided
}

std::optional<SculptHit> sculpt_testSelect( const View& view, bool findBrush = true ){
	SelectionVolume test( view );
	ScenePointSelector selector;
	Scene_forEachVisible_testselect_scene_point_selected_brushes( view, selector, test );
	const Patch* patch = nullptr;
	Scene_forEachVisibleSelectedPatchInstance( [&]( PatchInstance& instance ){
		ScenePointSelector patchSelector;
		instance.testSelect( patchSelector, test );
		if( patchSelector.isSelected() && SelectionIntersection_closer( patchSelector.best(), selector.best() ) ){
			selector.addIntersection( patchSelector.best() );
			patch = &instance.getPatch();
		}
	} );
	if( !selector.isSelected() )
		return {};
	test.BeginMesh( g_matrix4_identity, true );

	SculptHit hit;
	hit.point = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, selector.best().depth(), 1 ) ) );
	if( view.fill() && patch != nullptr ){
		hit.normal = patch_normal_at( *patch, hit.point, view.getViewDir() );
	}
	else if( view.fill() && selector.face() != nullptr ){
		hit.normal = selector.face()->plane3().normal();
	}
	else{ // 2D: surfaces facing the viewer
		const std::size_t axis = vector3_max_abs_component_index( view.getViewDir() );
		hit.normal = DoubleVector3( g_vector3_axes[axis] ) * ( view.getViewDir()[axis] < 0? 1.0 : -1.0 );
	}
	if( findBrush && selector.face() != nullptr ){
		Scene_forEachVisibleSelectedBrush( [&hit, face = selector.face()]( BrushInstance& brush ){
			if( hit.brush == nullptr )
				Brush_ForEachFaceInstance( brush, [&hit, &brush, face]( FaceInstance& faceInstance ){
					if( &faceInstance.getFace() == face )
						hit.brush = &brush;
				} );
		} );
	}
	return hit;
}

inline double sculpt_falloff( double t ){
	return 1 - t * t * ( 3 - 2 * t );
}

// rounds away from zero, so any vertex inside the radius moves at least one grid step
inline double sculpt_steps( double offset, double snap ){
	const double steps = offset / snap;
	return steps > 0? std::ceil( steps - 1e-6 ) : std::floor( steps + 1e-6 );
}

inline double distance_across( const DoubleVector3& a, const DoubleVector3& b, std::size_t axis ){
	const std::size_t i = ( axis + 1 ) % 3, j = ( axis + 2 ) % 3;
	return std::hypot( a[i] - b[i], a[j] - b[j] );
}

// every vertex has a partner, differing only along the axis: the brush is built to be sculpted along it
bool brush_has_columns( const std::vector<DoubleVector3>& vertices, std::size_t axis ){
	return !vertices.empty() && std::ranges::all_of( vertices, [&vertices, axis]( const DoubleVector3& v ){
		return std::ranges::any_of( vertices, [&v, axis]( const DoubleVector3& other ){
			return distance_across( other, v, axis ) < 0.05 && std::fabs( other[axis] - v[axis] ) > 0.05;
		} );
	} );
}

using PositionKey = std::array<long long, 3>;

inline PositionKey position_key( const DoubleVector3& v ){
	return { std::llround( v.x() * 16 ), std::llround( v.y() * 16 ), std::llround( v.z() * 16 ) };
}

struct SculptColumn
{
	DoubleVector3 outer;
	double inner; // signed coordinate along the axis
	std::size_t count;
};

void gather_columns( const std::vector<DoubleVector3>& vertices, std::size_t axis, double sign, std::vector<SculptColumn>& columns ){
	columns.clear();
	for( const auto& v : vertices ){
		const double s = sign * v[axis];
		auto column = std::ranges::find_if( columns, [&v, axis]( const SculptColumn& c ){ return distance_across( c.outer, v, axis ) < 0.05; } );
		if( column == columns.end() ){
			columns.push_back( SculptColumn{ v, s, 1 } );
		}
		else{
			if( s > sign * column->outer[axis] )
				column->outer = v;
			column->inner = std::min( column->inner, s );
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
		double sOrig;
		double sCurrent;
		double sTarget;
		double sMin;
		double offset;
		std::size_t dab;
	};
private:
	using Key = std::tuple<std::size_t, long long, long long>;
	std::map<Key, std::deque<Vertex>> m_vertices;
	static Key key( const DoubleVector3& v, std::size_t axis, double sign ){
		const std::size_t i = ( axis + 1 ) % 3, j = ( axis + 2 ) % 3;
		return { axis * 2 + ( sign > 0 ), std::llround( v[i] * 16 ), std::llround( v[j] * 16 ) };
	}
public:
	Vertex* find( const DoubleVector3& v, std::size_t axis, double sign ){
		if( auto it = m_vertices.find( key( v, axis, sign ) ); it != m_vertices.end() )
			for( auto& sv : it->second )
				if( std::fabs( sv.sCurrent - sign * v[axis] ) < 0.1 )
					return &sv;
		return nullptr;
	}
	Vertex& insert( const DoubleVector3& v, std::size_t axis, double sign ){
		if( Vertex* sv = find( v, axis, sign ) )
			return *sv;
		const double s = sign * v[axis];
		return m_vertices[key( v, axis, sign )].emplace_back( Vertex{ s, s, s, std::numeric_limits<double>::lowest(), 0, 0 } );
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

	std::size_t m_axis = 2;
	double m_sign = 1;

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
		const DoubleVector3 u( g_vector3_axes[( m_axis + 1 ) % 3] );
		const DoubleVector3 v( g_vector3_axes[( m_axis + 2 ) % 3] );
		for( std::size_t i = 0; i < m_circle.m_vertices.size(); ++i ){
			const double angle = c_2pi * i / m_circle.m_vertices.size();
			m_circle.m_vertices[i] = PointVertex( vertex3f_for_vector3( m_center + ( u * std::cos( angle ) + v * std::sin( angle ) ) * g_sculpt_radius ), colour );
		}
		m_point.m_point = PointVertex( vertex3f_for_vector3( m_center ), colour );
		m_renderedRadius = g_sculpt_radius;
	}
	// surface normal averaged over the brush area, as Blender's area normal
	DoubleVector3 areaNormal( const SculptHit& hit ) const {
		DoubleVector3 sum = hit.normal;
		const DoubleVector3 n = vector3_normalised( hit.normal );
		const DoubleVector3 u = vector3_normalised( vector3_cross( n, DoubleVector3( g_vector3_axes[vector3_min_abs_component_index( n )] ) ) );
		const DoubleVector3 v = vector3_cross( n, u );
		for( int i = 0; i < 8; ++i ){
			const double angle = c_2pi * i / 8;
			const DoubleVector3 p = hit.point + ( u * std::cos( angle ) + v * std::sin( angle ) ) * ( g_sculpt_radius * 0.5 );
			const Vector4 device = matrix4_transformed_vector4( m_view->GetViewMatrix(), Vector4( Vector3( p ), 1 ) );
			if( device.w() <= 0 )
				continue;
			const DeviceVector point( device.x() / device.w(), device.y() / device.w() );
			if( std::fabs( point.x() ) > 1 || std::fabs( point.y() ) > 1 )
				continue;
			View scissored( *m_view );
			ConstructSelectionTest( scissored, SelectionBoxForPoint( point, m_device_epsilon ) );
			if( const auto sample = sculpt_testSelect( scissored, false ) )
				sum += sample->normal;
		}
		return vector3_normalised( sum );
	}
	// axis from the column structure of the brush under the cursor, the visible end from the area normal
	void updateDirection( const SculptHit& hit, bool fill ){
		const DoubleVector3 normal = fill? areaNormal( hit ) : hit.normal;
		std::size_t axis = vector3_max_abs_component_index( normal );
		if( fill && hit.brush != nullptr ){
			std::vector<DoubleVector3> vertices;
			hit.brush->gather_vertices( vertices );
			double best = -1;
			for( std::size_t i = 0; i < 3; ++i ){
				if( brush_has_columns( vertices, i ) && std::fabs( normal[i] ) > best ){
					best = std::fabs( normal[i] );
					axis = i;
				}
			}
		}
		if( std::fabs( normal[axis] ) > 0.1 )
			m_sign = normal[axis] > 0? 1 : -1;
		else if( axis != m_axis )
			m_sign = 1;
		m_axis = axis;
	}
	void setCircle( const std::optional<SculptHit>& hit, bool lower, bool updateDir, bool fill = true ){
		const std::size_t axis = m_axis;
		const double sign = m_sign;
		if( hit && updateDir )
			updateDirection( *hit, fill );
		const bool visible = hit.has_value();
		if( visible != m_visible
		 || lower != m_lowerHighlight
		 || m_renderedRadius != g_sculpt_radius
		 || axis != m_axis || sign != m_sign
		 || ( visible && hit->point != m_center ) ){
			m_visible = visible;
			m_lowerHighlight = lower;
			if( visible )
				m_center = hit->point;
			updateRenderables();
			SceneChangeNotify();
		}
	}

	void dab( const DoubleVector3& point ){
		const double radius = g_sculpt_radius;
		const double snap = GetSnapGridSize();
		const double amount = ( m_lower? -GetGridSize() : GetGridSize() ) * c_sculpt_spacing / 0.25;
		const std::size_t axis = m_axis;
		const double sign = m_sign;
		++m_dab;
		m_lastDab = point;

		struct Candidate
		{
			BrushInstance* brush;
			std::vector<SculptColumn> columns;
		};
		std::vector<Candidate> candidates;
		std::set<PositionKey> pinned; // vertices, not moved with their brush, keep surfaces joined
		std::vector<DoubleVector3> vertices;
		const AABB region( point, Vector3( radius, radius, radius ) );
		Scene_forEachVisibleBrush( GlobalSceneGraph(), [&]( BrushInstance& brush ){
			if( !aabb_intersects_aabb( brush.worldAABB(), region ) )
				return;
			vertices.clear();
			brush.gather_vertices( vertices );
			std::vector<SculptColumn> columns;
			if( brush.isSelected() )
				gather_columns( vertices, axis, sign, columns );
			for( const auto& v : vertices )
				if( std::ranges::none_of( columns, [&v]( const SculptColumn& c ){ return c.outer == v; } ) )
					pinned.insert( position_key( v ) );
			std::erase_if( columns, [&point, radius]( const SculptColumn& c ){ return vector3_length( c.outer - point ) >= radius; } );
			if( !columns.empty() )
				candidates.push_back( Candidate{ &brush, std::move( columns ) } );
		} );

		std::vector<PatchInstance*> patches;
		Scene_forEachVisiblePatchInstance( [&]( PatchInstance& patch ){
			if( !aabb_intersects_aabb( patch.worldAABB(), region ) )
				return;
			if( patch.isSelected() )
				patches.push_back( &patch );
			else
				for( const auto& ctrl : patch.getPatch().getControlPoints() )
					pinned.insert( position_key( ctrl.m_vertex ) );
		} );

		std::vector<SculptStroke::Vertex*> touched;
		const auto touch = [&]( const DoubleVector3& v ) -> SculptStroke::Vertex& {
			SculptStroke::Vertex& sv = m_stroke.insert( v, axis, sign );
			if( sv.dab != m_dab ){
				sv.dab = m_dab;
				sv.offset += amount * sculpt_falloff( vector3_length( v - point ) / radius );
				touched.push_back( &sv );
			}
			return sv;
		};
		for( const auto& candidate : candidates ){
			for( const auto& column : candidate.columns ){
				if( !pinned.contains( position_key( column.outer ) ) ){
					SculptStroke::Vertex& sv = touch( column.outer );
					sv.sMin = std::max( sv.sMin, column.inner + 1 );
				}
			}
		}
		for( PatchInstance* patch : patches ){
			for( const auto& ctrl : patch->getPatch().getControlPoints() ){
				const DoubleVector3 v( ctrl.m_vertex );
				if( vector3_length( v - point ) < radius && !pinned.contains( position_key( v ) ) )
					touch( v );
			}
		}

		bool changed = false;
		for( SculptStroke::Vertex *sv : touched ){
			double s = sv->sOrig + ( snap > 0? sculpt_steps( sv->offset, snap ) * snap : sv->offset );
			if( s < sv->sMin ){
				const double lowest = snap > 0? sv->sOrig + std::ceil( ( sv->sMin - sv->sOrig ) / snap ) * snap : sv->sMin;
				s = std::min( lowest, sv->sCurrent );
			}
			else if( s > g_MaxWorldCoord ){
				const double highest = snap > 0? sv->sOrig + std::floor( ( g_MaxWorldCoord - sv->sOrig ) / snap ) * snap : g_MaxWorldCoord;
				s = std::max( highest, sv->sCurrent );
			}
			sv->sTarget = s;
			changed |= ( s != sv->sCurrent );
		}
		if( !changed )
			return;

		for( const auto& candidate : candidates ){
			candidate.brush->sculpt_vertices( [&]( DoubleVector3& v ){
				if( SculptStroke::Vertex *sv = m_stroke.find( v, axis, sign ); sv != nullptr && sv->dab == m_dab && sv->sTarget != sv->sCurrent ){
					v[axis] = sign * sv->sTarget;
					return true;
				}
				return false;
			} );
		}
		for( PatchInstance* instance : patches ){
			Patch& patch = instance->getPatch();
			const auto target = [&]( const PatchControl& ctrl ) -> SculptStroke::Vertex* {
				SculptStroke::Vertex *sv = m_stroke.find( DoubleVector3( ctrl.m_vertex ), axis, sign );
				return sv != nullptr && sv->dab == m_dab && sv->sTarget != sv->sCurrent? sv : nullptr;
			};
			if( std::ranges::none_of( patch.getControlPoints(), [&target]( const PatchControl& ctrl ){ return target( ctrl ) != nullptr; } ) )
				continue;
			patch.undoSave();
			for( auto& ctrl : patch.getControlPoints() )
				if( SculptStroke::Vertex *sv = target( ctrl ) )
					ctrl.m_vertex[axis] = sign * sv->sTarget;
			patch.controlPointsChanged();
		}
		for( SculptStroke::Vertex *sv : touched )
			sv->sCurrent = sv->sTarget;

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
		setCircle( sculpt_testSelect( view ), g_modifiers.ctrl(), true, view.fill() );
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		m_isSelected = false;
		if( g_modifiers == c_modifierNone || g_modifiers == c_modifierControl ){
			const auto hit = sculpt_testSelect( view );
			setCircle( hit, g_modifiers.ctrl(), true, view.fill() );
			m_isSelected = hit.has_value();
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
		const auto hit = sculpt_testSelect( scissored );
		setCircle( hit, m_lower, false ); // direction stays locked during a stroke
		if( hit && vector3_length( hit->point - m_lastDab ) >= g_sculpt_radius * c_sculpt_spacing )
			dab( hit->point );
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

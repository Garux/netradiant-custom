/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "selection_mtor_rotate.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtable_rotate.h"
#include "selectionlib.h"

template<typename remap_policy>
void draw_semicircle( const std::size_t segments, const float radius, PointVertex* vertices, remap_policy remap ){
	const double increment = c_pi / double( segments << 2 );

	std::size_t count = 0;
	float x = radius;
	float y = 0;
	remap_policy::set( vertices[segments << 2].vertex, -radius, 0, 0 );
	while ( count < segments )
	{
		PointVertex* i = vertices + count;
		PointVertex* j = vertices + ( ( segments << 1 ) - ( count + 1 ) );

		PointVertex* k = i + ( segments << 1 );
		PointVertex* l = j + ( segments << 1 );

#if 0
		PointVertex* m = i + ( segments << 2 );
		PointVertex* n = j + ( segments << 2 );
		PointVertex* o = k + ( segments << 2 );
		PointVertex* p = l + ( segments << 2 );
#endif

		remap_policy::set( i->vertex, x,-y, 0 );
		remap_policy::set( k->vertex,-y,-x, 0 );
#if 0
		remap_policy::set( m->vertex,-x, y, 0 );
		remap_policy::set( o->vertex, y, x, 0 );
#endif

		++count;

		{
			const double theta = increment * count;
			x = radius * cos( theta );
			y = radius * sin( theta );
		}

		remap_policy::set( j->vertex, y,-x, 0 );
		remap_policy::set( l->vertex,-x,-y, 0 );
#if 0
		remap_policy::set( n->vertex,-y, x, 0 );
		remap_policy::set( p->vertex, x, y, 0 );
#endif
	}
}


inline Vector3 normalised_safe( const Vector3& self ){
	if ( vector3_equal( self, g_vector3_identity ) ) {
		return g_vector3_identity;
	}
	return vector3_normalised( self );
}



class RotateManipulatorImpl final : public RotateManipulator, public ManipulatorSelectionChangeable
{
	RotateFree m_free;
	RotateAxis m_axis;
	Vector3 m_axis_screen;
	RenderableSemiCircle m_circle_x;
	RenderableSemiCircle m_circle_y;
	RenderableSemiCircle m_circle_z;
	RenderableCircle m_circle_screen;
	RenderableCircle m_circle_sphere;
	SelectableBool m_selectable_x;
	SelectableBool m_selectable_y;
	SelectableBool m_selectable_z;
	SelectableBool m_selectable_screen;
	SelectableBool m_selectable_sphere;
	Pivot2World m_pivot;
	Matrix4 m_local2world_x;
	Matrix4 m_local2world_y;
	Matrix4 m_local2world_z;
	bool m_circle_x_visible;
	bool m_circle_y_visible;
	bool m_circle_z_visible;
public:
	RotateManipulatorImpl( Rotatable& rotatable, std::size_t segments, float radius ) :
		m_free( rotatable ),
		m_axis( rotatable ),
		m_circle_x( ( segments << 2 ) + 1 ),
		m_circle_y( ( segments << 2 ) + 1 ),
		m_circle_z( ( segments << 2 ) + 1 ),
		m_circle_screen( segments << 3 ),
		m_circle_sphere( segments << 3 ){
		draw_semicircle( segments, radius, m_circle_x.m_vertices.data(), RemapYZX() );
		draw_semicircle( segments, radius, m_circle_y.m_vertices.data(), RemapZXY() );
		draw_semicircle( segments, radius, m_circle_z.m_vertices.data(), RemapXYZ() );

		draw_circle( segments, radius * 1.15f, m_circle_screen.m_vertices.data(), RemapXYZ() );
		draw_circle( segments, radius, m_circle_sphere.m_vertices.data(), RemapXYZ() );
	}


	void UpdateColours(){
		m_circle_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
		m_circle_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
		m_circle_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
		m_circle_screen.setColour( colourSelected( g_colour_screen, m_selectable_screen.isSelected() ) );
		m_circle_sphere.setColour( colourSelected( g_colour_sphere, false ) );
	}

	void updateCircleTransforms(){
		Vector3 localViewpoint( matrix4_transformed_direction( matrix4_transposed( m_pivot.m_worldSpace ), m_pivot.m_viewpointSpace.z().vec3() ) );

		m_circle_x_visible = !vector3_equal_epsilon( g_vector3_axis_x, localViewpoint, 1e-6f );
		if ( m_circle_x_visible ) {
			m_local2world_x = g_matrix4_identity;
			m_local2world_x.y().vec3() = normalised_safe(
			            vector3_cross( g_vector3_axis_x, localViewpoint )
			        );
			m_local2world_x.z().vec3() = normalised_safe(
			            vector3_cross( m_local2world_x.x().vec3(), m_local2world_x.y().vec3() )
			        );
			matrix4_premultiply_by_matrix4( m_local2world_x, m_pivot.m_worldSpace );
		}

		m_circle_y_visible = !vector3_equal_epsilon( g_vector3_axis_y, localViewpoint, 1e-6f );
		if ( m_circle_y_visible ) {
			m_local2world_y = g_matrix4_identity;
			m_local2world_y.z().vec3() = normalised_safe(
			            vector3_cross( g_vector3_axis_y, localViewpoint )
			        );
			m_local2world_y.x().vec3() = normalised_safe(
			            vector3_cross( m_local2world_y.y().vec3(), m_local2world_y.z().vec3() )
			        );
			matrix4_premultiply_by_matrix4( m_local2world_y, m_pivot.m_worldSpace );
		}

		m_circle_z_visible = !vector3_equal_epsilon( g_vector3_axis_z, localViewpoint, 1e-6f );
		if ( m_circle_z_visible ) {
			m_local2world_z = g_matrix4_identity;
			m_local2world_z.x().vec3() = normalised_safe(
			            vector3_cross( g_vector3_axis_z, localViewpoint )
			        );
			m_local2world_z.y().vec3() = normalised_safe(
			            vector3_cross( m_local2world_z.z().vec3(), m_local2world_z.x().vec3() )
			        );
			matrix4_premultiply_by_matrix4( m_local2world_z, m_pivot.m_worldSpace );
		}
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		updateCircleTransforms();

		// temp hack
		UpdateColours();

		renderer.SetState( m_state_outer, Renderer::eWireframeOnly );
		renderer.SetState( m_state_outer, Renderer::eFullMaterials );

		renderer.addRenderable( m_circle_screen, m_pivot.m_viewpointSpace );
		renderer.addRenderable( m_circle_sphere, m_pivot.m_viewpointSpace );

		if ( m_circle_x_visible ) {
			renderer.addRenderable( m_circle_x, m_local2world_x );
		}
		if ( m_circle_y_visible ) {
			renderer.addRenderable( m_circle_y, m_local2world_y );
		}
		if ( m_circle_z_visible ) {
			renderer.addRenderable( m_circle_z, m_local2world_z );
		}
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		if( g_modifiers != c_modifierNone )
			return selectionChange( nullptr );

		m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );
		updateCircleTransforms();

		SelectionPool selector;

		{
			{
				const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_local2world_x ) );

#if defined( DEBUG_SELECTION )
				g_render_clipped.construct( view.GetViewMatrix() );
#endif

				SelectionIntersection best;
				LineStrip_BestPoint( local2view, m_circle_x.m_vertices.data(), m_circle_x.m_vertices.size(), best );
				selector.addSelectable( best, &m_selectable_x );
			}

			{
				const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_local2world_y ) );

#if defined( DEBUG_SELECTION )
				g_render_clipped.construct( view.GetViewMatrix() );
#endif

				SelectionIntersection best;
				LineStrip_BestPoint( local2view, m_circle_y.m_vertices.data(), m_circle_y.m_vertices.size(), best );
				selector.addSelectable( best, &m_selectable_y );
			}

			{
				const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_local2world_z ) );

#if defined( DEBUG_SELECTION )
				g_render_clipped.construct( view.GetViewMatrix() );
#endif

				SelectionIntersection best;
				LineStrip_BestPoint( local2view, m_circle_z.m_vertices.data(), m_circle_z.m_vertices.size(), best );
				selector.addSelectable( best, &m_selectable_z );
			}
		}

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_viewpointSpace ) );

			{
				SelectionIntersection best;
				LineLoop_BestPoint( local2view, m_circle_screen.m_vertices.data(), m_circle_screen.m_vertices.size(), best );
				selector.addSelectable( best, &m_selectable_screen );
			}

//		{
//			SelectionIntersection best;
//			Circle_BestPoint( local2view, EClipCull::CW, m_circle_sphere.m_vertices.data(), m_circle_sphere.m_vertices.size(), best );
//			selector.addSelectable( best, &m_selectable_sphere );
//		}
		}

		m_axis_screen = m_pivot.m_axis_screen;

		if ( selector.failed() )
			selector.addSelectable( SelectionIntersection( 0, 0 ), &m_selectable_sphere );

		selectionChange( selector );
	}

	Manipulatable* GetManipulatable() override {
		if ( m_selectable_x.isSelected() ) {
			m_axis.SetAxis( g_vector3_axis_x );
			return &m_axis;
		}
		else if ( m_selectable_y.isSelected() ) {
			m_axis.SetAxis( g_vector3_axis_y );
			return &m_axis;
		}
		else if ( m_selectable_z.isSelected() ) {
			m_axis.SetAxis( g_vector3_axis_z );
			return &m_axis;
		}
		else if ( m_selectable_screen.isSelected() ) {
			m_axis.SetAxis( m_axis_screen );
			return &m_axis;
		}
		else{
			return &m_free;
		}
	}

	void setSelected( bool select ) override {
		m_selectable_x.setSelected( select );
		m_selectable_y.setSelected( select );
		m_selectable_z.setSelected( select );
		m_selectable_screen.setSelected( select );
		m_selectable_sphere.setSelected( select );
	}
	bool isSelected() const override {
		return m_selectable_x.isSelected()
		    || m_selectable_y.isSelected()
		    || m_selectable_z.isSelected()
		    || m_selectable_screen.isSelected()
		    || m_selectable_sphere.isSelected();
	}
};


RotateManipulator* New_RotateManipulator( Rotatable& rotatable, std::size_t segments, float radius ){
	return new RotateManipulatorImpl( rotatable, segments, radius );
}

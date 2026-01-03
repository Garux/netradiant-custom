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

#include "selection_mtor_translate.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtable_translate.h"
#include "selectionlib.h"

class TranslateManipulatorImpl final : public TranslateManipulator, public ManipulatorSelectionChangeable
{
	TranslateFree m_free;
	TranslateAxis m_axis;
	RenderableLine m_arrow_x;
	RenderableLine m_arrow_y;
	RenderableLine m_arrow_z;
	RenderableArrowHead m_arrow_head_x;
	RenderableArrowHead m_arrow_head_y;
	RenderableArrowHead m_arrow_head_z;
	RenderableQuad m_quad_screen;
	SelectableBool m_selectable_x;
	SelectableBool m_selectable_y;
	SelectableBool m_selectable_z;
	SelectableBool m_selectable_screen;
	Pivot2World m_pivot;
public:
	TranslateManipulatorImpl( Translatable& translatable, std::size_t segments, float length ) :
		m_free( translatable ),
		m_axis( translatable ),
		m_arrow_head_x( 3 * 2 * ( segments << 3 ) ),
		m_arrow_head_y( 3 * 2 * ( segments << 3 ) ),
		m_arrow_head_z( 3 * 2 * ( segments << 3 ) ){
		draw_arrowline( length, m_arrow_x.m_line, 0 );
		draw_arrowhead( segments, length, m_arrow_head_x.m_vertices.data(), TripleRemapXYZ<Vertex3f>(), TripleRemapXYZ<Normal3f>() );
		draw_arrowline( length, m_arrow_y.m_line, 1 );
		draw_arrowhead( segments, length, m_arrow_head_y.m_vertices.data(), TripleRemapYZX<Vertex3f>(), TripleRemapYZX<Normal3f>() );
		draw_arrowline( length, m_arrow_z.m_line, 2 );
		draw_arrowhead( segments, length, m_arrow_head_z.m_vertices.data(), TripleRemapZXY<Vertex3f>(), TripleRemapZXY<Normal3f>() );

		draw_quad( 16, m_quad_screen.m_quad );
	}

	void UpdateColours(){
		m_arrow_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
		m_arrow_head_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
		m_arrow_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
		m_arrow_head_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
		m_arrow_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
		m_arrow_head_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
		m_quad_screen.setColour( colourSelected( g_colour_screen, m_selectable_screen.isSelected() ) );
	}

	bool manipulator_show_axis( const Pivot2World& pivot, const Vector3& axis ){
		return std::fabs( vector3_dot( pivot.m_axis_screen, axis ) ) < 0.95;
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

		// temp hack
		UpdateColours();

		Vector3 x = vector3_normalised( m_pivot.m_worldSpace.x().vec3() );
		bool show_x = manipulator_show_axis( m_pivot, x );

		Vector3 y = vector3_normalised( m_pivot.m_worldSpace.y().vec3() );
		bool show_y = manipulator_show_axis( m_pivot, y );

		Vector3 z = vector3_normalised( m_pivot.m_worldSpace.z().vec3() );
		bool show_z = manipulator_show_axis( m_pivot, z );

		renderer.SetState( m_state_wire, Renderer::eWireframeOnly );
		renderer.SetState( m_state_wire, Renderer::eFullMaterials );

		if ( show_x ) {
			renderer.addRenderable( m_arrow_x, m_pivot.m_worldSpace );
		}
		if ( show_y ) {
			renderer.addRenderable( m_arrow_y, m_pivot.m_worldSpace );
		}
		if ( show_z ) {
			renderer.addRenderable( m_arrow_z, m_pivot.m_worldSpace );
		}

		renderer.addRenderable( m_quad_screen, m_pivot.m_viewplaneSpace );

		renderer.SetState( m_state_fill, Renderer::eWireframeOnly );
		renderer.SetState( m_state_fill, Renderer::eFullMaterials );

		if ( show_x ) {
			renderer.addRenderable( m_arrow_head_x, m_pivot.m_worldSpace );
		}
		if ( show_y ) {
			renderer.addRenderable( m_arrow_head_y, m_pivot.m_worldSpace );
		}
		if ( show_z ) {
			renderer.addRenderable( m_arrow_head_z, m_pivot.m_worldSpace );
		}
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		if( g_modifiers != c_modifierNone )
			return selectionChange( nullptr );

		m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );

		SelectionPool selector;

		Vector3 x = vector3_normalised( m_pivot.m_worldSpace.x().vec3() );
		bool show_x = manipulator_show_axis( m_pivot, x );

		Vector3 y = vector3_normalised( m_pivot.m_worldSpace.y().vec3() );
		bool show_y = manipulator_show_axis( m_pivot, y );

		Vector3 z = vector3_normalised( m_pivot.m_worldSpace.z().vec3() );
		bool show_z = manipulator_show_axis( m_pivot, z );

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_viewpointSpace ) );

			{
				SelectionIntersection best;
				Quad_BestPoint( local2view, EClipCull::CW, m_quad_screen.m_quad, best );
				if ( best.valid() ) {
					best = SelectionIntersection( 0, 0 );
					selector.addSelectable( best, &m_selectable_screen );
				}
			}
		}

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_worldSpace ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif

			if ( show_x ) {
				SelectionIntersection best;
				Line_BestPoint( local2view, m_arrow_x.m_line, best );
				Triangles_BestPoint( local2view, EClipCull::CW, m_arrow_head_x.m_vertices.begin(), m_arrow_head_x.m_vertices.end(), best );
				selector.addSelectable( best, &m_selectable_x );
			}

			if ( show_y ) {
				SelectionIntersection best;
				Line_BestPoint( local2view, m_arrow_y.m_line, best );
				Triangles_BestPoint( local2view, EClipCull::CW, m_arrow_head_y.m_vertices.begin(), m_arrow_head_y.m_vertices.end(), best );
				selector.addSelectable( best, &m_selectable_y );
			}

			if ( show_z ) {
				SelectionIntersection best;
				Line_BestPoint( local2view, m_arrow_z.m_line, best );
				Triangles_BestPoint( local2view, EClipCull::CW, m_arrow_head_z.m_vertices.begin(), m_arrow_head_z.m_vertices.end(), best );
				selector.addSelectable( best, &m_selectable_z );
			}
		}

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
		else
		{
			return &m_free;
		}
	}

	void setSelected( bool select ) override {
		m_selectable_x.setSelected( select );
		m_selectable_y.setSelected( select );
		m_selectable_z.setSelected( select );
		m_selectable_screen.setSelected( select );
	}
	bool isSelected() const override {
		return m_selectable_x.isSelected()
		    || m_selectable_y.isSelected()
		    || m_selectable_z.isSelected()
		    || m_selectable_screen.isSelected();
	}
};


TranslateManipulator* New_TranslateManipulator( Translatable& translatable, std::size_t segments, float length ){
	return new TranslateManipulatorImpl( translatable, segments, length );
}

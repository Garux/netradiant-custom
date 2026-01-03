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

#include "selection_mtor_scale.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtable_scale.h"
#include "selectionlib.h"

class ScaleManipulatorImpl final : public ScaleManipulator, public ManipulatorSelectionChangeable
{
	ScaleFree m_free;
	ScaleAxis m_axis;
	RenderableLine m_arrow_x;
	RenderableLine m_arrow_y;
	RenderableLine m_arrow_z;
	RenderableQuad m_quad_screen;
	SelectableBool m_selectable_x;
	SelectableBool m_selectable_y;
	SelectableBool m_selectable_z;
	SelectableBool m_selectable_screen;
	Pivot2World m_pivot;
public:
	ScaleManipulatorImpl( Scalable& scalable, std::size_t segments, float length ) :
		m_free( scalable ),
		m_axis( scalable ){
		draw_arrowline( length, m_arrow_x.m_line, 0 );
		draw_arrowline( length, m_arrow_y.m_line, 1 );
		draw_arrowline( length, m_arrow_z.m_line, 2 );

		draw_quad( 16, m_quad_screen.m_quad );
	}

	void UpdateColours(){
		m_arrow_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
		m_arrow_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
		m_arrow_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
		m_quad_screen.setColour( colourSelected( g_colour_screen, m_selectable_screen.isSelected() ) );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

		// temp hack
		UpdateColours();

		renderer.addRenderable( m_arrow_x, m_pivot.m_worldSpace );
		renderer.addRenderable( m_arrow_y, m_pivot.m_worldSpace );
		renderer.addRenderable( m_arrow_z, m_pivot.m_worldSpace );

		renderer.addRenderable( m_quad_screen, m_pivot.m_viewpointSpace );
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		if( g_modifiers != c_modifierNone )
			return selectionChange( nullptr );

		m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );

		SelectionPool selector;

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_worldSpace ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif

			{
				SelectionIntersection best;
				Line_BestPoint( local2view, m_arrow_x.m_line, best );
				selector.addSelectable( best, &m_selectable_x );
			}

			{
				SelectionIntersection best;
				Line_BestPoint( local2view, m_arrow_y.m_line, best );
				selector.addSelectable( best, &m_selectable_y );
			}

			{
				SelectionIntersection best;
				Line_BestPoint( local2view, m_arrow_z.m_line, best );
				selector.addSelectable( best, &m_selectable_z );
			}
		}

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_viewpointSpace ) );

			{
				SelectionIntersection best;
				Quad_BestPoint( local2view, EClipCull::CW, m_quad_screen.m_quad, best );
				selector.addSelectable( best, &m_selectable_screen );
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
		else{
			m_free.SetAxes( g_vector3_identity, g_vector3_identity );
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


ScaleManipulator* New_ScaleManipulator( Scalable& scalable, std::size_t segments, float length ){
	return new ScaleManipulatorImpl( scalable, segments, length );
}

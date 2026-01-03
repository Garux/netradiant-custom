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

#include "selection_mtor_skew.h"

#include "selection_.h"
#include "dragplanes.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtable_translate.h"
#include "selection_mtable_rotate.h"
#include "selection_mtable_scale.h"
#include "selection_mtable_skew.h"

class SkewManipulatorImpl final : public SkewManipulator, public ManipulatorSelectionChangeable
{
	SkewAxis m_skew;
	TranslateFreeXY_Z m_translateFreeXY_Z;
	ScaleAxis m_scaleAxis;
	ScaleFree m_scaleFree;
	RotateAxis m_rotateAxis;
	AABB m_bounds_draw;
	const AABB& m_bounds;
	Matrix4& m_pivot2world;
	const bool& m_pivotIsCustom;
/*
	RenderableLine m_lineXy_;
	RenderableLine m_lineXy;
	RenderableLine m_lineXz_;
	RenderableLine m_lineXz;
	RenderableLine m_lineYz_;
	RenderableLine m_lineYz;
	RenderableLine m_lineYx_;
	RenderableLine m_lineYx;
	RenderableLine m_lineZx_;
	RenderableLine m_lineZx;
	RenderableLine m_lineZy_;
	RenderableLine m_lineZy;
*/
	RenderableLine m_lines[3][2][2];
	SelectableBool m_selectables[3][2][2];	//[X][YZ][-+]
	SelectableBool m_selectable_translateFree;
	DragPlanes m_selectables_scale; //+-X+-Y+-Z
	SelectableBool m_selectables_rotate[3][2][2];	//[X][-+Y][-+Z]
	Pivot2World m_pivot;
	Matrix4 m_worldSpace;
	RenderableArrowHead m_arrow;
	Matrix4 m_arrow_modelview;
	Matrix4 m_arrow_modelview2;
	RenderablePoint m_point;
public:
	SkewManipulatorImpl( Skewable& skewable, Translatable& translatable, Scalable& scalable, Rotatable& rotatable,
			AllTransformable& transformable, const AABB& bounds, Matrix4& pivot2world, const bool& pivotIsCustom, const std::size_t segments = 2 ) :
		m_skew( skewable ),
		m_translateFreeXY_Z( translatable, transformable ),
		m_scaleAxis( scalable ),
		m_scaleFree( scalable ),
		m_rotateAxis( rotatable ),
		m_bounds( bounds ),
		m_pivot2world( pivot2world ),
		m_pivotIsCustom( pivotIsCustom ),
		m_selectables_scale( {} ),
		m_arrow( 3 * 2 * ( segments << 3 ) ) {
		for ( int i = 0; i < 3; ++i ){
			for ( int j = 0; j < 2; ++j ){
				const int x = i;
				const int y = ( i + j + 1 ) % 3;
				Vertex3f& xy_ = m_lines[i][j][0].m_line[0].vertex;
				Vertex3f& x_y_ = m_lines[i][j][0].m_line[1].vertex;
				Vertex3f& xy = m_lines[i][j][1].m_line[0].vertex;
				Vertex3f& x_y = m_lines[i][j][1].m_line[1].vertex;
				xy = x_y = xy_ = x_y_ = vertex3f_identity;
				xy[x] = xy_[x] = 1;
				x_y[x] = x_y_[x] = -1;
				xy[y] = x_y[y] = 1;
				xy_[y] = x_y_[y] = -1;
			}
		}
		draw_arrowhead( segments, 0, m_arrow.m_vertices.data(), TripleRemapXYZ<Vertex3f>(), TripleRemapXYZ<Normal3f>() );
		m_arrow.setColour( g_colour_selected );
		m_point.setColour( g_colour_selected );
	}

	void UpdateColours() {
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					m_lines[i][j][k].setColour( colourSelected( g_colour_screen, m_selectables[i][j][k].isSelected() ) );
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				if( m_selectables_scale.getSelectables()[i * 2 + j].isSelected() ){
					m_lines[( i + 1 ) % 3][1][j ^ 1].setColour( g_colour_selected );
					m_lines[( i + 2 ) % 3][0][j ^ 1].setColour( g_colour_selected );
				}
	}

	void updateModelview( const VolumeTest& volume, const Matrix4& pivot2world ){
		//m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		//m_pivot.update( matrix4_translation_for_vec3( matrix4_get_translation_vec3( pivot2world ) ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		m_pivot.update( matrix4_translation_for_vec3( m_bounds.origin ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		//m_pivot.update( g_matrix4_identity, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() ); //no shaking in cam due to low precision this way; smooth and sometimes very incorrect result
//		globalOutputStream() << m_pivot.m_worldSpace << '\n';
		Matrix4& m = m_pivot.m_worldSpace; /* go affine to increase precision */
		m[1] = m[2] = m[3] = m[4] = m[6] = m[7] = m[8] = m[9] = m[11] = 0;
		m[15] = 1;
		m_bounds_draw = aabb_for_oriented_aabb( m_bounds, matrix4_affine_inverse( m_pivot.m_worldSpace ) ); //screen scale
		for ( int i = 0; i < 3; ++i ){
			if( m_bounds_draw.extents[i] < 16 )
				m_bounds_draw.extents[i] = 18;
			else
				m_bounds_draw.extents[i] += 2.0f;
		}
		m_bounds_draw = aabb_for_oriented_aabb( m_bounds_draw, m_pivot.m_worldSpace ); //world scale
		m_bounds_draw.origin = m_bounds.origin;

		m_worldSpace = matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( m_bounds_draw.origin ), matrix4_scale_for_vec3( m_bounds_draw.extents ) );
		matrix4_premultiply_by_matrix4( m_worldSpace, matrix4_translation_for_vec3( -matrix4_get_translation_vec3( pivot2world ) ) );
		matrix4_premultiply_by_matrix4( m_worldSpace, pivot2world );

//		globalOutputStream() << m_worldSpace << '\n';
//		globalOutputStream() << pivot2world << '\n';
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		updateModelview( volume, pivot2world );

		// temp hack
		UpdateColours();

		renderer.SetState( m_state_wire, Renderer::eWireframeOnly );
		renderer.SetState( m_state_wire, Renderer::eFullMaterials );

		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j ){
#if 0
				const Vector3 dir = ( m_lines[i][j][0].m_line[0].vertex - m_lines[i][j][1].m_line[0].vertex ) / 2;
				const float dot = vector3_dot( dir, m_pivot.m_axis_screen );
				if( dot > 0.9999f )
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
				else if( dot < -0.9999f )
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
				else{
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
				}
#else
				if( m_selectables[i][j][0].isSelected() ){ /* add selected last to get highlighted one rendered on top in 2d */
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
				}
				else{
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
				}
#endif
			}

		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					if( m_selectables[i][j][k].isSelected() ){
						Vector3 origin = matrix4_transformed_point( m_worldSpace, m_lines[i][j][k].m_line[0].vertex );
						Vector3 origin2 = matrix4_transformed_point( m_worldSpace, m_lines[i][j][k].m_line[1].vertex );

						Pivot2World_worldSpace( m_arrow_modelview, matrix4_translation_for_vec3( origin ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
						Pivot2World_worldSpace( m_arrow_modelview2, matrix4_translation_for_vec3( origin2 ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

						const Matrix4 rot( i == 0? g_matrix4_identity: i == 1? matrix4_rotation_for_sincos_z( 1, 0 ): matrix4_rotation_for_sincos_y( -1, 0 ) );
						matrix4_multiply_by_matrix4( m_arrow_modelview, rot );
						matrix4_multiply_by_matrix4( m_arrow_modelview2, rot );
						const float x = 0.7f;
						matrix4_multiply_by_matrix4( m_arrow_modelview, matrix4_scale_for_vec3( Vector3( x, x, x ) ) );
						matrix4_multiply_by_matrix4( m_arrow_modelview2, matrix4_scale_for_vec3( Vector3( -x, x, x ) ) );

						renderer.SetState( m_state_fill, Renderer::eWireframeOnly );
						renderer.SetState( m_state_fill, Renderer::eFullMaterials );
						renderer.addRenderable( m_arrow, m_arrow_modelview );
						renderer.addRenderable( m_arrow, m_arrow_modelview2 );
						return;
					}

		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					if( m_selectables_rotate[i][j][k].isSelected() ){
						renderer.SetState( m_state_point, Renderer::eWireframeOnly );
						renderer.SetState( m_state_point, Renderer::eFullMaterials );
						renderer.addRenderable( m_point, m_worldSpace );
						renderer.addRenderable( m_point, m_worldSpace );
						return;
					}
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		updateModelview( view, pivot2world );
		SelectionPool selector;
		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_worldSpace ) );

		if( g_modifiers == c_modifierAlt && view.fill() )
			goto testSelectBboxPlanes;
		if( g_modifiers != c_modifierNone )
			return selectionChange( nullptr );

		/* try corner points to rotate */
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k ){
					m_point.m_point.vertex[i] = 0;
					m_point.m_point.vertex[( i + 1 ) % 3] = j? 1 : -1;
					m_point.m_point.vertex[( i + 2 ) % 3] = k? 1 : -1;
					SelectionIntersection best;
					Point_BestPoint( local2view, m_point.m_point, best );
					selector.addSelectable( best, &m_selectables_rotate[i][j][k] );
				}
		if( !selector.failed() ) {
			( *selector.begin() ).second->setSelected( true );
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					for ( int k = 0; k < 2; ++k )
						if( m_selectables_rotate[i][j][k].isSelected() ){
							m_point.m_point.vertex[i] = 0;
							m_point.m_point.vertex[( i + 1 ) % 3] = j? 1 : -1;
							m_point.m_point.vertex[( i + 2 ) % 3] = k? 1 : -1;
							if( !m_pivotIsCustom ){
								const Vector3 origin = m_bounds.origin + m_point.m_point.vertex * -1 * m_bounds.extents;
								m_pivot2world = matrix4_translation_for_vec3( origin );
							}
							/* set radius */
							if( std::fabs( vector3_dot( m_pivot.m_axis_screen, g_vector3_axes[i] ) ) < 0.2 ){
								Vector3 origin = matrix4_get_translation_vec3( m_pivot2world );
								Vector3 point = m_bounds_draw.origin + m_point.m_point.vertex * m_bounds_draw.extents;
								const Matrix4 inv = matrix4_affine_inverse( m_pivot.m_worldSpace );
								matrix4_transform_point( inv, origin );
								matrix4_transform_point( inv, point );
								point -= origin;
								point = vector3_added( point, vector3_scaled( m_pivot.m_axis_screen, -vector3_dot( point, m_pivot.m_axis_screen ) ) ); //constrain_to_axis
								m_rotateAxis.SetRadius( vector3_length( point ) - g_SELECT_EPSILON / 2.0 - 1.0 ); /* use smaller radius to constrain to one rotation direction in 2D */
								//globalOutputStream() << "radius " << ( vector3_length( point ) - g_SELECT_EPSILON / 2.0 - 1.0 ) << '\n';
							}
							else{
								m_rotateAxis.SetRadius( g_radius );
								//globalOutputStream() << "g_radius\n";
							}
						}
		}
		else{
			/* try lines to skew */
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					for ( int k = 0; k < 2; ++k ){
						SelectionIntersection best;
						Line_BestPoint( local2view, m_lines[i][j][k].m_line, best );
						selector.addSelectable( best, &m_selectables[i][j][k] );
					}

			if( !selector.failed() ) {
				( *selector.begin() ).second->setSelected( true );
				m_skew.set0( vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( view.GetViewMatrix() ), Vector4( 0, 0, selector.begin()->first.depth(), 1 ) ) ) );
				if( !m_pivotIsCustom )
					for ( int i = 0; i < 3; ++i )
						for ( int j = 0; j < 2; ++j )
							for ( int k = 0; k < 2; ++k )
								if( m_selectables[i][j][k].isSelected() ){
									const int axis_by = ( i + j + 1 ) % 3;
									Vector3 origin = m_bounds.origin;
									origin[axis_by] += k? -m_bounds.extents[axis_by] : m_bounds.extents[axis_by];
									m_pivot2world = matrix4_translation_for_vec3( origin );
								}
			}
			else{ /* try bbox to translate */
				SelectionIntersection best;
				AABB_BestPoint( local2view, EClipCull::CW, AABB( Vector3( 0, 0, 0 ), Vector3( 1, 1, 1 ) ), best );
				selector.addSelectable( best, &m_selectable_translateFree );
				if( !selector.failed() )
					m_translateFreeXY_Z.set0( vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( view.GetViewMatrix() ), Vector4( 0, 0, selector.begin()->first.depth(), 1 ) ) ) );
			}
		}
testSelectBboxPlanes:
		/* try bbox planes to scale */
		if( selector.failed() ){
			SelectionVolume test( view );
			test.BeginMesh( g_matrix4_identity, true );

			if( g_modifiers == c_modifierAlt ){
				PlaneSelectable::BestPlaneData planeData;
				m_selectables_scale.bestPlaneDirect( m_bounds_draw, test, planeData );
				if( !planeData.valid() ){
					m_selectables_scale.bestPlaneIndirect( m_bounds_draw, test, planeData );
				}
				if( planeData.valid() ){
					m_selectables_scale.selectByPlane( m_bounds_draw, planeData.m_plane );
				}
			}
			else{
				m_selectables_scale.selectPlanes( m_bounds_draw, selector, test, {} );
				for( auto& [ intersection, selectable ] : selector )
					selectable->setSelected( true );
			}

			std::uintptr_t newsel = 0;
			Vector3 origin = m_bounds.origin;
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					if( m_selectables_scale.getSelectables()[i * 2 + j].isSelected() ){
						origin[i] += j? m_bounds.extents[i] : -m_bounds.extents[i];
						newsel += reinterpret_cast<std::uintptr_t>( &m_selectables_scale.getSelectables()[i * 2 + j] ); // hack: store up to 2 pointers in one
					}
			if( !m_pivotIsCustom )
				m_pivot2world = matrix4_translation_for_vec3( origin );
			return selectionChange( reinterpret_cast<const ObservedSelectable *>( newsel ) );
		}

		selectionChange( selector );
	}

	Manipulatable* GetManipulatable() override {
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					if( m_selectables[i][j][k].isSelected() ){
						m_skew.SetAxes( i, ( i + j + 1 ) % 3, k? 1 : -1 );
						return &m_skew;
					}
					else if( m_selectables_rotate[i][j][k].isSelected() ){
						m_rotateAxis.SetAxis( g_vector3_axes[i] );
						return &m_rotateAxis;
					}
		{
			Vector3 axes[2] = { g_vector3_identity, g_vector3_identity };
			Vector3* axis = axes;
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					if( m_selectables_scale.getSelectables()[i * 2 + j].isSelected() )
						( *axis++ )[i] = j? -1 : 1;
			if( axis - axes == 2 ){
				m_scaleFree.SetAxes( axes[0], axes[1] );
				return &m_scaleFree;
			}
			else if( axis - axes == 1 ){
				m_scaleAxis.SetAxis( axes[0] );
				return &m_scaleAxis;
			}
		}
		return &m_translateFreeXY_Z;
	}

	void setSelected( bool select ) override {
		m_selectable_translateFree.setSelected( select );
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k ){
					m_selectables[i][j][k].setSelected( select );
					m_selectables_rotate[i][j][k].setSelected( select );
				}
		m_selectables_scale.setSelected( select );
	}
	bool isSelected() const override {
		bool selected = false;
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k ){
					selected |= m_selectables[i][j][k].isSelected();
					selected |= m_selectables_rotate[i][j][k].isSelected();
				}
		selected |= m_selectables_scale.isSelected();
		return selected | m_selectable_translateFree.isSelected();
	}
};


SkewManipulator* New_SkewManipulator( Skewable& skewable, Translatable& translatable, Scalable& scalable, Rotatable& rotatable,
		AllTransformable& transformable, const AABB& bounds, Matrix4& pivot2world, const bool& pivotIsCustom, const std::size_t segments /*= 2*/ ){
	return new SkewManipulatorImpl( skewable, translatable, scalable, rotatable, transformable, bounds, pivot2world, pivotIsCustom, segments );
}

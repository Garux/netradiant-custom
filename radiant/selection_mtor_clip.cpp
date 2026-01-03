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

#include "selection_mtor_clip.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtable_translate.h"
#include "selectionlib.h"
#include "clippertool.h"
#include "grid.h"
#include "brush.h"

class ClipManipulatorImpl final : public ClipManipulator, public ManipulatorSelectionChangeable, public Translatable, public AllTransformable, public Manipulatable
{
	struct ClipperPoint : public OpenGLRenderable, public SelectableBool
	{
		PointVertex m_p; //for render
		ClipperPoint():
			m_p( vertex3f_identity ), m_set( false ) {
		}
		void render( RenderStateFlags state ) const override {
			gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_p.colour );
			gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_p.vertex );
			gl().glDrawArrays( GL_POINTS, 0, 1 );

			gl().glColor4ub( m_p.colour.r, m_p.colour.g, m_p.colour.b, m_p.colour.a ); ///?
			gl().glRasterPos3f( m_namePos.x(), m_namePos.y(), m_namePos.z() );
			GlobalOpenGL().drawChar( m_name );
		}
		void setColour( const Colour4b& colour ) {
			m_p.colour = colour;
		}
		bool m_set;
		DoubleVector3 m_point;
		DoubleVector3 m_pointNonTransformed;
		char m_name;
		Vector3 m_namePos;
	};
	Matrix4& m_pivot2world;
	ClipperPoint m_points[3];
	TranslateFreeXY_Z m_dragXY_Z;
	const AABB& m_bounds;
	Vector3 m_viewdir;
public:
	ClipManipulatorImpl( Matrix4& pivot2world, const AABB& bounds ) : m_pivot2world( pivot2world ), m_dragXY_Z( *this, *this ), m_bounds( bounds ){
		m_points[0].m_name = '1';
		m_points[1].m_name = '2';
		m_points[2].m_name = '3';
	}

	void UpdateColours() {
		for( std::size_t i = 0; i < 3; ++i )
			m_points[i].setColour( colourSelected( g_colour_screen, m_points[i].isSelected() ) );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		// temp hack
		UpdateColours();

		renderer.SetState( m_state, Renderer::eWireframeOnly );
		renderer.SetState( m_state, Renderer::eFullMaterials );

		const Matrix4 proj( matrix4_multiplied_by_matrix4( volume.GetViewport(), volume.GetViewMatrix() ) );
		const Matrix4 proj_inv( matrix4_full_inverse( proj ) );
		for( std::size_t i = 0; i < 3; ++i )
			if( m_points[i].m_set ){
				m_points[i].m_p.vertex = vertex3f_for_vector3( m_points[i].m_point );
				renderer.addRenderable( m_points[i], g_matrix4_identity );
				const Vector3 pos = vector4_projected( matrix4_transformed_vector4( proj, Vector4( m_points[i].m_point, 1 ) ) ) + Vector3( 2, 0, 0 );
				m_points[i].m_namePos = vector4_projected( matrix4_transformed_vector4( proj_inv, Vector4( pos, 1 ) ) );
			}
	}
	/* these three functions and m_viewdir for 2 points only */
	void viewdir_set( const Vector3 viewdir ){
		const std::size_t maxi = vector3_max_abs_component_index( viewdir );
		m_viewdir = ( viewdir[maxi] > 0 )? g_vector3_axes[maxi] : -g_vector3_axes[maxi];
	}
	void viewdir_fixup(){
		if( std::fabs( vector3_length( m_points[1].m_point - m_points[0].m_point ) ) > 1e-3 //two non coincident points
		 && std::fabs( vector3_dot( m_viewdir, vector3_normalised( m_points[1].m_point - m_points[0].m_point ) ) ) > 0.999 ){ //on axis = m_viewdir
			viewdir_set( m_view->getViewDir() );
			if( std::fabs( vector3_dot( m_viewdir, vector3_normalised( m_points[1].m_point - m_points[0].m_point ) ) ) > 0.999 ){
				const Matrix4 screen2world( matrix4_full_inverse( m_view->GetViewMatrix() ) );
				Vector3 p[2];
				for( std::size_t i = 0; i < 2; ++i ){
					p[i] = vector4_projected( matrix4_transformed_vector4( m_view->GetViewMatrix(), Vector4( m_points[i].m_point, 1 ) ) );
				}
				const float depthdir = p[1].z() > p[0].z()? -1 : 1;
				for( std::size_t i = 0; i < 2; ++i ){
					p[i].z() = -1;
					p[i] = vector4_projected( matrix4_transformed_vector4( screen2world, Vector4( p[i], 1 ) ) );
				}
				viewdir_set( ( p[1] - p[0] ) * depthdir );
			}
		}
	}
	void viewdir_make_cut_worthy( const Plane3& plane ){
		const std::size_t maxi = vector3_max_abs_component_index( plane.normal() );
		if( plane3_valid( plane )
		 && aabb_valid( m_bounds )
		 && std::fabs( plane.normal()[maxi] ) > 0.999 ){ //axial plane
			const double anchor = plane.normal()[maxi] * plane.dist();
			if( anchor > m_bounds.origin[maxi] ){
				if( ( anchor - ( m_bounds.origin[maxi] + m_bounds.extents[maxi] ) ) > -0.1 )
					viewdir_set( -g_vector3_axes[maxi] );
			}
			else{
				if( ( -anchor + ( m_bounds.origin[maxi] - m_bounds.extents[maxi] ) ) > -0.1 )
					viewdir_set( g_vector3_axes[maxi] );
			}
		}
	}
	void updatePlane(){
		std::size_t npoints = 0;
		for(; npoints < 3; )
			if( m_points[npoints].m_set )
				++npoints;
			else
				break;

		switch ( npoints )
		{
		case 1:
			Clipper_setPlanePoints( ClipperPoints( m_points[0].m_point, m_points[0].m_point, m_points[0].m_point, npoints ) );
			break;
		case 2:
			{
				if( m_view->fill() ){ //3d
					viewdir_fixup();
					m_points[2].m_point = m_points[0].m_point - m_viewdir * vector3_length( m_points[0].m_point - m_points[1].m_point );
					viewdir_make_cut_worthy( plane3_for_points( m_points[0].m_point, m_points[1].m_point, m_points[2].m_point ) );
				}
				m_points[2].m_point = m_points[0].m_point - m_viewdir * vector3_length( m_points[0].m_point - m_points[1].m_point );
			} // fall through
		case 3:
			Clipper_setPlanePoints( ClipperPoints( m_points[0].m_point, m_points[1].m_point, m_points[2].m_point, npoints ) );
			break;

		default:
			Clipper_setPlanePoints( ClipperPoints() );
			break;
		}
	}
	std::size_t newPointIndex( bool viewfill ) const {
		const std::size_t maxi = ( !viewfill && Clipper_get2pointsIn2d() )? 2 : 3;
		std::size_t i;
		for( i = 0; i < maxi; ++i )
			if( !m_points[i].m_set )
				break;
		return i % maxi;
	}
	void newPoint( const DoubleVector3& point, const View& view ){
		const std::size_t i = newPointIndex( view.fill() );
		if( i == 0 )
			m_points[1].m_set = m_points[2].m_set = false;
		m_points[i].m_set = true;
		m_points[i].m_point = point;

		SelectionPool selector;
		selector.addSelectable( SelectionIntersection( 0, 0 ), &m_points[i] );
		selectionChange( selector );

		if( i == 1 )
			viewdir_set( m_view->getViewDir() );

		updatePlane();
	}
	bool testSelect_scene( const View& view, DoubleVector3& point ) const {
		SelectionVolume test( view );
		ScenePointSelector selector;
		Scene_forEachVisible_testselect_scene_point( view, selector, test );
		test.BeginMesh( g_matrix4_identity, true );
		if( selector.isSelected() ){
			point = testSelected_scene_snapped_point( test, selector );
			return true;
		}
		return false;
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		if( g_modifiers != c_modifierNone && !quickCondition( g_modifiers, view ) )
			return selectionChange( nullptr );

		testSelect_points( view );
		if( !isSelected() ){
			if( view.fill() ){
				DoubleVector3 point;
				if( testSelect_scene( view, point ) )
					newPoint( point, view );
			}
			else{
				DoubleVector3 point = vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( view.GetViewMatrix() ), Vector4( 0, 0, 0, 1 ) ) );
				vector3_snap( point, GetSnapGridSize() );
				{
					const std::size_t maxi = vector3_max_abs_component_index( view.getViewDir() );
					const std::size_t i = newPointIndex( false );
					point[maxi] = m_bounds.origin[maxi] + ( i == 2? -1 : 1 ) * m_bounds.extents[maxi];
				}
				newPoint( point, view );
			}
		}
		for( std::size_t i = 0; i < 3; ++i )
			if( m_points[i].isSelected() ){
				m_points[i].m_pointNonTransformed = m_points[i].m_point;
				m_pivot2world = matrix4_translation_for_vec3( m_points[i].m_pointNonTransformed );
				break;
			}
	}
	void highlight( const View& view, const Matrix4& pivot2world ) override {
		testSelect_points( view );
	}
	void testSelect_points( const View& view ){
		if( g_modifiers != c_modifierNone && !quickCondition( g_modifiers, view ) )
			return selectionChange( nullptr );

		SelectionPool selector;
		{
			const Matrix4 local2view( view.GetViewMatrix() );

			for( std::size_t i = 0; i < 3; ++i ){
				if( m_points[i].m_set ){
					SelectionIntersection best;
					Point_BestPoint( local2view, PointVertex( vertex3f_for_vector3( m_points[i].m_point ) ), best );
					selector.addSelectable( best, &m_points[i] );
				}
			}
		}
		selectionChange( selector );
	}
	void reset( bool initFromFace ) override {
		for( std::size_t i = 0; i < 3; ++i ){
			m_points[i].m_set = false;
			m_points[i].setSelected( false ); ///?
		}
		if( initFromFace && !g_SelectedFaceInstances.empty() && g_SelectedFaceInstances.last().getFace().contributes() ){
			const Winding& w = g_SelectedFaceInstances.last().getFace().getWinding();
			for( std::size_t i = 0; i < 3; ++i ){
				m_points[i].m_set = true;
				m_points[i].m_point = w[i].vertex;
			}
		}
		updatePlane();
	}
	/* Translatable */
	void translate( const Vector3& translation ) override { //in 2d and ( 3d + m_dragXY_Z )
		for( std::size_t i = 0; i < 3; ++i )
			if( m_points[i].isSelected() ){
				m_points[i].m_point = m_points[i].m_pointNonTransformed + translation;
				updatePlane();
				break;
			}
	}
	/* AllTransformable */
	void alltransform( const Transforms& transforms, const Vector3& world_pivot ) override {
		ERROR_MESSAGE( "unreachable" );
	}
	/* Manipulatable */
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_dragXY_Z.set0( transform_origin );
		m_dragXY_Z.Construct( device2manip, device_point, AABB( transform_origin, g_vector3_identity ), transform_origin );
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		// any 2D or 3D with modifiers besides SnapBounds
		if( !( g_modifiers == c_modifierNone && m_view->fill() ) && !SnapBounds::useCondition( g_modifiers, *m_view ) )
			return m_dragXY_Z.Transform( manip2object, device2manip, device_point );

		View scissored( *m_view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, m_device_epsilon ) );

		DoubleVector3 point;
		if( testSelect_scene( scissored, point ) )
			for( std::size_t i = 0; i < 3; ++i )
				if( m_points[i].isSelected() ){
					m_points[i].m_point = point;
					updatePlane();
					break;
				}
	}

	Manipulatable* GetManipulatable() override {
		return this;
	}

	void setSelected( bool select ) override {
		for( std::size_t i = 0; i < 3; ++i )
			m_points[i].setSelected( select );
	}
	bool isSelected() const override {
		return m_points[0].isSelected() || m_points[1].isSelected() || m_points[2].isSelected();
	}
};


ClipManipulator* New_ClipManipulator( Matrix4& pivot2world, const AABB& bounds ){
	return new ClipManipulatorImpl( pivot2world, bounds );
}

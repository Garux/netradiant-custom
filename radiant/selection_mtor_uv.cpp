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

#include "selection_mtor_uv.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "brush.h"
#include "patch.h"
#include "iglrender.h"

class UVManipulatorImpl final : public UVManipulator, public Manipulatable
{
	struct RenderablePoints : public OpenGLRenderable
	{
		std::vector<PointVertex> m_points;
		void render( RenderStateFlags state ) const override {
			gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_points[0].colour );
			gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_points[0].vertex );
			gl().glDrawArrays( GL_POINTS, 0, m_points.size() );
		}
	};
	struct RenderableLines : public OpenGLRenderable
	{
		std::vector<PointVertex> m_lines;

		void render( RenderStateFlags state ) const override {
			if( m_lines.size() != 0 ){
				gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_lines[0].colour );
				gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_lines[0].vertex );
				gl().glDrawArrays( GL_LINES, 0, m_lines.size() );
			}
		}
	};
	typedef Array<PatchControl> PatchControlArray;
	struct RenderablePatchTexture : public OpenGLRenderable
	{
		std::vector<RenderIndex> m_trianglesIndices;
		const PatchControlArray* m_patchControlArray;

		void render( RenderStateFlags state ) const override {
			if( state & RENDER_FILL ){
				const std::vector<Vector3> normals( m_patchControlArray->size(), g_vector3_axis_z );
				gl().glNormalPointer( GL_FLOAT, sizeof( Vector3 ), normals.data() );
				gl().glVertexPointer( 2, GL_FLOAT, sizeof( PatchControl ), &m_patchControlArray->data()->m_texcoord );
				gl().glTexCoordPointer( 2, GL_FLOAT, sizeof( PatchControl ), &m_patchControlArray->data()->m_texcoord );
				gl().glDrawElements( GL_TRIANGLES, GLsizei( m_trianglesIndices.size() ), RenderIndexTypeID, m_trianglesIndices.data() );
			}
		}
	};
	const Colour4b m_cWhite { 255, 255, 255, 255 };
	const Colour4b m_cGray  { 255, 255, 255, 125 };
	const Colour4b m_cGrayer{ 100, 100, 100, 150 };
	const Colour4b m_cRed   { 255,   0,   0, 255 };
	const Colour4b m_cGreen {   0, 255,   0, 255 };
	const Colour4b m_cGree  {   0, 150,   0, 255 };
	const Colour4b m_cPink  { 255,   0, 255, 255 };
	const Colour4b m_cPin   { 150,   0, 150, 255 };
	const Colour4b m_cOrange{ 255, 125,   0, 255 };
	const Colour4b m_cOrang { 255, 125,   0, 125 };

	enum EUVSelection{
		eNone,
		ePivot,
		eGridU,
		eGridV,
		ePatchPoint,
		ePatchRow,
		ePatchColumn,
		eCircle,
		ePivotU,
		ePivotV,
		eU,
		eV,
		eUV,
		eSkewU,
		eSkewV,
		eTex,
	} m_selection;
	PointVertex* m_selectedU = 0; // must nullify this on m_Ulines, m_Vlines change
	PointVertex* m_selectedV = 0;
	int m_selectedPatchIndex = -1;
	bool m_isSelected = false;

	class UVSelector : public Selector {
		SelectionIntersection m_bestIntersection;
	public:
		EUVSelection m_selection = eNone;
		int m_index = -1;
		UVSelector() : m_bestIntersection( SelectionIntersection() ) {
		}
		void pushSelectable( Selectable& selectable ) override {
		}
		void popSelectable() override {
			m_bestIntersection = SelectionIntersection();
		}
		void addIntersection( const SelectionIntersection& intersection ) override {
			if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
				m_bestIntersection = intersection;
			}
		}
		void addIntersection( const SelectionIntersection& intersection, EUVSelection selection, int index ) {
			if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
				m_bestIntersection = intersection;
				m_selection = selection;
				m_index = index;
			}
		}
		void addIntersection( const SelectionIntersection& intersection, EUVSelection selection ) {
			if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
				m_bestIntersection = intersection;
				m_selection = selection;
			}
		}
		bool isSelected() {
			return m_bestIntersection.valid();
		}
	};

	Face* m_face = 0;
	Plane3 m_plane;
	std::size_t m_width, m_height;
	TextureProjection m_projection;

	Matrix4 m_local2tex; //real projection
	Matrix4 m_tex2local; //real unprojection aka projection space basis aka texture axes
	Matrix4 m_faceLocal2tex; //x,y projected to the face for z = const
	Matrix4 m_faceTex2local;
	Vector3 m_origin;

	RenderablePivot m_pivot;
	Matrix4 m_pivot2world0; // original
	Matrix4 m_pivot2world; // transformed during transformation
	RenderablePoint m_pivotPoint;
	RenderableLines m_pivotLines;
	Matrix4 m_pivotLines2world;
	/* lines in uv space */
	RenderableLines m_Ulines;
	RenderableLines m_Vlines;
	Matrix4 m_lines2world; // line * ( transform during transformation ) * m_faceTex2local = world

	unsigned int m_gridU = 1; // n - 1 of U directed sub lines, 1-16
	unsigned int m_gridV = 1;
	RenderablePoint m_gridPointU; // control of U grid lines density, rendered on V axis
	RenderablePoint m_gridPointV;
	Vector2 m_gridSign; // orientation of controls relative to origin

	RenderableCircle m_circle;
	Matrix4 m_circle2world;

	Patch* m_patch = 0; //tracking face/patch mode by only nonzero pointer
	std::size_t m_patchWidth;
	std::size_t m_patchHeight;
	PatchControlArray m_patchCtrl;
	RenderablePoints m_patchRenderPoints;
	RenderableLines m_patchRenderLattice;
	RenderablePatchTexture m_patchRenderTex;
	const Shader* m_state_patch_raw = 0; // original patch texture shader
	Shader* m_state_patch = 0; // local patch texture overlay
	const char* m_state_patch_name = "$uvtool/patchtexture";

public:
	UVManipulatorImpl() : m_pivot( 32 ), m_circle( 8 << 3 ) {
		draw_circle( 8, 1, m_circle.m_vertices.data(), RemapXYZ() );
		m_circle.setColour( m_cGray );
		m_pivotPoint.setColour( m_cWhite );
		m_gridPointU.setColour( m_cWhite );
		m_gridPointV.setColour( m_cWhite );
		m_pivotLines.m_lines.resize( 4, PointVertex( vertex3f_identity, m_cWhite ) );
	}
	~UVManipulatorImpl() {
		patchShaderDestroy();
	}

private:
	void patchShaderConstruct(){
		patchShaderDestroy();

		OpenGLState state;
		GlobalOpenGLStateLibrary().getDefaultState( state );
		state.m_state = RENDER_FILL /*| RENDER_CULLFACE*/ | RENDER_TEXTURE | RENDER_COLOURWRITE | RENDER_LIGHTING | RENDER_SMOOTH;
		state.m_sort = OpenGLState::eSortOverlayLast;
		state.m_texture = m_patch->getShader()->getTexture().texture_number;

		GlobalOpenGLStateLibrary().insert( m_state_patch_name, state );
		m_state_patch = GlobalShaderCache().capture( m_state_patch_name );
	}

	void patchShaderDestroy(){
		if( m_state_patch ){
			m_state_patch = 0;
			GlobalShaderCache().release( m_state_patch_name );
			GlobalOpenGLStateLibrary().erase( m_state_patch_name );
		}
	}
	bool patchCtrl_isInside( std::size_t i ) const {
		return ( i % 2 || ( i / m_patchWidth ) % 2 );
	}
	template<typename Functor>
	void forEachEdge( const Functor& functor ) const {
		if( m_face ){
			const Winding& winding = m_face->getWinding();
			for( Winding::const_iterator next = winding.begin(), i = winding.end() - 1; next != winding.end(); i = next, ++next )
				functor( ( *i ).vertex, ( *next ).vertex );
		}
		else if( m_patch ){
			for( std::vector<PointVertex>::const_iterator i = m_patchRenderLattice.m_lines.begin(); i != m_patchRenderLattice.m_lines.end(); ++++i ){
				const Vector3 p0( matrix4_transformed_point( m_faceTex2local, ( *i ).vertex ) );
				const Vector3 p1( matrix4_transformed_point( m_faceTex2local, ( *( i + 1 ) ).vertex ) );
				if( vector3_length_squared( p1 - p0 ) > 0.1 )
					functor( p0, p1 );
			}
		}
	}
	template<typename Functor>
	void forEachPoint( const Functor& functor ) const {
		if( m_face ){
			const Winding& winding = m_face->getWinding();
			for( const auto& v : winding )
				functor( v.vertex );
		}
		else if( m_patch ){
			for( const auto& v : m_patchCtrl )
				functor( matrix4_transformed_point( m_faceTex2local, Vector3( v.m_texcoord, 0 ) ) );
		}
	}
	template<typename Functor>
	void forEachUVPoint( const Functor& functor ) const {
		if( m_face ){
			const Winding& winding = m_face->getWinding();
			for( const auto& v : winding )
				functor( matrix4_transformed_point( m_faceLocal2tex, v.vertex ) );
		}
		else if( m_patch ){
			for( const auto& v : m_patchCtrl )
				functor( Vector3( v.m_texcoord, 0 ) );
		}
	}
	bool projection_valid() const {
		return !( !std::isfinite( m_local2tex[0] ) //nan
		       || !std::isfinite( m_tex2local[0] ) //nan
		       || std::fabs( vector3_dot( m_plane.normal(), m_tex2local.z().vec3() ) ) < 1e-6 //projected along face
		       || vector3_length_squared( m_tex2local.x().vec3() ) < .01 //srsly scaled down, limit at max 10 textures per world unit
		       || vector3_length_squared( m_tex2local.y().vec3() ) < .01
		       || vector3_length_squared( m_tex2local.x().vec3() ) > 1e9 //very upscaled or product of nearly nan
		       || vector3_length_squared( m_tex2local.y().vec3() ) > 1e9 );
	}
	void UpdateFaceData( bool updateOrigin, bool updateLines = true ) {
		//!? todo fewer outer quads for large textures
		//!? todo auto subdivisions num, based on tex size and world scale
		//! todo update on undo/redo, when face stays the same, but transformed
		//! todo update on nudgeSelectedLeft and the rest, qe tool move w/o projection change or with tex lock off
		//+ todo put default origin to winding's UV aabb corner
		//+ todo disable 3d workzone in this manipulator mode
		if( m_face ){
			m_plane = m_face->getPlane().plane3();
			m_width = m_face->getShader().width();
			m_height = m_face->getShader().height();
//			m_face->GetTexdef( m_projection );
			m_projection = m_face->getTexdef().m_projection;

			Texdef_Construct_local2tex( m_projection, m_width, m_height, m_plane.normal(), m_local2tex );
			m_tex2local = matrix4_affine_inverse( m_local2tex );
		}
		else if( m_patch ){
			m_plane.normal() = m_patch->Calculate_AvgNormal();
			m_plane.dist() = vector3_dot( m_plane.normal(), m_patch->localAABB().origin );
			m_patchWidth = m_patch->getWidth();
			m_patchHeight = m_patch->getHeight();
			m_patchCtrl = m_patch->getControlPoints();
			m_state_patch_raw = m_patch->getShader();
			patchShaderConstruct();
			{	//! todo force or deduce orthogonal uv axes for convenience
				Vector3 wDir, hDir;
				m_patch->Calculate_AvgAxes( wDir, hDir );
				vector3_normalise( wDir );
				vector3_normalise( hDir );
//					globalOutputStream() << wDir << " wDir\n";
//					globalOutputStream() << hDir << " hDir\n";
//					globalOutputStream() << m_plane.normal() << " m_plane.normal()\n";

				/* find longest row and column */
				float wLength = 0, hLength = 0; //!? todo break, if some of these is 0
				std::size_t row = 0, col = 0;
				for ( std::size_t r = 0; r < m_patchHeight; ++r ){
					float length = 0;
					for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
						length += vector3_length( m_patch->ctrlAt( r, c + 1 ).m_vertex - m_patch->ctrlAt( r, c ).m_vertex );
					}
					if( length - wLength > .1f || ( ( r == 0 || r == m_patchHeight - 1 ) && float_equal_epsilon( length, wLength, .1f ) ) ){ // prioritize first and last rows
						wLength = length;
						row = r;
					}
				}
				for ( std::size_t c = 0; c < m_patchWidth; ++c ){
					float length = 0;
					for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
						length += vector3_length( m_patch->ctrlAt( r + 1, c ).m_vertex - m_patch->ctrlAt( r, c ).m_vertex );
					}
					if( length - hLength > .1f || ( ( c == 0 || c == m_patchWidth - 1 ) && float_equal_epsilon( length, hLength, .1f ) ) ){
						hLength = length;
						col = c;
					}
				}
				//! todo handle case, when uv start = end, like projection to cylinder
				//! todo consider max uv length to have manipulator size according to patch size
				/* pick 3 points at the found row and column */
				const PatchControl* p0, *p1, *p2;
				Vector3 v0, v1, v2;
				{
					float distW0 = 0, distW1 = 0;
					for ( std::size_t c = 0; c < col; ++c ){
						distW0 += vector3_length( m_patch->ctrlAt( row, c + 1 ).m_vertex - m_patch->ctrlAt( row, c ).m_vertex );
					}
					for ( std::size_t c = col; c < m_patchWidth - 1; ++c ){
						distW1 += vector3_length( m_patch->ctrlAt( row, c + 1 ).m_vertex - m_patch->ctrlAt( row, c ).m_vertex );
					}
					float distH0 = 0, distH1 = 0;
					for ( std::size_t r = 0; r < row; ++r ){
						distH0 += vector3_length( m_patch->ctrlAt( r + 1, col ).m_vertex - m_patch->ctrlAt( r, col ).m_vertex );
					}
					for ( std::size_t r = row; r < m_patchHeight - 1; ++r ){
						distH1 += vector3_length( m_patch->ctrlAt( r + 1, col ).m_vertex - m_patch->ctrlAt( r, col ).m_vertex );
					}

					if( ( distW0 > distH0 && distW0 > distH1 ) || ( distW1 > distH0 && distW1 > distH1 ) ){
						p0 = &m_patch->ctrlAt( 0, col );
						p1 = &m_patch->ctrlAt( m_patchHeight - 1, col );
						p2 = distW0 > distW1? &m_patch->ctrlAt( row, 0 ) : &m_patch->ctrlAt( row, m_patchWidth - 1 );
						v0 = m_patch->localAABB().origin
						     + hDir * vector3_dot( m_patch->localAABB().extents, Vector3( std::fabs( hDir.x() ), std::fabs( hDir.y() ), std::fabs( hDir.z() ) ) ) * 1.1
						     + wDir * ( distW0 - wLength / 2 );
						v1 = v0 + hDir * hLength;
						v2 = v0 + hDir * distH0 + ( distW0 > distW1? ( wDir * -distW0 ) : ( wDir * distW1 ) );
					}
					else{
						p0 = &m_patch->ctrlAt( row, 0 );
						p1 = &m_patch->ctrlAt( row, m_patchWidth - 1 );
						p2 = distH0 > distH1? &m_patch->ctrlAt( 0, col ) : &m_patch->ctrlAt( m_patchHeight - 1, col );
						v0 = m_patch->localAABB().origin
						     + wDir * vector3_dot( m_patch->localAABB().extents, Vector3( std::fabs( wDir.x() ), std::fabs( wDir.y() ), std::fabs( wDir.z() ) ) ) * 1.1
						     + hDir * ( distH0 - hLength / 2 );
						v1 = v0 + wDir * wLength;
						v2 = v0 + wDir * distW0 + ( distH0 > distH1? ( hDir * -distH0 ) : ( hDir * distH1 ) );
					}

					if( vector3_dot( plane3_for_points( v0, v1, v2 ).normal(), m_plane.normal() ) < 0 ){
						std::swap( p0, p1 );
						std::swap( v0, v1 );
					}
				}
				const PlanePoints vertices{ v0, v1, v2 };
				const DoubleVector3 sts[3]{ DoubleVector3( p0->m_texcoord, 0 ),
				                            DoubleVector3( p1->m_texcoord, 0 ),
				                            DoubleVector3( p2->m_texcoord, 0 ) };
				Texdef_Construct_local2tex_from_ST( vertices, sts, m_local2tex );
				m_tex2local = matrix4_affine_inverse( m_local2tex );
			}
		}

//		globalOutputStream() << m_local2tex << " m_local2tex\n";
//		globalOutputStream() << m_tex2local << " m_tex2local\n";
		/* error checking */
		if( !projection_valid() ){
			m_selectedU = m_selectedV = 0;
			m_Ulines.m_lines.clear();
			m_Vlines.m_lines.clear();
			m_selectedPatchIndex = -1;
			return;
		}

		m_faceTex2local = m_tex2local;
		m_faceTex2local.x().vec3() = plane3_project_point( Plane3( m_plane.normal(), 0 ), m_tex2local.x().vec3(), m_tex2local.z().vec3() );
		m_faceTex2local.y().vec3() = plane3_project_point( Plane3( m_plane.normal(), 0 ), m_tex2local.y().vec3(), m_tex2local.z().vec3() );
		m_faceTex2local = matrix4_multiplied_by_matrix4( // adjust to have UV's z = 0: move the plane along m_tex2local.z() so that plane.dist() = 0
		                      matrix4_translation_for_vec3(
		                          m_tex2local.z().vec3() * ( m_plane.dist() - vector3_dot( m_plane.normal(), m_tex2local.t().vec3() ) )
		                          / vector3_dot( m_plane.normal(), m_tex2local.z().vec3() )
		                      ),
		                      m_faceTex2local );
		m_faceLocal2tex = matrix4_affine_inverse( m_faceTex2local );

		if( m_patch ){
			m_patchRenderPoints.m_points.clear();
			m_patchRenderPoints.m_points.reserve( m_patchWidth * m_patchHeight );
			for( std::size_t i = 0; i < m_patchCtrl.size(); ++i ){
				m_patchRenderPoints.m_points.emplace_back( vertex3f_for_vector3( Vector3( m_patchCtrl[i].m_texcoord, 0 ) ), patchCtrl_isInside( i )? m_cPin : m_cGree );
			}

			m_patchRenderLattice.m_lines.clear();
			m_patchRenderLattice.m_lines.reserve( ( ( m_patchWidth - 1 ) * m_patchHeight + ( m_patchHeight - 1 ) * m_patchWidth ) * 2 );
			for ( std::size_t r = 0; r < m_patchHeight; ++r ){
				for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
					const Vector2& a = m_patch->ctrlAt( r, c ).m_texcoord;
					const Vector2& b = m_patch->ctrlAt( r, c + 1 ).m_texcoord;
					m_patchRenderLattice.m_lines.emplace_back( vertex3f_for_vector3( Vector3( a, 0 ) ), m_cOrang );
					m_patchRenderLattice.m_lines.emplace_back( vertex3f_for_vector3( Vector3( b, 0 ) ), m_cOrang );
				}
			}
			for ( std::size_t c = 0; c < m_patchWidth; ++c ){
				for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
					const Vector2& a = m_patch->ctrlAt( r, c ).m_texcoord;
					const Vector2& b = m_patch->ctrlAt( r + 1, c ).m_texcoord;
					m_patchRenderLattice.m_lines.emplace_back( vertex3f_for_vector3( Vector3( a, 0 ) ), m_cOrang );
					m_patchRenderLattice.m_lines.emplace_back( vertex3f_for_vector3( Vector3( b, 0 ) ), m_cOrang );
				}
			}

			m_patchRenderTex.m_trianglesIndices.clear();
			m_patchRenderTex.m_trianglesIndices.reserve( ( m_patchHeight - 1 ) * ( m_patchWidth - 1 ) * 2 * 3 );
			const PatchControlArray& pc = m_patch->getControlPointsTransformed();
			m_patchRenderTex.m_patchControlArray = &pc;
			const double degenerate_epsilon = 1e-5;
			for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
				for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
					const RenderIndex i0 = m_patchWidth * r + c;
					const RenderIndex i1 = m_patchWidth * ( r + 1 ) + c;
					const RenderIndex i2 = m_patchWidth * ( r + 1 ) + c + 1;
					const RenderIndex i3 = m_patchWidth * r + c + 1;
					double cross = vector2_cross( pc[i2].m_texcoord - pc[i0].m_texcoord, pc[i1].m_texcoord - pc[i0].m_texcoord );
					if( !float_equal_epsilon( cross, 0, degenerate_epsilon ) ){
						m_patchRenderTex.m_trianglesIndices.push_back( i0 );
						m_patchRenderTex.m_trianglesIndices.push_back( i1 );
						m_patchRenderTex.m_trianglesIndices.push_back( i2 );
						if( cross < 0 )
							std::swap( *( m_patchRenderTex.m_trianglesIndices.end() - 1 ), *( m_patchRenderTex.m_trianglesIndices.end() - 2 ) );
					}
					cross = vector2_cross( pc[i3].m_texcoord - pc[i0].m_texcoord, pc[i2].m_texcoord - pc[i0].m_texcoord );
					if( !float_equal_epsilon( cross, 0, degenerate_epsilon ) ){
						m_patchRenderTex.m_trianglesIndices.push_back( i0 );
						m_patchRenderTex.m_trianglesIndices.push_back( i2 );
						m_patchRenderTex.m_trianglesIndices.push_back( i3 );
						if( cross < 0 )
							std::swap( *( m_patchRenderTex.m_trianglesIndices.end() - 1 ), *( m_patchRenderTex.m_trianglesIndices.end() - 2 ) );
					}
				}
			}
			if( m_patchRenderTex.m_trianglesIndices.size() == 0 ){ // try to make at least one triangle or more
				RenderIndex i0 = 0, i1 = 1, i2;
				for( ; i1 < pc.size(); ++i1 ){
					if( vector2_length( pc[i1].m_texcoord - pc[i0].m_texcoord ) > degenerate_epsilon ){
						i2 = i1 + 1;
						for( ; i2 < pc.size(); ++i2 ){
							const double cross = vector2_cross( pc[i2].m_texcoord - pc[i0].m_texcoord, pc[i1].m_texcoord - pc[i0].m_texcoord );
							if( !float_equal_epsilon( cross, 0, degenerate_epsilon ) ){
								m_patchRenderTex.m_trianglesIndices.push_back( i0 );
								m_patchRenderTex.m_trianglesIndices.push_back( i1 );
								m_patchRenderTex.m_trianglesIndices.push_back( i2 );
								if( cross < 0 )
									std::swap( *( m_patchRenderTex.m_trianglesIndices.end() - 1 ), *( m_patchRenderTex.m_trianglesIndices.end() - 2 ) );
								break;
							}
						}
					}
				}
			}
		}

		Vector2 min( FLT_MAX, FLT_MAX );
		Vector2 max( -FLT_MAX, -FLT_MAX );
		forEachUVPoint( [&]( const Vector3& point ){
			min.x() = std::min( min.x(), point.x() );
			max.x() = std::max( max.x(), point.x() );
			min.y() = std::min( min.y(), point.y() );
			max.y() = std::max( max.y(), point.y() );
		} );

		if( updateOrigin )
			m_origin = matrix4_transformed_point( m_faceTex2local, Vector3( min, 0 ) );

		const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );

		{	// grid grain controls, on the polygon side of origin
			m_gridSign.x() = max.y() - uv_origin.y() >=  uv_origin.y() - min.y()? 1 : -1;
			m_gridSign.y() = max.x() - uv_origin.x() >=  uv_origin.x() - min.x()? 1 : -1;
			m_gridPointU.m_point.vertex = Vertex3f( uv_origin.x(),
			                                        float_to_integer( uv_origin.y() + m_gridSign.x() * .25 ) + m_gridSign.x() * ( 1 - 1.0 / std::max( float( m_gridU ), 1.8f ) ),
			                                        0 );
			m_gridPointV.m_point.vertex = Vertex3f( float_to_integer( uv_origin.x() + m_gridSign.y() * .25 ) + m_gridSign.y() * ( 1 - 1.0 / std::max( float( m_gridV ), 1.8f ) ),
			                                        uv_origin.y(),
			                                        0 );
		}

		m_pivot2world = m_tex2local;
		vector3_normalise( m_pivot2world.x().vec3() );
		vector3_normalise( m_pivot2world.y().vec3() );
		m_pivot2world.t().vec3() = m_origin;
		m_pivot2world0 = m_pivot2world;

		{
			float bestDist = 0;
			forEachPoint( [&]( const Vector3& point ){
				const float dist = vector3_length_squared( point - m_origin );
				if( dist > bestDist ){
					bestDist = dist;
				}
			} );
			bestDist = sqrt( bestDist );
			m_circle2world = g_matrix4_identity;
			ComputeAxisBase( m_plane.normal(), m_circle2world.x().vec3(), m_circle2world.y().vec3() );
			m_circle2world.x().vec3() *= bestDist;
			m_circle2world.y().vec3() *= bestDist;
			m_circle2world.z().vec3() = m_plane.normal();
			m_circle2world.t().vec3() = m_origin;
		}

		min -= Vector2( 5, 5 );
		max += Vector2( 5, 5 );
		min.x() = float_to_integer( min.x() );
		min.y() = float_to_integer( min.y() );
		max.x() = float_to_integer( max.x() );
		max.y() = float_to_integer( max.y() );

		m_selectedU = m_selectedV = 0;
		m_selectedPatchIndex = -1;
		m_lines2world = m_faceTex2local;
		m_pivotLines2world = m_faceTex2local;
		if( updateLines ){
			const int imax = float_to_integer( max.y() - min.y() ) + 1;
			m_Ulines.m_lines.clear();
			m_Ulines.m_lines.reserve( ( imax + ( m_gridU - 1 ) * ( imax - 1 ) ) * 2 );
			for( int i = 0; i < imax; ++i ){
				if( i != 0 ){
					for( std::size_t j = m_gridU - 1; j != 0; --j ){ //subgrid lines
						m_Ulines.m_lines.emplace_back( Vertex3f( min.x(), min.y() + i - static_cast<float>( j ) / m_gridU, 0 ), m_cGrayer );
						m_Ulines.m_lines.emplace_back( Vertex3f( max.x(), min.y() + i - static_cast<float>( j ) / m_gridU, 0 ), m_cGrayer );
					}
				}
				m_Ulines.m_lines.emplace_back( Vertex3f( min.x(), min.y() + i, 0 ), m_cGray );
				m_Ulines.m_lines.emplace_back( Vertex3f( max.x(), min.y() + i, 0 ), m_cGray );
			}
		}
		if( updateLines ){
			const int imax = float_to_integer( max.x() - min.x() ) + 1;
			m_Vlines.m_lines.clear();
			m_Vlines.m_lines.reserve( ( imax + ( m_gridV - 1 ) * ( imax - 1 ) ) * 2 );
			for( int i = 0; i < imax; ++i ){
				if( i != 0 ){
					for( std::size_t j = m_gridV - 1; j != 0; --j ){
						m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i - static_cast<float>( j ) / m_gridV, min.y(), 0 ), m_cGrayer );
						m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i - static_cast<float>( j ) / m_gridV, max.y(), 0 ), m_cGrayer );
					}
				}
				m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i, min.y(), 0 ), m_cGray );
				m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i, max.y(), 0 ), m_cGray );
			}
		}
		{
			{	// u pivot line
				m_pivotLines.m_lines[0].vertex = Vertex3f( min.x(), uv_origin.y(), 0 );
				m_pivotLines.m_lines[1].vertex = Vertex3f( max.x(), uv_origin.y(), 0 );
			}
			{	// v pivot line
				m_pivotLines.m_lines[2].vertex = Vertex3f( uv_origin.x(), min.y(), 0 );
				m_pivotLines.m_lines[3].vertex = Vertex3f( uv_origin.x(), max.y(), 0 );
			}
		}
	}
	bool UpdateData() {
		if( !g_SelectedFaceInstances.empty() ){
			Face* face = &g_SelectedFaceInstances.last().getFace();
			if( m_face != face ){
				m_face = face;
				m_patch = 0;
				UpdateFaceData( true );
			}
			else if( memcmp( &m_projection, &m_face->getTexdef().m_projection, sizeof( TextureProjection ) ) != 0
			         || m_width != m_face->getShader().width()
			         || m_height != m_face->getShader().height() ) {
				UpdateFaceData( !projection_valid() ); // updateOrigin when prev state was invalid on the same face
			}
			return projection_valid();
		}
		else if( GlobalSelectionSystem().countSelected() != 0 ){
			Patch* patch = Node_getPatch( GlobalSelectionSystem().ultimateSelected().path().top() );
			if( patch ){
				if( m_patch != patch ){
					m_patch = patch;
					m_face = 0;
					UpdateFaceData( true );
				}
				else if( m_patchWidth != m_patch->getWidth()
				      || m_patchHeight != m_patch->getHeight()
				      || memcmp( m_patchCtrl.data(), m_patch->getControlPoints().data(), sizeof( *m_patchCtrl.data() ) * m_patchCtrl.size() ) != 0
				      || m_state_patch_raw != m_patch->getShader() ){
					UpdateFaceData( !projection_valid() ); // updateOrigin when prev state was invalid on the same patch
				}
				return projection_valid();
			}
		}
		return false;
	}
public:
	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		if( volume.fill() && UpdateData() ){
			if( m_patch ){
				renderer.SetState( const_cast<Shader*>( m_state_patch ), Renderer::eFullMaterials );
				renderer.addRenderable( m_patchRenderTex, m_lines2world );
			}
			renderer.SetState( m_state_line, Renderer::eFullMaterials );
			renderer.addRenderable( m_Ulines, m_lines2world );
			renderer.addRenderable( m_Vlines, m_lines2world );
			renderer.addRenderable( m_pivotLines, m_pivotLines2world );
			if( m_patch )
				renderer.addRenderable( m_patchRenderLattice, m_faceTex2local );

			//fix pivot position for better visibility
			m_pivot.render( renderer, volume, matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( vector3_normalised( volume.getViewer() - m_origin ) ), m_pivot2world ) );

			renderer.addRenderable( m_circle, m_circle2world );

			renderer.SetState( m_state_point, Renderer::eFullMaterials );
			if( m_patch )
				renderer.addRenderable( m_patchRenderPoints, m_faceTex2local );
			renderer.addRenderable( m_pivotPoint, m_pivot2world );
			renderer.addRenderable( m_gridPointU, m_pivotLines2world );
			renderer.addRenderable( m_gridPointV, m_pivotLines2world );
		}
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		//!? todo fix: eUV selection possibility may be blocked by the circle
		if( !view.fill() || !UpdateData() ){
			m_isSelected = false;
			return;
		}

		UVSelector selector;

		if( g_modifiers == c_modifierAlt ) // only try skew with alt // note also grabs eTex
			goto testSelectUVlines;
		if( g_modifiers != c_modifierNone )
			return applySelection( selector.m_selection, nullptr, nullptr, selector.m_index );

		{	// try pivot point
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot2world ) );
			SelectionIntersection best;
			Point_BestPoint( local2view, m_pivotPoint.m_point.vertex, best );
			selector.addIntersection( best, ePivot );
		}

		if( !selector.isSelected() ){ // try grid control points
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			SelectionIntersection best;
			Point_BestPoint( local2view, m_gridPointU.m_point.vertex, best );
			selector.addIntersection( best, eGridU );
			Point_BestPoint( local2view, m_gridPointV.m_point.vertex, best );
			selector.addIntersection( best, eGridV );
		}

		if( !selector.isSelected() && m_patch ){ // try patch points
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			SelectionIntersection best;
			for( std::size_t i = 0; i < m_patchRenderPoints.m_points.size(); ++i ){
				Point_BestPoint( local2view, m_patchRenderPoints.m_points[i], best );
				selector.addIntersection( best, ePatchPoint, i );
			}
		}
		if( !selector.isSelected() && m_patch ){ // try patch rows, columns
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			SelectionIntersection best;
			for ( std::size_t r = 0; r < m_patchHeight; ++r ){
				for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
					Line_BestPoint( local2view, &m_patchRenderLattice.m_lines[( r * ( m_patchWidth - 1 ) + c ) * 2], best );
					selector.addIntersection( best, ePatchRow, r );
				}
			}
			for ( std::size_t c = 0; c < m_patchWidth; ++c ){
				for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
					Line_BestPoint( local2view, &m_patchRenderLattice.m_lines[( m_patchWidth - 1 ) * m_patchHeight * 2 + ( c * ( m_patchHeight - 1 ) + r ) * 2], best );
					selector.addIntersection( best, ePatchColumn, c );
				}
			}
		}

		if( !selector.isSelected() ){ // try circle
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_circle2world ) );
			SelectionIntersection best;
			LineLoop_BestPoint( local2view, m_circle.m_vertices.data(), m_circle.m_vertices.size(), best );
			selector.addIntersection( best, eCircle );
		}

		if( !selector.isSelected() ){ // try pivot lines
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			SelectionIntersection best;
			Line_BestPoint( local2view, &m_pivotLines.m_lines[0], best );
			selector.addIntersection( best, ePivotU );
			Line_BestPoint( local2view, &m_pivotLines.m_lines[2], best );
			selector.addIntersection( best, ePivotV );
		}
testSelectUVlines:
		PointVertex* selectedU = 0;
		PointVertex* selectedV = 0;
		EUVSelection& selection = selector.m_selection;
		if( !selector.isSelected() ){ // try UV lines
/*
            -|------
             |
             |
V line center| - -  tex U center - -
             |         tex
             |          V
      -cross-|-----U line center-----|
             |
*/
			// special fuckage with the grid for better distinguishing of user's intentions
			// better picking of tex, only line for skew or scale with dense grid
			const Matrix4 screen2world( matrix4_full_inverse( view.GetViewMatrix() ) );
			const DoubleRay ray = ray_for_points( vector4_projected( matrix4_transformed_vector4( screen2world, BasicVector4<double>( 0, 0, -1, 1 ) ) ),
			                                      vector4_projected( matrix4_transformed_vector4( screen2world, BasicVector4<double>( 0, 0, 1, 1 ) ) ) );
			const DoubleVector3 hit = ray_intersect_plane( ray, m_plane );
			const Vector3 uvhit = matrix4_transformed_point( m_faceLocal2tex, hit );
			if( std::fabs( vector3_dot( ray.direction, m_plane.normal() ) ) > 1e-6
			 && !m_Ulines.m_lines.empty()
			 && !m_Vlines.m_lines.empty()
			 && matrix4_transformed_vector4( view.GetViewMatrix(), Vector4( hit, 1 ) ).w() > 0 ){
				PointVertex* closestU = &m_Ulines.m_lines[std::min( m_Ulines.m_lines.size() - 2,
				                                  static_cast<std::size_t>( float_to_integer( std::max( 0.f, uvhit.y() - m_Ulines.m_lines.front().vertex.y() ) * m_gridU ) * 2 ) )];
				PointVertex* closestV = &m_Vlines.m_lines[std::min( m_Vlines.m_lines.size() - 2,
				                                  static_cast<std::size_t>( float_to_integer( std::max( 0.f, uvhit.x() - m_Vlines.m_lines.front().vertex.x() ) * m_gridV ) * 2 ) )];
				const Vector2 sign( uvhit.y() > closestU->vertex.y()? 1 : -1, uvhit.x() > closestV->vertex.x()? 1 : -1 ); //hit in positive or negative part of lines u, v
				const PointVertex pCross( Vertex3f( closestV->vertex.x(), closestU->vertex.y(), 0 ) );
				const PointVertex pUcenter( Vertex3f( closestV->vertex.x() + sign.y() / ( m_gridV * 2 ), closestU->vertex.y(), 0 ) );
				const PointVertex pVcenter( Vertex3f( closestV->vertex.x(), closestU->vertex.y() + sign.x() / ( m_gridU * 2 ), 0 ) );

				PointVertex pTexUcenter[2]{ *closestU, *( closestU + 1 ) };
				pTexUcenter[0].vertex.y() = pTexUcenter[1].vertex.y() = pVcenter.vertex.y();
				PointVertex pTexVcenter[2]{ *closestV, *( closestV + 1 ) };
				pTexVcenter[0].vertex.x() = pTexVcenter[1].vertex.x() = pUcenter.vertex.x();

				SelectionIntersection iCross, iUcenter, iVcenter, iTexUcenter, iTexVcenter, iU, iV, iNull;

				const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
#if defined( DEBUG_SELECTION )
				g_render_clipped.construct( view.GetViewMatrix() );
#endif
				Line_BestPoint( local2view, closestU, iU );
				Line_BestPoint( local2view, closestV, iV );
				Line_BestPoint( local2view, pTexUcenter, iTexUcenter );
				Line_BestPoint( local2view, pTexVcenter, iTexVcenter );
				const bool uselected = iU < iNull;
				const bool vselected = iV < iNull;
				if( !uselected && !vselected ){ //no lines hit, definitely tex
					selection = eTex;
				}
				else if( ( !uselected || iTexUcenter < iU ) && ( !vselected || iTexVcenter < iV ) ){ //yes lines, but tex ones are closer
					selection = eTex;
				}
				else if( uselected != vselected ){ //only line selected
					if( uselected ){
						selection = g_modifiers == c_modifierAlt? eSkewU : eU;
						selectedU = closestU;
					}
					else{
						selection = g_modifiers == c_modifierAlt? eSkewV : eV;
						selectedV = closestV;
					}
				}
				else{ //two lines hit
					if( g_modifiers == c_modifierAlt ){ //pick only line for skew
						if( iU < iV ){
							selection = eSkewU;
							selectedU = closestU;
						}
						else{
							selection = eSkewV;
							selectedV = closestV;
						}
					}
					else{
						Point_BestPoint( local2view, pUcenter, iUcenter );
						Point_BestPoint( local2view, pVcenter, iVcenter );
						Point_BestPoint( local2view, pCross, iCross );
						const bool ucenter = iUcenter < iNull;
						const bool vcenter = iVcenter < iNull;
						if( !ucenter && !vcenter ){ // no centers, definitely two lines
							selection = eUV;
							selectedU = closestU;
							selectedV = closestV;
						}
						else if( iCross < iUcenter && iCross < iVcenter ){ // some center(s), cross is closer = two lines
							selection = eUV;
							selectedU = closestU;
							selectedV = closestV;
						}
						else{ // some center(s), pick closest line
							if( iUcenter < iVcenter ){
								selection = eU;
								selectedU = closestU;
							}
							else{
								selection = eV;
								selectedV = closestV;
							}
						}
					}
				}
			}
		}

		applySelection( selector.m_selection, selectedU, selectedV, selector.m_index );
	}
private:
	void applySelection( EUVSelection selection, PointVertex* selectedU, PointVertex* selectedV, int selectedPatchIndex ){
		if( m_selection != selection
		 || m_selectedU != selectedU
		 || m_selectedV != selectedV
		 || m_selectedPatchIndex != selectedPatchIndex ){
			if( m_selection != selection ){
				switch ( m_selection )
				{
				case ePivot:
					m_pivotPoint.m_point.colour = m_cWhite;
					break;
				case eGridU:
					m_gridPointU.m_point.colour = m_cWhite;
					break;
				case eGridV:
					m_gridPointV.m_point.colour = m_cWhite;
					break;
				case eCircle:
					m_circle.setColour( m_cGray );
					break;
				case ePivotU:
					m_pivotLines.m_lines[0].colour = m_cWhite;
					m_pivotLines.m_lines[1].colour = m_cWhite;
					break;
				case ePivotV:
					m_pivotLines.m_lines[2].colour = m_cWhite;
					m_pivotLines.m_lines[3].colour = m_cWhite;
					break;
				default:
					break;
				}
				switch ( selection )
				{
				case ePivot:
					m_pivotPoint.m_point.colour = m_cRed;
					break;
				case eGridU:
					m_gridPointU.m_point.colour = m_cRed;
					break;
				case eGridV:
					m_gridPointV.m_point.colour = m_cRed;
					break;
				case eCircle:
					m_circle.setColour( g_colour_selected );
					break;
				case ePivotU:
					m_pivotLines.m_lines[0].colour = m_cRed;
					m_pivotLines.m_lines[1].colour = m_cRed;
					break;
				case ePivotV:
					m_pivotLines.m_lines[2].colour = m_cRed;
					m_pivotLines.m_lines[3].colour = m_cRed;
					break;
				default:
					break;
				}
			}

			const Colour4b colour_selected = g_modifiers == c_modifierAlt? m_cGreen : g_colour_selected;
			if( m_selectedU != selectedU || m_selection != selection ){ // selected line changed or not, but scale<->skew modes exchanged
				if( m_selectedU )
					m_selectedU->colour =
					( m_selectedU + 1 )->colour = ( ( m_selectedU - &m_Ulines.m_lines[0] ) / 2 ) % m_gridU == 0? m_cGray : m_cGrayer;
				if( selectedU )
					selectedU->colour =
					( selectedU + 1 )->colour = colour_selected;
			}
			if( m_selectedV != selectedV || m_selection != selection ){
				if( m_selectedV )
					m_selectedV->colour =
					( m_selectedV + 1 )->colour = ( ( m_selectedV - &m_Vlines.m_lines[0] ) / 2 ) % m_gridV == 0? m_cGray : m_cGrayer;
				if( selectedV )
					selectedV->colour =
					( selectedV + 1 )->colour = colour_selected;
			}

			if( m_selectedPatchIndex != selectedPatchIndex || m_selection != selection ){
				if( m_selectedPatchIndex >= 0 ){
					switch ( m_selection )
					{
					case ePatchPoint:
						m_patchRenderPoints.m_points[m_selectedPatchIndex].colour = patchCtrl_isInside( m_selectedPatchIndex )? m_cPin : m_cGree;
						break;
					case ePatchRow:
						for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
							const std::size_t i = ( m_selectedPatchIndex * ( m_patchWidth - 1 ) + c ) * 2;
							m_patchRenderLattice.m_lines[i].colour =
							m_patchRenderLattice.m_lines[i + 1].colour = m_cOrang;
						}
						for ( std::size_t c = 0; c < m_patchWidth; ++c ){
							const std::size_t i = m_selectedPatchIndex * m_patchWidth + c;
							m_patchRenderPoints.m_points[i].colour = patchCtrl_isInside( i )? m_cPin : m_cGree;
						}
						break;
					case ePatchColumn:
						for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
							const std::size_t i = ( m_patchWidth - 1 ) * m_patchHeight * 2 + ( m_selectedPatchIndex * ( m_patchHeight - 1 ) + r ) * 2;
							m_patchRenderLattice.m_lines[i].colour =
							m_patchRenderLattice.m_lines[i + 1].colour = m_cOrang;
						}
						for ( std::size_t r = 0; r < m_patchHeight; ++r ){
							const std::size_t i = r * m_patchWidth + m_selectedPatchIndex;
							m_patchRenderPoints.m_points[i].colour = patchCtrl_isInside( i )? m_cPin : m_cGree;
						}
						break;
					default:
						break;
					}
				}
				if( selectedPatchIndex >= 0 ){
					switch ( selection )
					{
					case ePatchPoint:
						m_patchRenderPoints.m_points[selectedPatchIndex].colour = patchCtrl_isInside( selectedPatchIndex )? m_cPink : m_cGreen;
						break;
					case ePatchRow:
						for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
							const std::size_t i = ( selectedPatchIndex * ( m_patchWidth - 1 ) + c ) * 2;
							m_patchRenderLattice.m_lines[i].colour =
							m_patchRenderLattice.m_lines[i + 1].colour = m_cOrange;
						}
						for ( std::size_t c = 0; c < m_patchWidth; ++c ){
							const std::size_t i = selectedPatchIndex * m_patchWidth + c;
							m_patchRenderPoints.m_points[i].colour = patchCtrl_isInside( i )? m_cPink : m_cGreen;
						}
						break;
					case ePatchColumn:
						for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
							const std::size_t i = ( m_patchWidth - 1 ) * m_patchHeight * 2 + ( selectedPatchIndex * ( m_patchHeight - 1 ) + r ) * 2;
							m_patchRenderLattice.m_lines[i].colour =
							m_patchRenderLattice.m_lines[i + 1].colour = m_cOrange;
						}
						for ( std::size_t r = 0; r < m_patchHeight; ++r ){
							const std::size_t i = r * m_patchWidth + selectedPatchIndex;
							m_patchRenderPoints.m_points[i].colour = patchCtrl_isInside( i )? m_cPink : m_cGreen;
						}
						break;
					default:
						break;
					}
				}
			}

			m_selection = selection;
			m_selectedU = selectedU;
			m_selectedV = selectedV;
			m_selectedPatchIndex = selectedPatchIndex;
			SceneChangeNotify();
		}
		m_isSelected = ( selection != eNone );
	}
	void commitTransform( const Matrix4& transform ) const {
		if( m_face ){
			m_face->transform_texdef( transform, m_origin ); //! todo make SI update after Brush_textureChanged(); same problem after brush moved with tex lock
		} // also after Patch_textureChanged(); calling them now in this->freezeTransform() works good nuff
		else if( m_patch ){
			const Matrix4 uvTransform = transform_local2object( matrix4_affine_inverse( transform ), m_faceLocal2tex, m_faceTex2local );
			for( std::size_t i = 0; i < m_patchCtrl.size(); ++i ){
				const Vector3 uv = matrix4_transformed_point( uvTransform, Vector3( m_patchCtrl[i].m_texcoord, 0 ) );
				m_patch->getControlPointsTransformed()[i].m_texcoord = uv.vec2();
			}
//			m_patch->controlPointsChanged();
			m_patch->UpdateCachedData();
		}
		SceneChangeNotify();
	}
	/* Manipulatable */
	Vector3 m_start;
public:
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_plane( m_plane, m_view->GetViewMatrix(), device_point );
	}
	//!? fix meaningless undo on grid/origin change, then click tex or lines
	//!? todo no snap mode with alt modifier
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		const Vector3 current = point_on_plane( m_plane, m_view->GetViewMatrix(), device_point );

		const bool snap = g_modifiers.shift(), snapHard = g_modifiers.ctrl();

		const class Snapper
		{
			float m_x; //uv axis to screen coef
			float m_y;
		public:
			Snapper( const Vector3& current, const Matrix4& faceTex2local ) {
				Vector3 scale( m_view->GetViewport().x().x(), m_view->GetViewport().y().y(), 0 );
				scale /= float{ std::max( scale.x(), scale.y() ) }; // normalise to be consistent over screen width & height
				const Matrix4 proj = matrix4_multiplied_by_matrix4( matrix4_scale_for_vec3( scale ), m_view->GetViewMatrix() );
				// get unary world displacements over uv axes to screenspace
				const Vector3 curr = vector4_projected( matrix4_transformed_vector4( proj, Vector4( current, 1 ) ) );
				const Vector3 x = vector4_projected( matrix4_transformed_vector4( proj, Vector4( current + vector3_normalised( faceTex2local.x().vec3() ), 1 ) ) );
				const Vector3 y = vector4_projected( matrix4_transformed_vector4( proj, Vector4( current + vector3_normalised( faceTex2local.y().vec3() ), 1 ) ) );
				m_x = vector3_length( x - curr ) * vector3_length( faceTex2local.x().vec3() ); // consider uv space scaling
				m_y = vector3_length( y - curr ) * vector3_length( faceTex2local.y().vec3() );
			}
			bool x_snaps( float uv_dist, float epsilon = .01f ) const {
				return uv_dist * m_x < epsilon;
			}
			bool y_snaps( float uv_dist, float epsilon = .01f ) const {
				return uv_dist * m_y < epsilon;
			}
		} snapper( current, m_faceTex2local );

		switch ( m_selection )
		{
		case ePivot:
			{
				const Vector3 uv_origin_start = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, current );
				float bestDistU = FLT_MAX;
				float bestDistV = FLT_MAX;
				float snapToU = 0;
				float snapToV = 0;
				for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
					const float dist = std::fabs( ( *i ).vertex.y() - uv_origin.y() );
					if( dist < bestDistU ){
						bestDistU = dist;
						snapToU = ( *i ).vertex.y();
					}
				}
				for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
					const float dist = std::fabs( ( *i ).vertex.x() - uv_origin.x() );
					if( dist < bestDistV ){
						bestDistV = dist;
						snapToV = ( *i ).vertex.x();
					}
				}
				forEachUVPoint( [&]( const Vector3& point ){
					const float distU = std::fabs( point.y() - uv_origin.y() );
					if( distU < bestDistU ){
						bestDistU = distU;
						snapToU = point.y();
					}
					const float distV = std::fabs( point.x() - uv_origin.x() );
					if( distV < bestDistV ){
						bestDistV = distV;
						snapToV = point.x();
					}
				} );
				Vector3 result( uv_origin_start );
				if( snapper.y_snaps( bestDistU ) || snapHard ){
					result.y() = snapToU;
				}
				else{
					result.y() = uv_origin.y();
				}
				if( snapper.x_snaps( bestDistV ) || snapHard ){
					result.x() = snapToV;
				}
				else{
					result.x() = uv_origin.x();
				}
				m_origin = matrix4_transformed_point( m_faceTex2local, result );
				UpdateFaceData( false, false );
				SceneChangeNotify();
			}
			break;
		case ePivotU:
			{
				const Vector3 uv_origin_start = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, current );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
					const float dist = std::fabs( ( *i ).vertex.y() - uv_origin.y() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = ( *i ).vertex.y();
					}
				}
				forEachUVPoint( [&]( const Vector3& point ){
					const float dist = std::fabs( point.y() - uv_origin.y() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = point.y();
					}
				} );
				Vector3 result( uv_origin_start );
				if( snapper.y_snaps( bestDist ) || snapHard ){
					result.y() = snapTo;
				}
				else{
					result.y() = uv_origin.y();
				}
				m_origin = matrix4_transformed_point( m_faceTex2local, result );
				UpdateFaceData( false, false );
				SceneChangeNotify();
			}
			break;
		case ePivotV:
			{
				const Vector3 uv_origin_start = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, current );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
					const float dist = std::fabs( ( *i ).vertex.x() - uv_origin.x() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = ( *i ).vertex.x();
					}
				}
				forEachUVPoint( [&]( const Vector3& point ){
					const float dist = std::fabs( point.x() - uv_origin.x() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = point.x();
					}
				} );
				Vector3 result( uv_origin_start );
				if( snapper.x_snaps( bestDist ) || snapHard ){
					result.x() = snapTo;
				}
				else{
					result.x() = uv_origin.x();
				}
				m_origin = matrix4_transformed_point( m_faceTex2local, result );
				UpdateFaceData( false, false );
				SceneChangeNotify();
			}
			break;
		case eGridU:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_current = matrix4_transformed_point( m_faceLocal2tex, current );

				const float dist = std::max( ( float_to_integer( uv_origin.y() + m_gridSign.x() * .25 ) + m_gridSign.x() - uv_current.y() ) * m_gridSign.x(), .01f );
				unsigned int grid = std::max( 1, std::min( 16, int( 1 / dist ) ) );

				if( snapHard ){ // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
					grid--;
					grid |= grid >> 1;
					grid |= grid >> 2;
					grid |= grid >> 4;
					grid |= grid >> 8;
					grid |= grid >> 16;
					grid++;
				}

				if( m_gridU != grid || ( snap && m_gridV != grid ) ){
					m_gridU = grid;
					if( snap )
						m_gridV = grid;
					UpdateFaceData( false );
					SceneChangeNotify();
				}
			}
			break;
		case eGridV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_current = matrix4_transformed_point( m_faceLocal2tex, current );

				const float dist = std::max( ( float_to_integer( uv_origin.x() + m_gridSign.y() * .25 ) + m_gridSign.y() - uv_current.x() ) * m_gridSign.y(), .01f );
				unsigned int grid = std::max( 1, std::min( 16, int( 1 / dist ) ) );

				if( snapHard ){ // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
					grid--;
					grid |= grid >> 1;
					grid |= grid >> 2;
					grid |= grid >> 4;
					grid |= grid >> 8;
					grid |= grid >> 16;
					grid++;
				}

				if( m_gridV != grid || ( snap && m_gridU != grid ) ){
					m_gridV = grid;
					if( snap )
						m_gridU = grid;
					UpdateFaceData( false );
					SceneChangeNotify();
				}
			}
			break;
		case eCircle:
			{
				Vector3 from = m_start - m_origin;
				constrain_to_axis( from, m_tex2local.z().vec3() );
				Vector3 to = current - m_origin;
				constrain_to_axis( to, m_tex2local.z().vec3() );
				Matrix4 rot = g_matrix4_identity;
				if( snap ){
					matrix4_pivoted_rotate_by_axisangle( rot,
					                                     m_tex2local.z().vec3(),
					                                     float_snapped( angle_for_axis( from, to, m_tex2local.z().vec3() ), static_cast<float>( c_pi / 12.0 ) ),
					                                     m_origin );
				}
				else{
					matrix4_pivoted_rotate_by_axisangle( rot,
					                                     m_tex2local.z().vec3(),
					                                     angle_for_axis( from, to, m_tex2local.z().vec3() ),
					                                     m_origin );
				}
				{	// snap
					const Vector3 uvec = vector3_normalised( matrix4_transformed_direction( rot, m_tex2local.x().vec3() ) );
					const Vector3 vvec = vector3_normalised( matrix4_transformed_direction( rot, m_tex2local.y().vec3() ) );
					float bestDot = 0;
					Vector3 bestTo;
					bool V = false;
					forEachEdge( [&]( const Vector3& point0, const Vector3& point1 ){
						Vector3 vec( point1 - point0 );
						constrain_to_axis( vec, m_tex2local.z().vec3() );
						const float dotU = std::fabs( vector3_dot( uvec, vec ) );
						if( dotU > bestDot ){
							bestDot = dotU;
							bestTo = vector3_dot( uvec, vec ) > 0? vec : -vec;
							V = false;
						}
						const float dotV = std::fabs( vector3_dot( vvec, vec ) );
						if( dotV > bestDot ){
							bestDot = dotV;
							bestTo = vector3_dot( vvec, vec ) > 0? vec : -vec;
							V = true;
						}
					} );
					if( bestDot > 0.9994f || snapHard ){
						const Vector3 bestFrom = vector3_normalised( V? m_tex2local.y().vec3() : m_tex2local.x().vec3() );
						rot = g_matrix4_identity;
						matrix4_pivoted_rotate_by_axisangle( rot,
						                                     m_tex2local.z().vec3(),
						                                     angle_for_axis( bestFrom, bestTo, m_tex2local.z().vec3() ),
						                                     m_origin );
					}
				}

				Matrix4 faceTex2local = matrix4_multiplied_by_matrix4( rot, m_tex2local );
				faceTex2local.x().vec3() = plane3_project_point( Plane3( m_plane.normal(), 0 ), faceTex2local.x().vec3(), m_tex2local.z().vec3() );
				faceTex2local.y().vec3() = plane3_project_point( Plane3( m_plane.normal(), 0 ), faceTex2local.y().vec3(), m_tex2local.z().vec3() );
				faceTex2local = matrix4_multiplied_by_matrix4( // adjust to have UV's z = 0: move the plane along m_tex2local.z() so that plane.dist() = 0
				                    matrix4_translation_for_vec3(
				                        m_tex2local.z().vec3() * ( m_plane.dist() - vector3_dot( m_plane.normal(), faceTex2local.t().vec3() ) )
				                        / vector3_dot( m_plane.normal(), m_tex2local.z().vec3() )
				                    ),
				                    faceTex2local );
				m_lines2world = m_pivotLines2world = faceTex2local;

				m_pivot2world = matrix4_multiplied_by_matrix4( rot, m_pivot2world0 );

				commitTransform( rot );
			}
			break;
		case eU: //!? todo modifier or default snap to set scale u = scale v
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_local2tex, m_origin );
				const Vector3 uv_start = m_selectedU->vertex;
				const Vector3 uv_current = m_selectedU->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				forEachUVPoint( [&]( const Vector3& point ){
					const float dist = std::fabs( point.y() - uv_current.y() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = point.y();
					}
				} );
				Vector3 result( 1, uv_current.y(), 1 );
				if( snapper.y_snaps( bestDist ) || snapHard ){
					result.y() = snapTo;
				}
				result.y() = ( result.y() - uv_origin.y() ) / ( uv_start.y() - uv_origin.y() );

				if( snap )
					result.x() = std::fabs( result.y() );

				/* prevent scaling to 0, limit at max 10 textures per world unit */
				if( vector3_length_squared( m_tex2local.y().vec3() * result.y() ) < .01 )
					return;

				Matrix4 scale = g_matrix4_identity;
				matrix4_pivoted_scale_by_vec3( scale, result, uv_origin );
				scale = transform_local2object( scale, m_tex2local, m_local2tex );
				{
					Matrix4 linescale = g_matrix4_identity;
					matrix4_pivoted_scale_by_vec3( linescale, result, matrix4_transformed_point( m_faceLocal2tex, m_origin ) );
					m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, linescale );

					m_pivot2world = matrix4_multiplied_by_matrix4( m_pivot2world0, matrix4_scale_for_vec3( result ) );
				}
				commitTransform( scale );
			}
			break;
		case eV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_local2tex, m_origin );
				const Vector3 uv_start = m_selectedV->vertex;
				const Vector3 uv_current = m_selectedV->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				forEachUVPoint( [&]( const Vector3& point ){
					const float dist = std::fabs( point.x() - uv_current.x() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = point.x();
					}
				} );
				Vector3 result( uv_current.x(), 1, 1 );
				if( snapper.x_snaps( bestDist ) || snapHard ){
					result.x() = snapTo;
				}
				result.x() = ( result.x() - uv_origin.x() ) / ( uv_start.x() - uv_origin.x() );

				if( snap )
					result.y() = std::fabs( result.x() );

				/* prevent scaling to 0, limit at max 10 textures per world unit */
				if( vector3_length_squared( m_tex2local.x().vec3() * result.x() ) < .01 )
					return;

				Matrix4 scale = g_matrix4_identity;
				matrix4_pivoted_scale_by_vec3( scale, result, uv_origin );
				scale = transform_local2object( scale, m_tex2local, m_local2tex );
				{
					Matrix4 linescale = g_matrix4_identity;
					matrix4_pivoted_scale_by_vec3( linescale, result, matrix4_transformed_point( m_faceLocal2tex, m_origin ) );
					m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, linescale );

					m_pivot2world = matrix4_multiplied_by_matrix4( m_pivot2world0, matrix4_scale_for_vec3( result ) );
				}
				commitTransform( scale );
			}
			break;
		case eUV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_local2tex, m_origin );
				const Vector3 uv_start{ m_selectedV->vertex.x(), m_selectedU->vertex.y(), 0 };
				const Vector3 uv_current{ ( m_selectedV->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start ) ).x(),
				                          ( m_selectedU->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start ) ).y(),
				                          0 };
				float bestDistU = FLT_MAX;
				float snapToU = 0;
				float bestDistV = FLT_MAX;
				float snapToV = 0;
				forEachUVPoint( [&]( const Vector3& point ){
					const float distU = std::fabs( point.y() - uv_current.y() );
					if( distU < bestDistU ){
						bestDistU = distU;
						snapToU = point.y();
					}
					const float distV = std::fabs( point.x() - uv_current.x() );
					if( distV < bestDistV ){
						bestDistV = distV;
						snapToV = point.x();
					}
				} );

				Vector3 result( uv_current.x(), uv_current.y(), 1 );
				if( snapper.y_snaps( bestDistU ) || snapHard ){
					result.y() = snapToU;
				}
				result.y() = ( result.y() - uv_origin.y() ) / ( uv_start.y() - uv_origin.y() );

				if( snapper.x_snaps( bestDistV ) || snapHard ){
					result.x() = snapToV;
				}
				result.x() = ( result.x() - uv_origin.x() ) / ( uv_start.x() - uv_origin.x() );

				if( snap ){
					const std::size_t best = std::fabs( result.x() ) > std::fabs( result.y() )? 0 : 1;
					result[( best + 1 ) % 2] = std::copysign( result[best], result[( best + 1 ) % 2] );
				}

				/* prevent scaling to 0, limit at max 10 textures per world unit */
				if( vector3_length_squared( m_tex2local.x().vec3() * result.x() ) < .01 ||
				    vector3_length_squared( m_tex2local.y().vec3() * result.y() ) < .01 )
					return;

				Matrix4 scale = g_matrix4_identity;
				matrix4_pivoted_scale_by_vec3( scale, result, uv_origin );
				scale = transform_local2object( scale, m_tex2local, m_local2tex );
				{
					Matrix4 linescale = g_matrix4_identity;
					matrix4_pivoted_scale_by_vec3( linescale, result, matrix4_transformed_point( m_faceLocal2tex, m_origin ) );
					m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, linescale );

					m_pivot2world = matrix4_multiplied_by_matrix4( m_pivot2world0, matrix4_scale_for_vec3( result ) );
				}
				commitTransform( scale );
			}
			break;
		case eSkewU:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_move = matrix4_transformed_point( m_faceLocal2tex, current ) - matrix4_transformed_point( m_faceLocal2tex, m_start );
				Matrix4 skew( g_matrix4_identity );
				skew[4] = uv_move.x() / ( m_selectedU->vertex - uv_origin ).y();

				const Vector3 skewed = matrix4_transformed_direction( skew, g_vector3_axis_y );
				const float uv_y_measure_dist = ( m_selectedU->vertex - uv_origin ).y();
				float bestDist = FLT_MAX;
				Vector3 bestTo;
				const auto snap_to_edge = [&]( const Vector3 edge ){
					if( std::fabs( edge.y() ) > 1e-5f ){ // don't snap so, that one axis = the other
						const float dist = std::fabs( edge.x() * uv_y_measure_dist / edge.y() - skewed.x() * uv_y_measure_dist / skewed.y() );
						if( dist < bestDist ){
							bestDist = dist;
							bestTo = edge;
						}
					}
				};
				forEachEdge( [&]( const Vector3& point0, const Vector3& point1 ){
					snap_to_edge( matrix4_transformed_point( m_faceLocal2tex, point1 ) - matrix4_transformed_point( m_faceLocal2tex, point0 ) );
				} );
				forEachPoint( [&]( const Vector3& point ){
					const Vector3 po = matrix4_transformed_point( m_faceLocal2tex, point );
					for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.cbegin(); i != m_Vlines.m_lines.cend(); ++++i ){
						snap_to_edge( po - Vector3( i->vertex.x(), uv_origin.y(), 0 ) );
					}
					snap_to_edge( po - Vector3( uv_origin.x(), uv_origin.y(), 0 ) );
				} );
				if( snapper.x_snaps( bestDist, .015f ) || snapHard ){ //!? todo add snap: make manipulated axis orthogonal to the other
					skew[4] = bestTo.x() / bestTo.y();
				}

				{
					Matrix4 mat( g_matrix4_identity );
					matrix4_translate_by_vec3( mat, uv_origin );
					matrix4_multiply_by_matrix4( mat, skew );
					matrix4_translate_by_vec3( mat, -uv_origin );
					skew = mat;
				}

				m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, skew );
				m_pivot2world = transform_local2object( skew, m_tex2local, m_local2tex );
				matrix4_multiply_by_matrix4( m_pivot2world, m_pivot2world0 );

				skew = transform_local2object( skew, m_faceTex2local, m_faceLocal2tex );
				commitTransform( skew );
			}
			break;
		case eSkewV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_move = matrix4_transformed_point( m_faceLocal2tex, current ) - matrix4_transformed_point( m_faceLocal2tex, m_start );
				Matrix4 skew( g_matrix4_identity );
				skew[1] = uv_move.y() / ( m_selectedV->vertex - uv_origin ).x();

				const Vector3 skewed = matrix4_transformed_direction( skew, g_vector3_axis_x );
				const float uv_x_measure_dist = ( m_selectedV->vertex - uv_origin ).x();
				float bestDist = FLT_MAX;
				Vector3 bestTo;
				const auto snap_to_edge = [&]( const Vector3 edge ){
					if( std::fabs( edge.x() ) > 1e-5f ){ // don't snap so, that one axis = the other
						const float dist = std::fabs( edge.y() * uv_x_measure_dist / edge.x() - skewed.y() * uv_x_measure_dist / skewed.x() );
						if( dist < bestDist ){
							bestDist = dist;
							bestTo = edge;
						}
					}
				};
				forEachEdge( [&]( const Vector3& point0, const Vector3& point1 ){
					snap_to_edge( matrix4_transformed_point( m_faceLocal2tex, point1 ) - matrix4_transformed_point( m_faceLocal2tex, point0 ) );
				} );
				forEachPoint( [&]( const Vector3& point ){
					const Vector3 po = matrix4_transformed_point( m_faceLocal2tex, point );
					for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.cbegin(); i != m_Ulines.m_lines.cend(); ++++i ){
						snap_to_edge( po - Vector3( uv_origin.x(), i->vertex.y(), 0 ) );
					}
					snap_to_edge( po - Vector3( uv_origin.x(), uv_origin.y(), 0 ) );
				} );
				if( snapper.y_snaps( bestDist, .015f ) || snapHard ){ //!? todo add snap: make manipulated axis orthogonal to the other
					skew[1] = bestTo.y() / bestTo.x();
				}

				{
					Matrix4 mat( g_matrix4_identity );
					matrix4_translate_by_vec3( mat, uv_origin );
					matrix4_multiply_by_matrix4( mat, skew );
					matrix4_translate_by_vec3( mat, -uv_origin );
					skew = mat;
				}

				m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, skew );
				m_pivot2world = transform_local2object( skew, m_tex2local, m_local2tex );
				matrix4_multiply_by_matrix4( m_pivot2world, m_pivot2world0 );

				skew = transform_local2object( skew, m_faceTex2local, m_faceLocal2tex );
				commitTransform( skew );
			}
			break;
		case eTex:
			{
				const Vector3 uvstart = matrix4_transformed_point( m_faceLocal2tex, m_start );
				const Vector3 uvcurrent = matrix4_transformed_point( m_faceLocal2tex, current );
				const Vector3 uvmove = uvcurrent - uvstart;
				float bestDistU = FLT_MAX;
				float bestDistV = FLT_MAX;
				float snapMoveU = 0;
				float snapMoveV = 0;
				// snap uvmove
				const auto functor = [&]( const Vector3& point ){
					for( auto it = m_Ulines.m_lines.cbegin(); it != m_Ulines.m_lines.cend(); ++++it ){
						const float dist = point.y() - ( ( *it ).vertex.y() + uvmove.y() );
						if( std::fabs( dist ) < bestDistU ){
							bestDistU = std::fabs( dist );
							snapMoveU = uvmove.y() + dist;
						}
					}
					for( auto it = m_Vlines.m_lines.cbegin(); it != m_Vlines.m_lines.cend(); ++++it ){
						const float dist = point.x() - ( ( *it ).vertex.x() + uvmove.x() );
						if( std::fabs( dist ) < bestDistV ){
							bestDistV = std::fabs( dist );
							snapMoveV = uvmove.x() + dist;
						}
					}
				};
				forEachUVPoint( functor );
				functor( matrix4_transformed_point( m_faceLocal2tex, m_origin ) );

				Vector3 result( uvmove );
				if( snapper.y_snaps( bestDistU ) || snapHard ){
					result.y() = snapMoveU;
				}
				if( snapper.x_snaps( bestDistV ) || snapHard ){
					result.x() = snapMoveV;
				}

				if( snap ){
					auto& smaller = std::fabs( uvmove.x() * vector3_length( m_faceTex2local.x().vec3() ) ) <
					                std::fabs( uvmove.y() * vector3_length( m_faceTex2local.y().vec3() ) )? result.x() : result.y();
					smaller = 0;
				}

				result = translation_local2object( result, m_faceTex2local, m_faceLocal2tex );

				const Matrix4 translation = matrix4_translation_for_vec3( result );

				m_lines2world = matrix4_multiplied_by_matrix4( translation, m_faceTex2local );

				commitTransform( translation );
			}
			break;
		case ePatchPoint:
		case ePatchRow:
		case ePatchColumn:
			{
				std::vector<std::size_t> indices;
				if( m_selection == ePatchPoint )
					indices.push_back( m_selectedPatchIndex );
				else if( m_selection == ePatchRow )
					for ( std::size_t c = 0; c < m_patchWidth; ++c )
						indices.push_back( m_selectedPatchIndex * m_patchWidth + c );
				else if( m_selection == ePatchColumn )
					for ( std::size_t r = 0; r < m_patchHeight; ++r )
						indices.push_back( r * m_patchWidth + m_selectedPatchIndex );

				const Vector3 uvstart = matrix4_transformed_point( m_faceLocal2tex, m_start );
				const Vector3 uvcurrent = matrix4_transformed_point( m_faceLocal2tex, current );
				const Vector3 uvmove = uvcurrent - uvstart;
				float bestDistU = FLT_MAX;
				float bestDistV = FLT_MAX;
				float snapMoveU = 0;
				float snapMoveV = 0;
				// snap uvmove
				for( std::size_t index : indices ){
					for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
						const float dist = m_patchCtrl[index].m_texcoord.y() + uvmove.y() - ( *i ).vertex.y();
						if( std::fabs( dist ) < bestDistU ){
							bestDistU = std::fabs( dist );
							snapMoveU = uvmove.y() - dist;
						}
					}
					for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
						const float dist = m_patchCtrl[index].m_texcoord.x() + uvmove.x() - ( *i ).vertex.x();
						if( std::fabs( dist ) < bestDistV ){
							bestDistV = std::fabs( dist );
							snapMoveV = uvmove.x() - dist;
						}
					}
					const Vector3 origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
					{
						const float dist = m_patchCtrl[index].m_texcoord.y() + uvmove.y() - origin.y();
						if( std::fabs( dist ) < bestDistU ){
							bestDistU = std::fabs( dist );
							snapMoveU = uvmove.y() - dist;
						}
					}
					{
						const float dist = m_patchCtrl[index].m_texcoord.x() + uvmove.x() - origin.x();
						if( std::fabs( dist ) < bestDistV ){
							bestDistV = std::fabs( dist );
							snapMoveV = uvmove.x() - dist;
						}
					}
				}

				Vector3 result( uvmove );
				if( snapper.y_snaps( bestDistU ) || snapHard ){
					result.y() = snapMoveU;
				}
				if( snapper.x_snaps( bestDistV ) || snapHard ){
					result.x() = snapMoveV;
				}

				if( snap ){
					auto& smaller = std::fabs( uvmove.x() * vector3_length( m_faceTex2local.x().vec3() ) ) <
					                std::fabs( uvmove.y() * vector3_length( m_faceTex2local.y().vec3() ) )? result.x() : result.y();
					smaller = 0;
				}

				const Matrix4 translation = matrix4_translation_for_vec3( result );
				for( std::size_t i : indices ){
					const Vector3 uv = matrix4_transformed_point( translation, Vector3( m_patchCtrl[i].m_texcoord, 0 ) );
					m_patch->getControlPointsTransformed()[i].m_texcoord = uv.vec2();
					m_patchRenderPoints.m_points[i].vertex = vertex3f_for_vector3( uv );
				}

				// update lattice renderable entirely
				for ( std::size_t r = 0; r < m_patchHeight; ++r ){
					for ( std::size_t c = 0; c < m_patchWidth - 1; ++c ){
						const Vector2& a = m_patch->getControlPointsTransformed()[r * m_patchWidth + c].m_texcoord;
						const Vector2& b = m_patch->getControlPointsTransformed()[r * m_patchWidth + c + 1].m_texcoord;
						m_patchRenderLattice.m_lines[( r * ( m_patchWidth - 1 ) + c ) * 2].vertex = vertex3f_for_vector3( Vector3( a, 0 ) );
						m_patchRenderLattice.m_lines[( r * ( m_patchWidth - 1 ) + c ) * 2 + 1].vertex = vertex3f_for_vector3( Vector3( b, 0 ) );
					}
				}
				for ( std::size_t c = 0; c < m_patchWidth; ++c ){
					for ( std::size_t r = 0; r < m_patchHeight - 1; ++r ){
						const Vector2& a = m_patch->getControlPointsTransformed()[r * m_patchWidth + c].m_texcoord;
						const Vector2& b = m_patch->getControlPointsTransformed()[( r + 1 ) * m_patchWidth + c].m_texcoord;
						m_patchRenderLattice.m_lines[( m_patchWidth - 1 ) * m_patchHeight * 2 + ( c * ( m_patchHeight - 1 ) + r ) * 2].vertex = vertex3f_for_vector3( Vector3( a, 0 ) );
						m_patchRenderLattice.m_lines[( m_patchWidth - 1 ) * m_patchHeight * 2 + ( c * ( m_patchHeight - 1 ) + r ) * 2 + 1].vertex = vertex3f_for_vector3( Vector3( b, 0 ) );
					}
				}

				m_patch->UpdateCachedData();
				SceneChangeNotify();
			}
		default:
			break;
		}
	}

	void freezeTransform() override {
		if( m_selection == eCircle
		 || m_selection == eU
		 || m_selection == eV
		 || m_selection == eUV
		 || m_selection == eSkewU
		 || m_selection == eSkewV
		 || m_selection == eTex
		 || m_selection == ePatchPoint
		 || m_selection == ePatchRow
		 || m_selection == ePatchColumn )
		{
			if( m_face ){
				m_face->freezeTransform();
				Brush_textureChanged();
			}
			else if( m_patch ){
				m_patch->freezeTransform();
				Patch_textureChanged();
			}
		}
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

UVManipulator* New_UVManipulator(){
	return new UVManipulatorImpl;
}

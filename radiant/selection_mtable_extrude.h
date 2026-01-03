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

#pragma once

#include "selection_.h"
#include "grid.h"
#include "brush.h"
#include "brushnode.h"

class DragExtrudeFaces : public Manipulatable
{
	Vector3 m_0;
	Plane3 m_planeSelected;
	std::size_t m_axisZ;
	Plane3 m_planeZ;
	Vector3 m_startZ;

	bool m_originalBrushSaved;
	bool m_originalBrushChanged;
public:
	class ExtrudeSource
	{
	public:
		BrushInstance* m_brushInstance;
		struct InFaceOutBrush{
			Face* m_face;
			PlanePoints m_planepoints;
			Brush* m_outBrush;
		};
		std::vector<InFaceOutBrush> m_faces;
		std::vector<InFaceOutBrush>::iterator faceFind( const Face* face ){
			return std::ranges::find( m_faces, face, &InFaceOutBrush::m_face );
		}
		std::vector<InFaceOutBrush>::const_iterator faceFind( const Face* face ) const {
			return std::ranges::find( m_faces, face, &InFaceOutBrush::m_face );
		}
		bool faceExcluded( const Face* face ) const {
			return faceFind( face ) == m_faces.end();
		}
	};
	std::vector<ExtrudeSource> m_extrudeSources;

	DragExtrudeFaces() = default;
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_axisZ = vector3_max_abs_component_index( m_planeSelected.normal() );
		Vector3 xydir( m_view->getViewer() - m_0 );
		xydir[m_axisZ] = 0;
		vector3_normalise( xydir );
		m_planeZ = Plane3( xydir, vector3_dot( xydir, m_0 ) );
		m_startZ = point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point );

		m_originalBrushSaved = false;
		m_originalBrushChanged = false;

		UndoableCommand undo( "ExtrudeBrushFaces" );
		for( ExtrudeSource& source : m_extrudeSources ){
			for( auto& infaceoutbrush : source.m_faces ){
				const Face* face = infaceoutbrush.m_face;

				NodeSmartReference node( GlobalBrushCreator().createBrush() );
				Node_getTraversable( source.m_brushInstance->path().parent() )->insert( node );

				scene::Path path( source.m_brushInstance->path() );
				path.pop();
				path.push( makeReference( node.get() ) );
				selectPath( path, true );

				Brush* brush = Node_getBrush( node.get() );
				infaceoutbrush.m_outBrush = brush;

				Face* f = brush->addFace( *face );
				f->getPlane().offset( GetGridSize() );
				f->planeChanged();

				f = brush->addFace( *face );
				f->getPlane().reverse();
				f->planeChanged();

				for( const WindingVertex& vertex : face->getWinding() ){
					if( vertex.adjacent != c_brush_maxFaces ){
						f = brush->addFace( **std::next( source.m_brushInstance->getBrush().begin(), vertex.adjacent ) );

						const DoubleVector3 cross = vector3_cross( f->plane3_().normal(), face->plane3_().normal() );
						f->getPlane().copy( vertex.vertex, vertex.vertex + cross * 64, vertex.vertex + face->plane3_().normal() * 64 );
						f->planeChanged();
					}
				}
			}
		}
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = g_vector3_axes[m_axisZ] * vector3_dot( m_planeSelected.normal(), ( point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point ) - m_startZ ) )
		                  * ( m_planeSelected.normal()[m_axisZ] >= 0? 1 : -1 );

		if( !std::isfinite( current[0] ) || !std::isfinite( current[1] ) || !std::isfinite( current[2] ) ) // catch INF case, is likely with top of the box in 2D
			return;

		vector3_snap( current, GetSnapGridSize() );

		const float offset = std::fabs( m_planeSelected.normal()[m_axisZ] ) * std::copysign(
		                              std::max( static_cast<double>( GetGridSize() ), vector3_length( current ) ),
		                              vector3_dot( current, m_planeSelected.normal() ) );

		if( offset >= 0 ){ // extrude outside
			if( m_originalBrushChanged ){
				m_originalBrushChanged = false;
				for( ExtrudeSource& source : m_extrudeSources ){
					// revert original brush
					for( auto& infaceoutbrush : source.m_faces ){
						Face* face = infaceoutbrush.m_face;
						face->getPlane().copy( infaceoutbrush.m_planepoints );
						face->planeChanged();
					}
				}
			}
			for( ExtrudeSource& source : m_extrudeSources ){
				Brush& brush0 = source.m_brushInstance->getBrush();
				if( source.m_faces.size() > 1 ){
					auto *tmpbrush = new Brush( brush0 );
					offsetFaces( source, *tmpbrush, offset );
					brush_extrudeDiag( brush0, *tmpbrush, source );
					delete tmpbrush;
				}
				else{
					for( auto& infaceoutbrush : source.m_faces ){
						const Face* face = infaceoutbrush.m_face;
						Brush* brush = infaceoutbrush.m_outBrush;
						brush->clear();

						Face* f = brush->addFace( *face );
						f->getPlane().offset( offset );
						f->planeChanged();

						f = brush->addFace( *face );
						f->getPlane().reverse();
						f->planeChanged();

						for( const WindingVertex& vertex : face->getWinding() ){
							if( vertex.adjacent != c_brush_maxFaces ){
								brush->addFace( **std::next( brush0.begin(), vertex.adjacent ) );
							}
						}
					}
				}
			}
		}
		else{ // extrude inside
			if( !m_originalBrushSaved ){
				m_originalBrushSaved = true;
				for( ExtrudeSource& source : m_extrudeSources )
					for( auto& infaceoutbrush : source.m_faces )
						infaceoutbrush.m_face->undoSave();
			}
			m_originalBrushChanged = true;

			for( ExtrudeSource& source : m_extrudeSources ){
				Brush& brush0 = source.m_brushInstance->getBrush();
				// revert original brush
				for( auto& infaceoutbrush : source.m_faces ){
					Face* face = infaceoutbrush.m_face;
					face->getPlane().copy( infaceoutbrush.m_planepoints );
					face->planeChanged();
				}
				if( source.m_faces.size() > 1 ){
					auto *tmpbrush = new Brush( brush0 );
					tmpbrush->evaluateBRep();
					offsetFaces( source, brush0, offset );
					if( brush0.hasContributingFaces() )
						brush_extrudeDiag( brush0, *tmpbrush, source );
					delete tmpbrush;
				}
				else{
					for( auto& infaceoutbrush : source.m_faces ){
						Face* face = infaceoutbrush.m_face;
						Brush* brush = infaceoutbrush.m_outBrush;
						brush->clear();

						brush->copy( brush0 );

						Face* f = brush->addFace( *face );
						f->getPlane().offset( offset );
						f->getPlane().reverse();
						f->planeChanged();

						brush->removeEmptyFaces();
						// modify original brush
						face->getPlane().offset( offset );
						face->planeChanged();
					}
				}
			}
		}
	}
	void set0( const Vector3& start, const Plane3& planeSelected ){
		m_0 = start;
		m_planeSelected = planeSelected;
	}

private:
	void offsetFaces( const ExtrudeSource& source, Brush& brush, const float offset ){
		const Brush& brush0 = source.m_brushInstance->getBrush();
		for( Brush::const_iterator i0 = brush0.begin(); i0 != brush0.end(); ++i0 ){
			const Face& face0 = *( *i0 );
			if( !source.faceExcluded( &face0 ) ){
				Face& face = *( *std::next( brush.begin(), std::distance( brush0.begin(), i0 ) ) );
				face.getPlane().offset( offset );
				face.planeChanged();
			}
		}
		brush.evaluateBRep();
	}
	/* brush0, brush2 are supposed to have same amount of faces in the same order; brush2 bigger than brush0 */
	void brush_extrudeDiag( const Brush& brush0, const Brush& brush2, ExtrudeSource& source ){
		TextureProjection projection;
		TexDef_Construct_Default( projection );

		for( Brush::const_iterator i0 = brush0.begin(); i0 != brush0.end(); ++i0 ){
			const Face& face0 = *( *i0 );
			const Face& face2 = *( *std::next( brush2.begin(), std::distance( brush0.begin(), i0 ) ) );

			auto infaceoutbrush_iter = source.faceFind( &face0 ); // brush0 = source.m_brushInstance->getBrush()
			if( infaceoutbrush_iter != source.m_faces.end() ) {
				if( face0.contributes() || face2.contributes() ) {
					const char* shader = face0.GetShader();

					Brush* outBrush = ( *infaceoutbrush_iter ).m_outBrush;
					outBrush->clear();

					if( face0.contributes() ){
						if( Face* newFace = outBrush->addFace( face0 ) ) {
							newFace->flipWinding();
						}
					}
					if( face2.contributes() ){
						outBrush->addFace( face2 );
					}

					if( face0.contributes() && face2.contributes() ){ //sew two valid windings
						const auto addSidePlanes = [&outBrush, shader, &projection]( const Winding& winding0, const Winding& winding2, const DoubleVector3 normal, const bool swap ){
							for( std::size_t index0 = 0; index0 < winding0.numpoints; ++index0 ){
								const std::size_t next = Winding_next( winding0, index0 );
								DoubleVector3 BestPoint;
								double bestdot = -1;
								for( std::size_t index2 = 0; index2 < winding2.numpoints; ++index2 ){
									const double dot = vector3_dot(
									                       vector3_normalised(
									                           vector3_cross(
									                               winding0[index0].vertex - winding0[next].vertex,
									                               winding0[index0].vertex - winding2[index2].vertex
									                           )
									                       ),
									                       normal
									                   );
									if( dot > bestdot ) {
										bestdot = dot;
										BestPoint = winding2[index2].vertex;
									}
								}
								outBrush->addPlane( winding0[swap? next : index0].vertex,
								                    winding0[swap? index0 : next].vertex,
								                    BestPoint,
								                    shader,
								                    projection );
							}
						};
						//insert side planes from each winding perspective, as their form may change after brush expansion
						addSidePlanes( face0.getWinding(), face2.getWinding(), face0.getPlane().plane3().normal(), false );
						addSidePlanes( face2.getWinding(), face0.getWinding(), face0.getPlane().plane3().normal(), true );
					}
					else{ //one valid winding: this way may produce garbage with complex brushes, extruded partially, but does preferred result with simple ones
						const auto addSidePlanes = [&outBrush, shader, &projection]( const Winding& winding0, const Brush& brush2, const Plane3 plane, const bool swap ){
							for( std::size_t index0 = 0; index0 < winding0.numpoints; ++index0 ){
								const std::size_t next = Winding_next( winding0, index0 );
								DoubleVector3 BestPoint;
								double bestdist = 999999;
								for( const Face* f : brush2 ) {
									const Winding& winding2 = f->getWinding();
									for( std::size_t index2 = 0; index2 < winding2.numpoints; ++index2 ){
										const double testdist = vector3_length( winding0[index0].vertex - winding2[index2].vertex );
										if( testdist < bestdist && plane3_distance_to_point( plane, winding2[index2].vertex ) > .05 ) {
											bestdist = testdist;
											BestPoint = winding2[index2].vertex;
										}
									}
								}
								outBrush->addPlane( winding0[swap? next : index0].vertex,
								                    winding0[swap? index0 : next].vertex,
								                    BestPoint,
								                    shader,
								                    projection );
							}
						};

						if( face0.contributes() )
							addSidePlanes( face0.getWinding(), brush2, face0.getPlane().plane3(), false );
						else if( face2.contributes() )
							addSidePlanes( face2.getWinding(), brush0, plane3_flipped( face2.getPlane().plane3() ), true );
					}
					outBrush->removeEmptyFaces();
				}
			}
		}
	}
};

/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "brush.h"
#include "signal/signal.h"

Signal0 g_brushTextureChangedCallbacks;

void Brush_addTextureChangedCallback( const SignalHandler& handler ){
	g_brushTextureChangedCallbacks.connectLast( handler );
}

void Brush_textureChanged(){
	g_brushTextureChangedCallbacks();
}

QuantiseFunc Face::m_quantise;
EBrushType Face::m_type;
EBrushType FacePlane::m_type;
bool g_brush_texturelock_enabled = false;
bool g_brush_textureVertexlock_enabled = false;

EBrushType Brush::m_type;
double Brush::m_maxWorldCoord = 0;
Shader* Brush::m_state_point;
Shader* Brush::m_state_deeppoint;
Shader* BrushClipPlane::m_state = 0;
Shader* BrushInstance::m_state_selpoint;
Counter* BrushInstance::m_counter = 0;

FaceInstanceSet g_SelectedFaceInstances;


struct SListNode
{
	SListNode* m_next;
};

class ProximalVertex
{
public:
const SListNode* m_vertices;

ProximalVertex( const SListNode* next )
	: m_vertices( next ){
}

bool operator<( const ProximalVertex& other ) const {
	if ( !( operator==( other ) ) ) {
		return m_vertices < other.m_vertices;
	}
	return false;
}
bool operator==( const ProximalVertex& other ) const {
	const SListNode* v = m_vertices;
	std::size_t DEBUG_LOOP = 0;
	do
	{
		if ( v == other.m_vertices ) {
			return true;
		}
		v = v->m_next;
		//ASSERT_MESSAGE(DEBUG_LOOP < c_brush_maxFaces, "infinite loop");
		if ( !( DEBUG_LOOP < c_brush_maxFaces ) ) {
			break;
		}
		++DEBUG_LOOP;
	}
	while ( v != m_vertices );
	return false;
}
};

typedef Array<SListNode> ProximalVertexArray;
std::size_t ProximalVertexArray_index( const ProximalVertexArray& array, const ProximalVertex& vertex ){
	return vertex.m_vertices - array.data();
}



inline bool Brush_isBounded( const Brush& brush ){
	for ( Brush::const_iterator i = brush.begin(); i != brush.end(); ++i )
	{
		if ( !( *i )->is_bounded() ) {
			return false;
		}
	}
	return true;
}

void Brush::buildBRep(){
	m_BRep_evaluation = true;

	bool degenerate = buildWindings();

	std::size_t faces_size = 0;
	std::size_t faceVerticesCount = 0;
	for ( Faces::const_iterator i = m_faces.begin(); i != m_faces.end(); ++i )
	{
		if ( ( *i )->contributes() ) {
			++faces_size;
		}
		faceVerticesCount += ( *i )->getWinding().numpoints;
	}

	if ( degenerate || faces_size < 4 || faceVerticesCount != ( faceVerticesCount >> 1 ) << 1 ) { // sum of vertices for each face of a valid polyhedron is always even
		m_uniqueVertexPoints.resize( 0 );

		vertex_clear();
		edge_clear();

		m_edge_indices.resize( 0 );
		m_edge_faces.resize( 0 );

		m_faceCentroidPoints.resize( 0 );
		m_uniqueEdgePoints.resize( 0 );
		m_uniqueVertexPoints.resize( 0 );

		for ( Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i )
		{
			( *i )->getWinding().resize( 0 );
		}
	}
	else
	{
		{
			typedef std::vector<FaceVertexId> FaceVertices;
			FaceVertices faceVertices;
			faceVertices.reserve( faceVerticesCount );

			{
				for ( std::size_t i = 0; i != m_faces.size(); ++i )
				{
					for ( std::size_t j = 0; j < m_faces[i]->getWinding().numpoints; ++j )
					{
						faceVertices.push_back( FaceVertexId( i, j ) );
					}
				}
			}

			IndexBuffer uniqueEdgeIndices;
			typedef VertexBuffer<ProximalVertex> UniqueEdges;
			UniqueEdges uniqueEdges;

			uniqueEdgeIndices.reserve( faceVertices.size() );
			uniqueEdges.reserve( faceVertices.size() );

			{
				ProximalVertexArray edgePairs;
				edgePairs.resize( faceVertices.size() );

				{
					for ( std::size_t i = 0; i < faceVertices.size(); ++i )
					{
						edgePairs[i].m_next = edgePairs.data() + absoluteIndex( next_edge( m_faces, faceVertices[i] ) );
					}
				}

				{
					UniqueVertexBuffer<ProximalVertex> inserter( uniqueEdges );
					for ( ProximalVertexArray::iterator i = edgePairs.begin(); i != edgePairs.end(); ++i )
					{
						uniqueEdgeIndices.insert( inserter.insert( ProximalVertex( &( *i ) ) ) );
					}
				}

				{
					edge_clear();
					m_select_edges.reserve( uniqueEdges.size() );
					for ( UniqueEdges::iterator i = uniqueEdges.begin(); i != uniqueEdges.end(); ++i )
					{
						edge_push_back( faceVertices[ProximalVertexArray_index( edgePairs, *i )] );
					}
				}

				{
					m_edge_faces.resize( uniqueEdges.size() );
					for ( std::size_t i = 0; i < uniqueEdges.size(); ++i )
					{
						FaceVertexId faceVertex = faceVertices[ProximalVertexArray_index( edgePairs, uniqueEdges[i] )];
						m_edge_faces[i] = EdgeFaces( faceVertex.getFace(), m_faces[faceVertex.getFace()]->getWinding()[faceVertex.getVertex()].adjacent );
					}
				}

				{
					m_uniqueEdgePoints.resize( uniqueEdges.size() );
					for ( std::size_t i = 0; i < uniqueEdges.size(); ++i )
					{
						FaceVertexId faceVertex = faceVertices[ProximalVertexArray_index( edgePairs, uniqueEdges[i] )];

						const Winding& w = m_faces[faceVertex.getFace()]->getWinding();
						Vector3 edge = vector3_mid( w[faceVertex.getVertex()].vertex, w[Winding_next( w, faceVertex.getVertex() )].vertex );
						m_uniqueEdgePoints[i] = pointvertex_for_windingpoint( edge, colour_vertex );
					}
				}

			}


			IndexBuffer uniqueVertexIndices;
			typedef VertexBuffer<ProximalVertex> UniqueVertices;
			UniqueVertices uniqueVertices;

			uniqueVertexIndices.reserve( faceVertices.size() );
			uniqueVertices.reserve( faceVertices.size() );

			{
				ProximalVertexArray vertexRings;
				vertexRings.resize( faceVertices.size() );

				{
					for ( std::size_t i = 0; i < faceVertices.size(); ++i )
					{
						vertexRings[i].m_next = vertexRings.data() + absoluteIndex( next_vertex( m_faces, faceVertices[i] ) );
					}
				}

				{
					UniqueVertexBuffer<ProximalVertex> inserter( uniqueVertices );
					for ( ProximalVertexArray::iterator i = vertexRings.begin(); i != vertexRings.end(); ++i )
					{
						uniqueVertexIndices.insert( inserter.insert( ProximalVertex( &( *i ) ) ) );
					}
				}

				{
					vertex_clear();
					m_select_vertices.reserve( uniqueVertices.size() );
					for ( UniqueVertices::iterator i = uniqueVertices.begin(); i != uniqueVertices.end(); ++i )
					{
						vertex_push_back( faceVertices[ProximalVertexArray_index( vertexRings, ( *i ) )] );
					}
				}

				{
					m_uniqueVertexPoints.resize( uniqueVertices.size() );
					for ( std::size_t i = 0; i < uniqueVertices.size(); ++i )
					{
						FaceVertexId faceVertex = faceVertices[ProximalVertexArray_index( vertexRings, uniqueVertices[i] )];

						const Winding& winding = m_faces[faceVertex.getFace()]->getWinding();
						m_uniqueVertexPoints[i] = depthtested_pointvertex_for_windingpoint( winding[faceVertex.getVertex()].vertex, colour_vertex );
					}
				}
			}

			if ( ( uniqueVertices.size() + faces_size ) - uniqueEdges.size() != 2 ) {
				globalErrorStream() << "Final B-Rep: inconsistent vertex count\n";
			}

#if BRUSH_CONNECTIVITY_DEBUG
			if ( ( uniqueVertices.size() + faces_size ) - uniqueEdges.size() != 2 ) {
				for ( Faces::iterator i = m_faces.begin(); i != m_faces.end(); ++i )
				{
					std::size_t faceIndex = std::distance( m_faces.begin(), i );

					if ( !( *i )->contributes() ) {
						globalOutputStream() << "face: " << Unsigned( faceIndex ) << " does not contribute\n";
					}

					Winding_printConnectivity( ( *i )->getWinding() );
				}
			}
#endif

			// edge-index list for wireframe rendering
			{
				m_edge_indices.resize( uniqueEdgeIndices.size() );

				for ( std::size_t i = 0, count = 0; i < m_faces.size(); ++i )
				{
					const Winding& winding = m_faces[i]->getWinding();
					for ( std::size_t j = 0; j < winding.numpoints; ++j )
					{
						const RenderIndex edge_index = uniqueEdgeIndices[count + j];

						m_edge_indices[edge_index].first = uniqueVertexIndices[count + j];
						m_edge_indices[edge_index].second = uniqueVertexIndices[count + Winding_next( winding, j )];
					}
					count += winding.numpoints;
				}
			}
		}

		{
			m_faceCentroidPoints.resize( m_faces.size() );
			for ( std::size_t i = 0; i < m_faces.size(); ++i )
			{
				m_faces[i]->construct_centroid();
				m_faceCentroidPoints[i] = pointvertex_for_windingpoint( m_faces[i]->centroid(), colour_vertex );
			}
		}

		if( m_vertexModeOn ){
			for ( Observers::iterator o = m_observers.begin(); o != m_observers.end(); ++o )
				( *o )->vertex_select();
		}
	}
	m_BRep_evaluation = false;
}


class FaceFilterWrapper : public Filter
{
FaceFilter& m_filter;
bool m_active;
bool m_invert;
public:
FaceFilterWrapper( FaceFilter& filter, bool invert ) :
	m_filter( filter ),
	m_invert( invert ){
}
void setActive( bool active ){
	m_active = active;
}
bool active(){
	return m_active;
}
bool filter( const Face& face ){
	return m_invert ^ m_filter.filter( face );
}
};


typedef std::list<FaceFilterWrapper> FaceFilters;
FaceFilters g_faceFilters;

void add_face_filter( FaceFilter& filter, int mask, bool invert ){
	g_faceFilters.push_back( FaceFilterWrapper( filter, invert ) );
	GlobalFilterSystem().addFilter( g_faceFilters.back(), mask );
}

bool face_filtered( Face& face ){
	for ( FaceFilters::iterator i = g_faceFilters.begin(); i != g_faceFilters.end(); ++i )
	{
		if ( ( *i ).active() && ( *i ).filter( face ) ) {
			return true;
		}
	}
	return false;
}


class BrushFilterWrapper : public Filter
{
bool m_active;
bool m_invert;
BrushFilter& m_filter;
public:
BrushFilterWrapper( BrushFilter& filter, bool invert ) : m_invert( invert ), m_filter( filter ){
}
void setActive( bool active ){
	m_active = active;
}
bool active(){
	return m_active;
}
bool filter( const Brush& brush ){
	return m_invert ^ m_filter.filter( brush );
}
};


typedef std::list<BrushFilterWrapper> BrushFilters;
BrushFilters g_brushFilters;

void add_brush_filter( BrushFilter& filter, int mask, bool invert ){
	g_brushFilters.push_back( BrushFilterWrapper( filter, invert ) );
	GlobalFilterSystem().addFilter( g_brushFilters.back(), mask );
}

bool brush_filtered( Brush& brush ){
	for ( BrushFilters::iterator i = g_brushFilters.begin(); i != g_brushFilters.end(); ++i )
	{
		if ( ( *i ).active() && ( *i ).filter( brush ) ) {
			return true;
		}
	}
	return false;
}


class VertexModePlane
{
public:
	Plane3 m_plane;
	const Face* m_face;
	const Brush::VertexModeVertex* const m_v[3];
	bool m_transformed;
	VertexModePlane( const Plane3& plane, const Face* face,
					const Brush::VertexModeVertex* v1, const Brush::VertexModeVertex* v2, const Brush::VertexModeVertex* v3,
					bool transformed ) : m_plane( plane ), m_face( face ), m_v{ v1, v2, v3 }, m_transformed( transformed ){
	}
};

class VertexModePlanes
{
	typedef std::vector<VertexModePlane> Planes;
	Planes m_planes;
public:
	typedef Planes::const_iterator const_iterator;
	typedef Planes::iterator iterator;
	void push_back( const VertexModePlane& plane ){
		m_planes.push_back( plane );
	}
	iterator find( const Plane3& plane ){
		iterator i = begin();
		for( ; i != end(); ++i )
			if( plane3_equal( plane, i->m_plane ) )
				break;
		return i;
	}
	const_iterator begin() const {
		return m_planes.begin();
	}
	const_iterator end() const {
		return m_planes.end();
	}
	iterator begin() {
		return m_planes.begin();
	}
	iterator end() {
		return m_planes.end();
	}
	std::size_t size() const {
		return m_planes.size();
	}
};

const Face* vertex_mode_find_common_face( const Brush::VertexModeVertex& v1, const Brush::VertexModeVertex& v2, const Brush::VertexModeVertex& v3 ){
	const Face* face = 0;
	for( const auto& i : v1.m_faces ){
		if( std::find( v2.m_faces.begin(), v2.m_faces.end(), i ) != v2.m_faces.end()
			&& std::find( v3.m_faces.begin(), v3.m_faces.end(), i ) != v3.m_faces.end() ){
			face = i;
			break;
		}
	}
	return face;
}

#include "quickhull/QuickHull.hpp"
void Brush::vertexModeBuildHull( bool allTransformed /*= false*/ ){
	quickhull::QuickHull<double> quickhull;
	std::vector<quickhull::Vector3<double>> pointCloud;
	pointCloud.reserve( m_vertexModeVertices.size() );
	for( auto& i : m_vertexModeVertices ){
		pointCloud.push_back( quickhull::Vector3<double>( static_cast<double>( i.m_vertexTransformed.x() ),
														static_cast<double>( i.m_vertexTransformed.y() ),
														static_cast<double>( i.m_vertexTransformed.z() ) ) );
	}
	auto hull = quickhull.getConvexHull( pointCloud, true, true );
	const auto& indexBuffer = hull.getIndexBuffer();
	const size_t triangleCount = indexBuffer.size() / 3;
	VertexModePlanes vertexModePlanes;
	for( size_t i = 0; i < triangleCount; ++i ) {
		const Brush::VertexModeVertex* v[3];
		bool transformed = allTransformed;
		for( size_t j = 0; j < 3; ++j ){
			v[j] = &m_vertexModeVertices[indexBuffer[i * 3 + j]];
			transformed |= v[j]->m_selected;
		}
		const Plane3 plane = plane3_for_points( v[0]->m_vertexTransformed, v[1]->m_vertexTransformed, v[2]->m_vertexTransformed );
		if( plane3_valid( plane ) ){
			VertexModePlanes::iterator it = vertexModePlanes.find( plane );
			if( it == vertexModePlanes.end() ){ //not found, add new plane
				const Face* face = vertex_mode_find_common_face( *v[0], *v[1], *v[2] );
				if( !face ){ //no common face, use some
					face = v[0]->m_faces[0];
					transformed = true;
				}
				if( vector3_dot( plane.normal(), face->getPlane().plane3().normal() ) < 0 ){ //likely reversed plane
					transformed = true;
				}
				vertexModePlanes.push_back( VertexModePlane( plane, face, v[0], v[1], v[2], transformed ) );
			}
			else{
				it->m_transformed |= transformed;
			}
		}
	}

	if( vertexModePlanes.size() >=4 ){ //avoid obvious transform to degenerate
		const bool isdetail = isDetail();
		clear();
		for( const auto& i : vertexModePlanes ){
			const Face& face = *i.m_face;
			if( i.m_transformed ){
				TextureProjection projection( face.getTexdef().m_projection );
				if( g_brush_textureVertexlock_enabled ){
					Matrix4 local2tex;
					Texdef_Construct_local2tex( face.getTexdef().m_projection, face.getShader().width(), face.getShader().height(), face.getPlane().plane3().normal(), local2tex );
					const DoubleVector3 st[3]{ matrix4_transformed_point( local2tex, i.m_v[0]->m_vertex ),
												matrix4_transformed_point( local2tex, i.m_v[1]->m_vertex ),
												matrix4_transformed_point( local2tex, i.m_v[2]->m_vertex ) };
					const DoubleVector3 points[3]{ i.m_v[0]->m_vertexTransformed, i.m_v[1]->m_vertexTransformed, i.m_v[2]->m_vertexTransformed };
					Texdef_from_ST( projection, points, st, face.getShader().width(), face.getShader().height() );
				}
				Face* newFace = addPlane( i.m_v[0]->m_vertexTransformed, i.m_v[1]->m_vertexTransformed, i.m_v[2]->m_vertexTransformed, face.GetShader(), TextureProjection() );
				if( newFace ){
					newFace->getTexdef().m_projection = projection; //set TextureProjection later, addPlane() resets Valve220 basis
					newFace->revertTexdef();
					newFace->setDetail( isdetail );
				}
			}
			else{
				addFace( face );
			}
		}
	}
}


void Brush::vertexModeTransform( const Matrix4& matrix ){
	for( auto& i : m_vertexModeVertices )
		if( i.m_selected )
			i.m_vertexTransformed = matrix4_transformed_point( matrix, i.m_vertex );
	vertexModeBuildHull();
}
void Brush::vertexModeSnap( const float snap, bool all ){
	for( auto& i : m_vertexModeVertices )
		if( all || i.m_selected )
			vector3_snap( i.m_vertexTransformed, snap );
	vertexModeBuildHull( all );
}

#include "grid.h"
void BrushInstance::transformComponents( const Matrix4& matrix ){
	auto transform = [this]( const Matrix4& matrix ){
		for ( auto& fi : m_faceInstances )
			fi.transformComponents( matrix );
	};

	transform( matrix );

	const Vector3 translation = matrix4_get_translation_vec3( matrix );
	if( translation != g_vector3_identity ){ //has translation
		Matrix4 ma( matrix );
		Vector3& tra = vector4_to_vector3( ma.t() );
		tra = g_vector3_identity;
		if( g_matrix4_identity == ma ){ //only translation
			for ( const auto& fi : m_faceInstances ){
				if( fi.isSelected() ){ //has faces selected
					if( !m_brush.contributes() ){ //do binary search of worthy transform
						for( std::size_t axis = 0; axis < 3; ++axis ){
							const float grid = translation[axis] < 0? -GetGridSize() : GetGridSize();
							int maxI = static_cast<int>( translation[axis] / grid + .5f );
							int minI = 0;
							while( maxI > minI ){
								const int curI = minI + ( maxI - minI + 1 ) / 2;
								tra[axis] = curI * grid;
								m_brush.revertTransform();
								transform( ma );
								if( m_brush.contributes() ){
									minI = curI;
								}
								else{
									maxI = curI - 1;
								}
							}
							tra[axis] = minI * grid;
						}
						m_brush.revertTransform();
						transform( ma );
					}
					break;
				}
			}
		}
	}
}

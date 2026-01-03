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

#include "generic/vector.h"
#include "selectable.h"
#include "math/line.h"
#include "view.h"
#include "math/frustum.h"
#include "render.h"
#include "selection_debug.h"

enum class EClipCull
{
	None,
	CW,
	CCW,
};

typedef Vector3 point_t;
typedef const Vector3* point_iterator_t;

// crossing number test for a point in a polygon
// This code is patterned after [Franklin, 2000]
inline bool point_test_polygon_2d( const point_t& P, point_iterator_t start, point_iterator_t finish ){
	std::size_t crossings = 0;

	// loop through all edges of the polygon
	for ( point_iterator_t prev = finish - 1, cur = start; cur != finish; prev = cur, ++cur )
	{	// edge from (*prev) to (*cur)
		if ( ( ( ( *prev )[1] <= P[1] ) && ( ( *cur )[1] > P[1] ) ) // an upward crossing
		  || ( ( ( *prev )[1] > P[1] ) && ( ( *cur )[1] <= P[1] ) ) ) { // a downward crossing
			// compute the actual edge-ray intersect x-coordinate
			const float vt = ( P[1] - ( *prev )[1] ) / ( ( *cur )[1] - ( *prev )[1] );
			if ( P[0] < ( *prev )[0] + vt * ( ( *cur )[0] - ( *prev )[0] ) ) { // P[0] < intersect
				++crossings; // a valid crossing of y=P[1] right of P[0]
			}
		}
	}
	return ( crossings & 0x1 ) != 0; // 0 if even (out), and 1 if odd (in)
}

inline double triangle_signed_area_XY( const Vector3& p0, const Vector3& p1, const Vector3& p2 ){
	return ( ( p1[0] - p0[0] ) * ( p2[1] - p0[1] ) ) - ( ( p2[0] - p0[0] ) * ( p1[1] - p0[1] ) );
}


inline SelectionIntersection select_point_from_clipped( Vector4& clipped ){
	return SelectionIntersection( clipped[2] / clipped[3], vector3_length_squared( Vector3( clipped[0] / clipped[3], clipped[1] / clipped[3], 0 ) ) );
}

inline void BestPoint( std::size_t count, Vector4 clipped[9], SelectionIntersection& best, EClipCull cull, const Plane3* plane = 0 ){
	Vector3 normalised[9];

	{
		for ( std::size_t i = 0; i < count; ++i )
		{
			normalised[i][0] = clipped[i][0] / clipped[i][3];
			normalised[i][1] = clipped[i][1] / clipped[i][3];
			normalised[i][2] = clipped[i][2] / clipped[i][3];
		}
	}

	if ( cull != EClipCull::None && count > 2 ) {
		double signed_area = triangle_signed_area_XY( normalised[0], normalised[1], normalised[2] );

		if ( ( cull == EClipCull::CW  && signed_area > 0 )
		  || ( cull == EClipCull::CCW && signed_area < 0 ) ) {
			return;
		}
	}

	if ( count == 2 ) {
		const Vector3 point = line_closest_point( Line( normalised[0], normalised[1] ), Vector3( 0, 0, 0 ) );
		assign_if_closer( best, SelectionIntersection( point.z(), vector3_length_squared( Vector3( point.x(), point.y(), 0 ) ) ) );
	}
	else if ( count > 2 && !point_test_polygon_2d( Vector3( 0, 0, 0 ), normalised, normalised + count ) ) {
		Plane3 plaine;
		if( !plane ){
			plaine = plane3_for_points( normalised[0], normalised[1], normalised[2] );
			plane = &plaine;
		}
//globalOutputStream() << plane.a << ' ' << plane.b << ' ' << plane.c << ' ' << '\n';
		const point_iterator_t end = normalised + count;
		for ( point_iterator_t previous = end - 1, current = normalised; current != end; previous = current, ++current )
		{
			Vector3 point = line_closest_point( Line( *previous, *current ), Vector3( 0, 0, 0 ) );
			const float depth = point.z();
			point.z() = 0;
			const float distance = vector3_length_squared( point );

			if( plane->c == 0 ){
				assign_if_closer( best, SelectionIntersection( depth, distance ) );
			}
			else{
				assign_if_closer( best, SelectionIntersection( depth, distance, ray_distance_to_plane(
				                      Ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 1 ) ),
				                      *plane
				                  ) ) );
//										globalOutputStream() << ray_distance_to_plane(
//										Ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 1 ) ),
//										plane
//										) << '\n';
			}
		}
	}
	else if ( count > 2 ) {
		Plane3 plaine;
		if( !plane ){
			plaine = plane3_for_points( normalised[0], normalised[1], normalised[2] );
			plane = &plaine;
		}
		assign_if_closer(
		    best,
		    SelectionIntersection(
		        ray_distance_to_plane(
		            Ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 1 ) ),
		            *plane
		        ),
		        0,
		        ray_distance_to_plane(
		            Ray( Vector3( 10, 8, 0 ), Vector3( 0, 0, 1 ) ),
		            *plane
		        )
		    )
		);
	}

#if defined( DEBUG_SELECTION )
	if ( count >= 2 ) {
		g_render_clipped.insert( clipped, count );
	}
#endif
}

inline void Point_BestPoint( const Matrix4& local2view, const PointVertex& vertex, SelectionIntersection& best ){
	Vector4 clipped;
	if ( matrix4_clip_point( local2view, vertex3f_to_vector3( vertex.vertex ), clipped ) == c_CLIP_PASS ) {
		assign_if_closer( best, select_point_from_clipped( clipped ) );
	}
}

inline void LineStrip_BestPoint( const Matrix4& local2view, const PointVertex* vertices, const std::size_t size, SelectionIntersection& best ){
	Vector4 clipped[2];
	for ( std::size_t i = 0; ( i + 1 ) < size; ++i )
	{
		const std::size_t count = matrix4_clip_line( local2view, vertex3f_to_vector3( vertices[i].vertex ), vertex3f_to_vector3( vertices[i + 1].vertex ), clipped );
		BestPoint( count, clipped, best, EClipCull::None );
	}
}

inline void LineLoop_BestPoint( const Matrix4& local2view, const PointVertex* vertices, const std::size_t size, SelectionIntersection& best ){
	Vector4 clipped[2];
	for ( std::size_t i = 0; i < size; ++i )
	{
		const std::size_t count = matrix4_clip_line( local2view, vertex3f_to_vector3( vertices[i].vertex ), vertex3f_to_vector3( vertices[( i + 1 ) % size].vertex ), clipped );
		BestPoint( count, clipped, best, EClipCull::None );
	}
}

inline void Line_BestPoint( const Matrix4& local2view, const PointVertex vertices[2], SelectionIntersection& best ){
	Vector4 clipped[2];
	const std::size_t count = matrix4_clip_line( local2view, vertex3f_to_vector3( vertices[0].vertex ), vertex3f_to_vector3( vertices[1].vertex ), clipped );
	BestPoint( count, clipped, best, EClipCull::None );
}

inline void Circle_BestPoint( const Matrix4& local2view, EClipCull cull, const PointVertex* vertices, const std::size_t size, SelectionIntersection& best ){
	Vector4 clipped[9];
	for ( std::size_t i = 0; i < size; ++i )
	{
		const std::size_t count = matrix4_clip_triangle( local2view, g_vector3_identity, vertex3f_to_vector3( vertices[i].vertex ), vertex3f_to_vector3( vertices[( i + 1 ) % size].vertex ), clipped );
		BestPoint( count, clipped, best, cull );
	}
}

inline void Quad_BestPoint( const Matrix4& local2view, EClipCull cull, const PointVertex* vertices, SelectionIntersection& best ){
	Vector4 clipped[9];
	{
		const std::size_t count = matrix4_clip_triangle( local2view, vertex3f_to_vector3( vertices[0].vertex ), vertex3f_to_vector3( vertices[1].vertex ), vertex3f_to_vector3( vertices[3].vertex ), clipped );
		BestPoint( count, clipped, best, cull );
	}
	{
		const std::size_t count = matrix4_clip_triangle( local2view, vertex3f_to_vector3( vertices[1].vertex ), vertex3f_to_vector3( vertices[2].vertex ), vertex3f_to_vector3( vertices[3].vertex ), clipped );
		BestPoint( count, clipped, best, cull );
	}
}

inline void AABB_BestPoint( const Matrix4& local2view, EClipCull cull, const AABB& aabb, SelectionIntersection& best ){
	const IndexPointer::index_type indices_[24] = {
		2, 1, 5, 6,
		1, 0, 4, 5,
		0, 1, 2, 3,
		3, 7, 4, 0,
		3, 2, 6, 7,
		7, 6, 5, 4,
	};

	const std::array<Vector3, 8> points = aabb_corners( aabb );

	const IndexPointer indices( indices_, 24 );

	Vector4 clipped[9];
	for ( IndexPointer::iterator i( indices.begin() ); i != indices.end(); i += 4 )
	{
		BestPoint(
		    matrix4_clip_triangle(
		        local2view,
		        points[*i],
		        points[*( i + 1 )],
		        points[*( i + 3 )],
		        clipped
		    ),
		    clipped,
		    best,
		    cull
		);
		BestPoint(
		    matrix4_clip_triangle(
		        local2view,
		        points[*( i + 1 )],
		        points[*( i + 2 )],
		        points[*( i + 3 )],
		        clipped
		    ),
		    clipped,
		    best,
		    cull
		);
	}
}


typedef FlatShadedVertex* FlatShadedVertexIterator;
inline void Triangles_BestPoint( const Matrix4& local2view, EClipCull cull, FlatShadedVertexIterator first, FlatShadedVertexIterator last, SelectionIntersection& best ){
	for ( FlatShadedVertexIterator x( first ), y( first + 1 ), z( first + 2 ); x != last; x += 3, y += 3, z += 3 )
	{
		Vector4 clipped[9];
		BestPoint(
		    matrix4_clip_triangle(
		        local2view,
		        reinterpret_cast<const Vector3&>( ( *x ).vertex ),
		        reinterpret_cast<const Vector3&>( ( *y ).vertex ),
		        reinterpret_cast<const Vector3&>( ( *z ).vertex ),
		        clipped
		    ),
		    clipped,
		    best,
		    cull
		);
	}
}


class SelectionVolume : public SelectionTest
{
	Matrix4 m_local2view;
	const View& m_view;
	EClipCull m_cull;
#if 0
	Vector3 m_near;
	Vector3 m_far;
#endif
	Matrix4 m_screen2world;
public:
	SelectionVolume( const View& view )
		: m_view( view ){
	}

	const VolumeTest& getVolume() const override {
		return m_view;
	}
#if 0
	const Vector3& getNear() const override {
		return m_near;
	}
	const Vector3& getFar() const override {
		return m_far;
	}
#endif
	const Matrix4& getScreen2world() const override {
		return m_screen2world;
	}

	void BeginMesh( const Matrix4& localToWorld, bool twoSided ) override {
		m_local2view = matrix4_multiplied_by_matrix4( m_view.GetViewMatrix(), localToWorld );

		// Cull back-facing polygons based on winding being clockwise or counter-clockwise.
		// Don't cull if the view is wireframe and the polygons are two-sided.
		m_cull = twoSided && !m_view.fill() ? EClipCull::None : ( matrix4_handedness( localToWorld ) == MATRIX4_RIGHTHANDED ) ? EClipCull::CW : EClipCull::CCW;

		{
			m_screen2world = matrix4_full_inverse( m_local2view );
#if 0
			m_near = vector4_projected(
			             matrix4_transformed_vector4(
			                 m_screen2world,
			                 Vector4( 0, 0, -1, 1 )
			             )
			         );

			m_far = vector4_projected(
			            matrix4_transformed_vector4(
			                m_screen2world,
			                Vector4( 0, 0, 1, 1 )
			            )
			        );
#endif
		}

#if defined( DEBUG_SELECTION )
		g_render_clipped.construct( m_view.GetViewMatrix() );
#endif
	}
	void TestPoint( const Vector3& point, SelectionIntersection& best ) override {
		Vector4 clipped;
		if ( matrix4_clip_point( m_local2view, point, clipped ) == c_CLIP_PASS ) {
			best = select_point_from_clipped( clipped );
		}
	}
	void TestPolygon( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best, const PlanePoints& planepoints ) override {
		const PlanePoints pts {
			vector4_projected( matrix4_transformed_vector4( m_local2view, BasicVector4<double>( planepoints[0], 1 ) ) ),
			vector4_projected( matrix4_transformed_vector4( m_local2view, BasicVector4<double>( planepoints[1], 1 ) ) ),
			vector4_projected( matrix4_transformed_vector4( m_local2view, BasicVector4<double>( planepoints[2], 1 ) ) )
		};
		const Plane3 planeTransformed( plane3_for_points( pts ) );

		Vector4 clipped[9];
		for ( std::size_t i = 0; i + 2 < count; ++i )
		{
			BestPoint(
			    matrix4_clip_triangle(
			        m_local2view,
			        reinterpret_cast<const DoubleVector3&>( vertices[0] ),
			        reinterpret_cast<const DoubleVector3&>( vertices[i + 1] ),
			        reinterpret_cast<const DoubleVector3&>( vertices[i + 2] ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull,
			    &planeTransformed
			);
		}
	}
	void TestLineLoop( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best ) override {
		if ( count == 0 ) {
			return;
		}
		Vector4 clipped[9];
		for ( VertexPointer::iterator i = vertices.begin(), end = i + count, prev = i + ( count - 1 ); i != end; prev = i, ++i )
		{
			BestPoint(
			    matrix4_clip_line(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( ( *prev ) ),
			        reinterpret_cast<const Vector3&>( ( *i ) ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
		}
	}
	void TestLineStrip( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best ) override {
		if ( count == 0 ) {
			return;
		}
		Vector4 clipped[9];
		for ( VertexPointer::iterator i = vertices.begin(), end = i + count, next = i + 1; next != end; i = next, ++next )
		{
			BestPoint(
			    matrix4_clip_line(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( ( *i ) ),
			        reinterpret_cast<const Vector3&>( ( *next ) ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
		}
	}
	void TestLines( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best ) override {
		if ( count == 0 ) {
			return;
		}
		Vector4 clipped[9];
		for ( VertexPointer::iterator i = vertices.begin(), end = i + count; i != end; i += 2 )
		{
			BestPoint(
			    matrix4_clip_line(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( ( *i ) ),
			        reinterpret_cast<const Vector3&>( ( *( i + 1 ) ) ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
		}
	}
	void TestTriangles( const VertexPointer& vertices, const IndexPointer& indices, SelectionIntersection& best ) override {
		Vector4 clipped[9];
		for ( IndexPointer::iterator i( indices.begin() ); i != indices.end(); i += 3 )
		{
			BestPoint(
			    matrix4_clip_triangle(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( vertices[*i] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
		}
	}
	void TestQuads( const VertexPointer& vertices, const IndexPointer& indices, SelectionIntersection& best ) override {
		Vector4 clipped[9];
		for ( IndexPointer::iterator i( indices.begin() ); i != indices.end(); i += 4 )
		{
			BestPoint(
			    matrix4_clip_triangle(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( vertices[*i] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 3 )] ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
			BestPoint(
			    matrix4_clip_triangle(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 3 )] ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
		}
	}
	void TestQuadStrip( const VertexPointer& vertices, const IndexPointer& indices, SelectionIntersection& best ) override {
		Vector4 clipped[9];
		for ( IndexPointer::iterator i( indices.begin() ); i + 2 != indices.end(); i += 2 )
		{
			BestPoint(
			    matrix4_clip_triangle(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( vertices[*i] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
			BestPoint(
			    matrix4_clip_triangle(
			        m_local2view,
			        reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
			        reinterpret_cast<const Vector3&>( vertices[*( i + 3 )] ),
			        clipped
			    ),
			    clipped,
			    best,
			    m_cull
			);
		}
	}
};

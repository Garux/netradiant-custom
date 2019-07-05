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

#include "winding.h"

#include <algorithm>

#include "math/line.h"


#if 0
#include "math/aabb.h"
void windingTestInfinity(){
	static std::size_t windingTestInfinityI = 0;
	static std::size_t windingTestInfinity_badNormal = 0;
	static std::size_t windingTestInfinity_planeOuttaWorld = 0;
	static std::size_t windingTestInfinity_OK = 0;
	static std::size_t windingTestInfinity_FAIL = 0;
	const double maxWorldCoord = 64 * 1024;
	AABB world( g_vector3_identity, Vector3( maxWorldCoord, maxWorldCoord, maxWorldCoord ) );
	Plane3 worldplanes[6];
	aabb_planes( world, worldplanes );
	world.extents += Vector3( 99, 99, 99 );

	const std::size_t iterations = 9999999;
	if( windingTestInfinityI >= iterations )
		return;

	while( windingTestInfinityI < iterations )
	{
		Plane3 plane;
		plane.d = ( (double)rand() / (double)RAND_MAX ) * maxWorldCoord * 2;
		plane.a = ( (double)rand() / (double)RAND_MAX );
		plane.b = ( (double)rand() / (double)RAND_MAX );
		plane.c = ( (double)rand() / (double)RAND_MAX );
		if( vector3_length( plane.normal() ) != 0 ){
			vector3_normalise( plane.normal() );
		}
		else{
			++windingTestInfinity_badNormal;
			continue;
		}

		FixedWinding buffer[2];
		bool swap = false;

		// get a poly that covers an effectively infinite area
		Winding_createInfinite( buffer[swap], plane, maxWorldCoord * 8.0 );

		// chop the poly by positive world box faces
		for ( std::size_t i = 0; i < 3; ++i )
		{
			if( buffer[swap].points.empty() ){
				break;
			}

			buffer[!swap].clear();

			{
				// flip the plane, because we want to keep the back side
				const Plane3 clipPlane( -g_vector3_axes[i], -maxWorldCoord );
				Winding_Clip( buffer[swap], plane, clipPlane, 0, buffer[!swap] );
			}

			swap = !swap;
		}

		if( buffer[swap].points.empty() ){
			++windingTestInfinity_planeOuttaWorld;
			continue;
		}

		++windingTestInfinityI;

		FixedWinding winding;
//		Winding_createInfinite( winding, plane, maxWorldCoord * sqrt( 2.75 ) ); //is ok for normalized vecs inside of Winding_createInfinite
		Winding_createInfinite( winding, plane, maxWorldCoord * 2.22 ); //ok for no normalization

		std::size_t i = 0;
		for( ; i < winding.size(); ++i ){
			for( std::size_t j = 0; j < 6; ++j ){
				if( vector3_dot( winding[i].edge.direction, worldplanes[j].normal() ) != 0 ){
					const DoubleVector3 v = ray_intersect_plane( winding[i].edge, worldplanes[j] );
					if( aabb_intersects_point( world, v ) ){
//						globalWarningStream() << "   INFINITE POINT INSIDE WORLD\n";
						++windingTestInfinity_FAIL;
						goto fail;
					}
				}
			}
		}
		if( i == winding.size() ){
			++windingTestInfinity_OK;
		}
		fail:
			;
	}

	globalWarningStream() << windingTestInfinity_badNormal << " windingTestInfinity_badNormal\n";
	globalWarningStream() << windingTestInfinity_planeOuttaWorld << " windingTestInfinity_planeOuttaWorld\n";
	globalWarningStream() << windingTestInfinity_OK << " windingTestInfinity_OK\n";
	globalWarningStream() << windingTestInfinity_FAIL << " windingTestInfinity_FAIL\n";

}
#endif


/// \brief Keep the value of \p infinity as small as possible to improve precision in Winding_Clip.
void Winding_createInfinite( FixedWinding& winding, const Plane3& plane, double infinity ){
#if 0
	double max = -infinity;
	int x = -1;
	for ( int i = 0 ; i < 3; i++ )
	{
		double d = fabs( plane.normal()[i] );
		if ( d > max ) {
			x = i;
			max = d;
		}
	}
	if ( x == -1 ) {
		globalErrorStream() << "invalid plane\n";
		return;
	}

	DoubleVector3 vup = g_vector3_identity;
	switch ( x )
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}


	vector3_add( vup, vector3_scaled( plane.normal(), -vector3_dot( vup, plane.normal() ) ) );
	vector3_normalise( vup );

	DoubleVector3 org = vector3_scaled( plane.normal(), plane.dist() );

	DoubleVector3 vright = vector3_cross( vup, plane.normal() );

	vector3_scale( vup, infinity );
	vector3_scale( vright, infinity );

	// project a really big  axis aligned box onto the plane

	DoubleRay r1, r2, r3, r4;
	r1.origin = vector3_added( vector3_subtracted( org, vright ), vup );
	r1.direction = vector3_normalised( vright );
	winding.push_back( FixedWindingVertex( r1.origin, r1, c_brush_maxFaces ) );
	r2.origin = vector3_added( vector3_added( org, vright ), vup );
	r2.direction = vector3_normalised( vector3_negated( vup ) );
	winding.push_back( FixedWindingVertex( r2.origin, r2, c_brush_maxFaces ) );
	r3.origin = vector3_subtracted( vector3_added( org, vright ), vup );
	r3.direction = vector3_normalised( vector3_negated( vright ) );
	winding.push_back( FixedWindingVertex( r3.origin, r3, c_brush_maxFaces ) );
	r4.origin = vector3_subtracted( vector3_subtracted( org, vright ), vup );
	r4.direction = vector3_normalised( vup );
	winding.push_back( FixedWindingVertex( r4.origin, r4, c_brush_maxFaces ) );
#else
	const auto normal = plane.normal();
	const auto maxi = vector3_max_abs_component_index( normal );

	if ( !std::isnormal( normal[maxi] ) ) {
		globalErrorStream() << "invalid plane\n";
		return;
	}

	const DoubleVector3 vup0 = ( maxi == 2 )? DoubleVector3( 0, -normal[2], normal[1] )
									: DoubleVector3( -normal[1], normal[0], 0 );
	const DoubleVector3 vright0 = vector3_cross( vup0, normal );
	const DoubleVector3 org = normal * plane.dist();

	const DoubleVector3 vup = vup0 * infinity * 2.22;
	const DoubleVector3 vright = vright0 * infinity * 2.22;

	// project a really big  axis aligned box onto the plane

	DoubleRay ray;
	ray.origin = org - vright + vup;
	ray.direction = vector3_normalised( vright0 );
	winding.push_back( FixedWindingVertex( ray.origin, ray, c_brush_maxFaces ) );
	ray.origin = org + vright + vup;
	ray.direction = vector3_normalised( -vup0 );
	winding.push_back( FixedWindingVertex( ray.origin, ray, c_brush_maxFaces ) );
	ray.origin = org + vright - vup;
	ray.direction = vector3_normalised( -vright0 );
	winding.push_back( FixedWindingVertex( ray.origin, ray, c_brush_maxFaces ) );
	ray.origin = org - vright - vup;
	ray.direction = vector3_normalised( vup0 );
	winding.push_back( FixedWindingVertex( ray.origin, ray, c_brush_maxFaces ) );
#endif
}


inline PlaneClassification Winding_ClassifyDistance( const double distance, const double epsilon ){
	if ( distance > epsilon ) {
		return ePlaneFront;
	}
	if ( distance < -epsilon ) {
		return ePlaneBack;
	}
	return ePlaneOn;
}

/// \brief Returns true if
/// !flipped && winding is completely BACK or ON
/// or flipped && winding is completely FRONT or ON
bool Winding_TestPlane( const Winding& winding, const Plane3& plane, bool flipped ){
	const int test = ( flipped ) ? ePlaneBack : ePlaneFront;
	for ( Winding::const_iterator i = winding.begin(); i != winding.end(); ++i )
	{
		if ( test == Winding_ClassifyDistance( plane3_distance_to_point( plane, ( *i ).vertex ), ON_EPSILON ) ) {
			return false;
		}
	}
	return true;
}

/// \brief Returns true if any point in \p w1 is in front of plane2, or any point in \p w2 is in front of plane1
bool Winding_PlanesConcave( const Winding& w1, const Winding& w2, const Plane3& plane1, const Plane3& plane2 ){
	return !Winding_TestPlane( w1, plane2, false ) || !Winding_TestPlane( w2, plane1, false );
}

brushsplit_t Winding_ClassifyPlane( const Winding& winding, const Plane3& plane ){
	brushsplit_t split;
	for ( Winding::const_iterator i = winding.begin(); i != winding.end(); ++i )
	{
		++split.counts[Winding_ClassifyDistance( plane3_distance_to_point( plane, ( *i ).vertex ), ON_EPSILON )];
	}
	return split;
}

void WindingVertex_ClassifyPlane( const Vector3& vertex, const Plane3& plane, brushsplit_t& split ){
	++split.counts[Winding_ClassifyDistance( plane3_distance_to_point( plane, vertex ), ON_EPSILON )];
}


const double ON_EPSILON_CLIP = 1.0 / ( 1 << 12 );

/// \brief Clip \p winding which lies on \p plane by \p clipPlane, resulting in \p clipped.
/// If \p winding is completely in front of the plane, \p clipped will be identical to \p winding.
/// If \p winding is completely in back of the plane, \p clipped will be empty.
/// If \p winding intersects the plane, the edge of \p clipped which lies on \p clipPlane will store the value of \p adjacent.
void Winding_Clip( const FixedWinding& winding, const Plane3& plane, const Plane3& clipPlane, std::size_t adjacent, FixedWinding& clipped ){
	PlaneClassification classification = Winding_ClassifyDistance( plane3_distance_to_point( clipPlane, winding.back().vertex ), ON_EPSILON_CLIP );
	PlaneClassification nextClassification;
	// for each edge
	for ( std::size_t next = 0, i = winding.size() - 1; next != winding.size(); i = next, ++next, classification = nextClassification )
	{
		nextClassification = Winding_ClassifyDistance( plane3_distance_to_point( clipPlane, winding[next].vertex ), ON_EPSILON_CLIP );
		const FixedWindingVertex& vertex = winding[i];

		// if first vertex of edge is ON
		if ( classification == ePlaneOn ) {
			// append first vertex to output winding
			if ( nextClassification == ePlaneBack ) {
				// this edge lies on the clip plane
				clipped.push_back( FixedWindingVertex( vertex.vertex, plane3_intersect_plane3( plane, clipPlane ), adjacent ) );
			}
			else
			{
				clipped.push_back( vertex );
			}
			continue;
		}

		// if first vertex of edge is FRONT
		if ( classification == ePlaneFront ) {
			// add first vertex to output winding
			clipped.push_back( vertex );
		}
		// if second vertex of edge is ON
		if ( nextClassification == ePlaneOn ) {
			continue;
		}
		// else if second vertex of edge is same as first
		else if ( nextClassification == classification ) {
			continue;
		}
		// else if first vertex of edge is FRONT and there are only two edges
		else if ( classification == ePlaneFront && winding.size() == 2 ) {
			continue;
		}
		// else first vertex is FRONT and second is BACK or vice versa
		else
		{
			// append intersection point of line and plane to output winding
			DoubleVector3 mid( ray_intersect_plane( vertex.edge, clipPlane ) );

			if ( classification == ePlaneFront ) {
				// this edge lies on the clip plane
				clipped.push_back( FixedWindingVertex( mid, plane3_intersect_plane3( plane, clipPlane ), adjacent ) );
			}
			else
			{
				clipped.push_back( FixedWindingVertex( mid, vertex.edge, vertex.adjacent ) );
			}
		}
	}
}

std::size_t Winding_FindAdjacent( const Winding& winding, std::size_t face ){
	for ( std::size_t i = 0; i < winding.numpoints; ++i )
	{
		ASSERT_MESSAGE( winding[i].adjacent != c_brush_maxFaces, "edge connectivity data is invalid" );
		if ( winding[i].adjacent == face ) {
			return i;
		}
	}
	return c_brush_maxFaces;
}

std::size_t Winding_Opposite( const Winding& winding, const std::size_t index, const std::size_t other ){
	ASSERT_MESSAGE( index < winding.numpoints && other < winding.numpoints, "Winding_Opposite: index out of range" );

	double dist_best = 0;
	std::size_t index_best = c_brush_maxFaces;

	Ray edge( ray_for_points( winding[index].vertex, winding[other].vertex ) );

	for ( std::size_t i = 0; i < winding.numpoints; ++i )
	{
		if ( i == index || i == other ) {
			continue;
		}

		double dist_squared = ray_squared_distance_to_point( edge, winding[i].vertex );

		if ( dist_squared > dist_best ) {
			dist_best = dist_squared;
			index_best = i;
		}
	}
	return index_best;
}

std::size_t Winding_Opposite( const Winding& winding, const std::size_t index ){
	return Winding_Opposite( winding, index, Winding_next( winding, index ) );
}

/// \brief Calculate the \p centroid of the polygon defined by \p winding which lies on plane \p plane.
void Winding_Centroid( const Winding& winding, const Plane3& plane, Vector3& centroid ){
	double area2 = 0, x_sum = 0, y_sum = 0;
	const ProjectionAxis axis = projectionaxis_for_normal( plane.normal() );
	const indexremap_t remap = indexremap_for_projectionaxis( axis );
	for ( std::size_t i = winding.numpoints - 1, j = 0; j < winding.numpoints; i = j, ++j )
	{
		const double ai = static_cast<double>( winding[i].vertex[remap.x] ) * winding[j].vertex[remap.y] - static_cast<double>( winding[j].vertex[remap.x] ) * winding[i].vertex[remap.y];
		area2 += ai;
		x_sum += ( static_cast<double>( winding[j].vertex[remap.x] ) + winding[i].vertex[remap.x] ) * ai;
		y_sum += ( static_cast<double>( winding[j].vertex[remap.y] ) + winding[i].vertex[remap.y] ) * ai;
	}

	centroid[remap.x] = static_cast<float>( x_sum / ( 3 * area2 ) );
	centroid[remap.y] = static_cast<float>( y_sum / ( 3 * area2 ) );
	{
		Ray ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 0 ) );
		ray.origin[remap.x] = centroid[remap.x];
		ray.origin[remap.y] = centroid[remap.y];
		ray.direction[remap.z] = 1;
		centroid[remap.z] = static_cast<float>( ray_distance_to_plane( ray, plane ) );
	}
//	windingTestInfinity();
}

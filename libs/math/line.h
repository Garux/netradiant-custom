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

#if !defined( INCLUDED_MATH_LINE_H )
#define INCLUDED_MATH_LINE_H

/// \file
/// \brief Line data types and related operations.

#include "math/vector.h"
#include "math/plane.h"

/// \brief A line segment defined by a start point and and end point.
class Line
{
public:
Vector3 start, end;

Line(){
}
Line( const Vector3& start_, const Vector3& end_ ) : start( start_ ), end( end_ ){
}
};

inline Vector3 line_closest_point( const Line& line, const Vector3& point ){
	Vector3 v = line.end - line.start;
	Vector3 w = point - line.start;

	double c1 = vector3_dot( w,v );
	if ( c1 <= 0 ) {
		return line.start;
	}

	double c2 = vector3_dot( v,v );
	if ( c2 <= c1 ) {
		return line.end;
	}

	return Vector3( line.start + v * ( c1 / c2 ) );
}


class Segment
{
public:
Vector3 origin, extents;

Segment(){
}
Segment( const Vector3& origin_, const Vector3& extents_ ) :
	origin( origin_ ), extents( extents_ ){
}
};


inline Segment segment_for_startend( const Vector3& start, const Vector3& end ){
	Segment segment;
	segment.origin = vector3_mid( start, end );
	segment.extents = vector3_subtracted( end, segment.origin );
	return segment;
}

inline unsigned int segment_classify_plane( const Segment& segment, const Plane3& plane ){
	double distance_origin = vector3_dot( plane.normal(), segment.origin ) + plane.dist();

	if ( fabs( distance_origin ) < fabs( vector3_dot( plane.normal(), segment.extents ) ) ) {
		return 1; // partially inside
	}
	else if ( distance_origin < 0 ) {
		return 2; // totally inside
	}
	return 0; // totally outside
}


template<typename T>
class BasicRay
{
public:
BasicVector3<T> origin, direction;

BasicRay(){
}
BasicRay( const BasicVector3<T>& origin_, const BasicVector3<T>& direction_ ) :
	origin( origin_ ), direction( direction_ ){
}
};

typedef BasicRay<float> Ray;
typedef BasicRay<double> DoubleRay;

template<typename T>
inline BasicRay<T> ray_for_points( const BasicVector3<T>& origin, const BasicVector3<T>& p2 ){
	return BasicRay<T>( origin, vector3_normalised( vector3_subtracted( p2, origin ) ) );
}

inline void ray_transform( Ray& ray, const Matrix4& matrix ){
	matrix4_transform_point( matrix, ray.origin );
	matrix4_transform_direction( matrix, ray.direction );
}

// closest-point-on-line
inline double ray_squared_distance_to_point( const Ray& ray, const Vector3& point ){
	return vector3_length_squared(
			   vector3_subtracted(
				   point,
				   vector3_added(
					   ray.origin,
					   vector3_scaled(
						   ray.direction,
						   vector3_dot(
							   vector3_subtracted( point, ray.origin ),
							   ray.direction
							   )
						   )
					   )
				   )
			   );
}

inline double ray_distance_to_plane( const Ray& ray, const Plane3& plane ){
	return -plane3_distance_to_point( plane, ray.origin ) / vector3_dot( ray.direction, plane.normal() );
}

/// \brief Returns the point at which \p ray intersects \p plane, or an undefined value if there is no intersection.
template<typename T>
inline BasicVector3<T> ray_intersect_plane( const BasicRay<T>& ray, const Plane3& plane ){
	return ray.origin + vector3_scaled(
			   ray.direction,
			   -plane3_distance_to_point( plane, ray.origin )
			   / vector3_dot( ray.direction, plane.normal() )
			   );
}

/// \brief Returns the infinite line that is the intersection of \p plane and \p other.
inline DoubleRay plane3_intersect_plane3( const Plane3& plane, const Plane3& other ){
	DoubleRay line;
	line.direction = vector3_cross( plane.normal(), other.normal() );
	switch ( vector3_max_abs_component_index( line.direction ) )
	{
	case 0:
		line.origin.x() = 0;
		line.origin.y() = ( -other.dist() * plane.normal().z() - -plane.dist() * other.normal().z() ) / line.direction.x();
		line.origin.z() = ( -plane.dist() * other.normal().y() - -other.dist() * plane.normal().y() ) / line.direction.x();
		break;
	case 1:
		line.origin.x() = ( -plane.dist() * other.normal().z() - -other.dist() * plane.normal().z() ) / line.direction.y();
		line.origin.y() = 0;
		line.origin.z() = ( -other.dist() * plane.normal().x() - -plane.dist() * other.normal().x() ) / line.direction.y();
		break;
	case 2:
		line.origin.x() = ( -other.dist() * plane.normal().y() - -plane.dist() * other.normal().y() ) / line.direction.z();
		line.origin.y() = ( -plane.dist() * other.normal().x() - -other.dist() * plane.normal().x() ) / line.direction.z();
		line.origin.z() = 0;
		break;
	default:
		break;
	}

	return line;
}

#endif

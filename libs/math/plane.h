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

/// \file
/// \brief Plane data types and related operations.

#include "math/matrix.h"

template<typename T>
class Plane3___
{
public:
	T a, b, c, d;

	Plane3___(){
	}
	Plane3___( double _a, double _b, double _c, double _d )
		: a( _a ), b( _b ), c( _c ), d( _d ){
	}
	template<typename Element>
	Plane3___( const BasicVector3<Element>& normal, double dist )
		: a( normal.x() ), b( normal.y() ), c( normal.z() ), d( dist ){
	}
	template<typename Element>
	explicit Plane3___( const Plane3___<Element>& other )
		: a( other.a ), b( other.b ), c( other.c ), d( other.d ){
	}

	BasicVector3<T>& normal(){
		return reinterpret_cast<BasicVector3<T>&>( *this );
	}
	const BasicVector3<T>& normal() const {
		return reinterpret_cast<const BasicVector3<T>&>( *this );
	}
	T& dist(){
		return d;
	}
	const T& dist() const {
		return d;
	}
};

/// \brief A plane equation stored in double-precision floating-point.
using Plane3 = Plane3___<double>;
/// \brief A plane equation stored in single-precision floating-point.
using Plane3f = Plane3___<float>;

inline Plane3 plane3_normalised( const Plane3& plane ){
	double rmagnitude = 1.0 / sqrt( plane.a * plane.a + plane.b * plane.b + plane.c * plane.c );
	return Plane3(
	           plane.a * rmagnitude,
	           plane.b * rmagnitude,
	           plane.c * rmagnitude,
	           plane.d * rmagnitude
	       );
}

inline Plane3 plane3_translated( const Plane3& plane, const Vector3& translation ){
	Plane3 transformed;
	transformed.a = plane.a;
	transformed.b = plane.b;
	transformed.c = plane.c;
	transformed.d = -( ( -plane.d * transformed.a + translation.x() ) * transformed.a +
	                   ( -plane.d * transformed.b + translation.y() ) * transformed.b +
	                   ( -plane.d * transformed.c + translation.z() ) * transformed.c );
	return transformed;
}

inline Plane3 plane3_transformed( const Plane3& plane, const Matrix4& transform ){
	Plane3 transformed;
	transformed.a = transform[0] * plane.a + transform[4] * plane.b + transform[8] * plane.c;
	transformed.b = transform[1] * plane.a + transform[5] * plane.b + transform[9] * plane.c;
	transformed.c = transform[2] * plane.a + transform[6] * plane.b + transform[10] * plane.c;
	transformed.d = -( ( -plane.d * transformed.a + transform[12] ) * transformed.a +
	                   ( -plane.d * transformed.b + transform[13] ) * transformed.b +
	                   ( -plane.d * transformed.c + transform[14] ) * transformed.c );
	return transformed;
}

inline Plane3 plane3_inverse_transformed( const Plane3& plane, const Matrix4& transform ){
	return Plane3
	       (
	           transform[ 0] * plane.a + transform[ 1] * plane.b + transform[ 2] * plane.c + transform[ 3] * plane.d,
	           transform[ 4] * plane.a + transform[ 5] * plane.b + transform[ 6] * plane.c + transform[ 7] * plane.d,
	           transform[ 8] * plane.a + transform[ 9] * plane.b + transform[10] * plane.c + transform[11] * plane.d,
	           transform[12] * plane.a + transform[13] * plane.b + transform[14] * plane.c + transform[15] * plane.d
	       );
}

inline Plane3 plane3_transformed_affine_full( const Plane3& plane, const Matrix4& transform ){
	const DoubleVector3 anchor( matrix4_transformed_point( transform, plane.normal() * plane.dist() ) );
	const DoubleVector3 normal( matrix4_transformed_normal( transform, plane.normal() ) );
	return Plane3( normal, vector3_dot( normal, anchor ) );
}

template<typename T>
inline Plane3___<T> plane3_flipped( const Plane3___<T>& plane ){
	return Plane3___<T>( vector3_negated( plane.normal() ), -plane.dist() );
}

const double c_PLANE_NORMAL_EPSILON = 0.0001f;
const double c_PLANE_DIST_EPSILON = 0.02;

inline bool plane3_equal( const Plane3& self, const Plane3& other ){
	return vector3_equal_epsilon( self.normal(), other.normal(), c_PLANE_NORMAL_EPSILON )
	       && float_equal_epsilon( self.dist(), other.dist(), c_PLANE_DIST_EPSILON );
}

inline bool plane3_opposing( const Plane3& self, const Plane3& other ){
	return plane3_equal( self, plane3_flipped( other ) );
}

inline bool plane3_valid( const Plane3& self ){
	return float_equal_epsilon( vector3_dot( self.normal(), self.normal() ), 1.0, 0.01 );
}

	/*
	* The order of points, when looking from outside the face:
	*
	* 1
	* |
	* |
	* |
	* |
	* 0-----------2
	*/
template<typename Element>
inline Plane3 plane3_for_points( const BasicVector3<Element>& p0, const BasicVector3<Element>& p1, const BasicVector3<Element>& p2 ){
	Plane3 self;
	self.normal() = vector3_normalised( vector3_cross( vector3_subtracted( p2, p0 ), vector3_subtracted( p1, p0 ) ) );
	self.dist() = vector3_dot( p0, self.normal() );
	return self;
}

template<typename Element>
inline Plane3 plane3_for_points( const BasicVector3<Element> planepts[3] ){
	return plane3_for_points( planepts[0], planepts[1], planepts[2] );
}

#include <array>
using PlanePoints = std::array<DoubleVector3, 3>;

inline Plane3 plane3_for_points( const PlanePoints& planepts ){
	return plane3_for_points( planepts[0], planepts[1], planepts[2] );
}

template<typename P, typename V>
inline double plane3_distance_to_point( const Plane3___<P>& plane, const BasicVector3<V>& point ){
	return vector3_dot( point, plane.normal() ) - plane.dist();
}

template<typename T, typename U>
inline BasicVector3<T> plane3_project_point( const Plane3& plane, const BasicVector3<T>& point, const BasicVector3<U>& direction ){
	const double f = vector3_dot( plane.normal(), direction );
	const double d = ( vector3_dot( plane.normal() * plane.dist() - point, plane.normal() ) ) / f;
	return point + direction * d;
}

template<typename P, typename V>
inline BasicVector3<V> plane3_project_point( const Plane3___<P>& plane, const BasicVector3<V>& point ){
	return point - plane.normal() * plane3_distance_to_point( plane, point );
}

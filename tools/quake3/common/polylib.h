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

#pragma once

#include "qmath.h"
#include <vector>

using winding_t = std::vector<Vector3>;

// index < w.size()
template<class T>
size_t winding_next( const std::vector<BasicVector3<T>>& w, size_t index ){
	return ++index == w.size()? 0 : index;
}
template<class T>
const BasicVector3<T>& winding_next_point( const std::vector<BasicVector3<T>>& w, size_t index ){
	return w[ winding_next( w, index ) ];
}
// it < w.end()
template<class T>
std::vector<BasicVector3<T>>::iterator winding_next( std::vector<BasicVector3<T>>& w, typename std::vector<BasicVector3<T>>::iterator it ){
	return ++it == w.end()? w.begin() : it;
}
template<class T>
std::vector<BasicVector3<T>>::const_iterator winding_next( const std::vector<BasicVector3<T>>& w, typename std::vector<BasicVector3<T>>::const_iterator it ){
	return ++it == w.cend()? w.cbegin() : it;
}
// it < w.end()
template<class T>
std::vector<BasicVector3<T>>::iterator winding_prev( std::vector<BasicVector3<T>>& w, typename std::vector<BasicVector3<T>>::iterator it ){
	return it == w.begin()? w.end() - 1 : --it;
}
template<class T>
std::vector<BasicVector3<T>>::const_iterator winding_prev( const std::vector<BasicVector3<T>>& w, typename std::vector<BasicVector3<T>>::const_iterator it ){
	return it == w.cbegin()? w.cend() - 1 : --it;
}

#define MAX_POINTS_ON_WINDING   512

// you can define on_epsilon in the makefile as tighter
#ifndef ON_EPSILON
#define ON_EPSILON  0.1
#endif

enum EPlaneSide
{
	eSideFront = 0, //! in front of plane ---->| *
	eSideBack = 1,  //! behind the  plane -*-->|
	eSideOn = 2,
	eSideCross = 3,
};

enum : bool
{
	eFront = false,
	eBack = true,
};
class ESide
{
	bool m_value;
public:
	ESide(){
	}
	ESide( bool value ) : m_value( value ){}
	operator bool() const {
		return m_value;
	}
};

winding_t   AllocWinding( int points );
float   WindingArea( const winding_t& w );
template<class T> BasicVector3<T> WindingCenter( const std::vector<BasicVector3<T>>& w );
template<class T> BasicVector3<T> WindingCentroid( const std::vector<BasicVector3<T>>& w );
std::pair<winding_t, winding_t>    ClipWindingEpsilon( const winding_t& in, const Plane3f& plane, float epsilon ); // returns { front, back } windings pair
std::pair<winding_t, winding_t>    ClipWindingEpsilonStrict( const winding_t& in, const Plane3f& plane, float epsilon ); // returns { front, back } windings pair
winding_t   ReverseWinding( const winding_t& w );
winding_t   BaseWindingForPlane( const Plane3f& plane );
void    CheckWinding( const winding_t& w );
Plane3f WindingPlane( const winding_t& w );
void    RemoveColinearPoints( winding_t& w );
EPlaneSide     WindingOnPlaneSide( const winding_t& w, const Plane3f& plane );
void WindingExtendBounds( const winding_t& w, MinMax& minmax );

void    AddWindingToConvexHull( const winding_t& w, winding_t& hull, const Vector3& normal );

void    ChopWindingInPlace( winding_t& w, const Plane3f& plane, float epsilon );
// frees the original if clipped

void pw( const winding_t& w );

bool windings_intersect_coplanar( const winding_t& w1, const winding_t& w2, const Plane3& plane );

///////////////////////////////////////////////////////////////////////////////////////
// Below is double-precision stuff.  This was initially needed by the base winding code
// in q3map2 brush processing.
///////////////////////////////////////////////////////////////////////////////////////

using winding_accu_t = std::vector<DoubleVector3>;

winding_accu_t  BaseWindingForPlaneAccu( const Plane3& plane, const DoubleMinMax& minmax );
winding_accu_t  BaseWindingForPlaneAccu( const Plane3& plane );
void    ChopWindingInPlaceAccu( winding_accu_t& w, const Plane3& plane, float epsilon );
winding_t   CopyWindingAccuToRegular( const winding_accu_t& w );

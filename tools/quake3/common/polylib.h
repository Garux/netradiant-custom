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

struct winding_t
{
	int numpoints;
	Vector3 p[];
};

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

winding_t   *AllocWinding( int points );
float   WindingArea( const winding_t *w );
Vector3 WindingCenter( const winding_t *w );
void    ClipWindingEpsilon( winding_t *in, const Plane3f& plane,
                            float epsilon, winding_t **front, winding_t **back );
void    ClipWindingEpsilonStrict( winding_t *in, const Plane3f& plane,
                                  float epsilon, winding_t **front, winding_t **back );
winding_t   *ChopWinding( winding_t *in, const Plane3f& plane );
winding_t   *CopyWinding( const winding_t *w );
winding_t   *ReverseWinding( const winding_t *w );
winding_t   *BaseWindingForPlane( const Plane3f& plane );
void    CheckWinding( winding_t *w );
Plane3f WindingPlane( const winding_t *w );
void    RemoveColinearPoints( winding_t *w );
EPlaneSide     WindingOnPlaneSide( const winding_t *w, const Plane3f& plane );
void    FreeWinding( winding_t *w );
void WindingExtendBounds( const winding_t *w, MinMax& minmax );

void    AddWindingToConvexHull( winding_t *w, winding_t **hull, const Vector3& normal );

void    ChopWindingInPlace( winding_t **w, const Plane3f& plane, float epsilon );
// frees the original if clipped

void pw( winding_t *w );


///////////////////////////////////////////////////////////////////////////////////////
// Below is double-precision stuff.  This was initially needed by the base winding code
// in q3map2 brush processing.
///////////////////////////////////////////////////////////////////////////////////////

struct winding_accu_t
{
	int numpoints;
	DoubleVector3 p[];
};

winding_accu_t  *BaseWindingForPlaneAccu( const Plane3f& plane );
void    ChopWindingInPlaceAccu( winding_accu_t **w, const Plane3f& plane, float epsilon );
winding_t   *CopyWindingAccuToRegular( const winding_accu_t *w );
void    FreeWindingAccu( winding_accu_t *w );

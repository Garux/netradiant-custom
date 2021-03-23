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


#include <stddef.h>

#include "cmdlib.h"
#include "inout.h"
#include "polylib.h"
#include "maxworld.h"

#define BOGUS_RANGE WORLD_SIZE

void pw( winding_t *w ){
	int i;
	for ( i = 0 ; i < w->numpoints ; i++ )
		Sys_Printf( "(%5.1f, %5.1f, %5.1f)\n",w->p[i][0], w->p[i][1],w->p[i][2] );
}


/*
   =============
   AllocWinding
   =============
 */
winding_t   *AllocWinding( int points ){
	if ( points >= MAX_POINTS_ON_WINDING ) {
		Error( "AllocWinding failed: MAX_POINTS_ON_WINDING exceeded" );
	}
	return safe_calloc( offsetof( winding_t, p[points] ) );
}

/*
   =============
   AllocWindingAccu
   =============
 */
winding_accu_t *AllocWindingAccu( int points ){
	if ( points >= MAX_POINTS_ON_WINDING ) {
		Error( "AllocWindingAccu failed: MAX_POINTS_ON_WINDING exceeded" );
	}
	return safe_calloc( offsetof( winding_accu_t, p[points] ) );
}

/*
   =============
   FreeWinding
   =============
 */
void FreeWinding( winding_t *w ){
	if ( !w ) {
		Error( "FreeWinding: winding is NULL" );
	}

	if ( *(unsigned *)w == 0xdeaddead ) {
		Error( "FreeWinding: freed a freed winding" );
	}
	*(unsigned *)w = 0xdeaddead;

	free( w );
}

/*
   =============
   FreeWindingAccu
   =============
 */
void FreeWindingAccu( winding_accu_t *w ){
	if ( !w ) {
		Error( "FreeWindingAccu: winding is NULL" );
	}

	if ( *( (unsigned *) w ) == 0xdeaddead ) {
		Error( "FreeWindingAccu: freed a freed winding" );
	}
	*( (unsigned *) w ) = 0xdeaddead;

	free( w );
}

/*
   ============
   RemoveColinearPoints
   ============
 */
void    RemoveColinearPoints( winding_t *w ){
	int i, j, k;
	int nump;
	Vector3 p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for ( i = 0 ; i < w->numpoints ; i++ )
	{
		j = ( i + 1 ) % w->numpoints;
		k = ( i + w->numpoints - 1 ) % w->numpoints;
		if ( vector3_dot( VectorNormalized( w->p[j] - w->p[i] ), VectorNormalized( w->p[j] - w->p[k] ) ) < 0.999 ) {
			p[nump] = w->p[i];
			nump++;
		}
	}

	if ( nump == w->numpoints ) {
		return;
	}

	w->numpoints = nump;
	memcpy( w->p, p, nump * sizeof( p[0] ) );
}

/*
   ============
   WindingPlane
   ============
 */
Plane3f WindingPlane( const winding_t *w ){
	Plane3f plane;
	PlaneFromPoints( plane, w->p[0], w->p[1], w->p[2] );
	return plane;
}

/*
   =============
   WindingArea
   =============
 */
float   WindingArea( const winding_t *w ){
	float total = 0;

	for ( int i = 2 ; i < w->numpoints ; i++ )
	{
		total += 0.5 * vector3_length( vector3_cross( w->p[i - 1] - w->p[0], w->p[i] - w->p[0] ) );
	}
	return total;
}

void WindingExtendBounds( const winding_t *w, MinMax& minmax ){
	for ( int i = 0 ; i < w->numpoints ; ++i )
	{
		minmax.extend( w->p[i] );
	}
}

/*
   =============
   WindingCenter
   =============
 */
Vector3 WindingCenter( const winding_t *w ){
	Vector3 center( 0 );

	for ( int i = 0 ; i < w->numpoints ; i++ )
		center += w->p[i];

	return center / w->numpoints;
}

/*
   =================
   BaseWindingForPlaneAccu
   =================
 */
winding_accu_t *BaseWindingForPlaneAccu( const Plane3f& inplane ){
	// The goal in this function is to replicate the behavior of the original BaseWindingForPlane()
	// function (see below) but at the same time increasing accuracy substantially.

	// The original code gave a preference for the vup vector to start out as (0, 0, 1), unless the
	// normal had a dominant Z value, in which case vup started out as (1, 0, 0).  After that, vup
	// was "bent" [along the plane defined by normal and vup] to become perpendicular to normal.
	// After that the vright vector was computed as the cross product of vup and normal.

	// I'm constructing the winding polygon points in a fashion similar to the method used in the
	// original function.  Orientation is the same.  The size of the winding polygon, however, is
	// variable in this function (depending on the angle of normal), and is larger (by about a factor
	// of 2) than the winding polygon in the original function.

	int x, i;
	float max, v;
	DoubleVector3 vright, vup, org;
	const Plane3 plane( inplane.normal(), inplane.dist() );
	winding_accu_t  *w;

	// One of the components of normal must have a magnitiude greater than this value,
	// otherwise normal is not a unit vector.  This is a little bit of inexpensive
	// partial error checking we can do.
	max = 0.56; // 1 / sqrt(1^2 + 1^2 + 1^2) = 0.577350269

	x = -1;
	for ( i = 0; i < 3; i++ ) {
		v = fabs( plane.normal()[i] );
		if ( v > max ) {
			x = i;
			max = v;
		}
	}
	if ( x == -1 ) {
		Error( "BaseWindingForPlaneAccu: no dominant axis found because normal is too short" );
	}

	switch ( x ) {
	case 0:     // Fall through to next case.
	case 1:
		vright[0] = -plane.normal()[1];
		vright[1] = plane.normal()[0];
		vright[2] = 0;
		break;
	case 2:
		vright[0] = 0;
		vright[1] = -plane.normal()[2];
		vright[2] = plane.normal()[1];
		break;
	}

	// vright and normal are now perpendicular; you can prove this by taking their
	// dot product and seeing that it's always exactly 0 (with no error).

	// NOTE: vright is NOT a unit vector at this point.  vright will have length
	// not exceeding 1.0.  The minimum length that vright can achieve happens when,
	// for example, the Z and X components of the normal input vector are equal,
	// and when normal's Y component is zero.  In that case Z and X of the normal
	// vector are both approximately 0.70711.  The resulting vright vector in this
	// case will have a length of 0.70711.

	// We're relying on the fact that MAX_WORLD_COORD is a power of 2 to keep
	// our calculation precise and relatively free of floating point error.
	// [However, the code will still work fine if that's not the case.]
	vright *= ( (double) MAX_WORLD_COORD ) * 4.0;

	// At time time of this writing, MAX_WORLD_COORD was 65536 (2^16).  Therefore
	// the length of vright at this point is at least 185364.  In comparison, a
	// corner of the world at location (65536, 65536, 65536) is distance 113512
	// away from the origin.

	vup = vector3_cross( plane.normal(), vright );

	// vup now has length equal to that of vright.

	org = plane.normal() * plane.dist();

	// org is now a point on the plane defined by normal and dist.  Furthermore,
	// org, vright, and vup are pairwise perpendicular.  Now, the 4 vectors
	// { (+-)vright + (+-)vup } have length that is at least sqrt(185364^2 + 185364^2),
	// which is about 262144.  That length lies outside the world, since the furthest
	// point in the world has distance 113512 from the origin as mentioned above.
	// Also, these 4 vectors are perpendicular to the org vector.  So adding them
	// to org will only increase their length.  Therefore the 4 points defined below
	// all lie outside of the world.  Furthermore, it can be easily seen that the
	// edges connecting these 4 points (in the winding_accu_t below) lie completely
	// outside the world.  sqrt(262144^2 + 262144^2)/2 = 185363, which is greater than
	// 113512.

	w = AllocWindingAccu( 4 );

	w->p[0] = org - vright + vup;
	w->p[1] = org + vright + vup;
	w->p[2] = org + vright - vup;
	w->p[3] = org - vright - vup;

	w->numpoints = 4;

	return w;
}

/*
   =================
   BaseWindingForPlane

   Original BaseWindingForPlane() function that has serious accuracy problems.  Here is why.
   The base winding is computed as a rectangle with very large coordinates.  These coordinates
   are in the range 2^17 or 2^18.  "Epsilon" (meaning the distance between two adjacent numbers)
   at these magnitudes in 32 bit floating point world is about 0.02.  So for example, if things
   go badly (by bad luck), then the whole plane could be shifted by 0.02 units (its distance could
   be off by that much).  Then if we were to compute the winding of this plane and another of
   the brush's planes met this winding at a very acute angle, that error could multiply to around
   0.1 or more when computing the final vertex coordinates of the winding.  0.1 is a very large
   error, and can lead to all sorts of disappearing triangle problems.
   =================
 */
winding_t *BaseWindingForPlane( const Plane3f& plane ){
	int i, x;
	float max, v;
	Vector3 org, vright, vup;
	winding_t   *w;

// find the major axis

	max = -BOGUS_RANGE;
	x = -1;
	for ( i = 0 ; i < 3; i++ )
	{
		v = fabs( plane.normal()[i] );
		if ( v > max ) {
			x = i;
			max = v;
		}
	}
	if ( x == -1 ) {
		Error( "BaseWindingForPlane: no axis found" );
	}

	vup.set( 0 );
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

	vup -= plane.normal() * vector3_dot( vup, plane.normal() );
	VectorNormalize( vup );

	org = plane.normal() * plane.dist();

	vright = vector3_cross( vup, plane.normal() );

	// LordHavoc: this has to use *2 because otherwise some created points may
	// be inside the world (think of a diagonal case), and any brush with such
	// points should be removed, failure to detect such cases is disasterous
	vup *= MAX_WORLD_COORD * 2;
	vright *= MAX_WORLD_COORD * 2;

	// project a really big	axis aligned box onto the plane
	w = AllocWinding( 4 );

	w->p[0] = org - vright + vup;
	w->p[1] = org + vright + vup;
	w->p[2] = org + vright - vup;
	w->p[3] = org - vright - vup;

	w->numpoints = 4;

	return w;
}

/*
   ==================
   CopyWinding
   ==================
 */
winding_t   *CopyWinding( const winding_t *w ){
	if ( !w ) {
		Error( "CopyWinding: winding is NULL" );
	}
	return void_ptr( memcpy( AllocWinding( w->numpoints ), w, offsetof( winding_t, p[w->numpoints] ) ) );
}

/*
   ==================
   CopyWindingAccuIncreaseSizeAndFreeOld
   ==================
 */
winding_accu_t *CopyWindingAccuIncreaseSizeAndFreeOld( winding_accu_t *w ){
	if ( !w ) {
		Error( "CopyWindingAccuIncreaseSizeAndFreeOld: winding is NULL" );
	}

	winding_accu_t *c = void_ptr( memcpy( AllocWindingAccu( w->numpoints + 1 ), w, offsetof( winding_accu_t, p[w->numpoints] ) ) );
	FreeWindingAccu( w );
	return c;
}

/*
   ==================
   CopyWindingAccuToRegular
   ==================
 */
winding_t   *CopyWindingAccuToRegular( const winding_accu_t *w ){
	int i;
	winding_t   *c;

	if ( !w ) {
		Error( "CopyWindingAccuToRegular: winding is NULL" );
	}

	c = AllocWinding( w->numpoints );
	c->numpoints = w->numpoints;
	for ( i = 0; i < c->numpoints; i++ )
	{
		c->p[i] = w->p[i];
	}
	return c;
}

/*
   ==================
   ReverseWinding
   ==================
 */
winding_t   *ReverseWinding( const winding_t *w ){
	int i;
	winding_t   *c;

	c = AllocWinding( w->numpoints );
	for ( i = 0 ; i < w->numpoints ; i++ )
	{
		c->p[i] = w->p[w->numpoints - 1 - i];
	}
	c->numpoints = w->numpoints;
	return c;
}


/*
   =============
   ClipWindingEpsilon
   =============
 */
void    ClipWindingEpsilonStrict( winding_t *in, const Plane3f& plane,
                                  float epsilon, winding_t **front, winding_t **back ){
	float dists[MAX_POINTS_ON_WINDING + 4];
	EPlaneSide sides[MAX_POINTS_ON_WINDING + 4];
	int counts[3];
	int i, j;
	winding_t   *f, *b;
	int maxpts;

	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for ( i = 0 ; i < in->numpoints ; i++ )
	{

		dists[i] = plane3_distance_to_point( plane, in->p[i] );
		if ( dists[i] > epsilon ) {
			sides[i] = eSideFront;
		}
		else if ( dists[i] < -epsilon ) {
			sides[i] = eSideBack;
		}
		else
		{
			sides[i] = eSideOn;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if ( !counts[0] && !counts[1] ) {
		return;
	}
	if ( !counts[0] ) {
		*back = CopyWinding( in );
		return;
	}
	if ( !counts[1] ) {
		*front = CopyWinding( in );
		return;
	}

	maxpts = in->numpoints + 4;   // cant use counts[0]+2 because
	                              // of fp grouping errors

	*front = f = AllocWinding( maxpts );
	*back = b = AllocWinding( maxpts );

	for ( i = 0 ; i < in->numpoints ; i++ )
	{
		const Vector3& p1 = in->p[i];

		if ( sides[i] == eSideOn ) {
			f->p[f->numpoints] = p1;
			f->numpoints++;
			b->p[b->numpoints] = p1;
			b->numpoints++;
			continue;
		}

		if ( sides[i] == eSideFront ) {
			f->p[f->numpoints] = p1;
			f->numpoints++;
		}
		if ( sides[i] == eSideBack ) {
			b->p[b->numpoints] = p1;
			b->numpoints++;
		}

		if ( sides[i + 1] == eSideOn || sides[i + 1] == sides[i] ) {
			continue;
		}

		// generate a split point
		const Vector3& p2 = in->p[( i + 1 ) % in->numpoints];
		const double dot = dists[i] / ( dists[i] - dists[i + 1] );
		Vector3 mid;
		for ( j = 0 ; j < 3 ; j++ )
		{	// avoid round off error when possible
			if ( plane.normal()[j] == 1 ) {
				mid[j] = plane.dist();
			}
			else if ( plane.normal()[j] == -1 ) {
				mid[j] = -plane.dist();
			}
			else{
				mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
			}
		}

		f->p[f->numpoints] = mid;
		f->numpoints++;
		b->p[b->numpoints] = mid;
		b->numpoints++;
	}

	if ( f->numpoints > maxpts || b->numpoints > maxpts ) {
		Error( "ClipWinding: points exceeded estimate" );
	}
	if ( f->numpoints > MAX_POINTS_ON_WINDING || b->numpoints > MAX_POINTS_ON_WINDING ) {
		Error( "ClipWinding: MAX_POINTS_ON_WINDING" );
	}
}

void    ClipWindingEpsilon( winding_t *in, const Plane3f& plane,
                            float epsilon, winding_t **front, winding_t **back ){
	ClipWindingEpsilonStrict( in, plane, epsilon, front, back );
	/* apparently most code expects that in the winding-on-plane case, the back winding is the original winding */
	if ( !*front && !*back ) {
		*back = CopyWinding( in );
	}
}


// Smallest positive value for vec_t such that 1.0 + VEC_SMALLEST_EPSILON_AROUND_ONE != 1.0.
// In the case of 32 bit floats (which is almost certainly the case), it's 0.00000011921.
// Don't forget that your epsilons should depend on the possible range of values,
// because for example adding VEC_SMALLEST_EPSILON_AROUND_ONE to 1024.0 will have no effect.
#define VEC_SMALLEST_EPSILON_AROUND_ONE FLT_EPSILON

/*
   =============
   ChopWindingInPlaceAccu
   =============
 */
void ChopWindingInPlaceAccu( winding_accu_t **inout, const Plane3f& plane, float crudeEpsilon ){
	winding_accu_t  *in;
	int counts[3];
	int i, j;
	double dists[MAX_POINTS_ON_WINDING + 1];
	EPlaneSide sides[MAX_POINTS_ON_WINDING + 1];
	int maxpts;
	winding_accu_t  *f;
	double w;

	// We require at least a very small epsilon.  It's a good idea for several reasons.
	// First, we will be dividing by a potentially very small distance below.  We don't
	// want that distance to be too small; otherwise, things "blow up" with little accuracy
	// due to the division.  (After a second look, the value w below is in range (0,1), but
	// graininess problem remains.)  Second, Having minimum epsilon also prevents the following
	// situation.  Say for example we have a perfect octagon defined by the input winding.
	// Say our chopping plane (defined by normal and dist) is essentially the same plane
	// that the octagon is sitting on.  Well, due to rounding errors, it may be that point
	// 1 of the octagon might be in front, point 2 might be in back, point 3 might be in
	// front, point 4 might be in back, and so on.  So we could end up with a very ugly-
	// looking chopped winding, and this might be undesirable, and would at least lead to
	// a possible exhaustion of MAX_POINTS_ON_WINDING.  It's better to assume that points
	// very very close to the plane are on the plane, using an infinitesimal epsilon amount.

	// Now, the original ChopWindingInPlace() function used a vec_t-based winding_t.
	// So this minimum epsilon is quite similar to casting the higher resolution numbers to
	// the lower resolution and comparing them in the lower resolution mode.  We explicitly
	// choose the minimum epsilon as something around the vec_t epsilon of one because we
	// want the resolution of double to have a large resolution around the epsilon.
	// Some of that leftover resolution even goes away after we scale to points far away.

	// Here is a further discussion regarding the choice of smallestEpsilonAllowed.
	// In the 32 float world (we can assume vec_t is that), the "epsilon around 1.0" is
	// 0.00000011921.  In the 64 bit float world (we can assume double is that), the
	// "epsilon around 1.0" is 0.00000000000000022204.  (By the way these two epsilons
	// are defined as VEC_SMALLEST_EPSILON_AROUND_ONE VEC_ACCU_SMALLEST_EPSILON_AROUND_ONE
	// respectively.)  If you divide the first by the second, you get approximately
	// 536,885,246.  Dividing that number by 200,000 (a typical base winding coordinate)
	// gives 2684.  So in other words, if our smallestEpsilonAllowed was chosen as exactly
	// VEC_SMALLEST_EPSILON_AROUND_ONE, you would be guaranteed at least 2000 "ticks" in
	// 64-bit land inside of the epsilon for all numbers we're dealing with.

	static const double smallestEpsilonAllowed = ( (double) VEC_SMALLEST_EPSILON_AROUND_ONE ) * 0.5;
	const double fineEpsilon = std::max( smallestEpsilonAllowed, (double) crudeEpsilon );

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	for ( i = 0; i < in->numpoints; i++ )
	{
		dists[i] = plane3_distance_to_point( plane, in->p[i] );
		if ( dists[i] > fineEpsilon ) {
			sides[i] = eSideFront;
		}
		else if ( dists[i] < -fineEpsilon ) {
			sides[i] = eSideBack;
		}
		else{
			sides[i] = eSideOn;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	// I'm wondering if whatever code that handles duplicate planes is robust enough
	// that we never get a case where two nearly equal planes result in 2 NULL windings
	// due to the 'if' statement below.  TODO: Investigate this.
	if ( !counts[eSideFront] ) {
		FreeWindingAccu( in );
		*inout = NULL;
		return;
	}
	if ( !counts[eSideBack] ) {
		return; // Winding is unmodified.
	}

	// NOTE: The least number of points that a winding can have at this point is 2.
	// In that case, one point is SIDE_FRONT and the other is SIDE_BACK.

	maxpts = counts[eSideFront] + 2; // We dynamically expand if this is too small.
	f = AllocWindingAccu( maxpts );

	for ( i = 0; i < in->numpoints; i++ )
	{
		const DoubleVector3& p1 = in->p[i];

		if ( sides[i] == eSideOn || sides[i] == eSideFront ) {
			if ( f->numpoints >= MAX_POINTS_ON_WINDING ) {
				Error( "ChopWindingInPlaceAccu: MAX_POINTS_ON_WINDING" );
			}
			if ( f->numpoints >= maxpts ) { // This will probably never happen.
				Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: estimate on chopped winding size incorrect (no problem)\n" );
				f = CopyWindingAccuIncreaseSizeAndFreeOld( f );
				maxpts++;
			}
			f->p[f->numpoints] = p1;
			f->numpoints++;
			if ( sides[i] == eSideOn ) {
				continue;
			}
		}
		if ( sides[i + 1] == eSideOn || sides[i + 1] == sides[i] ) {
			continue;
		}

		// Generate a split point.
		const DoubleVector3& p2 = in->p[( ( i + 1 ) == in->numpoints ) ? 0 : ( i + 1 )];

		// The divisor's absolute value is greater than the dividend's absolute value.
		// w is in the range (0,1).
		w = dists[i] / ( dists[i] - dists[i + 1] );
		DoubleVector3 mid;
		for ( j = 0; j < 3; j++ )
		{
			// Avoid round-off error when possible.  Check axis-aligned normal.
			if ( plane.normal()[j] == 1 ) {
				mid[j] = plane.dist();
			}
			else if ( plane.normal()[j] == -1 ) {
				mid[j] = -plane.dist();
			}
			else{
				mid[j] = p1[j] + ( w * ( p2[j] - p1[j] ) );
			}
		}
		if ( f->numpoints >= MAX_POINTS_ON_WINDING ) {
			Error( "ChopWindingInPlaceAccu: MAX_POINTS_ON_WINDING" );
		}
		if ( f->numpoints >= maxpts ) { // This will probably never happen.
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: estimate on chopped winding size incorrect (no problem)\n" );
			f = CopyWindingAccuIncreaseSizeAndFreeOld( f );
			maxpts++;
		}
		f->p[f->numpoints] = mid;
		f->numpoints++;
	}

	FreeWindingAccu( in );
	*inout = f;
}

/*
   =============
   ChopWindingInPlace
   =============
 */
void ChopWindingInPlace( winding_t **inout, const Plane3f& plane, float epsilon ){
	winding_t   *in;
	float dists[MAX_POINTS_ON_WINDING + 4];
	EPlaneSide sides[MAX_POINTS_ON_WINDING + 4];
	int counts[3];
	int i, j;
	winding_t   *f;
	int maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for ( i = 0 ; i < in->numpoints ; i++ )
	{
		dists[i] = plane3_distance_to_point( plane, in->p[i] );
		if ( dists[i] > epsilon ) {
			sides[i] = eSideFront;
		}
		else if ( dists[i] < -epsilon ) {
			sides[i] = eSideBack;
		}
		else
		{
			sides[i] = eSideOn;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if ( !counts[0] ) {
		FreeWinding( in );
		*inout = NULL;
		return;
	}
	if ( !counts[1] ) {
		return;     // inout stays the same

	}
	maxpts = in->numpoints + 4;   // cant use counts[0]+2 because
	                              // of fp grouping errors

	f = AllocWinding( maxpts );

	for ( i = 0 ; i < in->numpoints ; i++ )
	{
		const Vector3& p1 = in->p[i];

		if ( sides[i] == eSideOn ) {
			f->p[f->numpoints] = p1;
			f->numpoints++;
			continue;
		}

		if ( sides[i] == eSideFront ) {
			f->p[f->numpoints] = p1;
			f->numpoints++;
		}

		if ( sides[i + 1] == eSideOn || sides[i + 1] == sides[i] ) {
			continue;
		}

		// generate a split point
		const Vector3& p2 = in->p[( i + 1 ) % in->numpoints];

		const double dot = dists[i] / ( dists[i] - dists[i + 1] );
		Vector3 mid;
		for ( j = 0 ; j < 3 ; j++ )
		{	// avoid round off error when possible
			if ( plane.normal()[j] == 1 ) {
				mid[j] = plane.dist();
			}
			else if ( plane.normal()[j] == -1 ) {
				mid[j] = -plane.dist();
			}
			else{
				mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
			}
		}

		f->p[f->numpoints] = mid;
		f->numpoints++;
	}

	if ( f->numpoints > maxpts ) {
		Error( "ClipWinding: points exceeded estimate" );
	}
	if ( f->numpoints > MAX_POINTS_ON_WINDING ) {
		Error( "ClipWinding: MAX_POINTS_ON_WINDING" );
	}

	FreeWinding( in );
	*inout = f;
}


/*
   =================
   ChopWinding

   Returns the fragment of in that is on the front side
   of the cliping plane.  The original is freed.
   =================
 */
winding_t   *ChopWinding( winding_t *in, const Plane3f& plane ){
	winding_t   *f, *b;

	ClipWindingEpsilon( in, plane, ON_EPSILON, &f, &b );
	FreeWinding( in );
	if ( b ) {
		FreeWinding( b );
	}
	return f;
}


inline const MinMax c_worldMinmax( Vector3( MIN_WORLD_COORD ), Vector3( MAX_WORLD_COORD ) );
/*
   =================
   CheckWinding

   =================
 */
void CheckWinding( winding_t *w ){
	int i, j;
	float edgedist;
	Vector3 dir, edgenormal;
	float area;

	if ( w->numpoints < 3 ) {
		Error( "CheckWinding: %i points",w->numpoints );
	}

	area = WindingArea( w );
	if ( area < 1 ) {
		Error( "CheckWinding: %f area", area );
	}

	const Plane3f faceplane = WindingPlane( w );

	for ( i = 0 ; i < w->numpoints ; i++ )
	{
		const Vector3& p1 = w->p[i];

		if ( !c_worldMinmax.test( p1 ) ) {
			Error( "CheckFace: MAX_WORLD_COORD exceeded: ( %f %f %f )", p1[0], p1[1], p1[2] );
		}

		j = ( i + 1 == w->numpoints )? 0 : i + 1;

		// check the point is on the face plane
		if ( fabs( plane3_distance_to_point( faceplane, p1 ) ) > ON_EPSILON ) {
			Error( "CheckWinding: point off plane" );
		}

		// check the edge isnt degenerate
		const Vector3& p2 = w->p[j];
		dir = p2 - p1;

		if ( vector3_length( dir ) < ON_EPSILON ) {
			Error( "CheckWinding: degenerate edge" );
		}

		edgenormal = VectorNormalized( vector3_cross( faceplane.normal(), dir ) );
		edgedist = vector3_dot( p1, edgenormal ) + ON_EPSILON;

		// all other points must be on front side
		for ( j = 0 ; j < w->numpoints ; j++ )
		{
			if ( j == i ) {
				continue;
			}
			if ( vector3_dot( w->p[j], edgenormal ) > edgedist ) {
				Error( "CheckWinding: non-convex" );
			}
		}
	}
}


/*
   ============
   WindingOnPlaneSide
   ============
 */
EPlaneSide     WindingOnPlaneSide( const winding_t *w, const Plane3f& plane ){
	bool front = false;
	bool back = false;
	for ( int i = 0 ; i < w->numpoints ; i++ )
	{
		const double d = plane3_distance_to_point( plane, w->p[i] );
		if ( d < -ON_EPSILON ) {
			if ( front ) {
				return eSideCross;
			}
			back = true;
			continue;
		}
		if ( d > ON_EPSILON ) {
			if ( back ) {
				return eSideCross;
			}
			front = true;
			continue;
		}
	}

	if ( back ) {
		return eSideBack;
	}
	if ( front ) {
		return eSideFront;
	}
	return eSideOn;
}


/*
   =================
   AddWindingToConvexHull

   Both w and *hull are on the same plane
   =================
 */
#define MAX_HULL_POINTS     128
void    AddWindingToConvexHull( winding_t *w, winding_t **hull, const Vector3& normal ) {
	int i, j, k;
	int numHullPoints, numNew;
	Vector3 hullPoints[MAX_HULL_POINTS];
	Vector3 newHullPoints[MAX_HULL_POINTS];
	Vector3 hullDirs[MAX_HULL_POINTS];
	bool hullSide[MAX_HULL_POINTS];
	bool outside;

	if ( *hull == nullptr ) {
		*hull = CopyWinding( w );
		return;
	}

	numHullPoints = ( *hull )->numpoints;
	memcpy( hullPoints, ( *hull )->p, numHullPoints * sizeof( Vector3 ) );

	for ( i = 0 ; i < w->numpoints ; i++ ) {
		const Vector3 &p = w->p[i];

		// calculate hull side vectors
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			k = ( j + 1 ) % numHullPoints;

			hullDirs[j] = vector3_cross( normal, VectorNormalized( hullPoints[k] - hullPoints[j] ) );
		}

		outside = false;
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			const double d = vector3_dot( p - hullPoints[j], hullDirs[j] );
			if ( d >= ON_EPSILON ) {
				outside = true;
			}
			hullSide[j] = ( d >= -ON_EPSILON );
		}

		// if the point is effectively inside, do nothing
		if ( !outside ) {
			continue;
		}

		// find the back side to front side transition
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			if ( !hullSide[ j % numHullPoints ] && hullSide[ ( j + 1 ) % numHullPoints ] ) {
				break;
			}
		}
		if ( j == numHullPoints ) {
			continue;
		}

		// insert the point here
		newHullPoints[0] = p;
		numNew = 1;

		// copy over all points that aren't double fronts
		j = ( j + 1 ) % numHullPoints;
		for ( k = 0 ; k < numHullPoints ; k++ ) {
			if ( hullSide[ ( j + k ) % numHullPoints ] && hullSide[ ( j + k + 1 ) % numHullPoints ] ) {
				continue;
			}
			newHullPoints[numNew] = hullPoints[ ( j + k + 1 ) % numHullPoints ];
			numNew++;
		}

		numHullPoints = numNew;
		memcpy( hullPoints, newHullPoints, numHullPoints * sizeof( Vector3 ) );
	}

	FreeWinding( *hull );
	w = AllocWinding( numHullPoints );
	w->numpoints = numHullPoints;
	*hull = w;
	memcpy( w->p, hullPoints, numHullPoints * sizeof( Vector3 ) );
}

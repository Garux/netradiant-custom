/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"



/* FIXME: remove these vars */

/* undefine to make plane finding use linear sort (note: really slow) */
#define USE_HASHING
#define PLANE_HASHES    8192

namespace{
int planehash[ PLANE_HASHES ];

int c_boxbevels;
int c_edgebevels;
int c_areaportals;
int c_detail;
int c_structural;
int c_patches;
}



/*
   PlaneEqual()
   ydnar: replaced with variable epsilon for djbob
 */

bool PlaneEqual( const plane_t& p, const Plane3f& plane ){
	/* get local copies */
	const float ne = normalEpsilon;
	const float de = distanceEpsilon;

	/* compare */
	// We check equality of each component since we're using '<', not '<='
	// (the epsilons may be zero).  We want to use '<' instead of '<=' to be
	// consistent with the true meaning of "epsilon", and also because other
	// parts of the code uses this inequality.
	if ( ( p.dist() == plane.dist() || fabs( p.dist() - plane.dist() ) < de ) &&
	     ( p.normal()[0] == plane.normal()[0] || fabs( p.normal()[0] - plane.normal()[0] ) < ne ) &&
	     ( p.normal()[1] == plane.normal()[1] || fabs( p.normal()[1] - plane.normal()[1] ) < ne ) &&
	     ( p.normal()[2] == plane.normal()[2] || fabs( p.normal()[2] - plane.normal()[2] ) < ne ) ) {
		return true;
	}

	/* different */
	return false;
}



/*
   AddPlaneToHash()
 */

inline void AddPlaneToHash( plane_t& p ){
	const int hash = ( PLANE_HASHES - 1 ) & (int) fabs( p.dist() );

	p.hash_chain = planehash[hash];
	planehash[hash] = &p - mapplanes.data() + 1;
}

/*
   ================
   CreateNewFloatPlane
   ================
 */
static int CreateNewFloatPlane( const Plane3f& plane ){

	if ( vector3_length( plane.normal() ) < 0.5 ) {
		Sys_Printf( "FloatPlane: bad normal\n" );
		return -1;
	}

	// create a new plane
	mapplanes.resize( mapplanes.size() + 2 );
	plane_t& p = *( mapplanes.end() - 2 );
	plane_t& p2 = *( mapplanes.end() - 1 );
	p.plane = plane;
	p2.plane = plane3_flipped( plane );
	p.type = p2.type = PlaneTypeForNormal( p.normal() );

	// always put axial planes facing positive first
	if ( p.type < ePlaneNonAxial ) {
		if ( p.normal()[0] < 0 || p.normal()[1] < 0 || p.normal()[2] < 0 ) {
			// flip order
			std::swap( p, p2 );

			AddPlaneToHash( p );
			AddPlaneToHash( p2 );
			return mapplanes.size() - 1;
		}
	}

	AddPlaneToHash( p );
	AddPlaneToHash( p2 );
	return mapplanes.size() - 2;
}



/*
   SnapNormal()
   Snaps a near-axial normal vector.
   Returns true if and only if the normal was adjusted.
 */

static bool SnapNormal( Vector3& normal ){
#if Q3MAP2_EXPERIMENTAL_SNAP_NORMAL_FIX
	int i;
	bool adjusted = false;

	// A change from the original SnapNormal() is that we snap each
	// component that's close to 0.  So for example if a normal is
	// (0.707, 0.707, 0.0000001), it will get snapped to lie perfectly in the
	// XY plane (its Z component will be set to 0 and its length will be
	// normalized).  The original SnapNormal() didn't snap such vectors - it
	// only snapped vectors that were near a perfect axis.

	//adjusting vectors, that are near perfect axis, with bigger epsilon
	//they cause precision errors


	if ( ( normal[0] != 0.0 || normal[1] != 0.0 ) && fabs( normal[0] ) < 0.00025 && fabs( normal[1] ) < 0.00025){
		normal[0] = normal[1] = 0.0;
		adjusted = true;
	}
	else if ( ( normal[0] != 0.0 || normal[2] != 0.0 ) && fabs( normal[0] ) < 0.00025 && fabs( normal[2] ) < 0.00025){
		normal[0] = normal[2] = 0.0;
		adjusted = true;
	}
	else if ( ( normal[2] != 0.0 || normal[1] != 0.0 ) && fabs( normal[2] ) < 0.00025 && fabs( normal[1] ) < 0.00025){
		normal[2] = normal[1] = 0.0;
		adjusted = true;
	}


	/*
	for ( i = 0; i < 30; i++ )
	{
		double x, y, z, length;
		x = (double) 1.0;
		y = (double) ( 0.00001 * i );
		z = (double) 0.0;

		Sys_Printf( "(%6.18f %6.18f %6.18f)inNormal\n", x, y, z );

		length = sqrt( ( x * x ) + ( y * y ) + ( z * z ) );
		Sys_Printf( "(%6.18f)length\n", length );
		x = (vec_t) ( x / length );
		y = (vec_t) ( y / length );
		z = (vec_t) ( z / length );
		Sys_Printf( "(%6.18f %6.18f %6.18f)outNormal\n\n", x, y, z );
	}
	Error( "vectorNormalize test completed" );
	*/

	for ( i = 0; i < 3; i++ )
	{
		if ( normal[i] != 0.0 && -normalEpsilon < normal[i] && normal[i] < normalEpsilon ) {
			normal[i] = 0.0;
			adjusted = true;
		}
	}

	if ( adjusted ) {
		VectorNormalize( normal );
		return true;
	}
	return false;
#else
	int i;

	// I would suggest that you uncomment the following code and look at the
	// results:

	/*
	   Sys_Printf( "normalEpsilon is %f\n", normalEpsilon );
	   for ( i = 0;; i++ )
	   {
	    normal[0] = 1.0;
	    normal[1] = 0.0;
	    normal[2] = i * 0.000001;
	    VectorNormalize( normal, normal );
	    if ( 1.0 - normal[0] >= normalEpsilon ) {
	        Sys_Printf( "(%f %f %f)\n", normal[0], normal[1], normal[2] );
	        Error( "SnapNormal: test completed" );
	    }
	   }
	 */

	// When the normalEpsilon is 0.00001, the loop will break out when normal is
	// (0.999990 0.000000 0.004469).  In other words, this is the vector closest
	// to axial that will NOT be snapped.  Anything closer will be snaped.  Now,
	// 0.004469 is close to 1/225.  The length of a circular quarter-arc of radius
	// 1 is PI/2, or about 1.57.  And 0.004469/1.57 is about 0.0028, or about
	// 1/350.  Expressed a different way, 1/350 is also about 0.26/90.
	// This means is that a normal with an angle that is within 1/4 of a degree
	// from axial will be "snapped".  My belief is that the person who wrote the
	// code below did not intend it this way.  I think the person intended that
	// the epsilon be measured against the vector components close to 0, not 1.0.
	// I think the logic should be: if 2 of the normal components are within
	// epsilon of 0, then the vector can be snapped to be perfectly axial.
	// We may consider adjusting the epsilon to a larger value when we make this
	// code fix.

	for ( i = 0; i < 3; i++ )
	{
		if ( fabs( normal[ i ] - 1 ) < normalEpsilon ) {
			normal.set( 0 );
			normal[ i ] = 1;
			return true;
		}
		if ( fabs( normal[ i ] - -1 ) < normalEpsilon ) {
			normal.set( 0 );
			normal[ i ] = -1;
			return true;
		}
	}
	return false;
#endif
}



/*
   SnapPlane()
   snaps a plane to normal/distance epsilons
 */

static void SnapPlane( Plane3f& plane ){
// SnapPlane disabled by LordHavoc because it often messes up collision
// brushes made from triangles of embedded models, and it has little effect
// on anything else (axial planes are usually derived from snapped points)
/*
   SnapPlane reenabled by namespace because of multiple reports of
   q3map2-crashes which were triggered by this patch.
 */
	SnapNormal( plane.normal() );

	// TODO: Rambetter has some serious comments here as well.  First off,
	// in the case where a normal is non-axial, there is nothing special
	// about integer distances.  I would think that snapping a distance might
	// make sense for axial normals, but I'm not so sure about snapping
	// non-axial normals.  A shift by 0.01 in a plane, multiplied by a clipping
	// against another plane that is 5 degrees off, and we introduce 0.1 error
	// easily.  A 0.1 error in a vertex is where problems start to happen, such
	// as disappearing triangles.

	// Second, assuming we have snapped the normal above, let's say that the
	// plane we just snapped was defined for some points that are actually
	// quite far away from normal * dist.  Well, snapping the normal in this
	// case means that we've just moved those points by potentially many units!
	// Therefore, if we are going to snap the normal, we need to know the
	// points we're snapping for so that the plane snaps with those points in
	// mind (points remain close to the plane).

	// I would like to know exactly which problems SnapPlane() is trying to
	// solve so that we can better engineer it (I'm not saying that SnapPlane()
	// should be removed altogether).  Fix all this snapping code at some point!

	if ( fabs( plane.dist() - std::rint( plane.dist() ) ) < distanceEpsilon ) {
		plane.dist() = std::rint( plane.dist() );
	}
}

/*
   SnapPlaneImproved()
   snaps a plane to normal/distance epsilons, improved code
 */
static void SnapPlaneImproved( Plane3f& plane, int numPoints, const Vector3 *points ){
	if ( SnapNormal( plane.normal() ) ) {
		if ( numPoints > 0 ) {
			// Adjust the dist so that the provided points don't drift away.
			DoubleVector3 center( 0 );
			for ( const Vector3& point : Span( points, numPoints ) )
			{
				center += point;
			}
			center /= numPoints;
			plane.dist() = vector3_dot( plane.normal(), center );
		}
	}

	if ( VectorIsOnAxis( plane.normal() ) ) {
		// Only snap distance if the normal is an axis.  Otherwise there
		// is nothing "natural" about snapping the distance to an integer.
		const float distNearestInt = std::rint( plane.dist() );
		if ( -distanceEpsilon < plane.dist() - distNearestInt && plane.dist() - distNearestInt < distanceEpsilon ) {
			plane.dist() = distNearestInt;
		}
	}
}



/*
   FindFloatPlane()
   ydnar: changed to allow a number of test points to be supplied that
   must be within an epsilon distance of the plane
 */

int FindFloatPlane( const Plane3f& inplane, int numPoints, const Vector3 *points ) // NOTE: this has a side effect on the normal. Good or bad?

#ifdef USE_HASHING

{
	Plane3f plane( inplane );
#if Q3MAP2_EXPERIMENTAL_SNAP_PLANE_FIX
	SnapPlaneImproved( plane, numPoints, points );
#else
	SnapPlane( plane );
#endif
	/* hash the plane */
	const int hash = ( PLANE_HASHES - 1 ) & (int) fabs( plane.dist() );

	/* search the border bins as well */
	for ( int i = -1; i <= 1; i++ )
	{
		const int h = ( hash + i ) & ( PLANE_HASHES - 1 );
		for ( int pidx = planehash[ h ] - 1; pidx != -1; pidx = mapplanes[pidx].hash_chain - 1 )
		{
			const plane_t& p = mapplanes[pidx];

			/* do standard plane compare */
			if ( !PlaneEqual( p, plane ) ) {
				continue;
			}

			/* ydnar: uncomment the following line for old-style plane finding */
			//%	return p - mapplanes;

			/* ydnar: test supplied points against this plane */
			int j;
			for ( j = 0; j < numPoints; j++ )
			{
				// NOTE: When dist approaches 2^16, the resolution of 32 bit floating
				// point number is greatly decreased.  The distanceEpsilon cannot be
				// very small when world coordinates extend to 2^16.  Making the
				// dot product here in 64 bit land will not really help the situation
				// because the error will already be carried in dist.
				const double d = fabs( plane3_distance_to_point( p.plane, points[ j ] ) );
				if ( d != 0.0 && d >= distanceEpsilon ) {
					break; // Point is too far from plane.
				}
			}

			/* found a matching plane */
			if ( j >= numPoints ) {
				return pidx;
			}
		}
	}

	/* none found, so create a new one */
	return CreateNewFloatPlane( plane );
}

#else

{
	int i, j;
	plane_t *p;
	Plane3f plane( innormal, dist );

#if Q3MAP2_EXPERIMENTAL_SNAP_PLANE_FIX
	SnapPlaneImproved( plane, numPoints, points );
#else
	SnapPlane( plane );
#endif
	for ( i = 0, p = mapplanes; i < nummapplanes; i++, p++ )
	{
		if ( !PlaneEqual( *p, plane ) ) {
			continue;
		}

		/* ydnar: uncomment the following line for old-style plane finding */
		//%	return i;

		/* ydnar: test supplied points against this plane */
		for ( j = 0; j < numPoints; j++ )
		{
			if ( fabs( plane3_distance_to_point( p->plane, points[ j ] ) ) > distanceEpsilon ) {
				break;
			}
		}

		/* found a matching plane */
		if ( j >= numPoints ) {
			return i;
		}
		// TODO: Note that the non-USE_HASHING code does not compute epsilons
		// for the provided points.  It should do that.  I think this code
		// is unmaintained because nobody sets USE_HASHING to off.
	}

	return CreateNewFloatPlane( plane );
}

#endif



/*
   MapPlaneFromPoints()
   takes 3 points and finds the plane they lie in
 */

inline int MapPlaneFromPoints( DoubleVector3 p[3] ){
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
	Plane3 plane;
	PlaneFromPoints( plane, p );
	// TODO: A 32 bit float for the plane distance isn't enough resolution
	// if the plane is 2^16 units away from the origin (the "epsilon" approaches
	// 0.01 in that case).
	const Vector3 points[3] = { p[0], p[1], p[2] };
	return FindFloatPlane( Plane3f( plane ), 3, points );
#else
	Plane3f plane;
	PlaneFromPoints( plane, p );
	return FindFloatPlane( plane, 3, p );
#endif
}



/*
   SetBrushContents()
   the content flags and compile flags on all sides of a brush should be the same
 */

static void SetBrushContents( brush_t& b ){
	if( b.sides.empty() )
		return;

	//%	bool	mixed = false;
	/* get initial compile flags from first side */
	auto s = b.sides.cbegin();
	int contentFlags = s->contentFlags;
	int compileFlags = s->compileFlags;
	b.contentShader = s->shaderInfo;

	/* get the content/compile flags for every side in the brush */
	for ( ++s; s != b.sides.cend(); ++s )
	{
		if ( s->shaderInfo == NULL ) {
			continue;
		}
		//%	if( s->contentFlags != contentFlags || s->compileFlags != compileFlags )
		//%		mixed = true;

		contentFlags |= s->contentFlags;
		compileFlags |= s->compileFlags;

		/* resolve inconsistency, when brush content was determined by 1st face */
		if ( b.contentShader->compileFlags & C_LIQUID ){
			continue;
		}
		else if ( s->compileFlags & C_LIQUID ){
			b.contentShader = s->shaderInfo;
		}
		else if ( b.contentShader->compileFlags & C_FOG ){
			continue;
		}
		else if ( s->compileFlags & C_FOG ){
			b.contentShader = s->shaderInfo;
		}
		else if ( b.contentShader->contentFlags & GetRequiredSurfaceParm<"playerclip">().contentFlags ){
			continue;
		}
		else if ( s->contentFlags & GetRequiredSurfaceParm<"playerclip">().contentFlags ){
			b.contentShader = s->shaderInfo;
		}
		else if ( !( b.contentShader->compileFlags & C_SOLID ) ){
			continue;
		}
		else if ( !( s->compileFlags & C_SOLID ) ){
			b.contentShader = s->shaderInfo;
		}
	}

	/* ydnar: getting rid of this stupid warning */
	//%	if( mixed )
	//%		Sys_FPrintf( SYS_WRN | SYS_VRBflag, "Entity %i, Brush %i: mixed face contentFlags\n", b.entitynum, b.brushnum );

	/* check for detail & structural */
	if ( ( compileFlags & C_DETAIL ) && ( compileFlags & C_STRUCTURAL ) ) {
		xml_Select( "Mixed detail and structural (defaulting to structural)", b.entityNum, b.brushNum, false );
		compileFlags &= ~C_DETAIL;
	}

	/* the fulldetail flag will cause detail brushes to be treated like normal brushes */
	if ( fulldetail ) {
		compileFlags &= ~C_DETAIL;
	}

	/* all translucent brushes that aren't specifically made structural will be detail */
	if ( ( compileFlags & C_TRANSLUCENT ) && !( compileFlags & C_STRUCTURAL ) ) {
		compileFlags |= C_DETAIL;
	}

	/* detail? */
	if ( compileFlags & C_DETAIL ) {
		c_detail++;
		b.detail = true;
	}
	else
	{
		c_structural++;
		b.detail = false;
	}

	/* opaque? */
	if ( compileFlags & C_TRANSLUCENT ) {
		b.opaque = false;
	}
	else{
		b.opaque = true;
	}

	/* areaportal? */
	if ( compileFlags & C_AREAPORTAL ) {
		c_areaportals++;
	}

	/* set brush flags */
	b.contentFlags = contentFlags;
	b.compileFlags = compileFlags;
}



/*
   AddBrushBevels()
   adds any additional planes necessary to allow the brush being
   built to be expanded against axial bounding boxes
   ydnar 2003-01-20: added mrelusive fixes
 */

void AddBrushBevels(){
	const int surfaceFlagsMask = g_game->brushBevelsSurfaceFlagsMask;
	auto& sides = buildBrush.sides;

	//
	// add the axial planes
	//
	size_t order = 0;
	for ( size_t axis = 0; axis < 3; axis++ ) {
		for ( int dir = -1; dir <= 1; dir += 2, order++ ) {
			// see if the plane is already present
			size_t i = 0;
			for ( ; i < sides.size(); ++i )
			{
				/* ydnar: testing disabling of mre code */
				#if 0
				if ( dir > 0 ) {
					if ( mapplanes[sides[i].planenum].normal()[axis] >= 0.9999f ) {
						break;
					}
				}
				else {
					if ( mapplanes[sides[i].planenum].normal()[axis] <= -0.9999f ) {
						break;
					}
				}
				#else
				if ( ( dir > 0 && mapplanes[ sides[i].planenum ].normal()[ axis ] == 1.0f ) ||
				     ( dir < 0 && mapplanes[ sides[i].planenum ].normal()[ axis ] == -1.0f ) ) {
					break;
				}
				#endif
			}

			if ( i == sides.size() ) {
				// add a new side
				if ( sides.size() == MAX_BUILD_SIDES ) {
					xml_Select( "MAX_BUILD_SIDES", buildBrush.entityNum, buildBrush.brushNum, true );
				}
				side_t& s = sides.emplace_back();
				Plane3f plane;
				plane.normal().set( 0 );
				plane.normal()[axis] = dir;

				if ( dir == 1 ) {
					/* ydnar: adding bevel plane snapping for fewer bsp planes */
					if ( bevelSnap > 0 ) {
						plane.dist() = floor( buildBrush.minmax.maxs[ axis ] / bevelSnap ) * bevelSnap;
					}
					else{
						plane.dist() = buildBrush.minmax.maxs[ axis ];
					}
				}
				else
				{
					/* ydnar: adding bevel plane snapping for fewer bsp planes */
					if ( bevelSnap > 0 ) {
						plane.dist() = -ceil( buildBrush.minmax.mins[ axis ] / bevelSnap ) * bevelSnap;
					}
					else{
						plane.dist() = -buildBrush.minmax.mins[ axis ];
					}
				}

				s.planenum = FindFloatPlane( plane, 0, NULL );
				s.contentFlags = sides[ 0 ].contentFlags;
				/* handle bevel surfaceflags */
				for ( const side_t& side : sides ) {
					for ( const Vector3& point : side.winding ) {
						if ( fabs( plane.dist() - point[axis] ) < .1f ) {
							s.surfaceFlags |= ( side.surfaceFlags & surfaceFlagsMask );
							break;
						}
					}
				}
				s.bevel = true;
				c_boxbevels++;
			}

			// if the plane is not in it canonical order, swap it
			if ( i != order ) {
				std::swap( sides[order], sides[i] );
			}
		}
	}

	//
	// add the edge bevels
	//
	if ( sides.size() == 6 ) {
		return;     // pure axial
	}

	// test the non-axial plane edges
	for ( size_t i = 6; i < sides.size(); ++i ) {
		for ( size_t j = 0; j < sides[i].winding.size(); j++ ) {
			Vector3 vec = sides[i].winding[j] - sides[i].winding[winding_next( sides[i].winding, j )];
			if ( VectorNormalize( vec ) < 0.5f ) {
				continue;
			}
			SnapNormal( vec );
			if ( vec[0] == -1.0f || vec[0] == 1.0f || ( vec[0] == 0.0f && vec[1] == 0.0f )
			  || vec[1] == -1.0f || vec[1] == 1.0f || ( vec[1] == 0.0f && vec[2] == 0.0f )
			  || vec[2] == -1.0f || vec[2] == 1.0f || ( vec[2] == 0.0f && vec[0] == 0.0f ) ) {
				continue; // axial, only test non-axial edges
			}

			/* debug code */
			//%	Sys_Printf( "-------------\n" );

			// try the six possible slanted axials from this edge
			for ( int axis = 0; axis < 3; axis++ ) {
				for ( int dir = -1; dir <= 1; dir += 2 ) {
					// construct a plane
					Vector3 vec2( 0 );
					vec2[axis] = dir;
					Plane3f plane;
					plane.normal() = vector3_cross( vec, vec2 );
					if ( VectorNormalize( plane.normal() ) < 0.5f ) {
						continue;
					}
					plane.dist() = vector3_dot( sides[i].winding[j], plane.normal() );

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					auto iside = sides.begin();
					for ( ; iside != sides.end(); ++iside ) {

						// if this plane has already been used, skip it
						if ( PlaneEqual( mapplanes[iside->planenum], plane ) ) {
							if( iside->bevel ){ /* handle bevel surfaceflags */
								iside->surfaceFlags |= ( sides[i].surfaceFlags & surfaceFlagsMask );
							}
							break;
						}

						const winding_t& w2 = iside->winding;
						if ( w2.empty() ) {
							continue;
						}
						float minBack = 0.0f;
						const auto point_in_front = [&w2, &plane, &minBack](){
							for ( const Vector3& point : w2 ) {
								const float d = plane3_distance_to_point( plane, point );
								if ( d > 0.1f ) {
									return true;
								}
								value_minimize( minBack, d );
							}
							return false;
						};
						// if some point was at the front
						if ( point_in_front() ) {
							break;
						}

						// if no points at the back then the winding is on the bevel plane
						if ( minBack > -0.1f ) {
							//%	Sys_Printf( "On bevel plane\n" );
							break;
						}
					}

					if ( iside != sides.end() ) {
						continue;   // wasn't part of the outer hull
					}

					/* debug code */
					//%	Sys_Printf( "n = %f %f %f\n", normal[ 0 ], normal[ 1 ], normal[ 2 ] );

					// add this plane
					if ( sides.size() == MAX_BUILD_SIDES ) {
						xml_Select( "MAX_BUILD_SIDES", buildBrush.entityNum, buildBrush.brushNum, true );
					}
					side_t& s2 = sides.emplace_back();

					s2.planenum = FindFloatPlane( plane, 1, &sides[i].winding[ j ] );
					s2.contentFlags = sides[0].contentFlags;
					s2.surfaceFlags = ( sides[i].surfaceFlags & surfaceFlagsMask ); /* handle bevel surfaceflags */
					s2.bevel = true;
					c_edgebevels++;
				}
			}
		}
	}
}



static void MergeOrigin( entity_t& ent, const Vector3& origin ){
	char string[128];

	/* we have not parsed the brush completely yet... */
	ent.origin = ent.vectorForKey( "origin" ) + origin - ent.originbrush_origin;

	ent.originbrush_origin = origin;

	sprintf( string, "%f %f %f", ent.origin[0], ent.origin[1], ent.origin[2] );
	ent.setKeyValue( "origin", string );
}

/*
   FinishBrush()
   produces a final brush based on the buildBrush->sides array
   and links it to the current entity
 */

static void FinishBrush( bool noCollapseGroups, entity_t& mapEnt ){
	/* create windings for sides and bounds for brush */
	if ( !CreateBrushWindings( buildBrush ) ) {
		return;
	}

	/* origin brushes are removed, but they set the rotation origin for the rest of the brushes in the entity.
	   after the entire entity is parsed, the planenums and texinfos will be adjusted for the origin brush */
	if ( buildBrush.compileFlags & C_ORIGIN ) {
		Sys_Printf( "Entity %i (%s), Brush %i: origin brush detected\n",
		            mapEnt.mapEntityNum, mapEnt.classname(), buildBrush.brushNum );

		if ( entities.size() == 1 ) {
			Sys_FPrintf( SYS_WRN, "Entity %i, Brush %i: origin brushes not allowed in world\n",
			             mapEnt.mapEntityNum, buildBrush.brushNum );
			return;
		}

		MergeOrigin( mapEnt, buildBrush.minmax.origin() );

		/* don't keep this brush */
		return;
	}

	/* determine if the brush is an area portal */
	if ( buildBrush.compileFlags & C_AREAPORTAL ) {
		if ( entities.size() != 1 ) {
			Sys_FPrintf( SYS_WRN, "Entity %i (%s), Brush %i: areaportals only allowed in world\n", mapEnt.mapEntityNum, mapEnt.classname(), buildBrush.brushNum );
			return;
		}
	}

	/* add bevel planes */
	if ( !noCollapseGroups ) {
		AddBrushBevels();
	}

	/* keep it */
	/* link opaque brushes to head of list, translucent brushes to end */
	brush_t& b = ( buildBrush.opaque )? mapEnt.brushes.emplace_front( buildBrush )
	                                   : mapEnt.brushes.emplace_back( buildBrush );

	/* set original */
	b.original = &b;

	/* link colorMod volume brushes to the entity directly */
	if ( b.contentShader != NULL &&
	     b.contentShader->colorMod != NULL &&
	     b.contentShader->colorMod->type == EColorMod::Volume ) {
		mapEnt.colorModBrushes.push_back( &b );
	}
}



/*
   TextureAxisFromPlane()
   determines best orthogonal axis to project a texture onto a wall
   (must be identical in radiant!)
 */

static const Vector3 baseaxis[18] =
{
	 g_vector3_axis_z, g_vector3_axis_x, -g_vector3_axis_y,        // floor
	-g_vector3_axis_z, g_vector3_axis_x, -g_vector3_axis_y,        // ceiling
	 g_vector3_axis_x, g_vector3_axis_y, -g_vector3_axis_z,        // west wall
	-g_vector3_axis_x, g_vector3_axis_y, -g_vector3_axis_z,        // east wall
	 g_vector3_axis_y, g_vector3_axis_x, -g_vector3_axis_z,        // south wall
	-g_vector3_axis_y, g_vector3_axis_x, -g_vector3_axis_z         // north wall
};

std::array<Vector3, 2> TextureAxisFromPlane( const plane_t& plane ){
	float best = 0;
	int bestaxis = 0;

	for ( int i = 0; i < 6; ++i )
	{
		const float dot = vector3_dot( plane.normal(), baseaxis[i * 3] );
		if ( dot > best + 0.0001f ) { /* ydnar: bug 637 fix, suggested by jmonroe */
			best = dot;
			bestaxis = i;
		}
	}

	return { baseaxis[bestaxis * 3 + 1],
	         baseaxis[bestaxis * 3 + 2] };
}



/*
   QuakeTextureVecs()
   creates world-to-texture mapping vecs for crappy quake plane arrangements
 */

static void QuakeTextureVecs( const plane_t& plane, float shift[ 2 ], float rotate, float scale[ 2 ], Vector4 mappingVecs[ 2 ] ){
	int sv, tv;
	float sinv, cosv;
	auto vecs = TextureAxisFromPlane( plane );


	if ( !scale[0] ) {
		scale[0] = 1;
	}
	if ( !scale[1] ) {
		scale[1] = 1;
	}

	// rotate axis
	if ( rotate == 0 ) {
		sinv = 0; cosv = 1;
	}
	else if ( rotate == 90 ) {
		sinv = 1; cosv = 0;
	}
	else if ( rotate == 180 ) {
		sinv = 0; cosv = -1;
	}
	else if ( rotate == 270 ) {
		sinv = -1; cosv = 0;
	}
	else
	{
		const double ang = degrees_to_radians( rotate );
		sinv = sin( ang );
		cosv = cos( ang );
	}

	if ( vecs[0][0] ) {
		sv = 0;
	}
	else if ( vecs[0][1] ) {
		sv = 1;
	}
	else{
		sv = 2;
	}

	if ( vecs[1][0] ) {
		tv = 0;
	}
	else if ( vecs[1][1] ) {
		tv = 1;
	}
	else{
		tv = 2;
	}

	for ( int i = 0; i < 2; ++i ) {
		const float ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
		const float nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
		vecs[i][sv] = ns;
		vecs[i][tv] = nt;

		mappingVecs[i].vec3() = vecs[i] / scale[i];
	}

	mappingVecs[0][3] = shift[0];
	mappingVecs[1][3] = shift[1];
}



/*
   ParseRawBrush()
   parses the sides into buildBrush->sides[], nothing else.
   no validation, back plane removal, etc.

   Timo - 08/26/99
   added brush epairs parsing ( ignoring actually )
   Timo - 08/04/99
   added exclusive brush primitive parsing
   Timo - 08/08/99
   support for old brush format back in
   NOTE: it would be "cleaner" to have separate functions to parse between old and new brushes
 */

static void ParseRawBrush( bool onlyLights ){
	/* initial setup */
	buildBrush.sides.clear();
	buildBrush.detail = false;

	/* bp */
	if ( g_brushType == EBrushType::Bp ) {
		MatchToken( "{" );
	}

	/* parse sides */
	while ( GetToken( true ) && !strEqual( token, "}" ) )
	{
		/* ttimo : bp: here we may have to jump over brush epairs (only used in editor) */
		if ( g_brushType == EBrushType::Bp ) {
			while ( !strEqual( token, "(" ) )
			{
				GetToken( false );
				GetToken( true );
			}
		}
		UnGetToken();

		/* test side count */
		if ( buildBrush.sides.size() >= MAX_BUILD_SIDES ) {
			xml_Select( "MAX_BUILD_SIDES", buildBrush.entityNum, buildBrush.brushNum, true );
		}

		/* add side */
		side_t& side = buildBrush.sides.emplace_back();

		/* read the three point plane definition */
		DoubleVector3 planePoints[ 3 ];
		Parse1DMatrix( 3, planePoints[ 0 ].data() );
		Parse1DMatrix( 3, planePoints[ 1 ].data() );
		Parse1DMatrix( 3, planePoints[ 2 ].data() );

		/* find the plane number */
		side.planenum = MapPlaneFromPoints( planePoints );
		PlaneFromPoints( side.plane, planePoints );

		/* bp: read the texture matrix */
		if ( g_brushType == EBrushType::Bp ) {
			Parse2DMatrix( 2, 3, side.texMat->data() );
		}

		/* read shader name */
		GetToken( false );
		const String64 shader( "textures/", token );

		/* set default flags and values */
		shaderInfo_t *si = onlyLights? &shaderInfo[ 0 ]
		                             : ShaderInfoForShader( shader );
		side.shaderInfo = si;
		side.surfaceFlags = si->surfaceFlags;
		side.contentFlags = si->contentFlags;
		side.compileFlags = si->compileFlags;
		side.value = si->value;

		/* AP or 220? */
		if ( g_brushType == EBrushType::Undefined ){
			GetToken( false );
			if ( strEqual( token, "[" ) ){
				g_brushType = EBrushType::Valve220;
				Sys_FPrintf( SYS_VRB, "detected brushType = VALVE 220\n" );
			}
			else{
				g_brushType = EBrushType::Quake;
				Sys_FPrintf( SYS_VRB, "detected brushType = QUAKE (Axial Projection)\n" );
			}
			UnGetToken();
		}

		if ( g_brushType == EBrushType::Quake ) {
			float shift[ 2 ], rotate, scale[ 2 ];

			GetToken( false );
			shift[ 0 ] = atof( token );
			GetToken( false );
			shift[ 1 ] = atof( token );
			GetToken( false );
			rotate = atof( token );
			GetToken( false );
			scale[ 0 ] = atof( token );
			GetToken( false );
			scale[ 1 ] = atof( token );

			/* ydnar: gs mods: bias texture shift */
			if ( !si->globalTexture ) {
				shift[ 0 ] -= ( floor( shift[ 0 ] / si->shaderWidth ) * si->shaderWidth );
				shift[ 1 ] -= ( floor( shift[ 1 ] / si->shaderHeight ) * si->shaderHeight );
			}

			/* get the texture mapping for this texturedef / plane combination */
			QuakeTextureVecs( mapplanes[ side.planenum ], shift, rotate, scale, side.vecs );
		}
		else if ( g_brushType == EBrushType::Valve220 ){
			for ( int axis = 0; axis < 2; ++axis ){
				MatchToken( "[" );
				for ( int comp = 0; comp < 4; ++comp ){
					GetToken( false );
					side.vecs[axis][comp] = atof( token );
				}
				MatchToken( "]" );
			}
			GetToken( false ); // rotate
			float scale[2];
			GetToken( false );
			scale[ 0 ] = atof( token );
			GetToken( false );
			scale[ 1 ] = atof( token );

			if ( !scale[0] ) scale[0] = 1.f;
			if ( !scale[1] ) scale[1] = 1.f;
			for ( int axis = 0; axis < 2; ++axis )
				side.vecs[axis].vec3() /= scale[axis];
		}

		/*
		    historically, there are 3 integer values at the end of a brushside line in a .map file.
		    in quake 3, the only thing that mattered was the first of these three values, which
		    was previously the content flags. and only then did a single bit matter, the detail
		    bit. because every game has its own special flags for specifying detail, the
		    traditionally game-specified CONTENTS_DETAIL flag was overridden for Q3Map 2.3.0
		    by C_DETAIL, defined in q3map2.h. the value is exactly as it was before, but
		    is stored in compileFlags, as opposed to contentFlags, for multiple-game
		    portability. :sigh:
		 */

		if ( TokenAvailable() ) {
			/* get detail bit from map content flags */
			GetToken( false );
			const int flags = atoi( token );
			if ( flags & C_DETAIL ) {
				side.compileFlags |= C_DETAIL;
			}

			/* historical */
			GetToken( false );
			//% td.flags = atoi( token );
			GetToken( false );
			//% td.value = atoi( token );
		}
	}

	/* bp */
	if ( g_brushType == EBrushType::Bp ) {
		UnGetToken();
		MatchToken( "}" );
		MatchToken( "}" );
	}
}



/*
   RemoveDuplicateBrushPlanes
   returns false if the brush has a mirrored set of planes,
   meaning it encloses no volume.
   also removes planes without any normal
 */

static bool RemoveDuplicateBrushPlanes( brush_t& b ){
	auto& sides = b.sides;

	for( auto it = sides.cbegin(); it != sides.cend(); ){
		// check for a degenerate plane
		if ( it->planenum == -1 ) {
			xml_Select( "degenerate plane", b.entityNum, b.brushNum, false );
			// remove it
			it = sides.erase( it );
		}
		else{
			++it;
		}
	}

	for ( size_t i = 0; i < sides.size(); ++i ) {
		// check for duplication and mirroring
		for ( size_t j = i + 1; j < sides.size(); ) {
			if ( sides[i].planenum == sides[j].planenum ) {
				xml_Select( "duplicate plane", b.entityNum, b.brushNum, false );
				// remove the second duplicate
				sides.erase( sides.cbegin() + j );
			}
			else if ( sides[i].planenum == ( sides[j].planenum ^ 1 ) ) {
				// mirror plane, brush is invalid
				xml_Select( "mirrored plane", b.entityNum, b.brushNum, false );
				return false;
			}
			else{
				++j;
			}
		}
	}
	return true;
}



/*
   ParseBrush()
   parses a brush out of a map file and sets it up
 */

static void ParseBrush( bool onlyLights, bool noCollapseGroups, entity_t& mapEnt, int mapPrimitiveNum ){
	/* parse the brush out of the map */
	ParseRawBrush( onlyLights );

	/* only go this far? */
	if ( onlyLights ) {
		return;
	}

	/* set some defaults */
	buildBrush.portalareas[ 0 ] = -1;
	buildBrush.portalareas[ 1 ] = -1;
	/* set map entity and brush numbering */
	buildBrush.entityNum = mapEnt.mapEntityNum;
	buildBrush.brushNum = mapPrimitiveNum;

	/* if there are mirrored planes, the entire brush is invalid */
	if ( !RemoveDuplicateBrushPlanes( buildBrush ) ) {
		return;
	}

	/* get the content for the entire brush */
	SetBrushContents( buildBrush );

	/* allow detail brushes to be removed */
	if ( nodetail && ( buildBrush.compileFlags & C_DETAIL ) ) {
		return;
	}

	/* allow liquid brushes to be removed */
	if ( nowater && ( buildBrush.compileFlags & C_LIQUID ) ) {
		return;
	}

	/* ydnar: allow hint brushes to be removed */
	if ( noHint && ( buildBrush.compileFlags & C_HINT ) ) {
		return;
	}

	/* finish the brush */
	FinishBrush( noCollapseGroups, mapEnt );
}



/*
   AdjustBrushesForOrigin()
 */

static void AdjustBrushesForOrigin( entity_t& ent ){
	/* walk brush list */
	for ( brush_t& b : ent.brushes )
	{
		/* offset brush planes */
		for ( side_t& side : b.sides )
		{
			/* offset side plane */
			const float newdist = -plane3_distance_to_point( mapplanes[ side.planenum ].plane, ent.originbrush_origin );

			/* find a new plane */
			side.planenum = FindFloatPlane( mapplanes[ side.planenum ].normal(), newdist, 0, NULL );
			side.plane.dist() = -plane3_distance_to_point( side.plane, ent.originbrush_origin );
		}

		/* rebuild brush windings (ydnar: just offsetting the winding above should be fine) */
		CreateBrushWindings( b );
	}

	/* walk patch list */
	for ( parseMesh_t *p = ent.patches; p != NULL; p = p->next )
	{
		for ( bspDrawVert_t& vert : Span( p->mesh.verts, p->mesh.width * p->mesh.height ) )
			vert.xyz -= ent.originbrush_origin;
	}
}



/*
   MoveBrushesToWorld()
   takes all of the brushes from the current entity and
   adds them to the world's brush list
   (used by func_group)
 */

static void MoveBrushesToWorld( entity_t& ent ){
	/* we need to undo the common/origin adjustment, and instead shift them by the entity key origin */
	ent.originbrush_origin = -ent.origin;
	AdjustBrushesForOrigin( ent );
	ent.originbrush_origin.set( 0 );

	/* move brushes */
	for ( brushlist_t::const_iterator next, b = ent.brushes.begin(); b != ent.brushes.end(); b = next )
	{
		/* get next brush */
		next = std::next( b );

		/* link opaque brushes to head of list, translucent brushes to end */
		if ( b->opaque ) {
			entities[ 0 ].brushes.splice( entities[ 0 ].brushes.begin(), ent.brushes, b );
		}
		else
		{
			entities[ 0 ].brushes.splice( entities[ 0 ].brushes.end(), ent.brushes, b );
		}
	}

	/* ydnar: move colormod brushes */
	if ( !ent.colorModBrushes.empty() ) {
		entities[ 0 ].colorModBrushes.insert( entities[ 0 ].colorModBrushes.end(), ent.colorModBrushes.begin(), ent.colorModBrushes.end() );
		ent.colorModBrushes.clear();
	}

	/* move patches */
	if ( ent.patches != NULL ) {
		parseMesh_t *pm;
		for ( pm = ent.patches; pm->next != NULL; pm = pm->next ){};

		pm->next = entities[ 0 ].patches;
		entities[ 0 ].patches = ent.patches;

		ent.patches = NULL;
	}
}



/*
   SetEntityBounds() - ydnar
   finds the bounds of an entity's brushes (necessary for terrain-style generic metashaders)
 */

static void SetEntityBounds( entity_t& e ){
	MinMax minmax;

	/* walk the entity's brushes/patches and determine bounds */
	for ( const brush_t& b : e.brushes )
	{
		minmax.extend( b.minmax );
	}
	for ( const parseMesh_t *p = e.patches; p; p = p->next )
	{
		for ( const bspDrawVert_t& vert : Span( p->mesh.verts, p->mesh.width * p->mesh.height ) )
			minmax.extend( vert.xyz );
	}

	/* try to find explicit min/max key */
	e.read_keyvalue( minmax.mins, "min" );
	e.read_keyvalue( minmax.maxs, "max" );

	/* store the bounds */
	for ( brush_t& b : e.brushes )
	{
		b.eMinmax = minmax;
	}
	for ( parseMesh_t *p = e.patches; p; p = p->next )
	{
		p->eMinmax = minmax;
	}
}



/*
   LoadEntityIndexMap() - ydnar
   based on LoadAlphaMap() from terrain.c, a little more generic
 */

static void LoadEntityIndexMap( entity_t& e ){
	int numLayers, w, h;
	const char      *indexMapFilename, *shader;
	byte            *pixels;


	/* this only works with bmodel ents */
	if ( e.brushes.empty() && e.patches == NULL ) {
		return;
	}

	/* determine if there is an index map (support legacy "alphamap" key as well) */
	if( !e.read_keyvalue( indexMapFilename, "_indexmap", "alphamap" ) )
		return;

	/* get number of layers (support legacy "layers" key as well) */
	if( !e.read_keyvalue( numLayers, "_layers", "layers" ) ){
		Sys_Warning( "Entity with index/alpha map \"%s\" has missing \"_layers\" or \"layers\" key\n", indexMapFilename );
		Sys_Printf( "Entity will not be textured properly. Check your keys/values.\n" );
		return;
	}
	if ( numLayers < 1 ) {
		Sys_Warning( "Entity with index/alpha map \"%s\" has < 1 layer (%d)\n", indexMapFilename, numLayers );
		Sys_Printf( "Entity will not be textured properly. Check your keys/values.\n" );
		return;
	}

	/* get base shader name (support legacy "shader" key as well) */
	if( !e.read_keyvalue( shader, "_shader", "shader" ) ){
		Sys_Warning( "Entity with index/alpha map \"%s\" has missing \"_shader\" or \"shader\" key\n", indexMapFilename );
		Sys_Printf( "Entity will not be textured properly. Check your keys/values.\n" );
		return;
	}

	/* note it */
	Sys_FPrintf( SYS_VRB, "Entity %d (%s) has shader index map \"%s\"\n", e.mapEntityNum, e.classname(), indexMapFilename );

	/* handle tga image */
	if ( path_extension_is( indexMapFilename, "tga" ) ) {
		/* load it */
		unsigned int    *pixels32;
		Load32BitImage( indexMapFilename, &pixels32, &w, &h );

		/* convert to bytes */
		const int size = w * h;
		pixels = safe_malloc( size );
		for ( int i = 0; i < size; i++ )
		{
			pixels[ i ] = ( ( pixels32[ i ] & 0xFF ) * numLayers ) / 256;
			if ( pixels[ i ] >= numLayers ) {
				pixels[ i ] = numLayers - 1;
			}
		}

		/* free the 32 bit image */
		free( pixels32 );
	}
	else
	{
		/* load it */
		Load256Image( indexMapFilename, &pixels, NULL, &w, &h );

		/* debug code */
		//%	Sys_Printf( "-------------------------------" );

		/* fix up out-of-range values */
		const int size = w * h;
		for ( int i = 0; i < size; i++ )
		{
			if ( pixels[ i ] >= numLayers ) {
				pixels[ i ] = numLayers - 1;
			}

			/* debug code */
			//%	if( ( i % w ) == 0 )
			//%		Sys_Printf( "\n" );
			//%	Sys_Printf( "%c", pixels[ i ] + '0' );
		}

		/* debug code */
		//%	Sys_Printf( "\n-------------------------------\n" );
	}

	/* the index map must be at least 2x2 pixels */
	if ( w < 2 || h < 2 ) {
		Sys_Warning( "Entity with index/alpha map \"%s\" is smaller than 2x2 pixels\n", indexMapFilename );
		Sys_Printf( "Entity will not be textured properly. Check your keys/values.\n" );
		free( pixels );
		return;
	}

	/* create a new index map */
	indexMap_t *im = safe_malloc( sizeof( *im ) );
	new ( im ) indexMap_t{}; // placement new

	/* set it up */
	im->w = w;
	im->h = h;
	im->numLayers = numLayers;
	im->shader = shader;
	im->pixels = pixels;

	/* get height offsets */
	const char *offset;
	if( e.read_keyvalue( offset, "_offsets", "offsets" ) ){
		/* value is a space-separated set of numbers */
		/* get each value */
		for ( int i = 0; i < 256 && !strEmpty( offset ); ++i )
		{
			const char *space = strchr( offset, ' ' );
			if ( space == NULL ) {
				space = offset + strlen( offset );
			}
			im->offsets[ i ] = atof( String64( StringRange( offset, space ) ) );
			if ( space == NULL ) {
				break;
			}
			offset = space + 1;
		}
	}

	/* store the index map in every brush/patch in the entity */
	for ( brush_t& b : e.brushes )
		b.im = im;
	for ( parseMesh_t *p = e.patches; p != NULL; p = p->next )
		p->im = im;
}







/*
   ParseMapEntity()
   parses a single entity out of a map file
 */

static bool ParseMapEntity( bool onlyLights, bool noCollapseGroups, int mapEntityNum ){
	/* eof check */
	if ( !GetToken( true ) ) {
		return false;
	}

	/* conformance check */
	if ( !strEqual( token, "{" ) ) {
		Sys_Warning( "ParseEntity: { not found, found %s on line %d - last entity was at: <%4.2f, %4.2f, %4.2f>...\n"
		             "Continuing to process map, but resulting BSP may be invalid.\n",
		             token, scriptline, entities.back().origin[ 0 ], entities.back().origin[ 1 ], entities.back().origin[ 2 ] );
		return false;
	}

	/* setup */
	entity_t& mapEnt = entities.emplace_back();
	int mapPrimitiveNum = 0; /* track .map file numbering of primitives inside an entity */

	/* ydnar: true entity numbering */
	mapEnt.mapEntityNum = mapEntityNum;

	/* loop */
	while ( 1 )
	{
		/* get initial token */
		if ( !GetToken( true ) ) {
			Sys_Warning( "ParseEntity: EOF without closing brace\n"
			             "Continuing to process map, but resulting BSP may be invalid.\n" );
			return false;
		}

		if ( strEqual( token, "}" ) ) {
			break;
		}

		if ( strEqual( token, "{" ) ) {
			/* parse a brush or patch */
			if ( !GetToken( true ) ) {
				break;
			}

			/* check */
			if ( strEqual( token, "patchDef2" ) ) {
				++c_patches;
				ParsePatch( onlyLights, mapEnt, mapPrimitiveNum );
			}
			else if ( strEqual( token, "terrainDef" ) ) {
				//% ParseTerrain();
				Sys_Warning( "Terrain entity parsing not supported in this build.\n" ); /* ydnar */
			}
			else if ( strEqual( token, "brushDef" ) ) {
				if ( g_brushType == EBrushType::Undefined ) {
					Sys_FPrintf( SYS_VRB, "detected brushType = BRUSH PRIMITIVES\n" );
					g_brushType = EBrushType::Bp;
				}
				ParseBrush( onlyLights, noCollapseGroups, mapEnt, mapPrimitiveNum );
			}
			else
			{
				/* AP or 220 */
				UnGetToken(); // (
				ParseBrush( onlyLights, noCollapseGroups, mapEnt, mapPrimitiveNum );
			}
			++mapPrimitiveNum;
		}
		else
		{
			/* parse a key / value pair */
			ParseEPair( mapEnt.epairs );
		}
	}

	/* ydnar: get classname */
	const char *classname = mapEnt.classname();

	/* ydnar: only lights? */
	if ( onlyLights && !striEqualPrefix( classname, "light" ) ) {
		entities.pop_back();
		return true;
	}

	/* ydnar: determine if this is a func_group */
	const bool funcGroup = striEqual( "func_group", classname );

	/* worldspawn (and func_groups) default to cast/recv shadows in worldspawn group */
	int castShadows, recvShadows;
	if ( funcGroup || mapEnt.mapEntityNum == 0 ) {
		//%	Sys_Printf( "World:  %d\n", mapEnt.mapEntityNum );
		castShadows = WORLDSPAWN_CAST_SHADOWS;
		recvShadows = WORLDSPAWN_RECV_SHADOWS;
	}
	else{    /* other entities don't cast any shadows, but recv worldspawn shadows */
		//%	Sys_Printf( "Entity: %d\n", mapEnt.mapEntityNum );
		castShadows = ENTITY_CAST_SHADOWS;
		recvShadows = ENTITY_RECV_SHADOWS;
	}

	/* get explicit shadow flags */
	GetEntityShadowFlags( &mapEnt, NULL, &castShadows, &recvShadows );

	/* ydnar: get lightmap scaling value for this entity */
	const float lightmapScale = std::max( 0.f, mapEnt.floatForKey( "lightmapscale", "_lightmapscale", "_ls" ) );
	if ( lightmapScale != 0 )
		Sys_Printf( "Entity %d (%s) has lightmap scale of %.4f\n", mapEnt.mapEntityNum, classname, lightmapScale );

	/* ydnar: get cel shader :) for this entity */
	shaderInfo_t *celShader;
	const char *value;
	if( mapEnt.read_keyvalue( value, "_celshader" ) ||
	    entities[ 0 ].read_keyvalue( value, "_celshader" ) ){
		celShader = ShaderInfoForShader( String64( "textures/", value ) );
		Sys_Printf( "Entity %d (%s) has cel shader %s\n", mapEnt.mapEntityNum, classname, celShader->shader.c_str() );
	}
	else{
		celShader = globalCelShader.empty() ? NULL : ShaderInfoForShader( globalCelShader );
	}

	/* jal : entity based _shadeangle */
	const float shadeAngle = std::max( 0.f, mapEnt.floatForKey( "_shadeangle",
	                                      "_smoothnormals", "_sn", "_sa", "_smooth" ) ); /* vortex' aliases */
	if ( shadeAngle != 0 )
		Sys_Printf( "Entity %d (%s) has shading angle of %.4f\n", mapEnt.mapEntityNum, classname, shadeAngle );

	/* jal : entity based _samplesize */
	const int lightmapSampleSize = std::max( 0, mapEnt.intForKey( "_lightmapsamplesize", "_samplesize", "_ss" ) );
	if ( lightmapSampleSize != 0 )
		Sys_Printf( "Entity %d (%s) has lightmap sample size of %d\n", mapEnt.mapEntityNum, classname, lightmapSampleSize );

	/* attach stuff to everything in the entity */
	for ( brush_t& brush : mapEnt.brushes )
	{
		brush.entityNum = mapEnt.mapEntityNum;
		brush.castShadows = castShadows;
		brush.recvShadows = recvShadows;
		brush.lightmapSampleSize = lightmapSampleSize;
		brush.lightmapScale = lightmapScale;
		brush.celShader = celShader;
		brush.shadeAngleDegrees = shadeAngle;
	}

	for ( parseMesh_t *patch = mapEnt.patches; patch != NULL; patch = patch->next )
	{
		patch->entityNum = mapEnt.mapEntityNum;
		patch->castShadows = castShadows;
		patch->recvShadows = recvShadows;
		patch->lightmapSampleSize = lightmapSampleSize;
		patch->lightmapScale = lightmapScale;
		patch->celShader = celShader;
	}

	/* ydnar: gs mods: set entity bounds */
	SetEntityBounds( mapEnt );

	/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
	LoadEntityIndexMap( mapEnt );

	/* get entity origin and adjust brushes */
	mapEnt.origin = mapEnt.vectorForKey( "origin" );
	if ( mapEnt.originbrush_origin != g_vector3_identity ) {
		AdjustBrushesForOrigin( mapEnt );
	}

	/* group_info entities are just for editor grouping (fixme: leak!) */
	if ( !noCollapseGroups && striEqual( "group_info", classname ) ) {
		entities.pop_back();
		return true;
	}

	/* group entities are just for editor convenience, toss all brushes into worldspawn */
	if ( !noCollapseGroups && funcGroup ) {
		MoveBrushesToWorld( mapEnt );
		entities.pop_back();
		return true;
	}

	/* done */
	return true;
}



/*
   LoadMapFile()
   loads a map file into a list of entities
 */

void LoadMapFile( const char *filename, bool onlyLights, bool noCollapseGroups ){
	int oldNumEntities = 0;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- LoadMapFile ---\n" );

	/* load the map file */
	if( !LoadScriptFile( filename, -1 ) )
		Error( "" );

	/* setup */
	if ( onlyLights ) {
		oldNumEntities = entities.size();
	}
	else{
		entities.clear();
	}

	/* initial setup */
	numMapDrawSurfs = 0;
	c_detail = 0;
	g_brushType = EBrushType::Undefined;

	/* allocate a very large temporary brush for building the brushes as they are loaded */
	buildBrush.sides.reserve( MAX_BUILD_SIDES );

	/* parse the map file */
	int mapEntityNum = 0; /* track .map file entities numbering */
	while ( ParseMapEntity( onlyLights, noCollapseGroups, mapEntityNum++ ) ){};

	/* light loading */
	if ( onlyLights ) {
		/* emit some statistics */
		Sys_FPrintf( SYS_VRB, "%9zu light entities\n", entities.size() - oldNumEntities );
	}
	else
	{
		/* set map bounds */
		g_mapMinmax.clear();
		for ( const brush_t& brush : entities[ 0 ].brushes )
		{
			g_mapMinmax.extend( brush.minmax );
		}

		/* get brush counts */
		const int numMapBrushes = entities[ 0 ].brushes.size();
		if ( (float) c_detail / (float) numMapBrushes < 0.10f && numMapBrushes > 500 ) {
			Sys_Warning( "Over 90 percent structural map detected. Compile time may be adversely affected.\n" );
		}

		/* emit some statistics */
		Sys_FPrintf( SYS_VRB, "%9d total world brushes\n", numMapBrushes );
		Sys_FPrintf( SYS_VRB, "%9d detail brushes\n", c_detail );
		Sys_FPrintf( SYS_VRB, "%9d patches\n", c_patches );
		Sys_FPrintf( SYS_VRB, "%9d boxbevels\n", c_boxbevels );
		Sys_FPrintf( SYS_VRB, "%9d edgebevels\n", c_edgebevels );
		Sys_FPrintf( SYS_VRB, "%9zu entities\n", entities.size() );
		Sys_FPrintf( SYS_VRB, "%9zu planes\n", mapplanes.size() );
		Sys_Printf( "%9d areaportals\n", c_areaportals );
		Sys_Printf( "Size: %5.0f, %5.0f, %5.0f to %5.0f, %5.0f, %5.0f\n",
		            g_mapMinmax.mins[0], g_mapMinmax.mins[1], g_mapMinmax.mins[2],
		            g_mapMinmax.maxs[0], g_mapMinmax.maxs[1], g_mapMinmax.maxs[2] );

		/* write bogus map */
		if ( fakemap ) {
			WriteBSPBrushMap( "fakemap.map", entities[ 0 ].brushes );
		}
	}
}

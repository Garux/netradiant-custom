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
#include "vis.h"
#include "visflow.h"

vportal_t          *sorted_portals[ MAX_MAP_PORTALS * 2 ];


static visPlane_t PlaneFromWinding( const fixedWinding_t *w ){
	// calc plane
	visPlane_t plane;
	PlaneFromPoints( plane, w->points[0], w->points[1], w->points[2] );
	return plane;
}


/*
   NewFixedWinding()
   returns a new fixed winding
   ydnar: altered this a bit to reconcile multiply-defined winding_t
 */

static fixedWinding_t *NewFixedWinding( int numpoints ){
	if ( numpoints > MAX_POINTS_ON_WINDING ) {
		Error( "NewWinding: %i points", numpoints );
	}
	return safe_calloc( offsetof_array( fixedWinding_t, points, numpoints ) );
}



static void print_leaf( const leaf_t *l ){
	for ( const vportal_t *p : Span( l->portals, l->numportals ) )
	{
		const visPlane_t pl = p->plane;
		Sys_Printf( "portal %4i to leaf %4i : %7.1f : (%4.1f, %4.1f, %4.1f)\n", (int)( p - portals ), p->leaf, pl.dist(), pl.normal()[0], pl.normal()[1], pl.normal()[2] );
	}
}


//=============================================================================

/*
   =============
   SortPortals

   Sorts the portals from the least complex, so the later ones can reuse
   the earlier information.
   =============
 */
static void SortPortals(){
	for ( int i = 0; i < numportals * 2; ++i )
		sorted_portals[i] = &portals[i];

	if ( !nosort ) {
		std::sort( sorted_portals, sorted_portals + numportals * 2, []( vportal_t* const & a, vportal_t* const & b ){
			return a->nummightsee < b->nummightsee;
		} );
	}
}


/*
   ==============
   LeafVectorFromPortalVector
   ==============
 */
static int LeafVectorFromPortalVector( byte *portalbits, byte *leafbits ){
	for ( int i = 0; i < numportals * 2; ++i )
	{
		if ( bit_is_enabled( portalbits, i ) ) {
			const vportal_t& p = portals[i];
			bit_enable( leafbits, p.leaf );
		}
	}

	for ( int i = 0; i < portalclusters; ++i )
	{
		int leafnum = i;
		while ( leafs[leafnum].merged >= 0 )
			leafnum = leafs[leafnum].merged;
		//if the merged leaf is visible then the original leaf is visible
		if ( bit_is_enabled( leafbits, leafnum ) ) {
			bit_enable( leafbits, i );
		}
	}
	return CountBits( leafbits, portalclusters ); //c_leafs
}


/*
   ===============
   ClusterMerge

   Merges the portal visibility for a leaf
   ===============
 */
static int clustersizehistogram[MAX_MAP_LEAFS] = {0};

static void ClusterMerge( int leafnum ){
	byte portalvector[MAX_PORTALS / 8];
	byte uncompressed[MAX_MAP_LEAFS / 8];
	int numvis, mergedleafnum;

	// OR together all the portalvis bits

	mergedleafnum = leafnum;
	while ( leafs[mergedleafnum].merged >= 0 )
		mergedleafnum = leafs[mergedleafnum].merged;

	memset( portalvector, 0, portalbytes );

	for ( const vportal_t *p : Span( leafs[mergedleafnum].portals, leafs[mergedleafnum].numportals ) )
	{
		if ( p->removed ) {
			continue;
		}

		if ( p->status != EVStatus::Done ) {
			Error( "portal not done" );
		}
		for ( int j = 0; j < portallongs; ++j )
			( (long *)portalvector )[j] |= ( (long *)p->portalvis )[j];
		bit_enable( portalvector, p - portals );
	}

	memset( uncompressed, 0, leafbytes );

	bit_enable( uncompressed, mergedleafnum );
	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector( portalvector, uncompressed );

//	if ( uncompressed[leafnum >> 3] & ( 1 << ( leafnum & 7 ) ) )
//		Sys_Warning( "Leaf portals saw into leaf\n" );

//	uncompressed[leafnum >> 3] |= ( 1 << ( leafnum & 7 ) );

	numvis++;       // count the leaf itself

	//Sys_FPrintf( SYS_VRB, "cluster %4i : %4i visible\n", leafnum, numvis );
	++clustersizehistogram[numvis];

	memcpy( bspVisBytes.data() + VIS_HEADER_SIZE + leafnum * leafbytes, uncompressed, leafbytes );
}

/*
   ==================
   CalcPortalVis
   ==================
 */
static void CalcPortalVis(){
#ifdef MREDEBUG
	Sys_Printf( "%6d portals out of %d", 0, numportals * 2 );
	//get rid of the counter
	RunThreadsOnIndividual( numportals * 2, false, PortalFlow );
#else
	RunThreadsOnIndividual( numportals * 2, true, PortalFlow );
#endif

}

/*
   ==================
   CalcPassageVis
   ==================
 */
static void CalcPassageVis(){
	PassageMemory();

#ifdef MREDEBUG
	_printf( "%6d portals out of %d", 0, numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, false, CreatePassages );
	_printf( "\n" );
	_printf( "%6d portals out of %d", 0, numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, false, PassageFlow );
	_printf( "\n" );
#else
	Sys_Printf( "\n--- CreatePassages (%d) ---\n", numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, true, CreatePassages );

	Sys_Printf( "\n--- PassageFlow (%d) ---\n", numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, true, PassageFlow );
#endif
}

/*
   ==================
   CalcPassagePortalVis
   ==================
 */
static void CalcPassagePortalVis(){
	PassageMemory();

#ifdef MREDEBUG
	Sys_Printf( "%6d portals out of %d", 0, numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, false, CreatePassages );
	Sys_Printf( "\n" );
	Sys_Printf( "%6d portals out of %d", 0, numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, false, PassagePortalFlow );
	Sys_Printf( "\n" );
#else
	Sys_Printf( "\n--- CreatePassages (%d) ---\n", numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, true, CreatePassages );

	Sys_Printf( "\n--- PassagePortalFlow (%d) ---\n", numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, true, PassagePortalFlow );
#endif
}

/*
   ==================
   CalcFastVis
   ==================
 */
static void CalcFastVis(){
	// fastvis just uses mightsee for a very loose bound
	for ( vportal_t& p : Span( portals, numportals * 2 ) )
	{
		p.portalvis = p.portalflood;
		p.status = EVStatus::Done;
	}
}

/*
   ==================
   CalcVis
   ==================
 */
static void CalcVis(){
	int i, minvis, maxvis;
	double mu, sigma, totalvis, totalvis2;


	/* ydnar: rr2do2's farplane code */
	const char *value;
	if( entities[ 0 ].read_keyvalue( value, "_farplanedist",         /* proper '_' prefixed key */
	                                        "fogclip",               /* wolf compatibility */
	                                        "distancecull" ) ){      /* sof2 compatibility */
		farPlaneDist = atof( value );
		farPlaneDistMode = value[strlen( value ) - 1 ];
		if ( farPlaneDist != 0.0f ) {
			Sys_Printf( "farplane distance = %.1f\n", farPlaneDist );
			if ( farPlaneDistMode == 'o' )
				Sys_Printf( "farplane Origin2Origin mode on\n" );
			else if ( farPlaneDistMode == 'r' )
				Sys_Printf( "farplane Radius+Radius mode on\n" );
			else if ( farPlaneDistMode == 'e' )
				Sys_Printf( "farplane Exact distance mode on\n" );
		}
	}

	Sys_Printf( "\n--- BasePortalVis (%d) ---\n", numportals * 2 );
	RunThreadsOnIndividual( numportals * 2, true, BasePortalVis );

//	RunThreadsOnIndividual( numportals * 2, true, BetterPortalVis );

	SortPortals();

	if ( fastvis ) {
		CalcFastVis();
	}
	else if ( noPassageVis ) {
		CalcPortalVis();
	}
	else if ( passageVisOnly ) {
		CalcPassageVis();
	}
	else {
		CalcPassagePortalVis();
	}
	//
	// assemble the leaf vis lists by oring and compressing the portal lists
	//
	Sys_Printf( "creating leaf vis...\n" );
	for ( i = 0; i < portalclusters; ++i )
		ClusterMerge( i );

	totalvis = 0;
	totalvis2 = 0;
	minvis = -1;
	maxvis = -1;
	for ( i = 0; i < MAX_MAP_LEAFS; ++i )
		if ( clustersizehistogram[i] ) {
			if ( debugCluster ) {
				Sys_FPrintf( SYS_VRB, "%4i clusters have exactly %4i visible clusters\n", clustersizehistogram[i], i );
			}
			/* cast is to prevent integer overflow */
			totalvis  += ( (double) i )                * ( (double) clustersizehistogram[i] );
			totalvis2 += ( (double) i ) * ( (double) i ) * ( (double) clustersizehistogram[i] );

			if ( minvis < 0 ) {
				minvis = i;
			}
			maxvis = i;
		}

	mu = totalvis / portalclusters;
	sigma = sqrt( totalvis2 / portalclusters - mu * mu );

	Sys_Printf( "Total clusters: %i\n", portalclusters );
	Sys_Printf( "Total visible clusters: %.0f\n", totalvis );
	Sys_Printf( "Average clusters visible: %.2f (%.3f%%/total)\n", mu, mu / portalclusters * 100.0 );
	Sys_Printf( "  Standard deviation: %.2f (%.3f%%/total, %.3f%%/avg)\n", sigma, sigma / portalclusters * 100.0, sigma / mu * 100.0 );
	Sys_Printf( "  Minimum: %i (%.3f%%/total, %.3f%%/avg)\n", minvis, minvis / (double) portalclusters * 100.0, minvis / mu * 100.0 );
	Sys_Printf( "  Maximum: %i (%.3f%%/total, %.3f%%/avg)\n", maxvis, maxvis / (double) portalclusters * 100.0, maxvis / mu * 100.0 );
}

/*
   ==================
   SetPortalSphere
   ==================
 */
static void SetPortalSphere( vportal_t& p ){
	Vector3 origin( 0 );

	for ( const Vector3& point : Span( p.winding->points, p.winding->numpoints ) )
	{
		origin += point;
	}

	origin /= p.winding->numpoints;

	double bestr = 0;
	for ( const Vector3& point : Span( p.winding->points, p.winding->numpoints ) )
	{
		value_maximize( bestr, vector3_length( point - origin ) );
	}
	p.origin = origin;
	p.radius = bestr;
}

/*
   =============
   Winding_PlanesConcave
   =============
 */
#define WCONVEX_EPSILON     0.2

static bool Winding_PlanesConcave( const fixedWinding_t *w1, const fixedWinding_t *w2,
                                   const Plane3f& plane1, const Plane3f& plane2 ){
	if ( !w1 || !w2 ) {
		return false;
	}

	// check if one of the points of winding 1 is at the front of the plane of winding 2
	for ( const Vector3& point : Span( w1->points, w1->numpoints ) )
	{
		if ( plane3_distance_to_point( plane2, point ) > WCONVEX_EPSILON ) {
			return true;
		}
	}
	// check if one of the points of winding 2 is at the front of the plane of winding 1
	for ( const Vector3& point : Span( w2->points, w2->numpoints ) )
	{
		if ( plane3_distance_to_point( plane1, point ) > WCONVEX_EPSILON ) {
			return true;
		}
	}

	return false;
}

/*
   ============
   TryMergeLeaves
   ============
 */
static bool TryMergeLeaves( int l1num, int l2num ){
	vportal_t *portals[MAX_PORTALS_ON_LEAF];

	for ( const leaf_t *l1 : { &faceleafs[l1num], &leafs[l1num] } )
	{
		for ( const vportal_t *p1 : Span( l1->portals, l1->numportals ) )
		{
			if ( p1->leaf == l2num ) {
				continue;
			}
			for ( const leaf_t *l2 : { &faceleafs[l2num], &leafs[l2num] } )
			{
				for ( const vportal_t *p2 : Span( l2->portals, l2->numportals ) )
				{
					if ( p2->leaf == l1num ) {
						continue;
					}
					//
					if ( Winding_PlanesConcave( p1->winding, p2->winding, p1->plane, p2->plane ) ) {
						return false;
					}
				}
			}
		}
	}
	for ( leaf_t *lfs : { faceleafs, leafs } )
	{
		leaf_t& l1 = lfs[l1num];
		leaf_t& l2 = lfs[l2num];
		int numportals = 0;
		//the leaves can be merged now
		for ( vportal_t *p1 : Span( l1.portals, l1.numportals ) )
		{
			if ( p1->leaf == l2num ) {
				p1->removed = true;
				continue;
			}
			portals[numportals++] = p1;
		}
		for ( vportal_t *p2 : Span( l2.portals, l2.numportals ) )
		{
			if ( p2->leaf == l1num ) {
				p2->removed = true;
				continue;
			}
			portals[numportals++] = p2;
		}
		std::copy_n( portals, numportals, l2.portals );
		l2.numportals = numportals;
		l1.merged = l2num;
	}
	return true;
}

/*
   ============
   UpdatePortals
   ============
 */
static void UpdatePortals(){
	for ( vportal_t& p : Span( portals, numportals * 2 ) )
		if ( !p.removed )
			while ( leafs[p.leaf].merged >= 0 )
				p.leaf = leafs[p.leaf].merged;
}

/*
   ============
   MergeLeaves

   try to merge leaves but don't merge through hint splitters
   ============
 */
static void MergeLeaves(){
	int nummerges, totalnummerges = 0;

	do
	{
		nummerges = 0;
		for ( int i = 0; i < portalclusters; ++i )
		{
			const leaf_t& leaf = leafs[i];
			//if this leaf is merged already

			/* ydnar: vmods: merge all non-hint portals */
			if ( leaf.merged >= 0 && !hint ) {
				continue;
			}


			for ( const vportal_t *p : Span( leaf.portals, leaf.numportals ) )
			{
				//never merge through hint portals
				if ( !p->removed && !p->hint ) {
					if ( TryMergeLeaves( i, p->leaf ) ) {
						UpdatePortals();
						nummerges++;
						break;
					}
				}
			}
		}
		totalnummerges += nummerges;
	} while ( nummerges );
	Sys_Printf( "%6d leaves merged\n", totalnummerges );
}

/*
   ============
   TryMergeWinding
   ============
 */
#define CONTINUOUS_EPSILON  0.005

static fixedWinding_t *TryMergeWinding( fixedWinding_t *f1, fixedWinding_t *f2, const Vector3& planenormal ){
	const Vector3       *p1, *p2, *p3, *p4, *back;
	fixedWinding_t  *newf;
	int i, j, k, l;
	Vector3 normal;
	float dot;
	bool keep1, keep2;


	//
	// find a common edge
	//
	p1 = p2 = NULL; // stop compiler warning
	j = 0;          //

	for ( i = 0; i < f1->numpoints; i++ )
	{
		p1 = &f1->points[i];
		p2 = &f1->points[( i + 1 ) % f1->numpoints];
		for ( j = 0; j < f2->numpoints; j++ )
		{
			p3 = &f2->points[j];
			p4 = &f2->points[( j + 1 ) % f2->numpoints];
			for ( k = 0; k < 3; k++ )
			{
				if ( fabs( ( *p1 )[k] - ( *p4 )[k] ) > 0.1 ) { //EQUAL_EPSILON) //ME
					break;
				}
				if ( fabs( ( *p2 )[k] - ( *p3 )[k] ) > 0.1 ) { //EQUAL_EPSILON) //ME
					break;
				}
			}
			if ( k == 3 ) {
				break;
			}
		}
		if ( j < f2->numpoints ) {
			break;
		}
	}

	if ( i == f1->numpoints ) {
		return NULL;            // no matching edges

	}
	//
	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	//
	back = &f1->points[( i + f1->numpoints - 1 ) % f1->numpoints];
	normal = VectorNormalized( vector3_cross( planenormal, *p1 - *back ) );

	back = &f2->points[( j + 2 ) % f2->numpoints];
	dot = vector3_dot( *back - *p1, normal );
	if ( dot > CONTINUOUS_EPSILON ) {
		return NULL;            // not a convex polygon
	}
	keep1 = ( dot < -CONTINUOUS_EPSILON );

	back = &f1->points[( i + 2 ) % f1->numpoints];
	normal = VectorNormalized( vector3_cross( planenormal, *back - *p2 ) );

	back = &f2->points[( j + f2->numpoints - 1 ) % f2->numpoints];
	dot = vector3_dot( *back - *p2, normal );
	if ( dot > CONTINUOUS_EPSILON ) {
		return NULL;            // not a convex polygon
	}
	keep2 = ( dot < -CONTINUOUS_EPSILON );

	//
	// build the new polygon
	//
	newf = NewFixedWinding( f1->numpoints + f2->numpoints );

	// copy first polygon
	for ( k = ( i + 1 ) % f1->numpoints; k != i; k = ( k + 1 ) % f1->numpoints )
	{
		if ( k == ( i + 1 ) % f1->numpoints && !keep2 ) {
			continue;
		}

		newf->points[newf->numpoints] = f1->points[k];
		newf->numpoints++;
	}

	// copy second polygon
	for ( l = ( j + 1 ) % f2->numpoints; l != j; l = ( l + 1 ) % f2->numpoints )
	{
		if ( l == ( j + 1 ) % f2->numpoints && !keep1 ) {
			continue;
		}
		newf->points[newf->numpoints] = f2->points[l];
		newf->numpoints++;
	}

	return newf;
}

/*
   ============
   MergeLeafPortals
   ============
 */
static void MergeLeafPortals(){
	int i, j, k, nummerges, hintsmerged;
	leaf_t *leaf;
	vportal_t *p1, *p2;
	fixedWinding_t *w;

	nummerges = 0;
	hintsmerged = 0;
	for ( i = 0; i < portalclusters; i++ )
	{
		leaf = &leafs[i];
		if ( leaf->merged >= 0 ) {
			continue;
		}
		for ( j = 0; j < leaf->numportals; j++ )
		{
			p1 = leaf->portals[j];
			if ( p1->removed ) {
				continue;
			}
			for ( k = j + 1; k < leaf->numportals; k++ )
			{
				p2 = leaf->portals[k];
				if ( p2->removed ) {
					continue;
				}
				if ( p1->leaf == p2->leaf ) {
					w = TryMergeWinding( p1->winding, p2->winding, p1->plane.normal() );
					if ( w ) {
						free( p1->winding );    //% FreeWinding( p1->winding );
						p1->winding = w;
						if ( p1->hint && p2->hint ) {
							hintsmerged++;
						}
						p1->hint |= p2->hint;
						SetPortalSphere( *p1 );
						p2->removed = true;
						nummerges++;
						i--;
						break;
					}
				}
			}
			if ( k < leaf->numportals ) {
				break;
			}
		}
	}
	Sys_Printf( "%6d portals merged\n", nummerges );
	Sys_Printf( "%6d hint portals merged\n", hintsmerged );
}


/*
   ============
   WritePortals
   ============
 */
static int CountActivePortals(){
	int num = 0, hints = 0;

	for ( const vportal_t& p : Span( portals, numportals * 2 ) )
	{
		if ( !p.removed ) {
			num++;
			if ( p.hint )
				hints++;
		}
	}
	Sys_Printf( "%6d active portals\n", num );
	Sys_Printf( "%6d hint portals\n", hints );
	return num;
}

/*
   ============
   LoadPortals
   ============
 */
static void LoadPortals( char *name ){
	char magic[80];
	FILE        *f;
	int numpoints, leafnums[2], flags;

	if ( strEqual( name, "-" ) ) {
		f = stdin;
	}
	else
	{
		f = SafeOpenRead( name, "rt" );
	}

	if ( fscanf( f, "%79s\n%i\n%i\n%i\n", magic, &portalclusters, &numportals, &numfaces ) != 4 ) {
		Error( "LoadPortals: failed to read header" );
	}
	if ( !strEqual( magic, PORTALFILE ) ) {
		Error( "LoadPortals: not a portal file" );
	}

	Sys_Printf( "%6i portalclusters\n", portalclusters );
	Sys_Printf( "%6i numportals\n", numportals );
	Sys_Printf( "%6i numfaces\n", numfaces );

	if ( numportals > MAX_PORTALS ) {
		Error( "MAX_PORTALS" );
	}

	// these counts should take advantage of 64 bit systems automatically
	leafbytes = ( ( portalclusters + 63 ) & ~63 ) >> 3;

	portalbytes = ( ( numportals * 2 + 63 ) & ~63 ) >> 3;
	portallongs = portalbytes / sizeof( long );

	// each file portal is split into two memory portals
	portals = safe_calloc( 2 * numportals * sizeof( vportal_t ) );
	leafs = safe_calloc( portalclusters * sizeof( leaf_t ) );

	for ( leaf_t& leaf : Span( leafs, portalclusters ) )
		leaf.merged = -1;

	bspVisBytes.resize( VIS_HEADER_SIZE + portalclusters * leafbytes );

	if ( bspVisBytes.size() > MAX_MAP_VISIBILITY ) {
		Error( "MAX_MAP_VISIBILITY exceeded" );
	}

	( (int *)bspVisBytes.data() )[0] = portalclusters;
	( (int *)bspVisBytes.data() )[1] = leafbytes;

	for ( int i = 0; i < numportals; ++i )
	{
		if ( fscanf( f, "%i %i %i ", &numpoints, &leafnums[0], &leafnums[1] ) != 3 ) {
			Error( "LoadPortals: reading portal %i", i );
		}
		if ( numpoints > MAX_POINTS_ON_WINDING ) {
			Error( "LoadPortals: portal %i has too many points", i );
		}
		if ( leafnums[0] > portalclusters
		  || leafnums[1] > portalclusters ) {
			Error( "LoadPortals: reading portal %i", i );
		}
		if ( fscanf( f, "%i ", &flags ) != 1 ) {
			Error( "LoadPortals: reading flags" );
		}

		fixedWinding_t *w = NewFixedWinding( numpoints );
		w->numpoints = numpoints;

		for ( Vector3& point : Span( w->points, w->numpoints ) )
		{
			if ( fscanf( f, "(%f %f %f ) ",
			             &point[0], &point[1], &point[2] ) != 3 ) {
				Error( "LoadPortals: reading portal %i", i );
			}
		}
		if ( fscanf( f, "\n" ) != 0 ) {
			// silence gcc warning
		}

		// calc plane
		const visPlane_t plane = PlaneFromWinding( w );

		// create forward portal
		{
			vportal_t& p = portals[i * 2];
			p.num = i + 1;
			p.hint = ( ( flags & 1 ) != 0 );
			p.sky = ( ( flags & 2 ) != 0 );
			p.winding = w;
			p.plane = plane3_flipped( plane );
			p.leaf = leafnums[1];
			SetPortalSphere( p );

			leaf_t& l = leafs[leafnums[0]];
			if ( l.numportals == MAX_PORTALS_ON_LEAF ) {
				Error( "Leaf with too many portals" );
			}
			l.portals[l.numportals] = &p;
			l.numportals++;
		}

		// create backwards portal
		{
			vportal_t& p = portals[i * 2 + 1];
			p.num = i + 1;
			p.hint = hint;
			p.winding = NewFixedWinding( w->numpoints );
			p.winding->numpoints = w->numpoints;
			std::reverse_copy( w->points, w->points + w->numpoints, p.winding->points );

			p.plane = plane;
			p.leaf = leafnums[0];
			SetPortalSphere( p );

			leaf_t& l = leafs[leafnums[1]];
			if ( l.numportals == MAX_PORTALS_ON_LEAF ) {
				Error( "Leaf with too many portals" );
			}
			l.portals[l.numportals] = &p;
			l.numportals++;
		}
	}

	faces = safe_calloc( numfaces * sizeof( vportal_t ) );
	faceleafs = safe_calloc( portalclusters * sizeof( leaf_t ) );

	for ( int i = 0; i < numfaces; ++i )
	{
		if ( fscanf( f, "%i %i ", &numpoints, &leafnums[0] ) != 2 ) {
			Error( "LoadPortals: reading portal %i", i );
		}

		fixedWinding_t *w = NewFixedWinding( numpoints );
		w->numpoints = numpoints;

		for ( Vector3& point : Span( w->points, w->numpoints ) )
		{
			if ( fscanf( f, "(%f %f %f ) ",
			             &point[0], &point[1], &point[2] ) != 3 ) {
				Error( "LoadPortals: reading portal %i", i );
			}
		}
		if ( fscanf( f, "\n" ) != 0 ) {
			// silence gcc warning
		}

		vportal_t& p = faces[i];
		p.num = i + 1;
		p.winding = w;
		// normal pointing out of the leaf
		p.plane = plane3_flipped( PlaneFromWinding( w ) );
		p.leaf = -1;
		SetPortalSphere( p );

		leaf_t& l = faceleafs[leafnums[0]];
		l.merged = -1;
		if ( l.numportals == MAX_PORTALS_ON_LEAF ) {
			Error( "Leaf with too many faces" );
		}
		l.portals[l.numportals] = &p;
		l.numportals++;
	}

	fclose( f );
}



/*
   ===========
   VisMain
   ===========
 */
int VisMain( Args& args ){
	char portalfile[1024];


	/* note it */
	Sys_Printf( "--- Vis ---\n" );

	/* process arguments */
	if ( args.empty() ) {
		Error( "usage: vis [-threads #] [-fast] [-v] bspfile" );
	}
	const char *fileName = args.takeBack();
	const auto argsToInject = args.getVector();
	{
		while ( args.takeArg( "-fast" ) ) {
			Sys_Printf( "fastvis = true\n" );
			fastvis = true;
		}
		while ( args.takeArg( "-merge" ) ) {
			Sys_Printf( "merge = true\n" );
			mergevis = true;
		}
		while ( args.takeArg( "-mergeportals" ) ) {
			Sys_Printf( "mergeportals = true\n" );
			mergevisportals = true;
		}
		while ( args.takeArg( "-nopassage" ) ) {
			Sys_Printf( "nopassage = true\n" );
			noPassageVis = true;
		}
		while ( args.takeArg( "-passageOnly" ) ) {
			Sys_Printf( "passageOnly = true\n" );
			passageVisOnly = true;
		}
		while ( args.takeArg( "-nosort" ) ) {
			Sys_Printf( "nosort = true\n" );
			nosort = true;
		}
		while ( args.takeArg( "-saveprt" ) ) {
			Sys_Printf( "saveprt = true\n" );
			saveprt = true;
		}
		while ( args.takeArg( "-v" ) ) {
			debugCluster = true;
			Sys_Printf( "Extra verbose mode enabled\n" );
		}
		/* ydnar: -hint to merge all but hint portals */
		while ( args.takeArg( "-hint" ) ) {
			Sys_Printf( "hint = true\n" );
			hint = true;
			mergevis = true;
		}

		while( !args.empty() )
		{
			Sys_Warning( "Unknown option \"%s\"\n", args.takeFront() );
		}
	}


	/* load the bsp */
	strcpy( source, ExpandArg( fileName ) );
	path_set_extension( source, ".bsp" );
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );

	/* load the portal file */
	strcpy( portalfile, ExpandArg( fileName ) );
	path_set_extension( portalfile, ".prt" );
	Sys_Printf( "Loading %s\n", portalfile );
	LoadPortals( portalfile );

	/* ydnar: exit if no portals, hence no vis */
	if ( numportals == 0 ) {
		Sys_Printf( "No portals means no vis, exiting.\n" );
		return 0;
	}

	/* ydnar: for getting far plane */
	ParseEntities();

	/* inject command line parameters */
	InjectCommandLine( "-vis", argsToInject );
	UnparseEntities();

	if ( mergevis ) {
		MergeLeaves();
	}

	if ( mergevis || mergevisportals ) {
		MergeLeafPortals();
	}

	CountActivePortals();

	Sys_Printf( "visdatasize:%zu\n", bspVisBytes.size() );

	CalcVis();

	/* delete the prt file */
	if ( !saveprt ) {
		remove( portalfile );
	}

	/* write the bsp file */
	WriteBSPFile( source );

	return 0;
}

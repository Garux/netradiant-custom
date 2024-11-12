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
#include "tjunction.h"




struct edgePoint_t
{
	float intercept;
	Vector3 xyz;
};

struct edgeLine_t
{
	Vector3 normal1;
	float dist1;

	Vector3 normal2;
	float dist2;

	Vector3 origin;
	Vector3 dir;

	std::list<edgePoint_t> points;
};

struct originalEdge_t
{
	float length;
	bspDrawVert_t *dv1;
	bspDrawVert_t *dv2;
};

namespace
{
std::vector<originalEdge_t> originalEdges;

std::vector<edgeLine_t> edgeLines;

int c_degenerateEdges;
int c_addedVerts;
int c_totalVerts;

int c_natural, c_rotate, c_cant;
int c_broken;
}

// these should be whatever epsilon we actually expect,
// plus SNAP_INT_TO_FLOAT
#define LINE_POSITION_EPSILON   0.25f
#define POINT_ON_LINE_EPSILON   0.25

/*
   ====================
   InsertPointOnEdge
   ====================
 */
static void InsertPointOnEdge( const Vector3 &v, edgeLine_t& e ) {
	const edgePoint_t p = { .intercept = vector3_dot( v - e.origin, e.dir ), .xyz = v };

	for ( auto it = e.points.cbegin(); it != e.points.cend(); ++it ) {
		if ( float_equal_epsilon( p.intercept, it->intercept, LINE_POSITION_EPSILON ) ) {
			return;     // the point is already set
		}

		if ( p.intercept < it->intercept ) {
			// insert here
			e.points.insert( it, p );
			return;
		}
	}

	// add at the end if empty list or greatest new point
	e.points.push_back( p );
}


/*
   ====================
   AddEdge
   ====================
 */
static int AddEdge( bspDrawVert_t& dv1, bspDrawVert_t& dv2, bool createNonAxial ) {
	const Vector3& v1 = dv1.xyz;
	const Vector3& v2 = dv2.xyz;

	Vector3 dir = v2 - v1;
	const float d = VectorNormalize( dir );
	if ( d < 0.1 ) {
		// if we added a 0 length vector, it would make degenerate planes
		c_degenerateEdges++;
		return -1;
	}

	if ( !createNonAxial ) {
		if ( fabs( dir[0] + dir[1] + dir[2] ) != 1.0 ) {
			originalEdges.push_back( originalEdge_t{ .length = d, .dv1 = &dv1, .dv2 = &dv2 } );
			return -1;
		}
	}

	for ( edgeLine_t& e : edgeLines ) {
		if ( !float_equal_epsilon( vector3_dot( v1, e.normal1 ), e.dist1, POINT_ON_LINE_EPSILON )
		  || !float_equal_epsilon( vector3_dot( v1, e.normal2 ), e.dist2, POINT_ON_LINE_EPSILON )
		  || !float_equal_epsilon( vector3_dot( v2, e.normal1 ), e.dist1, POINT_ON_LINE_EPSILON )
		  || !float_equal_epsilon( vector3_dot( v2, e.normal2 ), e.dist2, POINT_ON_LINE_EPSILON ) ) {
			continue;
		}

		// this is the edge
		InsertPointOnEdge( v1, e );
		InsertPointOnEdge( v2, e );
		return &e - &edgeLines[0];
	}

	// create a new edge
	edgeLine_t& e = edgeLines.emplace_back();

	e.origin = v1;
	e.dir = dir;

	MakeNormalVectors( e.dir, e.normal1, e.normal2 );
	e.dist1 = vector3_dot( e.origin, e.normal1 );
	e.dist2 = vector3_dot( e.origin, e.normal2 );

	InsertPointOnEdge( v1, e );
	InsertPointOnEdge( v2, e );

	return edgeLines.size() - 1;
}



/*
   AddSurfaceEdges()
   adds a surface's edges
 */

static void AddSurfaceEdges( mapDrawSurface_t& ds ){
	for ( int i = 0; i < ds.numVerts; i++ )
	{
		/* save the edge number in the lightmap field so we don't need to look it up again */
		bspDrawVert_edge_index_write( ds.verts[ i ], AddEdge( ds.verts[ i ], ds.verts[ ( i + 1 ) % ds.numVerts ], false ) );
	}
}



/*
   ColinearEdge()
   determines if an edge is colinear
 */

static bool ColinearEdge( const Vector3& v1, const Vector3& v2, const Vector3& v3 ){
	const Vector3 midpoint = v2 - v1;
	Vector3 dir = v3 - v1;
	if ( VectorNormalize( dir ) == 0 ) {
		return false;  // degenerate
	}

	const float d = vector3_dot( midpoint, dir );
	const Vector3 on = dir * d;
	const Vector3 offset = midpoint - on;

	return vector3_length( offset ) < 0.1;
}



/*
   ====================
   AddPatchEdges

   Add colinear border edges, which will fix some classes of patch to
   brush tjunctions
   ====================
 */
static void AddPatchEdges( mapDrawSurface_t& ds ) {
	for ( int i = 0; i < ds.patchWidth - 2; i += 2 ) {
		{
			bspDrawVert_t& v1 = ds.verts[ i + 0 ];
			bspDrawVert_t& v2 = ds.verts[ i + 1 ];
			bspDrawVert_t& v3 = ds.verts[ i + 2 ];

			// if v2 is the midpoint of v1 to v3, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
		{
			bspDrawVert_t& v1 = ds.verts[ ( ds.patchHeight - 1 ) * ds.patchWidth + i + 0 ];
			bspDrawVert_t& v2 = ds.verts[ ( ds.patchHeight - 1 ) * ds.patchWidth + i + 1 ];
			bspDrawVert_t& v3 = ds.verts[ ( ds.patchHeight - 1 ) * ds.patchWidth + i + 2 ];

			// if v2 is on the v1 to v3 line, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
	}

	for ( int i = 0; i < ds.patchHeight - 2; i += 2 ) {
		{
			bspDrawVert_t& v1 = ds.verts[ ( i + 0 ) * ds.patchWidth ];
			bspDrawVert_t& v2 = ds.verts[ ( i + 1 ) * ds.patchWidth ];
			bspDrawVert_t& v3 = ds.verts[ ( i + 2 ) * ds.patchWidth ];

			// if v2 is the midpoint of v1 to v3, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
		{
			bspDrawVert_t& v1 = ds.verts[ ( ds.patchWidth - 1 ) + ( i + 0 ) * ds.patchWidth ];
			bspDrawVert_t& v2 = ds.verts[ ( ds.patchWidth - 1 ) + ( i + 1 ) * ds.patchWidth ];
			bspDrawVert_t& v3 = ds.verts[ ( ds.patchWidth - 1 ) + ( i + 2 ) * ds.patchWidth ];

			// if v2 is the midpoint of v1 to v3, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
	}
}


/*
   ====================
   FixSurfaceJunctions
   ====================
 */
#define MAX_SURFACE_VERTS   256
static void FixSurfaceJunctions( mapDrawSurface_t& ds ) {
	int counts[MAX_SURFACE_VERTS];
	int originals[MAX_SURFACE_VERTS];
	bspDrawVert_t verts[MAX_SURFACE_VERTS];
	int numVerts = 0;


	for ( int i = 0; i < ds.numVerts; ++i )
	{
		counts[i] = 0;

		// copy first vert
		if ( numVerts == MAX_SURFACE_VERTS ) {
			Error( "MAX_SURFACE_VERTS" );
		}
		verts[numVerts] = ds.verts[i];
		originals[numVerts] = i;
		numVerts++;

		// check to see if there are any t junctions before the next vert
		const bspDrawVert_t& v1 = ds.verts[i];
		const bspDrawVert_t& v2 = ds.verts[ ( i + 1 ) % ds.numVerts ];

		const int j = bspDrawVert_edge_index_read( ds.verts[ i ] );
		if ( j == -1 ) {
			continue;       // degenerate edge
		}
		const edgeLine_t& e = edgeLines[ j ];

		const float start = vector3_dot( v1.xyz - e.origin, e.dir );

		const float end = vector3_dot( v2.xyz - e.origin, e.dir );

		const auto insert_this_point = [&]( const edgePoint_t& p ){
			// insert this point
			if ( numVerts == MAX_SURFACE_VERTS ) {
				Error( "MAX_SURFACE_VERTS" );
			}
			bspDrawVert_t& v = verts[ numVerts ];

			/* take the exact intercept point */
			v.xyz = p.xyz;

			/* interpolate the texture coordinates */
			const float frac = ( p.intercept - start ) / ( end - start );
			v.st = v1.st + ( v2.st - v1.st ) * frac;

			/* copy the normal (FIXME: what about nonplanar surfaces? */
			v.normal = v1.normal;

			/* ydnar: interpolate the color */
			for ( int k = 0; k < MAX_LIGHTMAPS; ++k )
			{
				for ( int j = 0; j < 4; ++j )
				{
					const float c = v1.color[ k ][ j ] + frac * ( v2.color[ k ][ j ] - v1.color[ k ][ j ] );
					v.color[ k ][ j ] = color_to_byte( c );
				}
				v.lightmap[ k ] = { 0, 0 }; // do zero init
			}
			v.lightmap[ 0 ] = vector2_mid( v1.lightmap[ 0 ], v2.lightmap[ 0 ] );
			bspDrawVert_mark_tjunc( v );

			/* next... */
			originals[ numVerts ] = i;
			numVerts++;
			counts[ i ]++;
		};

		if( start < end ){
			for( auto p = e.points.cbegin(); p != e.points.cend(); ++p ){
				if( p->intercept > start + ON_EPSILON ){
					if ( p->intercept > end - ON_EPSILON )
						break;
					else
						insert_this_point( *p );
				}
			}
		}
		else{
			for( auto p = e.points.crbegin(); p != e.points.crend(); ++p ){
				if( p->intercept < start - ON_EPSILON ){
					if( p->intercept < end + ON_EPSILON )
						break;
					else
						insert_this_point( *p );
				}
			}
		}
	}

	c_addedVerts += numVerts - ds.numVerts;
	c_totalVerts += numVerts;


	// FIXME: check to see if the entire surface degenerated
	// after snapping

	// rotate the points so that the initial vertex is between
	// two non-subdivided edges
	int i;
	for ( i = 0; i < numVerts; ++i ) {
		if ( originals[ ( i + 1 ) % numVerts ] == originals[ i ] ) {
			continue;
		}
		const int j = ( i + numVerts - 1 ) % numVerts;
		const int k = ( i + numVerts - 2 ) % numVerts;
		if ( originals[ j ] == originals[ k ] ) {
			continue;
		}
		break;
	}

	if ( i == 0 ) {
		// fine the way it is
		c_natural++;

		free( ds.verts );
		ds.numVerts = numVerts;
		ds.verts = safe_malloc( numVerts * sizeof( *ds.verts ) );
		memcpy( ds.verts, verts, numVerts * sizeof( *ds.verts ) );

		return;
	}
	if ( i == numVerts ) {
		// create a vertex in the middle to start the fan
		c_cant++;

/*
		memset ( &verts[numVerts], 0, sizeof( verts[numVerts] ) );
		for ( i = 0; i < numVerts; ++i ) {
			for ( j = 0; j < 10; ++j ) {
				verts[numVerts].xyz[j] += verts[i].xyz[j];
			}
		}
		for ( j = 0; j < 10; ++j ) {
			verts[numVerts].xyz[j] /= numVerts;
		}

		i = numVerts;
		numVerts++;
 */
	}
	else {
		// just rotate the vertexes
		c_rotate++;

	}

	free( ds.verts );
	ds.numVerts = numVerts;
	ds.verts = safe_malloc( numVerts * sizeof( *ds.verts ) );

	for ( int j = 0; j < ds.numVerts; ++j ) {
		ds.verts[j] = verts[ ( j + i ) % ds.numVerts ];
	}
}





/*
   FixBrokenSurface() - ydnar
   removes nearly coincident verts from a planar winding surface
   returns false if the surface is broken
 */

#define DEGENERATE_EPSILON  0.1

static bool FixBrokenSurface( mapDrawSurface_t& ds ){
	/* dummy check */
	if ( ds.type != ESurfaceType::Face ) {
		return false;
	}

	/* check all verts */
	for ( int i = 0; i < ds.numVerts; ++i )
	{
		/* get verts */
		bspDrawVert_t& dv1 = ds.verts[ i ];
		bspDrawVert_t& dv2 = ds.verts[ ( i + 1 ) % ds.numVerts ];
		bspDrawVert_t avg;

		/* degenerate edge? */
		avg.xyz = dv1.xyz - dv2.xyz;
		if ( vector3_length( avg.xyz ) < DEGENERATE_EPSILON ) {
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: Degenerate T-junction edge found, fixing...\n" );

			/* create an average drawvert */
			/* ydnar 2002-01-26: added nearest-integer welding preference */
			avg.xyz = SnapWeldVector( dv1.xyz, dv2.xyz );
			avg.normal = VectorNormalized( dv1.normal + dv2.normal );
			avg.st = vector2_mid( dv1.st, dv2.st );

			/* lightmap st/colors */
			for ( int k = 0; k < MAX_LIGHTMAPS; ++k )
			{
				avg.lightmap[ k ] = { 0, 0 };
				for ( int j = 0; j < 4; ++j )
					avg.color[ k ][ j ] = ( dv1.color[ k ][ j ] + dv2.color[ k ][ j ] ) >> 1;
			}
			avg.lightmap[ 0 ] = vector2_mid( dv1.lightmap[ 0 ], dv2.lightmap[ 0 ] );

			if( bspDrawVert_is_tjunc( dv1 ) && bspDrawVert_is_tjunc( dv2 ) )
				bspDrawVert_mark_tjunc( avg );

			/* ydnar: der... */
			dv1 = avg;

			/* move the remaining verts */
			for ( int k = i + 2; k < ds.numVerts; ++k )
			{
				ds.verts[ k - 1 ] = ds.verts[ k ];
			}
			ds.numVerts--;

			/* after welding, we have to consider the same vertex again, as it now has a new neighbor dv2 */
			--i;

			/* should ds.numVerts have become 0, then i is now -1. In the next iteration, the loop will abort. */
		}
	}

	/* one last check and return */
	return ds.numVerts >= 3;
}






/*
   FixTJunctions
   call after the surface list has been pruned
 */

void FixTJunctions( const entity_t& ent ){
	/* meta mode has its own t-junction code (currently not as good as this code) */
	//%	if( meta )
	//%		return;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- FixTJunctions ---\n" );

	// add all the edges
	// this actually creates axial edges, but it
	// only creates originalEdge_t structures
	// for non-axial edges
	for ( int i = ent.firstDrawSurf; i < numMapDrawSurfs; ++i )
	{
		/* get surface and early out if possible */
		mapDrawSurface_t& ds = mapDrawSurfs[ i ];
		const shaderInfo_t *si = ds.shaderInfo;
		if ( ( si->compileFlags & C_NODRAW ) || si->autosprite || si->notjunc || ds.numVerts == 0 ) {
			continue;
		}

		/* ydnar: gs mods: handle the various types of surfaces */
		switch ( ds.type )
		{
		/* handle brush faces */
		case ESurfaceType::Face:
			AddSurfaceEdges( ds );
			break;

		/* handle patches */
		case ESurfaceType::Patch:
			AddPatchEdges( ds );
			break;

		/* fixme: make triangle surfaces t-junction */
		default:
			break;
		}
	}

	const size_t axialEdgeLines = edgeLines.size();

	// sort the non-axial edges by length
	std::sort( originalEdges.begin(), originalEdges.end(), []( const originalEdge_t& a, const originalEdge_t& b ){
		return a.length < b.length;
	} );

	// add the non-axial edges, longest first
	// this gives the most accurate edge description
	for ( originalEdge_t& e : originalEdges ) { // originalEdges might not change during AddEdge( true )
		bspDrawVert_edge_index_write( *e.dv1, AddEdge( *e.dv1, *e.dv2, true ) );
	}
	originalEdges.clear();

	Sys_FPrintf( SYS_VRB, "%9zu axial edge lines\n", axialEdgeLines );
	Sys_FPrintf( SYS_VRB, "%9zu non-axial edge lines\n", edgeLines.size() - axialEdgeLines );
	Sys_FPrintf( SYS_VRB, "%9d degenerate edges\n", c_degenerateEdges );

	// insert any needed vertexes
	for ( int i = ent.firstDrawSurf; i < numMapDrawSurfs; ++i )
	{
		/* get surface and early out if possible */
		mapDrawSurface_t& ds = mapDrawSurfs[ i ];
		const shaderInfo_t *si = ds.shaderInfo;
		if ( ( si->compileFlags & C_NODRAW ) || si->autosprite || si->notjunc || ds.numVerts == 0 || ds.type != ESurfaceType::Face ) {
			continue;
		}

		/* ydnar: gs mods: handle the various types of surfaces */
		switch ( ds.type )
		{
		/* handle brush faces */
		case ESurfaceType::Face:
			FixSurfaceJunctions( ds );
			if ( !FixBrokenSurface( ds ) ) {
				c_broken++;
				ClearSurface( &ds );
			}
			break;

		/* fixme: t-junction triangle models and patches */
		default:
			break;
		}
	}

	edgeLines.clear();

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d verts added for T-junctions\n", c_addedVerts );
	Sys_FPrintf( SYS_VRB, "%9d total verts\n", c_totalVerts );
	Sys_FPrintf( SYS_VRB, "%9d naturally ordered\n", c_natural );
	Sys_FPrintf( SYS_VRB, "%9d rotated orders\n", c_rotate );
	Sys_FPrintf( SYS_VRB, "%9d can't order\n", c_cant );
	Sys_FPrintf( SYS_VRB, "%9d broken (degenerate) surfaces removed\n", c_broken );
}

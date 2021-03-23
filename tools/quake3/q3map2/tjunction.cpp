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




struct edgePoint_t
{
	float intercept;
	Vector3 xyz;
	struct edgePoint_t  *prev, *next;
};

struct edgeLine_t
{
	Vector3 normal1;
	float dist1;

	Vector3 normal2;
	float dist2;

	Vector3 origin;
	Vector3 dir;

	edgePoint_t *chain;     // unused element of doubly linked list
};

struct originalEdge_t
{
	float length;
	bspDrawVert_t   *dv[2];
};

originalEdge_t  *originalEdges = NULL;
int numOriginalEdges;
int allocatedOriginalEdges = 0;


edgeLine_t      *edgeLines = NULL;
int numEdgeLines;
int allocatedEdgeLines = 0;

int c_degenerateEdges;
int c_addedVerts;
int c_totalVerts;

int c_natural, c_rotate, c_cant;

// these should be whatever epsilon we actually expect,
// plus SNAP_INT_TO_FLOAT
#define LINE_POSITION_EPSILON   0.25f
#define POINT_ON_LINE_EPSILON   0.25

/*
   ====================
   InsertPointOnEdge
   ====================
 */
void InsertPointOnEdge( const Vector3 &v, edgeLine_t *e ) {
	edgePoint_t *p, *scan;

	p = safe_malloc( sizeof( edgePoint_t ) );
	p->intercept = vector3_dot( v - e->origin, e->dir );
	p->xyz = v;

	if ( e->chain->next == e->chain ) {
		e->chain->next = e->chain->prev = p;
		p->next = p->prev = e->chain;
		return;
	}

	scan = e->chain->next;
	for ( ; scan != e->chain; scan = scan->next ) {
		if ( float_equal_epsilon( p->intercept, scan->intercept, LINE_POSITION_EPSILON ) ) {
			free( p );
			return;     // the point is already set
		}

		if ( p->intercept < scan->intercept ) {
			// insert here
			p->prev = scan->prev;
			p->next = scan;
			scan->prev->next = p;
			scan->prev = p;
			return;
		}
	}

	// add at the end
	p->prev = scan->prev;
	p->next = scan;
	scan->prev->next = p;
	scan->prev = p;
}


/*
   ====================
   AddEdge
   ====================
 */
int AddEdge( bspDrawVert_t& dv1, bspDrawVert_t& dv2, bool createNonAxial ) {
	int i;
	edgeLine_t  *e;
	float d;
	Vector3 dir;
	const Vector3& v1 = dv1.xyz;
	const Vector3& v2 = dv2.xyz;

	dir = v2 - v1;
	d = VectorNormalize( dir );
	if ( d < 0.1 ) {
		// if we added a 0 length vector, it would make degenerate planes
		c_degenerateEdges++;
		return -1;
	}

	if ( !createNonAxial ) {
		if ( fabs( dir[0] + dir[1] + dir[2] ) != 1.0 ) {
			AUTOEXPAND_BY_REALLOC( originalEdges, numOriginalEdges, allocatedOriginalEdges, 1024 );
			originalEdges[ numOriginalEdges ].dv[0] = &dv1;
			originalEdges[ numOriginalEdges ].dv[1] = &dv2;
			originalEdges[ numOriginalEdges ].length = d;
			numOriginalEdges++;
			return -1;
		}
	}

	for ( i = 0 ; i < numEdgeLines ; i++ ) {
		e = &edgeLines[i];

		if ( !float_equal_epsilon( vector3_dot( v1, e->normal1 ), e->dist1, POINT_ON_LINE_EPSILON )
		  || !float_equal_epsilon( vector3_dot( v1, e->normal2 ), e->dist2, POINT_ON_LINE_EPSILON )
		  || !float_equal_epsilon( vector3_dot( v2, e->normal1 ), e->dist1, POINT_ON_LINE_EPSILON )
		  || !float_equal_epsilon( vector3_dot( v2, e->normal2 ), e->dist2, POINT_ON_LINE_EPSILON ) ) {
			continue;
		}

		// this is the edge
		InsertPointOnEdge( v1, e );
		InsertPointOnEdge( v2, e );
		return i;
	}

	// create a new edge
	AUTOEXPAND_BY_REALLOC( edgeLines, numEdgeLines, allocatedEdgeLines, 1024 );

	e = &edgeLines[ numEdgeLines ];
	numEdgeLines++;

	e->chain = safe_malloc( sizeof( edgePoint_t ) );
	e->chain->next = e->chain->prev = e->chain;

	e->origin = v1;
	e->dir = dir;

	MakeNormalVectors( e->dir, e->normal1, e->normal2 );
	e->dist1 = vector3_dot( e->origin, e->normal1 );
	e->dist2 = vector3_dot( e->origin, e->normal2 );

	InsertPointOnEdge( v1, e );
	InsertPointOnEdge( v2, e );

	return numEdgeLines - 1;
}



/*
   AddSurfaceEdges()
   adds a surface's edges
 */

void AddSurfaceEdges( mapDrawSurface_t *ds ){
	for ( int i = 0; i < ds->numVerts; i++ )
	{
		/* save the edge number in the lightmap field so we don't need to look it up again */
		ds->verts[i].lightmap[ 0 ][ 0 ] =
		    AddEdge( ds->verts[ i ], ds->verts[ ( i + 1 ) % ds->numVerts ], false );
	}
}



/*
   ColinearEdge()
   determines if an edge is colinear
 */

bool ColinearEdge( const Vector3& v1, const Vector3& v2, const Vector3& v3 ){
	Vector3 midpoint, dir, offset, on;
	float d;

	midpoint = v2 - v1;
	dir = v3 - v1;
	if ( VectorNormalize( dir ) == 0 ) {
		return false;  // degenerate
	}

	d = vector3_dot( midpoint, dir );
	on = dir * d;
	offset = midpoint - on;
	d = vector3_length( offset );

	if ( d < 0.1 ) {
		return true;
	}

	return false;
}



/*
   ====================
   AddPatchEdges

   Add colinear border edges, which will fix some classes of patch to
   brush tjunctions
   ====================
 */
void AddPatchEdges( mapDrawSurface_t *ds ) {
	int i;

	for ( i = 0 ; i < ds->patchWidth - 2; i += 2 ) {
		{
			bspDrawVert_t& v1 = ds->verts[ i ];
			bspDrawVert_t& v2 = ds->verts[ i + 1 ];
			bspDrawVert_t& v3 = ds->verts[ i + 2 ];

			// if v2 is the midpoint of v1 to v3, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
		{
			bspDrawVert_t& v1 = ds->verts[ ( ds->patchHeight - 1 ) * ds->patchWidth + i ];
			bspDrawVert_t& v2 = ds->verts[ ( ds->patchHeight - 1 ) * ds->patchWidth + i + 1 ];
			bspDrawVert_t& v3 = ds->verts[ ( ds->patchHeight - 1 ) * ds->patchWidth + i + 2 ];

			// if v2 is on the v1 to v3 line, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
	}

	for ( i = 0 ; i < ds->patchHeight - 2 ; i += 2 ) {
		{
			bspDrawVert_t& v1 = ds->verts[ i * ds->patchWidth ];
			bspDrawVert_t& v2 = ds->verts[ ( i + 1 ) * ds->patchWidth ];
			bspDrawVert_t& v3 = ds->verts[ ( i + 2 ) * ds->patchWidth ];

			// if v2 is the midpoint of v1 to v3, add an edge from v1 to v3
			if ( ColinearEdge( v1.xyz, v2.xyz, v3.xyz ) ) {
				AddEdge( v1, v3, false );
			}
		}
		{
			bspDrawVert_t& v1 = ds->verts[ ( ds->patchWidth - 1 ) + i * ds->patchWidth ];
			bspDrawVert_t& v2 = ds->verts[ ( ds->patchWidth - 1 ) + ( i + 1 ) * ds->patchWidth ];
			bspDrawVert_t& v3 = ds->verts[ ( ds->patchWidth - 1 ) + ( i + 2 ) * ds->patchWidth ];

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
void FixSurfaceJunctions( mapDrawSurface_t *ds ) {
	int i, j, k;
	edgeLine_t  *e;
	edgePoint_t *p;
	int counts[MAX_SURFACE_VERTS];
	int originals[MAX_SURFACE_VERTS];
	bspDrawVert_t verts[MAX_SURFACE_VERTS], *v1, *v2;
	int numVerts;
	float start, end, c;


	numVerts = 0;
	for ( i = 0 ; i < ds->numVerts ; i++ )
	{
		counts[i] = 0;

		// copy first vert
		if ( numVerts == MAX_SURFACE_VERTS ) {
			Error( "MAX_SURFACE_VERTS" );
		}
		verts[numVerts] = ds->verts[i];
		originals[numVerts] = i;
		numVerts++;

		// check to see if there are any t junctions before the next vert
		v1 = &ds->verts[i];
		v2 = &ds->verts[ ( i + 1 ) % ds->numVerts ];

		j = (int)ds->verts[i].lightmap[ 0 ][ 0 ];
		if ( j == -1 ) {
			continue;       // degenerate edge
		}
		e = &edgeLines[ j ];

		start = vector3_dot( v1->xyz - e->origin, e->dir );

		end = vector3_dot( v2->xyz - e->origin, e->dir );


		if ( start < end ) {
			p = e->chain->next;
		}
		else {
			p = e->chain->prev;
		}

		for (  ; p != e->chain ; ) {
			if ( start < end ) {
				if ( p->intercept > end - ON_EPSILON ) {
					break;
				}
			}
			else {
				if ( p->intercept < end + ON_EPSILON ) {
					break;
				}
			}

			if ( ( start < end && p->intercept > start + ON_EPSILON ) ||
			     ( start > end && p->intercept < start - ON_EPSILON ) ) {
				// insert this point
				if ( numVerts == MAX_SURFACE_VERTS ) {
					Error( "MAX_SURFACE_VERTS" );
				}

				/* take the exact intercept point */
				verts[ numVerts ].xyz = p->xyz;

				/* interpolate the texture coordinates */
				const float frac = ( p->intercept - start ) / ( end - start );
				verts[ numVerts ].st = v1->st + ( v2->st - v1->st ) * frac;

				/* copy the normal (FIXME: what about nonplanar surfaces? */
				verts[ numVerts ].normal = v1->normal;

				/* ydnar: interpolate the color */
				for ( k = 0; k < MAX_LIGHTMAPS; k++ )
				{
					for ( j = 0; j < 4; j++ )
					{
						c = (float) v1->color[ k ][ j ] + frac * ( (float) v2->color[ k ][ j ] - (float) v1->color[ k ][ j ] );
						verts[ numVerts ].color[ k ][ j ] = color_to_byte( c );
					}
				}

				/* next... */
				originals[ numVerts ] = i;
				numVerts++;
				counts[ i ]++;
			}

			if ( start < end ) {
				p = p->next;
			}
			else {
				p = p->prev;
			}
		}
	}

	c_addedVerts += numVerts - ds->numVerts;
	c_totalVerts += numVerts;


	// FIXME: check to see if the entire surface degenerated
	// after snapping

	// rotate the points so that the initial vertex is between
	// two non-subdivided edges
	for ( i = 0 ; i < numVerts ; i++ ) {
		if ( originals[ ( i + 1 ) % numVerts ] == originals[ i ] ) {
			continue;
		}
		j = ( i + numVerts - 1 ) % numVerts;
		k = ( i + numVerts - 2 ) % numVerts;
		if ( originals[ j ] == originals[ k ] ) {
			continue;
		}
		break;
	}

	if ( i == 0 ) {
		// fine the way it is
		c_natural++;

		ds->numVerts = numVerts;
		ds->verts = safe_malloc( numVerts * sizeof( *ds->verts ) );
		memcpy( ds->verts, verts, numVerts * sizeof( *ds->verts ) );

		return;
	}
	if ( i == numVerts ) {
		// create a vertex in the middle to start the fan
		c_cant++;

/*
		memset ( &verts[numVerts], 0, sizeof( verts[numVerts] ) );
		for ( i = 0 ; i < numVerts ; i++ ) {
			for ( j = 0 ; j < 10 ; j++ ) {
				verts[numVerts].xyz[j] += verts[i].xyz[j];
			}
		}
		for ( j = 0 ; j < 10 ; j++ ) {
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

	ds->numVerts = numVerts;
	ds->verts = safe_malloc( numVerts * sizeof( *ds->verts ) );

	for ( j = 0 ; j < ds->numVerts ; j++ ) {
		ds->verts[j] = verts[ ( j + i ) % ds->numVerts ];
	}
}





/*
   FixBrokenSurface() - ydnar
   removes nearly coincident verts from a planar winding surface
   returns false if the surface is broken
 */

#define DEGENERATE_EPSILON  0.1

int c_broken = 0;

bool FixBrokenSurface( mapDrawSurface_t *ds ){
	bspDrawVert_t   *dv1, *dv2, avg;
	int i, j, k;


	/* dummy check */
	if ( ds == NULL ) {
		return false;
	}
	if ( ds->type != ESurfaceType::Face ) {
		return false;
	}

	/* check all verts */
	for ( i = 0; i < ds->numVerts; i++ )
	{
		/* get verts */
		dv1 = &ds->verts[ i ];
		dv2 = &ds->verts[ ( i + 1 ) % ds->numVerts ];

		/* degenerate edge? */
		avg.xyz = dv1->xyz - dv2->xyz;
		if ( vector3_length( avg.xyz ) < DEGENERATE_EPSILON ) {
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: Degenerate T-junction edge found, fixing...\n" );

			/* create an average drawvert */
			/* ydnar 2002-01-26: added nearest-integer welding preference */
			SnapWeldVector( dv1->xyz, dv2->xyz, avg.xyz );
			avg.normal = VectorNormalized( dv1->normal + dv2->normal );
			avg.st = vector2_mid( dv1->st, dv2->st );

			/* lightmap st/colors */
			for ( k = 0; k < MAX_LIGHTMAPS; k++ )
			{
				avg.lightmap[ k ] = vector2_mid( dv1->lightmap[ k ], dv2->lightmap[ k ] );
				for ( j = 0; j < 4; j++ )
					avg.color[ k ][ j ] = (int) ( dv1->color[ k ][ j ] + dv2->color[ k ][ j ] ) >> 1;
			}

			/* ydnar: der... */
			memcpy( dv1, &avg, sizeof( avg ) );

			/* move the remaining verts */
			for ( k = i + 2; k < ds->numVerts; k++ )
			{
				/* get verts */
				dv1 = &ds->verts[ k ];
				dv2 = &ds->verts[ k - 1 ];

				/* copy */
				memcpy( dv2, dv1, sizeof( bspDrawVert_t ) );
			}
			ds->numVerts--;

			/* after welding, we have to consider the same vertex again, as it now has a new neighbor dv2 */
			--i;

			/* should ds->numVerts have become 0, then i is now -1. In the next iteration, the loop will abort. */
		}
	}

	/* one last check and return */
	return ds->numVerts >= 3;
}






/*
   FixTJunctions
   call after the surface list has been pruned
 */

void FixTJunctions( entity_t *ent ){
	int i;
	mapDrawSurface_t    *ds;
	shaderInfo_t        *si;
	int axialEdgeLines;
	originalEdge_t      *e;
	bspDrawVert_t   *dv;

	/* meta mode has its own t-junction code (currently not as good as this code) */
	//%	if( meta )
	//%		return;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- FixTJunctions ---\n" );
	numEdgeLines = 0;
	numOriginalEdges = 0;

	// add all the edges
	// this actually creates axial edges, but it
	// only creates originalEdge_t structures
	// for non-axial edges
	for ( i = ent->firstDrawSurf ; i < numMapDrawSurfs ; i++ )
	{
		/* get surface and early out if possible */
		ds = &mapDrawSurfs[ i ];
		si = ds->shaderInfo;
		if ( ( si->compileFlags & C_NODRAW ) || si->autosprite || si->notjunc || ds->numVerts == 0 ) {
			continue;
		}

		/* ydnar: gs mods: handle the various types of surfaces */
		switch ( ds->type )
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

	axialEdgeLines = numEdgeLines;

	// sort the non-axial edges by length
	std::sort( originalEdges, originalEdges + numOriginalEdges, []( const originalEdge_t& a, const originalEdge_t& b ){
		return a.length < b.length;
	} );

	// add the non-axial edges, longest first
	// this gives the most accurate edge description
	for ( i = 0 ; i < numOriginalEdges ; i++ ) {
		e = &originalEdges[i];
		dv = e->dv[0]; // e might change during AddEdge
		dv->lightmap[ 0 ][ 0 ] = AddEdge( *e->dv[ 0 ], *e->dv[ 1 ], true );
	}

	Sys_FPrintf( SYS_VRB, "%9d axial edge lines\n", axialEdgeLines );
	Sys_FPrintf( SYS_VRB, "%9d non-axial edge lines\n", numEdgeLines - axialEdgeLines );
	Sys_FPrintf( SYS_VRB, "%9d degenerate edges\n", c_degenerateEdges );

	// insert any needed vertexes
	for ( i = ent->firstDrawSurf; i < numMapDrawSurfs ; i++ )
	{
		/* get surface and early out if possible */
		ds = &mapDrawSurfs[ i ];
		si = ds->shaderInfo;
		if ( ( si->compileFlags & C_NODRAW ) || si->autosprite || si->notjunc || ds->numVerts == 0 || ds->type != ESurfaceType::Face ) {
			continue;
		}

		/* ydnar: gs mods: handle the various types of surfaces */
		switch ( ds->type )
		{
		/* handle brush faces */
		case ESurfaceType::Face:
			FixSurfaceJunctions( ds );
			if ( !FixBrokenSurface( ds ) ) {
				c_broken++;
				ClearSurface( ds );
			}
			break;

		/* fixme: t-junction triangle models and patches */
		default:
			break;
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d verts added for T-junctions\n", c_addedVerts );
	Sys_FPrintf( SYS_VRB, "%9d total verts\n", c_totalVerts );
	Sys_FPrintf( SYS_VRB, "%9d naturally ordered\n", c_natural );
	Sys_FPrintf( SYS_VRB, "%9d rotated orders\n", c_rotate );
	Sys_FPrintf( SYS_VRB, "%9d can't order\n", c_cant );
	Sys_FPrintf( SYS_VRB, "%9d broken (degenerate) surfaces removed\n", c_broken );
}

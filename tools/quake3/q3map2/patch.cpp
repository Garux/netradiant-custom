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



/*
   ExpandLongestCurve() - ydnar
   finds length of quadratic curve specified and determines if length is longer than the supplied max
 */

#define APPROX_SUBDIVISION  8

static void ExpandLongestCurve( float *longestCurve, const Vector3& a, const Vector3& b, const Vector3& c ){
	int i;
	float t, len;
	Vector3 ab, bc, ac, pt, last, delta;


	/* calc vectors */
	ab = b - a;
	if ( VectorNormalize( ab ) < 0.125f ) {
		return;
	}
	bc = c - b;
	if ( VectorNormalize( bc ) < 0.125f ) {
		return;
	}
	ac = c - a;
	if ( VectorNormalize( ac ) < 0.125f ) {
		return;
	}

	/* if all 3 vectors are the same direction, then this edge is linear, so we ignore it */
	if ( vector3_dot( ab, bc ) > 0.99f && vector3_dot( ab, ac ) > 0.99f ) {
		return;
	}

	/* recalculate vectors */
	ab = b - a;
	bc = c - b;

	/* determine length */
	last = a;
	for ( i = 0, len = 0.0f, t = 0.0f; i < APPROX_SUBDIVISION; i++, t += ( 1.0f / APPROX_SUBDIVISION ) )
	{
		/* calculate delta */
		delta = ab * ( 1.0f - t ) + bc * t;

		/* add to first point and calculate pt-pt delta */
		pt = a + delta;
		delta = pt - last;

		/* add it to length and store last point */
		len += vector3_length( delta );
		last = pt;
	}

	/* longer? */
	value_maximize( *longestCurve, len );
}



/*
   ExpandMaxIterations() - ydnar
   determines how many iterations a quadratic curve needs to be subdivided with to fit the specified error
 */

static void ExpandMaxIterations( int *maxIterations, int maxError, const Vector3& a, const Vector3& b, const Vector3& c ){
	int i, j;
	Vector3 prev, next, mid;
	int numPoints, iterations;
	Vector3 points[ MAX_EXPANDED_AXIS ];


	/* initial setup */
	numPoints = 3;
	points[ 0 ] = a;
	points[ 1 ] = b;
	points[ 2 ] = c;

	/* subdivide */
	for ( i = 0; i + 2 < numPoints; i += 2 )
	{
		/* check subdivision limit */
		if ( numPoints + 2 >= MAX_EXPANDED_AXIS ) {
			break;
		}

		/* calculate new curve deltas */
		prev = points[ i + 1 ] - points[ i ];
		next = points[ i + 2 ] - points[ i + 1 ];
		mid = ( points[ i ] + points[ i + 1 ] * 2.0f + points[ i + 2 ] ) * 0.25f;

		/* see if this midpoint is off far enough to subdivide */
		if ( vector3_length( points[ i + 1 ] - mid ) < maxError ) {
			continue;
		}

		/* subdivide */
		numPoints += 2;

		/* create new points */
		prev = ( points[ i ] + points[ i + 1 ] ) * 0.5f;
		next = ( points[ i + 1 ] + points[ i + 2 ] ) * 0.5f;
		mid = ( prev + next ) * 0.5f;

		/* push points out */
		for ( j = numPoints - 1; j > i + 3; j-- )
			points[ j ] = points[ j - 2 ];

		/* insert new points */
		points[ i + 1 ] = prev;
		points[ i + 2 ] = mid;
		points[ i + 3 ] = next;

		/* back up and recheck this set again, it may need more subdivision */
		i -= 2;
	}

	/* put the line on the curve */
	for ( i = 1; i < numPoints; i += 2 )
	{
		prev = ( points[ i ] + points[ i + 1 ] ) * 0.5f;
		next = ( points[ i ] + points[ i - 1 ] ) * 0.5f;
		points[ i ] = ( prev + next ) * 0.5f;
	}

	/* eliminate linear sections */
	for ( i = 0; i + 2 < numPoints; i++ )
	{
		/* create vectors */
		Vector3 delta = points[ i + 1 ] - points[ i ];
		const float len = VectorNormalize( delta );
		Vector3 delta2 = points[ i + 2 ] - points[ i + 1 ];
		const float len2 = VectorNormalize( delta2 );

		/* if either edge is degenerate, then eliminate it */
		if ( len < 0.0625f || len2 < 0.0625f || vector3_dot( delta, delta2 ) >= 1.0f ) {
			for ( j = i + 1; j + 1 < numPoints; j++ )
				points[ j ] = points[ j + 1 ];
			numPoints--;
			continue;
		}
	}

	/* the number of iterations is 2^(points - 1) - 1 */
	numPoints >>= 1;
	iterations = 0;
	while ( numPoints > 1 )
	{
		numPoints >>= 1;
		iterations++;
	}

	/* more? */
	value_maximize( *maxIterations, iterations );
}



/*
   ParsePatch()
   creates a mapDrawSurface_t from the patch text
 */

void ParsePatch( bool onlyLights ){
	float info[ 5 ];
	int i, j, k;
	parseMesh_t     *pm;
	mesh_t m;
	bspDrawVert_t   *verts;
	bool degenerate;
	float longestCurve;
	int maxIterations;

	MatchToken( "{" );

	/* get shader name */
	GetToken( true );
	const auto shader = String64()( "textures/", token );

	Parse1DMatrix( 5, info );
	m.width = info[0];
	m.height = info[1];
	m.verts = verts = safe_malloc( m.width * m.height * sizeof( m.verts[0] ) );

	if ( m.width < 0 || m.width > MAX_PATCH_SIZE || m.height < 0 || m.height > MAX_PATCH_SIZE ) {
		Error( "ParsePatch: bad size" );
	}

	MatchToken( "(" );
	for ( j = 0; j < m.width ; j++ )
	{
		MatchToken( "(" );
		for ( i = 0; i < m.height ; i++ )
		{
			Parse1DMatrix( 5, verts[ i * m.width + j ].xyz.data() );

			/* ydnar: fix colors */
			for ( k = 0; k < MAX_LIGHTMAPS; k++ )
			{
				verts[ i * m.width + j ].color[ k ].set( 255 );
			}
		}
		MatchToken( ")" );
	}
	MatchToken( ")" );

	// if brush primitives format, we may have some epairs to ignore here
	GetToken( true );
	if ( !strEqual( token, "}" ) && ( g_brushType == EBrushType::Bp || g_brushType == EBrushType::Undefined ) ) {
		std::list<epair_t> dummy;
		ParseEPair( dummy );
	}
	else{
		UnGetToken();
	}

	MatchToken( "}" );
	MatchToken( "}" );

	/* short circuit */
	if ( noCurveBrushes || onlyLights ) {
		return;
	}


	/* ydnar: delete and warn about degenerate patches */
	j = ( m.width * m.height );
	Vector4 delta( 0, 0, 0, 0 );
	degenerate = true;

	/* find first valid vector */
	for ( i = 1; i < j && delta[ 3 ] == 0; i++ )
	{
		delta.vec3() = m.verts[ 0 ].xyz - m.verts[ i ].xyz;
		delta[ 3 ] = VectorNormalize( delta.vec3() );
	}

	/* secondary degenerate test */
	if ( delta[ 3 ] == 0 ) {
		degenerate = true;
	}
	else
	{
		/* if all vectors match this or are zero, then this is a degenerate patch */
		for ( i = 1; i < j && degenerate; i++ )
		{
			Vector4 delta2( m.verts[ 0 ].xyz - m.verts[ i ].xyz, 0 );
			delta2[ 3 ] = VectorNormalize( delta2.vec3() );
			if ( delta2[ 3 ] != 0 ) {
				/* create inverse vector */
				Vector4 delta3( delta2 );
				vector3_negate( delta3.vec3() );

				/* compare */
				if ( !VectorCompare( delta.vec3(), delta2.vec3() ) && !VectorCompare( delta.vec3(), delta3.vec3() ) ) {
					degenerate = false;
				}
			}
		}
	}

	/* warn and select degenerate patch */
	if ( degenerate ) {
		xml_Select( "degenerate patch", mapEnt->mapEntityNum, entitySourceBrushes, false );
		free( m.verts );
		return;
	}

	/* find longest curve on the mesh */
	longestCurve = 0.0f;
	maxIterations = 0;
	for ( j = 0; j + 2 < m.width; j += 2 )
	{
		for ( i = 0; i + 2 < m.height; i += 2 )
		{
			ExpandLongestCurve( &longestCurve, verts[ i * m.width + j ].xyz, verts[ i * m.width + ( j + 1 ) ].xyz, verts[ i * m.width + ( j + 2 ) ].xyz );      /* row */
			ExpandLongestCurve( &longestCurve, verts[ i * m.width + j ].xyz, verts[ ( i + 1 ) * m.width + j ].xyz, verts[ ( i + 2 ) * m.width + j ].xyz );      /* col */
			ExpandMaxIterations( &maxIterations, patchSubdivisions, verts[ i * m.width + j ].xyz, verts[ i * m.width + ( j + 1 ) ].xyz, verts[ i * m.width + ( j + 2 ) ].xyz );     /* row */
			ExpandMaxIterations( &maxIterations, patchSubdivisions, verts[ i * m.width + j ].xyz, verts[ ( i + 1 ) * m.width + j ].xyz, verts[ ( i + 2 ) * m.width + j ].xyz  );    /* col */
		}
	}

	/* allocate patch mesh */
	pm = safe_calloc( sizeof( *pm ) );

	/* ydnar: add entity/brush numbering */
	pm->entityNum = mapEnt->mapEntityNum;
	pm->brushNum = entitySourceBrushes;

	/* set shader */
	pm->shaderInfo = ShaderInfoForShader( shader );

	/* set mesh */
	pm->mesh = m;

	/* set longest curve */
	pm->longestCurve = longestCurve;
	pm->maxIterations = maxIterations;

	/* link to the entity */
	pm->next = mapEnt->patches;
	mapEnt->patches = pm;
}



/*
   GrowGroup_r()
   recursively adds patches to a lod group
 */

static void GrowGroup_r( parseMesh_t *pm, int patchNum, int patchCount, parseMesh_t **meshes, byte *bordering, byte *group ){
	int i;
	const byte  *row;


	/* early out check */
	if ( group[ patchNum ] ) {
		return;
	}


	/* set it */
	group[ patchNum ] = 1;
	row = bordering + patchNum * patchCount;

	/* check maximums */
	value_maximize( pm->longestCurve, meshes[ patchNum ]->longestCurve );
	value_maximize( pm->maxIterations, meshes[ patchNum ]->maxIterations );

	/* walk other patches */
	for ( i = 0; i < patchCount; i++ )
	{
		if ( row[ i ] ) {
			GrowGroup_r( pm, i, patchCount, meshes, bordering, group );
		}
	}
}


/*
   PatchMapDrawSurfs()
   any patches that share an edge need to choose their
   level of detail as a unit, otherwise the edges would
   pull apart.
 */

void PatchMapDrawSurfs( entity_t *e ){
	int i, j, k, l, c1, c2;
	parseMesh_t             *pm;
	parseMesh_t             *check, *scan;
	mapDrawSurface_t        *ds;
	int patchCount, groupCount;
	bspDrawVert_t           *v1, *v2;
	byte                    *bordering;

	parseMesh_t  *meshes[ MAX_MAP_DRAW_SURFS ];
	bool grouped[ MAX_MAP_DRAW_SURFS ];
	byte group[ MAX_MAP_DRAW_SURFS ];


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- PatchMapDrawSurfs ---\n" );

	patchCount = 0;
	for ( pm = e->patches ; pm ; pm = pm->next  ) {
		meshes[patchCount] = pm;
		patchCount++;
	}

	if ( !patchCount ) {
		return;
	}
	bordering = safe_calloc( patchCount * patchCount );

	// build the bordering matrix
	for ( k = 0 ; k < patchCount ; k++ ) {
		bordering[k * patchCount + k] = 1;

		for ( l = k + 1 ; l < patchCount ; l++ ) {
			check = meshes[k];
			scan = meshes[l];
			c1 = scan->mesh.width * scan->mesh.height;
			v1 = scan->mesh.verts;

			for ( i = 0 ; i < c1 ; i++, v1++ ) {
				c2 = check->mesh.width * check->mesh.height;
				v2 = check->mesh.verts;
				for ( j = 0 ; j < c2 ; j++, v2++ ) {
					if ( vector3_equal_epsilon( v1->xyz, v2->xyz, 1.f ) ) {
						break;
					}
				}
				if ( j != c2 ) {
					break;
				}
			}
			if ( i != c1 ) {
				// we have a connection
				bordering[k * patchCount + l] =
				    bordering[l * patchCount + k] = 1;
			}
			else {
				// no connection
				bordering[k * patchCount + l] =
				    bordering[l * patchCount + k] = 0;
			}

		}
	}

	/* build groups */
	memset( grouped, 0, patchCount );
	groupCount = 0;
	for ( i = 0; i < patchCount; i++ )
	{
		/* get patch */
		scan = meshes[ i ];

		/* start a new group */
		if ( !grouped[ i ] ) {
			groupCount++;
		}

		/* recursively find all patches that belong in the same group */
		memset( group, 0, patchCount );
		GrowGroup_r( scan, i, patchCount, meshes, bordering, group );

		/* bound them */
		MinMax bounds;
		for ( j = 0; j < patchCount; j++ )
		{
			if ( group[ j ] ) {
				grouped[ j ] = true;
				check = meshes[ j ];
				c1 = check->mesh.width * check->mesh.height;
				v1 = check->mesh.verts;
				for ( k = 0; k < c1; k++, v1++ )
					bounds.extend( v1->xyz );
			}
		}

		/* debug code */
		//%	Sys_Printf( "Longest curve: %f Iterations: %d\n", scan->longestCurve, scan->maxIterations );

		/* create drawsurf */
		scan->grouped = true;
		ds = DrawSurfaceForMesh( e, scan, NULL );   /* ydnar */
		ds->bounds = bounds;
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d patches\n", patchCount );
	Sys_FPrintf( SYS_VRB, "%9d patch LOD groups\n", groupCount );
}

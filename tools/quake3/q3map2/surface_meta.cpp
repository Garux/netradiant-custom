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



#define LIGHTMAP_EXCEEDED   -1
#define S_EXCEEDED          -2
#define T_EXCEEDED          -3
#define ST_EXCEEDED         -4
#define UNSUITABLE_TRIANGLE -10
#define VERTS_EXCEEDED      -1000
#define INDEXES_EXCEEDED    -2000

#define GROW_META_VERTS     1024
#define GROW_META_TRIANGLES 1024

static int numMetaSurfaces, numPatchMetaSurfaces;

static int maxMetaVerts = 0;
static int numMetaVerts = 0;
static int firstSearchMetaVert = 0;
static bspDrawVert_t        *metaVerts = NULL;

static int maxMetaTriangles = 0;
static int numMetaTriangles = 0;
static metaTriangle_t       *metaTriangles = NULL;



/*
   ClearMetaVertexes()
   called before staring a new entity to clear out the triangle list
 */

void ClearMetaTriangles( void ){
	numMetaVerts = 0;
	numMetaTriangles = 0;
}



/*
   FindMetaVertex()
   finds a matching metavertex in the global list, returning its index
 */

static int FindMetaVertex( bspDrawVert_t *src ){
	int i;
	bspDrawVert_t   *v;

	/* try to find an existing drawvert */
	for ( i = firstSearchMetaVert, v = &metaVerts[ i ]; i < numMetaVerts; i++, v++ )
	{
		if ( memcmp( src, v, sizeof( bspDrawVert_t ) ) == 0 ) {
			return i;
		}
	}

	/* enough space? */
	AUTOEXPAND_BY_REALLOC_ADD( metaVerts, numMetaVerts, maxMetaVerts, GROW_META_VERTS );

	/* add the triangle */
	memcpy( &metaVerts[ numMetaVerts ], src, sizeof( bspDrawVert_t ) );
	numMetaVerts++;

	/* return the count */
	return ( numMetaVerts - 1 );
}



/*
   AddMetaTriangle()
   adds a new meta triangle, allocating more memory if necessary
 */

static int AddMetaTriangle( void ){
	/* enough space? */
	AUTOEXPAND_BY_REALLOC_ADD( metaTriangles, numMetaTriangles, maxMetaTriangles, GROW_META_TRIANGLES );

	/* increment and return */
	numMetaTriangles++;
	return numMetaTriangles - 1;
}



/*
   FindMetaTriangle()
   finds a matching metatriangle in the global list,
   otherwise adds it and returns the index to the metatriangle
 */

int FindMetaTriangle( metaTriangle_t *src, bspDrawVert_t *a, bspDrawVert_t *b, bspDrawVert_t *c, int planeNum ){
	int triIndex;

	/* detect degenerate triangles fixme: do something proper here */
	if ( vector3_length( a->xyz - b->xyz ) < 0.125f
	  || vector3_length( b->xyz - c->xyz ) < 0.125f
	  || vector3_length( c->xyz - a->xyz ) < 0.125f ) {
		return -1;
	}

	/* find plane */
	if ( planeNum >= 0 ) {
		/* because of precision issues with small triangles, try to use the specified plane */
		src->planeNum = planeNum;
		src->plane = mapplanes[ planeNum ].plane;
	}
	else
	{
		/* calculate a plane from the triangle's points (and bail if a plane can't be constructed) */
		src->planeNum = -1;
		if ( !PlaneFromPoints( src->plane, a->xyz, b->xyz, c->xyz ) ) {
			return -1;
		}
	}

	/* ydnar 2002-10-03: repair any bogus normals (busted ase import kludge) */
	if ( vector3_length( a->normal ) == 0.0f ) {
		a->normal = src->plane.normal();
	}
	if ( vector3_length( b->normal ) == 0.0f ) {
		b->normal = src->plane.normal();
	}
	if ( vector3_length( c->normal ) == 0.0f ) {
		c->normal = src->plane.normal();
	}

	/* ydnar 2002-10-04: set lightmap axis if not already set */
	if ( !( src->si->compileFlags & C_VERTEXLIT ) &&
	     src->lightmapAxis == g_vector3_identity ) {
		/* the shader can specify an explicit lightmap axis */
		if ( src->si->lightmapAxis != g_vector3_identity ) {
			src->lightmapAxis = src->si->lightmapAxis;
		}

		/* new axis-finding code */
		else{
			src->lightmapAxis = CalcLightmapAxis( src->plane.normal() );
		}
	}

	/* fill out the src triangle */
	src->indexes[ 0 ] = FindMetaVertex( a );
	src->indexes[ 1 ] = FindMetaVertex( b );
	src->indexes[ 2 ] = FindMetaVertex( c );

	/* try to find an existing triangle */
	#ifdef USE_EXHAUSTIVE_SEARCH
	{
		int i;
		metaTriangle_t  *tri;


		for ( i = 0, tri = metaTriangles; i < numMetaTriangles; i++, tri++ )
		{
			if ( memcmp( src, tri, sizeof( metaTriangle_t ) ) == 0 ) {
				return i;
			}
		}
	}
	#endif

	/* get a new triangle */
	triIndex = AddMetaTriangle();

	/* add the triangle */
	memcpy( &metaTriangles[ triIndex ], src, sizeof( metaTriangle_t ) );

	/* return the triangle index */
	return triIndex;
}



/*
   SurfaceToMetaTriangles()
   converts a classified surface to metatriangles
 */

static void SurfaceToMetaTriangles( mapDrawSurface_t *ds ){
	int i;
	metaTriangle_t src;
	bspDrawVert_t a, b, c;


	/* only handle certain types of surfaces */
	if ( ds->type != ESurfaceType::Face &&
	     ds->type != ESurfaceType::Meta &&
	     ds->type != ESurfaceType::ForcedMeta &&
	     ds->type != ESurfaceType::Decal ) {
		return;
	}

	/* speed at the expense of memory */
	firstSearchMetaVert = numMetaVerts;

	/* only handle valid surfaces */
	if ( ds->type != ESurfaceType::Bad && ds->numVerts >= 3 && ds->numIndexes >= 3 ) {
		/* walk the indexes and create triangles */
		for ( i = 0; i < ds->numIndexes; i += 3 )
		{
			/* sanity check the indexes */
			if ( ds->indexes[ i ] == ds->indexes[ i + 1 ] ||
			     ds->indexes[ i ] == ds->indexes[ i + 2 ] ||
			     ds->indexes[ i + 1 ] == ds->indexes[ i + 2 ] ) {
				//%	Sys_Printf( "%d! ", ds->numVerts );
				continue;
			}

			/* build a metatriangle */
			src.si = ds->shaderInfo;
			src.side = ( ds->sideRef != NULL ? ds->sideRef->side : NULL );
			src.entityNum = ds->entityNum;
			src.surfaceNum = ds->surfaceNum;
			src.planeNum = ds->planeNum;
			src.castShadows = ds->castShadows;
			src.recvShadows = ds->recvShadows;
			src.fogNum = ds->fogNum;
			src.sampleSize = ds->sampleSize;
			src.shadeAngleDegrees = ds->shadeAngleDegrees;
			src.lightmapAxis = ds->lightmapAxis;

			/* copy drawverts */
			memcpy( &a, &ds->verts[ ds->indexes[ i ] ], sizeof( a ) );
			memcpy( &b, &ds->verts[ ds->indexes[ i + 1 ] ], sizeof( b ) );
			memcpy( &c, &ds->verts[ ds->indexes[ i + 2 ] ], sizeof( c ) );
			FindMetaTriangle( &src, &a, &b, &c, ds->planeNum );
		}

		/* add to count */
		numMetaSurfaces++;
	}

	/* clear the surface (free verts and indexes, sets it to ESurfaceType::Bad) */
	ClearSurface( ds );
}



/*
   TriangulatePatchSurface()
   creates triangles from a patch
 */

void TriangulatePatchSurface( entity_t *e, mapDrawSurface_t *ds ){
	int x, y, pw[ 5 ], r;
	mapDrawSurface_t    *dsNew;
	mesh_t src, *subdivided, *mesh;

	/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
	const bool forcePatchMeta = e->boolForKey( "_patchMeta", "patchMeta" );

	/* try to early out */
	if ( ds->numVerts == 0 || ds->type != ESurfaceType::Patch || ( !patchMeta && !forcePatchMeta ) ) {
		return;
	}
	/* make a mesh from the drawsurf */
	src.width = ds->patchWidth;
	src.height = ds->patchHeight;
	src.verts = ds->verts;
	//%	subdivided = SubdivideMesh( src, 8, 999 );

	int iterations;
	int patchSubdivision;
	if ( e->read_keyvalue( patchSubdivision, "_patchSubdivide", "patchSubdivide" ) ) {
		iterations = IterationsForCurve( ds->longestCurve, patchSubdivision );
	}
	else{
		const int patchQuality = e->intForKey( "_patchQuality", "patchQuality" );
		iterations = IterationsForCurve( ds->longestCurve, patchSubdivisions / ( patchQuality == 0? 1 : patchQuality ) );
	}

	subdivided = SubdivideMesh2( src, iterations ); //%	ds->maxIterations

	/* fit it to the curve and remove colinear verts on rows/columns */
	PutMeshOnCurve( *subdivided );
	mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );
	//% MakeMeshNormals( mesh );

	/* make a copy of the drawsurface */
	dsNew = AllocDrawSurface( ESurfaceType::Meta );
	memcpy( dsNew, ds, sizeof( *ds ) );

	/* if the patch is nonsolid, then discard it */
	if ( !( ds->shaderInfo->compileFlags & C_SOLID ) ) {
		ClearSurface( ds );
	}

	/* set new pointer */
	ds = dsNew;

	/* basic transmogrification */
	ds->type = ESurfaceType::Meta;
	ds->numIndexes = 0;
	ds->indexes = safe_malloc( mesh->width * mesh->height * 6 * sizeof( int ) );

	/* copy the verts in */
	ds->numVerts = ( mesh->width * mesh->height );
	ds->verts = mesh->verts;

	/* iterate through the mesh quads */
	for ( y = 0; y < ( mesh->height - 1 ); y++ )
	{
		for ( x = 0; x < ( mesh->width - 1 ); x++ )
		{
			/* set indexes */
			pw[ 0 ] = x + ( y * mesh->width );
			pw[ 1 ] = x + ( ( y + 1 ) * mesh->width );
			pw[ 2 ] = x + 1 + ( ( y + 1 ) * mesh->width );
			pw[ 3 ] = x + 1 + ( y * mesh->width );
			pw[ 4 ] = x + ( y * mesh->width );    /* same as pw[ 0 ] */

			/* set radix */
			r = ( x + y ) & 1;

			/* make first triangle */
			ds->indexes[ ds->numIndexes++ ] = pw[ r + 0 ];
			ds->indexes[ ds->numIndexes++ ] = pw[ r + 1 ];
			ds->indexes[ ds->numIndexes++ ] = pw[ r + 2 ];

			/* make second triangle */
			ds->indexes[ ds->numIndexes++ ] = pw[ r + 0 ];
			ds->indexes[ ds->numIndexes++ ] = pw[ r + 2 ];
			ds->indexes[ ds->numIndexes++ ] = pw[ r + 3 ];
		}
	}

	/* free the mesh, but not the verts */
	free( mesh );

	/* add to count */
	numPatchMetaSurfaces++;

	/* classify it */
	ClassifySurfaces( 1, ds );
}

#define TINY_AREA 1.0f
#define MAXAREA_MAXTRIES 8
int MaxAreaIndexes( bspDrawVert_t *vert, int cnt, int *indexes ){
	int r, s, t, bestR = 0, bestS = 1, bestT = 2;
	int i, j;
	double A, bestA = -1, V, bestV = -1;
	bspDrawVert_t *buf;
	double shiftWidth;

	if ( cnt < 3 ) {
		return 0;
	}

	/* calculate total area */
	A = 0;
	for ( i = 1; i + 1 < cnt; ++i )
	{
		A += vector3_length( vector3_cross( vert[i].xyz - vert[0].xyz, vert[i + 1].xyz - vert[0].xyz ) );
	}
	V = 0;
	for ( i = 0; i < cnt; ++i )
	{
		V += vector3_length( vert[( i + 1 ) % cnt].xyz - vert[i].xyz );
	}

	/* calculate shift width from the area sensibly, assuming the polygon
	 * fits about 25% of the screen in both dimensions
	 * we assume 1280x1024
	 * 1 pixel is then about sqrt(A) / (0.25 * screenwidth)
	 * 8 pixels are then about sqrt(A) /  (0.25 * 1280) * 8
	 * 8 pixels are then about sqrt(A) * 0.025
	 * */
	shiftWidth = sqrt( A ) * 0.0125;
	/*     3->1 6->2 12->3 ... */
	if ( A - ceil( log( cnt / 1.5 ) / log( 2 ) ) * V * shiftWidth * 2 < 0 ) {
		/* Sys_FPrintf( SYS_WRN, "Small triangle detected (area %f, circumference %f), adjusting shiftWidth from %f to ", A, V, shiftWidth ); */
		shiftWidth = A / ( ceil( log( cnt / 1.5 ) / log( 2 ) ) * V * 2 );
		/* Sys_FPrintf( SYS_WRN, "%f\n", shiftWidth ); */
	}

	/* find the triangle with highest area */
	for ( r = 0; r + 2 < cnt; ++r )
		for ( s = r + 1; s + 1 < cnt; ++s )
			for ( t = s + 1; t < cnt; ++t )
			{
				const Vector3 ab = vert[s].xyz - vert[r].xyz;
				const Vector3 ac = vert[t].xyz - vert[r].xyz;
				const Vector3 bc = vert[t].xyz - vert[s].xyz;
				A = vector3_length( vector3_cross( ab, ac ) );

				V = A - ( vector3_length( ab ) - vector3_length( ac ) - vector3_length( bc ) ) * shiftWidth;
				/* value = A - circumference * shiftWidth, i.e. we back out by shiftWidth units from each side, to prevent too acute triangles */
				/* this kind of simulates "number of shiftWidth*shiftWidth fragments in the triangle not touched by an edge" */

				if ( bestA < 0 || V > bestV ) {
					bestA = A;
					bestV = V;
					bestR = r;
					bestS = s;
					bestT = t;
				}
			}

	/*
		if(bestV < 0)
			Sys_FPrintf( SYS_WRN, "value was REALLY bad\n" );
	 */

	for ( int trai = 0; trai < MAXAREA_MAXTRIES; ++trai )
	{
		if ( trai ) {
			bestR = rand() % cnt;
			bestS = rand() % cnt;
			bestT = rand() % cnt;
			if ( bestR == bestS || bestR == bestT || bestS == bestT ) {
				continue;
			}
			// bubblesort inline
			// abc acb bac bca cab cba
			if ( bestR > bestS ) {
				j = bestR;
				bestR = bestS;
				bestS = j;
			}
			// abc acb abc bca acb bca
			if ( bestS > bestT ) {
				j = bestS;
				bestS = bestT;
				bestT = j;
			}
			// abc abc abc bac abc bac
			if ( bestR > bestS ) {
				j = bestR;
				bestR = bestS;
				bestS = j;
			}
			// abc abc abc abc abc abc

			bestA = vector3_length( vector3_cross( vert[bestS].xyz - vert[bestR].xyz, vert[bestT].xyz - vert[bestR].xyz ) );
		}

		if ( bestA < TINY_AREA ) {
			/* the biggest triangle is degenerate - then every other is too, and the other algorithms wouldn't generate anything useful either */
			continue;
		}

		i = 0;
		indexes[i++] = bestR;
		indexes[i++] = bestS;
		indexes[i++] = bestT;
		/* uses 3 */

		/* identify the other fragments */

		/* full polygon without triangle (bestR,bestS,bestT) = three new polygons:
		 * 1. bestR..bestS
		 * 2. bestS..bestT
		 * 3. bestT..bestR
		 */

		j = MaxAreaIndexes( vert + bestR, bestS - bestR + 1, indexes + i );
		if ( j < 0 ) {
			continue;
		}
		j += i;
		for (; i < j; ++i )
			indexes[i] += bestR;
		/* uses 3*(bestS-bestR+1)-6 */
		j = MaxAreaIndexes( vert + bestS, bestT - bestS + 1, indexes + i );
		if ( j < 0 ) {
			continue;
		}
		j += i;
		for (; i < j; ++i )
			indexes[i] += bestS;
		/* uses 3*(bestT-bestS+1)-6 */

		/* can'bestT recurse this one directly... therefore, buffering */
		if ( cnt + bestR - bestT + 1 >= 3 ) {
			buf = safe_malloc( sizeof( *vert ) * ( cnt + bestR - bestT + 1 ) );
			memcpy( buf, vert + bestT, sizeof( *vert ) * ( cnt - bestT ) );
			memcpy( buf + ( cnt - bestT ), vert, sizeof( *vert ) * ( bestR + 1 ) );
			j = MaxAreaIndexes( buf, cnt + bestR - bestT + 1, indexes + i );
			if ( j < 0 ) {
				free( buf );
				continue;
			}
			j += i;
			for (; i < j; ++i )
				indexes[i] = ( indexes[i] + bestT ) % cnt;
			/* uses 3*(cnt+bestR-bestT+1)-6 */
			free( buf );
		}

		/* together 3 + 3*(cnt+3) - 18 = 3*cnt-6 q.e.d. */
		return i;
	}

	return -1;
}

/*
   MaxAreaFaceSurface() - divVerent
   creates a triangle list using max area indexes
 */

void MaxAreaFaceSurface( mapDrawSurface_t *ds ){
	int n;
	/* try to early out  */
	if ( !ds->numVerts || ( ds->type != ESurfaceType::Face && ds->type != ESurfaceType::Decal ) ) {
		return;
	}

	/* is this a simple triangle? */
	if ( ds->numVerts == 3 ) {
		ds->numIndexes = 3;
		ds->indexes = safe_malloc( ds->numIndexes * sizeof( int ) );
		ds->indexes[0] = 0;
		ds->indexes[1] = 1;
		ds->indexes[2] = 2;
		numMaxAreaSurfaces++;
		return;
	}

	/* do it! */
	ds->numIndexes = 3 * ds->numVerts - 6;
	ds->indexes = safe_malloc( ds->numIndexes * sizeof( int ) );
	n = MaxAreaIndexes( ds->verts, ds->numVerts, ds->indexes );
	if ( n < 0 ) {
		/* whatever we do, it's degenerate */
		free( ds->indexes );
		ds->numIndexes = 0;
		StripFaceSurface( ds );
		return;
	}
	ds->numIndexes = n;

	/* add to count */
	numMaxAreaSurfaces++;

	/* classify it */
	ClassifySurfaces( 1, ds );
}


/*
   FanFaceSurface() - ydnar
   creates a tri-fan from a brush face winding
   loosely based on SurfaceAsTriFan()
 */

void FanFaceSurface( mapDrawSurface_t *ds ){
	int i, k, a, b, c;
	Color4f color[ MAX_LIGHTMAPS ];
	for ( k = 0; k < MAX_LIGHTMAPS; k++ )
		color[k].set( 0 );
	bspDrawVert_t   *verts, *centroid, *dv;
	double iv;


	/* try to early out */
	if ( !ds->numVerts || ( ds->type != ESurfaceType::Face && ds->type != ESurfaceType::Decal ) ) {
		return;
	}

	/* add a new vertex at the beginning of the surface */
	verts = safe_malloc( ( ds->numVerts + 1 ) * sizeof( bspDrawVert_t ) );
	memset( verts, 0, sizeof( bspDrawVert_t ) );
	memcpy( &verts[ 1 ], ds->verts, ds->numVerts * sizeof( bspDrawVert_t ) );
	free( ds->verts );
	ds->verts = verts;

	/* add up the drawverts to create a centroid */
	centroid = &verts[ 0 ];
	for ( i = 1, dv = &verts[ 1 ]; i < ( ds->numVerts + 1 ); i++, dv++ )
	{
		centroid->xyz += dv->xyz;
		centroid->normal += dv->normal;
		centroid->st += dv->st;
		for ( k = 0; k < MAX_LIGHTMAPS; k++ ){
			color[ k ] += dv->color[ k ];
			centroid->lightmap[ k ] += dv->lightmap[ k ];
		}
	}

	/* average the centroid */
	iv = 1.0f / ds->numVerts;
	centroid->xyz *= iv;
	if ( VectorNormalize( centroid->normal ) == 0 ) {
		centroid->normal = verts[ 1 ].normal;
	}
	centroid->st *= iv;
	for ( k = 0; k < MAX_LIGHTMAPS; k++ ){
		centroid->lightmap[ k ] *= iv;
		centroid->color[ k ] = color_to_byte( color[ k ] / ds->numVerts );
	}

	/* add to vert count */
	ds->numVerts++;

	/* fill indexes in triangle fan order */
	ds->numIndexes = 0;
	ds->indexes = safe_malloc( ds->numVerts * 3 * sizeof( int ) );
	for ( i = 1; i < ds->numVerts; i++ )
	{
		a = 0;
		b = i;
		c = ( i + 1 ) % ds->numVerts;
		c = c ? c : 1;
		ds->indexes[ ds->numIndexes++ ] = a;
		ds->indexes[ ds->numIndexes++ ] = b;
		ds->indexes[ ds->numIndexes++ ] = c;
	}

	/* add to count */
	numFanSurfaces++;

	/* classify it */
	ClassifySurfaces( 1, ds );
}



/*
   StripFaceSurface() - ydnar
   attempts to create a valid tri-strip w/o degenerate triangles from a brush face winding
   based on SurfaceAsTriStrip()
 */

#define MAX_INDEXES     1024

void StripFaceSurface( mapDrawSurface_t *ds ){
	int i, r, least, rotate, numIndexes, ni, a, b, c, indexes[ MAX_INDEXES ];


	/* try to early out  */
	if ( !ds->numVerts || ( ds->type != ESurfaceType::Face && ds->type != ESurfaceType::Decal ) ) {
		return;
	}

	/* is this a simple triangle? */
	if ( ds->numVerts == 3 ) {
		numIndexes = 3;
		indexes[0] = 0;
		indexes[1] = 1;
		indexes[2] = 2;
	}
	else
	{
		/* ydnar: find smallest coordinate */
		least = 0;
		if ( ds->shaderInfo != NULL && !ds->shaderInfo->autosprite ) {
			for ( i = 0; i < ds->numVerts; i++ )
			{
				/* get points */
				const Vector3& v1 = ds->verts[ i ].xyz;
				const Vector3& v2 = ds->verts[ least ].xyz;

				/* compare */
				if ( v1[ 0 ] < v2[ 0 ] ||
				     ( v1[ 0 ] == v2[ 0 ] && v1[ 1 ] < v2[ 1 ] ) ||
				     ( v1[ 0 ] == v2[ 0 ] && v1[ 1 ] == v2[ 1 ] && v1[ 2 ] < v2[ 2 ] ) ) {
					least = i;
				}
			}
		}

		/* determine the triangle strip order */
		numIndexes = ( ds->numVerts - 2 ) * 3;
		if ( numIndexes > MAX_INDEXES ) {
			Error( "MAX_INDEXES exceeded for surface (%d > %d) (%d verts)", numIndexes, MAX_INDEXES, ds->numVerts );
		}

		/* try all possible orderings of the points looking for a non-degenerate strip order */
		ni = 0;
		for ( r = 0; r < ds->numVerts; r++ )
		{
			/* set rotation */
			rotate = ( r + least ) % ds->numVerts;

			/* walk the winding in both directions */
			for ( ni = 0, i = 0; i < ds->numVerts - 2 - i; i++ )
			{
				/* make indexes */
				a = ( ds->numVerts - 1 - i + rotate ) % ds->numVerts;
				b = ( i + rotate ) % ds->numVerts;
				c = ( ds->numVerts - 2 - i + rotate ) % ds->numVerts;

				/* test this triangle */
				if ( ds->numVerts > 4 && IsTriangleDegenerate( ds->verts, a, b, c ) ) {
					break;
				}
				indexes[ ni++ ] = a;
				indexes[ ni++ ] = b;
				indexes[ ni++ ] = c;

				/* handle end case */
				if ( i + 1 != ds->numVerts - 1 - i ) {
					/* make indexes */
					a = ( ds->numVerts - 2 - i + rotate ) % ds->numVerts;
					b = ( i + rotate ) % ds->numVerts;
					c = ( i + 1 + rotate ) % ds->numVerts;

					/* test triangle */
					if ( ds->numVerts > 4 && IsTriangleDegenerate( ds->verts, a, b, c ) ) {
						break;
					}
					indexes[ ni++ ] = a;
					indexes[ ni++ ] = b;
					indexes[ ni++ ] = c;
				}
			}

			/* valid strip? */
			if ( ni == numIndexes ) {
				break;
			}
		}

		/* if any triangle in the strip is degenerate, render from a centered fan point instead */
		if ( ni < numIndexes ) {
			FanFaceSurface( ds );
			return;
		}
	}

	/* copy strip triangle indexes */
	ds->numIndexes = numIndexes;
	ds->indexes = safe_malloc( ds->numIndexes * sizeof( int ) );
	memcpy( ds->indexes, indexes, ds->numIndexes * sizeof( int ) );

	/* add to count */
	numStripSurfaces++;

	/* classify it */
	ClassifySurfaces( 1, ds );
}


/*
   EmitMetaStatictics
   vortex: prints meta statistics in general output
 */

void EmitMetaStats(){
	Sys_Printf( "--- EmitMetaStats ---\n" );
	Sys_Printf( "%9d total meta surfaces\n", numMetaSurfaces );
	Sys_Printf( "%9d stripped surfaces\n", numStripSurfaces );
	Sys_Printf( "%9d fanned surfaces\n", numFanSurfaces );
	Sys_Printf( "%9d maxarea'd surfaces\n", numMaxAreaSurfaces );
	Sys_Printf( "%9d patch meta surfaces\n", numPatchMetaSurfaces );
	Sys_Printf( "%9d meta verts\n", numMetaVerts );
	Sys_Printf( "%9d meta triangles\n", numMetaTriangles );
}

/*
   MakeEntityMetaTriangles()
   builds meta triangles from brush faces (tristrips and fans)
 */

void MakeEntityMetaTriangles( entity_t *e ){
	int i, f, fOld, start;
	mapDrawSurface_t    *ds;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- MakeEntityMetaTriangles ---\n" );

	/* init pacifier */
	fOld = -1;
	start = I_FloatTime();

	/* walk the list of surfaces in the entity */
	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ )
	{
		/* print pacifier */
		f = 10 * ( i - e->firstDrawSurf ) / ( numMapDrawSurfs - e->firstDrawSurf );
		if ( f != fOld ) {
			fOld = f;
			Sys_FPrintf( SYS_VRB, "%d...", f );
		}

		/* get surface */
		ds = &mapDrawSurfs[ i ];
		if ( ds->numVerts <= 0 ) {
			continue;
		}

		/* ignore autosprite surfaces */
		if ( ds->shaderInfo->autosprite ) {
			continue;
		}

		/* meta this surface? */
		if ( !meta && !ds->shaderInfo->forceMeta ) {
			continue;
		}

		/* switch on type */
		switch ( ds->type )
		{
		case ESurfaceType::Face:
		case ESurfaceType::Decal:
			if ( maxAreaFaceSurface ) {
				MaxAreaFaceSurface( ds );
			}
			else{
				StripFaceSurface( ds );
			}
			SurfaceToMetaTriangles( ds );
			break;

		case ESurfaceType::Patch:
			TriangulatePatchSurface( e, ds );
			break;

		case ESurfaceType::Triangles:
			break;

		case ESurfaceType::ForcedMeta:
		case ESurfaceType::Meta:
			SurfaceToMetaTriangles( ds );
			break;

		default:
			break;
		}
	}

	/* print time */
	if ( ( numMapDrawSurfs - e->firstDrawSurf ) ) {
		Sys_FPrintf( SYS_VRB, " (%d)\n", (int) ( I_FloatTime() - start ) );
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d total meta surfaces\n", numMetaSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d stripped surfaces\n", numStripSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d fanned surfaces\n", numFanSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d maxarea'd surfaces\n", numMaxAreaSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d patch meta surfaces\n", numPatchMetaSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d meta verts\n", numMetaVerts );
	Sys_FPrintf( SYS_VRB, "%9d meta triangles\n", numMetaTriangles );

	/* tidy things up */
	TidyEntitySurfaces( e );
}



/*
   CreateEdge()
   sets up an edge structure from a plane and 2 points that the edge ab falls lies in
 */

struct edge_t
{
	Vector3 origin;
	Plane3f edge;
	float length, kingpinLength;
	int kingpin;
	Plane3f plane;
};

void CreateEdge( const Plane3f& plane, const Vector3& a, const Vector3& b, edge_t *edge ){
	/* copy edge origin */
	edge->origin = a;

	/* create vector aligned with winding direction of edge */
	edge->edge.normal() = b - a;

	edge->kingpin = vector3_max_abs_component_index( edge->edge.normal() );
	edge->kingpinLength = edge->edge.normal()[ edge->kingpin ];

	VectorNormalize( edge->edge.normal() );
	edge->edge.dist() = vector3_dot( a, edge->edge.normal() );
	edge->length = plane3_distance_to_point( edge->edge, b );

	/* create perpendicular plane that edge lies in */
	edge->plane.normal() = vector3_cross( plane.normal(), edge->edge.normal() );
	edge->plane.dist() = vector3_dot( a, edge->plane.normal() );
}



/*
   FixMetaTJunctions()
   fixes t-junctions on meta triangles
 */

#define TJ_PLANE_EPSILON    ( 1.0f / 8.0f )
#define TJ_EDGE_EPSILON     ( 1.0f / 8.0f )
#define TJ_POINT_EPSILON    ( 1.0f / 8.0f )

void FixMetaTJunctions( void ){
	int i, j, k, f, fOld, start, vertIndex, triIndex, numTJuncs;
	metaTriangle_t  *tri, *newTri;
	shaderInfo_t    *si;
	bspDrawVert_t   *a, *b, *c, junc;
	float amount;
	Plane3f plane;
	edge_t edges[ 3 ];


	/* this code is crap; revisit later */
	return;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- FixMetaTJunctions ---\n" );

	/* init pacifier */
	fOld = -1;
	start = I_FloatTime();

	/* walk triangle list */
	numTJuncs = 0;
	for ( i = 0; i < numMetaTriangles; i++ )
	{
		/* get triangle */
		tri = &metaTriangles[ i ];

		/* print pacifier */
		f = 10 * i / numMetaTriangles;
		if ( f != fOld ) {
			fOld = f;
			Sys_FPrintf( SYS_VRB, "%d...", f );
		}

		/* attempt to early out */
		si = tri->si;
		if ( ( si->compileFlags & C_NODRAW ) || si->autosprite || si->notjunc ) {
			continue;
		}

		/* calculate planes */
		plane = tri->plane;
		CreateEdge( plane, metaVerts[ tri->indexes[ 0 ] ].xyz, metaVerts[ tri->indexes[ 1 ] ].xyz, &edges[ 0 ] );
		CreateEdge( plane, metaVerts[ tri->indexes[ 1 ] ].xyz, metaVerts[ tri->indexes[ 2 ] ].xyz, &edges[ 1 ] );
		CreateEdge( plane, metaVerts[ tri->indexes[ 2 ] ].xyz, metaVerts[ tri->indexes[ 0 ] ].xyz, &edges[ 2 ] );

		/* walk meta vert list */
		for ( j = 0; j < numMetaVerts; j++ )
		{
			/* get vert */
			const Vector3 pt = metaVerts[ j ].xyz;

			/* determine if point lies in the triangle's plane */
			if ( fabs( plane3_distance_to_point( plane, pt ) ) > TJ_PLANE_EPSILON ) {
				continue;
			}

			/* skip this point if it already exists in the triangle */
			for ( k = 0; k < 3; k++ )
			{
				if ( vector3_equal_epsilon( pt, metaVerts[ tri->indexes[ k ] ].xyz, TJ_POINT_EPSILON ) ) {
					break;
				}
			}
			if ( k < 3 ) {
				continue;
			}

			/* walk edges */
			for ( k = 0; k < 3; k++ )
			{
				/* ignore bogus edges */
				if ( fabs( edges[ k ].kingpinLength ) < TJ_EDGE_EPSILON ) {
					continue;
				}

				/* determine if point lies on the edge */
				if ( fabs( plane3_distance_to_point( edges[ k ].plane, pt ) ) > TJ_EDGE_EPSILON ) {
					continue;
				}

				/* determine how far along the edge the point lies */
				amount = ( pt[ edges[ k ].kingpin ] - edges[ k ].origin[ edges[ k ].kingpin ] ) / edges[ k ].kingpinLength;
				if ( amount <= 0.0f || amount >= 1.0f ) {
					continue;
				}

				#if 0
				dist = plane3_distance_to_point( edges[ k ].edge, pt );
				if ( dist <= -0.0f || dist >= edges[ k ].length ) {
					continue;
				}
				amount = dist / edges[ k ].length;
				#endif

				/* the edge opposite the zero-weighted vertex was hit, so use that as an amount */
				a = &metaVerts[ tri->indexes[ k % 3 ] ];
				b = &metaVerts[ tri->indexes[ ( k + 1 ) % 3 ] ];
				c = &metaVerts[ tri->indexes[ ( k + 2 ) % 3 ] ];

				/* make new vert */
				LerpDrawVertAmount( a, b, amount, &junc );
				junc.xyz = pt;

				/* compare against existing verts */
				if ( VectorCompare( junc.xyz, a->xyz ) || VectorCompare( junc.xyz, b->xyz ) || VectorCompare( junc.xyz, c->xyz ) ) {
					continue;
				}

				/* see if we can just re-use the existing vert */
				if ( !memcmp( &metaVerts[ j ], &junc, sizeof( junc ) ) ) {
					vertIndex = j;
				}
				else
				{
					/* find new vertex (note: a and b are invalid pointers after this) */
					firstSearchMetaVert = numMetaVerts;
					vertIndex = FindMetaVertex( &junc );
					if ( vertIndex < 0 ) {
						continue;
					}
				}

				/* make new triangle */
				triIndex = AddMetaTriangle();
				if ( triIndex < 0 ) {
					continue;
				}

				/* get triangles */
				tri = &metaTriangles[ i ];
				newTri = &metaTriangles[ triIndex ];

				/* copy the triangle */
				memcpy( newTri, tri, sizeof( *tri ) );

				/* fix verts */
				tri->indexes[ ( k + 1 ) % 3 ] = vertIndex;
				newTri->indexes[ k ] = vertIndex;

				/* recalculate edges */
				CreateEdge( plane, metaVerts[ tri->indexes[ 0 ] ].xyz, metaVerts[ tri->indexes[ 1 ] ].xyz, &edges[ 0 ] );
				CreateEdge( plane, metaVerts[ tri->indexes[ 1 ] ].xyz, metaVerts[ tri->indexes[ 2 ] ].xyz, &edges[ 1 ] );
				CreateEdge( plane, metaVerts[ tri->indexes[ 2 ] ].xyz, metaVerts[ tri->indexes[ 0 ] ].xyz, &edges[ 2 ] );

				/* debug code */
				metaVerts[ vertIndex ].color[ 0 ].rgb() = { 255, 204, 0 };

				/* add to counter and end processing of this vert */
				numTJuncs++;
				break;
			}
		}
	}

	/* print time */
	Sys_FPrintf( SYS_VRB, " (%d)\n", (int) ( I_FloatTime() - start ) );

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d T-junctions added\n", numTJuncs );
}



/*
   SmoothMetaTriangles()
   averages coincident vertex normals in the meta triangles
 */

#define MAX_SAMPLES             256
#define THETA_EPSILON           0.000001
#define EQUAL_NORMAL_EPSILON    0.01f

void SmoothMetaTriangles( void ){
	int i, j, k, f, fOld, start, numSmoothed;
	float shadeAngle, defaultShadeAngle, maxShadeAngle;
	metaTriangle_t  *tri;
	int indexes[ MAX_SAMPLES ];
	Vector3 votes[ MAX_SAMPLES ];

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SmoothMetaTriangles ---\n" );

	/* allocate shade angle table */
	std::vector<float> shadeAngles( numMetaVerts, 0 );

	/* allocate smoothed table */
	std::vector<std::uint8_t> smoothed( numMetaVerts, false );

	/* set default shade angle */
	defaultShadeAngle = degrees_to_radians( npDegrees );
	maxShadeAngle = 0.0f;

	/* run through every surface and flag verts belonging to non-lightmapped surfaces
	   and set per-vertex smoothing angle */
	for ( i = 0, tri = &metaTriangles[ i ]; i < numMetaTriangles; i++, tri++ )
	{
		shadeAngle = defaultShadeAngle;

		/* get shade angle from shader */
		if ( tri->si->shadeAngleDegrees > 0.0f ) {
			shadeAngle = degrees_to_radians( tri->si->shadeAngleDegrees );
		}
		/* get shade angle from entity */
		else if ( tri->shadeAngleDegrees > 0.0f ) {
			shadeAngle = degrees_to_radians( tri->shadeAngleDegrees );
		}

		if ( shadeAngle <= 0.0f ) {
			shadeAngle = defaultShadeAngle;
		}

		value_maximize( maxShadeAngle, shadeAngle );

		/* flag its verts */
		for ( j = 0; j < 3; j++ )
		{
			shadeAngles[ tri->indexes[ j ] ] = shadeAngle;
			if ( shadeAngle <= 0 ) {
				smoothed[ tri->indexes[ j ] ] = true;
			}
		}
	}

	/* bail if no surfaces have a shade angle */
	if ( maxShadeAngle <= 0 ) {
		Sys_FPrintf( SYS_VRB, "No smoothing angles specified, aborting\n" );
		return;
	}

	/* init pacifier */
	fOld = -1;
	start = I_FloatTime();

	/* go through the list of vertexes */
	numSmoothed = 0;
	for ( i = 0; i < numMetaVerts; i++ )
	{
		/* print pacifier */
		f = 10 * i / numMetaVerts;
		if ( f != fOld ) {
			fOld = f;
			Sys_FPrintf( SYS_VRB, "%d...", f );
		}

		/* already smoothed? */
		if ( smoothed[ i ] ) {
			continue;
		}

		/* clear */
		Vector3 average( 0 );
		int numVerts = 0;
		int numVotes = 0;

		/* build a table of coincident vertexes */
		for ( j = i; j < numMetaVerts && numVerts < MAX_SAMPLES; j++ )
		{
			/* already smoothed? */
			if ( smoothed[ j ] ) {
				continue;
			}

			/* test vertexes */
			if ( !VectorCompare( metaVerts[ i ].xyz, metaVerts[ j ].xyz ) ) {
				continue;
			}

			/* use smallest shade angle */
			shadeAngle = std::min( shadeAngles[ i ], shadeAngles[ j ] );

			/* check shade angle */
			const double dot = std::clamp( vector3_dot( metaVerts[ i ].normal, metaVerts[ j ].normal ), -1.0, 1.0 );
			if ( acos( dot ) + THETA_EPSILON >= shadeAngle ) {
				continue;
			}

			/* add to the list */
			indexes[ numVerts++ ] = j;

			/* flag vertex */
			smoothed[ j ] = true;

			/* see if this normal has already been voted */
			for ( k = 0; k < numVotes; k++ )
			{
				if ( vector3_equal_epsilon( metaVerts[ j ].normal, votes[ k ], EQUAL_NORMAL_EPSILON ) ) {
					break;
				}
			}

			/* add a new vote? */
			if ( k == numVotes && numVotes < MAX_SAMPLES ) {
				average += metaVerts[ j ].normal;
				votes[ numVotes ] = metaVerts[ j ].normal;
				numVotes++;
			}
		}

		/* don't average for less than 2 verts */
		if ( numVerts < 2 ) {
			continue;
		}

		/* average normal */
		if ( VectorNormalize( average ) != 0 ) {
			/* smooth */
			for ( j = 0; j < numVerts; j++ )
				metaVerts[ indexes[ j ] ].normal = average;
			numSmoothed++;
		}
	}

	/* print time */
	Sys_FPrintf( SYS_VRB, " (%d)\n", (int) ( I_FloatTime() - start ) );

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d smoothed vertexes\n", numSmoothed );
}



/*
   AddMetaVertToSurface()
   adds a drawvert to a surface unless an existing vert matching already exists
   returns the index of that vert (or < 0 on failure)
 */

int AddMetaVertToSurface( mapDrawSurface_t *ds, bspDrawVert_t *dv1, int *coincident ){
	int i;
	bspDrawVert_t   *dv2;


	/* go through the verts and find a suitable candidate */
	for ( i = 0; i < ds->numVerts; i++ )
	{
		/* get test vert */
		dv2 = &ds->verts[ i ];

		/* compare xyz and normal */
		if ( !VectorCompare( dv1->xyz, dv2->xyz ) ) {
			continue;
		}
		if ( !VectorCompare( dv1->normal, dv2->normal ) ) {
			continue;
		}

		/* good enough at this point */
		( *coincident )++;

		/* compare texture coordinates and color */
		if ( dv1->st[ 0 ] != dv2->st[ 0 ] || dv1->st[ 1 ] != dv2->st[ 1 ] ) {
			continue;
		}
		if ( dv1->color[ 0 ].alpha() != dv2->color[ 0 ].alpha() ) {
			continue;
		}

		/* found a winner */
		numMergedVerts++;
		return i;
	}

	/* overflow check */
	if ( ds->numVerts >= ( ( ds->shaderInfo->compileFlags & C_VERTEXLIT ) ? maxSurfaceVerts : maxLMSurfaceVerts ) ) {
		return VERTS_EXCEEDED;
	}

	/* made it this far, add the vert and return */
	dv2 = &ds->verts[ ds->numVerts++ ];
	*dv2 = *dv1;
	return ( ds->numVerts - 1 );
}




/*
   AddMetaTriangleToSurface()
   attempts to add a metatriangle to a surface
   returns the score of the triangle added
 */

#define AXIS_SCORE          100000
#define AXIS_MIN            100000
#define VERT_SCORE          10000
#define SURFACE_SCORE           1000
#define ST_SCORE            50
#define ST_SCORE2           ( 2 * ( ST_SCORE ) )

#define DEFAULT_ADEQUATE_SCORE      ( (AXIS_MIN) +1 * ( VERT_SCORE ) )
#define DEFAULT_GOOD_SCORE      ( (AXIS_MIN) +2 * (VERT_SCORE)                   +4 * ( ST_SCORE ) )
#define         PERFECT_SCORE       ( (AXIS_MIN) +3 * ( VERT_SCORE ) + (SURFACE_SCORE) +4 * ( ST_SCORE ) )

#define ADEQUATE_SCORE          ( metaAdequateScore >= 0 ? metaAdequateScore : DEFAULT_ADEQUATE_SCORE )
#define GOOD_SCORE          ( metaGoodScore     >= 0 ? metaGoodScore     : DEFAULT_GOOD_SCORE )

static int AddMetaTriangleToSurface( mapDrawSurface_t *ds, metaTriangle_t *tri, bool testAdd ){
	int i, score, coincident, ai, bi, ci, oldTexRange[ 2 ];
	float lmMax;
	bool inTexRange;
	mapDrawSurface_t old;


	/* overflow check */
	if ( ds->numIndexes >= maxSurfaceIndexes ) {
		return 0;
	}

	/* test the triangle */
	if ( ds->entityNum != tri->entityNum ) { /* ydnar: added 2002-07-06 */
		return 0;
	}
	if ( ds->castShadows != tri->castShadows || ds->recvShadows != tri->recvShadows ) {
		return 0;
	}
	if ( ds->shaderInfo != tri->si || ds->fogNum != tri->fogNum || ds->sampleSize != tri->sampleSize ) {
		return 0;
	}
	#if 0
	if ( !( ds->shaderInfo->compileFlags & C_VERTEXLIT ) &&
	     //% !VectorCompare( ds->lightmapAxis, tri->lightmapAxis ) )
		 vector3_dot( ds->lightmapAxis, tri->plane.normal() ) < 0.25f ) {
		return 0;
	}
	#endif

	/* planar surfaces will only merge with triangles in the same plane */
	if ( npDegrees == 0.0f && !ds->shaderInfo->nonplanar && ds->planeNum >= 0 ) {
		if ( !VectorCompare( mapplanes[ ds->planeNum ].normal(), tri->plane.normal() ) || mapplanes[ ds->planeNum ].dist() != tri->plane.dist() ) {
			return 0;
		}
		if ( tri->planeNum >= 0 && tri->planeNum != ds->planeNum ) {
			return 0;
		}
	}



	if ( metaMaxBBoxDistance >= 0 && ds->numIndexes > 0 ) {
		if( !ds->minmax.test( metaVerts[ tri->indexes[ 0 ] ].xyz, metaMaxBBoxDistance )
		 && !ds->minmax.test( metaVerts[ tri->indexes[ 1 ] ].xyz, metaMaxBBoxDistance )
		 && !ds->minmax.test( metaVerts[ tri->indexes[ 2 ] ].xyz, metaMaxBBoxDistance ) )
			return 0;
	}

	/* set initial score */
	score = tri->surfaceNum == ds->surfaceNum ? SURFACE_SCORE : 0;

	/* score the the dot product of lightmap axis to plane */
	if ( ( ds->shaderInfo->compileFlags & C_VERTEXLIT ) || VectorCompare( ds->lightmapAxis, tri->lightmapAxis ) ) {
		score += AXIS_SCORE;
	}
	else{
		score += AXIS_SCORE * vector3_dot( ds->lightmapAxis, tri->plane.normal() );
	}

	/* preserve old drawsurface if this fails */
	memcpy( &old, ds, sizeof( *ds ) );

	/* attempt to add the verts */
	coincident = 0;
	ai = AddMetaVertToSurface( ds, &metaVerts[ tri->indexes[ 0 ] ], &coincident );
	bi = AddMetaVertToSurface( ds, &metaVerts[ tri->indexes[ 1 ] ], &coincident );
	ci = AddMetaVertToSurface( ds, &metaVerts[ tri->indexes[ 2 ] ], &coincident );

	/* check vertex underflow */
	if ( ai < 0 || bi < 0 || ci < 0 ) {
		memcpy( ds, &old, sizeof( *ds ) );
		return 0;
	}

	/* score coincident vertex count (2003-02-14: changed so this only matters on planar surfaces) */
	score += ( coincident * VERT_SCORE );

	/* add new vertex bounds to mins/maxs */
	MinMax minmax( ds->minmax );
	minmax.extend( metaVerts[ tri->indexes[ 0 ] ].xyz );
	minmax.extend( metaVerts[ tri->indexes[ 1 ] ].xyz );
	minmax.extend( metaVerts[ tri->indexes[ 2 ] ].xyz );

	/* check lightmap bounds overflow (after at least 1 triangle has been added) */
	if ( !( ds->shaderInfo->compileFlags & C_VERTEXLIT ) &&
	     ds->numIndexes > 0 && vector3_length( ds->lightmapAxis ) != 0.0f &&
	     ( !VectorCompare( ds->minmax.mins, minmax.mins ) || !VectorCompare( ds->minmax.maxs, minmax.maxs ) ) ) {
		/* set maximum size before lightmap scaling (normally 2032 units) */
		/* 2004-02-24: scale lightmap test size by 2 to catch larger brush faces */
		/* 2004-04-11: reverting to actual lightmap size */
		lmMax = ( ds->sampleSize * ( ds->shaderInfo->lmCustomWidth - 1 ) );
		for ( i = 0; i < 3; i++ )
		{
			if ( ( minmax.maxs[ i ] - minmax.mins[ i ] ) > lmMax ) {
				memcpy( ds, &old, sizeof( *ds ) );
				return 0;
			}
		}
	}

	/* check texture range overflow */
	oldTexRange[ 0 ] = ds->texRange[ 0 ];
	oldTexRange[ 1 ] = ds->texRange[ 1 ];
	inTexRange = CalcSurfaceTextureRange( ds );

	if ( !inTexRange && ds->numIndexes > 0 ) {
		memcpy( ds, &old, sizeof( *ds ) );
		return UNSUITABLE_TRIANGLE;
	}

	/* score texture range */
	if ( ds->texRange[ 0 ] <= oldTexRange[ 0 ] ) {
		score += ST_SCORE2;
	}
	else if ( ds->texRange[ 0 ] > oldTexRange[ 0 ] && oldTexRange[ 1 ] > oldTexRange[ 0 ] ) {
		score += ST_SCORE;
	}

	if ( ds->texRange[ 1 ] <= oldTexRange[ 1 ] ) {
		score += ST_SCORE2;
	}
	else if ( ds->texRange[ 1 ] > oldTexRange[ 1 ] && oldTexRange[ 0 ] > oldTexRange[ 1 ] ) {
		score += ST_SCORE;
	}


	/* go through the indexes and try to find an existing triangle that matches abc */
	for ( i = 0; i < ds->numIndexes; i += 3 )
	{
		/* 2002-03-11 (birthday!): rotate the triangle 3x to find an existing triangle */
		if ( ( ai == ds->indexes[ i ] && bi == ds->indexes[ i + 1 ] && ci == ds->indexes[ i + 2 ] ) ||
		     ( bi == ds->indexes[ i ] && ci == ds->indexes[ i + 1 ] && ai == ds->indexes[ i + 2 ] ) ||
		     ( ci == ds->indexes[ i ] && ai == ds->indexes[ i + 1 ] && bi == ds->indexes[ i + 2 ] ) ) {
			/* triangle already present */
			memcpy( ds, &old, sizeof( *ds ) );
			tri->si = NULL;
			return 0;
		}

		/* rotate the triangle 3x to find an inverse triangle (error case) */
		if ( ( ai == ds->indexes[ i ] && bi == ds->indexes[ i + 2 ] && ci == ds->indexes[ i + 1 ] ) ||
		     ( bi == ds->indexes[ i ] && ci == ds->indexes[ i + 2 ] && ai == ds->indexes[ i + 1 ] ) ||
		     ( ci == ds->indexes[ i ] && ai == ds->indexes[ i + 2 ] && bi == ds->indexes[ i + 1 ] ) ) {
			/* warn about it */
			Sys_Warning( "Flipped triangle: (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f)\n",
			             ds->verts[ ai ].xyz[ 0 ], ds->verts[ ai ].xyz[ 1 ], ds->verts[ ai ].xyz[ 2 ],
			             ds->verts[ bi ].xyz[ 0 ], ds->verts[ bi ].xyz[ 1 ], ds->verts[ bi ].xyz[ 2 ],
			             ds->verts[ ci ].xyz[ 0 ], ds->verts[ ci ].xyz[ 1 ], ds->verts[ ci ].xyz[ 2 ] );

			/* reverse triangle already present */
			memcpy( ds, &old, sizeof( *ds ) );
			tri->si = NULL;
			return 0;
		}
	}

	/* add the triangle indexes */
	if ( ds->numIndexes < maxSurfaceIndexes ) {
		ds->indexes[ ds->numIndexes++ ] = ai;
	}
	if ( ds->numIndexes < maxSurfaceIndexes ) {
		ds->indexes[ ds->numIndexes++ ] = bi;
	}
	if ( ds->numIndexes < maxSurfaceIndexes ) {
		ds->indexes[ ds->numIndexes++ ] = ci;
	}

	/* check index overflow */
	if ( ds->numIndexes >= maxSurfaceIndexes  ) {
		memcpy( ds, &old, sizeof( *ds ) );
		return 0;
	}

	/* sanity check the indexes */
	if ( ds->numIndexes >= 3 &&
	     ( ds->indexes[ ds->numIndexes - 3 ] == ds->indexes[ ds->numIndexes - 2 ] ||
	       ds->indexes[ ds->numIndexes - 3 ] == ds->indexes[ ds->numIndexes - 1 ] ||
	       ds->indexes[ ds->numIndexes - 2 ] == ds->indexes[ ds->numIndexes - 1 ] ) ) {
		Sys_Printf( "DEG:%d! ", ds->numVerts );
	}

	/* testing only? */
	if ( testAdd ) {
		memcpy( ds, &old, sizeof( *ds ) );
	}
	else
	{
		/* copy bounds back to surface */
		ds->minmax = minmax;

		/* mark triangle as used */
		tri->si = NULL;
	}

	/* add a side reference */
	ds->sideRef = AllocSideRef( tri->side, ds->sideRef );

	/* return to sender */
	return score;
}



/*
   MetaTrianglesToSurface()
   creates map drawsurface(s) from the list of possibles
 */

static void MetaTrianglesToSurface( int numPossibles, metaTriangle_t *possibles, int *fOld, int *numAdded ){
	int i, j, f, best, score, bestScore;
	metaTriangle_t      *seed, *test;
	mapDrawSurface_t    *ds;
	bspDrawVert_t       *verts;
	int                 *indexes;
	bool added;


	/* allocate arrays */
	verts = safe_malloc( sizeof( *verts ) * maxSurfaceVerts );
	indexes = safe_malloc( sizeof( *indexes ) * maxSurfaceIndexes );

	/* walk the list of triangles */
	for ( i = 0, seed = possibles; i < numPossibles; i++, seed++ )
	{
		/* skip this triangle if it has already been merged */
		if ( seed->si == NULL ) {
			continue;
		}

		/* -----------------------------------------------------------------
		   initial drawsurf construction
		   ----------------------------------------------------------------- */

		/* start a new drawsurface */
		ds = AllocDrawSurface( ESurfaceType::Meta );
		ds->entityNum = seed->entityNum;
		ds->surfaceNum = seed->surfaceNum;
		ds->castShadows = seed->castShadows;
		ds->recvShadows = seed->recvShadows;

		ds->shaderInfo = seed->si;
		ds->planeNum = seed->planeNum;
		ds->fogNum = seed->fogNum;
		ds->sampleSize = seed->sampleSize;
		ds->shadeAngleDegrees = seed->shadeAngleDegrees;
		ds->verts = verts;
		ds->indexes = indexes;
		ds->lightmapAxis = seed->lightmapAxis;
		ds->sideRef = AllocSideRef( seed->side, NULL );

		ds->minmax.clear();

		/* clear verts/indexes */
		memset( verts, 0, sizeof( *verts ) * maxSurfaceVerts );
		memset( indexes, 0, sizeof( *indexes ) * maxSurfaceIndexes );


		/* add the first triangle */
		if ( AddMetaTriangleToSurface( ds, seed, false ) ) {
			( *numAdded )++;
		}

		/* -----------------------------------------------------------------
		   add triangles
		   ----------------------------------------------------------------- */

		/* progressively walk the list until no more triangles can be added */
		added = true;
		while ( added )
		{
			/* print pacifier */
			f = 10 * *numAdded / numMetaTriangles;
			if ( f > *fOld ) {
				*fOld = f;
				Sys_FPrintf( SYS_VRB, "%d...", f );
			}

			/* reset best score */
			best = -1;
			bestScore = 0;
			added = false;

			/* walk the list of possible candidates for merging */
			for ( j = i + 1, test = &possibles[ j ]; j < numPossibles; j++, test++ )
			{
				/* skip this triangle if it has already been merged */
				if ( test->si == NULL ) {
					continue;
				}

				/* score this triangle */
				score = AddMetaTriangleToSurface( ds, test, true );
				if ( score > bestScore ) {
					best = j;
					bestScore = score;

					/* if we have a score over a certain threshold, just use it */
					if ( bestScore >= GOOD_SCORE ) {
						if ( AddMetaTriangleToSurface( ds, &possibles[ best ], false ) ) {
							( *numAdded )++;
						}

						/* reset */
						best = -1;
						bestScore = 0;
						added = true;
					}
				}
			}

			/* add best candidate */
			if ( best >= 0 && bestScore > ADEQUATE_SCORE ) {
				if ( AddMetaTriangleToSurface( ds, &possibles[ best ], false ) ) {
					( *numAdded )++;
				}

				/* reset */
				added = true;
			}
		}

		/* copy the verts and indexes to the new surface */
		ds->verts = safe_malloc( ds->numVerts * sizeof( bspDrawVert_t ) );
		memcpy( ds->verts, verts, ds->numVerts * sizeof( bspDrawVert_t ) );
		ds->indexes = safe_malloc( ds->numIndexes * sizeof( int ) );
		memcpy( ds->indexes, indexes, ds->numIndexes * sizeof( int ) );

		/* classify the surface */
		ClassifySurfaces( 1, ds );

		/* add to count */
		numMergedSurfaces++;
	}

	/* free arrays */
	free( verts );
	free( indexes );
}



/*
   CompareMetaTriangles
   compare functor for std::sort()
 */

struct CompareMetaTriangles
{
	bool operator()( const metaTriangle_t& a, const metaTriangle_t& b ) const {
		/* shader first */
		if ( a.si < b.si ) {
			return false;
		}
		else if ( a.si > b.si ) {
			return true;
		}

		/* then fog */
		else if ( a.fogNum < b.fogNum ) {
			return false;
		}
		else if ( a.fogNum > b.fogNum ) {
			return true;
		}

		/* then plane */
		#if 0
		else if ( npDegrees == 0.0f && !a.si->nonplanar &&
				  a.planeNum >= 0 && a.planeNum >= 0 ) {
			if ( a.plane.dist() < b.plane.dist() ) {
				return false;
			}
			else if ( a.plane.dist() > b.plane.dist() ) {
				return true;
			}
			else if ( a.plane.normal()[ 0 ] < b.plane.normal()[ 0 ] ) {
				return false;
			}
			else if ( a.plane.normal()[ 0 ] > b.plane.normal()[ 0 ] ) {
				return true;
			}
			else if ( a.plane.normal()[ 1 ] < b.plane.normal()[ 1 ] ) {
				return false;
			}
			else if ( a.plane.normal()[ 1 ] > b.plane.normal()[ 1 ] ) {
				return true;
			}
			else if ( a.plane.normal()[ 2 ] < b.plane.normal()[ 2 ] ) {
				return false;
			}
			else if ( a.plane.normal()[ 2 ] > b.plane.normal()[ 2 ] ) {
				return true;
			}
		}
		#endif

		/* then position in world */

		/* find mins */
		Vector3 aMins( 999999 );
		Vector3 bMins( 999999 );
		for ( int i = 0; i < 3; i++ )
		{
			const int av = a.indexes[ i ];
			const int bv = b.indexes[ i ];
			for ( int j = 0; j < 3; j++ )
			{
				value_minimize( aMins[ j ], metaVerts[ av ].xyz[ j ] );
				value_minimize( bMins[ j ], metaVerts[ bv ].xyz[ j ] );
			}
		}

		/* test it */
		for ( int i = 0; i < 3; i++ )
		{
			if ( aMins[ i ] < bMins[ i ] ) {
				return false;
			}
			else if ( aMins[ i ] > bMins[ i ] ) {
				return true;
			}
		}

		/* functionally equivalent */
		return false;
	}
};



/*
   MergeMetaTriangles()
   merges meta triangles into drawsurfaces
 */

void MergeMetaTriangles( void ){
	int i, j, fOld, start, numAdded;
	metaTriangle_t      *head, *end;


	/* only do this if there are meta triangles */
	if ( numMetaTriangles <= 0 ) {
		return;
	}

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- MergeMetaTriangles ---\n" );

	/* sort the triangles by shader major, fognum minor */
	std::sort( metaTriangles, metaTriangles + numMetaTriangles, CompareMetaTriangles() );

	/* init pacifier */
	fOld = -1;
	start = I_FloatTime();
	numAdded = 0;

	/* merge */
	for ( i = 0, j = 0; i < numMetaTriangles; i = j )
	{
		/* get head of list */
		head = &metaTriangles[ i ];

		/* skip this triangle if it has already been merged */
		if ( head->si == NULL ) {
			continue;
		}

		/* find end */
		if ( j <= i ) {
			for ( j = i + 1; j < numMetaTriangles; j++ )
			{
				/* get end of list */
				end = &metaTriangles[ j ];
				if ( head->si != end->si || head->fogNum != end->fogNum ) {
					break;
				}
			}
		}

		/* try to merge this list of possible merge candidates */
		MetaTrianglesToSurface( ( j - i ), head, &fOld, &numAdded );
	}

	/* clear meta triangle list */
	ClearMetaTriangles();

	/* print time */
	if ( i ) {
		Sys_FPrintf( SYS_VRB, " (%d)\n", (int) ( I_FloatTime() - start ) );
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d surfaces merged\n", numMergedSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d vertexes merged\n", numMergedVerts );
}

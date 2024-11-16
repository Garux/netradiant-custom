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
#include "timer.h"
#include <map>
#include <set>


const Plane3f c_spatial_sort_plane( 0.786868, 0.316861, 0.529564, 0 );
const float c_spatial_EQUAL_EPSILON = EQUAL_EPSILON * 2;

inline float spatial_distance( const Vector3& point ){
	return plane3_distance_to_point( c_spatial_sort_plane, point );
}

struct metaTriangle_t;
struct metaVertex_t : public bspDrawVert_t
{
	std::vector<metaTriangle_t*> m_triangles;    // references to triangles, introducing this vertex // bspDrawVert_equal()
	std::list<metaVertex_t> *m_metaVertexGroup;  // reference to own group of vertices with equal .xyz position

	metaVertex_t() = default;
	metaVertex_t( const bspDrawVert_t& vert ) : bspDrawVert_t( vert ){}
};

using MetaVertexGroups = std::multimap
<float, // spatial_distance( std::list<metaVertex_t>>.front().xyz )
std::list<metaVertex_t>>; // must be maintained non empty

struct MinMax1D
{
	float min, max;
	MinMax1D() : min( std::numeric_limits<float>::max() ), max( std::numeric_limits<float>::lowest() ){}
	void extend( float val ){
		value_minimize( min, val );
		value_maximize( max, val );
	}
};

/* ydnar: metasurfaces are constructed from lists of metatriangles so they can be merged in the best way */
struct metaTriangle_t
{
	shaderInfo_t        *si;
	const side_t        *side;
	int entityNum, surfaceNum, planeNum, fogNum, sampleSize, castShadows, recvShadows;
	float shadeAngleDegrees;
	Plane3f plane;
	Vector3 lightmapAxis;
	std::array<metaVertex_t*, 3> m_vertices;
	MinMax1D minmax;
};




#define VERTS_EXCEEDED      -1000

static int numMetaSurfaces, numPatchMetaSurfaces;

static MetaVertexGroups metaVerts;
static std::list<metaTriangle_t> metaTriangles;



/*
   ClearMetaVertexes()
   called before staring a new entity to clear out the triangle list
 */

void ClearMetaTriangles(){
	metaVerts.clear();
	metaTriangles.clear();
}



inline bool bspDrawVert_equal( const bspDrawVert_t& a, const bspDrawVert_t& b ){
	return VectorCompare( a.xyz, b.xyz )
	    && VectorCompare( a.normal, b.normal )
	    && vector2_equal_epsilon( a.st, b.st, 1e-4f )
	    && a.color[ 0 ].alpha() == b.color[ 0 ].alpha();
}

/*
   metaVertex_findOrInsert()
   finds a matching metavertex in the global list or inserts new one
 */

static metaVertex_t* metaVertex_findOrInsert( const bspDrawVert_t& src ){
	/* try to find an existing drawvert */
	const auto begin = metaVerts.lower_bound( spatial_distance( src.xyz ) - c_spatial_EQUAL_EPSILON );
	const auto end = metaVerts.upper_bound( spatial_distance( src.xyz ) + c_spatial_EQUAL_EPSILON );

	for( auto it = begin; it != end; ++it ){
		for( auto& vertex : it->second )
			if( bspDrawVert_equal( src, vertex ) )
				return &vertex;
	}
	/* try to put to exisitng group */
	for( auto it = begin; it != end; ++it ){
		auto& list = it->second;
		if( VectorCompare( src.xyz, list.front().xyz ) ){
			auto& newVertex = list.emplace_back( src );
			newVertex.m_metaVertexGroup = &list;
			return &newVertex;
		}
	}

	/* add new vertex group */
	auto& list = metaVerts.emplace_hint( begin, spatial_distance( src.xyz ), decltype( metaVerts )::mapped_type() )->second;
	auto& newVertex = list.emplace_back( src );
	newVertex.m_metaVertexGroup = &list;
	/* return the vertex */
	return &newVertex;
}



/*
   CompareMetaTriangles
   compare functor for std::sort()
 */

template<bool sort_spatially>
struct CompareMetaTriangles
{
	bool operator()( const metaTriangle_t& a, const metaTriangle_t& b ) const {
		/* shader first */
		if ( a.si != b.si ) {
			return a.si < b.si;
		}
		/* then fog */
		else if ( a.fogNum != b.fogNum ) {
			return a.fogNum < b.fogNum;
		}
		else if ( a.entityNum != b.entityNum ) { /* ydnar: added 2002-07-06 */
			return a.entityNum < b.entityNum;
		}
		else if ( a.castShadows != b.castShadows ) {
			return a.castShadows < b.castShadows;
		}
		else if ( a.recvShadows != b.recvShadows ) {
			return a.recvShadows < b.recvShadows;
		}
		else if ( a.sampleSize != b.sampleSize ) {
			return a.sampleSize < b.sampleSize;
		}

		/* then position in world */
		if constexpr ( sort_spatially ){
			return a.minmax.min < b.minmax.min;
		}

		/* functionally equivalent */
		return false;
	}
	/* equal in terms of mergeability */
	static bool equal( const metaTriangle_t& a, const metaTriangle_t& b ) {
		return ( a.si == b.si )
		&& ( a.fogNum == b.fogNum )
		&& ( a.entityNum == b.entityNum )
		&& ( a.castShadows == b.castShadows )
		&& ( a.recvShadows == b.recvShadows )
		&& ( a.sampleSize == b.sampleSize );
	}
};


/*
   metaTriangle_insert()
   finds a matching metatriangle in the global list,
   otherwise adds it
 */

static void metaTriangle_insert( metaTriangle_t& src, std::array<bspDrawVert_t, 3> verts, int planeNum ){
	/* detect degenerate triangles fixme: do something proper here */
	if ( vector3_length( verts[0].xyz - verts[1].xyz ) < 0.125f
	  || vector3_length( verts[1].xyz - verts[2].xyz ) < 0.125f
	  || vector3_length( verts[2].xyz - verts[0].xyz ) < 0.125f ) {
		return;
	}

	/* find plane */
	if ( planeNum >= 0 ) {
		/* because of precision issues with small triangles, try to use the specified plane */
		src.planeNum = planeNum;
		src.plane = mapplanes[ planeNum ].plane;
	}
	else
	{
		/* calculate a plane from the triangle's points (and bail if a plane can't be constructed) */
		src.planeNum = -1;
		if ( !PlaneFromPoints( src.plane, verts[0].xyz, verts[1].xyz, verts[2].xyz ) ) {
			return;
		}
	}

	/* ydnar 2002-10-03: repair any bogus normals (busted ase import kludge) */
	for( auto& ve : verts )
		if ( vector3_length( ve.normal ) == 0.0f )
			ve.normal = src.plane.normal();

	/* ydnar 2002-10-04: set lightmap axis if not already set */
	if ( !( src.si->compileFlags & C_VERTEXLIT ) &&
	     src.lightmapAxis == g_vector3_identity ) {
		/* the shader can specify an explicit lightmap axis */
		if ( src.si->lightmapAxis != g_vector3_identity ) {
			src.lightmapAxis = src.si->lightmapAxis;
		}
		/* new axis-finding code */
		else{
			src.lightmapAxis = CalcLightmapAxis( src.plane.normal() );
		}
	}

	/* fill out the src triangle */
	src.m_vertices[0] = metaVertex_findOrInsert( verts[0] );
	src.m_vertices[1] = metaVertex_findOrInsert( verts[1] );
	src.m_vertices[2] = metaVertex_findOrInsert( verts[2] );

	/* try to find an existing triangle */
	if( !src.m_vertices[0]->m_triangles.empty() // all vertices aren't brand new and have triangles assinged already
	 && !src.m_vertices[1]->m_triangles.empty()
	 && !src.m_vertices[2]->m_triangles.empty() ) {
		/* find common triangle */
		for( const auto t1 : src.m_vertices[0]->m_triangles ){
			for( const auto t2 : src.m_vertices[1]->m_triangles ){
				if( t1 == t2 ){
					for( const auto t3 : src.m_vertices[2]->m_triangles ){
						if( t1 == t3 ){
							if( CompareMetaTriangles<false>::equal( src, *t1 ) ){ // equal to src
								Sys_Warning( "Duplicate or Flipped triangle: (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f)\n",
									verts[0].xyz.x(), verts[0].xyz.y(), verts[0].xyz.z(),
									verts[1].xyz.x(), verts[1].xyz.y(), verts[1].xyz.z(),
									verts[2].xyz.x(), verts[2].xyz.y(), verts[2].xyz.z() );
								return;
							}
						}
					}
				}
			}
		}
	}

	/* add the triangle */
	auto& newTriangle = metaTriangles.emplace_back( src );
	/* reference it */
	for( auto ve : newTriangle.m_vertices )
		ve->m_triangles.push_back( &newTriangle );
}



/*
   SurfaceToMetaTriangles()
   converts a classified surface to metatriangles
 */

static void SurfaceToMetaTriangles( mapDrawSurface_t *ds ){
	/* only handle certain types of surfaces */
	if ( ds->type != ESurfaceType::Face &&
	     ds->type != ESurfaceType::Meta &&
	     ds->type != ESurfaceType::ForcedMeta &&
	     ds->type != ESurfaceType::Decal ) {
		return;
	}

	/* only handle valid surfaces */
	if ( ds->type != ESurfaceType::Bad && ds->numVerts >= 3 && ds->numIndexes >= 3 ) {
		/* walk the indexes and create triangles */
		for ( int i = 0; i < ds->numIndexes; i += 3 )
		{
			/* sanity check the indexes */
			if ( ds->indexes[ i ] == ds->indexes[ i + 1 ] ||
			     ds->indexes[ i ] == ds->indexes[ i + 2 ] ||
			     ds->indexes[ i + 1 ] == ds->indexes[ i + 2 ] ) {
				//%	Sys_Printf( "%d! ", ds->numVerts );
				continue;
			}

			/* build a metatriangle */
			metaTriangle_t src;
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

			metaTriangle_insert( src, { ds->verts[ ds->indexes[ i ] ],
			                            ds->verts[ ds->indexes[ i + 1 ] ],
			                            ds->verts[ ds->indexes[ i + 2 ] ] }, ds->planeNum );
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

static void TriangulatePatchSurface( const entity_t& e, mapDrawSurface_t *ds ){
	int x, y, pw[ 5 ], r;
	mapDrawSurface_t    *dsNew;
	mesh_t src, *subdivided, *mesh;

	/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
	const bool forcePatchMeta = e.boolForKey( "_patchMeta", "patchMeta" );

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
	if ( e.read_keyvalue( patchSubdivision, "_patchSubdivide", "patchSubdivide" ) ) {
		iterations = IterationsForCurve( ds->longestCurve, patchSubdivision );
	}
	else{
		const int patchQuality = e.intForKey( "_patchQuality", "patchQuality" );
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
	if ( !( ds->shaderInfo->compileFlags & C_SOLID ) && !( ds->shaderInfo->contentFlags & GetRequiredSurfaceParm( "playerclip"_Tstring ).contentFlags ) ) {
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

#define TINY_AREA   1.0
#define MAXAREA_MAXTRIES 8
static int MaxAreaIndexes( bspDrawVert_t *vert, int cnt, int *indexes ){
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
		A += triangle_area2x( vert[0].xyz, vert[i].xyz, vert[i + 1].xyz );
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

			bestA = triangle_area2x( vert[bestR].xyz, vert[bestS].xyz, vert[bestT].xyz );
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

static void FanFaceSurface( mapDrawSurface_t *ds ){
	int i, k, a, b, c;
	Color4f color[ MAX_LIGHTMAPS ];
	for ( auto& co : color )
		co.set( 0 );
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
	int numIndexes, indexes[ MAX_INDEXES ];

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
		int least = 0;
		if ( ds->shaderInfo != NULL && !ds->shaderInfo->autosprite ) {
			for ( int i = 0; i < ds->numVerts; i++ )
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

		class TriEval
		{
			const bspDrawVert_t *m_verts;
			double m_area = std::numeric_limits<double>::max(); // 2x area
			double m_angle = std::numeric_limits<double>::max(); // squared sin of the angle
		public:
			TriEval( const bspDrawVert_t *verts ) : m_verts( verts ){
			}
			void push( int a, int b, int c ){
				value_minimize( m_angle, triangle_min_angle_squared_sin( m_verts[a].xyz, m_verts[b].xyz, m_verts[c].xyz ) );
				value_minimize( m_area, triangle_area2x( m_verts[a].xyz, m_verts[b].xyz, m_verts[c].xyz ) );
			}
			bool decent() const {
				return m_angle > 1e-5 && m_area > TINY_AREA;
			}
			void reset(){
				*this = TriEval( m_verts );
			}
		} triEval( ds->verts );

		const auto idx = [n = ds->numVerts]( int i ){ return i < 0? i + n : i < n? i : i - n; };

		/* try all possible orderings of the points looking for a non-degenerate strip order */
		for ( int r = 0; r < ds->numVerts; ++r )
		{
			triEval.reset();
			/* walk the winding in both directions */
			for( int i = idx( r + least ), j = idx( i - 1 ), k, swap = 0, out = 0;
			    ( swap ^= bspDrawVert_is_tjunc( ds->verts[idx( swap? i + 1 : j - 1 )] )
			           >= bspDrawVert_is_tjunc( ds->verts[idx( swap? j - 1 : i + 1 )] ) )
			    ? ( k = j, j = idx( --j ) ) : ( k = i, i = idx( ++i ) ), i != j; )
			{
				/* test this triangle */
				if ( triEval.push( i, j, k ), !triEval.decent() ) {
					break;
				}
				indexes[ out++ ] = i;
				indexes[ out++ ] = j;
				indexes[ out++ ] = k;
			}

			if( triEval.decent() )
				goto okej;
		}

		/* if any triangle in the strip is degenerate, render from a centered fan point instead */
		return FanFaceSurface( ds );
	}
okej:
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
	Sys_Printf( "%9zu meta verts\n", metaVerts.size() );
	Sys_Printf( "%9zu meta triangles\n", metaTriangles.size() );
}

/*
   MakeEntityMetaTriangles()
   builds meta triangles from brush faces (tristrips and fans)
 */

void MakeEntityMetaTriangles( const entity_t& e ){
	/* note it */
	Sys_FPrintf( SYS_VRB, "--- MakeEntityMetaTriangles ---\n" );

	/* init pacifier */
	int fOld = -1;
	Timer timer;

	/* walk the list of surfaces in the entity */
	for ( int i = e.firstDrawSurf; i < numMapDrawSurfs; ++i )
	{
		/* print pacifier */
		if ( const int f = 10 * ( i - e.firstDrawSurf ) / ( numMapDrawSurfs - e.firstDrawSurf ); f != fOld ) {
			fOld = f;
			Sys_FPrintf( SYS_VRB, "%d...", f );
		}

		/* get surface */
		mapDrawSurface_t *ds = &mapDrawSurfs[ i ];
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
	if ( ( numMapDrawSurfs - e.firstDrawSurf ) ) {
		Sys_FPrintf( SYS_VRB, " (%d)\n", int( timer.elapsed_sec() ) );
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d total meta surfaces\n", numMetaSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d stripped surfaces\n", numStripSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d fanned surfaces\n", numFanSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d maxarea'd surfaces\n", numMaxAreaSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d patch meta surfaces\n", numPatchMetaSurfaces );
	Sys_FPrintf( SYS_VRB, "%9zu meta verts\n", metaVerts.size() );
	Sys_FPrintf( SYS_VRB, "%9zu meta triangles\n", metaTriangles.size() );

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

static void CreateEdge( const Plane3f& plane, const Vector3& a, const Vector3& b, edge_t *edge ){
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

void FixMetaTJunctions(){
#if 0
	int i, j, k, fOld, vertIndex, triIndex, numTJuncs;
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
	Timer timer;

	/* walk triangle list */
	numTJuncs = 0;
	for ( i = 0; i < numMetaTriangles; i++ )
	{
		/* get triangle */
		tri = &metaTriangles[ i ];

		/* print pacifier */
		if ( const int f = 10 * i / numMetaTriangles; f != fOld ) {
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
					vertIndex = metaVertex_findOrInsert( &junc );
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
	Sys_FPrintf( SYS_VRB, " (%d)\n", int( timer.elapsed_sec() ) );

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d T-junctions added\n", numTJuncs );
#endif
}



/*
   SmoothMetaTriangles()
   averages coincident vertex normals in the meta triangles
 */

#define THETA_EPSILON           0.000001
#define EQUAL_NORMAL_EPSILON    0.01f

void SmoothMetaTriangles(){
	Timer timer;
	int numSmoothed = 0;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SmoothMetaTriangles ---\n" );

	/* set default shade angle */
	const float defaultShadeAngle = degrees_to_radians( npDegrees );

	for( auto& [ d, list ] : metaVerts ){
		if( list.size() > 1 || ( list.size() == 1 && list.front().m_triangles.size() > 1 ) ){
			float maxShadeAngle = 0.f;

			struct VT{ metaVertex_t *vertex; metaTriangle_t *triangle; float angle{}; bool skipped{}; bool smoothed{}; Vector3 newnormal{ 0 }; };
			std::vector<VT> verts;
			for( auto& v : list )
				for( auto* t : v.m_triangles )
					verts.push_back( VT{ &v, t } );
			/* get per-vertex smoothing angle */
			for( auto& v : verts )
			{
				float shadeAngle = defaultShadeAngle;
				/* get shade angle from shader */
				if ( v.triangle->si->shadeAngleDegrees > 0.0f ) {
					shadeAngle = degrees_to_radians( v.triangle->si->shadeAngleDegrees );
				}
				/* get shade angle from entity */
				else if ( v.triangle->shadeAngleDegrees > 0.0f ) {
					shadeAngle = degrees_to_radians( v.triangle->shadeAngleDegrees );
				}

				/* flag verts */
				v.angle = shadeAngle;
				v.skipped = shadeAngle <= 0;

				value_maximize( maxShadeAngle, shadeAngle );
			}

			if( maxShadeAngle > 0 ){
				/* go through the list of vertexes */
				for ( auto v = verts.begin(); v != verts.end(); ++v )
				{
					/* already smoothed? */
					if ( v->skipped || v->smoothed ) {
						continue;
					}

					/* clear */
					Vector3 average( 0 );
					std::vector<VT*> smoothedVerts;
					std::vector<Vector3> votes;

					/* test the rest, including self */
					for ( auto v2 = v; v2 != verts.end(); ++v2 )
					{
						/* already smoothed? */
						if ( v2->skipped || v2->smoothed ) {
							continue;
						}

						/* use smallest shade angle */
						const float shadeAngle = std::min( v->angle, v2->angle );

						/* check shade angle */
						const double dot = std::clamp( vector3_dot( v->vertex->normal, v2->vertex->normal ), -1.0, 1.0 );
						if ( acos( dot ) + THETA_EPSILON >= shadeAngle ) {
							continue;
						}

						/* add to the list */
						smoothedVerts.push_back( v2.operator->() );

						/* see if this normal has already been voted */
						if( std::none_of( votes.begin(), votes.end(),
							[normal = v2->vertex->normal]( const Vector3& vote ){
								return vector3_equal_epsilon( normal, vote, EQUAL_NORMAL_EPSILON );
							} ) )
						{ /* add a new vote */
							average += v2->vertex->normal;
							votes.push_back( v2->vertex->normal );
						}
					}

					/* flag vertices */
					/* don't average for less than 2 verts */
					if ( smoothedVerts.size() > 1 && VectorNormalize( average ) != 0 ) {
						for ( auto *v : smoothedVerts ){
							v->smoothed = true;
							v->newnormal = average;
						}
						numSmoothed++;
					}
					else{
						for ( auto *v : smoothedVerts ){
							v->skipped = true;
						}
					}
				}
				/* reconstruct meta data from smoothed verts */
				if( std::any_of( verts.cbegin(), verts.cend(), []( const VT& vt ){ return vt.smoothed; } ) ){
					decltype( list ) newlist;
					for( auto& v : verts ){
						bspDrawVert_t newv = *v.vertex;
						if( v.smoothed )
							newv.normal = v.newnormal;

						auto it = std::find_if( newlist.begin(), newlist.end(), [&newv]( const metaVertex_t& v ){
							return bspDrawVert_equal( newv, v );
						} );
						if( it == newlist.end() ){ /* insert vertex */
							newlist.push_back( newv );
							it = --newlist.end();
							it->m_metaVertexGroup = &list;
						}
						/* link vertex <> triangle */
						it->m_triangles.push_back( v.triangle );
						*std::find( v.triangle->m_vertices.begin(), v.triangle->m_vertices.end(), v.vertex ) = it.operator->();
					}
					list.swap( newlist );
				}
			}
		}
	}

	/* print time */
	Sys_FPrintf( SYS_VRB, " (%d)\n", int( timer.elapsed_sec() ) );

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d smoothed vertexes\n", numSmoothed );
}



using Sorted_indices = std::multimap
<float, // plane3_distance_to_point( c_spatial_sort_plane, point )
int>;   // index of bspDrawVert_t in mapDrawSurface_t::verts array


/*
   AddMetaVertToSurface()
   adds a drawvert to a surface unless an existing vert matching already exists
   returns the index of that vert (or < 0 on failure)
 */

static int AddMetaVertToSurface( mapDrawSurface_t *ds, const bspDrawVert_t& dv1, const Sorted_indices& sorted_indices, int *coincident ){
	/* go through the verts and find a suitable candidate */
	const auto begin = sorted_indices.lower_bound( spatial_distance( dv1.xyz ) - c_spatial_EQUAL_EPSILON );
	const auto end = sorted_indices.upper_bound( spatial_distance( dv1.xyz ) + c_spatial_EQUAL_EPSILON );
	for( auto it = begin; it != end; ++it ){
		/* get test vert */
		const bspDrawVert_t& dv2 = ds->verts[ it->second ];

		/* compare xyz and normal */
		if ( !VectorCompare( dv1.xyz, dv2.xyz ) ) {
			continue;
		}
		if ( !VectorCompare( dv1.normal, dv2.normal ) ) {
			continue;
		}

		/* good enough at this point */
		( *coincident )++;

		/* compare texture coordinates and color */
		if ( !vector2_equal_epsilon( dv1.st, dv2.st, 1e-4f ) ) {
			continue;
		}
		if ( dv1.color[ 0 ].alpha() != dv2.color[ 0 ].alpha() ) {
			continue;
		}

		/* found a winner */
		numMergedVerts++;
		return it->second;
	}

	/* overflow check */
	if ( ds->numVerts >= ( ( ds->shaderInfo->compileFlags & C_VERTEXLIT ) ? maxSurfaceVerts : maxLMSurfaceVerts ) ) {
		return VERTS_EXCEEDED;
	}

	/* made it this far, add the vert and return */
	ds->verts[ ds->numVerts ] = dv1;
	return ds->numVerts++;
}




/*
   AddMetaTriangleToSurface()
   attempts to add a metatriangle to a surface
   returns the score of the triangle added
 */

#define AXIS_SCORE          100000
#define AXIS_MIN            100000
#define VERT_SCORE          10000
#define SURFACE_SCORE       1000
#define ST_SCORE            50
#define ST_SCORE2           ( 2 * ( ST_SCORE ) )

#define DEFAULT_ADEQUATE_SCORE  ( (AXIS_MIN) + 1 * (VERT_SCORE) )
#define DEFAULT_GOOD_SCORE      ( (AXIS_MIN) + 2 * (VERT_SCORE) + 4 * (ST_SCORE) )
#define PERFECT_SCORE           ( (AXIS_MIN) + 3 * (VERT_SCORE) + (SURFACE_SCORE) + 4 * (ST_SCORE) )

#define ADEQUATE_SCORE          ( metaAdequateScore >= 0 ? metaAdequateScore : DEFAULT_ADEQUATE_SCORE )
#define GOOD_SCORE              ( metaGoodScore     >= 0 ? metaGoodScore     : DEFAULT_GOOD_SCORE )

static int AddMetaTriangleToSurface( mapDrawSurface_t *ds, const metaTriangle_t& tri, MinMax& texMinMax, Sorted_indices& sorted_indices, bool testAdd ){
	int i, score, coincident, ai, bi, ci;


	/* test the triangle */
	#if 0
	if ( !( ds->shaderInfo->compileFlags & C_VERTEXLIT ) &&
	     //% !VectorCompare( ds->lightmapAxis, tri.lightmapAxis ) )
		 vector3_dot( ds->lightmapAxis, tri.plane.normal() ) < 0.25f ) {
		return 0;
	}
	#endif

	/* planar surfaces will only merge with triangles in the same plane */
	if ( npDegrees == 0.0f && !ds->shaderInfo->nonplanar && ds->planeNum >= 0 ) {
		if ( tri.planeNum >= 0 && tri.planeNum != ds->planeNum ) {
			return 0;
		}
		if ( !VectorCompare( mapplanes[ ds->planeNum ].normal(), tri.plane.normal() ) || mapplanes[ ds->planeNum ].dist() != tri.plane.dist() ) {
			return 0;
		}
	}

	/* set initial score */
	score = tri.surfaceNum == ds->surfaceNum ? SURFACE_SCORE : 0;

	/* score the the dot product of lightmap axis to plane */
	if ( ( ds->shaderInfo->compileFlags & C_VERTEXLIT ) || VectorCompare( ds->lightmapAxis, tri.lightmapAxis ) ) {
		score += AXIS_SCORE;
	}
	else{
		score += AXIS_SCORE * vector3_dot( ds->lightmapAxis, tri.plane.normal() );
	}

	/* preserve old drawsurface if this fails */
	mapDrawSurface_t old( *ds );

	/* attempt to add the verts */
	const int numVerts_original = ds->numVerts;
	coincident = 0;
	ai = AddMetaVertToSurface( ds, *tri.m_vertices[ 0 ], sorted_indices, &coincident );
	bi = AddMetaVertToSurface( ds, *tri.m_vertices[ 1 ], sorted_indices, &coincident );
	ci = AddMetaVertToSurface( ds, *tri.m_vertices[ 2 ], sorted_indices, &coincident );

	/* check vertex underflow */
	if ( ai < 0 || bi < 0 || ci < 0 ) {
		memcpy( ds, &old, sizeof( *ds ) );
		return 0;
	}

	/* score coincident vertex count (2003-02-14: changed so this only matters on planar surfaces) */
	score += ( coincident * VERT_SCORE );

	/* add new vertex bounds to mins/maxs */
	MinMax minmax( ds->minmax );
	minmax.extend( tri.m_vertices[ 0 ]->xyz );
	minmax.extend( tri.m_vertices[ 1 ]->xyz );
	minmax.extend( tri.m_vertices[ 2 ]->xyz );

	/* check lightmap bounds overflow (after at least 1 triangle has been added) */
	if ( !( ds->shaderInfo->compileFlags & C_VERTEXLIT ) &&
	     ds->numIndexes > 0 && ds->lightmapAxis != g_vector3_identity &&
	     ( !VectorCompare( ds->minmax.mins, minmax.mins ) || !VectorCompare( ds->minmax.maxs, minmax.maxs ) ) ) {
		/* set maximum size before lightmap scaling (normally 2032 units) */
		/* 2004-02-24: scale lightmap test size by 2 to catch larger brush faces */
		/* 2004-04-11: reverting to actual lightmap size */
		const float lmMax = ( ds->sampleSize * ( ds->shaderInfo->lmCustomWidth - 1 ) );
		for ( i = 0; i < 3; i++ )
		{
			if ( ( minmax.maxs[ i ] - minmax.mins[ i ] ) > lmMax ) {
				memcpy( ds, &old, sizeof( *ds ) );
				return 0;
			}
		}
	}

	/* check texture range overflow */
	MinMax newTexMinMax( texMinMax );
	{
		newTexMinMax.extend( Vector3( tri.m_vertices[ 0 ]->st ) );
		newTexMinMax.extend( Vector3( tri.m_vertices[ 1 ]->st ) );
		newTexMinMax.extend( Vector3( tri.m_vertices[ 2 ]->st ) );
		if( numVerts_original == 0 || texMinMax.surrounds( newTexMinMax ) ){
			score += 4 * ST_SCORE;
		}
		else{
			const Vector2 wh( ds->shaderInfo->shaderWidth, ds->shaderInfo->shaderHeight );
			BasicVector2<int> oldTexRange( ( texMinMax.maxs - texMinMax.mins ).vec2() * wh );
			BasicVector2<int> newTexRange( ( newTexMinMax.maxs - newTexMinMax.mins ).vec2() * wh );
			/* score texture range */
			if ( newTexRange[ 0 ] <= oldTexRange[ 0 ] ) {
				score += ST_SCORE2;
			}
			else if ( oldTexRange[ 1 ] > oldTexRange[ 0 ] ) {
				score += ST_SCORE;
			}

			if ( newTexRange[ 1 ] <= oldTexRange[ 1 ] ) {
				score += ST_SCORE2;
			}
			else if ( oldTexRange[ 0 ] > oldTexRange[ 1 ] ) {
				score += ST_SCORE;
			}
		}
	}


	/* check index overflow */
	if ( ds->numIndexes + 3 > maxSurfaceIndexes  ) {
		memcpy( ds, &old, sizeof( *ds ) );
		return 0;
	}
	else{ /* add the triangle indexes */
		ds->indexes[ ds->numIndexes++ ] = ai;
		ds->indexes[ ds->numIndexes++ ] = bi;
		ds->indexes[ ds->numIndexes++ ] = ci;
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
		/* store new bounds */
		ds->minmax = minmax;
		texMinMax = newTexMinMax;

		/* add a side reference */
		ds->sideRef = AllocSideRef( tri.side, ds->sideRef );

		for( const auto id : { ai, bi, ci } ){
			if( id >= numVerts_original )
				sorted_indices.emplace( spatial_distance( ds->verts[id].xyz ), id );
		}
	}

	/* return to sender */
	return score;
}



/*
   MetaTrianglesToSurface()
   creates map drawsurface(s) from the list of possibles
 */

static void MetaTrianglesToSurface( int *fOld, int *numAdded ){
	/* allocate arrays */
	bspDrawVert_t *verts = safe_malloc( sizeof( *verts ) * maxSurfaceVerts );
	int *indexes = safe_malloc( sizeof( *indexes ) * maxSurfaceIndexes );

	/* walk the list of triangles */
	for ( auto& seed : metaTriangles )
	{
		/* skip this triangle if it has already been merged */
		if ( seed.si == NULL ) {
			continue;
		}

		/* -----------------------------------------------------------------
		   initial drawsurf construction
		   ----------------------------------------------------------------- */

		/* start a new drawsurface */
		mapDrawSurface_t *ds = AllocDrawSurface( ESurfaceType::Meta );
		ds->entityNum = seed.entityNum;
		ds->surfaceNum = seed.surfaceNum;
		ds->castShadows = seed.castShadows;
		ds->recvShadows = seed.recvShadows;

		ds->shaderInfo = seed.si;
		ds->planeNum = seed.planeNum;
		ds->fogNum = seed.fogNum;
		ds->sampleSize = seed.sampleSize;
		ds->shadeAngleDegrees = seed.shadeAngleDegrees;
		ds->verts = verts;
		ds->indexes = indexes;
		ds->lightmapAxis = seed.lightmapAxis;
		ds->sideRef = AllocSideRef( seed.side, NULL );

		ds->minmax.clear();

		MinMax texMinMax;

		Sorted_indices sorted_indices;

		/*
			strategy:
			DEFAULT_ADEQUATE_SCORE implies at least single vertex coincident with ones of surface
			thus we crosslink vertices<->triangles on construction and only test cloud of linked triangles
		*/

		std::vector<metaTriangle_t*> testCloud;

		const auto expand_cloud = [&testCloud]( metaTriangle_t& triangle ){
			for( metaVertex_t *trivert : triangle.m_vertices ){
				for( metaVertex_t& groupvert : *trivert->m_metaVertexGroup ){
					for( metaTriangle_t *tri : groupvert.m_triangles ){
						if( tri->si != nullptr
						&& CompareMetaTriangles<false>::equal( *tri, triangle )    // note: triangle.si must be still there for comparison
						&& std::find( testCloud.cbegin(), testCloud.cend(), tri ) == testCloud.cend() ){
							testCloud.push_back( tri );
						}
					}
				}
			}
			/* mark triangle as used */
			triangle.si = NULL;
			/* remove from cloud */
			if( auto it = std::find( testCloud.cbegin(), testCloud.cend(), &triangle ); it != testCloud.cend() ){
				testCloud.erase( it );
			}
#if 0
			/* sort spatially */
			std::sort( testCloud.begin(), testCloud.end(), []( const metaTriangle_t *a, const metaTriangle_t *b ){
				return a->minmax.min < b->minmax.min;
			} );
#endif
		};

		/* add the first triangle */
		if ( AddMetaTriangleToSurface( ds, seed, texMinMax, sorted_indices, false ) ) {
			( *numAdded )++;
		}
		expand_cloud( seed );

		/* -----------------------------------------------------------------
		   add triangles
		   ----------------------------------------------------------------- */

		/* progressively walk the list until no more triangles can be added */
		for( bool added = true; added; )
		{
			/* print pacifier */
			if ( const int f = 10 * *numAdded / metaTriangles.size(); f > *fOld ) {
				*fOld = f;
				Sys_FPrintf( SYS_VRB, "%d...", f );
			}

			/* reset best score */
			metaTriangle_t *best = nullptr;
			int bestScore = 0;
			added = false;

			/* walk the list of possible candidates for merging */
			for ( metaTriangle_t *test : testCloud )
			{
				/* skip this triangle if it has already been merged */
				if ( test->si == NULL ) {
					continue;
				}

				/* score this triangle */
				const int score = AddMetaTriangleToSurface( ds, *test, texMinMax, sorted_indices, true );
				if ( score > bestScore ) {
					best = test;
					bestScore = score;

					/* if we have a score over a certain threshold, just use it */
					if ( bestScore >= GOOD_SCORE ) {
						/* add it and restart loop; this way we make sure to add all possibly newly available GOOD_SCORE candidates; produces fewer and more quality surfaces */
						break;
					}
				}
			}

			/* add best candidate */
			if ( best != nullptr && bestScore > ADEQUATE_SCORE ) {
				if ( AddMetaTriangleToSurface( ds, *best, texMinMax, sorted_indices, false ) ) {
					( *numAdded )++;
					expand_cloud( *best );
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
		//% Sys_Warning( "numV: %d numIdx: %d\n", ds->numVerts, ds->numIndexes );
		/* ClassifySurfaces() sets axis from vertex normals
		   method is very questionable and axis actually happens to be wrong after normals passed through SmoothMetaTriangles()
		   use metaTriangle_t::lightmapAxis which is guaranteedly set and used as main factor for triangles merge */
		ds->lightmapAxis = seed.lightmapAxis;

		/* add to count */
		numMergedSurfaces++;
	}

	/* free arrays */
	free( verts );
	free( indexes );
}



/*
   MergeMetaTriangles()
   merges meta triangles into drawsurfaces
 */

void MergeMetaTriangles(){
	/* only do this if there are meta triangles */
	if ( metaTriangles.empty() ) {
		return;
	}

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- MergeMetaTriangles ---\n" );

	/* init pacifier */
	int fOld = -1;
	Timer timer;
	int numAdded = 0;
#if 1
	for( metaTriangle_t& tri : metaTriangles ){
		for( const metaVertex_t *vert : tri.m_vertices ){
			tri.minmax.extend( spatial_distance( vert->xyz ) );
		}
	}
	metaTriangles.sort( CompareMetaTriangles<true>() );
#endif
	MetaTrianglesToSurface( &fOld, &numAdded );

	/* clear meta triangle list */
	ClearMetaTriangles();

	/* print time */
	Sys_FPrintf( SYS_VRB, " (%d)\n", int( timer.elapsed_sec() ) );

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d surfaces merged\n", numMergedSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d vertexes merged\n", numMergedVerts );
}

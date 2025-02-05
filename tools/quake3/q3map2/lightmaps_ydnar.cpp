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
#include "bspfile_rbsp.h"
#include "surface_extra.h"
#include "timer.h"




/* -------------------------------------------------------------------------------

   this file contains code that doe lightmap allocation and projection that
   runs in the -light phase.

   this is handled here rather than in the bsp phase for a few reasons--
   surfaces are no longer necessarily convex polygons, patches may or may not be
   planar or have lightmaps projected directly onto control points.

   also, this allows lightmaps to be calculated before being allocated and stored
   in the bsp. lightmaps that have little high-frequency information are candidates
   for having their resolutions scaled down.

   ------------------------------------------------------------------------------- */

/*
   WriteTGA24()
   based on WriteTGA() from qimagelib.cpp
 */

static void WriteTGA24( char *filename, byte *data, int width, int height, bool flip ){
	int i;
	const int headSz = 18;
	const int sz = width * height * 3 + headSz;
	byte    *buffer, *pix, *in;
	FILE    *file;

	/* allocate a buffer and set it up */
	buffer = safe_malloc( sz );
	pix = buffer + headSz;
	memset( buffer, 0, headSz );
	buffer[ 2 ] = 2;		// uncompressed type
	buffer[ 12 ] = width & 255;
	buffer[ 13 ] = width >> 8;
	buffer[ 14 ] = height & 255;
	buffer[ 15 ] = height >> 8;
	buffer[ 16 ] = 24;	// pixel size

	/* swap rgb to bgr */
	for ( i = 0; i < sz - headSz; i += 3 )
	{
		pix[ i + 0 ] = data[ i + 2 ];   /* blue */
		pix[ i + 1 ] = data[ i + 1 ];   /* green */
		pix[ i + 2 ] = data[ i + 0 ];   /* red */
	}

	/* write it and free the buffer */
	file = SafeOpenWrite( filename );

	/* flip vertically? */
	if ( flip ) {
		fwrite( buffer, 1, headSz, file );
		for ( in = pix + ( ( height - 1 ) * width * 3 ); in >= pix; in -= ( width * 3 ) )
			fwrite( in, 1, ( width * 3 ), file );
	}
	else{
		fwrite( buffer, 1, sz, file );
	}

	/* close the file */
	fclose( file );
	free( buffer );
}



/*
   ExportLightmaps()
   exports the lightmaps as a list of numbered tga images
 */

void ExportLightmaps(){
	int i;
	char dirname[ 1024 ], filename[ 1024 ];
	byte        *lightmap;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- ExportLightmaps ---\n" );

	/* do some path mangling */
	strcpy( dirname, source );
	StripExtension( dirname );

	/* sanity check */
	if ( bspLightBytes.empty() ) {
		Sys_Warning( "No BSP lightmap data\n" );
		return;
	}

	/* make a directory for the lightmaps */
	Q_mkdir( dirname );

	/* iterate through the lightmaps */
	for ( i = 0, lightmap = bspLightBytes.data(); lightmap < ( bspLightBytes.data() + bspLightBytes.size() ); i++, lightmap += ( g_game->lightmapSize * g_game->lightmapSize * 3 ) )
	{
		/* write a tga image out */
		sprintf( filename, "%s/lightmap_%04d.tga", dirname, i );
		Sys_Printf( "Writing %s\n", filename );
		WriteTGA24( filename, lightmap, g_game->lightmapSize, g_game->lightmapSize, false );
	}
}



/*
   ExportLightmapsMain()
   exports the lightmaps as a list of numbered tga images
 */

int ExportLightmapsMain( Args& args ){
	/* arg checking */
	if ( args.empty() ) {
		Sys_Printf( "Usage: q3map2 -export [-v] <mapname>\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( args.takeBack() ) );
	path_set_extension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );

	/* export the lightmaps */
	ExportLightmaps();

	/* return to sender */
	return 0;
}



/*
   ImportLightmapsMain()
   imports the lightmaps from a list of numbered tga images
 */

int ImportLightmapsMain( Args& args ){
	int i, x, y, width, height;
	char dirname[ 1024 ], filename[ 1024 ];
	byte        *lightmap, *pixels, *in, *out;


	/* arg checking */
	if ( args.empty() ) {
		Sys_Printf( "Usage: q3map2 -import [-v] <mapname>\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( args.takeBack() ) );
	path_set_extension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- ImportLightmaps ---\n" );

	/* do some path mangling */
	strcpy( dirname, source );
	StripExtension( dirname );

	/* sanity check */
	if ( bspLightBytes.empty() ) {
		Error( "No lightmap data" );
	}

	/* make a directory for the lightmaps */
	Q_mkdir( dirname );

	/* iterate through the lightmaps */
	for ( i = 0, lightmap = bspLightBytes.data(); lightmap < ( bspLightBytes.data() + bspLightBytes.size() ); i++, lightmap += ( g_game->lightmapSize * g_game->lightmapSize * 3 ) )
	{
		/* read a tga image */
		sprintf( filename, "%s/lightmap_%04d.tga", dirname, i );
		Sys_Printf( "Loading %s\n", filename );
		MemBuffer buffer = vfsLoadFile( filename, -1 );
		if ( !buffer ) {
			Sys_Warning( "Unable to load image %s\n", filename );
			continue;
		}

		/* parse file into an image */
		pixels = NULL;
		LoadTGABuffer( buffer.data(), buffer.size(), &pixels, &width, &height );

		/* sanity check it */
		if ( pixels == NULL ) {
			Sys_Warning( "Unable to load image %s\n", filename );
			continue;
		}
		if ( width != g_game->lightmapSize || height != g_game->lightmapSize ) {
			Sys_Warning( "Image %s is not the right size (%d, %d) != (%d, %d)\n",
			             filename, width, height, g_game->lightmapSize, g_game->lightmapSize );
		}

		/* copy the pixels */
		in = pixels;
		for ( y = 1; y <= g_game->lightmapSize; y++ )
		{
			out = lightmap + ( ( g_game->lightmapSize - y ) * g_game->lightmapSize * 3 );
			for ( x = 0; x < g_game->lightmapSize; x++, in += 4, out += 3 )
				std::copy_n( in, 3, out );
		}

		/* free the image */
		free( pixels );
	}

	/* write the bsp */
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}



/* -------------------------------------------------------------------------------

   this section deals with projecting a lightmap onto a raw drawsurface

   ------------------------------------------------------------------------------- */

namespace
{
int numSurfsVertexLit;
int numSurfsVertexForced;
int numSurfsVertexApproximated;
int numSurfsLightmapped;
int numPlanarsLightmapped;
int numNonPlanarsLightmapped;
int numPatchesLightmapped;
int numPlanarPatchesLightmapped;
}

/*
   FinishRawLightmap()
   allocates a raw lightmap's necessary buffers
 */

static void FinishRawLightmap( rawLightmap_t *lm ){
	int i, j, c, size, *sc;
	float is;
	surfaceInfo_t       *info;


	/* sort light surfaces by shader name */
	std::sort( &lightSurfaces[ lm->firstLightSurface ], &lightSurfaces[ lm->firstLightSurface ] + lm->numLightSurfaces, []( const int a, const int b ){
		/* get shaders */
		const shaderInfo_t *asi = surfaceInfos[ a ].si;
		const shaderInfo_t *bsi = surfaceInfos[ b ].si;

		/* dummy check */
		if ( asi == NULL ) {
			return true;
		}
		if ( bsi == NULL ) {
			return false;
		}

		/* compare shader names */
		return strcmp( asi->shader, bsi->shader ) < 0;
	} );

	/* count clusters */
	lm->numLightClusters = 0;
	for ( i = 0; i < lm->numLightSurfaces; i++ )
	{
		/* get surface info */
		info = &surfaceInfos[ lightSurfaces[ lm->firstLightSurface + i ] ];

		/* add surface clusters */
		lm->numLightClusters += info->numSurfaceClusters;
	}

	/* allocate buffer for clusters and copy */
	lm->lightClusters = safe_malloc( lm->numLightClusters * sizeof( *lm->lightClusters ) );
	c = 0;
	for ( i = 0; i < lm->numLightSurfaces; i++ )
	{
		/* get surface info */
		info = &surfaceInfos[ lightSurfaces[ lm->firstLightSurface + i ] ];

		/* add surface clusters */
		for ( j = 0; j < info->numSurfaceClusters; j++ )
			lm->lightClusters[ c++ ] = surfaceClusters[ info->firstSurfaceCluster + j ];
	}

	/* set styles */
	lm->styles[ 0 ] = LS_NORMAL;
	for ( i = 1; i < MAX_LIGHTMAPS; i++ )
		lm->styles[ i ] = LS_NONE;

	/* set supersampling size */
	lm->sw = lm->w * superSample;
	lm->sh = lm->h * superSample;

	/* manipulate origin/vecs for supersampling */
	if ( superSample > 1 && lm->vecs != NULL ) {
		/* calc inverse supersample */
		is = 1.0f / superSample;

		/* scale the vectors and shift the origin */
		#if 1
		/* new code that works for arbitrary supersampling values */
		lm->origin -= vector3_mid( lm->vecs[ 0 ], lm->vecs[ 1 ] );
		lm->vecs[ 0 ] *= is;
		lm->vecs[ 1 ] *= is;
		lm->origin += ( lm->vecs[ 0 ] + lm->vecs[ 1 ] ) * is;
		#else
		/* old code that only worked with a value of 2 */
		lm->vecs[ 0 ] *= is;
		lm->vecs[ 1 ] *= is;
		lm->origin -=  ( lm->vecs[ 0 ] + lm->vecs[ 1 ] ) * is;
		#endif
	}

	/* allocate bsp lightmap storage */
	size = lm->w * lm->h * sizeof( *( lm->bspLuxels[ 0 ] ) );
	if ( lm->bspLuxels[ 0 ] == NULL ) {
		lm->bspLuxels[ 0 ] = safe_malloc( size );
	}
	memset( lm->bspLuxels[ 0 ], 0, size );

	/* allocate radiosity lightmap storage */
	if ( bounce ) {
		size = lm->w * lm->h * sizeof( *lm->radLuxels[ 0 ] );
		if ( lm->radLuxels[ 0 ] == NULL ) {
			lm->radLuxels[ 0 ] = safe_malloc( size );
		}
		memset( lm->radLuxels[ 0 ], 0, size );
	}

	/* allocate sampling lightmap storage */
	size = lm->sw * lm->sh * sizeof( *lm->superLuxels[ 0 ] );
	if ( lm->superLuxels[ 0 ] == NULL ) {
		lm->superLuxels[ 0 ] = safe_malloc( size );
	}
	memset( lm->superLuxels[ 0 ], 0, size );

	/* allocate origin map storage */
	size = lm->sw * lm->sh * sizeof( *lm->superOrigins );
	if ( lm->superOrigins == NULL ) {
		lm->superOrigins = safe_malloc( size );
	}
	memset( lm->superOrigins, 0, size );

	/* allocate normal map storage */
	size = lm->sw * lm->sh * sizeof( *lm->superNormals );
	if ( lm->superNormals == NULL ) {
		lm->superNormals = safe_malloc( size );
	}
	memset( lm->superNormals, 0, size );

	/* allocate dirt map storage */
	size = lm->sw * lm->sh * sizeof( *lm->superDirt );
	if ( lm->superDirt == NULL ) {
		lm->superDirt = safe_malloc( size );
	}
	memset( lm->superDirt, 0, size );

	/* allocate floodlight map storage */
	size = lm->sw * lm->sh * sizeof( *lm->superFloodLight );
	if ( lm->superFloodLight == NULL ) {
		lm->superFloodLight = safe_malloc( size );
	}
	memset( lm->superFloodLight, 0, size );

	/* allocate cluster map storage */
	size = lm->sw * lm->sh * sizeof( *lm->superClusters );
	if ( lm->superClusters == NULL ) {
		lm->superClusters = safe_malloc( size );
	}
	size = lm->sw * lm->sh;
	sc = lm->superClusters;
	for ( i = 0; i < size; i++ )
		( *sc++ ) = CLUSTER_UNMAPPED;

	/* deluxemap allocation */
	if ( deluxemap ) {
		/* allocate sampling deluxel storage */
		size = lm->sw * lm->sh * sizeof( *lm->superDeluxels );
		if ( lm->superDeluxels == NULL ) {
			lm->superDeluxels = safe_malloc( size );
		}
		memset( lm->superDeluxels, 0, size );

		/* allocate bsp deluxel storage */
		size = lm->w * lm->h * sizeof( *lm->bspDeluxels );
		if ( lm->bspDeluxels == NULL ) {
			lm->bspDeluxels = safe_malloc( size );
		}
		memset( lm->bspDeluxels, 0, size );
	}

	/* add to count */
	numLuxels += ( lm->sw * lm->sh );
}



/*
   AddPatchToRawLightmap()
   projects a lightmap for a patch surface
   since lightmap calculation for surfaces is now handled in a general way (light_ydnar.c),
   it is no longer necessary for patch verts to fall exactly on a lightmap sample
   based on AllocateLightmapForPatch()
 */

static bool AddPatchToRawLightmap( int num, rawLightmap_t *lm ){
	bspDrawVert_t       *verts, *a, *b;
	float sBasis, tBasis, s, t;
	float length, widthTable[ MAX_EXPANDED_AXIS ] = {0}, heightTable[ MAX_EXPANDED_AXIS ] = {0};


	/* patches finish a raw lightmap */
	lm->finished = true;

	/* get surface and info  */
	const bspDrawSurface_t& ds = bspDrawSurfaces[ num ];
	const surfaceInfo_t& info = surfaceInfos[ num ];

	/* make a temporary mesh from the drawsurf */
	mesh_t src;
	src.width = ds.patchWidth;
	src.height = ds.patchHeight;
	src.verts = &yDrawVerts[ ds.firstVert ];
	//%	mesh_t *subdivided = SubdivideMesh( src, 8, 512 );
	mesh_t *subdivided = SubdivideMesh2( src, info.patchIterations );

	/* fit it to the curve and remove colinear verts on rows/columns */
	PutMeshOnCurve( *subdivided );
	mesh_t *mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );

	/* find the longest distance on each row/column */
	verts = mesh->verts;
	for ( int y = 0; y < mesh->height; y++ )
	{
		for ( int x = 0; x < mesh->width; x++ )
		{
			/* get width */
			if ( x + 1 < mesh->width ) {
				a = &verts[ ( y * mesh->width ) + x ];
				b = &verts[ ( y * mesh->width ) + x + 1 ];
				value_maximize( widthTable[ x ], (float)vector3_length( a->xyz - b->xyz ) );
			}

			/* get height */
			if ( y + 1 < mesh->height ) {
				a = &verts[ ( y * mesh->width ) + x ];
				b = &verts[ ( ( y + 1 ) * mesh->width ) + x ];
				value_maximize( heightTable[ y ], (float)vector3_length( a->xyz - b->xyz ) );
			}
		}
	}

	/* determine lightmap width */
	length = 0;
	for ( int x = 0; x < ( mesh->width - 1 ); x++ )
		length += widthTable[ x ];
	lm->w = lm->sampleSize != 0 ? ceil( length / lm->sampleSize ) + 1 : 0;
	value_maximize( lm->w, ds.patchWidth );
	value_minimize( lm->w, lm->customWidth );
	sBasis = (float) ( lm->w - 1 ) / (float) ( ds.patchWidth - 1 );

	/* determine lightmap height */
	length = 0;
	for ( int y = 0; y < ( mesh->height - 1 ); y++ )
		length += heightTable[ y ];
	lm->h = lm->sampleSize != 0 ? ceil( length / lm->sampleSize ) + 1 : 0;
	value_maximize( lm->h, ds.patchHeight );
	value_minimize( lm->h, lm->customHeight );
	tBasis = (float) ( lm->h - 1 ) / (float) ( ds.patchHeight - 1 );

	/* free the temporary mesh */
	FreeMesh( mesh );

	/* set the lightmap texture coordinates in yDrawVerts */
	lm->wrap[ 0 ] = true;
	lm->wrap[ 1 ] = true;
	verts = &yDrawVerts[ ds.firstVert ];
	for ( int y = 0; y < ds.patchHeight; y++ )
	{
		t = ( tBasis * y ) + 0.5f;
		for ( int x = 0; x < ds.patchWidth; x++ )
		{
			s = ( sBasis * x ) + 0.5f;
			verts[ ( y * ds.patchWidth ) + x ].lightmap[ 0 ][ 0 ] = s * superSample;
			verts[ ( y * ds.patchWidth ) + x ].lightmap[ 0 ][ 1 ] = t * superSample;

			if ( y == 0 && !VectorCompare( verts[ x ].xyz, verts[ ( ( ds.patchHeight - 1 ) * ds.patchWidth ) + x ].xyz ) ) {
				lm->wrap[ 1 ] = false;
			}
		}

		if ( !VectorCompare( verts[ ( y * ds.patchWidth ) ].xyz, verts[ ( y * ds.patchWidth ) + ( ds.patchWidth - 1 ) ].xyz ) ) {
			lm->wrap[ 0 ] = false;
		}
	}

	/* debug code: */
	//%	Sys_Printf( "wrap S: %d wrap T: %d\n", lm->wrap[ 0 ], lm->wrap[ 1 ] );
	//% if( lm->w > ( ds.lightmapWidth & 0xFF ) || lm->h > ( ds.lightmapHeight & 0xFF ) )
	//%		Sys_Printf( "Patch lightmap: (%3d %3d) > (%3d, %3d)\n", lm->w, lm->h, ds.lightmapWidth & 0xFF, ds.lightmapHeight & 0xFF );
	//% ds.lightmapWidth = lm->w | ( ds.lightmapWidth & 0xFFFF0000 );
	//% ds.lightmapHeight = lm->h | ( ds.lightmapHeight & 0xFFFF0000 );

	/* add to counts */
	numPatchesLightmapped++;

	/* return */
	return true;
}



/*
   AddSurfaceToRawLightmap()
   projects a lightmap for a surface
   based on AllocateLightmapForSurface()
 */

static bool AddSurfaceToRawLightmap( int num, rawLightmap_t *lm ){
	bspDrawSurface_t    *ds, *ds2;
	surfaceInfo_t       *info;
	int num2, n, i, axisNum;
	float s, t, len, sampleSize;
	Vector3 mins, maxs, origin, faxis, size, delta, normalized, vecs[ 2 ];
	Plane3f plane;
	bspDrawVert_t       *verts;


	/* get surface and info  */
	ds = &bspDrawSurfaces[ num ];
	info = &surfaceInfos[ num ];

	/* add the surface to the raw lightmap */
	lightSurfaces[ numLightSurfaces++ ] = num;
	lm->numLightSurfaces++;

	/* does this raw lightmap already have any surfaces? */
	if ( lm->numLightSurfaces > 1 ) {
		/* surface and raw lightmap must have the same lightmap projection axis */
		if ( !VectorCompare( info->axis, lm->axis ) ) {
			return false;
		}

		/* match identical attributes */
		if ( info->sampleSize != lm->sampleSize ||
		     info->entityNum != lm->entityNum ||
		     info->recvShadows != lm->recvShadows ||
		     info->si->lmCustomWidth != lm->customWidth ||
		     info->si->lmCustomHeight != lm->customHeight ||
		     info->si->lmBrightness != lm->brightness ||
		     info->si->lmFilterRadius != lm->filterRadius ||
		     info->si->splotchFix != lm->splotchFix ) {
			return false;
		}

		/* surface bounds must intersect with raw lightmap bounds */
		if( !info->minmax.test( lm->minmax ) ){
			return false;
		}

		/* plane check (fixme: allow merging of nonplanars) */
		if ( !info->si->lmMergable ) {
			if ( info->plane == NULL || lm->plane == NULL ) {
				return false;
			}

			/* compare planes */
			if( !vector3_equal_epsilon( info->plane->normal(), lm->plane->normal(), EQUAL_EPSILON )
			 || !float_equal_epsilon( info->plane->dist(), lm->plane->dist(), EQUAL_EPSILON ) ){
				return false;
			}
		}

		/* debug code hacking */
		//%	if( lm->numLightSurfaces > 1 )
		//%		return false;
	}

	/* set plane */
	if ( info->plane == NULL ) {
		lm->plane = NULL;
	}

	/* add surface to lightmap bounds */
	lm->minmax.extend( info->minmax );

	/* check to see if this is a non-planar patch */
	if ( ds->surfaceType == MST_PATCH &&
	     lm->axis == g_vector3_identity ) {
		return AddPatchToRawLightmap( num, lm );
	}

	/* start with initially requested sample size */
	sampleSize = lm->sampleSize;

	/* round to the lightmap resolution */
	for ( i = 0; i < 3; i++ )
	{
		mins[ i ] = sampleSize * floor( lm->minmax.mins[ i ] / sampleSize );
		maxs[ i ] = sampleSize * ceil( lm->minmax.maxs[ i ] / sampleSize );
		size[ i ] = ( maxs[ i ] - mins[ i ] ) / sampleSize + 1.0f;

		/* hack (god this sucks) */
		if ( size[ i ] > lm->customWidth || size[ i ] > lm->customHeight  || ( lmLimitSize && size[i] > lmLimitSize ) ) {
			i = -1;
			sampleSize += 1.0f;
		}
	}

	if ( sampleSize != lm->sampleSize && lmLimitSize == 0 ){
		if ( debugSampleSize == 1 || lm->customWidth > 128 ){
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: surface at (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) too large for desired samplesize/lightmapsize/lightmapscale combination, increased samplesize from %d to %d\n",
			             info->minmax.mins[0],
			             info->minmax.mins[1],
			             info->minmax.mins[2],
			             info->minmax.maxs[0],
			             info->minmax.maxs[1],
			             info->minmax.maxs[2],
			             lm->sampleSize,
			             (int) sampleSize );
		}
		else if ( debugSampleSize == 0 ){
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: surface at (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) too large for desired samplesize/lightmapsize/lightmapscale combination, increased samplesize from %d to %d\n",
			             info->minmax.mins[0],
			             info->minmax.mins[1],
			             info->minmax.mins[2],
			             info->minmax.maxs[0],
			             info->minmax.maxs[1],
			             info->minmax.maxs[2],
			             lm->sampleSize,
			             (int) sampleSize );
			debugSampleSize--;
		}
		else{
			debugSampleSize--;
		}
	}

	/* set actual sample size */
	lm->actualSampleSize = sampleSize;

	/* fixme: copy rounded mins/maxes to lightmap record? */
	if ( lm->plane == NULL ) {
		lm->minmax = { mins, maxs };
	}

	/* set lightmap origin */
	origin = lm->minmax.mins;

	/* make absolute axis */
	faxis[ 0 ] = fabs( lm->axis[ 0 ] );
	faxis[ 1 ] = fabs( lm->axis[ 1 ] );
	faxis[ 2 ] = fabs( lm->axis[ 2 ] );

	/* clear out lightmap vectors */
	memset( vecs, 0, sizeof( vecs ) );

	/* classify the plane (x y or z major) (ydnar: biased to z axis projection) */
	if ( faxis[ 2 ] >= faxis[ 0 ] && faxis[ 2 ] >= faxis[ 1 ] ) {
		axisNum = 2;
		lm->w = size[ 0 ];
		lm->h = size[ 1 ];
		vecs[ 0 ][ 0 ] = 1.0f / sampleSize;
		vecs[ 1 ][ 1 ] = 1.0f / sampleSize;
	}
	else if ( faxis[ 0 ] >= faxis[ 1 ] && faxis[ 0 ] >= faxis[ 2 ] ) {
		axisNum = 0;
		lm->w = size[ 1 ];
		lm->h = size[ 2 ];
		vecs[ 0 ][ 1 ] = 1.0f / sampleSize;
		vecs[ 1 ][ 2 ] = 1.0f / sampleSize;
	}
	else
	{
		axisNum = 1;
		lm->w = size[ 0 ];
		lm->h = size[ 2 ];
		vecs[ 0 ][ 0 ] = 1.0f / sampleSize;
		vecs[ 1 ][ 2 ] = 1.0f / sampleSize;
	}

	/* check for bogus axis */
	if ( faxis[ axisNum ] == 0.0f ) {
		Sys_Warning( "ProjectSurfaceLightmap: Chose a 0 valued axis\n" );
		lm->w = lm->h = 0;
		return false;
	}

	/* store the axis number in the lightmap */
	lm->axisNum = axisNum;

	/* walk the list of surfaces on this raw lightmap */
	for ( n = 0; n < lm->numLightSurfaces; n++ )
	{
		/* get surface */
		num2 = lightSurfaces[ lm->firstLightSurface + n ];
		ds2 = &bspDrawSurfaces[ num2 ];
		verts = &yDrawVerts[ ds2->firstVert ];

		/* set the lightmap texture coordinates in yDrawVerts in [0, superSample * lm->customWidth] space */
		for ( i = 0; i < ds2->numVerts; i++ )
		{
			delta = verts[ i ].xyz - origin;
			s = vector3_dot( delta, vecs[ 0 ] ) + 0.5f;
			t = vector3_dot( delta, vecs[ 1 ] ) + 0.5f;
			verts[ i ].lightmap[ 0 ][ 0 ] = s * superSample;
			verts[ i ].lightmap[ 0 ][ 1 ] = t * superSample;

			if ( s > (float) lm->w || t > (float) lm->h ) {
				Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: Lightmap texture coords out of range: S %1.4f > %3d || T %1.4f > %3d\n",
				             s, lm->w, t, lm->h );
			}
		}
	}

	/* get first drawsurface */
	num2 = lightSurfaces[ lm->firstLightSurface ];
	ds2 = &bspDrawSurfaces[ num2 ];
	verts = &yDrawVerts[ ds2->firstVert ];

	/* calculate lightmap origin */
	if ( vector3_length( ds2->lightmapVecs[ 2 ] ) ) {
		plane.normal() = ds2->lightmapVecs[ 2 ];
	}
	else{
		plane.normal() = lm->axis;
	}
	plane.dist() = vector3_dot( verts[ 0 ].xyz, plane.normal() );

	lm->origin = origin;
	lm->origin[ axisNum ] -= plane3_distance_to_point( plane, lm->origin ) / plane.normal()[ axisNum ];

	/* legacy support */
	ds->lightmapOrigin = lm->origin;

	/* for planar surfaces, create lightmap vectors for st->xyz conversion */
	if ( vector3_length( ds->lightmapVecs[ 2 ] ) || 1 ) {  /* ydnar: can't remember what exactly i was thinking here... */
		/* allocate space for the vectors */
		lm->vecs = safe_calloc( 3 * sizeof( *lm->vecs ) );
		lm->vecs[ 2 ] = ds->lightmapVecs[ 2 ];

		/* project stepped lightmap blocks and subtract to get planevecs */
		for ( i = 0; i < 2; i++ )
		{
			normalized = vecs[ i ];
			len = VectorNormalize( normalized );
			lm->vecs[ i ] = normalized * ( 1.0 / len );
			lm->vecs[ i ][ axisNum ] -= vector3_dot( lm->vecs[ i ], plane.normal() ) / plane.normal()[ axisNum ];
		}
	}
	else
	{
		/* lightmap vectors are useless on a non-planar surface */
		lm->vecs = NULL;
	}

	/* add to counts */
	if ( ds->surfaceType == MST_PATCH ) {
		numPatchesLightmapped++;
		if ( lm->plane != NULL ) {
			numPlanarPatchesLightmapped++;
		}
	}
	else
	{
		if ( lm->plane != NULL ) {
			numPlanarsLightmapped++;
		}
		else{
			numNonPlanarsLightmapped++;
		}
	}

	/* return */
	return true;
}



/*
   CompareSurfaceInfo()
   compare functor for std::sort()
 */

struct CompareSurfaceInfo
{
	bool operator()( const int a, const int b ) const {
		/* get surface info */
		const surfaceInfo_t& aInfo = surfaceInfos[ a ];
		const surfaceInfo_t& bInfo = surfaceInfos[ b ];

		/* model first */
		if ( aInfo.modelindex < bInfo.modelindex ) {
			return false;
		}
		else if ( aInfo.modelindex > bInfo.modelindex ) {
			return true;
		}

		/* then lightmap status */
		if ( aInfo.hasLightmap < bInfo.hasLightmap ) {
			return false;
		}
		else if ( aInfo.hasLightmap > bInfo.hasLightmap ) {
			return true;
		}

		/* 27: then shader! */
		if ( aInfo.si < bInfo.si ) {
			return false;
		}
		else if ( aInfo.si > bInfo.si ) {
			return true;
		}


		/* then lightmap sample size */
		if ( aInfo.sampleSize < bInfo.sampleSize ) {
			return false;
		}
		else if ( aInfo.sampleSize > bInfo.sampleSize ) {
			return true;
		}

		/* then lightmap axis */
		for ( int i = 0; i < 3; i++ )
		{
			if ( aInfo.axis[ i ] < bInfo.axis[ i ] ) {
				return false;
			}
			else if ( aInfo.axis[ i ] > bInfo.axis[ i ] ) {
				return true;
			}
		}

		/* then plane */
		if ( aInfo.plane == NULL && bInfo.plane != NULL ) {
			return false;
		}
		else if ( aInfo.plane != NULL && bInfo.plane == NULL ) {
			return true;
		}
		else if ( aInfo.plane != NULL && bInfo.plane != NULL ) {
			for ( int i = 0; i < 3; i++ )
			{
				if ( aInfo.plane->normal()[ i ] < bInfo.plane->normal()[ i ] ) {
					return false;
				}
				else if ( aInfo.plane->normal()[ i ] > bInfo.plane->normal()[ i ] ) {
					return true;
				}
			}
			if ( aInfo.plane->dist() < bInfo.plane->dist() ) {
				return false;
			}
			else if ( aInfo.plane->dist() > bInfo.plane->dist() ) {
				return true;
			}

		}

		/* then position in world */
		for ( int i = 0; i < 3; i++ )
		{
			if ( aInfo.minmax.mins[ i ] < bInfo.minmax.mins[ i ] ) {
				return false;
			}
			else if ( aInfo.minmax.mins[ i ] > bInfo.minmax.mins[ i ] ) {
				return true;
			}
		}

		/* these are functionally identical (this should almost never happen) */
		return false;
	}
};



/*
   SetupSurfaceLightmaps()
   allocates lightmaps for every surface in the bsp that needs one
   this depends on yDrawVerts being allocated
 */

void SetupSurfaceLightmaps(){
	int i, j, k, s, num, num2;
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info, *info2;
	rawLightmap_t       *lm;
	bool added;
	const int numBSPDrawSurfaces = bspDrawSurfaces.size();


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SetupSurfaceLightmaps ---\n" );

	/* determine supersample amount */
	value_maximize( superSample, 1 );
	if ( superSample > 8 ) {
		Sys_Warning( "Insane supersampling amount (%d) detected.\n", superSample );
		superSample = 8;
	}

	/* clear map bounds */
	g_mapMinmax.clear();

	/* allocate a list of surface clusters */
	numSurfaceClusters = 0;
	maxSurfaceClusters = bspLeafSurfaces.size();
	surfaceClusters = safe_calloc( maxSurfaceClusters * sizeof( *surfaceClusters ) );

	/* allocate a list for per-surface info */
	surfaceInfos = safe_calloc( numBSPDrawSurfaces * sizeof( *surfaceInfos ) );
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
		surfaceInfos[ i ].childSurfaceNum = -1;

	/* allocate a list of surface indexes to be sorted */
	int *sortSurfaces = safe_calloc( numBSPDrawSurfaces * sizeof( int ) );

	/* walk each model in the bsp */
	for ( const bspModel_t& model : bspModels )
	{
		/* walk the list of surfaces in this model and fill out the info structs */
		for ( j = 0; j < model.numBSPSurfaces; j++ )
		{
			/* make surface index */
			num = model.firstBSPSurface + j;

			/* copy index to sort list */
			sortSurfaces[ num ] = num;

			/* get surface and info */
			ds = &bspDrawSurfaces[ num ];
			info = &surfaceInfos[ num ];

			/* basic setup */
			info->modelindex = i;
			info->lm = NULL;
			info->plane = NULL;
			info->firstSurfaceCluster = numSurfaceClusters;

			{ /* get extra data */
				const surfaceExtra_t& se = GetSurfaceExtra( num );
				info->si = se.si;
				if ( info->si == NULL ) {
					info->si = ShaderInfoForShader( bspShaders[ ds->shaderNum ].shader );
				}
				info->parentSurfaceNum = se.parentSurfaceNum;
				info->entityNum = se.entityNum;
				info->castShadows = se.castShadows;
				info->recvShadows = se.recvShadows;
				info->sampleSize = se.sampleSize;
				info->longestCurve = se.longestCurve;
				info->patchIterations = IterationsForCurve( info->longestCurve, patchSubdivisions );
				info->axis = se.lightmapAxis;
			}

			/* mark parent */
			if ( info->parentSurfaceNum >= 0 ) {
				surfaceInfos[ info->parentSurfaceNum ].childSurfaceNum = j;
			}

			/* determine surface bounds */
			info->minmax.clear();
			for ( k = 0; k < ds->numVerts; k++ )
			{
				g_mapMinmax.extend( yDrawVerts[ ds->firstVert + k ].xyz );
				info->minmax.extend( yDrawVerts[ ds->firstVert + k ].xyz );
			}

			/* find all the bsp clusters the surface falls into */
			for ( const bspLeaf_t& leaf : bspLeafs )
			{
				/* test bbox */
				if( !leaf.minmax.test( info->minmax ) ) {
					continue;
				}

				/* test leaf surfaces */
				for ( s = 0; s < leaf.numBSPLeafSurfaces; s++ )
				{
					if ( bspLeafSurfaces[ leaf.firstBSPLeafSurface + s ] == num ) {
						if ( numSurfaceClusters >= maxSurfaceClusters ) {
							Error( "maxSurfaceClusters exceeded" );
						}
						surfaceClusters[ numSurfaceClusters ] = leaf.cluster;
						numSurfaceClusters++;
						info->numSurfaceClusters++;
					}
				}
			}

			/* determine if surface is planar */
			if ( vector3_length( ds->lightmapVecs[ 2 ] ) != 0.0f ) {
				/* make a plane */
				info->plane = safe_malloc( sizeof( *( info->plane ) ) );
				info->plane->normal() = ds->lightmapVecs[ 2 ];
				info->plane->dist() = vector3_dot( yDrawVerts[ ds->firstVert ].xyz, info->plane->normal() );
			}

			/* determine if surface requires a lightmap */
			if ( ds->surfaceType == MST_TRIANGLE_SOUP ||
			     ds->surfaceType == MST_FOLIAGE ||
			     ( info->si->compileFlags & C_VERTEXLIT ) ||
			     noLightmaps ) {
				numSurfsVertexLit++;
			}
			else
			{
				numSurfsLightmapped++;
				info->hasLightmap = true;
			}
		}
	}

	/* sort the surfaces info list */
	std::sort( sortSurfaces, sortSurfaces + numBSPDrawSurfaces, CompareSurfaceInfo() );

	/* allocate a list of surfaces that would go into raw lightmaps */
	numLightSurfaces = 0;
	lightSurfaces = safe_calloc( numSurfsLightmapped * sizeof( int ) );

	/* allocate a list of raw lightmaps */
	numRawLightmaps = 0;
	rawLightmaps = safe_calloc( numSurfsLightmapped * sizeof( *rawLightmaps ) );

	/* walk the list of sorted surfaces */
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		/* get info and attempt early out */
		num = sortSurfaces[ i ];
		info = &surfaceInfos[ num ];
		if ( !info->hasLightmap || info->lm != NULL || info->parentSurfaceNum >= 0 ) {
			continue;
		}

		/* allocate a new raw lightmap */
		lm = &rawLightmaps[ numRawLightmaps ];
		numRawLightmaps++;

		/* set it up */
		lm->splotchFix = info->si->splotchFix;
		lm->firstLightSurface = numLightSurfaces;
		lm->numLightSurfaces = 0;
		/* vortex: multiply lightmap sample size by -samplescale */
		if ( sampleScale > 0 ) {
			lm->sampleSize = info->sampleSize * sampleScale;
		}
		else{
			lm->sampleSize = info->sampleSize;
		}
		lm->actualSampleSize = lm->sampleSize;
		lm->entityNum = info->entityNum;
		lm->recvShadows = info->recvShadows;
		lm->brightness = info->si->lmBrightness;
		lm->filterRadius = info->si->lmFilterRadius;
		lm->floodlightRGB = info->si->floodlightRGB;
		lm->floodlightDistance = info->si->floodlightDistance;
		lm->floodlightIntensity = info->si->floodlightIntensity;
		lm->floodlightDirectionScale = info->si->floodlightDirectionScale;
		lm->axis = info->axis;
		lm->plane = info->plane;
		lm->minmax = info->minmax;

		lm->customWidth = info->si->lmCustomWidth;
		lm->customHeight = info->si->lmCustomHeight;

		/* add the surface to the raw lightmap */
		AddSurfaceToRawLightmap( num, lm );
		info->lm = lm;

		/* do an exhaustive merge */
		added = true;
		while ( added )
		{
			/* walk the list of surfaces again */
			added = false;
			for ( j = i + 1; j < numBSPDrawSurfaces && !lm->finished; j++ )
			{
				/* get info and attempt early out */
				num2 = sortSurfaces[ j ];
				info2 = &surfaceInfos[ num2 ];
				if ( !info2->hasLightmap || info2->lm != NULL ) {
					continue;
				}

				/* add the surface to the raw lightmap */
				if ( AddSurfaceToRawLightmap( num2, lm ) ) {
					info2->lm = lm;
					added = true;
				}
				else
				{
					/* back up one */
					lm->numLightSurfaces--;
					numLightSurfaces--;
				}
			}
		}

		/* finish the lightmap and allocate the various buffers */
		FinishRawLightmap( lm );
	}

	if ( debugSampleSize < -1 ){
		Sys_FPrintf( SYS_WRN | SYS_VRBflag, "+%d similar occurrences;\t-debugsamplesize to show ones\n", -debugSampleSize - 1 );
	}

	/* allocate vertex luxel storage */
	for ( k = 0; k < MAX_LIGHTMAPS; k++ )
	{
		vertexLuxels[ k ] = safe_calloc( bspDrawVerts.size() * sizeof( *vertexLuxels[ 0 ] ) );
		radVertexLuxels[ k ] = safe_calloc( bspDrawVerts.size() * sizeof( *radVertexLuxels[ 0 ] ) );
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d surfaces\n", numBSPDrawSurfaces );
	Sys_FPrintf( SYS_VRB, "%9d raw lightmaps\n", numRawLightmaps );
	Sys_FPrintf( SYS_VRB, "%9d surfaces vertex lit\n", numSurfsVertexLit );
	Sys_FPrintf( SYS_VRB, "%9d surfaces lightmapped\n", numSurfsLightmapped );
	Sys_FPrintf( SYS_VRB, "%9d planar surfaces lightmapped\n", numPlanarsLightmapped );
	Sys_FPrintf( SYS_VRB, "%9d non-planar surfaces lightmapped\n", numNonPlanarsLightmapped );
	Sys_FPrintf( SYS_VRB, "%9d patches lightmapped\n", numPatchesLightmapped );
	Sys_FPrintf( SYS_VRB, "%9d planar patches lightmapped\n", numPlanarPatchesLightmapped );
}



/*
   StitchSurfaceLightmaps()
   stitches lightmap edges
   2002-11-20 update: use this func only for stitching nonplanar patch lightmap seams
 */

#define MAX_STITCH_CANDIDATES   32
#define MAX_STITCH_LUXELS       64

void StitchSurfaceLightmaps(){
	int i, j, x, y, x2, y2,
	    numStitched, numCandidates, numLuxels, fOld;
	rawLightmap_t   *lm, *a, *b, *c[ MAX_STITCH_CANDIDATES ];
	float           sampleSize, totalColor;


	/* disabled for now */
	return;

	/* note it */
	Sys_Printf( "--- StitchSurfaceLightmaps ---\n" );

	/* init pacifier */
	fOld = -1;
	Timer timer;

	/* walk the list of raw lightmaps */
	numStitched = 0;
	for ( i = 0; i < numRawLightmaps; i++ )
	{
		/* print pacifier */
		if ( const int f = 10 * i / numRawLightmaps; f != fOld ) {
			fOld = f;
			Sys_Printf( "%i...", f );
		}

		/* get lightmap a */
		a = &rawLightmaps[ i ];

		/* walk rest of lightmaps */
		numCandidates = 0;
		for ( j = i + 1; j < numRawLightmaps && numCandidates < MAX_STITCH_CANDIDATES; j++ )
		{
			/* get lightmap b */
			b = &rawLightmaps[ j ];

			/* test bounding box */
			if ( !a->minmax.test( b->minmax ) ) {
				continue;
			}

			/* add candidate */
			c[ numCandidates++ ] = b;
		}

		/* walk luxels */
		for ( y = 0; y < a->sh; y++ )
		{
			for ( x = 0; x < a->sw; x++ )
			{
				/* ignore unmapped/unlit luxels */
				lm = a;
				if ( lm->getSuperCluster( x, y ) == CLUSTER_UNMAPPED ) {
					continue;
				}
				SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );
				if ( luxel.count <= 0.0f ) {
					continue;
				}

				/* get particulars */
				const Vector3& origin = lm->getSuperOrigin( x, y );
				const Vector3& normal = lm->getSuperNormal( x, y );

				/* walk candidate list */
				for ( j = 0; j < numCandidates; j++ )
				{
					/* get candidate */
					b = c[ j ];
					lm = b;

					/* set samplesize to the smaller of the pair */
					sampleSize = 0.5f * std::min( a->actualSampleSize, b->actualSampleSize );

					/* test bounding box */
					if ( !b->minmax.test( origin, sampleSize ) ) {
						continue;
					}

					/* walk candidate luxels */
					Vector3 average( 0 );
					numLuxels = 0;
					totalColor = 0.0f;
					for ( y2 = 0; y2 < b->sh && numLuxels < MAX_STITCH_LUXELS; y2++ )
					{
						for ( x2 = 0; x2 < b->sw && numLuxels < MAX_STITCH_LUXELS; x2++ )
						{
							/* ignore same luxels */
							if ( a == b && abs( x - x2 ) <= 1 && abs( y - y2 ) <= 1 ) {
								continue;
							}

							/* ignore unmapped/unlit luxels */
							if ( lm->getSuperCluster( x2, y2 ) == CLUSTER_UNMAPPED ) {
								continue;
							}
							const SuperLuxel& luxel2 = lm->getSuperLuxel( 0, x2, y2 );
							if ( luxel2.count <= 0.0f ) {
								continue;
							}

							/* get particulars */
							const Vector3& origin2 = lm->getSuperOrigin( x2, y2 );
							const Vector3& normal2 = lm->getSuperNormal( x2, y2 );

							/* test normal */
							if ( vector3_dot( normal, normal2 ) < 0.5f ) {
								continue;
							}

							/* test bounds */
							if ( !vector3_equal_epsilon( origin, origin2, sampleSize ) ) {
								continue;
							}

							/* add luxel */
							//%	luxel2.value = { 255, 0, 255 };
							numLuxels++;
							average += luxel2.value;
							totalColor += luxel2.count;
						}
					}

					/* early out */
					if ( numLuxels == 0 ) {
						continue;
					}

					/* scale average */
					luxel.value = average * ( 1.0f / totalColor );
					luxel.count = 1.0f;
					numStitched++;
				}
			}
		}
	}

	/* emit statistics */
	Sys_Printf( " (%i)\n", int( timer.elapsed_sec() ) );
	Sys_FPrintf( SYS_VRB, "%9d luxels stitched\n", numStitched );
}



/*
   CompareBSPLuxels()
   compares two surface lightmaps' bsp luxels, ignoring occluded luxels
 */

#define SOLID_EPSILON       0.0625f
#define LUXEL_TOLERANCE     0.0025
#define LUXEL_COLOR_FRAC    0.001302083 /* 1 / 3 / 256 */

static bool CompareBSPLuxels( rawLightmap_t *a, int aNum, rawLightmap_t *b, int bNum ){
	/* styled lightmaps will never be collapsed to non-styled lightmaps when there is _minlight */
	if ( minLight != g_vector3_identity &&
	     ( aNum == 0 ) != ( bNum == 0 ) ) {
		return false;
	}

	/* basic tests */
	if ( a->customWidth != b->customWidth || a->customHeight != b->customHeight ||
	     a->brightness != b->brightness ||
	     a->solid[ aNum ] != b->solid[ bNum ] ||
	     a->bspLuxels[ aNum ] == NULL || b->bspLuxels[ bNum ] == NULL ) {
		return false;
	}

	/* compare solid color lightmaps */
	if ( a->solid[ aNum ] && b->solid[ bNum ] ) {
		/* compare color */
		if ( !vector3_equal_epsilon( a->solidColor[ aNum ], b->solidColor[ bNum ], SOLID_EPSILON ) ) {
			return false;
		}

		/* okay */
		return true;
	}

	/* compare nonsolid lightmaps */
	if ( a->w != b->w || a->h != b->h ) {
		return false;
	}

	/* compare luxels */
	double delta = 0.0;
	double total = 0.0;
	for ( int y = 0; y < a->h; y++ )
	{
		for ( int x = 0; x < a->w; x++ )
		{
			/* increment total */
			total += 1.0;

			/* get luxels */
			const Vector3& aLuxel = a->getBspLuxel( aNum, x, y );
			const Vector3& bLuxel = b->getBspLuxel( bNum, x, y );

			/* ignore unused luxels */
			if ( aLuxel[ 0 ] < 0 || bLuxel[ 0 ] < 0 ) {
				continue;
			}

			/* 2003-09-27: compare individual luxels */
			if ( !vector3_equal_epsilon( aLuxel, bLuxel, 3.f ) ) {
				return false;
			}

			/* compare (fixme: take into account perceptual differences) */
			Vector3 diff = aLuxel - bLuxel;
			for( int i = 0; i < 3; ++i )
				diff[i] = fabs( diff[i] );
			delta += vector3_dot( diff, Vector3( LUXEL_COLOR_FRAC ) );

			/* is the change too high? */
			if ( total > 0.0 && ( ( delta / total ) > LUXEL_TOLERANCE ) ) {
				return false;
			}
		}
	}

	/* made it this far, they must be identical (or close enough) */
	return true;
}



/*
   MergeBSPLuxels()
   merges two surface lightmaps' bsp luxels, overwriting occluded luxels
 */

static bool MergeBSPLuxels( rawLightmap_t *a, int aNum, rawLightmap_t *b, int bNum ){
	int x, y;
	Vector3 luxel;


	/* basic tests */
	if ( a->customWidth != b->customWidth || a->customHeight != b->customHeight ||
	     a->brightness != b->brightness ||
	     a->solid[ aNum ] != b->solid[ bNum ] ||
	     a->bspLuxels[ aNum ] == NULL || b->bspLuxels[ bNum ] == NULL ) {
		return false;
	}

	/* compare solid lightmaps */
	if ( a->solid[ aNum ] && b->solid[ bNum ] ) {
		/* average */
		luxel = vector3_mid( a->solidColor[ aNum ], b->solidColor[ bNum ] );

		/* copy to both */
		a->solidColor[ aNum ] = luxel;
		b->solidColor[ bNum ] = luxel;

		/* return to sender */
		return true;
	}

	/* compare nonsolid lightmaps */
	if ( a->w != b->w || a->h != b->h ) {
		return false;
	}

	/* merge luxels */
	for ( y = 0; y < a->h; y++ )
	{
		for ( x = 0; x < a->w; x++ )
		{
			/* get luxels */
			Vector3& aLuxel = a->getBspLuxel( aNum, x, y );
			Vector3& bLuxel = b->getBspLuxel( bNum, x, y );

			/* handle occlusion mismatch */
			if ( aLuxel[ 0 ] < 0.0f ) {
				aLuxel = bLuxel;
			}
			else if ( bLuxel[ 0 ] < 0.0f ) {
				bLuxel = aLuxel;
			}
			else
			{
				/* average */
				luxel = vector3_mid( aLuxel, bLuxel );

				/* debugging code */
				//%	luxel[ 2 ] += 64.0f;

				/* copy to both */
				aLuxel = luxel;
				bLuxel = luxel;
			}
		}
	}

	/* done */
	return true;
}



/*
   ApproximateLuxel()
   determines if a single luxel is can be approximated with the interpolated vertex rgba
 */

static bool ApproximateLuxel( rawLightmap_t *lm, const bspDrawVert_t& dv ){
	/* find luxel xy coords */
	const int x = std::clamp( int( dv.lightmap[ 0 ][ 0 ] / superSample ), 0, lm->w - 1 );
	const int y = std::clamp( int( dv.lightmap[ 0 ][ 1 ] / superSample ), 0, lm->h - 1 );

	/* walk list */
	for ( int lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
	{
		/* early out */
		if ( lm->styles[ lightmapNum ] == LS_NONE ) {
			continue;
		}

		/* get luxel */
		const Vector3& luxel = lm->getBspLuxel( lightmapNum, x, y );

		/* ignore occluded luxels */
		if ( luxel[ 0 ] < 0.0f || luxel[ 1 ] < 0.0f || luxel[ 2 ] < 0.0f ) {
			return true;
		}

		/* copy, set min color and compare */
		Vector3 color = luxel;
		Vector3 vertexColor = dv.color[ 0 ].rgb();

		/* styles are not affected by minlight */
		if ( lightmapNum == 0 ) {
			for ( int i = 0; i < 3; i++ )
			{
				/* set min color */
				value_maximize( color[ i ], minLight[ i ] );
				value_maximize( vertexColor[ i ], minLight[ i ] ); /* note NOT minVertexLight */
			}
		}

		/* set to bytes */
		Color4b cb( ColorToBytes( color, 1.0f ), 0 );
		Color4b vcb( ColorToBytes( vertexColor, 1.0f ), 0 );

		/* compare */
		for ( int i = 0; i < 3; i++ )
		{
			if ( abs( cb[ i ] - vcb[ i ] ) > approximateTolerance ) {
				return false;
			}
		}
	}

	/* close enough for the girls i date */
	return true;
}



/*
   ApproximateTriangle()
   determines if a single triangle can be approximated with vertex rgba
 */

static bool ApproximateTriangle_r( rawLightmap_t *lm, const TriRef& tri ){
	/* approximate the vertexes */
	if ( !ApproximateLuxel( lm, *tri[ 0 ] )
	  || !ApproximateLuxel( lm, *tri[ 1 ] )
	  || !ApproximateLuxel( lm, *tri[ 2 ] ) )
		return false;

	/* subdivide calc */
	int max = -1;
	{
		/* find the longest edge and split it */
		float maxDist = 0;
		for ( int i = 0; i < 3; i++ )
		{
			const float dist = vector2_length( tri[ i ]->lightmap[ 0 ] - tri[ ( i + 1 ) % 3 ]->lightmap[ 0 ] );
			if ( dist > maxDist ) {
				maxDist = dist;
				max = i;
			}
		}

		/* try to early out */
		if ( max < 0 || maxDist < subdivideThreshold ) {
			return true;
		}
	}

	/* split the longest edge and map it */
	const bspDrawVert_t mid = LerpDrawVert( *tri[ max ], *tri[ ( max + 1 ) % 3 ] );
	if ( !ApproximateLuxel( lm, mid ) ) {
		return false;
	}

	/* recurse to first triangle */
	TriRef tri2 = tri;
	tri2[ max ] = &mid;
	if ( !ApproximateTriangle_r( lm, tri2 ) ) {
		return false;
	}

	/* recurse to second triangle */
	tri2 = tri;
	tri2[ ( max + 1 ) % 3 ] = &mid;
	return ApproximateTriangle_r( lm, tri2 );
}



/*
   ApproximateLightmap()
   determines if a raw lightmap can be approximated sufficiently with vertex colors
 */

static bool ApproximateLightmap( rawLightmap_t *lm ){
	/* approximating? */
	if ( approximateTolerance <= 0 ) {
		return false;
	}

	/* test for jmonroe */
	#if 0
	/* don't approx lightmaps with styled twins */
	if ( lm->numStyledTwins > 0 ) {
		return false;
	}

	/* don't approx lightmaps with styles */
	for ( int i = 1; i < MAX_LIGHTMAPS; i++ )
	{
		if ( lm->styles[ i ] != LS_NONE ) {
			return false;
		}
	}
	#endif

	/* assume reduced until shadow detail is found */
	bool approximated = true;

	/* walk the list of surfaces on this raw lightmap */
	for ( int n = 0; n < lm->numLightSurfaces; n++ )
	{
		/* get surface */
		const int num = lightSurfaces[ lm->firstLightSurface + n ];
		const bspDrawSurface_t& ds = bspDrawSurfaces[ num ];
		surfaceInfo_t& info = surfaceInfos[ num ];

		/* assume not-reduced initially */
		info.approximated = false;

		/* bail if lightmap doesn't match up */
		if ( info.lm != lm ) {
			continue;
		}

		/* bail if not vertex lit */
		if ( info.si->noVertexLight ) {
			continue;
		}

		/* assume that surfaces whose bounding boxes is smaller than 2x samplesize will be forced to vertex */
		if ( ( info.minmax.maxs[ 0 ] - info.minmax.mins[ 0 ] ) <= ( 2.0f * info.sampleSize ) &&
		     ( info.minmax.maxs[ 1 ] - info.minmax.mins[ 1 ] ) <= ( 2.0f * info.sampleSize ) &&
		     ( info.minmax.maxs[ 2 ] - info.minmax.mins[ 2 ] ) <= ( 2.0f * info.sampleSize ) ) {
			info.approximated = true;
			numSurfsVertexForced++;
			continue;
		}

		/* handle the triangles */
		switch ( ds.surfaceType )
		{
		case MST_PLANAR:
		{
			/* get verts */
			const bspDrawVert_t *verts = &yDrawVerts[ ds.firstVert ];

			/* map the triangles */
			info.approximated = true;
			for ( int i = 0; i < ds.numIndexes && info.approximated; i += 3 )
			{
				info.approximated = ApproximateTriangle_r( lm, TriRef{
					&verts[ bspDrawIndexes[ ds.firstIndex + i + 0 ] ],
					&verts[ bspDrawIndexes[ ds.firstIndex + i + 1 ] ],
					&verts[ bspDrawIndexes[ ds.firstIndex + i + 2 ] ] } );
			}
			break;
		}
		case MST_PATCH:
		{
			/* make a mesh from the drawsurf */
			mesh_t src;
			src.width = ds.patchWidth;
			src.height = ds.patchHeight;
			src.verts = &yDrawVerts[ ds.firstVert ];
			//%	mesh_t *subdivided = SubdivideMesh( src, 8, 512 );
			mesh_t *subdivided = SubdivideMesh2( src, info.patchIterations );

			/* fit it to the curve and remove colinear verts on rows/columns */
			PutMeshOnCurve( *subdivided );
			mesh_t *mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );

			/* get verts */
			const bspDrawVert_t *verts = mesh->verts;

			/* map the mesh quads */
			info.approximated = true;
			for ( int y = 0; y < ( mesh->height - 1 ) && info.approximated; y++ )
			{
				for ( int x = 0; x < ( mesh->width - 1 ) && info.approximated; x++ )
				{
					/* set indexes */
					const int pw[ 5 ] = {
						x + ( y * mesh->width ),
						x + ( ( y + 1 ) * mesh->width ),
						x + 1 + ( ( y + 1 ) * mesh->width ),
						x + 1 + ( y * mesh->width ),
						x + ( y * mesh->width )      /* same as pw[ 0 ] */
					};
					/* set radix */
					const int r = ( x + y ) & 1;

					/* get drawverts and map first triangle */
					info.approximated = ApproximateTriangle_r( lm, TriRef{
						&verts[ pw[ r + 0 ] ],
						&verts[ pw[ r + 1 ] ],
						&verts[ pw[ r + 2 ] ] } );

					/* get drawverts and map second triangle */
					if ( info.approximated ) {
						info.approximated = ApproximateTriangle_r( lm, TriRef{
							&verts[ pw[ r + 0 ] ],
							&verts[ pw[ r + 2 ] ],
							&verts[ pw[ r + 3 ] ] } );
					}
				}
			}

			/* free the mesh */
			FreeMesh( mesh );
			break;
		}
		default:
			break;
		}

		/* reduced? */
		if ( !info.approximated ) {
			approximated = false;
		}
		else{
			numSurfsVertexApproximated++;
		}
	}

	/* return */
	return approximated;
}



/*
   TestOutLightmapStamp()
   tests a stamp on a given lightmap for validity
 */

static bool TestOutLightmapStamp( rawLightmap_t *lm, int lightmapNum, outLightmap_t *olm, int x, int y ){
	/* bounds check */
	if ( x < 0 || y < 0 || ( x + lm->w ) > olm->customWidth || ( y + lm->h ) > olm->customHeight ) {
		return false;
	}

	/* solid lightmaps test a 1x1 stamp */
	if ( lm->solid[ lightmapNum ] ) {
		if ( bit_is_enabled( olm->lightBits, ( y * olm->customWidth ) + x ) ) {
			return false;
		}
		return true;
	}

	/* test the stamp */
	for ( int sy = 0; sy < lm->h; ++sy )
	{
		for ( int sx = 0; sx < lm->w; ++sx )
		{
			/* get luxel */
			if ( lm->getBspLuxel( lightmapNum, sx, sy )[ 0 ] < 0.0f ) {
				continue;
			}

			/* get bsp lightmap coords and test */
			const int ox = x + sx;
			const int oy = y + sy;
			if ( bit_is_enabled( olm->lightBits, ( oy * olm->customWidth ) + ox ) ) {
				return false;
			}
		}
	}

	/* stamp is empty */
	return true;
}



/*
   SetupOutLightmap()
   sets up an output lightmap
 */

static void SetupOutLightmap( rawLightmap_t *lm, outLightmap_t *olm ){
	/* dummy check */
	if ( lm == NULL || olm == NULL ) {
		return;
	}

	/* is this a "normal" bsp-stored lightmap? */
	if ( ( lm->customWidth == g_game->lightmapSize && lm->customHeight == g_game->lightmapSize ) || externalLightmaps ) {
		olm->lightmapNum = numBSPLightmaps;
		numBSPLightmaps++;

		/* lightmaps are interleaved with light direction maps */
		if ( deluxemap ) {
			numBSPLightmaps++;
		}
	}
	else{
		olm->lightmapNum = -3;
	}

	/* set external lightmap number */
	olm->extLightmapNum = -1;

	/* set it up */
	olm->numLightmaps = 0;
	olm->customWidth = lm->customWidth;
	olm->customHeight = lm->customHeight;
	olm->freeLuxels = olm->customWidth * olm->customHeight;
	olm->numShaders = 0;

	/* allocate buffers */
	olm->lightBits = safe_calloc( ( olm->customWidth * olm->customHeight / 8 ) + 8 );
	olm->bspLightBytes = safe_calloc( olm->customWidth * olm->customHeight * sizeof( *olm->bspLightBytes ) );
	if ( deluxemap ) {
		olm->bspDirBytes = safe_calloc( olm->customWidth * olm->customHeight * sizeof( *olm->bspDirBytes ) );
	}
}



/*
   FindOutLightmaps()
   for a given surface lightmap, find output lightmap pages and positions for it
 */

#define LIGHTMAP_RESERVE_COUNT 1
static void FindOutLightmaps( rawLightmap_t *lm, bool fastAllocate ){
	int i, j, k, lightmapNum, xMax, yMax, x = -1, y = -1, sx, sy, ox, oy;
	outLightmap_t       *olm;
	surfaceInfo_t       *info;
	bool ok;
	int xIncrement, yIncrement;


	/* set default lightmap number (-3 = LIGHTMAP_BY_VERTEX) */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		lm->outLightmapNums[ lightmapNum ] = -3;

	/* can this lightmap be approximated with vertex color? */
	if ( ApproximateLightmap( lm ) ) {
		return;
	}

	/* walk list */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
	{
		/* early out */
		if ( lm->styles[ lightmapNum ] == LS_NONE ) {
			continue;
		}

		/* don't store twinned lightmaps */
		if ( lm->twins[ lightmapNum ] != NULL ) {
			continue;
		}

		/* if this is a styled lightmap, try some normalized locations first */
		ok = false;
		if ( lightmapNum > 0 && outLightmaps != NULL ) {
			/* loop twice */
			for ( j = 0; j < 2; j++ )
			{
				/* try identical position */
				for ( i = 0; i < numOutLightmaps; i++ )
				{
					/* get the output lightmap */
					olm = &outLightmaps[ i ];

					/* simple early out test */
					if ( olm->freeLuxels < lm->used ) {
						continue;
					}

					/* don't store non-custom raw lightmaps on custom bsp lightmaps */
					if ( olm->customWidth != lm->customWidth ||
					     olm->customHeight != lm->customHeight ) {
						continue;
					}

					/* try identical */
					if ( j == 0 ) {
						x = lm->lightmapX[ 0 ];
						y = lm->lightmapY[ 0 ];
						ok = TestOutLightmapStamp( lm, lightmapNum, olm, x, y );
					}

					/* try shifting */
					else
					{
						for ( sy = -1; sy <= 1; sy++ )
						{
							for ( sx = -1; sx <= 1; sx++ )
							{
								x = lm->lightmapX[ 0 ] + sx * ( olm->customWidth >> 1 );  //%	lm->w;
								y = lm->lightmapY[ 0 ] + sy * ( olm->customHeight >> 1 ); //%	lm->h;
								ok = TestOutLightmapStamp( lm, lightmapNum, olm, x, y );

								if ( ok ) {
									break;
								}
							}

							if ( ok ) {
								break;
							}
						}
					}

					if ( ok ) {
						break;
					}
				}

				if ( ok ) {
					break;
				}
			}
		}

		/* try normal placement algorithm */
		if ( !ok ) {
			/* reset origin */
			x = 0;
			y = 0;

			/* walk the list of lightmap pages */
			if ( lightmapSearchBlockSize <= 0 || numOutLightmaps < LIGHTMAP_RESERVE_COUNT ) {
				i = 0;
			}
			else{
				i = ( ( numOutLightmaps - LIGHTMAP_RESERVE_COUNT ) / lightmapSearchBlockSize ) * lightmapSearchBlockSize;
			}
			for ( ; i < numOutLightmaps; ++i )
			{
				/* get the output lightmap */
				olm = &outLightmaps[ i ];

				/* simple early out test */
				if ( olm->freeLuxels < lm->used ) {
					continue;
				}

				/* if fast allocation, skip lightmap files that are more than 90% complete */
				if ( fastAllocate ) {
					if ( olm->freeLuxels < ( olm->customWidth * olm->customHeight ) / 10 ) {
						continue;
					}
				}

				/* don't store non-custom raw lightmaps on custom bsp lightmaps */
				if ( olm->customWidth != lm->customWidth ||
				     olm->customHeight != lm->customHeight ) {
					continue;
				}

				/* set maxs */
				if ( lm->solid[ lightmapNum ] ) {
					xMax = olm->customWidth;
					yMax = olm->customHeight;
				}
				else
				{
					xMax = ( olm->customWidth - lm->w ) + 1;
					yMax = ( olm->customHeight - lm->h ) + 1;
				}

				/* if fast allocation, do not test allocation on every pixels, especially for large lightmaps */
				if ( fastAllocate ) {
					xIncrement = std::max( 1, lm->w / 15 );
					yIncrement = std::max( 1, lm->h / 15 );
				}
				else {
					xIncrement = 1;
					yIncrement = 1;
				}

				/* walk the origin around the lightmap */
				for ( y = 0; y < yMax; y += yIncrement )
				{
					for ( x = 0; x < xMax; x += xIncrement )
					{
						/* find a fine tract of lauhnd */
						ok = TestOutLightmapStamp( lm, lightmapNum, olm, x, y );

						if ( ok ) {
							break;
						}
					}

					if ( ok ) {
						break;
					}
				}

				if ( ok ) {
					break;
				}

				/* reset x and y */
				x = 0;
				y = 0;
			}
		}

		/* no match? */
		if ( !ok ) {
			/* allocate LIGHTMAP_RESERVE_COUNT new output lightmaps */
			numOutLightmaps += LIGHTMAP_RESERVE_COUNT;
			olm = safe_malloc( numOutLightmaps * sizeof( outLightmap_t ) );

			if ( outLightmaps != NULL && numOutLightmaps > LIGHTMAP_RESERVE_COUNT ) {
				memcpy( olm, outLightmaps, ( numOutLightmaps - LIGHTMAP_RESERVE_COUNT ) * sizeof( outLightmap_t ) );
				free( outLightmaps );
			}
			outLightmaps = olm;

			/* initialize both out lightmaps */
			for ( k = numOutLightmaps - LIGHTMAP_RESERVE_COUNT; k < numOutLightmaps; ++k )
				SetupOutLightmap( lm, &outLightmaps[ k ] );

			/* set out lightmap */
			i = numOutLightmaps - LIGHTMAP_RESERVE_COUNT;
			olm = &outLightmaps[ i ];

			/* set stamp xy origin to the first surface lightmap */
			if ( lightmapNum > 0 ) {
				x = lm->lightmapX[ 0 ];
				y = lm->lightmapY[ 0 ];
			}
		}

		/* if this is a style-using lightmap, it must be exported */
		if ( lightmapNum > 0 && g_game->load != LoadRBSPFile ) {
			olm->extLightmapNum = 0;
		}

		/* add the surface lightmap to the bsp lightmap */
		lm->outLightmapNums[ lightmapNum ] = i;
		lm->lightmapX[ lightmapNum ] = x;
		lm->lightmapY[ lightmapNum ] = y;
		olm->numLightmaps++;

		/* add shaders */
		for ( i = 0; i < lm->numLightSurfaces; i++ )
		{
			/* get surface info */
			info = &surfaceInfos[ lightSurfaces[ lm->firstLightSurface + i ] ];

			/* test for shader */
			for ( j = 0; j < olm->numShaders; j++ )
			{
				if ( olm->shaders[ j ] == info->si ) {
					break;
				}
			}

			/* if it doesn't exist, add it */
			if ( j >= olm->numShaders && olm->numShaders < MAX_LIGHTMAP_SHADERS ) {
				olm->shaders[ olm->numShaders ] = info->si;
				olm->numShaders++;
				numLightmapShaders++;
			}
		}

		/* set maxs */
		if ( lm->solid[ lightmapNum ] ) {
			xMax = 1;
			yMax = 1;
		}
		else
		{
			xMax = lm->w;
			yMax = lm->h;
		}

		/* mark the bits used */
		for ( y = 0; y < yMax; y++ )
		{
			for ( x = 0; x < xMax; x++ )
			{
				/* get luxel */
				const Vector3& luxel = lm->getBspLuxel( lightmapNum, x, y );
				if ( luxel[ 0 ] < 0.0f && !lm->solid[ lightmapNum ] ) {
					continue;
				}
				Vector3 color;
				/* set minimum light */
				if ( lm->solid[ lightmapNum ] ) {
					if ( debug ) {
						color = { 255.0f, 0.0f, 0.0f };
					}
					else{
						color = lm->solidColor[ lightmapNum ];
					}
				}
				else{

					color = luxel;
				}

				/* styles are not affected by minlight */
				if ( lightmapNum == 0 ) {
					for ( i = 0; i < 3; i++ )
					{
						value_maximize( color[ i ], minLight[ i ] );
					}
				}

				/* get bsp lightmap coords  */
				ox = x + lm->lightmapX[ lightmapNum ];
				oy = y + lm->lightmapY[ lightmapNum ];

				/* flag pixel as used */
				bit_enable( olm->lightBits, ( oy * olm->customWidth ) + ox );
				olm->freeLuxels--;

				/* store color */
				olm->bspLightBytes[ oy * olm->customWidth + ox] = ColorToBytes( color, lm->brightness );

				/* store direction */
				if ( deluxemap ) {
					/* normalize average light direction */
					const Vector3 direction = VectorNormalized( lm->getBspDeluxel( x, y ) * 1000.0f ) * 127.5f;
					olm->bspDirBytes[ oy * olm->customWidth + ox ] = direction + Vector3( 127.5f );
				}
			}
		}
	}
}



/*
   CompareRawLightmap
   compare functor for std::sort()
 */

struct CompareRawLightmap
{
	bool operator()( const int a, const int b ) const {
		int diff;

		/* get lightmaps */
		const rawLightmap_t& alm = rawLightmaps[ a ];
		const rawLightmap_t& blm = rawLightmaps[ b ];

		/* get min number of surfaces */
		const int min = std::min( alm.numLightSurfaces, blm.numLightSurfaces );

		/* iterate */
		for ( int i = 0; i < min; i++ )
		{
			/* get surface info */
			const surfaceInfo_t& aInfo = surfaceInfos[ lightSurfaces[ alm.firstLightSurface + i ] ];
			const surfaceInfo_t& bInfo = surfaceInfos[ lightSurfaces[ blm.firstLightSurface + i ] ];

			/* compare shader names */
			diff = strcmp( aInfo.si->shader, bInfo.si->shader );
			if ( diff != 0 ) {
				return diff < 0;
			}
		}

		/* test style count */
		diff = 0;
		for ( int i = 0; i < MAX_LIGHTMAPS; i++ )
			diff += blm.styles[ i ] - alm.styles[ i ];
		if ( diff != 0 ) {
			return diff < 0;
		}

		/* compare size */
		diff = ( blm.w * blm.h ) - ( alm.w * alm.h );
		if ( diff != 0 ) {
			return diff < 0;
		}
		/* must be equivalent */
		return false;
	}
};

static void FillOutLightmap( outLightmap_t *olm ){
	int x, y;
	int ofs;
	int cnt, filled;
	byte *lightBitsNew = NULL;
	Vector3b *lightBytesNew = NULL;
	Vector3b *dirBytesNew = NULL;
	const size_t size = olm->customWidth * olm->customHeight * sizeof( Vector3b );

	lightBitsNew = safe_malloc( ( olm->customWidth * olm->customHeight + 8 ) / 8 );
	lightBytesNew = safe_malloc( size );
	if ( deluxemap ) {
		dirBytesNew = safe_malloc( size );
	}

	/*
	   memset( olm->lightBits, 0, ( olm->customWidth * olm->customHeight + 8 ) / 8 );
	    olm->lightBits[0] |= 1;
	    olm->lightBits[( 10 * olm->customWidth + 30 ) >> 3] |= 1 << ( ( 10 * olm->customWidth + 30 ) & 7 );
	   memset( olm->bspLightBytes, 0, olm->customWidth * olm->customHeight * 3 );
	    olm->bspLightBytes[0] = 255;
	    olm->bspLightBytes[( 10 * olm->customWidth + 30 ) * 3 + 2] = 255;
	 */

	memcpy( lightBitsNew, olm->lightBits, ( olm->customWidth * olm->customHeight + 8 ) / 8 );
	memcpy( lightBytesNew, olm->bspLightBytes, size );
	if ( deluxemap ) {
		memcpy( dirBytesNew, olm->bspDirBytes, size );
	}

	for (;; )
	{
		filled = 0;
		for ( y = 0; y < olm->customHeight; ++y )
		{
			for ( x = 0; x < olm->customWidth; ++x )
			{
				ofs = y * olm->customWidth + x;
				if ( bit_is_enabled( olm->lightBits, ofs ) ) { /* already filled */
					continue;
				}
				cnt = 0;
				Vector3 dir_sum( 0 ), light_sum( 0 );

				/* try all four neighbors */
				ofs = ( ( y + olm->customHeight - 1 ) % olm->customHeight ) * olm->customWidth + x;
				if ( bit_is_enabled( olm->lightBits, ofs ) ) { /* already filled */
					++cnt;
					light_sum += olm->bspLightBytes[ofs];
					if ( deluxemap ) {
						dir_sum += olm->bspDirBytes[ofs];
					}
				}

				ofs = ( ( y + 1 ) % olm->customHeight ) * olm->customWidth + x;
				if ( bit_is_enabled( olm->lightBits, ofs ) ) { /* already filled */
					++cnt;
					light_sum += olm->bspLightBytes[ofs];
					if ( deluxemap ) {
						dir_sum += olm->bspDirBytes[ofs];
					}
				}

				ofs = y * olm->customWidth + ( x + olm->customWidth - 1 ) % olm->customWidth;
				if ( bit_is_enabled( olm->lightBits, ofs ) ) { /* already filled */
					++cnt;
					light_sum += olm->bspLightBytes[ofs];
					if ( deluxemap ) {
						dir_sum += olm->bspDirBytes[ofs];
					}
				}

				ofs = y * olm->customWidth + ( x + 1 ) % olm->customWidth;
				if ( bit_is_enabled( olm->lightBits, ofs ) ) { /* already filled */
					++cnt;
					light_sum += olm->bspLightBytes[ofs];
					if ( deluxemap ) {
						dir_sum += olm->bspDirBytes[ofs];
					}
				}

				if ( cnt ) {
					++filled;
					ofs = y * olm->customWidth + x;
					bit_enable( lightBitsNew, ofs );
					lightBytesNew[ofs] = light_sum * ( 1.0 / cnt );
					if ( deluxemap ) {
						dirBytesNew[ofs] = dir_sum * ( 1.0 / cnt );
					}
				}
			}
		}

		if ( !filled ) {
			break;
		}

		memcpy( olm->lightBits, lightBitsNew, ( olm->customWidth * olm->customHeight + 8 ) / 8 );
		memcpy( olm->bspLightBytes, lightBytesNew, size );
		if ( deluxemap ) {
			memcpy( olm->bspDirBytes, dirBytesNew, size );
		}
	}

	free( lightBitsNew );
	free( lightBytesNew );
	if ( deluxemap ) {
		free( dirBytesNew );
	}
}

/*
   StoreSurfaceLightmaps()
   stores the surface lightmaps into the bsp as byte rgb triplets
 */

void StoreSurfaceLightmaps( bool fastAllocate, bool storeForReal ){
	int i, j, k, x, y, lx, ly, sx, sy, mappedSamples;
	int style, lightmapNum, lightmapNum2;
	float               samples, occludedSamples;
	Vector3 sample, occludedSample, dirSample;
	byte                *lb;
	int numUsed, numTwins, numTwinLuxels, numStored;
	float lmx, lmy, efficiency;
	bspDrawSurface_t    *ds, *parent, dsTemp;
	surfaceInfo_t       *info;
	rawLightmap_t       *lm, *lm2;
	outLightmap_t       *olm;
	bspDrawVert_t       *dv, *ydv, *dvParent;
	char dirname[ 1024 ], filename[ 1024 ];
	const shaderInfo_t  *csi;
	char lightmapName[ 128 ];
	const char              *rgbGenValues[ 256 ] = {0};
	const char              *alphaGenValues[ 256 ] = {0};


	/* note it */
	Sys_Printf( "--- StoreSurfaceLightmaps ---\n" );

	/* setup */
	if ( lmCustomDir ) {
		strcpy( dirname, lmCustomDir );
	}
	else
	{
		strcpy( dirname, source );
		StripExtension( dirname );
	}

	/* -----------------------------------------------------------------
	   average the sampled luxels into the bsp luxels
	   ----------------------------------------------------------------- */

	/* note it */
	Sys_Printf( "Subsampling..." );

	Timer timer;

	/* walk the list of raw lightmaps */
	numUsed = 0;
	numTwins = 0;
	numTwinLuxels = 0;
	numSolidLightmaps = 0;
	for ( i = 0; i < numRawLightmaps; i++ )
	{
		/* get lightmap */
		lm = &rawLightmaps[ i ];

		/* walk individual lightmaps */
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			/* early outs */
			if ( lm->superLuxels[ lightmapNum ] == NULL ) {
				continue;
			}

			/* allocate bsp luxel storage */
			if ( lm->bspLuxels[ lightmapNum ] == NULL ) {
				const size_t size = lm->w * lm->h * sizeof( *( lm->bspLuxels[ 0 ] ) );
				lm->bspLuxels[ lightmapNum ] = safe_calloc( size );
			}

			/* allocate radiosity lightmap storage */
			if ( bounce ) {
				const size_t size = lm->w * lm->h * sizeof( *lm->radLuxels[ 0 ] );
				if ( lm->radLuxels[ lightmapNum ] == NULL ) {
					lm->radLuxels[ lightmapNum ] = safe_malloc( size );
				}
				memset( lm->radLuxels[ lightmapNum ], 0, size );
			}

			/* average supersampled luxels */
			for ( y = 0; y < lm->h; y++ )
			{
				for ( x = 0; x < lm->w; x++ )
				{
					/* subsample */
					samples = 0.0f;
					occludedSamples = 0.0f;
					mappedSamples = 0;
					sample.set( 0 );
					occludedSample.set( 0 );
					dirSample.set( 0 );
					for ( ly = 0; ly < superSample; ly++ )
					{
						for ( lx = 0; lx < superSample; lx++ )
						{
							/* sample luxel */
							sx = x * superSample + lx;
							sy = y * superSample + ly;
							SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, sx, sy );
							int& cluster = lm->getSuperCluster( sx, sy );

							/* sample deluxemap */
							if ( deluxemap && lightmapNum == 0 ) {
								dirSample += lm->getSuperDeluxel( sx, sy );
							}

							/* keep track of used/occluded samples */
							if ( cluster != CLUSTER_UNMAPPED ) {
								mappedSamples++;
							}

							/* handle lightmap border? */
							if ( lightmapBorder && ( sx == 0 || sx == ( lm->sw - 1 ) || sy == 0 || sy == ( lm->sh - 1 ) ) && luxel.count > 0.0f ) {
								sample = { 255, 0, 0 };
								samples += 1.0f;
							}

							/* handle debug */
							else if ( debug && cluster < 0 ) {
								if ( cluster == CLUSTER_UNMAPPED ) {
									luxel.value = { 255, 204, 0 };
								}
								else if ( cluster == CLUSTER_OCCLUDED ) {
									luxel.value = { 255, 0, 255 };
								}
								else if ( cluster == CLUSTER_FLOODED ) {
									luxel.value = { 0, 32, 255 };
								}
								occludedSample += luxel.value;
								occludedSamples += 1.0f;
							}

							/* normal luxel handling */
							else if ( luxel.count > 0.0f ) {
								/* handle lit or flooded luxels */
								if ( cluster > 0 || cluster == CLUSTER_FLOODED ) {
									sample += luxel.value;
									samples += luxel.count;
								}

								/* handle occluded or unmapped luxels */
								else
								{
									occludedSample += luxel.value;
									occludedSamples += luxel.count;
								}

								/* handle style debugging */
								if ( debug && lightmapNum > 0 && x < 2 && y < 2 ) {
									sample = debugColors[ 0 ];
									samples = 1;
								}
							}
						}
					}

					/* only use occluded samples if necessary */
					if ( samples <= 0.0f ) {
						sample = occludedSample;
						samples = occludedSamples;
					}

					/* get luxels */
					SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );

					/* store light direction */
					if ( deluxemap && lightmapNum == 0 ) {
						lm->getSuperDeluxel( x, y ) = dirSample;
					}

					/* store the sample back in super luxels */
					if ( samples > 0.01f ) {
						luxel.value = sample * ( 1.0f / samples );
						luxel.count = 1.0f;
					}

					/* if any samples were mapped in any way, store ambient color */
					else if ( mappedSamples > 0 ) {
						if ( lightmapNum == 0 ) {
							luxel.value = ambientColor;
						}
						else{
							luxel.value.set( 0 );
						}
						luxel.count = 1.0f;
					}

					/* store a bogus value to be fixed later */
					else
					{
						luxel.value.set( 0 );
						luxel.count = -1.0f;
					}
				}
			}

			/* setup */
			lm->used = 0;
			MinMax colorMinmax;

			/* clean up and store into bsp luxels */
			for ( y = 0; y < lm->h; y++ )
			{
				for ( x = 0; x < lm->w; x++ )
				{
					/* get luxels */
					const SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );

					/* copy light direction */
					if ( deluxemap && lightmapNum == 0 ) {
						dirSample = lm->getSuperDeluxel( x, y );
					}

					/* is this a valid sample? */
					if ( luxel.count > 0.0f ) {
						sample = luxel.value;
						samples = luxel.count;
						numUsed++;
						lm->used++;

						/* fix negative samples */
						for ( j = 0; j < 3; j++ )
						{
							value_maximize( sample[ j ], 0.0f );
						}
					}
					else
					{
						/* nick an average value from the neighbors */
						sample.set( 0 );
						dirSample.set( 0 );
						samples = 0.0f;

						/* fixme: why is this disabled?? */
						for ( sy = ( y - 1 ); sy <= ( y + 1 ); sy++ )
						{
							if ( sy < 0 || sy >= lm->h ) {
								continue;
							}

							for ( sx = ( x - 1 ); sx <= ( x + 1 ); sx++ )
							{
								if ( sx < 0 || sx >= lm->w || ( sx == x && sy == y ) ) {
									continue;
								}

								/* get neighbor's particulars */
								const SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, sx, sy );
								if ( luxel.count < 0.0f ) {
									continue;
								}
								sample += luxel.value;
								samples += luxel.count;
							}
						}

						/* no samples? */
						if ( samples == 0.0f ) {
							sample.set( -1 );
							samples = 1.0f;
						}
						else
						{
							numUsed++;
							lm->used++;

							/* fix negative samples */
							for ( j = 0; j < 3; j++ )
							{
								value_maximize( sample[ j ], 0.0f );
							}
						}
					}

					/* scale the sample */
					sample *= ( 1.0f / samples );

					/* store the sample in the radiosity luxels */
					if ( bounce > 0 ) {
						lm->getRadLuxel( lightmapNum, x, y ) = sample;

						/* if only storing bounced light, early out here */
						if ( bounceOnly && !bouncing ) {
							continue;
						}
					}

					/* store the sample in the bsp luxels */
					Vector3& bspLuxel = lm->getBspLuxel( lightmapNum, x, y );

					bspLuxel += sample;
					if ( deluxemap && lightmapNum == 0 ) {
						lm->getBspDeluxel( x, y ) += dirSample;
					}

					/* add color to bounds for solid checking */
					if ( samples > 0.0f ) {
						colorMinmax.extend( bspLuxel );
					}
				}
			}

			/* set solid color */
			lm->solid[ lightmapNum ] = false;
			lm->solidColor[ lightmapNum ] = colorMinmax.origin();

			/* nocollapse prevents solid lightmaps */
			if ( !noCollapse ) {
				/* check solid color */
				sample = colorMinmax.maxs - colorMinmax.mins;
				if ( ( sample[ 0 ] <= SOLID_EPSILON && sample[ 1 ] <= SOLID_EPSILON && sample[ 2 ] <= SOLID_EPSILON ) ||
				     ( lm->w <= 2 && lm->h <= 2 ) ) { /* small lightmaps get forced to solid color */
					/* set to solid */
					lm->solidColor[ lightmapNum ] = colorMinmax.mins;
					lm->solid[ lightmapNum ] = true;
					numSolidLightmaps++;
				}

				/* if all lightmaps aren't solid, then none of them are solid */
				if ( lm->solid[ lightmapNum ] != lm->solid[ 0 ] ) {
					for ( y = 0; y < MAX_LIGHTMAPS; y++ )
					{
						if ( lm->solid[ y ] ) {
							numSolidLightmaps--;
						}
						lm->solid[ y ] = false;
					}
				}
			}

			/* wrap bsp luxels if necessary */
			if ( lm->wrap[ 0 ] ) {
				for ( y = 0; y < lm->h; y++ )
				{
					Vector3& bspLuxel = lm->getBspLuxel( lightmapNum, 0, y );
					Vector3& bspLuxel2 = lm->getBspLuxel( lightmapNum, lm->w - 1, y );
					bspLuxel = bspLuxel2 = vector3_mid( bspLuxel, bspLuxel2 );
					if ( deluxemap && lightmapNum == 0 ) {
						Vector3& bspDeluxel = lm->getBspDeluxel( 0, y );
						Vector3& bspDeluxel2 = lm->getBspDeluxel( lm->w - 1, y );
						bspDeluxel = bspDeluxel2 = vector3_mid( bspDeluxel, bspDeluxel2 );
					}
				}
			}
			if ( lm->wrap[ 1 ] ) {
				for ( x = 0; x < lm->w; x++ )
				{
					Vector3& bspLuxel = lm->getBspLuxel( lightmapNum, x, 0 );
					Vector3& bspLuxel2 = lm->getBspLuxel( lightmapNum, x, lm->h - 1 );
					bspLuxel = vector3_mid( bspLuxel, bspLuxel2 );
					bspLuxel2 = bspLuxel;
					if ( deluxemap && lightmapNum == 0 ) {
						Vector3& bspDeluxel = lm->getBspDeluxel( x, 0 );
						Vector3& bspDeluxel2 = lm->getBspDeluxel( x, lm->h - 1 );
						bspDeluxel = bspDeluxel2 = vector3_mid( bspDeluxel, bspDeluxel2 );
					}
				}
			}
		}
	}

	Sys_Printf( "%d.", int( timer.elapsed_sec() ) );

	/* -----------------------------------------------------------------
	   convert modelspace deluxemaps to tangentspace
	   ----------------------------------------------------------------- */
	/* note it */
	if ( !bouncing ) {
		if ( deluxemap && deluxemode == 1 ) {
			timer.start();

			Sys_Printf( "converting..." );

			for ( i = 0; i < numRawLightmaps; i++ )
			{
				/* get lightmap */
				lm = &rawLightmaps[ i ];

				/* walk lightmap samples */
				for ( y = 0; y < lm->sh; y++ )
				{
					for ( x = 0; x < lm->sw; x++ )
					{
						/* get normal and deluxel */
						Vector3& bspDeluxel = lm->getBspDeluxel( x, y );

						/* get normal */
						const Vector3 myNormal = lm->getSuperNormal( x, y );

						/* get tangent vectors */
						Vector3 myTangent, myBinormal;
						if ( myNormal[ 0 ] == 0.0f && myNormal[ 1 ] == 0.0f ) {
							if ( myNormal.z() == 1.0f ) {
								myTangent = g_vector3_axis_x;
								myBinormal = g_vector3_axis_y;
							}
							else if ( myNormal.z() == -1.0f ) {
								myTangent = -g_vector3_axis_x;
								myBinormal = g_vector3_axis_y;
							}
						}
						else
						{
							myTangent = VectorNormalized( vector3_cross( myNormal, g_vector3_axis_z ) );
							myBinormal = VectorNormalized( vector3_cross( myTangent, myNormal ) );
						}

						/* project onto plane */
						myTangent -= myNormal * vector3_dot( myTangent, myNormal );
						myBinormal -= myNormal * vector3_dot( myBinormal, myNormal );

						/* renormalize */
						VectorNormalize( myTangent );
						VectorNormalize( myBinormal );

						/* convert modelspace deluxel to tangentspace */
						dirSample = VectorNormalized( bspDeluxel );

						/* fix tangents to world matrix */
						if ( myNormal.x() > 0 || myNormal.y() < 0 || myNormal.z() < 0 ) {
							vector3_negate( myTangent );
						}

						/* build tangentspace vectors */
						bspDeluxel[0] = vector3_dot( dirSample, myTangent );
						bspDeluxel[1] = vector3_dot( dirSample, myBinormal );
						bspDeluxel[2] = vector3_dot( dirSample, myNormal );
					}
				}
			}

			Sys_Printf( "%d.", int( timer.elapsed_sec() ) );
		}
	}

	/* -----------------------------------------------------------------
	   blend lightmaps
	   ----------------------------------------------------------------- */

#ifdef sdfsdfwq312323
	/* note it */
	Sys_Printf( "blending..." );

	for ( i = 0; i < numRawLightmaps; i++ )
	{
		Vector3 myColor;
		float myBrightness;

		/* get lightmap */
		lm = &rawLightmaps[ i ];

		/* walk individual lightmaps */
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			/* early outs */
			if ( lm->superLuxels[ lightmapNum ] == NULL ) {
				continue;
			}

			/* walk lightmap samples */
			for ( y = 0; y < lm->sh; y++ )
			{
				for ( x = 0; x < lm->sw; x++ )
				{
					/* get luxel */
					Vector3& bspLuxel = lm->getBspLuxel( lightmapNum, x, y );

					/* get color */
					myColor = VectorNormalized( bspLuxel );
					myBrightness = vector3_length( bspLuxel );
					myBrightness *= ( 1 / 127.0f );
					myBrightness = myBrightness * myBrightness;
					myBrightness *= 127.0f;
					bspLuxel = myColor * myBrightness;
				}
			}
		}
	}
#endif

	/* -----------------------------------------------------------------
	   collapse non-unique lightmaps
	   ----------------------------------------------------------------- */

	if ( storeForReal && !noCollapse && !deluxemap ) {
		/* note it */
		Sys_Printf( "collapsing..." );

		timer.start();

		/* set all twin refs to null */
		for ( i = 0; i < numRawLightmaps; i++ )
		{
			for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
			{
				rawLightmaps[ i ].twins[ lightmapNum ] = NULL;
				rawLightmaps[ i ].twinNums[ lightmapNum ] = -1;
				rawLightmaps[ i ].numStyledTwins = 0;
			}
		}

		/* walk the list of raw lightmaps */
		for ( i = 0; i < numRawLightmaps; i++ )
		{
			/* get lightmap */
			lm = &rawLightmaps[ i ];

			/* walk lightmaps */
			for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
			{
				/* early outs */
				if ( lm->bspLuxels[ lightmapNum ] == NULL ||
				     lm->twins[ lightmapNum ] != NULL ) {
					continue;
				}

				/* find all lightmaps that are virtually identical to this one */
				for ( j = i + 1; j < numRawLightmaps; j++ )
				{
					/* get lightmap */
					lm2 = &rawLightmaps[ j ];

					/* walk lightmaps */
					for ( lightmapNum2 = 0; lightmapNum2 < MAX_LIGHTMAPS; lightmapNum2++ )
					{
						/* early outs */
						if ( lm2->bspLuxels[ lightmapNum2 ] == NULL ||
						     lm2->twins[ lightmapNum2 ] != NULL ) {
							continue;
						}

						/* compare them */
						if ( CompareBSPLuxels( lm, lightmapNum, lm2, lightmapNum2 ) ) {
							/* merge and set twin */
							if ( MergeBSPLuxels( lm, lightmapNum, lm2, lightmapNum2 ) ) {
								lm2->twins[ lightmapNum2 ] = lm;
								lm2->twinNums[ lightmapNum2 ] = lightmapNum;
								numTwins++;
								numTwinLuxels += ( lm->w * lm->h );

								/* count styled twins */
								if ( lightmapNum > 0 ) {
									lm->numStyledTwins++;
								}
							}
						}
					}
				}
			}
		}

		Sys_Printf( "%d.", int( timer.elapsed_sec() ) );
	}

	/* -----------------------------------------------------------------
	   sort raw lightmaps by shader
	   ----------------------------------------------------------------- */

	if ( storeForReal ) {
		/* note it */
		Sys_Printf( "sorting..." );

		timer.start();

		/* allocate a new sorted list */
		if ( sortLightmaps == NULL ) {
			sortLightmaps = safe_malloc( numRawLightmaps * sizeof( int ) );
		}

		/* fill it out and sort it */
		for ( i = 0; i < numRawLightmaps; i++ )
			sortLightmaps[ i ] = i;
		std::sort( sortLightmaps, sortLightmaps + numRawLightmaps, CompareRawLightmap() );

		Sys_Printf( "%d.", int( timer.elapsed_sec() ) );
	}

	/* -----------------------------------------------------------------
	   allocate output lightmaps
	   ----------------------------------------------------------------- */

	if ( storeForReal ) {
		/* note it */
		Sys_Printf( "allocating..." );

		timer.start();

		/* kill all existing output lightmaps */
		if ( outLightmaps != NULL ) {
			for ( i = 0; i < numOutLightmaps; i++ )
			{
				free( outLightmaps[ i ].lightBits );
				free( outLightmaps[ i ].bspLightBytes );
			}
			free( outLightmaps );
			outLightmaps = NULL;
		}

		numLightmapShaders = 0;
		numOutLightmaps = 0;
		numBSPLightmaps = 0;
		numExtLightmaps = 0;

		/* find output lightmap */
		for ( i = 0; i < numRawLightmaps; i++ )
		{
			lm = &rawLightmaps[ sortLightmaps[ i ] ];
			FindOutLightmaps( lm, fastAllocate );
		}

		/* set output numbers in twinned lightmaps */
		for ( i = 0; i < numRawLightmaps; i++ )
		{
			/* get lightmap */
			lm = &rawLightmaps[ sortLightmaps[ i ] ];

			/* walk lightmaps */
			for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
			{
				/* get twin */
				lm2 = lm->twins[ lightmapNum ];
				if ( lm2 == NULL ) {
					continue;
				}
				lightmapNum2 = lm->twinNums[ lightmapNum ];

				/* find output lightmap from twin */
				lm->outLightmapNums[ lightmapNum ] = lm2->outLightmapNums[ lightmapNum2 ];
				lm->lightmapX[ lightmapNum ] = lm2->lightmapX[ lightmapNum2 ];
				lm->lightmapY[ lightmapNum ] = lm2->lightmapY[ lightmapNum2 ];
			}
		}

		Sys_Printf( "%d.", int( timer.elapsed_sec() ) );
	}

	/* -----------------------------------------------------------------
	   store output lightmaps
	   ----------------------------------------------------------------- */

	if ( storeForReal ) {
		/* note it */
		Sys_Printf( "storing..." );

		timer.start();

		/* count the bsp lightmaps and allocate space */
		const size_t gameLmSize = g_game->lightmapSize * g_game->lightmapSize * sizeof( Vector3b );
		if ( numBSPLightmaps == 0 || externalLightmaps ) {
			bspLightBytes.clear();
		}
		else
		{
			bspLightBytes = decltype( bspLightBytes )( numBSPLightmaps * gameLmSize, 0 );
		}

		/* walk the list of output lightmaps */
		for ( i = 0; i < numOutLightmaps; i++ )
		{
			/* get output lightmap */
			olm = &outLightmaps[ i ];

			/* fill output lightmap */
			if ( lightmapFill ) {
				FillOutLightmap( olm );
			}
			else if( lightmapPink ){
				for ( x = 0; x < olm->customHeight * olm->customWidth; ++x ){
					if ( !bit_is_enabled( olm->lightBits, x ) ) { /* not filled */
						olm->bspLightBytes[x] = { 255, 0, 255 };
					}
				}
			}

			/* is this a valid bsp lightmap? */
			if ( olm->lightmapNum >= 0 && !externalLightmaps ) {
				/* copy lighting data */
				lb = bspLightBytes.data() + ( olm->lightmapNum * gameLmSize );
				memcpy( lb, olm->bspLightBytes, gameLmSize );

				/* copy direction data */
				if ( deluxemap ) {
					lb = bspLightBytes.data() + ( ( olm->lightmapNum + 1 ) * gameLmSize );
					memcpy( lb, olm->bspDirBytes, gameLmSize );
				}
			}

			/* external lightmap? */
			if ( olm->lightmapNum < 0 || olm->extLightmapNum >= 0 || externalLightmaps ) {
				/* make a directory for the lightmaps */
				Q_mkdir( dirname );

				/* set external lightmap number */
				olm->extLightmapNum = numExtLightmaps;

				/* write lightmap */
				sprintf( filename, "%s/" EXTERNAL_LIGHTMAP, dirname, numExtLightmaps );
				Sys_FPrintf( SYS_VRB, "\nwriting %s", filename );
				WriteTGA24( filename, olm->bspLightBytes->data(), olm->customWidth, olm->customHeight, true );
				numExtLightmaps++;

				/* write deluxemap */
				if ( deluxemap ) {
					sprintf( filename, "%s/" EXTERNAL_LIGHTMAP, dirname, numExtLightmaps );
					Sys_FPrintf( SYS_VRB, "\nwriting %s", filename );
					WriteTGA24( filename, olm->bspDirBytes->data(), olm->customWidth, olm->customHeight, true );
					numExtLightmaps++;

					if ( debugDeluxemap ) {
						olm->extLightmapNum++;
					}
				}
			}
		}

		if ( numExtLightmaps > 0 ) {
			Sys_Printf( "\n" );
		}

		/* delete unused external lightmaps */
		for ( i = numExtLightmaps; i; i++ )
		{
			/* determine if file exists */
			sprintf( filename, "%s/" EXTERNAL_LIGHTMAP, dirname, i );
			if ( !FileExists( filename ) ) {
				break;
			}

			/* delete it */
			remove( filename );
		}

		Sys_Printf( "%d.", int( timer.elapsed_sec() ) );
	}

	/* -----------------------------------------------------------------
	   project the lightmaps onto the bsp surfaces
	   ----------------------------------------------------------------- */

	if ( storeForReal ) {
		/* note it */
		Sys_Printf( "projecting..." );

		timer.start();

		/* walk the list of surfaces */
		for ( size_t i = 0; i < bspDrawSurfaces.size(); ++i )
		{
			/* get the surface and info */
			ds = &bspDrawSurfaces[ i ];
			info = &surfaceInfos[ i ];
			lm = info->lm;
			olm = NULL;

			/* handle surfaces with identical parent */
			if ( info->parentSurfaceNum >= 0 ) {
				/* preserve original data and get parent */
				parent = &bspDrawSurfaces[ info->parentSurfaceNum ];
				memcpy( &dsTemp, ds, sizeof( *ds ) );

				/* overwrite child with parent data */
				memcpy( ds, parent, sizeof( *ds ) );

				/* restore key parts */
				ds->fogNum = dsTemp.fogNum;
				ds->firstVert = dsTemp.firstVert;
				ds->firstIndex = dsTemp.firstIndex;
				memcpy( ds->lightmapVecs, dsTemp.lightmapVecs, sizeof( dsTemp.lightmapVecs ) );

				/* set vertex data */
				dv = &bspDrawVerts[ ds->firstVert ];
				dvParent = &bspDrawVerts[ parent->firstVert ];
				for ( j = 0; j < ds->numVerts; j++ )
				{
					memcpy( dv[ j ].lightmap, dvParent[ j ].lightmap, sizeof( dv[ j ].lightmap ) );
					memcpy( dv[ j ].color, dvParent[ j ].color, sizeof( dv[ j ].color ) );
				}

				/* skip the rest */
				continue;
			}

			/* handle vertex lit or approximated surfaces */
			else if ( lm == NULL || lm->outLightmapNums[ 0 ] < 0 ) {
				for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
				{
					ds->lightmapNum[ lightmapNum ] = -3;
					ds->lightmapStyles[ lightmapNum ] = ds->vertexStyles[ lightmapNum ];
				}
			}

			/* handle lightmapped surfaces */
			else
			{
				/* walk lightmaps */
				for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
				{
					/* set style */
					ds->lightmapStyles[ lightmapNum ] = lm->styles[ lightmapNum ];

					/* handle unused style */
					if ( lm->styles[ lightmapNum ] == LS_NONE || lm->outLightmapNums[ lightmapNum ] < 0 ) {
						ds->lightmapNum[ lightmapNum ] = -3;
						continue;
					}

					/* get output lightmap */
					olm = &outLightmaps[ lm->outLightmapNums[ lightmapNum ] ];

					/* set bsp lightmap number */
					ds->lightmapNum[ lightmapNum ] = olm->lightmapNum;

					/* deluxemap debugging makes the deluxemap visible */
					if ( deluxemap && debugDeluxemap && lightmapNum == 0 ) {
						ds->lightmapNum[ lightmapNum ]++;
					}

					/* calc lightmap origin in texture space */
					lmx = (float) lm->lightmapX[ lightmapNum ] / (float) olm->customWidth;
					lmy = (float) lm->lightmapY[ lightmapNum ] / (float) olm->customHeight;

					/* calc lightmap st coords */
					dv = &bspDrawVerts[ ds->firstVert ];
					ydv = &yDrawVerts[ ds->firstVert ];
					for ( j = 0; j < ds->numVerts; j++ )
					{
						if ( lm->solid[ lightmapNum ] ) {
							dv[ j ].lightmap[ lightmapNum ][ 0 ] = lmx + ( 0.5f / (float) olm->customWidth );
							dv[ j ].lightmap[ lightmapNum ][ 1 ] = lmy + ( 0.5f / (float) olm->customWidth );
						}
						else
						{
							dv[ j ].lightmap[ lightmapNum ][ 0 ] = lmx + ( ydv[ j ].lightmap[ 0 ][ 0 ] / ( superSample * olm->customWidth ) );
							dv[ j ].lightmap[ lightmapNum ][ 1 ] = lmy + ( ydv[ j ].lightmap[ 0 ][ 1 ] / ( superSample * olm->customHeight ) );
						}
					}
				}
			}

			/* store vertex colors */
			dv = &bspDrawVerts[ ds->firstVert ];
			for ( j = 0; j < ds->numVerts; j++ )
			{
				/* walk lightmaps */
				for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
				{
					Vector3 color;
					/* handle unused style */
					if ( ds->vertexStyles[ lightmapNum ] == LS_NONE ) {
						color.set( 0 );
					}
					else
					{
						/* get vertex color */
						color = getVertexLuxel( lightmapNum, ds->firstVert + j );

						/* set minimum light */
						if ( lightmapNum == 0 ) {
							for ( k = 0; k < 3; k++ )
								value_maximize( color[ k ], minVertexLight[ k ] );
						}
					}

					/* store to bytes */
					if ( !info->si->noVertexLight ) {
						dv[ j ].color[ lightmapNum ].rgb() = ColorToBytes( color, info->si->vertexScale );
					}
				}
			}

			/* surfaces with styled lightmaps and a style marker get a custom generated shader (fixme: make this work with external lightmaps) */
			if ( olm != NULL && lm != NULL && lm->styles[ 1 ] != LS_NONE && g_game->load != LoadRBSPFile ) { //%	info->si->styleMarker > 0 )
				char key[ 32 ], styleStage[ 512 ], styleStages[ 4096 ], rgbGen[ 128 ], alphaGen[ 128 ];


				/* setup */
				sprintf( styleStages, "\n\t// Q3Map2 custom lightstyle stage(s)\n" );
				dv = &bspDrawVerts[ ds->firstVert ];

				/* depthFunc equal? */
				const bool dfEqual = ( info->si->styleMarker == 2 || info->si->implicitMap == EImplicitMap::Masked );

				/* generate stages for styled lightmaps */
				for ( lightmapNum = 1; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
				{
					/* early out */
					style = lm->styles[ lightmapNum ];
					if ( style == LS_NONE || lm->outLightmapNums[ lightmapNum ] < 0 ) {
						continue;
					}

					/* get output lightmap */
					olm = &outLightmaps[ lm->outLightmapNums[ lightmapNum ] ];

					/* lightmap name */
					if ( lm->outLightmapNums[ lightmapNum ] == lm->outLightmapNums[ 0 ] ) {
						strcpy( lightmapName, "$lightmap" );
					}
					else{
						sprintf( lightmapName, "maps/%s/" EXTERNAL_LIGHTMAP, mapName.c_str(), olm->extLightmapNum );
					}

					/* get rgbgen string */
					if ( rgbGenValues[ style ] == NULL ) {
						sprintf( key, "_style%drgbgen", style );
						rgbGenValues[ style ] = entities[ 0 ].valueForKey( key );
						if ( strEmpty( rgbGenValues[ style ] ) ) {
							rgbGenValues[ style ] = "wave noise 0.5 1 0 5.37";
						}
					}
					strClear( rgbGen );
					if ( !strEmpty( rgbGenValues[ style ] ) ) {
						sprintf( rgbGen, "\t\trgbGen %s // style %d\n", rgbGenValues[ style ], style );
					}
					else{
						strClear( rgbGen );
					}

					/* get alphagen string */
					if ( alphaGenValues[ style ] == NULL ) {
						sprintf( key, "_style%dalphagen", style );
						alphaGenValues[ style ] = entities[ 0 ].valueForKey( key );
					}
					if ( !strEmpty( alphaGenValues[ style ] ) ) {
						sprintf( alphaGen, "\t\talphaGen %s // style %d\n", alphaGenValues[ style ], style );
					}
					else{
						strClear( alphaGen );
					}

					/* calculate st offset */
					const Vector2 lmxy = dv[ 0 ].lightmap[ lightmapNum ] - dv[ 0 ].lightmap[ 0 ];

					/* create additional stage */
					if ( lmxy.x() == 0.0f && lmxy.y() == 0.0f ) {
						sprintf( styleStage, "\t{\n"
						                     "\t\tmap %s\n"                                      /* lightmap */
						                     "\t\tblendFunc GL_SRC_ALPHA GL_ONE\n"
						                     "%s"                                                /* depthFunc equal */
						                     "%s"                                                /* rgbGen */
						                     "%s"                                                /* alphaGen */
						                     "\t\ttcGen lightmap\n"
						                     "\t}\n",
						         lightmapName,
						         ( dfEqual ? "\t\tdepthFunc equal\n" : "" ),
						         rgbGen,
						         alphaGen );
					}
					else
					{
						sprintf( styleStage, "\t{\n"
						                     "\t\tmap %s\n"                                      /* lightmap */
						                     "\t\tblendFunc GL_SRC_ALPHA GL_ONE\n"
						                     "%s"                                                /* depthFunc equal */
						                     "%s"                                                /* rgbGen */
						                     "%s"                                                /* alphaGen */
						                     "\t\ttcGen lightmap\n"
						                     "\t\ttcMod transform 1 0 0 1 %1.5f %1.5f\n"         /* st offset */
						                     "\t}\n",
						         lightmapName,
						         ( dfEqual ? "\t\tdepthFunc equal\n" : "" ),
						         rgbGen,
						         alphaGen,
						         lmxy.x(), lmxy.y() );

					}

					/* concatenate */
					strcat( styleStages, styleStage );
				}

				/* create custom shader */
				if ( info->si->styleMarker == 2 ) {
					csi = CustomShader( info->si, "q3map_styleMarker2", styleStages );
				}
				else{
					csi = CustomShader( info->si, "q3map_styleMarker", styleStages );
				}

				/* emit remap command */
				//%	EmitVertexRemapShader( csi->shader, info->si->shader );

				/* store it */
				//%	Sys_Printf( "Emitting: %s (%d", csi->shader, strlen( csi->shader ) );
				const int cont = bspShaders[ ds->shaderNum ].contentFlags;
				const int surf = bspShaders[ ds->shaderNum ].surfaceFlags;
				ds->shaderNum = EmitShader( csi->shader, &cont, &surf );
				//%	Sys_Printf( ")\n" );
			}

			/* devise a custom shader for this surface (fixme: make this work with light styles) */
			else if ( olm != NULL && lm != NULL && !externalLightmaps &&
			          ( olm->customWidth != g_game->lightmapSize || olm->customHeight != g_game->lightmapSize ) ) {
				/* get output lightmap */
				olm = &outLightmaps[ lm->outLightmapNums[ 0 ] ];

				/* do some name mangling */
				sprintf( lightmapName, "maps/%s/" EXTERNAL_LIGHTMAP "\n\t\ttcgen lightmap", mapName.c_str(), olm->extLightmapNum );

				/* create custom shader */
				csi = CustomShader( info->si, "$lightmap", lightmapName );

				/* store it */
				//%	Sys_Printf( "Emitting: %s (%d", csi->shader, strlen( csi->shader ) );
				const int cont = bspShaders[ ds->shaderNum ].contentFlags;
				const int surf = bspShaders[ ds->shaderNum ].surfaceFlags;
				ds->shaderNum = EmitShader( csi->shader, &cont, &surf );
				//%	Sys_Printf( ")\n" );
			}

			/* use the normal plain-jane shader */
			else{
				const int cont = bspShaders[ ds->shaderNum ].contentFlags;
				const int surf = bspShaders[ ds->shaderNum ].surfaceFlags;
				ds->shaderNum = EmitShader( info->si->shader, &cont, &surf );
			}
		}

		Sys_Printf( "%d.", int( timer.elapsed_sec() ) );
	}

	/* finish */
	Sys_Printf( "done.\n" );

	if ( storeForReal ) {
		/* calc num stored */
		numStored = bspLightBytes.size() / 3;
		efficiency = ( numStored <= 0 )
	                 ? 0
	                 : (float) numUsed / (float) numStored;

		/* print stats */
		Sys_Printf( "%9d luxels used\n", numUsed );
		Sys_Printf( "%9d luxels stored (%3.2f percent efficiency)\n", numStored, efficiency * 100.0f );
		Sys_Printf( "%9d solid surface lightmaps\n", numSolidLightmaps );
		Sys_Printf( "%9d identical surface lightmaps, using %d luxels\n", numTwins, numTwinLuxels );
		Sys_Printf( "%9d vertex forced surfaces\n", numSurfsVertexForced );
		Sys_Printf( "%9d vertex approximated surfaces\n", numSurfsVertexApproximated );
		Sys_Printf( "%9d BSP lightmaps\n", numBSPLightmaps );
		Sys_Printf( "%9d total lightmaps\n", numOutLightmaps );
		Sys_Printf( "%9d unique lightmap/shader combinations\n", numLightmapShaders );

		/* write map shader file */
		WriteMapShaderFile();
	}
}

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
#include "bspfile_abstract.h"




/* -------------------------------------------------------------------------------

   this file handles translating the bsp file format used by quake 3, rtcw, and ef
   into the abstracted bsp file used by q3map2.

   ------------------------------------------------------------------------------- */

/* constants */
#define LUMP_ENTITIES       0
#define LUMP_SHADERS        1
#define LUMP_PLANES         2
#define LUMP_NODES          3
#define LUMP_LEAFS          4
#define LUMP_LEAFSURFACES   5
#define LUMP_LEAFBRUSHES    6
#define LUMP_MODELS         7
#define LUMP_BRUSHES        8
#define LUMP_BRUSHSIDES     9
#define LUMP_DRAWVERTS      10
#define LUMP_DRAWINDEXES    11
#define LUMP_FOGS           12
#define LUMP_SURFACES       13
#define LUMP_LIGHTMAPS      14
#define LUMP_LIGHTGRID      15
#define LUMP_VISIBILITY     16
#define LUMP_ADVERTISEMENTS 17
#define HEADER_LUMPS        18


/* types */
struct ibspHeader_t
{
	char ident[ 4 ];
	int version;

	bspLump_t lumps[ HEADER_LUMPS ];
};



/* brush sides */
struct ibspBrushSide_t
{
	int planeNum;
	int shaderNum;
	ibspBrushSide_t( const bspBrushSide_t& other ) :
		planeNum( other.planeNum ),
		shaderNum( other.shaderNum ){}
	operator bspBrushSide_t() const {
		return { planeNum, shaderNum, -1 };
	}
};



/* drawsurfaces */
struct ibspDrawSurface_t
{
	int shaderNum;
	int fogNum;
	int surfaceType;

	int firstVert;
	int numVerts;

	int firstIndex;
	int numIndexes;

	int lightmapNum;
	int lightmapX, lightmapY;
	int lightmapWidth, lightmapHeight;

	Vector3 lightmapOrigin;
	Vector3 lightmapVecs[ 3 ];

	int patchWidth;
	int patchHeight;
};


static void CopyDrawSurfacesLump( ibspHeader_t *header ){
	int i, j;
	ibspDrawSurface_t   *in;
	bspDrawSurface_t    *out;


	/* get count */
	numBSPDrawSurfaces = GetLumpElements( (bspHeader_t*) header, LUMP_SURFACES, sizeof( *in ) );
	SetDrawSurfaces( numBSPDrawSurfaces );

	/* copy */
	in = GetLump( (bspHeader_t*) header, LUMP_SURFACES );
	out = bspDrawSurfaces;
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		out->shaderNum = in->shaderNum;
		out->fogNum = in->fogNum;
		out->surfaceType = in->surfaceType;
		out->firstVert = in->firstVert;
		out->numVerts = in->numVerts;
		out->firstIndex = in->firstIndex;
		out->numIndexes = in->numIndexes;

		out->lightmapStyles[ 0 ] = LS_NORMAL;
		out->vertexStyles[ 0 ] = LS_NORMAL;
		out->lightmapNum[ 0 ] = in->lightmapNum;
		out->lightmapX[ 0 ] = in->lightmapX;
		out->lightmapY[ 0 ] = in->lightmapY;

		for ( j = 1; j < MAX_LIGHTMAPS; j++ )
		{
			out->lightmapStyles[ j ] = LS_NONE;
			out->vertexStyles[ j ] = LS_NONE;
			out->lightmapNum[ j ] = -3;
			out->lightmapX[ j ] = 0;
			out->lightmapY[ j ] = 0;
		}

		out->lightmapWidth = in->lightmapWidth;
		out->lightmapHeight = in->lightmapHeight;

		out->lightmapOrigin = in->lightmapOrigin;
		out->lightmapVecs[ 0 ] = in->lightmapVecs[ 0 ];
		out->lightmapVecs[ 1 ] = in->lightmapVecs[ 1 ];
		out->lightmapVecs[ 2 ] = in->lightmapVecs[ 2 ];

		out->patchWidth = in->patchWidth;
		out->patchHeight = in->patchHeight;

		in++;
		out++;
	}
}


static void AddDrawSurfacesLump( FILE *file, ibspHeader_t *header ){
	int i, size;
	bspDrawSurface_t    *in;
	ibspDrawSurface_t   *buffer, *out;


	/* allocate output buffer */
	size = numBSPDrawSurfaces * sizeof( *buffer );
	buffer = safe_calloc( size );

	/* convert */
	in = bspDrawSurfaces;
	out = buffer;
	for ( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		out->shaderNum = in->shaderNum;
		out->fogNum = in->fogNum;
		out->surfaceType = in->surfaceType;
		out->firstVert = in->firstVert;
		out->numVerts = in->numVerts;
		out->firstIndex = in->firstIndex;
		out->numIndexes = in->numIndexes;

		out->lightmapNum = in->lightmapNum[ 0 ];
		out->lightmapX = in->lightmapX[ 0 ];
		out->lightmapY = in->lightmapY[ 0 ];
		out->lightmapWidth = in->lightmapWidth;
		out->lightmapHeight = in->lightmapHeight;

		out->lightmapOrigin = in->lightmapOrigin;
		out->lightmapVecs[ 0 ] = in->lightmapVecs[ 0 ];
		out->lightmapVecs[ 1 ] = in->lightmapVecs[ 1 ];
		out->lightmapVecs[ 2 ] = in->lightmapVecs[ 2 ];

		out->patchWidth = in->patchWidth;
		out->patchHeight = in->patchHeight;

		in++;
		out++;
	}

	/* write lump */
	AddLump( file, (bspHeader_t*) header, LUMP_SURFACES, buffer, size );

	/* free buffer */
	free( buffer );
}



/* drawverts */
struct ibspDrawVert_t
{
	Vector3 xyz;
	Vector2 st;
	Vector2 lightmap;
	Vector3 normal;
	Color4b color;
};


static void CopyDrawVertsLump( ibspHeader_t *header ){
	int i;
	ibspDrawVert_t  *in;
	bspDrawVert_t   *out;


	/* get count */
	numBSPDrawVerts = GetLumpElements( (bspHeader_t*) header, LUMP_DRAWVERTS, sizeof( *in ) );
	SetDrawVerts( numBSPDrawVerts );

	/* copy */
	in = GetLump( (bspHeader_t*) header, LUMP_DRAWVERTS );
	out = bspDrawVerts;
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		out->xyz = in->xyz;
		out->st = in->st;
		out->lightmap[ 0 ] = in->lightmap;
		out->normal = in->normal;
		out->color[ 0 ] = in->color;
		in++;
		out++;
	}
}


static void AddDrawVertsLump( FILE *file, ibspHeader_t *header ){
	int i, size;
	bspDrawVert_t   *in;
	ibspDrawVert_t  *buffer, *out;


	/* allocate output buffer */
	size = numBSPDrawVerts * sizeof( *buffer );
	buffer = safe_calloc( size );

	/* convert */
	in = bspDrawVerts;
	out = buffer;
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		out->xyz = in->xyz;
		out->st = in->st;
		out->lightmap = in->lightmap[ 0 ];
		out->normal = in->normal;
		out->color = in->color[ 0 ];
		in++;
		out++;
	}

	/* write lump */
	AddLump( file, (bspHeader_t*) header, LUMP_DRAWVERTS, buffer, size );

	/* free buffer */
	free( buffer );
}



/* light grid */
struct ibspGridPoint_t
{
	Vector3b ambient;
	Vector3b directed;
	byte latLong[ 2 ];
	ibspGridPoint_t( const bspGridPoint_t& other ) :
		ambient( other.ambient[0] ),
		directed( other.directed[0] ),
		latLong{ other.latLong[0], other.latLong[1] } {}
	operator bspGridPoint_t() const {
		static_assert( MAX_LIGHTMAPS == 4 );
		return {
			{ ambient, ambient, ambient, ambient },
			{ directed, directed, directed, directed },
			{ LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			{ latLong[0], latLong[1] }
		};
	}
};



/*
   LoadIBSPFile()
   loads a quake 3 bsp file into memory
 */

void LoadIBSPFile( const char *filename ){
	ibspHeader_t    *header;


	/* load the file header */
	LoadFile( filename, (void**) &header );

	/* swap the header (except the first 4 bytes) */
	SwapBlock( (int*) ( (byte*) header + sizeof( int ) ), sizeof( *header ) - sizeof( int ) );

	/* make sure it matches the format we're trying to load */
	if ( !force && *( (int*) header->ident ) != *( (const int*) g_game->bspIdent ) ) {
		Error( "%s is not a %s file", filename, g_game->bspIdent );
	}
	if ( !force && header->version != g_game->bspVersion ) {
		Error( "%s is version %d, not %d", filename, header->version, g_game->bspVersion );
	}

	/* load/convert lumps */
	CopyLump( (bspHeader_t*) header, LUMP_SHADERS, bspShaders );

	CopyLump( (bspHeader_t*) header, LUMP_MODELS, bspModels );

	CopyLump( (bspHeader_t*) header, LUMP_PLANES, bspPlanes );

	CopyLump( (bspHeader_t*) header, LUMP_LEAFS, bspLeafs );

	CopyLump( (bspHeader_t*) header, LUMP_NODES, bspNodes );

	CopyLump( (bspHeader_t*) header, LUMP_LEAFSURFACES, bspLeafSurfaces );

	CopyLump( (bspHeader_t*) header, LUMP_LEAFBRUSHES, bspLeafBrushes );

	CopyLump( (bspHeader_t*) header, LUMP_BRUSHES, bspBrushes );

	CopyLump<bspBrushSide_t, ibspBrushSide_t>( (bspHeader_t*) header, LUMP_BRUSHSIDES, bspBrushSides );

	CopyDrawVertsLump( header );

	CopyDrawSurfacesLump( header );

	numBSPFogs = CopyLump( (bspHeader_t*) header, LUMP_FOGS, bspFogs, sizeof( bspFog_t ) ); // TODO fix overflow

	numBSPDrawIndexes = CopyLump_Allocate( (bspHeader_t*) header, LUMP_DRAWINDEXES, (void **) &bspDrawIndexes, sizeof( bspDrawIndexes[ 0 ] ), &allocatedBSPDrawIndexes );

	numBSPVisBytes = CopyLump( (bspHeader_t*) header, LUMP_VISIBILITY, bspVisBytes, 1 ); // TODO fix overflow

	CopyLump( (bspHeader_t*) header, LUMP_LIGHTMAPS, bspLightBytes );

	CopyLump( (bspHeader_t*) header, LUMP_ENTITIES, bspEntData );

	CopyLump<bspGridPoint_t, ibspGridPoint_t>( (bspHeader_t*) header, LUMP_LIGHTGRID, bspGridPoints );

	/* advertisements */
	if ( header->version == 47 && strEqual( g_game->arg, "quakelive" ) ) { // quake live's bsp version minus wolf, et, etut
		numBSPAds = CopyLump( (bspHeader_t*) header, LUMP_ADVERTISEMENTS, bspAds, sizeof( bspAdvertisement_t ) );
	}
	else{
		numBSPAds = 0;
	}

	/* free the file buffer */
	free( header );
}

/*
   PartialLoadIBSPFile()
   loads a part of quake 3 bsp file, required by packer, into memory
 */

void PartialLoadIBSPFile( const char *filename ){
	ibspHeader_t    *header;


	/* load the file header */
	LoadFile( filename, (void**) &header );

	/* swap the header (except the first 4 bytes) */
	SwapBlock( (int*) ( (byte*) header + sizeof( int ) ), sizeof( *header ) - sizeof( int ) );

	/* make sure it matches the format we're trying to load */
	if ( !force && *( (int*) header->ident ) != *( (const int*) g_game->bspIdent ) ) {
		Error( "%s is not a %s file", filename, g_game->bspIdent );
	}
	if ( !force && header->version != g_game->bspVersion ) {
		Error( "%s is version %d, not %d", filename, header->version, g_game->bspVersion );
	}

	/* load/convert lumps */
	CopyLump( (bspHeader_t*) header, LUMP_SHADERS, bspShaders );

	CopyDrawSurfacesLump( header );

	numBSPFogs = CopyLump( (bspHeader_t*) header, LUMP_FOGS, bspFogs, sizeof( bspFog_t ) ); // TODO fix overflow

	CopyLump( (bspHeader_t*) header, LUMP_ENTITIES, bspEntData );

	/* free the file buffer */
	free( header );
}

/*
   WriteIBSPFile()
   writes an id bsp file
 */

void WriteIBSPFile( const char *filename ){
	ibspHeader_t outheader, *header;
	FILE            *file;
	time_t t;
	char marker[ 1024 ];
	int size;


	/* set header */
	header = &outheader;
	memset( header, 0, sizeof( *header ) );

	//%	Swapfile();

	/* set up header */
	*( (int*) (bspHeader_t*) header->ident ) = *( (const int*) g_game->bspIdent );
	header->version = LittleLong( g_game->bspVersion );

	/* write initial header */
	file = SafeOpenWrite( filename );
	SafeWrite( file, (bspHeader_t*) header, sizeof( *header ) );    /* overwritten later */

	/* add marker lump */
	time( &t );
	/* asctime adds an implicit trailing \n */
	sprintf( marker, "I LOVE MY Q3MAP2 %s on %s", Q3MAP_VERSION, asctime( localtime( &t ) ) );
	AddLump( file, (bspHeader_t*) header, 0, marker, strlen( marker ) + 1 );

	/* add lumps */
	AddLump( file, header->lumps[LUMP_SHADERS], bspShaders );
	AddLump( file, header->lumps[LUMP_PLANES], bspPlanes );
	AddLump( file, header->lumps[LUMP_LEAFS], bspLeafs );
	AddLump( file, header->lumps[LUMP_NODES], bspNodes );
	AddLump( file, header->lumps[LUMP_BRUSHES], bspBrushes );
	AddLump( file, header->lumps[LUMP_BRUSHSIDES], std::vector<ibspBrushSide_t>( bspBrushSides.begin(), bspBrushSides.end() ) );
	AddLump( file, header->lumps[LUMP_LEAFSURFACES], bspLeafSurfaces );
	AddLump( file, header->lumps[LUMP_LEAFBRUSHES], bspLeafBrushes );
	AddLump( file, header->lumps[LUMP_MODELS], bspModels );
	AddDrawVertsLump( file, header );
	AddDrawSurfacesLump( file, header );
	AddLump( file, (bspHeader_t*) header, LUMP_VISIBILITY, bspVisBytes, numBSPVisBytes );
	AddLump( file, header->lumps[LUMP_LIGHTMAPS], bspLightBytes );
	AddLump( file, header->lumps[LUMP_LIGHTGRID], std::vector<ibspGridPoint_t>( bspGridPoints.begin(), bspGridPoints.end() ) );
	AddLump( file, header->lumps[LUMP_ENTITIES], bspEntData );
	AddLump( file, (bspHeader_t*) header, LUMP_FOGS, bspFogs, numBSPFogs * sizeof( bspFog_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_DRAWINDEXES, bspDrawIndexes, numBSPDrawIndexes * sizeof( bspDrawIndexes[ 0 ] ) );

	/* advertisements */
	AddLump( file, (bspHeader_t*) header, LUMP_ADVERTISEMENTS, bspAds, numBSPAds * sizeof( bspAdvertisement_t ) );

	/* emit bsp size */
	size = ftell( file );
	Sys_Printf( "Wrote %.1f MB (%d bytes)\n", (float) size / ( 1024 * 1024 ), size );

	/* write the completed header */
	fseek( file, 0, SEEK_SET );
	SafeWrite( file, header, sizeof( *header ) );

	/* close the file */
	fclose( file );
}

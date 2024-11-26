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
#include <ctime>




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
	ibspDrawSurface_t( const bspDrawSurface_t& other ) :
		shaderNum( other.shaderNum ),
		fogNum( other.fogNum ),
		surfaceType( other.surfaceType ),
		firstVert( other.firstVert ),
		numVerts( other.numVerts ),
		firstIndex( other.firstIndex ),
		numIndexes( other.numIndexes ),
		lightmapNum( other.lightmapNum[0] ),
		lightmapX( other.lightmapX[0] ),
		lightmapY( other.lightmapY[0] ),
		lightmapWidth( other.lightmapWidth ),
		lightmapHeight( other.lightmapHeight ),
		lightmapOrigin( other.lightmapOrigin ),
		lightmapVecs{ other.lightmapVecs[0], other.lightmapVecs[1], other.lightmapVecs[2] },
		patchWidth( other.patchWidth ),
		patchHeight( other.patchHeight ) {}
	operator bspDrawSurface_t() const {
		static_assert( MAX_LIGHTMAPS == 4 );
		return{
			shaderNum,
			fogNum,
			surfaceType,
			firstVert,
			numVerts,
			firstIndex,
			numIndexes,
			{ LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			{ LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			{ lightmapNum, -3, -3, -3 },
			{ lightmapX, 0, 0, 0 },
			{ lightmapY, 0, 0, 0 },
			lightmapWidth,
			lightmapHeight,
			lightmapOrigin,
			{ lightmapVecs[0], lightmapVecs[1], lightmapVecs[2] },
			patchWidth,
			patchHeight
		};
	}
};



/* drawverts */
struct ibspDrawVert_t
{
	Vector3 xyz;
	Vector2 st;
	Vector2 lightmap;
	Vector3 normal;
	Color4b color;
	ibspDrawVert_t( const bspDrawVert_t& other ) :
		xyz( other.xyz ),
		st( other.st ),
		lightmap( other.lightmap[0] ),
		normal( other.normal ),
		color( other.color[0] ) {}
	operator bspDrawVert_t() const {
		static_assert( MAX_LIGHTMAPS == 4 );
		return {
			xyz,
			st,
			{ lightmap, Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ) },
			normal,
			{ color, Color4b( 0, 0, 0, 0 ), Color4b( 0, 0, 0, 0 ), Color4b( 0, 0, 0, 0 ) }
		};
	}
};



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
	/* load the file */
	MemBuffer file = LoadFile( filename );

	ibspHeader_t    *header = file.data();

	/* swap the header (except the first 4 bytes) */
	SwapBlock( (int*) ( (byte*) header + 4 ), sizeof( *header ) - 4 );

	/* make sure it matches the format we're trying to load */
	if ( !force && memcmp( header->ident, g_game->bspIdent, 4 ) ) {
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
	CopyLump<bspDrawVert_t, ibspDrawVert_t>( (bspHeader_t*) header, LUMP_DRAWVERTS, bspDrawVerts );
	CopyLump<bspDrawSurface_t, ibspDrawSurface_t>( (bspHeader_t*) header, LUMP_SURFACES, bspDrawSurfaces );
	CopyLump( (bspHeader_t*) header, LUMP_FOGS, bspFogs );
	CopyLump( (bspHeader_t*) header, LUMP_DRAWINDEXES, bspDrawIndexes );
	CopyLump( (bspHeader_t*) header, LUMP_VISIBILITY, bspVisBytes );
	CopyLump( (bspHeader_t*) header, LUMP_LIGHTMAPS, bspLightBytes );
	CopyLump( (bspHeader_t*) header, LUMP_ENTITIES, bspEntData );
	CopyLump<bspGridPoint_t, ibspGridPoint_t>( (bspHeader_t*) header, LUMP_LIGHTGRID, bspGridPoints );

	/* advertisements */
	if ( header->version == 47 && strEqual( g_game->arg, "quakelive" ) ) { // quake live's bsp version minus wolf, et, etut
		CopyLump( (bspHeader_t*) header, LUMP_ADVERTISEMENTS, bspAds );
	}
	else{
		bspAds.clear();
	}
}

/*
   LoadIBSPorRBSPFilePartially()
   loads bsp file parts meaningful for autopacker
 */

void LoadIBSPorRBSPFilePartially( const char *filename ){
	/* load the file */
	MemBuffer file = LoadFile( filename );

	ibspHeader_t    *header = file.data();

	/* swap the header (except the first 4 bytes) */
	SwapBlock( (int*) ( (byte*) header + 4 ), sizeof( *header ) - 4 );

	/* make sure it matches the format we're trying to load */
	if ( !force && memcmp( header->ident, g_game->bspIdent, 4 ) ) {
		Error( "%s is not a %s file", filename, g_game->bspIdent );
	}
	if ( !force && header->version != g_game->bspVersion ) {
		Error( "%s is version %d, not %d", filename, header->version, g_game->bspVersion );
	}

	/* load/convert lumps */
	CopyLump( (bspHeader_t*) header, LUMP_SHADERS, bspShaders );
	if( g_game->load == LoadIBSPFile )
		CopyLump<bspDrawSurface_t, ibspDrawSurface_t>( (bspHeader_t*) header, LUMP_SURFACES, bspDrawSurfaces );
	else
		CopyLump( (bspHeader_t*) header, LUMP_SURFACES, bspDrawSurfaces );

	CopyLump( (bspHeader_t*) header, LUMP_FOGS, bspFogs );
	CopyLump( (bspHeader_t*) header, LUMP_ENTITIES, bspEntData );
}

/*
   WriteIBSPFile()
   writes an id bsp file
 */

void WriteIBSPFile( const char *filename ){
	ibspHeader_t header{};

	//%	Swapfile();

	/* set up header */
	memcpy( header.ident, g_game->bspIdent, 4 );
	header.version = LittleLong( g_game->bspVersion );

	/* write initial header */
	FILE *file = SafeOpenWrite( filename );
	SafeWrite( file, &header, sizeof( header ) );    /* overwritten later */

	{ /* add marker lump */
		time_t t;
		time( &t );
		/* asctime adds an implicit trailing \n */
		const auto marker = StringStream( "I LOVE MY Q3MAP2 " Q3MAP_VERSION " on ", asctime( localtime( &t ) ) );
		AddLump( file, header.lumps[0], std::vector<char>( marker.cbegin(), marker.cend() + 1 ) );
	}

	/* add lumps */
	AddLump( file, header.lumps[LUMP_SHADERS], bspShaders );
	AddLump( file, header.lumps[LUMP_PLANES], bspPlanes );
	AddLump( file, header.lumps[LUMP_LEAFS], bspLeafs );
	AddLump( file, header.lumps[LUMP_NODES], bspNodes );
	AddLump( file, header.lumps[LUMP_BRUSHES], bspBrushes );
	AddLump( file, header.lumps[LUMP_BRUSHSIDES], std::vector<ibspBrushSide_t>( bspBrushSides.begin(), bspBrushSides.end() ) );
	AddLump( file, header.lumps[LUMP_LEAFSURFACES], bspLeafSurfaces );
	AddLump( file, header.lumps[LUMP_LEAFBRUSHES], bspLeafBrushes );
	AddLump( file, header.lumps[LUMP_MODELS], bspModels );
	AddLump( file, header.lumps[LUMP_DRAWVERTS], std::vector<ibspDrawVert_t>( bspDrawVerts.begin(), bspDrawVerts.end() ) );
	AddLump( file, header.lumps[LUMP_SURFACES], std::vector<ibspDrawSurface_t>( bspDrawSurfaces.begin(), bspDrawSurfaces.end() ) );
	AddLump( file, header.lumps[LUMP_VISIBILITY], bspVisBytes );
	AddLump( file, header.lumps[LUMP_LIGHTMAPS], bspLightBytes );
	AddLump( file, header.lumps[LUMP_LIGHTGRID], std::vector<ibspGridPoint_t>( bspGridPoints.begin(), bspGridPoints.end() ) );
	AddLump( file, header.lumps[LUMP_ENTITIES], bspEntData );
	AddLump( file, header.lumps[LUMP_FOGS], bspFogs );
	AddLump( file, header.lumps[LUMP_DRAWINDEXES], bspDrawIndexes );

	/* advertisements */
	AddLump( file, header.lumps[LUMP_ADVERTISEMENTS], bspAds );

	/* emit bsp size */
	const int size = ftell( file );
	Sys_Printf( "Wrote %.1f MB (%d bytes)\n", (float) size / ( 1024 * 1024 ), size );

	/* write the completed header */
	fseek( file, 0, SEEK_SET );
	SafeWrite( file, &header, sizeof( header ) );

	/* close the file */
	fclose( file );
}

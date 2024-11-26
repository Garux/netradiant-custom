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
#define LUMP_LIGHTARRAY     17
#define HEADER_LUMPS        18


/* types */
struct rbspHeader_t
{
	char ident[ 4 ];
	int version;

	bspLump_t lumps[ HEADER_LUMPS ];
};



/* light grid */
#define MAX_MAP_GRID        0xffff
#define MAX_MAP_GRIDARRAY   0x100000
#define LG_EPSILON          4


static void CopyLightGridLumps( rbspHeader_t *header ){
	std::vector<bspGridPoint_t> gridPoints;
	std::vector<unsigned short> gridArray;
	CopyLump( (bspHeader_t*) header, LUMP_LIGHTGRID, gridPoints );
	CopyLump( (bspHeader_t*) header, LUMP_LIGHTARRAY, gridArray );

	bspGridPoints.clear();
	bspGridPoints.reserve( gridArray.size() );

	for( const auto id : gridArray )
		bspGridPoints.push_back( gridPoints[ id ] );
}


static void AddLightGridLumps( FILE *file, rbspHeader_t& header ){
	/* allocate temporary buffers */
	const size_t maxGridPoints = std::min( bspGridPoints.size(), size_t( MAX_MAP_GRID ) );
	std::vector<bspGridPoint_t> gridPoints;
	std::vector<unsigned short> gridArray( bspGridPoints.size() );

	/* for each bsp grid point, find an approximate twin */
	Sys_Printf( "Storing lightgrid: %zu points\n", bspGridPoints.size() );
	for ( size_t i = 0; i < gridArray.size(); ++i )
	{
		/* get points */
		const bspGridPoint_t& in = bspGridPoints[ i ];

		/* walk existing list */
		size_t j;
		for ( j = 0; j < gridPoints.size(); ++j )
		{
			/* get point */
			const bspGridPoint_t& out = gridPoints[ j ];

			/* compare styles */
			if ( memcmp( in.styles, out.styles, MAX_LIGHTMAPS ) ) {
				continue;
			}

			/* compare direction */
			if ( const int d = abs( in.latLong[ 0 ] - out.latLong[ 0 ] );
				d < ( 255 - LG_EPSILON ) && d > LG_EPSILON ) {
				continue;
			}
			if ( const int d = abs( in.latLong[ 1 ] - out.latLong[ 1 ] );
				d < 255 - LG_EPSILON && d > LG_EPSILON ) {
				continue;
			}

			/* compare light */
			bool bad = false;
			for ( int k = 0; ( k < MAX_LIGHTMAPS && !bad ); k++ )
			{
				for ( int c = 0; c < 3; c++ )
				{
					if ( abs( (int) in.ambient[ k ][ c ] - (int) out.ambient[ k ][ c ] ) > LG_EPSILON ||
					     abs( (int) in.directed[ k ][ c ] - (int) out.directed[ k ][ c ] ) > LG_EPSILON ) {
						bad = true;
						break;
					}
				}
			}

			/* failure */
			if ( bad ) {
				continue;
			}

			/* this sample is ok */
			break;
		}

		/* set sample index */
		gridArray[ i ] = (unsigned short) j;

		/* if no sample found, add a new one */
		if ( j >= gridPoints.size() && gridPoints.size() < maxGridPoints ) {
			gridPoints.push_back( in );
		}
	}

	/* swap array */
	for ( auto&& a : gridArray )
		a = LittleShort( a );

	/* write lumps */
	AddLump( file, header.lumps[LUMP_LIGHTGRID], gridPoints );
	AddLump( file, header.lumps[LUMP_LIGHTARRAY], gridArray );
}



/*
   LoadRBSPFile()
   loads a raven bsp file into memory
 */

void LoadRBSPFile( const char *filename ){
	/* load the file */
	MemBuffer file = LoadFile( filename );

	rbspHeader_t    *header = file.data();

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
	CopyLump( (bspHeader_t*) header, LUMP_BRUSHSIDES, bspBrushSides );
	CopyLump( (bspHeader_t*) header, LUMP_DRAWVERTS, bspDrawVerts );
	CopyLump( (bspHeader_t*) header, LUMP_SURFACES, bspDrawSurfaces );
	CopyLump( (bspHeader_t*) header, LUMP_FOGS, bspFogs );
	CopyLump( (bspHeader_t*) header, LUMP_DRAWINDEXES, bspDrawIndexes );
	CopyLump( (bspHeader_t*) header, LUMP_VISIBILITY, bspVisBytes );
	CopyLump( (bspHeader_t*) header, LUMP_LIGHTMAPS, bspLightBytes );
	CopyLump( (bspHeader_t*) header, LUMP_ENTITIES, bspEntData );
	CopyLightGridLumps( header );
}



/*
   WriteRBSPFile()
   writes a raven bsp file
 */

void WriteRBSPFile( const char *filename ){
	rbspHeader_t header{};

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
	AddLump( file, header.lumps[LUMP_BRUSHSIDES], bspBrushSides );
	AddLump( file, header.lumps[LUMP_LEAFSURFACES], bspLeafSurfaces );
	AddLump( file, header.lumps[LUMP_LEAFBRUSHES], bspLeafBrushes );
	AddLump( file, header.lumps[LUMP_MODELS], bspModels );
	AddLump( file, header.lumps[LUMP_DRAWVERTS], bspDrawVerts );
	AddLump( file, header.lumps[LUMP_SURFACES], bspDrawSurfaces );
	AddLump( file, header.lumps[LUMP_VISIBILITY], bspVisBytes );
	AddLump( file, header.lumps[LUMP_LIGHTMAPS], bspLightBytes );
	AddLightGridLumps( file, header );
	AddLump( file, header.lumps[LUMP_ENTITIES], bspEntData );
	AddLump( file, header.lumps[LUMP_FOGS], bspFogs );
	AddLump( file, header.lumps[LUMP_DRAWINDEXES], bspDrawIndexes );

	/* emit bsp size */
	const int size = ftell( file );
	Sys_Printf( "Wrote %.1f MB (%d bytes)\n", (float) size / ( 1024 * 1024 ), size );

	/* write the completed header */
	fseek( file, 0, SEEK_SET );
	SafeWrite( file, &header, sizeof( header ) );

	/* close the file */
	fclose( file );
}

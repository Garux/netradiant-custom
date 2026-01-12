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
		planeNum ( other.planeNum ),
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
	std::array<Vector3, 3> lightmapVecs;

	int patchWidth;
	int patchHeight;
	ibspDrawSurface_t( const bspDrawSurface_t& other ) :
		shaderNum     ( other.shaderNum ),
		fogNum        ( other.fogNum ),
		surfaceType   ( other.surfaceType ),
		firstVert     ( other.firstVert ),
		numVerts      ( other.numVerts ),
		firstIndex    ( other.firstIndex ),
		numIndexes    ( other.numIndexes ),
		lightmapNum   ( other.lightmapNum[0] ),
		lightmapX     ( other.lightmapX[0] ),
		lightmapY     ( other.lightmapY[0] ),
		lightmapWidth ( other.lightmapWidth ),
		lightmapHeight( other.lightmapHeight ),
		lightmapOrigin( other.lightmapOrigin ),
		lightmapVecs  ( other.lightmapVecs ),
		patchWidth    ( other.patchWidth ),
		patchHeight   ( other.patchHeight )
	{}
	operator bspDrawSurface_t() const {
		return bspDrawSurface_t{
			.shaderNum      = shaderNum,
			.fogNum         = fogNum,
			.surfaceType    = surfaceType,
			.firstVert      = firstVert,
			.numVerts       = numVerts,
			.firstIndex     = firstIndex,
			.numIndexes     = numIndexes,
			.lightmapStyles { LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			.vertexStyles   { LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			.lightmapNum    { lightmapNum, LIGHTMAP_BY_VERTEX, LIGHTMAP_BY_VERTEX, LIGHTMAP_BY_VERTEX },
			.lightmapX      { lightmapX, 0, 0, 0 },
			.lightmapY      { lightmapY, 0, 0, 0 },
			.lightmapWidth  = lightmapWidth,
			.lightmapHeight = lightmapHeight,
			.lightmapOrigin = lightmapOrigin,
			.lightmapVecs   = lightmapVecs,
			.patchWidth     = patchWidth,
			.patchHeight    = patchHeight
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
		xyz     ( other.xyz ),
		st      ( other.st ),
		lightmap( other.lightmap[0] ),
		normal  ( other.normal ),
		color   ( other.color[0] )
	{}
	operator bspDrawVert_t() const {
		return bspDrawVert_t{
			.xyz     = xyz,
			.st      = st,
			.lightmap{ lightmap, Vector2( 0 ), Vector2( 0 ), Vector2( 0 ) },
			.normal  = normal,
			.color   { color, Color4b( 0 ), Color4b( 0 ), Color4b( 0 ) }
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
		ambient ( other.ambient[0] ),
		directed( other.directed[0] ),
		latLong { other.latLong[0], other.latLong[1] }
	{}
	operator bspGridPoint_t() const {
		return bspGridPoint_t{
			.ambient  = makeArray4( ambient ),
			.directed = makeArray4( directed ),
			.styles   { LS_NORMAL, LS_NONE, LS_NONE, LS_NONE },
			.latLong  { latLong[0], latLong[1] }
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




using uint = std::uint32_t;

struct XbspPlane_t
{
	bspPlane_t plane;
	int idk;
	operator bspPlane_t() const {
		return plane;
	}
};

struct cLeaf_t
{
	int			cluster;
	int			area;

	int			firstLeafBrush;
	int			numLeafBrushes;

	int			firstLeafSurface;
	int			numLeafSurfaces;

	uint leafBrush;
	uint leafSurf;

	operator bspLeaf_t() const {
		return bspLeaf_t{
			.cluster = cluster,
			.area = area,
			.firstBSPLeafSurface = firstLeafSurface,
			.numBSPLeafSurfaces = numLeafSurfaces,
			.firstBSPLeafBrush = firstLeafBrush,
			.numBSPLeafBrushes = numLeafBrushes
		};
};

};

struct cmodel_t
{
	MinMax minmax;
	cLeaf_t		leaf;			// submodels don't reference the main tree
};

struct cbrush_t
{
	int			shaderNum;		// the shader that determined the contents
	int			contents;
	MinMax minmax;
	int			numsides;
	uint	sidesPTR;
	int			checkcount;		// to avoid repeated testings
};

struct XBrushSide
{
	uint planePTR; //	Plane index.
	int			surfaceFlags;
	int			shaderNum; //	Texture index.
};





struct Clip
{
	char name[64];
	int datasize;	// -8917903  4286049393
	int nShaders;	// 67
	int offShaders;	// 183238916		1
	int nBSides;	// 22064

	int offBSides;	// 183243740		2
	int nPlanes;	// 11400
	int offPlanes;	// 183508580		3
	int nC;			// 998		sz12

	int offC;		// 183736820		4
	int nLeafs;		// 1042		sz32
	int offLeafs;	// 183748796		5
	int nLeafBrushes;// 7094		sz4

	int offLeafbrushes;// 183782204		6
	int nF;			// 46		sz4
	int offF;		// 183810584		7
	int nLeafSurfaces;// 7647		sz4

	int offLeafSurfaces;// 183810768		8
	int nH;			// 91		sz4
	int offH;		// 183841356		9
	int nModels;	// 43		sz56

	int offModels;	// 184050596		16
	int nBrushes;	// 2437		sz44
	int offBrushes;	// 183841720		10
	int nK;			// 29184

	int nL;			// 456
	int nM;			// 64
	int offM;		// 183948992 ?vis	11
	int nN;			// 1

	int nEnts;		// 42492
	int offEnts;	// 183978176		12
	int nO;			// 5		sz8
	int offO;		// 184020668		13

	int offP;		// 184020708		14
	int nSurfaces;	// 2664
	int offSurfaces;// 184020808		15					// pointer to array of pointers to cPatch_t // non-patches will be NULL
	int nR;			// 1

	int nS;			// 0
};


struct XSurface
{
	int					viewCount;		// if == tr.viewCount, already added
	uint				offShader;
	int					fogIndex;
	int					surfType;
	int					offSurf;			// any of srf*_t
};

using XVertex = ibspDrawVert_t;
struct XVertexPlanar
{
	Vector3 xyz;
	Vector2 st;
	Vector2 lightmap;
//	Vector3 normal;
	Color4b color;
	operator bspDrawVert_t() const {
		return bspDrawVert_t{
			.xyz     = xyz,
			.st      = st,
			.lightmap{ lightmap, Vector2( 0 ), Vector2( 0 ), Vector2( 0 ) },
			.normal  = Vector3( 0 ),
			.color   { color, Color4b( 0 ), Color4b( 0 ), Color4b( 0 ) }
		};
	}
};



struct World
{
	char		name[64];		// ie: maps/tim_dm2.bsp
	char		baseName[64];	// ie: tim_dm2
//---
	int			dataSize;

	int			numShaders;
	uint		offShaders;

	int			nModels;					// 43 sz32
//---
	uint		offBmodels;					// 166134168	2

	int			numplanes;
	uint		offPlanes;

	int			numnodes;		// includes leafs
//---
	int			numDecisionNodes;
	uint		offNodes;					// 167112188	4

	int			numsurfaces;				// 2664
	uint		offSurfaces;				// 166135544	3
//---
	int			nummarksurfaces;
	uint		offMarksurfaces;			// 167242748	5

	int			numfogs;					// 1 sz72
	uint		offFogs;					// 166134096	1
//---
	float		lightGridOrigin[3];
	float		lightGridSize[3];
	float		lightGridInverseSize[3];
	int			lightGridBounds[3];
//---
	uint		offLightGridData;			// 167273336	6


	int			numClusters;
	int			clusterBytes;
	int idk;
//---
	uint		offVis;			// may be passed in by CM_LoadMap to save space
	int idk0[3];
//---
	uint		offNovis;			// clusterBytes of 0xff

	uint		offEntityString;
	int idk_0;
	uint		offEntityParsePoint;		// 167582840	7
};



void LoadXboxFile( const char *filename ){
	MemBuffer fcachemap = LoadFile( filename );
	MemBuffer fclip = LoadFile( StringStream( PathFilenameless( filename ), "clip" ) );
	MemBuffer fworld = LoadFile( StringStream( PathFilenameless( filename ), "world" ) );

	{
		Clip *clip = fclip.data();
		const int OFF = clip->offShaders - 212;

		bspEntData = {
			( char* )( (byte*)fclip.data() + clip->offEnts - OFF ),
			( char* )( (byte*)fclip.data() + clip->offEnts - OFF ) + clip->nEnts
		};
		bspShaders = {
			( bspShader_t* )( (byte*)fclip.data() + clip->offShaders - OFF ),
			( bspShader_t* )( (byte*)fclip.data() + clip->offShaders - OFF ) + clip->nShaders
		};
		bspPlanes = {
			( XbspPlane_t* )( (byte*)fclip.data() + clip->offPlanes - OFF ),
			( XbspPlane_t* )( (byte*)fclip.data() + clip->offPlanes - OFF ) + clip->nPlanes
		};
		bspLeafSurfaces = {
			( int* )( (byte*)fclip.data() + clip->offLeafSurfaces - OFF ),
			( int* )( (byte*)fclip.data() + clip->offLeafSurfaces - OFF ) + clip->nLeafSurfaces
		};
		bspLeafBrushes = {
			( int* )( (byte*)fclip.data() + clip->offLeafbrushes - OFF ),
			( int* )( (byte*)fclip.data() + clip->offLeafbrushes - OFF ) + clip->nLeafBrushes
		};
		bspLeafs = {
			( cLeaf_t* )( (byte*)fclip.data() + clip->offLeafs - OFF ),
			( cLeaf_t* )( (byte*)fclip.data() + clip->offLeafs - OFF ) + clip->nLeafs
		};

		Span cmodels( ( cmodel_t* )( (byte*)fclip.data() + clip->offModels - OFF ), clip->nModels );
		for( auto& cmodel : cmodels ){
			bspModels.push_back( bspModel_t{ .minmax = cmodel.minmax,
				.firstBSPSurface = 0, .numBSPSurfaces = cmodel.leaf.numLeafSurfaces,
				.firstBSPBrush = 0, .numBSPBrushes = cmodel.leaf.numLeafBrushes } );
			if( cmodel.leaf.numLeafSurfaces != 0 ){
				bspModels.back().firstBSPSurface = *( int* )( (byte*)fclip.data() + ( cmodel.leaf.leafSurf & 0x00FFFFFF ) );
				if( bspModels.front().numBSPSurfaces > bspModels.back().firstBSPSurface || bspModels.front().numBSPSurfaces == 0 ){
					bspModels.front().numBSPSurfaces = bspModels.back().firstBSPSurface;
				}
			}
			if( cmodel.leaf.numLeafBrushes != 0 ){
				bspModels.back().firstBSPBrush = *( int* )( (byte*)fclip.data() + ( cmodel.leaf.leafBrush & 0x00FFFFFF ) );
				if( bspModels.front().numBSPBrushes > bspModels.back().firstBSPBrush || bspModels.front().numBSPBrushes == 0 ){
					bspModels.front().numBSPBrushes = bspModels.back().firstBSPBrush;
				}
			}
		}

		Span cbrushes( ( cbrush_t* )( (byte*)fclip.data() + clip->offBrushes - OFF ), clip->nBrushes );
		for( auto& cbrush : cbrushes ){
			bspBrushes.push_back( bspBrush_t{ .firstSide = 0, .numSides = cbrush.numsides, .shaderNum = cbrush.shaderNum } );
			bspBrushes.back().firstSide = ( ( cbrush.sidesPTR & 0x00FFFFFF ) - ( clip->offBSides - OFF ) ) / sizeof( XBrushSide );
		}

		Span xbrushsides( ( XBrushSide* )( (byte*)fclip.data() + clip->offBSides - OFF ), clip->nBSides );
		for( auto& xbs : xbrushsides ){
			bspBrushSides.push_back( bspBrushSide_t{ .planeNum = 0, .shaderNum = xbs.shaderNum, .surfaceNum = 0 } );
			bspBrushSides.back().planeNum = ( ( xbs.planePTR & 0x00FFFFFF ) - ( clip->offPlanes - OFF ) ) / sizeof( XbspPlane_t );
		}
	}

	{
		World *world = fworld.data();
		const int OFF = world->offFogs - 288;

		// note this is start of 0008.block.04
		const char *shadersSatrt = std::ranges::find_if( Span( (const char*)fcachemap.data(), fcachemap.size() ), []( const char& s ){
			return strEqual( &s, "<default>" );
		} ).operator->() - 572;

		Span xsurfs( ( XSurface* )( (byte*)fworld.data() + world->offSurfaces - OFF ), world->numsurfaces );
		for( auto& xsurf : xsurfs ){
			bspDrawSurface_t& surf = bspDrawSurfaces.emplace_back( bspDrawSurface_t{ .surfaceType = xsurf.surfType - 1 } );
			String64 shader( shadersSatrt + ( xsurf.offShader & 0x00FFFFFF ) );
			// String64 shader( (char*)fcachemap.data() + xsurf.offShader - 2193354400u );
			for( size_t i = 0; i != bspShaders.size(); ++i ){
				if( strEqual( shader, bspShaders[i].shader ) ){
					surf.shaderNum = i;
					break;
				}
			}

			if( surf.surfaceType == MST_PLANAR ) { // planar SF_FACE
				struct PLANAR {
					int surfType;
					int idk[7];
					int numVerts;
					int numIndices;
					int idk2;
					XVertexPlanar verts; //[numVerts];
					int indices; //[numIndices];
				} *planar = ( PLANAR* )( (byte*)fworld.data() + xsurf.offSurf - OFF );
				surf.numVerts = planar->numVerts;
				surf.numIndexes = planar->numIndices;
				surf.firstVert = bspDrawVerts.size();
				surf.firstIndex = bspDrawIndexes.size();
				for( auto& v : Span( &planar->verts, planar->numVerts ) )
					bspDrawVerts.push_back( v );
				for( auto& i : Span( (int*)( &planar->verts + planar->numVerts ), planar->numIndices ) )
					bspDrawIndexes.push_back( i );
			}
			else if( surf.surfaceType == MST_PATCH ) { // patches SF_GRID
				struct PATCH {
					int					surfType;
					int idk[18];
					int width, height;
					int idk2[2];
					XVertex verts; //[width * height]; // !! NOTE width, height may be optimized and be even
				} *patch = ( PATCH* )( (byte*)fworld.data() + xsurf.offSurf - OFF );

				/* not a case in the maps */
				if( ( patch->height | 1 ) >= MAX_PATCH_SIZE ){
					Sys_Warning( "big patch %dx%d: ignoring\n", patch->width, patch->height );
					surf.surfaceType = MST_BAD;
					continue;
				}
				if( ( patch->width | 1 ) >= MAX_PATCH_SIZE ){
					Sys_Warning( "big patch %dx%d: splitting\n", patch->width, patch->height );

					mesh_t mesh( patch->width | 1, patch->height | 1 );

					XVertex *v = &patch->verts;
					for( int h = 0; h < patch->height; ++h ){
						for( int w = 0; w < mesh.width; ++w ){
							mesh[h][w] = *v;
							if( !( !( patch->width & 1 ) && w == ( patch->width - 1 ) ) ){
								++v;
							}
						}
					}
					if( !( patch->height & 1 ) ){
						for( int w = 0; w < mesh.width; ++w )
							mesh[patch->height][w] = mesh[patch->height - 1][w];
					}

					const int wsplit = ( mesh.width >> 1 ) | 1;
					surf.patchWidth = wsplit;
					surf.patchHeight = mesh.height;
					surf.firstVert = bspDrawVerts.size();
					for( int h = 0; h < surf.patchHeight; ++h ){
						for( int w = 0; w < surf.patchWidth; ++w ){
							bspDrawVerts.push_back( mesh[h][w] );
						}
					}

					bspDrawSurface_t& surf2 = bspDrawSurfaces.emplace_back( bspDrawSurface_t( surf ) ); // avoid dead reference on realloc
					surf2.patchWidth = mesh.width - ( wsplit - 1 );
					surf2.patchHeight = mesh.height;
					surf2.firstVert = bspDrawVerts.size();
					for( int h = 0; h < surf2.patchHeight; ++h ){
						for( int w = ( wsplit - 1 ); w < mesh.width; ++w ){
							bspDrawVerts.push_back( mesh[h][w] );
						}
					}

					/* fix surfaces indexing... */
					const int surfidx = bspDrawSurfaces.size() - 2; // 1st patch index
					// bspLeafSurfaces - skip - are not needed to decompile
					for( auto& model : bspModels ){
						if( model.firstBSPSurface > surfidx )
							++model.firstBSPSurface;
						if( model.firstBSPSurface <= surfidx
						 && model.firstBSPSurface + model.numBSPSurfaces > surfidx )
							++model.numBSPSurfaces;
					}

					continue;
				}

				surf.patchWidth = patch->width;
				surf.patchHeight = patch->height;
				surf.firstVert = bspDrawVerts.size();
				if( patch->width & 1 && patch->height & 1 ){
					for( auto& v : Span( &patch->verts, patch->width * patch->height ) )
						bspDrawVerts.push_back( v );
				}
				else{
					surf.patchWidth |= 1;
					surf.patchHeight |= 1;
					XVertex *v = &patch->verts;
					for( int h = 0; h < patch->height; ++h ){
						for( int w = 0; w < surf.patchWidth; ++w ){
							bspDrawVerts.push_back( *v );
							if( !( !( patch->width & 1 ) && w == ( patch->width - 1 ) ) ){
								++v;
							}
						}
					}
					if( !( patch->height & 1 ) ){
						for( int i = bspDrawVerts.size() - surf.patchWidth, end = bspDrawVerts.size(); i < end; ++i )
							bspDrawVerts.push_back( bspDrawVert_t( bspDrawVerts[i] ) ); // avoid dead reference on realloc
					}
				}
			}
			else if( surf.surfaceType == MST_TRIANGLE_SOUP ) { // trisoup SF_TRIANGLES
				struct TRISOUP {
					int surfType;
					int idk[11];
					int numIndices;
					int offIndices; // -OFF -> short
					int numVerts;
					int offVerts;   // -OFF -> XVertex
				} *trisoup = ( TRISOUP* )( (byte*)fworld.data() + xsurf.offSurf - OFF );
				surf.numVerts = trisoup->numVerts;
				surf.numIndexes = trisoup->numIndices;
				surf.firstVert = bspDrawVerts.size();
				surf.firstIndex = bspDrawIndexes.size();
				for( auto& v : Span( ( XVertex* )( (byte*)fworld.data() + trisoup->offVerts - OFF ), trisoup->numVerts ) )
					bspDrawVerts.push_back( v );
				for( auto& i : Span( ( std::int16_t* )( (byte*)fworld.data() + trisoup->offIndices - OFF ), trisoup->numIndices ) )
					bspDrawIndexes.push_back( i );
			}
		}
	}
}


#pragma once

#include "mathlib.h"

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
#define HEADER_LUMPS        17

typedef struct {
	int fileofs, filelen;
} lump_t;

typedef struct {
	int ident;
	int version;

	lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float normal[3];
	float dist;
} dplane_t;

typedef struct {
	int planeNum;
	int children[2];            // negative numbers are -(leafs+1), not nodes
	int mins[3];                // for frustum culling
	int maxs[3];
} dnode_t;

typedef struct {
	int cluster;                    // -1 = opaque cluster (do I still store these?)
	int area;

	int mins[3];                    // for frustum culling
	int maxs[3];

	int firstLeafSurface;
	int numLeafSurfaces;

	int firstLeafBrush;
	int numLeafBrushes;
} dleaf_t;

typedef struct {
	vec3_t xyz;
	float st[2];
	float lightmap[2];
	vec3_t normal;
	byte color[4];
} qdrawVert_t;

#define MAX_LIGHTMAPS 4

struct rbspDrawVert_t
{
	vec3_t xyz;
	float st[2];
	float lightmap[ MAX_LIGHTMAPS ][2];         /* RBSP */
	vec3_t normal;
	byte color[ MAX_LIGHTMAPS ][4];             /* RBSP */
	operator qdrawVert_t() const {
		return{
			{ xyz[0], xyz[1], xyz[2] },
			{ st[0], st[1] },
			{ lightmap[0][0], lightmap[0][1] },
			{ normal[0], normal[1], normal[2] },
			{ color[0][0], color[0][1], color[0][2], color[0][3] }
		};
	}
};

typedef struct {
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

	vec3_t lightmapOrigin;
	vec3_t lightmapVecs[3];         // for patches, [0] and [1] are lodbounds

	int patchWidth;
	int patchHeight;
} dsurface_t;

struct rbspDrawSurface_t
{
	int shaderNum;
	int fogNum;
	int surfaceType;

	int firstVert;
	int numVerts;

	int firstIndex;
	int numIndexes;

	byte lightmapStyles[ MAX_LIGHTMAPS ];                               /* RBSP */
	byte vertexStyles[ MAX_LIGHTMAPS ];                                 /* RBSP */
	int lightmapNum[ MAX_LIGHTMAPS ];                                   /* RBSP */
	int lightmapX[ MAX_LIGHTMAPS ], lightmapY[ MAX_LIGHTMAPS ];         /* RBSP */
	int lightmapWidth, lightmapHeight;

	vec3_t lightmapOrigin;
	vec3_t lightmapVecs[ 3 ];       /* on patches, [ 0 ] and [ 1 ] are lodbounds */

	int patchWidth;
	int patchHeight;
	operator dsurface_t() const {
		return{
			shaderNum,
			fogNum,
			surfaceType,
			firstVert,
			numVerts,
			firstIndex,
			numIndexes,
			lightmapNum[0],
			lightmapX[0], lightmapY[0],
			lightmapWidth, lightmapHeight,
			{ lightmapOrigin[0], lightmapOrigin[1], lightmapOrigin[2] },
			{ { lightmapVecs[0][0], lightmapVecs[0][1], lightmapVecs[0][2] },
			  { lightmapVecs[1][0], lightmapVecs[1][1], lightmapVecs[1][2] },
			  { lightmapVecs[2][0], lightmapVecs[2][1], lightmapVecs[2][2] } },
			patchWidth,
			patchHeight
		};
	}
};

typedef struct {
	int planeNum;                   // positive plane side faces out of the leaf
	int shaderNum;
} dbrushside_t;

struct rbspBrushSide_t
{
	int planeNum;                   /* positive plane side faces out of the leaf */
	int shaderNum;
	int surfaceNum;                 /* RBSP */
	operator dbrushside_t() const {
		return { planeNum, shaderNum };
	}
};

typedef struct {
	int firstSide;
	int numSides;
	int shaderNum;              // the shader that determines the contents flags
} dbrush_t;

typedef enum {
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE
} mapSurfaceType_t;

#define MAX_MAP_VISIBILITY  0x200000
#define MAX_MAP_NODES       0x20000
#define MAX_MAP_PLANES      0x100000
#define MAX_MAP_LEAFS       0x20000

extern int numVisBytes;
extern int numleafs;
extern int numplanes;
extern int numnodes;
extern int numDrawVerts;
extern int numDrawVertsIndices;
extern int numDrawSurfaces;
extern int numleafsurfaces;
extern int numbrushes;
extern int numbrushsides;
extern int numleafbrushes;

extern dnode_t         *dnodes;
extern dplane_t        *dplanes;
extern dleaf_t         *dleafs;
extern byte            *visBytes;
extern qdrawVert_t     *drawVerts;
extern int             *drawVertsIndices;
extern dsurface_t      *drawSurfaces;
extern int             *dleafsurfaces;
extern dbrush_t        *dbrushes;
extern dbrushside_t    *dbrushsides;
extern int             *dleafbrushes;

bool LoadBSPFile( const char *filename );
void FreeBSPData();

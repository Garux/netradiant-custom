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


#pragma once


/* version */
#ifndef Q3MAP_VERSION
#error no Q3MAP_VERSION defined
#endif
#define Q3MAP_MOTD      "Your map saw the pretty lights from q3map2's BFG"




/* -------------------------------------------------------------------------------

   dependencies

   ------------------------------------------------------------------------------- */


/* general */
#include "version.h"            /* ttimo: might want to guard that if built outside of the GtkRadiant tree */

#include "cmdlib.h"
#include "qstringops.h"
#include "qpathops.h"

#include "scriplib.h"
#include "polylib.h"
#include "qimagelib.h"
#include "qthreads.h"
#include "inout.h"
#include "inout_xml.h"
#include "vfs.h"
#include "md4.h"

#include "stringfixedsize.h"
#include "stream/stringstream.h"
#include "bitflags.h"
#include <list>
#include <forward_list>
#include <algorithm>
#include "qmath.h"
#include "unsortedset.h"

#include <cstddef>
#include <cstdlib>

#include "maxworld.h"
#include "games.h"


/* -------------------------------------------------------------------------------

   constants

   ------------------------------------------------------------------------------- */

/* temporary hacks and tests (please keep off in SVN to prevent anyone's legacy map from screwing up) */
/* 2011-01-10 TTimo says we should turn these on in SVN, so turning on now */
#define Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES   1
#define Q3MAP2_EXPERIMENTAL_SNAP_NORMAL_FIX     1
#define Q3MAP2_EXPERIMENTAL_SNAP_PLANE_FIX      1

/* general */
// actual shader name length limit depends on game engine and name use manner (plain texture/custom shader)
// now checking it for strlen() < MAX_QPATH (so it's null terminated), though this check may be not enough/too much, depending on the use case
#define MAX_QPATH               64

#define DEFAULT_IMAGE           "*default"

#define DEF_BACKSPLASH_FRACTION 0.05f   /* 5% backsplash by default */
#define DEF_BACKSPLASH_DISTANCE 23

#define DEF_RADIOSITY_BOUNCE    1.0f    /* ydnar: default to 100% re-emitted light */


/* epair parsing (note case-sensitivity directive) */
#define CASE_INSENSITIVE_EPAIRS 1

#if CASE_INSENSITIVE_EPAIRS
	#define EPAIR_EQUAL        striEqual
#else
	#define EPAIR_EQUAL        strEqual
#endif


/* shadow flags */
#define WORLDSPAWN_CAST_SHADOWS 1
#define WORLDSPAWN_RECV_SHADOWS 1
#define ENTITY_CAST_SHADOWS     0
#define ENTITY_RECV_SHADOWS     1


/* bsp */
#define MAX_PATCH_SIZE          31
#define MAX_BRUSH_SIDES         1024
#define MAX_BUILD_SIDES         1024

#define MAX_EXPANDED_AXIS       128

#define CLIP_EPSILON            0.1f
#define PLANESIDE_EPSILON       0.001f
#define PLANENUM_LEAF           -1
constexpr int AREA_INVALID    = -1;
constexpr int CLUSTER_OPAQUE  = -1;

enum class EBrushType
{
	Undefined,
	Quake,
	Bp,
	Valve220
};

/* vis */
#define VIS_HEADER_SIZE         8

#define SEPERATORCACHE          /* separator caching helps a bit */

#define PORTALFILE              "PRT1"

#define MAX_PORTALS             0x20000 /* same as MAX_MAP_PORTALS */
#define MAX_SEPERATORS          MAX_POINTS_ON_WINDING
#define MAX_POINTS_ON_FIXED_WINDING 24  /* ydnar: increased this from 12 at the expense of more memory */
#define MAX_PORTALS_ON_LEAF     1024


/* light */
#define MAX_TRACE_TEST_NODES    256
#define DEFAULT_INHIBIT_RADIUS  1.5f

#define LUXEL_EPSILON           0.125f
#define VERTEX_EPSILON          -0.125f
#define GRID_EPSILON            0.0f

#define DEFAULT_LIGHTMAP_SAMPLE_SIZE    16
#define DEFAULT_LIGHTMAP_MIN_SAMPLE_SIZE    0
#define DEFAULT_LIGHTMAP_SAMPLE_OFFSET  1.0f
#define DEFAULT_SUBDIVIDE_THRESHOLD     1.0f

#define CLUSTER_NORMAL           0
#define CLUSTER_UNMAPPED        -1
#define CLUSTER_OCCLUDED        -2
#define CLUSTER_FLOODED         -3

#define FLAG_FORCE_SUBSAMPLING  1
#define FLAG_ALREADY_SUBSAMPLED 2

#define MAX_SKIES               32 // how many light emitting skies can shine separately and only where they should


/* -------------------------------------------------------------------------------

   abstracted bsp file

   ------------------------------------------------------------------------------- */

#define EXTERNAL_LIGHTMAP       "lm_%04d.tga"

#define MAX_LIGHTMAPS           4           /* RBSP */
#define MAX_SWITCHED_LIGHTS     32
#define LS_NORMAL               0x00
#define LS_UNUSED               0xFE
#define LS_NONE                 0xFF

inline bool style_is_valid( int style ){ return LS_NORMAL <= style && style < LS_NONE; }

#define MAX_LIGHTMAP_SHADERS    256

/* ok to increase these at the expense of more memory */
#define MAX_MAP_AREAS           0x100       /* MAX_MAP_AREA_BYTES in q_shared must match! */
/* MAX_MAP_FOGS is technically unlimited in engine, but drawsurf sorting code only has 5 bits for fogs */
#define MAX_IBSP_FOGS           31          /* (2^5 - world fog) */
#define MAX_RBSP_FOGS           30          /* (2^5 - world fog - goggles) */
#define MAX_MAP_LEAFS           0x20000
#define MAX_MAP_PORTALS         0x20000
#define MAX_MAP_LIGHTING        0x800000
#define MAX_MAP_LIGHTGRID       0x100000    //%	0x800000 /* ydnar: set to points, not bytes */
#define MAX_MAP_VISCLUSTERS     0x4000 // <= MAX_MAP_LEAFS
#define MAX_MAP_VISIBILITY      ( VIS_HEADER_SIZE + MAX_MAP_VISCLUSTERS * ( ( ( MAX_MAP_VISCLUSTERS + 63 ) & ~63 ) >> 3 ) )

/* the editor uses these predefined yaw angles to orient entities up or down */
#define ANGLE_UP                -1
#define ANGLE_DOWN              -2

#define LIGHTMAP_WIDTH          128
#define LIGHTMAP_HEIGHT         128



struct bspLump_t
{
	int offset, length;
};


struct bspHeader_t
{
	char ident[ 4 ];
	int version;

	bspLump_t lumps[ 100 ];     /* theoretical maximum # of bsp lumps */
};


struct bspModel_t
{
	MinMax minmax;
	int firstBSPSurface, numBSPSurfaces;
	int firstBSPBrush, numBSPBrushes;
};


struct bspShader_t
{
	char shader[ MAX_QPATH ];
	int surfaceFlags;
	int contentFlags;
};


/* planes x^1 is always the opposite of plane x */

using bspPlane_t = Plane3f;


struct bspNode_t
{
	int planeNum;
	int children[ 2 ];              /* negative numbers are -(leafs+1), not nodes */
	MinMax___<int> minmax;          /* for frustum culling */
};


struct bspLeaf_t
{
	int cluster;                    /* -1 = opaque cluster (do I still store these?) */
	int area;

	MinMax___<int> minmax;          /* for frustum culling */

	int firstBSPLeafSurface;
	int numBSPLeafSurfaces;

	int firstBSPLeafBrush;
	int numBSPLeafBrushes;
};


struct bspBrushSide_t
{
	int planeNum;                   /* positive plane side faces out of the leaf */
	int shaderNum;
	int surfaceNum;                 /* RBSP */
};


struct bspBrush_t
{
	int firstSide;
	int numSides;
	int shaderNum;                  /* the shader that determines the content flags */
};


struct bspFog_t
{
	char shader[ MAX_QPATH ];
	int brushNum;
	int visibleSide;                /* the brush side that ray tests need to clip against (-1 == none) */
};


struct bspDrawVert_t
{
	Vector3 xyz;
	Vector2 st;
	Vector2 lightmap[ MAX_LIGHTMAPS ];          /* RBSP */
	Vector3 normal;
	Color4b color[ MAX_LIGHTMAPS ];             /* RBSP */
};

inline const bspDrawVert_t c_bspDrawVert_t0 =
{
	.xyz{ 0 },
	.st{ 0, 0 },
	.lightmap{ { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
	.normal{ 0 },
	.color{ { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }
};

using TriRef = std::array<const bspDrawVert_t *, 3>;
using QuadRef = std::array<const bspDrawVert_t *, 4>;

using DrawVerts = std::vector<bspDrawVert_t>;
using DrawIndexes = std::vector<int>;


enum bspSurfaceType_t
{
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE,
	MST_FOLIAGE
};


struct bspGridPoint_t
{
	Vector3b ambient[ MAX_LIGHTMAPS ];    /* RBSP - array */
	Vector3b directed[ MAX_LIGHTMAPS ];   /* RBSP - array */
	byte styles[ MAX_LIGHTMAPS ];         /* RBSP - whole */
	byte latLong[ 2 ];
};


struct bspDrawSurface_t
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

	Vector3 lightmapOrigin;
	Vector3 lightmapVecs[ 3 ];       /* on patches, [ 0 ] and [ 1 ] are lodbounds */

	int patchWidth;
	int patchHeight;
};


/* advertisements */
struct bspAdvertisement_t
{
	int cellId;
	Vector3 normal;
	Vector3 rect[4];
	char model[ MAX_QPATH ];
};


/* -------------------------------------------------------------------------------

   general types

   ------------------------------------------------------------------------------- */

/* ydnar: for q3map_tcMod */
typedef Vector3 tcMod_t[ 3 ];


struct image_t
{
	CopiedString name;       // relative path w/o extension
	CopiedString filename;   // relative path with extension
	int width, height;
	byte *pixels = nullptr;

	image_t(){
	}
	image_t( const char *name, const char *filename, int width, int height, byte *pixels ) :
		name( name ),
		filename( filename ),
		width( width ),
		height( height ),
		pixels( pixels )
	{}
	image_t( image_t&& other ) noexcept :
		name( std::move( other.name ) ),
		filename( std::move( other.filename ) ),
		width( other.width ),
		height( other.height ),
		pixels( std::exchange( other.pixels, nullptr ) )
	{}
	~image_t(){
		free( pixels );
	}

	byte *at( int width, int height ) const {
		return pixels + 4 * ( height * this->width + width );
	}
};


struct sun_t
{
	Vector3 direction, color;
	float photons, deviance, filterRadius;
	int numSamples, style;
	int skyIndex = -1;
};

struct skylight_t
{
	float value;
	int iterations;
	int horizon_min = 0;
	int horizon_max = 90;
	bool sample_color = true;
	int skyIndex = -1;
};


struct surfaceModel_t
{
	CopiedString model;
	float density, odds;
	float minScale, maxScale;
	float minAngle, maxAngle;
	bool oriented;
};


/* ydnar/sd: foliage stuff for wolf et (engine-supported optimization of the above) */
struct foliage_t
{
	CopiedString model;
	float scale, density, odds;
	int inverseAlpha;
};

struct foliageInstance_t
{
	Vector3 xyz, normal;
};


struct remap_t
{
	char from[ 1024 ];
	char to[ MAX_QPATH ];
};


enum class EColorMod
{
	None,
	Volume,
	ColorSet,
	AlphaSet,
	ColorScale,
	AlphaScale,
	ColorDotProduct,
	AlphaDotProduct,
	ColorDotProductScale,
	AlphaDotProductScale,
	ColorDotProduct2,
	AlphaDotProduct2,
	ColorDotProduct2Scale,
	AlphaDotProduct2Scale
};


struct colorMod_t
{
	EColorMod type;
	float data[ 16 ];
};


enum class EImplicitMap
{
	None,
	Opaque,
	Masked,
	Blend
};


struct shaderInfo_t_data
{
	String64 shader;
	int surfaceFlags;
	int contentFlags;
	int compileFlags;
	float value;                                        /* light value */

	const char          *flareShader;                   /* for light flares */
	char                *damageShader;                  /* ydnar: sof2 damage shader name */
	char                *backShader;                    /* for surfaces that generate different front and back passes */
	char                *cloneShader;                   /* ydnar: for cloning of a surface */
	char                *remapShader;                   /* ydnar: remap a shader in final stage */
	char                *deprecateShader;               /* vortex: shader is deprecated and replaced by this on use */

	std::forward_list<surfaceModel_t> surfaceModels;    /* ydnar: for distribution of models */
	std::forward_list<foliage_t>      foliage;          /* ydnar/splash damage: wolf et foliage */

	float subdivisions;                                 /* from a "tesssize xxx" */
	float backsplashFraction;                           /* floating point value, usually 0.05 */
	float backsplashDistance;                           /* default 23 */
	float lightSubdivide;                               /* default 999 */
	float lightFilterRadius;                            /* ydnar: lightmap filtering/blurring radius for lights created by this shader (default: 0) */

	int lightmapSampleSize;                             /* lightmap sample size */
	float lightmapSampleOffset;                         /* ydnar: lightmap sample offset (default: 1.0) */

	float bounceScale;                                  /* ydnar: radiosity re-emission [0,1.0+] */
	float offset;                                       /* ydnar: offset in units */
	float shadeAngleDegrees;                            /* ydnar: breaking angle for smooth shading (degrees) */

	MinMax minmax;                                      /* ydnar: for particle studio vertexDeform move support */

	bool legacyTerrain;                                 /* ydnar: enable legacy terrain crutches */
	bool indexed;                                       /* ydnar: attempt to use indexmap (terrain alphamap style) */
	bool forceMeta;                                     /* ydnar: force metasurface path */
	bool noClip;                                        /* ydnar: don't clip into bsp, preserve original face winding */
	bool noFast;                                        /* ydnar: suppress fast lighting for surfaces with this shader */
	bool invert;                                        /* ydnar: reverse facing */
	bool nonplanar;                                     /* ydnar: for nonplanar meta surface merging */
	bool tcGen;                                         /* ydnar: has explicit texcoord generation */
	Vector3 vecs[ 2 ];                                  /* ydnar: explicit texture vectors for [0,1] texture space */
	tcMod_t mod;                                        /* ydnar: q3map_tcMod matrix for djbob :) */
	Vector3 lightmapAxis{ 0 };                          /* ydnar: explicit lightmap axis projection */
	std::forward_list<colorMod_t> colorMod;             /* ydnar: q3map_rgb/color/alpha/Set/Mod support */

	int furNumLayers;                                   /* ydnar: number of fur layers */
	float furOffset;                                    /* ydnar: offset of each layer */
	float furFade;                                      /* ydnar: alpha fade amount per layer */

	bool splotchFix;                                    /* ydnar: filter splotches on lightmaps */

	bool hasPasses;                                     /* false if the shader doesn't define any rendering passes */
	bool globalTexture;                                 /* don't normalize texture repeats */
	bool twoSided;                                      /* cull none */
	bool autosprite;                                    /* autosprite shaders will become point lights instead of area lights */
	bool polygonOffset;                                 /* ydnar: don't face cull this or against this */
	bool patchShadows;                                  /* have patches casting shadows when using -light for this surface */
	bool vertexShadows;                                 /* shadows will be casted at this surface even when vertex lit */
	bool forceSunlight;                                 /* force sun light at this surface even tho we might not calculate shadows in vertex lighting */
	bool notjunc;                                       /* don't use this surface for tjunction fixing */
	bool fogParms;                                      /* ydnar: has fogparms */
	bool noFog;                                         /* ydnar: suppress fogging */
	bool clipModel;                                     /* ydnar: solid model hack */
	bool noVertexLight;                                 /* ydnar: leave vertex color alone */
	bool noDirty;                                       /* jal: do not apply the dirty pass to this surface */

	byte styleMarker;                                   /* ydnar: light styles hack */

	float vertexScale;                                  /* vertex light scale */

	String64 skyParmsImageBase;                         /* ydnar: for skies */

	String64 editorImagePath;                           /* use this image to generate texture coordinates */
	String64 lightImagePath;                            /* use this image to generate color / averageColor */
	String64 normalImagePath;                           /* ydnar: normalmap image for bumpmapping */

	EImplicitMap implicitMap;                           /* ydnar: enemy territory implicit shaders */
	String64 implicitImagePath;

	const image_t       *shaderImage;
	const image_t       *lightImage;
	const image_t       *normalImage;

	std::vector<skylight_t>  skylights;                 /* ydnar */
	std::vector<sun_t>  suns;                           /* ydnar */
	int	skyIndex = -1;                                  /* shaders that are used and emit sun/skylight get unique emitter index */

	Vector3 color{ 0 };                                 /* normalized color */
	Color4f averageColor = { 0, 0, 0, 0 };
	byte lightStyle;

	/* vortex: per-surface floodlight */
	float floodlightDirectionScale;
	Vector3 floodlightRGB;
	float floodlightIntensity;
	float floodlightDistance;

	bool lmMergable;                                    /* ydnar */
	int lmCustomWidth, lmCustomHeight;                  /* ydnar */
	float lmBrightness;                                 /* ydnar */
	float lmFilterRadius;                               /* ydnar: lightmap filtering/blurring radius for this shader (default: 0) */

	int shaderWidth, shaderHeight;                      /* ydnar */
	Vector2 stFlat;

	Vector3 fogDir{ 0 };                                /* ydnar */

	char                *shaderText;                    /* ydnar */
	bool custom;
	bool finished;
};

struct shaderInfo_t : public shaderInfo_t_data
{
	const String64 shader;
	shaderInfo_t( const char *shaderName ) : shaderInfo_t_data{}, shader( shaderName ){ // zero-initialze shaderInfo_t_data
	}
	void copyData( const shaderInfo_t& other ) noexcept {
		static_cast<shaderInfo_t_data&>( *this ) = static_cast<const shaderInfo_t_data&>( other );
	}
};



/* -------------------------------------------------------------------------------

   bsp structures

   ------------------------------------------------------------------------------- */

struct face_t
{
	int planenum;
	int priority;
	//bool			checked;
	int compileFlags;
	winding_t           w;
};
using facelist_t = std::forward_list<face_t>;


struct plane_t
{
	Plane3f plane;
	Vector3& normal(){
		return plane.normal();
	}
	const Vector3& normal() const {
		return plane.normal();
	}
	float& dist(){
		return plane.dist();
	}
	const float& dist() const {
		return plane.dist();
	}
	EPlaneType type;
	int counter;
	int hash_chain;
};


struct side_t
{
	int planenum;

	int outputNum;                          /* set when the side is written to the file list */

	Vector3 texMat[ 2 ];                    /* brush primitive texture matrix */
	Vector4 vecs[ 2 ];                      /* old-style texture coordinate mapping */

	Plane3 plane{ 0, 0, 0, 0 };             /* optional plane in double precision for building windings */
	winding_t           winding;
	winding_t           visibleHull;        /* convex hull of all visible fragments */

	shaderInfo_t        *shaderInfo;

	int contentFlags;                       /* from shaderInfo */
	int surfaceFlags;                       /* from shaderInfo */
	int compileFlags;                       /* from shaderInfo */
	int value;                              /* from shaderInfo */

	bool bevel;                             /* don't ever use for bsp splitting, and don't bother making windings for it */
	bool culled;                            /* ydnar: face culling */
};


struct sideRef_t
{
	sideRef_t           *next;
	const side_t        &side;
	sideRef_t( sideRef_t *next, const side_t &side ) : next( next ), side( side ){
	}
};


/* ydnar: generic index mapping for entities (natural extension of terrain texturing) */
struct indexMap_t
{
	int w, h, numLayers;
	String64 shader;
	float offsets[ 256 ];
	byte                *pixels;
};


struct brush_t
{
	brush_t             *original;          /* chopped up brushes will reference the originals */

	int entityNum, brushNum;                /* editor numbering */
	int outputNum;                          /* set when the brush is written to the file list */

	/* ydnar: for shadowcasting entities */
	int castShadows;
	int recvShadows;

	shaderInfo_t        *contentShader;
	shaderInfo_t        *celShader;         /* :) */

	/* ydnar: gs mods */
	int lightmapSampleSize;                 /* jal : entity based _lightmapsamplesize */
	float lightmapScale;
	float shadeAngleDegrees;                /* jal : entity based _shadeangle */
	Vector3 ambientColor{ 0 };
	MinMax eMinmax;
	indexMap_t          *im;

	int contentFlags;
	int compileFlags;                       /* ydnar */
	bool detail;
	bool opaque;

	MinMax minmax;

	std::vector<side_t> sides;
};
using brushlist_t = std::list<brush_t>;


struct fog_t
{
	shaderInfo_t        *si;
	const brush_t       *brush;
	int visibleSide;                        /* the brush side that ray tests need to clip against (-1 == none) */
};


struct mesh_t
{
	int width, height;
	bspDrawVert_t       *verts;

	mesh_t() = default;
	mesh_t( int width, int height, bspDrawVert_t *verts ) : width( width ), height( height ), verts( verts ){}
	size_t numVerts() const {
		return width * height;
	}
	void freeVerts(){
		free( verts );
	}
};


struct parseMesh_t
{
	int entityNum, brushNum;                    /* ydnar: editor numbering */

	/* ydnar: for shadowcasting entities */
	int castShadows;
	int recvShadows;

	mesh_t mesh;
	shaderInfo_t        *shaderInfo;
	shaderInfo_t        *celShader;             /* :) */

	/* ydnar: gs mods */
	int lightmapSampleSize;                     /* jal : entity based _lightmapsamplesize */
	float lightmapScale;
	Vector3 ambientColor{ 0 };
	MinMax eMinmax;
	indexMap_t          *im;

	/* grouping */
	float longestCurve;
	int maxIterations;
};


struct EntityCompileParams
{
	int castShadows;
	int recvShadows;
	shaderInfo_t        *celShader;
	int lightmapSampleSize;
	float lightmapScale;
	float shadeAngle;
	Vector3 ambientColor;
};


/*
    ydnar: the drawsurf struct was extended to allow for:
    - non-convex planar surfaces
    - non-planar brushface surfaces
    - lightmapped terrain
    - planar patches
 */

enum class ESurfaceType
{
	/* ydnar: these match up exactly with bspSurfaceType_t */
	Bad,
	Face,
	Patch,
	Triangles,
	Flare,
	Foliage,    /* wolf et */

	/* ydnar: compiler-relevant surface types */
	ForcedMeta,
	Meta,
	Foghull,
	Decal,
	Shader,  // this is used to define number of enum items
};

constexpr const char *surfaceTypeName( ESurfaceType type ){
	switch ( type )
	{
	case ESurfaceType::Bad:        return "ESurfaceType::Bad";
	case ESurfaceType::Face:       return "ESurfaceType::Face";
	case ESurfaceType::Patch:      return "ESurfaceType::Patch";
	case ESurfaceType::Triangles:  return "ESurfaceType::Triangles";
	case ESurfaceType::Flare:      return "ESurfaceType::Flare";
	case ESurfaceType::Foliage:    return "ESurfaceType::Foliage";
	case ESurfaceType::ForcedMeta: return "ESurfaceType::ForcedMeta";
	case ESurfaceType::Meta:       return "ESurfaceType::Meta";
	case ESurfaceType::Foghull:    return "ESurfaceType::Foghull";
	case ESurfaceType::Decal:      return "ESurfaceType::Decal";
	case ESurfaceType::Shader:     return "ESurfaceType::Shader";
	}
	return "SURFACE NAME ERROR";
}


/* ydnar: this struct needs an overhaul (again, heh) */
struct mapDrawSurface_t
{
	ESurfaceType type;
	bool planar;
	int outputNum = -1;                     /* ydnar: to match this sort of thing up */

	bool fur;                               /* ydnar: this is kind of a hack, but hey... */
	bool skybox;                            /* ydnar: yet another fun hack */
	bool backSide;                          /* ydnar: q3map_backShader support */

	mapDrawSurface_t *parent;        /* ydnar: for cloned (skybox) surfaces to share lighting data */
	mapDrawSurface_t *clone;         /* ydnar: for cloned surfaces */
	mapDrawSurface_t *cel;           /* ydnar: for cloned cel surfaces */

	shaderInfo_t        *shaderInfo;
	shaderInfo_t        *celShader;
	const brush_t       *mapBrush;
	sideRef_t           *sideRef;

	int fogNum;

	/* vertexes and triangles */
	DrawVerts verts;
	DrawIndexes indexes;

	int numVerts() const {
		return verts.size();
	};

	int planeNum = -1;
	Vector3 lightmapOrigin{ 0 };            /* also used for flares */
	Vector3 lightmapVecs[ 3 ]{ Vector3( 0 ), Vector3( 0 ), Vector3( 0 ) };              /* also used for flares */
	int lightStyle;                         /* used for flares */

	/* ydnar: per-surface (per-entity, actually) lightmap sample size scaling */
	float lightmapScale;

	/* jal: per-surface (per-entity, actually) shadeangle */
	float shadeAngleDegrees;

	/* per-surface (per-entity, actually) ambientColor */
	Vector3 ambientColor{ 0 };

	/* ydnar: surface classification */
	MinMax minmax;
	Vector3 lightmapAxis{ 0 };
	int sampleSize;

	/* ydnar: shadow group support */
	int castShadows, recvShadows;

	/* ydnar: for patches */
	float longestCurve;
	int maxIterations;
	int patchWidth, patchHeight;
	MinMax bounds;

	/* ydnar/sd: for foliage */
	int numFoliageInstances;

	/* ydnar: editor/useful numbering */
	int entityNum;
	int surfaceNum;

	void addSideRef( const side_t *side ){
		if ( side != nullptr ) {
			sideRef = new sideRef_t( sideRef, *side );
		}
	}
};


struct drawSurfRef_t
{
	drawSurfRef_t    *nextRef;
	int outputNum;
};


struct epair_t
{
	CopiedString key, value;
};


struct entity_t
{
	Vector3 origin{ 0 };
	brushlist_t brushes;
	std::vector<brush_t*>  colorModBrushes;
	std::forward_list<parseMesh_t> patches;
	int mapEntityNum;                               /* .map file entities numbering */
	int firstDrawSurf;
	int firstBrush, numBrushes;                     /* only valid during BSP compile */
	std::list<epair_t> epairs;
	Vector3 originbrush_origin{ 0 };

	void setKeyValue( const char *key, const char *value );
	void setKeyValue( const char *key, int value, const char *format = "%i" );
	void setKeyValue( const char *key, float value );
	void setKeyValue( const char *key, const Vector3& value );
	const char *valueForKey( const char *key ) const;

	template<typename ... Keys>
	bool boolForKey( Keys ... keys ) const {
		bool bool_value = false;
		read_keyvalue_( bool_value, { keys ... } );
		return bool_value;
	}
	template<typename ... Keys>
	int intForKey( Keys ... keys ) const {
		int int_value = 0;
		read_keyvalue_( int_value, { keys ... } );
		return int_value;
	}
	template<typename ... Keys>
	float floatForKey( Keys ... keys ) const {
		float float_value = 0;
		read_keyvalue_( float_value, { keys ... } );
		return float_value;
	}
	Vector3 vectorForKey( const char *key ) const {
		Vector3 vec( 0 );
		read_keyvalue_( vec, { key } );
		return vec;
	}

	const char *classname() const {
		return valueForKey( "classname" );
	}
	bool classname_is( const char *name ) const {
		return striEqual( classname(), name );
	}
	bool classname_prefixed( const char *prefix ) const {
		return striEqualPrefix( classname(), prefix );
	}

		/* entity: read key value variadic template
		returns true on successful read
		returns false and does not modify value otherwise */
	template<typename T, typename ... Keys>
	bool read_keyvalue( T& value_ref, Keys ... keys ) const {
		return read_keyvalue_( value_ref, { keys ... } );
	}
private:
	bool read_keyvalue_( bool &bool_value, std::initializer_list<const char*>&& keys ) const;
	bool read_keyvalue_( int &int_value, std::initializer_list<const char*>&& keys ) const;
	bool read_keyvalue_( float &float_value, std::initializer_list<const char*>&& keys ) const;
	bool read_keyvalue_( Vector3& vector3_value, std::initializer_list<const char*>&& keys ) const;
	bool read_keyvalue_( const char *&string_ptr_value, std::initializer_list<const char*>&& keys ) const;
};


struct node_t
{
	/* both leafs and nodes */
	int planenum;                       /* -1 = leaf node */
	node_t              *parent;
	MinMax minmax;                      /* valid after portalization */

	/* nodes only */
	node_t              *children[ 2 ];
	int compileFlags;                   /* ydnar: hint, antiportal */
	int tinyportals;
	Vector3 referencepoint;

	/* leafs only */
	bool opaque;                        /* view can never be inside */
	bool skybox;                        /* ydnar: a skybox leaf */
	bool sky;                           /* ydnar: a sky leaf */
	int cluster;                        /* for portalfile writing */
	int area;                           /* for areaportals */
	brushlist_t          brushlist;     /* fragments of all brushes in this leaf */
	drawSurfRef_t       *drawSurfReferences;

	int occupied;                       /* 1 or greater can reach entity */
	const entity_t      *occupant;      /* for leak file testing */

	struct portal_t     *portals;       /* also on nodes during construction */

	bool has_structural_children;
};


struct portal_t
{
	plane_t plane;
	node_t              *onnode;        /* NULL = outside box */
	node_t              *nodes[ 2 ];    /* [ 0 ] = front side of plane */
	portal_t            *next[ 2 ];
	winding_t            winding;

	bool sidefound;                     /* false if ->side hasn't been checked */
	int compileFlags;                   /* from original face that caused the split */

	ESide thisSide( const node_t *node ) const {
		return nodes[ eBack ] == node;
	}
	portal_t* nextPortal( const node_t *node ) const {
		return next[ thisSide( node ) ];
	}
	node_t* otherNode( const node_t *node ) const {
		return nodes[ !thisSide( node ) ];
	}
};


struct tree_t
{
	node_t              *headnode;
	node_t outside_node;
	MinMax minmax;
};



/* -------------------------------------------------------------------------------

   light structures

   ------------------------------------------------------------------------------- */

enum class ELightType
{
	Point,
	Area,
	Spot,
	Sun
};

struct LightFlags : BitFlags<std::uint32_t, LightFlags>
{
	constexpr static BitFlags AttenLinear      {1 << 0};
	constexpr static BitFlags AttenAngle       {1 << 1};
	constexpr static BitFlags AttenDistance    {1 << 2};
	constexpr static BitFlags Twosided         {1 << 3};
	constexpr static BitFlags Grid             {1 << 4};
	constexpr static BitFlags Surfaces         {1 << 5};
	constexpr static BitFlags Dark             {1 << 6};      /* probably never use this */
	constexpr static BitFlags Fast             {1 << 7};
	constexpr static BitFlags FastTemp         {1 << 8};
	constexpr static BitFlags FastActual       { Fast | FastTemp };
	constexpr static BitFlags Negative         {1 << 9};
	constexpr static BitFlags Unnormalized     {1 << 10};     /* vortex: do not normalize _color */

	constexpr static BitFlags DefaultSun       { AttenAngle | Grid | Surfaces };
	constexpr static BitFlags DefaultArea      { AttenAngle | AttenDistance | Grid | Surfaces };    /* q3a and wolf are the same */
	constexpr static BitFlags DefaultQ3A       { AttenAngle | AttenDistance | Grid | Surfaces | Fast };
	constexpr static BitFlags DefaultWolf      { AttenLinear | AttenDistance | Grid | Surfaces | Fast };
};

/* ydnar: new light struct with flags */
struct light_t
{
	ELightType type;
	LightFlags flags;                   /* ydnar: condensed all the booleans into one flags int */
	const shaderInfo_t  *si;

	Vector3 origin{ 0 };
	Vector3 normal{ 0 };                /* for surfaces, spotlights, and suns */
	float dist;                         /* plane location along normal */

	float photons;
	int style;
	Vector3 color{ 0 };
	float radiusByDist;                 /* for spotlights */
	float fade;                         /* ydnar: from wolf, for linear lights */
	float angleScale;                   /* ydnar: stolen from vlight for K */
	float extraDist;                    /* "extra dimension" distance of the light, to kill hot spots */

	float add;                          /* ydnar: used for area lights */
	float envelope;                     /* ydnar: units until falloff < tolerance */
	float envelope2;                    /* ydnar: envelope squared (tiny optimization) */
	MinMax minmax;                      /* ydnar: pvs envelope */
	int cluster;                        /* ydnar: cluster light falls into */

	winding_t           w;

	float falloffTolerance;             /* ydnar: minimum attenuation threshold */
	float filterRadius;                 /* ydnar: lightmap filter radius in world units, 0 == default */

	int skyIndex = -1;                  /* For sun/skylights. Check if particular sky triangle emits a given light. -1 = always emits */
};


struct trace_t
{
	/* constant input */
	bool testOcclusion, forceSunlight, testAll;
	int recvShadows;

	int numSurfaces;
	int                 *surfaces;

	int numLights;
	const light_t       **lights;

	bool twoSided;

	/* per-sample input */
	int cluster;
	Vector3 origin, normal;
	float inhibitRadius;                /* sphere in which occluding geometry is ignored */

	/* per-light input */
	const light_t             *light = nullptr;
	Vector3 end;

	/* multisky support */
	byte skyIndices[ ( MAX_SKIES + 7 ) / 8 ];

	/* calculated input */
	Vector3 displacement, direction;
	float distance;

	/* input and output */
	Vector3 color;                      /* starts out at full color, may be reduced if transparent surfaces are crossed */
	Vector3 directionContribution;      /* result contribution to the deluxe map */

	/* output */
	Vector3 hit;
	int compileFlags;                   /* for determining surface compile flags traced through */
	bool passSolid;
	bool opaque;
	float forceSubsampling;             /* needs subsampling (alphashadow), value = max color contribution possible from it */

	/* working data */
	int numTestNodes;
	int testNodes[ MAX_TRACE_TEST_NODES ];
};


/* ydnar: new lightmap handling code */
struct outLightmap_t
{
	int lightmapNum, extLightmapNum;
	int customWidth, customHeight;
	int numLightmaps;
	int freeLuxels;
	int numShaders;
	const shaderInfo_t  *shaders[ MAX_LIGHTMAP_SHADERS ];
	byte                *lightBits;
	Vector3b            *bspLightBytes;
	Vector3b            *bspDirBytes;
};

struct SuperLuxel{
	Vector3 value;
	float count;
};

struct SuperFloodLight{
	Vector3 value;
	float scale;
};

struct rawLightmap_t
{
	bool finished, splotchFix, wrap[ 2 ];
	int customWidth, customHeight;
	float brightness;
	float filterRadius;

	int firstLightSurface, numLightSurfaces;                        /* index into lightSurfaces */
	int numLightClusters, *lightClusters;

	int sampleSize, actualSampleSize, axisNum;

	Vector3 ambientColor;

	/* vortex: per-surface floodlight */
	float floodlightDirectionScale;
	Vector3 floodlightRGB;
	float floodlightIntensity;
	float floodlightDistance;

	int entityNum;
	int recvShadows;
	MinMax minmax;
	Vector3 axis, origin, *vecs;
	Plane3f                   *plane;
	int w, h, sw, sh, used;

	bool solid[ MAX_LIGHTMAPS ];
	Vector3 solidColor[ MAX_LIGHTMAPS ];

	int numStyledTwins;
	rawLightmap_t           *twins[ MAX_LIGHTMAPS ];

	int outLightmapNums[ MAX_LIGHTMAPS ];
	int twinNums[ MAX_LIGHTMAPS ];
	int lightmapX[ MAX_LIGHTMAPS ], lightmapY[ MAX_LIGHTMAPS ];
	byte styles[ MAX_LIGHTMAPS ];
	Vector3                 *bspLuxels[ MAX_LIGHTMAPS ];
	Vector3                 *radLuxels[ MAX_LIGHTMAPS ];
	SuperLuxel              *superLuxels[ MAX_LIGHTMAPS ];
	byte                    *superFlags;
	Vector3                 *superOrigins;
	Vector3                 *superNormals;
	float                   *superDirt;
	int                     *superClusters;

	Vector3                 *superDeluxels; /* average light direction */
	Vector3                 *bspDeluxels;
	SuperFloodLight         *superFloodLight;
	Vector3& getBspLuxel( int lightmapNum, int x, int y ){
		return bspLuxels[ lightmapNum ][y * w + x];
	}
	const Vector3& getBspLuxel( int lightmapNum, int x, int y ) const {
		return bspLuxels[ lightmapNum ][y * w + x];
	}
	Vector3& getRadLuxel( int lightmapNum, int x, int y ){
		return radLuxels[ lightmapNum ][y * w + x];
	}
	const Vector3& getRadLuxel( int lightmapNum, int x, int y ) const {
		return radLuxels[ lightmapNum ][y * w + x];
	}
	SuperLuxel& getSuperLuxel( int lightmapNum, int x, int y ){
		return superLuxels[ lightmapNum ][y * sw + x];
	}
	const SuperLuxel& getSuperLuxel( int lightmapNum, int x, int y ) const {
		return superLuxels[ lightmapNum ][y * sw + x];
	}
	const byte& getSuperFlag( int x, int y ) const {
		return superFlags[y * sw + x];
	}
	byte& getSuperFlag( int x, int y ){
		return superFlags[y * sw + x];
	}
	Vector3& getSuperOrigin( int x, int y ){
		return superOrigins[y * sw + x];
	}
	const Vector3& getSuperOrigin( int x, int y ) const {
		return superOrigins[y * sw + x];
	}
	Vector3& getSuperNormal( int x, int y ){
		return superNormals[y * sw + x];
	}
	const Vector3& getSuperNormal( int x, int y ) const {
		return superNormals[y * sw + x];
	}
	float& getSuperDirt( int x, int y ){
		return superDirt[y * sw + x];
	}
	const float& getSuperDirt( int x, int y ) const {
		return superDirt[y * sw + x];
	}
	int& getSuperCluster( int x, int y ){
		return superClusters[y * sw + x];
	}
	const int& getSuperCluster( int x, int y ) const {
		return superClusters[y * sw + x];
	}
	Vector3& getSuperDeluxel( int x, int y ){
		return superDeluxels[y * sw + x];
	}
	const Vector3& getSuperDeluxel( int x, int y ) const {
		return superDeluxels[y * sw + x];
	}
	Vector3& getBspDeluxel( int x, int y ){
		return bspDeluxels[y * w + x];
	}
	const Vector3& getBspDeluxel( int x, int y ) const {
		return bspDeluxels[y * w + x];
	}
	SuperFloodLight& getSuperFloodLight( int x, int y ){
		return superFloodLight[y * sw + x];
	}
	const SuperFloodLight& getSuperFloodLight( int x, int y ) const {
		return superFloodLight[y * sw + x];
	}
};


struct rawGridPoint_t
{
	Vector3 ambient[ MAX_LIGHTMAPS ];
	Vector3 directed[ MAX_LIGHTMAPS ];
	Vector3 dir;
	byte styles[ MAX_LIGHTMAPS ];
};


struct surfaceInfo_t
{
	int modelindex;
	const shaderInfo_t  *si;
	rawLightmap_t       *lm;
	int parentSurfaceNum, childSurfaceNum;
	int entityNum, castShadows, recvShadows, sampleSize, patchIterations;
	Vector3 ambientColor;
	float longestCurve;
	Plane3f               *plane;
	Vector3 axis;
	MinMax minmax;
	bool hasLightmap, approximated;
	int firstSurfaceCluster, numSurfaceClusters;
};


class Args
{
private:
	const char *m_arg0;
	std::vector<const char*> m_args;
	std::vector<const char*>::const_iterator m_next;
	const char *m_current;
public:
	Args( int argc, char **argv ){
		ENSURE( argc > 0 );
		m_arg0 = argv[0];
		m_args = { argv + 1, argv + argc };
	}
	const char *getArg0() const {
		return m_arg0;
	}
	std::vector<const char*> getVector(){
		return m_args;
	}
	template<typename ...Args>
	bool takeArg( Args... args ){
		const std::array<const char*, sizeof...(Args)> array = { args ... };
		for( auto&& arg : array )
			for( auto it = m_args.cbegin(); it != m_args.cend(); ++it )
				if( striEqual( *it, arg ) ){
					m_current = *it;
					m_next = m_args.erase( it );
					return true;
				}
		return false;
	}
	/* next three are only valid after takeArg() == true */
	const char *takeNext(){
		if( m_next == m_args.cend() )
			Error( "Out of arguments: No parameters specified after %s", m_current );
		const char *ret = *m_next;
		m_next = m_args.erase( m_next );
		return ret;
	}
	bool nextAvailable() const {
		return( m_next != m_args.cend() );
	}
	const char *next() const {
		return *m_next;
	}
	/* --- */
	size_t size() const {
		return m_args.size();
	}
	bool empty() const {
		return size() == 0;
	}
	bool takeFront( const char *arg ){
		if( !m_args.empty() && striEqual( m_args.front(), arg ) ){
			m_args.erase( m_args.cbegin() );
			return true;
		}
		return false;
	}
	const char *takeFront(){
		ENSURE( !m_args.empty() );
		const char *ret = m_args.front();
		m_args.erase( m_args.cbegin() );
		return ret;
	}
	const char *takeBack(){
		ENSURE( !m_args.empty() );
		const char *ret = m_args.back();
		m_args.pop_back();
		return ret;
	}
};


/* -------------------------------------------------------------------------------

   prototypes

   ------------------------------------------------------------------------------- */

inline float Random(){ /* returns a pseudorandom number between 0 and 1 */
	return (float) rand() / RAND_MAX;
}

/* help.c */
void                        HelpMain( const char* arg );
void                        HelpGames();

/* path_init.c */
const game_t                *GetGame( const char *arg );
void                        InitPaths( Args& args );


/* bsp.c */
int                         BSPMain( Args& args );


/* minimap.c */
int                         MiniMapBSPMain( Args& args );

/* convert_bsp.c */
int                         FixAAS( Args& args );
int                         AnalyzeBSP( Args& args );
int                         BSPInfo( Args& args );
int                         ScaleBSPMain( Args& args );
int                         ShiftBSPMain( Args& args );
int                         MergeBSPMain( Args& args );
int                         ConvertBSPMain( Args& args );

/* convert_map.c */
int                         ConvertBSPToMap( char *bspName );
int                         ConvertBSPToMap_BP( char *bspName );
int                         ConvertBSPToMap_220( char *bspName );


/* convert_ase.c */
int                         ConvertBSPToASE( char *bspName );

/* convert_obj.c */
int                         ConvertBSPToOBJ( char *bspName );

/* convert_json.c */
int                         ConvertJsonMain( Args& args );


/* brush.c */
Vector3                     SnapWeldVector( const Vector3& a, const Vector3& b );
bool                        CreateBrushWindings( brush_t& brush );
void                        WriteBSPBrushMap( const char *name, const brushlist_t& list );

void                        FilterDetailBrushesIntoTree( const entity_t& e, tree_t& tree );
void                        FilterStructuralBrushesIntoTree( const entity_t& e, tree_t& tree );

bool                        WindingIsTiny( const winding_t& w );


/* mesh.c */
bspDrawVert_t               LerpDrawVert( const bspDrawVert_t& a, const bspDrawVert_t& b );
void                        LerpDrawVertAmount( bspDrawVert_t *a, bspDrawVert_t *b, float amount, bspDrawVert_t *out );
mesh_t                      CopyMesh( const mesh_t m );
void                        PrintMesh( const mesh_t m );
void                        TransposeMesh( mesh_t& m );
void                        InvertMesh( mesh_t& m );
mesh_t                      SubdivideMesh( const mesh_t in, float maxError, float minLength );
int                         IterationsForCurve( float len, int subdivisions );
mesh_t                      SubdivideMesh2( const mesh_t in, int iterations );
mesh_t                      RemoveLinearMeshColumnsRows( const mesh_t in );
mesh_t                      TessellatedMesh( const mesh_t in, int iterations );
void                        MakeMeshNormals( mesh_t& in );
void                        PutMeshOnCurve( mesh_t& in );

class MeshQuadIterator
{
	const mesh_t m;
	int y, x;
	std::array<int, 4> _idx;
	void update_idx(){
		/* set indexes */
		const int pw[ 5 ] = {
			x + ( y * m.width ),
			x + ( ( y + 1 ) * m.width ),
			x + 1 + ( ( y + 1 ) * m.width ),
			x + 1 + ( y * m.width ),
			x + ( y * m.width )    /* same as pw[ 0 ] */
		};
		/* set radix */
		const int r = ( x + y ) & 1;

		_idx = { pw[ r + 0 ],
		         pw[ r + 1 ],
		         pw[ r + 2 ],
		         pw[ r + 3 ] };
	}
public:
	MeshQuadIterator( const mesh_t m ) : m( m ), y( 0 ), x( 0 ) {
		update_idx();
	}
	void operator++(){
		/* iterate through the mesh quads */
		// for ( int y = 0; y < ( m.height - 1 ); ++y )
			// for ( int x = 0; x < ( m.width - 1 ); ++x )
		if( ++x >= ( m.width - 1 ) ){
			x = 0;
			++y;
		}
		update_idx();
	}
	operator bool() const {
		return y < ( m.height - 1 );
	}
	const std::array<int, 4>& idx() const {
		return _idx;
	}
	QuadRef quad() const {
		return { m.verts + _idx[0],
		         m.verts + _idx[1],
		         m.verts + _idx[2],
		         m.verts + _idx[3] };
	}
	std::array<TriRef, 2> tris() const {
		return { TriRef{ m.verts + _idx[0],
		                 m.verts + _idx[1],
		                 m.verts + _idx[2] },
		         TriRef{ m.verts + _idx[0],
		                 m.verts + _idx[2],
		                 m.verts + _idx[3] } };
	}
};


/* map.c */
void                        LoadMapFile( const char *filename, bool onlyLights, bool noCollapseGroups );
template<class T> bool      SnapPlaneImproved( Plane3f& plane, const Span<const BasicVector3<T>>& points );
int                         FindFloatPlane( const Plane3f& plane, const Span<const Vector3>& points = {} );
int                         FindFloatPlane( const Plane3f& plane, const Span<const DoubleVector3>& points );
bool                        PlaneEqual( const plane_t& p, const Plane3f& plane );
void                        AddBrushBevels();
EntityCompileParams         ParseEntityCompileParams( const entity_t& e, const entity_t *eparent, bool worldShadowGroup );


/* portals.c */
bool                        PortalPassable( const portal_t *p );
void                        RemovePortalFromNode( portal_t *portal, node_t *l );

enum class EFloodEntities
{
	Leaked,
	Good,
	Empty
};
EFloodEntities              FloodEntities( tree_t& tree );
void                        FillOutside( node_t *headnode );
void                        FloodAreas( tree_t& tree );
inline portal_t             *AllocPortal(){ return new portal_t(); } // zero initializes
inline void                 FreePortal( portal_t *p ){ delete p; }

void                        MakeTreePortals( tree_t& tree );


/* leakfile.c */
void                        Leak_feedback( const tree_t& tree );


/* prtfile.c */
void                        NumberClusters( tree_t& tree );
void                        WritePortalFile( const tree_t& tree );


/* writebsp.c */
void                        SetModelNumbers();
void                        SetLightStyles();
void                        UnSetLightStyles();

int                         EmitShader( const char *shader, const int *contentFlags, const int *surfaceFlags );

void                        BeginBSPFile();
void                        EndBSPFile( bool do_write );
void                        EmitBrushes( entity_t& e );
void                        EmitFogs();

void                        BeginModel( const entity_t& e );
void                        EndModel( const entity_t& e, node_t *headnode );


/* tree.c */
void                        FreeTree( tree_t& tree );
inline node_t               *AllocNode(){ return new node_t(); } // zero initializes


/* patch.c */
void                        ParsePatch( bool onlyLights, entity_t& mapEnt, int mapPrimitiveNum );
void                        PatchMapDrawSurfs( entity_t& e );


/* tjunction.c */
void                        FixTJunctions( const entity_t& e );


/* fog.c */
winding_t                   WindingFromDrawSurf( const mapDrawSurface_t& ds );
void                        FogDrawSurfaces( const entity_t& e );
int                         FogForPoint( const Vector3& point, float epsilon );
int                         FogForBounds( const MinMax& minmax, float epsilon );
void                        CreateMapFogs();


/* facebsp.c */
facelist_t                  MakeStructuralBSPFaceList( const brushlist_t& list );
facelist_t                  MakeVisibleBSPFaceList( const brushlist_t& list );
tree_t                      FaceBSP( facelist_t& list );


/* model.c */
void                        assimp_init();
void                        InsertModel( const char *name, const char *skin, int frame, const Matrix4& transform, const std::list<remap_t> *remaps,
                                         entity_t& entity, int spawnFlags, float clipDepth, const EntityCompileParams& params );
void                        AddTriangleModels( entity_t& eparent );


/* surface.c */
mapDrawSurface_t&           AllocDrawSurface( ESurfaceType type );
void                        StripFaceSurface( mapDrawSurface_t& ds );
void                        MaxAreaFaceSurface( mapDrawSurface_t& ds );
Vector3                     CalcLightmapAxis( const Vector3& normal );
void                        ClassifySurface( mapDrawSurface_t& ds );
void                        ClassifyEntitySurfaces( const entity_t& e );
void                        TidyEntitySurfaces( const entity_t& e );
mapDrawSurface_t            *CloneSurface( const mapDrawSurface_t& src, shaderInfo_t *si );
void                        ClearSurface( mapDrawSurface_t& ds );
mapDrawSurface_t            *DrawSurfaceForSide( const entity_t& e, const brush_t& b, const side_t& s, const winding_t& w );
mapDrawSurface_t            *DrawSurfaceForMesh( const entity_t& e, parseMesh_t& p, mesh_t *mesh );
mapDrawSurface_t            *DrawSurfaceForFlare( int entNum, const Vector3& origin, const Vector3& normal, const Vector3& color, const char *flareShader, int lightStyle );
void                        ClipSidesIntoTree( entity_t& e, const tree_t& tree );
void                        MakeDebugPortalSurfs( const tree_t& tree );
void                        MakeFogHullSurfs( const char *shader );
void                        SubdivideFaceSurfaces( const entity_t& e );
void                        AddEntitySurfaceModels( entity_t& e );
void                        FilterDrawsurfsIntoTree( entity_t& e, tree_t& tree );


/* surface_fur.c */
void                        Fur( mapDrawSurface_t& src );


/* surface_foliage.c */
void                        Foliage( mapDrawSurface_t& src, entity_t& entity );


/* ydnar: surface_meta.c */
void                        ClearMetaTriangles();
void                        MakeEntityMetaTriangles( const entity_t& e );
void                        FixMetaTJunctions();
void                        SmoothMetaTriangles();
void                        MergeMetaTriangles();
void                        EmitMetaStats(); // vortex: print meta statistics even in no-verbose mode


/* surface_extra.c */
void                        SetDefaultSampleSize( int sampleSize );
void                        SetDefaultAmbientColor( const Vector3& color );
void                        SetSurfaceExtra( const mapDrawSurface_t& ds );
void                        WriteSurfaceExtraFile( const char *path );
void                        LoadSurfaceExtraFile( const char *path );


/* decals.c */
void                        ProcessDecals();
void                        MakeEntityDecals( const entity_t& e );

/* map.c */
std::array<Vector3, 2>      TextureAxisFromPlane( const plane_t& plane );

/* vis.c */
int                         VisMain( Args& args );


/* light.c  */
float                       PointToPolygonFormFactor( const Vector3& point, const Vector3& normal, const winding_t& w );
int                         LightContributionToSample( trace_t *trace );
void                        LightingAtSample( trace_t * trace, byte (&styles)[ MAX_LIGHTMAPS ], Vector3 (&colors)[ MAX_LIGHTMAPS ], const Vector3& ambientColor );
int                         LightMain( Args& args );


/* light_trace.c */
void                        SetupTraceNodes();
void                        TraceLine( trace_t *trace );
float                       SetupTrace( trace_t *trace );


/* light_bounce.c */
bool                        RadSampleImage( const byte * pixels, int width, int height, const Vector2& st, Color4f& color );
void                        RadLightForTriangles( int num, int lightmapNum, const rawLightmap_t *lm, const shaderInfo_t& si, float scale, float subdivide );
void                        RadLightForPatch( int num, int lightmapNum, const rawLightmap_t *lm, const shaderInfo_t& si, float scale, float subdivide );
void                        RadCreateDiffuseLights();


/* light_ydnar.c */
Vector3b                    ColorToBytes( const Vector3& color, float scale );
void                        SmoothNormals();

void                        MapRawLightmap( int num );

void                        SetupDirt();
void                        DirtyRawLightmap( int num );

void                        SetupFloodLight();
void                        FloodlightRawLightmaps();
float                       FloodLightForSample( trace_t *trace, float floodLightDistance, bool floodLightLowQuality );

void                        IlluminateRawLightmap( int num );
void                        IlluminateVertexes( int num );

void                        SetupBrushesFlags( int mask_any, int test_any, int mask_all, int test_all );
void                        SetupBrushes();
bool                        ClusterVisible( int a, int b );
int                         ClusterForPointExt( const Vector3& point, float epsilon );
void                        SetupEnvelopes( bool forGrid, bool fastFlag );


/* lightmaps_ydnar.c */
void                        ExportLightmaps();

int                         ExportLightmapsMain( Args& args );
int                         ImportLightmapsMain( Args& args );

void                        SetupSurfaceLightmaps();
void                        StitchSurfaceLightmaps();
void                        StoreSurfaceLightmaps( bool fastAllocate, bool storeForReal );


/* exportents.c */
int                         ExportEntitiesMain( Args& args );


/* image.c */
const image_t               *ImageLoad( const char *name );


/* shaders.c */
void                        ColorMod( const std::forward_list<colorMod_t>& colormod, const Span<bspDrawVert_t> drawVerts );

void                        TCMod( const tcMod_t& mod, Vector2& st );

bool                        ApplySurfaceParm( const char *name, int *contentFlags, int *surfaceFlags, int *compileFlags );
const surfaceParm_t         *GetSurfaceParm( const char *name );

// Encode the string as a structural literal class type
template <std::size_t N>
struct TemplateString
{
	consteval TemplateString( const char(&string)[N] ) {
		std::copy_n( string, N, m_data );
		ENSURE( string[N - 1] == '\0' && "TemplateString must be null-terminated" ); // consteval ensures this is evaluated at compile time, despite not being a static_assert
	}
	char m_data[N];
};

/// \brief returns statically evaluated \c surfaceParm_t for the given name or emits \c Error
template<TemplateString string>
const surfaceParm_t         &GetRequiredSurfaceParm(){
	static const surfaceParm_t *const sp = GetSurfaceParm( string.m_data ); // null-termination ensured in constructor
	ENSURE( sp != nullptr );
    return *sp;
}

void                        BeginMapShaderFile( const char *mapFile );
void                        WriteMapShaderFile();
const shaderInfo_t          &CustomShader( const shaderInfo_t *si, const char *find, char *replace );
void                        EmitVertexRemapShader( char *from, char *to );

void                        LoadShaderInfo();
shaderInfo_t                &ShaderInfoForShader( const char *shader );
shaderInfo_t                *ShaderInfoForShaderNull( const char *shader );


/* bspfile_abstract.c */
void                        SwapBlock( int *block, int size );

void                        LoadBSPFile( const char *filename );
void                        LoadBSPFilePartially( const char *filename );
void                        WriteBSPFile( const char *filename );
void                        PrintBSPFileSizes();

void                        ParseEPair( std::list<epair_t>& epairs );
void                        ParseEntities();
void                        UnparseEntities();
void                        PrintEntity( const entity_t *ent );

entity_t                    *FindTargetEntity( const char *target );
void                        GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, int *castShadows, int *recvShadows );
void                        InjectCommandLine( const char *stage, const std::vector<const char *>& args );



/* -------------------------------------------------------------------------------

   bsp/general global variables

   ------------------------------------------------------------------------------- */

struct shaderInfo_t_compare
{
	bool operator()( const shaderInfo_t& si, const shaderInfo_t& si2 ) const {
		return RawStringLessNoCase()( si.shader, si2.shader );
	}
	bool operator()( const shaderInfo_t& si, const char *si2 ) const {
		return RawStringLessNoCase()( si.shader, si2 );
	}
	bool operator()( const char *si, const shaderInfo_t& si2 ) const {
		return RawStringLessNoCase()( si, si2.shader );
	}
};

/* general */
inline UnsortedSet<shaderInfo_t, false, shaderInfo_t_compare>       shaderInfo;

inline String64 mapName;                 /* ydnar: per-map custom shaders for larger lightmaps */
inline CopiedString mapShaderFile;

/* can't code */
inline bool doingBSP;

// for .ase conversion
inline bool shadersAsBitmap;
inline bool lightmapsAsTexcoord;
// bsp to map conversion
inline bool g_decompile_modelClip;
inline bool g_decompile_wtf;

/* general commandline arguments */
inline bool force;
inline int patchSubdivisions = 8;                       /* ydnar: -patchmeta subdivisions */

/* commandline arguments */
inline bool verboseEntities;
inline bool useCustomInfoParms;
inline bool leaktest;
inline bool nodetail;
inline bool nosubdivide;
inline bool notjunc;
inline bool fulldetail;
inline bool nowater;
inline bool noCurveBrushes;
inline bool fakemap;
inline bool nofog;
inline bool noHint;                        /* ydnar */
inline bool renameModelShaders;            /* ydnar */
inline bool skyFixHack;                    /* ydnar */
inline bool bspAlternateSplitWeights;                      /* 27 */
inline bool deepBSP;                   /* div0 */
inline bool maxAreaFaceSurface;                    /* divVerent */

inline int maxLMSurfaceVerts = 64;                      /* ydnar */
inline int maxSurfaceVerts = 999;                       /* ydnar */
inline int maxSurfaceIndexes = 6000;                    /* ydnar */
inline float npDegrees;                                 /* ydnar: nonplanar degrees */
inline int bevelSnap;                                   /* ydnar: bevel plane snap */
inline bool g_brushSnap = true;
inline bool flat;
inline bool meta;
inline bool patchMeta;
inline bool emitFlares;
inline bool debugSurfaces;
inline bool debugInset;
inline bool debugPortals;
inline bool debugClip;                     /* debug model autoclipping */
inline float clipDepthGlobal = 2.0f;
inline int metaAdequateScore = -1;
inline int metaGoodScore = -1;
inline bool g_noob;
inline int g_globalSurfaceFlags;
inline String64 globalCelShader;
inline bool keepLights;
inline bool keepModels;

#if Q3MAP2_EXPERIMENTAL_SNAP_NORMAL_FIX
// Increasing the normalEpsilon to compensate for new logic in SnapNormal(), where
// this epsilon is now used to compare against 0 components instead of the 1 or -1
// components.  Unfortunately, normalEpsilon is also used in PlaneEqual().  So changing
// this will affect anything that calls PlaneEqual() as well (which are, at the time
// of this writing, FindFloatPlane() and AddBrushBevels()).
inline double normalEpsilon = 0.00005;
#else
inline double normalEpsilon = 0.00001;
#endif

#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
// NOTE: This distanceEpsilon is too small if parts of the map are at maximum world
// extents (in the range of plus or minus 2^16).  The smallest epsilon at values
// close to 2^16 is about 0.007, which is greater than distanceEpsilon.  Therefore,
// maps should be constrained to about 2^15, otherwise slightly undesirable effects
// may result.  The 0.01 distanceEpsilon used previously is just too coarse in my
// opinion.  The real fix for this problem is to have 64 bit distances and then make
// this epsilon even smaller, or to constrain world coordinates to plus minus 2^15
// (or even 2^14).
inline double distanceEpsilon = 0.005;
#else
inline double distanceEpsilon = 0.01;
#endif


/* bsp */
inline int blockSize[ 3 ] = { 1024, 1024, 1024 };                          /* should be the same as in radiant */

inline CopiedString g_enginePath;

inline char source[ 1024 ];

inline int sampleSize = DEFAULT_LIGHTMAP_SAMPLE_SIZE;          /* lightmap sample size in units */
inline int minSampleSize = DEFAULT_LIGHTMAP_MIN_SAMPLE_SIZE;   /* minimum sample size to use at all */
inline int sampleScale;                                                  /* vortex: lightmap sample scale (ie quality)*/

inline std::vector<plane_t> mapplanes;       /* mapplanes[ num ^ 1 ] will always be the mirror or mapplanes[ num ] */ /* nummapplanes will always be even */
inline MinMax g_mapMinmax;

inline const MinMax c_worldMinmax( Vector3( MIN_WORLD_COORD ), Vector3( MAX_WORLD_COORD ) );

constexpr int FOG_INVALID = -1;
inline int defaultFogNum = FOG_INVALID;                  /* ydnar: cleaner fog handling */
inline std::vector<fog_t> mapFogs;

inline brush_t            buildBrush;
inline EBrushType g_brushType = EBrushType::Undefined;


/* surface stuff */
inline mapDrawSurface_t   *mapDrawSurfs;
inline int numMapDrawSurfs;
inline int max_map_draw_surfs = 0x20000;

inline int numSurfacesByType[ static_cast<std::size_t>( ESurfaceType::Shader ) + 1 ];
inline int numStripSurfaces;
inline int numMaxAreaSurfaces;
inline int numFanSurfaces;
inline int numMergedSurfaces;
inline int numMergedVerts;

inline int numRedundantIndexes;

inline int numSurfaceModels;

inline const Vector3b debugColors[ 12 ] =
	{
		{ 255,   0,   0 },
		{ 192, 128, 128 },
		{ 255, 255,   0 },
		{ 192, 192, 128 },
		{   0, 255, 255 },
		{ 128, 192, 192 },
		{   0,   0, 255 },
		{ 128, 128, 192 },
		{ 255,   0, 255 },
		{ 192, 128, 192 },
		{   0, 255,   0 },
		{ 128, 192, 128 }
	};

inline int skyboxArea = AREA_INVALID;
inline Matrix4 skyboxTransform;



/* -------------------------------------------------------------------------------

   light global variables

   ------------------------------------------------------------------------------- */

/* commandline arguments */
inline bool wolfLight;
inline float extraDist;
inline bool loMem;
inline bool noStyles;

//inline int sampleSize = DEFAULT_LIGHTMAP_SAMPLE_SIZE;
//inline int minSampleSize = DEFAULT_LIGHTMAP_MIN_SAMPLE_SIZE;
inline float noVertexLighting;
inline bool noLightmaps;
inline bool noGridLighting;

inline bool noTrace;
inline bool noSurfaces;
inline bool patchShadows;
inline bool cpmaHack;

inline bool deluxemap;
inline bool debugDeluxemap;
inline int deluxemode;                  /* deluxemap format (0 - modelspace, 1 - tangentspace with renormalization, 2 - tangentspace without renormalization) */

inline bool fast;
inline bool fastpoint = true;
inline bool faster;
inline bool fastgrid;
inline bool fastbounce;
inline bool cheap;
inline bool cheapgrid;
inline int bounce;
inline bool bounceOnly;
inline bool bouncing;
inline bool bouncegrid;
inline bool normalmap;
inline bool trisoup;
inline bool shade;
inline float shadeAngleDegrees;
inline int superSample;
inline int lightSamples = 1;
inline bool lightRandomSamples;
inline int lightSamplesSearchBoxSize = 1;
inline bool filter;
inline bool dark;
inline bool sunOnly;
inline bool g_oneSky; /* fallback to old behavior: any sky emits total of all suns/skylights in the map */
inline int approximateTolerance;
inline bool noCollapse;
inline int lightmapSearchBlockSize;
inline bool exportLightmaps;
inline bool externalLightmaps;
inline int lmCustomSizeW = LIGHTMAP_WIDTH;
inline int lmCustomSizeH = LIGHTMAP_WIDTH;
inline const char *       lmCustomDir;
inline int lmLimitSize;

inline bool lightmapTriangleCheck;
inline bool lightmapExtraVisClusterNudge;
inline bool lightmapFill;
inline bool lightmapPink;

inline bool dirty;
inline bool dirtDebug;
inline int dirtMode;
inline float dirtDepth = 128.0f;
inline float dirtScale = 1;
inline float dirtGain = 1;

/* 27: floodlighting */
inline bool debugnormals;
inline bool floodlighty;
inline bool floodlight_lowquality;
inline Vector3 floodlightRGB;
inline float floodlightIntensity = 128.0f;
inline float floodlightDistance = 1024.0f;
inline float floodlightDirectionScale = 1;

inline bool dump;
inline bool debug;
inline bool debugAxis;
inline bool debugCluster;
inline bool debugOrigin;
inline bool lightmapBorder;
inline int debugSampleSize; // 1=warn; 0=warn if lmsize>128

/* for run time tweaking of light sources */
inline float pointScale = 7500.0f;
inline float spotScale = 7500.0f;
inline float areaScale = 0.25f;
inline float skyScale = 1;
inline float bounceScale = 0.25f;
inline float bounceColorRatio = 1;
inline float vertexglobalscale = 1;
inline float g_backsplashFractionScale = 1;
inline float g_backsplashDistance = -999.0f;

/* jal: alternative angle attenuation curve */
inline bool lightAngleHL;

/* vortex: gridscale and gridambientscale */
inline float gridScale = 1;
inline float gridAmbientScale = 1;
inline float gridDirectionality = 1;
inline float gridAmbientDirectionality;
inline bool inGrid;

/* ydnar: lightmap gamma/compensation */
inline float lightmapGamma = 1;
inline float lightmapsRGB;
inline float texturesRGB;
inline float colorsRGB;
inline float lightmapExposure;
inline float lightmapCompensate = 1;
inline float lightmapBrightness = 1;
inline float lightmapContrast = 1;
inline float g_lightmapSaturation = 1;

/* ydnar: for runtime tweaking of falloff tolerance */
inline float falloffTolerance = 1;
inline const bool exactPointToPolygon = true;
inline const float formFactorValueScale = 3.0f;
inline const float linearScale = 1.0f / 8000.0f;

inline std::list<light_t> lights;
inline int numPointLights;
inline int numSpotLights;
inline int numSunLights;

/* ydnar: for luxel placement */
inline int numSurfaceClusters, maxSurfaceClusters;
inline int                *surfaceClusters;

/* ydnar: for radiosity */
inline int numDiffuseLights;
inline int numBrushDiffuseLights;
inline int numTriangleDiffuseLights;
inline int numPatchDiffuseLights;
inline const float diffuseSubdivide = 256.0f;
inline const float minDiffuseSubdivide = 64.0f;
inline int numDiffuseSurfaces;

/* ydnar: general purpose extra copy of drawvert list */
inline DrawVerts yDrawVerts;

inline const int defaultLightSubdivide = 999;

inline Vector3 minLight, minVertexLight, minGridLight;
inline float maxLight = 255.f;

/* ydnar: light optimization */
inline float subdivideThreshold = DEFAULT_SUBDIVIDE_THRESHOLD;

inline int maxOpaqueBrush;
inline std::vector<std::uint8_t> opaqueBrushes;

inline int gridBoundsCulled;
inline int gridEnvelopeCulled;

inline int lightsBoundsCulled;
inline int lightsEnvelopeCulled;
inline int lightsPlaneCulled;
inline int lightsClusterCulled;

/* ydnar: list of surface information necessary for lightmap calculation */
inline surfaceInfo_t      *surfaceInfos;

/* clumps of surfaces that share a raw lightmap */
inline int numLightSurfaces;
inline int                *lightSurfaces;

/* raw lightmaps */
inline int numRawLightmaps;
inline rawLightmap_t      *rawLightmaps;
inline int                *sortLightmaps;

/* vertex luxels */
inline Vector3            *vertexLuxels[ MAX_LIGHTMAPS ];
inline Vector3            *radVertexLuxels[ MAX_LIGHTMAPS ];

inline Vector3& getVertexLuxel( int lightmapNum, int vertexNum ){
	return vertexLuxels[lightmapNum][vertexNum];
}
inline Vector3& getRadVertexLuxel( int lightmapNum, int vertexNum ){
	return radVertexLuxels[lightmapNum][vertexNum];
}

/* bsp lightmaps */
inline int numLightmapShaders;
inline int numSolidLightmaps;
inline int numOutLightmaps;
inline int numBSPLightmaps;
inline int numExtLightmaps;
inline outLightmap_t      *outLightmaps;

/* grid points */
inline std::vector<rawGridPoint_t> rawGridPoints;

inline int numLuxels;
inline int numLuxelsMapped;
inline int numLuxelsOccluded;
inline int numLuxelsIlluminated;
inline int numVertsIlluminated;

/* lightgrid */
inline Vector3 gridMins;
inline int gridBounds[ 3 ];
inline Vector3 gridSize = { 64, 64, 128 };



/* -------------------------------------------------------------------------------

   abstracted bsp globals

   ------------------------------------------------------------------------------- */

inline std::size_t numBSPEntities;
inline std::vector<entity_t> entities;

inline std::vector<bspModel_t> bspModels;

inline std::vector<bspShader_t> bspShaders;

inline std::vector<char> bspEntData;

inline std::vector<bspLeaf_t> bspLeafs; // MAX_MAP_LEAFS

inline std::vector<bspPlane_t> bspPlanes;

inline std::vector<bspNode_t> bspNodes;

inline std::vector<int> bspLeafSurfaces;

inline std::vector<int> bspLeafBrushes;

inline std::vector<bspBrush_t> bspBrushes;

inline std::vector<bspBrushSide_t> bspBrushSides;

inline std::vector<byte> bspLightBytes;

inline std::vector<bspGridPoint_t> bspGridPoints;

inline std::vector<byte> bspVisBytes; // MAX_MAP_VISIBILITY

inline DrawVerts bspDrawVerts;

inline DrawIndexes bspDrawIndexes;

inline std::vector<bspDrawSurface_t> bspDrawSurfaces; // MAX_MAP_DRAW_SURFS

inline std::vector<bspFog_t> bspFogs;

inline std::vector<bspAdvertisement_t> bspAds;

#define AUTOEXPAND_BY_REALLOC_ADD( ptr, used, allocated, add ) \
	do \
	{ \
		if ( used >= allocated )	\
		{ \
			allocated += add; \
			ptr = void_ptr( realloc( ptr, sizeof( *ptr ) * allocated ) ); \
			if ( !ptr ) { \
				Error( # ptr " out of memory" ); } \
		} \
	} \
	while ( 0 )

#define Image_LinearFloatFromsRGBFloat( c ) ( ( ( c ) <= 0.04045f ) ? ( c ) * ( 1.0f / 12.92f ) : (float)pow( ( ( c ) + 0.055f ) * ( 1.0f / 1.055f ), 2.4f ) )
#define Image_sRGBFloatFromLinearFloat( c ) ( ( ( c ) < 0.0031308f ) ? ( c ) * 12.92f : 1.055f * (float)pow( ( c ), 1.0f / 2.4f ) - 0.055f )

inline void ColorFromSRGB( Vector3& color ){
	if ( colorsRGB ) {
		color[0] = Image_LinearFloatFromsRGBFloat( color[0] );
		color[1] = Image_LinearFloatFromsRGBFloat( color[1] );
		color[2] = Image_LinearFloatFromsRGBFloat( color[2] );
	}
}

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



/* marker */
#ifndef Q3MAP2_H
#define Q3MAP2_H



/* version */
#ifndef Q3MAP_VERSION
#error no Q3MAP_VERSION defined
#endif
#define Q3MAP_MOTD      "Your map saw the pretty lights from q3map2's BFG"




/* -------------------------------------------------------------------------------

   dependencies

   ------------------------------------------------------------------------------- */

/* platform-specific */
#if defined( __linux__ ) || defined( __APPLE__ )
	#define Q_UNIX
#endif

#ifdef Q_UNIX
	#include <unistd.h>
	#include <pwd.h>
	#include <limits.h>
#endif

#ifdef WIN32
	#include <windows.h>
#endif


/* general */
#include "version.h"            /* ttimo: might want to guard that if built outside of the GtkRadiant tree */

#include "cmdlib.h"
#include "md5lib.h"
#include "ddslib.h"

#include "picomodel.h"

#include "scriplib.h"
#include "polylib.h"
#include "imagelib.h"
#include "qthreads.h"
#include "inout.h"
#include "vfs.h"
#include "png.h"
#include "md4.h"

#include "stringfixedsize.h"
#include "stream/stringstream.h"
#include "bitflags.h"
#include <list>
#include "qmath.h"

#include <stddef.h>
#include <stdlib.h>

#include "maxworld.h"


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

#define MAX_IMAGES              2048
#define DEFAULT_IMAGE           "*default"

#define MAX_MODELS              2048

#define DEF_BACKSPLASH_FRACTION 0.05f   /* 5% backsplash by default */
#define DEF_BACKSPLASH_DISTANCE 23

#define DEF_RADIOSITY_BOUNCE    1.0f    /* ydnar: default to 100% re-emitted light */

#define MAX_SHADER_INFO         8192
#define MAX_CUST_SURFACEPARMS   256

#define SHADER_MAX_VERTEXES     1000
#define SHADER_MAX_INDEXES      ( 6 * SHADER_MAX_VERTEXES )

#define MAX_JITTERS             256


/* epair parsing (note case-sensitivity directive) */
#define CASE_INSENSITIVE_EPAIRS 1

#if CASE_INSENSITIVE_EPAIRS
	#define EPAIR_EQUAL        striEqual
#else
	#define EPAIR_EQUAL        strEqual
#endif


/* ydnar: compiler flags, because games have widely varying content/surface flags */
#define C_SOLID                 0x00000001
#define C_TRANSLUCENT           0x00000002
#define C_STRUCTURAL            0x00000004
#define C_HINT                  0x00000008
#define C_NODRAW                0x00000010
#define C_LIGHTGRID             0x00000020
#define C_ALPHASHADOW           0x00000040
#define C_LIGHTFILTER           0x00000080
#define C_VERTEXLIT             0x00000100
#define C_LIQUID                0x00000200
#define C_FOG                   0x00000400
#define C_SKY                   0x00000800
#define C_ORIGIN                0x00001000
#define C_AREAPORTAL            0x00002000
#define C_ANTIPORTAL            0x00004000  /* like hint, but doesn't generate portals */
#define C_SKIP                  0x00008000  /* like hint, but skips this face (doesn't split bsp) */
#define C_NOMARKS               0x00010000  /* no decals */
#define C_OB                    0x00020000  /* skip -noob for this */
#define C_DETAIL                0x08000000  /* THIS MUST BE THE SAME AS IN RADIANT! */


/* shadow flags */
#define WORLDSPAWN_CAST_SHADOWS 1
#define WORLDSPAWN_RECV_SHADOWS 1
#define ENTITY_CAST_SHADOWS     0
#define ENTITY_RECV_SHADOWS     1


/* bsp */
#define MAX_PATCH_SIZE          32
#define MAX_BRUSH_SIDES         1024
#define MAX_BUILD_SIDES         1024

#define MAX_EXPANDED_AXIS       128

#define CLIP_EPSILON            0.1f
#define PLANESIDE_EPSILON       0.001f
#define PLANENUM_LEAF           -1

#define HINT_PRIORITY           1000        /* ydnar: force hint splits first and antiportal/areaportal splits last */
#define ANTIPORTAL_PRIORITY     -1000
#define AREAPORTAL_PRIORITY     -1000
#define DETAIL_PRIORITY     -3000

#define PSIDE_FRONT             1
#define PSIDE_BACK              2
#define PSIDE_BOTH              ( PSIDE_FRONT | PSIDE_BACK )
#define PSIDE_FACING            4

enum class EBrushType
{
	Undefined,
	Quake,
	Bp,
	Valve220
};

/* vis */
#define VIS_HEADER_SIZE         8

#define SEPERATORCACHE          /* seperator caching helps a bit */

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

#define EXTRA_SCALE             2   /* -extrawide = -super 2 */
#define EXTRAWIDE_SCALE         2   /* -extrawide = -super 2 -filter */

#define CLUSTER_UNMAPPED        -1
#define CLUSTER_OCCLUDED        -2
#define CLUSTER_FLOODED         -3

#define FLAG_FORCE_SUBSAMPLING 1
#define FLAG_ALREADY_SUBSAMPLED 2


/* -------------------------------------------------------------------------------

   abstracted bsp file

   ------------------------------------------------------------------------------- */

#define EXTERNAL_LIGHTMAP       "lm_%04d.tga"

#define MAX_LIGHTMAPS           4           /* RBSP */
#define MAX_LIGHT_STYLES        64
#define MAX_SWITCHED_LIGHTS     32
#define LS_NORMAL               0x00
#define LS_UNUSED               0xFE
#define LS_NONE                 0xFF

#define MAX_LIGHTMAP_SHADERS    256

/* ok to increase these at the expense of more memory */
#define MAX_MAP_AREAS           0x100       /* MAX_MAP_AREA_BYTES in q_shared must match! */
#define	MAX_MAP_FOGS			0x100			//& 0x100	/* RBSP (32 - world fog - goggles) */
#define MAX_MAP_LEAFS           0x20000
#define MAX_MAP_PORTALS         0x20000
#define MAX_MAP_LIGHTING        0x800000
#define MAX_MAP_LIGHTGRID       0x100000    //%	0x800000 /* ydnar: set to points, not bytes */
#define MAX_MAP_VISCLUSTERS     0x4000 // <= MAX_MAP_LEAFS
#define MAX_MAP_VISIBILITY      ( VIS_HEADER_SIZE + MAX_MAP_VISCLUSTERS * ( ( ( MAX_MAP_VISCLUSTERS + 63 ) & ~63 ) >> 3 ) )

#define	MAX_MAP_DRAW_SURFS		0x20000

#define MAX_MAP_ADVERTISEMENTS  30

/* the editor uses these predefined yaw angles to orient entities up or down */
#define ANGLE_UP                -1
#define ANGLE_DOWN              -2

#define LIGHTMAP_WIDTH          128
#define LIGHTMAP_HEIGHT         128



typedef void ( *bspFunc )( const char * );


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
	Vector3b ambient[ MAX_LIGHTMAPS ];
	Vector3b directed[ MAX_LIGHTMAPS ];
	byte styles[ MAX_LIGHTMAPS ];
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


/* ydnar: for multiple game support */
struct surfaceParm_t
{
	const char  *name;
	int contentFlags, contentFlagsClear;
	int surfaceFlags, surfaceFlagsClear;
	int compileFlags, compileFlagsClear;
};

enum class EMiniMapMode
{
	Gray,
	Black,
	White
};

struct game_t
{
	const char          *arg;                           /* -game matches this */
	const char          *gamePath;                      /* main game data dir */
	const char          *homeBasePath;                  /* home sub-dir on unix */
	const char          *magic;                         /* magic word for figuring out base path */
	const char          *shaderPath;                    /* shader directory */
	int maxLMSurfaceVerts;                              /* default maximum meta surface verts */
	int maxSurfaceVerts;                                /* default maximum surface verts */
	int maxSurfaceIndexes;                              /* default maximum surface indexes (tris * 3) */
	bool emitFlares;                                    /* when true, emit flare surfaces */
	const char          *flareShader;                   /* default flare shader (MUST BE SET) */
	bool wolfLight;                                     /* when true, lights work like wolf q3map  */
	int lightmapSize;                                   /* bsp lightmap width/height */
	float lightmapGamma;                                /* default lightmap gamma */
	bool lightmapsRGB;                                  /* default lightmap sRGB mode */
	bool texturesRGB;                                   /* default texture sRGB mode */
	bool colorsRGB;                                     /* default color sRGB mode */
	float lightmapExposure;                             /* default lightmap exposure */
	float lightmapCompensate;                           /* default lightmap compensate value */
	float gridScale;                                    /* vortex: default lightgrid scale (affects both directional and ambient spectres) */
	float gridAmbientScale;                             /* vortex: default lightgrid ambient spectre scale */
	bool lightAngleHL;                                  /* jal: use half-lambert curve for light angle attenuation */
	bool noStyles;                                      /* use lightstyles hack or not */
	bool keepLights;                                    /* keep light entities on bsp */
	int patchSubdivisions;                              /* default patch subdivisions tolerance */
	bool patchShadows;                                  /* patch casting enabled */
	bool deluxeMap;                                     /* compile deluxemaps */
	int deluxeMode;                                     /* deluxemap mode (0 - modelspace, 1 - tangentspace with renormalization, 2 - tangentspace without renormalization) */
	int miniMapSize;                                    /* minimap size */
	float miniMapSharpen;                               /* minimap sharpening coefficient */
	float miniMapBorder;                                /* minimap border amount */
	bool miniMapKeepAspect;                             /* minimap keep aspect ratio by letterboxing */
	EMiniMapMode miniMapMode;                           /* minimap mode */
	const char          *miniMapNameFormat;             /* minimap name format */
	const char          *bspIdent;                      /* 4-letter bsp file prefix */
	int bspVersion;                                     /* bsp version to use */
	bool lumpSwap;                                      /* cod-style len/ofs order */
	bspFunc load, write;                                /* load/write function pointers */
	surfaceParm_t surfaceParms[ 128 ];                  /* surfaceparm array */
	int brushBevelsSurfaceFlagsMask;                    /* apply only these surfaceflags to bevels to reduce extra bsp shaders amount; applying them to get correct physics at walkable brush edges and vertices */
};


struct image_t
{
	char                *name, *filename;
	int refCount;
	int width, height;
	byte                *pixels;
};


struct sun_t
{
	sun_t        *next;
	Vector3 direction, color;
	float photons, deviance, filterRadius;
	int numSamples, style;
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
	colorMod_t   *next;
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


struct shaderInfo_t
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

	std::list<surfaceModel_t> surfaceModels;            /* ydnar: for distribution of models */
	std::list<foliage_t>      foliage;                  /* ydnar/splash damage: wolf et foliage */

	float subdivisions;                                 /* from a "tesssize xxx" */
	float backsplashFraction;                           /* floating point value, usually 0.05 */
	float backsplashDistance;                           /* default 16 */
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
	colorMod_t          *colorMod;                      /* ydnar: q3map_rgb/color/alpha/Set/Mod support */

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

	image_t             *shaderImage;
	image_t             *lightImage;
	image_t             *normalImage;

	float skyLightValue;                                /* ydnar */
	int skyLightIterations;                             /* ydnar */
	sun_t               *sun;                           /* ydnar */

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



/* -------------------------------------------------------------------------------

   bsp structures

   ------------------------------------------------------------------------------- */

struct face_t
{
	face_t              *next;
	int planenum;
	int priority;
	//bool			checked;
	int compileFlags;
	winding_t           *w;
};


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

	winding_t           *winding;
	winding_t           *visibleHull;       /* convex hull of all visible fragments */

	shaderInfo_t        *shaderInfo;

	int contentFlags;                       /* from shaderInfo */
	int surfaceFlags;                       /* from shaderInfo */
	int compileFlags;                       /* from shaderInfo */
	int value;                              /* from shaderInfo */

	bool visible;                           /* choose visible planes first */
	bool bevel;                             /* don't ever use for bsp splitting, and don't bother making windings for it */
	bool culled;                            /* ydnar: face culling */
};


struct sideRef_t
{
	sideRef_t           *next;
	side_t              *side;
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
	brush_t             *next;
	brush_t             *nextColorModBrush; /* ydnar: colorMod volume brushes go here */
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
	MinMax eMinmax;
	indexMap_t          *im;

	int contentFlags;
	int compileFlags;                       /* ydnar */
	bool detail;
	bool opaque;

	int portalareas[ 2 ];

	MinMax minmax;
	int numsides;

	side_t sides[];                         /* variably sized */
};


struct fog_t
{
	shaderInfo_t        *si;
	brush_t             *brush;
	int visibleSide;                        /* the brush side that ray tests need to clip against (-1 == none) */
};


struct mesh_t
{
	int width, height;
	bspDrawVert_t       *verts;
};


struct parseMesh_t
{
	parseMesh_t  *next;

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
	MinMax eMinmax;
	indexMap_t          *im;

	/* grouping */
	bool grouped;
	float longestCurve;
	int maxIterations;
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
	int outputNum;                          /* ydnar: to match this sort of thing up */

	bool fur;                               /* ydnar: this is kind of a hack, but hey... */
	bool skybox;                            /* ydnar: yet another fun hack */
	bool backSide;                          /* ydnar: q3map_backShader support */

	mapDrawSurface_t *parent;        /* ydnar: for cloned (skybox) surfaces to share lighting data */
	mapDrawSurface_t *clone;         /* ydnar: for cloned surfaces */
	mapDrawSurface_t *cel;           /* ydnar: for cloned cel surfaces */

	shaderInfo_t        *shaderInfo;
	shaderInfo_t        *celShader;
	brush_t             *mapBrush;
	parseMesh_t         *mapMesh;
	sideRef_t           *sideRef;

	int fogNum;

	int numVerts;                           /* vertexes and triangles */
	bspDrawVert_t       *verts;
	int numIndexes;
	int                 *indexes;

	int planeNum;
	Vector3 lightmapOrigin;                 /* also used for flares */
	Vector3 lightmapVecs[ 3 ];              /* also used for flares */
	int lightStyle;                         /* used for flares */

	/* ydnar: per-surface (per-entity, actually) lightmap sample size scaling */
	float lightmapScale;

	/* jal: per-surface (per-entity, actually) shadeangle */
	float shadeAngleDegrees;

	/* ydnar: surface classification */
	MinMax minmax;
	Vector3 lightmapAxis;
	int sampleSize;

	/* ydnar: shadow group support */
	int castShadows, recvShadows;

	/* ydnar: texture coordinate range monitoring for hardware with limited texcoord precision (in texel space) */
	Vector2 bias;
	int texMins[ 2 ], texMaxs[ 2 ], texRange[ 2 ];

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
};


struct drawSurfRef_t
{
	drawSurfRef_t    *nextRef;
	int outputNum;
};


/* ydnar: metasurfaces are constructed from lists of metatriangles so they can be merged in the best way */
struct metaTriangle_t
{
	shaderInfo_t        *si;
	side_t              *side;
	int entityNum, surfaceNum, planeNum, fogNum, sampleSize, castShadows, recvShadows;
	float shadeAngleDegrees;
	Plane3f plane;
	Vector3 lightmapAxis;
	int indexes[ 3 ];
};


struct epair_t
{
	CopiedString key, value;
};


struct entity_t
{
	Vector3 origin;
	brush_t             *brushes, *lastBrush, *colorModBrushes;
	parseMesh_t         *patches;
	int mapEntityNum, firstDrawSurf;
	int firstBrush, numBrushes;                     /* only valid during BSP compile */
	std::list<epair_t> epairs;
	Vector3 originbrush_origin;

	void setKeyValue( const char *key, const char *value );
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
	bool read_keyvalue_( char (&string_value)[1024], std::initializer_list<const char*>&& keys ) const;
	bool read_keyvalue_( const char *&string_ptr_value, std::initializer_list<const char*>&& keys ) const;
};


struct node_t
{
	/* both leafs and nodes */
	int planenum;                       /* -1 = leaf node */
	node_t              *parent;
	MinMax minmax;                      /* valid after portalization */
	brush_t             *volume;        /* one for each leaf/node */

	/* nodes only */
	side_t              *side;          /* the side that created the node */
	node_t              *children[ 2 ];
	int compileFlags;                   /* ydnar: hint, antiportal */
	int tinyportals;
	Vector3 referencepoint;

	/* leafs only */
	bool opaque;                        /* view can never be inside */
	bool areaportal;
	bool skybox;                        /* ydnar: a skybox leaf */
	bool sky;                           /* ydnar: a sky leaf */
	int cluster;                        /* for portalfile writing */
	int area;                           /* for areaportals */
	brush_t             *brushlist;     /* fragments of all brushes in this leaf */
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
	winding_t           *winding;

	bool sidefound;                     /* false if ->side hasn't been checked */
	int compileFlags;                   /* from original face that caused the split */
	side_t              *side;          /* NULL = non-visible */
};


struct tree_t
{
	node_t              *headnode;
	node_t outside_node;
	MinMax minmax;
};



/* -------------------------------------------------------------------------------

   vis structures

   ------------------------------------------------------------------------------- */

using visPlane_t = Plane3f;


struct fixedWinding_t
{
	int numpoints;
	Vector3 points[ MAX_POINTS_ON_FIXED_WINDING ];                   /* variable sized */
};


struct passage_t
{
	struct passage_t    *next;
	byte cansee[ 1 ];                   /* all portals that can be seen through this passage */
};


enum class EVStatus
{
	None,
	Working,
	Done
};


struct vportal_t
{
	int num;
	bool hint;                          /* true if this portal was created from a hint splitter */
	bool sky;                           /* true if this portal belongs to a sky leaf */
	bool removed;
	visPlane_t plane;                   /* normal pointing into neighbor */
	int leaf;                           /* neighbor */

	Vector3 origin;                     /* for fast clip testing */
	float radius;

	fixedWinding_t      *winding;
	EVStatus status;
	byte                *portalfront;   /* [portals], preliminary */
	byte                *portalflood;   /* [portals], intermediate */
	byte                *portalvis;     /* [portals], final */

	int nummightsee;                    /* bit count on portalflood for sort */
	passage_t           *passages;      /* there are just as many passages as there */
	                                    /* are portals in the leaf this portal leads */
};


struct leaf_t
{
	int numportals;
	int merged;
	vportal_t           *portals[MAX_PORTALS_ON_LEAF];
};


struct pstack_t
{
	byte mightsee[ MAX_PORTALS / 8 ];
	pstack_t            *next;
	leaf_t              *leaf;
	vportal_t           *portal;        /* portal exiting */
	fixedWinding_t      *source;
	fixedWinding_t      *pass;

	fixedWinding_t windings[ 3 ];       /* source, pass, temp in any order */
	int freewindings[ 3 ];

	visPlane_t portalplane;
	int depth;
#ifdef SEPERATORCACHE
	visPlane_t seperators[ 2 ][ MAX_SEPERATORS ];
	int numseperators[ 2 ];
#endif
};


struct threaddata_t
{
	vportal_t           *base;
	int c_chains;
	pstack_t pstack_head;
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
	light_t             *next;

	ELightType type;
	LightFlags flags;                   /* ydnar: condensed all the booleans into one flags int */
	shaderInfo_t        *si;

	Vector3 origin;
	Vector3 normal;                     /* for surfaces, spotlights, and suns */
	float dist;                         /* plane location along normal */

	float photons;
	int style;
	Vector3 color;
	float radiusByDist;                 /* for spotlights */
	float fade;                         /* ydnar: from wolf, for linear lights */
	float angleScale;                   /* ydnar: stolen from vlight for K */
	float extraDist;                    /* "extra dimension" distance of the light, to kill hot spots */

	float add;                          /* ydnar: used for area lights */
	float envelope;                     /* ydnar: units until falloff < tolerance */
	float envelope2;                    /* ydnar: envelope squared (tiny optimization) */
	MinMax minmax;                      /* ydnar: pvs envelope */
	int cluster;                        /* ydnar: cluster light falls into */

	winding_t           *w;

	float falloffTolerance;             /* ydnar: minimum attenuation threshold */
	float filterRadius;                 /* ydnar: lightmap filter radius in world units, 0 == default */
};


struct trace_t
{
	/* constant input */
	bool testOcclusion, forceSunlight, testAll;
	int recvShadows;

	int numSurfaces;
	int                 *surfaces;

	int numLights;
	light_t             **lights;

	bool twoSided;

	/* per-sample input */
	int cluster;
	Vector3 origin, normal;
	float inhibitRadius;                /* sphere in which occluding geometry is ignored */

	/* per-light input */
	light_t             *light;
	Vector3 end;

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


/* must be identical to bspDrawVert_t except for float color! */
struct radVert_t
{
	Vector3 xyz;
	Vector2 st;
	Vector2 lightmap[ MAX_LIGHTMAPS ];
	Vector3 normal;
	Color4f color[ MAX_LIGHTMAPS ];
};


struct radWinding_t
{
	int numVerts;
	radVert_t verts[ MAX_POINTS_ON_WINDING ];
};


/* crutch for poor local allocations in win32 smp */
struct clipWork_t
{
	float dists[ MAX_POINTS_ON_WINDING + 4 ];
	EPlaneSide sides[ MAX_POINTS_ON_WINDING + 4 ];
};


/* ydnar: new lightmap handling code */
struct outLightmap_t
{
	int lightmapNum, extLightmapNum;
	int customWidth, customHeight;
	int numLightmaps;
	int freeLuxels;
	int numShaders;
	shaderInfo_t        *shaders[ MAX_LIGHTMAP_SHADERS ];
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
	shaderInfo_t        *si;
	rawLightmap_t       *lm;
	int parentSurfaceNum, childSurfaceNum;
	int entityNum, castShadows, recvShadows, sampleSize, patchIterations;
	float longestCurve;
	Plane3f               *plane;
	Vector3 axis;
	MinMax minmax;
	bool hasLightmap, approximated;
	int firstSurfaceCluster, numSurfaceClusters;
};

/* -------------------------------------------------------------------------------

   prototypes

   ------------------------------------------------------------------------------- */

inline float Random( void ){ /* returns a pseudorandom number between 0 and 1 */
	return (float) rand() / RAND_MAX;
}

/* help.c */
void                        HelpMain(const char* arg);
void                        HelpGames();

/* path_init.c */
game_t                      *GetGame( char *arg );
void                        InitPaths( int *argc, char **argv );


/* bsp.c */
int                         BSPMain( int argc, char **argv );


/* minimap.c */
int                         MiniMapBSPMain( int argc, char **argv );

/* convert_bsp.c */
int                         FixAAS( int argc, char **argv );
int                         AnalyzeBSP( int argc, char **argv );
int                         BSPInfo( int count, char **fileNames );
int                         ScaleBSPMain( int argc, char **argv );
int                         ShiftBSPMain( int argc, char **argv );
int                         ConvertBSPMain( int argc, char **argv );

/* convert_map.c */
int                         ConvertBSPToMap( char *bspName );
int                         ConvertBSPToMap_BP( char *bspName );


/* convert_ase.c */
int                         ConvertBSPToASE( char *bspName );

/* convert_obj.c */
int                         ConvertBSPToOBJ( char *bspName );


/* brush.c */
sideRef_t                   *AllocSideRef( side_t *side, sideRef_t *next );
int                         CountBrushList( brush_t *brushes );
brush_t                     *AllocBrush( int numsides );
void                        FreeBrush( brush_t *brushes );
void                        FreeBrushList( brush_t *brushes );
brush_t                     *CopyBrush( const brush_t *brush );
bool                        BoundBrush( brush_t *brush );
void                        SnapWeldVector( const Vector3& a, const Vector3& b, Vector3& out );
bool                        CreateBrushWindings( brush_t *brush );
brush_t                     *BrushFromBounds( const Vector3& mins, const Vector3& maxs );
float                       BrushVolume( brush_t *brush );
void                        WriteBSPBrushMap( const char *name, brush_t *list );

void                        FilterDetailBrushesIntoTree( entity_t *e, tree_t *tree );
void                        FilterStructuralBrushesIntoTree( entity_t *e, tree_t *tree );

bool                        WindingIsTiny( winding_t *w );

void                        SplitBrush( brush_t *brush, int planenum, brush_t **front, brush_t **back );

tree_t                      *AllocTree( void );
node_t                      *AllocNode( void );


/* mesh.c */
void                        LerpDrawVert( const bspDrawVert_t *a, const bspDrawVert_t *b, bspDrawVert_t *out );
void                        LerpDrawVertAmount( bspDrawVert_t *a, bspDrawVert_t *b, float amount, bspDrawVert_t *out );
void                        FreeMesh( mesh_t *m );
mesh_t                      *CopyMesh( mesh_t *mesh );
void                        PrintMesh( mesh_t *m );
mesh_t                      *TransposeMesh( mesh_t *in );
void                        InvertMesh( mesh_t *m );
mesh_t                      *SubdivideMesh( mesh_t in, float maxError, float minLength );
int                         IterationsForCurve( float len, int subdivisions );
mesh_t                      *SubdivideMesh2( mesh_t in, int iterations );
mesh_t                      *SubdivideMeshQuads( mesh_t *in, float minLength, int maxsize, int *widthtable, int *heighttable );
mesh_t                      *RemoveLinearMeshColumnsRows( mesh_t *in );
void                        MakeMeshNormals( mesh_t in );
void                        PutMeshOnCurve( mesh_t in );


/* map.c */
void                        LoadMapFile( char *filename, bool onlyLights, bool noCollapseGroups );
int                         FindFloatPlane( const Plane3f& plane, int numPoints, const Vector3 *points );
inline int                  FindFloatPlane( const Vector3& normal, float dist, int numPoints, const Vector3 *points ){
	return FindFloatPlane( Plane3f( normal, dist ), numPoints, points );
}
bool                        PlaneEqual( const plane_t& p, const Plane3f& plane );
void                        AddBrushBevels( void );
brush_t                     *FinishBrush( bool noCollapseGroups );


/* portals.c */
void                        MakeHeadnodePortals( tree_t *tree );
void                        MakeNodePortal( node_t *node );
void                        SplitNodePortals( node_t *node );

bool                        PortalPassable( portal_t *p );

enum class EFloodEntities
{
	Leaked,
	Good,
	Empty
};
EFloodEntities              FloodEntities( tree_t *tree );
void                        FillOutside( node_t *headnode );
void                        FloodAreas( tree_t *tree );
face_t                      *VisibleFaces( entity_t *e, tree_t *tree );
void                        FreePortal( portal_t *p );

void                        MakeTreePortals( tree_t *tree );


/* leakfile.c */
xmlNodePtr                  LeakFile( tree_t *tree );


/* prtfile.c */
void                        NumberClusters( tree_t *tree );
void                        WritePortalFile( tree_t *tree );


/* writebsp.c */
void                        SetModelNumbers( void );
void                        SetLightStyles( void );

int                         EmitShader( const char *shader, int *contentFlags, int *surfaceFlags );

void                        BeginBSPFile( void );
void                        EndBSPFile( bool do_write );
void                        EmitBrushes( brush_t *brushes, int *firstBrush, int *numBrushes );
void                        EmitFogs( void );

void                        BeginModel( void );
void                        EndModel( entity_t *e, node_t *headnode );


/* tree.c */
void                        FreeTree( tree_t *tree );
void                        FreeTree_r( node_t *node );
void                        PrintTree_r( node_t *node, int depth );
void                        FreeTreePortals_r( node_t *node );


/* patch.c */
void                        ParsePatch( bool onlyLights );
mesh_t                      *SubdivideMesh( mesh_t in, float maxError, float minLength );
void                        PatchMapDrawSurfs( entity_t *e );
void                        TriangulatePatchSurface( entity_t *e, mapDrawSurface_t *ds );


/* tjunction.c */
void                        FixTJunctions( entity_t *e );


/* fog.c */
winding_t                   *WindingFromDrawSurf( mapDrawSurface_t *ds );
void                        FogDrawSurfaces( entity_t *e );
int                         FogForPoint( const Vector3& point, float epsilon );
int                         FogForBounds( const MinMax& minmax, float epsilon );
void                        CreateMapFogs( void );


/* facebsp.c */
face_t                      *MakeStructuralBSPFaceList( brush_t *list );
face_t                      *MakeVisibleBSPFaceList( brush_t *list );
tree_t                      *FaceBSP( face_t *list );


/* model.c */
void                        PicoPrintFunc( int level, const char *str );
void                        PicoLoadFileFunc( const char *name, byte **buffer, int *bufSize );
picoModel_t                 *FindModel( const char *name, int frame );
picoModel_t                 *LoadModel( const char *name, int frame );
void                        InsertModel( const char *name, int skin, int frame, const Matrix4& transform, const std::list<remap_t> *remaps, shaderInfo_t *celShader, int eNum, int castShadows, int recvShadows, int spawnFlags, float lightmapScale, int lightmapSampleSize, float shadeAngle, float clipDepth );
void                        AddTriangleModels( entity_t *e );


/* surface.c */
mapDrawSurface_t            *AllocDrawSurface( ESurfaceType type );
void                        FinishSurface( mapDrawSurface_t *ds );
void                        StripFaceSurface( mapDrawSurface_t *ds );
void                        MaxAreaFaceSurface( mapDrawSurface_t *ds );
bool                        CalcSurfaceTextureRange( mapDrawSurface_t *ds );
Vector3                     CalcLightmapAxis( const Vector3& normal );
void                        ClassifySurfaces( int numSurfs, mapDrawSurface_t *ds );
void                        ClassifyEntitySurfaces( entity_t *e );
void                        TidyEntitySurfaces( entity_t *e );
mapDrawSurface_t            *CloneSurface( mapDrawSurface_t *src, shaderInfo_t *si );
mapDrawSurface_t            *MakeCelSurface( mapDrawSurface_t *src, shaderInfo_t *si );
bool                        IsTriangleDegenerate( bspDrawVert_t *points, int a, int b, int c );
void                        ClearSurface( mapDrawSurface_t *ds );
void                        AddEntitySurfaceModels( entity_t *e );
mapDrawSurface_t            *DrawSurfaceForSide( entity_t *e, brush_t *b, side_t *s, winding_t *w );
mapDrawSurface_t            *DrawSurfaceForMesh( entity_t *e, parseMesh_t *p, mesh_t *mesh );
mapDrawSurface_t            *DrawSurfaceForFlare( int entNum, const Vector3& origin, const Vector3& normal, const Vector3& color, const char *flareShader, int lightStyle );
mapDrawSurface_t            *DrawSurfaceForShader( const char *shader );
void                        ClipSidesIntoTree( entity_t *e, tree_t *tree );
void                        MakeDebugPortalSurfs( tree_t *tree );
void                        MakeFogHullSurfs( entity_t *e, tree_t *tree, const char *shader );
void                        SubdivideFaceSurfaces( entity_t *e, tree_t *tree );
void                        AddEntitySurfaceModels( entity_t *e );
int                         AddSurfaceModels( mapDrawSurface_t *ds );
void                        FilterDrawsurfsIntoTree( entity_t *e, tree_t *tree );
void                        EmitPatchSurface( entity_t *e, mapDrawSurface_t *ds );
void                        EmitTriangleSurface( mapDrawSurface_t *ds );


/* surface_fur.c */
void                        Fur( mapDrawSurface_t *src );


/* surface_foliage.c */
void                        Foliage( mapDrawSurface_t *src );


/* ydnar: surface_meta.c */
void                        ClearMetaTriangles( void );
int                         FindMetaTriangle( metaTriangle_t *src, bspDrawVert_t *a, bspDrawVert_t *b, bspDrawVert_t *c, int planeNum );
void                        MakeEntityMetaTriangles( entity_t *e );
void                        FixMetaTJunctions( void );
void                        SmoothMetaTriangles( void );
void                        MergeMetaTriangles( void );
void                        EmitMetaStats(); // vortex: print meta statistics even in no-verbose mode


/* surface_extra.c */
void                        SetDefaultSampleSize( int sampleSize );

void                        SetSurfaceExtra( mapDrawSurface_t *ds, int num );

shaderInfo_t                *GetSurfaceExtraShaderInfo( int num );
int                         GetSurfaceExtraParentSurfaceNum( int num );
int                         GetSurfaceExtraEntityNum( int num );
int                         GetSurfaceExtraCastShadows( int num );
int                         GetSurfaceExtraRecvShadows( int num );
int                         GetSurfaceExtraSampleSize( int num );
int                         GetSurfaceExtraMinSampleSize( int num );
float                       GetSurfaceExtraLongestCurve( int num );
Vector3                     GetSurfaceExtraLightmapAxis( int num );

void                        WriteSurfaceExtraFile( const char *path );
void                        LoadSurfaceExtraFile( const char *path );


/* decals.c */
void                        ProcessDecals( void );
void                        MakeEntityDecals( entity_t *e );

/* map.c */
std::array<Vector3, 2>      TextureAxisFromPlane( const plane_t& plane );

/* vis.c */
fixedWinding_t              *NewFixedWinding( int points );
int                         VisMain( int argc, char **argv );

/* visflow.c */
int                         CountBits( byte *bits, int numbits );
void                        PassageFlow( int portalnum );
void                        CreatePassages( int portalnum );
void                        PassageMemory( void );
void                        BasePortalVis( int portalnum );
void                        BetterPortalVis( int portalnum );
void                        PortalFlow( int portalnum );
void                        PassagePortalFlow( int portalnum );



/* light.c  */
float                       PointToPolygonFormFactor( const Vector3& point, const Vector3& normal, const winding_t *w );
int                         LightContributionToSample( trace_t *trace );
void                        LightingAtSample( trace_t * trace, byte styles[ MAX_LIGHTMAPS ], Vector3 (&colors)[ MAX_LIGHTMAPS ] );
bool                        LightContributionToPoint( trace_t *trace );
int                         LightMain( int argc, char **argv );


/* light_trace.c */
void                        SetupTraceNodes( void );
void                        TraceLine( trace_t *trace );
float                       SetupTrace( trace_t *trace );


/* light_bounce.c */
bool                        RadSampleImage( byte * pixels, int width, int height, const Vector2& st, Color4f& color );
void                        RadLightForTriangles( int num, int lightmapNum, rawLightmap_t *lm, shaderInfo_t *si, float scale, float subdivide, clipWork_t *cw );
void                        RadLightForPatch( int num, int lightmapNum, rawLightmap_t *lm, shaderInfo_t *si, float scale, float subdivide, clipWork_t *cw );
void                        RadCreateDiffuseLights( void );
void                        RadFreeLights();


/* light_ydnar.c */
Vector3b                    ColorToBytes( const Vector3& color, float scale );
void                        SmoothNormals( void );

void                        MapRawLightmap( int num );

void                        SetupDirt();
float                       DirtForSample( trace_t *trace );
void                        DirtyRawLightmap( int num );

void                        SetupFloodLight();
void                        FloodlightRawLightmaps();
void                        FloodlightIlluminateLightmap( rawLightmap_t *lm );
float                       FloodLightForSample( trace_t *trace, float floodLightDistance, bool floodLightLowQuality );
void                        FloodLightRawLightmap( int num );

void                        IlluminateRawLightmap( int num );
void                        IlluminateVertexes( int num );

void                        SetupBrushesFlags( int mask_any, int test_any, int mask_all, int test_all );
void                        SetupBrushes( void );
void                        SetupClusters( void );
bool                        ClusterVisible( int a, int b );
bool                        ClusterVisibleToPoint( const Vector3& point, int cluster );
int                         ClusterForPoint( const Vector3& point );
int                         ClusterForPointExt( const Vector3& point, float epsilon );
int                         ClusterForPointExtFilter( const Vector3& point, float epsilon, int numClusters, int *clusters );
int                         ShaderForPointInLeaf( const Vector3& point, int leafNum, float epsilon, int wantContentFlags, int wantSurfaceFlags, int *contentFlags, int *surfaceFlags );
void                        SetupEnvelopes( bool forGrid, bool fastFlag );
void                        FreeTraceLights( trace_t *trace );
void                        CreateTraceLightsForBounds( const MinMax& minmax, const Vector3 *normal, int numClusters, int *clusters, LightFlags flags, trace_t *trace );
void                        CreateTraceLightsForSurface( int num, trace_t *trace );


/* lightmaps_ydnar.c */
void                        ExportLightmaps( void );

int                         ExportLightmapsMain( int argc, char **argv );
int                         ImportLightmapsMain( int argc, char **argv );

void                        SetupSurfaceLightmaps( void );
void                        StitchSurfaceLightmaps( void );
void                        StoreSurfaceLightmaps( bool fastAllocate );


/* exportents.c */
void                        ExportEntities( void );
int                         ExportEntitiesMain( int argc, char **argv );


/* image.c */
void                        ImageFree( image_t *image );
image_t                     *ImageFind( const char *name );
image_t                     *ImageLoad( const char *filename );


/* shaders.c */
void                        ColorMod( colorMod_t *am, int numVerts, bspDrawVert_t *drawVerts );

void                        TCMod( const tcMod_t& mod, Vector2& st );
void                        TCModIdentity( tcMod_t& mod );
void                        TCModMultiply( const tcMod_t& a, const tcMod_t& b, tcMod_t& out );
void                        TCModTranslate( tcMod_t& mod, float s, float t );
void                        TCModScale( tcMod_t& mod, float s, float t );
void                        TCModRotate( tcMod_t& mod, float euler );

bool                        ApplySurfaceParm( const char *name, int *contentFlags, int *surfaceFlags, int *compileFlags );

void                        BeginMapShaderFile( const char *mapFile );
void                        WriteMapShaderFile( void );
shaderInfo_t                *CustomShader( shaderInfo_t *si, const char *find, char *replace );
void                        EmitVertexRemapShader( char *from, char *to );

void                        LoadShaderInfo( void );
shaderInfo_t                *ShaderInfoForShader( const char *shader );
shaderInfo_t                *ShaderInfoForShaderNull( const char *shader );


/* bspfile_abstract.c */
void                        SetGridPoints( int n );
void                        SetDrawVerts( int n );
void                        IncDrawVerts();
void                        SetDrawSurfaces( int n );
void                        SetDrawSurfacesBuffer();
void                        BSPFilesCleanup();

void                        SwapBlock( int *block, int size );

int                         GetLumpElements( bspHeader_t *header, int lump, int size );
void_ptr                    GetLump( bspHeader_t *header, int lump );
int                         CopyLump( bspHeader_t *header, int lump, void *dest, int size );
int                         CopyLump_Allocate( bspHeader_t *header, int lump, void **dest, int size, int *allocationVariable );
void                        AddLump( FILE *file, bspHeader_t *header, int lumpNum, const void *data, int length );

void                        LoadBSPFile( const char *filename );
void                        PartialLoadBSPFile( const char *filename );
void                        WriteBSPFile( const char *filename );
void                        PrintBSPFileSizes( void );

void                        ParseEPair( std::list<epair_t>& epairs );
void                        ParseEntities( void );
void                        UnparseEntities( void );
void                        PrintEntity( const entity_t *ent );

entity_t                    *FindTargetEntity( const char *target );
void                        GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, int *castShadows, int *recvShadows );
void                        InjectCommandLine( char **argv, int beginArgs, int endArgs );



/* bspfile_ibsp.c */
void                        LoadIBSPFile( const char *filename );
void                        WriteIBSPFile( const char *filename );
void						PartialLoadIBSPFile( const char *filename );



/* bspfile_rbsp.c */
void                        LoadRBSPFile( const char *filename );
void                        WriteRBSPFile( const char *filename );



/* -------------------------------------------------------------------------------

   bsp/general global variables

   ------------------------------------------------------------------------------- */

#ifdef MAIN_C
	#define Q_EXTERN
	#define Q_ASSIGN( a )   = a
#else
	#define Q_EXTERN extern
	#define Q_ASSIGN( a )
#endif

/* game support */
Q_EXTERN game_t games[]
#ifndef MAIN_C
;
#else
	=
	{
	#include "game_quake3.h"
	,
	#include "game_quakelive.h" /* must be after game_quake3.h as they share defines! */
	,
	#include "game_nexuiz.h" /* must be after game_quake3.h as they share defines! */
	,
	#include "game_xonotic.h" /* must be after game_quake3.h as they share defines! */
	,
	#include "game_tremulous.h" /*LinuxManMikeC: must be after game_quake3.h, depends on #define's set in it */
	,
	#include "game_unvanquished.h"
	,
	#include "game_tenebrae.h"
	,
	#include "game_wolf.h"
	,
	#include "game_wolfet.h" /* must be after game_wolf.h as they share defines! */
	,
	#include "game_etut.h"
	,
	#include "game_ef.h"
	,
	#include "game_sof2.h"
	,
	#include "game_jk2.h"   /* must be after game_sof2.h as they share defines! */
	,
	#include "game_ja.h"    /* must be after game_jk2.h as they share defines! */
	,
	#include "game_qfusion.h"   /* qfusion game */
	,
	#include "game_reaction.h" /* must be after game_quake3.h */
	,
	#include "game_darkplaces.h"    /* vortex: darkplaces q1 engine */
	,
	#include "game_dq.h"    /* vortex: deluxe quake game ( darkplaces q1 engine) */
	,
	#include "game_prophecy.h"  /* vortex: prophecy game ( darkplaces q1 engine) */
	,
	#include "game__null.h" /* null game (must be last item) */
	};
#endif
Q_EXTERN game_t             *game Q_ASSIGN( &games[ 0 ] );


/* general */
Q_EXTERN int numImages Q_ASSIGN( 0 );
Q_EXTERN image_t images[ MAX_IMAGES ];

Q_EXTERN int numPicoModels Q_ASSIGN( 0 );
Q_EXTERN picoModel_t        *picoModels[ MAX_MODELS ];

Q_EXTERN shaderInfo_t       *shaderInfo Q_ASSIGN( NULL );
Q_EXTERN int numShaderInfo Q_ASSIGN( 0 );

Q_EXTERN surfaceParm_t custSurfaceParms[ MAX_CUST_SURFACEPARMS ];
Q_EXTERN int numCustSurfaceParms Q_ASSIGN( 0 );

Q_EXTERN String64 mapName;                 /* ydnar: per-map custom shaders for larger lightmaps */
Q_EXTERN CopiedString mapShaderFile;
Q_EXTERN bool warnImage Q_ASSIGN( true );

/* ydnar: sinusoid samples */
Q_EXTERN float jitters[ MAX_JITTERS ];

/* can't code */
Q_EXTERN bool doingBSP Q_ASSIGN( false );

/* commandline arguments */
Q_EXTERN bool verboseEntities Q_ASSIGN( false );
Q_EXTERN bool force Q_ASSIGN( false );
Q_EXTERN bool useCustomInfoParms Q_ASSIGN( false );
Q_EXTERN bool leaktest Q_ASSIGN( false );
Q_EXTERN bool nodetail Q_ASSIGN( false );
Q_EXTERN bool nosubdivide Q_ASSIGN( false );
Q_EXTERN bool notjunc Q_ASSIGN( false );
Q_EXTERN bool fulldetail Q_ASSIGN( false );
Q_EXTERN bool nowater Q_ASSIGN( false );
Q_EXTERN bool noCurveBrushes Q_ASSIGN( false );
Q_EXTERN bool fakemap Q_ASSIGN( false );
Q_EXTERN bool nofog Q_ASSIGN( false );
Q_EXTERN bool noHint Q_ASSIGN( false );                        /* ydnar */
Q_EXTERN bool renameModelShaders Q_ASSIGN( false );            /* ydnar */
Q_EXTERN bool skyFixHack Q_ASSIGN( false );                    /* ydnar */
Q_EXTERN bool bspAlternateSplitWeights Q_ASSIGN( false );                      /* 27 */
Q_EXTERN bool deepBSP Q_ASSIGN( false );                   /* div0 */
Q_EXTERN bool maxAreaFaceSurface Q_ASSIGN( false );                    /* divVerent */
Q_EXTERN bool nocmdline Q_ASSIGN( false );

Q_EXTERN int patchSubdivisions Q_ASSIGN( 8 );                       /* ydnar: -patchmeta subdivisions */

Q_EXTERN int maxLMSurfaceVerts Q_ASSIGN( 64 );                      /* ydnar */
Q_EXTERN int maxSurfaceVerts Q_ASSIGN( 999 );                       /* ydnar */
Q_EXTERN int maxSurfaceIndexes Q_ASSIGN( 6000 );                    /* ydnar */
Q_EXTERN float npDegrees Q_ASSIGN( 0.0f );                          /* ydnar: nonplanar degrees */
Q_EXTERN int bevelSnap Q_ASSIGN( 0 );                               /* ydnar: bevel plane snap */
Q_EXTERN int texRange Q_ASSIGN( 0 );
Q_EXTERN bool flat Q_ASSIGN( false );
Q_EXTERN bool meta Q_ASSIGN( false );
Q_EXTERN bool patchMeta Q_ASSIGN( false );
Q_EXTERN bool emitFlares Q_ASSIGN( false );
Q_EXTERN bool debugSurfaces Q_ASSIGN( false );
Q_EXTERN bool debugInset Q_ASSIGN( false );
Q_EXTERN bool debugPortals Q_ASSIGN( false );
Q_EXTERN bool debugClip Q_ASSIGN( false );			/* debug model autoclipping */
Q_EXTERN float clipDepthGlobal Q_ASSIGN( 2.0f );
Q_EXTERN bool lightmapTriangleCheck Q_ASSIGN( false );
Q_EXTERN bool lightmapExtraVisClusterNudge Q_ASSIGN( false );
Q_EXTERN bool lightmapFill Q_ASSIGN( false );
Q_EXTERN bool lightmapPink Q_ASSIGN( false );
Q_EXTERN int metaAdequateScore Q_ASSIGN( -1 );
Q_EXTERN int metaGoodScore Q_ASSIGN( -1 );
Q_EXTERN float metaMaxBBoxDistance Q_ASSIGN( -1 );
Q_EXTERN bool noob Q_ASSIGN( false );

#if Q3MAP2_EXPERIMENTAL_SNAP_NORMAL_FIX
// Increasing the normalEpsilon to compensate for new logic in SnapNormal(), where
// this epsilon is now used to compare against 0 components instead of the 1 or -1
// components.  Unfortunately, normalEpsilon is also used in PlaneEqual().  So changing
// this will affect anything that calls PlaneEqual() as well (which are, at the time
// of this writing, FindFloatPlane() and AddBrushBevels()).
Q_EXTERN double normalEpsilon Q_ASSIGN( 0.00005 );
#else
Q_EXTERN double normalEpsilon Q_ASSIGN( 0.00001 );
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
Q_EXTERN double distanceEpsilon Q_ASSIGN( 0.005 );
#else
Q_EXTERN double distanceEpsilon Q_ASSIGN( 0.01 );
#endif


/* bsp */
Q_EXTERN int numMapEntities Q_ASSIGN( 0 );

Q_EXTERN int blockSize[ 3 ]                                 /* should be the same as in radiant */
#ifndef MAIN_C
;
#else
	= { 1024, 1024, 1024 };
#endif

Q_EXTERN char EnginePath[ 1024 ];

Q_EXTERN char name[ 1024 ];
Q_EXTERN char source[ 1024 ];

Q_EXTERN int sampleSize Q_ASSIGN( DEFAULT_LIGHTMAP_SAMPLE_SIZE );          /* lightmap sample size in units */
Q_EXTERN int minSampleSize Q_ASSIGN( DEFAULT_LIGHTMAP_MIN_SAMPLE_SIZE );   /* minimum sample size to use at all */
Q_EXTERN int sampleScale;                                                  /* vortex: lightmap sample scale (ie quality)*/

Q_EXTERN std::size_t mapEntityNum Q_ASSIGN( 0 );

Q_EXTERN int entitySourceBrushes;

Q_EXTERN plane_t            *mapplanes Q_ASSIGN( NULL );  /* mapplanes[ num ^ 1 ] will always be the mirror or mapplanes[ num ] */
Q_EXTERN int nummapplanes Q_ASSIGN( 0 );                    /* nummapplanes will always be even */
Q_EXTERN int allocatedmapplanes Q_ASSIGN( 0 );
Q_EXTERN int numMapPatches;
Q_EXTERN MinMax g_mapMinmax;

inline const MinMax c_worldMinmax( Vector3( MIN_WORLD_COORD ), Vector3( MAX_WORLD_COORD ) );

Q_EXTERN int defaultFogNum Q_ASSIGN( -1 );                  /* ydnar: cleaner fog handling */
Q_EXTERN int numMapFogs Q_ASSIGN( 0 );
Q_EXTERN fog_t mapFogs[ MAX_MAP_FOGS ];

Q_EXTERN entity_t           *mapEnt;
Q_EXTERN brush_t            *buildBrush;
Q_EXTERN EBrushType g_brushType Q_ASSIGN( EBrushType::Undefined );

Q_EXTERN int numStrippedLights Q_ASSIGN( 0 );


/* surface stuff */
Q_EXTERN mapDrawSurface_t   *mapDrawSurfs Q_ASSIGN( NULL );
Q_EXTERN int numMapDrawSurfs;

Q_EXTERN int numSurfacesByType[ static_cast<std::size_t>( ESurfaceType::Shader ) + 1 ];
Q_EXTERN int numStripSurfaces;
Q_EXTERN int numMaxAreaSurfaces;
Q_EXTERN int numFanSurfaces;
Q_EXTERN int numMergedSurfaces;
Q_EXTERN int numMergedVerts;

Q_EXTERN int numRedundantIndexes;

Q_EXTERN int numSurfaceModels Q_ASSIGN( 0 );

Q_EXTERN Vector3b debugColors[ 12 ]
#ifndef MAIN_C
;
#else
	=
	{
	{ 255, 0, 0 },
	{ 192, 128, 128 },
	{ 255, 255, 0 },
	{ 192, 192, 128 },
	{ 0, 255, 255 },
	{ 128, 192, 192 },
	{ 0, 0, 255 },
	{ 128, 128, 192 },
	{ 255, 0, 255 },
	{ 192, 128, 192 },
	{ 0, 255, 0 },
	{ 128, 192, 128 }
	};
#endif

Q_EXTERN int skyboxArea Q_ASSIGN( -1 );
Q_EXTERN Matrix4 skyboxTransform;



/* -------------------------------------------------------------------------------

   vis global variables

   ------------------------------------------------------------------------------- */

/* commandline arguments */
Q_EXTERN bool fastvis;
Q_EXTERN bool noPassageVis;
Q_EXTERN bool passageVisOnly;
Q_EXTERN bool mergevis;
Q_EXTERN bool mergevisportals;
Q_EXTERN bool nosort;
Q_EXTERN bool saveprt;
Q_EXTERN bool hint;             /* ydnar */
Q_EXTERN String64 globalCelShader;

Q_EXTERN float farPlaneDist Q_ASSIGN( 0.0f );                /* rr2do2, rf, mre, ydnar all contributed to this one... */
Q_EXTERN int farPlaneDistMode Q_ASSIGN( 0 );

Q_EXTERN int numportals;
Q_EXTERN int portalclusters;

Q_EXTERN vportal_t          *portals;
Q_EXTERN leaf_t             *leafs;

Q_EXTERN vportal_t          *faces;
Q_EXTERN leaf_t             *faceleafs;

Q_EXTERN int numfaces;

Q_EXTERN int leafbytes;
Q_EXTERN int portalbytes, portallongs;

Q_EXTERN vportal_t          *sorted_portals[ MAX_MAP_PORTALS * 2 ];



/* -------------------------------------------------------------------------------

   light global variables

   ------------------------------------------------------------------------------- */

/* commandline arguments */
Q_EXTERN bool wolfLight Q_ASSIGN( false );
Q_EXTERN float extraDist Q_ASSIGN( 0.0f );
Q_EXTERN bool loMem Q_ASSIGN( false );
Q_EXTERN bool noStyles Q_ASSIGN( false );
Q_EXTERN bool keepLights Q_ASSIGN( false );

//Q_EXTERN int sampleSize Q_ASSIGN( DEFAULT_LIGHTMAP_SAMPLE_SIZE );
//Q_EXTERN int minSampleSize Q_ASSIGN( DEFAULT_LIGHTMAP_MIN_SAMPLE_SIZE );
Q_EXTERN float noVertexLighting Q_ASSIGN( 0.0f );
Q_EXTERN bool nolm Q_ASSIGN( false );
Q_EXTERN bool noGridLighting Q_ASSIGN( false );

Q_EXTERN bool noTrace Q_ASSIGN( false );
Q_EXTERN bool noSurfaces Q_ASSIGN( false );
Q_EXTERN bool patchShadows Q_ASSIGN( false );
Q_EXTERN bool cpmaHack Q_ASSIGN( false );

Q_EXTERN bool deluxemap Q_ASSIGN( false );
Q_EXTERN bool debugDeluxemap Q_ASSIGN( false );
Q_EXTERN int deluxemode Q_ASSIGN( 0 );                  /* deluxemap format (0 - modelspace, 1 - tangentspace with renormalization, 2 - tangentspace without renormalization) */

Q_EXTERN bool fast Q_ASSIGN( false );
Q_EXTERN bool fastpoint Q_ASSIGN( true );
Q_EXTERN bool faster Q_ASSIGN( false );
Q_EXTERN bool fastgrid Q_ASSIGN( false );
Q_EXTERN bool fastbounce Q_ASSIGN( false );
Q_EXTERN bool cheap Q_ASSIGN( false );
Q_EXTERN bool cheapgrid Q_ASSIGN( false );
Q_EXTERN int bounce Q_ASSIGN( 0 );
Q_EXTERN bool bounceOnly Q_ASSIGN( false );
Q_EXTERN bool bouncing Q_ASSIGN( false );
Q_EXTERN bool bouncegrid Q_ASSIGN( false );
Q_EXTERN bool normalmap Q_ASSIGN( false );
Q_EXTERN bool trisoup Q_ASSIGN( false );
Q_EXTERN bool shade Q_ASSIGN( false );
Q_EXTERN float shadeAngleDegrees Q_ASSIGN( 0.0f );
Q_EXTERN int superSample Q_ASSIGN( 0 );
Q_EXTERN int lightSamples Q_ASSIGN( 1 );
Q_EXTERN bool lightRandomSamples Q_ASSIGN( false );
Q_EXTERN int lightSamplesSearchBoxSize Q_ASSIGN( 1 );
Q_EXTERN bool filter Q_ASSIGN( false );
Q_EXTERN bool dark Q_ASSIGN( false );
Q_EXTERN bool sunOnly Q_ASSIGN( false );
Q_EXTERN int approximateTolerance Q_ASSIGN( 0 );
Q_EXTERN bool noCollapse Q_ASSIGN( false );
Q_EXTERN int lightmapSearchBlockSize Q_ASSIGN( 0 );
Q_EXTERN bool exportLightmaps Q_ASSIGN( false );
Q_EXTERN bool externalLightmaps Q_ASSIGN( false );
Q_EXTERN int lmCustomSizeW Q_ASSIGN( LIGHTMAP_WIDTH );
Q_EXTERN int lmCustomSizeH Q_ASSIGN( LIGHTMAP_WIDTH );
Q_EXTERN char *             lmCustomDir Q_ASSIGN( NULL );
Q_EXTERN int lmLimitSize Q_ASSIGN( 0 );

Q_EXTERN bool dirty Q_ASSIGN( false );
Q_EXTERN bool dirtDebug Q_ASSIGN( false );
Q_EXTERN int dirtMode Q_ASSIGN( 0 );
Q_EXTERN float dirtDepth Q_ASSIGN( 128.0f );
Q_EXTERN float dirtScale Q_ASSIGN( 1.0f );
Q_EXTERN float dirtGain Q_ASSIGN( 1.0f );

/* 27: floodlighting */
Q_EXTERN bool debugnormals Q_ASSIGN( false );
Q_EXTERN bool floodlighty Q_ASSIGN( false );
Q_EXTERN bool floodlight_lowquality Q_ASSIGN( false );
Q_EXTERN Vector3 floodlightRGB;
Q_EXTERN float floodlightIntensity Q_ASSIGN( 512.0f );
Q_EXTERN float floodlightDistance Q_ASSIGN( 1024.0f );
Q_EXTERN float floodlightDirectionScale Q_ASSIGN( 1.0f );

Q_EXTERN bool dump Q_ASSIGN( false );
Q_EXTERN bool debug Q_ASSIGN( false );
Q_EXTERN bool debugAxis Q_ASSIGN( false );
Q_EXTERN bool debugCluster Q_ASSIGN( false );
Q_EXTERN bool debugOrigin Q_ASSIGN( false );
Q_EXTERN bool lightmapBorder Q_ASSIGN( false );
//1=warn; 0=warn if lmsize>128
Q_EXTERN int debugSampleSize Q_ASSIGN( 0 );

/* for run time tweaking of light sources */
Q_EXTERN float pointScale Q_ASSIGN( 7500.0f );
Q_EXTERN float spotScale Q_ASSIGN( 7500.0f );
Q_EXTERN float areaScale Q_ASSIGN( 0.25f );
Q_EXTERN float skyScale Q_ASSIGN( 1.0f );
Q_EXTERN float bounceScale Q_ASSIGN( 0.25f );
Q_EXTERN float bounceColorRatio Q_ASSIGN( 1.0f );
Q_EXTERN float vertexglobalscale Q_ASSIGN( 1.0f );
Q_EXTERN float g_backsplashFractionScale Q_ASSIGN( 1.0f );
Q_EXTERN float g_backsplashDistance Q_ASSIGN( -999.0f );

/* jal: alternative angle attenuation curve */
Q_EXTERN bool lightAngleHL Q_ASSIGN( false );

/* vortex: gridscale and gridambientscale */
Q_EXTERN float gridScale Q_ASSIGN( 1.0f );
Q_EXTERN float gridAmbientScale Q_ASSIGN( 1.0f );
Q_EXTERN float gridDirectionality Q_ASSIGN( 1.0f );
Q_EXTERN float gridAmbientDirectionality Q_ASSIGN( 0.0f );
Q_EXTERN bool inGrid Q_ASSIGN( false );

/* ydnar: lightmap gamma/compensation */
Q_EXTERN float lightmapGamma Q_ASSIGN( 1.0f );
Q_EXTERN float lightmapsRGB Q_ASSIGN( 0.0f );
Q_EXTERN float texturesRGB Q_ASSIGN( 0.0f );
Q_EXTERN float colorsRGB Q_ASSIGN( 0.0f );
Q_EXTERN float lightmapExposure Q_ASSIGN( 0.0f );
Q_EXTERN float lightmapCompensate Q_ASSIGN( 1.0f );
Q_EXTERN float lightmapBrightness Q_ASSIGN( 1.0f );
Q_EXTERN float lightmapContrast Q_ASSIGN( 1.0f );
inline float g_lightmapSaturation = 1;

/* ydnar: for runtime tweaking of falloff tolerance */
Q_EXTERN float falloffTolerance Q_ASSIGN( 1.0f );
Q_EXTERN bool exactPointToPolygon Q_ASSIGN( true );
Q_EXTERN float formFactorValueScale Q_ASSIGN( 3.0f );
Q_EXTERN float linearScale Q_ASSIGN( 1.0f / 8000.0f );

// for .ase conversion
Q_EXTERN bool shadersAsBitmap Q_ASSIGN( false );
Q_EXTERN bool lightmapsAsTexcoord Q_ASSIGN( false );

Q_EXTERN light_t            *lights;
Q_EXTERN int numPointLights;
Q_EXTERN int numSpotLights;
Q_EXTERN int numSunLights;

/* ydnar: for luxel placement */
Q_EXTERN int numSurfaceClusters, maxSurfaceClusters;
Q_EXTERN int                *surfaceClusters;

/* ydnar: for radiosity */
Q_EXTERN int numDiffuseLights;
Q_EXTERN int numBrushDiffuseLights;
Q_EXTERN int numTriangleDiffuseLights;
Q_EXTERN int numPatchDiffuseLights;

/* ydnar: general purpose extra copy of drawvert list */
Q_EXTERN bspDrawVert_t      *yDrawVerts;

Q_EXTERN int defaultLightSubdivide Q_ASSIGN( 999 );

Q_EXTERN Vector3 ambientColor;
Q_EXTERN Vector3 minLight, minVertexLight, minGridLight;
Q_EXTERN float maxLight Q_ASSIGN( 255.f );

/* ydnar: light optimization */
Q_EXTERN float subdivideThreshold Q_ASSIGN( DEFAULT_SUBDIVIDE_THRESHOLD );

Q_EXTERN int numOpaqueBrushes, maxOpaqueBrush;
Q_EXTERN byte               *opaqueBrushes;

Q_EXTERN int numLights;
Q_EXTERN int numCulledLights;

Q_EXTERN int gridBoundsCulled;
Q_EXTERN int gridEnvelopeCulled;

Q_EXTERN int lightsBoundsCulled;
Q_EXTERN int lightsEnvelopeCulled;
Q_EXTERN int lightsPlaneCulled;
Q_EXTERN int lightsClusterCulled;

/* ydnar: radiosity */
Q_EXTERN float diffuseSubdivide Q_ASSIGN( 256.0f );
Q_EXTERN float minDiffuseSubdivide Q_ASSIGN( 64.0f );
Q_EXTERN int numDiffuseSurfaces Q_ASSIGN( 0 );

/* ydnar: list of surface information necessary for lightmap calculation */
Q_EXTERN surfaceInfo_t      *surfaceInfos Q_ASSIGN( NULL );

/* ydnar: sorted list of surfaces */
Q_EXTERN int                *sortSurfaces Q_ASSIGN( NULL );

/* clumps of surfaces that share a raw lightmap */
Q_EXTERN int numLightSurfaces Q_ASSIGN( 0 );
Q_EXTERN int                *lightSurfaces Q_ASSIGN( NULL );

/* raw lightmaps */
Q_EXTERN int numRawLightmaps Q_ASSIGN( 0 );
Q_EXTERN rawLightmap_t      *rawLightmaps Q_ASSIGN( NULL );
Q_EXTERN int                *sortLightmaps Q_ASSIGN( NULL );

/* vertex luxels */
Q_EXTERN Vector3            *vertexLuxels[ MAX_LIGHTMAPS ];
Q_EXTERN Vector3            *radVertexLuxels[ MAX_LIGHTMAPS ];

inline Vector3& getVertexLuxel( int lightmapNum, int vertexNum ){
	return vertexLuxels[lightmapNum][vertexNum];
}
inline Vector3& getRadVertexLuxel( int lightmapNum, int vertexNum ){
	return radVertexLuxels[lightmapNum][vertexNum];
}

/* bsp lightmaps */
Q_EXTERN int numLightmapShaders Q_ASSIGN( 0 );
Q_EXTERN int numSolidLightmaps Q_ASSIGN( 0 );
Q_EXTERN int numOutLightmaps Q_ASSIGN( 0 );
Q_EXTERN int numBSPLightmaps Q_ASSIGN( 0 );
Q_EXTERN int numExtLightmaps Q_ASSIGN( 0 );
Q_EXTERN outLightmap_t      *outLightmaps Q_ASSIGN( NULL );

/* vortex: per surface floodlight statictics */
Q_EXTERN int numSurfacesFloodlighten Q_ASSIGN( 0 );

/* grid points */
Q_EXTERN int numRawGridPoints Q_ASSIGN( 0 );
Q_EXTERN rawGridPoint_t     *rawGridPoints Q_ASSIGN( NULL );

Q_EXTERN int numSurfsVertexLit Q_ASSIGN( 0 );
Q_EXTERN int numSurfsVertexForced Q_ASSIGN( 0 );
Q_EXTERN int numSurfsVertexApproximated Q_ASSIGN( 0 );
Q_EXTERN int numSurfsLightmapped Q_ASSIGN( 0 );
Q_EXTERN int numPlanarsLightmapped Q_ASSIGN( 0 );
Q_EXTERN int numNonPlanarsLightmapped Q_ASSIGN( 0 );
Q_EXTERN int numPatchesLightmapped Q_ASSIGN( 0 );
Q_EXTERN int numPlanarPatchesLightmapped Q_ASSIGN( 0 );

Q_EXTERN int numLuxels Q_ASSIGN( 0 );
Q_EXTERN int numLuxelsMapped Q_ASSIGN( 0 );
Q_EXTERN int numLuxelsOccluded Q_ASSIGN( 0 );
Q_EXTERN int numLuxelsIlluminated Q_ASSIGN( 0 );
Q_EXTERN int numVertsIlluminated Q_ASSIGN( 0 );

/* lightgrid */
Q_EXTERN Vector3 gridMins;
Q_EXTERN int gridBounds[ 3 ];
Q_EXTERN Vector3 gridSize
#ifndef MAIN_C
;
#else
	= { 64, 64, 128 };
#endif



/* -------------------------------------------------------------------------------

   abstracted bsp globals

   ------------------------------------------------------------------------------- */

Q_EXTERN std::size_t numBSPEntities Q_ASSIGN( 0 );
Q_EXTERN std::vector<entity_t> entities;

Q_EXTERN int numBSPModels Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPModels Q_ASSIGN( 0 );
Q_EXTERN bspModel_t*        bspModels Q_ASSIGN( NULL );

Q_EXTERN int numBSPShaders Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPShaders Q_ASSIGN( 0 );
Q_EXTERN bspShader_t*       bspShaders Q_ASSIGN( 0 );

Q_EXTERN int bspEntDataSize Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPEntData Q_ASSIGN( 0 );
Q_EXTERN char               *bspEntData Q_ASSIGN( 0 );

Q_EXTERN int numBSPLeafs Q_ASSIGN( 0 );
Q_EXTERN bspLeaf_t bspLeafs[ MAX_MAP_LEAFS ];

Q_EXTERN int numBSPPlanes Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPPlanes Q_ASSIGN( 0 );
Q_EXTERN bspPlane_t         *bspPlanes;

Q_EXTERN int numBSPNodes Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPNodes Q_ASSIGN( 0 );
Q_EXTERN bspNode_t*         bspNodes Q_ASSIGN( NULL );

Q_EXTERN int numBSPLeafSurfaces Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPLeafSurfaces Q_ASSIGN( 0 );
Q_EXTERN int*               bspLeafSurfaces Q_ASSIGN( NULL );

Q_EXTERN int numBSPLeafBrushes Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPLeafBrushes Q_ASSIGN( 0 );
Q_EXTERN int*               bspLeafBrushes Q_ASSIGN( NULL );

Q_EXTERN int numBSPBrushes Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPBrushes Q_ASSIGN( 0 );
Q_EXTERN bspBrush_t*        bspBrushes Q_ASSIGN( NULL );

Q_EXTERN int numBSPBrushSides Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPBrushSides Q_ASSIGN( 0 );
Q_EXTERN bspBrushSide_t*    bspBrushSides Q_ASSIGN( NULL );

Q_EXTERN int numBSPLightBytes Q_ASSIGN( 0 );
Q_EXTERN byte               *bspLightBytes Q_ASSIGN( NULL );

Q_EXTERN int numBSPGridPoints Q_ASSIGN( 0 );
Q_EXTERN bspGridPoint_t     *bspGridPoints Q_ASSIGN( NULL );

Q_EXTERN int numBSPVisBytes Q_ASSIGN( 0 );
Q_EXTERN byte bspVisBytes[ MAX_MAP_VISIBILITY ];

Q_EXTERN int numBSPDrawVerts Q_ASSIGN( 0 );
Q_EXTERN bspDrawVert_t          *bspDrawVerts Q_ASSIGN( NULL );

Q_EXTERN int numBSPDrawIndexes Q_ASSIGN( 0 );
Q_EXTERN int allocatedBSPDrawIndexes Q_ASSIGN( 0 );
Q_EXTERN int                *bspDrawIndexes Q_ASSIGN( NULL );

Q_EXTERN int numBSPDrawSurfaces Q_ASSIGN( 0 );
Q_EXTERN bspDrawSurface_t   *bspDrawSurfaces Q_ASSIGN( NULL );

Q_EXTERN int numBSPFogs Q_ASSIGN( 0 );
Q_EXTERN bspFog_t bspFogs[ MAX_MAP_FOGS ];

Q_EXTERN int numBSPAds Q_ASSIGN( 0 );
Q_EXTERN bspAdvertisement_t bspAds[ MAX_MAP_ADVERTISEMENTS ];

#define AUTOEXPAND_BY_REALLOC( ptr, reqitem, allocated, def ) \
	do \
	{ \
		if ( reqitem >= allocated )	\
		{ \
			if ( allocated == 0 ) {	\
				allocated = def; } \
			while ( reqitem >= allocated && allocated )	\
				allocated *= 2;	\
			if ( !allocated || allocated > 2147483647 / (int)sizeof( *ptr ) ) \
			{ \
				Error( # ptr " over 2 GB" ); \
			} \
			ptr = void_ptr( realloc( ptr, sizeof( *ptr ) * allocated ) ); \
			if ( !ptr ) { \
				Error( # ptr " out of memory" ); } \
		} \
	} \
	while ( 0 )

#define AUTOEXPAND_BY_REALLOC_BSP( suffix, def ) AUTOEXPAND_BY_REALLOC( bsp ## suffix, numBSP ## suffix, allocatedBSP ## suffix, def )

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

/* end marker */
#endif

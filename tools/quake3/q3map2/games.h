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
*/



/* marker */
#pragma once

#include <vector>


/* ydnar: compiler flags, because games have widely varying content/surface flags */
const int C_SOLID                = 0x00000001;
const int C_TRANSLUCENT          = 0x00000002;
const int C_STRUCTURAL           = 0x00000004;
const int C_HINT                 = 0x00000008;
const int C_NODRAW               = 0x00000010;
const int C_LIGHTGRID            = 0x00000020;
const int C_ALPHASHADOW          = 0x00000040;
const int C_LIGHTFILTER          = 0x00000080;
const int C_VERTEXLIT            = 0x00000100;
const int C_LIQUID               = 0x00000200;
const int C_FOG                  = 0x00000400;
const int C_SKY                  = 0x00000800;
const int C_ORIGIN               = 0x00001000;
const int C_AREAPORTAL           = 0x00002000;
const int C_ANTIPORTAL           = 0x00004000;  /* like hint, but doesn't generate portals */
const int C_SKIP                 = 0x00008000;  /* like hint, but skips this face (doesn't split bsp) */
const int C_NOMARKS              = 0x00010000;  /* no decals */
const int C_OB                   = 0x00020000;  /* skip -noob for this */
const int C_DETAIL               = 0x08000000;  /* THIS MUST BE THE SAME AS IN RADIANT! */


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
	int maxLMSurfaceVerts;                              /* default maximum lightmapped surface verts */
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
	int patchSubdivisions;                              /* default patchMeta subdivisions tolerance */
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
	typedef void ( *bspFunc )( const char * );
	bspFunc load, write;                                /* load/write function pointers */
	std::vector<surfaceParm_t> surfaceParms;            /* surfaceparm array */
	int brushBevelsSurfaceFlagsMask;                    /* apply only these surfaceflags to bevels to reduce extra bsp shaders amount; applying them to get correct physics at walkable brush edges and vertices */
};

extern const std::vector<game_t> g_games;
extern const game_t *g_game;

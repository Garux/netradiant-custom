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

#include "games.h"
#include "bspfile_ibsp.h"
#include "bspfile_rbsp.h"
#include "qstringops.h"
#include "inout.h"

struct game_default : game_t
{
	/* game flags */
	static const int Q_CONT_SOLID               = 1;           /* an eye is never valid in a solid */
	static const int Q_CONT_LAVA                = 8;
	static const int Q_CONT_SLIME               = 16;
	static const int Q_CONT_WATER               = 32;
	static const int Q_CONT_FOG                 = 64;

	static const int Q_CONT_AREAPORTAL          = 0x8000;

	static const int Q_CONT_PLAYERCLIP          = 0x10000;
	static const int Q_CONT_MONSTERCLIP         = 0x20000;
	static const int Q_CONT_TELEPORTER          = 0x40000;
	static const int Q_CONT_JUMPPAD             = 0x80000;
	static const int Q_CONT_CLUSTERPORTAL       = 0x100000;
	static const int Q_CONT_DONOTENTER          = 0x200000;
	static const int Q_CONT_BOTCLIP             = 0x400000;

	static const int Q_CONT_ORIGIN              = 0x1000000;   /* removed before bsping an entity */

	static const int Q_CONT_BODY                = 0x2000000;   /* should never be on a brush, only in game */
	static const int Q_CONT_CORPSE              = 0x4000000;
	static const int Q_CONT_DETAIL              = 0x8000000;   /* brushes not used for the bsp */
	static const int Q_CONT_STRUCTURAL          = 0x10000000;  /* brushes used for the bsp */
	static const int Q_CONT_TRANSLUCENT         = 0x20000000;  /* don't consume surface fragments inside */
	static const int Q_CONT_TRIGGER             = 0x40000000;
	static const int Q_CONT_NODROP              = 0x80000000;  /* don't leave bodies or items (death fog, lava) */

	static const int Q_SURF_NODAMAGE            = 0x1;         /* never give falling damage */
	static const int Q_SURF_SLICK               = 0x2;         /* effects game physics: zero friction on this */
	static const int Q_SURF_SKY                 = 0x4;         /* lighting from environment map */
	static const int Q_SURF_LADDER              = 0x8;
	static const int Q_SURF_NOIMPACT            = 0x10;        /* don't make missile explosions */
	static const int Q_SURF_NOMARKS             = 0x20;        /* don't leave missile marks */
	static const int Q_SURF_FLESH               = 0x40;        /* make flesh sounds and effects */
	static const int Q_SURF_NODRAW              = 0x80;        /* don't generate a drawsurface at all */
	static const int Q_SURF_HINT                = 0x100;       /* make a primary bsp splitter */
	static const int Q_SURF_SKIP                = 0x200;       /* completely ignore, allowing non-closed brushes */
	static const int Q_SURF_NOLIGHTMAP          = 0x400;       /* surface doesn't need a lightmap */
	static const int Q_SURF_POINTLIGHT          = 0x800;       /* generate lighting info at vertexes */
	static const int Q_SURF_METALSTEPS          = 0x1000;      /* clanking footsteps */
	static const int Q_SURF_NOSTEPS             = 0x2000;      /* no footstep sounds */
	static const int Q_SURF_NONSOLID            = 0x4000;      /* don't collide against curves with this set */
	static const int Q_SURF_LIGHTFILTER         = 0x8000;      /* act as a light filter during q3map -light */
	static const int Q_SURF_ALPHASHADOW         = 0x10000;     /* do per-pixel light shadow casting in q3map */
	static const int Q_SURF_NODLIGHT            = 0x20000;     /* don't dlight even if solid (solid lava, skies) */
	static const int Q_SURF_DUST                = 0x40000;     /* leave a dust trail when walking on this surface */

	/* ydnar flags */
	static const int Q_SURF_VERTEXLIT           = ( Q_SURF_POINTLIGHT | Q_SURF_NOLIGHTMAP );
	static const int Q_SURF_BEVELSMASK          = ( Q_SURF_NODAMAGE | Q_SURF_SLICK | Q_SURF_FLESH | Q_SURF_METALSTEPS | Q_SURF_NOSTEPS | Q_SURF_DUST );

	game_default() : game_t{
	"quake3",           /* -game x */
	"baseq3",           /* default base game data dir */
	".q3a",             /* unix home sub-dir */
	"quake",            /* magic path word */
	"scripts",          /* shader directory */
	64,                 /* max lightmapped surface verts */
	999,                /* max surface verts */
	6000,               /* max surface indexes */
	false,              /* flares */
	"flareshader",      /* default flare shader */
	false,              /* wolf lighting model? */
	128,                /* lightmap width/height */
	1.0f,               /* lightmap gamma */
	false,              /* lightmap sRGB */
	false,              /* texture sRGB */
	false,              /* color sRGB */
	0.0f,               /* lightmap exposure */
	1.0f,               /* lightmap compensate */
	1.0f,               /* lightgrid scale */
	1.0f,               /* lightgrid ambient scale */
	false,              /* light angle attenuation uses half-lambert curve */
	false,              /* disable shader lightstyles hack */
	false,              /* keep light entities on bsp */
	8,                  /* default patchMeta subdivisions tolerance */
	false,              /* patch casting enabled */
	false,              /* compile deluxemaps */
	0,                  /* deluxemaps default mode */
	512,                /* minimap size */
	1.0f,               /* minimap sharpener */
	0.0f,               /* minimap border */
	true,               /* minimap keep aspect */
	EMiniMapMode::Gray, /* minimap mode */
	"%s.tga",           /* minimap name format */
	"IBSP",             /* bsp file prefix */
	46,                 /* bsp file version */
	false,              /* cod-style lump len/ofs order */
	LoadIBSPFile,       /* bsp load function */
	WriteIBSPFile,      /* bsp write function */

	{
		/* name             contentFlags                contentFlagsClear           surfaceFlags                surfaceFlagsClear           compileFlags                compileFlagsClear */

		/* default */
		{ "default",        Q_CONT_SOLID,               -1,                         0,                          -1,                         C_SOLID,                    -1 },


		/* ydnar */
		{ "lightgrid",      0,                          0,                          0,                          0,                          C_LIGHTGRID,                0 },
		{ "antiportal",     0,                          0,                          0,                          0,                          C_ANTIPORTAL,               0 },
		{ "skip",           0,                          0,                          0,                          0,                          C_SKIP,                     0 },


		/* compiler */
		{ "origin",         Q_CONT_ORIGIN,              Q_CONT_SOLID,               0,                          0,                          C_ORIGIN | C_TRANSLUCENT,   C_SOLID },
		{ "areaportal",     Q_CONT_AREAPORTAL,          Q_CONT_SOLID,               0,                          0,                          C_AREAPORTAL | C_TRANSLUCENT,   C_SOLID },
		{ "trans",          Q_CONT_TRANSLUCENT,         0,                          0,                          0,                          C_TRANSLUCENT,              0 },
		{ "detail",         Q_CONT_DETAIL,              0,                          0,                          0,                          C_DETAIL,                   0 },
		{ "structural",     Q_CONT_STRUCTURAL,          0,                          0,                          0,                          C_STRUCTURAL,               0 },
		{ "hint",           0,                          0,                          Q_SURF_HINT,                0,                          C_HINT,                     0 },
		{ "nodraw",         0,                          0,                          Q_SURF_NODRAW,              0,                          C_NODRAW,                   0 },

		{ "alphashadow",    0,                          0,                          Q_SURF_ALPHASHADOW,         0,                          C_ALPHASHADOW | C_TRANSLUCENT,  0 },
		{ "lightfilter",    0,                          0,                          Q_SURF_LIGHTFILTER,         0,                          C_LIGHTFILTER | C_TRANSLUCENT,  0 },
		{ "nolightmap",     0,                          0,                          Q_SURF_VERTEXLIT,           0,                          C_VERTEXLIT,                0 },
		{ "pointlight",     0,                          0,                          Q_SURF_VERTEXLIT,           0,                          C_VERTEXLIT,                0 },


		/* game */
		{ "nonsolid",       0,                          Q_CONT_SOLID,               Q_SURF_NONSOLID,            0,                          0,                          C_SOLID },

		{ "trigger",        Q_CONT_TRIGGER,             Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

		{ "water",          Q_CONT_WATER,               Q_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },
		{ "slime",          Q_CONT_SLIME,               Q_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },
		{ "lava",           Q_CONT_LAVA,                Q_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },

		{ "playerclip",     Q_CONT_PLAYERCLIP,          Q_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "monsterclip",    Q_CONT_MONSTERCLIP,         Q_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "nodrop",         Q_CONT_NODROP,              Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

		{ "clusterportal",  Q_CONT_CLUSTERPORTAL,       Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },
		{ "donotenter",     Q_CONT_DONOTENTER,          Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },
		{ "botclip",        Q_CONT_BOTCLIP,             Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

		{ "fog",            Q_CONT_FOG,                 Q_CONT_SOLID,               0,                          0,                          C_FOG,                      C_SOLID },
		{ "sky",            0,                          0,                          Q_SURF_SKY,                 0,                          C_SKY,                      0 },

		{ "slick",          0,                          0,                          Q_SURF_SLICK,               0,                          0,                          0 },

		{ "noimpact",       0,                          0,                          Q_SURF_NOIMPACT,            0,                          0,                          0 },
		{ "nomarks",        0,                          0,                          Q_SURF_NOMARKS,             0,                          C_NOMARKS,                  0 },
		{ "ladder",         0,                          0,                          Q_SURF_LADDER,              0,                          0,                          0 },
		{ "nodamage",       0,                          0,                          Q_SURF_NODAMAGE,            0,                          0,                          0 },
		{ "metalsteps",     0,                          0,                          Q_SURF_METALSTEPS,          0,                          0,                          0 },
		{ "flesh",          0,                          0,                          Q_SURF_FLESH,               0,                          0,                          0 },
		{ "nosteps",        0,                          0,                          Q_SURF_NOSTEPS,             0,                          0,                          0 },
		{ "nodlight",       0,                          0,                          Q_SURF_NODLIGHT,            0,                          0,                          0 },
		{ "dust",           0,                          0,                          Q_SURF_DUST,                0,                          0,                          0 },
	},

	Q_SURF_BEVELSMASK
	}{}
};

struct game_quake3 : game_default
{
	static const int Q_SURF_NOOB                = 0x80000;     /* no overbounces on this surface in Q3A:Defrag mod */

	game_quake3(){
		surfaceParms.insert( surfaceParms.end(), {
		{ "noob",           0,                          0,                          Q_SURF_NOOB,                0,                          0,                          0 },
		{ "ob",             0,                          0,                          0,                          0,                          C_OB,                       0 },
		} );
		brushBevelsSurfaceFlagsMask |= Q_SURF_NOOB;
	}
};

struct game_quakelive : game_default
{
	// Additional surface flags for Quake Live

	static const int Q_SURF_SNOWSTEPS 	= 0x80000;  // snow footsteps
	static const int Q_SURF_WOODSTEPS 	= 0x100000; // wood footsteps
	static const int Q_SURF_DMGTHROUGH 	= 0x200000; // Missile dmg through surface(?)
	                                                // (This is not in use atm, will
	                                                //  probably be re-purposed some day.)
	game_quakelive(){
		arg = "quakelive";
		homeBasePath = ".quakelive";
		bspVersion = 47;
		surfaceParms.insert( surfaceParms.end(), {
		{ "snowsteps",      0,                          0,                          Q_SURF_SNOWSTEPS,           0,                          0,                          0 },
		{ "woodsteps",      0,                          0,                          Q_SURF_WOODSTEPS,           0,                          0,                          0 },
		{ "dmgthrough",     0,                          0,                          Q_SURF_DMGTHROUGH,          0,                          0,                          0 },
		} );
		brushBevelsSurfaceFlagsMask |= ( Q_SURF_SNOWSTEPS | Q_SURF_WOODSTEPS );
	}
};

struct game_nexuiz : game_default
{
	game_nexuiz(){
		arg = "nexuiz";
		gamePath = "data";
		homeBasePath = ".nexuiz";
		magic = "nexuiz";
		maxLMSurfaceVerts = 999;
		noStyles = true;
		keepLights = true;
		miniMapBorder = 1.0f / 66.0f;
		miniMapNameFormat = "../gfx/%s_mini.tga";
	}
};

struct game_xonotic : game_default
{
	game_xonotic(){
		arg = "xonotic";
		gamePath = "data";
		homeBasePath = ".xonotic";
		magic = "xonotic";
		maxLMSurfaceVerts = 1048575;
		maxSurfaceVerts = 1048575;
		maxSurfaceIndexes = 1048575;
		lightmapsRGB = true;
		texturesRGB = true;
		colorsRGB = true;
		noStyles = true;
		keepLights = true;
		patchSubdivisions = 4;
		patchShadows = true;
		deluxeMap = true;
		miniMapBorder = 1.0f / 66.0f;
		miniMapNameFormat = "../gfx/%s_mini.tga";
	}
};

struct game_tremulous : game_default
{
	static const int TREM_CONT_NOALIENBUILD         = 0x1000;
	static const int TREM_CONT_NOHUMANBUILD         = 0x2000;
	static const int TREM_CONT_NOBUILD              = 0x4000;

	static const int TREM_SURF_NOALIENBUILDSURFACE  = 0x80000;
	static const int TREM_SURF_NOHUMANBUILDSURFACE  = 0x100000;
	static const int TREM_SURF_NOBUILDSURFACE       = 0x200000;

	game_tremulous(){
		arg = "tremulous";
		gamePath = "base";
		homeBasePath = ".tremulous";
		magic = "tremulous";
		surfaceParms.insert( surfaceParms.end(), {
		{ "noalienbuild",        TREM_CONT_NOALIENBUILD, 0, 0,                             0, 0, 0 },
		{ "nohumanbuild",        TREM_CONT_NOHUMANBUILD, 0, 0,                             0, 0, 0 },
		{ "nobuild",             TREM_CONT_NOBUILD,      0, 0,                             0, 0, 0 },

		{ "noalienbuildsurface", 0,                      0, TREM_SURF_NOALIENBUILDSURFACE, 0, 0, 0 },
		{ "nohumanbuildsurface", 0,                      0, TREM_SURF_NOHUMANBUILDSURFACE, 0, 0, 0 },
		{ "nobuildsurface",      0,                      0, TREM_SURF_NOBUILDSURFACE,      0, 0, 0 },
		} );
	}
};

struct game_unvanquished : game_tremulous
{
	game_unvanquished(){
		arg = "unvanquished";
		gamePath = "pkg";
		homeBasePath = ".local/share/unvanquished";
		magic = "unvanquished";
		maxLMSurfaceVerts = 1048575;
		maxSurfaceVerts = 1048575;
		maxSurfaceIndexes = 1048575;
		keepLights = true;
		miniMapMode = EMiniMapMode::White;
		miniMapNameFormat = "../minimaps/%s.tga";
	}
};

struct game_tenebrae : game_default
{
	game_tenebrae(){
		arg = "tenebrae";
		gamePath = "base";
		homeBasePath = ".tenebrae";
		magic = "tenebrae";
		maxLMSurfaceVerts = 1024;
		maxSurfaceVerts = 1024;
		maxSurfaceIndexes = 6144;
		lightmapSize = 512;
		lightmapGamma = 2.0f;
		noStyles = true;
		deluxeMap = true;
	}
};

struct game_wolf : game_default
{
	static const int W_CONT_MISSILECLIP          = 0x80;        /* wolf ranged missile blocking */
	static const int W_CONT_ITEM                 = 0x100;       /* wolf item contents */
	static const int W_CONT_AI_NOSIGHT           = 0x1000;      /* wolf ai sight blocking */
	static const int W_CONT_CLIPSHOT             = 0x2000;      /* wolf shot clip */

	static const int W_CONT_DONOTENTER_LARGE     = 0x400000;    /* wolf dne */ // == Q_CONT_BOTCLIP

	static const int W_SURF_CERAMIC              = 0x40;        /* wolf ceramic material */ // == Q_SURF_FLESH
	static const int W_SURF_METAL                = 0x1000;      /* wolf metal material */ // == Q_SURF_METALSTEPS
	static const int W_SURF_WOOD                 = 0x40000;     /* wolf wood material */ // == Q_SURF_DUST
	static const int W_SURF_GRASS                = 0x80000;     /* wolf grass material */
	static const int W_SURF_GRAVEL               = 0x100000;    /* wolf gravel material */
	static const int W_SURF_GLASS                = 0x200000;    /* wolf glass material */
	static const int W_SURF_SNOW                 = 0x400000;    /* wolf snow material */
	static const int W_SURF_ROOF                 = 0x800000;    /* wolf roof material */
	static const int W_SURF_RUBBLE               = 0x1000000;   /* wolf rubble material */
	static const int W_SURF_CARPET               = 0x2000000;   /* wolf carpet material */

	static const int W_SURF_MONSTERSLICK         = 0x4000000;   /* wolf npc slick surface */
	static const int W_SURF_MONSLICK_W           = 0x8000000;   /* wolf slide bodies west */
	static const int W_SURF_MONSLICK_N           = 0x10000000;  /* wolf slide bodies north */
	static const int W_SURF_MONSLICK_E           = 0x20000000;  /* wolf slide bodies east */
	static const int W_SURF_MONSLICK_S           = 0x40000000;  /* wolf slide bodies south */

	game_wolf(){
		arg = "wolf";
		gamePath = "main";
		homeBasePath = ".wolf";
		magic = "wolf";
		wolfLight = true;
		bspVersion = 47;
		surfaceParms.insert( surfaceParms.end(), {
			/* game */
			{ "slag",           Q_CONT_SLIME,               Q_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },

			{ "clipmissile",    W_CONT_MISSILECLIP,         Q_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
			{ "clipshot",       W_CONT_CLIPSHOT,            Q_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },

			{ "donotenterlarge",W_CONT_DONOTENTER_LARGE,    Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

			/* materials */
			{ "metal",          0,                          0,                          W_SURF_METAL,               0,                          0,                          0 },
			{ "metalsteps",     0,                          0,                          W_SURF_METAL,               0,                          0,                          0 },
			{ "glass",          0,                          0,                          W_SURF_GLASS,               0,                          0,                          0 },
			{ "ceramic",        0,                          0,                          W_SURF_CERAMIC,             0,                          0,                          0 },
			{ "woodsteps",      0,                          0,                          W_SURF_WOOD,                0,                          0,                          0 },
			{ "grasssteps",     0,                          0,                          W_SURF_GRASS,               0,                          0,                          0 },
			{ "gravelsteps",    0,                          0,                          W_SURF_GRAVEL,              0,                          0,                          0 },
			{ "rubble",         0,                          0,                          W_SURF_RUBBLE,              0,                          0,                          0 },
			{ "carpetsteps",    0,                          0,                          W_SURF_CARPET,              0,                          0,                          0 },
			{ "snowsteps",      0,                          0,                          W_SURF_SNOW,                0,                          0,                          0 },
			{ "roofsteps",      0,                          0,                          W_SURF_ROOF,                0,                          0,                          0 },


			/* ai */
			{ "ai_nosight",     W_CONT_AI_NOSIGHT,          Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

			/* ydnar: experimental until bits are confirmed! */
			{ "ai_nopass",      Q_CONT_DONOTENTER,          Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },
			{ "ai_nopasslarge", W_CONT_DONOTENTER_LARGE,    Q_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },


			/* sliding bodies */
			{ "monsterslick",   0,                          0,                          W_SURF_MONSTERSLICK,        0,                          C_TRANSLUCENT,              0 },
			{ "monsterslicknorth",  0,                      0,                          W_SURF_MONSLICK_N,          0,                          C_TRANSLUCENT,              0 },
			{ "monsterslickeast",   0,                      0,                          W_SURF_MONSLICK_E,          0,                          C_TRANSLUCENT,              0 },
			{ "monsterslicksouth",  0,                      0,                          W_SURF_MONSLICK_S,          0,                          C_TRANSLUCENT,              0 },
			{ "monsterslickwest",   0,                      0,                          W_SURF_MONSLICK_W,          0,                          C_TRANSLUCENT,              0 },

		} );
		brushBevelsSurfaceFlagsMask |= ( W_SURF_CERAMIC | W_SURF_METAL | W_SURF_WOOD | W_SURF_GRASS | W_SURF_GRAVEL | W_SURF_GLASS | W_SURF_SNOW | W_SURF_ROOF | W_SURF_RUBBLE | W_SURF_CARPET | W_SURF_MONSTERSLICK | W_SURF_MONSLICK_W | W_SURF_MONSLICK_N | W_SURF_MONSLICK_E | W_SURF_MONSLICK_S );
	}
};

struct game_wolfet : game_wolf
{
	static const int W_SURF_SPLASH              = 0x00000040;  /* enemy territory water splash surface */ // == W_SURF_CERAMIC
	static const int W_SURF_LANDMINE            = 0x80000000;  /* enemy territory 'landminable' surface */

	game_wolfet(){
		arg = "et";
		gamePath = "etmain";
		homeBasePath = ".etwolf";
		magic = "et";
		maxLMSurfaceVerts = 1024;
		maxSurfaceVerts = 1024;
		maxSurfaceIndexes = 6144;
		surfaceParms.insert( surfaceParms.end(), {
		{ "landmine",       0,                          0,                          W_SURF_LANDMINE,            0,                          0,                          0 },
		{ "splash",         0,                          0,                          W_SURF_SPLASH,              0,                          0,                          0 },
		} );
	}
};

struct game_etut : game_default
{
	/* materials */
	static const int U_MAT_MASK                 = 0xFFF00000;  /* mask to get the material type */

	static const int U_MAT_NONE                 = 0x00000000;
	static const int U_MAT_TIN                  = 0x00100000;
	static const int U_MAT_ALUMINUM             = 0x00200000;
	static const int U_MAT_IRON                 = 0x00300000;
	static const int U_MAT_TITANIUM             = 0x00400000;
	static const int U_MAT_STEEL                = 0x00500000;
	static const int U_MAT_BRASS                = 0x00600000;
	static const int U_MAT_COPPER               = 0x00700000;
	static const int U_MAT_CEMENT               = 0x00800000;
	static const int U_MAT_ROCK                 = 0x00900000;
	static const int U_MAT_GRAVEL               = 0x00A00000;
	static const int U_MAT_PAVEMENT             = 0x00B00000;
	static const int U_MAT_BRICK                = 0x00C00000;
	static const int U_MAT_CLAY                 = 0x00D00000;
	static const int U_MAT_GRASS                = 0x00E00000;
	static const int U_MAT_DIRT                 = 0x00F00000;
	static const int U_MAT_MUD                  = 0x01000000;
	static const int U_MAT_SNOW                 = 0x01100000;
	static const int U_MAT_ICE                  = 0x01200000;
	static const int U_MAT_SAND                 = 0x01300000;
	static const int U_MAT_CERAMICTILE          = 0x01400000;
	static const int U_MAT_LINOLEUM             = 0x01500000;
	static const int U_MAT_RUG                  = 0x01600000;
	static const int U_MAT_PLASTER              = 0x01700000;
	static const int U_MAT_PLASTIC              = 0x01800000;
	static const int U_MAT_CARDBOARD            = 0x01900000;
	static const int U_MAT_HARDWOOD             = 0x01A00000;
	static const int U_MAT_SOFTWOOD             = 0x01B00000;
	static const int U_MAT_PLANK                = 0x01C00000;
	static const int U_MAT_GLASS                = 0x01D00000;
	static const int U_MAT_WATER                = 0x01E00000;
	static const int U_MAT_STUCCO               = 0x01F00000;

	game_etut(){
		arg = "etut";
		gamePath = "etut";
		homeBasePath = ".etwolf";
		magic = "et";
		maxLMSurfaceVerts = 1024;
		maxSurfaceVerts = 1024;
		maxSurfaceIndexes = 6144;
		lightmapGamma = 2.2f;
		lightmapsRGB = true;
		// texturesRGB = false; /* texture sRGB (yes, this is incorrect, but we better match ET:UT) */
		bspVersion = 47;
		surfaceParms.insert( surfaceParms.end(), {
		/* materials */
		{ "*mat_none",      0,                          0,                          U_MAT_NONE,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_tin",       0,                          0,                          U_MAT_TIN,                  U_MAT_MASK,                 0,                          0 },
		{ "*mat_aluminum",  0,                          0,                          U_MAT_ALUMINUM,             U_MAT_MASK,                 0,                          0 },
		{ "*mat_iron",      0,                          0,                          U_MAT_IRON,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_titanium",  0,                          0,                          U_MAT_TITANIUM,             U_MAT_MASK,                 0,                          0 },
		{ "*mat_steel",     0,                          0,                          U_MAT_STEEL,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_brass",     0,                          0,                          U_MAT_BRASS,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_copper",    0,                          0,                          U_MAT_COPPER,               U_MAT_MASK,                 0,                          0 },
		{ "*mat_cement",    0,                          0,                          U_MAT_CEMENT,               U_MAT_MASK,                 0,                          0 },
		{ "*mat_rock",      0,                          0,                          U_MAT_ROCK,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_gravel",    0,                          0,                          U_MAT_GRAVEL,               U_MAT_MASK,                 0,                          0 },
		{ "*mat_pavement",  0,                          0,                          U_MAT_PAVEMENT,             U_MAT_MASK,                 0,                          0 },
		{ "*mat_brick",     0,                          0,                          U_MAT_BRICK,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_clay",      0,                          0,                          U_MAT_CLAY,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_grass",     0,                          0,                          U_MAT_GRASS,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_dirt",      0,                          0,                          U_MAT_DIRT,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_mud",       0,                          0,                          U_MAT_MUD,                  U_MAT_MASK,                 0,                          0 },
		{ "*mat_snow",      0,                          0,                          U_MAT_SNOW,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_ice",       0,                          0,                          U_MAT_ICE,                  U_MAT_MASK,                 0,                          0 },
		{ "*mat_sand",      0,                          0,                          U_MAT_SAND,                 U_MAT_MASK,                 0,                          0 },
		{ "*mat_ceramic",   0,                          0,                          U_MAT_CERAMICTILE,          U_MAT_MASK,                 0,                          0 },
		{ "*mat_ceramictile",   0,                      0,                          U_MAT_CERAMICTILE,          U_MAT_MASK,                 0,                          0 },
		{ "*mat_linoleum",  0,                          0,                          U_MAT_LINOLEUM,             U_MAT_MASK,                 0,                          0 },
		{ "*mat_rug",       0,                          0,                          U_MAT_RUG,                  U_MAT_MASK,                 0,                          0 },
		{ "*mat_plaster",   0,                          0,                          U_MAT_PLASTER,              U_MAT_MASK,                 0,                          0 },
		{ "*mat_plastic",   0,                          0,                          U_MAT_PLASTIC,              U_MAT_MASK,                 0,                          0 },
		{ "*mat_cardboard", 0,                          0,                          U_MAT_CARDBOARD,            U_MAT_MASK,                 0,                          0 },
		{ "*mat_hardwood",  0,                          0,                          U_MAT_HARDWOOD,             U_MAT_MASK,                 0,                          0 },
		{ "*mat_softwood",  0,                          0,                          U_MAT_SOFTWOOD,             U_MAT_MASK,                 0,                          0 },
		{ "*mat_plank",     0,                          0,                          U_MAT_PLANK,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_glass",     0,                          0,                          U_MAT_GLASS,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_water",     0,                          0,                          U_MAT_WATER,                U_MAT_MASK,                 0,                          0 },
		{ "*mat_stucco",    0,                          0,                          U_MAT_STUCCO,               U_MAT_MASK,                 0,                          0 },
		} );
	}
};

struct game_ef : game_default
{
	static const int E_CONT_LADDER              = 128;         /* elite force ladder contents */
	static const int E_CONT_SHOTCLIP            = 0x40000;     /* elite force shot clip */
	static const int E_CONT_ITEM                = 0x80000;
	static const int E_SURF_FORCEFIELD          = 0x40000;     /* elite force forcefield brushes */

	game_ef(){
		arg = "ef";
		gamePath = "baseef";
		homeBasePath = ".ef";
		magic = "elite";
		/* overwrite "ladder" entry; note: magic number */
		ENSURE( strEqual( surfaceParms[31].name, "ladder" ) );
		surfaceParms[31] =
		{ "ladder",         E_CONT_LADDER,              Q_CONT_SOLID,               Q_SURF_LADDER,              0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID };
		surfaceParms.insert( surfaceParms.end(), {
		{ "shotclip",       E_CONT_SHOTCLIP,            Q_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "forcefield",     0,                          0,                          E_SURF_FORCEFIELD,          0,                          0,                          0 },
		} );
	}
};

struct game_qfusion : game_default
{
	game_qfusion(){
		arg = "qfusion";
		gamePath = "base";
		homeBasePath = ".qfusion";
		magic = "qfusion";
		maxLMSurfaceVerts = 65535;
		maxSurfaceVerts = 65535;
		maxSurfaceIndexes = 393210;
		lightmapSize = 512;
		lightmapsRGB = true;
		texturesRGB = true;
		colorsRGB = true;
		lightAngleHL = true;
		noStyles = true;
		keepLights = true;
		patchSubdivisions = 4;
		patchShadows = true;
		deluxeMap = true;
		miniMapBorder = 1.0f / 66.0f;
		miniMapNameFormat = "../minimaps/%s.tga";
		bspIdent = "FBSP";
		bspVersion = 1;
		load = LoadRBSPFile;
		write = WriteRBSPFile;
	}
};

struct game_reaction : game_default
{
/* Additional surface flags follow.  These were given to me (Rambetter) by TTI from the Reaction team.
   Note that some of these values have more than one bit set.  I'm not sure how Reaction is using these
   bits, but this is what I got from TTI. */
	static const int REACTION_SURF_GRAVEL    = 0x80000;
	static const int REACTION_SURF_WOOD      = 0x81000;
	static const int REACTION_SURF_CARPET    = 0x100000;
	static const int REACTION_SURF_METAL2    = 0x101000;
	static const int REACTION_SURF_GLASS     = 0x180000;
	static const int REACTION_SURF_GRASS     = 0x181000;
	static const int REACTION_SURF_SNOW      = 0x200000;
	static const int REACTION_SURF_MUD       = 0x201000;
	static const int REACTION_SURF_WOOD2     = 0x280000;
	static const int REACTION_SURF_HARDMETAL = 0x281000;
	static const int REACTION_SURF_LEAVES    = 0x300000;
	static const int REACTION_SURF_CEMENT    = 0x301000;
	static const int REACTION_SURF_MARBLE    = 0x380000;
	static const int REACTION_SURF_SNOW2     = 0x381000;
	static const int REACTION_SURF_HARDSTEPS = 0x400000;
	static const int REACTION_SURF_SAND      = 0x401000;

	game_reaction(){
		arg = "reaction";
		gamePath = "Boomstick";
		homeBasePath = ".Reaction";
		magic = "reaction";
		surfaceParms.insert( surfaceParms.end(), {
		{ "rq3_gravel",     0,  0,  REACTION_SURF_GRAVEL,       0,  0,  0 },
		{ "rq3_wood",       0,  0,  REACTION_SURF_WOOD,         0,  0,  0 },
		{ "rq3_carpet",     0,  0,  REACTION_SURF_CARPET,       0,  0,  0 },
		{ "rq3_metal2",     0,  0,  REACTION_SURF_METAL2,       0,  0,  0 },
		{ "rq3_glass",      0,  0,  REACTION_SURF_GLASS,        0,  0,  0 },
		{ "rq3_grass",      0,  0,  REACTION_SURF_GRASS,        0,  0,  0 },
		{ "rq3_snow",       0,  0,  REACTION_SURF_SNOW,         0,  0,  0 },
		{ "rq3_mud",        0,  0,  REACTION_SURF_MUD,          0,  0,  0 },
		{ "rq3_wood2",      0,  0,  REACTION_SURF_WOOD2,        0,  0,  0 },
		{ "rq3_hardmetal",  0,  0,  REACTION_SURF_HARDMETAL,    0,  0,  0 },
		{ "rq3_leaves",     0,  0,  REACTION_SURF_LEAVES,       0,  0,  0 },
		{ "rq3_cement",     0,  0,  REACTION_SURF_CEMENT,       0,  0,  0 },
		{ "rq3_marble",     0,  0,  REACTION_SURF_MARBLE,       0,  0,  0 },
		{ "rq3_snow2",      0,  0,  REACTION_SURF_SNOW2,        0,  0,  0 },
		{ "rq3_hardsteps",  0,  0,  REACTION_SURF_HARDSTEPS,    0,  0,  0 },
		{ "rq3_sand",       0,  0,  REACTION_SURF_SAND,         0,  0,  0 },
		} );
	}
};

/* vortex: darkplaces q1 engine */
struct game_darkplaces : game_default
{
	game_darkplaces(){
		arg = "darkplaces";
		gamePath = "id1";
		homeBasePath = ".darkplaces";
		magic = "darkplaces";
		maxLMSurfaceVerts = 999;
		lightmapExposure = 200.0f;
		gridScale = 0.3f;
		gridAmbientScale = 0.6f;
		noStyles = true;
		keepLights = true;
		patchSubdivisions = 4;
	}
};

/* vortex: deluxe quake game ( darkplaces q1 engine) */
struct game_dq : game_default
{
	game_dq(){
		arg = "dq";
		gamePath = "basedq";
		homeBasePath = ".dq";
		magic = "dq";
		lightmapGamma = 1.2f;
		lightmapExposure = 200.0f;
		gridScale = 0.3f;
		gridAmbientScale = 0.6f;
		noStyles = true;
		keepLights = true;
		patchSubdivisions = 4;
		patchShadows = true;
		deluxeMap = true;
		deluxeMode = 1;
	}
};

/* vortex: prophecy game ( darkplaces q1 engine) */
struct game_prophecy : game_default
{
	game_prophecy(){
		arg = "prophecy";
		gamePath = "base";
		homeBasePath = ".prophecy";
		magic = "prophecy";
		lightmapExposure = 200.0f;
		gridScale = 0.4f;
		gridAmbientScale = 0.6f;
		noStyles = true;
		keepLights = true;
		patchSubdivisions = 4;
		patchShadows = true;
		deluxeMap = true;
	}
};

struct game_sof2 : game_t
{
	/* thanks to the gracious fellows at raven */
	static const int S_CONT_SOLID               = 0x00000001;  /* Default setting. An eye is never valid in a solid */
	static const int S_CONT_LAVA                = 0x00000002;
	static const int S_CONT_WATER               = 0x00000004;
	static const int S_CONT_FOG                 = 0x00000008;
	static const int S_CONT_PLAYERCLIP          = 0x00000010;
	static const int S_CONT_MONSTERCLIP         = 0x00000020;
	static const int S_CONT_BOTCLIP             = 0x00000040;
	static const int S_CONT_SHOTCLIP            = 0x00000080;
	static const int S_CONT_BODY                = 0x00000100;  /* should never be on a brush, only in game */
	static const int S_CONT_CORPSE              = 0x00000200;  /* should never be on a brush, only in game */
	static const int S_CONT_TRIGGER             = 0x00000400;
	static const int S_CONT_NODROP              = 0x00000800;  /* don't leave bodies or items (death fog, lava) */
	static const int S_CONT_TERRAIN             = 0x00001000;  /* volume contains terrain data */
	static const int S_CONT_LADDER              = 0x00002000;
	static const int S_CONT_ABSEIL              = 0x00004000;  /* used like ladder to define where an NPC can abseil */
	static const int S_CONT_OPAQUE              = 0x00008000;  /* defaults to on, when off, solid can be seen through */
	static const int S_CONT_OUTSIDE             = 0x00010000;  /* volume is considered to be in the outside (i.e. not indoors) */
	static const int S_CONT_SLIME               = 0x00020000;  /* don't be fooled. it may SAY "slime" but it really means "projectileclip" */
	static const int S_CONT_LIGHTSABER          = 0x00040000;
	static const int S_CONT_TELEPORTER          = 0x00080000;
	static const int S_CONT_ITEM                = 0x00100000;
	static const int S_CONT_DETAIL              = 0x08000000;  /* brushes not used for the bsp */
	static const int S_CONT_TRANSLUCENT         = 0x80000000;  /* don't consume surface fragments inside */

	static const int S_SURF_SKY                 = 0x00002000;  /* lighting from environment map */
	static const int S_SURF_SLICK               = 0x00004000;  /* affects game physics */
	static const int S_SURF_METALSTEPS          = 0x00008000;  /* chc needs this since we use same tools */
	static const int S_SURF_FORCEFIELD          = 0x00010000;  /* chc */
	static const int S_SURF_NODAMAGE            = 0x00040000;  /* never give falling damage */
	static const int S_SURF_NOIMPACT            = 0x00080000;  /* don't make missile explosions */
	static const int S_SURF_NOMARKS             = 0x00100000;  /* don't leave missile marks */
	static const int S_SURF_NODRAW              = 0x00200000;  /* don't generate a drawsurface at all */
	static const int S_SURF_NOSTEPS             = 0x00400000;  /* no footstep sounds */
	static const int S_SURF_NODLIGHT            = 0x00800000;  /* don't dlight even if solid (solid lava, skies) */
	static const int S_SURF_NOMISCENTS          = 0x01000000;  /* no client models allowed on this surface */

	static const int S_SURF_PATCH               = 0x80000000;  /* mark this face as a patch(editor only) */

	static const int S_SURF_BEVELSMASK          = ( S_SURF_SLICK | S_SURF_METALSTEPS | S_SURF_NODAMAGE | S_SURF_NOSTEPS | S_SURF_NOMISCENTS ); /* compiler utility */

	/* materials */
	static const int S_MAT_BITS                 = 5;
	static const int S_MAT_MASK                 = 0x1f;        /* mask to get the material type */

	static const int S_MAT_NONE                 = 0;           /* for when the artist hasn't set anything up =) */
	static const int S_MAT_SOLIDWOOD            = 1;           /* freshly cut timber */
	static const int S_MAT_HOLLOWWOOD           = 2;           /* termite infested creaky wood */
	static const int S_MAT_SOLIDMETAL           = 3;           /* solid girders */
	static const int S_MAT_HOLLOWMETAL          = 4;           /* hollow metal machines */
	static const int S_MAT_SHORTGRASS           = 5;           /* manicured lawn */
	static const int S_MAT_LONGGRASS            = 6;           /* long jungle grass */
	static const int S_MAT_DIRT                 = 7;           /* hard mud */
	static const int S_MAT_SAND                 = 8;           /* sandy beach */
	static const int S_MAT_GRAVEL               = 9;           /* lots of small stones */
	static const int S_MAT_GLASS                = 10;
	static const int S_MAT_CONCRETE             = 11;          /* hardened concrete pavement */
	static const int S_MAT_MARBLE               = 12;          /* marble floors */
	static const int S_MAT_WATER                = 13;          /* light covering of water on a surface */
	static const int S_MAT_SNOW                 = 14;          /* freshly laid snow */
	static const int S_MAT_ICE                  = 15;          /* packed snow/solid ice */
	static const int S_MAT_FLESH                = 16;          /* hung meat, corpses in the world */
	static const int S_MAT_MUD                  = 17;          /* wet soil */
	static const int S_MAT_BPGLASS              = 18;          /* bulletproof glass */
	static const int S_MAT_DRYLEAVES            = 19;          /* dried up leaves on the floor */
	static const int S_MAT_GREENLEAVES          = 20;          /* fresh leaves still on a tree */
	static const int S_MAT_FABRIC               = 21;          /* Cotton sheets */
	static const int S_MAT_CANVAS               = 22;          /* tent material */
	static const int S_MAT_ROCK                 = 23;
	static const int S_MAT_RUBBER               = 24;          /* hard tire like rubber */
	static const int S_MAT_PLASTIC              = 25;
	static const int S_MAT_TILES                = 26;          /* tiled floor */
	static const int S_MAT_CARPET               = 27;          /* lush carpet */
	static const int S_MAT_PLASTER              = 28;          /* drywall style plaster */
	static const int S_MAT_SHATTERGLASS         = 29;          /* glass with the Crisis Zone style shattering */
	static const int S_MAT_ARMOR                = 30;          /* body armor */
	static const int S_MAT_COMPUTER             = 31;          /* computers/electronic equipment */
	static const int S_MAT_LAST                 = 32;          /* number of materials */

	game_sof2() : game_t{
	"sof2",                 /* -game x */
	"base",                 /* default base game data dir */
	".sof2",                /* unix home sub-dir */
	"soldier",              /* magic path word */
	"shaders",              /* shader directory */
	64,                     /* max lightmapped surface verts */
	999,                    /* max surface verts */
	6000,                   /* max surface indexes */
	true,                   /* flares */
	"gfx/misc/lens_flare",  /* default flare shader */
	false,                  /* wolf lighting model? */
	128,                    /* lightmap width/height */
	1.0f,                   /* lightmap gamma */
	false,                  /* lightmap sRGB */
	false,                  /* texture sRGB */
	false,                  /* color sRGB */
	0.0f,                   /* lightmap exposure */
	1.0f,                   /* lightmap compensate */
	1.0f,                   /* lightgrid scale */
	1.0f,                   /* lightgrid ambient scale */
	false,                  /* light angle attenuation uses half-lambert curve */
	false,                  /* disable shader lightstyles hack */
	false,                  /* keep light entities on bsp */
	8,                      /* default patchMeta subdivisions tolerance */
	false,                  /* patch casting enabled */
	false,                  /* compile deluxemaps */
	0,                      /* deluxemaps default mode */
	512,                    /* minimap size */
	1.0f,                   /* minimap sharpener */
	0.0f,                   /* minimap border */
	true,                   /* minimap keep aspect */
	EMiniMapMode::Gray,     /* minimap mode */
	"%s.tga",               /* minimap name format */
	"RBSP",                 /* bsp file prefix */
	1,                      /* bsp file version */
	false,                  /* cod-style lump len/ofs order */
	LoadRBSPFile,           /* bsp load function */
	WriteRBSPFile,          /* bsp write function */

	{
		/* name             contentFlags                contentFlagsClear           surfaceFlags                surfaceFlagsClear           compileFlags                compileFlagsClear */

		/* default */
		{ "default",        S_CONT_SOLID | S_CONT_OPAQUE,   -1,                     0,                          -1,                         C_SOLID,                    -1 },


		/* ydnar */
		{ "lightgrid",      0,                          0,                          0,                          0,                          C_LIGHTGRID,                0 },
		{ "antiportal",     0,                          0,                          0,                          0,                          C_ANTIPORTAL,               0 },
		{ "skip",           0,                          0,                          0,                          0,                          C_SKIP,                     0 },


		/* compiler */
		{ "origin",         0,                          S_CONT_SOLID,               0,                          0,                          C_ORIGIN | C_TRANSLUCENT,   C_SOLID },
		{ "areaportal",     S_CONT_TRANSLUCENT,         S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_AREAPORTAL | C_TRANSLUCENT,   C_SOLID },
		{ "trans",          S_CONT_TRANSLUCENT,         0,                          0,                          0,                          C_TRANSLUCENT,              0 },
		{ "detail",         S_CONT_DETAIL,              0,                          0,                          0,                          C_DETAIL,                   0 },
		{ "structural",     0,                          0,                          0,                          0,                          C_STRUCTURAL,               0 },
		{ "hint",           0,                          0,                          0,                          0,                          C_HINT,                     0 },
		{ "nodraw",         0,                          0,                          S_SURF_NODRAW,              0,                          C_NODRAW,                   0 },

		{ "alphashadow",    0,                          0,                          0,                          0,                          C_ALPHASHADOW | C_TRANSLUCENT,  0 },
		{ "lightfilter",    0,                          0,                          0,                          0,                          C_LIGHTFILTER | C_TRANSLUCENT,  0 },
		{ "nolightmap",     0,                          0,                          0,                          0,                          C_VERTEXLIT,                0 },
		{ "pointlight",     0,                          0,                          0,                          0,                          C_VERTEXLIT,                0 },


		/* game */
		{ "nonsolid",       0,                          S_CONT_SOLID,               0,                          0,                          0,                          C_SOLID },
		{ "nonopaque",      0,                          S_CONT_OPAQUE,              0,                          0,                          C_TRANSLUCENT,              0 },        /* setting trans ok? */

		{ "trigger",        S_CONT_TRIGGER,             S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_TRANSLUCENT,              C_SOLID },

		{ "water",          S_CONT_WATER,               S_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },
		{ "slime",          S_CONT_SLIME,               S_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },
		{ "lava",           S_CONT_LAVA,                S_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },

		{ "shotclip",       S_CONT_SHOTCLIP,            S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },  /* setting trans/detail ok? */
		{ "playerclip",     S_CONT_PLAYERCLIP,          S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "monsterclip",    S_CONT_MONSTERCLIP,         S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "nodrop",         S_CONT_NODROP,              S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },

		{ "terrain",        S_CONT_TERRAIN,             S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "ladder",         S_CONT_LADDER,              S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "abseil",         S_CONT_ABSEIL,              S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "outside",        S_CONT_OUTSIDE,             S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },

		{ "botclip",        S_CONT_BOTCLIP,             S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },

		{ "fog",            S_CONT_FOG,                 S_CONT_SOLID | S_CONT_OPAQUE,   0,                      0,                          C_FOG | C_DETAIL | C_TRANSLUCENT,   C_SOLID },  /* nonopaque? */
		{ "sky",            0,                          0,                          S_SURF_SKY,                 0,                          C_SKY,                      0 },

		{ "slick",          0,                          0,                          S_SURF_SLICK,               0,                          0,                          0 },

		{ "noimpact",       0,                          0,                          S_SURF_NOIMPACT,            0,                          0,                          0 },
		{ "nomarks",        0,                          0,                          S_SURF_NOMARKS,             0,                          C_NOMARKS,                  0 },
		{ "nodamage",       0,                          0,                          S_SURF_NODAMAGE,            0,                          0,                          0 },
		{ "metalsteps",     0,                          0,                          S_SURF_METALSTEPS,          0,                          0,                          0 },
		{ "nosteps",        0,                          0,                          S_SURF_NOSTEPS,             0,                          0,                          0 },
		{ "nodlight",       0,                          0,                          S_SURF_NODLIGHT,            0,                          0,                          0 },
		{ "nomiscents",     0,                          0,                          S_SURF_NOMISCENTS,          0,                          0,                          0 },
		{ "forcefield",     0,                          0,                          S_SURF_FORCEFIELD,          0,                          0,                          0 },


		/* materials */
		{ "*mat_none",      0,                          0,                          S_MAT_NONE,                 S_MAT_MASK,                 0,                          0 },
		{ "*mat_solidwood", 0,                          0,                          S_MAT_SOLIDWOOD,            S_MAT_MASK,                 0,                          0 },
		{ "*mat_hollowwood",    0,                      0,                          S_MAT_HOLLOWWOOD,           S_MAT_MASK,                 0,                          0 },
		{ "*mat_solidmetal",    0,                      0,                          S_MAT_SOLIDMETAL,           S_MAT_MASK,                 0,                          0 },
		{ "*mat_hollowmetal",   0,                      0,                          S_MAT_HOLLOWMETAL,          S_MAT_MASK,                 0,                          0 },
		{ "*mat_shortgrass",    0,                      0,                          S_MAT_SHORTGRASS,           S_MAT_MASK,                 0,                          0 },
		{ "*mat_longgrass",     0,                      0,                          S_MAT_LONGGRASS,            S_MAT_MASK,                 0,                          0 },
		{ "*mat_dirt",      0,                          0,                          S_MAT_DIRT,                 S_MAT_MASK,                 0,                          0 },
		{ "*mat_sand",      0,                          0,                          S_MAT_SAND,                 S_MAT_MASK,                 0,                          0 },
		{ "*mat_gravel",    0,                          0,                          S_MAT_GRAVEL,               S_MAT_MASK,                 0,                          0 },
		{ "*mat_glass",     0,                          0,                          S_MAT_GLASS,                S_MAT_MASK,                 0,                          0 },
		{ "*mat_concrete",  0,                          0,                          S_MAT_CONCRETE,             S_MAT_MASK,                 0,                          0 },
		{ "*mat_marble",    0,                          0,                          S_MAT_MARBLE,               S_MAT_MASK,                 0,                          0 },
		{ "*mat_water",     0,                          0,                          S_MAT_WATER,                S_MAT_MASK,                 0,                          0 },
		{ "*mat_snow",      0,                          0,                          S_MAT_SNOW,                 S_MAT_MASK,                 0,                          0 },
		{ "*mat_ice",       0,                          0,                          S_MAT_ICE,                  S_MAT_MASK,                 0,                          0 },
		{ "*mat_flesh",     0,                          0,                          S_MAT_FLESH,                S_MAT_MASK,                 0,                          0 },
		{ "*mat_mud",       0,                          0,                          S_MAT_MUD,                  S_MAT_MASK,                 0,                          0 },
		{ "*mat_bpglass",   0,                          0,                          S_MAT_BPGLASS,              S_MAT_MASK,                 0,                          0 },
		{ "*mat_dryleaves", 0,                          0,                          S_MAT_DRYLEAVES,            S_MAT_MASK,                 0,                          0 },
		{ "*mat_greenleaves",   0,                      0,                          S_MAT_GREENLEAVES,          S_MAT_MASK,                 0,                          0 },
		{ "*mat_fabric",    0,                          0,                          S_MAT_FABRIC,               S_MAT_MASK,                 0,                          0 },
		{ "*mat_canvas",    0,                          0,                          S_MAT_CANVAS,               S_MAT_MASK,                 0,                          0 },
		{ "*mat_rock",      0,                          0,                          S_MAT_ROCK,                 S_MAT_MASK,                 0,                          0 },
		{ "*mat_rubber",    0,                          0,                          S_MAT_RUBBER,               S_MAT_MASK,                 0,                          0 },
		{ "*mat_plastic",   0,                          0,                          S_MAT_PLASTIC,              S_MAT_MASK,                 0,                          0 },
		{ "*mat_tiles",     0,                          0,                          S_MAT_TILES,                S_MAT_MASK,                 0,                          0 },
		{ "*mat_carpet",    0,                          0,                          S_MAT_CARPET,               S_MAT_MASK,                 0,                          0 },
		{ "*mat_plaster",   0,                          0,                          S_MAT_PLASTER,              S_MAT_MASK,                 0,                          0 },
		{ "*mat_shatterglass",  0,                      0,                          S_MAT_SHATTERGLASS,         S_MAT_MASK,                 0,                          0 },
		{ "*mat_armor",     0,                          0,                          S_MAT_ARMOR,                S_MAT_MASK,                 0,                          0 },
		{ "*mat_computer",  0,                          0,                          S_MAT_COMPUTER,             S_MAT_MASK,                 0,                          0 },
	},

	S_SURF_BEVELSMASK
	}{}
};

struct game_jk2 : game_sof2
{
	game_jk2(){
		arg = "jk2";
		homeBasePath = ".jk2";
		magic = "GameData";
		flareShader = "gfx/misc/flare";
	}
};

struct game_ja : game_sof2
{
	static const int JA_CONT_INSIDE         = 0x10000000;  /* jedi academy 'inside' */
	static const int JA_SURF_FORCESIGHT     = 0x02000000;  /* jedi academy 'forcesight' */

	game_ja(){
		arg = "ja";
		homeBasePath = ".ja";
		magic = "GameData";
		flareShader = "gfx/misc/flare";
		surfaceParms.insert( surfaceParms.end(), {
		{ "inside",         JA_CONT_INSIDE,             0,                          0,                          0,                          0,                          0 },
		{ "forcesight",     0,                          0,                          JA_SURF_FORCESIGHT,         0,                          0,                          0 },
		} );
	}
};



const std::vector<game_t> g_games = { game_quake3(),
                                      game_quakelive(),
                                      game_nexuiz(),
                                      game_xonotic(),
                                      game_tremulous(),
                                      game_unvanquished(),
                                      game_tenebrae(),
                                      game_wolf(),
                                      game_wolfet(),
                                      game_etut(),
                                      game_ef(),
                                      game_qfusion(),
                                      game_reaction(),
                                      game_darkplaces(),
                                      game_dq(),
                                      game_prophecy(),
                                      game_sof2(),
                                      game_jk2(),
                                      game_ja(),
                                    };
const game_t *g_game = &g_games[0];

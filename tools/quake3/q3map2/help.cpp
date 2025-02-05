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

   -------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"



struct HelpOption
{
	const char* name;
	const char* description;
};

static void HelpOptions( const char* group_name, int indentation, int width, const std::vector<HelpOption>& options )
{
	indentation *= 2;
	char* indent = safe_malloc( indentation + 1 );
	memset( indent, ' ', indentation );
	indent[indentation] = 0;
	printf( "%s%s:\n", indent, group_name );
	indentation += 2;
	indent = void_ptr( realloc( indent, indentation + 1 ) );
	memset( indent, ' ', indentation );
	indent[indentation] = 0;

	for ( auto&& option : options )
	{
		int printed = printf( "%s%-24s  ", indent, option.name );
		int descsz = strlen( option.description );
		int j = 0;
		while ( j < descsz && descsz-j > width - printed )
		{
			if ( j != 0 )
				printf( "%s%26c", indent, ' ' );
			int fragment = width - printed;
			while ( fragment > 0 && option.description[j + fragment - 1] != ' ' )
				fragment--;
			j += fwrite( option.description + j, sizeof( char ), fragment, stdout );
			putchar( '\n' );
			printed = indentation + 26;
		}
		if ( j == 0 )
		{
			printf( "%s\n", option.description + j );
		}
		else if ( j < descsz )
		{
			printf( "%s%26c%s\n", indent, ' ', option.description + j );
		}
	}

	putchar( '\n' );

	free( indent );
}

static void HelpBsp()
{
	const std::vector<HelpOption> options = {
		{ "-bsp [options] <filename.map>", "Switch that enters this stage" },
		{ "-altsplit", "Alternate BSP tree splitting weights (should give more fps)" },
		{ "-autocaulk", "Only output special .caulk file for use by radiant" },
		{ "-celshader <shadername>", "Sets a global cel shader name" },
		{ "-clipdepth <F>", "Model autoclip brushes thickness, default = 2" },
		{ "-custinfoparms", "Read scripts/custinfoparms.txt" },
		{ "-debugclip", "Make model autoclip brushes visible, using shaders debugclip, debugclip2" },
		{ "-debuginset", "Push all triangle vertexes towards the triangle center" },
		{ "-debugportals", "Make BSP portals visible in the map" },
		{ "-debugsurfaces", "Color the vertexes according to the index of the surface" },
		{ "-deep", "Use detail brushes in the BSP tree, but at lowest priority (should give more fps)" },
		{ "-de <F>", "Distance epsilon for plane snapping etc." },
		{ "-fakemap", "Write fakemap.map containing all world brushes" },
		{ "-flares", "Turn on support for flares" },
		{ "-flat", "Enable flat shading (good for combining with -celshader)" },
		{ "-fulldetail", "Treat detail brushes as structural ones" },
		{ "-globalflag <surfaceparm>", "Add surface flag to every bsp shader. Reusable, e.g. -globalflag slick -globalflag nodamage." },
		{ "-keeplights", "Keep light entities in the BSP file after compile" },
		{ "-keepmodels", "Keep misc_model entities in the BSP file after compile" },
		{ "-leaktest", "Abort if a leak was found" },
		{ "-maxarea", "Use Max Area face surface generation" },
		{ "-meta", "Combine adjacent triangles of the same texture to surfaces (ALWAYS USE THIS)" },
		{ "-metaadequatescore <N>", "Adequate score for adding triangles to meta surfaces" },
		{ "-metagoodscore <N>", "Good score for adding triangles to meta surfaces" },
		{ "-minsamplesize <N>", "Sets minimum lightmap resolution in luxels/qu" },
		{ "-mi <N>", "Sets the maximum number of indexes per surface" },
		{ "-mv <N>", "Sets the maximum number of vertices of a lightmapped surface" },
		{ "-ne <F>", "Normal epsilon for plane snapping etc." },
		{ "-nobrushsnap", "Disable brush vertices snapping" },
		{ "-nocurves", "Turn off support for patches" },
		{ "-nodetail", "Leave out detail brushes" },
		{ "-noflares", "Turn off support for flares" },
		{ "-nofog", "Turn off support for fog volumes" },
		{ "-nohint", "Turn off support for hint brushes" },
		{ "-noob", "Assign surfaceparm noob to all map surfaces (Q3A:Defrag mod no-overbounces flag)" },
		{ "-nosRGB", "Treat colors and textures as linear colorspace" },
		{ "-nosRGBcolor", "Treat shader and light entity colors as linear colorspace" },
		{ "-nosRGBtex", "Treat textures as linear colorspace" },
		{ "-nosubdivide", "Turn off support for `q3map_tessSize` (breaks water vertex deforms)" },
		{ "-notjunc", "Do not fix T-junctions (causes cracks between triangles, do not use)" },
		{ "-nowater", "Turn off support for water, slime or lava (Stef, this is for you)" },
		{ "-np <A>", "Force all surfaces to be nonplanar with a given shade angle" },
		{ "-onlyents", "Only update entities in the BSP" },
		{ "-patchmeta", "Turn patches into triangle meshes for display" },
		{ "-rename", "Append suffix to miscmodel shaders (needed for SoF2)" },
		{ "-samplesize <N>", "Sets default lightmap resolution in luxels/qu" },
		{ "-skyfix", "Turn sky box into six surfaces to work around ATI problems" },
		{ "-snap <N>", "Snap brush bevel planes to the given number of units" },
		{ "-sRGBcolor", "Treat shader and light entity colors as sRGB colorspace" },
		{ "-sRGBtex", "Treat textures as sRGB colorspace" },
		{ "-tempname <filename.map>", "Read the MAP file from the given file name" },
		{ "-verboseentities", "Enable `-v` only for map entities, not for the world" },
	};
	HelpOptions( "BSP Stage", 0, 80, options );
}

static void HelpVis()
{
	const std::vector<HelpOption> options = {
		{ "-vis [options] <filename.map>", "Switch that enters this stage" },
		{ "-fast", "Very fast and crude vis calculation" },
		{ "-hint", "Merge all but hint portals" },
		{ "-mergeportals", "The less crude half of `-merge`, makes vis sometimes much faster but doesn't hurt fps usually" },
		{ "-merge", "Faster but still okay vis calculation" },
		{ "-nopassage", "Just use PortalFlow vis (usually less fps)" },
		{ "-nosort", "Do not sort the portals before calculating vis (usually slower)" },
		{ "-passageOnly", "Just use PassageFlow vis (usually less fps)" },
		{ "-saveprt", "Keep the Portal file after running vis (so you can run vis again)" },
		{ "-v -v", "Extra verbose mode for cluster debug" }, // q3map2 common takes first -v
	};
	HelpOptions( "VIS Stage", 0, 80, options );
}

static void HelpLight()
{
	const std::vector<HelpOption> options = {
		{ "-light [options] <filename.map>", "Switch that enters this stage" },
		{ "-approx <N>", "Vertex light approximation tolerance (never use in conjunction with deluxemapping)" },
		{ "-areascale <F>, -area <F>", "Scaling factor for area lights (surfacelight)" },
		{ "-backsplash <Fscale Fdistance>", "scale area lights backsplash fraction + set distance globally; (distance < -900 to omit distance setting); default = 1 23; real area lights have no backsplash (scale = 0); q3map_backsplash shader keyword overrides this setting" },
		{ "-border", "Add a red border to lightmaps for debugging" },
		{ "-bouncecolorratio <F>", "0..1 ratio of colorizing light sample by texture" },
		{ "-bouncegrid", "Also compute radiosity on the light grid" },
		{ "-bounceonly", "Only compute radiosity" },
		{ "-bouncescale <F>", "Scaling factor for radiosity" },
		{ "-bounce <N>", "Maximal number of bounces for radiosity" },
		{ "-brightness <F>", "Scaling factor for resulting lightmaps brightness" },
		{ "-cheapgrid", "Use `-cheap` style lighting for radiosity" },
		{ "-cheap", "Abort vertex light calculations when white is reached" },
		{ "-compensate <F>", "Lightmap compensate (darkening factor applied after everything else)" },
		{ "-contrast <F>", "-255 .. 255 lighting contrast, default = 0" },
		{ "-cpma", "CPMA vertex lighting mode" },
		{ "-custinfoparms", "Read scripts/custinfoparms.txt" },
		{ "-dark", "Darken lightmap seams" },
		{ "-debugaxis", "Color the lightmaps according to the lightmap axis" },
		{ "-debugcluster", "Color the lightmaps according to the index of the cluster" },
		{ "-debugdeluxe", "Show deluxemaps on the lightmap" },
		{ "-debugnormals", "Color the lightmaps according to the direction of the surface normal" },
		{ "-debugorigin", "Color the lightmaps according to the origin of the luxels" },
		{ "-debugsamplesize", "display all of 'surface too large for desired samplesize+lightmapsize' warnings" },
		{ "-debugsurfaces, -debugsurface", "Color the lightmaps according to the index of the surface" },
		{ "-debug", "Mark the lightmaps according to the cluster: unmapped clusters get yellow, occluded ones get pink, flooded ones get blue overlay color, otherwise red" },
		{ "-deluxemode 0", "Use modelspace deluxemaps (DarkPlaces)" },
		{ "-deluxemode 1", "Use tangentspace deluxemaps" },
		{ "-deluxe, -deluxemap", "Enable deluxemapping (light direction maps)" },
		{ "-dirtdebug, -debugdirt", "Store the dirtmaps as lightmaps for debugging" },
		{ "-dirtdepth", "Dirtmapping depth" },
		{ "-dirtgain", "Dirtmapping exponent" },
		{ "-dirtmode 0", "Ordered direction dirtmapping" },
		{ "-dirtmode 1", "Randomized direction dirtmapping" },
		{ "-dirtscale", "Dirtmapping scaling factor" },
		{ "-dirty", "Enable dirtmapping" },
		{ "-dump", "Dump radiosity from `-bounce` into numbered MAP file prefabs" },
		{ "-export", "Export lightmaps when compile finished (like `-export` mode)" },
		{ "-exposure <F>", "Lightmap exposure to better support overbright spots" },
		{ "-external", "Force external lightmaps even if at size of internal lightmaps" },
		{ "-extlmhacksize <N|N N>", "External lightmaps hack size: similar to -lightmapsize N: Size of lightmaps to generate (must be a power of two), but instead of native external lightmaps enables hack to reference them in autogenerated shader (for vanilla Q3 etc)" },
		{ "-extradist <F>", "Extra distance for lights in map units" },
		{ "-extravisnudge", "Broken feature to nudge the luxel origin to a better vis cluster" },
		{ "-fastbounce", "Use `-fast` style lighting for radiosity" },
		{ "-faster", "Use a faster falloff curve for lighting; also implies `-fast`" },
		{ "-fastgrid", "Use `-fast` style lighting for the light grid" },
		{ "-fast", "Ignore tiny light contributions" },
		{ "-fill", "Fill lightmap colors from surrounding pixels to improve JPEG compression" },
		{ "-fillpink", "Fill unoccupied lightmap pixels with pink colour" },
		{ "-filter", "Lightmap filtering" },
		{ "-floodlight", "Enable floodlight (zero-effort somewhat decent lighting)" },
		{ "-gamma <F>", "Lightmap gamma" },
		{ "-gridambientdirectionality <F>", "Ambient directional lighting received (default: 0.0)" },
		{ "-gridambientscale <F>", "Scaling factor for the light grid ambient components only" },
		{ "-griddirectionality <F>", "Directional lighting received (default: 1.0)" },
		{ "-gridscale <F>", "Scaling factor for the light grid only" },
		{ "-lightanglehl 0", "Disable half lambert light angle attenuation" },
		{ "-lightanglehl 1", "Enable half lambert light angle attenuation" },
		{ "-lightmapdir <directory>", "Directory to store external lightmaps (default: same as map name without extension)" },
		{ "-lightmapsearchblocksize <N>", "Restricted lightmap searching - block size" },
		{ "-lightmapsearchpower <N>", "Restricted lightmap searching - merge power" },
		{ "-lightmapsize <N>", "Size of lightmaps to generate (must be a power of two)" },
		{ "-lomem", "Low memory but slower lighting mode" },
		{ "-lowquality", "Low quality floodlight (appears to currently break floodlight)" },
		{ "-minsamplesize <N>", "Sets minimum lightmap resolution in luxels/qu" },
		{ "-nobouncestore", "Do not store BSP, lightmap and shader files between bounces" },
		{ "-nocollapse", "Do not collapse identical lightmaps" },
		{ "-nodeluxe, -nodeluxemap", "Disable deluxemapping" },
		{ "-nofastpoint", "Disable fast point light calculation" },
		{ "-nogrid", "Disable grid light calculation (makes all entities fullbright)" },
		{ "-nolightmapsearch", "Do not optimize lightmap packing for GPU memory usage (as doing so costs fps)" },
		{ "-nolm", "Skip lightmaps calculation" },
		{ "-normalmap", "Color the lightmaps according to the direction of the surface normal (TODO is this identical to `-debugnormals`?)" },
		{ "-nosRGB", "Treat colors, textures, and lightmaps as linear colorspace" },
		{ "-nosRGBcolor", "Treat shader and light entity colors as linear colorspace" },
		{ "-nosRGBlight", "Write lightmaps as linear colorspace" },
		{ "-nosRGBtex", "Treat textures as linear colorspace" },
		{ "-nostyle, -nostyles", "Disable support for light styles" },
		{ "-nosurf", "Disable tracing against surfaces (only uses BSP nodes then)" },
		{ "-notrace", "Disable shadow occlusion" },
		{ "-novertex", "Disable vertex lighting; optional (0..1) value sets constant vertex light globally" },
		{ "-patchshadows", "Cast shadows from patches" },
		{ "-pointscale <F>, -point <F>", "Scaling factor for spherical and spot point lights (light entities)" },
		{ "-q3", "Use nonlinear falloff curve by default (like Q3A)" },
		{ "-randomsamples", "Use random sampling for lightmaps" },
		{ "-rawlightmapsizelimit <N>", "Sets maximum lightmap resolution in luxels/qu (only affects patches if used -patchmeta in BSP stage)" },
		{ "-samplescale <F>", "Scales all lightmap resolutions" },
		{ "-samplesize <N>", "Sets default lightmap resolution in luxels/qu" },
		{ "-samplessearchboxsize <N>", "Search box size (1 to 4) for lightmap adaptive supersampling" },
		{ "-samples <N>", "Adaptive supersampling quality" },
		{ "-saturation <F>", "Lighting saturation: default = 1, > 1 = saturate, 0 = grayscale, < 0 = complementary colors" },
		{ "-scale <F>", "Scaling factor for all light types" },
		{ "-shadeangle <A>", "Angle for phong shading" },
		{ "-shade", "Enable phong shading at default shade angle" },
		{ "-skyscale <F>, -sky <F>", "Scaling factor for sky and sun light" },
		{ "-slowallocate", "Use old (a bit more careful, but much slower) lightmaps packing algorithm" },
		{ "-sphericalscale <F>, -spherical <F>", "Scaling factor for spherical point light entities" },
		{ "-spotscale <F>, -spot <F>", "Scaling factor for spot point light entities" },
		{ "-sRGB", "Treat colors, textures, and lightmaps as sRGB colorspace" },
		{ "-sRGBcolor", "Treat shader and light entity colors as sRGB colorspace" },
		{ "-sRGBlight", "Write lightmaps as sRGB colorspace" },
		{ "-sRGBtex", "Treat textures as sRGB colorspace" },
		{ "-style, -styles", "Enable support for light styles" },
		{ "-sunonly", "Only compute sun light" },
		{ "-super <N>, -supersample <N>", "Ordered grid supersampling quality" },
		{ "-thresh <F>", "Triangle subdivision threshold" },
		{ "-trianglecheck", "Broken check that should ensure luxels apply to the right triangle" },
		{ "-trisoup", "Convert brush faces to triangle soup" },
		{ "-vertexscale <F>", "Scaling factor for resulting vertex light values" },
		{ "-wolf", "Use linear falloff curve by default (like W:ET)" },
	};

	HelpOptions( "Light Stage", 0, 80, options );
}

static void HelpAnalyze()
{
	const std::vector<HelpOption> options = {
		{ "-analyze [options] <filename.bsp>", "Switch that enters this mode" },
		{ "-lumpswap", "Swap byte order in the lumps" },
	};

	HelpOptions( "Analyzing BSP-like file structure", 0, 80, options );
}

static void HelpScale()
{
	const std::vector<HelpOption> options = {
		{ "-scale [options] <S filename.bsp>", "Scale uniformly" },
		{ "-scale [options] <SX SY SZ filename.bsp>", "Scale non-uniformly" },
		{ "-tex", "Option to scale without texture lock" },
		{ "-spawn_ref <F>", "Option to vertically offset info_player_* entities (adds spawn_ref, scales, subtracts spawn_ref)" },
	};
	HelpOptions( "BSP Scaling", 0, 80, options );
}

static void HelpShift()
{
	const std::vector<HelpOption> options = {
		{ "-shift <S filename.bsp>", "Shift uniformly" },
		{ "-shift <SX SY SZ filename.bsp>", "Shift non-uniformly" },
	};
	HelpOptions( "BSP Shift", 0, 80, options );
}

static void HelpConvert()
{
	const std::vector<HelpOption> options = {
		{ "-convert [options] <filename.bsp>", "Switch that enters this mode" },
		{ "-deluxemapsastexcoord", "Save deluxemap names and texcoords instead of textures (only when writing ase and obj)" },
		{ "-de <F>", "Distance epsilon for the conversion (only when reading map)" },
		{ "-fast", "fast bsp to map conversion mode (without texture alignments)" },
		{ "-format <converter>", "Select the converter, default ase (available: map, map_bp, ase, obj, or game names)" },
		{ "-lightmapsastexcoord", "Save lightmap names and texcoords instead of textures (only when writing ase and obj)" },
		{ "-meta", "Combine adjacent triangles of the same texture to surfaces (only when reading map)" },
		{ "-ne <F>", "Normal epsilon for the conversion (only when reading map)" },
		{ "-patchmeta", "Turn patches into triangle meshes for display (only when reading map)" },
		{ "-readbsp", "Force converting bsp to selected format" },
		{ "-readmap", "Force converting map to selected format" },
		{ "-shadersasbitmap", "Save shader names as bitmap names in the model so it works as a prefab (only when writing ase and obj)" },
	};

	HelpOptions( "Converting & Decompiling", 0, 80, options );
}

static void HelpExport()
{
	const std::vector<HelpOption> options = {
		{ "-export <filename.bsp>", "Copies lightmaps from the BSP to `filename/lightmap_NNNN.tga`" },
	};

	HelpOptions( "Exporting lightmaps", 0, 80, options );
}

static void HelpImport()
{
	const std::vector<HelpOption> options = {
		{ "-import <filename.bsp>", "Copies lightmaps from `filename/lightmap_NNNN.tga` into the BSP" },
	};

	HelpOptions( "Importing lightmaps", 0, 80, options );
}

static void HelpExportEnts()
{
	const std::vector<HelpOption> options = {
		{ "-exportents <filename.bsp>", "Exports the entities to a text file (.ent)" },
	};
	HelpOptions( "ExportEnts Stage", 0, 80, options );
}

static void HelpFixaas()
{
	const std::vector<HelpOption> options = {
		{ "-fixaas <filename.bsp>", "Writes BSP checksum to AAS file, so that it's accepted as valid by engine" },
	};

	HelpOptions( "Fixing AAS checksum", 0, 80, options );
}

static void HelpInfo()
{
	const std::vector<HelpOption> options = {
		{ "-info <filename.bsp .. filenameN.bsp>", "Switch that enters this mode" },
	};

	HelpOptions( "Get info about BSP file", 0, 80, options );
}

static void HelpMinimap()
{
	const std::vector<HelpOption> options = {
		{ "-minimap [options] <filename.bsp>", "Creates a minimap of the BSP, by default writes to `../gfx/filename_mini.tga`" },
		{ "-autolevel", "Automatically level brightness and contrast" },
		{ "-black", "Write the minimap as a black-on-transparency RGBA32 image" },
		{ "-boost <F>", "Sets the contrast boost value (higher values make a brighter image); contrast boost is somewhat similar to gamma, but continuous even at zero" },
		{ "-border <F>", "Sets the amount of border pixels relative to the total image size" },
		{ "-brightness <F>", "Sets brightness value to add to minimap values" },
		{ "-contrast <F>", "Sets contrast value to scale minimap values (doesn't affect brightness)" },
		{ "-gray", "Write the minimap as a white-on-black GRAY8 image" },
		{ "-keepaspect", "Ensure the aspect ratio is kept (the minimap is then letterboxed to keep aspect)" },
		{ "-minmax <xmin ymin zmin xmax ymax zmax>", "Forces specific map dimensions (note: the minimap actually uses these dimensions, scaled to the target size while keeping aspect with centering, and 1/64 of border appended to all sides)" },
		{ "-noautolevel", "Do not automatically level brightness and contrast" },
		{ "-nokeepaspect", "Do not ensure the aspect ratio is kept (makes it easier to use the image in your code, but looks bad together with sharpening)" },
		{ "-o <filename.tga>", "Sets the output file name" },
		{ "-random <N>", "Sets the randomized supersampling count (cannot be combined with `-samples`)" },
		{ "-samples <N>", "Sets the ordered supersampling count (cannot be combined with `-random`)" },
		{ "-sharpen <F>", "Sets the sharpening coefficient" },
		{ "-size <N>", "Sets the width and height of the output image" },
		{ "-white", "Write the minimap as a white-on-transparency RGBA32 image" },
	};

	HelpOptions( "MiniMap", 0, 80, options );
}

static void HelpPk3()
{
	const std::vector<HelpOption> options = {
		{ "-pk3 [options] <filename.bsp .. filenameN.bsp>", "Creates a pk3 for the BSP(s) (complete Q3 support). Using file 'gamename.exclude' to exclude vanilla game resources." },
		{ "-complevel <N>", "Set compression level (-1 .. 10); 0 = uncompressed, -1 = 6, 10 = ultra zlib incompatible preset" },
		{ "-dbg", "Print wall of debug text, useful for .exclude file creation" },
		{ "-png", "include png textures, at highest priority; taking tga, jpg by default" },
	};

	HelpOptions( "PK3 creation", 0, 80, options );
}

static void HelpRepack()
{
	const std::vector<HelpOption> options = {
		{ "-repack [options] <filename.bsp .. filenameN.bsp|filenames.txt>", "Creates repack of BSP(s) (complete Q3 support). Rips off only used shaders to new shader file. Using file 'gamename.exclude' to exclude vanilla game resources and 'repack.exclude' to exclude resources of existing repack." },
		{ "-analyze", "Only print bsp resource references and exit" },
		{ "-complevel <N>", "Set compression level (-1 .. 10); 0 = uncompressed, -1 = 6, 10 = ultra zlib incompatible preset" },
		{ "-dbg", "Print wall of debug text" },
		{ "-png", "include png textures, at highest priority; taking tga, jpg by default" },
	};

	HelpOptions( "Maps repack creation", 0, 80, options );
}

static void HelpJson()
{
	const std::vector<HelpOption> options = {
		{ "-json [options] <filename.bsp>", "Export/import BSP to/from json text files for debugging and editing purposes" },
		{ "-unpack", "Unpack BSP to json" },
		{ "-pack", "Pack json to BSP" },
		{ "-useflagnames", "While packing, deduce surface/content flag values from their names in shaders.json (useful for conversion to a game with different flag values)" },
		{ "-skipflags", "While -useflagnames, skip unknown flag names" },
	};

	HelpOptions( "BSP json export/import", 0, 80, options );
}

static void HelpMergeBsp()
{
	const std::vector<HelpOption> options = {
		{ "-mergebsp [options] <mainBsp.bsp> <bspToinject.bsp>", "Inject latter BSP to former. Tree and vis data of the main one are preserved." },
		{ "-fixnames", "Make incoming BSP target/targetname names unique to not collide with existing names" },
		{ "-world", "Also merge worldspawn model (brushes as if they were detail, no BSP tree is affected) (only merges entities by default)" },
	};

	HelpOptions( "BSP merge", 0, 80, options );
}

static void HelpCommon()
{
	const std::vector<HelpOption> options = {
		{ "-connect <address>", "Talk to a NetRadiant instance using a specific XML based protocol" },
		{ "-force", "Allow reading some broken/unsupported BSP files e.g. when decompiling, may crash. Also enables decompilation of model autoclip brushes." },
		{ "-fs_basepath <path>", "Sets the given path as main directory of the game (can be used more than once to look in multiple paths)" },
		{ "-fs_forbiddenpath <pattern>", "Pattern to ignore directories, pk3, and pk3dir; example pak?.pk3 (can be used more than once to look for multiple patterns)" },
		{ "-fs_game <gamename>", "Sets extra game directory name to additionally load mod's resources from at higher priority (by default for Q3A 'baseq3' is loaded, -fs_game cpma will also load 'cpma'; can be used more than once)" },
		{ "-fs_basegame <gamename>", "Overrides default game directory name (e.g. Q3A uses 'baseq3', OpenArena 'baseoa', so -game quake3 -fs_basegame baseoa for OA )" },
		{ "-fs_home <dir>", "Specifies where the user home directory is on Linux" },
		{ "-fs_homebase <dir>", "Specifies game home directory relative to user home directory on Linux (default for Q3A: .q3a)" },
		{ "-fs_homepath <path>", "Sets the given path as the game home directory name (fs_home + fs_homebase)" },
		{ "-fs_pakpath <path>", "Specify a package directory (can be used more than once to look in multiple paths)" },
		{ "-game <gamename>", "Load settings for the given game (default: quake3), -help -game lists available games" },
		{ "-maxmapdrawsurfs <N>", "Sets max amount of mapDrawSurfs, used during .map compilation (-bsp, -convert), default = 131072" },
		{ "-maxshaderinfo <N>", "Sets max amount of shaderInfo, default = 8192" },
		{ "-subdivisions <F>", "multiplier for patch subdivisions quality" },
		{ "-threads <N>", "number of threads to use" },
		{ "-v", "Verbose mode" },
	};

	HelpOptions( "Common Options", 0, 80, options );

}

void HelpGames(){
	Sys_Printf( "Available games:\n" );
	for( const game_t& game : g_games )
		Sys_Printf( "  %s\n", game.arg );
}

void HelpMain( const char* arg )
{
	printf( "Usage: q3map2 [stage] [common options...] [stage options...] [stage source file]\n" );
	printf( "       q3map2 -help [stage]\n\n" );

	HelpCommon();

	const std::vector<HelpOption> stages = {
		{ "-bsp", "BSP Stage" },
		{ "-vis", "VIS Stage" },
		{ "-light", "Light Stage" },
		{ "-analyze", "Analyzing BSP-like file structure" },
		{ "-scale", "Scaling" },
		{ "-shift", "Shift" },
		{ "-convert", "Converting & Decompiling" },
		{ "-export", "Exporting lightmaps" },
		{ "-import", "Importing lightmaps" },
		{ "-exportents", "Exporting entities" },
		{ "-fixaas", "Fixing AAS checksum" },
		{ "-info", "Get info about BSP file" },
		{ "-minimap", "MiniMap" },
		{ "-pk3", "PK3 creation" },
		{ "-repack", "Maps repack creation" },
		{ "-json", "BSP json export/import" },
		{ "-mergebsp", "BSP merge" },
	};
	void( *help_funcs[] )() = {
		HelpBsp,
		HelpVis,
		HelpLight,
		HelpAnalyze,
		HelpScale,
		HelpShift,
		HelpConvert,
		HelpExport,
		HelpImport,
		HelpExportEnts,
		HelpFixaas,
		HelpInfo,
		HelpMinimap,
		HelpPk3,
		HelpRepack,
		HelpJson,
		HelpMergeBsp,
	};

	if ( !strEmptyOrNull( arg ) )
	{
		if ( arg[0] == '-' )
			arg++;

		for ( size_t i = 0; i < stages.size(); ++i )
			if ( striEqual( arg, stages[i].name + 1 ) )
				return help_funcs[i]();

		if( striEqual( arg, "game" ) )
			return HelpGames();
	}

	HelpOptions( "Stages", 0, 80, stages );
}

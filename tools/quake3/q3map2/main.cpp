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
#include "autopk3.h"
#include "timer.h"



static void new_handler(){
	Error( "Memory allocation failed, terminating" );
}

/*
   ExitQ3Map()
   cleanup routine
 */

static void ExitQ3Map(){
	/* flush xml send buffer, shut down connection */
	Broadcast_Shutdown();
	free( mapDrawSurfs );
}



/*
   main()
   q3map mojo...
 */

int main( int argc, char **argv ){
	int r;

#ifdef WIN32
	_setmaxstdio( 2048 );
#endif

	/* we want consistent 'randomness' */
	srand( 0 );

	/* start timer */
	Timer timer;

	/* this was changed to emit version number over the network */
	printf( Q3MAP_VERSION "\n" );

	/* set exit call */
	atexit( ExitQ3Map );

	/* set allocation error callback */
	std::set_new_handler( new_handler );

	Args args( argc, argv );

	/* read general options first */
	{
		/* -help */
		if ( args.takeArg( "-h", "--help", "-help" ) ) {
			HelpMain( args.nextAvailable()? args.takeNext() : nullptr );
			return 0;
		}

		/* -connect */
		if ( args.takeArg( "-connect" ) ) {
			Broadcast_Setup( args.takeNext() );
		}

		/* verbose */
		if ( args.takeArg( "-v" ) ) { // test just once: leave other possible -v for -vis
			verbose = true;
		}

		/* force */
		while ( args.takeArg( "-force" ) ) {
			force = true;
		}

		/* patch subdivisions */
		while ( args.takeArg( "-subdivisions" ) ) {
			patchSubdivisions = std::max( atoi( args.takeNext() ), 1 );
		}

		/* threads */
		while ( args.takeArg( "-threads" ) ) {
			numthreads = atoi( args.takeNext() );
		}

		/* max_map_draw_surfs */
		while ( args.takeArg( "-maxmapdrawsurfs" ) ) {
			max_map_draw_surfs = abs( atoi( args.takeNext() ) );
			Sys_Printf( "max_map_draw_surfs = %d, mapDrawSurfs size = %.2f MBytes \n",
			            max_map_draw_surfs, sizeof( mapDrawSurface_t ) * max_map_draw_surfs / ( 1024.f * 1024.f ) );
		}

		/* max_shader_info */
		while ( args.takeArg( "-maxshaderinfo" ) ) {
			max_shader_info = abs( atoi( args.takeNext() ) );
			Sys_Printf( "max_shader_info = %d, shaderInfo size = %.2f MBytes \n",
			            max_shader_info, sizeof( shaderInfo_t ) * max_shader_info / ( 1024.f * 1024.f ) );
		}
	}

	/* init model library */
	assimp_init();

	/* set number of threads */
	ThreadSetDefault();

	/* we print out two versions, q3map's main version (since it evolves a bit out of GtkRadiant)
	   and we put the GtkRadiant version to make it easy to track with what version of Radiant it was built with */

	Sys_Printf( "Q3Map         - v1.0r (c) 1999 Id Software Inc.\n" );
	Sys_Printf( "Q3Map (ydnar) - v" Q3MAP_VERSION "\n" );
	Sys_Printf( "NetRadiant    - v" RADIANT_VERSION " " __DATE__ " " __TIME__ "\n" );
	Sys_Printf( "%s\n", Q3MAP_MOTD );
	Sys_Printf( "%s\n", args.getArg0() );

	/* ydnar: new path initialization */
	InitPaths( args );

	/* set game options */
	if ( !patchSubdivisions ) {
		patchSubdivisions = g_game->patchSubdivisions;
	}

	/* check if we have enough options left to attempt something */
	if ( args.empty() ) {
		Error( "Usage: %s [general options] [options] mapfile\n%s -help for help", args.getArg0(), args.getArg0() );
	}

	/* fixaas */
	if ( args.takeFront( "-fixaas" ) ) {
		r = FixAAS( args );
	}

	/* analyze */
	else if ( args.takeFront( "-analyze" ) ) {
		r = AnalyzeBSP( args );
	}

	/* info */
	else if ( args.takeFront( "-info" ) ) {
		r = BSPInfo( args );
	}

	/* vis */
	else if ( args.takeFront( "-vis" ) ) {
		r = VisMain( args );
	}

	/* light */
	else if ( args.takeFront( "-light" ) ) {
		r = LightMain( args );
	}

	/* QBall: export entities */
	else if ( args.takeFront( "-exportents" ) ) {
		r = ExportEntitiesMain( args );
	}

	/* ydnar: lightmap export */
	else if ( args.takeFront( "-export" ) ) {
		r = ExportLightmapsMain( args );
	}

	/* ydnar: lightmap import */
	else if ( args.takeFront( "-import" ) ) {
		r = ImportLightmapsMain( args );
	}

	/* ydnar: bsp scaling */
	else if ( args.takeFront( "-scale" ) ) {
		r = ScaleBSPMain( args );
	}

	/* bsp shifting */
	else if ( args.takeFront( "-shift" ) ) {
		r = ShiftBSPMain( args );
	}

	/* autopacking */
	else if ( args.takeFront( "-pk3" ) ) {
		r = pk3BSPMain( args );
	}

	/* repacker */
	else if ( args.takeFront( "-repack" ) ) {
		r = repackBSPMain( args );
	}

	/* ydnar: bsp conversion */
	else if ( args.takeFront( "-convert" ) ) {
		r = ConvertBSPMain( args );
	}

	/* json export/import */
	else if ( args.takeFront( "-json" ) ) {
		r = ConvertJsonMain( args );
	}

	/* merge two bsps */
	else if ( args.takeFront( "-mergebsp" ) ) {
		r = MergeBSPMain( args );
	}

	/* div0: minimap */
	else if ( args.takeFront( "-minimap" ) ) {
		r = MiniMapBSPMain( args );
	}

	/* ydnar: otherwise create a bsp */
	else{
		r = BSPMain( args );
	}

	/* emit time */
	Sys_Printf( "%9.0f seconds elapsed\n", timer.elapsed_sec() );

	/* return any error code */
	return r;
}

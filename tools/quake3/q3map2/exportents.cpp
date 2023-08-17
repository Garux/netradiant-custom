/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2013 id Software, Inc. and contributors.
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
#include "bspfile_rbsp.h"




/* -------------------------------------------------------------------------------

   this file contains code that exports entities to a .ent file.

   ------------------------------------------------------------------------------- */

/*
   ExportEntities()
   exports the entities to a text file (.ent)
 */

static void ExportEntities(){
	/* note it */
	Sys_FPrintf( SYS_VRB, "--- ExportEntities ---\n" );

	/* sanity check */
	if ( bspEntData.empty() ) {
		Sys_Warning( "No BSP entity data. aborting...\n" );
		return;
	}

	if( g_game->load == LoadRBSPFile ){ // intent here is to make RBSP specific stuff usable for entity modding with -onlyents
		ParseEntities();
		UnSetLightStyles();
		UnparseEntities();
	}

	/* write it */
	auto filename = StringOutputStream( 256 )( PathExtensionless( source ), ".ent" );
	Sys_Printf( "Writing %s\n", filename.c_str() );
	Sys_FPrintf( SYS_VRB, "(%zu bytes)\n", bspEntData.size() );
	FILE *file = SafeOpenWrite( filename, "wt" );

	fprintf( file, "%s\n", bspEntData.data() );
	fclose( file );
}



/*
   ExportEntitiesMain()
   exports the entities to a text file (.ent)
 */

int ExportEntitiesMain( Args& args ){
	/* arg checking */
	if ( args.empty() ) {
		Sys_Printf( "Usage: q3map2 -exportents [-v] <mapname>\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( args.takeBack() ) );
	path_set_extension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );

	/* export entities */
	ExportEntities();

	/* return to sender */
	return 0;
}

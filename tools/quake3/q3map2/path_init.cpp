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
#define PATH_INIT_C



/* dependencies */
#include "q3map2.h"



/* path support */
#define MAX_BASE_PATHS  10
#define MAX_GAME_PATHS  10
#define MAX_PAK_PATHS  200

char                    *homePath;
char installPath[ MAX_OS_PATH ];

int numBasePaths;
char                    *basePaths[ MAX_BASE_PATHS ];
int numGamePaths;
char                    *gamePaths[ MAX_GAME_PATHS ];
int numPakPaths;
char                    *pakPaths[ MAX_PAK_PATHS ];
char                    *homeBasePath = NULL;


/*
   some of this code is based off the original q3map port from loki
   and finds various paths. moved here from bsp.c for clarity.
 */

/*
   PathLokiGetHomeDir()
   gets the user's home dir (for ~/.q3a)
 */

char *LokiGetHomeDir( void ){
	#ifndef Q_UNIX
	return NULL;
	#else
	static char	buf[ 4096 ];
	struct passwd   pw, *pwp;
	char            *home;
	static char homeBuf[MAX_OS_PATH];


	/* get the home environment variable */
	home = getenv( "HOME" );

	/* look up home dir in password database */
	if( home == NULL )
	{
		if ( getpwuid_r( getuid(), &pw, buf, sizeof( buf ), &pwp ) == 0 ) {
			return pw.pw_dir;
		}
	}
	else{
		snprintf( homeBuf, sizeof( homeBuf ), "%s/.", home );
	}
	/* return it */
	return homeBuf;
	#endif
}



/*
   PathLokiInitPaths()
   initializes some paths on linux/os x
 */

void LokiInitPaths( char *argv0 ){
	if ( homePath == NULL ) {
		/* get home dir */
		homePath = LokiGetHomeDir();
		if ( homePath == NULL ) {
			homePath = ".";
		}
	}

	#ifndef Q_UNIX
	/* this is kinda crap, but hey */
	strcpy( installPath, "../" );
	#else
	char temp[ MAX_OS_PATH ];
	char        *path;
	char        *last;
	bool found;


	path = getenv( "PATH" );

	/* do some path divining */
	strcpyQ( temp, argv0, sizeof( temp ) );
	if ( strEmpty( path_get_last_separator( temp ) ) && path != NULL ) {

		/*
		   This code has a special behavior when q3map2 is a symbolic link.

		   For each dir in ${PATH} (example: "/usr/bin", "/usr/local/bin" if ${PATH} == "/usr/bin:/usr/local/bin"),
		   it looks for "${dir}/q3map2" (file exists and is executable),
		   then it uses "dirname(realpath("${dir}/q3map2"))/../" as installPath.

		   So, if "/usr/bin/q3map2" is a symbolic link to "/opt/radiant/tools/q3map2",
		   it will find the installPath "/usr/share/radiant/",
		   so q3map2 will look for "/opt/radiant/baseq3" to find paks.

		   More precisely, it looks for "${dir}/${argv[0]}",
		   so if "/usr/bin/q3map2" is a symbolic link to "/opt/radiant/tools/q3map2",
		   and if "/opt/radiant/tools/q3ma2" is a symbolic link to "/opt/radiant/tools/q3map2.x86_64",
		   it will use "dirname("/opt/radiant/tools/q3map2.x86_64")/../" as path,
		   so it will use "/opt/radiant/" as installPath, which will be expanded later to "/opt/radiant/baseq3" to find paks.
		*/

		found = false;
		last = path;

		/* go through each : segment of path */
		while ( !strEmpty( last ) && !found )
		{
			/* null out temp */
			strClear( temp );

			/* find next chunk */
			last = strchr( path, ':' );
			if ( last == NULL ) {
				last = path + strlen( path );
			}

			/* found home dir candidate */
			if ( *path == '~' ) {
				strcpyQ( temp, homePath, sizeof( temp ) );
				path++;
			}


			/* concatenate */
			if ( last > ( path + 1 ) ) {
				strncatQ( temp, path, sizeof( temp ), ( last - path ) );
				strcatQ( temp, "/", sizeof( temp ) );
			}
			strcatQ( temp, argv0, sizeof( temp ) );

			/* verify the path */
			if ( access( temp, X_OK ) == 0 ) {
				found = true;
			}
			path = last + 1;
		}
	}

	/* flake */
	if ( realpath( temp, installPath ) ) {
		/*
		   if "q3map2" is "/opt/radiant/tools/q3map2",
		   installPath is "/opt/radiant"
		*/
		strClear( path_get_last_separator( installPath ) );
		strClear( path_get_last_separator( installPath ) );
	}
	#endif
}



/*
   GetGame() - ydnar
   gets the game_t based on a -game argument
   returns NULL if no match found
 */

game_t *GetGame( char *arg ){
	int i;


	/* dummy check */
	if ( strEmptyOrNull( arg ) ) {
		return NULL;
	}

	/* joke */
	if ( striEqual( arg, "quake1" ) ||
		 striEqual( arg, "quake2" ) ||
		 striEqual( arg, "unreal" ) ||
		 striEqual( arg, "ut2k3" ) ||
		 striEqual( arg, "dn3d" ) ||
		 striEqual( arg, "dnf" ) ||
		 striEqual( arg, "hl" ) ) {
		Sys_Printf( "April fools, silly rabbit!\n" );
		exit( 0 );
	}

	/* test it */
	i = 0;
	while ( games[ i ].arg != NULL )
	{
		if ( striEqual( arg, games[ i ].arg ) ) {
			return &games[ i ];
		}
		i++;
	}

	/* no matching game */
	return NULL;
}



/*
   AddBasePath() - ydnar
   adds a base path to the list
 */

void AddBasePath( char *path ){
	/* dummy check */
	if ( strEmptyOrNull( path ) || numBasePaths >= MAX_BASE_PATHS ) {
		return;
	}

	/* add it to the list */
	basePaths[ numBasePaths ] = copystring( path );
	FixDOSName( basePaths[ numBasePaths ] );
	if ( strEmpty( EnginePath ) )
		strcpy( EnginePath, basePaths[ numBasePaths ] );
	numBasePaths++;
}



/*
   AddHomeBasePath() - ydnar
   adds a base path to the beginning of the list, prefixed by ~/
 */

void AddHomeBasePath( char *path ){
	int i;
	char temp[ MAX_OS_PATH ];

	if ( homePath == NULL ) {
		return;
	}

	/* dummy check */
	if ( strEmptyOrNull( path ) ) {
		return;
	}

	/* strip leading dot, if homePath does not end in /. */
	if ( strEqual( path, "." ) ) {
		/* -fs_homebase . means that -fs_home is to be used as is */
		strcpy( temp, homePath );
	}
	else if ( strEqualSuffix( homePath, "/." ) ) {
		/* concatenate home dir and path */ /* remove trailing /. of homePath */
		sprintf( temp, "%.*s/%s", (int)strlen( homePath ) - 2, homePath, path );
	}
	else
	{
		/* remove leading . of path */
		if ( path[0] == '.' ) {
			++path;
		}

		/* concatenate home dir and path */
		sprintf( temp, "%s/%s", homePath, path );
	}

	/* make a hole */
	for ( i = ( MAX_BASE_PATHS - 2 ); i >= 0; i-- )
		basePaths[ i + 1 ] = basePaths[ i ];

	/* add it to the list */
	basePaths[ 0 ] = copystring( temp );
	FixDOSName( basePaths[ 0 ] );
	numBasePaths++;
}



/*
   AddGamePath() - ydnar
   adds a game path to the list
 */

void AddGamePath( char *path ){
	int i;

	/* dummy check */
	if ( strEmptyOrNull( path ) || numGamePaths >= MAX_GAME_PATHS ) {
		return;
	}

	/* add it to the list */
	gamePaths[ numGamePaths ] = copystring( path );
	FixDOSName( gamePaths[ numGamePaths ] );
	numGamePaths++;

	/* don't add it if it's already there */
	for ( i = 0; i < numGamePaths - 1; i++ )
	{
		if ( strEqual( gamePaths[i], gamePaths[numGamePaths - 1] ) ) {
			free( gamePaths[numGamePaths - 1] );
			gamePaths[numGamePaths - 1] = NULL;
			numGamePaths--;
			break;
		}
	}

}


/*
   AddPakPath()
   adds a pak path to the list
 */

void AddPakPath( char *path ){
	/* dummy check */
	if ( strEmptyOrNull( path ) || numPakPaths >= MAX_PAK_PATHS ) {
		return;
	}

	/* add it to the list */
	pakPaths[ numPakPaths ] = copystring( path );
	FixDOSName( pakPaths[ numPakPaths ] );
	numPakPaths++;
}



/*
   InitPaths() - ydnar
   cleaned up some of the path initialization code from bsp.c
   will remove any arguments it uses
 */

void InitPaths( int *argc, char **argv ){
	int i, j, k;
	char temp[ MAX_OS_PATH ];


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- InitPaths ---\n" );

	/* get the install path for backup */
	LokiInitPaths( argv[ 0 ] );

	/* set game to default (q3a) */
	game = &games[ 0 ];
	numBasePaths = 0;
	numGamePaths = 0;

	strClear( EnginePath );

	/* parse through the arguments and extract those relevant to paths */
	for ( i = 0; i < *argc; i++ )
	{
		/* check for null */
		if ( argv[ i ] == NULL ) {
			continue;
		}

		/* -game */
		if ( strEqual( argv[ i ], "-game" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No game specified after %s", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			game = GetGame( argv[ i ] );
			if ( game == NULL ) {
				game = &games[ 0 ];
			}
			argv[ i ] = NULL;
		}

		/* -fs_forbiddenpath */
		else if ( strEqual( argv[ i ], "-fs_forbiddenpath" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			if ( g_numForbiddenDirs < VFS_MAXDIRS ) {
				strncpy( g_strForbiddenDirs[g_numForbiddenDirs], argv[i], PATH_MAX );
				g_strForbiddenDirs[g_numForbiddenDirs][PATH_MAX] = 0;
				++g_numForbiddenDirs;
			}
			argv[ i ] = NULL;
		}

		/* -fs_basepath */
		else if ( strEqual( argv[ i ], "-fs_basepath" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			AddBasePath( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* -fs_game */
		else if ( strEqual( argv[ i ], "-fs_game" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			AddGamePath( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* -fs_home */
		else if ( strEqual( argv[ i ], "-fs_home" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			homePath = argv[i];
			argv[ i ] = NULL;
		}

		/* -fs_homebase */
		else if ( strEqual( argv[ i ], "-fs_homebase" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			homeBasePath = argv[i];
			argv[ i ] = NULL;
		}

		/* -fs_homepath - sets both of them */
		else if ( strEqual( argv[ i ], "-fs_homepath" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			homePath = argv[i];
			homeBasePath = ".";
			argv[ i ] = NULL;
		}

		/* -fs_pakpath */
		else if ( strEqual( argv[ i ], "-fs_pakpath" ) ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			AddPakPath( argv[ i ] );
			argv[ i ] = NULL;
		}

	}

	/* remove processed arguments */
	for ( i = 0, j = 0, k = 0; i < *argc && j < *argc; i++, j++ )
	{
		for ( ; j < *argc && argv[ j ] == NULL; j++ ){
		}
		argv[ i ] = argv[ j ];
		if ( argv[ i ] != NULL ) {
			k++;
		}
	}
	*argc = k;

	/* add standard game path */
	AddGamePath( game->gamePath );

	/* if there is no base path set, figure it out */
	if ( numBasePaths == 0 ) {
		/* this is another crappy replacement for SetQdirFromPath() */
		for ( i = 0; i < *argc && numBasePaths == 0; i++ )
		{
			/* extract the arg */
			strcpy( temp, argv[ i ] );
			FixDOSName( temp );
			Sys_FPrintf( SYS_VRB, "Searching for \"%s\" in \"%s\" (%d)...\n", game->magic, temp, i );
			/* check for the game's magic word */
			char* found = strIstr( temp, game->magic );
			if( found ){
				/* now find the next slash and nuke everything after it */
				found = strchr( found, '/' );
				if( found )
					strClear( found );
				/* add this as a base path */
				AddBasePath( temp );
			}
		}

		/* add install path */
		if ( numBasePaths == 0 ) {
			AddBasePath( installPath );
		}

		/* check again */
		if ( numBasePaths == 0 ) {
			Error( "Failed to find a valid base path." );
		}
	}

	/* this only affects unix */
	if ( homeBasePath != NULL ) {
		AddHomeBasePath( homeBasePath );
	}
	else{
		AddHomeBasePath( game->homeBasePath );
	}

	/* initialize vfs paths */
	if ( numBasePaths > MAX_BASE_PATHS ) {
		numBasePaths = MAX_BASE_PATHS;
	}
	if ( numGamePaths > MAX_GAME_PATHS ) {
		numGamePaths = MAX_GAME_PATHS;
	}

	/* walk the list of game paths */
	for ( j = 0; j < numGamePaths; j++ )
	{
		/* walk the list of base paths */
		for ( i = 0; i < numBasePaths; i++ )
		{
			/* create a full path and initialize it */
			sprintf( temp, "%s/%s/", basePaths[ i ], gamePaths[ j ] );
			vfsInitDirectory( temp );
		}
	}

	/* initialize vfs paths */
	if ( numPakPaths > MAX_PAK_PATHS ) {
		numPakPaths = MAX_PAK_PATHS;
	}

	/* walk the list of pak paths */
	for ( i = 0; i < numPakPaths; i++ )
	{
		/* initialize this pak path */
		vfsInitDirectory( pakPaths[ i ] );
	}

	/* done */
	Sys_Printf( "\n" );
}

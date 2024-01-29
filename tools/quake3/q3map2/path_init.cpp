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

/* platform-specific */
#if defined( __linux__ ) || defined( __APPLE__ )
	#include <unistd.h>
	#include <pwd.h>
	#define Q_UNIX
#endif

/*
   some of this code is based off the original q3map port from loki
   and finds various paths. moved here from bsp.c for clarity.
 */

/*
   PathLokiGetHomeDir()
   gets the user's home dir (for ~/.q3a)
 */

static CopiedString LokiGetHomeDir(){
	#ifndef Q_UNIX
	return "";
	#else
	/* get the home environment variable */
	const char *home = getenv( "HOME" );

	/* look up home dir in password database */
	if( home == NULL )
	{
		struct passwd   pw, *pwp;
		char buf[ 4096 ];
		if ( getpwuid_r( getuid(), &pw, buf, sizeof( buf ), &pwp ) == 0 ) {
			return pw.pw_dir;
		}
	}
	else{
		return StringStream( home, "/." ).c_str();
	}
	/* return it */
	return "";
	#endif
}



/*
   PathLokiInitPaths()
   initializes some paths on linux/os x
 */

static void LokiInitPaths( const char *argv0, CopiedString& homePath, CopiedString& installPath ){
	if ( homePath.empty() ) {
		/* get home dir */
		homePath = LokiGetHomeDir();
		if ( homePath.empty() ) {
			homePath = ".";
		}
	}

	#ifndef Q_UNIX
	/* this is kinda crap, but hey */
	installPath = "../";
	#else
	const char *path = getenv( "PATH" );
	auto temp = StringStream( argv0 );

	/* do some path divining */
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

		bool found = false;
		const char *last = path;

		/* go through each : segment of path */
		while ( !strEmpty( last ) && !found )
		{
			/* null out temp */
			temp.clear();

			/* find next chunk */
			last = strchr( path, ':' );
			if ( last == NULL ) {
				last = path + strlen( path );
			}

			/* found home dir candidate */
			if ( *path == '~' ) {
				temp( homePath );
				path++;
			}


			/* concatenate */
			if ( last > ( path + 1 ) ) {
				temp << StringRange( path, last ) << '/';
			}
			temp << argv0;

			/* verify the path */
			if ( access( temp, X_OK ) == 0 ) {
				found = true;
			}
			path = last + 1;
		}
	}

	/* flake */
	if ( char *real = realpath( temp, nullptr ); real != nullptr ) {
		/*
		   if "q3map2" is "/opt/radiant/tools/q3map2",
		   installPath is "/opt/radiant"
		*/
		strClear( path_get_last_separator( real ) );
		strClear( path_get_last_separator( real ) );
		installPath = real;
		free( real );
	}
	#endif
}



/*
   GetGame() - ydnar
   gets the game_t based on a -game argument
   returns NULL if no match found
 */

const game_t *GetGame( const char *arg ){
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
	for( const game_t& game : g_games )
	{
		if ( striEqual( arg, game.arg ) )
			return &game;
	}

	/* no matching game */
	Sys_Warning( "Game \"%s\" is unknown.\n", arg );
	HelpGames();
	return NULL;
}


inline bool is_unique( const std::vector<CopiedString>& list, const char *string ){
	for( const auto& str : list )
		if( striEqual( str.c_str(), string ) )
			return false;
	return true;
}

inline void insert_unique( std::vector<CopiedString>& list, const char *string ){
	if( is_unique( list, string ) )
		list.emplace_back( string );
}


/*
   AddBasePath() - ydnar
   adds a base path to the list
 */

static void AddBasePath( std::vector<CopiedString>& basePaths, const char *path ){
	/* dummy check */
	if ( !strEmptyOrNull( path ) ) {
		/* add it to the list */
		insert_unique( basePaths, StringStream( DirectoryCleaned( path ) ) );
		if ( g_enginePath.empty() )
			g_enginePath = basePaths.back();
	}
}



/*
   AddHomeBasePath() - ydnar
   adds a base path to the beginning of the list, prefixed by ~/
 */

static void AddHomeBasePath( std::vector<CopiedString>& basePaths, const char *homePath, const char *homeBasePath ){
	if ( strEmpty( homePath ) ) {
		return;
	}

	/* dummy check */
	if ( strEmptyOrNull( homeBasePath ) ) {
		return;
	}

	StringOutputStream str( 256 );

	/* strip leading dot, if homePath does not end in /. */
	if ( strEqual( homeBasePath, "." ) ) {
		/* -fs_homebase . means that -fs_home is to be used as is */
		str( homePath );
	}
	else if ( strEqualSuffix( homePath, "/." ) ) {
		/* concatenate home dir and path */ /* remove trailing /. of homePath */
		str( StringRange( homePath, strlen( homePath ) - 1 ), homeBasePath );
	}
	else
	{
		/* remove leading . of path */
		if ( homeBasePath[0] == '.' ) {
			++homeBasePath;
		}

		/* concatenate home dir and path */
		str( homePath, '/', homeBasePath );
	}

	/* add it to the beginning of the list */
	const auto clean = StringStream( DirectoryCleaned( str ) );
	if( is_unique( basePaths, clean ) )
		basePaths.emplace( basePaths.cbegin(), clean );
}



/*
   InitPaths() - ydnar
   cleaned up some of the path initialization code from bsp.c
   will remove any arguments it uses
 */

void InitPaths( Args& args ){
	std::vector<CopiedString> basePaths;
	std::vector<CopiedString> gamePaths;
	std::vector<CopiedString> pakPaths;

	CopiedString homePath;
	CopiedString installPath;
	const char *homeBasePath = nullptr;

	const char *baseGame = nullptr;
	StringOutputStream stream( 256 );


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- InitPaths ---\n" );

	/* get the install path for backup */
	LokiInitPaths( args.getArg0(), homePath, installPath );

	/* set game to default (q3a) */
	g_game = &g_games[ 0 ];

	/* parse through the arguments and extract those relevant to paths */
	{
		/* -game */
		while ( args.takeArg( "-game" ) ) {
			g_game = GetGame( args.takeNext() );
			if ( g_game == NULL ) {
				g_game = &g_games[ 0 ];
			}
		}

		/* -fs_forbiddenpath */
		while ( args.takeArg( "-fs_forbiddenpath" ) ) {
			g_strForbiddenDirs.emplace_back( args.takeNext() );
		}

		/* -fs_basepath */
		while ( args.takeArg( "-fs_basepath" ) ) {
			AddBasePath( basePaths, args.takeNext() );
		}

		/* -fs_game */
		while ( args.takeArg( "-fs_game" ) ) {
			insert_unique( gamePaths, stream( DirectoryCleaned( args.takeNext() ) ) );
		}

		/* -fs_basegame */
		while ( args.takeArg( "-fs_basegame" ) ) {
			baseGame = args.takeNext();
		}

		/* -fs_home */
		while ( args.takeArg( "-fs_home" ) ) {
			homePath = args.takeNext();
		}

		/* -fs_homebase */
		while ( args.takeArg( "-fs_homebase" ) ) {
			homeBasePath = args.takeNext();
		}

		/* -fs_homepath - sets both of them */
		while ( args.takeArg( "-fs_homepath" ) ) {
			homePath = args.takeNext();
			homeBasePath = ".";
		}

		/* -fs_pakpath */
		while ( args.takeArg( "-fs_pakpath" ) ) {
			insert_unique( pakPaths, stream( DirectoryCleaned( args.takeNext() ) ) );
		}

	}

	/* add standard game path */
	insert_unique( gamePaths, stream( DirectoryCleaned( baseGame == nullptr? g_game->gamePath : baseGame ) ) );

	/* if there is no base path set, figure it out */
	if ( basePaths.empty() ) {
		/* this is another crappy replacement for SetQdirFromPath() */
		auto argv = args.getVector();
		argv.insert( argv.cbegin(), args.getArg0() );
		for ( auto&& arg : argv )
		{
			/* extract the arg */
			stream( DirectoryCleaned( arg ) );
			Sys_FPrintf( SYS_VRB, "Searching for \"%s\" in \"%s\"...\n", g_game->magic, stream.c_str() );
			/* check for the game's magic word */
			char* found = strIstr( stream.c_str(), g_game->magic );
			if( found ){
				/* now find the next slash and nuke everything after it */
				found = strchr( found, '/' );
				if( found )
					strClear( found );
				/* add this as a base path */
				AddBasePath( basePaths, stream );
				if( !basePaths.empty() )
					break;
			}
		}

		/* add install path */
		if ( basePaths.empty() ) {
			AddBasePath( basePaths, installPath.c_str() );
		}

		/* check again */
		if ( basePaths.empty() ) {
			Error( "Failed to find a valid base path." );
		}
	}

	/* this only affects unix */
	AddHomeBasePath( basePaths, homePath.c_str(), homeBasePath != nullptr? homeBasePath : g_game->homeBasePath );

	/* initialize vfs paths */
	/* walk the list of game paths */
	for ( const auto& gamePath : gamePaths )
	{
		/* walk the list of base paths */
		for ( const auto& basePath : basePaths )
		{
			/* create a full path and initialize it */
			vfsInitDirectory( stream( basePath, gamePath ) );
		}
	}

	/* walk the list of pak paths */
	for ( const auto& pakPath : pakPaths )
	{
		/* initialize this pak path */
		vfsInitDirectory( pakPath.c_str() );
	}

	/* done */
	Sys_Printf( "\n" );
}

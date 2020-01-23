/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#include "environment.h"

#include "stream/textstream.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "debugging/debugging.h"
#include "os/path.h"
#include "os/file.h"
#include "cmdlib.h"

int g_argc;
const char** g_argv;

void args_init( int argc, char* argv[] ){
	int i, j, k;

	for ( i = 1; i < argc; i++ )
	{
		for ( k = i; k < argc; k++ )
			if ( argv[k] != 0 ) {
				break;
			}

		if ( k > i ) {
			k -= i;
			for ( j = i + k; j < argc; j++ )
				argv[j - k] = argv[j];
			argc -= k;
		}
	}

	g_argc = argc;
	g_argv = const_cast<const char **>( argv );
}

const char *gamedetect_argv_buffer[1024];
void gamedetect_found_game( const char *game, char *path ){
	int argc;
	static char buf[128];

	if ( g_argv == gamedetect_argv_buffer ) {
		return;
	}

	globalOutputStream() << "Detected game " << game << " in " << path << "\n";

	sprintf( buf, "-%s-EnginePath", game );
	argc = 0;
	gamedetect_argv_buffer[argc++] = "-global-gamefile";
	gamedetect_argv_buffer[argc++] = game;
	gamedetect_argv_buffer[argc++] = buf;
	gamedetect_argv_buffer[argc++] = path;
	if ( (size_t) ( argc + g_argc ) >= sizeof( gamedetect_argv_buffer ) / sizeof( *gamedetect_argv_buffer ) - 1 ) {
		g_argc = sizeof( gamedetect_argv_buffer ) / sizeof( *gamedetect_argv_buffer ) - g_argc - 1;
	}
	memcpy( gamedetect_argv_buffer + 4, g_argv, sizeof( *gamedetect_argv_buffer ) * g_argc );
	g_argc += argc;
	g_argv = gamedetect_argv_buffer;
}

bool gamedetect_check_game( const char *gamefile, const char *checkfile1, const char *checkfile2, char *buf /* must have 64 bytes free after bufpos */, int bufpos ){
	buf[bufpos] = '/';

	strcpy( buf + bufpos + 1, checkfile1 );
	globalOutputStream() << "Checking for a game file in " << buf << "\n";
	if ( !file_exists( buf ) ) {
		return false;
	}

	if ( checkfile2 ) {
		strcpy( buf + bufpos + 1, checkfile2 );
		globalOutputStream() << "Checking for a game file in " << buf << "\n";
		if ( !file_exists( buf ) ) {
			return false;
		}
	}

	buf[bufpos + 1] = 0;
	gamedetect_found_game( gamefile, buf );
	return true;
}

void gamedetect(){
	// if we're inside a Nexuiz install
	// default to nexuiz.game (unless the user used an option to inhibit this)
	bool gamedetect = false;
	int i;
	for ( i = 1; i < g_argc; ++i ){
		if ( !strcmp( g_argv[i], "-gamedetect" ) ) {
			gamedetect = true;
			//nogamedetect = !strcmp( g_argv[i + 1], "false" );
			break;
		}
	}
	if ( gamedetect ) {
		static char buf[1024 + 64];
		strncpy( buf, environment_get_app_path(), sizeof( buf ) );
		buf[sizeof( buf ) - 1 - 64] = 0;
		if ( !strlen( buf ) ) {
			return;
		}

		char *p = buf + strlen( buf ) - 1; // point directly on the slash of get_app_path
		while ( p != buf )
		{
			// TODO add more games to this

			if ( gamedetect_check_game( "q3.game", "baseq3/pak0.pk3", NULL, buf, p - buf ) ) {
				return;
			}
			if ( gamedetect_check_game( "q1.game", "id1/pak0.pak", NULL, buf, p - buf ) ) {
				return;
			}
			// try to detect Nexuiz installs
#if defined( WIN32 )
			if ( gamedetect_check_game( "nexuiz.game", "data/common-spog.pk3", "nexuiz.exe", buf, p - buf ) )
#elif defined( __APPLE__ )
			if ( gamedetect_check_game( "nexuiz.game", "data/common-spog.pk3", "Nexuiz.app/Contents/Info.plist", buf, p - buf ) )
#else
			if ( gamedetect_check_game( "nexuiz.game", "data/common-spog.pk3", "nexuiz-linux-glx.sh", buf, p - buf ) )
#endif
			{ return; }

			// try to detect Q2World installs
			if ( gamedetect_check_game( "q2w.game", "default/quake2world.version", NULL, buf, p - buf ) ) {
				return;
			}

			// try to detect Warsow installs
			if ( gamedetect_check_game( "warsow.game", "basewsw/dedicated_autoexec.cfg", NULL, buf, p - buf ) ) {
				return;
			}

			// we found nothing
			// go backwards
			--p;
			while ( p != buf && *p != '/' && *p != '\\' )
				--p;
		}
	}
}

namespace
{
// directories
CopiedString home_path;
CopiedString app_path;
// executable file path
CopiedString app_filepath;
}

const char* environment_get_home_path(){
	return home_path.c_str();
}

const char* environment_get_app_path(){
	return app_path.c_str();
}

const char* environment_get_app_filepath(){
	return app_filepath.c_str();
}

bool portable_app_setup(){
	StringOutputStream confdir( 256 );
	confdir << app_path.c_str() << "settings/";
	if ( file_exists( confdir.c_str() ) ) {
		home_path = confdir.c_str();
		return true;
	}
	return false;
}


CopiedString g_openMapByCmd;

void cmdMap(){
	for ( int i = 1; i < g_argc; ++i )
		if( string_equal_suffix_nocase( g_argv[i], ".map" ) ){
			StringOutputStream stream( 256 );
			stream << PathCleaned( g_argv[i] );
			g_openMapByCmd = stream.c_str();
			return;
		}
}

#if defined( POSIX )

#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

#include <glib.h>

const char* LINK_NAME =
#if defined ( __linux__ )
	"/proc/self/exe"
#else // FreeBSD and OSX
	"/proc/curproc/file"
#endif
;

/// brief Returns the filename of the executable belonging to the current process, or empty string, if not found.
const char* getexename( char *buf ){
	/* Now read the symbolic link */
	const int ret = readlink( LINK_NAME, buf, PATH_MAX );

	if ( ret == -1 ) {
		globalWarningStream() << "getexename: falling back to argv[0]: " << makeQuoted( g_argv[0] );
		if( realpath( g_argv[0], buf ) == 0 )
			*buf = '\0'; /* In case of an error, leave the handling up to the caller */
	}
	else{
		/* Ensure proper NUL termination */
		buf[ret] = 0;
	}
	return buf;
}

void environment_init( int argc, char* argv[] ){
	// Give away unnecessary root privileges.
	// Important: must be done before calling gtk_init().
	char *loginname;
	struct passwd *pw;
	seteuid( getuid() );
	if ( geteuid() == 0 && ( loginname = getlogin() ) != 0 &&
		 ( pw = getpwnam( loginname ) ) != 0 ) {
		setuid( pw->pw_uid );
	}

	args_init( argc, argv );

	{
		char real[PATH_MAX];
		app_filepath = getexename( real );
		ASSERT_MESSAGE( !string_empty( app_filepath.c_str() ), "failed to deduce app path" );
		// NOTE: we build app path with a trailing '/'
		// it's a general convention in Radiant to have the slash at the end of directories
		app_path = StringRange( real, path_get_filename_start( real ) );
	}

	if ( !portable_app_setup() ) {
		StringOutputStream home( 256 );
		home << DirectoryCleaned( g_get_home_dir() ) << ".netradiant/";
		Q_mkdir( home.c_str() );
		home_path = home.c_str();
	}
	gamedetect();
	cmdMap();
}

#elif defined( WIN32 )

#include <windows.h>

void environment_init( int argc, char* argv[] ){
	args_init( argc, argv );

	{
		// get path to the editor
		char filename[MAX_PATH + 1];
		GetModuleFileName( 0, filename, MAX_PATH );

		StringOutputStream stream( 256 );
		stream << PathCleaned( filename );
		app_filepath = stream.c_str();
		app_path = StringRange( stream.c_str(), path_get_filename_start( stream.c_str() ) );
	}

	if ( !portable_app_setup() ) {
		char *appdata = getenv( "APPDATA" );
		StringOutputStream home( 256 );
		if ( !appdata || string_empty( appdata ) ) {
			ERROR_MESSAGE( "Application Data folder not available.\n"
						   "Radiant will use C:\\ for user preferences.\n" );
			home << "C:";
		}
		else
		{
			home << PathCleaned( appdata );
		}
		home << "/NetRadiantSettings/";
		Q_mkdir( home.c_str() );
		home_path = home.c_str();
	}
	gamedetect();
	cmdMap();
}

#else
#error "unsupported platform"
#endif

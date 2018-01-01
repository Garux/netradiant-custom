/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
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

/*
   The following source code is licensed by Id Software and subject to the terms of
   its LIMITED USE SOFTWARE LICENSE AGREEMENT, a copy of which is included with
   GtkRadiant. If you did not receive a LIMITED USE SOFTWARE LICENSE AGREEMENT,
   please contact Id Software immediately at info@idsoftware.com.
 */

//
// Linux stuff
//
// Leonardo Zide (leo@lokigames.com)
//

#include "qe3.h"

#include "debugging/debugging.h"

#include "ifilesystem.h"

#include <map>

#include <QWidget>
#include <QMessageBox>

#include "stream/textfilestream.h"
#include "commandlib.h"
#include "stream/stringstream.h"
#include "os/path.h"
#include "scenelib.h"

#include "gtkutil/messagebox.h"
#include "error.h"
#include "map.h"
#include "build.h"
#include "points.h"
#include "camwindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "watchbsp.h"
#include "autosave.h"

QEGlobals_t g_qeglobals;


#if defined( POSIX )
#include <sys/stat.h> // chmod
#endif

#define RADIANT_MONITOR_ADDRESS "127.0.0.1:39000"


void QE_InitVFS(){
	// VFS initialization -----------------------
	// we will call GlobalFileSystem().initDirectory, giving the directories to look in (for files in pk3's and for standalone files)
	// we need to call in order, the mod ones first, then the base ones .. they will be searched in this order
	// *nix systems have a dual filesystem in ~/.q3a, which is searched first .. so we need to add that too

	const char* gamename = gamename_get();
	const char* basegame = basegame_get();
	const char* userRoot = g_qeglobals.m_userEnginePath.c_str();
	const char* globalRoot = EnginePath_get();

	std::vector<CopiedString> paths;
	const auto paths_push = [&paths]( const char* newPath ){ // collects unique paths
		if( !string_empty( newPath )
		&& std::none_of( paths.cbegin(), paths.cend(), [newPath]( const CopiedString& path ){ return path_equal( path.c_str(), newPath ); } ) )
			paths.emplace_back( newPath );
	};


	for( const auto& path : ExtraResourcePaths_get() )
		paths_push( path.c_str() );

	StringOutputStream str( 256 );
	// ~/.<gameprefix>/<fs_game>
	paths_push( str( userRoot, gamename, '/' ) ); // userGamePath
	// <fs_basepath>/<fs_game>
	paths_push( str( globalRoot, gamename, '/' ) ); // globalGamePath
	// ~/.<gameprefix>/<fs_main>
	paths_push( str( userRoot, basegame, '/' ) ); // userBasePath
	// <fs_basepath>/<fs_main>
	paths_push( str( globalRoot, basegame, '/' ) ); // globalBasePath

	for( const auto& path : paths )
		GlobalFileSystem().initDirectory( path.c_str() );
}


SimpleCounter g_brushCount;
SimpleCounter g_patchCount;
SimpleCounter g_entityCount;

void QE_brushCountChange(){
	std::size_t counts[3] = { g_brushCount.get(), g_patchCount.get(), g_entityCount.get() };
	if( GlobalSelectionSystem().countSelected() != 0 )
		GlobalSelectionSystem().countSelectedStuff( counts[0], counts[1], counts[2] );
	for ( int i = 0; i < 3; ++i ){
		char buffer[32];
		buffer[0] = '\0';
		if( counts[i] != 0 )
			sprintf( buffer, "%zu", counts[i] );
		g_pParentWnd->SetStatusText( c_status_brushcount + i, buffer );
	}
}

IdleDraw g_idle_scene_counts_update = IdleDraw( FreeCaller<void(), QE_brushCountChange>() );
void QE_brushCountChanged(){
	g_idle_scene_counts_update.queueDraw();
}


bool ConfirmModified( const char* title ){
	if ( !Map_Modified( g_map ) ) {
		return true;
	}

	const QMessageBox::StandardButton result =
		QMessageBox::question( MainFrame_getWindow(), title,
		                       "The current map has changed since it was last saved.\nDo you want to save the current map before continuing?",
		                       QMessageBox::StandardButton::Save | QMessageBox::StandardButton::Discard | QMessageBox::StandardButton::Cancel );
	if ( result == QMessageBox::StandardButton::Cancel ) {
		return false;
	}
	if ( result == QMessageBox::StandardButton::Save ) {
		if ( Map_Unnamed( g_map ) ) {
			return Map_SaveAs();
		}
		else
		{
			return Map_Save();
		}
	}
	return true; // QMessageBox::StandardButton::Discard
}

void bsp_init(){
	StringOutputStream stream( 256 );

	build_set_variable( "RadiantPath", AppPath_get() );
	build_set_variable( "ExecutableType", RADIANT_EXECUTABLE );
	build_set_variable( "EnginePath", EnginePath_get() );
	build_set_variable( "UserEnginePath", g_qeglobals.m_userEnginePath.c_str() );
	for( const auto& path : ExtraResourcePaths_get() )
		if( !path.empty() )
			stream << " -fs_pakpath " << makeQuoted( path );
	build_set_variable( "ExtraResourcePaths", stream );
	build_set_variable( "MonitorAddress", ( g_WatchBSP_Enabled ) ? RADIANT_MONITOR_ADDRESS : "" );
	build_set_variable( "GameName", gamename_get() );

	const char* mapname = Map_Name( g_map );

	build_set_variable( "BspFile", stream( PathExtensionless( mapname ), ".bsp" ) );

	if( g_region_active ){
		build_set_variable( "MapFile", stream( PathExtensionless( mapname ), ".reg" ) );
	}
	else{
		build_set_variable( "MapFile", mapname );
	}

	build_set_variable( "MapName", stream( PathFilename( mapname ) ) );
}

void bsp_shutdown(){
	build_clear_variables();
}

class BatchCommandListener
{
	TextOutputStream& m_file;
	std::size_t m_commandCount;
	const char* m_outputRedirect;
public:
	BatchCommandListener( TextOutputStream& file, const char* outputRedirect ) : m_file( file ), m_commandCount( 0 ), m_outputRedirect( outputRedirect ){
	}

	void execute( const char* command ){
		m_file << command;
		if( m_outputRedirect ){
			m_file << ( m_commandCount == 0? " > " : " >> " );
			m_file << makeQuoted( m_outputRedirect );
		}
		m_file << '\n';
		++m_commandCount;
	}
};


void RunBSP( size_t buildIdx ){
	if( !g_region_active )
		SaveMap();

	if ( Map_Unnamed( g_map ) ) {
		globalErrorStream() << "build cancelled: the map is unnamed\n";
		return;
	}

	if ( !g_region_active && g_SnapShots_Enabled && Map_Modified( g_map ) )
		Map_Snapshot();

	if ( g_region_active ) {
		const char* mapname = Map_Name( g_map );
		Map_SaveRegion( StringStream( PathExtensionless( mapname ), ".reg" ) );
	}

	Pointfile_Delete();

	bsp_init();

	const std::vector<CopiedString> commands = build_construct_commands( buildIdx );
	const bool monitor = std::any_of( commands.cbegin(), commands.cend(), []( const CopiedString& command ){
		return strstr( command.c_str(), RADIANT_MONITOR_ADDRESS ) != 0;
	} );

	if ( g_WatchBSP_Enabled && monitor ) {
		// grab the file name for engine running
		const char* fullname = Map_Name( g_map );
		const auto bspname = StringStream<64>( PathFilename( fullname ) );
		BuildMonitor_Run( commands, bspname );
	}
	else
	{
		const auto junkpath = StringStream( SettingsPath_get(), "junk.txt" );

#if defined( POSIX )
		const auto batpath = StringStream( SettingsPath_get(), "qe3bsp.sh" );
#elif defined( WIN32 )
		const auto batpath = StringStream( SettingsPath_get(), "qe3bsp.bat" );
#else
#error "unsupported platform"
#endif
		bool written = false;
		{
			TextFileOutputStream batchFile( batpath );
			if ( !batchFile.failed() ) {
#if defined ( POSIX )
				batchFile << "#!/bin/sh \n\n";
#endif
				BatchCommandListener listener( batchFile, g_WatchBSP0_DumpLog? junkpath.c_str() : nullptr );
				for( const auto& command : commands )
					listener.execute( command.c_str() );
				written = true;
			}
		}
		if ( written ) {
#if defined ( POSIX )
			chmod( batpath, 0744 );
#endif
			globalOutputStream() << "Writing the compile script to '" << batpath << "'\n";
			if( g_WatchBSP0_DumpLog )
				globalOutputStream() << "The build output will be saved in '" << junkpath << "'\n";
			Q_Exec( batpath, NULL, NULL, true, false );
		}
	}

	bsp_shutdown();
}

// =============================================================================
// Sys_ functions

void Sys_SetTitle( const char *text, bool modified ){
	auto title = StringStream( text );

	if ( modified ) {
		title << " *";
	}

	MainFrame_getWindow()->setWindowTitle( title.c_str() );
}

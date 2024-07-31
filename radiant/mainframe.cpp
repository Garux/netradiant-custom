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

//
// Main Window for Q3Radiant
//
// Leonardo Zide (leo@lokigames.com)
//

#include "mainframe.h"

#include "debugging/debugging.h"
#include "version.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "ifilter.h"
#include "itoolbar.h"
#include "editable.h"
#include "ientity.h"
#include "ishaders.h"
#include "igl.h"
#include "moduleobserver.h"

#include <ctime>

#include <QWidget>
#include <QSplashScreen>
#include <QCoreApplication>
#include <QMainWindow>
#include <QLabel>
#include <QSplitter>
#include <QMenuBar>
#include <QApplication>
#include <QToolBar>
#include <QStatusBar>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QSettings>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>

#include "commandlib.h"
#include "scenelib.h"
#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "os/path.h"
#include "os/file.h"
#include <glib.h>
#include "eclasslib.h"
#include "moduleobservers.h"

#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/menu.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/widget.h"
#include "gtkutil/guisettings.h"

#include "autosave.h"
#include "build.h"
#include "brushmanip.h"
#include "brushmodule.h"
#include "camwindow.h"
#include "csg.h"
#include "commands.h"
#include "console.h"
#include "entity.h"
#include "entityinspector.h"
#include "entitylist.h"
#include "filters.h"
#include "findtexturedialog.h"
#include "grid.h"
#include "groupdialog.h"
#include "gtkdlgs.h"
#include "gtkmisc.h"
#include "help.h"
#include "map.h"
#include "mru.h"
#include "patchdialog.h"
#include "patchmanip.h"
#include "plugin.h"
#include "pluginmanager.h"
#include "pluginmenu.h"
#include "plugintoolbar.h"
#include "points.h"
#include "preferences.h"
#include "qe3.h"
#include "qgl.h"
#include "select.h"
#include "server.h"
#include "surfacedialog.h"
#include "textures.h"
#include "texwindow.h"
#include "modelwindow.h"
#include "url.h"
#include "xywindow.h"
#include "windowobservers.h"
#include "renderstate.h"
#include "feedback.h"
#include "referencecache.h"

#include "colors.h"
#include "theme.h"
#include "tools.h"
#include "filterbar.h"


// VFS
class VFSModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	VFSModuleObserver() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			QE_InitVFS();
			GlobalFileSystem().initialise();
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			GlobalFileSystem().shutdown();
		}
	}
};

VFSModuleObserver g_VFSModuleObserver;

void VFS_Construct(){
	Radiant_attachHomePathsObserver( g_VFSModuleObserver );
}
void VFS_Destroy(){
	Radiant_detachHomePathsObserver( g_VFSModuleObserver );
}

// Home Paths

#ifdef WIN32
#include <shlobj.h>
#include <objbase.h>
const GUID qFOLDERID_SavedGames = {0x4C5C32FF, 0xBB9D, 0x43b0, {0xB5, 0xB4, 0x2D, 0x72, 0xE5, 0x4E, 0xAA, 0xA4}};
#define qREFKNOWNFOLDERID GUID
#define qKF_FLAG_CREATE 0x8000
#define qKF_FLAG_NO_ALIAS 0x1000
typedef HRESULT ( WINAPI qSHGetKnownFolderPath_t )( qREFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath );
static qSHGetKnownFolderPath_t *qSHGetKnownFolderPath;
#endif
void HomePaths_Realise(){
	do
	{
		const char* prefix = g_pGameDescription->getKeyValue( "prefix" );
		if ( !string_empty( prefix ) ) {
			StringOutputStream path( 256 );

#if defined( __APPLE__ )
			path( DirectoryCleaned( g_get_home_dir() ), "Library/Application Support", ( prefix + 1 ), '/' );
			if ( file_is_directory( path ) ) {
				g_qeglobals.m_userEnginePath = path;
				break;
			}
			path( DirectoryCleaned( g_get_home_dir() ), prefix, '/' );
#endif

#if defined( WIN32 )
			TCHAR mydocsdir[MAX_PATH + 1];
			wchar_t *mydocsdirw;
			HMODULE shfolder = LoadLibrary( "shfolder.dll" );
			if ( shfolder ) {
				qSHGetKnownFolderPath = (qSHGetKnownFolderPath_t *) GetProcAddress( shfolder, "SHGetKnownFolderPath" );
			}
			else{
				qSHGetKnownFolderPath = NULL;
			}
			CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
			if ( qSHGetKnownFolderPath && qSHGetKnownFolderPath( qFOLDERID_SavedGames, qKF_FLAG_CREATE | qKF_FLAG_NO_ALIAS, NULL, &mydocsdirw ) == S_OK ) {
				memset( mydocsdir, 0, sizeof( mydocsdir ) );
				wcstombs( mydocsdir, mydocsdirw, sizeof( mydocsdir ) - 1 );
				CoTaskMemFree( mydocsdirw );
				path( DirectoryCleaned( mydocsdir ), ( prefix + 1 ), '/' );
				if ( file_is_directory( path ) ) {
					g_qeglobals.m_userEnginePath = path;
					CoUninitialize();
					FreeLibrary( shfolder );
					break;
				}
			}
			CoUninitialize();
			if ( shfolder ) {
				FreeLibrary( shfolder );
			}
			if ( SUCCEEDED( SHGetFolderPath( NULL, CSIDL_PERSONAL, NULL, 0, mydocsdir ) ) ) {
				path( DirectoryCleaned( mydocsdir ), "My Games/", ( prefix + 1 ), '/' );
				// win32: only add it if it already exists
				if ( file_is_directory( path ) ) {
					g_qeglobals.m_userEnginePath = path;
					break;
				}
			}
#endif

#if defined( POSIX )
			path( DirectoryCleaned( g_get_home_dir() ), prefix, '/' );
			g_qeglobals.m_userEnginePath = path;
			break;
#endif
		}

		g_qeglobals.m_userEnginePath = EnginePath_get();
	}
	while ( 0 );

	Q_mkdir( g_qeglobals.m_userEnginePath.c_str() );

	g_qeglobals.m_userGamePath = StringStream( g_qeglobals.m_userEnginePath, gamename_get(), '/' );
	ASSERT_MESSAGE( !g_qeglobals.m_userGamePath.empty(), "HomePaths_Realise: user-game-path is empty" );
	Q_mkdir( g_qeglobals.m_userGamePath.c_str() );
}

ModuleObservers g_homePathObservers;

void Radiant_attachHomePathsObserver( ModuleObserver& observer ){
	g_homePathObservers.attach( observer );
}

void Radiant_detachHomePathsObserver( ModuleObserver& observer ){
	g_homePathObservers.detach( observer );
}

class HomePathsModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	HomePathsModuleObserver() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			HomePaths_Realise();
			g_homePathObservers.realise();
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			g_homePathObservers.unrealise();
		}
	}
};

HomePathsModuleObserver g_HomePathsModuleObserver;

void HomePaths_Construct(){
	Radiant_attachEnginePathObserver( g_HomePathsModuleObserver );
}
void HomePaths_Destroy(){
	Radiant_detachEnginePathObserver( g_HomePathsModuleObserver );
}


// Engine Path

CopiedString g_strEnginePath;
ModuleObservers g_enginePathObservers;
std::size_t g_enginepath_unrealised = 1;

void Radiant_attachEnginePathObserver( ModuleObserver& observer ){
	g_enginePathObservers.attach( observer );
}

void Radiant_detachEnginePathObserver( ModuleObserver& observer ){
	g_enginePathObservers.detach( observer );
}


void EnginePath_Realise(){
	if ( --g_enginepath_unrealised == 0 ) {
		g_enginePathObservers.realise();
	}
}


const char* EnginePath_get(){
	ASSERT_MESSAGE( g_enginepath_unrealised == 0, "EnginePath_get: engine path not realised" );
	return g_strEnginePath.c_str();
}

void EnginePath_Unrealise(){
	if ( ++g_enginepath_unrealised == 1 ) {
		g_enginePathObservers.unrealise();
	}
}

static CopiedString g_installedDevFilesPath; // track last engine path, where dev files installation occured, to prompt again when changed

static void installDevFiles(){
	if( !path_equal( g_strEnginePath.c_str(), g_installedDevFilesPath.c_str() ) ){
		ASSERT_MESSAGE( g_enginepath_unrealised != 0, "installDevFiles: engine path realised" );
		DoInstallDevFilesDlg( g_strEnginePath.c_str() );
		g_installedDevFilesPath = g_strEnginePath;
	}
}

void setEnginePath( CopiedString& self, const char* value ){
	const auto buffer = StringStream( DirectoryCleaned( value ) );
	if ( !path_equal( buffer, self.c_str() ) ) {
#if 0
		while ( !ConfirmModified( "Paths Changed" ) )
		{
			if ( Map_Unnamed( g_map ) ) {
				Map_SaveAs();
			}
			else
			{
				Map_Save();
			}
		}
		Map_RegionOff();
#endif

		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing Engine Path" );

		EnginePath_Unrealise();

		self = buffer;

		installDevFiles();

		EnginePath_Realise();
	}
}
typedef ReferenceCaller1<CopiedString, const char*, setEnginePath> EnginePathImportCaller;


// Extra Resource Path

std::array<CopiedString, 5> g_strExtraResourcePaths;

const std::array<CopiedString, 5>& ExtraResourcePaths_get(){
	return g_strExtraResourcePaths;
}


// App Path

CopiedString g_strAppPath;                 ///< holds the full path of the executable

const char* AppPath_get(){
	return g_strAppPath.c_str();
}

/// the path to the local rc-dir
const char* LocalRcPath_get(){
	static CopiedString rc_path;
	if ( rc_path.empty() ) {
		rc_path = StringStream( GlobalRadiant().getSettingsPath(), g_pGameDescription->mGameFile, '/' );
	}
	return rc_path.c_str();
}

/// directory for temp files
/// NOTE: on *nix this is were we check for .pid
CopiedString g_strSettingsPath;
const char* SettingsPath_get(){
	return g_strSettingsPath.c_str();
}


/*!
   points to the game tools directory, for instance
   C:/Program Files/Quake III Arena/GtkRadiant
   (or other games)
   this is one of the main variables that are configured by the game selection on startup
   [GameToolsPath]/plugins
   [GameToolsPath]/modules
   and also q3map, bspc
 */
CopiedString g_strGameToolsPath;           ///< this is set by g_GamesDialog

const char* GameToolsPath_get(){
	return g_strGameToolsPath.c_str();
}


void Paths_constructPreferences( PreferencesPage& page ){
	page.appendPathEntry( "Engine Path", true,
	                      StringImportCallback( EnginePathImportCaller( g_strEnginePath ) ),
	                      StringExportCallback( StringExportCaller( g_strEnginePath ) )
	                    );
}
void Paths_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Paths", "Path Settings" ) );
	Paths_constructPreferences( page );
	for( auto& extraPath : g_strExtraResourcePaths )
		page.appendPathEntry( "Extra Resource Path", true,
		                      StringImportCallback( EnginePathImportCaller( extraPath ) ),
		                      StringExportCallback( StringExportCaller( extraPath ) )
		                    );

}
void Paths_registerPreferencesPage(){
	PreferencesDialog_addGamePage( FreeCaller1<PreferenceGroup&, Paths_constructPage>() );
}


class PathsDialog : public Dialog
{
public:
	void BuildDialog() override {
		GetWidget()->setWindowTitle( "Engine Path Configuration" );

		auto vbox = new QVBoxLayout( GetWidget() );
		{
			auto frame = new QGroupBox( "Path settings" );
			vbox->addWidget( frame );

			auto grid = new QGridLayout( frame );
			grid->setAlignment( Qt::AlignmentFlag::AlignTop );
			grid->setColumnStretch( 0, 111 );
			grid->setColumnStretch( 1, 333 );
			{
				const char* engine;
#if defined( WIN32 )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_win32" );
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_linux" );
#elif defined( __APPLE__ )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_macos" );
#else
#error "unsupported platform"
#endif
				const auto text = StringStream( "Select directory, where game executable sits (typically ", makeQuoted( engine ), ")\n" );
				grid->addWidget( new QLabel( text.c_str() ), 0, 0, 1, 2 );
			}
			{
				PreferencesPage preferencesPage( *this, grid );
				Paths_constructPreferences( preferencesPage );
			}
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok );
			vbox->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, GetWidget(), &QDialog::accept );
		}
	}
};

PathsDialog g_PathsDialog;

static bool g_strEnginePath_was_empty_1st_start = false;

void EnginePath_verify(){
	if ( !file_exists( g_strEnginePath.c_str() ) || g_strEnginePath_was_empty_1st_start ) {
		g_installedDevFilesPath = ""; // trigger install for non existing engine path case
		g_PathsDialog.Create( nullptr );
		g_PathsDialog.DoModal();
		g_PathsDialog.Destroy();
	}
	installDevFiles(); // try this anytime, as engine path may be set via command line or -gamedetect
}

namespace
{
CopiedString g_gamename;
CopiedString g_gamemode;
ModuleObservers g_gameNameObservers;
ModuleObservers g_gameModeObservers;
}

void Radiant_attachGameNameObserver( ModuleObserver& observer ){
	g_gameNameObservers.attach( observer );
}

void Radiant_detachGameNameObserver( ModuleObserver& observer ){
	g_gameNameObservers.detach( observer );
}

const char* basegame_get(){
	return g_pGameDescription->getRequiredKeyValue( "basegame" );
}

const char* gamename_get(){
	if ( g_gamename.empty() ) {
		return basegame_get();
	}
	return g_gamename.c_str();
}

void gamename_set( const char* gamename ){
	if ( !string_equal( gamename, g_gamename.c_str() ) ) {
		g_gameNameObservers.unrealise();
		g_gamename = gamename;
		g_gameNameObservers.realise();
	}
}

void Radiant_attachGameModeObserver( ModuleObserver& observer ){
	g_gameModeObservers.attach( observer );
}

void Radiant_detachGameModeObserver( ModuleObserver& observer ){
	g_gameModeObservers.detach( observer );
}

const char* gamemode_get(){
	return g_gamemode.c_str();
}

void gamemode_set( const char* gamemode ){
	if ( !string_equal( gamemode, g_gamemode.c_str() ) ) {
		g_gameModeObservers.unrealise();
		g_gamemode = gamemode;
		g_gameModeObservers.realise();
	}
}

#include "os/dir.h"

class CLoadModule
{
	const char* m_path;
public:
	CLoadModule( const char* path ) : m_path( path ){
	}
	void operator()( const char* name ) const {
		char fullname[1024];
		ASSERT_MESSAGE( strlen( m_path ) + strlen( name ) < 1024, "" );
		strcpy( fullname, m_path );
		strcat( fullname, name );
		globalOutputStream() << "Found '" << fullname << "'\n";
		GlobalModuleServer_loadModule( fullname );
	}
};

const char* const c_library_extension =
#if defined( WIN32 )
    "dll"
#elif defined ( __APPLE__ )
    "dylib"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
    "so"
#endif
    ;

void Radiant_loadModules( const char* path ){
	Directory_forEach( path, MatchFileExtension<CLoadModule>( c_library_extension, CLoadModule( path ) ) );
}

void Radiant_loadModulesFromRoot( const char* directory ){
	Radiant_loadModules( StringStream( directory, g_pluginsDir ) );

	if ( !string_equal( g_pluginsDir, g_modulesDir ) ) {
		Radiant_loadModules( StringStream( directory, g_modulesDir ) );
	}
}


class WorldspawnColourEntityClassObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	WorldspawnColourEntityClassObserver() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			SetWorldspawnColour( g_xywindow_globals.color_brushes );
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
		}
	}
};

WorldspawnColourEntityClassObserver g_WorldspawnColourEntityClassObserver;


ModuleObservers g_gameToolsPathObservers;

void Radiant_attachGameToolsPathObserver( ModuleObserver& observer ){
	g_gameToolsPathObservers.attach( observer );
}

void Radiant_detachGameToolsPathObserver( ModuleObserver& observer ){
	g_gameToolsPathObservers.detach( observer );
}

void Radiant_Initialise(){
	GlobalModuleServer_Initialise();

	Radiant_loadModulesFromRoot( AppPath_get() );

	Preferences_Load();

	bool success = Radiant_Construct( GlobalModuleServer_get() );
	ASSERT_MESSAGE( success, "module system failed to initialise - see radiant.log for error messages" );

	g_gameToolsPathObservers.realise();
	g_gameModeObservers.realise();
	g_gameNameObservers.realise();
}

void Radiant_Shutdown(){
	g_gameNameObservers.unrealise();
	g_gameModeObservers.unrealise();
	g_gameToolsPathObservers.unrealise();

	if ( !g_preferences_globals.disable_ini ) {
		globalOutputStream() << "Start writing prefs\n";
		Preferences_Save();
		globalOutputStream() << "Done prefs\n";
	}

	Radiant_Destroy();

	GlobalModuleServer_Shutdown();
}

void Exit(){
	if ( ConfirmModified( "Exit Radiant" ) ) {
		QCoreApplication::quit();
	}
}

#include "environment.h"

#ifdef WIN32
#include <process.h>
#else
#include <spawn.h>
/* According to the Single Unix Specification, environ is not
 * in any system header, although unistd.h often declares it.
 */
extern char **environ;
#endif
void Radiant_Restart(){
	if( ConfirmModified( "Restart Radiant" ) ){
		const auto mapname = StringStream( makeQuoted( Map_Name( g_map ) ) );

		char *argv[] = { string_clone( environment_get_app_filepath() ),
	                     Map_Unnamed( g_map )? NULL : string_clone( mapname ),
	                     NULL };
#ifdef WIN32
		const int status = !_spawnv( P_NOWAIT, argv[0], argv );
#else
		const int status = posix_spawn( NULL, argv[0], NULL, NULL, argv, environ );
#endif

		// quit if radiant successfully started
		if ( status == 0 ) {
			QCoreApplication::quit();
		}
	}
}


void Restart(){
	PluginsMenu_clear();
	PluginToolbar_clear();

	Radiant_Shutdown();
	Radiant_Initialise();

	PluginsMenu_populate();

	PluginToolbar_populate();
}


void OpenUpdateURL(){
	OpenURL( "https://github.com/Garux/netradiant-custom/releases/latest" );
#if 0
	// build the URL
	StringOutputStream URL( 256 );
	URL << "http://www.icculus.org/netradiant/?cmd=update&data=dlupdate&query_dlup=1";
#ifdef WIN32
	URL << "&OS_dlup=1";
#elif defined( __APPLE__ )
	URL << "&OS_dlup=2";
#else
	URL << "&OS_dlup=3";
#endif
	URL << "&Version_dlup=" RADIANT_VERSION;
	g_GamesDialog.AddPacksURL( URL );
	OpenURL( URL );
#endif
}

// open the Q3Rad manual
void OpenHelpURL(){
	// at least on win32, AppPath + "docs/index.html"
	OpenURL( StringStream( AppPath_get(), "docs/index.html" ) );
}

void OpenBugReportURL(){
	// OpenURL( "http://www.icculus.org/netradiant/?cmd=bugs" );
	OpenURL( "https://github.com/Garux/netradiant-custom/issues" );
}


QWidget* g_page_console;

void Console_ToggleShow(){
	GroupDialog_showPage( g_page_console );
}

QWidget* g_page_entity;

void EntityInspector_ToggleShow(){
	GroupDialog_showPage( g_page_entity );
}

QWidget* g_page_models;

void ModelBrowser_ToggleShow(){
	GroupDialog_showPage( g_page_models );
}


static class EverySecondTimer
{
	QTimer m_timer;
public:
	EverySecondTimer(){
		m_timer.setInterval( 1000 );
		m_timer.callOnTimeout( [](){
			if ( QGuiApplication::mouseButtons().testFlag( Qt::MouseButton::NoButton ) ) {
				QE_CheckAutoSave();
			}
		} );
	}
	void enable(){
		m_timer.start();
	}
	void disable(){
		m_timer.stop();
	}
}
s_qe_every_second_timer;


class WaitDialog
{
public:
	QWidget* m_window;
	QLabel* m_label;
};

WaitDialog create_wait_dialog( const char* title, const char* text ){
	/* Qt::Tool window type doesn't steal focus, which saves e.g. from losing freelook camera mode on autosave
	   or entity menu from hiding, while clicked with ctrl, by tex/model loading popup.
	   Qt::WidgetAttribute::WA_ShowWithoutActivating is implied, but lets have it set too. */
	auto window = new QWidget( MainFrame_getWindow(), Qt::Tool | Qt::WindowTitleHint );
	window->setWindowTitle( title );
	window->setWindowModality( Qt::WindowModality::ApplicationModal );
	window->setAttribute( Qt::WidgetAttribute::WA_ShowWithoutActivating );

	auto label = new QLabel( text );
	{
		auto box = new QHBoxLayout( window );
		box->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		box->setContentsMargins( 20, 5, 20, 3 );
		box->addWidget( label );
		label->setMinimumWidth( 200 );
	}
	return WaitDialog{ window, label };
}

namespace
{
clock_t g_lastRedrawTime = 0;
const clock_t c_redrawInterval = clock_t( CLOCKS_PER_SEC / 10 );

bool redrawRequired(){
	clock_t currentTime = std::clock();
	if ( currentTime - g_lastRedrawTime >= c_redrawInterval ) {
		g_lastRedrawTime = currentTime;
		return true;
	}
	return false;
}
}

typedef std::list<CopiedString> StringStack;
StringStack g_wait_stack;
WaitDialog g_wait;

bool ScreenUpdates_Enabled(){
	return g_wait_stack.empty();
}

void ScreenUpdates_process(){
	if ( redrawRequired() ) {
		process_gui();
	}
}


void ScreenUpdates_Disable( const char* message, const char* title ){
	if ( g_wait_stack.empty() ) {
		s_qe_every_second_timer.disable();

		process_gui();

		g_wait = create_wait_dialog( title, message );

		g_wait.m_window->show();
		ScreenUpdates_process();
	}
	else {
		g_wait.m_window->setWindowTitle( title );
		g_wait.m_label->setText( message );
		ScreenUpdates_process();
	}
	g_wait_stack.push_back( message );
}

void ScreenUpdates_Enable(){
	ASSERT_MESSAGE( !ScreenUpdates_Enabled(), "screen updates already enabled" );
	g_wait_stack.pop_back();
	if ( g_wait_stack.empty() ) {
		s_qe_every_second_timer.enable();

		delete std::exchange( g_wait.m_window, nullptr );
	}
	else {
		g_wait.m_label->setText( g_wait_stack.back().c_str() );
		ScreenUpdates_process();
	}
}



void GlobalCamera_UpdateWindow(){
	if ( g_pParentWnd != 0 ) {
		CamWnd_Update( *g_pParentWnd->GetCamWnd() );
	}
}

void XY_UpdateAllWindows(){
	if ( g_pParentWnd != 0 ) {
		g_pParentWnd->forEachXYWnd( []( XYWnd* xywnd ){
			XYWnd_Update( *xywnd );
		} );
	}
}

void UpdateAllWindows(){
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}


LatchedInt g_Layout_viewStyle( 0, "Window Layout" );
LatchedBool g_Layout_enableDetachableMenus( true, "Detachable Menus" );
LatchedBool g_Layout_builtInGroupDialog( false, "Built-In Group Dialog" );



void create_file_menu( QMenuBar *menubar ){
	// File menu
	QMenu *menu = menubar->addMenu( "&File" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&New Map", "NewMap" );
	menu->addSeparator();

	create_menu_item_with_mnemonic( menu, "&Open...", "OpenMap" );
	create_menu_item_with_mnemonic( menu, "&Import...", "ImportMap" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Save", "SaveMap" );
	create_menu_item_with_mnemonic( menu, "Save &as...", "SaveMapAs" );
	create_menu_item_with_mnemonic( menu, "Save s&elected...", "SaveSelected" );
	create_menu_item_with_mnemonic( menu, "Save re&gion...", "SaveRegion" );
	menu->addSeparator();
//	create_menu_item_with_mnemonic( menu, "&Refresh models", "RefreshReferences" );
//	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Pointfile", "TogglePointfile" );
	menu->addSeparator();
	MRU_constructMenu( menu );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "E&xit", "Exit" );
}

void create_edit_menu( QMenuBar *menubar ){
	// Edit menu
	QMenu *menu = menubar->addMenu( "&Edit" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&Undo", "Undo" );
	create_menu_item_with_mnemonic( menu, "&Redo", "Redo" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Copy", "Copy" );
	create_menu_item_with_mnemonic( menu, "&Paste", "Paste" );
	create_menu_item_with_mnemonic( menu, "P&aste To Camera", "PasteToCamera" );
	create_menu_item_with_mnemonic( menu, "Move To Camera", "MoveToCamera" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Duplicate", "CloneSelection" );
	create_menu_item_with_mnemonic( menu, "Duplicate, make uni&que", "CloneSelectionAndMakeUnique" );
	create_menu_item_with_mnemonic( menu, "D&elete", "DeleteSelection" );
	//create_menu_item_with_mnemonic( menu, "Pa&rent", "ParentSelection" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "C&lear Selection", "UnSelectSelection" );
	create_menu_item_with_mnemonic( menu, "&Invert Selection", "InvertSelection" );
	create_menu_item_with_mnemonic( menu, "Select i&nside", "SelectInside" );
	create_menu_item_with_mnemonic( menu, "Select &touching", "SelectTouching" );

	menu->addSeparator();

	create_menu_item_with_mnemonic( menu, "Select All Of Type", "SelectAllOfType" );
	create_menu_item_with_mnemonic( menu, "Select Textured", "SelectTextured" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Primitives", "ExpandSelectionToPrimitives" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Entities", "ExpandSelectionToEntities" );
	create_menu_item_with_mnemonic( menu, "Select Connected Entities", "SelectConnectedEntities" );

	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Shortcuts...", FreeCaller<DoCommandListDlg>() );
	create_menu_item_with_mnemonic( menu, "Pre&ferences...", "Preferences" );
}

void create_view_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	// View menu
	QMenu *menu = menubar->addMenu( "Vie&w" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	if ( style == MainFrame::eFloating ) {
		create_check_menu_item_with_mnemonic( menu, "Camera View", "ToggleCamera" );
		create_check_menu_item_with_mnemonic( menu, "XY (Top) View", "ToggleView" );
		create_check_menu_item_with_mnemonic( menu, "XZ (Front) View", "ToggleFrontView" );
		create_check_menu_item_with_mnemonic( menu, "YZ (Side) View", "ToggleSideView" );
	}
	if ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) {
		create_menu_item_with_mnemonic( menu, "Console", "ToggleConsole" );
	}
	if ( ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) || g_Layout_builtInGroupDialog.m_value ) {
		create_menu_item_with_mnemonic( menu, "Texture Browser", "ToggleTextures" );
	}
	create_menu_item_with_mnemonic( menu, "Model Browser", "ToggleModelBrowser" );
	create_menu_item_with_mnemonic( menu, "Entity Inspector", "ToggleEntityInspector" );
	create_menu_item_with_mnemonic( menu, "&Surface Inspector", "SurfaceInspector" );
	create_menu_item_with_mnemonic( menu, "Entity List", "EntityList" );

	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Camera" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Focus on Selected", "CameraFocusOnSelected" );
		create_menu_item_with_mnemonic( submenu, "&Center", "CenterView" );
		create_menu_item_with_mnemonic( submenu, "&Up Floor", "UpFloor" );
		create_menu_item_with_mnemonic( submenu, "&Down Floor", "DownFloor" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Far Clip Plane In", "CubicClipZoomIn" );
		create_menu_item_with_mnemonic( submenu, "Far Clip Plane Out", "CubicClipZoomOut" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Next leak spot", "NextLeakSpot" );
		create_menu_item_with_mnemonic( submenu, "Previous leak spot", "PrevLeakSpot" );
		//cameramodel is not implemented in instances, thus useless
//		submenu->addSeparator();
//		create_menu_item_with_mnemonic( submenu, "Look Through Selected", "LookThroughSelected" );
//		create_menu_item_with_mnemonic( submenu, "Look Through Camera", "LookThroughCamera" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Orthographic" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		if ( style == MainFrame::eRegular || style == MainFrame::eRegularLeft || style == MainFrame::eFloating ) {
			create_menu_item_with_mnemonic( submenu, "&Next (XY, XZ, YZ)", "NextView" );
			create_menu_item_with_mnemonic( submenu, "XY (Top)", "ViewTop" );
			create_menu_item_with_mnemonic( submenu, "XZ (Front)", "ViewFront" );
			create_menu_item_with_mnemonic( submenu, "YZ (Side)", "ViewSide" );
			submenu->addSeparator();
		}
		else{
			create_menu_item_with_mnemonic( submenu, "Center on Selected", "NextView" );
		}

		create_menu_item_with_mnemonic( submenu, "Focus on Selected", "XYFocusOnSelected" );
		create_menu_item_with_mnemonic( submenu, "Center on Selected", "CenterXYView" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "&XY 100%", "Zoom100" );
		create_menu_item_with_mnemonic( submenu, "XY Zoom &In", "ZoomIn" );
		create_menu_item_with_mnemonic( submenu, "XY Zoom &Out", "ZoomOut" );
	}

	menu->addSeparator();

	{
		QMenu* submenu = menu->addMenu( "Show" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_check_menu_item_with_mnemonic( submenu, "Show Entity &Angles", "ShowAngles" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity &Names", "ShowNames" );
		create_check_menu_item_with_mnemonic( submenu, "Show Light Radiuses", "ShowLightRadiuses" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity Boxes", "ShowBboxes" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity Connections", "ShowConnections" );

		submenu->addSeparator();

		create_check_menu_item_with_mnemonic( submenu, "Show 2D Size Info", "ShowSize2d" );
		create_check_menu_item_with_mnemonic( submenu, "Show 3D Size Info", "ShowSize3d" );
		create_check_menu_item_with_mnemonic( submenu, "Show Crosshair", "ToggleCrosshairs" );
		create_check_menu_item_with_mnemonic( submenu, "Show Grid", "ToggleGrid" );
		create_check_menu_item_with_mnemonic( submenu, "Show Blocks", "ShowBlocks" );
		create_check_menu_item_with_mnemonic( submenu, "Show C&oordinates", "ShowCoordinates" );
		create_check_menu_item_with_mnemonic( submenu, "Show Window Outline", "ShowWindowOutline" );
		create_check_menu_item_with_mnemonic( submenu, "Show Axes", "ShowAxes" );
		create_check_menu_item_with_mnemonic( submenu, "Show 2D Workzone", "ShowWorkzone2d" );
		create_check_menu_item_with_mnemonic( submenu, "Show 3D Workzone", "ShowWorkzone3d" );
		create_check_menu_item_with_mnemonic( submenu, "Show Renderer Stats", "ShowStats" );
	}

	{
		QMenu* submenu = menu->addMenu( "Filter" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		Filters_constructMenu( submenu );
	}
	menu->addSeparator();
	{
		create_check_menu_item_with_mnemonic( menu, "Hide Selected", "HideSelected" );
		create_menu_item_with_mnemonic( menu, "Show Hidden", "ShowHidden" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Region" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "&Off", "RegionOff" );
		create_menu_item_with_mnemonic( submenu, "&Set XY", "RegionSetXY" );
		create_menu_item_with_mnemonic( submenu, "Set &Brush", "RegionSetBrush" );
		create_check_menu_item_with_mnemonic( submenu, "Set Se&lection", "RegionSetSelection" );
	}

	//command_connect_accelerator( "CenterXYView" );
}

void create_selection_menu( QMenuBar *menubar ){
	// Selection menu
	QMenu *menu = menubar->addMenu( "M&odify" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
	{
		QMenu* submenu = menu->addMenu( "Components" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_check_menu_item_with_mnemonic( submenu, "&Edges", "DragEdges" );
		create_check_menu_item_with_mnemonic( submenu, "&Vertices", "DragVertices" );
		create_check_menu_item_with_mnemonic( submenu, "&Faces", "DragFaces" );
	}

	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Snap To Grid", "SnapToGrid" );

	menu->addSeparator();

	{
		QMenu* submenu = menu->addMenu( "Nudge" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Nudge Left", "SelectNudgeLeft" );
		create_menu_item_with_mnemonic( submenu, "Nudge Right", "SelectNudgeRight" );
		create_menu_item_with_mnemonic( submenu, "Nudge Up", "SelectNudgeUp" );
		create_menu_item_with_mnemonic( submenu, "Nudge Down", "SelectNudgeDown" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Nudge +Z", "MoveSelectionUP" );
		create_menu_item_with_mnemonic( submenu, "Nudge -Z", "MoveSelectionDOWN" );
	}
	{
		QMenu* submenu = menu->addMenu( "Rotate" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Rotate X", "RotateSelectionX" );
		create_menu_item_with_mnemonic( submenu, "Rotate Y", "RotateSelectionY" );
		create_menu_item_with_mnemonic( submenu, "Rotate Z", "RotateSelectionZ" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Rotate Clockwise", "RotateSelectionClockwise" );
		create_menu_item_with_mnemonic( submenu, "Rotate Anticlockwise", "RotateSelectionAnticlockwise" );
	}
	{
		QMenu* submenu = menu->addMenu( "Flip" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Flip &X", "MirrorSelectionX" );
		create_menu_item_with_mnemonic( submenu, "Flip &Y", "MirrorSelectionY" );
		create_menu_item_with_mnemonic( submenu, "Flip &Z", "MirrorSelectionZ" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Flip Horizontally", "MirrorSelectionHorizontally" );
		create_menu_item_with_mnemonic( submenu, "Flip Vertically", "MirrorSelectionVertically" );
	}
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Arbitrary rotation...", "ArbitraryRotation" );
	create_menu_item_with_mnemonic( menu, "Arbitrary scale...", "ArbitraryScale" );
	create_menu_item_with_mnemonic( menu, "Repeat Transforms", "RepeatTransforms" );
}

void create_bsp_menu( QMenuBar *menubar ){
	// BSP menu
	QMenu *menu = menubar->addMenu( "&Build" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Customize...", "BuildMenuCustomize" );
	create_menu_item_with_mnemonic( menu, "Run recent build", "Build_runRecentExecutedBuild" );

	menu->addSeparator();

	menu->setToolTipsVisible( true );
	Build_constructMenu( menu );

	g_bsp_menu = menu;
}

void create_grid_menu( QMenuBar *menubar ){
	// Grid menu
	QMenu *menu = menubar->addMenu( "&Grid" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Grid_constructMenu( menu );
}

void create_misc_menu( QMenuBar *menubar ){
	// Misc menu
	QMenu *menu = menubar->addMenu( "M&isc" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
#if 0
	create_menu_item_with_mnemonic( menu, "&Benchmark", FreeCaller<GlobalCamera_Benchmark>() );
#endif
	create_colours_menu( menu );

	create_menu_item_with_mnemonic( menu, "Find brush...", "FindBrush" );
	create_menu_item_with_mnemonic( menu, "Map Info...", "MapInfo" );
	create_menu_item_with_mnemonic( menu, "Set 2D &Background image...", FreeCaller<WXY_SetBackgroundImage>() );
	create_menu_item_with_mnemonic( menu, "Fullscreen", "Fullscreen" );
	create_menu_item_with_mnemonic( menu, "Maximize view", "MaximizeView" );
}

void create_entity_menu( QMenuBar *menubar ){
	// Entity menu
	QMenu *menu = menubar->addMenu( "E&ntity" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Entity_constructMenu( menu );
}

void create_brush_menu( QMenuBar *menubar ){
	// Brush menu
	QMenu *menu = menubar->addMenu( "B&rush" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Brush_constructMenu( menu );
}

void create_patch_menu( QMenuBar *menubar ){
	// Curve menu
	QMenu *menu = menubar->addMenu( "&Curve" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Patch_constructMenu( menu );
}

void create_help_menu( QMenuBar *menubar ){
	// Help menu
	QMenu *menu = menubar->addMenu( "&Help" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

//	create_menu_item_with_mnemonic( menu, "Manual", "OpenManual" );

	// this creates all the per-game drop downs for the game pack helps
	// it will take care of hooking the Sys_OpenURL calls etc.
	create_game_help_menu( menu );

	create_menu_item_with_mnemonic( menu, "Bug report", FreeCaller<OpenBugReportURL>() );
	create_menu_item_with_mnemonic( menu, "Check for NetRadiant update (web)", "CheckForUpdate" ); // FIXME
	create_menu_item_with_mnemonic( menu, "&About", FreeCaller<DoAbout>() );
}

void create_main_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	create_file_menu( menubar );
 	create_edit_menu( menubar );
	create_view_menu( menubar, style );
	create_selection_menu( menubar );
	create_bsp_menu( menubar );
	create_grid_menu( menubar );
	create_misc_menu( menubar );
	create_entity_menu( menubar );
	create_brush_menu( menubar );
	create_patch_menu( menubar );
	create_plugins_menu( menubar );
	create_help_menu( menubar );
}


void Patch_registerShortcuts(){
	command_connect_accelerator( "InvertCurveTextureX" );
	command_connect_accelerator( "InvertCurveTextureY" );
	command_connect_accelerator( "PatchInsertInsertColumn" );
	command_connect_accelerator( "PatchInsertInsertRow" );
	command_connect_accelerator( "PatchDeleteLastColumn" );
	command_connect_accelerator( "PatchDeleteLastRow" );
	command_connect_accelerator( "NaturalizePatch" );
}

void Manipulators_registerShortcuts(){
	command_connect_accelerator( "MouseRotateOrScale" );
	command_connect_accelerator( "MouseDragOrTransform" );
}

void TexdefNudge_registerShortcuts(){
	command_connect_accelerator( "TexRotateClock" );
	command_connect_accelerator( "TexRotateCounter" );
	command_connect_accelerator( "TexScaleUp" );
	command_connect_accelerator( "TexScaleDown" );
	command_connect_accelerator( "TexScaleLeft" );
	command_connect_accelerator( "TexScaleRight" );
	command_connect_accelerator( "TexShiftUp" );
	command_connect_accelerator( "TexShiftDown" );
	command_connect_accelerator( "TexShiftLeft" );
	command_connect_accelerator( "TexShiftRight" );
}

void SelectNudge_registerShortcuts(){
	command_connect_accelerator( "MoveSelectionDOWN" );
	command_connect_accelerator( "MoveSelectionUP" );
	command_connect_accelerator( "SelectNudgeLeft" );
	command_connect_accelerator( "SelectNudgeRight" );
	command_connect_accelerator( "SelectNudgeUp" );
	command_connect_accelerator( "SelectNudgeDown" );
}

void SnapToGrid_registerShortcuts(){
	command_connect_accelerator( "SnapToGrid" );
}

void SelectByType_registerShortcuts(){
	command_connect_accelerator( "SelectAllOfType" );
}

void SurfaceInspector_registerShortcuts(){
	command_connect_accelerator( "FitTexture" );
	command_connect_accelerator( "FitTextureWidth" );
	command_connect_accelerator( "FitTextureHeight" );
	command_connect_accelerator( "FitTextureWidthOnly" );
	command_connect_accelerator( "FitTextureHeightOnly" );
	command_connect_accelerator( "TextureProjectAxial" );
	command_connect_accelerator( "TextureProjectOrtho" );
	command_connect_accelerator( "TextureProjectCam" );
}

void TexBro_registerShortcuts(){
	toggle_add_accelerator( "SearchFromStart" );
}

void Misc_registerShortcuts(){
	command_connect_accelerator( "Redo2" );
	command_connect_accelerator( "UnSelectSelection2" );
	command_connect_accelerator( "DeleteSelection2" );

}


void register_shortcuts(){
//	Patch_registerShortcuts();
	Grid_registerShortcuts();
//	XYWnd_registerShortcuts();
	CamWnd_registerShortcuts();
	Manipulators_registerShortcuts();
	SurfaceInspector_registerShortcuts();
	TexdefNudge_registerShortcuts();
//	SelectNudge_registerShortcuts();
//	SnapToGrid_registerShortcuts();
//	SelectByType_registerShortcuts();
	TexBro_registerShortcuts();
	Misc_registerShortcuts();
	Entity_registerShortcuts();
}

void File_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Open an existing map", "file_open.png", "OpenMap" );
	toolbar_append_button( toolbar, "Save the active map", "file_save.png", "SaveMap" );
}

void UndoRedo_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Undo", "undo.png", "Undo" );
	toolbar_append_button( toolbar, "Redo", "redo.png", "Redo" );
}

void RotateFlip_constructToolbar( QToolBar* toolbar ){
//	toolbar_append_button( toolbar, "x-axis Flip", "brush_flipx.png", "MirrorSelectionX" );
//	toolbar_append_button( toolbar, "x-axis Rotate", "brush_rotatex.png", "RotateSelectionX" );
//	toolbar_append_button( toolbar, "y-axis Flip", "brush_flipy.png", "MirrorSelectionY" );
//	toolbar_append_button( toolbar, "y-axis Rotate", "brush_rotatey.png", "RotateSelectionY" );
//	toolbar_append_button( toolbar, "z-axis Flip", "brush_flipz.png", "MirrorSelectionZ" );
//	toolbar_append_button( toolbar, "z-axis Rotate", "brush_rotatez.png", "RotateSelectionZ" );
	toolbar_append_button( toolbar, "Flip Horizontally", "brush_flip_hor.png", "MirrorSelectionHorizontally" );
	toolbar_append_button( toolbar, "Flip Vertically", "brush_flip_vert.png", "MirrorSelectionVertically" );

	toolbar_append_button( toolbar, "Rotate Anticlockwise", "brush_rotate_anti.png", "RotateSelectionAnticlockwise" );
	toolbar_append_button( toolbar, "Rotate Clockwise", "brush_rotate_clock.png", "RotateSelectionClockwise" );
}

void Select_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Select touching", "selection_selecttouching.png", "SelectTouching" );
	toolbar_append_button( toolbar, "Select inside", "selection_selectinside.png", "SelectInside" );
}

void CSG_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "CSG Subtract", "selection_csgsubtract.png", "CSGSubtract" );
	toolbar_append_button( toolbar, "CSG Wrap Merge", "selection_csgmerge.png", "CSGWrapMerge" );
	toolbar_append_button( toolbar, "Room", "selection_makeroom.png", "CSGroom" );
	toolbar_append_button( toolbar, "CSG Tool", "ellipsis.png", "CSGTool" );
}

void ComponentModes_constructToolbar( QToolBar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Select Vertices", "modify_vertices.png", "DragVertices" );
	toolbar_append_toggle_button( toolbar, "Select Edges", "modify_edges.png", "DragEdges" );
	toolbar_append_toggle_button( toolbar, "Select Faces", "modify_faces.png", "DragFaces" );
}

void XYWnd_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Change views", "view_change.png", "NextView" );
}

void Manipulators_constructToolbar( QToolBar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Translate", "select_mousetranslate.png", "MouseTranslate" );
	toolbar_append_toggle_button( toolbar, "Rotate", "select_mouserotate.png", "MouseRotate" );
	toolbar_append_toggle_button( toolbar, "Scale", "select_mousescale.png", "MouseScale" );
	toolbar_append_toggle_button( toolbar, "Transform (Q)", "select_mousetransform.png", "MouseTransform" ); // hardcoded shortcut tip of "MouseDragOrTransform"...
	toolbar_append_toggle_button( toolbar, "Resize (Q)", "select_mouseresize.png", "MouseDrag" ); // hardcoded shortcut tip of "MouseDragOrTransform"...
	toolbar_append_toggle_button( toolbar, "Clipper", "select_clipper.png", "ToggleClipper" );
//	toolbar_append_toggle_button( toolbar, "Build", "select_mouserotate.png", "MouseBuild" );
	toolbar_append_toggle_button( toolbar, "UV Tool", "select_mouseuv.png", "MouseUV" );
}

void create_main_toolbar( QToolBar *toolbar,  MainFrame::EViewStyle style ){
 	File_constructToolbar( toolbar );
	toolbar->addSeparator();

	UndoRedo_constructToolbar( toolbar );
	toolbar->addSeparator();

	RotateFlip_constructToolbar( toolbar );
	toolbar->addSeparator();

	Select_constructToolbar( toolbar );
	toolbar->addSeparator();

	CSG_constructToolbar( toolbar );
	toolbar->addSeparator();

	ComponentModes_constructToolbar( toolbar );
	toolbar->addSeparator();

	if ( style != MainFrame::eSplit ) {
		XYWnd_constructToolbar( toolbar );
		toolbar->addSeparator();
	}

	CamWnd_constructToolbar( toolbar );
	toolbar->addSeparator();

	Manipulators_constructToolbar( toolbar );
	toolbar->addSeparator();

	if ( !string_equal( g_pGameDescription->getKeyValue( "no_patch" ), "1" ) ) {
		Patch_constructToolbar( toolbar );
		toolbar->addSeparator();
	}

	toolbar_append_toggle_button( toolbar, "Texture Lock", "texture_lock.png", "TogTexLock" );
	toolbar_append_toggle_button( toolbar, "Texture Vertex Lock", "texture_vertexlock.png", "TogTexVertexLock" );
 	toolbar->addSeparator();

	toolbar_append_button( toolbar, "Entities", "entities.png", "ToggleEntityInspector" );
	// disable the console and texture button in the regular layouts
	if ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) {
		toolbar_append_button( toolbar, "Console", "console.png", "ToggleConsole" );
	}
	if ( ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) || g_Layout_builtInGroupDialog.m_value ) {
		toolbar_append_button( toolbar, "Texture Browser", "texture_browser.png", "ToggleTextures" );
	}

	// TODO: call light inspector
	//QAction* g_view_lightinspector_button = toolbar_append_button(toolbar, "Light Inspector", "lightinspector.png", "ToggleLightInspector");

	toolbar->addSeparator();
	toolbar_append_button( toolbar, "Refresh Models", "refresh_models.png", "RefreshReferences" );
}


void create_main_statusbar( QStatusBar *statusbar, QLabel *pStatusLabel[c_status__count] ){
	statusbar->setSizeGripEnabled( false );
	{
		QLabel *label = new QLabel;
		statusbar->addPermanentWidget( label, 1 );
		pStatusLabel[c_status_command] = label;
	}

	for ( int i = 1; i < c_status__count; ++i )
	{
		if( i == c_status_brushcount ){
			QWidget *widget = new QWidget;
			QHBoxLayout *hbox = new QHBoxLayout( widget );
			hbox->setMargin( 0 );
			statusbar->addPermanentWidget( widget, 0 );
			const char* imgs[3] = { "status_brush.png", "patch_wireframe.png", "status_entity.png" };
			for( ; i < c_status_brushcount + 3; ++i ){
				QLabel *label = new QLabel();
				label->setPixmap( new_local_image( imgs[i - c_status_brushcount] ) );
				hbox->addWidget( label );

				label = new QLabel();
				label->setMinimumWidth( label->fontMetrics().horizontalAdvance( "99999" ) );
				hbox->addWidget( label );
				pStatusLabel[i] = label;
			}
			--i;
		}
		else{
			QLabel *label = new QLabel;
			if( i == c_status_grid ){
				statusbar->addPermanentWidget( label, 0 );
				label->setToolTip( " <b>G</b>: <u>G</u>rid size<br> <b>F</b>: map <u>F</u>ormat<br> <b>C</b>: camera <u>C</u>lip distance <br> <b>L</b>: texture <u>L</u>ock" );
			}
			else
				statusbar->addPermanentWidget( label, 1 );
			pStatusLabel[i] = label;
		}
	}
}

SignalHandlerId XYWindowDestroyed_connect( const SignalHandler& handler ){
	return g_pParentWnd->GetXYWnd()->onDestroyed.connectFirst( handler );
}

void XYWindowDestroyed_disconnect( SignalHandlerId id ){
	g_pParentWnd->GetXYWnd()->onDestroyed.disconnect( id );
}

MouseEventHandlerId XYWindowMouseDown_connect( const MouseEventHandler& handler ){
	return g_pParentWnd->GetXYWnd()->onMouseDown.connectFirst( handler );
}

void XYWindowMouseDown_disconnect( MouseEventHandlerId id ){
	g_pParentWnd->GetXYWnd()->onMouseDown.disconnect( id );
}

// =============================================================================
// MainFrame class

MainFrame* g_pParentWnd = 0;

QWidget* MainFrame_getWindow(){
	return g_pParentWnd == 0? 0 : g_pParentWnd->m_window;
}

MainFrame::MainFrame() : m_idleRedrawStatusText( RedrawStatusTextCaller( *this ) ){
	Create();
}

MainFrame::~MainFrame(){
	SaveGuiState();

	m_window->hide(); // hide to avoid resize events during content deletion

	Shutdown();

	delete m_window;
}

void MainFrame::SetActiveXY( XYWnd* p ){
	if ( m_pActiveXY ) {
		m_pActiveXY->SetActive( false );
	}

	m_pActiveXY = p;

	if ( m_pActiveXY ) {
		m_pActiveXY->SetActive( true );
	}

}

#ifdef WIN32
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif
void MainFrame_toggleFullscreen(){
	QWidget *w = MainFrame_getWindow();
#ifdef WIN32 // https://doc.qt.io/qt-5.15/windows-issues.html#fullscreen-opengl-based-windows
	QWindowsWindowFunctions::setHasBorderInFullScreen( w->windowHandle(), true );
#endif
	w->setWindowState( w->windowState() ^ Qt::WindowState::WindowFullScreen );
}

class MaximizeView
{
	bool m_maximized{};
	QList<int> m_vSplitSizes;
	QList<int> m_vSplit2Sizes;
	QList<int> m_hSplitSizes;

	void maximize(){
		m_maximized = true;
		m_vSplitSizes = g_pParentWnd->m_vSplit->sizes();
		m_vSplit2Sizes = g_pParentWnd->m_vSplit2->sizes();
		m_hSplitSizes = g_pParentWnd->m_hSplit->sizes();

		const QPoint cursor = g_pParentWnd->m_hSplit->mapFromGlobal( QCursor::pos() );

		if( cursor.y() < m_vSplitSizes[0] )
			g_pParentWnd->m_vSplit->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_vSplit->setSizes( { 0, 9999 } );

		if( cursor.y() < m_vSplit2Sizes[0] )
			g_pParentWnd->m_vSplit2->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_vSplit2->setSizes( { 0, 9999 } );

		if( cursor.x() < m_hSplitSizes[0] )
			g_pParentWnd->m_hSplit->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_hSplit->setSizes( { 0, 9999 } );
	}
public:
	void unmaximize(){
		if( m_maximized ){
			m_maximized = false;
			g_pParentWnd->m_vSplit->setSizes( m_vSplitSizes );
			g_pParentWnd->m_vSplit2->setSizes( m_vSplit2Sizes );
			g_pParentWnd->m_hSplit->setSizes( m_hSplitSizes );
		}
	}
	void toggle(){
		return m_maximized ? unmaximize() : maximize();
	}
};

MaximizeView g_maximizeview;

void Maximize_View(){
	if( g_pParentWnd != 0 && g_pParentWnd->m_vSplit != 0 && g_pParentWnd->m_vSplit2 != 0 && g_pParentWnd->m_hSplit != 0 )
		g_maximizeview.toggle();
}



class RadiantQMainWindow : public QMainWindow
{
protected:
	void closeEvent( QCloseEvent *event ) override {
		event->ignore();
		Exit();
	}
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride && !QGuiApplication::mouseButtons().testFlag( Qt::MouseButton::NoButton ) ){
			event->accept(); // block shortcuts while mouse buttons are pressed
		}
		return QMainWindow::event( event );
	}
};


QSplashScreen *create_splash(){
	QSplashScreen *splash = new QSplashScreen( new_local_image( "splash.png" ) );
	splash->show();
	return splash;
}

static QSplashScreen *splash_screen = 0;

void show_splash(){
	splash_screen = create_splash();

	process_gui();
}

void hide_splash(){
//.	splash_screen->finish();
	delete splash_screen;
}


void user_shortcuts_init(){
	const auto path = StringStream( SettingsPath_get(), g_pGameDescription->mGameFile, '/' );
	LoadCommandMap( path );
	SaveCommandMap( path );
}

void user_shortcuts_save(){
	const auto path = StringStream( SettingsPath_get(), g_pGameDescription->mGameFile, '/' );
	SaveCommandMap( path );
}


void MainFrame::Create(){
	QMainWindow *window = m_window = new RadiantQMainWindow();

	GlobalWindowObservers_connectTopLevel( window );

	QApplication::setWindowIcon( new_local_icon( "radiant.ico" ) );

	/* GlobalCommands_insert plugins commands */
	GetPlugInMgr().Init( window );
	/* then load shortcuts cfg */
	user_shortcuts_init();

	GlobalPressedKeys_connect( window );
	GlobalShortcuts_setWidget( window );
	register_shortcuts();

	m_nCurrentStyle = (EViewStyle)g_Layout_viewStyle.m_value;

	create_main_menu( window->menuBar(), CurrentStyle() );

	{
		{
			QToolBar *toolbar = window->addToolBar( "Main Toolbar" );
			toolbar->setObjectName( "Main Toolbar" );
			create_main_toolbar( toolbar, CurrentStyle() );
		}
		{
			QToolBar *toolbar = window->addToolBar( "Filter Toolbar" );
			toolbar->setObjectName( "Filter Toolbar" );
			create_filter_toolbar( toolbar );
		}
		{
			QToolBar *toolbar = window->addToolBar( "Plugin Toolbar" );
			toolbar->setObjectName( "Plugin Toolbar" );
			create_plugin_toolbar( toolbar );
		}
	}

	create_main_statusbar( window->statusBar(), m_statusLabel );

	GroupDialog_constructWindow( window );

	g_page_entity = GroupDialog_addPage( "Entities", EntityInspector_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Entities" ) );

	if ( FloatingGroupDialog() ) {
		g_page_console = GroupDialog_addPage( "Console", Console_constructWindow(), RawStringExportCaller( "Console" ) );
		g_page_textures = GroupDialog_addPage( "Textures", TextureBrowser_constructWindow( GroupDialog_getWindow() ), TextureBrowserExportTitleCaller() );
	}

	g_page_models = GroupDialog_addPage( "Models", ModelBrowser_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Models" ) );

	window->show();

	if ( CurrentStyle() == eRegular || CurrentStyle() == eRegularLeft ) {
		window->setCentralWidget( m_hSplit = new QSplitter() );
		{
			m_vSplit = new QSplitter( Qt::Vertical );
			m_vSplit2 = new QSplitter( Qt::Vertical );
			if ( CurrentStyle() == eRegular ){
				m_hSplit->addWidget( m_vSplit );
				m_hSplit->addWidget( m_vSplit2 );
			}
			else{
				m_hSplit->addWidget( m_vSplit2 );
				m_hSplit->addWidget( m_vSplit );
			}
			// console
			m_vSplit->addWidget( Console_constructWindow() );

			// xy
			m_pXYWnd = new XYWnd();
			m_pXYWnd->SetViewType( XY );
			m_vSplit->insertWidget( 0, m_pXYWnd->GetWidget() );
			{
				// camera
				m_pCamWnd = NewCamWnd();
				GlobalCamera_setCamWnd( *m_pCamWnd );
				CamWnd_setParent( *m_pCamWnd, window );
				m_vSplit2->addWidget( CamWnd_getWidget( *m_pCamWnd ) );

				// textures
				if( g_Layout_builtInGroupDialog.m_value )
					g_page_textures = GroupDialog_addPage( "Textures", TextureBrowser_constructWindow( GroupDialog_getWindow() ), TextureBrowserExportTitleCaller() );
				else
					m_vSplit2->addWidget( TextureBrowser_constructWindow( window ) );
			}
		}
	}
	else if ( CurrentStyle() == eFloating ) {
		{
			QWidget* window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			window->setWindowTitle( "Camera" );
			g_guiSettings.addWindow( window, "floating/cam", 400, 300, 50, 100 );

			m_pCamWnd = NewCamWnd();
			GlobalCamera_setCamWnd( *m_pCamWnd );

			{
				auto box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( CamWnd_getWidget( *m_pCamWnd ) );
			}

			CamWnd_setParent( *m_pCamWnd, window );
			GlobalPressedKeys_connect( window );
			GlobalWindowObservers_connectTopLevel( window );
			CamWnd_Shown_Construct( window );
		}

		{
			QWidget* window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/xy", 400, 300, 500, 100 );

			m_pXYWnd = new XYWnd();
			m_pXYWnd->m_parent = window;
			m_pXYWnd->SetViewType( XY );

			{
				auto box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pXYWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			XY_Top_Shown_Construct( window );
		}

		{
			QWidget* window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/xz", 400, 300, 500, 450 );

			m_pXZWnd = new XYWnd();
			m_pXZWnd->m_parent = window;
			m_pXZWnd->SetViewType( XZ );

			{
				auto box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pXZWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			XZ_Front_Shown_Construct( window );
		}

		{
			QWidget* window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/yz", 400, 300, 50, 450 );

			m_pYZWnd = new XYWnd();
			m_pYZWnd->m_parent = window;
			m_pYZWnd->SetViewType( YZ );

			{
				auto box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pYZWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			YZ_Side_Shown_Construct( window );
		}

		GroupDialog_show();
	}
	else // 4 way
	{
		window->setCentralWidget( m_hSplit = new QSplitter() );
		m_hSplit->addWidget( m_vSplit = new QSplitter( Qt::Vertical ) );
		m_hSplit->addWidget( m_vSplit2 = new QSplitter( Qt::Vertical ) );

		m_pCamWnd = NewCamWnd();
		GlobalCamera_setCamWnd( *m_pCamWnd );
		CamWnd_setParent( *m_pCamWnd, window );

		m_vSplit->addWidget( CamWnd_getWidget( *m_pCamWnd ) );

		m_pYZWnd = new XYWnd();
		m_pYZWnd->SetViewType( YZ );

		m_vSplit->addWidget( m_pYZWnd->GetWidget() );

		m_pXYWnd = new XYWnd();
		m_pXYWnd->SetViewType( XY );

		m_vSplit2->addWidget( m_pXYWnd->GetWidget() );

		m_pXZWnd = new XYWnd();
		m_pXZWnd->SetViewType( XZ );

		m_vSplit2->addWidget( m_pXZWnd->GetWidget() );
	}

	if( g_Layout_builtInGroupDialog.m_value && CurrentStyle() != eFloating ){
		m_hSplit->addWidget( GroupDialog_getWindow() );
		m_hSplit->setStretchFactor( 0, 2222 ); // set relative splitter sizes for eSplit (no sizes are restored)
		m_hSplit->setStretchFactor( 1, 2222 );
		m_hSplit->setStretchFactor( 2, 0 );
	}

	EntityList_constructWindow( window );
	PreferencesDialog_constructWindow( window );
	FindTextureDialog_constructWindow( window );
	SurfaceInspector_constructWindow( window );
	theme_contruct();

	SetActiveXY( m_pXYWnd );

	AddGridChangeCallback( SetGridStatusCaller( *this ) );
	AddGridChangeCallback( FreeCaller<XY_UpdateAllWindows>() );

	s_qe_every_second_timer.enable();

	RestoreGuiState();

	//GlobalShortcuts_reportUnregistered();
}

void MainFrame::SaveGuiState(){
	//restore good state first
	g_maximizeview.unmaximize();

	g_guiSettings.save();
}

void MainFrame::RestoreGuiState(){
	g_guiSettings.addWindow( m_window, "MainFrame/geometry", 962, 480 );
	g_guiSettings.addMainWindow( m_window, "MainFrame/state" );

	if( !FloatingGroupDialog() && m_hSplit != nullptr && m_vSplit != nullptr && m_vSplit2 != nullptr ){
		g_guiSettings.addSplitter( m_hSplit, "MainFrame/m_hSplit", { 384, 576 } );
		g_guiSettings.addSplitter( m_vSplit, "MainFrame/m_vSplit", { 377, 20 } );
		g_guiSettings.addSplitter( m_vSplit2, "MainFrame/m_vSplit2", { 250, 150 } );
	}
}

void MainFrame::Shutdown(){
	s_qe_every_second_timer.disable();

	EntityList_destroyWindow();

	delete std::exchange( m_pXYWnd, nullptr );
	delete std::exchange( m_pYZWnd, nullptr );
	delete std::exchange( m_pXZWnd, nullptr );

	ModelBrowser_destroyWindow();
	TextureBrowser_destroyWindow();

	DeleteCamWnd( m_pCamWnd );
	m_pCamWnd = 0;

	PreferencesDialog_destroyWindow();
	SurfaceInspector_destroyWindow();
	FindTextureDialog_destroyWindow();

	g_DbgDlg.destroyWindow();

	// destroying group-dialog last because it may contain texture-browser
	GroupDialog_destroyWindow();

	user_shortcuts_save();
}

void MainFrame::RedrawStatusText(){
	for( int i = 0; i < c_status__count; ++i )
		m_statusLabel[i]->setText( m_status[i].c_str() );
}

void MainFrame::UpdateStatusText(){
	m_idleRedrawStatusText.queueDraw();
}

void MainFrame::SetStatusText( int status_n, const char* status ){
	m_status[status_n] = status;
	UpdateStatusText();
}

void Sys_Status( const char* status ){
	if ( g_pParentWnd )
		g_pParentWnd->SetStatusText( c_status_command, status );
}

void brushCountChanged( const Selectable& selectable ){
	QE_brushCountChanged();
}

//int getRotateIncrement(){
//	return static_cast<int>( g_si_globals.rotate );
//}

int getFarClipDistance(){
	return g_camwindow_globals.m_nCubicScale;
}

float ( *GridStatus_getGridSize )() = GetGridSize;
//int ( *GridStatus_getRotateIncrement )() = getRotateIncrement;
int ( *GridStatus_getFarClipDistance )() = getFarClipDistance;
bool ( *GridStatus_getTextureLockEnabled )();
const char* ( *GridStatus_getTexdefTypeIdLabel )();

void MainFrame::SetGridStatus(){
	StringOutputStream status( 64 );
	const char* lock = ( GridStatus_getTextureLockEnabled() ) ? "ON   " : "OFF  ";
	status << ( GetSnapGridSize() > 0 ? "G:" : "g:" ) << GridStatus_getGridSize()
	       << "  F:" << GridStatus_getTexdefTypeIdLabel()
	       << "  C:" << GridStatus_getFarClipDistance()
	       << "  L:" << lock;
	SetStatusText( c_status_grid, status );
}

void GridStatus_changed(){
	if ( g_pParentWnd != 0 ) {
		g_pParentWnd->SetGridStatus();
	}
}

CopiedString g_OpenGLFont( "Myriad Pro" );
int g_OpenGLFontSize = 8;

void OpenGLFont_select(){
	CopiedString newfont;
	int newsize;
	if( OpenGLFont_dialog( MainFrame_getWindow(), g_OpenGLFont.c_str(), g_OpenGLFontSize, newfont, newsize ) ){
		{
			ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing OpenGL Font" );
			delete GlobalOpenGL().m_font;
			g_OpenGLFont = newfont;
			g_OpenGLFontSize = newsize;
			GlobalOpenGL().m_font = glfont_create( g_OpenGLFont.c_str(), g_OpenGLFontSize, g_strAppPath.c_str() );
		}
		UpdateAllWindows();
	}
}


void GlobalGL_sharedContextCreated(){
	// report OpenGL information
	globalOutputStream() << "GL_VENDOR: " << reinterpret_cast<const char*>( gl().glGetString( GL_VENDOR ) ) << '\n';
	globalOutputStream() << "GL_RENDERER: " << reinterpret_cast<const char*>( gl().glGetString( GL_RENDERER ) ) << '\n';
	globalOutputStream() << "GL_VERSION: " << reinterpret_cast<const char*>( gl().glGetString( GL_VERSION ) ) << '\n';
	globalOutputStream() << "GL_EXTENSIONS: " << reinterpret_cast<const char*>( gl().glGetString( GL_EXTENSIONS ) ) << '\n';

	QGL_sharedContextCreated( GlobalOpenGL() );

	ShaderCache_extensionsInitialised();

	GlobalShaderCache().realise();
	Textures_Realise();

	GlobalOpenGL().m_font = glfont_create( g_OpenGLFont.c_str(), g_OpenGLFontSize, g_strAppPath.c_str() );
}

void GlobalGL_sharedContextDestroyed(){
	Textures_Unrealise();
	GlobalShaderCache().unrealise();

	QGL_sharedContextDestroyed( GlobalOpenGL() );
}


void Layout_constructPreferences( PreferencesPage& page ){
	{
		const char* layouts[] = { "window1.png", "window2.png", "window3.png", "window4.png" };
		page.appendRadioIcons(
		    "Window Layout",
		    StringArrayRange( layouts ),
		    LatchedImportCaller( g_Layout_viewStyle ),
		    IntExportCaller( g_Layout_viewStyle.m_latched )
		);
	}
	page.appendCheckBox(
	    "", "Detachable Menus",
	    LatchedImportCaller( g_Layout_enableDetachableMenus ),
	    BoolExportCaller( g_Layout_enableDetachableMenus.m_latched )
	);
	page.appendCheckBox(
	    "", "Built-In Group Dialog",
	    LatchedImportCaller( g_Layout_builtInGroupDialog ),
	    BoolExportCaller( g_Layout_builtInGroupDialog.m_latched )
	);
}

void Layout_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Layout", "Layout Preferences" ) );
	Layout_constructPreferences( page );
}

void Layout_registerPreferencesPage(){
	PreferencesDialog_addInterfacePage( FreeCaller1<PreferenceGroup&, Layout_constructPage>() );
}


void FocusAllViews(){
	XY_Centralize(); //using centralizing here, not focusing function
	GlobalCamera_FocusOnSelected();
}

#include "preferencesystem.h"
#include "stringio.h"

void MainFrame_Construct(){
	GlobalCommands_insert( "OpenManual", FreeCaller<OpenHelpURL>(), QKeySequence( "F1" ) );

	GlobalCommands_insert( "RefreshReferences", FreeCaller<RefreshReferences>() );
	GlobalCommands_insert( "CheckForUpdate", FreeCaller<OpenUpdateURL>() );
	GlobalCommands_insert( "Exit", FreeCaller<Exit>() );

	GlobalCommands_insert( "Preferences", FreeCaller<PreferencesDialog_showDialog>(), QKeySequence( "P" ) );

	GlobalCommands_insert( "ToggleConsole", FreeCaller<Console_ToggleShow>(), QKeySequence( "O" ) );
	GlobalCommands_insert( "ToggleEntityInspector", FreeCaller<EntityInspector_ToggleShow>(), QKeySequence( "N" ) );
	GlobalCommands_insert( "ToggleModelBrowser", FreeCaller<ModelBrowser_ToggleShow>(), QKeySequence( "/" ) );
	GlobalCommands_insert( "EntityList", FreeCaller<EntityList_toggleShown>(), QKeySequence( "L" ) );

	Select_registerCommands();

	Tools_registerCommands();

	GlobalCommands_insert( "BuildMenuCustomize", FreeCaller<DoBuildMenu>() );
	GlobalCommands_insert( "Build_runRecentExecutedBuild", FreeCaller<Build_runRecentExecutedBuild>(), QKeySequence( "F5" ) );

	GlobalCommands_insert( "OpenGLFont", FreeCaller<OpenGLFont_select>() );

	Colors_registerCommands();

	GlobalCommands_insert( "Fullscreen", FreeCaller<MainFrame_toggleFullscreen>(), QKeySequence( "F11" ) );
	GlobalCommands_insert( "MaximizeView", FreeCaller<Maximize_View>(), QKeySequence( "F12" ) );

	CSG_registerCommands();

	Grid_registerCommands();

	Patch_registerCommands();
	XYShow_registerCommands();

	GlobalPreferenceSystem().registerPreference( "DetachableMenus", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_enableDetachableMenus ) ), BoolExportStringCaller( g_Layout_enableDetachableMenus.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "QE4StyleWindows", makeIntStringImportCallback( LatchedAssignCaller( g_Layout_viewStyle ) ), IntExportStringCaller( g_Layout_viewStyle.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "BuiltInGroupDialog", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_builtInGroupDialog ) ), BoolExportStringCaller( g_Layout_builtInGroupDialog.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "OpenGLFont", CopiedStringImportStringCaller( g_OpenGLFont ), CopiedStringExportStringCaller( g_OpenGLFont ) );
	GlobalPreferenceSystem().registerPreference( "OpenGLFontSize", IntImportStringCaller( g_OpenGLFontSize ), IntExportStringCaller( g_OpenGLFontSize ) );

	for( size_t i = 0; i < g_strExtraResourcePaths.size(); ++i )
		GlobalPreferenceSystem().registerPreference( StringStream<32>( "ExtraResourcePath", i ),
			CopiedStringImportStringCaller( g_strExtraResourcePaths[i] ), CopiedStringExportStringCaller( g_strExtraResourcePaths[i] ) );

	GlobalPreferenceSystem().registerPreference( "EnginePath", CopiedStringImportStringCaller( g_strEnginePath ), CopiedStringExportStringCaller( g_strEnginePath ) );
	GlobalPreferenceSystem().registerPreference( "InstalledDevFilesPath", CopiedStringImportStringCaller( g_installedDevFilesPath ), CopiedStringExportStringCaller( g_installedDevFilesPath ) );
	if ( g_strEnginePath.empty() )
	{
		g_strEnginePath_was_empty_1st_start = true;
		const char* ENGINEPATH_ATTRIBUTE =
#if defined( WIN32 )
		    "enginepath_win32"
#elif defined( __APPLE__ )
		    "enginepath_macos"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
		    "enginepath_linux"
#else
#error "unknown platform"
#endif
		    ;
		g_strEnginePath = StringStream( DirectoryCleaned( g_pGameDescription->getRequiredKeyValue( ENGINEPATH_ATTRIBUTE ) ) );
	}


	Layout_registerPreferencesPage();
	Paths_registerPreferencesPage();

	g_brushCount.setCountChangedCallback( FreeCaller<QE_brushCountChanged>() );
	g_patchCount.setCountChangedCallback( FreeCaller<QE_brushCountChanged>() );
	g_entityCount.setCountChangedCallback( FreeCaller<QE_brushCountChanged>() );
	GlobalEntityCreator().setCounter( &g_entityCount );
	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller1<const Selectable&, brushCountChanged>() );

	GLWidget_sharedContextCreated = GlobalGL_sharedContextCreated;
	GLWidget_sharedContextDestroyed = GlobalGL_sharedContextDestroyed;

	GlobalEntityClassManager().attach( g_WorldspawnColourEntityClassObserver );
}

void MainFrame_Destroy(){
	GlobalEntityClassManager().detach( g_WorldspawnColourEntityClassObserver );

	GlobalEntityCreator().setCounter( 0 );
	g_entityCount.setCountChangedCallback( Callback() );
	g_patchCount.setCountChangedCallback( Callback() );
	g_brushCount.setCountChangedCallback( Callback() );
}


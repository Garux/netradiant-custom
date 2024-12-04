/*
   PrtView plugin for GtkRadiant
   Copyright (C) 2001 Geoffrey Dewan, Loki software and qeradiant.com

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "prtview.h"
#include <cstdio>
#include <cstdlib>

#include "profile/profile2.h"

#include "qerplugin.h"
#include "iscenegraph.h"
#include "iglrender.h"
#include "iplugin.h"
#include "stream/stringstream.h"

#include "portals.h"
#include "AboutDialog.h"
#include "ConfigDialog.h"
#include "LoadPortalFileDialog.h"

#define Q3R_CMD_SPLITTER "-"
#define Q3R_CMD_ABOUT    "About Portal Viewer"
#define Q3R_CMD_LOAD     "Load .prt file"
#define Q3R_CMD_RELEASE  "Unload .prt file"
#define Q3R_CMD_SHOW_3D  "Toggle portals (3D)"
#define Q3R_CMD_SHOW_2D  "Toggle portals (2D)"
#define Q3R_CMD_OPTIONS  "Configure Portal Viewer"

/////////////////////////////////////////////////////////////////////////////
// CPrtViewApp construction

const char RENDER_2D    [] = "Render2D";
const char WIDTH_2D     [] = "Width2D";
const char COLOR_2D     [] = "Color2D";

const char DRAW_HINTS   [] = "DrawHints";
const char DRAW_NONHINTS[] = "DrawNonHints";

const char RENDER_3D    [] = "Render3D";
const char ZBUFFER      [] = "ZBuffer";
const char FOG          [] = "Fog";
const char POLYGON      [] = "Polygons";
const char LINE         [] = "Lines";
const char WIDTH_3D     [] = "Width3D";
const char COLOR_3D     [] = "Color3D";
const char COLOR_FOG    [] = "ColorFog";
const char OPACITY_3D   [] = "Opacity";
const char CLIP         [] = "Clip";
const char CLIP_RANGE   [] = "ClipRange";

class PrtViewIniFile
{
	IniFile m_ini;
	StringOutputStream INI_path() const {
		return StringStream( GlobalRadiant().getSettingsPath(), "prtview.ini" );
	}
	static constexpr char CONFIG_SECTION[] = "Configuration";
public:
	void read(){
		m_ini.read( INI_path() );
	}
	void write() const {
		m_ini.write( INI_path() );
	}
	int GetInt( const char *key, int def ) const {
		const auto value = m_ini.getValue( CONFIG_SECTION, key );
		return value? atoi( *value ) : def;
	}
	void SetInt( const char *key, int val, const char *comment ){
		char s[512];
		std::snprintf( s, std::size( s ), "%d        ; %s", val, comment );
		m_ini.setValue( CONFIG_SECTION, key, s );
	}
};

void LoadConfig(){
	PrtViewIniFile ini;
	ini.read();

	portals.show_2d                = ini.GetInt( RENDER_2D    , 0 );
	portals.width_2d   = std::clamp( ini.GetInt( WIDTH_2D     , 3 ), 1, 10 );
	portals.color_2d               = ini.GetInt( COLOR_2D     , RGB_PACK( 0, 0, 255 ) ) & 0xFFFFFF;

	portals.draw_hints             = ini.GetInt( DRAW_HINTS   , 1 );
	portals.draw_nonhints          = ini.GetInt( DRAW_NONHINTS, 1 );

	portals.show_3d                = ini.GetInt( RENDER_3D    , 1 );
	portals.zbuffer                = ini.GetInt( ZBUFFER      , 1 );
	portals.fog                    = ini.GetInt( FOG          , 0 );
	portals.polygons               = ini.GetInt( POLYGON      , 1 );
	portals.lines                  = ini.GetInt( LINE         , 1 );
	portals.width_3d   = std::clamp( ini.GetInt( WIDTH_3D     , 3 ), 1, 10 );
	portals.color_3d               = ini.GetInt( COLOR_3D     , RGB_PACK( 255, 255, 0 ) ) & 0xFFFFFF;
	portals.color_fog              = ini.GetInt( COLOR_FOG    , RGB_PACK( 127, 127, 127 ) ) & 0xFFFFFF;
	portals.opacity_3d = std::clamp( ini.GetInt( OPACITY_3D   , 50 ), 0, 100 );
	portals.clip                   = ini.GetInt( CLIP         , 0 );
	portals.clip_range = std::clamp( ini.GetInt( CLIP_RANGE   , 1024 ), 64, 8192 );

	if ( portals.zbuffer < 0 || portals.zbuffer > 2 )
		portals.zbuffer = 0;

	portals.FixColors();
}

void SaveConfig(){
	PrtViewIniFile ini;

	ini.SetInt( RENDER_2D,     portals.show_2d,       "Draw in 2D windows" );
	ini.SetInt( WIDTH_2D,      portals.width_2d,      "Width of lines in 2D windows" );
	ini.SetInt( COLOR_2D,      portals.color_2d,      "Color of lines in 2D windows" );

	ini.SetInt( DRAW_HINTS,    portals.draw_hints,    "Draw Hint Portals" );
	ini.SetInt( DRAW_NONHINTS, portals.draw_nonhints, "Draw Regular Portals" );

	ini.SetInt( RENDER_3D,     portals.show_3d,       "Draw in 3D windows" );
	ini.SetInt( ZBUFFER,       portals.zbuffer,       "ZBuffer level in 3D window" );
	ini.SetInt( FOG,           portals.fog,           "Use depth cueing in 3D window" );
	ini.SetInt( POLYGON,       portals.polygons,      "Render using polygons in 3D window" );
	ini.SetInt( LINE,          portals.lines,         "Render using lines in 3D window" );
	ini.SetInt( WIDTH_3D,      portals.width_3d,      "Width of lines in 3D window" );
	ini.SetInt( COLOR_3D,      portals.color_3d,      "Color of lines/polygons in 3D window" );
	ini.SetInt( COLOR_FOG,     portals.color_fog,     "Color of distant lines/polygons in 3D window" );
	ini.SetInt( OPACITY_3D,    portals.opacity_3d,    "Opacity in 3d view (0 = invisible, 100 = solid)" );
	ini.SetInt( CLIP,          portals.clip,          "Cubic clipper active for portal viewer" );
	ini.SetInt( CLIP_RANGE,    portals.clip_range,    "Portal viewer cubic clip distance (in units of 64)" );

	ini.write();
}


void PrtView_construct(){
	LoadConfig();
	Portals_constructShaders();
	GlobalShaderCache().attachRenderable( render );
}

void PrtView_destroy(){
	SaveConfig();
	GlobalShaderCache().detachRenderable( render );
	Portals_destroyShaders();
}


// plugin name
static const char *PLUGIN_NAME = "Portal Viewer";
// commands in the menu
static const char *PLUGIN_COMMANDS =
    Q3R_CMD_ABOUT ";"
    Q3R_CMD_SPLITTER ";"
    Q3R_CMD_OPTIONS ";"
    Q3R_CMD_SPLITTER ";"
    Q3R_CMD_SHOW_2D ";"
    Q3R_CMD_SHOW_3D ";"
    Q3R_CMD_SPLITTER ";"
    Q3R_CMD_RELEASE ";"
    Q3R_CMD_LOAD;


QWidget *g_pRadiantWnd = NULL;

const char* QERPlug_Init( void *hApp, void* pMainWidget ){
	g_pRadiantWnd = static_cast<QWidget*>( pMainWidget );
	return "Portal Viewer for Q3Radiant";
}

const char* QERPlug_GetName(){
	return PLUGIN_NAME;
}

const char* QERPlug_GetCommandList(){
	return PLUGIN_COMMANDS;
}


const char* QERPlug_GetCommandTitleList(){
	return "";
}


void QERPlug_Dispatch( const char* p, float* vMin, float* vMax, bool bSingleBrush ){
	globalOutputStream() << MSG_PREFIX "Command \"" << p << "\"\n";

	if ( !strcmp( p, Q3R_CMD_ABOUT ) ) {
		DoAboutDlg();
	}
	else if ( !strcmp( p, Q3R_CMD_LOAD ) ) {
		if ( DoLoadPortalFileDialog() ) {
			portals.Load();
			SceneChangeNotify();
		}
		else
		{
			globalOutputStream() << MSG_PREFIX "Portal file load aborted.\n";
		}
	}
	else if ( !strcmp( p, Q3R_CMD_RELEASE ) ) {
		portals.Purge();

		SceneChangeNotify();

		globalOutputStream() << MSG_PREFIX "Portals unloaded.\n";
	}
	else if ( !strcmp( p, Q3R_CMD_SHOW_2D ) ) {
		portals.show_2d = !portals.show_2d;

		SceneChangeNotify();

		if ( portals.show_2d ) {
			globalOutputStream() << MSG_PREFIX "Portals will be rendered in 2D view.\n";
		}
		else{
			globalOutputStream() << MSG_PREFIX "Portals will NOT be rendered in 2D view.\n";
		}
	}
	else if ( !strcmp( p, Q3R_CMD_SHOW_3D ) ) {
		portals.show_3d = !portals.show_3d;

		SceneChangeNotify();

		if ( portals.show_3d ) {
			globalOutputStream() << MSG_PREFIX "Portals will be rendered in 3D view.\n";
		}
		else{
			globalOutputStream() << MSG_PREFIX "Portals will NOT be rendered in 3D view.\n";
		}
	}
	else if ( !strcmp( p, Q3R_CMD_OPTIONS ) ) {
		DoConfigDialog();

		SceneChangeNotify();
	}
}


#include "modulesystem/singletonmodule.h"

class PrtViewPluginDependencies :
	public GlobalSceneGraphModuleRef,
	public GlobalRadiantModuleRef,
	public GlobalShaderCacheModuleRef,
	public GlobalOpenGLModuleRef,
	public GlobalOpenGLStateLibraryModuleRef
{
};

class PrtViewPluginModule
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT( Name, "prtview" );

	PrtViewPluginModule(){
		m_plugin.m_pfnQERPlug_Init = QERPlug_Init;
		m_plugin.m_pfnQERPlug_GetName = QERPlug_GetName;
		m_plugin.m_pfnQERPlug_GetCommandList = QERPlug_GetCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = QERPlug_GetCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = QERPlug_Dispatch;

		PrtView_construct();
	}
	~PrtViewPluginModule(){
		PrtView_destroy();
	}
	_QERPluginTable* getTable(){
		return &m_plugin;
	}
};

typedef SingletonModule<PrtViewPluginModule, PrtViewPluginDependencies> SingletonPrtViewPluginModule;

SingletonPrtViewPluginModule g_PrtViewPluginModule;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );

	g_PrtViewPluginModule.selfRegister();
}

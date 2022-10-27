/*
   Copyright (C) 2006, Thomas Nitschke.
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
#include "plugin.h"

#include "iplugin.h"
#include "qerplugin.h"

#include "debugging/debugging.h"
#include "string/string.h"
#include "modulesystem/singletonmodule.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"

#include "ibrush.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ifilesystem.h"
#include "ifiletypes.h"

#include "typesystem.h"

void CreateWindow( void );

QWidget *g_pRadiantWnd = nullptr;

namespace BrushExport
{
QWidget* g_mainwnd;

const char* init( void* hApp, void* pMainWidget ){
	g_pRadiantWnd = static_cast<QWidget*>( pMainWidget );
	ASSERT_NOTNULL( g_pRadiantWnd );
	return "";
}
const char* getName(){
	return "Brush export Plugin";
}
const char* getCommandList(){
	return "Export .obj;About";
}
const char* getCommandTitleList(){
	return "Export selected as Wavefront Object;About";
}

void dispatch( const char* command, float* vMin, float* vMax, bool bSingleBrush ){
	if ( string_equal( command, "About" ) ) {
		GlobalRadiant().m_pfnMessageBox( g_pRadiantWnd,
		                                 "Brushexport plugin v 3.0 by namespace (<a href='http://www.codecreator.net'>http://www.codecreator.net</a>)<br>"
		                                 "Enjoy!<br><br>"
										 "Send feedback to <a href='mailto:spam@codecreator.net'>spam@codecreator.net</a>",
										 "About me...", EMessageBoxType::Info, 0 );
	}
	else if ( string_equal( command, "Export .obj" ) ) {
		CreateWindow();
	}
}
}

class BrushExportDependencies :
	public GlobalRadiantModuleRef,
	public GlobalFiletypesModuleRef,
	public GlobalBrushModuleRef,
	public GlobalFileSystemModuleRef,
	public GlobalSceneGraphModuleRef,
	public GlobalSelectionModuleRef
{
public:
	BrushExportDependencies( void )
		: GlobalBrushModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "brushtypes" ) )
	{}
};

class BrushExportModule : public TypeSystemRef
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT( Name, "brushexport2" );

	BrushExportModule(){
		m_plugin.m_pfnQERPlug_Init = &BrushExport::init;
		m_plugin.m_pfnQERPlug_GetName = &BrushExport::getName;
		m_plugin.m_pfnQERPlug_GetCommandList = &BrushExport::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &BrushExport::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = &BrushExport::dispatch;
	}
	_QERPluginTable* getTable(){
		return &m_plugin;
	}
};

typedef SingletonModule<BrushExportModule, BrushExportDependencies> SingletonBrushExportModule;
SingletonBrushExportModule g_BrushExportModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );
	g_BrushExportModule.selfRegister();
}

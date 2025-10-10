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

#include "plugintoolbar.h"


#include "itoolbar.h"
#include "gtkmisc.h"
#include "gtkutil/image.h"
#include "modulesystem.h"

#include "stream/stringstream.h"
#include "os/file.h"
#include "os/path.h"

#include "mainframe.h"
#include "plugin.h"
#include "pluginmanager.h"

QIcon new_plugin_icon( const char* filename ){
	StringOutputStream fullpath( 256 );

	const char *rootdir[][ 2 ] = { { AppPath_get(), g_pluginsDir }, { GameToolsPath_get(), g_pluginsDir }, { AppPath_get(), g_modulesDir } };

	for( const auto [ root, dir ] : rootdir )
		for( const auto *ext : { ".svg", ".png" } )
			if( file_exists( fullpath( root, dir, "bitmaps/", PathExtensionless( filename ), ext ) ) )
				return QIcon( fullpath.c_str() );

	return {};
}

void toolbar_insert( QToolBar *toolbar, const char* icon, const char* text, const char* tooltip, IToolbarButton::EType type, const IToolbarButton* ibutton, const char *pluginName ){
	switch ( type )
	{
	case IToolbarButton::eSpace:
		toolbar_append_separator( toolbar );
		return;
	case IToolbarButton::eButton:
		{
			toolbar_append_button( toolbar, tooltip, new_local_icon( icon ), plugin_construct_command_name( pluginName, text ).c_str() );
			// QAction *button = toolbar->addAction( new_plugin_icon( icon ), text, [ibutton](){ ibutton->activate(); } );
			// button->setToolTip( tooltip );
		}
		return;
	case IToolbarButton::eToggleButton:
		{
			//. fixme need consistent plugin command names (same in menu and toolbar) for the current command system
			// now they are defined in 3 places, also must be used in menu to work in toolbar
			// also no defined toggle menu item support (->setCheckable( true ) is some workaround now)
			toolbar_append_button( toolbar, tooltip, new_local_icon( icon ), plugin_construct_command_name( pluginName, text ).c_str() )->setCheckable( true );
			// toolbar_append_toggle_button( toolbar, tooltip, new_plugin_icon( icon ), plugin_construct_command_name( pluginName, text ).c_str() );
			// QAction *button = toolbar->addAction( new_plugin_icon( icon ), text, [ibutton](){ ibutton->activate(); } );
			// button->setToolTip( tooltip );
			// button->setCheckable( true );
		}
		return;
	case IToolbarButton::eRadioButton:
		ERROR_MESSAGE( "IToolbarButton::eRadioButton not implemented" );
		return;
	default:
		ERROR_MESSAGE( "invalid toolbar button type" );
		return;
	}
}

void PlugInToolbar_AddButton( QToolBar* toolbar, const IToolbarButton* button, const char *pluginName ){
	toolbar_insert( toolbar, button->getImage(), button->getText(), button->getTooltip(), button->getType(), button, pluginName );
}

QToolBar* g_plugin_toolbar = 0;

void PluginToolbar_populate(){
	class AddToolbarItemVisitor : public ToolbarModules::Visitor
	{
		QToolBar* m_toolbar;
	public:
		AddToolbarItemVisitor( QToolBar* toolbar )
			: m_toolbar( toolbar ){
		}
		void visit( const char* name, const _QERPlugToolbarTable& table ) const override {
			const std::size_t count = table.m_pfnToolbarButtonCount();
			for ( std::size_t i = 0; i < count; ++i )
			{
				PlugInToolbar_AddButton( m_toolbar, table.m_pfnGetToolbarButton( i ), name );
			}
		}
	} visitor( g_plugin_toolbar );

	Radiant_getToolbarModules().foreachModule( visitor );
}

void PluginToolbar_clear(){
#if 0 // unused, intended for "Refresh"
	container_remove_all( GTK_CONTAINER( g_plugin_toolbar ) );
#endif
}

void create_plugin_toolbar( QToolBar *toolbar ){
	g_plugin_toolbar = toolbar;
	PluginToolbar_populate();
}

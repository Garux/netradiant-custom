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
#include "modulesystem.h"

#include "stream/stringstream.h"
#include "os/file.h"
#include "gtkutil/image.h"
#include "gtkutil/toolbar.h"

#include "mainframe.h"
#include "plugin.h"

QIcon new_plugin_icon( const char* filename ){
	StringOutputStream fullpath( 256 );
	{
		fullpath( AppPath_get(), g_pluginsDir, "bitmaps/", filename );
		if( file_exists( fullpath ) )
			return QIcon( fullpath.c_str() );
	}

	{
		fullpath( GameToolsPath_get(), g_pluginsDir, "bitmaps/", filename );
		if( file_exists( fullpath ) )
			return QIcon( fullpath.c_str() );
	}

	{
		fullpath( AppPath_get(), g_modulesDir, "bitmaps/", filename );
		if( file_exists( fullpath ) )
			return QIcon( fullpath.c_str() );
	}

	return {};
}

void toolbar_insert( QToolBar *toolbar, const char* icon, const char* text, const char* tooltip, IToolbarButton::EType type, const IToolbarButton* ibutton ){
	switch ( type )
	{
	case IToolbarButton::eSpace:
		toolbar->addSeparator();
		return;
	case IToolbarButton::eButton:
		{
			QAction *button = toolbar->addAction( new_plugin_icon( icon ), text, [ibutton](){ ibutton->activate(); } );
			button->setToolTip( tooltip );
		}
		return;
	case IToolbarButton::eToggleButton:
		{
			QAction *button = toolbar->addAction( new_plugin_icon( icon ), text, [ibutton](){ ibutton->activate(); } );
			button->setToolTip( tooltip );
			button->setCheckable( true );
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

void PlugInToolbar_AddButton( QToolBar* toolbar, const IToolbarButton* button ){
	toolbar_insert( toolbar, button->getImage(), button->getText(), button->getTooltip(), button->getType(), button );
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
		void visit( const char* name, const _QERPlugToolbarTable& table ) const {
			const std::size_t count = table.m_pfnToolbarButtonCount();
			for ( std::size_t i = 0; i < count; ++i )
			{
				PlugInToolbar_AddButton( m_toolbar, table.m_pfnGetToolbarButton( i ) );
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

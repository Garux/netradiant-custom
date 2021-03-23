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

#include <gtk/gtk.h>

#include "stream/stringstream.h"
#include "gtkutil/image.h"
#include "gtkutil/container.h"
#include "gtkutil/toolbar.h"

#include "mainframe.h"
#include "plugin.h"

GtkImage* new_plugin_image( const char* filename ){
	{
		StringOutputStream fullpath( 256 );
		fullpath << AppPath_get() << g_pluginsDir << "bitmaps/" << filename;
		GtkImage* image = image_new_from_file_with_mask( fullpath.c_str() );
		if ( image != 0 ) {
			return image;
		}
	}

	{
		StringOutputStream fullpath( 256 );
		fullpath << GameToolsPath_get() << g_pluginsDir << "bitmaps/" << filename;
		GtkImage* image = image_new_from_file_with_mask( fullpath.c_str() );
		if ( image != 0 ) {
			return image;
		}
	}

	{
		StringOutputStream fullpath( 256 );
		fullpath << AppPath_get() << g_modulesDir << "bitmaps/" << filename;
		GtkImage* image = image_new_from_file_with_mask( fullpath.c_str() );
		if ( image != 0 ) {
			return image;
		}
	}

	return image_new_missing();
}

void toolbar_insert( GtkToolbar *toolbar, const char* icon, const char* text, const char* tooltip, IToolbarButton::EType type, GCallback callback, gpointer data ){
	switch ( type )
	{
	case IToolbarButton::eSpace:
		toolbar_append_space( toolbar );
		return;
	case IToolbarButton::eButton:
		{
			GtkToolButton* button = GTK_TOOL_BUTTON( gtk_tool_button_new( GTK_WIDGET( new_plugin_image( icon ) ), text ) );
			g_signal_connect( G_OBJECT( button ), "clicked", callback, data );
			toolbar_append( toolbar, GTK_TOOL_ITEM( button ), tooltip );
		}
		return;
	case IToolbarButton::eToggleButton:
		{
			GtkToggleToolButton* button = GTK_TOGGLE_TOOL_BUTTON( gtk_toggle_tool_button_new() );
			gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( button ), GTK_WIDGET( new_plugin_image( icon ) ) );
			gtk_tool_button_set_label( GTK_TOOL_BUTTON( button ), text );
			g_signal_connect( G_OBJECT( button ), "toggled", callback, data );
			toolbar_append( toolbar, GTK_TOOL_ITEM( button ), tooltip );
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

void ActivateToolbarButton( GtkWidget *widget, gpointer data ){
	const_cast<const IToolbarButton*>( reinterpret_cast<IToolbarButton*>( data ) )->activate();
}

void PlugInToolbar_AddButton( GtkToolbar* toolbar, const IToolbarButton* button ){
	toolbar_insert( toolbar, button->getImage(), button->getText(), button->getTooltip(), button->getType(), G_CALLBACK( ActivateToolbarButton ), reinterpret_cast<gpointer>( const_cast<IToolbarButton*>( button ) ) );
}

GtkToolbar* g_plugin_toolbar = 0;

void PluginToolbar_populate(){
	class AddToolbarItemVisitor : public ToolbarModules::Visitor
	{
		GtkToolbar* m_toolbar;
	public:
		AddToolbarItemVisitor( GtkToolbar* toolbar )
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
	container_remove_all( GTK_CONTAINER( g_plugin_toolbar ) );
}

GtkToolbar* create_plugin_toolbar(){
	g_plugin_toolbar = toolbar_new();
	PluginToolbar_populate();
	return g_plugin_toolbar;
}

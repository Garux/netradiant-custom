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

#include "pluginmenu.h"

#include "stream/textstream.h"

#include <gtk/gtk.h>

#include "gtkutil/pointer.h"
#include "gtkutil/menu.h"

#include "pluginmanager.h"
#include "mainframe.h"
#include "preferences.h"

#include "gtkmisc.h"

#include <stack>
typedef std::stack<GtkMenu*> MenuStack;

void PlugInMenu_Add( GtkMenu* plugin_menu, IPlugIn* pPlugIn ){
	GtkMenu *menu;
	const char *menuText;
	MenuStack menuStack;

	menu = create_sub_menu_with_mnemonic( plugin_menu, pPlugIn->getMenuName() );
	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	std::size_t nCount = pPlugIn->getCommandCount();
	{
		while ( nCount > 0 )
		{
			menuText = pPlugIn->getCommandTitle( --nCount );

			if ( menuText != 0 && strlen( menuText ) > 0 ) {
				if ( plugin_menu_separator( menuText ) ) {
					menu_separator( menu );
				}
				else if ( plugin_submenu_in( menuText ) ) {
					menuText = pPlugIn->getCommandTitle( --nCount );
					if ( plugin_menu_special( menuText ) ) {
						globalErrorStream() << pPlugIn->getMenuName() << " Invalid title (" << menuText << ") for submenu.\n";
						continue;
					}
					menuStack.push( menu );
					menu = create_sub_menu_with_mnemonic( menu, menuText );
					if ( g_Layout_enableDetachableMenus.m_value ) {
						menu_tearoff( menu );
					}
					continue;
				}
				else if ( plugin_submenu_out( menuText ) ) {
					if ( !menuStack.empty() ) {
						menu = menuStack.top();
						menuStack.pop();
					}
					else
					{
						globalErrorStream() << pPlugIn->getMenuName() << ": Attempt to end non-existent submenu ignored.\n";
					}
					continue;
				}
				else
				{
					create_menu_item_with_mnemonic( menu, menuText, pPlugIn->getGlobalCommand( nCount ) );
				}
			}
		}
		if ( !menuStack.empty() ) {
			globalErrorStream() << pPlugIn->getMenuName() << " mismatched > <. " << Unsigned( menuStack.size() ) << " submenu(s) not closed.\n";
		}
	}
}

GtkMenu* g_plugins_menu = 0;
GtkMenuItem* g_plugins_menu_separator = 0;

void PluginsMenu_populate(){
	class PluginsMenuConstructor : public PluginsVisitor
	{
		GtkMenu* m_menu;
	public:
		PluginsMenuConstructor( GtkMenu* menu ) : m_menu( menu ){
		}
		void visit( IPlugIn& plugin ){
			PlugInMenu_Add( m_menu, &plugin );
		}
	};

	PluginsMenuConstructor constructor( g_plugins_menu );
	GetPlugInMgr().constructMenu( constructor );
}

void PluginsMenu_clear(){
	GList* lst = g_list_find( gtk_container_get_children( GTK_CONTAINER( g_plugins_menu ) ), GTK_WIDGET( g_plugins_menu_separator ) );
	while ( lst->next )
	{
		gtk_container_remove( GTK_CONTAINER( g_plugins_menu ), GTK_WIDGET( lst->next->data ) );
		lst = g_list_find( gtk_container_get_children( GTK_CONTAINER( g_plugins_menu ) ),  GTK_WIDGET( g_plugins_menu_separator ) );
	}
}

GtkMenuItem* create_plugins_menu(){
	// Plugins menu
	GtkMenuItem* plugins_menu_item = new_sub_menu_item_with_mnemonic( "_Plugins" );
	GtkMenu* menu = GTK_MENU( gtk_menu_item_get_submenu( plugins_menu_item ) );
	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	g_plugins_menu = menu;

	//TODO: some modules/plugins do not yet support refresh
#if 0
	create_menu_item_with_mnemonic( menu, "Refresh", FreeCaller<Restart>() );

	// NOTE: the separator is used when doing a refresh of the list, everything past the separator is removed
	g_plugins_menu_separator = menu_separator( menu );
#endif

	PluginsMenu_populate();

	return plugins_menu_item;
}

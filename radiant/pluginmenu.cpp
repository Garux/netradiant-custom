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

#include <QMenuBar>

#include "gtkutil/menu.h"

#include "pluginmanager.h"
#include "mainframe.h"
#include "preferences.h"

#include "gtkmisc.h"

#include <stack>
typedef std::stack<QMenu*> MenuStack;

void PlugInMenu_Add( QMenu* plugin_menu, IPlugIn* pPlugIn ){
	QMenu *menu;
	const char *menuText;
	MenuStack menuStack;

	std::size_t nCount = pPlugIn->getCommandCount();

	if ( nCount > 1 ) { // create submenu
		menu = plugin_menu->addMenu( pPlugIn->getMenuName() );

		menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
		while ( nCount > 0 )
		{
			menuText = pPlugIn->getCommandTitle( --nCount );

			if ( menuText != 0 && strlen( menuText ) > 0 ) {
				if ( plugin_menu_separator( menuText ) ) {
					menu->addSeparator();
				}
				else if ( plugin_submenu_in( menuText ) ) {
					menuText = pPlugIn->getCommandTitle( --nCount );
					if ( plugin_menu_special( menuText ) ) {
						globalErrorStream() << pPlugIn->getMenuName() << " Invalid title (" << menuText << ") for submenu.\n";
						continue;
					}
					menuStack.push( menu );
					menu = menu->addMenu( menuText );

					menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

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
			globalErrorStream() << pPlugIn->getMenuName() << " mismatched > <. " << menuStack.size() << " submenu(s) not closed.\n";
		}
	}
	else if ( nCount == 1 ) { // add only command directly
		menuText = pPlugIn->getCommandTitle( --nCount );

		if ( menuText != 0 && strlen( menuText ) > 0 ) {
			create_menu_item_with_mnemonic( plugin_menu, menuText, pPlugIn->getGlobalCommand( nCount ) );
		}
	}
}

QMenu* g_plugins_menu = 0;
QAction* g_plugins_menu_separator = 0;

void PluginsMenu_populate(){
	class PluginsMenuConstructor : public PluginsVisitor
	{
		QMenu* m_menu;
	public:
		PluginsMenuConstructor( QMenu* menu ) : m_menu( menu ){
		}
		void visit( IPlugIn& plugin ){
			PlugInMenu_Add( m_menu, &plugin );
		}
	};

	PluginsMenuConstructor constructor( g_plugins_menu );
	GetPlugInMgr().constructMenu( constructor );
}

void PluginsMenu_clear(){
#if 0 // unused, intended for "Refresh"
	GList* lst = g_list_find( gtk_container_get_children( GTK_CONTAINER( g_plugins_menu ) ), GTK_WIDGET( g_plugins_menu_separator ) );
	while ( lst->next )
	{
		gtk_container_remove( GTK_CONTAINER( g_plugins_menu ), GTK_WIDGET( lst->next->data ) );
		lst = g_list_find( gtk_container_get_children( GTK_CONTAINER( g_plugins_menu ) ),  GTK_WIDGET( g_plugins_menu_separator ) );
	}
#endif
}

void create_plugins_menu( QMenuBar *menubar ){
	// Plugins menu
	QMenu *menu = menubar->addMenu( "&Plugins" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	g_plugins_menu = menu;

	//TODO: some modules/plugins do not yet support refresh
#if 0
	create_menu_item_with_mnemonic( menu, "Refresh", makeCallbackF( Restart ) );

	// NOTE: the separator is used when doing a refresh of the list, everything past the separator is removed
	g_plugins_menu_separator = menu_separator( menu );
#endif

	PluginsMenu_populate();
}

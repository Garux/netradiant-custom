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

#pragma once

#include <cstddef>

/*!
   \class IPlugin
   pure virtual interface for a plugin
   temporary solution for migration from old plugin tech to synapse plugins
 */
class IPlugIn
{
public:
	virtual const char* getMenuName() = 0;
	virtual std::size_t getCommandCount() = 0;
	virtual const char* getCommand( std::size_t ) = 0;
	virtual const char* getCommandTitle( std::size_t ) = 0;
	virtual const char* getGlobalCommand( std::size_t ) = 0;
};

class PluginsVisitor
{
public:
	virtual void visit( IPlugIn& plugin ) = 0;
};

class CPlugInManager
{
public:
	void Init( class QWidget* main_window );
	void constructMenu( PluginsVisitor& menu );
	void Shutdown();
};

CPlugInManager& GetPlugInMgr();

inline bool plugin_submenu_in( const char* text ){
	return text[0] == '>' && text[1] == '\0';
}
inline bool plugin_submenu_out( const char* text ){
	return text[0] == '<' && text[1] == '\0';
}
inline bool plugin_menu_separator( const char* text ){
	return text[0] == '-' && text[1] == '\0';
}
inline bool plugin_menu_special( const char* text ){
	return plugin_menu_separator( text )
	       || plugin_submenu_in( text )
	       || plugin_submenu_out( text );
}

#include "stream/stringstream.h"

StringBuffer plugin_construct_command_name( const char *pluginName, const char *commandName );

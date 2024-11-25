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

// PlugInManager.cpp: implementation of the CPlugInManager class.
//
//////////////////////////////////////////////////////////////////////

#include "pluginmanager.h"

#include "modulesystem.h"
#include "qerplugin.h"
#include "iplugin.h"

#include "math/vector.h"
#include "string/string.h"

#include "error.h"
#include "select.h"
#include "plugin.h"

#include "modulesystem.h"

#include "stream/stringstream.h"
#include "commands.h"

#include <list>

/* plugin manager --------------------------------------- */
class CPluginSlot : public IPlugIn
{
	CopiedString m_menu_name;
	const _QERPluginTable *mpTable;
	std::list<CopiedString> m_CommandStrings;
	std::list<CopiedString> m_CommandTitleStrings;

	std::list<CopiedString> m_globalCommandNames;
	class PluginCaller
	{
		CPluginSlot* slot;
		const char* name;
	public:
		PluginCaller( CPluginSlot* slot_, const char* name_ ) : slot( slot_ ), name( name_ ){
		}
		void operator()(){
			slot->Dispatch( name );
		}
	};
	std::list<PluginCaller> m_callbacks;

public:
	/*!
	   build directly from a SYN_PROVIDE interface
	 */
	CPluginSlot( QWidget* main_window, const char* name, const _QERPluginTable& table );
	/*!
	   dispatching a command by name to the plugin
	 */
	void Dispatch( const char *p );

// IPlugIn ------------------------------------------------------------
	const char* getMenuName();
	std::size_t getCommandCount();
	const char* getCommand( std::size_t n );
	const char* getCommandTitle( std::size_t n );
	const char* getGlobalCommand( std::size_t n );
};

CPluginSlot::CPluginSlot( QWidget* main_window, const char* name, const _QERPluginTable& table ){
	mpTable = &table;
	m_menu_name = name;

	const char* commands = mpTable->m_pfnQERPlug_GetCommandList();
	const char* titles = mpTable->m_pfnQERPlug_GetCommandTitleList();

	StringTokeniser commandTokeniser( commands, ",;" );
	StringTokeniser titleTokeniser( titles, ",;" );

	while ( 1 ) {
		const char* cmdToken = commandTokeniser.getToken();
		const char *titleToken = titleTokeniser.getToken();
		if( string_empty( cmdToken ) )
			break;
		m_CommandStrings.push_back( cmdToken );
		if ( string_empty( titleToken ) )
			m_CommandTitleStrings.push_back( cmdToken );
		else
			m_CommandTitleStrings.push_back( titleToken );

		m_callbacks.emplace_back( PluginCaller( this, m_CommandStrings.back().c_str() ) );
		StringBuffer str( 64 );
		{
			if( !string_equal_prefix_nocase( cmdToken, getMenuName() ) ){ //plugin name is not part of command name
				str.push_string( getMenuName() );
				str.push_string( "::" );
			}
			/* remove spaces + camelcasify */
			const char* p = cmdToken;
			bool wasspace = false;
			while( *p ){
				if( *p == ' ' ){
					wasspace = true;
				}
				else if( wasspace ){
					wasspace = false;
					str.push_back( std::toupper( *p ) );
				}
				else{
					str.push_back( *p );
				}
				++p;
			}
			/* del trailing periods */
			while( !str.empty() && str.back() == '.' ){
				str.pop_back();
			}
			*str.c_str() = std::tolower( *str.c_str() ); //put to the end of the list this way //not in Qt 🤔
		}
		m_globalCommandNames.emplace_back( str.c_str() );
		if ( !plugin_menu_special( cmdToken ) ) //ain't special
			GlobalCommands_insert( str.c_str(), makeCallback( m_callbacks.back() ) );
	}
	mpTable->m_pfnQERPlug_Init( 0, (void*)main_window );
}

const char* CPluginSlot::getMenuName(){
	return m_menu_name.c_str();
}

std::size_t CPluginSlot::getCommandCount(){
	return m_CommandStrings.size();
}

const char* CPluginSlot::getCommand( std::size_t n ){
	std::list<CopiedString>::iterator i = m_CommandStrings.begin();
	while ( n-- != 0 )
		++i;
	return ( *i ).c_str();
}

const char* CPluginSlot::getCommandTitle( std::size_t n ){
	std::list<CopiedString>::iterator i = m_CommandTitleStrings.begin();
	while ( n-- != 0 )
		++i;
	return ( *i ).c_str();
}

const char* CPluginSlot::getGlobalCommand( std::size_t n ){
	std::list<CopiedString>::iterator i = m_globalCommandNames.begin();
	while ( n-- != 0 )
		++i;
	return ( *i ).c_str();
}

void CPluginSlot::Dispatch( const char *p ){
	Vector3 vMin, vMax;
	Select_GetBounds( vMin, vMax );
	mpTable->m_pfnQERPlug_Dispatch( p, reinterpret_cast<float*>( &vMin ), reinterpret_cast<float*>( &vMax ), true ); //QE_SingleBrush(true));
}


class CPluginSlots
{
	std::list<CPluginSlot *> mSlots;
public:
	virtual ~CPluginSlots();

	void AddPluginSlot( QWidget* main_window, const char* name, const _QERPluginTable& table ){
		mSlots.push_back( new CPluginSlot( main_window, name, table ) );
	}

	void PopulateMenu( PluginsVisitor& menu );
};

CPluginSlots::~CPluginSlots(){
	std::list<CPluginSlot *>::iterator iSlot;
	for ( iSlot = mSlots.begin(); iSlot != mSlots.end(); ++iSlot )
	{
		delete *iSlot;
		*iSlot = 0;
	}
}

void CPluginSlots::PopulateMenu( PluginsVisitor& menu ){
	std::list<CPluginSlot *>::iterator iPlug;
	for ( iPlug = mSlots.begin(); iPlug != mSlots.end(); ++iPlug )
	{
		menu.visit( *( *iPlug ) );
	}
}

CPluginSlots g_plugin_slots;


void FillPluginSlots( CPluginSlots& slots, QWidget* main_window ){
	class AddPluginVisitor : public PluginModules::Visitor
	{
		CPluginSlots& m_slots;
		QWidget* m_main_window;
	public:
		AddPluginVisitor( CPluginSlots& slots, QWidget* main_window )
			: m_slots( slots ), m_main_window( main_window ){
		}
		void visit( const char* name, const _QERPluginTable& table ) const {
			m_slots.AddPluginSlot( m_main_window, name, table );
		}
	} visitor( slots, main_window );

	Radiant_getPluginModules().foreachModule( visitor );
}


CPlugInManager g_PlugInMgr;

CPlugInManager& GetPlugInMgr(){
	return g_PlugInMgr;
}

void CPlugInManager::Init( QWidget* main_window ){
	FillPluginSlots( g_plugin_slots, main_window );
}

void CPlugInManager::constructMenu( PluginsVisitor& menu ){
	g_plugin_slots.PopulateMenu( menu );
}

void CPlugInManager::Shutdown(){
}

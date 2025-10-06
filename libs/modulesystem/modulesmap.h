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

#pragma once

#include "modulesystem.h"
#include "string/string.h"
#include <map>

template<typename Type>
class ModulesMap : public Modules<Type>
{
	typedef std::map<CopiedString, Module*> modules_t;
	modules_t m_modules;
public:
	~ModulesMap(){
		for ( auto& [ name, module ] : m_modules )
		{
			module->release();
		}
	}

	typedef modules_t::const_iterator iterator;

	iterator begin() const {
		return m_modules.begin();
	}
	iterator end() const {
		return m_modules.end();
	}

	void insert( const char* name, Module& module ){
		module.capture();
		if ( globalModuleServer().getError() ) {
			module.release();
			globalModuleServer().setError( false );
		}
		else
		{
			m_modules.insert( modules_t::value_type( name, &module ) );
		}
	}

	Type* find( const char* name ){
		modules_t::iterator i = m_modules.find( name );
		if ( i != m_modules.end() ) {
			return static_cast<Type*>( Module_getTable( *( *i ).second ) );
		}
		return 0;
	}

	Type* findModule( const char* name ) override {
		return find( name );
	}
	void foreachModule( const typename Modules<Type>::Visitor& visitor ) override {
		for ( auto& [ name, module ] : m_modules )
		{
			visitor.visit( name.c_str(), *static_cast<const Type*>( Module_getTable( *module ) ) );
		}
	}
};

template<typename Type>
class InsertModules : public ModuleServer::Visitor
{
	ModulesMap<Type>& m_modules;
public:
	InsertModules( ModulesMap<Type>& modules )
		: m_modules( modules ){
	}
	void visit( const char* name, Module& module ) const override {
		m_modules.insert( name, module );
	}
};

template<typename Type>
class ModulesRef
{
	ModulesMap<Type> m_modules;
public:
	ModulesRef( const char* names ){
		if ( !globalModuleServer().getError() ) {
			if ( string_equal( names, "*" ) ) {
				InsertModules<Type> visitor( m_modules );
				globalModuleServer().foreachModule( Type::Name, Type::Version, visitor );
			}
			else
			{
				StringTokeniser tokeniser( names );
				for (;; )
				{
					const char* name = tokeniser.getToken();
					if ( string_empty( name ) ) {
						break;
					}
					Module* module = globalModuleServer().findModule( Type::Name, Type::Version, name );
					if ( module == 0 ) {
						globalErrorStream() << "ModulesRef::initialise: type=" << Quoted( Type::Name ) << " version=" << Quoted( Type::Version ) << " name=" << Quoted( name ) << " - not found\n";
						// do not fail on missing image or model plugin, they can be optional
						if ( !string_equal( Type::Name, "image" ) && !string_equal( Type::Name, "model" ) ){
							globalModuleServer().setError( true );
							break;
						}
					}
					else
					{
						m_modules.insert( name, *module );
					}
				}
			}
		}
	}
	ModulesMap<Type>& get(){
		return m_modules;
	}
};

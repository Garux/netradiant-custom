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

#include "eclass_fgd.h"

#include "debugging/debugging.h"

#include <map>
#include <set>

#include "iscriplib.h"

#include "string/string.h"
#include "eclasslib.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "stringio.h"
#include "stream/textfilestream.h"
#include "math/vector.h"

namespace
{
typedef std::map<const char*, EntityClass*, RawStringLessNoCase> EntityClasses;
EntityClasses g_EntityClassFGD_classes;
typedef std::map<const char*, EntityClass*, RawStringLessNoCase> BaseClasses;
BaseClasses g_EntityClassFGD_bases;
typedef std::map<CopiedString, ListAttributeType> ListAttributeTypes;
ListAttributeTypes g_listTypesFGD;

const auto pathLess = []( const CopiedString& one, const CopiedString& other ){
	return path_less( one.c_str(), other.c_str() );
};
std::set<CopiedString, decltype( pathLess )> g_loadedFgds( pathLess );
}


void EntityClassFGD_clear(){
	for ( auto [ name, eclass ] : g_EntityClassFGD_bases )
	{
		eclass_capture_state( eclass );
		eclass->free( eclass );
	}
	g_EntityClassFGD_classes.clear();
	g_EntityClassFGD_bases.clear();
	g_listTypesFGD.clear();
	g_loadedFgds.clear();
}

EntityClass* EntityClassFGD_insertUniqueBase( EntityClass* entityClass ){
	auto [ it, inserted ] = g_EntityClassFGD_bases.insert( BaseClasses::value_type( entityClass->name(), entityClass ) );
	if ( !inserted ) {
		globalErrorStream() << "duplicate base class: " << Quoted( entityClass->name() ) << '\n';
		eclass_capture_state( entityClass );
		entityClass->free( entityClass );
	}
	return it->second;
}

EntityClass* EntityClassFGD_insertUnique( EntityClass* entityClass ){
	auto [ it, inserted ] = g_EntityClassFGD_classes.insert( EntityClasses::value_type( entityClass->name(), entityClass ) );
	if ( !inserted ) {
		globalErrorStream() << "duplicate entity class: " << Quoted( entityClass->name() ) << '\n';
		eclass_capture_state( entityClass );
		entityClass->free( entityClass );
	}
	return it->second;
}

#define PARSE_ERROR "error parsing fgd entity class definition at line " << tokeniser.getLine() << ':' << tokeniser.getColumn()

static bool s_fgd_warned = false;

inline bool EntityClassFGD_parseToken( Tokeniser& tokeniser, const char* token ){
	const bool w = s_fgd_warned;
	const bool ok = string_equal( tokeniser.getToken(), token );
	if( !ok ){
		globalErrorStream() << PARSE_ERROR << "\nExpected " << Quoted( token ) << '\n';
		s_fgd_warned = true;
	}
	return w || ok;
}

#define ERROR_FGD( message )\
do{\
	if( s_fgd_warned )\
		globalErrorStream() << message << '\n';\
	else{\
		ERROR_MESSAGE( message );\
		s_fgd_warned = true;\
	}\
}while( false )


void EntityClassFGD_parseSplitString( Tokeniser& tokeniser, CopiedString& string ){
	StringOutputStream buffer( 256 );
	for (;; )
	{
		buffer << tokeniser.getToken();
		if ( !string_equal( tokeniser.getToken(), "+" ) ) {
			tokeniser.ungetToken();
			string = buffer;
			return;
		}
	}
}

void EntityClassFGD_parseClass( Tokeniser& tokeniser, bool fixedsize, bool isBase ){
	EntityClass* entityClass = Eclass_Alloc();
	entityClass->free = &Eclass_Free;
	entityClass->fixedsize = fixedsize;
	entityClass->inheritanceResolved = false;
	entityClass->mins = Vector3( -8, -8, -8 );
	entityClass->maxs = Vector3( 8, 8, 8 );

	for (;; )
	{
		const char* property = tokeniser.getToken();
		if ( string_equal( property, "=" ) ) {
			break;
		}
		else if ( string_equal( property, "base" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			for (;; )
			{
				const char* base = tokeniser.getToken();
				if ( string_equal( base, ")" ) ) {
					break;
				}
				else if ( !string_equal( base, "," ) ) {
					entityClass->m_parent.push_back( base );
				}
			}
		}
		else if ( string_equal( property, "size" ) ) {
			entityClass->sizeSpecified = true;
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			Tokeniser_getFloat( tokeniser, entityClass->mins.x() );
			Tokeniser_getFloat( tokeniser, entityClass->mins.y() );
			Tokeniser_getFloat( tokeniser, entityClass->mins.z() );
			const char* token = tokeniser.getToken();
			if ( string_equal( token, "," ) ) {
				Tokeniser_getFloat( tokeniser, entityClass->maxs.x() );
				Tokeniser_getFloat( tokeniser, entityClass->maxs.y() );
				Tokeniser_getFloat( tokeniser, entityClass->maxs.z() );
				ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
			}
			else
			{
				entityClass->maxs = entityClass->mins;
				vector3_negate( entityClass->mins );
				ASSERT_MESSAGE( string_equal( token, ")" ), "" );
			}
		}
		else if ( string_equal( property, "color" ) ) {
			entityClass->colorSpecified = true;
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			Tokeniser_getFloat( tokeniser, entityClass->color.x() );
			entityClass->color.x() /= 255.0;
			Tokeniser_getFloat( tokeniser, entityClass->color.y() );
			entityClass->color.y() /= 255.0;
			Tokeniser_getFloat( tokeniser, entityClass->color.z() );
			entityClass->color.z() /= 255.0;
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else if ( string_equal( property, "iconsprite" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			entityClass->m_modelpath = StringStream<64>( PathCleaned( tokeniser.getToken() ) );
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else if ( string_equal( property, "sprite" )
		       || string_equal( property, "decal" )
		       // hl2 below
		       || string_equal( property, "overlay" )
		       || string_equal( property, "light" )
		       || string_equal( property, "keyframe" )
		       || string_equal( property, "animator" )
		       || string_equal( property, "quadbounds" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		// hl2 below
		else if ( string_equal( property, "sphere" )
		       || string_equal( property, "sweptplayerhull" )
		       || string_equal( property, "studioprop" )
		       || string_equal( property, "lightprop" )
		       || string_equal( property, "lightcone" )
		       || string_equal( property, "sidelist" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			if ( string_equal( tokeniser.getToken(), ")" ) ) {
				tokeniser.ungetToken();
			}
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else if ( string_equal( property, "studio" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			const char *token = tokeniser.getToken();
			if ( string_equal( token, ")" ) ) {
				tokeniser.ungetToken();
			}
			else{
				entityClass->m_modelpath = token;
			}
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else if ( string_equal( property, "line" )
		       || string_equal( property, "cylinder" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			//const char* r =
			tokeniser.getToken();
			//const char* g =
			tokeniser.getToken();
			//const char* b =
			tokeniser.getToken();
			for (;; )
			{
				if ( string_equal( tokeniser.getToken(), ")" ) ) {
					tokeniser.ungetToken();
					break;
				}
				//const char* name =
				tokeniser.getToken();
			}
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else if ( string_equal( property, "wirebox" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			//const char* mins =
			tokeniser.getToken();
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "," ), PARSE_ERROR );
			//const char* maxs =
			tokeniser.getToken();
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else if ( string_equal( property, "halfgridsnap" ) ) {
		}
		else if ( string_equal( property, "flags" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			for (;; )
			{
				const char* base = tokeniser.getToken();
				if ( string_equal( base, ")" ) ) {
					break;
				}
				else if ( !string_equal( base, "," ) ) {
					if( string_equal_nocase( base, "Angle" ) ){
						entityClass->has_angles = true;
					}
				}
			}
		}
		else
		{
			ERROR_FGD( PARSE_ERROR );
		}
	}

	entityClass->name_set( tokeniser.getToken() );

	if ( !isBase ) {
		ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ":" ), PARSE_ERROR );

		EntityClassFGD_parseSplitString( tokeniser, entityClass->m_comments );

		const char* urlSeparator = tokeniser.getToken();
		if ( string_equal( urlSeparator, ":" ) ) {
			CopiedString tmp;
			EntityClassFGD_parseSplitString( tokeniser, tmp );
		}
		else
		{
			tokeniser.ungetToken();
		}
	}

	tokeniser.nextLine();

	ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "[" ), PARSE_ERROR );

	tokeniser.nextLine();

	for (;; )
	{
		CopiedString key = tokeniser.getToken();
		if ( string_equal( key.c_str(), "]" ) ) {
			tokeniser.nextLine();
			break;
		}

		if ( string_equal_nocase( key.c_str(), "input" )
		  || string_equal_nocase( key.c_str(), "output" ) ) {
			const char* name = tokeniser.getToken();
			if ( !string_equal( name, "(" ) ) {
				ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
				//const char* type =
				tokeniser.getToken();
				ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
				const char* descriptionSeparator = tokeniser.getToken();
				if ( string_equal( descriptionSeparator, ":" ) ) {
					CopiedString description;
					EntityClassFGD_parseSplitString( tokeniser, description );
				}
				else
				{
					tokeniser.ungetToken();
				}
				tokeniser.nextLine();
				continue;
			}
		}

		ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
		CopiedString type = tokeniser.getToken();
		ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );

		if ( string_equal_nocase( type.c_str(), "flags" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "=" ), PARSE_ERROR );
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "[" ), PARSE_ERROR );
			for (;; )
			{
				const char* flag = tokeniser.getToken();
				if ( string_equal( flag, "]" ) ) {
					tokeniser.nextLine();
					break;
				}
				else
				{
					const size_t bit = std::log2( atoi( flag ) );
					ASSERT_MESSAGE( bit < MAX_FLAGS, "invalid flag bit" << PARSE_ERROR );
					ASSERT_MESSAGE( string_empty( entityClass->flagnames[bit] ), "non-unique flag bit" << PARSE_ERROR );

					ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ":" ), PARSE_ERROR );

					const char* name = tokeniser.getToken();
					strncpy( entityClass->flagnames[bit], name, std::size( entityClass->flagnames[bit] ) - 1 );
					EntityClassAttribute *attribute = &EntityClass_insertAttribute( *entityClass, name, EntityClassAttribute( "flag", name ) ).second;
					entityClass->flagAttributes[bit] = attribute;
					{
						const char* defaultSeparator = tokeniser.getToken();
						if ( string_equal( defaultSeparator, ":" ) ) {
							tokeniser.getToken();
							{
								const char* descriptionSeparator = tokeniser.getToken();
								if ( string_equal( descriptionSeparator, ":" ) ) {
									EntityClassFGD_parseSplitString( tokeniser, attribute->m_description );
								}
								else
								{
									tokeniser.ungetToken();
								}
							}
						}
						else
						{
							tokeniser.ungetToken();
						}
					}
				}
				tokeniser.nextLine();
			}
		}
		else if ( string_equal_nocase( type.c_str(), "choices" ) ) {
			EntityClassAttribute attribute;

			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ":" ), PARSE_ERROR );
			attribute.m_name = tokeniser.getToken();
			const char* valueSeparator = tokeniser.getToken();
			if ( string_equal( valueSeparator, ":" ) ) {
				const char* value = tokeniser.getToken();
				if ( !string_equal( value, ":" ) ) {
					attribute.m_value = value;
				}
				else
				{
					tokeniser.ungetToken();
				}
				{
					const char* descriptionSeparator = tokeniser.getToken();
					if ( string_equal( descriptionSeparator, ":" ) ) {
						EntityClassFGD_parseSplitString( tokeniser, attribute.m_description );
					}
					else
					{
						tokeniser.ungetToken();
					}
				}
			}
			else
			{
				tokeniser.ungetToken();
			}
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "=" ), PARSE_ERROR );
			tokeniser.nextLine();
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "[" ), PARSE_ERROR );
			tokeniser.nextLine();

			const auto listTypeName = StringStream<64>( entityClass->name(), '_', key );
			attribute.m_type = listTypeName;

			ListAttributeType& listType = g_listTypesFGD[listTypeName.c_str()];

			for (;; )
			{
				const char* value = tokeniser.getToken();
				if ( string_equal( value, "]" ) ) {
					tokeniser.nextLine();
					break;
				}
				else
				{
					CopiedString tmp( value );
					ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ":" ), PARSE_ERROR );
					const char* name = tokeniser.getToken();
					listType.push_back( name, tmp.c_str() );

					const char* descriptionSeparator = tokeniser.getToken();
					if ( string_equal( descriptionSeparator, ":" ) ) {
						EntityClassFGD_parseSplitString( tokeniser, tmp );
					}
					else
					{
						tokeniser.ungetToken();
					}
				}
				tokeniser.nextLine();
			}

			for ( const auto& [ name, value ] : listType )
			{
				if ( string_equal( attribute.m_value.c_str(), name.c_str() ) ) {
					attribute.m_value = value;
				}
			}

			EntityClass_insertAttribute( *entityClass, key.c_str(), attribute );
		}
		else if ( string_equal_nocase( type.c_str(), "decal" ) ) {
		}
		else if ( string_equal_nocase( type.c_str(), "string" )
		       || string_equal_nocase( type.c_str(), "integer" )
		       || string_equal_nocase( type.c_str(), "studio" )
		       || string_equal_nocase( type.c_str(), "sprite" )
		       || string_equal_nocase( type.c_str(), "color255" )
		       || string_equal_nocase( type.c_str(), "color1" )
		       || string_equal_nocase( type.c_str(), "target_source" )
		       || string_equal_nocase( type.c_str(), "target_destination" )
		       || string_equal_nocase( type.c_str(), "sound" )
		       // hl2 below
		       || string_equal_nocase( type.c_str(), "angle" )
		       || string_equal_nocase( type.c_str(), "origin" )
		       || string_equal_nocase( type.c_str(), "float" )
		       || string_equal_nocase( type.c_str(), "node_dest" )
		       || string_equal_nocase( type.c_str(), "filterclass" )
		       || string_equal_nocase( type.c_str(), "vector" )
		       || string_equal_nocase( type.c_str(), "sidelist" )
		       || string_equal_nocase( type.c_str(), "material" )
		       || string_equal_nocase( type.c_str(), "vecline" )
		       || string_equal_nocase( type.c_str(), "axis" )
		       || string_equal_nocase( type.c_str(), "npcclass" )
		       || string_equal_nocase( type.c_str(), "target_name_or_class" )
		       || string_equal_nocase( type.c_str(), "pointentityclass" )
		       || string_equal_nocase( type.c_str(), "scene" ) ) {
			if ( !string_equal( tokeniser.getToken(), "readonly" ) ) {
				tokeniser.ungetToken();
			}

			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ":" ), PARSE_ERROR );
			const char* attributeType = "string";
			if ( string_equal_nocase( type.c_str(), "studio" ) ) {
				attributeType = "model";
			}
			else if ( string_equal_nocase( type.c_str(), "color1" ) ) {
				attributeType = "color";
			}

			EntityClassAttribute attribute;
			attribute.m_type = attributeType;
			attribute.m_name = tokeniser.getToken();

			const char* defaultSeparator = tokeniser.getToken();
			if ( string_equal( defaultSeparator, ":" ) ) {
				const char* value = tokeniser.getToken();
				if ( !string_equal( value, ":" ) ) {
					attribute.m_value = value;
				}
				else
				{
					tokeniser.ungetToken();
				}

				{
					const char* descriptionSeparator = tokeniser.getToken();
					if ( string_equal( descriptionSeparator, ":" ) ) {
						EntityClassFGD_parseSplitString( tokeniser, attribute.m_description );
					}
					else
					{
						tokeniser.ungetToken();
					}
				}
			}
			else
			{
				tokeniser.ungetToken();
			}
			EntityClass_insertAttribute( *entityClass, key.c_str(), attribute );
		}
		else
		{
			ERROR_FGD( "unknown key type: " << Quoted( type ) );
		}
		tokeniser.nextLine();
	}

	if ( isBase ) {
		EntityClassFGD_insertUniqueBase( entityClass );
	}
	else
	{
		EntityClassFGD_insertUnique( entityClass );
	}
}

void EntityClassFGD_loadUniqueFile( const char* filename );

void EntityClassFGD_parse( TextInputStream& inputStream, const char* path ){
	Tokeniser& tokeniser = GlobalScriptLibrary().m_pfnNewScriptTokeniser( inputStream );

	tokeniser.nextLine();

	for (;; )
	{
		const char* blockType = tokeniser.getToken();
		if ( blockType == 0 ) {
			break;
		}
		if ( string_equal_nocase( blockType, "@SolidClass" ) ) {
			EntityClassFGD_parseClass( tokeniser, false, false );
		}
		else if ( string_equal_nocase( blockType, "@BaseClass" ) ) {
			EntityClassFGD_parseClass( tokeniser, false, true );
		}
		else if ( string_equal_nocase( blockType, "@PointClass" )
		       // hl2 below
		       || string_equal_nocase( blockType, "@KeyFrameClass" )
		       || string_equal_nocase( blockType, "@MoveClass" )
		       || string_equal_nocase( blockType, "@FilterClass" )
		       || string_equal_nocase( blockType, "@NPCClass" ) ) {
			EntityClassFGD_parseClass( tokeniser, true, false );
		}
		// hl2 below
		else if ( string_equal( blockType, "@include" ) ) {
			EntityClassFGD_loadUniqueFile( StringStream( PathFilenameless( path ), tokeniser.getToken() ) );
		}
		else if ( string_equal( blockType, "@mapsize" ) ) {
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "(" ), PARSE_ERROR );
			//const char* min =
			tokeniser.getToken();
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, "," ), PARSE_ERROR );
			//const char* max =
			tokeniser.getToken();
			ASSERT_MESSAGE( EntityClassFGD_parseToken( tokeniser, ")" ), PARSE_ERROR );
		}
		else
		{
			ERROR_FGD( "unknown block type: " << Quoted( blockType ) );
		}
	}

	tokeniser.release();
}


void EntityClassFGD_loadFile( const char* filename ){
	TextFileInputStream file( filename );
	if ( !file.failed() ) {
		globalOutputStream() << "parsing entity classes from " << Quoted( filename ) << '\n';

		EntityClassFGD_parse( file, filename );
	}
}

void EntityClassFGD_loadUniqueFile( const char* filename ){
	if( g_loadedFgds.insert( filename ).second )
		EntityClassFGD_loadFile( filename );
}


void EntityClassFGD_resolveInheritance( EntityClass* derivedClass ){
	if ( derivedClass->inheritanceResolved == false ) {
		derivedClass->inheritanceResolved = true;
		for ( const auto& parentName : derivedClass->m_parent )
		{
			BaseClasses::iterator i = g_EntityClassFGD_bases.find( parentName.c_str() );
			if ( i == g_EntityClassFGD_bases.end() ) {
				i = g_EntityClassFGD_classes.find( parentName.c_str() );
				if ( i == g_EntityClassFGD_classes.end() ) {
					globalErrorStream() << "failed to find entityDef " << Quoted( parentName.c_str() ) << " inherited by " << Quoted( derivedClass->name() ) << '\n';
					continue;
				}
			}

			{
				EntityClass* parentClass = ( *i ).second;
				EntityClassFGD_resolveInheritance( parentClass );
				if ( !derivedClass->colorSpecified ) {
					derivedClass->colorSpecified = parentClass->colorSpecified;
					derivedClass->color = parentClass->color;
				}
				if ( !derivedClass->sizeSpecified ) {
					derivedClass->sizeSpecified = parentClass->sizeSpecified;
					derivedClass->mins = parentClass->mins;
					derivedClass->maxs = parentClass->maxs;
				}

				for ( const auto& [ key, attr ] : parentClass->m_attributes )
				{
					EntityClass_insertAttribute( *derivedClass, key.c_str(), attr );
				}

				for( size_t flag = 0; flag < MAX_FLAGS; ++flag ){
					if( !string_empty( parentClass->flagnames[flag] ) && string_empty( derivedClass->flagnames[flag] ) ){
						strncpy( derivedClass->flagnames[flag], parentClass->flagnames[flag], std::size( derivedClass->flagnames[flag] ) - 1 );
						// this ptr ref is cool, but requires parents to stay alive (e.g. bases)
						// non base parent also may be deleted during insertion to global entity stack, if entity is already present there
						// derivedClass->flagAttributes[flag] = parentClass->flagAttributes[flag];
						derivedClass->flagAttributes[flag] = &EntityClass_insertAttribute( *derivedClass,
							parentClass->flagAttributes[flag]->m_name.c_str(),
							*parentClass->flagAttributes[flag] ).second;
					}
				}
			}
		}
	}
}


void Eclass_ScanFile_fgd( EntityClassCollector& collector, const char *filename ){
	EntityClassFGD_loadUniqueFile( filename );
}

void EClass_finalize_fgd( EntityClassCollector& collector ){
	for ( auto [ name, eclass ] : g_EntityClassFGD_classes )
	{
		EntityClassFGD_resolveInheritance( eclass );
		if ( eclass->fixedsize && eclass->m_modelpath.empty() ) {
			if ( !eclass->sizeSpecified ) {
				globalErrorStream() << "size not specified for entity class: " << Quoted( eclass->name() ) << '\n';
			}
			if ( !eclass->colorSpecified ) {
				globalErrorStream() << "color not specified for entity class: " << Quoted( eclass->name() ) << '\n';
			}
		}
	}

	for ( auto [ name, eclass ] : g_EntityClassFGD_classes )
	{
		eclass_capture_state( eclass );
		collector.insert( eclass );
	}
	for( const auto& [ name, list ] : g_listTypesFGD )
	{
		collector.insert( name.c_str(), list );
	}
	EntityClassFGD_clear();
}


#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class EntityClassFgdDependencies : public GlobalShaderCacheModuleRef, public GlobalScripLibModuleRef
{
};

class EclassFgdAPI
{
	EntityClassScanner m_eclassfgd;
public:
	typedef EntityClassScanner Type;
	STRING_CONSTANT( Name, "fgd" );

	EclassFgdAPI(){
		m_eclassfgd.scanFile = &Eclass_ScanFile_fgd;
		m_eclassfgd.getExtension = [](){ return "fgd"; };
		m_eclassfgd.finalize = &EClass_finalize_fgd;
	}
	EntityClassScanner* getTable(){
		return &m_eclassfgd;
	}
};

typedef SingletonModule<EclassFgdAPI, EntityClassFgdDependencies> EclassFgdModule;
typedef Static<EclassFgdModule> StaticEclassFgdModule;
StaticRegisterModule staticRegisterEclassFgd( StaticEclassFgdModule::instance() );

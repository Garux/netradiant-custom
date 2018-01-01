/*
   Copyright (C) 2006, Stefan Greven.
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

#include "shaderplug.h"

#include "debugging/debugging.h"

#include <vector>
#include "string/string.h"
#include "modulesystem/singletonmodule.h"
#include "stream/stringstream.h"
#include "os/file.h"

#include "iplugin.h"
#include "qerplugin.h"
#include "ifilesystem.h"
#include "iarchive.h"
#include "ishaders.h"
#include "iscriplib.h"

#include "generic/callback.h"


class ShaderPlugPluginDependencies : public GlobalRadiantModuleRef,
	public GlobalFileSystemModuleRef,
	public GlobalShadersModuleRef
{
public:
	ShaderPlugPluginDependencies() :
		GlobalShadersModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "shaders" ) ){
	}
};

namespace Shaderplug
{
QWidget* g_window;

std::vector<const char*> archives;
std::set<CopiedString> shaders;
std::set<CopiedString> textures;

XmlTagBuilder TagBuilder;
void CreateTagFile();

const char* init( void* hApp, void* pMainWidget ){
	g_window = static_cast<QWidget*>( pMainWidget );
	return "";
}
const char* getName(){
	return "ShaderPlug";
}
const char* getCommandList(){
	return "About;Create tag file";
}
const char* getCommandTitleList(){
	return "";
}
void dispatch( const char* command, float* vMin, float* vMax, bool bSingleBrush ){
	if ( string_equal( command, "About" ) ) {
		GlobalRadiant().m_pfnMessageBox( g_window,
		                                 "Shaderplug (1.0)<br><br>"
		                                 "by Shaderman (<a href='mailto:shaderman@gmx.net'>shaderman@gmx.net</a>)",
		                                 "About",
		                                 EMessageBoxType::Info,
		                                 0 );
	}
	if ( string_equal( command, "Create tag file" ) ) {
		CreateTagFile();
	}
}

void loadArchiveFile( const char* filename ){
	archives.push_back( filename );
}

void LoadTextureFile( const char* filename ){
	char buffer[256] = "textures/";

	// append filename without trailing file extension (.tga or .jpg for example)
	strncat( buffer, filename, strlen( filename ) - 4 );

	// a shader with this name already exists
	if ( !shaders.contains( buffer ) ) {
		textures.insert( buffer );
	}
}

void GetTextures( const char* extension ){
	GlobalFileSystem().forEachFile( "textures/", extension, makeCallbackF( LoadTextureFile ), 0 );
}

void LoadShaderList( const char* filename ){
	if ( string_equal_prefix( filename, "textures/" ) ) {
		shaders.insert( filename );
	}
}

void GetAllShaders(){
	GlobalShaderSystem().foreachShaderName( makeCallbackF( LoadShaderList ) );
}

void GetArchiveList(){
	GlobalFileSystem().forEachArchive( makeCallbackF( loadArchiveFile ) );
	globalOutputStream() << "Shaderplug: " << Shaderplug::archives.size() << " archives found.\n";
}

void CreateTagFile(){
	const char* shader_type = GlobalRadiant().getGameDescriptionKeyValue( "shaders" );

	GetAllShaders();
	globalOutputStream() << "Shaderplug: " << shaders.size() << " shaders found.\n";

	if ( string_equal( shader_type, "quake3" ) ) {
		GetTextures( "jpg" );
		GetTextures( "tga" );
		GetTextures( "png" );

		globalOutputStream() << "Shaderplug: " << textures.size() << " textures found.\n";
	}

	if ( shaders.size() || textures.size() != 0 ) {
		globalOutputStream() << "Shaderplug: Creating XML tag file.\n";

		TagBuilder.CreateXmlDocument();

		for ( auto r_iter = textures.crbegin(); r_iter != textures.crend(); ++r_iter )
		{
			TagBuilder.AddShaderNode( r_iter->c_str(), TextureType::STOCK, NodeShaderType::TEXTURE );
		}

		for ( auto r_iter = shaders.crbegin(); r_iter != shaders.crend(); ++r_iter )
		{
			TagBuilder.AddShaderNode( r_iter->c_str(), TextureType::STOCK, NodeShaderType::SHADER );
		}

		// Get the tag file
		const auto tagFile = StringStream( GlobalRadiant().getLocalRcPath(), SHADERTAG_FILE );
		const auto message = StringStream( "Tag file saved to\n", tagFile, "\nPlease restart Radiant now.\n" );

		if ( file_exists( tagFile ) ) {
			EMessageBoxReturn result = GlobalRadiant().m_pfnMessageBox( g_window,
			                                                            "WARNING! A tag file already exists! Overwrite it?", "Overwrite tag file?",
			                                                            EMessageBoxType::Warning,
			                                                            eIDYES | eIDNO );

			if ( result == eIDYES ) {
				TagBuilder.SaveXmlDoc( tagFile );
				GlobalRadiant().m_pfnMessageBox( g_window, message, "INFO", EMessageBoxType::Info, 0 );
			}
		}
		else {
			TagBuilder.SaveXmlDoc( tagFile );
			GlobalRadiant().m_pfnMessageBox( g_window, message, "INFO", EMessageBoxType::Info, 0 );
		}
	}
	else {
		GlobalRadiant().m_pfnMessageBox( g_window,
		                                 "No shaders or textures found. No XML tag file created!\n",
		                                 "ERROR",
		                                 EMessageBoxType::Error, 0 );
	}
}
} // namespace

class ShaderPluginModule
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT( Name, "ShaderPlug" );

	ShaderPluginModule(){
		m_plugin.m_pfnQERPlug_Init = &Shaderplug::init;
		m_plugin.m_pfnQERPlug_GetName = &Shaderplug::getName;
		m_plugin.m_pfnQERPlug_GetCommandList = &Shaderplug::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &Shaderplug::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch = &Shaderplug::dispatch;
	}
	_QERPluginTable* getTable(){
		return &m_plugin;
	}
};

typedef SingletonModule<ShaderPluginModule, ShaderPlugPluginDependencies> SingletonShaderPluginModule;

SingletonShaderPluginModule g_ShaderPluginModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );

	g_ShaderPluginModule.selfRegister();
}

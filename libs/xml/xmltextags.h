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

#pragma once

#include <set>
#include <string/string.h>
#include <vector>

#include "iscriplib.h"

#include <libxml/xpath.h>
#include <libxml/xmlwriter.h>

constexpr char SHADERTAG_FILE[] = "shadertags.xml";

enum class NodeTagType
{
	TAG,
	EMPTY
};

enum class NodeShaderType
{
	SHADER,
	TEXTURE
};

enum class TextureType
{
	STOCK,
	CUSTOM
};

class XmlTagBuilder
{
private:
	CopiedString m_savefilename;
	xmlDocPtr doc{};
	xmlXPathContextPtr context{};

	xmlXPathObjectPtr XpathEval( const char* queryString ){
		const xmlChar* expression = (const xmlChar*)queryString;
		xmlXPathObjectPtr result = xmlXPathEvalExpression( expression, context );
		return result;
	};

	char* GetTagsXpathExpression( char* buffer, const char* shader, NodeTagType nodeTagType ){
		strcpy( buffer, "/root/*/*[@path='" );
		strcat( buffer, shader );

		switch ( nodeTagType )
		{
		case NodeTagType::TAG:
			strcat( buffer, "']/tag" );
			break;
		case NodeTagType::EMPTY:
			strcat( buffer, "']" );
		};

		return buffer;
	}

public:
	XmlTagBuilder();
	~XmlTagBuilder();

	bool CreateXmlDocument( const char* savefile = nullptr );
	bool OpenXmlDoc( const char* file, const char* savefile = 0 );
	bool SaveXmlDoc( const char* file );
	bool SaveXmlDoc();
	bool AddShaderNode( const char* shader, TextureType textureType, NodeShaderType nodeShaderType );
	bool DeleteShaderNode( const char* shader );
	bool CheckShaderTag( const char* shader );
	bool CheckShaderTag( const char* shader, const char* content );
	bool AddShaderTag( const char* shader, const char* content, NodeTagType nodeTagType );
	bool DeleteTag( const char* tag );
	int RenameShaderTag( const char* oldtag, CopiedString newtag );
	bool DeleteShaderTag( const char* shader, const char* tag );
	void GetShaderTags( const char* shader, std::vector<CopiedString>& tags );
	void GetUntagged( std::set<CopiedString>& shaders );
	void GetAllTags( std::set<CopiedString>& tags );
	void TagSearch( const char* expression, std::set<CopiedString>& paths );
};

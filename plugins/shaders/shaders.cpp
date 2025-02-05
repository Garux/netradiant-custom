/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Shaders Manager Plugin
//
// Leonardo Zide (leo@lokigames.com)
//

#include "shaders.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <list>

#include "ifilesystem.h"
#include "ishaders.h"
#include "iscriplib.h"
#include "itextures.h"
#include "qerplugin.h"
#include "irender.h"

#include "debugging/debugging.h"
#include "string/pooledstring.h"
#include "math/vector.h"
#include "generic/callback.h"
#include "generic/referencecounted.h"
#include "stream/memstream.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "os/path.h"
#include "os/dir.h"
#include "os/file.h"
#include "stringio.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "commandlib.h"
#include "moduleobservers.h"
#include "archivelib.h"
#include "imagelib.h"

const char* g_shadersExtension = "";
const char* g_shadersDirectory = "";
bool g_enableDefaultShaders = true;
ShaderLanguage g_shaderLanguage = SHADERLANGUAGE_QUAKE3;
bool g_useShaderList = true;
_QERPlugImageTable* g_bitmapModule = 0;
const char* g_texturePrefix = "textures/";

void ActiveShaders_IteratorBegin();
bool ActiveShaders_IteratorAtEnd();
IShader *ActiveShaders_IteratorCurrent();
void ActiveShaders_IteratorIncrement();
Callback<void()> g_ActiveShadersChangedNotify;

void FreeShaders();
void LoadShaderFile( const char *filename );

/*!
   NOTE TTimo: there is an important distinction between SHADER_NOT_FOUND and SHADER_NOTEX:
   SHADER_NOT_FOUND means we didn't find the raw texture or the shader for this
   SHADER_NOTEX means we recognize this as a shader script, but we are missing the texture to represent it
   this was in the initial design of the shader code since early GtkRadiant alpha, and got sort of foxed in 1.2 and put back in
 */

Image* loadBitmap( void* environment, const char* name ){
	DirectoryArchiveFile file( name, name );
	if ( !file.failed() ) {
		return g_bitmapModule->loadImage( file );
	}
	return 0;
}

inline byte* getPixel( byte* pixels, int width, int height, int x, int y ){
	return pixels + ( ( ( ( ( y + height ) % height ) * width ) + ( ( x + width ) % width ) ) * 4 );
}

class KernelElement
{
public:
	int x, y;
	float w;
};

Image& convertHeightmapToNormalmap( Image& heightmap, float scale ){
	int w = heightmap.getWidth();
	int h = heightmap.getHeight();

	Image& normalmap = *( new RGBAImage( heightmap.getWidth(), heightmap.getHeight() ) );

	byte* in = heightmap.getRGBAPixels();
	byte* out = normalmap.getRGBAPixels();

#if 1
	// no filtering
	const int kernelSize = 2;
	KernelElement kernel_du[kernelSize] = {
		{-1, 0,-0.5f },
		{ 1, 0, 0.5f }
	};
	KernelElement kernel_dv[kernelSize] = {
		{ 0, 1, 0.5f },
		{ 0,-1,-0.5f }
	};
#else
	// 3x3 Prewitt
	const int kernelSize = 6;
	KernelElement kernel_du[kernelSize] = {
		{-1, 1,-1.0f },
		{-1, 0,-1.0f },
		{-1,-1,-1.0f },
		{ 1, 1, 1.0f },
		{ 1, 0, 1.0f },
		{ 1,-1, 1.0f }
	};
	KernelElement kernel_dv[kernelSize] = {
		{-1, 1, 1.0f },
		{ 0, 1, 1.0f },
		{ 1, 1, 1.0f },
		{-1,-1,-1.0f },
		{ 0,-1,-1.0f },
		{ 1,-1,-1.0f }
	};
#endif

	int x, y = 0;
	while ( y < h )
	{
		x = 0;
		while ( x < w )
		{
			float du = 0;
			for ( KernelElement* i = kernel_du; i != kernel_du + kernelSize; ++i )
			{
				du += ( getPixel( in, w, h, x + ( *i ).x, y + ( *i ).y )[0] / 255.0 ) * ( *i ).w;
			}
			float dv = 0;
			for ( KernelElement* i = kernel_dv; i != kernel_dv + kernelSize; ++i )
			{
				dv += ( getPixel( in, w, h, x + ( *i ).x, y + ( *i ).y )[0] / 255.0 ) * ( *i ).w;
			}

			float nx = -du * scale;
			float ny = -dv * scale;
			float nz = 1.0;

			// Normalize
			float norm = 1.0 / sqrt( nx * nx + ny * ny + nz * nz );
			out[0] = float_to_integer( ( ( nx * norm ) + 1 ) * 127.5 );
			out[1] = float_to_integer( ( ( ny * norm ) + 1 ) * 127.5 );
			out[2] = float_to_integer( ( ( nz * norm ) + 1 ) * 127.5 );
			out[3] = 255;

			x++;
			out += 4;
		}

		y++;
	}

	return normalmap;
}

Image* loadHeightmap( void* environment, const char* name ){
	Image* heightmap = GlobalTexturesCache().loadImage( name );
	if ( heightmap != 0 ) {
		Image& normalmap = convertHeightmapToNormalmap( *heightmap, *reinterpret_cast<float*>( environment ) );
		heightmap->release();
		return &normalmap;
	}
	return 0;
}


Image* loadSpecial( void* environment, const char* name ){
	if ( *name == '_' ) { // special image
		Image* image = loadBitmap( environment, StringStream( GlobalRadiant().getAppPath(), "bitmaps/", name + 1, ".png" ) );
		if ( image != 0 ) {
			return image;
		}
	}
	return GlobalTexturesCache().loadImage( name );
}

class ShaderPoolContext
{
};
typedef Static<StringPool, ShaderPoolContext> ShaderPool;
typedef PooledString<ShaderPool> ShaderString;
typedef ShaderString ShaderVariable;
typedef ShaderString ShaderValue;
typedef CopiedString TextureExpression;

// clean a texture name to the qtexture_t name format we use internally
// NOTE: case sensitivity: the engine is case sensitive. we store the shader name with case information and save with case
// information as well. but we assume there won't be any case conflict and so when doing lookups based on shader name,
// we compare as case insensitive. That is Radiant is case insensitive, but knows that the engine is case sensitive.
//++timo FIXME: we need to put code somewhere to detect when two shaders that are case insensitive equal are present
template<typename StringType>
void parseTextureName( StringType& name, const char* token ){
	name = StringStream<64>( PathCleaned( PathExtensionless( token ) ) ).c_str(); // remove extension
}

bool Tokeniser_parseTextureName( Tokeniser& tokeniser, TextureExpression& name ){
	const char* token = tokeniser.getToken();
	if ( token == 0 ) {
		Tokeniser_unexpectedError( tokeniser, token, "#texture-name" );
		return false;
	}
	parseTextureName( name, token );
	return true;
}

bool Tokeniser_parseShaderName( Tokeniser& tokeniser, CopiedString& name ){
	const char* token = tokeniser.getToken();
	if ( token == 0 ) {
		Tokeniser_unexpectedError( tokeniser, token, "#shader-name" );
		return false;
	}
	parseTextureName( name, token );
	return true;
}

bool Tokeniser_parseString( Tokeniser& tokeniser, ShaderString& string ){
	const char* token = tokeniser.getToken();
	if ( token == 0 ) {
		Tokeniser_unexpectedError( tokeniser, token, "#string" );
		return false;
	}
	string = token;
	return true;
}



typedef std::list<ShaderVariable> ShaderParameters;
typedef std::list<ShaderVariable> ShaderArguments;

typedef std::pair<ShaderVariable, ShaderVariable> BlendFuncExpression;

class ShaderTemplate
{
	std::size_t m_refcount;
	CopiedString m_Name;
public:

	ShaderParameters m_params;

	TextureExpression m_textureName;
	TextureExpression m_skyBox;
	TextureExpression m_diffuse;
	TextureExpression m_bump;
	ShaderValue m_heightmapScale;
	TextureExpression m_specular;
	TextureExpression m_lightFalloffImage;

	int m_nFlags;
	float m_fTrans;

// alphafunc stuff
	IShader::EAlphaFunc m_AlphaFunc;
	float m_AlphaRef;
// cull stuff
	IShader::ECull m_Cull;

	ShaderTemplate() :
		m_refcount( 0 ){
		m_nFlags = 0;
		m_fTrans = 1.0f;
	}

	void IncRef(){
		++m_refcount;
	}
	void DecRef(){
		ASSERT_MESSAGE( m_refcount != 0, "shader reference-count going below zero" );
		if ( --m_refcount == 0 ) {
			delete this;
		}
	}

	std::size_t refcount(){
		return m_refcount;
	}

	const char* getName() const {
		return m_Name.c_str();
	}
	void setName( const char* name ){
		m_Name = name;
	}

// -----------------------------------------

	bool parseDoom3( Tokeniser& tokeniser );
	bool parseQuake3( Tokeniser& tokeniser );
	bool parseTemplate( Tokeniser& tokeniser );


	void CreateDefault( const char *name ){
		if ( g_enableDefaultShaders ) {
			m_textureName = name;
		}
		else
		{
			m_textureName = "";
		}
		setName( name );
	}


	class MapLayerTemplate
	{
		TextureExpression m_texture;
		BlendFuncExpression m_blendFunc;
		bool m_clampToBorder;
		ShaderValue m_alphaTest;
	public:
		MapLayerTemplate( const TextureExpression& texture, const BlendFuncExpression& blendFunc, bool clampToBorder, const ShaderValue& alphaTest ) :
			m_texture( texture ),
			m_blendFunc( blendFunc ),
			m_clampToBorder( false ),
			m_alphaTest( alphaTest ){
		}
		const TextureExpression& texture() const {
			return m_texture;
		}
		const BlendFuncExpression& blendFunc() const {
			return m_blendFunc;
		}
		bool clampToBorder() const {
			return m_clampToBorder;
		}
		const ShaderValue& alphaTest() const {
			return m_alphaTest;
		}
	};
	typedef std::vector<MapLayerTemplate> MapLayers;
	MapLayers m_layers;
};


bool Doom3Shader_parseHeightmap( Tokeniser& tokeniser, TextureExpression& bump, ShaderValue& heightmapScale ){
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "(" ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, bump ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "," ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseString( tokeniser, heightmapScale ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, ")" ) );
	return true;
}

bool Doom3Shader_parseAddnormals( Tokeniser& tokeniser, TextureExpression& bump ){
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "(" ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, bump ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "," ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "heightmap" ) );
	TextureExpression heightmapName;
	ShaderValue heightmapScale;
	RETURN_FALSE_IF_FAIL( Doom3Shader_parseHeightmap( tokeniser, heightmapName, heightmapScale ) );
	RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, ")" ) );
	return true;
}

bool Doom3Shader_parseBumpmap( Tokeniser& tokeniser, TextureExpression& bump, ShaderValue& heightmapScale ){
	const char* token = tokeniser.getToken();
	if ( token == 0 ) {
		Tokeniser_unexpectedError( tokeniser, token, "#bumpmap" );
		return false;
	}
	if ( string_equal( token, "heightmap" ) ) {
		RETURN_FALSE_IF_FAIL( Doom3Shader_parseHeightmap( tokeniser, bump, heightmapScale ) );
	}
	else if ( string_equal( token, "addnormals" ) ) {
		RETURN_FALSE_IF_FAIL( Doom3Shader_parseAddnormals( tokeniser, bump ) );
	}
	else
	{
		parseTextureName( bump, token );
	}
	return true;
}

enum LayerTypeId
{
	LAYER_NONE,
	LAYER_BLEND,
	LAYER_DIFFUSEMAP,
	LAYER_BUMPMAP,
	LAYER_SPECULARMAP
};

class LayerTemplate
{
public:
	LayerTypeId m_type;
	TextureExpression m_texture;
	BlendFuncExpression m_blendFunc;
	bool m_clampToBorder;
	ShaderValue m_alphaTest;
	ShaderValue m_heightmapScale;

	LayerTemplate() : m_type( LAYER_NONE ), m_blendFunc( "GL_ONE", "GL_ZERO" ), m_clampToBorder( false ), m_alphaTest( "-1" ), m_heightmapScale( "0" ){
	}
};

bool parseShaderParameters( Tokeniser& tokeniser, ShaderParameters& params ){
	Tokeniser_parseToken( tokeniser, "(" );
	for (;; )
	{
		const char* param = tokeniser.getToken();
		if ( string_equal( param, ")" ) ) {
			break;
		}
		params.push_back( param );
		const char* comma = tokeniser.getToken();
		if ( string_equal( comma, ")" ) ) {
			break;
		}
		if ( !string_equal( comma, "," ) ) {
			Tokeniser_unexpectedError( tokeniser, comma, "," );
			return false;
		}
	}
	return true;
}

bool ShaderTemplate::parseTemplate( Tokeniser& tokeniser ){
	m_Name = tokeniser.getToken();
	if ( !parseShaderParameters( tokeniser, m_params ) ) {
		globalErrorStream() << "shader template: " << makeQuoted( m_Name ) << ": parameter parse failed\n";
		return false;
	}

	return parseDoom3( tokeniser );
}

bool ShaderTemplate::parseDoom3( Tokeniser& tokeniser ){
	LayerTemplate currentLayer;
	bool isFog = false;

	// we need to read until we hit a balanced }
	int depth = 0;
	for (;; )
	{
		tokeniser.nextLine();
		const char* token = tokeniser.getToken();

		if ( token == 0 ) {
			return false;
		}

		if ( string_equal( token, "{" ) ) {
			++depth;
			continue;
		}
		else if ( string_equal( token, "}" ) ) {
			--depth;
			if ( depth < 0 ) { // error
				return false;
			}
			if ( depth == 0 ) { // end of shader
				break;
			}
			if ( depth == 1 ) { // end of layer
				if ( currentLayer.m_type == LAYER_DIFFUSEMAP ) {
					m_diffuse = currentLayer.m_texture;
				}
				else if ( currentLayer.m_type == LAYER_BUMPMAP ) {
					m_bump = currentLayer.m_texture;
				}
				else if ( currentLayer.m_type == LAYER_SPECULARMAP ) {
					m_specular = currentLayer.m_texture;
				}
				else if ( !currentLayer.m_texture.empty() ) {
					m_layers.push_back( MapLayerTemplate(
					                        currentLayer.m_texture.c_str(),
					                        currentLayer.m_blendFunc,
					                        currentLayer.m_clampToBorder,
					                        currentLayer.m_alphaTest
					                    ) );
				}
				currentLayer.m_type = LAYER_NONE;
				currentLayer.m_texture = "";
			}
			continue;
		}

		if ( depth == 2 ) { // in layer
			if ( string_equal_nocase( token, "blend" ) ) {
				const char* blend = tokeniser.getToken();

				if ( blend == 0 ) {
					Tokeniser_unexpectedError( tokeniser, blend, "#blend" );
					return false;
				}

				if ( string_equal_nocase( blend, "diffusemap" ) ) {
					currentLayer.m_type = LAYER_DIFFUSEMAP;
				}
				else if ( string_equal_nocase( blend, "bumpmap" ) ) {
					currentLayer.m_type = LAYER_BUMPMAP;
				}
				else if ( string_equal_nocase( blend, "specularmap" ) ) {
					currentLayer.m_type = LAYER_SPECULARMAP;
				}
				else
				{
					currentLayer.m_blendFunc.first = blend;

					const char* comma = tokeniser.getToken();

					if ( comma == 0 ) {
						Tokeniser_unexpectedError( tokeniser, comma, "#comma" );
						return false;
					}

					if ( string_equal( comma, "," ) ) {
						RETURN_FALSE_IF_FAIL( Tokeniser_parseString( tokeniser, currentLayer.m_blendFunc.second ) );
					}
					else
					{
						currentLayer.m_blendFunc.second = "";
						tokeniser.ungetToken();
					}
				}
			}
			else if ( string_equal_nocase( token, "map" ) ) {
				if ( currentLayer.m_type == LAYER_BUMPMAP ) {
					RETURN_FALSE_IF_FAIL( Doom3Shader_parseBumpmap( tokeniser, currentLayer.m_texture, currentLayer.m_heightmapScale ) );
				}
				else
				{
					const char* map = tokeniser.getToken();

					if ( map == 0 ) {
						Tokeniser_unexpectedError( tokeniser, map, "#map" );
						return false;
					}

					if ( string_equal( map, "makealpha" ) ) {
						RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "(" ) );
						const char* texture = tokeniser.getToken();
						if ( texture == 0 ) {
							Tokeniser_unexpectedError( tokeniser, texture, "#texture" );
							return false;
						}
						currentLayer.m_texture = texture;
						RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, ")" ) );
					}
					else
					{
						parseTextureName( currentLayer.m_texture, map );
					}
				}
			}
			else if ( string_equal_nocase( token, "zeroclamp" ) ) {
				currentLayer.m_clampToBorder = true;
			}
#if 0
			else if ( string_equal_nocase( token, "alphaTest" ) ) {
				Tokeniser_getFloat( tokeniser, currentLayer.m_alphaTest );
			}
#endif
		}
		else if ( depth == 1 ) {
			if ( string_equal_nocase( token, "qer_editorimage" ) ) {
				RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, m_textureName ) );
			}
			else if ( string_equal_nocase( token, "qer_trans" ) ) {
				m_fTrans = string_read_float( tokeniser.getToken() );
				m_nFlags |= QER_TRANS;
			}
			else if ( string_equal_nocase( token, "translucent" ) ) {
				m_fTrans = 1;
				m_nFlags |= QER_TRANS;
			}
			else if ( string_equal( token, "DECAL_MACRO" ) ) {
				m_fTrans = 1;
				m_nFlags |= QER_TRANS;
			}
			else if ( string_equal_nocase( token, "bumpmap" ) ) {
				RETURN_FALSE_IF_FAIL( Doom3Shader_parseBumpmap( tokeniser, m_bump, m_heightmapScale ) );
			}
			else if ( string_equal_nocase( token, "diffusemap" ) ) {
				RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, m_diffuse ) );
			}
			else if ( string_equal_nocase( token, "specularmap" ) ) {
				RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, m_specular ) );
			}
			else if ( string_equal_nocase( token, "twosided" ) ) {
				m_Cull = IShader::eCullNone;
				m_nFlags |= QER_CULL;
			}
			else if ( string_equal_nocase( token, "nodraw" ) ) {
				m_nFlags |= QER_NODRAW;
			}
			else if ( string_equal_nocase( token, "nonsolid" ) ) {
				m_nFlags |= QER_NONSOLID;
			}
			else if ( string_equal_nocase( token, "liquid" ) ) {
				m_nFlags |= QER_LIQUID;
			}
			else if ( string_equal_nocase( token, "areaportal" ) ) {
				m_nFlags |= QER_AREAPORTAL;
			}
			else if ( string_equal_nocase( token, "playerclip" )
			       || string_equal_nocase( token, "monsterclip" )
			       || string_equal_nocase( token, "ikclip" )
			       || string_equal_nocase( token, "moveableclip" ) ) {
				m_nFlags |= QER_CLIP;
			}
			if ( string_equal_nocase( token, "fogLight" ) ) {
				isFog = true;
			}
			else if ( !isFog && string_equal_nocase( token, "lightFalloffImage" ) ) {
				const char* lightFalloffImage = tokeniser.getToken();
				if ( lightFalloffImage == 0 ) {
					Tokeniser_unexpectedError( tokeniser, lightFalloffImage, "#lightFalloffImage" );
					return false;
				}
				if ( string_equal_nocase( lightFalloffImage, "makeintensity" ) ) {
					RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, "(" ) );
					TextureExpression name;
					RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, name ) );
					m_lightFalloffImage = name;
					RETURN_FALSE_IF_FAIL( Tokeniser_parseToken( tokeniser, ")" ) );
				}
				else
				{
					m_lightFalloffImage = lightFalloffImage;
				}
			}
		}
	}

	if ( m_textureName.empty() ) {
		m_textureName = m_diffuse;
	}

	return true;
}

typedef SmartPointer<ShaderTemplate> ShaderTemplatePointer;
typedef std::map<CopiedString, ShaderTemplatePointer> ShaderTemplateMap;

ShaderTemplateMap g_shaders;
ShaderTemplateMap g_shaderTemplates;

ShaderTemplate* findTemplate( const char* name ){
	ShaderTemplateMap::iterator i = g_shaderTemplates.find( name );
	if ( i != g_shaderTemplates.end() ) {
		return ( *i ).second.get();
	}
	return 0;
}

class ShaderDefinition
{
public:
	ShaderDefinition( ShaderTemplate* shaderTemplate, const ShaderArguments& args, const char* filename )
		: shaderTemplate( shaderTemplate ), args( args ), filename( filename ){
	}
	ShaderTemplate* shaderTemplate;
	ShaderArguments args;
	const char* filename;
};

typedef std::map<CopiedString, ShaderDefinition, shader_less_t> ShaderDefinitionMap;

ShaderDefinitionMap g_shaderDefinitions;

bool parseTemplateInstance( Tokeniser& tokeniser, const char* filename ){
	CopiedString name;
	RETURN_FALSE_IF_FAIL( Tokeniser_parseShaderName( tokeniser, name ) );
	const char* templateName = tokeniser.getToken();
	ShaderTemplate* shaderTemplate = findTemplate( templateName );
	if ( shaderTemplate == 0 ) {
		globalErrorStream() << "shader instance: " << makeQuoted( name ) << ": shader template not found: " << makeQuoted( templateName ) << '\n';
	}

	ShaderArguments args;
	if ( !parseShaderParameters( tokeniser, args ) ) {
		globalErrorStream() << "shader instance: " << makeQuoted( name ) << ": argument parse failed\n";
		return false;
	}

	if ( shaderTemplate != 0 ) {
		if ( !g_shaderDefinitions.insert( ShaderDefinitionMap::value_type( name, ShaderDefinition( shaderTemplate, args, filename ) ) ).second ) {
			globalErrorStream() << "shader instance: " << makeQuoted( name ) << ": already exists, second definition ignored\n";
		}
	}
	return true;
}


const char* evaluateShaderValue( const char* value, const ShaderParameters& params, const ShaderArguments& args ){
	ShaderArguments::const_iterator j = args.begin();
	for ( ShaderParameters::const_iterator i = params.begin(); i != params.end(); ++i, ++j )
	{
		const char* other = ( *i ).c_str();
		if ( string_equal( value, other ) ) {
			return ( *j ).c_str();
		}
	}
	return value;
}

///\todo BlendFunc parsing
BlendFunc evaluateBlendFunc( const BlendFuncExpression& blendFunc, const ShaderParameters& params, const ShaderArguments& args ){
	return BlendFunc( BLEND_ONE, BLEND_ZERO );
}

qtexture_t* evaluateTexture( const TextureExpression& texture, const ShaderParameters& params, const ShaderArguments& args, const LoadImageCallback& loader = GlobalTexturesCache().defaultLoader() ){
	StringOutputStream result( 64 );
	const char* expression = texture.c_str();
	const char* end = expression + string_length( expression );
	if ( !string_empty( expression ) ) {
		for (;; )
		{
			const char* best = end;
			const char* bestParam = 0;
			const char* bestArg = 0;
			ShaderArguments::const_iterator j = args.begin();
			for ( ShaderParameters::const_iterator i = params.begin(); i != params.end(); ++i, ++j )
			{
				const char* found = strstr( expression, ( *i ).c_str() );
				if ( found != 0 && found < best ) {
					best = found;
					bestParam = ( *i ).c_str();
					bestArg = ( *j ).c_str();
				}
			}
			if ( best != end ) {
				result << StringRange( expression, best );
				result << PathCleaned( bestArg );
				expression = best + string_length( bestParam );
			}
			else
			{
				break;
			}
		}
		result << expression;
	}
	return GlobalTexturesCache().capture( loader, result );
}

float evaluateFloat( const ShaderValue& value, const ShaderParameters& params, const ShaderArguments& args ){
	const char* result = evaluateShaderValue( value.c_str(), params, args );
	float f;
	if ( !string_parse_float( result, f ) ) {
		globalErrorStream() << "parsing float value failed: " << makeQuoted( result ) << '\n';
		return 1.f;
	}
	return f;
}

BlendFactor evaluateBlendFactor( const ShaderValue& value, const ShaderParameters& params, const ShaderArguments& args ){
	const char* result = evaluateShaderValue( value.c_str(), params, args );

	if ( string_equal_nocase( result, "gl_zero" ) ) {
		return BLEND_ZERO;
	}
	if ( string_equal_nocase( result, "gl_one" ) ) {
		return BLEND_ONE;
	}
	if ( string_equal_nocase( result, "gl_src_color" ) ) {
		return BLEND_SRC_COLOUR;
	}
	if ( string_equal_nocase( result, "gl_one_minus_src_color" ) ) {
		return BLEND_ONE_MINUS_SRC_COLOUR;
	}
	if ( string_equal_nocase( result, "gl_src_alpha" ) ) {
		return BLEND_SRC_ALPHA;
	}
	if ( string_equal_nocase( result, "gl_one_minus_src_alpha" ) ) {
		return BLEND_ONE_MINUS_SRC_ALPHA;
	}
	if ( string_equal_nocase( result, "gl_dst_color" ) ) {
		return BLEND_DST_COLOUR;
	}
	if ( string_equal_nocase( result, "gl_one_minus_dst_color" ) ) {
		return BLEND_ONE_MINUS_DST_COLOUR;
	}
	if ( string_equal_nocase( result, "gl_dst_alpha" ) ) {
		return BLEND_DST_ALPHA;
	}
	if ( string_equal_nocase( result, "gl_one_minus_dst_alpha" ) ) {
		return BLEND_ONE_MINUS_DST_ALPHA;
	}
	if ( string_equal_nocase( result, "gl_src_alpha_saturate" ) ) {
		return BLEND_SRC_ALPHA_SATURATE;
	}

	globalErrorStream() << "parsing blend-factor value failed: " << makeQuoted( result ) << '\n';
	return BLEND_ZERO;
}

class CShader : public IShader
{
	std::size_t m_refcount;

	const ShaderTemplate& m_template;
	const ShaderArguments& m_args;
	const char* m_filename;
// name is shader-name, otherwise texture-name (if not a real shader)
	CopiedString m_Name;

	qtexture_t* m_pTexture;
	qtexture_t* m_pSkyBox;
	qtexture_t* m_notfound;
	qtexture_t* m_pDiffuse;
	float m_heightmapScale;
	qtexture_t* m_pBump;
	qtexture_t* m_pSpecular;
	qtexture_t* m_pLightFalloffImage;
	BlendFunc m_blendFunc;

	bool m_bInUse;


public:
	static bool m_lightingEnabled;

	CShader( const ShaderDefinition& definition ) :
		m_refcount( 0 ),
		m_template( *definition.shaderTemplate ),
		m_args( definition.args ),
		m_filename( definition.filename ),
		m_blendFunc( BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA ),
		m_bInUse( false ){
		m_pTexture = 0;
		m_pSkyBox = 0;
		m_pDiffuse = 0;
		m_pBump = 0;
		m_pSpecular = 0;

		m_notfound = 0;

		realise();
	}
	virtual ~CShader(){
		unrealise();

		ASSERT_MESSAGE( m_refcount == 0, "deleting active shader" );
	}

// IShaders implementation -----------------
	void IncRef() override {
		++m_refcount;
	}
	void DecRef() override {
		ASSERT_MESSAGE( m_refcount != 0, "shader reference-count going below zero" );
		if ( --m_refcount == 0 ) {
			delete this;
		}
	}

	std::size_t refcount(){
		return m_refcount;
	}

// get/set the qtexture_t* Radiant uses to represent this shader object
	qtexture_t* getTexture() const override {
		return m_pTexture;
	}
	qtexture_t* getSkyBox() override {
		/* load skybox if only used */
		if( m_pSkyBox == nullptr && !m_template.m_skyBox.empty() )
			m_pSkyBox = GlobalTexturesCache().capture( LoadImageCallback( 0, GlobalTexturesCache().defaultLoader().m_func, true ), m_template.m_skyBox.c_str() );

		return m_pSkyBox;
	}
	qtexture_t* getDiffuse() const override {
		return m_pDiffuse;
	}
	qtexture_t* getBump() const override {
		return m_pBump;
	}
	qtexture_t* getSpecular() const override {
		return m_pSpecular;
	}
// get shader name
	const char* getName() const override {
		return m_Name.c_str();
	}
	bool IsInUse() const override {
		return m_bInUse;
	}
	void SetInUse( bool bInUse ) override {
		m_bInUse = bInUse;
		g_ActiveShadersChangedNotify();
	}
// get the shader flags
	int getFlags() const override {
		return m_template.m_nFlags;
	}
// get the transparency value
	float getTrans() const override {
		return m_template.m_fTrans;
	}
// test if it's a true shader, or a default shader created to wrap around a texture
	bool IsDefault() const override {
		return string_empty( m_filename );
	}
// get the alphaFunc
	void getAlphaFunc( EAlphaFunc *func, float *ref ) override {
		*func = m_template.m_AlphaFunc;
		*ref = m_template.m_AlphaRef;
	};
	BlendFunc getBlendFunc() const override {
		return m_blendFunc;
	}
// get the cull type
	ECull getCull() override {
		return m_template.m_Cull;
	};
// get shader file name (ie the file where this one is defined)
	const char* getShaderFileName() const override {
		return m_filename;
	}
// -----------------------------------------

	void realise(){
		m_pTexture = evaluateTexture( m_template.m_textureName, m_template.m_params, m_args );

		if ( m_pTexture->texture_number == 0 ) {
			m_notfound = m_pTexture;

			const auto name = StringStream( GlobalRadiant().getAppPath(), "bitmaps/", ( IsDefault() ? "notex.png" : "shadernotex.png" ) );
			m_pTexture = GlobalTexturesCache().capture( LoadImageCallback( 0, loadBitmap ), name );
		}

		realiseLighting();
	}

	void unrealise(){
		GlobalTexturesCache().release( m_pTexture );

		if ( m_notfound != 0 ) {
			GlobalTexturesCache().release( m_notfound );
		}

		if ( m_pSkyBox != 0 ) {
			GlobalTexturesCache().release( m_pSkyBox );
		}

		unrealiseLighting();
	}

	void realiseLighting(){
		if ( m_lightingEnabled ) {
			LoadImageCallback loader = GlobalTexturesCache().defaultLoader();
			if ( !string_empty( m_template.m_heightmapScale.c_str() ) ) {
				m_heightmapScale = evaluateFloat( m_template.m_heightmapScale, m_template.m_params, m_args );
				loader = LoadImageCallback( &m_heightmapScale, loadHeightmap );
			}
			m_pDiffuse = evaluateTexture( m_template.m_diffuse, m_template.m_params, m_args );
			m_pBump = evaluateTexture( m_template.m_bump, m_template.m_params, m_args, loader );
			m_pSpecular = evaluateTexture( m_template.m_specular, m_template.m_params, m_args );
			m_pLightFalloffImage = evaluateTexture( m_template.m_lightFalloffImage, m_template.m_params, m_args );

			for ( ShaderTemplate::MapLayers::const_iterator i = m_template.m_layers.begin(); i != m_template.m_layers.end(); ++i )
			{
				m_layers.push_back( evaluateLayer( *i, m_template.m_params, m_args ) );
			}

			if ( m_layers.size() == 1 ) {
				const BlendFuncExpression& blendFunc = m_template.m_layers.front().blendFunc();
				if ( !string_empty( blendFunc.second.c_str() ) ) {
					m_blendFunc = BlendFunc(
					                  evaluateBlendFactor( blendFunc.first.c_str(), m_template.m_params, m_args ),
					                  evaluateBlendFactor( blendFunc.second.c_str(), m_template.m_params, m_args )
					              );
				}
				else
				{
					const char* blend = evaluateShaderValue( blendFunc.first.c_str(), m_template.m_params, m_args );

					if ( string_equal_nocase( blend, "add" ) ) {
						m_blendFunc = BlendFunc( BLEND_ONE, BLEND_ONE );
					}
					else if ( string_equal_nocase( blend, "filter" ) ) {
						m_blendFunc = BlendFunc( BLEND_DST_COLOUR, BLEND_ZERO );
					}
					else if ( string_equal_nocase( blend, "blend" ) ) {
						m_blendFunc = BlendFunc( BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA );
					}
					else
					{
						globalErrorStream() << "parsing blend value failed: " << makeQuoted( blend ) << '\n';
					}
				}
			}
		}
	}

	void unrealiseLighting(){
		if ( m_lightingEnabled ) {
			GlobalTexturesCache().release( m_pDiffuse );
			GlobalTexturesCache().release( m_pBump );
			GlobalTexturesCache().release( m_pSpecular );

			GlobalTexturesCache().release( m_pLightFalloffImage );

			for ( MapLayers::iterator i = m_layers.begin(); i != m_layers.end(); ++i )
			{
				GlobalTexturesCache().release( ( *i ).texture() );
			}
			m_layers.clear();

			m_blendFunc = BlendFunc( BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA );
		}
	}

// set shader name
	void setName( const char* name ){
		m_Name = name;
	}

	class MapLayer final : public ShaderLayer
	{
		qtexture_t* m_texture;
		BlendFunc m_blendFunc;
		bool m_clampToBorder;
		float m_alphaTest;
	public:
		MapLayer( qtexture_t* texture, BlendFunc blendFunc, bool clampToBorder, float alphaTest ) :
			m_texture( texture ),
			m_blendFunc( blendFunc ),
			m_clampToBorder( false ),
			m_alphaTest( alphaTest ){
		}
		qtexture_t* texture() const override {
			return m_texture;
		}
		BlendFunc blendFunc() const override {
			return m_blendFunc;
		}
		bool clampToBorder() const override {
			return m_clampToBorder;
		}
		float alphaTest() const override {
			return m_alphaTest;
		}
	};

	static MapLayer evaluateLayer( const ShaderTemplate::MapLayerTemplate& layerTemplate, const ShaderParameters& params, const ShaderArguments& args ){
		return MapLayer(
		           evaluateTexture( layerTemplate.texture(), params, args ),
		           evaluateBlendFunc( layerTemplate.blendFunc(), params, args ),
		           layerTemplate.clampToBorder(),
		           evaluateFloat( layerTemplate.alphaTest(), params, args )
		       );
	}

	typedef std::vector<MapLayer> MapLayers;
	MapLayers m_layers;

	const ShaderLayer* firstLayer() const override {
		if ( m_layers.empty() ) {
			return 0;
		}
		return &m_layers.front();
	}
	void forEachLayer( const ShaderLayerCallback& callback ) const override {
		for ( MapLayers::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i )
		{
			callback( *i );
		}
	}

	qtexture_t* lightFalloffImage() const override {
		if ( !m_template.m_lightFalloffImage.empty() ) {
			return m_pLightFalloffImage;
		}
		return 0;
	}
};

bool CShader::m_lightingEnabled = false;

typedef SmartPointer<CShader> ShaderPointer;
typedef std::map<CopiedString, ShaderPointer, shader_less_t> shaders_t;

shaders_t g_ActiveShaders;

static shaders_t::iterator g_ActiveShadersIterator;

void ActiveShaders_IteratorBegin(){
	g_ActiveShadersIterator = g_ActiveShaders.begin();
}

bool ActiveShaders_IteratorAtEnd(){
	return g_ActiveShadersIterator == g_ActiveShaders.end();
}

IShader *ActiveShaders_IteratorCurrent(){
	return static_cast<CShader*>( g_ActiveShadersIterator->second );
}

void ActiveShaders_IteratorIncrement(){
	++g_ActiveShadersIterator;
}

void debug_check_shaders( shaders_t& shaders ){
	for ( shaders_t::iterator i = shaders.begin(); i != shaders.end(); ++i )
	{
		ASSERT_MESSAGE( i->second->refcount() == 1, "orphan shader still referenced" );
	}
}

// will free all GL binded qtextures and shaders
// NOTE: doesn't make much sense out of Radiant exit or called during a reload
void FreeShaders(){
	// reload shaders
	// empty the actives shaders list
	debug_check_shaders( g_ActiveShaders );
	g_ActiveShaders.clear();
	g_shaders.clear();
	g_shaderTemplates.clear();
	g_shaderDefinitions.clear();
	g_ActiveShadersChangedNotify();
}

bool ShaderTemplate::parseQuake3( Tokeniser& tokeniser ){
	// name of the qtexture_t we'll use to represent this shader (this one has the "textures\" before)
	m_textureName = m_Name;

	tokeniser.nextLine();

	// we need to read until we hit a balanced }
	int depth = 0;
	for (;; )
	{
		tokeniser.nextLine();
		const char* token = tokeniser.getToken();

		if ( token == 0 ) {
			return false;
		}

		if ( string_equal( token, "{" ) ) {
			++depth;
			continue;
		}
		else if ( string_equal( token, "}" ) ) {
			--depth;
			if ( depth < 0 ) { // underflow
				return false;
			}
			if ( depth == 0 ) { // end of shader
				break;
			}

			continue;
		}

		if ( depth == 1 ) {
			if ( string_equal_nocase( token, "qer_nocarve" ) ) {
				m_nFlags |= QER_NOCARVE;
			}
			else if ( string_equal_nocase( token, "qer_trans" ) ) {
				RETURN_FALSE_IF_FAIL( Tokeniser_getFloat( tokeniser, m_fTrans ) );
				m_nFlags |= QER_TRANS;
			}
			else if ( string_equal_nocase( token, "qer_editorimage" ) ) {
				RETURN_FALSE_IF_FAIL( Tokeniser_parseTextureName( tokeniser, m_textureName ) );
			}
			else if ( string_equal_nocase( token, "qer_alphafunc" ) ) {
				const char* alphafunc = tokeniser.getToken();

				if ( alphafunc == 0 ) {
					Tokeniser_unexpectedError( tokeniser, alphafunc, "#alphafunc" );
					return false;
				}

				if ( string_equal_nocase( alphafunc, "equal" ) ) {
					m_AlphaFunc = IShader::eEqual;
				}
				else if ( string_equal_nocase( alphafunc, "greater" ) ) {
					m_AlphaFunc = IShader::eGreater;
				}
				else if ( string_equal_nocase( alphafunc, "less" ) ) {
					m_AlphaFunc = IShader::eLess;
				}
				else if ( string_equal_nocase( alphafunc, "gequal" ) ) {
					m_AlphaFunc = IShader::eGEqual;
				}
				else if ( string_equal_nocase( alphafunc, "lequal" ) ) {
					m_AlphaFunc = IShader::eLEqual;
				}
				else
				{
					m_AlphaFunc = IShader::eAlways;
				}

				m_nFlags |= QER_ALPHATEST;

				RETURN_FALSE_IF_FAIL( Tokeniser_getFloat( tokeniser, m_AlphaRef ) );
			}
			else if ( string_equal_nocase( token, "skyparms" ) ) {
				const char* sky = tokeniser.getToken();

				if ( sky == 0 ) {
					Tokeniser_unexpectedError( tokeniser, sky, "#skyparms" );
					return false;
				}

				if( !string_equal( sky, "-" ) ){
					m_skyBox = sky;
				}

				m_nFlags |= QER_SKY;
			}
			else if ( string_equal_nocase( token, "cull" ) ) {
				const char* cull = tokeniser.getToken();

				if ( cull == 0 ) {
					Tokeniser_unexpectedError( tokeniser, cull, "#cull" );
					return false;
				}

				if ( string_equal_nocase( cull, "none" )
				  || string_equal_nocase( cull, "twosided" )
				  || string_equal_nocase( cull, "disable" ) ) {
					m_Cull = IShader::eCullNone;
				}
				else if ( string_equal_nocase( cull, "back" )
				       || string_equal_nocase( cull, "backside" )
				       || string_equal_nocase( cull, "backsided" ) ) {
					m_Cull = IShader::eCullBack;
				}
				else
				{
					m_Cull = IShader::eCullBack;
				}

				m_nFlags |= QER_CULL;
			}
			else if ( string_equal_nocase( token, "surfaceparm" ) ) {
				const char* surfaceparm = tokeniser.getToken();

				if ( surfaceparm == 0 ) {
					Tokeniser_unexpectedError( tokeniser, surfaceparm, "#surfaceparm" );
					return false;
				}

				if ( string_equal_nocase( surfaceparm, "fog" ) ) {
					m_nFlags |= QER_FOG;
					m_nFlags |= QER_TRANS;
					if ( m_fTrans == 1.0f ) { // has not been explicitly set by qer_trans
						m_fTrans = 0.35f;
					}
				}
				else if ( string_equal_nocase( surfaceparm, "nodraw" ) ) {
					m_nFlags |= QER_NODRAW;
				}
				else if ( string_equal_nocase( surfaceparm, "nonsolid" ) ) {
					m_nFlags |= QER_NONSOLID;
				}
				else if ( string_equal_nocase( surfaceparm, "water" ) ||
				          string_equal_nocase( surfaceparm, "lava" ) ||
				          string_equal_nocase( surfaceparm, "slime") ){
					m_nFlags |= QER_LIQUID;
				}
				else if ( string_equal_nocase( surfaceparm, "areaportal" ) ) {
					m_nFlags |= QER_AREAPORTAL;
				}
				else if ( string_equal_nocase( surfaceparm, "playerclip" ) ) {
					m_nFlags |= QER_CLIP;
				}
				else if ( string_equal_nocase( surfaceparm, "botclip" ) ) {
					m_nFlags |= QER_BOTCLIP;
				}
			}
		}
	}

	return true;
}

class Layer
{
public:
	LayerTypeId m_type;
	TextureExpression m_texture;
	BlendFunc m_blendFunc;
	bool m_clampToBorder;
	float m_alphaTest;
	float m_heightmapScale;

	Layer() : m_type( LAYER_NONE ), m_blendFunc( BLEND_ONE, BLEND_ZERO ), m_clampToBorder( false ), m_alphaTest( -1 ), m_heightmapScale( 0 ){
	}
};

std::list<CopiedString> g_shaderFilenames;

void ParseShaderFile( Tokeniser& tokeniser, const char* filename ){
	g_shaderFilenames.push_back( filename );
	filename = g_shaderFilenames.back().c_str();
	tokeniser.nextLine();
	for (;; )
	{
		const char* token = tokeniser.getToken();

		if ( token == 0 ) {
			break;
		}

		if ( string_equal( token, "table" ) ) {
			if ( tokeniser.getToken() == 0 ) {
				Tokeniser_unexpectedError( tokeniser, 0, "#table-name" );
				return;
			}
			if ( !Tokeniser_parseToken( tokeniser, "{" ) ) {
				return;
			}
			for (;; )
			{
				const char* option = tokeniser.getToken();
				if ( string_equal( option, "{" ) ) {
					for (;; )
					{
						const char* value = tokeniser.getToken();
						if ( string_equal( value, "}" ) ) {
							break;
						}
					}

					if ( !Tokeniser_parseToken( tokeniser, "}" ) ) {
						return;
					}
					break;
				}
			}
		}
		else
		{
			if ( string_equal( token, "guide" ) ) {
				parseTemplateInstance( tokeniser, filename );
			}
			else
			{
				if ( !string_equal( token, "material" )
				  && !string_equal( token, "particle" )
				  && !string_equal( token, "skin" ) ) {
					tokeniser.ungetToken();
				}
				// first token should be the path + name.. (from base)
				CopiedString name;
				if ( !Tokeniser_parseShaderName( tokeniser, name ) ) {
				}
				ShaderTemplatePointer shaderTemplate( new ShaderTemplate() );
				shaderTemplate->setName( name.c_str() );

				g_shaders.insert( ShaderTemplateMap::value_type( shaderTemplate->getName(), shaderTemplate ) );

				bool result = ( g_shaderLanguage == SHADERLANGUAGE_QUAKE3 )
				              ? shaderTemplate->parseQuake3( tokeniser )
				              : shaderTemplate->parseDoom3( tokeniser );
				if ( result ) {
					// do we already have this shader?
					if ( !g_shaderDefinitions.insert( ShaderDefinitionMap::value_type( shaderTemplate->getName(), ShaderDefinition( shaderTemplate.get(), ShaderArguments(), filename ) ) ).second ) {
#ifdef _DEBUG
						globalWarningStream() << "WARNING: shader " << shaderTemplate->getName() << " is already in memory, definition in " << filename << " ignored.\n";
#endif
					}
				}
				else
				{
					globalErrorStream() << "Error parsing shader " << shaderTemplate->getName() << '\n';
					return;
				}
			}
		}
	}
}

void parseGuideFile( Tokeniser& tokeniser, const char* filename ){
	tokeniser.nextLine();
	for (;; )
	{
		const char* token = tokeniser.getToken();

		if ( token == 0 ) {
			break;
		}

		if ( string_equal( token, "guide" ) ) {
			// first token should be the path + name.. (from base)
			ShaderTemplatePointer shaderTemplate( new ShaderTemplate );
			shaderTemplate->parseTemplate( tokeniser );
			if ( !g_shaderTemplates.insert( ShaderTemplateMap::value_type( shaderTemplate->getName(), shaderTemplate ) ).second ) {
				globalErrorStream() << "guide " << makeQuoted( shaderTemplate->getName() ) << ": already defined, second definition ignored\n";
			}
		}
		else if ( string_equal( token, "inlineGuide" ) ) {
			// skip entire inlineGuide definition
			std::size_t depth = 0;
			for (;; )
			{
				tokeniser.nextLine();
				token = tokeniser.getToken();
				if ( string_equal( token, "{" ) ) {
					++depth;
				}
				else if ( string_equal( token, "}" ) ) {
					if ( --depth == 0 ) {
						break;
					}
				}
			}
		}
	}
}

void LoadShaderFile( const char* filename ){
	ArchiveTextFile* file = GlobalFileSystem().openTextFile( filename );

	if ( file != 0 ) {
		globalOutputStream() << "Parsing shaderfile " << filename << '\n';

		Tokeniser& tokeniser = GlobalScriptLibrary().m_pfnNewScriptTokeniser( file->getInputStream() );

		ParseShaderFile( tokeniser, filename );

		tokeniser.release();
		file->release();
	}
	else
	{
		globalWarningStream() << "Unable to read shaderfile " << filename << '\n';
	}
}

void loadGuideFile( const char* filename ){
	const auto fullname = StringStream( "guides/", filename );
	ArchiveTextFile* file = GlobalFileSystem().openTextFile( fullname );

	if ( file != 0 ) {
		globalOutputStream() << "Parsing guide file " << fullname << '\n';

		Tokeniser& tokeniser = GlobalScriptLibrary().m_pfnNewScriptTokeniser( file->getInputStream() );

		parseGuideFile( tokeniser, fullname );

		tokeniser.release();
		file->release();
	}
	else
	{
		globalWarningStream() << "Unable to read guide file " << fullname << '\n';
	}
}

CShader* Try_Shader_ForName( const char* name ){
	{
		shaders_t::iterator i = g_ActiveShaders.find( name );
		if ( i != g_ActiveShaders.end() ) {
			return ( *i ).second;
		}
	}
	// active shader was not found

	// find matching shader definition
	ShaderDefinitionMap::iterator i = g_shaderDefinitions.find( name );
	if ( i == g_shaderDefinitions.end() ) {
		// shader definition was not found

		// create new shader definition from default shader template
		ShaderTemplatePointer shaderTemplate( new ShaderTemplate() );
		shaderTemplate->CreateDefault( name );
		g_shaderTemplates.insert( ShaderTemplateMap::value_type( shaderTemplate->getName(), shaderTemplate ) );

		i = g_shaderDefinitions.insert( ShaderDefinitionMap::value_type( name, ShaderDefinition( shaderTemplate.get(), ShaderArguments(), "" ) ) ).first;
	}

	// create shader from existing definition
	ShaderPointer pShader( new CShader( ( *i ).second ) );
	pShader->setName( name );
	g_ActiveShaders.insert( shaders_t::value_type( name, pShader ) );
	g_ActiveShadersChangedNotify();
	return pShader;
}

IShader *Shader_ForName( const char *name ){
	ASSERT_NOTNULL( name );

	IShader *pShader = Try_Shader_ForName( name );
	pShader->IncRef();
	return pShader;
}




// the list of scripts/*.shader files we need to work with
// those are listed in shaderlist file
std::vector<CopiedString> l_shaderfiles;

/*
   ==================
   DumpUnreferencedShaders
   useful function: dumps the list of .shader files that are not referenced to the console
   ==================
 */
void IfFound_dumpUnreferencedShader( bool& bFound, const char* filename ){
	bool listed = false;

	for ( const CopiedString& sh : l_shaderfiles )
	{
		if ( !strcmp( sh.c_str(), filename ) ) {
			listed = true;
			break;
		}
	}

	if ( !listed ) {
		if ( !bFound ) {
			bFound = true;
			globalOutputStream() << "Following shader files are not referenced in any shaderlist.txt:\n";
		}
		globalOutputStream() << '\t' << filename << '\n';
	}
}
typedef ReferenceCaller<bool, void(const char*), IfFound_dumpUnreferencedShader> IfFoundDumpUnreferencedShaderCaller;

void DumpUnreferencedShaders(){
	bool bFound = false;
	GlobalFileSystem().forEachFile( g_shadersDirectory, g_shadersExtension, IfFoundDumpUnreferencedShaderCaller( bFound ) );
}

void ShaderList_addShaderFile( const char* dirstring ){
	bool found = false;

	for ( const CopiedString& sh : l_shaderfiles )
	{
		if ( string_equal_nocase( dirstring, sh.c_str() ) ) {
			found = true;
			globalOutputStream() << "duplicate entry \"" << sh << "\" in shaderlist.txt\n";
			break;
		}
	}

	if ( !found ) {
		l_shaderfiles.emplace_back( dirstring );
	}
}

/*
   ==================
   BuildShaderList
   build a CStringList of shader names
   ==================
 */
void BuildShaderList( TextInputStream& shaderlist ){
	Tokeniser& tokeniser = GlobalScriptLibrary().m_pfnNewSimpleTokeniser( shaderlist );
	StringOutputStream shaderFile( 64 );
	for( const char* token; tokeniser.nextLine(), token = tokeniser.getToken(); )
	{
		// each token should be a shader filename
		shaderFile( token );
		if( !path_extension_is( token, g_shadersExtension ) )
			shaderFile << '.' << g_shadersExtension;

		ShaderList_addShaderFile( shaderFile );
	}
	tokeniser.release();
}

void ShaderList_addFromArchive( const char *archivename ){
	const char *shaderpath = GlobalRadiant().getGameDescriptionKeyValue( "shaderpath" );
	if ( string_empty( shaderpath ) ) {
		return;
	}

	Archive *archive = GlobalFileSystem().getArchive( archivename, false );
	if ( archive ) {
		ArchiveTextFile *file = archive->openTextFile( StringStream<64>( DirectoryCleaned( shaderpath ), "shaderlist.txt" ) );
		if ( file ) {
			globalOutputStream() << "Found shaderlist.txt in " << archivename << '\n';
			BuildShaderList( file->getInputStream() );
			file->release();
		}
	}
}

#include "stream/filestream.h"

bool shaderlist_findOrInstall( const char* enginePath, const char* toolsPath, const char* shaderPath, const char* gamename ){
	const auto absShaderList = StringStream( enginePath, gamename, '/', shaderPath, "shaderlist.txt" );
	if ( file_exists( absShaderList ) ) {
		return true;
	}
	{
		const auto directory = StringStream( enginePath, gamename, '/', shaderPath );
		if ( !file_exists( directory ) && !Q_mkdir( directory ) ) {
			return false;
		}
	}
	{
		const auto defaultShaderList = StringStream( toolsPath, gamename, '/', "default_shaderlist.txt" );
		if ( file_exists( defaultShaderList ) ) {
			return file_copy( defaultShaderList, absShaderList );
		}
	}
	return false;
}

void Shaders_Load(){
	if ( g_shaderLanguage == SHADERLANGUAGE_QUAKE4 ) {
		GlobalFileSystem().forEachFile( "guides/", "guide", makeCallbackF( loadGuideFile ), 0 );
	}

	const char* shaderPath = GlobalRadiant().getGameDescriptionKeyValue( "shaderpath" );
	if ( !string_empty( shaderPath ) ) {
		const auto path = StringStream<64>( DirectoryCleaned( shaderPath ) );

		if ( g_useShaderList ) {
			// preload shader files that have been listed in shaderlist.txt
			const char* basegame = GlobalRadiant().getRequiredGameDescriptionKeyValue( "basegame" );
			const char* gamename = GlobalRadiant().getGameName();
			const char* enginePath = GlobalRadiant().getEnginePath();
			const char* toolsPath = GlobalRadiant().getGameToolsPath();

			bool isMod = !string_equal( basegame, gamename );

			if ( !isMod || !shaderlist_findOrInstall( enginePath, toolsPath, path, gamename ) ) {
				gamename = basegame;
				shaderlist_findOrInstall( enginePath, toolsPath, path, gamename );
			}

			GlobalFileSystem().forEachArchive( makeCallbackF( ShaderList_addFromArchive ), false, true );
			if( !l_shaderfiles.empty() ){
				DumpUnreferencedShaders();
			}
			else{
				globalOutputStream() << "No shaderlist.txt found: loading all shaders\n";
				GlobalFileSystem().forEachFile( path, g_shadersExtension, makeCallbackF( ShaderList_addShaderFile ), 1 );
			}
		}
		else
		{
			GlobalFileSystem().forEachFile( path, g_shadersExtension, makeCallbackF( ShaderList_addShaderFile ), 0 );
		}

		StringOutputStream shadername( 256 );
		for( const CopiedString& sh : l_shaderfiles )
		{
			LoadShaderFile( shadername( path, sh ) );
		}
	}

	//StringPool_analyse( ShaderPool::instance() );
}

void Shaders_Free(){
	FreeShaders();
	l_shaderfiles.clear();
	g_shaderFilenames.clear();
}

ModuleObservers g_observers;

std::size_t g_shaders_unrealised = 1; // wait until filesystem and is realised before loading anything
bool Shaders_realised(){
	return g_shaders_unrealised == 0;
}
void Shaders_Realise(){
	if ( --g_shaders_unrealised == 0 ) {
		Shaders_Load();
		g_observers.realise();
	}
}
void Shaders_Unrealise(){
	if ( ++g_shaders_unrealised == 1 ) {
		g_observers.unrealise();
		Shaders_Free();
	}
}

void Shaders_Refresh(){
	Shaders_Unrealise();
	Shaders_Realise();
}

class Quake3ShaderSystem : public ShaderSystem, public ModuleObserver
{
public:
	void realise(){
		Shaders_Realise();
	}
	void unrealise(){
		Shaders_Unrealise();
	}
	void refresh(){
		Shaders_Refresh();
	}

	IShader* getShaderForName( const char* name ){
		return Shader_ForName( name );
	}

	void foreachShaderName( const ShaderNameCallback& callback ){
		for ( ShaderDefinitionMap::const_iterator i = g_shaderDefinitions.begin(); i != g_shaderDefinitions.end(); ++i )
		{
			callback( ( *i ).first.c_str() );
		}
	}

	void beginActiveShadersIterator(){
		ActiveShaders_IteratorBegin();
	}
	bool endActiveShadersIterator(){
		return ActiveShaders_IteratorAtEnd();
	}
	IShader* dereferenceActiveShadersIterator(){
		return ActiveShaders_IteratorCurrent();
	}
	void incrementActiveShadersIterator(){
		ActiveShaders_IteratorIncrement();
	}
	void setActiveShadersChangedNotify( const Callback<void()>& notify ){
		g_ActiveShadersChangedNotify = notify;
	}

	void attach( ModuleObserver& observer ){
		g_observers.attach( observer );
	}
	void detach( ModuleObserver& observer ){
		g_observers.detach( observer );
	}

	void setLightingEnabled( bool enabled ){
		if ( CShader::m_lightingEnabled != enabled ) {
			for ( shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i )
			{
				( *i ).second->unrealiseLighting();
			}
			CShader::m_lightingEnabled = enabled;
			for ( shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i )
			{
				( *i ).second->realiseLighting();
			}
		}
	}

	const char* getTexturePrefix() const {
		return g_texturePrefix;
	}
};

Quake3ShaderSystem g_Quake3ShaderSystem;

ShaderSystem& GetShaderSystem(){
	return g_Quake3ShaderSystem;
}

void Shaders_Construct(){
	GlobalFileSystem().attach( g_Quake3ShaderSystem );
}
void Shaders_Destroy(){
	GlobalFileSystem().detach( g_Quake3ShaderSystem );

	if ( Shaders_realised() ) {
		Shaders_Free();
	}
}

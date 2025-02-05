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

#include "textures.h"

#include "debugging/debugging.h"

#include "itextures.h"
#include "igl.h"
#include "preferencesystem.h"
#include "qgl.h"

#include "texturelib.h"
#include "container/hashfunc.h"
#include "container/cache.h"
#include "generic/callback.h"
#include "stream/stringstream.h"
#include "stringio.h"

#include "image.h"
#include "texmanip.h"
#include "preferences.h"



enum ETexturesMode
{
	eTextures_NEAREST = 0,
	eTextures_NEAREST_MIPMAP_NEAREST = 1,
	eTextures_NEAREST_MIPMAP_LINEAR = 2,
	eTextures_LINEAR = 3,
	eTextures_LINEAR_MIPMAP_NEAREST = 4,
	eTextures_LINEAR_MIPMAP_LINEAR = 5,
};

enum TextureCompressionFormat
{
	TEXTURECOMPRESSION_NONE = 0,
	TEXTURECOMPRESSION_RGBA = 1,
	TEXTURECOMPRESSION_RGBA_S3TC_DXT1 = 2,
	TEXTURECOMPRESSION_RGBA_S3TC_DXT3 = 3,
	TEXTURECOMPRESSION_RGBA_S3TC_DXT5 = 4,
};

struct texture_globals_t
{
	// RIANT
	// texture compression format
	TextureCompressionFormat m_nTextureCompressionFormat;

	float fGamma;

	bool bTextureCompressionSupported; // is texture compression supported by hardware?
	GLint texture_components;

	// temporary values that should be initialised only once at run-time
	bool m_bOpenGLCompressionSupported;
	bool m_bS3CompressionSupported;

	texture_globals_t( GLint components ) :
		m_nTextureCompressionFormat( TEXTURECOMPRESSION_NONE ),
		fGamma( 1.0f ),
		bTextureCompressionSupported( false ),
		texture_components( components ),
		m_bOpenGLCompressionSupported( false ),
		m_bS3CompressionSupported( false ){
	}
};

texture_globals_t g_texture_globals( GL_RGBA );

void SetTexParameters( ETexturesMode mode ){
	switch ( mode )
	{
	case eTextures_NEAREST:
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		break;
	case eTextures_NEAREST_MIPMAP_NEAREST:
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		break;
	case eTextures_NEAREST_MIPMAP_LINEAR:
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		break;
	case eTextures_LINEAR:
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		break;
	case eTextures_LINEAR_MIPMAP_NEAREST:
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		break;
	case eTextures_LINEAR_MIPMAP_LINEAR:
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		break;
	default:
		globalErrorStream() << "invalid texture mode\n";
	}
}

void SetTexAnisotropy( bool anisotropy ){
	float maxAniso = QGL_maxTextureAnisotropy();
	if ( maxAniso > 1 ) {
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy ? maxAniso : 1.f );
	}
}

ETexturesMode g_texture_mode = eTextures_LINEAR_MIPMAP_LINEAR;
bool g_TextureAnisotropy = true;




byte g_gammatable[256];
void ResampleGamma( float fGamma ){
	int i, inf;
	if ( fGamma == 1.0 ) {
		for ( i = 0; i < 256; i++ )
			g_gammatable[i] = i;
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			inf = (int)( 255 * pow( static_cast<double>( ( i + 0.5 ) / 255.5 ), static_cast<double>( fGamma ) ) + 0.5 );
			if ( inf < 0 ) {
				inf = 0;
			}
			if ( inf > 255 ) {
				inf = 255;
			}
			g_gammatable[i] = inf;
		}
	}
}

int max_tex_size = 0;
int g_Textures_mipLevel = 0;

/// \brief This function does the actual processing of raw RGBA data into a GL texture.
/// It will also resample to power-of-two dimensions, generate the mipmaps and adjust gamma.
void LoadTextureRGBA( qtexture_t* q, unsigned char* pPixels, int nWidth, int nHeight ){
	static float fGamma = -1;
	float total[3];
	int nCount = nWidth * nHeight;

	if ( fGamma != g_texture_globals.fGamma ) {
		fGamma = g_texture_globals.fGamma;
		ResampleGamma( fGamma );
	}

	q->width = nWidth;
	q->height = nHeight;

	total[0] = total[1] = total[2] = 0.0f;

	// resample texture gamma according to user settings
	for ( int i = 0; i < ( nCount * 4 ); i += 4 )
	{
		for ( int j = 0; j < 3; j++ )
		{
			total[j] += ( pPixels + i )[j];
			byte b = ( pPixels + i )[j];
			( pPixels + i )[j] = g_gammatable[b];
		}
	}

	q->color[0] = total[0] / ( nCount * 255 );
	q->color[1] = total[1] / ( nCount * 255 );
	q->color[2] = total[2] / ( nCount * 255 );

	gl().glGenTextures( 1, &q->texture_number );

	gl().glBindTexture( GL_TEXTURE_2D, q->texture_number );

	SetTexParameters( g_texture_mode );
	SetTexAnisotropy( g_TextureAnisotropy );
#if 1
	gl().glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
	gl().glTexImage2D( GL_TEXTURE_2D, 0, g_texture_globals.texture_components, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pPixels );

	gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, std::min( g_Textures_mipLevel, static_cast<int>( log2( static_cast<float>( std::max( nWidth, nHeight ) ) ) ) ) );

	gl().glBindTexture( GL_TEXTURE_2D, 0 );
#else
	int gl_width = 1;
	while ( gl_width < nWidth )
		gl_width <<= 1;

	int gl_height = 1;
	while ( gl_height < nHeight )
		gl_height <<= 1;

	byte  *outpixels = 0;
	bool resampled = false;
	if ( !( gl_width == nWidth && gl_height == nHeight ) ) {
		resampled = true;
		outpixels = (byte *)malloc( gl_width * gl_height * 4 );
		R_ResampleTexture( pPixels, nWidth, nHeight, outpixels, gl_width, gl_height, 4 );
	}
	else
	{
		outpixels = pPixels;
	}

	const int target_width = std::max( std::min( gl_width >> g_Textures_mipLevel, max_tex_size ), 1 );
	const int target_height = std::max( std::min( gl_height >> g_Textures_mipLevel, max_tex_size ), 1 );

	while ( gl_width > target_width || gl_height > target_height )
	{
		GL_MipReduce( outpixels, outpixels, gl_width, gl_height, target_width, target_height );

		if ( gl_width > target_width ) {
			gl_width >>= 1;
		}
		if ( gl_height > target_height ) {
			gl_height >>= 1;
		}
	}

	int mip = 0;
	gl().glTexImage2D( GL_TEXTURE_2D, mip++, g_texture_globals.texture_components, gl_width, gl_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, outpixels );
	while ( gl_width > 1 || gl_height > 1 )
	{
		GL_MipReduce( outpixels, outpixels, gl_width, gl_height, 1, 1 );

		if ( gl_width > 1 ) {
			gl_width >>= 1;
		}
		if ( gl_height > 1 ) {
			gl_height >>= 1;
		}

		gl().glTexImage2D( GL_TEXTURE_2D, mip++, g_texture_globals.texture_components, gl_width, gl_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, outpixels );
	}

	gl().glBindTexture( GL_TEXTURE_2D, 0 );
	if ( resampled ) {
		free( outpixels );
	}
#endif
}

#if 0
/*
   ==============
   Texture_InitPalette
   ==============
 */
void Texture_InitPalette( byte *pal ){
	int r, g, b;
	int i;
	int inf;
	byte gammatable[256];
	float gamma;

	gamma = g_texture_globals.fGamma;

	if ( gamma == 1.0 ) {
		for ( i = 0; i < 256; i++ )
			gammatable[i] = i;
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			inf = (int)( 255 * pow( ( i + 0.5 ) / 255.5, gamma ) + 0.5 );
			if ( inf < 0 ) {
				inf = 0;
			}
			if ( inf > 255 ) {
				inf = 255;
			}
			gammatable[i] = inf;
		}
	}

	for ( i = 0; i < 256; i++ )
	{
		r = gammatable[pal[0]];
		g = gammatable[pal[1]];
		b = gammatable[pal[2]];
		pal += 3;

		//v = ( r << 24 ) + ( g << 16 ) + ( b << 8 ) + 255;
		//v = BigLong( v );

		//tex_palette[i] = v;
		tex_palette[i * 3 + 0] = r;
		tex_palette[i * 3 + 1] = g;
		tex_palette[i * 3 + 2] = b;
	}
}
#endif

#if 0
class TestHashtable
{
public:
	TestHashtable(){
		HashTable<CopiedString, CopiedString, HashStringNoCase, StringEqualNoCase> strings;
		strings["Monkey"] = "bleh";
		strings["MonkeY"] = "blah";
	}
};

const TestHashtable g_testhashtable;

#endif

typedef std::pair<LoadImageCallback, CopiedString> TextureKey;

void qtexture_realise( qtexture_t& texture, const TextureKey& key ){
	texture.texture_number = 0;
	if ( !key.second.empty() ) {
		if( !key.first.m_skybox ){
			Image* image = key.first.loadImage( key.second.c_str() );
			if ( image != 0 ) {
				LoadTextureRGBA( &texture, image->getRGBAPixels(), image->getWidth(), image->getHeight() );
				texture.surfaceFlags = image->getSurfaceFlags();
				texture.contentFlags = image->getContentFlags();
				texture.value = image->getValue();
				image->release();
				globalOutputStream() << "Loaded Texture: \"" << key.second << "\"\n";
				GlobalOpenGL_debugAssertNoErrors();
			}
			else
			{
				globalErrorStream() << "Texture load failed: \"" << key.second << "\"\n";
			}
		}
		else {
			Image *images[6]{};
			/* load in order, so that Q3 cubemap is seamless in openGL, but rotated & flipped; fix misorientation in shader later */
			const char *suffixes[] = { "_ft", "_bk", "_up", "_dn", "_rt", "_lf" };
			for( int i = 0; i < 6; ++i ){
				images[i] = key.first.loadImage( StringStream<64>( key.second, suffixes[i] ) );
			}
			if( std::all_of( images, images + std::size( images ), []( const Image *img ){ return img != nullptr; } ) ){
				gl().glGenTextures( 1, &texture.texture_number );
				gl().glBindTexture( GL_TEXTURE_CUBE_MAP, texture.texture_number );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_GENERATE_MIPMAP, GL_FALSE );

				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0 );
				gl().glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0); //this or mipmaps are required for samplerCube to work
				// fix non quadratic, varying sizes; GL_TEXTURE_CUBE_MAP requires this
				unsigned int size = 0;
				for( const auto img : images )
					size = std::max( { size, img->getWidth(), img->getHeight() } );
				for( int i = 0; i < 6; ++i ){
					const Image& img = *images[i];
					byte *pix = img.getRGBAPixels();
					if( img.getWidth() != size || img.getHeight() != size ){
						pix = static_cast<byte*>( malloc( size * size * 4 ) );
						R_ResampleTexture( img.getRGBAPixels(), img.getWidth(), img.getHeight(), pix, size, size, 4 );
					}
					gl().glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, g_texture_globals.texture_components, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix );
					if( pix != img.getRGBAPixels() )
						free( pix );
				}

				gl().glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
				globalOutputStream() << "Loaded Skybox: \"" << key.second << "\"\n";
				GlobalOpenGL_debugAssertNoErrors();
			}
			else
			{
				globalErrorStream() << "Skybox load failed: \"" << key.second << "\"\n";
			}

			std::for_each_n( images, std::size( images ), []( Image *img ){ if( img != nullptr ) img->release(); } );
		}
	}
}

void qtexture_unrealise( qtexture_t& texture ){
	if ( GlobalOpenGL().contextValid && texture.texture_number != 0 ) {
		gl().glDeleteTextures( 1, &texture.texture_number );
		GlobalOpenGL_debugAssertNoErrors();
	}
}

class TextureKeyEqualNoCase
{
public:
	bool operator()( const TextureKey& key, const TextureKey& other ) const {
		return key.first == other.first && string_equal_nocase( key.second.c_str(), other.second.c_str() );
	}
};

class TextureKeyHashNoCase
{
public:
	typedef hash_t hash_type;
	hash_t operator()( const TextureKey& key ) const {
		return hash_combine( string_hash_nocase( key.second.c_str() ), pod_hash( key.first ) );
	}
};

#define DEBUG_TEXTURES 0

class TexturesMap final : public TexturesCache
{
	class TextureConstructor
	{
		TexturesMap* m_cache;
	public:
		explicit TextureConstructor( TexturesMap* cache )
			: m_cache( cache ){
		}
		qtexture_t* construct( const TextureKey& key ){
			qtexture_t* texture = new qtexture_t( key.first, key.second.c_str() );
			if ( m_cache->realised() ) {
				qtexture_realise( *texture, key );
			}
			return texture;
		}
		void destroy( qtexture_t* texture ){
			if ( m_cache->realised() ) {
				qtexture_unrealise( *texture );
			}
			delete texture;
		}
	};

	typedef HashedCache<TextureKey, qtexture_t, TextureKeyHashNoCase, TextureKeyEqualNoCase, TextureConstructor> qtextures_t;
	qtextures_t m_qtextures;
	TexturesCacheObserver* m_observer;
	std::size_t m_unrealised;

public:
	TexturesMap() : m_qtextures( TextureConstructor( this ) ), m_observer( 0 ), m_unrealised( 1 ){
	}
	typedef qtextures_t::iterator iterator;

	iterator begin(){
		return m_qtextures.begin();
	}
	iterator end(){
		return m_qtextures.end();
	}

	LoadImageCallback defaultLoader() const {
		return LoadImageCallback( 0, QERApp_LoadImage );
	}
	Image* loadImage( const char* name ){
		return defaultLoader().loadImage( name );
	}
	qtexture_t* capture( const char* name ){
		return capture( defaultLoader(), name );
	}
	qtexture_t* capture( const LoadImageCallback& loader, const char* name ){
#if DEBUG_TEXTURES
		globalOutputStream() << "textures capture: " << makeQuoted( name ) << '\n';
#endif
		return m_qtextures.capture( TextureKey( loader, name ) ).get();
	}
	void release( qtexture_t* texture ){
#if DEBUG_TEXTURES
		globalOutputStream() << "textures release: " << makeQuoted( texture->name ) << '\n';
#endif
		m_qtextures.release( TextureKey( texture->load, texture->name ) );
	}
	void attach( TexturesCacheObserver& observer ){
		ASSERT_MESSAGE( m_observer == 0, "TexturesMap::attach: cannot attach observer" );
		m_observer = &observer;
	}
	void detach( TexturesCacheObserver& observer ){
		ASSERT_MESSAGE( m_observer == &observer, "TexturesMap::detach: cannot detach observer" );
		m_observer = 0;
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			g_texture_globals.bTextureCompressionSupported = false;

			if ( GlobalOpenGL().ARB_texture_compression() ) {
				g_texture_globals.bTextureCompressionSupported = true;
				g_texture_globals.m_bOpenGLCompressionSupported = true;
			}

			if ( GlobalOpenGL().EXT_texture_compression_s3tc() ) {
				g_texture_globals.bTextureCompressionSupported = true;
				g_texture_globals.m_bS3CompressionSupported = true;
			}

			switch ( g_texture_globals.texture_components )
			{
			case GL_RGBA:
				break;
			case GL_COMPRESSED_RGBA_ARB:
				if ( !g_texture_globals.m_bOpenGLCompressionSupported ) {
					globalOutputStream() << "OpenGL extension GL_ARB_texture_compression not supported by current graphics drivers\n";
					g_texture_globals.m_nTextureCompressionFormat = TEXTURECOMPRESSION_NONE;
					g_texture_globals.texture_components = GL_RGBA;
				}
				break;
			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				if ( !g_texture_globals.m_bS3CompressionSupported ) {
					globalOutputStream() << "OpenGL extension GL_EXT_texture_compression_s3tc not supported by current graphics drivers\n";
					if ( g_texture_globals.m_bOpenGLCompressionSupported ) {
						g_texture_globals.m_nTextureCompressionFormat = TEXTURECOMPRESSION_RGBA;
						g_texture_globals.texture_components = GL_COMPRESSED_RGBA_ARB;
					}
					else
					{
						g_texture_globals.m_nTextureCompressionFormat = TEXTURECOMPRESSION_NONE;
						g_texture_globals.texture_components = GL_RGBA;
					}
				}
				break;
			default:
				globalOutputStream() << "Unknown texture compression selected, reverting\n";
				g_texture_globals.m_nTextureCompressionFormat = TEXTURECOMPRESSION_NONE;
				g_texture_globals.texture_components = GL_RGBA;
				break;
			}


			gl().glGetIntegerv( GL_MAX_TEXTURE_SIZE, &max_tex_size );
			if ( max_tex_size == 0 ) {
				max_tex_size = 1024;
			}

			for ( qtextures_t::iterator i = m_qtextures.begin(); i != m_qtextures.end(); ++i )
			{
				if ( !( *i ).value.empty() ) {
					qtexture_realise( *( *i ).value, ( *i ).key );
				}
			}
			if ( m_observer != 0 ) {
				m_observer->realise();
			}
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			if ( m_observer != 0 ) {
				m_observer->unrealise();
			}
			for ( qtextures_t::iterator i = m_qtextures.begin(); i != m_qtextures.end(); ++i )
			{
				if ( !( *i ).value.empty() ) {
					qtexture_unrealise( *( *i ).value );
				}
			}
		}
	}
	bool realised(){
		return m_unrealised == 0;
	}
};

TexturesMap* g_texturesmap;

TexturesCache& GetTexturesCache(){
	return *g_texturesmap;
}


void Textures_Realise(){
	g_texturesmap->realise();
}

void Textures_Unrealise(){
	g_texturesmap->unrealise();
}


Callback<void()> g_texturesModeChangedNotify;

void Textures_setModeChangedNotify( const Callback<void()>& notify ){
	g_texturesModeChangedNotify = notify;
}

void Textures_ModeChanged(){
	if ( g_texturesmap->realised() ) {
		SetTexParameters( g_texture_mode );
		SetTexAnisotropy( g_TextureAnisotropy );

		for ( TexturesMap::iterator i = g_texturesmap->begin(); i != g_texturesmap->end(); ++i )
		{
			gl().glBindTexture( GL_TEXTURE_2D, ( *i ).value->texture_number );
			SetTexParameters( g_texture_mode );
			SetTexAnisotropy( g_TextureAnisotropy );
		}

		gl().glBindTexture( GL_TEXTURE_2D, 0 );
	}
	g_texturesModeChangedNotify();
}

void Textures_SetMode( ETexturesMode mode ){
	if ( g_texture_mode != mode ) {
		g_texture_mode = mode;

		Textures_ModeChanged();
	}
}

void Textures_SetAnisotropy( bool anisotropy ){
	if ( g_TextureAnisotropy != anisotropy ) {
		g_TextureAnisotropy = anisotropy;

		Textures_ModeChanged();
	}
}

void Textures_setTextureComponents( GLint texture_components ){
	if ( g_texture_globals.texture_components != texture_components ) {
		Textures_Unrealise();
		g_texture_globals.texture_components = texture_components;
		Textures_Realise();
	}
}

void Textures_UpdateTextureCompressionFormat(){
	GLint texture_components = GL_RGBA;

	switch ( g_texture_globals.m_nTextureCompressionFormat )
	{
	case ( TEXTURECOMPRESSION_NONE ):
		texture_components = GL_RGBA;
		break;
	case ( TEXTURECOMPRESSION_RGBA ):
		texture_components = GL_COMPRESSED_RGBA_ARB;
		break;
	case ( TEXTURECOMPRESSION_RGBA_S3TC_DXT1 ):
		texture_components = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case ( TEXTURECOMPRESSION_RGBA_S3TC_DXT3 ):
		texture_components = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case ( TEXTURECOMPRESSION_RGBA_S3TC_DXT5 ):
		texture_components = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	}

	Textures_setTextureComponents( texture_components );
}

void TextureCompressionImport( TextureCompressionFormat& self, int value ){
	if ( !g_texture_globals.m_bOpenGLCompressionSupported
	   && g_texture_globals.m_bS3CompressionSupported
	   && value >= 1 ) {
		++value;
	}
	switch ( value )
	{
	case 0:
		self = TEXTURECOMPRESSION_NONE;
		break;
	case 1:
		self = TEXTURECOMPRESSION_RGBA;
		break;
	case 2:
		self = TEXTURECOMPRESSION_RGBA_S3TC_DXT1;
		break;
	case 3:
		self = TEXTURECOMPRESSION_RGBA_S3TC_DXT3;
		break;
	case 4:
		self = TEXTURECOMPRESSION_RGBA_S3TC_DXT5;
		break;
	}
	Textures_UpdateTextureCompressionFormat();
}
typedef ReferenceCaller<TextureCompressionFormat, void(int), TextureCompressionImport> TextureCompressionImportCaller;

void TextureMiplevelImport( int& self, int value ){
	if ( self != value ) {
		Textures_Unrealise();
		self = value;
		Textures_Realise();
	}
}
typedef ReferenceCaller<int, void(int), TextureMiplevelImport> TextureMiplevelImportCaller;

void TextureGammaImport( float& self, float value ){
	if ( self != value ) {
		Textures_Unrealise();
		self = value;
		Textures_Realise();
	}
}
typedef ReferenceCaller<float, void(float), TextureGammaImport> TextureGammaImportCaller;

void TextureModeImport( ETexturesMode& self, int value ){
	switch ( value )
	{
	case 0:
		Textures_SetMode( eTextures_NEAREST );
		break;
	case 1:
		Textures_SetMode( eTextures_NEAREST_MIPMAP_NEAREST );
		break;
	case 2:
		Textures_SetMode( eTextures_LINEAR );
		break;
	case 3:
		Textures_SetMode( eTextures_NEAREST_MIPMAP_LINEAR );
		break;
	case 4:
		Textures_SetMode( eTextures_LINEAR_MIPMAP_NEAREST );
		break;
	case 5:
		Textures_SetMode( eTextures_LINEAR_MIPMAP_LINEAR );
	}
}
typedef ReferenceCaller<ETexturesMode, void(int), TextureModeImport> TextureModeImportCaller;

void TextureModeExport( ETexturesMode& self, const IntImportCallback& importer ){
	switch ( self )
	{
	case eTextures_NEAREST:
		importer( 0 );
		break;
	case eTextures_NEAREST_MIPMAP_NEAREST:
		importer( 1 );
		break;
	case eTextures_LINEAR:
		importer( 2 );
		break;
	case eTextures_NEAREST_MIPMAP_LINEAR:
		importer( 3 );
		break;
	case eTextures_LINEAR_MIPMAP_NEAREST:
		importer( 4 );
		break;
	case eTextures_LINEAR_MIPMAP_LINEAR:
		importer( 5 );
		break;
	default:
		importer( 4 );
	}
}
typedef ReferenceCaller<ETexturesMode, void(const IntImportCallback&), TextureModeExport> TextureModeExportCaller;

#include <QComboBox>
#include <QEvent>

void Textures_constructPreferences( PreferencesPage& page ){
	{
		const char* percentages[] = { "100%", "50%", "25%", "12.5%", };
		page.appendRadio(
		    "Texture Quality",
		    StringArrayRange( percentages ),
		    TextureMiplevelImportCaller( g_Textures_mipLevel ),
		    IntExportCaller( g_Textures_mipLevel )
		);
	}
	page.appendSpinner(
	    "Texture Gamma",
	    0.0,
	    5.0,
	    FloatImportCallback( TextureGammaImportCaller( g_texture_globals.fGamma ) ),
	    FloatExportCallback( FloatExportCaller( g_texture_globals.fGamma ) )
	);
	{
		const char* texture_mode[] = { "Nearest", "Nearest Mipmap", "Linear", "Bilinear", "Bilinear Mipmap", "Trilinear" };
		page.appendCombo(
		    "Texture Render Mode",
		    StringArrayRange( texture_mode ),
		    IntImportCallback( TextureModeImportCaller( g_texture_mode ) ),
		    IntExportCallback( TextureModeExportCaller( g_texture_mode ) )
		);
	}
	{
		//. note workaround: openGL is initialised after prefs dlg is constructed
		//. solution for now is to defer dependent preference construction
		class Filter : public QObject
		{
			using QObject::QObject;
		protected:
			bool eventFilter( QObject *obj, QEvent *event ) override {
				if( event->type() == QEvent::Polish ) {
					const char* compression_none[] = { "None" };
					const char* compression_opengl[] = { "None", "OpenGL ARB" };
					const char* compression_s3tc[] = { "None", "S3TC DXT1", "S3TC DXT3", "S3TC DXT5" };
					const char* compression_opengl_s3tc[] = { "None", "OpenGL ARB", "S3TC DXT1", "S3TC DXT3", "S3TC DXT5" };
					const StringArrayRange compression(
					    ( g_texture_globals.m_bOpenGLCompressionSupported )
					    ? ( g_texture_globals.m_bS3CompressionSupported )
					      ? StringArrayRange( compression_opengl_s3tc )
					      : StringArrayRange( compression_opengl )
					    : ( g_texture_globals.m_bS3CompressionSupported )
					      ? StringArrayRange( compression_s3tc )
					      : StringArrayRange( compression_none )
					);
					QComboBox *combo = static_cast<QComboBox *>( obj );
					for( const char *c : compression )
						combo->addItem( c );
					obj->removeEventFilter( this );
				}
				return QObject::eventFilter( obj, event ); // standard event processing
			}
		};

		QComboBox *combo = page.appendCombo(
		    "Hardware Texture Compression",
		    StringArrayRange(),
		    TextureCompressionImportCaller( g_texture_globals.m_nTextureCompressionFormat ),
		    IntExportCaller( reinterpret_cast<int&>( g_texture_globals.m_nTextureCompressionFormat ) )
		);
		combo->installEventFilter( new Filter( combo ) );
	}
	page.appendCheckBox( "", "Anisotropy",
	                     FreeCaller<void(bool), Textures_SetAnisotropy>(),
	                     BoolExportCaller( g_TextureAnisotropy ) );
}
void Textures_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Textures", "Texture Settings" ) );
	Textures_constructPreferences( page );
}
void Textures_registerPreferencesPage(){
	PreferencesDialog_addDisplayPage( makeCallbackF( Textures_constructPage ) );
}

void TextureCompression_importString( const char* string ){
	g_texture_globals.m_nTextureCompressionFormat = static_cast<TextureCompressionFormat>( atoi( string ) );
	Textures_UpdateTextureCompressionFormat();
}
typedef FreeCaller<void(const char*), TextureCompression_importString> TextureCompressionImportStringCaller;


void Textures_Construct(){
	g_texturesmap = new TexturesMap;

	GlobalPreferenceSystem().registerPreference( "TextureCompressionFormat", TextureCompressionImportStringCaller(), IntExportStringCaller( reinterpret_cast<int&>( g_texture_globals.m_nTextureCompressionFormat ) ) );
	GlobalPreferenceSystem().registerPreference( "TextureFiltering", IntImportStringCaller( reinterpret_cast<int&>( g_texture_mode ) ), IntExportStringCaller( reinterpret_cast<int&>( g_texture_mode ) ) );
	GlobalPreferenceSystem().registerPreference( "TextureAnisotropy", BoolImportStringCaller( g_TextureAnisotropy ), BoolExportStringCaller( g_TextureAnisotropy ) );
	GlobalPreferenceSystem().registerPreference( "TextureMipLevel", IntImportStringCaller( g_Textures_mipLevel ), IntExportStringCaller( g_Textures_mipLevel ) );
	GlobalPreferenceSystem().registerPreference( "SI_Gamma", FloatImportStringCaller( g_texture_globals.fGamma ), FloatExportStringCaller( g_texture_globals.fGamma ) );

	Textures_registerPreferencesPage();

	Textures_ModeChanged();
}
void Textures_Destroy(){
	delete g_texturesmap;
}


#include "modulesystem/modulesmap.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"
#include "qerplugin.h"

class TexturesDependencies :
	public GlobalRadiantModuleRef,
	public GlobalOpenGLModuleRef,
	public GlobalPreferenceSystemModuleRef
{
	ImageModulesRef m_image_modules;
public:
	TexturesDependencies() :
		m_image_modules( GlobalRadiant().getRequiredGameDescriptionKeyValue( "texturetypes" ) ){
	}
	ImageModules& getImageModules(){
		return m_image_modules.get();
	}
};

class TexturesAPI
{
	TexturesCache* m_textures;
public:
	typedef TexturesCache Type;
	STRING_CONSTANT( Name, "*" );

	TexturesAPI(){
		Textures_Construct();

		m_textures = &GetTexturesCache();
	}
	~TexturesAPI(){
		Textures_Destroy();
	}
	TexturesCache* getTable(){
		return m_textures;
	}
};

typedef SingletonModule<TexturesAPI, TexturesDependencies> TexturesModule;
typedef Static<TexturesModule> StaticTexturesModule;
StaticRegisterModule staticRegisterTextures( StaticTexturesModule::instance() );

ImageModules& Textures_getImageModules(){
	return StaticTexturesModule::instance().getDependencies().getImageModules();
}

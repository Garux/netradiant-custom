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

#include <cstddef>
#include <cstring>
#include "generic/constant.h"

#include <QOpenGLFunctions_2_0>
#include "gtkutil/glfont.h"

/// \brief A module which wraps a runtime-binding of the standard OpenGL functions.
/// Provides convenience functions for querying availability of extensions, rendering text and error-checking.
struct OpenGLBinding
{
	INTEGER_CONSTANT( Version, 2 );
	STRING_CONSTANT( Name, "qgl" );

	/// \brief OpenGL version, extracted from the GL_VERSION string.
	int major_version, minor_version;

	/// \brief Is true if the global shared OpenGL context is valid.
	bool contextValid;

	QOpenGLFunctions_2_0 *funcs;

	OpenGLBinding() : contextValid( false ), funcs( nullptr ){
	}

	/// \brief Asserts that there no OpenGL errors have occurred since the last call to glGetError.
	void ( *assertNoErrors )( const char *file, int line );

	GLFont *m_font; // MUST be set!

	/// \brief Renders \p string at the current raster-position of the current context.
	void drawString( const char* string ) const {
		m_font->printString( string );
	}

	/// \brief Renders \p character at the current raster-position of the current context.
	void drawChar( char character ) const {
		char s[2];
		s[0] = character;
		s[1] = 0;
		drawString( s );
	}

	// ARB_texture_compression
	bool support_ARB_texture_compression;
	bool ARB_texture_compression(){
		return support_ARB_texture_compression;
	}

	// EXT_texture_compression_s3tc
	bool support_EXT_texture_compression_s3tc;
	bool EXT_texture_compression_s3tc(){
		return support_EXT_texture_compression_s3tc;
	}
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<OpenGLBinding> GlobalOpenGLModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<OpenGLBinding> GlobalOpenGLModuleRef;

inline OpenGLBinding& GlobalOpenGL(){
	return GlobalOpenGLModule::getTable();
}

inline QOpenGLFunctions_2_0& gl(){
	return *GlobalOpenGL().funcs;
}

#if defined( _DEBUG )
#define GlobalOpenGL_debugAssertNoErrors() GlobalOpenGL().assertNoErrors( __FILE__, __LINE__ )
#else
#define GlobalOpenGL_debugAssertNoErrors()
#endif

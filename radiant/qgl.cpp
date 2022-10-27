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


#include "qgl.h"

#include "debugging/debugging.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>


#include "igl.h"



void QGL_Shutdown( OpenGLBinding& table ){
	globalOutputStream() << "Shutting down OpenGL module...";

	globalOutputStream() << "Done.\n";
}


typedef struct glu_error_struct
{
	GLenum errnum;
	const char *errstr;
} GLU_ERROR_STRUCT;

GLU_ERROR_STRUCT glu_errlist[] = {
	{GL_NO_ERROR, "GL_NO_ERROR - no error"},
	{GL_INVALID_ENUM, "GL_INVALID_ENUM - An unacceptable value is specified for an enumerated argument."},
	{GL_INVALID_VALUE, "GL_INVALID_VALUE - A numeric argument is out of range."},
	{GL_INVALID_OPERATION, "GL_INVALID_OPERATION - The specified operation is not allowed in the current state."},
	{GL_STACK_OVERFLOW, "GL_STACK_OVERFLOW - Function would cause a stack overflow."},
	{GL_STACK_UNDERFLOW, "GL_STACK_UNDERFLOW - Function would cause a stack underflow."},
	{GL_OUT_OF_MEMORY, "GL_OUT_OF_MEMORY - There is not enough memory left to execute the function."},
	{0, 0}
};

const GLubyte* qgluErrorString( GLenum errCode ){
	int search = 0;
	for ( search = 0; glu_errlist[search].errstr; search++ )
	{
		if ( errCode == glu_errlist[search].errnum ) {
			return (const GLubyte *)glu_errlist[search].errstr;
		}
	}
	return (const GLubyte *)"Unknown error";
}


void glInvalidFunction(){
	ERROR_MESSAGE( "calling an invalid OpenGL function" );
}


void QGL_clear( OpenGLBinding& table ){
}

int QGL_Init( OpenGLBinding& table ){
	QGL_clear( table );
	return 1;
}

int g_qglMajorVersion = 0;
int g_qglMinorVersion = 0;

// requires a valid gl context
void QGL_InitVersion(){
#if EXTENSIONS_ENABLED
	const std::size_t versionSize = 256;
	char version[versionSize];
	strncpy( version, reinterpret_cast<const char*>( gl().glGetString( GL_VERSION ) ), versionSize - 1 );
	version[versionSize - 1] = '\0';
	char* firstDot = strchr( version, '.' );
	ASSERT_NOTNULL( firstDot );
	*firstDot = '\0';
	g_qglMajorVersion = atoi( version );
	char* secondDot = strchr( firstDot + 1, '.' );
	if ( secondDot != 0 ) {
		*secondDot = '\0';
	}
	g_qglMinorVersion = atoi( firstDot + 1 );
#else
	g_qglMajorVersion = 1;
	g_qglMinorVersion = 1;
#endif
}


inline void extension_not_implemented( const char* extension ){
	globalWarningStream() << "WARNING: OpenGL driver reports support for " << extension << " but does not implement it\n";
}

float g_maxTextureAnisotropy;

float QGL_maxTextureAnisotropy(){
	return g_maxTextureAnisotropy;
}

void QGL_sharedContextCreated( OpenGLBinding& table ){
	QGL_InitVersion();

	table.major_version = g_qglMajorVersion;
	table.minor_version = g_qglMinorVersion;


	if ( QOpenGLContext::currentContext()->hasExtension( "GL_EXT_texture_filter_anisotropic" ) ) {
		gl().glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_maxTextureAnisotropy );
		globalOutputStream() << "Anisotropic filtering possible (max " << g_maxTextureAnisotropy << "x)\n";
	}
	else
	{
		globalOutputStream() << "No Anisotropic filtering available\n";
		g_maxTextureAnisotropy = 0;
	}

	table.support_ARB_texture_compression = QOpenGLContext::currentContext()->hasExtension( "GL_ARB_texture_compression" );
	table.support_EXT_texture_compression_s3tc = QOpenGLContext::currentContext()->hasExtension( "GL_EXT_texture_compression_s3tc" );
}

void QGL_sharedContextDestroyed( OpenGLBinding& table ){
	QGL_clear( table );
}


void QGL_assertNoErrors( const char *file, int line ){
	GLenum error = gl().glGetError();
	while ( error != GL_NO_ERROR )
	{
		const char* errorString = reinterpret_cast<const char*>( qgluErrorString( error ) );
		if ( error == GL_OUT_OF_MEMORY ) {
			ERROR_MESSAGE( "OpenGL out of memory error at " << file << ":" << line << ": " << errorString );
		}
		else
		{
			ERROR_MESSAGE( "OpenGL error at " << file << ":" << line << ": " << errorString );
		}
		error = gl().glGetError();
	}
}


class QglAPI
{
	OpenGLBinding m_qgl;
public:
	typedef OpenGLBinding Type;
	STRING_CONSTANT( Name, "*" );

	QglAPI(){
		QGL_Init( m_qgl );

		m_qgl.assertNoErrors = &QGL_assertNoErrors;
	}
	~QglAPI(){
		QGL_Shutdown( m_qgl );
	}
	OpenGLBinding* getTable(){
		return &m_qgl;
	}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<QglAPI> QglModule;
typedef Static<QglModule> StaticQglModule;
StaticRegisterModule staticRegisterQgl( StaticQglModule::instance() );

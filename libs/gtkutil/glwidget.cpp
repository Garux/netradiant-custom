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

// OpenGL widget based on GtkGLExt

#include "glwidget.h"

#include "debugging/debugging.h"

#include "igl.h"

#include <QSurfaceFormat>
#include <QCoreApplication>
#include <QOpenGLWidget>

void ( *GLWidget_sharedContextCreated )() = 0;
void ( *GLWidget_sharedContextDestroyed )() = 0;

unsigned int g_context_count = 0;


void glwidget_context_created( QOpenGLWidget& widget ){
	globalOutputStream() << "OpenGL window configuration:"
		<< " version: " << widget.format().majorVersion() << '.' << widget.format().minorVersion()
		<< " RGBA: " << widget.format().redBufferSize() << widget.format().greenBufferSize() << widget.format().blueBufferSize() << widget.format().alphaBufferSize()
		<< " depth: " << widget.format().depthBufferSize()
		<< " swapInterval: " << widget.format().swapInterval()
		<< " samples: " << widget.format().samples()
		<< "\n";

	ASSERT_MESSAGE( widget.isValid(), "failed to create OpenGL widget" );

	if ( ++g_context_count == 1 ) {
		GlobalOpenGL().funcs = widget.context()->versionFunctions<QOpenGLFunctions_2_0>();
		ASSERT_MESSAGE( GlobalOpenGL().funcs, "failed to initializeOpenGLFunctions" );
		GlobalOpenGL().contextValid = true;

		GLWidget_sharedContextCreated();
	}
}

void glwidget_context_destroyed(){
	if ( --g_context_count == 0 ) {
		GlobalOpenGL().funcs = nullptr;
		GlobalOpenGL().contextValid = false;

		GLWidget_sharedContextDestroyed();
	}
}

void glwidget_setDefaultFormat(){
	QCoreApplication::setAttribute( Qt::ApplicationAttribute::AA_ShareOpenGLContexts );
	QSurfaceFormat format;
	format.setVersion( 2, 0 );
	format.setSwapInterval( 0 );
//	format.setSamples( 8 );
	QSurfaceFormat::setDefaultFormat( format );
}
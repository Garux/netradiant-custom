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

#include <QOpenGLFramebufferObject>
#include "debugging/debugging.h"

class FBO
{
	QOpenGLFramebufferObject *m_fbo{};
public:
	const int m_samples;

	FBO( int w, int h, bool hasDepth, int samples ) : m_samples( samples )
	{
		ASSERT_MESSAGE( QOpenGLFramebufferObject::hasOpenGLFramebufferObjects(), "QOpenGLFramebufferObject::hasOpenGLFramebufferObjects()" );
		ASSERT_MESSAGE( QOpenGLFramebufferObject::hasOpenGLFramebufferBlit(), "QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()" );

		QOpenGLFramebufferObjectFormat format;
		if( hasDepth )
			format.setAttachment( QOpenGLFramebufferObject::Attachment::Depth );
		format.setSamples( samples );
		m_fbo = new QOpenGLFramebufferObject( w, h, format );

		ASSERT_MESSAGE( m_fbo->isValid(), "m_fbo->isValid()" );
	}
	FBO( FBO&& ) noexcept = delete;
	~FBO(){
		delete m_fbo;
	}
	bool bind(){
		if( m_fbo->format().samples() )
			gl().glEnable( GL_MULTISAMPLE );
		else
			gl().glDisable( GL_MULTISAMPLE );
		return m_fbo->bind();
	}
	bool release(){
		return m_fbo->release();
	}
	void blit(){
		QOpenGLFramebufferObject::blitFramebuffer( nullptr, m_fbo );
	}
};

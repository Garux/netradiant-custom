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

#if !defined ( INCLUDED_XORRECTANGLE_H )
#define INCLUDED_XORRECTANGLE_H

#include "rect_t.h"
#include "igl.h"

class XORRectangle {
	void draw( const rect_t& rect, const GLenum mode ) const{
		glBegin( mode );
		glVertex2f( rect.min[0], rect.max[1] );
		glVertex2f( rect.max[0], rect.max[1] );
		glVertex2f( rect.max[0], rect.min[1] );
		glVertex2f( rect.min[0], rect.min[1] );
		glEnd();
	}
public:
	XORRectangle() {
	}
	~XORRectangle() {
	}
	void set( rect_t rect, int width, int height ) {
		if( rect.max[0] - rect.min[0] != 0.f && rect.max[1] - rect.min[1] != 0.f ) {
			GlobalOpenGL_debugAssertNoErrors();

			glViewport( 0, 0, width, height );
			// set up viewpoint
			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
			glOrtho( -1, 1, -1, 1, -100, 100 );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_TEXTURE_2D );

			if( GlobalOpenGL().GL_1_3() ) {
				glDisable( GL_MULTISAMPLE );
			}

			glEnable( GL_BLEND );
			/* additive to handle dark background */
			glBlendFunc( GL_ONE, GL_ONE );
			const float r = 10.f;
			switch ( rect.modifier )
			{
			case rect_t::eSelect:   glColor3f( 1.f / r, .5f / r, 0.f );     break;
			case rect_t::eDeselect: glColor3f( 0.f, 0.f, 1.f / r  );        break;
			case rect_t::eToggle:   glColor3f( 1.f / r, 1.f / r, 1.f / r ); break;
			}
			draw( rect, GL_QUADS );
			/* filter to handle bright background */
			glBlendFunc( GL_ZERO, GL_SRC_COLOR );
			switch ( rect.modifier )
			{
			case rect_t::eSelect:   glColor3f( 1.f, .9f, 0.7f );    break;
			case rect_t::eDeselect: glColor3f( 0.8f, 0.8f, 1.f  );  break;
			case rect_t::eToggle:   glColor3f( .8f, .8f, .8f );     break;
			}
			draw( rect, GL_QUADS );
			/* alpha blend on top */
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			const float a = .3f;
			switch ( rect.modifier )
			{
			case rect_t::eSelect:   glColor4f( 1.f, .5f, 0.f, a );  break;
			case rect_t::eDeselect: glColor4f( 0.f, 0.f, 1.f, a );  break;
			case rect_t::eToggle:   glColor4f( 1.f, 1.f, 1.f, a );  break;
			}
			draw( rect, GL_QUADS );

			glDisable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glLineWidth( 1 );
			glColor3f( 1.f, .5f, 0.f );
			draw( rect, GL_LINE_LOOP );

			GlobalOpenGL_debugAssertNoErrors();
		}
	}
};


#endif

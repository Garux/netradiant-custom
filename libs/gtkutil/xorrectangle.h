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

#include "rect_t.h"
#include "igl.h"

class XORRectangle {
	void draw( const rect_t& rect, const GLenum mode ) const {
		gl().glBegin( mode );
		gl().glVertex2f( rect.min[0], rect.max[1] );
		gl().glVertex2f( rect.max[0], rect.max[1] );
		gl().glVertex2f( rect.max[0], rect.min[1] );
		gl().glVertex2f( rect.min[0], rect.min[1] );
		gl().glEnd();
	}
public:
	void render( rect_t rect, int viewWidth, int viewHeight ) {
		if( rect.max[0] != rect.min[0] && rect.max[1] != rect.min[1] ) {
			GlobalOpenGL_debugAssertNoErrors();

			gl().glViewport( 0, 0, viewWidth, viewHeight );
			// set up viewpoint
			gl().glMatrixMode( GL_PROJECTION );
			gl().glLoadIdentity();
			gl().glOrtho( -1, 1, -1, 1, -100, 100 );

			gl().glMatrixMode( GL_MODELVIEW );
			gl().glLoadIdentity();

			gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			gl().glDisable( GL_DEPTH_TEST );
			gl().glDisable( GL_TEXTURE_2D );

			gl().glDisable( GL_MULTISAMPLE );

			gl().glEnable( GL_BLEND );
			/* additive to handle dark background */
			gl().glBlendFunc( GL_ONE, GL_ONE );
			const float r = 10.f;
			switch ( rect.modifier )
			{
			case rect_t::eSelect:   gl().glColor3f( 1.f / r, .5f / r, 0.f     ); break;
			case rect_t::eDeselect: gl().glColor3f( 0.f    , 0.f    , 1.f / r ); break;
			case rect_t::eToggle:   gl().glColor3f( 1.f / r, 1.f / r, 1.f / r ); break;
			}
			draw( rect, GL_QUADS );
			/* filter to handle bright background */
			gl().glBlendFunc( GL_ZERO, GL_SRC_COLOR );
			switch ( rect.modifier )
			{
			case rect_t::eSelect:   gl().glColor3f( 1.f, .9f, .7f ); break;
			case rect_t::eDeselect: gl().glColor3f( .8f, .8f, 1.f ); break;
			case rect_t::eToggle:   gl().glColor3f( .8f, .8f, .8f ); break;
			}
			draw( rect, GL_QUADS );
			/* alpha blend on top */
			gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			const float a = .3f;
			switch ( rect.modifier )
			{
			case rect_t::eSelect:   gl().glColor4f( 1.f, .5f, 0.f, a ); break;
			case rect_t::eDeselect: gl().glColor4f( 0.f, 0.f, 1.f, a ); break;
			case rect_t::eToggle:   gl().glColor4f( 1.f, 1.f, 1.f, a ); break;
			}
			draw( rect, GL_QUADS );

			gl().glDisable( GL_BLEND );
			gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			gl().glLineWidth( 1 );
			gl().glColor3f( 1.f, .5f, 0.f );
			draw( rect, GL_LINE_LOOP );

			GlobalOpenGL_debugAssertNoErrors();
		}
	}
};

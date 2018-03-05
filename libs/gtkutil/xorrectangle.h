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

#include <gtk/gtkwidget.h>
#include "math/vector.h"


#include "gtkutil/glwidget.h"
#include "igl.h"

#include <gtk/gtkglwidget.h>

//#include "stream/stringstream.h"


class rectangle_t
{
public:
rectangle_t()
	: x( 0 ), y( 0 ), w( 0 ), h( 0 )
{}
rectangle_t( float _x, float _y, float _w, float _h )
	: x( _x ), y( _y ), w( _w ), h( _h )
{}
float x;
float y;
float w;
float h;
};

struct Coord2D
{
	float x, y;
	Coord2D( float _x, float _y )
		: x( _x ), y( _y ){
	}
};

inline Coord2D coord2d_device2screen( const Coord2D& coord, unsigned int width, unsigned int height ){
	return Coord2D( ( ( coord.x + 1.0f ) * 0.5f ) * width, ( ( coord.y + 1.0f ) * 0.5f ) * height );
}

inline rectangle_t rectangle_from_area( const float min[2], const float max[2], unsigned int width, unsigned int height ){
	Coord2D botleft( coord2d_device2screen( Coord2D( min[0], min[1] ), width, height ) );
	Coord2D topright( coord2d_device2screen( Coord2D( max[0], max[1] ), width, height ) );
	return rectangle_t( botleft.x, botleft.y, topright.x - botleft.x, topright.y - botleft.y );
}

class XORRectangle {
public:
	XORRectangle() {
	}
	~XORRectangle() {
	}
	void set( rectangle_t rectangle, int width, int height ) {
		if( rectangle.w != 0.f && rectangle.h != 0.f ) {
			GlobalOpenGL_debugAssertNoErrors();

			glViewport( 0, 0, width, height );
			// set up viewpoint
			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
			glOrtho( 0, width, 0, height, -100, 100 );

			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_TEXTURE_2D );

			if( GlobalOpenGL().GL_1_3() ) {
				glDisable( GL_MULTISAMPLE );
			}

			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE, GL_ONE );
			//glColor4f( 0.94902f / 5.f, 0.396078f / 5.f, 0.133333f / 5.f, .2f );
			glColor3f( 1.f / 10.f, .5f / 10.f, 0.f );

			glBegin( GL_QUADS );
			glVertex2f( rectangle.x, rectangle.y + rectangle.h );
			glVertex2f( rectangle.x + rectangle.w, rectangle.y + rectangle.h );
			glVertex2f( rectangle.x + rectangle.w, rectangle.y );
			glVertex2f( rectangle.x, rectangle.y );
			glEnd();

			glDisable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

			glLineWidth( 1 );
			//glColor3f( 0.94902f, 0.396078f, 0.133333f );
			glColor3f( 1.f, .5f, 0.f );

			glBegin( GL_LINE_LOOP );
			glVertex2f( rectangle.x, rectangle.y + rectangle.h );
			glVertex2f( rectangle.x + rectangle.w, rectangle.y + rectangle.h );
			glVertex2f( rectangle.x + rectangle.w, rectangle.y );
			glVertex2f( rectangle.x, rectangle.y );
			glEnd();

			GlobalOpenGL_debugAssertNoErrors();
		}
	}
};


#endif

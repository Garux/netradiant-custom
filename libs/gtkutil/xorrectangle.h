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

class XORRectangle
{
rectangle_t m_rectangle;
GtkWidget* m_widget;

public:
XORRectangle( GtkWidget* widget ) : m_widget( widget ){
}
~XORRectangle(){
}
void set( rectangle_t rectangle ){
	if ( GTK_WIDGET_REALIZED( m_widget ) ) {
		if( m_rectangle.w != rectangle.w || m_rectangle.h != rectangle.h ){
		//if( !(m_rectangle.w == 0 && m_rectangle.h == 0 && rectangle.w == 0 && rectangle.h == 0) ){
		//globalOutputStream() << "m_x" << m_rectangle.x << " m_y" << m_rectangle.y << " m_w" << m_rectangle.w << " m_h" << m_rectangle.h << "\n";
		//globalOutputStream() << "__x" << rectangle.x << " __y" << rectangle.y << " __w" << rectangle.w << " __h" << rectangle.h << "\n";
			if ( glwidget_make_current( m_widget ) != FALSE ) {
				GlobalOpenGL_debugAssertNoErrors();

				gint width, height;
				gdk_gl_drawable_get_size( gtk_widget_get_gl_drawable( m_widget ), &width, &height );

				glViewport( 0, 0, width, height );
				glMatrixMode( GL_PROJECTION );
				glLoadIdentity();
				glOrtho( 0, width, 0, height, -100, 100 );

				glMatrixMode( GL_MODELVIEW );
				glLoadIdentity();

				glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
				glDisable( GL_DEPTH_TEST );

				glDrawBuffer( GL_FRONT );

				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ZERO );

				glLineWidth( 2 );
				glColor3f( 1, 1, 1 );
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_LINE_LOOP );
				glVertex2f( m_rectangle.x, m_rectangle.y + m_rectangle.h );
				glVertex2f( m_rectangle.x + m_rectangle.w, m_rectangle.y + m_rectangle.h );
				glVertex2f( m_rectangle.x + m_rectangle.w, m_rectangle.y );
				glVertex2f( m_rectangle.x, m_rectangle.y );
				glEnd();

				glBegin( GL_LINE_LOOP );
				glVertex2f( rectangle.x, rectangle.y + rectangle.h );
				glVertex2f( rectangle.x + rectangle.w, rectangle.y + rectangle.h );
				glVertex2f( rectangle.x + rectangle.w, rectangle.y );
				glVertex2f( rectangle.x, rectangle.y );
				glEnd();

				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				glDrawBuffer( GL_BACK );
				GlobalOpenGL_debugAssertNoErrors();
				//glwidget_swap_buffers( m_widget );
				glwidget_make_current( m_widget );
			}
		}
		m_rectangle = rectangle;
	}
}
};


#endif

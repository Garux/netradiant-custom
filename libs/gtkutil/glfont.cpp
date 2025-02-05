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

#include "glfont.h"
#include "igl.h"
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>
#include "stream/stringstream.h"


void gray_to_texture( const unsigned int x_max, const unsigned int y_max, const unsigned char *in, unsigned char *out, const unsigned char fontColorR, const unsigned char fontColorG, const unsigned char fontColorB ){ /* normal with shadow */
	unsigned int x, y, bitmapIter = 0;

	const unsigned char backgroundColorR = 0;
	const unsigned char backgroundColorG = 0;
	const unsigned char backgroundColorB = 0;

	for( y = 0; y < y_max; y++ ) {
		for( x = 0; x < x_max; x++ ) {
			const unsigned int iter = ( y * x_max + x ) * 4;
			if( x == 0 || y == 0 || x == 1 || y == 1 ) {
				out[iter] = fontColorB;
				out[iter + 1] = fontColorG;
				out[iter + 2] = fontColorR;
				out[iter + 3] = 0;
				continue;
			}
			if( in[bitmapIter] == 0 ){
				out[iter] = fontColorB;
				out[iter + 1] = fontColorG;
				out[iter + 2] = fontColorR;
				out[iter + 3] = 0;
			}
			else{
				out[iter] = backgroundColorB;
				out[iter + 1] = backgroundColorG;
				out[iter + 2] = backgroundColorR;
				out[iter + 3] = in[bitmapIter];
			}
			++bitmapIter;
		}
	}

	bitmapIter = 0;
	for( y = 0; y < y_max; y++ ) {
		for( x = 0; x < x_max; x++ ) {
			const unsigned int iter = ( y * x_max + x ) * 4;
			if( x == 0 || y == 0 || x == ( x_max - 1 ) || y == ( y_max - 1 ) ) {
				continue;
			}
			if( in[bitmapIter] != 0 ) {
				if( out[iter + 3] == 0 ){
					out[iter] = fontColorB;
					out[iter + 1] = fontColorG;
					out[iter + 2] = fontColorR;
					out[iter + 3] = in[bitmapIter];
				}
				else{
					/* Calculate alpha (opacity). */
					const float opacityFont = in[bitmapIter] / 255.f;
					const float opacityBack = out[iter + 3] / 255.f;
					out[iter] = fontColorB * opacityFont + ( 1 - opacityFont ) * backgroundColorB;
					out[iter + 1] = fontColorG * opacityFont + ( 1 - opacityFont ) * backgroundColorG;
					out[iter + 2] = fontColorR * opacityFont + ( 1 - opacityFont ) * backgroundColorR;
					out[iter + 3] = ( opacityFont + ( 1 - opacityFont ) * opacityBack ) * 255.f;
				}
			}
			++bitmapIter;
		}
	}
}


// generic string printing with call lists
class GLFontCallList final : public GLFont
{
	GLuint m_displayList;
	GLuint m_atlas;
	QFont m_font;
	QFontMetrics m_metrics;
	const int m_pixelHeight;
	const int m_pixelAscent;
	const int m_pixelDescent;
public:
	GLFontCallList( GLuint displayList, GLuint atlas, const QFont& font, const QFontMetrics& metrics ) :
		m_displayList( displayList ), m_atlas( atlas ), m_font( font ), m_metrics( metrics ),
		m_pixelHeight( m_metrics.height() ), m_pixelAscent( m_metrics.ascent() ), m_pixelDescent( m_metrics.descent() ){
	}
	~GLFontCallList(){
		gl().glDeleteLists( m_displayList, 128 );
		gl().glDeleteTextures( 1, &m_atlas );
	}
	void printString( const char *s ){
		GLboolean rasterPosValid;
		gl().glGetBooleanv( GL_CURRENT_RASTER_POSITION_VALID, &rasterPosValid );
		if( !rasterPosValid )
			return;
		GLfloat rasterPos[4];
		gl().glGetFloatv( GL_CURRENT_RASTER_POSITION, rasterPos );

		GLint viewport[4];
		gl().glGetIntegerv( GL_VIEWPORT, viewport );
		gl().glMatrixMode( GL_PROJECTION );
		gl().glPushMatrix();
		gl().glLoadIdentity();
		gl().glOrtho( viewport[0], viewport[2], viewport[1], viewport[3], -1, 1 );

		gl().glMatrixMode( GL_MODELVIEW );
		gl().glPushMatrix();
		gl().glLoadIdentity();
		gl().glTranslatef( rasterPos[0], rasterPos[1], 0 );

		gl().glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT );
		gl().glDisable( GL_LIGHTING );
		gl().glEnable( GL_TEXTURE_2D );
		gl().glDisable( GL_DEPTH_TEST );
		gl().glEnable( GL_BLEND );
		gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

		gl().glBindTexture( GL_TEXTURE_2D, m_atlas );
		gl().glListBase( m_displayList );
		gl().glCallLists( GLsizei( strlen( s ) ), GL_UNSIGNED_BYTE, reinterpret_cast<const GLubyte*>( s ) );

		gl().glPopAttrib();

		gl().glMatrixMode( GL_PROJECTION );
		gl().glPopMatrix();
		gl().glMatrixMode( GL_MODELVIEW ); //! must leave GL_MODELVIEW mode, as renderer relies on this during Renderer.render()
		gl().glPopMatrix();
	}

	void renderString( const char *s, const GLuint& tex, const unsigned char colour[3], unsigned int& out_wid, unsigned int& out_hei ){
		// proper way would be using painter.metrics, however this requires it being active()...
		// same for painter.boundingRect() + result is not correct wrt width for some reason
		const QRect rect = m_metrics.boundingRect( s );
		unsigned int wid = rect.width();
		unsigned int hei = rect.height();

		if ( wid > 0 && hei > 0 ) {
			QImage image( wid, hei, QImage::Format::Format_Alpha8 );
			image.fill( 0 );
			QPainter painter;
			painter.begin( &image );
			painter.setFont( m_font );
			painter.drawText( 0, m_metrics.height() - m_metrics.descent(), s );
			painter.end();

			// using image.constBits() is inconsistently buggy for some reason
			unsigned char *boo = (unsigned char *) malloc( wid * hei );
			for( unsigned int w = 0; w < wid; ++w )
				for( unsigned int h = 0; h < hei; ++h )
					boo[wid * h + w] = qAlpha( image.pixel( w, h ) );

			hei += 2;
			wid += 2;
			unsigned char *buf = (unsigned char *) malloc( 4 * hei * wid );
			memset( buf, 0x00, 4 * hei * wid );

#if 0
			GLfloat color[4] = { 1, 0, 0, 1 };
			guint32 rgb = ( ( ( guint32 )( color[0] * 255.0 ) ) << 24 ) |
			              ( ( ( guint32 )( color[1] * 255.0 ) ) << 16 ) |
			              ( ( ( guint32 )( color[2] * 255.0 ) ) << 8 );

			GLfloat a = color[3];

			guint32 *t;
			guint8 *row, *row_end;
			row = bitmap.buffer + bitmap.rows * bitmap.width; /* past-the-end */
			row_end = bitmap.buffer;      /* beginning */

			t = ( guint32* ) buf;

			if( a == 1.0 ) {
				do {
					row -= bitmap.width;
					for( unsigned int i = 0; i < bitmap.width; i++ )
						*t++ = rgb | ( ( guint32 ) row[i] );

				} while( row != row_end );
			} else
			{
				do
				{
					row -= bitmap.width;
					for( unsigned int i = 0; i < bitmap.width; i++ )
						* t++ = rgb | ( ( guint32 )( a* row[i] ) );
				} while( row != row_end );
			}
#endif // 0

#if 0
			if( withfond ){ /* with background rectangle */
				const unsigned int x_max = wid;
				const unsigned int y_max = hei;
				unsigned int x, y, bitmapIter = 0;

				const unsigned char fontColorR = 255;
				const unsigned char fontColorG = 255;
				const unsigned char fontColorB = 0;

				const unsigned char backgroundColorR = 64;
				const unsigned char backgroundColorG = 64;
				const unsigned char backgroundColorB = 64;

				for( y = 0; y < y_max; y++ ) {
					for( x = 0; x < x_max; x++ ) {
						const unsigned int iter = ( y * x_max + x ) * 4;
						if( x == 0 || y == 0 || x == ( x_max - 1 ) || y == ( y_max - 1 ) ) {
							buf[iter] = backgroundColorB;
							buf[iter + 1] = backgroundColorG;
							buf[iter + 2] = backgroundColorR;
							buf[iter + 3] = 150; /* half trans border */
							continue;
						}
						if( bitmap.buffer[bitmapIter] == 0 ) {
							/* Render background color. */
							buf[iter] = backgroundColorB;
							buf[iter + 1] = backgroundColorG;
							buf[iter + 2] = backgroundColorR;
							buf[iter + 3] = 255;
						}
						else {
							/* Calculate alpha (opacity). */
							const float opacity = bitmap.buffer[bitmapIter] / 255.f;
							buf[iter] = fontColorB * opacity + ( 1 - opacity ) * backgroundColorB;
							buf[iter + 1] = fontColorG * opacity + ( 1 - opacity ) * backgroundColorG;
							buf[iter + 2] = fontColorR * opacity + ( 1 - opacity ) * backgroundColorR;
							buf[iter + 3] = 255;
						}
						++bitmapIter;
					}
				}
			}
#elif 0
			if( 1 ){ /* normal with shadow */
				const unsigned int x_max = wid;
				const unsigned int y_max = hei;
				unsigned int x, y, bitmapIter = 0;

//			unsigned char fontColorR = 255;
//			unsigned char fontColorG = 255;
//			unsigned char fontColorB = 0;
				unsigned char fontColorR = colour[0];
				unsigned char fontColorG = colour[1];
				unsigned char fontColorB = colour[2];

				unsigned char backgroundColorR = 0;
				unsigned char backgroundColorG = 0;
				unsigned char backgroundColorB = 0;

				for( y = 0; y < y_max; y++ ) {
					for( x = 0; x < x_max; x++ ) {
						const unsigned int iter = ( y * x_max + x ) * 4;
						if( x == 0 || y == 0 || x == 1 || y == 1 ) {
							buf[iter] = fontColorB;
							buf[iter + 1] = fontColorG;
							buf[iter + 2] = fontColorR;
							buf[iter + 3] = 0;
							continue;
						}
						if( bitmap.buffer[bitmapIter] == 0 ){
							buf[iter] = fontColorB;
							buf[iter + 1] = fontColorG;
							buf[iter + 2] = fontColorR;
							buf[iter + 3] = 0;
						}
						else{
							buf[iter] = backgroundColorB;
							buf[iter + 1] = backgroundColorG;
							buf[iter + 2] = backgroundColorR;
							buf[iter + 3] = bitmap.buffer[bitmapIter];
						}
						++bitmapIter;
					}
				}

				bitmapIter = 0;
				for( y = 0; y < y_max; y++ ) {
					for( x = 0; x < x_max; x++ ) {
						const unsigned int iter = ( y * x_max + x ) * 4;
						if( x == 0 || y == 0 || x == ( x_max - 1 ) || y == ( y_max - 1 ) ) {
							continue;
						}
						if( bitmap.buffer[bitmapIter] != 0 ) {
							if( buf[iter + 3] == 0 ){
								buf[iter] = fontColorB;
								buf[iter + 1] = fontColorG;
								buf[iter + 2] = fontColorR;
								buf[iter + 3] = bitmap.buffer[bitmapIter];
							}
							else{
								/* Calculate alpha (opacity). */
								const float opacityFont = bitmap.buffer[bitmapIter] / 255.f;
								const float opacityBack = buf[iter + 3] / 255.f;
								buf[iter] = fontColorB * opacityFont + ( 1 - opacityFont ) * backgroundColorB;
								buf[iter + 1] = fontColorG * opacityFont + ( 1 - opacityFont ) * backgroundColorG;
								buf[iter + 2] = fontColorR * opacityFont + ( 1 - opacityFont ) * backgroundColorR;
								buf[iter + 3] = ( opacityFont + ( 1 - opacityFont ) * opacityBack ) * 255.f;
							}
						}
						++bitmapIter;
					}
				}
			}
			else{ /* normal */
				const unsigned int x_max = wid;
				const unsigned int y_max = hei;
				unsigned int x, y, bitmapIter = 0;

//				unsigned char fontColorR = 0;
//				unsigned char fontColorG = 255;
//				unsigned char fontColorB = 0;
				unsigned char fontColorR = colour[0];
				unsigned char fontColorG = colour[1];
				unsigned char fontColorB = colour[2];

				for( y = 0; y < y_max; y++ ) {
					for( x = 0; x < x_max; x++ ) {
						const unsigned int iter = ( y * x_max + x ) * 4;
						if( x == 0 || y == 0 || x == ( x_max - 1 ) || y == ( y_max - 1 ) ) {
							buf[iter] = fontColorB;
							buf[iter + 1] = fontColorG;
							buf[iter + 2] = fontColorR;
							buf[iter + 3] = 0;
							continue;
						}
						buf[iter] = fontColorB;
						buf[iter + 1] = fontColorG;
						buf[iter + 2] = fontColorR;
						buf[iter + 3] = bitmap.buffer[bitmapIter];
						++bitmapIter;
					}
				}
			}
#endif // 0



			//Now we just setup some texture parameters.
			gl().glBindTexture( GL_TEXTURE_2D, tex );
			gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

//		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
//		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
//		gl().glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );

			gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
			gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
//		gl().glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			//Here we actually create the texture itself
			gl().glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, wid * 3, hei,
			              0, GL_BGRA, GL_UNSIGNED_BYTE, 0 );

			/* normal with shadow */
			gray_to_texture( wid, hei, boo, buf, colour[0], colour[1], colour[2] );
			gl().glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, wid, hei, GL_BGRA, GL_UNSIGNED_BYTE, buf );

			memset( buf, 0x00, 4 * hei * wid );
			/* yellow selected with shadow */
			gray_to_texture( wid, hei, boo, buf, 255, 255, 0 );
			gl().glTexSubImage2D( GL_TEXTURE_2D, 0, wid, 0, wid, hei, GL_BGRA, GL_UNSIGNED_BYTE, buf );

			memset( buf, 0x00, 4 * hei * wid );
			/* orange childSelected with shadow */
			gray_to_texture( wid, hei, boo, buf, 255, 128, 0 );
			gl().glTexSubImage2D( GL_TEXTURE_2D, 0, wid * 2, 0, wid, hei, GL_BGRA, GL_UNSIGNED_BYTE, buf );


			gl().glBindTexture( GL_TEXTURE_2D, 0 );

			free( buf );
			free( boo );

			out_wid = wid;
			out_hei = hei;
		}
	}
	int getPixelAscent() const {
		return m_pixelAscent;
	}
	int getPixelDescent() const {
		return m_pixelDescent;
	}
	int getPixelHeight() const {
		return m_pixelHeight;
	}
};

#ifdef _WIN32
#include <windows.h>
#endif
#include "debugging/debugging.h"

// LordHavoc: this is code for direct Xlib bitmap character fetching, as an
// alternative to requiring gtkglarea, it was created due to a lack of this
// package on SuSE 9.x but this package is now commonly shipping in Linux
// distributions so this code may be unnecessary, feel free however to enable
// it when building packages for distros that do not ship with that package,
// or if you just prefer less dependencies...
#if 0

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <GL/glx.h>

GLFont *glfont_create( const char* font_string ){
	GLuint font_list_base;
	XFontStruct *fontInfo;
	Display *dpy = GDK_DISPLAY();
	unsigned int i, first, last, firstrow, lastrow;
	int maxchars;
	int firstbitmap;

	fontInfo = XLoadQueryFont( dpy, "-*-fixed-*-*-*-*-8-*-*-*-*-*-*-*" );
	if ( fontInfo == NULL ) {
		// try to load other fonts
		fontInfo = XLoadQueryFont( dpy, "-*-fixed-*-*-*-*-*-*-*-*-*-*-*-*" );

		// any font will do !
		if ( fontInfo == NULL ) {
			fontInfo = XLoadQueryFont( dpy, "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" );
		}

		if ( fontInfo == NULL ) {
			ERROR_MESSAGE( "couldn't create font" );
		}
	}

	first = (int)fontInfo->min_char_or_byte2;
	last = (int)fontInfo->max_char_or_byte2;
	firstrow = (int)fontInfo->min_byte1;
	lastrow = (int)fontInfo->max_byte1;
	/*
	 * How many chars in the charset
	 */
	maxchars = 256 * lastrow + last;
	font_list_base = gl().glGenLists( maxchars + 1 );
	if ( font_list_base == 0 ) {
		ERROR_MESSAGE( "couldn't create font" );
	}

	/*
	 * Get offset to first char in the charset
	 */
	firstbitmap = 256 * firstrow + first;
	/*
	 * for each row of chars, call glXUseXFont to build the bitmaps.
	 */

	for ( i = firstrow; i <= lastrow; i++ )
	{
		glXUseXFont( fontInfo->fid, firstbitmap, last - first + 1, font_list_base + firstbitmap );
		firstbitmap += 256;
	}

	/*    *height = fontInfo->ascent + fontInfo->descent;
	 *width = fontInfo->max_bounds.width;  */
	return new GLFontCallList( font_list_base, fontInfo->ascent, fontInfo->descent, fontInfo->ascent + fontInfo->descent );
}

#elif 0

#include <gtk/gtkglwidget.h>

GLFont *glfont_create( const char* font_string ){
	GLuint font_list_base = gl().glGenLists( 256 );
	gint font_height = 0, font_ascent = 0, font_descent = 0;

	PangoFontDescription* font_desc = pango_font_description_from_string( font_string );

	PangoFont* font = gdk_gl_font_use_pango_font( font_desc, 0, 256, font_list_base );

	if ( font == 0 ) {
		pango_font_description_free( font_desc );
		font_desc = pango_font_description_from_string( "fixed 8" );
		font = gdk_gl_font_use_pango_font( font_desc, 0, 256, font_list_base );
	}

	if ( font == 0 ) {
		pango_font_description_free( font_desc );
		font_desc = pango_font_description_from_string( "courier new 8" );
		font = gdk_gl_font_use_pango_font( font_desc, 0, 256, font_list_base );
	}

	if ( font != 0 ) {
		PangoFontMetrics* font_metrics = pango_font_get_metrics( font, 0 );

		font_ascent = pango_font_metrics_get_ascent( font_metrics );
		font_descent = pango_font_metrics_get_descent( font_metrics );
		font_height = font_ascent + font_descent;

		font_ascent = PANGO_PIXELS( font_ascent );
		font_descent = PANGO_PIXELS( font_descent );
		font_height = PANGO_PIXELS( font_height );

		pango_font_metrics_unref( font_metrics );
	}

	pango_font_description_free( font_desc );

	// fix for pango/gtkglext metrix bug
	if ( font_height > 256 ) {
		font_height = 16;
	}

	return new GLFontCallList( font_list_base, font_ascent, font_descent, font_height );
}
#elif 0

// new font code ripped from ZeroRadiant

#include <pango/pangoft2.h>
#include <pango/pango-features.h>	//has PANGO_VERSION_CHECK
#include <pango/pango-utils.h>

class GLFontInternal : public GLFont
{
	const char *font_string;
	int font_height;
	int font_ascent;
	int font_descent;
	int y_offset_bitmap_render_pango_units;
	PangoContext *ft2_context;
	PangoFontMap *fontmap;

public:
	GLFontInternal( const char *_font_string ) : font_string( _font_string ){
		PangoFontDescription *font_desc;
		PangoLayout *layout;
		PangoRectangle log_rect;
		int font_ascent_pango_units;
		int font_descent_pango_units;

#if !PANGO_VERSION_CHECK( 1,22,0 )
		ft2_context = pango_ft2_get_context( 72, 72 );
#else
		fontmap = pango_ft2_font_map_new();
		pango_ft2_font_map_set_resolution( PANGO_FT2_FONT_MAP( fontmap ), 72, 72 );
		ft2_context = pango_font_map_create_context( fontmap );
#endif

		font_desc = pango_font_description_from_string( font_string );
		//pango_font_description_set_size( font_desc, 10 * PANGO_SCALE );
		pango_context_set_font_description( ft2_context, font_desc );
		pango_font_description_free( font_desc );
		// TODO fallback to fixed 8, courier new 8

		layout = pango_layout_new( ft2_context );

#ifdef FONT_SIZE_WORKAROUND
		pango_layout_set_width( layout, -1 );   // -1 no wrapping.  All text on one line.
		pango_layout_set_text( layout, "The quick brown fox jumped over the lazy sleeping dog's back then sat on a tack.", -1 );   // -1 null-terminated string.
#endif

#if !PANGO_VERSION_CHECK( 1,22,0 )
		PangoLayoutIter *iter;
		iter = pango_layout_get_iter( layout );
		font_ascent_pango_units = pango_layout_iter_get_baseline( iter );
		pango_layout_iter_free( iter );
#else
		font_ascent_pango_units = pango_layout_get_baseline( layout );
#endif
		pango_layout_get_extents( layout, NULL, &log_rect );
		g_object_unref( G_OBJECT( layout ) );
		font_descent_pango_units = log_rect.height - font_ascent_pango_units;

		font_ascent = PANGO_PIXELS_CEIL( font_ascent_pango_units );
		font_descent = PANGO_PIXELS_CEIL( font_descent_pango_units );
		font_height = font_ascent + font_descent;
		y_offset_bitmap_render_pango_units = ( font_ascent * PANGO_SCALE ) - font_ascent_pango_units;
	}

	virtual ~GLFontInternal(){
		g_object_unref( G_OBJECT( ft2_context ) );
		g_object_unref( G_OBJECT( fontmap ) );
	}

// Renders the input text at the current location with the current color.
// The X position of the current location is used to place the left edge of the text image,
// where the text image bounds are defined as the logical extents of the line of text.
// The Y position of the current location is used to place the bottom of the text image.
// You should offset the Y position by the amount returned by gtk_glwidget_font_descent()
// if you want to place the baseline of the text image at the current Y position.
// Note: A problem with this function is that if the lower left corner of the text falls
// just a hair outside of the viewport (meaning the current raster position is invalid),
// then no text will be rendered.  The solution to this is a very hacky one.  You can search
// Google for "glDrawPixels clipping".
	virtual void printString( const char *s ){
		// The idea for this code initially came from the font-pangoft2.c example that comes with GtkGLExt.

		PangoLayout *layout;
		PangoRectangle log_rect;
		FT_Bitmap bitmap;
		unsigned char *begin_bitmap_buffer;
		GLfloat color[4];
		GLint previous_unpack_alignment;
		GLboolean previous_blend_enabled;
		GLint previous_blend_func_src;
		GLint previous_blend_func_dst;
		GLfloat previous_red_bias;
		GLfloat previous_green_bias;
		GLfloat previous_blue_bias;
		GLfloat previous_alpha_scale;

		layout = pango_layout_new( ft2_context );
		pango_layout_set_width( layout, -1 );   // -1 no wrapping.  All text on one line.
		pango_layout_set_text( layout, s, -1 );   // -1 null-terminated string.
		pango_layout_get_extents( layout, NULL, &log_rect );

		if ( log_rect.width > 0 && log_rect.height > 0 ) {
			bitmap.rows = font_ascent + font_descent;
			bitmap.width = PANGO_PIXELS_CEIL( log_rect.width );
			bitmap.pitch = -bitmap.width;     // Rendering it "upside down" for OpenGL.
			begin_bitmap_buffer = (unsigned char *) g_malloc( bitmap.rows * bitmap.width );
			memset( begin_bitmap_buffer, 0, bitmap.rows * bitmap.width );
			bitmap.buffer = begin_bitmap_buffer + ( bitmap.rows - 1 ) * bitmap.width;   // See pitch above.
			bitmap.num_grays = 0xff;
			bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
			pango_ft2_render_layout_subpixel( &bitmap, layout, -log_rect.x,
			                                  y_offset_bitmap_render_pango_units );
			gl().glGetFloatv( GL_CURRENT_COLOR, color );

			// Save state.  I didn't see any OpenGL push/pop operations for these.
			// Question: Is saving/restoring this state necessary?  Being safe.
			gl().glGetIntegerv( GL_UNPACK_ALIGNMENT, &previous_unpack_alignment );
			previous_blend_enabled = gl().glIsEnabled( GL_BLEND );
			gl().glGetIntegerv( GL_BLEND_SRC, &previous_blend_func_src );
			gl().glGetIntegerv( GL_BLEND_DST, &previous_blend_func_dst );
			gl().glGetFloatv( GL_RED_BIAS, &previous_red_bias );
			gl().glGetFloatv( GL_GREEN_BIAS, &previous_green_bias );
			gl().glGetFloatv( GL_BLUE_BIAS, &previous_blue_bias );
			gl().glGetFloatv( GL_ALPHA_SCALE, &previous_alpha_scale );

			gl().glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			gl().glEnable( GL_BLEND );
			gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			gl().glPixelTransferf( GL_RED_BIAS, color[0] );
			gl().glPixelTransferf( GL_GREEN_BIAS, color[1] );
			gl().glPixelTransferf( GL_BLUE_BIAS, color[2] );
			gl().glPixelTransferf( GL_ALPHA_SCALE, color[3] );

			gl().glDrawPixels( bitmap.width, bitmap.rows,
			                               GL_ALPHA, GL_UNSIGNED_BYTE, begin_bitmap_buffer );
			g_free( begin_bitmap_buffer );

			// Restore state in reverse order of how we set it.
			gl().glPixelTransferf( GL_ALPHA_SCALE, previous_alpha_scale );
			gl().glPixelTransferf( GL_BLUE_BIAS, previous_blue_bias );
			gl().glPixelTransferf( GL_GREEN_BIAS, previous_green_bias );
			gl().glPixelTransferf( GL_RED_BIAS, previous_red_bias );
			gl().glBlendFunc( previous_blend_func_src, previous_blend_func_dst );
			if ( !previous_blend_enabled ) {
				gl().glDisable( GL_BLEND );
			}
			gl().glPixelStorei( GL_UNPACK_ALIGNMENT, previous_unpack_alignment );
		}

		g_object_unref( G_OBJECT( layout ) );
	}

	virtual int getPixelAscent() const {
		return font_ascent;
	}
	virtual int getPixelDescent() const {
		return font_descent;
	}
	virtual int getPixelHeight() const {
		return font_height;
	}
};

GLFont *glfont_create( const char* font_string ){
	return new GLFontInternal( font_string );
}

#elif 1

GLFont *glfont_create( const char* family, int fontSize, const char* appPath ){
	GLuint font_list_base = gl().glGenLists( 128 );

	QFont font;
	font.setPointSize( fontSize );
	{
		const int id = QFontDatabase::addApplicationFont( QString( appPath ) + "bitmaps/MyriadPro-Regular.ttf" );
		if( id >= 0 && string_equal( family, "Myriad Pro" ) )
			font.setFamily( QFontDatabase::applicationFontFamilies( id ).at( 0 ) );
		else if( !string_empty( family ) )
			font.setFamily( family );
	}
	globalOutputStream() << "Using OpenGL font " << makeQuoted( font.toString().toLatin1().constData() ) << '\n';

	QFontMetrics metrics( font );

	/* render displaylists */
	GLuint atlas;
	{
		const int wid = metrics.maxWidth(); // <-slow func
		const int hei = metrics.height();
		const int awidth = wid * 12;
		const int aheight = hei * 12;

		QImage image( awidth, aheight, QImage::Format::Format_Alpha8 );
		image.fill( 0 );
		QPainter painter;
		painter.begin( &image );
		painter.setFont( font );
		for( unsigned char c = 0; c < 128; ++c ){
			const QRect rect = painter.boundingRect( QRect(), Qt::AlignmentFlag::AlignCenter, QString( c ) );
			painter.drawText( c % 12 * wid, ( c / 12 + 1 ) * hei - metrics.descent(), QString( c ) );

			if ( rect.width() > 0 && rect.height() > 0 ) {
				gl().glNewList( font_list_base + c, GL_COMPILE );
				gl().glBegin( GL_QUADS );
				const float x0 = 0;
				const float x1 = rect.width();
				const float y0 = 0;
				const float y1 = rect.height();
				const float tx0 = ( c % 12 ) / 12.f;
				const float tx1 = ( c % 12 + static_cast<float>( rect.width() ) / wid ) / 12.f;
				const float ty0 = ( c / 12 ) / 12.f;
				const float ty1 = ( c / 12 + static_cast<float>( rect.height() ) / hei ) / 12.f;
				gl().glTexCoord2f( tx0, ty1 );
				gl().glVertex2f( x0, y0 );
				gl().glTexCoord2f( tx0, ty0 );
				gl().glVertex2f( x0, y1 );
				gl().glTexCoord2f( tx1, ty0 );
				gl().glVertex2f( x1, y1 );
				gl().glTexCoord2f( tx1, ty1 );
				gl().glVertex2f( x1, y0 );
				gl().glEnd();
				gl().glTranslatef( metrics.horizontalAdvance( c ), 0, 0 );
				gl().glEndList();
			}
		}
		painter.end();
		// image.save( "radAtlas.png" );

		GLint alignment;
		gl().glGetIntegerv( GL_UNPACK_ALIGNMENT, &alignment );
		gl().glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

		gl().glGenTextures( 1, &atlas );
		//Now we just setup some texture parameters.
		gl().glBindTexture( GL_TEXTURE_2D, atlas );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
		gl().glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		//Here we actually create the texture itself
		GLubyte* data = (GLubyte*)calloc( awidth * aheight * 2, sizeof( GLubyte ) );
		for( int w = 0; w < awidth; ++w )
			for( int h = 0; h < aheight; ++h ){
				data[( h * awidth + w ) * 2] = 255;
				data[( h * awidth + w ) * 2 + 1] = qAlpha( image.pixel( w, h ) );
			}
		gl().glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, awidth, aheight,
		              0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data );
		free( data );

		gl().glPixelStorei( GL_UNPACK_ALIGNMENT, alignment );

		GlobalOpenGL_debugAssertNoErrors();
	}

	return new GLFontCallList( font_list_base, atlas, font, metrics );
}


#endif

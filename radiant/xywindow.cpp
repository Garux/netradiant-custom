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

//
// XY Window
//
// Leonardo Zide (leo@lokigames.com)
//

#include "xywindow.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "igl.h"
#include "ibrush.h"
#include "iundo.h"
#include "iimage.h"
#include "ifilesystem.h"
#include "os/path.h"
#include "image.h"
#include "gtkutil/messagebox.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>

#include "generic/callback.h"
#include "string/string.h"
#include "stream/stringstream.h"

#include "scenelib.h"
#include "eclasslib.h"
#include "renderer.h"
#include "moduleobserver.h"

#include "gtkutil/menu.h"
#include "gtkutil/container.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/filechooser.h"
#include "gtkmisc.h"
#include "select.h"
#include "brushmanip.h"
#include "selection.h"
#include "entity.h"
#include "camwindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "commands.h"
#include "feedback.h"
#include "grid.h"
#include "windowobservers.h"

#include "render.h"

bool g_bCamEntityMenu = false;



struct xywindow_globals_private_t
{
	bool d_showgrid;

	// these are in the View > Show menu with Show coordinates
	bool show_names;
	bool show_coordinates;
	bool show_angles;
	bool show_outline;
	bool show_axis;

	bool show_workzone;

	bool show_blocks;
	int blockSize;

	bool m_bChaseMouse;
	bool m_bShowSize;

	int m_MSAA;

	xywindow_globals_private_t() :
		d_showgrid( true ),

		show_names( false ),
		show_coordinates( false ),
		show_angles( true ),
		show_outline( true ),
		show_axis( true ),

		show_workzone( false ),

		show_blocks( false ),

		m_bChaseMouse( true ),
		m_bShowSize( true ),
		m_MSAA( 8 ){
	}

};

xywindow_globals_t g_xywindow_globals;
xywindow_globals_private_t g_xywindow_globals_private;

const unsigned int RAD_NONE =    0x00;
const unsigned int RAD_SHIFT =   0x01;
const unsigned int RAD_ALT =     0x02;
const unsigned int RAD_CONTROL = 0x04;
const unsigned int RAD_PRESS   = 0x08;
const unsigned int RAD_LBUTTON = 0x10;
const unsigned int RAD_MBUTTON = 0x20;
const unsigned int RAD_RBUTTON = 0x40;

inline ButtonIdentifier button_for_flags( unsigned int flags ){
	if ( flags & RAD_LBUTTON ) {
		return c_buttonLeft;
	}
	if ( flags & RAD_RBUTTON ) {
		return c_buttonRight;
	}
	if ( flags & RAD_MBUTTON ) {
		return c_buttonMiddle;
	}
	return c_buttonInvalid;
}

inline ModifierFlags modifiers_for_flags( unsigned int flags ){
	ModifierFlags modifiers = c_modifierNone;
	if ( flags & RAD_SHIFT ) {
		modifiers |= c_modifierShift;
	}
	if ( flags & RAD_CONTROL ) {
		modifiers |= c_modifierControl;
	}
	if ( flags & RAD_ALT ) {
		modifiers |= c_modifierAlt;
	}
	return modifiers;
}

inline unsigned int buttons_for_button_and_modifiers( ButtonIdentifier button, ModifierFlags flags ){
	unsigned int buttons = 0;

	switch ( button.get() )
	{
	case ButtonEnumeration::LEFT: buttons |= RAD_LBUTTON; break;
	case ButtonEnumeration::MIDDLE: buttons |= RAD_MBUTTON; break;
	case ButtonEnumeration::RIGHT: buttons |= RAD_RBUTTON; break;
	default: break;
	}

	if ( bitfield_enabled( flags, c_modifierControl ) ) {
		buttons |= RAD_CONTROL;
	}

	if ( bitfield_enabled( flags, c_modifierShift ) ) {
		buttons |= RAD_SHIFT;
	}

	if ( bitfield_enabled( flags, c_modifierAlt ) ) {
		buttons |= RAD_ALT;
	}

	return buttons;
}

inline unsigned int buttons_for_event_button( GdkEventButton* event ){
	unsigned int flags = 0;

	switch ( event->button )
	{
	case 1: flags |= RAD_LBUTTON; break;
	case 2: flags |= RAD_MBUTTON; break;
	case 3: flags |= RAD_RBUTTON; break;
	}

	if ( ( event->state & GDK_CONTROL_MASK ) != 0 ) {
		flags |= RAD_CONTROL;
	}

	if ( ( event->state & GDK_SHIFT_MASK ) != 0 ) {
		flags |= RAD_SHIFT;
	}

	if ( ( event->state & GDK_MOD1_MASK ) != 0 ) {
		flags |= RAD_ALT;
	}

	return flags;
}

inline unsigned int buttons_for_state( guint state ){
	unsigned int flags = 0;

	if ( ( state & GDK_BUTTON1_MASK ) != 0 ) {
		flags |= RAD_LBUTTON;
	}

	if ( ( state & GDK_BUTTON2_MASK ) != 0 ) {
		flags |= RAD_MBUTTON;
	}

	if ( ( state & GDK_BUTTON3_MASK ) != 0 ) {
		flags |= RAD_RBUTTON;
	}

	if ( ( state & GDK_CONTROL_MASK ) != 0 ) {
		flags |= RAD_CONTROL;
	}

	if ( ( state & GDK_SHIFT_MASK ) != 0 ) {
		flags |= RAD_SHIFT;
	}

	if ( ( state & GDK_MOD1_MASK ) != 0 ) {
		flags |= RAD_ALT;
	}

	return flags;
}


void XYWnd::SetScale( float f ){
	const float max_scale = 64.f;
	const float min_scale = std::min( Width(), Height() )
							/ ( ( 1.1f + ( 1.f - GetMaxGridCoord() / g_MaxWorldCoord ) ) // adaptive min scale factor: from 2.0375 with 4096 grid to 1.1 with 64*1024
							* 2.f * GetMaxGridCoord() );
	f = std::min( max_scale, std::max( min_scale, f ) );
	if( !float_equal_epsilon( m_fScale, f, float_mid( m_fScale, f ) * 1e-5f ) ){
		m_fScale = f;
		updateProjection();
		updateModelview();
		XYWnd_Update( *this );
	}
}

void XYWnd::ZoomIn(){
	SetScale( Scale() * 5.0f / 4.0f );
}

// NOTE: the zoom out factor is 4/5, we could think about customizing it
//  we don't go below a zoom factor corresponding to 10% of the max world size
//  (this has to be computed against the window size)
void XYWnd::ZoomOut(){
	SetScale( Scale() * 4.0f / 5.0f );
}

void XYWnd::ZoomInWithMouse( int x, int y ){
	const float old_scale = Scale();
	ZoomIn();
	if ( g_xywindow_globals.m_bZoomInToPointer && old_scale != Scale() ) {
		const float scale_diff = 1.0 / old_scale - 1.0 / Scale();
		NDIM1NDIM2( m_viewType )
		Vector3 origin = GetOrigin();
		origin[nDim1] += scale_diff * ( x - 0.5 * Width() );
		origin[nDim2] -= scale_diff * ( y - 0.5 * Height() );
		SetOrigin( origin );
	}
}

void XYWnd::FocusOnBounds( const AABB& bounds ){
	SetOrigin( bounds.origin );
	NDIM1NDIM2( m_viewType )
	SetScale( std::min( Width() / ( 3.f * std::max( 128.f, bounds.extents[ nDim1 ] ) ),
						Height() / ( 3.f * std::max( 128.f, bounds.extents[ nDim2 ] ) ) ) );

}

VIEWTYPE GlobalXYWnd_getCurrentViewType(){
	ASSERT_NOTNULL( g_pParentWnd );
	ASSERT_NOTNULL( g_pParentWnd->ActiveXY() );
	return g_pParentWnd->ActiveXY()->GetViewType();
}

// =============================================================================
// variables

bool g_bCrossHairs = false;

GtkMenu* XYWnd::m_mnuDrop = 0;

// this is disabled, and broken
// http://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=394
#if 0
void WXY_Print(){
	long width, height;
	width = g_pParentWnd->ActiveXY()->Width();
	height = g_pParentWnd->ActiveXY()->Height();
	unsigned char* img;
	const char* filename;

	filename = file_dialog( GTK_WIDGET( MainFrame_getWindow() ), FALSE, "Save Image", 0, FILTER_BMP );
	if ( !filename ) {
		return;
	}

	g_pParentWnd->ActiveXY()->MakeCurrent();
	img = (unsigned char*)malloc( width * height * 3 );
	glReadPixels( 0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,img );

	FILE *fp;
	fp = fopen( filename, "wb" );
	if ( fp ) {
		unsigned short bits;
		unsigned long cmap, bfSize;

		bits = 24;
		cmap = 0;
		bfSize = 54 + width * height * 3;

		long byteswritten = 0;
		long pixoff = 54 + cmap * 4;
		short res = 0;
		char m1 = 'B', m2 = 'M';
		fwrite( &m1, 1, 1, fp );      byteswritten++; // B
		fwrite( &m2, 1, 1, fp );      byteswritten++; // M
		fwrite( &bfSize, 4, 1, fp );  byteswritten += 4; // bfSize
		fwrite( &res, 2, 1, fp );     byteswritten += 2; // bfReserved1
		fwrite( &res, 2, 1, fp );     byteswritten += 2; // bfReserved2
		fwrite( &pixoff, 4, 1, fp );  byteswritten += 4; // bfOffBits

		unsigned long biSize = 40, compress = 0, size = 0;
		long pixels = 0;
		unsigned short planes = 1;
		fwrite( &biSize, 4, 1, fp );  byteswritten += 4; // biSize
		fwrite( &width, 4, 1, fp );   byteswritten += 4; // biWidth
		fwrite( &height, 4, 1, fp );  byteswritten += 4; // biHeight
		fwrite( &planes, 2, 1, fp );  byteswritten += 2; // biPlanes
		fwrite( &bits, 2, 1, fp );    byteswritten += 2; // biBitCount
		fwrite( &compress, 4, 1, fp ); byteswritten += 4; // biCompression
		fwrite( &size, 4, 1, fp );    byteswritten += 4; // biSizeImage
		fwrite( &pixels, 4, 1, fp );  byteswritten += 4; // biXPelsPerMeter
		fwrite( &pixels, 4, 1, fp );  byteswritten += 4; // biYPelsPerMeter
		fwrite( &cmap, 4, 1, fp );    byteswritten += 4; // biClrUsed
		fwrite( &cmap, 4, 1, fp );    byteswritten += 4; // biClrImportant

		unsigned long widthDW = ( ( ( width * 24 ) + 31 ) / 32 * 4 );
		long row, row_size = width * 3;
		for ( row = 0; row < height; row++ )
		{
			unsigned char* buf = img + row * row_size;

			// write a row
			int col;
			for ( col = 0; col < row_size; col += 3 )
			{
				putc( buf[col + 2], fp );
				putc( buf[col + 1], fp );
				putc( buf[col], fp );
			}
			byteswritten += row_size;

			unsigned long count;
			for ( count = row_size; count < widthDW; count++ )
			{
				putc( 0, fp ); // dummy
				byteswritten++;
			}
		}

		fclose( fp );
	}

	free( img );
}
#endif


#include "timer.h"

Timer g_chasemouse_timer;

void XYWnd::ChaseMouse(){
	float multiplier = g_chasemouse_timer.elapsed_msec() / 10.0f;
	Scroll( float_to_integer( multiplier * m_chasemouse_delta_x ), float_to_integer( multiplier * -m_chasemouse_delta_y ) );

	//globalOutputStream() << "chasemouse: multiplier=" << multiplier << " x=" << m_chasemouse_delta_x << " y=" << m_chasemouse_delta_y << '\n';

	XY_MouseMoved( m_chasemouse_current_x, m_chasemouse_current_y, getButtonState() );
	g_chasemouse_timer.start();
}

gboolean xywnd_chasemouse( gpointer data ){
	reinterpret_cast<XYWnd*>( data )->ChaseMouse();
	return TRUE;
}

bool XYWnd::chaseMouseMotion( const int x, const int y ){
	m_chasemouse_delta_x = 0;
	m_chasemouse_delta_y = 0;

	if ( g_xywindow_globals_private.m_bChaseMouse && getButtonState() == RAD_LBUTTON ) {
		const int epsilon = 16;

		if ( x < epsilon ) {
			m_chasemouse_delta_x = std::max( x, 0 ) - epsilon;
		}
		else if ( ( x - m_nWidth ) > -epsilon ) {
			m_chasemouse_delta_x = std::min( ( x - m_nWidth ), 0 ) + epsilon;
		}

		if ( y < epsilon ) {
			m_chasemouse_delta_y = std::max( y, 0 ) - epsilon;
		}
		else if ( ( y - m_nHeight ) > -epsilon ) {
			m_chasemouse_delta_y = std::min( ( y - m_nHeight ), 0 ) + epsilon;
		}

		if ( m_chasemouse_delta_y != 0 || m_chasemouse_delta_x != 0 ) {
			//globalOutputStream() << "chasemouse motion: x=" << x << " y=" << y << "... ";
			m_chasemouse_current_x = x;
			m_chasemouse_current_y = y;
			if ( m_chasemouse_handler == 0 ) {
				//globalOutputStream() << "chasemouse timer start... ";
				g_chasemouse_timer.start();
				m_chasemouse_handler = g_idle_add( xywnd_chasemouse, this );
			}
			return true;
		}
		else
		{
			if ( m_chasemouse_handler != 0 ) {
				//globalOutputStream() << "chasemouse cancel\n";
				g_source_remove( m_chasemouse_handler );
				m_chasemouse_handler = 0;
			}
		}
	}
	else
	{
		if ( m_chasemouse_handler != 0 ) {
			//globalOutputStream() << "chasemouse cancel\n";
			g_source_remove( m_chasemouse_handler );
			m_chasemouse_handler = 0;
		}
	}
	return false;
}

// =============================================================================
// XYWnd class
Shader* XYWnd::m_state_selected = 0;

//! todo get rid of this completely; is needed for smooth navigation in camera (swapbuffers in overlay update for camera icon takes odd time in certain environments)
#if (defined _M_IX86 || defined __i386__)
#define OVERLAY_GL_FRONT_DRAW_HACK
#endif
bool XYWnd::overlayStart(){
	if ( GTK_WIDGET_VISIBLE( m_gl_widget ) ) {
		if ( glwidget_make_current( m_gl_widget ) != FALSE ) {
			if ( Map_Valid( g_map ) && ScreenUpdates_Enabled() ) {
				GlobalOpenGL_debugAssertNoErrors();
#ifdef OVERLAY_GL_FRONT_DRAW_HACK
				glDrawBuffer( GL_FRONT );
#endif
				fbo_get()->blit();
				return true;
			}
		}
	}
	return false;
}
void XYWnd::overlayFinish(){
#ifdef OVERLAY_GL_FRONT_DRAW_HACK
	glDrawBuffer( GL_BACK );
#endif
	GlobalOpenGL_debugAssertNoErrors();
#ifdef OVERLAY_GL_FRONT_DRAW_HACK
	glwidget_make_current( m_gl_widget );
#else
	glwidget_swap_buffers( m_gl_widget );
#endif
}
void XYWnd::overlayUpdate(){
	m_deferredOverlayDraw.queueDraw();
}
void XYWnd::overlayDraw(){
	glViewport( 0, 0, m_nWidth, m_nHeight );

	glDisable( GL_LINE_STIPPLE );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_LIGHTING );
	glDisable( GL_COLOR_MATERIAL );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_TEXTURE_1D );

//	glDisable( GL_BLEND );
	glLineWidth( 1 );

	if ( g_xywindow_globals_private.show_outline && Active() ) {
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glOrtho( 0, m_nWidth, 0, m_nHeight, 0, 1 );

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();

		// four view mode doesn't colorize
		glColor3fv( vector3_to_array( ( g_pParentWnd->CurrentStyle() == MainFrame::eSplit )? g_xywindow_globals.color_viewname
																							: m_viewType == YZ? g_xywindow_globals.AxisColorX
																							: m_viewType == XZ? g_xywindow_globals.AxisColorY
																							: g_xywindow_globals.AxisColorZ ) );
		glBegin( GL_LINE_LOOP );
		glVertex2f( 0.5, 0.5 );
		glVertex2f( m_nWidth - 0.5, 0.5 );
		glVertex2f( m_nWidth - 0.5, m_nHeight - 0.5 );
		glVertex2f( 0.5, m_nHeight - 0.5 );
		glEnd();
	}

	{
		NDIM1NDIM2( m_viewType )

		glMatrixMode( GL_PROJECTION );
		glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();
		glScalef( m_fScale, m_fScale, 1 );
		glTranslatef( -m_vOrigin[nDim1], -m_vOrigin[nDim2], 0 );
		DrawCameraIcon( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ), Camera_getAngles( *g_pParentWnd->GetCamWnd() ) );
	}

	if ( g_bCrossHairs ) {
		glMatrixMode( GL_PROJECTION );
		glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

		glMatrixMode( GL_MODELVIEW );
		glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );

		NDIM1NDIM2( m_viewType )
		Vector3 v( g_vector3_identity );
		glColor4f( 0.2f, 0.9f, 0.2f, 0.8f );
		glBegin( GL_LINES );
		for( int i = 0, dim1 = nDim1, dim2 = nDim2; i < 2; ++i, std::swap( dim1, dim2 ) ){
			v[dim1] = m_mousePosition[dim1];
			v[dim2] = 2.0f * -GetMaxGridCoord();
			glVertex3fv( vector3_to_array( v ) );
			v[dim2] = 2.0f * GetMaxGridCoord();
			glVertex3fv( vector3_to_array( v ) );
		}
		glEnd();
	}
}

void xy_update_xor_rectangle( XYWnd& self, rect_t area ){
	if ( self.overlayStart() ) {
		self.overlayDraw();
		self.m_XORRectangle.set( area, self.Width(), self.Height() );
		self.overlayFinish();
	}
}

void xy_update_overlay( XYWnd& self ){
	if ( self.overlayStart() ) {
		self.overlayDraw();
		self.overlayFinish();
	}
}

gboolean xywnd_button_press( GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		gtk_widget_grab_focus( xywnd->GetWidget() );

		if( !xywnd->Active() ){
			g_pParentWnd->SetActiveXY( xywnd );
		}

		xywnd->ButtonState_onMouseDown( buttons_for_event_button( event ) );

		xywnd->onMouseDown( WindowVector( event->x, event->y ), button_for_button( event->button ), modifiers_for_state( event->state ) );
	}
	return FALSE;
}

gboolean xywnd_button_release( GtkWidget* widget, GdkEventButton* event, XYWnd* xywnd ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		xywnd->XY_MouseUp( static_cast<int>( event->x ), static_cast<int>( event->y ), buttons_for_event_button( event ) );

		xywnd->ButtonState_onMouseUp( buttons_for_event_button( event ) );

		xywnd->chaseMouseMotion( static_cast<int>( event->x ), static_cast<int>( event->y ) ); /* stop chaseMouseMotion this way */
	}
	return FALSE;
}

gboolean xywnd_focus_in( GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd ){
	if ( event->type == GDK_FOCUS_CHANGE ) {
		if ( event->in ) {
			if( !xywnd->Active() ){
				g_pParentWnd->SetActiveXY( xywnd );
			}
		}
	}
	return FALSE;
}

void xywnd_motion( gdouble x, gdouble y, guint state, void* data ){
	if ( reinterpret_cast<XYWnd*>( data )->chaseMouseMotion( static_cast<int>( x ), static_cast<int>( y ) ) ) {
		return;
	}
	reinterpret_cast<XYWnd*>( data )->XY_MouseMoved( static_cast<int>( x ), static_cast<int>( y ), buttons_for_state( state ) );
}

gboolean xywnd_wheel_scroll( GtkWidget* widget, GdkEventScroll* event, XYWnd* xywnd ){
	gtk_widget_grab_focus( xywnd->GetWidget() );
	GtkWindow* window = xywnd->m_parent != 0 ? xywnd->m_parent : MainFrame_getWindow();
	if( !gtk_window_is_active( window ) )
		gtk_window_present( window );

	if( !xywnd->Active() ){
		g_pParentWnd->SetActiveXY( xywnd );
	}
	if ( event->direction == GDK_SCROLL_UP ) {
		xywnd->ZoomInWithMouse( (int)event->x, (int)event->y );
	}
	else if ( event->direction == GDK_SCROLL_DOWN ) {
		xywnd->ZoomOut();
	}
	return FALSE;
}

gboolean xywnd_size_allocate( GtkWidget* widget, GtkAllocation* allocation, XYWnd* xywnd ){
#if NV_DRIVER_GAMMA_BUG
	xywnd->fbo_get()->reset( allocation->width, allocation->height, g_xywindow_globals_private.m_MSAA, true );
#else
	xywnd->fbo_get()->reset( allocation->width, allocation->height, g_xywindow_globals_private.m_MSAA, false );
#endif
	xywnd->m_nWidth = allocation->width;
	xywnd->m_nHeight = allocation->height;
	xywnd->updateProjection();
	xywnd->m_window_observer->onSizeChanged( xywnd->Width(), xywnd->Height() );
	return FALSE;
}

gboolean xywnd_expose( GtkWidget* widget, GdkEventExpose* event, XYWnd* xywnd ){
	if ( glwidget_make_current( xywnd->GetWidget() ) != FALSE ) {
		if ( Map_Valid( g_map ) && ScreenUpdates_Enabled() ) {
			GlobalOpenGL_debugAssertNoErrors();
			xywnd->XY_Draw();
			GlobalOpenGL_debugAssertNoErrors();

			//xywnd->m_XORRectangle.set( rect_t() );
		}
		glwidget_swap_buffers( xywnd->GetWidget() );
	}
	return FALSE;
}


void XYWnd_CameraMoved( XYWnd& xywnd ){
	xywnd.overlayUpdate();
}

XYWnd::XYWnd() :
#if NV_DRIVER_GAMMA_BUG
	m_gl_widget( glwidget_new( TRUE ) ),
#else
	m_gl_widget( glwidget_new( FALSE ) ),
#endif
	m_deferredDraw( WidgetQueueDrawCaller( *m_gl_widget ) ),
	m_deferredOverlayDraw( ReferenceCaller<XYWnd, xy_update_overlay>( *this ) ),
	m_deferred_motion( xywnd_motion, this ),
	m_fbo( 0 ),
	m_parent( 0 ),
	m_window_observer( NewWindowObserver() ),
	m_chasemouse_handler( 0 )
{
	m_bActive = false;
	m_buttonstate = 0;

	m_bNewBrushDrag = false;
	m_move_started = false;
	m_zoom_started = false;

	m_nWidth = 0;
	m_nHeight = 0;

	m_vOrigin[0] = 0;
	m_vOrigin[1] = 20;
	m_vOrigin[2] = 46;
	m_fScale = 1;
	m_viewType = XY;

	m_entityCreate = false;

	m_mnuDrop = 0;

	GlobalWindowObservers_add( m_window_observer );
	GlobalWindowObservers_connectWidget( m_gl_widget );

	m_window_observer->setRectangleDrawCallback( ReferenceCaller1<XYWnd, rect_t, xy_update_xor_rectangle>( *this ) );
	m_window_observer->setView( m_view );

	gtk_widget_ref( m_gl_widget );

	gtk_widget_set_events( m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK );
	GTK_WIDGET_SET_FLAGS( m_gl_widget, GTK_CAN_FOCUS );

	m_sizeHandler = g_signal_connect( G_OBJECT( m_gl_widget ), "size_allocate", G_CALLBACK( xywnd_size_allocate ), this );
	m_exposeHandler = g_signal_connect( G_OBJECT( m_gl_widget ), "expose_event", G_CALLBACK( xywnd_expose ), this );

	g_signal_connect( G_OBJECT( m_gl_widget ), "button_press_event", G_CALLBACK( xywnd_button_press ), this );
	g_signal_connect( G_OBJECT( m_gl_widget ), "button_release_event", G_CALLBACK( xywnd_button_release ), this );
	g_signal_connect( G_OBJECT( m_gl_widget ), "focus_in_event", G_CALLBACK( xywnd_focus_in ), this );
	g_signal_connect( G_OBJECT( m_gl_widget ), "motion_notify_event", G_CALLBACK( DeferredMotion::gtk_motion ), &m_deferred_motion );

	g_signal_connect( G_OBJECT( m_gl_widget ), "scroll_event", G_CALLBACK( xywnd_wheel_scroll ), this );

	Map_addValidCallback( g_map, DeferredDrawOnMapValidChangedCaller( m_deferredDraw ) );

	updateProjection();
	updateModelview();

	AddSceneChangeCallback( ReferenceCaller<XYWnd, &XYWnd_Update>( *this ) );
	AddCameraMovedCallback( ReferenceCaller<XYWnd, &XYWnd_CameraMoved>( *this ) );

	PressedButtons_connect( g_pressedButtons, m_gl_widget );

	onMouseDown.connectLast( makeSignalHandler3( MouseDownCaller(), *this ) );
}

XYWnd::~XYWnd(){
	onDestroyed();

	delete m_fbo;

	if ( m_mnuDrop != 0 ) {
		gtk_widget_destroy( GTK_WIDGET( m_mnuDrop ) );
		m_mnuDrop = 0;
	}

	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_sizeHandler );
	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_exposeHandler );

	gtk_widget_unref( m_gl_widget );

	m_window_observer->release();
}

void XYWnd::captureStates(){
	m_state_selected = GlobalShaderCache().capture( "$XY_OVERLAY" );
}

void XYWnd::releaseStates(){
	GlobalShaderCache().release( "$XY_OVERLAY" );
}

const Vector3& XYWnd::GetOrigin() const {
	return m_vOrigin;
}

void XYWnd::SetOrigin( const Vector3& origin ){
	for( std::size_t i = 0; i < 3; ++i )
		m_vOrigin[i] = std::min( GetMaxGridCoord(), std::max( -GetMaxGridCoord(), origin[i] ) );
	updateModelview();
	XYWnd_Update( *this );
}

void XYWnd::Scroll( int x, int y ){
	NDIM1NDIM2( m_viewType )

	m_vOrigin[nDim1] += x / m_fScale;
	m_vOrigin[nDim2] += y / m_fScale;

	SetOrigin( m_vOrigin );
}

FBO* XYWnd::fbo_get(){
	return m_fbo = m_fbo? m_fbo : GlobalOpenGL().support_ARB_framebuffer_object? new FBO : new FBO_fallback;
}

void XYWnd::SetCustomPivotOrigin( int x, int y ) const {
	bool set[3] = { true, true, true };
	set[GetViewType()] = false;
	GlobalSelectionSystem().setCustomTransformOrigin( XY_ToPoint( x, y ), set );
	SceneChangeNotify();
}

unsigned int MoveCamera_buttons(){
	return RAD_CONTROL | RAD_MBUTTON;
}

void XYWnd_PositionCamera( XYWnd* xywnd, int x, int y, CamWnd& camwnd ){
	Vector3 origin = xywnd->XY_ToPoint( x, y, true );
	origin[xywnd->GetViewType()] = Camera_getOrigin( camwnd )[xywnd->GetViewType()];
	Camera_setOrigin( camwnd, origin );
}

unsigned int OrientCamera_buttons(){
	return RAD_MBUTTON;
}

void XYWnd_OrientCamera( XYWnd* xywnd, int x, int y, CamWnd& camwnd ){
	//globalOutputStream() << Camera_getAngles( camwnd ) << "  b4\n";
	const Vector3 point = xywnd->XY_ToPoint( x, y ) - Camera_getOrigin( camwnd );
	const VIEWTYPE viewtype = xywnd->GetViewType();
	NDIM1NDIM2( viewtype )
	const int nAngle = ( viewtype == XY ) ? CAMERA_YAW : CAMERA_PITCH;
	if ( point[nDim2] || point[nDim1] ) {
		Vector3 angles( Camera_getAngles( camwnd ) );
		angles[nAngle] = static_cast<float>( radians_to_degrees( atan2( point[nDim2], point[nDim1] ) ) );
		if( angles[CAMERA_YAW] < 0 )
			angles[CAMERA_YAW] += 360;
		if ( nAngle == CAMERA_PITCH ){
			if( fabs( angles[CAMERA_PITCH] ) > 90 ){
				angles[CAMERA_PITCH] = ( angles[CAMERA_PITCH] > 0 ) ? ( -angles[CAMERA_PITCH] + 180 ) : ( -angles[CAMERA_PITCH] - 180 );
				if( viewtype == YZ ){
					if( angles[CAMERA_YAW] < 180 ){
						angles[CAMERA_YAW] = 360 - angles[CAMERA_YAW];
					}
				}
				else if( angles[CAMERA_YAW] < 90 || angles[CAMERA_YAW] > 270 ){
					angles[CAMERA_YAW] = 180 - angles[CAMERA_YAW];
				}
			}
			else{
				if( viewtype == YZ ){
					if( angles[CAMERA_YAW] > 180 ){
						angles[CAMERA_YAW] = 360 - angles[CAMERA_YAW];
					}
				}
				else if( angles[CAMERA_YAW] > 90 && angles[CAMERA_YAW] < 270 ){
					angles[CAMERA_YAW] = 180 - angles[CAMERA_YAW];
				}
			}
		}
		Camera_setAngles( camwnd, angles );
	}
	//globalOutputStream() << Camera_getAngles( camwnd ) << "\n";
}

unsigned int SetCustomPivotOrigin_buttons(){
	return RAD_MBUTTON | RAD_SHIFT;
}

/*
   ==============
   NewBrushDrag
   ==============
 */
unsigned int NewBrushDrag_buttons(){
	return RAD_LBUTTON;
}

void XYWnd::NewBrushDrag_Begin( int x, int y ){
	m_NewBrushDrag = 0;
	m_nNewBrushPressx = x;
	m_nNewBrushPressy = y;

	m_bNewBrushDrag = true;
}

void XYWnd::NewBrushDrag_End( int x, int y ){
	if ( m_NewBrushDrag != 0 ) {
		GlobalUndoSystem().finish( "brushDragNew" );
	}
}

void XYWnd::NewBrushDrag( int x, int y, bool square, bool cube ){
	Vector3 mins = XY_ToPoint( m_nNewBrushPressx, m_nNewBrushPressy, true );
	Vector3 maxs = XY_ToPoint( x, y, true );

	const int nDim = GetViewType();

	mins[nDim] = float_snapped( Select_getWorkZone().d_work_min[nDim], GetSnapGridSize() );
	maxs[nDim] = float_snapped( Select_getWorkZone().d_work_max[nDim], GetSnapGridSize() );

	if ( maxs[nDim] <= mins[nDim] ) {
		maxs[nDim] = mins[nDim] + GetGridSize();
	}

	if( square || cube ){
		NDIM1NDIM2( nDim )
		const float squaresize = std::max( fabs( maxs[nDim1] - mins[nDim1] ), fabs( maxs[nDim2] - mins[nDim2] ) );
		for( auto i : { nDim1, nDim2 } )
			maxs[i] = mins[i] + std::copysign( squaresize, maxs[i] - mins[i] );
		if( cube ){
			maxs[nDim] = mins[nDim] + squaresize;
		}
	}

	for ( int i = 0 ; i < 3 ; i++ )
	{
		if ( mins[i] == maxs[i] )
			return; // don't create a degenerate brush
		if ( mins[i] > maxs[i] )
			std::swap( mins[i], maxs[i] );
	}

	if ( m_NewBrushDrag == 0 )
		GlobalUndoSystem().start();

	Scene_BrushResize_Cuboid( m_NewBrushDrag, aabb_for_minmax( mins, maxs ) );
}

int g_entityCreationOffset = 0;

void entitycreate_activated( GtkMenuItem* item, gpointer user_data ){
	const char* entity_name = gtk_label_get_text( GTK_LABEL( GTK_BIN( item )->child ) );
	if( g_bCamEntityMenu ){
		const Vector3 viewvector = -Camera_getViewVector( *g_pParentWnd->GetCamWnd() );
		const float offset_for_multiple = std::max( GetSnapGridSize(), 8.f ) * g_entityCreationOffset;
		Vector3 point = viewvector * ( 64.f + offset_for_multiple ) + Camera_getOrigin( *g_pParentWnd->GetCamWnd() );
		vector3_snap( point, GetSnapGridSize() );
		Entity_createFromSelection( entity_name, point );
	}
	else{
		g_pParentWnd->ActiveXY()->OnEntityCreate( entity_name );
	}
	++g_entityCreationOffset;
}

gboolean entitycreate_rightClicked( GtkWidget* widget, GdkEvent* event, gpointer user_data ) {
	/* convert entities */
	if ( event->button.button == 3 ) {
		Scene_EntitySetClassname_Selected( gtk_label_get_text( GTK_LABEL( GTK_BIN( widget )->child ) ) );
		if( ( event->button.state & GDK_CONTROL_MASK ) == 0 ){
			gtk_menu_popdown( XYWnd::m_mnuDrop );
		}
		return TRUE;
	}
	/* create entities, don't close menu */
	else if ( event->button.button == 1 && ( ( event->button.state & GDK_CONTROL_MASK ) != 0 || gtk_menu_get_tearoff_state( XYWnd::m_mnuDrop ) == TRUE ) ) {
		entitycreate_activated( GTK_MENU_ITEM( widget ), 0 );
		return TRUE;
	}
	return FALSE;
}

/* This handles unwanted rightclick release, that can occur with low res display, while activating menu from camera (=activate top menu entry) */
gboolean entitycreate_rightUnClicked( GtkWidget* widget, GdkEvent* event, gpointer user_data ) {
	if ( event->button.button == 3 ) {
		return TRUE;
	}
	else if ( event->button.button == 1 && ( ( event->button.state & GDK_CONTROL_MASK ) != 0 || gtk_menu_get_tearoff_state( XYWnd::m_mnuDrop ) == TRUE ) ) {
		return TRUE;
	}
	return FALSE;
}

void EntityClassMenu_addItem( GtkMenu* menu, const char* name ){
	GtkMenuItem* item = GTK_MENU_ITEM( gtk_menu_item_new_with_label( name ) );
	g_signal_connect( G_OBJECT( item ), "button-press-event", G_CALLBACK( entitycreate_rightClicked ), 0 );
	g_signal_connect( G_OBJECT( item ), "button-release-event", G_CALLBACK( entitycreate_rightUnClicked ), 0 );
	g_signal_connect( G_OBJECT( item ), "activate", G_CALLBACK( entitycreate_activated ), 0 );
	gtk_widget_show( GTK_WIDGET( item ) );
	menu_add_item( menu, item );
}

class EntityClassMenuInserter : public EntityClassVisitor
{
typedef std::pair<GtkMenu*, CopiedString> MenuPair;
typedef std::vector<MenuPair> MenuStack;
MenuStack m_stack;
CopiedString m_previous;
public:
EntityClassMenuInserter( GtkMenu* menu ){
	m_stack.reserve( 2 );
	m_stack.push_back( MenuPair( menu, "" ) );
}
~EntityClassMenuInserter(){
	if ( !string_empty( m_previous.c_str() ) ) {
		addItem( m_previous.c_str(), "" );
	}
}
void visit( EntityClass* e ){
	ASSERT_MESSAGE( !string_empty( e->name() ), "entity-class has no name" );
	if ( !string_empty( m_previous.c_str() ) ) {
		addItem( m_previous.c_str(), e->name() );
	}
	m_previous = e->name();
}
void pushMenu( const CopiedString& name ){
	GtkMenuItem* item = GTK_MENU_ITEM( gtk_menu_item_new_with_label( name.c_str() ) );
	gtk_widget_show( GTK_WIDGET( item ) );
	container_add_widget( GTK_CONTAINER( m_stack.back().first ), GTK_WIDGET( item ) );

	GtkMenu* submenu = GTK_MENU( gtk_menu_new() );
	gtk_menu_item_set_submenu( item, GTK_WIDGET( submenu ) );

	m_stack.push_back( MenuPair( submenu, name ) );
}
void popMenu(){
	m_stack.pop_back();
}
void addItem( const char* name, const char* next ){
	const char* underscore = strchr( name, '_' );

	if ( underscore != 0 && underscore != name ) {
		bool nextEqual = string_equal_n( name, next, ( underscore + 1 ) - name );
		const char* parent = m_stack.back().second.c_str();

		if ( !string_empty( parent )
			 && string_length( parent ) == std::size_t( underscore - name )
			 && string_equal_n( name, parent, underscore - name ) ) { // this is a child
		}
		else if ( nextEqual ) {
			if ( m_stack.size() == 2 ) {
				popMenu();
			}
			pushMenu( CopiedString( StringRange( name, underscore ) ) );
		}
		else if ( m_stack.size() == 2 ) {
			popMenu();
		}
	}
	else if ( m_stack.size() == 2 ) {
		popMenu();
	}

	EntityClassMenu_addItem( m_stack.back().first, name );
}
};

void XYWnd::OnContextMenu(){
	if ( m_mnuDrop == 0 ) { // first time, load it up
		GtkMenu* menu = m_mnuDrop = GTK_MENU( gtk_menu_new() );
//		menu_tearoff( menu );
		g_signal_connect( G_OBJECT( menu_tearoff( menu ) ), "button-release-event", G_CALLBACK( entitycreate_rightUnClicked ), 0 );
		gtk_menu_attach_to_widget( m_mnuDrop, GTK_WIDGET( m_parent != 0 ? m_parent : MainFrame_getWindow() ), NULL );
		gtk_menu_set_title( m_mnuDrop, "" );

		EntityClassMenuInserter inserter( menu );
		GlobalEntityClassManager().forEach( inserter );
	}

	g_entityCreationOffset = 0;
	g_bCamEntityMenu = false;
	gtk_menu_popup( m_mnuDrop, 0, 0, 0, 0, 1, GDK_CURRENT_TIME );
}

FreezePointer g_xywnd_freezePointer;

unsigned int Move_buttons(){
	return RAD_RBUTTON;
}

void XYWnd_moveDelta( int x, int y, unsigned int state, void* data ){
	reinterpret_cast<XYWnd*>( data )->EntityCreate_MouseMove( x, y );
	reinterpret_cast<XYWnd*>( data )->Scroll( -x, y );
}

gboolean XYWnd_Move_focusOut( GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd ){
	xywnd->Move_End();
	return FALSE;
}

void XYWnd::Move_Begin(){
	if ( m_move_started ) {
		Move_End();
	}
	m_move_started = true;
	g_xywnd_freezePointer.freeze_pointer( m_parent != 0 ? m_parent : MainFrame_getWindow(), m_gl_widget, XYWnd_moveDelta, this );
	m_move_focusOut = g_signal_connect( G_OBJECT( m_gl_widget ), "focus_out_event", G_CALLBACK( XYWnd_Move_focusOut ), this );
}

void XYWnd::Move_End(){
	m_move_started = false;
	g_xywnd_freezePointer.unfreeze_pointer( m_parent != 0 ? m_parent : MainFrame_getWindow(), false );
	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_move_focusOut );
}

unsigned int Zoom_buttons(){
	return RAD_RBUTTON | RAD_ALT;
}

int g_dragZoom = 0;
int g_zoom2x = 0;
int g_zoom2y = 0;

void XYWnd_zoomDelta( int x, int y, unsigned int state, void* data ){
	if ( y != 0 ) {
		g_dragZoom += y;
		const int threshold = 16;
		while ( abs( g_dragZoom ) > threshold )
		{
			if ( g_dragZoom > 0 ) {
				reinterpret_cast<XYWnd*>( data )->ZoomOut();
				g_dragZoom -= threshold;
			}
			else
			{
				reinterpret_cast<XYWnd*>( data )->ZoomInWithMouse( g_zoom2x, g_zoom2y );
				g_dragZoom += threshold;
			}
		}
	}
}

gboolean XYWnd_Zoom_focusOut( GtkWidget* widget, GdkEventFocus* event, XYWnd* xywnd ){
	xywnd->Zoom_End();
	return FALSE;
}

void XYWnd::Zoom_Begin( int x, int y ){
	if ( m_zoom_started ) {
		Zoom_End();
	}
	m_zoom_started = true;
	g_dragZoom = 0;
	g_zoom2x = x;
	g_zoom2y = y;
	g_xywnd_freezePointer.freeze_pointer( m_parent != 0 ? m_parent : MainFrame_getWindow(), m_gl_widget, XYWnd_zoomDelta, this );
	m_zoom_focusOut = g_signal_connect( G_OBJECT( m_gl_widget ), "focus_out_event", G_CALLBACK( XYWnd_Zoom_focusOut ), this );
}

void XYWnd::Zoom_End(){
	m_zoom_started = false;
	g_xywnd_freezePointer.unfreeze_pointer( m_parent != 0 ? m_parent : MainFrame_getWindow(), false );
	g_signal_handler_disconnect( G_OBJECT( m_gl_widget ), m_zoom_focusOut );
}

void XYWnd::SetViewType( VIEWTYPE viewType ){
	m_viewType = viewType;
	updateModelview();

	if ( m_parent != 0 ) {
		gtk_window_set_title( m_parent, ViewType_getTitle( m_viewType ) );
	}
}


bool isClipperMode(){
	return GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip;
}

void XYWnd::mouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ){
	XY_MouseDown( static_cast<int>( position.x() ), static_cast<int>( position.y() ), buttons_for_button_and_modifiers( button, modifiers ) );
}
void XYWnd::XY_MouseDown( int x, int y, unsigned int buttons ){
	if ( buttons == Move_buttons() ) {
		Move_Begin();
		EntityCreate_MouseDown( x, y );
	}
	else if ( buttons == Zoom_buttons() ) {
		Zoom_Begin( x, y );
	}
	else if ( buttons == NewBrushDrag_buttons() && GlobalSelectionSystem().countSelected() == 0 && !isClipperMode() ) {
		NewBrushDrag_Begin( x, y );
	}
	// control mbutton = move camera
	else if ( buttons == MoveCamera_buttons() ) {
		XYWnd_PositionCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}
	// mbutton = angle camera
	else if ( buttons == OrientCamera_buttons() ) {
		XYWnd_OrientCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}
	else if ( buttons == SetCustomPivotOrigin_buttons() ) {
		SetCustomPivotOrigin( x, y );
	}
	else
	{
		m_window_observer->onMouseDown( WindowVector( x, y ), button_for_flags( buttons ), modifiers_for_flags( buttons ) );
	}
}

void XYWnd::XY_MouseUp( int x, int y, unsigned int buttons ){
	if ( m_move_started ) {
		Move_End();
		EntityCreate_MouseUp( x, y );
	}
	else if ( m_zoom_started ) {
		Zoom_End();
	}
	else if ( m_bNewBrushDrag ) {
		m_bNewBrushDrag = false;
		NewBrushDrag_End( x, y );
		if ( m_NewBrushDrag == 0 ) {
			//L button w/o created brush = tunnel selection
			m_window_observer->onMouseUp( WindowVector( x, y ), button_for_flags( buttons ), modifiers_for_flags( buttons ) );
		}
	}
	else
	{
		m_window_observer->onMouseUp( WindowVector( x, y ), button_for_flags( buttons ), modifiers_for_flags( buttons ) );
	}
}

void XYWnd::XY_MouseMoved( int x, int y, unsigned int buttons ){
	m_mousePosition = XY_ToPoint( x, y, true );

	// rbutton = drag xy origin
	if ( m_move_started ) {
	}
	// zoom in/out
	else if ( m_zoom_started ) {
	}

	// lbutton without selection = drag new brush
	else if ( m_bNewBrushDrag ) {
		NewBrushDrag( x, y, buttons & RAD_SHIFT, buttons & RAD_CONTROL );
	}

	// control mbutton = move camera
	else if ( buttons == MoveCamera_buttons() ) {
		XYWnd_PositionCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}

	// mbutton = angle camera
	else if ( buttons == OrientCamera_buttons() ) {
		XYWnd_OrientCamera( this, x, y, *g_pParentWnd->GetCamWnd() );
	}

	else if ( buttons == SetCustomPivotOrigin_buttons() ) {
		SetCustomPivotOrigin( x, y );
	}

	else
	{
		m_window_observer->onMouseMotion( WindowVector( x, y ), modifiers_for_flags( buttons ) );

		{
			StringOutputStream status( 64 );
			status << "x:: " << FloatFormat( m_mousePosition[0], 6, 1 )
				<< "  y:: " << FloatFormat( m_mousePosition[1], 6, 1 )
				<< "  z:: " << FloatFormat( m_mousePosition[2], 6, 1 );
			g_pParentWnd->SetStatusText( c_status_position, status.c_str() );
		}

		if ( g_bCrossHairs && button_for_flags( buttons ) == c_buttonInvalid ) { // don't update with a button pressed, observer calls update itself
//			XYWnd_Update( *this );
			overlayUpdate();
		}
	}
}

void XYWnd::EntityCreate_MouseDown( int x, int y ){
	m_entityCreate = true;
	m_entityCreate_x = x;
	m_entityCreate_y = y;
}

void XYWnd::EntityCreate_MouseMove( int x, int y ){
	if ( m_entityCreate && ( m_entityCreate_x != x || m_entityCreate_y != y ) ) {
		m_entityCreate = false;
	}
}

void XYWnd::EntityCreate_MouseUp( int x, int y ){
	if ( m_entityCreate ) {
		m_entityCreate = false;
		OnContextMenu();
	}
}

inline float screen_normalised( int pos, unsigned int size ){
	return ( ( 2.0f * pos ) / size ) - 1.0f;
}

inline float normalised_to_world( float normalised, float world_origin, float normalised2world_scale ){
	return world_origin + normalised * normalised2world_scale;
}


Vector3 XYWnd::XY_ToPoint( int x, int y, bool snap /* = false */ ) const {
	Vector3 point( g_vector3_identity );
	const float normalised2world_scale_x = m_nWidth / 2 / m_fScale;
	const float normalised2world_scale_y = m_nHeight / 2 / m_fScale;
	NDIM1NDIM2( m_viewType )
	point[nDim1] = normalised_to_world( screen_normalised( x, m_nWidth ), m_vOrigin[nDim1], normalised2world_scale_x );
	point[nDim2] = normalised_to_world( -screen_normalised( y, m_nHeight ), m_vOrigin[nDim2], normalised2world_scale_y );
	return snap? vector3_snapped( point, GetSnapGridSize() ) : point;
}


void BackgroundImage::render( const VIEWTYPE viewtype ){
	if( viewtype == _viewtype && _tex > 0 ){
//		glPushAttrib( GL_ALL_ATTRIB_BITS ); //bug with intel

		if ( GlobalOpenGL().GL_1_3() ) {
			glActiveTexture( GL_TEXTURE0 );
			glClientActiveTexture( GL_TEXTURE0 );
		}
		glEnable( GL_TEXTURE_2D );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
//		glPolygonMode( GL_FRONT, GL_FILL );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );

		glBindTexture( GL_TEXTURE_2D, _tex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

		glBegin( GL_QUADS );

		glColor4f( 1, 1, 1, _alpha );
		glTexCoord2f( 0, 1 );
		glVertex2f( _xmin, _ymin );

		glTexCoord2f( 1, 1 );
		glVertex2f( _xmax, _ymin );

		glTexCoord2f( 1, 0 );
		glVertex2f( _xmax, _ymax );

		glTexCoord2f( 0, 0 );
		glVertex2f( _xmin, _ymax );

		glEnd();
		glBindTexture( GL_TEXTURE_2D, 0 );

//		glPopAttrib();
	}
}

#include "qe3.h"
#include "os/file.h"
const char* BackgroundImage::background_image_dialog(){
	StringOutputStream buffer( 1024 );

	buffer << g_qeglobals.m_userGamePath.c_str() << "textures/";

	if ( !file_readable( buffer.c_str() ) ) {
		// just go to fsmain
		buffer.clear();
		buffer << g_qeglobals.m_userGamePath.c_str();
	}

	const char *filename = file_dialog( GTK_WIDGET( MainFrame_getWindow() ), true, "Background Image", buffer.c_str() );
	if ( filename != 0 ) {
		// use VFS to get the correct relative path
		const char* relative = path_make_relative( filename, GlobalFileSystem().findRoot( filename ) );
		if ( relative == filename ) {
			globalWarningStream() << "WARNING: could not extract the relative path, using full path instead\n";
		}
		return relative;
	}
	return 0;
}

void BackgroundImage::free_tex(){
	if( _tex > 0 ){
		glDeleteTextures( 1, &_tex );
		_tex = 0;
	}
}

#include "texturelib.h"
void LoadTextureRGBA( qtexture_t* q, unsigned char* pPixels, int nWidth, int nHeight );

void BackgroundImage::set( const VIEWTYPE viewtype ){
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	NDIM1NDIM2( viewtype )
	if( !( bounds.extents[nDim1] > 0 && bounds.extents[nDim2] > 0 ) ){
		gtk_MessageBox( GTK_WIDGET( MainFrame_getWindow() ), "Select some objects to get the bounding box for image.\n",
						"No selection", eMB_OK, eMB_ICONERROR );
	}
	else{
		free_tex();
		const char *filename = background_image_dialog();
		if( filename ){
			StringOutputStream filename_noext( 1024 );
			const char* ext = path_get_extension( filename );
			if( string_empty( ext ) )
				filename_noext << filename;
			else
				filename_noext << StringRange( filename, ext - 1 );

			Image *image = QERApp_LoadImage( 0, filename_noext.c_str() );
			if ( !image ) {
				globalErrorStream() << "Could not load texture " << filename_noext.c_str() << "\n";
			}
			else{
				qtexture_t* qtex = (qtexture_t*)malloc( sizeof( qtexture_t ) ); /* srs hack :E */
				LoadTextureRGBA( qtex, image->getRGBAPixels(), image->getWidth(), image->getHeight() );
				if( qtex->texture_number > 0 ){
					globalOutputStream() << "Loaded background texture " << filename << "\n";
					_tex = qtex->texture_number;
					_viewtype = viewtype;

					_xmin = bounds.origin[nDim1] - bounds.extents[nDim1];
					_ymin = bounds.origin[nDim2] - bounds.extents[nDim2];
					_xmax = bounds.origin[nDim1] + bounds.extents[nDim1];
					_ymax = bounds.origin[nDim2] + bounds.extents[nDim2];
				}
				image->release();
				free( qtex );
			}
		}
	}
}

void WXY_SetBackgroundImage(){
	g_pParentWnd->ActiveXY()->setBackgroundImage();
}

/*
   ============================================================================

   DRAWING

   ============================================================================
 */

/*
   ==============
   XY_DrawGrid
   ==============
 */

void XYWnd::XY_DrawAxis( void ){
	const char g_AxisName[3] = { 'X', 'Y', 'Z' };
	NDIM1NDIM2( m_viewType )
	const float w = ( m_nWidth / 2 / m_fScale );
	const float h = ( m_nHeight / 2 / m_fScale );

	Vector3 colourX = ( m_viewType == YZ ) ? g_xywindow_globals.AxisColorY : g_xywindow_globals.AxisColorX;
	Vector3 colourY = ( m_viewType == XY ) ? g_xywindow_globals.AxisColorY : g_xywindow_globals.AxisColorZ;
#if 0 //gray for nonActive
	if( !Active() ){
		float grayX = vector3_dot( colourX, Vector3( 0.2989, 0.5870, 0.1140 ) );
		float grayY = vector3_dot( colourY, Vector3( 0.2989, 0.5870, 0.1140 ) );
		colourX[0] = colourX[1] = colourX[2] = grayX;
		colourY[0] = colourY[1] = colourY[2] = grayY;
	}
#endif
	// draw two lines with corresponding axis colors to highlight current view
	// horizontal line: nDim1 color
	glLineWidth( 2 );
	glBegin( GL_LINES );
	glColor3fv( vector3_to_array( colourX ) );
	glVertex2f( m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale );
	glVertex2f( m_vOrigin[nDim1] - w + 65 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale );
	glVertex2f( 0, 0 );
	glVertex2f( 32 / m_fScale, 0 );
	glColor3fv( vector3_to_array( colourY ) );
	glVertex2f( m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 45 / m_fScale );
	glVertex2f( m_vOrigin[nDim1] - w + 40 / m_fScale, m_vOrigin[nDim2] + h - 20 / m_fScale );
	glVertex2f( 0, 0 );
	glVertex2f( 0, 32 / m_fScale );
	glEnd();
	glLineWidth( 1 );
	// now print axis symbols
	glColor3fv( vector3_to_array( colourX ) );
	glRasterPos2f( m_vOrigin[nDim1] - w + 55 / m_fScale, m_vOrigin[nDim2] + h - 55 / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim1] );
	glRasterPos2f( 28 / m_fScale, -10 / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim1] );
	glColor3fv( vector3_to_array( colourY ) );
	glRasterPos2f( m_vOrigin[nDim1] - w + 25 / m_fScale, m_vOrigin[nDim2] + h - 30 / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim2] );
	glRasterPos2f( -10 / m_fScale, 28 / m_fScale );
	GlobalOpenGL().drawChar( g_AxisName[nDim2] );
}

void XYWnd::XY_DrawGrid( void ) {
	float x, y;
	char text[32];
	float step, minor_step, stepx, stepy;
	step = minor_step = stepx = stepy = GetGridSize();

	int minor_power = Grid_getPower();
	while ( ( minor_step * m_fScale ) <= 4.0f ) { // make sure minor grid spacing is at least 4 pixels on the screen
		++minor_power;
		minor_step *= 2;
	}

	int power = minor_power;
	while ( ( power % 3 ) != 0 || ( step * m_fScale ) <= 32.0f ) { // make sure major grid spacing is at least 32 pixels on the screen
		++power;
		step = pow( 2.0f, power );
	}

	const int mask = ( 1 << ( power - minor_power ) ) - 1;

	while ( ( stepx * m_fScale ) <= 32.0f ) // text step x must be at least 32
		stepx *= 2;
	while ( ( stepy * m_fScale ) <= 32.0f ) // text step y must be at least 32
		stepy *= 2;

	const float a = ( ( GetSnapGridSize() > 0.0f ) ? 1.0f : 0.3f );

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_TEXTURE_1D );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	glLineWidth( 1 );

	const float w = ( m_nWidth / 2 / m_fScale );
	const float h = ( m_nHeight / 2 / m_fScale );

	NDIM1NDIM2( m_viewType )

	const float xb = step * floor( std::max( m_vOrigin[nDim1] - w, g_region_mins[nDim1] ) / step );
	const float xe = step * ceil( std::min( m_vOrigin[nDim1] + w, g_region_maxs[nDim1] ) / step );
	const float yb = step * floor( std::max( m_vOrigin[nDim2] - h, g_region_mins[nDim2] ) / step );
	const float ye = step * ceil( std::min( m_vOrigin[nDim2] + h, g_region_maxs[nDim2] ) / step );

#define COLORS_DIFFER( a,b ) \
	( ( a )[0] != ( b )[0] || \
	  ( a )[1] != ( b )[1] || \
	  ( a )[2] != ( b )[2] )

	// djbob
	// draw minor blocks
	if ( g_xywindow_globals_private.d_showgrid /*|| a < 1.0f*/ ) {
		if ( a < 1.0f ) {
			glEnable( GL_BLEND );
		}

		if ( COLORS_DIFFER( g_xywindow_globals.color_gridminor, g_xywindow_globals.color_gridback ) ) {
			glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridminor, a ) ) );

			glBegin( GL_LINES );
			int i = 0;
			for ( x = xb ; x < xe ; x += minor_step, ++i ) {
				if ( ( i & mask ) != 0 ) {
					glVertex2f( x, yb );
					glVertex2f( x, ye );
				}
			}
			i = 0;
			for ( y = yb ; y < ye ; y += minor_step, ++i ) {
				if ( ( i & mask ) != 0 ) {
					glVertex2f( xb, y );
					glVertex2f( xe, y );
				}
			}
			glEnd();
		}

		// draw major blocks
		if ( COLORS_DIFFER( g_xywindow_globals.color_gridmajor, g_xywindow_globals.color_gridminor ) ) {
			glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridmajor, a ) ) );

			glBegin( GL_LINES );
			for ( x = xb ; x <= xe ; x += step ) {
				glVertex2f( x, yb );
				glVertex2f( x, ye );
			}
			for ( y = yb ; y <= ye ; y += step ) {
				glVertex2f( xb, y );
				glVertex2f( xe, y );
			}
			glEnd();
		}

		if ( a < 1.0f ) {
			glDisable( GL_BLEND );
		}

		if( g_region_active ){
			const float xb_ = step * floor( std::max( m_vOrigin[nDim1] - w, -GetMaxGridCoord() ) / step );
			const float xe_ = step * ceil( std::min( m_vOrigin[nDim1] + w, GetMaxGridCoord() ) / step );
			const float yb_ = step * floor( std::max( m_vOrigin[nDim2] - h, -GetMaxGridCoord() ) / step );
			const float ye_ = step * ceil( std::min( m_vOrigin[nDim2] + h, GetMaxGridCoord() ) / step );

			glEnable( GL_BLEND );
			// draw minor blocks
			if ( COLORS_DIFFER( g_xywindow_globals.color_gridminor, g_xywindow_globals.color_gridback ) ) {
				glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridminor, .5f ) ) );

				glBegin( GL_LINES );
				int i = 0;
				for ( x = xb_ ; x < xe_ ; x += minor_step, ++i ) {
					if ( ( i & mask ) != 0 ) {
						glVertex2f( x, yb_ );
						glVertex2f( x, ye_ );
					}
				}
				i = 0;
				for ( y = yb_ ; y < ye_ ; y += minor_step, ++i ) {
					if ( ( i & mask ) != 0 ) {
						glVertex2f( xb_, y );
						glVertex2f( xe_, y );
					}
				}
				glEnd();
			}

			// draw major blocks
			if ( COLORS_DIFFER( g_xywindow_globals.color_gridmajor, g_xywindow_globals.color_gridminor ) ) {
				glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridmajor, .5f ) ) );

				glBegin( GL_LINES );
				for ( x = xb_ ; x <= xe_ ; x += step ) {
					glVertex2f( x, yb_ );
					glVertex2f( x, ye_ );
				}
				for ( y = yb_ ; y <= ye_ ; y += step ) {
					glVertex2f( xb_, y );
					glVertex2f( xe_, y );
				}
				glEnd();
			}
			glDisable( GL_BLEND );
		}
	}

	// draw coordinate text if needed
	if ( g_xywindow_globals_private.show_coordinates ) {
		glColor4fv( vector4_to_array( Vector4( g_xywindow_globals.color_gridtext, 1.0f ) ) );
		const float offx = m_vOrigin[nDim2] + h - ( 4 + GlobalOpenGL().m_font->getPixelAscent() ) / m_fScale;
		const float offy = m_vOrigin[nDim1] - w +  4                                            / m_fScale;
		for ( x = xb - fmod( xb, stepx ); x <= xe ; x += stepx ) {
			glRasterPos2f( x, offx );
			sprintf( text, "%g", x );
			GlobalOpenGL().drawString( text );
		}
		for ( y = yb - fmod( yb, stepy ); y <= ye ; y += stepy ) {
			glRasterPos2f( offy, y );
			sprintf( text, "%g", y );
			GlobalOpenGL().drawString( text );
		}

	}

	if ( g_xywindow_globals_private.show_axis ){
		XY_DrawAxis();
	}
	else{
		glColor3fv( vector3_to_array( Active()? g_xywindow_globals.color_viewname : g_xywindow_globals.color_gridtext ) );
		glRasterPos2f( m_vOrigin[nDim1] - w + 35 / m_fScale, m_vOrigin[nDim2] + h - 20 / m_fScale );
		GlobalOpenGL().drawString( ViewType_getTitle( m_viewType ) );
	}

	// show current work zone?
	// the work zone is used to place dropped points and brushes
	if ( g_xywindow_globals_private.show_workzone ) {
		glColor4f( 1.0f, 0.0f, 0.0f, 1.0f );
		glBegin( GL_LINES );
		glVertex2f( xb, Select_getWorkZone().d_work_min[nDim2] );
		glVertex2f( xe, Select_getWorkZone().d_work_min[nDim2] );
		glVertex2f( xb, Select_getWorkZone().d_work_max[nDim2] );
		glVertex2f( xe, Select_getWorkZone().d_work_max[nDim2] );
		glVertex2f( Select_getWorkZone().d_work_min[nDim1], yb );
		glVertex2f( Select_getWorkZone().d_work_min[nDim1], ye );
		glVertex2f( Select_getWorkZone().d_work_max[nDim1], yb );
		glVertex2f( Select_getWorkZone().d_work_max[nDim1], ye );
		glEnd();
	}
}

/*
   ==============
   XY_DrawBlockGrid
   ==============
 */
void XYWnd::XY_DrawBlockGrid(){
	if ( Map_FindWorldspawn( g_map ) == 0 ) {
		return;
	}
	const char *value = Node_getEntity( *Map_GetWorldspawn( g_map ) )->getKeyValue( "_blocksize" );
	if ( strlen( value ) ) {
		sscanf( value, "%i", &g_xywindow_globals_private.blockSize );
	}

	if ( !g_xywindow_globals_private.blockSize || g_xywindow_globals_private.blockSize > 65536 || g_xywindow_globals_private.blockSize < 1024 ) {
		// don't use custom blocksize if it is less than the default, or greater than the maximum world coordinate
		g_xywindow_globals_private.blockSize = 1024;
	}

	float x, y;
	char text[32];

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_TEXTURE_1D );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );

	const float w = ( m_nWidth / 2 / m_fScale );
	const float h = ( m_nHeight / 2 / m_fScale );

	NDIM1NDIM2( m_viewType )

	const float xb = g_xywindow_globals_private.blockSize * floor( std::max( m_vOrigin[nDim1] - w, g_region_mins[nDim1] ) / g_xywindow_globals_private.blockSize );
	const float xe = g_xywindow_globals_private.blockSize * ceil( std::min( m_vOrigin[nDim1] + w, g_region_maxs[nDim1] ) / g_xywindow_globals_private.blockSize );
	const float yb = g_xywindow_globals_private.blockSize * floor( std::max( m_vOrigin[nDim2] - h, g_region_mins[nDim2] ) / g_xywindow_globals_private.blockSize );
	const float ye = g_xywindow_globals_private.blockSize * ceil( std::min( m_vOrigin[nDim2] + h, g_region_maxs[nDim2] ) / g_xywindow_globals_private.blockSize );

	// draw major blocks

	glColor3fv( vector3_to_array( g_xywindow_globals.color_gridblock ) );
	glLineWidth( 2 );

	glBegin( GL_LINES );

	for ( x = xb ; x <= xe ; x += g_xywindow_globals_private.blockSize )
	{
		glVertex2f( x, yb );
		glVertex2f( x, ye );
	}

	if ( m_viewType == XY ) {
		for ( y = yb ; y <= ye ; y += g_xywindow_globals_private.blockSize )
		{
			glVertex2f( xb, y );
			glVertex2f( xe, y );
		}
	}

	glEnd();
	glLineWidth( 1 );

	// draw coordinate text if needed

	if ( m_viewType == XY && m_fScale > .1 ) {
		for ( x = xb ; x < xe ; x += g_xywindow_globals_private.blockSize )
			for ( y = yb ; y < ye ; y += g_xywindow_globals_private.blockSize )
			{
				glRasterPos2f( x + ( g_xywindow_globals_private.blockSize / 2 ), y + ( g_xywindow_globals_private.blockSize / 2 ) );
				sprintf( text, "%i,%i",(int)floor( x / g_xywindow_globals_private.blockSize ), (int)floor( y / g_xywindow_globals_private.blockSize ) );
				GlobalOpenGL().drawString( text );
			}
	}

	glColor4f( 0, 0, 0, 0 );
}

void XYWnd::DrawCameraIcon( const Vector3& origin, const Vector3& angles ){
//	globalOutputStream() << "pitch " << angles[CAMERA_PITCH] << "   yaw " << angles[CAMERA_YAW] << "\n";
	const float fov = 48 / m_fScale;
	const float box = 16 / m_fScale;

	NDIM1NDIM2( m_viewType )
	const float x = origin[nDim1];
	const float y = origin[nDim2];
	const double a = ( m_viewType == XY )?
						degrees_to_radians( angles[CAMERA_YAW] )
						: ( m_viewType == YZ )?
						degrees_to_radians( ( angles[CAMERA_YAW] > 180 ) ? ( 180.0f - angles[CAMERA_PITCH] ) : angles[CAMERA_PITCH] )
						: degrees_to_radians( ( angles[CAMERA_YAW] < 270 && angles[CAMERA_YAW] > 90 ) ? ( 180.0f - angles[CAMERA_PITCH] ) : angles[CAMERA_PITCH] );

	glColor3f( 0.0, 0.0, 1.0 );
	glBegin( GL_LINE_STRIP );
	glVertex3f( x - box,y,0 );
	glVertex3f( x,y + ( box / 2 ),0 );
	glVertex3f( x + box,y,0 );
	glVertex3f( x,y - ( box / 2 ),0 );
	glVertex3f( x - box,y,0 );
	glVertex3f( x + box,y,0 );
	glEnd();

	glBegin( GL_LINE_STRIP );
	glVertex3f( x + static_cast<float>( fov * cos( a + c_pi / 4 ) ), y + static_cast<float>( fov * sin( a + c_pi / 4 ) ), 0 );
	glVertex3f( x, y, 0 );
	glVertex3f( x + static_cast<float>( fov * cos( a - c_pi / 4 ) ), y + static_cast<float>( fov * sin( a - c_pi / 4 ) ), 0 );
	glEnd();

}


void XYWnd::PaintSizeInfo( const int nDim1, const int nDim2 ){
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	if ( bounds.extents == g_vector3_identity ) {
		return;
	}

	const Vector3 min = bounds.origin - bounds.extents;
	const Vector3 max = bounds.origin + bounds.extents;
	const Vector3 mid = bounds.origin;
	const Vector3 size = bounds.extents * 2;

	const char* dimStrings[] = {"x:", "y:", "z:"};

	glColor3fv( vector3_to_array( g_xywindow_globals.color_selbrushes * .65f ) );

	StringOutputStream dimensions( 16 );

	Vector3 v( g_vector3_identity );

	glBegin( GL_LINE_STRIP );
	v[nDim1] = min[nDim1];
	v[nDim2] = min[nDim2] - 6.f / m_fScale;
	glVertex3fv( vector3_to_array( v ) );
	v[nDim2] = min[nDim2] - 10.f / m_fScale;
	glVertex3fv( vector3_to_array( v ) );
	v[nDim1] = max[nDim1];
	glVertex3fv( vector3_to_array( v ) );
	v[nDim2] = min[nDim2] - 6.f / m_fScale;
	glVertex3fv( vector3_to_array( v ) );
	glEnd();

	glBegin( GL_LINE_STRIP );
	v[nDim2] = max[nDim2];
	v[nDim1] = max[nDim1] + 6.f / m_fScale;
	glVertex3fv( vector3_to_array( v ) );
	v[nDim1] = max[nDim1] + 10.f / m_fScale;
	glVertex3fv( vector3_to_array( v ) );
	v[nDim2] = min[nDim2];
	glVertex3fv( vector3_to_array( v ) );
	v[nDim1] = max[nDim1] + 6.f / m_fScale;
	glVertex3fv( vector3_to_array( v ) );
	glEnd();

	v[nDim1] = mid[nDim1];
	v[nDim2] = min[nDim2] - 20.f / m_fScale;
	glRasterPos3fv( vector3_to_array( v ) );
	dimensions << dimStrings[nDim1] << size[nDim1];
	GlobalOpenGL().drawString( dimensions.c_str() );
	dimensions.clear();

	v[nDim1] = max[nDim1] + 16.f / m_fScale;
	v[nDim2] = mid[nDim2];
	glRasterPos3fv( vector3_to_array( v ) );
	dimensions << dimStrings[nDim2] << size[nDim2];
	GlobalOpenGL().drawString( dimensions.c_str() );
	dimensions.clear();

	v[nDim1] = min[nDim1] + 4.f;
	v[nDim2] = max[nDim2] + 8.f / m_fScale;
	glRasterPos3fv( vector3_to_array( v ) );
	dimensions << "(" << dimStrings[nDim1] << min[nDim1] << "  " << dimStrings[nDim2] << max[nDim2] << ")";
	GlobalOpenGL().drawString( dimensions.c_str() );
}

class XYRenderer : public Renderer
{
struct state_type
{
	state_type() :
		m_highlight( 0 ),
		m_state( 0 ){
	}
	unsigned int m_highlight;
	Shader* m_state;
};
public:
XYRenderer( RenderStateFlags globalstate, Shader* selected ) :
	m_globalstate( globalstate ),
	m_state_selected( selected ){
	ASSERT_NOTNULL( selected );
	m_state_stack.push_back( state_type() );
}

void SetState( Shader* state, EStyle style ){
	ASSERT_NOTNULL( state );
	if ( style == eWireframeOnly ) {
		m_state_stack.back().m_state = state;
	}
}
EStyle getStyle() const {
	return eWireframeOnly;
}
void PushState(){
	m_state_stack.push_back( m_state_stack.back() );
}
void PopState(){
	ASSERT_MESSAGE( !m_state_stack.empty(), "popping empty stack" );
	m_state_stack.pop_back();
}
void Highlight( EHighlightMode mode, bool bEnable = true ){
	( bEnable )
	? m_state_stack.back().m_highlight |= mode
	: m_state_stack.back().m_highlight &= ~mode;
}
void addRenderable( const OpenGLRenderable& renderable, const Matrix4& localToWorld ){
	if ( m_state_stack.back().m_highlight & ePrimitive ) {
		m_state_selected->addRenderable( renderable, localToWorld );
	}
	else
	{
		m_state_stack.back().m_state->addRenderable( renderable, localToWorld );
	}
}

void render( const Matrix4& modelview, const Matrix4& projection ){
	GlobalShaderCache().render( m_globalstate, modelview, projection );
}
private:
std::vector<state_type> m_state_stack;
RenderStateFlags m_globalstate;
Shader* m_state_selected;
};

void XYWnd::updateProjection(){
	m_projection[0] = 1.0f / static_cast<float>( m_nWidth / 2 );
	m_projection[5] = 1.0f / static_cast<float>( m_nHeight / 2 );
	m_projection[10] = 1.0f / ( g_MaxWorldCoord * m_fScale );

	m_projection[12] = 0.0f;
	m_projection[13] = 0.0f;
	m_projection[14] = -1.0f;

	m_projection[1] =
		m_projection[2] =
			m_projection[3] =

				m_projection[4] =
					m_projection[6] =
						m_projection[7] =

							m_projection[8] =
								m_projection[9] =
									m_projection[11] = 0.0f;

	m_projection[15] = 1.0f;

	m_view.Construct( m_projection, m_modelview, m_nWidth, m_nHeight );
}

// note: modelview matrix must have a uniform scale, otherwise strange things happen when rendering the rotation manipulator.
void XYWnd::updateModelview(){
	NDIM1NDIM2( m_viewType )

	// translation
	m_modelview[12] = -m_vOrigin[nDim1] * m_fScale;
	m_modelview[13] = -m_vOrigin[nDim2] * m_fScale;
	m_modelview[14] = g_MaxWorldCoord * m_fScale;

	// axis base
	switch ( m_viewType )
	{
	case XY:
		m_modelview[0]  =  m_fScale;
		m_modelview[1]  =  0;
		m_modelview[2]  =  0;

		m_modelview[4]  =  0;
		m_modelview[5]  =  m_fScale;
		m_modelview[6]  =  0;

		m_modelview[8]  =  0;
		m_modelview[9]  =  0;
		m_modelview[10] = -m_fScale;
		break;
	case XZ:
		m_modelview[0]  =  m_fScale;
		m_modelview[1]  =  0;
		m_modelview[2]  =  0;

		m_modelview[4]  =  0;
		m_modelview[5]  =  0;
		m_modelview[6]  =  m_fScale;

		m_modelview[8]  =  0;
		m_modelview[9]  =  m_fScale;
		m_modelview[10] =  0;
		break;
	case YZ:
		m_modelview[0]  =  0;
		m_modelview[1]  =  0;
		m_modelview[2]  = -m_fScale;

		m_modelview[4]  =  m_fScale;
		m_modelview[5]  =  0;
		m_modelview[6]  =  0;

		m_modelview[8]  =  0;
		m_modelview[9]  =  m_fScale;
		m_modelview[10] =  0;
		break;
	}

	m_modelview[3] = m_modelview[7] = m_modelview[11] = 0;
	m_modelview[15] = 1;

	m_view.Construct( m_projection, m_modelview, m_nWidth, m_nHeight );
}

/*
   ==============
   XY_Draw
   ==============
 */

//#define DBG_SCENEDUMP

void XYWnd::XY_Draw(){
//		globalOutputStream() << "XY_Draw()\n";
/*
	int maxSamples;
	glGetIntegerv(GL_MAX_SAMPLES,&maxSamples);
	globalOutputStream() << maxSamples << " GL_MAX_SAMPLES\n";
	int curSamples;
	glGetIntegerv(GL_SAMPLE_BUFFERS,&curSamples);
	globalOutputStream() << curSamples << " GL_SAMPLE_BUFFERS\n";
	glGetIntegerv(GL_SAMPLES,&curSamples);
	globalOutputStream() << curSamples << " GL_SAMPLES\n";
*/
	fbo_get()->start();
	//
	// clear
	//
	glViewport( 0, 0, m_nWidth, m_nHeight );
	glClearColor( g_xywindow_globals.color_gridback[0],
				  g_xywindow_globals.color_gridback[1],
				  g_xywindow_globals.color_gridback[2],0 );

	glClear( GL_COLOR_BUFFER_BIT );

	extern void Renderer_ResetStats();
	Renderer_ResetStats();

	//
	// set up viewpoint
	//

	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glScalef( m_fScale, m_fScale, 1 );
	NDIM1NDIM2( m_viewType )
	glTranslatef( -m_vOrigin[nDim1], -m_vOrigin[nDim2], 0 );

	glDisable( GL_LINE_STIPPLE );
	glLineWidth( 1 );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_LIGHTING );
	glDisable( GL_COLOR_MATERIAL );
	glDisable( GL_DEPTH_TEST );

	m_backgroundImage.render( m_viewType );

	XY_DrawGrid();

	if ( g_xywindow_globals_private.show_blocks ) {
		XY_DrawBlockGrid();
	}

	glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );

	unsigned int globalstate = RENDER_COLOURARRAY | RENDER_COLOURWRITE | RENDER_POLYGONSMOOTH | RENDER_LINESMOOTH;
	if ( !g_xywindow_globals.m_bNoStipple ) {
		globalstate |= RENDER_LINESTIPPLE;
	}

	{
		XYRenderer renderer( globalstate, m_state_selected );

		Scene_Render( renderer, m_view );

		GlobalOpenGL_debugAssertNoErrors();
		renderer.render( m_modelview, m_projection );
		GlobalOpenGL_debugAssertNoErrors();
	}

	glDepthMask( GL_FALSE );

	GlobalOpenGL_debugAssertNoErrors();

	glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );

	GlobalOpenGL_debugAssertNoErrors();
	glDisable( GL_LINE_STIPPLE );
	GlobalOpenGL_debugAssertNoErrors();
	glLineWidth( 1 );
	GlobalOpenGL_debugAssertNoErrors();
	if ( GlobalOpenGL().GL_1_3() ) {
		glActiveTexture( GL_TEXTURE0 );
		glClientActiveTexture( GL_TEXTURE0 );
	}
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GlobalOpenGL_debugAssertNoErrors();
	glDisableClientState( GL_NORMAL_ARRAY );
	GlobalOpenGL_debugAssertNoErrors();
	glDisableClientState( GL_COLOR_ARRAY );
	GlobalOpenGL_debugAssertNoErrors();
	glDisable( GL_TEXTURE_2D );
	GlobalOpenGL_debugAssertNoErrors();
	glDisable( GL_LIGHTING );
	GlobalOpenGL_debugAssertNoErrors();
	glDisable( GL_COLOR_MATERIAL );
	GlobalOpenGL_debugAssertNoErrors();

	GlobalOpenGL_debugAssertNoErrors();


	// size info
	if ( g_xywindow_globals_private.m_bShowSize && GlobalSelectionSystem().countSelected() != 0 ) {
		PaintSizeInfo( nDim1, nDim2 );
	}

	GlobalOpenGL_debugAssertNoErrors();

	{
		// reset modelview
		glLoadIdentity();
		glScalef( m_fScale, m_fScale, 1 );
		glTranslatef( -m_vOrigin[nDim1], -m_vOrigin[nDim2], 0 );

		Feedback_draw2D( m_viewType );
	}

	if( g_camwindow_globals.m_showStats ){
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glOrtho( 0, m_nWidth, 0, m_nHeight, 0, 1 );

		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();

		glColor3fv( vector3_to_array( g_xywindow_globals.color_viewname ) );

		glRasterPos3f( 2.f, GlobalOpenGL().m_font->getPixelDescent() + 1.f, 0.0f );
		extern const char* Renderer_GetStats();
		GlobalOpenGL().drawString( Renderer_GetStats() );

		//globalOutputStream() << m_render_time.elapsed_msec() << "\n";
		StringOutputStream stream;
		stream << " | f2f: " << m_render_time.elapsed_msec();
		GlobalOpenGL().drawString( stream.c_str() );
		m_render_time.start();
	}

	fbo_get()->save();

	overlayDraw(); //outline camera crosshair rectangle

	GlobalOpenGL_debugAssertNoErrors();

	glFinish();
}

void XYWnd_MouseToPoint( XYWnd* xywnd, int x, int y, Vector3& point ){
	point = xywnd->XY_ToPoint( x, y, true );
	const int nDim = xywnd->GetViewType();
	const float fWorkMid = float_mid( Select_getWorkZone().d_work_min[nDim], Select_getWorkZone().d_work_max[nDim] );
	point[nDim] = float_snapped( fWorkMid, GetGridSize() );
}

void XYWnd::OnEntityCreate( const char* item ){
	Vector3 point;
	XYWnd_MouseToPoint( this, m_entityCreate_x, m_entityCreate_y, point );

	const float offset = std::max( 8.f, GetSnapGridSize() ) * g_entityCreationOffset;
	NDIM1NDIM2( m_viewType )
	point += g_vector3_axes[nDim1] * offset;

	Entity_createFromSelection( item, point );
}



inline AABB GetCenterBbox(){
	return ( GlobalSelectionSystem().countSelected() != 0 )?
			GlobalSelectionSystem().getBoundsSelected() :
			AABB( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ), Vector3( 128.f, 128.f, 128.f ) );
}

void XYWnd_Centralize( XYWnd* xywnd ){
	xywnd->SetOrigin( GetCenterBbox().origin );
}

void XY_Centralize(){
	const Vector3 position( GetCenterBbox().origin );
	g_pParentWnd->forEachXYWnd( [&position]( XYWnd* xywnd ){
		xywnd->SetOrigin( position );
	} );
}

void XY_Focus(){
	const AABB bounds( GetCenterBbox() );
	g_pParentWnd->forEachXYWnd( [&bounds]( XYWnd* xywnd ){
		xywnd->FocusOnBounds( bounds );
	} );
}



void XY_SetViewType( VIEWTYPE viewtype ){
	if ( g_pParentWnd->CurrentStyle() != MainFrame::eSplit ) { // do not want this in a split window
		XYWnd* xywnd = g_pParentWnd->ActiveXY();
		xywnd->SetViewType( viewtype );
		XYWnd_Centralize( xywnd );
	}
	else{
		XY_Centralize(); // do something else that the user may want here
	}
}

void XY_Top(){
	XY_SetViewType( XY );
}

void XY_Front(){
	XY_SetViewType( XZ );
}

void XY_Side(){
	XY_SetViewType( YZ );
}

void XY_NextView(){
	XY_SetViewType( static_cast<VIEWTYPE>( ( g_pParentWnd->ActiveXY()->GetViewType() + 2 ) % 3 ) );
}

void XY_Zoom100(){
	g_pParentWnd->forEachXYWnd( []( XYWnd* xywnd ){
		xywnd->SetScale( 1 );
	} );
}

void XY_ZoomIn(){
	g_pParentWnd->ActiveXY()->ZoomIn();
}

void XY_ZoomOut(){
	g_pParentWnd->ActiveXY()->ZoomOut();
}



ToggleShown g_xy_top_shown( true );

void XY_Top_Shown_Construct( GtkWindow* parent ){
	g_xy_top_shown.connect( GTK_WIDGET( parent ) );
}

ToggleShown g_yz_side_shown( false );

void YZ_Side_Shown_Construct( GtkWindow* parent ){
	g_yz_side_shown.connect( GTK_WIDGET( parent ) );
}

ToggleShown g_xz_front_shown( false );

void XZ_Front_Shown_Construct( GtkWindow* parent ){
	g_xz_front_shown.connect( GTK_WIDGET( parent ) );
}


class EntityClassMenu : public ModuleObserver
{
std::size_t m_unrealised;
public:
EntityClassMenu() : m_unrealised( 1 ){
}
void realise(){
	if ( --m_unrealised == 0 ) {
	}
}
void unrealise(){
	if ( ++m_unrealised == 1 ) {
		if ( XYWnd::m_mnuDrop != 0 ) {
			gtk_widget_destroy( GTK_WIDGET( XYWnd::m_mnuDrop ) );
			XYWnd::m_mnuDrop = 0;
		}
	}
}
};

EntityClassMenu g_EntityClassMenu;




void ShowNamesExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowNames() );
}
typedef FreeCaller1<const BoolImportCallback&, ShowNamesExport> ShowNamesExportCaller;
ShowNamesExportCaller g_show_names_caller;
ToggleItem g_show_names( g_show_names_caller );
void ShowNamesToggle(){
	GlobalEntityCreator().setShowNames( !GlobalEntityCreator().getShowNames() );
	g_show_names.update();
	UpdateAllWindows();
}

void ShowBboxesExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowBboxes() );
}
typedef FreeCaller1<const BoolImportCallback&, ShowBboxesExport> ShowBboxesExportCaller;
ShowBboxesExportCaller g_show_bboxes_caller;
ToggleItem g_show_bboxes( g_show_bboxes_caller );
void ShowBboxesToggle(){
	GlobalEntityCreator().setShowBboxes( !GlobalEntityCreator().getShowBboxes() );
	g_show_bboxes.update();
	UpdateAllWindows();
}

void ShowConnectionsExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowConnections() );
}
typedef FreeCaller1<const BoolImportCallback&, ShowConnectionsExport> ShowConnectionsExportCaller;
ShowConnectionsExportCaller g_show_connections_caller;
ToggleItem g_show_connections( g_show_connections_caller );
void ShowConnectionsToggle(){
	GlobalEntityCreator().setShowConnections( !GlobalEntityCreator().getShowConnections() );
	g_show_connections.update();
	UpdateAllWindows();
}

void ShowAnglesExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getShowAngles() );
}
typedef FreeCaller1<const BoolImportCallback&, ShowAnglesExport> ShowAnglesExportCaller;
ShowAnglesExportCaller g_show_angles_caller;
ToggleItem g_show_angles( g_show_angles_caller );
void ShowAnglesToggle(){
	GlobalEntityCreator().setShowAngles( !GlobalEntityCreator().getShowAngles() );
	g_show_angles.update();
	UpdateAllWindows();
}

BoolExportCaller g_show_blocks_caller( g_xywindow_globals_private.show_blocks );
ToggleItem g_show_blocks( g_show_blocks_caller );
void ShowBlocksToggle(){
	g_xywindow_globals_private.show_blocks ^= 1;
	g_show_blocks.update();
	XY_UpdateAllWindows();
}

BoolExportCaller g_show_coordinates_caller( g_xywindow_globals_private.show_coordinates );
ToggleItem g_show_coordinates( g_show_coordinates_caller );
void ShowCoordinatesToggle(){
	g_xywindow_globals_private.show_coordinates ^= 1;
	g_show_coordinates.update();
	XY_UpdateAllWindows();
}

BoolExportCaller g_show_outline_caller( g_xywindow_globals_private.show_outline );
ToggleItem g_show_outline( g_show_outline_caller );
void ShowOutlineToggle(){
	g_xywindow_globals_private.show_outline ^= 1;
	g_show_outline.update();
	XY_UpdateAllWindows();
}

BoolExportCaller g_show_axes_caller( g_xywindow_globals_private.show_axis );
ToggleItem g_show_axes( g_show_axes_caller );
void ShowAxesToggle(){
	g_xywindow_globals_private.show_axis ^= 1;
	g_show_axes.update();
	XY_UpdateAllWindows();
}


BoolExportCaller g_show_workzone_caller( g_xywindow_globals_private.show_workzone );
ToggleItem g_show_workzone( g_show_workzone_caller );
void ShowWorkzoneToggle(){
	g_xywindow_globals_private.show_workzone ^= 1;
	g_show_workzone.update();
	XY_UpdateAllWindows();
}

/*
void ShowAxesToggle(){
	g_xywindow_globals_private.show_axis ^= 1;
	XY_UpdateAllWindows();
}
typedef FreeCaller<ShowAxesToggle> ShowAxesToggleCaller;
void ShowAxesExport( const BoolImportCallback& importer ){
	importer( g_xywindow_globals_private.show_axis );
}
typedef FreeCaller1<const BoolImportCallback&, ShowAxesExport> ShowAxesExportCaller;

ShowAxesExportCaller g_show_axes_caller;
BoolExportCallback g_show_axes_callback( g_show_axes_caller );
ToggleItem g_show_axes( g_show_axes_callback );
*/

/*
BoolExportCaller g_texdef_movelock_caller( g_brush_texturelock_enabled );
ToggleItem g_texdef_movelock_item( g_texdef_movelock_caller );

void Texdef_ToggleMoveLock(){
	g_brush_texturelock_enabled = !g_brush_texturelock_enabled;
	g_texdef_movelock_item.update();
}
*/

BoolExportCaller g_show_size_caller( g_xywindow_globals_private.m_bShowSize );
ToggleItem g_show_size_item( g_show_size_caller );
void ToggleShowSizeInfo(){
	g_xywindow_globals_private.m_bShowSize = !g_xywindow_globals_private.m_bShowSize;
	g_show_size_item.update();
	XY_UpdateAllWindows();
}

BoolExportCaller g_show_crosshair_caller( g_bCrossHairs );
ToggleItem g_show_crosshair_item( g_show_crosshair_caller );
void ToggleShowCrosshair(){
	g_bCrossHairs ^= 1;
	g_show_crosshair_item.update();
	XY_UpdateAllWindows();
}

BoolExportCaller g_show_grid_caller( g_xywindow_globals_private.d_showgrid );
ToggleItem g_show_grid_item( g_show_grid_caller );
void ToggleShowGrid(){
	g_xywindow_globals_private.d_showgrid = !g_xywindow_globals_private.d_showgrid;
	g_show_grid_item.update();
	XY_UpdateAllWindows();
}

void MSAAImport( int value ){
	g_xywindow_globals_private.m_MSAA = value ? 1 << value : value;
	g_pParentWnd->forEachXYWnd( []( XYWnd* xywnd ){
#if NV_DRIVER_GAMMA_BUG
		xywnd->fbo_get()->reset( xywnd->Width(), xywnd->Height(), g_xywindow_globals_private.m_MSAA, true );
#else
		xywnd->fbo_get()->reset( xywnd->Width(), xywnd->Height(), g_xywindow_globals_private.m_MSAA, false );
#endif
	} );
}
typedef FreeCaller1<int, MSAAImport> MSAAImportCaller;

void MSAAExport( const IntImportCallback& importer ){
	if( g_xywindow_globals_private.m_MSAA <= 0 ){
		importer( 0 );
	}
	else{
		int exponent = 1;
		while( !( ( g_xywindow_globals_private.m_MSAA >> exponent ) & 1 ) ){
			++exponent;
		}
		importer( exponent );
	}
}
typedef FreeCaller1<const IntImportCallback&, MSAAExport> MSAAExportCaller;


void XYShow_registerCommands(){
	GlobalToggles_insert( "ShowSize2d", FreeCaller<ToggleShowSizeInfo>(), ToggleItem::AddCallbackCaller( g_show_size_item ), Accelerator( 'J' ) );
	GlobalToggles_insert( "ToggleCrosshairs", FreeCaller<ToggleShowCrosshair>(), ToggleItem::AddCallbackCaller( g_show_crosshair_item ), Accelerator( 'X', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalToggles_insert( "ToggleGrid", FreeCaller<ToggleShowGrid>(), ToggleItem::AddCallbackCaller( g_show_grid_item ), Accelerator( '0' ) );

	GlobalToggles_insert( "ShowAngles", FreeCaller<ShowAnglesToggle>(), ToggleItem::AddCallbackCaller( g_show_angles ) );
	GlobalToggles_insert( "ShowNames", FreeCaller<ShowNamesToggle>(), ToggleItem::AddCallbackCaller( g_show_names ) );
	GlobalToggles_insert( "ShowBboxes", FreeCaller<ShowBboxesToggle>(), ToggleItem::AddCallbackCaller( g_show_bboxes ) );
	GlobalToggles_insert( "ShowConnections", FreeCaller<ShowConnectionsToggle>(), ToggleItem::AddCallbackCaller( g_show_connections ) );
	GlobalToggles_insert( "ShowBlocks", FreeCaller<ShowBlocksToggle>(), ToggleItem::AddCallbackCaller( g_show_blocks ) );
	GlobalToggles_insert( "ShowCoordinates", FreeCaller<ShowCoordinatesToggle>(), ToggleItem::AddCallbackCaller( g_show_coordinates ) );
	GlobalToggles_insert( "ShowWindowOutline", FreeCaller<ShowOutlineToggle>(), ToggleItem::AddCallbackCaller( g_show_outline ) );
	GlobalToggles_insert( "ShowAxes", FreeCaller<ShowAxesToggle>(), ToggleItem::AddCallbackCaller( g_show_axes ) );
	GlobalToggles_insert( "ShowWorkzone2d", FreeCaller<ShowWorkzoneToggle>(), ToggleItem::AddCallbackCaller( g_show_workzone ) );
}

void XYWnd_registerShortcuts(){
	command_connect_accelerator( "ToggleCrosshairs" );
	command_connect_accelerator( "ShowSize2d" );
}



void Orthographic_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Solid selection boxes ( no stipple )", g_xywindow_globals.m_bNoStipple );
	//page.appendCheckBox( "", "Display size info", g_xywindow_globals_private.m_bShowSize );
	page.appendCheckBox( "", "Chase mouse during drags", g_xywindow_globals_private.m_bChaseMouse );
	page.appendCheckBox( "", "Zoom In to Mouse pointer", g_xywindow_globals.m_bZoomInToPointer );

	if( GlobalOpenGL().support_ARB_framebuffer_object ){
		const char* samples[] = { "0", "2", "4", "8", "16", "32" };

		page.appendCombo(
			"MSAA",
			STRING_ARRAY_RANGE( samples ),
			IntImportCallback( MSAAImportCaller() ),
			IntExportCallback( MSAAExportCaller() )
			);
	}
}
void Orthographic_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Orthographic", "Orthographic View Preferences" ) );
	Orthographic_constructPreferences( page );
}
void Orthographic_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, Orthographic_constructPage>() );
}


#include "preferencesystem.h"
#include "stringio.h"


void XYWindow_Construct(){
	GlobalToggles_insert( "ToggleView", ToggleShown::ToggleCaller( g_xy_top_shown ), ToggleItem::AddCallbackCaller( g_xy_top_shown.m_item ) );
	GlobalToggles_insert( "ToggleSideView", ToggleShown::ToggleCaller( g_yz_side_shown ), ToggleItem::AddCallbackCaller( g_yz_side_shown.m_item ) );
	GlobalToggles_insert( "ToggleFrontView", ToggleShown::ToggleCaller( g_xz_front_shown ), ToggleItem::AddCallbackCaller( g_xz_front_shown.m_item ) );
	GlobalCommands_insert( "NextView", FreeCaller<XY_NextView>(), Accelerator( GDK_Tab, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "ZoomIn", FreeCaller<XY_ZoomIn>(), Accelerator( GDK_Delete ) );
	GlobalCommands_insert( "ZoomOut", FreeCaller<XY_ZoomOut>(), Accelerator( GDK_Insert ) );
	GlobalCommands_insert( "ViewTop", FreeCaller<XY_Top>(), Accelerator( GDK_KP_7 ) );
	GlobalCommands_insert( "ViewFront", FreeCaller<XY_Front>(), Accelerator( GDK_KP_1 ) );
	GlobalCommands_insert( "ViewSide", FreeCaller<XY_Side>(), Accelerator( GDK_KP_3 ) );
	GlobalCommands_insert( "Zoom100", FreeCaller<XY_Zoom100>() );
	GlobalCommands_insert( "CenterXYView", FreeCaller<XY_Centralize>(), Accelerator( GDK_Tab, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "XYFocusOnSelected", FreeCaller<XY_Focus>(), Accelerator( GDK_grave ) );

	GlobalPreferenceSystem().registerPreference( "XYMSAA", IntImportStringCaller( g_xywindow_globals_private.m_MSAA ), IntExportStringCaller( g_xywindow_globals_private.m_MSAA ) );
	GlobalPreferenceSystem().registerPreference( "2DZoomInToPointer", BoolImportStringCaller( g_xywindow_globals.m_bZoomInToPointer ), BoolExportStringCaller( g_xywindow_globals.m_bZoomInToPointer ) );
	GlobalPreferenceSystem().registerPreference( "ChaseMouse", BoolImportStringCaller( g_xywindow_globals_private.m_bChaseMouse ), BoolExportStringCaller( g_xywindow_globals_private.m_bChaseMouse ) );
	GlobalPreferenceSystem().registerPreference( "ShowSize2d", BoolImportStringCaller( g_xywindow_globals_private.m_bShowSize ), BoolExportStringCaller( g_xywindow_globals_private.m_bShowSize ) );
	GlobalPreferenceSystem().registerPreference( "ShowCrosshair", BoolImportStringCaller( g_bCrossHairs ), BoolExportStringCaller( g_bCrossHairs ) );
	GlobalPreferenceSystem().registerPreference( "NoStipple", BoolImportStringCaller( g_xywindow_globals.m_bNoStipple ), BoolExportStringCaller( g_xywindow_globals.m_bNoStipple ) );
	GlobalPreferenceSystem().registerPreference( "SI_ShowCoords", BoolImportStringCaller( g_xywindow_globals_private.show_coordinates ), BoolExportStringCaller( g_xywindow_globals_private.show_coordinates ) );
	GlobalPreferenceSystem().registerPreference( "SI_ShowOutlines", BoolImportStringCaller( g_xywindow_globals_private.show_outline ), BoolExportStringCaller( g_xywindow_globals_private.show_outline ) );
	GlobalPreferenceSystem().registerPreference( "SI_ShowAxis", BoolImportStringCaller( g_xywindow_globals_private.show_axis ), BoolExportStringCaller( g_xywindow_globals_private.show_axis ) );
	GlobalPreferenceSystem().registerPreference( "ShowWorkzone2d", BoolImportStringCaller( g_xywindow_globals_private.show_workzone ), BoolExportStringCaller( g_xywindow_globals_private.show_workzone ) );

	GlobalPreferenceSystem().registerPreference( "SI_AxisColors0", Vector3ImportStringCaller( g_xywindow_globals.AxisColorX ), Vector3ExportStringCaller( g_xywindow_globals.AxisColorX ) );
	GlobalPreferenceSystem().registerPreference( "SI_AxisColors1", Vector3ImportStringCaller( g_xywindow_globals.AxisColorY ), Vector3ExportStringCaller( g_xywindow_globals.AxisColorY ) );
	GlobalPreferenceSystem().registerPreference( "SI_AxisColors2", Vector3ImportStringCaller( g_xywindow_globals.AxisColorZ ), Vector3ExportStringCaller( g_xywindow_globals.AxisColorZ ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors1", Vector3ImportStringCaller( g_xywindow_globals.color_gridback ), Vector3ExportStringCaller( g_xywindow_globals.color_gridback ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors2", Vector3ImportStringCaller( g_xywindow_globals.color_gridminor ), Vector3ExportStringCaller( g_xywindow_globals.color_gridminor ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors3", Vector3ImportStringCaller( g_xywindow_globals.color_gridmajor ), Vector3ExportStringCaller( g_xywindow_globals.color_gridmajor ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors6", Vector3ImportStringCaller( g_xywindow_globals.color_gridblock ), Vector3ExportStringCaller( g_xywindow_globals.color_gridblock ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors7", Vector3ImportStringCaller( g_xywindow_globals.color_gridtext ), Vector3ExportStringCaller( g_xywindow_globals.color_gridtext ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors8", Vector3ImportStringCaller( g_xywindow_globals.color_brushes ), Vector3ExportStringCaller( g_xywindow_globals.color_brushes ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors9", Vector3ImportStringCaller( g_xywindow_globals.color_viewname ), Vector3ExportStringCaller( g_xywindow_globals.color_viewname ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors10", Vector3ImportStringCaller( g_xywindow_globals.color_clipper ), Vector3ExportStringCaller( g_xywindow_globals.color_clipper ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors11", Vector3ImportStringCaller( g_xywindow_globals.color_selbrushes ), Vector3ExportStringCaller( g_xywindow_globals.color_selbrushes ) );


	GlobalPreferenceSystem().registerPreference( "XYVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_xy_top_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_xy_top_shown ) ) );
	GlobalPreferenceSystem().registerPreference( "XZVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_xz_front_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_xz_front_shown ) ) );
	GlobalPreferenceSystem().registerPreference( "YZVIS", makeBoolStringImportCallback( ToggleShownImportBoolCaller( g_yz_side_shown ) ), makeBoolStringExportCallback( ToggleShownExportBoolCaller( g_yz_side_shown ) ) );

	Orthographic_registerPreferencesPage();

	XYWnd::captureStates();
	GlobalEntityClassManager().attach( g_EntityClassMenu );
}

void XYWindow_Destroy(){
	GlobalEntityClassManager().detach( g_EntityClassMenu );
	XYWnd::releaseStates();
}

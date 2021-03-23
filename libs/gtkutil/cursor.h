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

#if !defined( INCLUDED_GTKUTIL_CURSOR_H )
#define INCLUDED_GTKUTIL_CURSOR_H

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "debugging/debugging.h"

#if 0
GdkCursor* create_blank_cursor();
void blank_cursor( GtkWidget* widget );
void default_cursor( GtkWidget* widget );
#endif
void Sys_GetCursorPos( GtkWindow* window, int *x, int *y );
void Sys_SetCursorPos( GtkWindow* window, int x, int y );



class DeferredMotion
{
	guint m_handler;
	typedef void ( *MotionFunction )( gdouble x, gdouble y, guint state, void* data );
	MotionFunction m_function;
	void* m_data;
	gdouble m_x;
	gdouble m_y;
	guint m_state;

	static gboolean deferred( DeferredMotion* self ){
		self->m_handler = 0;
		self->m_function( self->m_x, self->m_y, self->m_state, self->m_data );
		return FALSE;
	}
public:
	DeferredMotion( MotionFunction function, void* data ) : m_handler( 0 ), m_function( function ), m_data( data ){
	}
	void motion( gdouble x, gdouble y, guint state ){
		m_x = x;
		m_y = y;
		m_state = state;
		if ( m_handler == 0 ) {
			m_handler = g_idle_add( (GSourceFunc)deferred, this );
		}
	}
	static gboolean gtk_motion( GtkWidget *widget, GdkEventMotion *event, DeferredMotion* self ){
		self->motion( event->x, event->y, event->state );
		return FALSE;
	}
};

class DeferredMotionDelta
{
	int m_delta_x;
	int m_delta_y;
	unsigned int m_state;
	guint m_motion_handler;
	typedef void ( *MotionDeltaFunction )( int x, int y, unsigned int state, void* data );
	MotionDeltaFunction m_function;
	void* m_data;

	static gboolean deferred_motion( gpointer data ){
		DeferredMotionDelta* self = reinterpret_cast<DeferredMotionDelta*>( data );
		self->m_function(
		    self->m_delta_x,
		    self->m_delta_y,
		    self->m_state,
		    self->m_data
		);
		self->m_motion_handler = 0;
		self->m_delta_x = 0;
		self->m_delta_y = 0;
		return FALSE;
	}
public:
	DeferredMotionDelta( MotionDeltaFunction function, void* data ) : m_delta_x( 0 ), m_delta_y( 0 ), m_motion_handler( 0 ), m_function( function ), m_data( data ){
	}
	void flush(){
		if ( m_motion_handler != 0 ) {
			g_source_remove( m_motion_handler );
			deferred_motion( this );
		}
	}
	void motion_delta( int x, int y, unsigned int state ){
		m_delta_x += x;
		m_delta_y += y;
		m_state = state;
		if ( m_motion_handler == 0 ) {
			m_motion_handler = g_idle_add( deferred_motion, this );
		}
	}
};

class FreezePointer
{
	unsigned int handle_motion;
	int recorded_x, recorded_y, last_x, last_y, center_x, center_y;
	GtkWindow* m_window;
	GtkWidget* m_widget;
	typedef void ( *MotionDeltaFunction )( int x, int y, unsigned int state, void* data );
	MotionDeltaFunction m_function;
	void* m_data;
public:
	FreezePointer() : handle_motion( 0 ), m_function( 0 ), m_data( 0 ){
	}
	static gboolean motion_delta( GtkWidget *widget, GdkEventMotion *event, FreezePointer* self ){
		int current_x, current_y;
		Sys_GetCursorPos( GTK_WINDOW( widget ), &current_x, &current_y );
#define FIX_LINUX_TOUCHPAD 0 /* issues with this: mouse juddering
													in win10 (system is already at normal scaling)
													Leaving the DPI scaling to the application instead of the system half fixes it
													in GNU/Linux
													Mouselook is broken with custom Input Coordinate Tranformation */
#if FIX_LINUX_TOUCHPAD
		const int dx = current_x - self->last_x;
		const int dy = current_y - self->last_y;
		const int ddx = current_x - self->center_x;
		const int ddy = current_y - self->center_y;
		self->last_x = current_x;
		self->last_y = current_y;
		if ( dx != 0 || dy != 0 ) {
			//globalOutputStream() << "motion x: " << dx << ", y: " << dy << "\n";
			if ( ddx < -32 || ddx > 32 || ddy < -32 || ddy > 32 ) {
				Sys_SetCursorPos( GTK_WINDOW( widget ), self->center_x, self->center_y );
				self->last_x = self->center_x;
				self->last_y = self->center_y;
			}
#else
		const int dx = current_x - self->center_x;
		const int dy = current_y - self->center_y;
		if ( dx != 0 || dy != 0 ) {
			//globalOutputStream() << "motion x: " << dx << ", y: " << dy << "\n";
			Sys_SetCursorPos( GTK_WINDOW( widget ), self->center_x, self->center_y );
#endif
			self->m_function( dx, dy, event->state, self->m_data );
		}
		return FALSE;
	}

	void freeze_pointer( GtkWindow* window, GtkWidget* widget, MotionDeltaFunction function, void* data ){
		ASSERT_MESSAGE( m_function == 0, "can't freeze pointer: already frozen" );

		const GdkEventMask mask = static_cast<GdkEventMask>( GDK_POINTER_MOTION_MASK
		                                                   | GDK_POINTER_MOTION_HINT_MASK
		                                                   | GDK_BUTTON_MOTION_MASK
		                                                   | GDK_BUTTON1_MOTION_MASK
		                                                   | GDK_BUTTON2_MOTION_MASK
		                                                   | GDK_BUTTON3_MOTION_MASK
		                                                   | GDK_BUTTON_PRESS_MASK
		                                                   | GDK_BUTTON_RELEASE_MASK
		                                                   | GDK_VISIBILITY_NOTIFY_MASK );

		GdkCursor* cursor = gdk_cursor_new( GDK_BLANK_CURSOR );
		//GdkCursor* cursor = create_blank_cursor();
		//GdkGrabStatus status =
		/* fixes cursor runaways during srsly quick drags in camera
		drags with pressed buttons have no problem at all w/o this */
		gdk_pointer_grab( gtk_widget_get_window( GTK_WIDGET( window ) ), TRUE, mask, 0, cursor, GDK_CURRENT_TIME );
		//gdk_window_set_cursor ( gtk_widget_get_window( GTK_WIDGET( window ) ), cursor );
		/* is needed to fix activating neighbor widgets, that happens, if using upper one */
		gtk_grab_add( widget );

		gdk_cursor_unref( cursor );

		Sys_GetCursorPos( window, &recorded_x, &recorded_y );

		/* using center for tracking for max safety */
		gdk_window_get_origin( gtk_widget_get_window( widget ), &center_x, &center_y );
		GtkAllocation allocation;
		gtk_widget_get_allocation( widget, &allocation );
		center_y += allocation.height / 2;
		center_x += allocation.width / 2;

		Sys_SetCursorPos( window, center_x, center_y );

		last_x = center_x;
		last_y = center_y;

		m_widget = widget;
		m_window = window;

		m_function = function;
		m_data = data;

		handle_motion = g_signal_connect( G_OBJECT( window ), "motion_notify_event", G_CALLBACK( motion_delta ), this );
	}

	void unfreeze_pointer( bool centerize ){
		ASSERT_MESSAGE( m_function != 0, "can't unfreeze pointer: is not frozen" );

		g_signal_handler_disconnect( G_OBJECT( m_window ), handle_motion );

		m_function = 0;
		m_data = 0;

		if( centerize ){
			Sys_SetCursorPos( m_window, center_x, center_y );
		}
		else{
			Sys_SetCursorPos( m_window, recorded_x, recorded_y );
		}
//	gdk_window_set_cursor( gtk_widget_get_window( GTK_WIDGET( m_window ) ), 0 );
		gdk_pointer_ungrab( GDK_CURRENT_TIME );

		gtk_grab_remove( m_widget );
	}
};





class DeferredAdjustment
{
	gdouble m_value;
	guint m_handler;
	typedef void ( *ValueChangedFunction )( void* data, gdouble value );
	ValueChangedFunction m_function;
	void* m_data;

	static gboolean deferred_value_changed( gpointer data ){
		DeferredAdjustment* self = reinterpret_cast<DeferredAdjustment*>( data );
		self->m_function(
		    self->m_data,
		    self->m_value
		);
		self->m_handler = 0;
		self->m_value = 0;
		return FALSE;
	}
public:
	DeferredAdjustment( ValueChangedFunction function, void* data ) : m_value( 0 ), m_handler( 0 ), m_function( function ), m_data( data ){
	}
	void flush(){
		if ( m_handler != 0 ) {
			g_source_remove( m_handler );
			deferred_value_changed( this );
		}
	}
	void value_changed( gdouble value ){
		m_value = value;
		if ( m_handler == 0 ) {
			m_handler = g_idle_add( deferred_value_changed, this );
		}
	}
	static void adjustment_value_changed( GtkAdjustment *adjustment, DeferredAdjustment* self ){
		self->value_changed( gtk_adjustment_get_value( adjustment ) );
	}
};


#endif

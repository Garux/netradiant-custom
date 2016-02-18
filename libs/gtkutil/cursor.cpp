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

#include "cursor.h"

#include "stream/textstream.h"

#include <string.h>
#include <gdk/gdk.h>


GdkCursor* create_blank_cursor(){
	return gdk_cursor_new(GDK_BLANK_CURSOR);
}

void blank_cursor( GtkWidget* widget ){
	GdkCursor* cursor = create_blank_cursor();
	gdk_window_set_cursor( gtk_widget_get_window(widget), cursor );
	gdk_cursor_unref( cursor );
}

void default_cursor( GtkWidget* widget ){
	gdk_window_set_cursor( gtk_widget_get_window(widget), 0 );
}


void Sys_GetCursorPos( GtkWindow* window, int *x, int *y ){
	gdk_display_get_pointer( gdk_display_get_default(), 0, x, y, 0 );
}

void Sys_SetCursorPos( GtkWindow* window, int x, int y ){
	GdkScreen *screen;
	gdk_display_get_pointer( gdk_display_get_default(), &screen, 0, 0, 0 );
	gdk_display_warp_pointer( gdk_display_get_default(), screen, x, y );
}

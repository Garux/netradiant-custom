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

#include "toolbar.h"

#include <gtk/gtk.h>

#include "generic/callback.h"

#include "accelerator.h"
#include "button.h"
#include "image.h"
#include "closure.h"
#include "pointer.h"


GtkToolbar* toolbar_new(){
	GtkToolbar* toolbar = GTK_TOOLBAR( gtk_toolbar_new() );
	gtk_orientable_set_orientation( GTK_ORIENTABLE( toolbar ), GTK_ORIENTATION_HORIZONTAL );
	gtk_toolbar_set_style( toolbar, GTK_TOOLBAR_ICONS );
	gtk_toolbar_set_show_arrow( toolbar, FALSE );
	gtk_widget_show( GTK_WIDGET( toolbar ) );
	return toolbar;
}

void toolbar_append_space( GtkToolbar* toolbar ){
	GtkToolItem* space = gtk_separator_tool_item_new();
	gtk_widget_show( GTK_WIDGET( space ) );
	gtk_toolbar_insert( toolbar, space, -1 );
}

void toolbar_append( GtkToolbar* toolbar, GtkToolItem* button, const char* description ){
	gtk_widget_show_all( GTK_WIDGET( button ) );
	gtk_tool_item_set_tooltip_text( button, description );
//	gtk_button_set_relief( button, GTK_RELIEF_NONE );
//	gtk_widget_set_can_focus( GTK_WIDGET( button ), FALSE );
//	gtk_widget_set_can_default( GTK_WIDGET( button ), FALSE );
	gtk_toolbar_insert( toolbar, button, -1 );
}

GtkToolButton* toolbar_append_button( GtkToolbar* toolbar, const char* description, const char* icon, const Callback& callback ){
	GtkToolButton* button = GTK_TOOL_BUTTON( gtk_tool_button_new( GTK_WIDGET( new_local_image( icon ) ), nullptr ) );
	button_connect_callback( button, callback );
	toolbar_append( toolbar, GTK_TOOL_ITEM( button ), description );
	return button;
}

GtkToggleToolButton* toolbar_append_toggle_button( GtkToolbar* toolbar, const char* description, const char* icon, const Callback& callback ){
	GtkToggleToolButton* button = GTK_TOGGLE_TOOL_BUTTON( gtk_toggle_tool_button_new() );
	gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( button ), GTK_WIDGET( new_local_image( icon ) ) );
	toggle_button_connect_callback( button, callback );
	toolbar_append( toolbar, GTK_TOOL_ITEM( button ), description );
	return button;
}

GtkToolButton* toolbar_append_button( GtkToolbar* toolbar, const char* description, const char* icon, const Command& command ){
	return toolbar_append_button( toolbar, description, icon, command.m_callback );
}

void toggle_button_set_active_callback( GtkToggleToolButton& button, bool active ){
	toggle_button_set_active_no_signal( &button, active );
}
typedef ReferenceCaller1<GtkToggleToolButton, bool, toggle_button_set_active_callback> ToggleButtonSetActiveCaller;

GtkToggleToolButton* toolbar_append_toggle_button( GtkToolbar* toolbar, const char* description, const char* icon, const Toggle& toggle ){
	GtkToggleToolButton* button = toolbar_append_toggle_button( toolbar, description, icon, toggle.m_command.m_callback );
	toggle.m_exportCallback( ToggleButtonSetActiveCaller( *button ) );
	return button;
}

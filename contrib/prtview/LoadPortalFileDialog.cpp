/*
   PrtView plugin for GtkRadiant
   Copyright (C) 2001 Geoffrey Dewan, Loki software and qeradiant.com

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// LoadPortalFileDialog.cpp : implementation file
//

#include "LoadPortalFileDialog.h"

#include <gtk/gtk.h>
#include "stream/stringstream.h"
#include "convert.h"
#include "gtkutil/pointer.h"

#include "qerplugin.h"

#include "prtview.h"
#include "portals.h"

static void dialog_button_callback( GtkWidget *widget, gpointer data ){
	GtkWidget *parent;
	int *loop, *ret;

	parent = gtk_widget_get_toplevel( widget );
	loop = (int*)g_object_get_data( G_OBJECT( parent ), "loop" );
	ret = (int*)g_object_get_data( G_OBJECT( parent ), "ret" );

	*loop = 0;
	*ret = gpointer_to_int( data );
}

static gint dialog_delete_callback( GtkWidget *widget, GdkEvent* event, gpointer data ){
	int *loop;

	gtk_widget_hide( widget );
	loop = (int*)g_object_get_data( G_OBJECT( widget ), "loop" );
	*loop = 0;

	return TRUE;
}

static void change_clicked( GtkWidget *widget, gpointer data ){
	if ( const char* filename = GlobalRadiant().m_pfnFileDialog( g_pRadiantWnd, true, "Locate portal (.prt) file", gtk_entry_get_text( GTK_ENTRY( data ) ), 0, true, false, false ) ) {
		strcpy( portals.fn, filename );
		gtk_entry_set_text( GTK_ENTRY( data ), filename );
	}
}

int DoLoadPortalFileDialog(){
	GtkWidget *dlg, *vbox, *hbox, *button, *entry, *check2d, *check3d;
	int loop = 1, ret = IDCANCEL;

	dlg = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );
	gtk_window_set_title( GTK_WINDOW( dlg ), "Load .prt" );
	g_signal_connect( G_OBJECT( dlg ), "delete_event",
	                  G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( G_OBJECT( dlg ), "destroy",
	                  G_CALLBACK( gtk_widget_destroy ), NULL );
	g_object_set_data( G_OBJECT( dlg ), "loop", &loop );
	g_object_set_data( G_OBJECT( dlg ), "ret", &ret );

	vbox = gtk_vbox_new( FALSE, 5 );
	gtk_widget_show( vbox );
	gtk_container_add( GTK_CONTAINER( dlg ), vbox );
	gtk_container_set_border_width( GTK_CONTAINER( vbox ), 5 );

	entry = gtk_entry_new();
	gtk_widget_show( entry );
//	gtk_editable_set_editable( GTK_EDITABLE( entry ), FALSE );
	gtk_box_pack_start( GTK_BOX( vbox ), entry, FALSE, FALSE, 0 );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );

	check3d = gtk_check_button_new_with_label( "Show 3D" );
	gtk_widget_show( check3d );
	gtk_box_pack_start( GTK_BOX( hbox ), check3d, FALSE, FALSE, 0 );

	check2d = gtk_check_button_new_with_label( "Show 2D" );
	gtk_widget_show( check2d );
	gtk_box_pack_start( GTK_BOX( hbox ), check2d, FALSE, FALSE, 0 );

	button = gtk_button_new_with_label( "Change" );
	gtk_widget_show( button );
	gtk_box_pack_end( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( change_clicked ), entry );
	gtk_widget_set_size_request( button, 60, -1 );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );

	button = gtk_button_new_with_label( "Cancel" );
	gtk_widget_show( button );
	gtk_box_pack_end( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked",
	                  G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDCANCEL ) );
	gtk_widget_set_size_request( button, 60, -1 );

	button = gtk_button_new_with_label( "OK" );
	gtk_widget_show( button );
	gtk_box_pack_end( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked",
	                  G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	gtk_widget_set_size_request( button, 60, -1 );

	strcpy( portals.fn, GlobalRadiant().getMapName() );
	char* fn = strrchr( portals.fn, '.' );
	if ( fn != NULL ) {
		strcpy( fn, ".prt" );
	}

	StringOutputStream value( 256 );
	value << portals.fn;
	gtk_entry_set_text( GTK_ENTRY( entry ), value.c_str() );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check2d ), portals.show_2d );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check3d ), portals.show_3d );

	gtk_widget_show( dlg );

	while ( loop )
		gtk_main_iteration();

	if ( ret == IDOK ) {
		strcpy( portals.fn, gtk_entry_get_text( GTK_ENTRY( entry ) ) );

		portals.Purge();

		portals.show_3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check3d ) ) ? true : false;
		portals.show_2d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check2d ) ) ? true : false;
	}

	gtk_widget_destroy( dlg );

	return ret;
}

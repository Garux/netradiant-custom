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

#include "AboutDialog.h"
#include <gtk/gtk.h>
#include "version.h"
#include "gtkutil/pointer.h"

#include "prtview.h"
#include "ConfigDialog.h"

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

void DoAboutDlg(){
	GtkWidget *dlg, *hbox, *vbox, *button, *label;
	int loop = 1, ret = IDCANCEL;

	dlg = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );
	gtk_window_set_title( GTK_WINDOW( dlg ), "About Portal Viewer" );
	g_signal_connect( G_OBJECT( dlg ), "delete_event",
						G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( G_OBJECT( dlg ), "destroy",
						G_CALLBACK( gtk_widget_destroy ), NULL );
	g_object_set_data( G_OBJECT( dlg ), "loop", &loop );
	g_object_set_data( G_OBJECT( dlg ), "ret", &ret );

	hbox = gtk_hbox_new( FALSE, 10 );
	gtk_widget_show( hbox );
	gtk_container_add( GTK_CONTAINER( dlg ), hbox );
	gtk_container_set_border_width( GTK_CONTAINER( hbox ), 10 );

	label = gtk_label_new( "Version 1.000\n\n"
						   "Gtk port by Leonardo Zide\nleo@lokigames.com\n\n"
						   "Written by Geoffrey DeWan\ngdewan@prairienet.org\n\n"
						   "Built against NetRadiant " RADIANT_VERSION "\n"
						   __DATE__
						   );
	gtk_widget_show( label );
	gtk_box_pack_start( GTK_BOX( hbox ), label, TRUE, TRUE, 0 );
	gtk_label_set_justify( GTK_LABEL( label ), GTK_JUSTIFY_LEFT );

	vbox = gtk_vbox_new( FALSE, 0 );
	gtk_widget_show( vbox );
	gtk_box_pack_start( GTK_BOX( hbox ), vbox, FALSE, FALSE, 0 );

	button = gtk_button_new_with_label( "OK" );
	gtk_widget_show( button );
	gtk_box_pack_start( GTK_BOX( vbox ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked",
						G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	gtk_widget_set_size_request( button, 60, -1 );

	gtk_widget_show( dlg );

	while ( loop )
		gtk_main_iteration();

	gtk_widget_destroy( dlg );
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDialog message handlers

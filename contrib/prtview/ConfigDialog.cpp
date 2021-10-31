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

#include "ConfigDialog.h"
#include <cstdio>
#include <gtk/gtk.h>
#include "gtkutil/pointer.h"

#include "iscenegraph.h"

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

// =============================================================================
// Color selection dialog

static int DoColor( PackedColour *c ){
	GtkWidget* dlg;
	GdkColor clr = { 0, guint16( GetRValue( *c ) * ( 65535 / 255 ) ),
	                    guint16( GetGValue( *c ) * ( 65535 / 255 ) ),
	                    guint16( GetBValue( *c ) * ( 65535 / 255 ) )
	               };
	int loop = 1, ret = IDCANCEL;

	dlg = gtk_color_selection_dialog_new( "Choose Color" );
	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );
	gtk_color_selection_set_current_color( GTK_COLOR_SELECTION( gtk_color_selection_dialog_get_color_selection( GTK_COLOR_SELECTION_DIALOG( dlg ) ) ), &clr );
	g_signal_connect( G_OBJECT( dlg ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( G_OBJECT( dlg ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	GtkWidget *ok_button, *cancel_button;
	g_object_get( G_OBJECT( dlg ), "ok-button", &ok_button, "cancel-button", &cancel_button, nullptr );

	g_signal_connect( G_OBJECT( ok_button ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	g_signal_connect( G_OBJECT( cancel_button ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDCANCEL ) );
	g_object_set_data( G_OBJECT( dlg ), "loop", &loop );
	g_object_set_data( G_OBJECT( dlg ), "ret", &ret );

	gtk_widget_show( dlg );

	while ( loop )
		gtk_main_iteration();

	gtk_color_selection_get_current_color( GTK_COLOR_SELECTION( gtk_color_selection_dialog_get_color_selection( GTK_COLOR_SELECTION_DIALOG( dlg ) ) ), &clr );

	gtk_widget_destroy( dlg );

	if ( ret == IDOK ) {
		*c = RGB( clr.red / (65535 / 255), clr.green / (65535 / 255), clr.blue / (65535 / 255) );
	}

	return ret;
}

static void Set2DText( GtkWidget* label ){
	char s[40];

	sprintf( s, "Line Width = %6.3f", portals.width_2d * 0.5f );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void Set3DText( GtkWidget* label ){
	char s[40];

	sprintf( s, "Line Width = %6.3f", portals.width_3d * 0.5f );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void Set3DTransText( GtkWidget* label ){
	char s[40];

	sprintf( s, "Polygon transparency = %d%%", (int)portals.trans_3d );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void SetClipText( GtkWidget* label ){
	char s[40];

	sprintf( s, "Cubic clip range = %d", (int)portals.clip_range * 64 );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void OnScroll2d( GtkAdjustment *adj, gpointer data ){
	portals.width_2d = static_cast<float>( gtk_adjustment_get_value( adj ) );
	Set2DText( GTK_WIDGET( data ) );

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnScroll3d( GtkAdjustment *adj, gpointer data ){
	portals.width_3d = static_cast<float>( gtk_adjustment_get_value( adj ) );
	Set3DText( GTK_WIDGET( data ) );

	SceneChangeNotify();
}

static void OnScrollTrans( GtkAdjustment *adj, gpointer data ){
	portals.trans_3d = static_cast<float>( gtk_adjustment_get_value( adj ) );
	Set3DTransText( GTK_WIDGET( data ) );

	SceneChangeNotify();
}

static void OnScrollClip( GtkAdjustment *adj, gpointer data ){
	portals.clip_range = static_cast<float>( gtk_adjustment_get_value( adj ) );
	SetClipText( GTK_WIDGET( data ) );

	SceneChangeNotify();
}

static void OnAntiAlias2d( GtkWidget *widget, gpointer data ){
	portals.aa_2d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	Portals_shadersChanged();

	SceneChangeNotify();
}

static void OnConfig2d( GtkWidget *widget, gpointer data ){
	portals.show_2d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	SceneChangeNotify();
}

static void OnColor2d( GtkWidget *widget, gpointer data ){
	if ( DoColor( &portals.color_2d ) == IDOK ) {
		Portals_shadersChanged();

		SceneChangeNotify();
	}
}

static void OnConfig3d( GtkWidget *widget, gpointer data ){
	portals.show_3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	SceneChangeNotify();
}


static void OnAntiAlias3d( GtkWidget *widget, gpointer data ){
	portals.aa_3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnColor3d( GtkWidget *widget, gpointer data ){
	if ( DoColor( &portals.color_3d ) == IDOK ) {
		Portals_shadersChanged();

		SceneChangeNotify();
	}
}

static void OnColorFog( GtkWidget *widget, gpointer data ){
	if ( DoColor( &portals.color_fog ) == IDOK ) {
		Portals_shadersChanged();

		SceneChangeNotify();
	}
}

static void OnFog( GtkWidget *widget, gpointer data ){
	portals.fog = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnSelchangeZbuffer( GtkComboBox *comboBox, gpointer data ){
	portals.zbuffer = gtk_combo_box_get_active( comboBox );

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnPoly( GtkWidget *widget, gpointer data ){
	portals.polygons = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) );

	SceneChangeNotify();
}

static void OnLines( GtkWidget *widget, gpointer data ){
	portals.lines = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) );

	SceneChangeNotify();
}

static void OnClip( GtkWidget *widget, gpointer data ){
	portals.clip = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	SceneChangeNotify();
}

void DoConfigDialog(){
	GtkWidget *dlg, *hbox, *vbox, *vbox2, *button, *table, *frame;
	GtkWidget *lw3slider, *lw3label, *lw2slider, *lw2label, *zlist;
	GtkWidget *aa2check, *aa3check, *depthcheck, *linescheck, *polyscheck;
	GtkWidget *transslider, *translabel, *clipslider, *cliplabel;
	GtkWidget *show2check, *show3check, *portalcheck;
	int loop = 1, ret = IDCANCEL;

	dlg = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );
	gtk_window_set_title( GTK_WINDOW( dlg ), "Portal Viewer Configuration" );
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

	frame = gtk_frame_new( "3D View" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, TRUE, TRUE, 0 );

	vbox2 = gtk_vbox_new( FALSE, 5 );
	gtk_widget_show( vbox2 );
	gtk_container_add( GTK_CONTAINER( frame ), vbox2 );
	gtk_container_set_border_width( GTK_CONTAINER( vbox2 ), 5 );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox, TRUE, TRUE, 0 );

	auto *adj = gtk_adjustment_new( portals.width_3d, 2, 40, 1, 1, 0 );
	lw3slider = gtk_hscale_new( GTK_ADJUSTMENT( adj ) );
	gtk_widget_show( lw3slider );
	gtk_box_pack_start( GTK_BOX( hbox ), lw3slider, TRUE, TRUE, 0 );
	gtk_scale_set_draw_value( GTK_SCALE( lw3slider ), FALSE );

	lw3label = gtk_label_new( "" );
	gtk_widget_show( lw3label );
	gtk_box_pack_start( GTK_BOX( hbox ), lw3label, FALSE, TRUE, 0 );
	g_signal_connect( adj, "value_changed", G_CALLBACK( OnScroll3d ), lw3label );

	table = gtk_table_new( 2, 4, FALSE );
	gtk_widget_show( table );
	gtk_box_pack_start( GTK_BOX( vbox2 ), table, TRUE, TRUE, 0 );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );

	button = gtk_button_new_with_label( "Color" );
	gtk_widget_show( button );
	gtk_table_attach( GTK_TABLE( table ), button, 0, 1, 0, 1,
	                  (GtkAttachOptions) ( GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( OnColor3d ), NULL );

	button = gtk_button_new_with_label( "Depth Color" );
	gtk_widget_show( button );
	gtk_table_attach( GTK_TABLE( table ), button, 0, 1, 1, 2,
	                  (GtkAttachOptions) ( GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( OnColorFog ), NULL );

	aa3check = gtk_check_button_new_with_label( "Anti-Alias (May not work on some video cards)" );
	gtk_widget_show( aa3check );
	gtk_table_attach( GTK_TABLE( table ), aa3check, 1, 4, 0, 1,
	                  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	g_signal_connect( G_OBJECT( aa3check ), "toggled", G_CALLBACK( OnAntiAlias3d ), NULL );

	depthcheck = gtk_check_button_new_with_label( "Depth Cue" );
	gtk_widget_show( depthcheck );
	gtk_table_attach( GTK_TABLE( table ), depthcheck, 1, 2, 1, 2,
	                  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	g_signal_connect( G_OBJECT( depthcheck ), "toggled", G_CALLBACK( OnFog ), NULL );

	linescheck = gtk_check_button_new_with_label( "Lines" );
	gtk_widget_show( linescheck );
	gtk_table_attach( GTK_TABLE( table ), linescheck, 2, 3, 1, 2,
	                  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	g_signal_connect( G_OBJECT( linescheck ), "toggled", G_CALLBACK( OnLines ), NULL );

	polyscheck = gtk_check_button_new_with_label( "Polygons" );
	gtk_widget_show( polyscheck );
	gtk_table_attach( GTK_TABLE( table ), polyscheck, 3, 4, 1, 2,
	                  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	g_signal_connect( G_OBJECT( polyscheck ), "toggled", G_CALLBACK( OnPoly ), NULL );

	zlist = gtk_combo_box_text_new();
	gtk_widget_show( zlist );
	gtk_box_pack_start( GTK_BOX( vbox2 ), zlist, TRUE, FALSE, 0 );

	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( zlist ), "Z-Buffer Test and Write (recommended for solid or no polygons)" );
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( zlist ), "Z-Buffer Test Only (recommended for transparent polygons)" );
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( zlist ), "Z-Buffer Off" );

	g_signal_connect( G_OBJECT( zlist ), "changed", G_CALLBACK( OnSelchangeZbuffer ), nullptr );

	table = gtk_table_new( 2, 2, FALSE );
	gtk_widget_show( table );
	gtk_box_pack_start( GTK_BOX( vbox2 ), table, TRUE, TRUE, 0 );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );

	adj = gtk_adjustment_new( portals.trans_3d, 0, 100, 1, 1, 0 );
	transslider = gtk_hscale_new( GTK_ADJUSTMENT( adj ) );
	gtk_widget_show( transslider );
	gtk_table_attach( GTK_TABLE( table ), transslider, 0, 1, 0, 1,
	                  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_scale_set_draw_value( GTK_SCALE( transslider ), FALSE );

	translabel = gtk_label_new( "" );
	gtk_widget_show( translabel );
	gtk_table_attach( GTK_TABLE( table ), translabel, 1, 2, 0, 1,
	                  (GtkAttachOptions) ( GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_misc_set_alignment( GTK_MISC( translabel ), 0.0, 0.0 );
	g_signal_connect( adj, "value_changed", G_CALLBACK( OnScrollTrans ), translabel );

	adj = gtk_adjustment_new( portals.clip_range, 1, 128, 1, 1, 0 );
	clipslider = gtk_hscale_new( GTK_ADJUSTMENT( adj ) );
	gtk_widget_show( clipslider );
	gtk_table_attach( GTK_TABLE( table ), clipslider, 0, 1, 1, 2,
	                  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_scale_set_draw_value( GTK_SCALE( clipslider ), FALSE );

	cliplabel = gtk_label_new( "" );
	gtk_widget_show( cliplabel );
	gtk_table_attach( GTK_TABLE( table ), cliplabel, 1, 2, 1, 2,
	                  (GtkAttachOptions) ( GTK_FILL ),
	                  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_misc_set_alignment( GTK_MISC( cliplabel ), 0.0, 0.0 );
	g_signal_connect( adj, "value_changed", G_CALLBACK( OnScrollClip ), cliplabel );

	hbox = gtk_hbox_new( TRUE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox, TRUE, FALSE, 0 );

	show3check = gtk_check_button_new_with_label( "Show" );
	gtk_widget_show( show3check );
	gtk_box_pack_start( GTK_BOX( hbox ), show3check, TRUE, TRUE, 0 );
	g_signal_connect( G_OBJECT( show3check ), "toggled", G_CALLBACK( OnConfig3d ), NULL );

	portalcheck = gtk_check_button_new_with_label( "Portal cubic clipper" );
	gtk_widget_show( portalcheck );
	gtk_box_pack_start( GTK_BOX( hbox ), portalcheck, TRUE, TRUE, 0 );
	g_signal_connect( G_OBJECT( portalcheck ), "toggled", G_CALLBACK( OnClip ), NULL );

	frame = gtk_frame_new( "2D View" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, TRUE, TRUE, 0 );

	vbox2 = gtk_vbox_new( FALSE, 5 );
	gtk_widget_show( vbox2 );
	gtk_container_add( GTK_CONTAINER( frame ), vbox2 );
	gtk_container_set_border_width( GTK_CONTAINER( vbox2 ), 5 );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox, TRUE, FALSE, 0 );

	adj = gtk_adjustment_new( portals.width_2d, 2, 40, 1, 1, 0 );
	lw2slider = gtk_hscale_new( GTK_ADJUSTMENT( adj ) );
	gtk_widget_show( lw2slider );
	gtk_box_pack_start( GTK_BOX( hbox ), lw2slider, TRUE, TRUE, 0 );
	gtk_scale_set_draw_value( GTK_SCALE( lw2slider ), FALSE );

	lw2label = gtk_label_new( "" );
	gtk_widget_show( lw2label );
	gtk_box_pack_start( GTK_BOX( hbox ), lw2label, FALSE, TRUE, 0 );
	g_signal_connect( adj, "value_changed", G_CALLBACK( OnScroll2d ), lw2label );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox, TRUE, FALSE, 0 );

	button = gtk_button_new_with_label( "Color" );
	gtk_widget_show( button );
	gtk_box_pack_start( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( OnColor2d ), NULL );
	gtk_widget_set_size_request( button, 60, -1 );

	aa2check = gtk_check_button_new_with_label( "Anti-Alias (May not work on some video cards)" );
	gtk_widget_show( aa2check );
	gtk_box_pack_start( GTK_BOX( hbox ), aa2check, TRUE, TRUE, 0 );
	g_signal_connect( G_OBJECT( aa2check ), "toggled", G_CALLBACK( OnAntiAlias2d ), NULL );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox, TRUE, FALSE, 0 );

	show2check = gtk_check_button_new_with_label( "Show" );
	gtk_widget_show( show2check );
	gtk_box_pack_start( GTK_BOX( hbox ), show2check, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( show2check ), "toggled", G_CALLBACK( OnConfig2d ), NULL );

	hbox = gtk_hbox_new( FALSE, 5 );
	gtk_widget_show( hbox );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );

	button = gtk_button_new_with_label( "OK" );
	gtk_widget_show( button );
	gtk_box_pack_end( GTK_BOX( hbox ), button, FALSE, FALSE, 0 );
	g_signal_connect( G_OBJECT( button ), "clicked",
	                  G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	gtk_widget_set_size_request( button, 60, -1 );

	// initialize dialog
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show2check ), portals.show_2d );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( aa2check ), portals.aa_2d );
	Set2DText( lw2label );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show3check ), portals.show_3d );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( depthcheck ), portals.fog );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( polyscheck ), portals.polygons );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( linescheck ), portals.lines );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( aa3check ), portals.aa_3d );
	gtk_combo_box_set_active( GTK_COMBO_BOX( zlist ), portals.zbuffer );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( portalcheck ), portals.clip );

	Set3DText( lw3label );
	Set3DTransText( translabel );
	SetClipText( cliplabel );

	gtk_widget_show( dlg );

	while ( loop )
		gtk_main_iteration();

	gtk_widget_destroy( dlg );
}

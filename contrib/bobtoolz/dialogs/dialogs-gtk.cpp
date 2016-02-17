/*
   BobToolz plugin for GtkRadiant
   Copyright (C) 2001 Gordon Biggans

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

#include "dialogs-gtk.h"
#include "../funchandlers.h"

#include <cstdlib>
#include "str.h"
#include <list>
#include <gtk/gtk.h>
#include "gtkutil/pointer.h"

#include "../lists.h"
#include "../misc.h"


/*--------------------------------
        Callback Functions
   ---------------------------------*/

typedef struct {
	GtkWidget *cbTexChange;
	GtkWidget *editTexOld, *editTexNew;

	GtkWidget *cbScaleHor, *cbScaleVert;
	GtkWidget *editScaleHor, *editScaleVert;

	GtkWidget *cbShiftHor, *cbShiftVert;
	GtkWidget *editShiftHor, *editShiftVert;

	GtkWidget *cbRotation;
	GtkWidget *editRotation;
}dlg_texReset_t;

dlg_texReset_t dlgTexReset;

void Update_TextureReseter();

static void dialog_button_callback_texreset_update( GtkWidget *widget, gpointer data ){
	Update_TextureReseter();
}

void Update_TextureReseter(){
	gboolean check;

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbTexChange ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editTexNew ), check );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editTexOld ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleHor ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editScaleHor ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleVert ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editScaleVert ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftHor ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editShiftHor ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftVert ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editShiftVert ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbRotation ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editRotation ), check );
}

static void dialog_button_callback( GtkWidget *widget, gpointer data ){
	GtkWidget *parent;
	int *loop;
	EMessageBoxReturn *ret;

	parent = gtk_widget_get_toplevel( widget );
	loop = (int*)g_object_get_data( G_OBJECT( parent ), "loop" );
	ret = (EMessageBoxReturn*)g_object_get_data( G_OBJECT( parent ), "ret" );

	*loop = 0;
	*ret = (EMessageBoxReturn)gpointer_to_int( data );
}

static gint dialog_delete_callback( GtkWidget *widget, GdkEvent* event, gpointer data ){
	int *loop;

	gtk_widget_hide( widget );
	loop = (int*)g_object_get_data( G_OBJECT( widget ), "loop" );
	*loop = 0;

	return TRUE;
}

static void dialog_button_callback_settex( GtkWidget *widget, gpointer data ){
	TwinWidget* tw = (TwinWidget*)data;

	GtkEntry* entry = GTK_ENTRY( tw->one );
	auto* combo = tw->two;
	const gchar* tex = gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
	gtk_entry_set_text( entry, tex );
}

/*--------------------------------
    Data validation Routines
   ---------------------------------*/

bool ValidateTextFloat( const char* pData, const char* error_title, float* value ){
	if ( pData ) {
		float testNum = (float)atof( pData );

		if ( ( testNum == 0.0f ) && strcmp( pData, "0" ) ) {
			DoMessageBox( "Please Enter A Floating Point Number", error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( "Please Enter A Floating Point Number", error_title, eMB_OK );
	return FALSE;
}

bool ValidateTextFloatRange( const char* pData, float min, float max, const char* error_title, float* value ){
	char error_buffer[256];
	sprintf( error_buffer, "Please Enter A Floating Point Number Between %.3f and %.3f", min, max );

	if ( pData ) {
		float testNum = (float)atof( pData );

		if ( ( testNum < min ) || ( testNum > max ) ) {
			DoMessageBox( error_buffer, error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( error_buffer, error_title, eMB_OK );
	return FALSE;
}

bool ValidateTextIntRange( const char* pData, int min, int max, const char* error_title, int* value ){
	char error_buffer[256];
	sprintf( error_buffer, "Please Enter An Integer Between %i and %i", min, max );

	if ( pData ) {
		int testNum = atoi( pData );

		if ( ( testNum < min ) || ( testNum > max ) ) {
			DoMessageBox( error_buffer, error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( error_buffer, error_title, eMB_OK );
	return FALSE;
}

bool ValidateTextInt( const char* pData, const char* error_title, int* value ){
	if ( pData ) {
		int testNum = atoi( pData );

		if ( ( testNum == 0 ) && strcmp( pData, "0" ) ) {
			DoMessageBox( "Please Enter An Integer", error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( "Please Enter An Integer", error_title, eMB_OK );
	return FALSE;
}

/*--------------------------------
        Modal Dialog Boxes
   ---------------------------------*/

/*

   Major clean up of variable names etc required, excluding Mars's ones,
   which are nicely done :)

 */

EMessageBoxReturn DoMessageBox( const char* lpText, const char* lpCaption, EMessageBoxType type ){
	ui::Widget window, w, vbox, hbox;
	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );
	g_signal_connect( GTK_OBJECT( window ), "delete_event",
						G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy",
						G_CALLBACK( gtk_widget_destroy ), NULL );
	gtk_window_set_title( GTK_WINDOW( window ), lpCaption );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );
	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );
	gtk_widget_realize( window );

	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	w = ui::Label( lpText );
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	w = ui::Widget(gtk_hseparator_new());
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 2 );
	gtk_widget_show( w );

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	if ( type == eMB_OK ) {
		w = ui::Button( "Ok" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
		gtk_widget_set_can_default(w, true);
		gtk_widget_grab_default( w );
		gtk_widget_show( w );
		ret = eIDOK;
	}
	else if ( type ==  eMB_OKCANCEL ) {
		w = ui::Button( "Ok" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
		gtk_widget_set_can_default( w, true );
		gtk_widget_grab_default( w );
		gtk_widget_show( w );

		w = ui::Button( "Cancel" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
		gtk_widget_show( w );
		ret = eIDCANCEL;
	}
	else if ( type == eMB_YESNOCANCEL ) {
		w = ui::Button( "Yes" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
		gtk_widget_set_can_default( w, true );
		gtk_widget_grab_default( w );
		gtk_widget_show( w );

		w = ui::Button( "No" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDNO ) );
		gtk_widget_show( w );

		w = ui::Button( "Cancel" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
		gtk_widget_show( w );
		ret = eIDCANCEL;
	}
	else /* if (mode == MB_YESNO) */
	{
		w = ui::Button( "Yes" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
		gtk_widget_set_can_default( w, true );
		gtk_widget_grab_default( w );
		gtk_widget_show( w );

		w = ui::Button( "No" );
		gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
		g_signal_connect( GTK_OBJECT( w ), "clicked",
							G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDNO ) );
		gtk_widget_show( w );
		ret = eIDNO;
	}

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

EMessageBoxReturn DoIntersectBox( IntersectRS* rs ){
	GtkWidget *window, *w, *vbox, *hbox;
	GtkWidget *check1, *check2;
	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Intersect" );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// ---- vbox ----


	auto radio1 = gtk_radio_button_new_with_label( NULL, "Use Whole Map" );
	gtk_box_pack_start( GTK_BOX( vbox ), radio1, FALSE, FALSE, 2 );
	gtk_widget_show( radio1 );

	auto radio2 = gtk_radio_button_new_with_label( gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio1)), "Use Selected Brushes" );
	gtk_box_pack_start( GTK_BOX( vbox ), radio2, FALSE, FALSE, 2 );
	gtk_widget_show( radio2 );

	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 2 );
	gtk_widget_show( w );

	check1 = ui::CheckButton( "Include Detail Brushes" );
	gtk_box_pack_start( GTK_BOX( vbox ), check1, FALSE, FALSE, 0 );
	gtk_widget_show( check1 );

	check2 = ui::CheckButton( "Select Duplicate Brushes Only" );
	gtk_box_pack_start( GTK_BOX( vbox ), check2, FALSE, FALSE, 0 );
	gtk_widget_show( check2 );

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ---- ok/cancel buttons

	w = ui::Button( "Ok" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	if ( gtk_toggle_button_get_active( (GtkToggleButton*)radio1 ) ) {
		rs->nBrushOptions = BRUSH_OPT_WHOLE_MAP;
	}
	else if ( gtk_toggle_button_get_active( (GtkToggleButton*)radio2 ) ) {
		rs->nBrushOptions = BRUSH_OPT_SELECTED;
	}

	rs->bUseDetail = gtk_toggle_button_get_active( (GtkToggleButton*)check1 ) ? true : false;
	rs->bDuplicateOnly = gtk_toggle_button_get_active( (GtkToggleButton*)check2 ) ? true : false;

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

EMessageBoxReturn DoPolygonBox( PolygonRS* rs ){
	GtkWidget *window, *w, *vbox, *hbox, *vbox2, *hbox2;

	GtkWidget *check1, *check2, *check3;
	GtkWidget *text1, *text2;

	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Polygon Builder" );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// ---- vbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----


	vbox2 = ui::VBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( hbox ), vbox2, FALSE, FALSE, 2 );
	gtk_widget_show( vbox2 );

	// ---- vbox2 ----

	hbox2 = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox2, FALSE, FALSE, 2 );
	gtk_widget_show( hbox2 );

	// ---- hbox2 ----

	text1 = ui::Entry( 256 );
	gtk_entry_set_text( (GtkEntry*)text1, "3" );
	gtk_box_pack_start( GTK_BOX( hbox2 ), text1, FALSE, FALSE, 2 );
	gtk_widget_show( text1 );

	w = ui::Label( "Number Of Sides" );
	gtk_box_pack_start( GTK_BOX( hbox2 ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	// ---- /hbox2 ----

	hbox2 = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox2 ), hbox2, FALSE, FALSE, 2 );
	gtk_widget_show( hbox2 );

	// ---- hbox2 ----

	text2 = ui::Entry( 256 );
	gtk_entry_set_text( (GtkEntry*)text2, "8" );
	gtk_box_pack_start( GTK_BOX( hbox2 ), text2, FALSE, FALSE, 2 );
	gtk_widget_show( text2 );

	w = ui::Label( "Border Width" );
	gtk_box_pack_start( GTK_BOX( hbox2 ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	// ---- /hbox2 ----

	// ---- /vbox2 ----



	vbox2 = ui::VBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( hbox ), vbox2, FALSE, FALSE, 2 );
	gtk_widget_show( vbox2 );

	// ---- vbox2 ----

	check1 = ui::CheckButton( "Use Border" );
	gtk_box_pack_start( GTK_BOX( vbox2 ), check1, FALSE, FALSE, 0 );
	gtk_widget_show( check1 );


	check2 = ui::CheckButton( "Inverse Polygon" );
	gtk_box_pack_start( GTK_BOX( vbox2 ), check2, FALSE, FALSE, 0 );
	gtk_widget_show( check2 );


	check3 = ui::CheckButton( "Align Top Edge" );
	gtk_box_pack_start( GTK_BOX( vbox2 ), check3, FALSE, FALSE, 0 );
	gtk_widget_show( check3 );

	// ---- /vbox2 ----

	// ---- /hbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	w = ui::Button( "Ok" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDOK ) {
			rs->bUseBorder = gtk_toggle_button_get_active( (GtkToggleButton*)check1 ) ? true : false;
			rs->bInverse = gtk_toggle_button_get_active( (GtkToggleButton*)check2 ) ? true : false;
			rs->bAlignTop = gtk_toggle_button_get_active( (GtkToggleButton*)check3 ) ? true : false;

			if ( !ValidateTextIntRange( gtk_entry_get_text( (GtkEntry*)text1 ), 3, 32, "Number Of Sides", &rs->nSides ) ) {
				dialogError = TRUE;
			}

			if ( rs->bUseBorder ) {
				if ( !ValidateTextIntRange( gtk_entry_get_text( (GtkEntry*)text2 ), 8, 256, "Border Width", &rs->nBorderWidth ) ) {
					dialogError = TRUE;
				}
			}
		}
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

// mars
// for stair builder stuck as close as i could to the MFC version
// obviously feel free to change it at will :)
EMessageBoxReturn DoBuildStairsBox( BuildStairsRS* rs ){
	// i made widgets for just about everything ... i think that's what i need to do  dunno tho
	GtkWidget   *window, *w, *vbox, *hbox;
	GtkWidget   *textStairHeight, *textRiserTex, *textMainTex;
	GtkWidget   *radioNorth, *radioSouth, *radioEast, *radioWest;   // i'm guessing we can't just abuse 'w' for these if we're getting a value
	GtkWidget   *radioOldStyle, *radioBobStyle, *radioCornerStyle;
	GtkWidget   *checkUseDetail;
	GSList      *radioDirection, *radioStyle;
	EMessageBoxReturn ret;
	int loop = 1;

	const gchar    *text = "Please set a value in the boxes below and press 'OK' to build the stairs";

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Stair Builder" );

	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	// new vbox
	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	hbox = ui::HBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( vbox ), hbox );
	gtk_widget_show( hbox );

	// dunno if you want this text or not ...
	w = ui::Label( text );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 0 ); // not entirely sure on all the parameters / what they do ...
	gtk_widget_show( w );

	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// ------------------------- // indenting == good way of keeping track of lines :)

	// new hbox
	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textStairHeight = ui::Entry( 256 );
	gtk_box_pack_start( GTK_BOX( hbox ), textStairHeight, FALSE, FALSE, 1 );
	gtk_widget_show( textStairHeight );

	w = ui::Label( "Stair Height" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 1 );
	gtk_widget_show( w );

	// ------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	w = ui::Label( "Direction:" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 5 );
	gtk_widget_show( w );

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	// radio buttons confuse me ...
	// but this _looks_ right

	// djbob: actually it looks very nice :), slightly better than the way i did it
	// edit: actually it doesn't work :P, you must pass the last radio item each time, ugh

	radioNorth = gtk_radio_button_new_with_label( NULL, "North" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioNorth, FALSE, FALSE, 3 );
	gtk_widget_show( radioNorth );

	radioDirection = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioNorth ) );

	radioSouth = gtk_radio_button_new_with_label( radioDirection, "South" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioSouth, FALSE, FALSE, 2 );
	gtk_widget_show( radioSouth );

	radioDirection = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioSouth ) );

	radioEast = gtk_radio_button_new_with_label( radioDirection, "East" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioEast, FALSE, FALSE, 1 );
	gtk_widget_show( radioEast );

	radioDirection = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioEast ) );

	radioWest = gtk_radio_button_new_with_label( radioDirection, "West" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioWest, FALSE, FALSE, 0 );
	gtk_widget_show( radioWest );

	// --------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	w = ui::Label( "Style:" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 5 );
	gtk_widget_show( w );

	// --------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	radioOldStyle = gtk_radio_button_new_with_label( NULL, "Original" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioOldStyle, FALSE, FALSE, 0 );
	gtk_widget_show( radioOldStyle );

	radioStyle = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioOldStyle ) );

	radioBobStyle = gtk_radio_button_new_with_label( radioStyle, "Bob's Style" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioBobStyle, FALSE, FALSE, 0 );
	gtk_widget_show( radioBobStyle );

	radioStyle = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioBobStyle ) );

	radioCornerStyle = gtk_radio_button_new_with_label( radioStyle, "Corner Style" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioCornerStyle, FALSE, FALSE, 0 );
	gtk_widget_show( radioCornerStyle );

	// err, the q3r has an if or something so you need bob style checked before this
	// is "ungreyed out" but you'll need to do that, as i suck :)

	// djbob: er.... yeah um, im not at all sure how i'm gonna sort this
	// djbob: think we need some button callback functions or smuffin
	// FIXME: actually get around to doing what i suggested!!!!

	checkUseDetail = ui::CheckButton( "Use Detail Brushes" );
	gtk_box_pack_start( GTK_BOX( hbox ), checkUseDetail, FALSE, FALSE, 0 );
	gtk_widget_show( checkUseDetail );

	// --------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textMainTex = ui::Entry( 512 );
	gtk_entry_set_text( GTK_ENTRY( textMainTex ), rs->mainTexture );
	gtk_box_pack_start( GTK_BOX( hbox ), textMainTex, FALSE, FALSE, 0 );
	gtk_widget_show( textMainTex );

	w = ui::Label( "Main Texture" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 1 );
	gtk_widget_show( w );

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textRiserTex = ui::Entry( 512 );
	gtk_box_pack_start( GTK_BOX( hbox ), textRiserTex, FALSE, FALSE, 0 );
	gtk_widget_show( textRiserTex );

	w = ui::Label( "Riser Texture" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 1 );
	gtk_widget_show( w );

	// -------------------------- //
	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	w = ui::Button( "OK" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );

	ret = eIDCANCEL;

// +djbob: need our "little" modal loop mars :P
	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDOK ) {
			rs->bUseDetail = gtk_toggle_button_get_active( (GtkToggleButton*)checkUseDetail ) ? true : false;

			strcpy( rs->riserTexture, gtk_entry_get_text( (GtkEntry*)textRiserTex ) );
			strcpy( rs->mainTexture, gtk_entry_get_text( (GtkEntry*)textMainTex ) );

			if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioNorth ) ) {
				rs->direction = MOVE_NORTH;
			}
			else if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioSouth ) ) {
				rs->direction = MOVE_SOUTH;
			}
			else if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioEast ) ) {
				rs->direction = MOVE_EAST;
			}
			else if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioWest ) ) {
				rs->direction = MOVE_WEST;
			}

			if ( !ValidateTextInt( gtk_entry_get_text( (GtkEntry*)textStairHeight ), "Stair Height", &rs->stairHeight ) ) {
				dialogError = TRUE;
			}

			if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioOldStyle ) ) {
				rs->style = STYLE_ORIGINAL;
			}
			else if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioBobStyle ) ) {
				rs->style = STYLE_BOB;
			}
			else if ( gtk_toggle_button_get_active( (GtkToggleButton*)radioCornerStyle ) ) {
				rs->style = STYLE_CORNER;
			}
		}
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
// -djbob

	// there we go, all done ... on my end at least, not bad for a night's work
}

EMessageBoxReturn DoDoorsBox( DoorRS* rs ){
	GtkWidget   *window, *hbox, *vbox, *w;
	GtkWidget   *textFrontBackTex, *textTrimTex;
	GtkWidget   *checkScaleMainH, *checkScaleMainV, *checkScaleTrimH, *checkScaleTrimV;
	GtkWidget   *comboMain, *comboTrim;
	GtkWidget   *buttonSetMain, *buttonSetTrim;
	GtkWidget   *radioNS, *radioEW;
	GSList      *radioOrientation;
	TwinWidget tw1, tw2;
	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Door Builder" );

	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	char buffer[256];
	GtkListStore *listMainTextures = gtk_list_store_new( 1, G_TYPE_STRING );
	GtkListStore *listTrimTextures = gtk_list_store_new( 1, G_TYPE_STRING );
	LoadGList( GetFilename( buffer, "plugins/bt/door-tex.txt" ), listMainTextures );
	LoadGList( GetFilename( buffer, "plugins/bt/door-tex-trim.txt" ), listTrimTextures );

	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textFrontBackTex = ui::Entry( 512 );
	gtk_entry_set_text( GTK_ENTRY( textFrontBackTex ), rs->mainTexture );
	gtk_box_pack_start( GTK_BOX( hbox ), textFrontBackTex, FALSE, FALSE, 0 );
	gtk_widget_show( textFrontBackTex );

	w = ui::Label( "Door Front/Back Texture" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// ------------------------ //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textTrimTex = ui::Entry( 512 );
	gtk_box_pack_start( GTK_BOX( hbox ), textTrimTex, FALSE, FALSE, 0 );
	gtk_widget_show( textTrimTex );

	w = ui::Label( "Door Trim Texture" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// ----------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	// sp: horizontally ????
	// djbob: yes mars, u can spell :]
	checkScaleMainH = ui::CheckButton( "Scale Main Texture Horizontally" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkScaleMainH ), TRUE );
	gtk_box_pack_start( GTK_BOX( hbox ), checkScaleMainH, FALSE, FALSE, 0 );
	gtk_widget_show( checkScaleMainH );

	checkScaleTrimH = ui::CheckButton( "Scale Trim Texture Horizontally" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkScaleTrimH ), TRUE );
	gtk_box_pack_start( GTK_BOX( hbox ), checkScaleTrimH, FALSE, FALSE, 0 );
	gtk_widget_show( checkScaleTrimH );

	// ---------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	checkScaleMainV = ui::CheckButton( "Scale Main Texture Vertically" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkScaleMainV ), TRUE );
	gtk_box_pack_start( GTK_BOX( hbox ), checkScaleMainV, FALSE, FALSE, 0 );
	gtk_widget_show( checkScaleMainV );

	checkScaleTrimV = ui::CheckButton( "Scale Trim Texture Vertically" );
	gtk_box_pack_start( GTK_BOX( hbox ), checkScaleTrimV, FALSE, FALSE, 0 );
	gtk_widget_show( checkScaleTrimV );

	// --------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	// djbob: lists added

	comboMain = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(listMainTextures));
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(comboMain), 0);
	gtk_box_pack_start( GTK_BOX( hbox ), comboMain, FALSE, FALSE, 0 );
	gtk_widget_show( comboMain );

	tw1.one = textFrontBackTex;
	tw1.two = GTK_COMBO_BOX(comboMain);

	buttonSetMain = ui::Button( "Set As Main Texture" );
	g_signal_connect( GTK_OBJECT( buttonSetMain ), "clicked", G_CALLBACK( dialog_button_callback_settex ), &tw1 );
	gtk_box_pack_start( GTK_BOX( hbox ), buttonSetMain, FALSE, FALSE, 0 );
	gtk_widget_show( buttonSetMain );

	// ------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	comboTrim = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(listTrimTextures));
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(comboMain), 0);
	gtk_box_pack_start( GTK_BOX( hbox ), comboTrim, FALSE, FALSE, 0 );
	gtk_widget_show( comboTrim );

	tw2.one = textTrimTex;
	tw2.two = GTK_COMBO_BOX(comboTrim);

	buttonSetTrim = ui::Button( "Set As Trim Texture" );
	g_signal_connect( GTK_OBJECT( buttonSetTrim ), "clicked", G_CALLBACK( dialog_button_callback_settex ), &tw2 );
	gtk_box_pack_start( GTK_BOX( hbox ), buttonSetTrim, FALSE, FALSE, 0 );
	gtk_widget_show( buttonSetTrim );

	// ------------------ //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	w = ui::Label( "Orientation" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// argh more radio buttons!
	radioNS = gtk_radio_button_new_with_label( NULL, "North - South" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioNS, FALSE, FALSE, 0 );
	gtk_widget_show( radioNS );

	radioOrientation = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioNS ) );

	radioEW = gtk_radio_button_new_with_label( radioOrientation, "East - West" );
	gtk_box_pack_start( GTK_BOX( hbox ), radioEW, FALSE, FALSE, 0 );
	gtk_widget_show( radioEW );

	// ----------------- //

	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// ----------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	w = ui::Button( "OK" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );
	ret = eIDCANCEL;

	// ----------------- //

//+djbob
	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	strcpy( rs->mainTexture, gtk_entry_get_text( GTK_ENTRY( textFrontBackTex ) ) );
	strcpy( rs->trimTexture, gtk_entry_get_text( GTK_ENTRY( textTrimTex ) ) );

	rs->bScaleMainH = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleMainH ) ) ? true : false;
	rs->bScaleMainV = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleMainV ) ) ? true : false;
	rs->bScaleTrimH = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleTrimH ) ) ? true : false;
	rs->bScaleTrimV = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleTrimV ) ) ? true : false;

	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radioNS ) ) ) {
		rs->nOrientation = DIRECTION_NS;
	}
	else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radioEW ) ) ) {
		rs->nOrientation = DIRECTION_EW;
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
//-djbob
}

EMessageBoxReturn DoPathPlotterBox( PathPlotterRS* rs ){
	GtkWidget *window, *w, *vbox, *hbox;

	GtkWidget *text1, *text2, *text3;
	GtkWidget *check1, *check2;

	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Texture Reset" );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// ---- vbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	text1 = ui::Entry( 256 );
	gtk_entry_set_text( (GtkEntry*)text1, "25" );
	gtk_box_pack_start( GTK_BOX( hbox ), text1, FALSE, FALSE, 2 );
	gtk_widget_show( text1 );

	w = ui::Label( "Number Of Points" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	// ---- /hbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	text2 = ui::Entry( 256 );
	gtk_entry_set_text( (GtkEntry*)text2, "3" );
	gtk_box_pack_start( GTK_BOX( hbox ), text2, FALSE, FALSE, 2 );
	gtk_widget_show( text2 );

	w = ui::Label( "Multipler" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	// ---- /hbox ----

	w = ui::Label( "Path Distance = dist(start -> apex) * multiplier" );
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	text3 = ui::Entry( 256 );
	gtk_entry_set_text( (GtkEntry*)text3, "-800" );
	gtk_box_pack_start( GTK_BOX( hbox ), text3, FALSE, FALSE, 2 );
	gtk_widget_show( text3 );

	w = ui::Label( "Gravity" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	// ---- /hbox ----

	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	check1 = ui::CheckButton( "No Dynamic Update" );
	gtk_box_pack_start( GTK_BOX( vbox ), check1, FALSE, FALSE, 0 );
	gtk_widget_show( check1 );

	check2 = ui::CheckButton( "Show Bounding Lines" );
	gtk_box_pack_start( GTK_BOX( vbox ), check2, FALSE, FALSE, 0 );
	gtk_widget_show( check2 );

	// ---- /vbox ----


	// ----------------- //

	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// ----------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	w = ui::Button( "Enable" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
	gtk_widget_show( w );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );

	w = ui::Button( "Disable" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDNO ) );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );

	ret = eIDCANCEL;

	// ----------------- //

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDYES ) {
			if ( !ValidateTextIntRange( gtk_entry_get_text( GTK_ENTRY( text1 ) ), 1, 200, "Number Of Points", &rs->nPoints ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloatRange( gtk_entry_get_text( GTK_ENTRY( text2 ) ), 1.0f, 10.0f, "Multiplier", &rs->fMultiplier ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloatRange( gtk_entry_get_text( GTK_ENTRY( text3 ) ), -10000.0f, -1.0f, "Gravity", &rs->fGravity ) ) {
				dialogError = TRUE;
			}

			rs->bNoUpdate = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check1 ) ) ? true : false;
			rs->bShowExtra = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check2 ) ) ? true : false;
		}
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

EMessageBoxReturn DoCTFColourChangeBox(){
	GtkWidget *window, *w, *vbox, *hbox;
	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "CTF Colour Changer" );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// ---- vbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, TRUE, TRUE, 0 );
	gtk_widget_show( hbox );

	// ---- hbox ---- ok/cancel buttons

	w = ui::Button( "Red->Blue" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Blue->Red" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

EMessageBoxReturn DoResetTextureBox( ResetTextureRS* rs ){
	Str texSelected;

	GtkWidget *window, *w, *vbox, *hbox, *frame, *table;

	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Texture Reset" );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// ---- vbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	texSelected = "Currently Selected Texture:   ";
	texSelected += GetCurrentTexture();

	w = ui::Label( texSelected );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	gtk_widget_show( w );

	// ---- /hbox ----

	frame = ui::Frame( "Reset Texture Names" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbTexChange = ui::CheckButton( "Enabled" );
	g_signal_connect( GTK_OBJECT( dlgTexReset.cbTexChange ), "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	gtk_widget_show( dlgTexReset.cbTexChange );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.cbTexChange, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );

	w = ui::Label( "Old Name: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editTexOld = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editTexOld ), rs->textureName );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editTexOld, 2, 3, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editTexOld );

	w = ui::Label( "New Name: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editTexNew = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editTexNew ), rs->textureName );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editTexNew, 2, 3, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editTexNew );

	// ---- /frame ----

	frame = ui::Frame( "Reset Scales" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbScaleHor = ui::CheckButton( "Enabled" );
	g_signal_connect( GTK_OBJECT( dlgTexReset.cbScaleHor ), "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	gtk_widget_show( dlgTexReset.cbScaleHor );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.cbScaleHor, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );

	w = ui::Label( "New Horizontal Scale: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editScaleHor = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editScaleHor ), "0.5" );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editScaleHor, 2, 3, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editScaleHor );


	dlgTexReset.cbScaleVert = ui::CheckButton( "Enabled" );
	g_signal_connect( GTK_OBJECT( dlgTexReset.cbScaleVert ), "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	gtk_widget_show( dlgTexReset.cbScaleVert );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.cbScaleVert, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );

	w = ui::Label( "New Vertical Scale: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editScaleVert = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editScaleVert ), "0.5" );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editScaleVert, 2, 3, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editScaleVert );

	// ---- /frame ----

	frame = ui::Frame( "Reset Shift" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbShiftHor = ui::CheckButton( "Enabled" );
	g_signal_connect( GTK_OBJECT( dlgTexReset.cbShiftHor ), "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	gtk_widget_show( dlgTexReset.cbShiftHor );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.cbShiftHor, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );

	w = ui::Label( "New Horizontal Shift: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editShiftHor = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editShiftHor ), "0" );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editShiftHor, 2, 3, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editShiftHor );


	dlgTexReset.cbShiftVert = ui::CheckButton( "Enabled" );
	g_signal_connect( GTK_OBJECT( dlgTexReset.cbShiftVert ), "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	gtk_widget_show( dlgTexReset.cbShiftVert );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.cbShiftVert, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );

	w = ui::Label( "New Vertical Shift: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editShiftVert = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editShiftVert ), "0" );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editShiftVert, 2, 3, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editShiftVert );

	// ---- /frame ----

	frame = ui::Frame( "Reset Rotation" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 1, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbRotation = ui::CheckButton( "Enabled" );
	gtk_widget_show( dlgTexReset.cbRotation );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.cbRotation, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );

	w = ui::Label( "New Rotation Value: " );
	gtk_table_attach( GTK_TABLE( table ), w, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	dlgTexReset.editRotation = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editRotation ), "0" );
	gtk_table_attach( GTK_TABLE( table ), dlgTexReset.editRotation, 2, 3, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( dlgTexReset.editRotation );

	// ---- /frame ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	w = ui::Button( "Use Selected Brushes" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Use All Brushes" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	Update_TextureReseter();

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret != eIDCANCEL ) {
			rs->bResetRotation =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbRotation ) );
			if ( rs->bResetRotation ) {
				if ( !ValidateTextInt( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editRotation ) ), "Rotation", &rs->rotation ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetScale[0] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleHor ) );
			if ( rs->bResetScale[0] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editScaleHor ) ), "Horizontal Scale", &rs->fScale[0] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetScale[1] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleVert ) );
			if ( rs->bResetScale[1] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editScaleVert ) ), "Vertical Scale", &rs->fScale[1] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetShift[0] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftHor ) );
			if ( rs->bResetShift[0] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editShiftHor ) ), "Horizontal Shift", &rs->fShift[0] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetShift[1] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftVert ) );
			if ( rs->bResetShift[1] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editShiftVert ) ), "Vertical Shift", &rs->fShift[1] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetTextureName =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbTexChange ) );
			if ( rs->bResetTextureName ) {
				strcpy( rs->textureName,     gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editTexOld ) ) );
				strcpy( rs->newTextureName,  gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editTexNew ) ) );
			}
		}
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

EMessageBoxReturn DoTrainThingBox( TrainThingRS* rs ){
	Str texSelected;

	GtkWidget *window, *w, *vbox, *hbox, *frame, *table;

	GtkWidget *radiusX, *radiusY;
	GtkWidget *angleStart, *angleEnd;
	GtkWidget *heightStart, *heightEnd;
	GtkWidget *numPoints;

	EMessageBoxReturn ret;
	int loop = 1;

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Train Thing" );
	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	gtk_object_set_data( GTK_OBJECT( window ), "loop", &loop );
	gtk_object_set_data( GTK_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	// ---- vbox ----

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- /hbox ----

	frame = ui::Frame( "Radii" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "X: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	radiusX = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( radiusX ), "100" );
	gtk_table_attach( GTK_TABLE( table ), radiusX, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( radiusX );



	w = ui::Label( "Y: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	radiusY = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( radiusY ), "100" );
	gtk_table_attach( GTK_TABLE( table ), radiusY, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( radiusY );



	frame = ui::Frame( "Angles" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "Start: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	angleStart = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( angleStart ), "0" );
	gtk_table_attach( GTK_TABLE( table ), angleStart, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( angleStart );



	w = ui::Label( "End: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	angleEnd = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( angleEnd ), "90" );
	gtk_table_attach( GTK_TABLE( table ), angleEnd, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( angleEnd );


	frame = ui::Frame( "Height" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "Start: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	heightStart = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( heightStart ), "0" );
	gtk_table_attach( GTK_TABLE( table ), heightStart, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( heightStart );



	w = ui::Label( "End: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	heightEnd = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( heightEnd ), "0" );
	gtk_table_attach( GTK_TABLE( table ), heightEnd, 1, 2, 1, 2,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( heightEnd );



	frame = ui::Frame( "Points" );
	gtk_widget_show( frame );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	gtk_widget_show( table );
	gtk_container_add( GTK_CONTAINER( frame ), table );
	gtk_table_set_row_spacings( GTK_TABLE( table ), 5 );
	gtk_table_set_col_spacings( GTK_TABLE( table ), 5 );
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "Number: " );
	gtk_table_attach( GTK_TABLE( table ), w, 0, 1, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( w );

	numPoints = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( numPoints ), "0" );
	gtk_table_attach( GTK_TABLE( table ), numPoints, 1, 2, 0, 1,
					  (GtkAttachOptions) ( GTK_FILL ),
					  (GtkAttachOptions) ( 0 ), 0, 0 );
	gtk_widget_show( numPoints );


	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 2 );
	gtk_widget_show( hbox );

	// ---- hbox ----

	w = ui::Button( "Ok" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );
	ret = eIDCANCEL;

	// ---- /hbox ----



	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret != eIDCANCEL ) {
			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( radiusX ) ), "Radius (X)", &rs->fRadiusX ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( radiusY ) ), "Radius (Y)", &rs->fRadiusY ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( angleStart ) ), "Angle (Start)", &rs->fStartAngle ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( angleEnd ) ), "Angle (End)", &rs->fEndAngle ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( heightStart ) ), "Height (Start)", &rs->fStartHeight ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( heightEnd ) ), "Height (End)", &rs->fEndHeight ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextInt( gtk_entry_get_text( GTK_ENTRY( numPoints ) ), "Num Points", &rs->iNumPoints ) ) {
				dialogError = TRUE;
			}
		}
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}
// ailmanki
// add a simple input for the MakeChain thing..
EMessageBoxReturn DoMakeChainBox( MakeChainRS* rs ){
	GtkWidget   *window, *w, *vbox, *hbox;
	GtkWidget   *textlinkNum, *textlinkName;
	EMessageBoxReturn ret;
	int loop = 1;

	const gchar    *text = "Please set a value in the boxes below and press 'OK' to make a chain";

	window = ui::Window( ui::window_type::TOP );

	g_signal_connect( GTK_OBJECT( window ), "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	g_signal_connect( GTK_OBJECT( window ), "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( GTK_WINDOW( window ), "Make Chain" );

	gtk_container_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	// new vbox
	vbox = ui::VBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );
	gtk_widget_show( vbox );

	hbox = ui::HBox( FALSE, 10 );
	gtk_container_add( GTK_CONTAINER( vbox ), hbox );
	gtk_widget_show( hbox );

	// dunno if you want this text or not ...
	w = ui::Label( text );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	w = gtk_hseparator_new();
	gtk_box_pack_start( GTK_BOX( vbox ), w, FALSE, FALSE, 0 );
	gtk_widget_show( w );

	// ------------------------- //

	// new hbox
	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textlinkNum = ui::Entry( 256 );
	gtk_box_pack_start( GTK_BOX( hbox ), textlinkNum, FALSE, FALSE, 1 );
	gtk_widget_show( textlinkNum );

	w = ui::Label( "Number of elements in chain" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 1 );
	gtk_widget_show( w );

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	gtk_widget_show( hbox );

	textlinkName = ui::Entry( 256 );
	gtk_box_pack_start( GTK_BOX( hbox ), textlinkName, FALSE, FALSE, 0 );
	gtk_widget_show( textlinkName );

	w = ui::Label( "Basename for chain's targetnames." );
	gtk_box_pack_start( GTK_BOX( hbox ), w, FALSE, FALSE, 1 );
	gtk_widget_show( w );


	w = ui::Button( "OK" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	gtk_widget_show( w );

	w = ui::Button( "Cancel" );
	gtk_box_pack_start( GTK_BOX( hbox ), w, TRUE, TRUE, 0 );
	g_signal_connect( GTK_OBJECT( w ), "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	gtk_widget_show( w );

	ret = eIDCANCEL;

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER );
	gtk_widget_show( window );
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDOK ) {
			strcpy( rs->linkName, gtk_entry_get_text( (GtkEntry*)textlinkName ) );
			if ( !ValidateTextInt( gtk_entry_get_text( (GtkEntry*)textlinkNum ), "Elements", &rs->linkNum ) ) {
				dialogError = TRUE;
			}
		}
	}

	gtk_grab_remove( window );
	gtk_widget_destroy( window );

	return ret;
}

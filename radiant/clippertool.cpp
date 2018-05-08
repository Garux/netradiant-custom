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

#include "clippertool.h"

#include "math/plane.h"
#include "csg.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"
#include "mainframe.h"
#include "camwindow.h"
#include "xywindow.h"
#include "gtkutil/cursor.h"

GdkCursor* g_clipper_cursor;

ClipperPoints g_clipper_points( g_vector3_identity, g_vector3_identity, g_vector3_identity );
bool g_clipper_flipped = false;

bool g_clipper_caulk = true;

bool Clipper_ok(){
	return GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip && plane3_valid( plane3_for_points( g_clipper_points._points ) );
}

ClipperPoints Clipper_getPlanePoints(){
	return g_clipper_flipped? ClipperPoints( g_clipper_points[0], g_clipper_points[2], g_clipper_points[1] ) : g_clipper_points;
}

void Clipper_update(){
	Scene_BrushSetClipPlane( GlobalSceneGraph(), Clipper_getPlanePoints() );
	SceneChangeNotify();
}

void Clipper_setPlanePoints( const ClipperPoints& points ){
	g_clipper_points = points;
	Clipper_update();
}

void Clipper_SelectionChanged( const Selectable& selectable ){
	if ( Clipper_ok() )
		Clipper_update();
}

void Clipper_modeChanged( bool isClipper ){
	GdkCursor* cursor = isClipper? g_clipper_cursor : 0;

	if( g_pParentWnd ){
		XYWnd* xywnd;
		if( ( xywnd = g_pParentWnd->GetXYWnd() ) )
			gdk_window_set_cursor( xywnd->GetWidget()->window, cursor );
		if( ( xywnd = g_pParentWnd->GetXZWnd() ) )
			gdk_window_set_cursor( xywnd->GetWidget()->window, cursor );
		if( ( xywnd = g_pParentWnd->GetYZWnd() ) )
			gdk_window_set_cursor( xywnd->GetWidget()->window, cursor );
		if( g_pParentWnd->GetCamWnd() )
			gdk_window_set_cursor( CamWnd_getWidget( *g_pParentWnd->GetCamWnd() )->window, cursor );
	}
}





void Clipper_do( bool split ){
	Scene_BrushSplitByPlane( GlobalSceneGraph(), Clipper_getPlanePoints(), g_clipper_caulk, split );
	GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eClip ); /* reset points this way */
}


void Clipper_doClip(){
	if ( Clipper_ok() ) {
		UndoableCommand undo( "clipperClip" );
		Clipper_do( false );
	}
}

void Clipper_doSplit(){
	if ( Clipper_ok() ) {
		UndoableCommand undo( "clipperSplit" );
		Clipper_do( true );
	}
}

void Clipper_doFlip(){
	if( Clipper_ok() ){
		g_clipper_flipped = !g_clipper_flipped;
		Clipper_update();
	}
}

#include "preferencesystem.h"
#include "stringio.h"
#include "preferences.h"
#include "commands.h"
#include "signal/isignal.h"
void Clipper_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Caulk Clipper Splits", g_clipper_caulk );
}
void Clipper_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Clipper", "Clipper Tool Settings" ) );
	Clipper_constructPreferences( page );
}
void Clipper_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, Clipper_constructPage>() );
}

void Clipper_registerCommands(){
	GlobalCommands_insert( "ClipperClip", FreeCaller<Clipper_doClip>(), Accelerator( GDK_Return ) );
	GlobalCommands_insert( "ClipperSplit", FreeCaller<Clipper_doSplit>(), Accelerator( GDK_Return, (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "ClipperFlip", FreeCaller<Clipper_doFlip>(), Accelerator( GDK_Return, (GdkModifierType)GDK_CONTROL_MASK ) );
}

void Clipper_Construct(){
	g_clipper_cursor = gdk_cursor_new( GDK_HAND2 );

	Clipper_registerCommands();
	GlobalPreferenceSystem().registerPreference( "ClipperCaulk", BoolImportStringCaller( g_clipper_caulk ), BoolExportStringCaller( g_clipper_caulk ) );
	Clipper_registerPreferencesPage();

	typedef FreeCaller1<const Selectable&, Clipper_SelectionChanged> ClipperSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( ClipperSelectionChangedCaller() );
}

void Clipper_Destroy(){
	gdk_cursor_unref( g_clipper_cursor );
}

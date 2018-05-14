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
bool g_clipper_quick = false;

/* preferences */
bool g_clipper_caulk = true;
bool g_clipper_resetFlip = true;
bool g_clipper_resetPoints = true;
bool g_clipper_2pointsIn2d = true;

bool Clipper_get2pointsIn2d(){
	return g_clipper_2pointsIn2d;
}

void ClipperModeQuick(){
	g_clipper_quick = true;
	ClipperMode(); //enable
}


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
//	globalOutputStream() << " Clipper_SelectionChanged\n";
	if ( Clipper_ok() )
		Clipper_update();
}
#include "gtkutil/idledraw.h"
void Clipper_BoundsChanged(){
//	globalOutputStream() << " Clipper_BoundsChanged\n";
	if ( Clipper_ok() )
		Clipper_update();
}

IdleDraw g_idle_clipper_update = IdleDraw( FreeCaller<Clipper_BoundsChanged>() );

void Clipper_BoundsChanged_Queue(){
	g_idle_clipper_update.queueDraw();
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
			if( !isClipper || gdk_pointer_is_grabbed() == FALSE ) /* prevent cursor change `GDK_BLANK_CURSOR->g_clipper_cursor` during freelook */
				gdk_window_set_cursor( CamWnd_getWidget( *g_pParentWnd->GetCamWnd() )->window, cursor );
	}

	if( g_clipper_resetFlip )
		g_clipper_flipped = false;
	if( !isClipper )
		g_clipper_quick = false;
}





void Clipper_do( bool split ){
	Scene_BrushSplitByPlane( GlobalSceneGraph(), Clipper_getPlanePoints(), g_clipper_caulk, split );
	if( g_clipper_resetPoints ){
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eClip ); /* reset points this way */
		if( g_clipper_resetFlip )
			g_clipper_flipped = false;
	}
	if( g_clipper_quick )
		ClipperMode(); //disable
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
	page.appendCheckBox( "", "Caulk Clipper Cuts", g_clipper_caulk );
	GtkWidget* resetFlip = page.appendCheckBox( "", "Reset Flipped State", g_clipper_resetFlip );
	GtkWidget* resetPoints = page.appendCheckBox( "", "Reset Points on Split", g_clipper_resetPoints );
	Widget_connectToggleDependency( resetFlip, resetPoints );
	page.appendCheckBox( "", "2 Points in 2D Views", g_clipper_2pointsIn2d );
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

SignalHandlerId ClipperTool_boundsChanged;

void Clipper_Construct(){
	g_clipper_cursor = gdk_cursor_new( GDK_HAND2 );

	Clipper_registerCommands();
	GlobalPreferenceSystem().registerPreference( "ClipperCaulk", BoolImportStringCaller( g_clipper_caulk ), BoolExportStringCaller( g_clipper_caulk ) );
	GlobalPreferenceSystem().registerPreference( "ClipperResetFlip", BoolImportStringCaller( g_clipper_resetFlip ), BoolExportStringCaller( g_clipper_resetFlip ) );
	GlobalPreferenceSystem().registerPreference( "ClipperResetPoints", BoolImportStringCaller( g_clipper_resetPoints ), BoolExportStringCaller( g_clipper_resetPoints ) );
	GlobalPreferenceSystem().registerPreference( "Clipper2PointsIn2D", BoolImportStringCaller( g_clipper_2pointsIn2d ), BoolExportStringCaller( g_clipper_2pointsIn2d ) );
	Clipper_registerPreferencesPage();

	typedef FreeCaller1<const Selectable&, Clipper_SelectionChanged> ClipperSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( ClipperSelectionChangedCaller() );

	ClipperTool_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback( FreeCaller<Clipper_BoundsChanged_Queue>() );
}

void Clipper_Destroy(){
	gdk_cursor_unref( g_clipper_cursor );
	GlobalSceneGraph().removeBoundsChangedCallback( ClipperTool_boundsChanged );
}

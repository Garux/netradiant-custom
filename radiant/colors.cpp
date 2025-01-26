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

#include "colors.h"

#include "ientity.h"
#include "ieclass.h"
#include "eclasslib.h"

#include "xywindow.h"
#include "camwindow.h"
#include "texwindow.h"
#include "mainframe.h"
#include "brushmodule.h"
#include "preferences.h"
#include "commands.h"
#include "gtkmisc.h"
#include "theme.h"



//! Make COLOR_BRUSHES override worldspawn eclass colour.
void SetWorldspawnColour( const Vector3& colour ){
	EntityClass* worldspawn = GlobalEntityClassManager().findOrInsert( "worldspawn", true );
	eclass_release_state( worldspawn );
	worldspawn->color = colour;
	eclass_capture_state( worldspawn );
}

void ColorScheme_Original(){
	TextureBrowser_setBackgroundColour( Vector3( 0.25f, 0.25f, 0.25f ) );

	g_camwindow_globals.color_selbrushes3d = Vector3( 1.0f, 0.0f, 0.0f );
	g_camwindow_globals.color_cameraback = Vector3( 0.25f, 0.25f, 0.25f );
	CamWnd_reconstructStatic();
	CamWnd_Update( *g_pParentWnd->GetCamWnd() );

	g_xywindow_globals.color_gridback = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_gridminor = Vector3( 0.75f, 0.75f, 0.75f );
	g_xywindow_globals.color_gridmajor = Vector3( 0.5f, 0.5f, 0.5f );
	g_xywindow_globals.color_gridblock = Vector3( 0.0f, 0.0f, 1.0f );
	g_xywindow_globals.color_gridtext = Vector3( 0.0f, 0.0f, 0.0f );
	g_xywindow_globals.color_selbrushes = Vector3( 1.0f, 0.0f, 0.0f );
	XYWnd::recaptureStates();
	g_xywindow_globals.color_clipper = Vector3( 0.0f, 0.0f, 1.0f );
	Brush_clipperColourChanged();
	g_xywindow_globals.color_brushes = Vector3( 0.0f, 0.0f, 0.0f );
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	g_xywindow_globals.color_viewname = Vector3( 0.5f, 0.0f, 0.75f );
	XY_UpdateAllWindows();
}

void ColorScheme_QER(){
	TextureBrowser_setBackgroundColour( Vector3( 0.25f, 0.25f, 0.25f ) );

	g_camwindow_globals.color_cameraback = Vector3( 0.25f, 0.25f, 0.25f );
	g_camwindow_globals.color_selbrushes3d = Vector3( 1.0f, 0.0f, 0.0f );
	CamWnd_reconstructStatic();
	CamWnd_Update( *g_pParentWnd->GetCamWnd() );

	g_xywindow_globals.color_gridback = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_gridminor = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_gridmajor = Vector3( 0.5f, 0.5f, 0.5f );
	g_xywindow_globals.color_gridblock = Vector3( 0.0f, 0.0f, 1.0f );
	g_xywindow_globals.color_gridtext = Vector3( 0.0f, 0.0f, 0.0f );
	g_xywindow_globals.color_selbrushes = Vector3( 1.0f, 0.0f, 0.0f );
	XYWnd::recaptureStates();
	g_xywindow_globals.color_clipper = Vector3( 0.0f, 0.0f, 1.0f );
	Brush_clipperColourChanged();
	g_xywindow_globals.color_brushes = Vector3( 0.0f, 0.0f, 0.0f );
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	g_xywindow_globals.color_viewname = Vector3( 0.5f, 0.0f, 0.75f );
	XY_UpdateAllWindows();
}

void ColorScheme_Black(){
	TextureBrowser_setBackgroundColour( Vector3( 0.25f, 0.25f, 0.25f ) );

	g_camwindow_globals.color_cameraback = Vector3( 0.25f, 0.25f, 0.25f );
	g_camwindow_globals.color_selbrushes3d = Vector3( 1.0f, 0.0f, 0.0f );
	CamWnd_reconstructStatic();
	CamWnd_Update( *g_pParentWnd->GetCamWnd() );

	g_xywindow_globals.color_gridback = Vector3( 0.0f, 0.0f, 0.0f );
	g_xywindow_globals.color_gridminor = Vector3( 0.2f, 0.2f, 0.2f );
	g_xywindow_globals.color_gridmajor = Vector3( 0.3f, 0.5f, 0.5f );
	g_xywindow_globals.color_gridblock = Vector3( 0.0f, 0.0f, 1.0f );
	g_xywindow_globals.color_gridtext = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_selbrushes = Vector3( 1.0f, 0.0f, 0.0f );
	XYWnd::recaptureStates();
	g_xywindow_globals.color_clipper = Vector3( 0.0f, 0.0f, 1.0f );
	Brush_clipperColourChanged();
	g_xywindow_globals.color_brushes = Vector3( 1.0f, 1.0f, 1.0f );
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	g_xywindow_globals.color_viewname = Vector3( 0.7f, 0.7f, 0.0f );
	XY_UpdateAllWindows();
}

/* ydnar: to emulate maya/max/lightwave color schemes */
void ColorScheme_Ydnar(){
	TextureBrowser_setBackgroundColour( Vector3( 0.25f, 0.25f, 0.25f ) );

	g_camwindow_globals.color_cameraback = Vector3( 0.25f, 0.25f, 0.25f );
	g_camwindow_globals.color_selbrushes3d = Vector3( 1.0f, 0.0f, 0.0f );
	CamWnd_reconstructStatic();
	CamWnd_Update( *g_pParentWnd->GetCamWnd() );

	g_xywindow_globals.color_gridback = Vector3( 0.77f, 0.77f, 0.77f );
	g_xywindow_globals.color_gridminor = Vector3( 0.83f, 0.83f, 0.83f );
	g_xywindow_globals.color_gridmajor = Vector3( 0.89f, 0.89f, 0.89f );
	g_xywindow_globals.color_gridblock = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_gridtext = Vector3( 0.0f, 0.0f, 0.0f );
	g_xywindow_globals.color_selbrushes = Vector3( 1.0f, 0.0f, 0.0f );
	XYWnd::recaptureStates();
	g_xywindow_globals.color_clipper = Vector3( 0.0f, 0.0f, 1.0f );
	Brush_clipperColourChanged();
	g_xywindow_globals.color_brushes = Vector3( 0.0f, 0.0f, 0.0f );
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	g_xywindow_globals.color_viewname = Vector3( 0.5f, 0.0f, 0.75f );
	XY_UpdateAllWindows();
}

void ColorScheme_Blender(){
	TextureBrowser_setBackgroundColour( Vector3( 0.25f, 0.25f, 0.25f ) );

	g_camwindow_globals.color_cameraback = Vector3( 0.25f, 0.25f, 0.25f );
	g_camwindow_globals.color_selbrushes3d = Vector3( 1.0f, 0.627451f, 0.0f );
	CamWnd_reconstructStatic();
	CamWnd_Update( *g_pParentWnd->GetCamWnd() );

	g_xywindow_globals.color_gridback = Vector3( .225803f, .225803f, .225803f );
	g_xywindow_globals.color_gridminor = Vector3( .254902f, .254902f, .254902f );
	g_xywindow_globals.color_gridmajor = Vector3( .301960f, .301960f, .301960f );
	g_xywindow_globals.color_gridblock = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_gridtext = Vector3( .972549f, .972549f, .972549f );
	g_xywindow_globals.color_selbrushes = Vector3( 1.0f, 0.627451f, 0.0f );
	XYWnd::recaptureStates();
	g_xywindow_globals.color_clipper = Vector3( 0.0f, 0.0f, 1.0f );
	Brush_clipperColourChanged();
	g_xywindow_globals.color_brushes = Vector3( 0.0f, 0.0f, 0.0f );
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	g_xywindow_globals.color_viewname = Vector3( 0.516136f, 0.516136f, 0.516136f );
	XY_UpdateAllWindows();
}

/* color scheme to fit the GTK Adwaita Dark theme */
void ColorScheme_AdwaitaDark()
{
	TextureBrowser_setBackgroundColour( Vector3( 0.25f, 0.25f, 0.25f ) );

	g_camwindow_globals.color_cameraback = Vector3( 0.25f, 0.25f, 0.25f );
	g_camwindow_globals.color_selbrushes3d = Vector3( 1.0f, 0.0f, 0.0f );
	CamWnd_reconstructStatic();
	CamWnd_Update( *g_pParentWnd->GetCamWnd() );

	g_xywindow_globals.color_gridback = Vector3( 0.25f, 0.25f, 0.25f );
	g_xywindow_globals.color_gridminor = Vector3( 0.21f, 0.23f, 0.23f );
	g_xywindow_globals.color_gridmajor = Vector3( 0.14f, 0.15f, 0.15f );
	g_xywindow_globals.color_gridblock = Vector3( 1.0f, 1.0f, 1.0f );
	g_xywindow_globals.color_gridtext = Vector3( 0.0f, 0.0f, 0.0f );
	g_xywindow_globals.color_selbrushes = Vector3( 1.0f, 0.0f, 0.0f );
	XYWnd::recaptureStates();
	g_xywindow_globals.color_clipper = Vector3( 0.0f, 0.0f, 1.0f );
	Brush_clipperColourChanged();
	g_xywindow_globals.color_brushes = Vector3( 0.73f, 0.73f, 0.73f );
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	g_xywindow_globals.color_viewname = Vector3( 0.5f, 0.0f, 0.75f );
	XY_UpdateAllWindows();
}

typedef Callback<Vector3()> GetColourCallback;
typedef Callback<void(const Vector3&)> SetColourCallback;

class ChooseColour
{
	GetColourCallback m_get;
	SetColourCallback m_set;
	QAction *m_action = nullptr;
	const char *m_menuName;
public:
	const char *m_commandName;
	const char *m_saveName;
	ChooseColour( const GetColourCallback& get, const SetColourCallback& set, const char *mnemonic, const char* commandName, const char* saveName )
	:	m_get ( get ),
		m_set ( set ),
		m_menuName ( mnemonic ),
		m_commandName ( commandName ),
		m_saveName ( saveName )
	{}
	void create_menu_item( QMenu *menu ){
		m_action = create_menu_item_with_mnemonic( menu, m_menuName, m_commandName );
		updateIcon();
	}
	void operator()() const {
		Vector3 colour = m_get();
		color_dialog( MainFrame_getWindow(), colour );
		m_set( colour );
		SceneChangeNotify();
		updateIcon( colour );
	}

	void updateIcon( const Vector3& colour ) const {
		QPixmap pixmap( QSize( 64, 64 ) ); // using larger pixmap, it gets downscaled
		pixmap.fill( QColor::fromRgbF( colour[0], colour[1], colour[2] ) );
		m_action->setIcon( QIcon( pixmap ) );
	}
	void updateIcon() const {
		updateIcon( m_get() );
	}
};



Vector3 Colour_get( const Vector3& colour ){
	return colour;
}
typedef ConstReferenceCaller<Vector3, Vector3(), Colour_get> ColourGetCaller;

void Colour_set( Vector3& colour, const Vector3& other ){
	colour = other;
}
typedef ReferenceCaller<Vector3, void(const Vector3&), Colour_set> ColourSetCaller;

void BrushColour_set( const Vector3& other ){
	SetWorldspawnColour( g_xywindow_globals.color_brushes = other );
}

void SelectedBrushColour_set( const Vector3& other ){
	g_xywindow_globals.color_selbrushes = other;
	XYWnd::recaptureStates();
}

void SelectedBrush3dColour_set( const Vector3& other ){
	g_camwindow_globals.color_selbrushes3d = other;
	CamWnd_reconstructStatic();
}

void ClipperColour_set( const Vector3& other ){
	g_xywindow_globals.color_clipper = other;
	Brush_clipperColourChanged();
}

Vector3 TextureBrowserColour_get(){
	return TextureBrowser_getBackgroundColour();
}


std::array g_ColoursMenu{
	ChooseColour( makeCallbackF  ( TextureBrowserColour_get )              , makeCallbackF( TextureBrowser_setBackgroundColour )    , "&Texture Background..."           , "ChooseTextureBackgroundColor"  , "SI_Colors0" ),
	ChooseColour( ColourGetCaller( g_camwindow_globals.color_cameraback )  , ColourSetCaller( g_camwindow_globals.color_cameraback ), "Camera Background..."             , "ChooseCameraBackgroundColor"   , "SI_Colors4" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridback )     , ColourSetCaller( g_xywindow_globals.color_gridback )   , "Grid Background..."               , "ChooseGridBackgroundColor"     , "SI_Colors1" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridmajor )    , ColourSetCaller( g_xywindow_globals.color_gridmajor )  , "Grid Major..."                    , "ChooseGridMajorColor"          , "SI_Colors3" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridminor )    , ColourSetCaller( g_xywindow_globals.color_gridminor )  , "Grid Minor..."                    , "ChooseGridMinorColor"          , "SI_Colors2" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridtext )     , ColourSetCaller( g_xywindow_globals.color_gridtext )   , "Grid Text..."                     , "ChooseGridTextColor"           , "SI_Colors7" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridblock )    , ColourSetCaller( g_xywindow_globals.color_gridblock )  , "Grid Block..."                    , "ChooseGridBlockColor"          , "SI_Colors6" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_brushes )      , makeCallbackF( BrushColour_set )                       , "Default Brush (2D)..."            , "ChooseBrushColor"              , "SI_Colors8" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_selbrushes )   , makeCallbackF( SelectedBrushColour_set )               , "Selected Brush and Sizing (2D)...", "ChooseSelectedBrushColor"      , "SI_Colors11" ),
	ChooseColour( ColourGetCaller( g_camwindow_globals.color_selbrushes3d ), makeCallbackF( SelectedBrush3dColour_set )             , "Selected Brush (Camera)..."       , "ChooseCameraSelectedBrushColor", "SI_Colors12" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_clipper )      , makeCallbackF( ClipperColour_set )                     , "Clipper..."                       , "ChooseClipperColor"            , "SI_Colors10" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_viewname )     , ColourSetCaller( g_xywindow_globals.color_viewname )   , "Active View Name and Outline..."  , "ChooseOrthoViewNameColor"      , "SI_Colors9" ),
};

void create_colours_menu( QMenu *menu ){
	menu = menu->addMenu( "Colors" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	{
		QMenu* submenu = menu->addMenu( "Viewports Theme" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "QE4 Original", "ColorSchemeOriginal" );
		create_menu_item_with_mnemonic( submenu, "Q3Radiant Original", "ColorSchemeQER" );
		create_menu_item_with_mnemonic( submenu, "Black and Green", "ColorSchemeBlackAndGreen" );
		create_menu_item_with_mnemonic( submenu, "Maya/Max/Lightwave Emulation", "ColorSchemeYdnar" );
		create_menu_item_with_mnemonic( submenu, "Blender/Dark", "ColorSchemeBlender" );
		create_menu_item_with_mnemonic( submenu, "Adwaita Dark", "ColorSchemeAdwaitaDark" );
	}

	theme_contruct_menu( menu );

	create_menu_item_with_mnemonic( menu, "OpenGL Font...", "OpenGLFont" );

	menu->addSeparator();

	for( auto& color : g_ColoursMenu )
		color.create_menu_item( menu );
}

void Colors_registerCommands(){
	GlobalCommands_insert( "ColorSchemeOriginal", FreeCaller<void(), ColorScheme_Original>() );
	GlobalCommands_insert( "ColorSchemeQER", FreeCaller<void(), ColorScheme_QER>() );
	GlobalCommands_insert( "ColorSchemeBlackAndGreen", FreeCaller<void(), ColorScheme_Black>() );
	GlobalCommands_insert( "ColorSchemeYdnar", FreeCaller<void(), ColorScheme_Ydnar>() );
	GlobalCommands_insert( "ColorSchemeBlender", FreeCaller<void(), ColorScheme_Blender>() );
	GlobalCommands_insert( "ColorSchemeAdwaitaDark", FreeCaller<void(), ColorScheme_AdwaitaDark>() );

	for( const auto& color : g_ColoursMenu )
		GlobalCommands_insert( color.m_commandName, ConstMemberCaller<ChooseColour, void(), &ChooseColour::operator()>( color ) );
}
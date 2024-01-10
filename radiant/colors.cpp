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

typedef Callback1<Vector3&> GetColourCallback;
typedef Callback1<const Vector3&> SetColourCallback;

class ChooseColour
{
	GetColourCallback m_get;
	SetColourCallback m_set;
public:
	ChooseColour( const GetColourCallback& get, const SetColourCallback& set )
		: m_get( get ), m_set( set ){
	}
	void operator()(){
		Vector3 colour;
		m_get( colour );
		color_dialog( MainFrame_getWindow(), colour );
		m_set( colour );
	}
};



void Colour_get( const Vector3& colour, Vector3& other ){
	other = colour;
}
typedef ConstReferenceCaller1<Vector3, Vector3&, Colour_get> ColourGetCaller;

void Colour_set( Vector3& colour, const Vector3& other ){
	colour = other;
	SceneChangeNotify();
}
typedef ReferenceCaller1<Vector3, const Vector3&, Colour_set> ColourSetCaller;

void BrushColour_set( const Vector3& other ){
	g_xywindow_globals.color_brushes = other;
	SetWorldspawnColour( g_xywindow_globals.color_brushes );
	SceneChangeNotify();
}
typedef FreeCaller1<const Vector3&, BrushColour_set> BrushColourSetCaller;

void SelectedBrushColour_set( const Vector3& other ){
	g_xywindow_globals.color_selbrushes = other;
	XYWnd::recaptureStates();
	SceneChangeNotify();
}
typedef FreeCaller1<const Vector3&, SelectedBrushColour_set> SelectedBrushColourSetCaller;

void SelectedBrush3dColour_set( const Vector3& other ){
	g_camwindow_globals.color_selbrushes3d = other;
	CamWnd_reconstructStatic();
	SceneChangeNotify();
}
typedef FreeCaller1<const Vector3&, SelectedBrush3dColour_set> SelectedBrush3dColourSetCaller;

void ClipperColour_set( const Vector3& other ){
	g_xywindow_globals.color_clipper = other;
	Brush_clipperColourChanged();
	SceneChangeNotify();
}
typedef FreeCaller1<const Vector3&, ClipperColour_set> ClipperColourSetCaller;

void TextureBrowserColour_get( Vector3& other ){
	other = TextureBrowser_getBackgroundColour();
}
typedef FreeCaller1<Vector3&, TextureBrowserColour_get> TextureBrowserColourGetCaller;

void TextureBrowserColour_set( const Vector3& other ){
	TextureBrowser_setBackgroundColour( other );
}
typedef FreeCaller1<const Vector3&, TextureBrowserColour_set> TextureBrowserColourSetCaller;


class ColoursMenu
{
public:
	ChooseColour m_textureback;
	ChooseColour m_xyback;
	ChooseColour m_gridmajor;
	ChooseColour m_gridminor;
	ChooseColour m_gridtext;
	ChooseColour m_gridblock;
	ChooseColour m_cameraback;
	ChooseColour m_brush;
	ChooseColour m_selectedbrush;
	ChooseColour m_selectedbrush3d;
	ChooseColour m_clipper;
	ChooseColour m_viewname;

	ColoursMenu() :
		m_textureback( TextureBrowserColourGetCaller(), TextureBrowserColourSetCaller() ),
		m_xyback( ColourGetCaller( g_xywindow_globals.color_gridback ), ColourSetCaller( g_xywindow_globals.color_gridback ) ),
		m_gridmajor( ColourGetCaller( g_xywindow_globals.color_gridmajor ), ColourSetCaller( g_xywindow_globals.color_gridmajor ) ),
		m_gridminor( ColourGetCaller( g_xywindow_globals.color_gridminor ), ColourSetCaller( g_xywindow_globals.color_gridminor ) ),
		m_gridtext( ColourGetCaller( g_xywindow_globals.color_gridtext ), ColourSetCaller( g_xywindow_globals.color_gridtext ) ),
		m_gridblock( ColourGetCaller( g_xywindow_globals.color_gridblock ), ColourSetCaller( g_xywindow_globals.color_gridblock ) ),
		m_cameraback( ColourGetCaller( g_camwindow_globals.color_cameraback ), ColourSetCaller( g_camwindow_globals.color_cameraback ) ),
		m_brush( ColourGetCaller( g_xywindow_globals.color_brushes ), BrushColourSetCaller() ),
		m_selectedbrush( ColourGetCaller( g_xywindow_globals.color_selbrushes ), SelectedBrushColourSetCaller() ),
		m_selectedbrush3d( ColourGetCaller( g_camwindow_globals.color_selbrushes3d ), SelectedBrush3dColourSetCaller() ),
		m_clipper( ColourGetCaller( g_xywindow_globals.color_clipper ), ClipperColourSetCaller() ),
		m_viewname( ColourGetCaller( g_xywindow_globals.color_viewname ), ColourSetCaller( g_xywindow_globals.color_viewname ) ){
	}
};

ColoursMenu g_ColoursMenu;

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

	create_menu_item_with_mnemonic( menu, "&Texture Background...", "ChooseTextureBackgroundColor" );
	create_menu_item_with_mnemonic( menu, "Camera Background...", "ChooseCameraBackgroundColor" );
	create_menu_item_with_mnemonic( menu, "Grid Background...", "ChooseGridBackgroundColor" );
	create_menu_item_with_mnemonic( menu, "Grid Major...", "ChooseGridMajorColor" );
	create_menu_item_with_mnemonic( menu, "Grid Minor...", "ChooseGridMinorColor" );
	create_menu_item_with_mnemonic( menu, "Grid Text...", "ChooseGridTextColor" );
	create_menu_item_with_mnemonic( menu, "Grid Block...", "ChooseGridBlockColor" );
	create_menu_item_with_mnemonic( menu, "Default Brush (2D)...", "ChooseBrushColor" );
	create_menu_item_with_mnemonic( menu, "Selected Brush and Sizing (2D)...", "ChooseSelectedBrushColor" );
	create_menu_item_with_mnemonic( menu, "Selected Brush (Camera)...", "ChooseCameraSelectedBrushColor" );
	create_menu_item_with_mnemonic( menu, "Clipper...", "ChooseClipperColor" );
	create_menu_item_with_mnemonic( menu, "Active View Name and Outline...", "ChooseOrthoViewNameColor" );
}

void Colors_registerCommands(){
	GlobalCommands_insert( "ColorSchemeOriginal", FreeCaller<ColorScheme_Original>() );
	GlobalCommands_insert( "ColorSchemeQER", FreeCaller<ColorScheme_QER>() );
	GlobalCommands_insert( "ColorSchemeBlackAndGreen", FreeCaller<ColorScheme_Black>() );
	GlobalCommands_insert( "ColorSchemeYdnar", FreeCaller<ColorScheme_Ydnar>() );
	GlobalCommands_insert( "ColorSchemeBlender", FreeCaller<ColorScheme_Blender>() );
	GlobalCommands_insert( "ColorSchemeAdwaitaDark", FreeCaller<ColorScheme_AdwaitaDark>() );
	GlobalCommands_insert( "ChooseTextureBackgroundColor", makeCallback( g_ColoursMenu.m_textureback ) );
	GlobalCommands_insert( "ChooseGridBackgroundColor", makeCallback( g_ColoursMenu.m_xyback ) );
	GlobalCommands_insert( "ChooseGridMajorColor", makeCallback( g_ColoursMenu.m_gridmajor ) );
	GlobalCommands_insert( "ChooseGridMinorColor", makeCallback( g_ColoursMenu.m_gridminor ) );
	GlobalCommands_insert( "ChooseGridTextColor", makeCallback( g_ColoursMenu.m_gridtext ) );
	GlobalCommands_insert( "ChooseGridBlockColor", makeCallback( g_ColoursMenu.m_gridblock ) );
	GlobalCommands_insert( "ChooseBrushColor", makeCallback( g_ColoursMenu.m_brush ) );
	GlobalCommands_insert( "ChooseCameraBackgroundColor", makeCallback( g_ColoursMenu.m_cameraback ) );
	GlobalCommands_insert( "ChooseSelectedBrushColor", makeCallback( g_ColoursMenu.m_selectedbrush ) );
	GlobalCommands_insert( "ChooseCameraSelectedBrushColor", makeCallback( g_ColoursMenu.m_selectedbrush3d ) );
	GlobalCommands_insert( "ChooseClipperColor", makeCallback( g_ColoursMenu.m_clipper ) );
	GlobalCommands_insert( "ChooseOrthoViewNameColor", makeCallback( g_ColoursMenu.m_viewname ) );
}
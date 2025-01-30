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
#include "modelwindow.h"
#include "render.h"
#include "mainframe.h"
#include "brushmodule.h"
#include "preferences.h"
#include "commands.h"
#include "gtkmisc.h"
#include "gtkutil/menu.h"
#include "os/dir.h"
#include "os/path.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "theme.h"

#define RAPIDJSON_PARSE_DEFAULT_FLAGS ( kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag )
#include "rapidjson/document.h"



//! Make COLOR_BRUSHES override worldspawn eclass colour.
void SetWorldspawnColour( const Vector3& colour ){
	EntityClass* worldspawn = GlobalEntityClassManager().findOrInsert( "worldspawn", true );
	eclass_release_state( worldspawn );
	worldspawn->color = colour;
	eclass_capture_state( worldspawn );
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
	const char *m_saveName;
	ChooseColour( const GetColourCallback& get, const SetColourCallback& set, const char *mnemonic, const char* saveName )
	:	m_get ( get ),
		m_set ( set ),
		m_menuName ( mnemonic ),
		m_saveName ( saveName )
	{}
	void create_menu_item( QMenu *menu ){
		m_action = create_menu_item_with_mnemonic( menu, m_menuName, ConstMemberCaller<ChooseColour, void(), &ChooseColour::operator()>( *this ) );
		updateIcon();
	}
	void operator()() const {
		Vector3 colour = m_get();
		color_dialog( MainFrame_getWindow(), colour );
		setColour( colour );
	}

	void setColour( const Vector3& colour ) const {
		m_set( colour );
		SceneChangeNotify();
		updateIcon( colour );
	}
private:
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


Vector3 Colour4b_get( const Colour4b& colour ){
	return Vector3( colour.r, colour.g, colour.b ) / 255.f;
}
typedef ConstReferenceCaller<Colour4b, Vector3(), Colour4b_get> Colour4bGetCaller;

void Colour4b_set( Colour4b& colour, const Vector3& other ){
	colour = Colour4b( other[0] * 255.f, other[1] * 255.f, other[2] * 255.f, colour.a );
}
typedef ReferenceCaller<Colour4b, void(const Vector3&), Colour4b_set> Colour4bSetCaller;


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

Vector3 ModelBrowserColour_get(){
	return ModelBrowser_getBackgroundColour();
}


std::array g_ColoursMenu{
	ChooseColour( makeCallbackF  ( TextureBrowserColour_get )              , makeCallbackF( TextureBrowser_setBackgroundColour )    , "Texture Browser Background..."    , "ColorTexBroBackground" ),
	ChooseColour( makeCallbackF  ( ModelBrowserColour_get )                , makeCallbackF( ModelBrowser_setBackgroundColour )      , "Model Browser Background..."      , "ColorModBroBackground" ),
	ChooseColour( ColourGetCaller( g_camwindow_globals.color_cameraback )  , ColourSetCaller( g_camwindow_globals.color_cameraback ), "Camera Background..."             , "ColorCameraBackground" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridback )     , ColourSetCaller( g_xywindow_globals.color_gridback )   , "Grid Background..."               , "ColorGridBackground" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridmajor )    , ColourSetCaller( g_xywindow_globals.color_gridmajor )  , "Grid Major..."                    , "ColorGridMajor" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridminor )    , ColourSetCaller( g_xywindow_globals.color_gridminor )  , "Grid Minor..."                    , "ColorGridMinor" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridtext )     , ColourSetCaller( g_xywindow_globals.color_gridtext )   , "Grid Text..."                     , "ColorGridText" ), // coords and inactive name
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_gridblock )    , ColourSetCaller( g_xywindow_globals.color_gridblock )  , "Grid _blocksize Blocks..."        , "ColorGridBlocks" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_brushes )      , makeCallbackF( BrushColour_set )                       , "Grid Worldspawn..."               , "ColorGridWorldspawn" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_selbrushes )   , makeCallbackF( SelectedBrushColour_set )               , "Grid Selection and Sizing..."     , "ColorGridSelection" ),
	ChooseColour( ColourGetCaller( g_camwindow_globals.color_selbrushes3d ), makeCallbackF( SelectedBrush3dColour_set )             , "Camera Selection..."              , "ColorCameraSelection" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_clipper )      , makeCallbackF( ClipperColour_set )                     , "Clipper Split..."                 , "ColorClipperSplit" ),
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_viewname )     , ColourSetCaller( g_xywindow_globals.color_viewname )   , "Active Grid Name and Outline..."  , "ColorGridActive" ), // also grid stats
	ChooseColour( ColourGetCaller( g_xywindow_globals.color_camera )       , ColourSetCaller( g_xywindow_globals.color_camera )     , "Camera Icon..."                   , "ColorCameraIcon" ),
	ChooseColour( Colour4bGetCaller( g_colour_x )                          , Colour4bSetCaller( g_colour_x )                        , "Axis X..."                        , "ColorAxisX" ),
	ChooseColour( Colour4bGetCaller( g_colour_y )                          , Colour4bSetCaller( g_colour_y )                        , "Axis Y..."                        , "ColorAxisY" ),
	ChooseColour( Colour4bGetCaller( g_colour_z )                          , Colour4bSetCaller( g_colour_z )                        , "Axis Z..."                        , "ColorAxisZ" ),
};

static void load_colors_theme( const char *filepath ){
	TextFileInputStream file( filepath );
	if( file.failed() ){
		globalErrorStream() << "File " << makeQuoted( filepath ) << " reading failed.\n";
		return;
	}

	StringOutputStream str( 4096 );
	str.c_str()[ file.read( str.c_str(), 4096 - 1 ) ] = '\0';

	rapidjson::Document doc;
	doc.Parse( str.c_str() );
	if( doc.HasParseError() ){
		globalErrorStream() << "File " << makeQuoted( filepath ) << " parsing failed.\n";
		return;
	}

	for( const auto& colour : g_ColoursMenu ){
		const auto it = doc.GetObj().FindMember( colour.m_saveName );
		if( it == doc.GetObj().MemberEnd() ){
			globalWarningStream() << makeQuoted( colour.m_saveName ) << " not found in file " << makeQuoted( filepath ) << '\n';
		}
		else if( !it->value.IsArray() ){
			globalWarningStream() << makeQuoted( colour.m_saveName ) << " is not an array in file " << makeQuoted( filepath ) << '\n';
		}
		else if( it->value.GetArray().Size() != 3 ){
			globalWarningStream() << makeQuoted( colour.m_saveName ) << " array.size != 3 in file " << makeQuoted( filepath ) << '\n';
		}
		else{
			Vector3 clr( 0 );
			for( size_t i = 0; i != 3; ++i )
				clr[i] = it->value.GetArray().operator[]( i ).Get<float>();
			colour.setColour( clr );
		}
	}
}

void create_colours_menu( QMenu *menu ){
	menu = menu->addMenu( "Colors" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	{
		QMenu* submenu = menu->addMenu( "Viewports Theme" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		const auto path = StringStream( AppPath_get(), "themes/_colors/" );

		Directory_forEach( path, matchFileExtension( "json", [&]( const char *name ){
			submenu->addAction( StringStream<64>( PathExtensionless( name ) ).c_str(), [path = CopiedString( StringStream( path, name ) )](){
				load_colors_theme( path.c_str() );
			} );
		}));

	}

	theme_contruct_menu( menu );

	create_menu_item_with_mnemonic( menu, "OpenGL Font...", "OpenGLFont" );

	menu->addSeparator();

	for( auto& color : g_ColoursMenu )
		color.create_menu_item( menu );
}

void Colors_registerCommands(){
}
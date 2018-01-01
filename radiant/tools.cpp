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

#include "tools.h"

#include "iscenegraph.h"
#include "iselection.h"
#include "mainframe.h"
#include "commands.h"
#include "generic/callback.h"
#include "signal/isignal.h"
#include "gtkutil/widget.h"


void ModeChangeNotify(){
	SceneChangeNotify();
}

typedef void ( *ToolMode )();
ToolMode g_currentToolMode = 0;
bool g_currentToolModeSupportsComponentEditing = false;
ToolMode g_defaultToolMode = 0;



void SelectionSystem_DefaultMode(){
	GlobalSelectionSystem().SetMode( SelectionSystem::ePrimitive );
	GlobalSelectionSystem().SetComponentMode( SelectionSystem::eDefault );
	ModeChangeNotify();
}


bool EdgeMode(){
	return GlobalSelectionSystem().Mode() == SelectionSystem::eComponent
	    && GlobalSelectionSystem().ComponentMode() == SelectionSystem::eEdge;
}

bool VertexMode(){
	return GlobalSelectionSystem().Mode() == SelectionSystem::eComponent
	    && GlobalSelectionSystem().ComponentMode() == SelectionSystem::eVertex;
}

bool FaceMode(){
	return GlobalSelectionSystem().Mode() == SelectionSystem::eComponent
	    && GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace;
}

template<bool( *BoolFunction ) ( )>
class BoolFunctionExport
{
public:
	static void apply( const BoolImportCallback& importCallback ){
		importCallback( BoolFunction() );
	}
};

typedef FreeCaller<void(const BoolImportCallback&), &BoolFunctionExport<EdgeMode>::apply> EdgeModeApplyCaller;
EdgeModeApplyCaller g_edgeMode_button_caller;
BoolExportCallback g_edgeMode_button_callback( g_edgeMode_button_caller );
ToggleItem g_edgeMode_button( g_edgeMode_button_callback );

typedef FreeCaller<void(const BoolImportCallback&), &BoolFunctionExport<VertexMode>::apply> VertexModeApplyCaller;
VertexModeApplyCaller g_vertexMode_button_caller;
BoolExportCallback g_vertexMode_button_callback( g_vertexMode_button_caller );
ToggleItem g_vertexMode_button( g_vertexMode_button_callback );

typedef FreeCaller<void(const BoolImportCallback&), &BoolFunctionExport<FaceMode>::apply> FaceModeApplyCaller;
FaceModeApplyCaller g_faceMode_button_caller;
BoolExportCallback g_faceMode_button_callback( g_faceMode_button_caller );
ToggleItem g_faceMode_button( g_faceMode_button_callback );

void ComponentModeChanged(){
	g_edgeMode_button.update();
	g_vertexMode_button.update();
	g_faceMode_button.update();
}

void ComponentMode_SelectionChanged( const Selectable& selectable ){
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent
	  && GlobalSelectionSystem().countSelected() == 0 ) {
		SelectionSystem_DefaultMode();
		ComponentModeChanged();
	}
}

void SelectEdgeMode(){
#if 0
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent ) {
		GlobalSelectionSystem().Select( false );
	}
#endif

	if ( EdgeMode() ) {
		SelectionSystem_DefaultMode();
	}
	else if ( GlobalSelectionSystem().countSelected() != 0 ) {
		if ( !g_currentToolModeSupportsComponentEditing ) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode( SelectionSystem::eComponent );
		GlobalSelectionSystem().SetComponentMode( SelectionSystem::eEdge );
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void SelectVertexMode(){
#if 0
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent ) {
		GlobalSelectionSystem().Select( false );
	}
#endif

	if ( VertexMode() ) {
		SelectionSystem_DefaultMode();
	}
	else if ( GlobalSelectionSystem().countSelected() != 0 ) {
		if ( !g_currentToolModeSupportsComponentEditing ) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode( SelectionSystem::eComponent );
		GlobalSelectionSystem().SetComponentMode( SelectionSystem::eVertex );
	}

	ComponentModeChanged();

	ModeChangeNotify();
}

void SelectFaceMode(){
#if 0
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent ) {
		GlobalSelectionSystem().Select( false );
	}
#endif

	if ( FaceMode() ) {
		SelectionSystem_DefaultMode();
	}
	else if ( GlobalSelectionSystem().countSelected() != 0 ) {
		if ( !g_currentToolModeSupportsComponentEditing ) {
			g_defaultToolMode();
		}

		GlobalSelectionSystem().SetMode( SelectionSystem::eComponent );
		GlobalSelectionSystem().SetComponentMode( SelectionSystem::eFace );
	}

	ComponentModeChanged();

	ModeChangeNotify();
}



void TranslateToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eTranslate );
}

void RotateToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eRotate );
}

void ScaleToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eScale );
}

void SkewToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eSkew );
}

void DragToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eDrag );
}

void ClipperToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eClip );
}

void BuildToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eBuild );
}

void UVToolExport( const BoolImportCallback& importCallback ){
	importCallback( GlobalSelectionSystem().ManipulatorMode() == SelectionSystem::eUV );
}

FreeCaller<void(const BoolImportCallback&), TranslateToolExport> g_translatemode_button_caller;
BoolExportCallback g_translatemode_button_callback( g_translatemode_button_caller );
ToggleItem g_translatemode_button( g_translatemode_button_callback );

FreeCaller<void(const BoolImportCallback&), RotateToolExport> g_rotatemode_button_caller;
BoolExportCallback g_rotatemode_button_callback( g_rotatemode_button_caller );
ToggleItem g_rotatemode_button( g_rotatemode_button_callback );

FreeCaller<void(const BoolImportCallback&), ScaleToolExport> g_scalemode_button_caller;
BoolExportCallback g_scalemode_button_callback( g_scalemode_button_caller );
ToggleItem g_scalemode_button( g_scalemode_button_callback );

FreeCaller<void(const BoolImportCallback&), SkewToolExport> g_skewmode_button_caller;
BoolExportCallback g_skewmode_button_callback( g_skewmode_button_caller );
ToggleItem g_skewmode_button( g_skewmode_button_callback );

FreeCaller<void(const BoolImportCallback&), DragToolExport> g_dragmode_button_caller;
BoolExportCallback g_dragmode_button_callback( g_dragmode_button_caller );
ToggleItem g_dragmode_button( g_dragmode_button_callback );

FreeCaller<void(const BoolImportCallback&), ClipperToolExport> g_clipper_button_caller;
BoolExportCallback g_clipper_button_callback( g_clipper_button_caller );
ToggleItem g_clipper_button( g_clipper_button_callback );

FreeCaller<void(const BoolImportCallback&), BuildToolExport> g_build_button_caller;
BoolExportCallback g_build_button_callback( g_build_button_caller );
ToggleItem g_build_button( g_build_button_callback );

FreeCaller<void(const BoolImportCallback&), UVToolExport> g_uv_button_caller;
BoolExportCallback g_uv_button_callback( g_uv_button_caller );
ToggleItem g_uv_button( g_uv_button_callback );

void ToolChanged(){
	g_translatemode_button.update();
	g_rotatemode_button.update();
	g_scalemode_button.update();
	g_skewmode_button.update();
	g_dragmode_button.update();
	g_clipper_button.update();
	g_build_button.update();
	g_uv_button.update();
}

constexpr char c_ResizeMode_status[] = "QE4 Drag Tool: move and resize objects";

void DragMode(){
	if ( g_currentToolMode == DragMode && g_defaultToolMode != DragMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = DragMode;
		g_currentToolModeSupportsComponentEditing = true;

		Sys_Status( c_ResizeMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eDrag );
		ToolChanged();
		ModeChangeNotify();
	}
}


constexpr char c_TranslateMode_status[] = "Translate Tool: translate objects and components";

void TranslateMode(){
	if ( g_currentToolMode == TranslateMode && g_defaultToolMode != TranslateMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = TranslateMode;
		g_currentToolModeSupportsComponentEditing = true;

		Sys_Status( c_TranslateMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eTranslate );
		ToolChanged();
		ModeChangeNotify();
	}
}

constexpr char c_RotateMode_status[] = "Rotate Tool: rotate objects and components";

void RotateMode(){
	if ( g_currentToolMode == RotateMode && g_defaultToolMode != RotateMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = RotateMode;
		g_currentToolModeSupportsComponentEditing = true;

		Sys_Status( c_RotateMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eRotate );
		ToolChanged();
		ModeChangeNotify();
	}
}

constexpr char c_ScaleMode_status[] = "Scale Tool: scale objects and components";

void ScaleMode(){
	if ( g_currentToolMode == ScaleMode && g_defaultToolMode != ScaleMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = ScaleMode;
		g_currentToolModeSupportsComponentEditing = true;

		Sys_Status( c_ScaleMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eScale );
		ToolChanged();
		ModeChangeNotify();
	}
}

constexpr char c_SkewMode_status[] = "Transform Tool: transform objects and components";

void SkewMode(){
	if ( g_currentToolMode == SkewMode && g_defaultToolMode != SkewMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = SkewMode;
		g_currentToolModeSupportsComponentEditing = true;

		Sys_Status( c_SkewMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eSkew );
		ToolChanged();
		ModeChangeNotify();
	}
}


constexpr char c_ClipperMode_status[] = "Clipper Tool: apply clip planes to brushes";

void ClipperMode(){
	if ( g_currentToolMode == ClipperMode && g_defaultToolMode != ClipperMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = ClipperMode;
		g_currentToolModeSupportsComponentEditing = false;

		SelectionSystem_DefaultMode();
		ComponentModeChanged();

		Sys_Status( c_ClipperMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eClip );
		ToolChanged();
		ModeChangeNotify();
	}
}


constexpr char c_BuildMode_status[] = "Build Tool: extrude, build chains, clone";

void BuildMode(){
	if ( g_currentToolMode == BuildMode && g_defaultToolMode != BuildMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = BuildMode;
		g_currentToolModeSupportsComponentEditing = false;

		SelectionSystem_DefaultMode();
		ComponentModeChanged();

		Sys_Status( c_BuildMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eBuild );
		ToolChanged();
		ModeChangeNotify();
	}
}


constexpr char c_UVMode_status[] = "UV Tool: edit texture alignment";

void UVMode(){
	if ( g_currentToolMode == UVMode && g_defaultToolMode != UVMode ) {
		g_defaultToolMode();
	}
	else
	{
		g_currentToolMode = UVMode;
		g_currentToolModeSupportsComponentEditing = false;

		SelectionSystem_DefaultMode();
		ComponentModeChanged();

		Sys_Status( c_UVMode_status );
		GlobalSelectionSystem().SetManipulatorMode( SelectionSystem::eUV );
		ToolChanged();
		ModeChangeNotify();
	}
}


void ToggleRotateScaleModes(){
	return g_currentToolMode == RotateMode? ScaleMode() : RotateMode();
}

void ToggleDragSkewModes(){
	return g_currentToolMode == DragMode? SkewMode() : DragMode();
}



void Tools_registerCommands(){
	GlobalToggles_insert( "DragVertices", makeCallbackF( SelectVertexMode ), ToggleItem::AddCallbackCaller( g_vertexMode_button ), QKeySequence( "V" ) );
	GlobalToggles_insert( "DragEdges", makeCallbackF( SelectEdgeMode ), ToggleItem::AddCallbackCaller( g_edgeMode_button ), QKeySequence( "E" ) );
	GlobalToggles_insert( "DragFaces", makeCallbackF( SelectFaceMode ), ToggleItem::AddCallbackCaller( g_faceMode_button ), QKeySequence( "F" ) );

	GlobalToggles_insert( "ToggleClipper", makeCallbackF( ClipperMode ), ToggleItem::AddCallbackCaller( g_clipper_button ), QKeySequence( "X" ) );

	GlobalToggles_insert( "MouseTranslate", makeCallbackF( TranslateMode ), ToggleItem::AddCallbackCaller( g_translatemode_button ), QKeySequence( "W" ) );
	GlobalToggles_insert( "MouseRotate", makeCallbackF( RotateMode ), ToggleItem::AddCallbackCaller( g_rotatemode_button ), QKeySequence( "R" ) );
	GlobalToggles_insert( "MouseScale", makeCallbackF( ScaleMode ), ToggleItem::AddCallbackCaller( g_scalemode_button ) );
	GlobalToggles_insert( "MouseTransform", makeCallbackF( SkewMode ), ToggleItem::AddCallbackCaller( g_skewmode_button ) );
	GlobalToggles_insert( "MouseDrag", makeCallbackF( DragMode ), ToggleItem::AddCallbackCaller( g_dragmode_button ) );
	GlobalToggles_insert( "MouseBuild", makeCallbackF( BuildMode ), ToggleItem::AddCallbackCaller( g_build_button ), QKeySequence( "B" ) );
	GlobalToggles_insert( "MouseUV", makeCallbackF( UVMode ), ToggleItem::AddCallbackCaller( g_uv_button ), QKeySequence( "G" ) );
	GlobalCommands_insert( "MouseRotateOrScale", makeCallbackF( ToggleRotateScaleModes ) );
	GlobalCommands_insert( "MouseDragOrTransform", makeCallbackF( ToggleDragSkewModes ), QKeySequence( "Q" ) );

	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), ComponentMode_SelectionChanged>() );

	g_defaultToolMode = DragMode;
	g_defaultToolMode();
}
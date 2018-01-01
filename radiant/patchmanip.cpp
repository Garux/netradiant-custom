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

#include "patchmanip.h"

#include "debugging/debugging.h"


#include "iselection.h"
#include "ipatch.h"

#include "math/vector.h"
#include "math/aabb.h"
#include "generic/callback.h"

#include "gtkutil/menu.h"
#include "gtkutil/image.h"
#include "map.h"
#include "mainframe.h"
#include "commands.h"
#include "gtkmisc.h"
#include "gtkdlgs.h"
#include "texwindow.h"
#include "xywindow.h"
#include "select.h"
#include "patch.h"
#include "grid.h"
#include "patchdialog.h"

PatchCreator* g_patchCreator = 0;

void Scene_PatchConstructPrefab( scene::Graph& graph, const AABB aabb, const char* shader, EPatchPrefab eType, int axis, std::size_t width = 3, std::size_t height = 3, bool redisperse = false ){
	Select_Delete();
	GlobalSelectionSystem().setSelectedAll( false );

	NodeSmartReference node( g_patchCreator->createPatch() );
	Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->insert( node );

	Patch* patch = Node_getPatch( node );
	patch->SetShader( shader );

	patch->ConstructPrefab( aabb, eType, axis, width, height );
	if( redisperse ){
		patch->Redisperse( COL );
		patch->Redisperse( ROW );
	}
	patch->controlPointsChanged();

	{
		scene::Path patchpath( makeReference( GlobalSceneGraph().root() ) );
		patchpath.push( makeReference( *Map_GetWorldspawn( g_map ) ) );
		patchpath.push( makeReference( node.get() ) );
		Instance_getSelectable( *graph.find( patchpath ) )->setSelected( true );
	}
}


void Patch_makeCaps( Patch& patch, scene::Instance& instance, EPatchCap type, const char* shader ){
	if ( ( type == EPatchCap::EndCap || type == EPatchCap::IEndCap )
	     && patch.getWidth() != 5 ) {
		globalErrorStream() << "cannot create end-cap - patch width != 5\n";
		return;
	}
	if ( ( type == EPatchCap::Bevel || type == EPatchCap::IBevel )
	     && patch.getWidth() != 3 && patch.getWidth() != 5 ) {
		globalErrorStream() << "cannot create bevel-cap - patch width != 3\n";
		return;
	}
	if ( type == EPatchCap::Cylinder
	     && patch.getWidth() != 9 ) {
		globalErrorStream() << "cannot create cylinder-cap - patch width != 9\n";
		return;
	}

	{
		NodeSmartReference cap( g_patchCreator->createPatch() );
		Node_getTraversable( instance.path().parent() )->insert( cap );

		patch.MakeCap( Node_getPatch( cap ), type, ROW, true );
		Node_getPatch( cap )->SetShader( shader );

		scene::Path path( instance.path() );
		path.pop();
		path.push( makeReference( cap.get() ) );
		selectPath( path, true );
	}

	{
		NodeSmartReference cap( g_patchCreator->createPatch() );
		Node_getTraversable( instance.path().parent() )->insert( cap );

		patch.MakeCap( Node_getPatch( cap ), type, ROW, false );
		Node_getPatch( cap )->SetShader( shader );

		scene::Path path( instance.path() );
		path.pop();
		path.push( makeReference( cap.get() ) );
		selectPath( path, true );
	}
}


typedef std::vector<scene::Instance*> InstanceVector;

class PatchStoreInstance
{
	InstanceVector& m_instances;
public:
	PatchStoreInstance( InstanceVector& instances ) : m_instances( instances ){
	}
	void operator()( PatchInstance& patch ) const {
		m_instances.push_back( &patch );
	}
};

void Scene_PatchDoCap_Selected( scene::Graph& graph, const char* shader, EPatchCap type ){
	InstanceVector instances;
	Scene_forEachVisibleSelectedPatchInstance( PatchStoreInstance( instances ) );
	for ( auto i : instances )
	{
		Patch_makeCaps( *Node_getPatch( i->path().top() ), *i, type, shader );
	}
}

void Patch_deform( Patch& patch, scene::Instance& instance, const int deform, const int axis ){
	patch.undoSave();

	for ( PatchControlIter i = patch.begin(); i != patch.end(); ++i ){
		PatchControl& control = *i;
		int randomNumber = int( deform * ( float( std::rand() ) / float( RAND_MAX ) ) );
		control.m_vertex[ axis ] += randomNumber;
	}

	patch.controlPointsChanged();
}

void Scene_PatchDeform( scene::Graph& graph, const int deform, const int axis )
{
	InstanceVector instances;
	Scene_forEachVisibleSelectedPatchInstance( PatchStoreInstance( instances ) );
	for ( auto i : instances )
	{
		Patch_deform( *Node_getPatch( i->path().top() ), *i, deform, axis );
	}

}

void Patch_thicken( Patch& patch, scene::Instance& instance, const float thickness, bool seams, const int axis ){

	const auto aabb_small = []( const AABB& aabb ){
		return ( aabb.extents[0] < 0.01 && aabb.extents[1] < 0.01 ) ||
		       ( aabb.extents[1] < 0.01 && aabb.extents[2] < 0.01 ) ||
		       ( aabb.extents[0] < 0.01 && aabb.extents[2] < 0.01 );
	};

	// Create a new patch node
	NodeSmartReference node( g_patchCreator->createPatch() );
	// Insert the node into original's entity
	Node_getTraversable( instance.path().parent() )->insert( node );

	// Retrieve the contained patch from the node
	Patch* targetPatch = Node_getPatch( node );

	// Create the opposite patch with the given thickness = distance
	bool no12 = true;
	bool no34 = true;
	targetPatch->createThickenedOpposite( patch, thickness, axis, no12, no34 );

	{ // Now select the newly created patch
		scene::Path path( instance.parent()->path() );
		path.push( makeReference( node.get() ) );
		selectPath( path, true );
	}

	if( seams && thickness != 0.0f ){
		int i = no12? 2 : 0;
		int iend = no34? 2 : 4;
		// Now create the four walls
		for ( ; i < iend; ++i ){
			// Allocate new patch
			NodeSmartReference node = NodeSmartReference( g_patchCreator->createPatch() );
			// Insert each node into worldspawn
			Node_getTraversable( instance.path().parent() )->insert( node );

			// Retrieve the contained patch from the node
			Patch* wallPatch = Node_getPatch( node );

			// Create the wall patch by passing i as wallIndex
			wallPatch->createThickenedWall( patch, *targetPatch, i );

			if( aabb_small( wallPatch->localAABB() ) ){
				//globalOutputStream() << "Thicken: Discarding degenerate patch.\n";
				Node_getTraversable( instance.path().parent() )->erase( node );
			}
			else { // Now select the newly created patch
				scene::Path path( instance.parent()->path() );
				path.push( makeReference( node.get() ) );
				selectPath( path, true );
			}
		}
	}

	// Invert the target patch so that it faces the opposite direction
	targetPatch->InvertMatrix();

	if( aabb_small( targetPatch->localAABB() ) ){
		//globalOutputStream() << "Thicken: Discarding degenerate patch.\n";
		Node_getTraversable( instance.path().parent() )->erase( node );
	}
}

void Scene_PatchThicken( scene::Graph& graph, const int thickness, bool seams, const int axis )
{
	InstanceVector instances;
	Scene_forEachVisibleSelectedPatchInstance( PatchStoreInstance( instances ) );
	for ( auto i : instances )
	{
		Patch_thicken( *Node_getPatch( i->path().top() ), *i, thickness, seams, axis );
	}

}

Patch* Scene_GetUltimateSelectedVisiblePatch(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		scene::Node& node = GlobalSelectionSystem().ultimateSelected().path().top();
		if ( node.visible() ) {
			return Node_getPatch( node );
		}
	}
	return 0;
}


void Scene_PatchCapTexture_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( []( Patch& patch ){ patch.CapTexture(); } );
	SceneChangeNotify();
}

void Scene_PatchProjectTexture_Selected( scene::Graph& graph, const texdef_t& texdef, const Vector3* direction ){
	Scene_forEachVisibleSelectedPatch( [texdef, direction]( Patch& patch ){ patch.ProjectTexture( texdef, direction ); } );
	SceneChangeNotify();
}

void Scene_PatchProjectTexture_Selected( scene::Graph& graph, const TextureProjection& projection, const Vector3& normal ){
	Scene_forEachVisibleSelectedPatch( [projection, normal]( Patch& patch ){ patch.ProjectTexture( projection, normal ); } );
	SceneChangeNotify();
}

void Scene_PatchFlipTexture_Selected( scene::Graph& graph, int axis ){
	Scene_forEachVisibleSelectedPatch( [axis]( Patch& patch ){ patch.FlipTexture( axis ); } );
}

void Scene_PatchNaturalTexture_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( []( Patch& patch ){ patch.NaturalTexture(); } );
	SceneChangeNotify();
}

void Scene_PatchTileTexture_Selected( scene::Graph& graph, float s, float t ){
	Scene_forEachVisibleSelectedPatch( [s, t]( Patch& patch ){ patch.SetTextureRepeat( s, t ); } );
	SceneChangeNotify();
}


void Scene_PatchInsertRemove_Selected( scene::Graph& graph, bool bInsert, bool bColumn, bool bFirst ){
	Scene_forEachVisibleSelectedPatch( [bInsert, bColumn, bFirst]( Patch& patch ){ patch.InsertRemove( bInsert, bColumn, bFirst ); } );
}

void Scene_PatchInvert_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( []( Patch& patch ){ patch.InvertMatrix(); } );
}

void Scene_PatchRedisperse_Selected( scene::Graph& graph, EMatrixMajor major ){
	Scene_forEachVisibleSelectedPatch( [major]( Patch& patch ){ patch.Redisperse( major ); } );
}

void Scene_PatchSmooth_Selected( scene::Graph& graph, EMatrixMajor major ){
	Scene_forEachVisibleSelectedPatch( [major]( Patch& patch ){ patch.Smooth( major ); } );
}

void Scene_PatchTranspose_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( []( Patch& patch ){ patch.TransposeMatrix(); } );
}

void Scene_PatchSetShader_Selected( scene::Graph& graph, const char* name ){
	Scene_forEachVisibleSelectedPatch( [name]( Patch& patch ){ patch.SetShader( name ); } );
	SceneChangeNotify();
}

void Scene_PatchGetShader_Selected( scene::Graph& graph, CopiedString& name ){
	if ( Patch* patch = Scene_GetUltimateSelectedVisiblePatch() )
		name = patch->GetShader();
}

class PatchSelectByShader
{
	const char* m_name;
public:
	inline PatchSelectByShader( const char* name )
		: m_name( name ){
	}
	void operator()( PatchInstance& patch ) const {
		if ( shader_equal( patch.getPatch().GetShader(), m_name ) ) {
			patch.setSelected( true );
		}
	}
};

void Scene_PatchSelectByShader( scene::Graph& graph, const char* name ){
	Scene_forEachVisiblePatchInstance( PatchSelectByShader( name ) );
}


class PatchFindReplaceShader
{
	const char* m_find;
	const char* m_replace;
public:
	PatchFindReplaceShader( const char* find, const char* replace ) : m_find( find ), m_replace( replace ){
	}
	void operator()( Patch& patch ) const {
		if ( shader_equal( patch.GetShader(), m_find ) ) {
			patch.SetShader( m_replace );
		}
	}
};

void Scene_PatchFindReplaceShader( scene::Graph& graph, const char* find, const char* replace ){
	if( !replace ){
		Scene_forEachVisiblePatchInstance( PatchSelectByShader( find ) );
	}
	else{
		Scene_forEachVisiblePatch( PatchFindReplaceShader( find, replace ) );
	}
}

void Scene_PatchFindReplaceShader_Selected( scene::Graph& graph, const char* find, const char* replace ){
	if( !replace ){
		//do nothing, because alternative is replacing to notex
		//perhaps deselect ones with not matching shaders here?
	}
	else{
		Scene_forEachVisibleSelectedPatch( PatchFindReplaceShader( find, replace ) );
	}
}


AABB PatchCreator_getBounds(){
	AABB aabb( aabb_for_minmax( Select_getWorkZone().d_work_min, Select_getWorkZone().d_work_max ) );

	float gridSize = GetGridSize();

	if ( aabb.extents[0] == 0 ) {
		aabb.extents[0] = gridSize;
	}
	if ( aabb.extents[1] == 0 ) {
		aabb.extents[1] = gridSize;
	}
	if ( aabb.extents[2] == 0 ) {
		aabb.extents[2] = gridSize;
	}

	if ( aabb_valid( aabb ) ) {
		return aabb;
	}
	return AABB( Vector3( 0, 0, 0 ), Vector3( 64, 64, 64 ) );
}

void DoNewPatchDlg( EPatchPrefab prefab, int minrows, int mincols, int defrows, int defcols, int maxrows, int maxcols );

void Patch_XactCylinder(){
	UndoableCommand undo( "patchCreateXactCylinder" );

	DoNewPatchDlg( EPatchPrefab::ExactCylinder, 3, 7, 3, 13, 0, 0 );
}

void Patch_XactSphere(){
	UndoableCommand undo( "patchCreateXactSphere" );

	DoNewPatchDlg( EPatchPrefab::ExactSphere, 5, 7, 7, 13, 0, 0 );
}

void Patch_XactCone(){
	UndoableCommand undo( "patchCreateXactCone" );

	DoNewPatchDlg( EPatchPrefab::ExactCone, 3, 7, 3, 13, 0, 0 );
}

void Patch_Cylinder(){
	UndoableCommand undo( "patchCreateCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::Cylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_DenseCylinder(){
	UndoableCommand undo( "patchCreateDenseCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::DenseCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_VeryDenseCylinder(){
	UndoableCommand undo( "patchCreateVeryDenseCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::VeryDenseCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_SquareCylinder(){
	UndoableCommand undo( "patchCreateSquareCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::SqCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Endcap(){
	UndoableCommand undo( "patchCreateEndCap" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::EndCap, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Bevel(){
	UndoableCommand undo( "patchCreateBevel" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::Bevel, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Sphere(){
	UndoableCommand undo( "patchCreateSphere" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::Sphere, GlobalXYWnd_getCurrentViewType() );
}

void Patch_SquareBevel(){
}

void Patch_SquareEndcap(){
}

void Patch_Cone(){
	UndoableCommand undo( "patchCreateCone" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), EPatchPrefab::Cone, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Plane(){
	UndoableCommand undo( "patchCreatePlane" );

	DoNewPatchDlg( EPatchPrefab::Plane, 3, 3, 3, 3, 0, 0 );
}

void Patch_InsertFirstColumn(){
	UndoableCommand undo( "patchInsertFirstColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, true, true );
}

void Patch_InsertLastColumn(){
	UndoableCommand undo( "patchInsertLastColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, true, false );
}

void Patch_InsertFirstRow(){
	UndoableCommand undo( "patchInsertFirstRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, false, true );
}

void Patch_InsertLastRow(){
	UndoableCommand undo( "patchInsertLastRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, false, false );
}

void Patch_DeleteFirstColumn(){
	UndoableCommand undo( "patchDeleteFirstColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, true, true );
}

void Patch_DeleteLastColumn(){
	UndoableCommand undo( "patchDeleteLastColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, true, false );
}

void Patch_DeleteFirstRow(){
	UndoableCommand undo( "patchDeleteFirstRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, false, true );
}

void Patch_DeleteLastRow(){
	UndoableCommand undo( "patchDeleteLastRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, false, false );
}

void Patch_Invert(){
	UndoableCommand undo( "patchInvert" );

	Scene_PatchInvert_Selected( GlobalSceneGraph() );
}

void Patch_RedisperseRows(){
	UndoableCommand undo( "patchRedisperseRows" );

	Scene_PatchRedisperse_Selected( GlobalSceneGraph(), ROW );
}

void Patch_RedisperseCols(){
	UndoableCommand undo( "patchRedisperseColumns" );

	Scene_PatchRedisperse_Selected( GlobalSceneGraph(), COL );
}

void Patch_SmoothRows(){
	UndoableCommand undo( "patchSmoothRows" );

	Scene_PatchSmooth_Selected( GlobalSceneGraph(), ROW );
}

void Patch_SmoothCols(){
	UndoableCommand undo( "patchSmoothColumns" );

	Scene_PatchSmooth_Selected( GlobalSceneGraph(), COL );
}

void Patch_Transpose(){
	UndoableCommand undo( "patchTranspose" );

	Scene_PatchTranspose_Selected( GlobalSceneGraph() );
}

void DoCapDlg();

void Patch_Cap(){
	// FIXME: add support for patch cap creation
	// Patch_CapCurrent();
	UndoableCommand undo( "patchPutCaps" );

	DoCapDlg();
}

///\todo Unfinished.
void Patch_OverlayOn(){
}

///\todo Unfinished.
void Patch_OverlayOff(){
}

void Patch_FlipTextureX(){
	UndoableCommand undo( "patchFlipTextureU" );

	Scene_PatchFlipTexture_Selected( GlobalSceneGraph(), 0 );
}

void Patch_FlipTextureY(){
	UndoableCommand undo( "patchFlipTextureV" );

	Scene_PatchFlipTexture_Selected( GlobalSceneGraph(), 1 );
}

void Patch_NaturalTexture(){
	UndoableCommand undo( "patchNaturalTexture" );

	Scene_PatchNaturalTexture_Selected( GlobalSceneGraph() );
}

void Patch_CapTexture(){
	UndoableCommand command( "patchCapTexture" );
	Scene_PatchCapTexture_Selected( GlobalSceneGraph() );
}

void Patch_FitTexture(){
}

void DoPatchDeformDlg();

void Patch_Deform(){
	UndoableCommand undo( "patchDeform" );

	DoPatchDeformDlg();
}

void DoPatchThickenDlg();

void Patch_Thicken(){
	UndoableCommand undo( "patchThicken" );

	DoPatchThickenDlg();
}



#include "ifilter.h"


class filter_patch_all : public PatchFilter
{
public:
	bool filter( const Patch& patch ) const {
		return true;
	}
};

class filter_patch_shader : public PatchFilter
{
	const char* m_shader;
public:
	filter_patch_shader( const char* shader ) : m_shader( shader ){
	}
	bool filter( const Patch& patch ) const {
		return shader_equal( patch.GetShader(), m_shader );
	}
};

class filter_patch_flags : public PatchFilter
{
	int m_flags;
public:
	filter_patch_flags( int flags ) : m_flags( flags ){
	}
	bool filter( const Patch& patch ) const {
		return ( patch.getShaderFlags() & m_flags ) != 0;
	}
};


filter_patch_all g_filter_patch_all;
filter_patch_flags g_filter_patch_clip( QER_CLIP );
filter_patch_shader g_filter_patch_commonclip( "textures/common/clip" );
filter_patch_shader g_filter_patch_weapclip( "textures/common/weapclip" );
filter_patch_flags g_filter_patch_translucent( QER_TRANS | QER_ALPHATEST );

void PatchFilters_construct(){
	add_patch_filter( g_filter_patch_all, EXCLUDE_CURVES );
	add_patch_filter( g_filter_patch_clip, EXCLUDE_CLIP );
	add_patch_filter( g_filter_patch_commonclip, EXCLUDE_CLIP );
	add_patch_filter( g_filter_patch_weapclip, EXCLUDE_CLIP );
	add_patch_filter( g_filter_patch_translucent, EXCLUDE_TRANSLUCENT );
}


#include "preferences.h"

void Patch_constructPreferences( PreferencesPage& page ){
	page.appendSpinner( "Patch Subdivide Threshold", g_PatchSubdivideThreshold, 0, 128 );
}
void Patch_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Patches", "Patch Display Preferences" ) );
	Patch_constructPreferences( page );
}
void Patch_registerPreferencesPage(){
	PreferencesDialog_addDisplayPage( makeCallbackF( Patch_constructPage ) );
}


#include "preferencesystem.h"

void PatchPreferences_construct(){
	GlobalPreferenceSystem().registerPreference( "Subdivisions", IntImportStringCaller( g_PatchSubdivideThreshold ), IntExportStringCaller( g_PatchSubdivideThreshold ) );
}


#include "generic/callback.h"

void Patch_registerCommands(){
	GlobalCommands_insert( "InvertCurveTextureX", makeCallbackF( Patch_FlipTextureX ), QKeySequence( "Ctrl+Shift+I" ) );
	GlobalCommands_insert( "InvertCurveTextureY", makeCallbackF( Patch_FlipTextureY ), QKeySequence( "Shift+I" ) );
	GlobalCommands_insert( "NaturalizePatch", makeCallbackF( Patch_NaturalTexture ), QKeySequence( "Ctrl+N" ) );
	GlobalCommands_insert( "PatchCylinder", makeCallbackF( Patch_Cylinder ) );
//	GlobalCommands_insert( "PatchDenseCylinder", makeCallbackF( Patch_DenseCylinder ) );
//	GlobalCommands_insert( "PatchVeryDenseCylinder", makeCallbackF( Patch_VeryDenseCylinder ) );
	GlobalCommands_insert( "PatchSquareCylinder", makeCallbackF( Patch_SquareCylinder ) );
	GlobalCommands_insert( "PatchXactCylinder", makeCallbackF( Patch_XactCylinder ) );
	GlobalCommands_insert( "PatchXactSphere", makeCallbackF( Patch_XactSphere ) );
	GlobalCommands_insert( "PatchXactCone", makeCallbackF( Patch_XactCone ) );
	GlobalCommands_insert( "PatchEndCap", makeCallbackF( Patch_Endcap ) );
	GlobalCommands_insert( "PatchBevel", makeCallbackF( Patch_Bevel ) );
//	GlobalCommands_insert( "PatchSquareBevel", makeCallbackF( Patch_SquareBevel ) );
//	GlobalCommands_insert( "PatchSquareEndcap", makeCallbackF( Patch_SquareEndcap ) );
	GlobalCommands_insert( "PatchCone", makeCallbackF( Patch_Cone ) );
	GlobalCommands_insert( "PatchSphere", makeCallbackF( Patch_Sphere ) );
	GlobalCommands_insert( "SimplePatchMesh", makeCallbackF( Patch_Plane ), QKeySequence( "Shift+P" ) );
	GlobalCommands_insert( "PatchInsertFirstColumn", makeCallbackF( Patch_InsertFirstColumn ), QKeySequence( Qt::CTRL + Qt::SHIFT + Qt::Key_Plus + Qt::KeypadModifier ) );
	GlobalCommands_insert( "PatchInsertLastColumn", makeCallbackF( Patch_InsertLastColumn ) );
	GlobalCommands_insert( "PatchInsertFirstRow", makeCallbackF( Patch_InsertFirstRow ), QKeySequence( Qt::CTRL + Qt::Key_Plus + Qt::KeypadModifier ) );
	GlobalCommands_insert( "PatchInsertLastRow", makeCallbackF( Patch_InsertLastRow ) );
	GlobalCommands_insert( "PatchDeleteFirstColumn", makeCallbackF( Patch_DeleteFirstColumn ) );
	GlobalCommands_insert( "PatchDeleteLastColumn", makeCallbackF( Patch_DeleteLastColumn ), QKeySequence( Qt::CTRL + Qt::SHIFT + Qt::Key_Minus + Qt::KeypadModifier ) );
	GlobalCommands_insert( "PatchDeleteFirstRow", makeCallbackF( Patch_DeleteFirstRow ) );
	GlobalCommands_insert( "PatchDeleteLastRow", makeCallbackF( Patch_DeleteLastRow ), QKeySequence( Qt::CTRL + Qt::Key_Minus + Qt::KeypadModifier ) );
	GlobalCommands_insert( "InvertCurve", makeCallbackF( Patch_Invert ), QKeySequence( "Ctrl+I" ) );
	//GlobalCommands_insert( "RedisperseRows", makeCallbackF( Patch_RedisperseRows ), QKeySequence( "Ctrl+E" ) );
	GlobalCommands_insert( "RedisperseRows", makeCallbackF( Patch_RedisperseRows ) );
	//GlobalCommands_insert( "RedisperseCols", makeCallbackF( Patch_RedisperseCols ), QKeySequence( "Ctrl+Shift+E" ) );
	GlobalCommands_insert( "RedisperseCols", makeCallbackF( Patch_RedisperseCols ) );
	GlobalCommands_insert( "SmoothRows", makeCallbackF( Patch_SmoothRows ), QKeySequence( "Ctrl+W" ) );
	GlobalCommands_insert( "SmoothCols", makeCallbackF( Patch_SmoothCols ), QKeySequence( "Ctrl+Shift+W" ) );
	GlobalCommands_insert( "MatrixTranspose", makeCallbackF( Patch_Transpose ), QKeySequence( "Ctrl+Shift+M" ) );
	GlobalCommands_insert( "CapCurrentCurve", makeCallbackF( Patch_Cap ), QKeySequence( "Shift+C" ) );
//	GlobalCommands_insert( "MakeOverlayPatch", makeCallbackF( Patch_OverlayOn ), QKeySequence( "Y" ) );
//	GlobalCommands_insert( "ClearPatchOverlays", makeCallbackF( Patch_OverlayOff ), QKeySequence( "Ctrl+L" ) );
	GlobalCommands_insert( "PatchDeform", makeCallbackF( Patch_Deform ) );
	GlobalCommands_insert( "PatchThicken", makeCallbackF( Patch_Thicken ), QKeySequence( "Ctrl+T" ) );
}

void Patch_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Put caps on the current patch", "curve_cap.png", "CapCurrentCurve" );
}

void Patch_constructMenu( QMenu* menu ){
	create_menu_item_with_mnemonic( menu, "Simple Patch Mesh...", "SimplePatchMesh" );
	create_menu_item_with_mnemonic( menu, "Bevel", "PatchBevel" );
	create_menu_item_with_mnemonic( menu, "End cap", "PatchEndCap" );
	create_menu_item_with_mnemonic( menu, "Cylinder (9x3)", "PatchCylinder" );
	create_menu_item_with_mnemonic( menu, "Square Cylinder (9x3)", "PatchSquareCylinder" );
	create_menu_item_with_mnemonic( menu, "Exact Cylinder...", "PatchXactCylinder" );
	create_menu_item_with_mnemonic( menu, "Cone (9x3)", "PatchCone" );
	create_menu_item_with_mnemonic( menu, "Exact Cone...", "PatchXactCone" );
	create_menu_item_with_mnemonic( menu, "Sphere (9x5)", "PatchSphere" );
	create_menu_item_with_mnemonic( menu, "Exact Sphere...", "PatchXactSphere" );
//	{
//		QMenu* submenu = menu->addMenu( "More Cylinders" );

//		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

//		create_menu_item_with_mnemonic( submenu, "Dense Cylinder", "PatchDenseCylinder" );
//		create_menu_item_with_mnemonic( submenu, "Very Dense Cylinder", "PatchVeryDenseCylinder" );
//		create_menu_item_with_mnemonic( submenu, "Square Cylinder", "PatchSquareCylinder" );
//	}
//	{
//		//not implemented
//		create_menu_item_with_mnemonic( menu, "Square Endcap", "PatchSquareBevel" );
//		create_menu_item_with_mnemonic( menu, "Square Bevel", "PatchSquareEndcap" );
//	}
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Cap Selection", "CapCurrentCurve" );
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Insert/Delete" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Insert (2) First Columns", "PatchInsertFirstColumn" );
		create_menu_item_with_mnemonic( submenu, "Insert (2) Last Columns", "PatchInsertLastColumn" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Insert (2) First Rows", "PatchInsertFirstRow" );
		create_menu_item_with_mnemonic( submenu, "Insert (2) Last Rows", "PatchInsertLastRow" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Del First (2) Columns", "PatchDeleteFirstColumn" );
		create_menu_item_with_mnemonic( submenu, "Del Last (2) Columns", "PatchDeleteLastColumn" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Del First (2) Rows", "PatchDeleteFirstRow" );
		create_menu_item_with_mnemonic( submenu, "Del Last (2) Rows", "PatchDeleteLastRow" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Matrix" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Invert", "InvertCurve" );
		create_menu_item_with_mnemonic( submenu, "Transpose", "MatrixTranspose" );

		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Re-disperse Rows", "RedisperseRows" );
		create_menu_item_with_mnemonic( submenu, "Re-disperse Columns", "RedisperseCols" );

		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Smooth Rows", "SmoothRows" );
		create_menu_item_with_mnemonic( submenu, "Smooth Columns", "SmoothCols" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Texture" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Reset Texture", "TextureReset/Cap" );
		create_menu_item_with_mnemonic( submenu, "Naturalize", "NaturalizePatch" );
		create_menu_item_with_mnemonic( submenu, "Invert X", "InvertCurveTextureX" );
		create_menu_item_with_mnemonic( submenu, "Invert Y", "InvertCurveTextureY" );

	}
//	menu->addSeparator();
//	{ //unfinished
//		QMenu* submenu = menu->addMenu( "Overlay" );

//		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

//		create_menu_item_with_mnemonic( submenu, "Set", "MakeOverlayPatch" );
//		create_menu_item_with_mnemonic( submenu, "Clear", "ClearPatchOverlays" );
//	}
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Deform...", "PatchDeform" );
	create_menu_item_with_mnemonic( menu, "Thicken...", "PatchThicken" );
}


#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include "gtkutil/spinbox.h"

#include <QDialog>
#include "gtkutil/combobox.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QButtonGroup>
#include <QRadioButton>

void DoNewPatchDlg( EPatchPrefab prefab, int minrows, int mincols, int defrows, int defcols, int maxrows, int maxcols ){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Patch density" );

	auto width = new ComboBox;
	auto height = new ComboBox;
	auto redisperseCheckBox = new QCheckBox( "Square" );

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			{
#define D_ITEM( x ) if ( x >= mincols && ( !maxcols || x <= maxcols ) ) width->addItem( # x )
				D_ITEM( 3 );
				D_ITEM( 5 );
				D_ITEM( 7 );
				D_ITEM( 9 );
				D_ITEM( 11 );
				D_ITEM( 13 );
				D_ITEM( 15 );
				D_ITEM( 17 );
				D_ITEM( 19 );
				D_ITEM( 21 );
				D_ITEM( 23 );
				D_ITEM( 25 );
				D_ITEM( 27 );
				D_ITEM( 29 );
				D_ITEM( 31 ); // MAX_PATCH_SIZE is 32, so we should be able to do 31...
#undef D_ITEM
				form->addRow( "Width:", width );
			}
			{
#define D_ITEM( x ) if ( x >= minrows && ( !maxrows || x <= maxrows ) ) height->addItem( # x )
				D_ITEM( 3 );
				D_ITEM( 5 );
				D_ITEM( 7 );
				D_ITEM( 9 );
				D_ITEM( 11 );
				D_ITEM( 13 );
				D_ITEM( 15 );
				D_ITEM( 17 );
				D_ITEM( 19 );
				D_ITEM( 21 );
				D_ITEM( 23 );
				D_ITEM( 25 );
				D_ITEM( 27 );
				D_ITEM( 29 );
				D_ITEM( 31 ); // MAX_PATCH_SIZE is 32, so we should be able to do 31...
#undef D_ITEM
				form->addRow( "Height:", height );
			}

			if( prefab != EPatchPrefab::Plane ){
				redisperseCheckBox->setToolTip( "Redisperse columns & rows" );
				form->addWidget( redisperseCheckBox );
			}
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	// Initialize dialog
	width->setCurrentIndex( ( defcols - mincols ) / 2 );
	height->setCurrentIndex( ( defrows - minrows ) / 2 );

	if ( dialog.exec() ) {
		const int w = width->currentIndex() * 2 + mincols;
		const int h = height->currentIndex() * 2 + minrows;
		const bool redisperse = redisperseCheckBox->isChecked();
		Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), prefab, GlobalXYWnd_getCurrentViewType(), w, h, redisperse );
	}
}


void DoPatchDeformDlg(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Patch deform" );

	auto spin = new SpinBox( -9999, 9999, 64 );

	RadioHBox radioBox = RadioHBox_new( (const char*[]){ "X", "Y", "Z" } );
	radioBox.m_radio->button( 2 )->setChecked( true );

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		form->addRow( new SpinBoxLabel( "Max deform:", spin ), spin );
		form->addRow( "", radioBox.m_hbox );
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if ( dialog.exec() ) {
		const int deform = spin->value();
		const int axis = radioBox.m_radio->checkedId();
		Scene_PatchDeform( GlobalSceneGraph(), deform, axis );
	}
}



void DoCapDlg(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Cap" );

	auto group = new QButtonGroup( &dialog );
	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			const char* iconlabel[][2] = { { "cap_bevel.png", "Bevel" },
			                               { "cap_endcap.png", "Endcap" },
			                               { "cap_ibevel.png", "Inverted Bevel" },
			                               { "cap_iendcap.png", "Inverted Endcap" },
			                               { "cap_cylinder.png", "Cylinder" } };
			for( size_t i = 0; i < std::size( iconlabel ); ++i ){
				const auto [ stricon, strlabel ] = iconlabel[i];
				auto label = new QLabel;
				label->setPixmap( new_local_image( stricon ) );
				auto button = new QRadioButton( strlabel );
				group->addButton( button, i ); // set ids 0+, default ones are negative
				form->addRow( label, button );
			}
			for( int i = 0; i < form->count(); ++i )
				form->itemAt( i )->setAlignment( Qt::AlignmentFlag::AlignVCenter );
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	// Initialize dialog
	group->button( 0 )->setChecked( true );

	if( dialog.exec() ){
		Scene_PatchDoCap_Selected( GlobalSceneGraph(), TextureBrowser_GetSelectedShader(), static_cast<EPatchCap>( group->checkedId() ) );
	}
}


void DoPatchThickenDlg(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Patch thicken" );

	const int grid = std::max( GetGridSize(), 1.f );
	auto spin = new SpinBox( -9999, 9999, grid, 2, grid );

	RadioHBox radioBox = RadioHBox_new( (const char*[]){ "X", "Y", "Z", "Normal" } );
	radioBox.m_radio->button( 3 )->setChecked( true );

	auto check = new QCheckBox( "Side walls" );
	check->setChecked( true );

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		form->addRow( new SpinBoxLabel( "Thickness:", spin ), spin );
		form->addRow( "", radioBox.m_hbox );
		form->addWidget( check );
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if ( dialog.exec() ) {
		const int thickness = spin->value();
		const bool seams = check->isChecked();
		const int axis = radioBox.m_radio->checkedId(); // 3 == Extrude along normals

		Scene_PatchThicken( GlobalSceneGraph(), thickness, seams, axis );
	}
}




class PatchTexdefConstructor
{
public:
	brushprimit_texdef_t m_bp;
	Matrix4 m_local2tex;
	Matrix4 m_tex2local;
	Plane3 m_plane;
	// rip from UVManipulator::UpdateFaceData
	PatchTexdefConstructor( Patch *patch ){
		m_plane.normal() = patch->Calculate_AvgNormal();
		m_plane.dist() = vector3_dot( m_plane.normal(), patch->localAABB().origin );
		const size_t patchWidth = patch->getWidth();
		const size_t patchHeight = patch->getHeight();
		{	//! todo force or deduce orthogonal uv axes for convenience
			Vector3 wDir, hDir;
			patch->Calculate_AvgAxes( wDir, hDir );
			vector3_normalise( wDir );
			vector3_normalise( hDir );
//					globalOutputStream() << wDir << " wDir\n";
//					globalOutputStream() << hDir << " hDir\n";
//					globalOutputStream() << m_plane.normal() << " m_plane.normal()\n";

			/* find longest row and column */
			float wLength = 0, hLength = 0; //!? todo break, if some of these is 0
			std::size_t row = 0, col = 0;
			for ( std::size_t r = 0; r < patchHeight; ++r ){
				float length = 0;
				for ( std::size_t c = 0; c < patchWidth - 1; ++c ){
					length += vector3_length( patch->ctrlAt( r, c + 1 ).m_vertex - patch->ctrlAt( r, c ).m_vertex );
				}
				if( length - wLength > .1f || ( ( r == 0 || r == patchHeight - 1 ) && float_equal_epsilon( length, wLength, .1f ) ) ){ // prioritize first and last rows
					wLength = length;
					row = r;
				}
			}
			for ( std::size_t c = 0; c < patchWidth; ++c ){
				float length = 0;
				for ( std::size_t r = 0; r < patchHeight - 1; ++r ){
					length += vector3_length( patch->ctrlAt( r + 1, c ).m_vertex - patch->ctrlAt( r, c ).m_vertex );
				}
				if( length - hLength > .1f || ( ( c == 0 || c == patchWidth - 1 ) && float_equal_epsilon( length, hLength, .1f ) ) ){
					hLength = length;
					col = c;
				}
			}
			//! todo handle case, when uv start = end, like projection to cylinder
			//! todo consider max uv length to have manipulator size according to patch size
			/* pick 3 points at the found row and column */
			const PatchControl* p0, *p1, *p2;
			Vector3 v0, v1, v2;
			{
				float distW0 = 0, distW1 = 0;
				for ( std::size_t c = 0; c < col; ++c ){
					distW0 += vector3_length( patch->ctrlAt( row, c + 1 ).m_vertex - patch->ctrlAt( row, c ).m_vertex );
				}
				for ( std::size_t c = col; c < patchWidth - 1; ++c ){
					distW1 += vector3_length( patch->ctrlAt( row, c + 1 ).m_vertex - patch->ctrlAt( row, c ).m_vertex );
				}
				float distH0 = 0, distH1 = 0;
				for ( std::size_t r = 0; r < row; ++r ){
					distH0 += vector3_length( patch->ctrlAt( r + 1, col ).m_vertex - patch->ctrlAt( r, col ).m_vertex );
				}
				for ( std::size_t r = row; r < patchHeight - 1; ++r ){
					distH1 += vector3_length( patch->ctrlAt( r + 1, col ).m_vertex - patch->ctrlAt( r, col ).m_vertex );
				}

				if( ( distW0 > distH0 && distW0 > distH1 ) || ( distW1 > distH0 && distW1 > distH1 ) ){
					p0 = &patch->ctrlAt( 0, col );
					p1 = &patch->ctrlAt( patchHeight - 1, col );
					p2 = distW0 > distW1? &patch->ctrlAt( row, 0 ) : &patch->ctrlAt( row, patchWidth - 1 );
					v0 = p0->m_vertex; //! the altered line, we want realistic offset values
					v1 = v0 + hDir * hLength;
					v2 = v0 + hDir * distH0 + ( distW0 > distW1? ( wDir * -distW0 ) : ( wDir * distW1 ) );
				}
				else{
					p0 = &patch->ctrlAt( row, 0 );
					p1 = &patch->ctrlAt( row, patchWidth - 1 );
					p2 = distH0 > distH1? &patch->ctrlAt( 0, col ) : &patch->ctrlAt( patchHeight - 1, col );
					v0 = p0->m_vertex; //! the altered line, we want realistic offset values
					v1 = v0 + wDir * wLength;
					v2 = v0 + wDir * distW0 + ( distH0 > distH1? ( hDir * -distH0 ) : ( hDir * distH1 ) );
				}

				if( vector3_dot( plane3_for_points( v0, v1, v2 ).normal(), m_plane.normal() ) < 0 ){
					std::swap( p0, p1 );
					std::swap( v0, v1 );
				}
			}
			const DoubleVector3 vertices[3]{ v0, v1, v2 };
			const DoubleVector3 sts[3]{ DoubleVector3( p0->m_texcoord ),
										DoubleVector3( p1->m_texcoord ),
										DoubleVector3( p2->m_texcoord ) };
			Texdef_Construct_local2tex_from_ST( vertices, sts, m_local2tex );
			m_tex2local = matrix4_affine_inverse( m_local2tex );
			BP_from_ST( m_bp, vertices, sts, plane3_for_points( vertices ).normal() );
			m_bp.removeScale( patch->getShader()->getTexture().width, patch->getShader()->getTexture().height );
		}
	}
	bool valid() const {
		return !( !std::isfinite( m_local2tex[0] ) //nan
		       || !std::isfinite( m_tex2local[0] ) //nan
		       || fabs( vector3_dot( m_plane.normal(), m_tex2local.z().vec3() ) ) < 1e-6 //projected along face
		       || vector3_length_squared( m_tex2local.x().vec3() ) < .01 //srsly scaled down, limit at max 10 textures per world unit
		       || vector3_length_squared( m_tex2local.y().vec3() ) < .01 );
	}
};

void Scene_PatchGetTexdef_Selected( scene::Graph& graph, TextureProjection &projection ){
	if ( Patch* patch = Scene_GetUltimateSelectedVisiblePatch() ){
		PatchTexdefConstructor c( patch );
		if( c.valid() )
			projection.m_brushprimit_texdef = c.m_bp;
	}
}

bool Scene_PatchGetShaderTexdef_Selected( scene::Graph& graph, CopiedString& name, TextureProjection &projection ){
	Patch* patch = nullptr;
	if ( !( patch = Scene_GetUltimateSelectedVisiblePatch() ) )
		Scene_forEachSelectedPatch( [&patch]( PatchInstance& p ){ if( !patch ) patch = &p.getPatch(); } );
	if( patch ){
		name = patch->GetShader();
		PatchTexdefConstructor c( patch );
		if( c.valid() )
			projection.m_brushprimit_texdef = c.m_bp;
		return true;
	}
	return false;
}

void Patch_SetTexdef( const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	Scene_forEachVisibleSelectedPatch( [hShift, vShift, hScale, vScale, rotation]( Patch& patch ){
		PatchTexdefConstructor c( &patch );
		if( c.valid() ){
			BPTexdef_Assign( c.m_bp, hShift, vShift, hScale, vScale, rotation );
			Matrix4 local2tex;
			c.m_bp.addScale( patch.getShader()->getTexture().width, patch.getShader()->getTexture().height );
			BP_Construct_local2tex( c.m_bp, c.m_plane, local2tex );
			matrix4_multiply_by_matrix4( local2tex, c.m_tex2local );
			patch.undoSave();
			for( auto& p : patch.getControlPoints() )
				p.m_texcoord = matrix4_transformed_point( local2tex, Vector3( p.m_texcoord ) ).vec2();
			patch.controlPointsChanged();
		}
		else{ // fallback //. fixme: this is not cool; may be more valid cases in PatchTexdefConstructor, as in find one good triangle in problematic case
			if( hShift )
				Scene_PatchTranslateTexture_Selected( GlobalSceneGraph(), *hShift > 0? 8 : -8, 0 );
			if( vShift )
				Scene_PatchTranslateTexture_Selected( GlobalSceneGraph(), 0, *vShift > 0? 8 : -8 );
			if( hScale )
				Scene_PatchScaleTexture_Selected( GlobalSceneGraph(), *hScale > 0? .5 : -.5, 0 );
			if( vScale )
				Scene_PatchScaleTexture_Selected( GlobalSceneGraph(), 0, *vScale > 0? .5 : -.5 );
			if( rotation )
				Scene_PatchRotateTexture_Selected( GlobalSceneGraph(), *rotation > 0? 15 : -15 );

		}
		Patch_textureChanged();
	} );
}

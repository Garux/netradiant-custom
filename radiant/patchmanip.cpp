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
	if ( ( type == eCapEndCap || type == eCapIEndCap )
		 && patch.getWidth() != 5 ) {
		globalErrorStream() << "cannot create end-cap - patch width != 5\n";
		return;
	}
	if ( ( type == eCapBevel || type == eCapIBevel )
		 && patch.getWidth() != 3 && patch.getWidth() != 5 ) {
		globalErrorStream() << "cannot create bevel-cap - patch width != 3\n";
		return;
	}
	if ( type == eCapCylinder
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

enum ECapDialog {
	PATCHCAP_BEVEL = 0,
	PATCHCAP_ENDCAP,
	PATCHCAP_INVERTED_BEVEL,
	PATCHCAP_INVERTED_ENDCAP,
	PATCHCAP_CYLINDER
};

EMessageBoxReturn DoCapDlg( ECapDialog *type );

void Scene_PatchDoCap_Selected( scene::Graph& graph, const char* shader ){
	ECapDialog nType;

	if ( DoCapDlg( &nType ) == eIDOK ) {
		EPatchCap eType;
		switch ( nType )
		{
		case PATCHCAP_INVERTED_BEVEL:
			eType = eCapIBevel;
			break;
		case PATCHCAP_BEVEL:
			eType = eCapBevel;
			break;
		case PATCHCAP_INVERTED_ENDCAP:
			eType = eCapIEndCap;
			break;
		case PATCHCAP_ENDCAP:
			eType = eCapEndCap;
			break;
		case PATCHCAP_CYLINDER:
			eType = eCapCylinder;
			break;
		default:
			ERROR_MESSAGE( "invalid patch cap type" );
			return;
		}

		InstanceVector instances;
		Scene_forEachVisibleSelectedPatchInstance( PatchStoreInstance( instances ) );
		for ( InstanceVector::const_iterator i = instances.begin(); i != instances.end(); ++i )
		{
			Patch_makeCaps( *Node_getPatch( ( *i )->path().top() ), *( *i ), eType, shader );
		}
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
	for ( InstanceVector::const_iterator i = instances.begin(); i != instances.end(); ++i )
	{
		Patch_deform( *Node_getPatch( ( *i )->path().top() ), *( *i ), deform, axis );
	}

}

void Patch_thicken( Patch& patch, scene::Instance& instance, const float thickness, bool seams, const int axis ){

		// Create a new patch node
		NodeSmartReference node( g_patchCreator->createPatch() );
		// Insert the node into worldspawn
		Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->insert( node );

		// Retrieve the contained patch from the node
		Patch* targetPatch = Node_getPatch( node );

		// Create the opposite patch with the given thickness = distance
		bool no12 = true;
		bool no34 = true;
		targetPatch->createThickenedOpposite( patch, thickness, axis, no12, no34 );

		// Now select the newly created patches
		{
			scene::Path patchpath( makeReference( GlobalSceneGraph().root() ) );
			patchpath.push( makeReference( *Map_GetWorldspawn( g_map ) ) );
			patchpath.push( makeReference( node.get() ) );
			Instance_getSelectable( *GlobalSceneGraph().find( patchpath ) )->setSelected( true );
		}

		if( seams && thickness != 0.0f){
			int i = 0;
			if ( no12 ){
				i = 2;
			}
			int iend = 4;
			if ( no34 ){
				iend = 2;
			}
			// Now create the four walls
			for ( ; i < iend; i++ ){
				// Allocate new patch
				NodeSmartReference node = NodeSmartReference( g_patchCreator->createPatch() );
				// Insert each node into worldspawn
				Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->insert( node );

				// Retrieve the contained patch from the node
				Patch* wallPatch = Node_getPatch( node );

				// Create the wall patch by passing i as wallIndex
				wallPatch->createThickenedWall( patch, *targetPatch, i );

				if( ( wallPatch->localAABB().extents[0] <= 0.00005 && wallPatch->localAABB().extents[1] <= 0.00005 ) ||
					( wallPatch->localAABB().extents[1] <= 0.00005 && wallPatch->localAABB().extents[2] <= 0.00005 ) ||
					( wallPatch->localAABB().extents[0] <= 0.00005 && wallPatch->localAABB().extents[2] <= 0.00005 ) ){
					//globalOutputStream() << "Thicken: Discarding degenerate patch.\n";
					Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->erase( node );
				}
				else
				// Now select the newly created patches
				{
					scene::Path patchpath( makeReference( GlobalSceneGraph().root() ) );
					patchpath.push( makeReference( *Map_GetWorldspawn(g_map) ) );
					patchpath.push( makeReference( node.get() ) );
					Instance_getSelectable( *GlobalSceneGraph().find( patchpath ) )->setSelected( true );
				}
			}
		}

		// Invert the target patch so that it faces the opposite direction
		targetPatch->InvertMatrix();
}

void Scene_PatchThicken( scene::Graph& graph, const int thickness, bool seams, const int axis )
{
	InstanceVector instances;
	Scene_forEachVisibleSelectedPatchInstance( PatchStoreInstance( instances ) );
	for ( InstanceVector::const_iterator i = instances.begin(); i != instances.end(); ++i )
	{
		Patch_thicken( *Node_getPatch( ( *i )->path().top() ), *( *i ), thickness, seams, axis );
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


class PatchCapTexture
{
public:
void operator()( Patch& patch ) const {
	//patch.ProjectTexture( Patch::m_CycleCapIndex );
	patch.CapTexture();
}
};

void Scene_PatchCapTexture_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( PatchCapTexture() );
	//Patch::m_CycleCapIndex = ( Patch::m_CycleCapIndex + 1 ) % 3;
	SceneChangeNotify();
}

class PatchProjectTexture
{
	const texdef_t& m_texdef;
	const Vector3* m_direction;
public:
	PatchProjectTexture( const texdef_t& texdef, const Vector3* direction ) : m_texdef( texdef ), m_direction( direction ) {
	}
	void operator()( Patch& patch ) const {
		patch.ProjectTexture( m_texdef, m_direction );
	}
};

void Scene_PatchProjectTexture_Selected( scene::Graph& graph, const texdef_t& texdef, const Vector3* direction ){
	Scene_forEachVisibleSelectedPatch( PatchProjectTexture( texdef, direction ) );
	SceneChangeNotify();
}

class PatchProjectTexture_fromFace
{
	const TextureProjection& m_projection;
	const Vector3& m_normal;
public:
	PatchProjectTexture_fromFace( const TextureProjection& projection, const Vector3& normal ) : m_projection( projection ), m_normal( normal ) {
	}
	void operator()( Patch& patch ) const {
		patch.ProjectTexture( m_projection, m_normal );
	}
};

void Scene_PatchProjectTexture_Selected( scene::Graph& graph, const TextureProjection& projection, const Vector3& normal ){
	Scene_forEachVisibleSelectedPatch( PatchProjectTexture_fromFace( projection, normal ) );
	SceneChangeNotify();
}

class PatchFlipTexture
{
int m_axis;
public:
PatchFlipTexture( int axis ) : m_axis( axis ){
}
void operator()( Patch& patch ) const {
	patch.FlipTexture( m_axis );
}
};

void Scene_PatchFlipTexture_Selected( scene::Graph& graph, int axis ){
	Scene_forEachVisibleSelectedPatch( PatchFlipTexture( axis ) );
}

class PatchNaturalTexture
{
public:
void operator()( Patch& patch ) const {
	patch.NaturalTexture();
}
};

void Scene_PatchNaturalTexture_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( PatchNaturalTexture() );
	SceneChangeNotify();
}


class PatchInsertRemove
{
bool m_insert, m_column, m_first;
public:
PatchInsertRemove( bool insert, bool column, bool first ) : m_insert( insert ), m_column( column ), m_first( first ){
}
void operator()( Patch& patch ) const {
	patch.InsertRemove( m_insert, m_column, m_first );
}
};

void Scene_PatchInsertRemove_Selected( scene::Graph& graph, bool bInsert, bool bColumn, bool bFirst ){
	Scene_forEachVisibleSelectedPatch( PatchInsertRemove( bInsert, bColumn, bFirst ) );
}

class PatchInvertMatrix
{
public:
void operator()( Patch& patch ) const {
	patch.InvertMatrix();
}
};

void Scene_PatchInvert_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( PatchInvertMatrix() );
}

class PatchRedisperse
{
EMatrixMajor m_major;
public:
PatchRedisperse( EMatrixMajor major ) : m_major( major ){
}
void operator()( Patch& patch ) const {
	patch.Redisperse( m_major );
}
};

void Scene_PatchRedisperse_Selected( scene::Graph& graph, EMatrixMajor major ){
	Scene_forEachVisibleSelectedPatch( PatchRedisperse( major ) );
}

class PatchSmooth
{
EMatrixMajor m_major;
public:
PatchSmooth( EMatrixMajor major ) : m_major( major ){
}
void operator()( Patch& patch ) const {
	patch.Smooth( m_major );
}
};

void Scene_PatchSmooth_Selected( scene::Graph& graph, EMatrixMajor major ){
	Scene_forEachVisibleSelectedPatch( PatchSmooth( major ) );
}

class PatchTransposeMatrix
{
public:
void operator()( Patch& patch ) const {
	patch.TransposeMatrix();
}
};

void Scene_PatchTranspose_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch( PatchTransposeMatrix() );
}

class PatchSetShader
{
const char* m_name;
public:
PatchSetShader( const char* name )
	: m_name( name ){
}
void operator()( Patch& patch ) const {
	patch.SetShader( m_name );
}
};

void Scene_PatchSetShader_Selected( scene::Graph& graph, const char* name ){
	Scene_forEachVisibleSelectedPatch( PatchSetShader( name ) );
	SceneChangeNotify();
}

void Scene_PatchGetShader_Selected( scene::Graph& graph, CopiedString& name ){
	Patch* patch = Scene_GetUltimateSelectedVisiblePatch();
	if ( patch != 0 ) {
		name = patch->GetShader();
	}
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

	DoNewPatchDlg( eXactCylinder, 3, 7, 3, 13, 0, 0 );
}

void Patch_XactSphere(){
	UndoableCommand undo( "patchCreateXactSphere" );

	DoNewPatchDlg( eXactSphere, 5, 7, 7, 13, 0, 0 );
}

void Patch_XactCone(){
	UndoableCommand undo( "patchCreateXactCone" );

	DoNewPatchDlg( eXactCone, 3, 7, 3, 13, 0, 0 );
}

void Patch_Cylinder(){
	UndoableCommand undo( "patchCreateCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_DenseCylinder(){
	UndoableCommand undo( "patchCreateDenseCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eDenseCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_VeryDenseCylinder(){
	UndoableCommand undo( "patchCreateVeryDenseCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eVeryDenseCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_SquareCylinder(){
	UndoableCommand undo( "patchCreateSquareCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eSqCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Endcap(){
	UndoableCommand undo( "patchCreateEndCap" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eEndCap, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Bevel(){
	UndoableCommand undo( "patchCreateBevel" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eBevel, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Sphere(){
	UndoableCommand undo( "patchCreateSphere" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eSphere, GlobalXYWnd_getCurrentViewType() );
}

void Patch_SquareBevel(){
}

void Patch_SquareEndcap(){
}

void Patch_Cone(){
	UndoableCommand undo( "patchCreateCone" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), eCone, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Plane(){
	UndoableCommand undo( "patchCreatePlane" );

	DoNewPatchDlg( ePlane, 3, 3, 3, 3, 0, 0 );
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

void Patch_Cap(){
	// FIXME: add support for patch cap creation
	// Patch_CapCurrent();
	UndoableCommand undo( "patchPutCaps" );

	Scene_PatchDoCap_Selected( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
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
	float fx, fy;
	if ( DoTextureLayout( &fx, &fy ) == eIDOK ) {
		UndoableCommand command( "patchTileTexture" );
		Scene_PatchTileTexture_Selected( GlobalSceneGraph(), fx, fy );
	}
}

void Patch_FitTexture11(){
	UndoableCommand command( "patchFitTexture" );
	Scene_PatchTileTexture_Selected( GlobalSceneGraph(), 1, 1 );
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
	page.appendEntry( "Patch Subdivide Threshold", g_PatchSubdivideThreshold );
}
void Patch_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Patches", "Patch Display Preferences" ) );
	Patch_constructPreferences( page );
}
void Patch_registerPreferencesPage(){
	PreferencesDialog_addDisplayPage( FreeCaller1<PreferenceGroup&, Patch_constructPage>() );
}


#include "preferencesystem.h"

void PatchPreferences_construct(){
	GlobalPreferenceSystem().registerPreference( "Subdivisions", IntImportStringCaller( g_PatchSubdivideThreshold ), IntExportStringCaller( g_PatchSubdivideThreshold ) );
}


#include "generic/callback.h"

void Patch_registerCommands(){
	GlobalCommands_insert( "InvertCurveTextureX", FreeCaller<Patch_FlipTextureX>(), Accelerator( 'I', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "InvertCurveTextureY", FreeCaller<Patch_FlipTextureY>(), Accelerator( 'I', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "NaturalizePatch", FreeCaller<Patch_NaturalTexture>(), Accelerator( 'N', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchCylinder", FreeCaller<Patch_Cylinder>() );
//	GlobalCommands_insert( "PatchDenseCylinder", FreeCaller<Patch_DenseCylinder>() );
//	GlobalCommands_insert( "PatchVeryDenseCylinder", FreeCaller<Patch_VeryDenseCylinder>() );
	GlobalCommands_insert( "PatchSquareCylinder", FreeCaller<Patch_SquareCylinder>() );
	GlobalCommands_insert( "PatchXactCylinder", FreeCaller<Patch_XactCylinder>() );
	GlobalCommands_insert( "PatchXactSphere", FreeCaller<Patch_XactSphere>() );
	GlobalCommands_insert( "PatchXactCone", FreeCaller<Patch_XactCone>() );
	GlobalCommands_insert( "PatchEndCap", FreeCaller<Patch_Endcap>() );
	GlobalCommands_insert( "PatchBevel", FreeCaller<Patch_Bevel>() );
//	GlobalCommands_insert( "PatchSquareBevel", FreeCaller<Patch_SquareBevel>() );
//	GlobalCommands_insert( "PatchSquareEndcap", FreeCaller<Patch_SquareEndcap>() );
	GlobalCommands_insert( "PatchCone", FreeCaller<Patch_Cone>() );
	GlobalCommands_insert( "PatchSphere", FreeCaller<Patch_Sphere>() );
	GlobalCommands_insert( "SimplePatchMesh", FreeCaller<Patch_Plane>(), Accelerator( 'P', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "PatchInsertFirstColumn", FreeCaller<Patch_InsertFirstColumn>(), Accelerator( GDK_KP_Add, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "PatchInsertLastColumn", FreeCaller<Patch_InsertLastColumn>() );
	GlobalCommands_insert( "PatchInsertFirstRow", FreeCaller<Patch_InsertFirstRow>(), Accelerator( GDK_KP_Add, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchInsertLastRow", FreeCaller<Patch_InsertLastRow>() );
	GlobalCommands_insert( "PatchDeleteFirstColumn", FreeCaller<Patch_DeleteFirstColumn>() );
	GlobalCommands_insert( "PatchDeleteLastColumn", FreeCaller<Patch_DeleteLastColumn>(), Accelerator( GDK_KP_Subtract, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "PatchDeleteFirstRow", FreeCaller<Patch_DeleteFirstRow>() );
	GlobalCommands_insert( "PatchDeleteLastRow", FreeCaller<Patch_DeleteLastRow>(), Accelerator( GDK_KP_Subtract, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "InvertCurve", FreeCaller<Patch_Invert>(), Accelerator( 'I', (GdkModifierType)GDK_CONTROL_MASK ) );
	//GlobalCommands_insert( "RedisperseRows", FreeCaller<Patch_RedisperseRows>(), Accelerator( 'E', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "RedisperseRows", FreeCaller<Patch_RedisperseRows>() );
	//GlobalCommands_insert( "RedisperseCols", FreeCaller<Patch_RedisperseCols>(), Accelerator( 'E', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "RedisperseCols", FreeCaller<Patch_RedisperseCols>() );
	GlobalCommands_insert( "SmoothRows", FreeCaller<Patch_SmoothRows>(), Accelerator( 'W', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "SmoothCols", FreeCaller<Patch_SmoothCols>(), Accelerator( 'W', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "MatrixTranspose", FreeCaller<Patch_Transpose>(), Accelerator( 'M', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "CapCurrentCurve", FreeCaller<Patch_Cap>(), Accelerator( 'C', (GdkModifierType)GDK_SHIFT_MASK ) );
//	GlobalCommands_insert( "MakeOverlayPatch", FreeCaller<Patch_OverlayOn>(), Accelerator( 'Y' ) );
//	GlobalCommands_insert( "ClearPatchOverlays", FreeCaller<Patch_OverlayOff>(), Accelerator( 'L', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchDeform", FreeCaller<Patch_Deform>() );
	GlobalCommands_insert( "PatchThicken", FreeCaller<Patch_Thicken>(), Accelerator( 'T', (GdkModifierType)GDK_CONTROL_MASK ) );
}

void Patch_constructToolbar( GtkToolbar* toolbar ){
	toolbar_append_button( toolbar, "Put caps on the current patch (SHIFT + C)", "curve_cap.png", "CapCurrentCurve" );
}

void Patch_constructMenu( GtkMenu* menu ){
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
//		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic( menu, "More Cylinders" );
//		if ( g_Layout_enableDetachableMenus.m_value ) {
//			menu_tearoff( menu_in_menu );
//		}
//		create_menu_item_with_mnemonic( menu_in_menu, "Dense Cylinder", "PatchDenseCylinder" );
//		create_menu_item_with_mnemonic( menu_in_menu, "Very Dense Cylinder", "PatchVeryDenseCylinder" );
//		create_menu_item_with_mnemonic( menu_in_menu, "Square Cylinder", "PatchSquareCylinder" );
//	}
//	{
//		//not implemented
//		create_menu_item_with_mnemonic( menu, "Square Endcap", "PatchSquareBevel" );
//		create_menu_item_with_mnemonic( menu, "Square Bevel", "PatchSquareEndcap" );
//	}
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Cap Selection", "CapCurrentCurve" );
	menu_separator( menu );
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic( menu, "Insert/Delete" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Insert (2) First Columns", "PatchInsertFirstColumn" );
		create_menu_item_with_mnemonic( menu_in_menu, "Insert (2) Last Columns", "PatchInsertLastColumn" );
		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "Insert (2) First Rows", "PatchInsertFirstRow" );
		create_menu_item_with_mnemonic( menu_in_menu, "Insert (2) Last Rows", "PatchInsertLastRow" );
		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "Del First (2) Columns", "PatchDeleteFirstColumn" );
		create_menu_item_with_mnemonic( menu_in_menu, "Del Last (2) Columns", "PatchDeleteLastColumn" );
		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "Del First (2) Rows", "PatchDeleteFirstRow" );
		create_menu_item_with_mnemonic( menu_in_menu, "Del Last (2) Rows", "PatchDeleteLastRow" );
	}
	menu_separator( menu );
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic( menu, "Matrix" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Invert", "InvertCurve" );
		create_menu_item_with_mnemonic( menu_in_menu, "Transpose", "MatrixTranspose" );

		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "Re-disperse Rows", "RedisperseRows" );
		create_menu_item_with_mnemonic( menu_in_menu, "Re-disperse Columns", "RedisperseCols" );

		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "Smooth Rows", "SmoothRows" );
		create_menu_item_with_mnemonic( menu_in_menu, "Smooth Columns", "SmoothCols" );
	}
	menu_separator( menu );
	{
		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic( menu, "Texture" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Project", "TextureReset/Cap" );
		create_menu_item_with_mnemonic( menu_in_menu, "Naturalize", "NaturalizePatch" );
		create_menu_item_with_mnemonic( menu_in_menu, "Invert X", "InvertCurveTextureX" );
		create_menu_item_with_mnemonic( menu_in_menu, "Invert Y", "InvertCurveTextureY" );

	}
//	menu_separator( menu );
//	{ //unfinished
//		GtkMenu* menu_in_menu = create_sub_menu_with_mnemonic( menu, "Overlay" );
//		if ( g_Layout_enableDetachableMenus.m_value ) {
//			menu_tearoff( menu_in_menu );
//		}
//		create_menu_item_with_mnemonic( menu_in_menu, "Set", "MakeOverlayPatch" );
//		create_menu_item_with_mnemonic( menu_in_menu, "Clear", "ClearPatchOverlays" );
//	}
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Deform...", "PatchDeform" );
	create_menu_item_with_mnemonic( menu, "Thicken...", "PatchThicken" );
}


#include <gtk/gtkbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtklabel.h>
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"

void DoNewPatchDlg( EPatchPrefab prefab, int minrows, int mincols, int defrows, int defcols, int maxrows, int maxcols ){
	ModalDialog dialog;
	GtkComboBox* width;
	GtkComboBox* height;
	GtkWidget* redisperseCheckBox;

	GtkWindow* window = create_dialog_window( MainFrame_getWindow(), "Patch density", G_CALLBACK( dialog_delete_callback ), &dialog );

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group( window, accel );

	{
		GtkHBox* hbox = create_dialog_hbox( 4, 4 );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( hbox ) );
		{
			GtkTable* table = create_dialog_table( 3, 2, 4, 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
			{
				GtkLabel* label = GTK_LABEL( gtk_label_new( "Width:" ) );
				gtk_widget_show( GTK_WIDGET( label ) );
				gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, 0, 1,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}
			{
				GtkLabel* label = GTK_LABEL( gtk_label_new( "Height:" ) );
				gtk_widget_show( GTK_WIDGET( label ) );
				gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, 1, 2,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}

			{
				GtkComboBox* combo = GTK_COMBO_BOX( gtk_combo_box_new_text() );
#define D_ITEM( x ) if ( x >= mincols && ( !maxcols || x <= maxcols ) ) gtk_combo_box_append_text( combo, # x )
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
				gtk_widget_show( GTK_WIDGET( combo ) );
				gtk_table_attach( table, GTK_WIDGET( combo ), 1, 2, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );

				width = combo;
			}
			{
				GtkComboBox* combo = GTK_COMBO_BOX( gtk_combo_box_new_text() );
#define D_ITEM( x ) if ( x >= minrows && ( !maxrows || x <= maxrows ) ) gtk_combo_box_append_text( combo, # x )
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
				gtk_widget_show( GTK_WIDGET( combo ) );
				gtk_table_attach( table, GTK_WIDGET( combo ), 1, 2, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );

				height = combo;
			}

			if( prefab != ePlane ){
				GtkWidget* _redisperseCheckBox = gtk_check_button_new_with_label( "Square" );
				gtk_widget_set_tooltip_text( _redisperseCheckBox, "Redisperse columns & rows" );
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( _redisperseCheckBox ), FALSE );
				gtk_widget_show( _redisperseCheckBox );
				gtk_table_attach( table, _redisperseCheckBox, 0, 2, 2, 3,
								(GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								(GtkAttachOptions) ( 0 ), 0, 0 );
				redisperseCheckBox = _redisperseCheckBox;
			}

		}

		{
			GtkVBox* vbox = create_dialog_vbox( 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE, 0 );
			{
				GtkButton* button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				widget_make_default( GTK_WIDGET( button ) );
				gtk_widget_grab_focus( GTK_WIDGET( button ) );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
				GtkButton* button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	// Initialize dialog
	gtk_combo_box_set_active( width, ( defcols - mincols ) / 2 );
	gtk_combo_box_set_active( height, ( defrows - minrows ) / 2 );

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		int w = gtk_combo_box_get_active( width ) * 2 + mincols;
		int h = gtk_combo_box_get_active( height ) * 2 + minrows;
		bool redisperse = false;
		if( prefab != ePlane ){
			redisperse = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( redisperseCheckBox ) ) ? true : false;
		}
		Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader(), prefab, GlobalXYWnd_getCurrentViewType(), w, h, redisperse );
	}

	gtk_widget_destroy( GTK_WIDGET( window ) );
}


void DoPatchDeformDlg(){
	ModalDialog dialog;
	GtkWidget* deformW;

    GtkWidget* rndY;
	GtkWidget* rndX;

	GtkWindow* window = create_dialog_window( MainFrame_getWindow(), "Patch deform", G_CALLBACK( dialog_delete_callback ), &dialog );

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group( window, accel );

	{
		GtkHBox* hbox = create_dialog_hbox( 4, 4 );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( hbox ) );
		{
			GtkTable* table = create_dialog_table( 2, 2, 4, 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
			{
				GtkLabel* label = GTK_LABEL( gtk_label_new( "Max deform:" ) );
				gtk_widget_show( GTK_WIDGET( label ) );
				gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, 0, 1,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}
//			{
//				GtkWidget* entry = gtk_entry_new();
//				gtk_entry_set_text( GTK_ENTRY( entry ), "64" );
//				gtk_widget_show( entry );
//				gtk_table_attach( table, entry, 1, 2, 0, 1,
//								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
//								  (GtkAttachOptions) ( 0 ), 0, 0 );
//
//				deformW = entry;
//			}
			{
				GtkAdjustment* adj = GTK_ADJUSTMENT( gtk_adjustment_new( 64, -9999, 9999, 1, 10, 0 ) );
				GtkWidget* spin = gtk_spin_button_new( adj, 1, 0 );
				gtk_widget_show( spin );
				gtk_table_attach( table, spin, 1, 2, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_widget_set_size_request( spin, 64, -1 );
				gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );

				deformW = spin;
			}
			{
				// Create the radio button group for choosing the axis
				GtkWidget* _rndZ = gtk_radio_button_new_with_label_from_widget( NULL, "Z" );
				GtkWidget* _rndY = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_rndZ), "Y" );
				GtkWidget* _rndX = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_rndZ), "X" );
				gtk_widget_show( _rndZ );
				gtk_widget_show( _rndY );
				gtk_widget_show( _rndX );


				GtkHBox* _hbox = create_dialog_hbox( 4, 4 );
				gtk_table_attach( table, GTK_WIDGET( _hbox ), 0, 2, 1, 2,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_box_pack_start( GTK_BOX( _hbox ), GTK_WIDGET( _rndX ), TRUE, TRUE, 0 );
				gtk_box_pack_start( GTK_BOX( _hbox ), GTK_WIDGET( _rndY ), TRUE, TRUE, 0 );
				gtk_box_pack_start( GTK_BOX( _hbox ), GTK_WIDGET( _rndZ ), TRUE, TRUE, 0 );

				rndX = _rndX;
				rndY = _rndY;
			}
		}
		{
			GtkVBox* vbox = create_dialog_vbox( 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE, 0 );
			{
				GtkButton* button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				widget_make_default( GTK_WIDGET( button ) );
				gtk_widget_grab_focus( GTK_WIDGET( button ) );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
				GtkButton* button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		//int deform = static_cast<int>( atoi( gtk_entry_get_text( GTK_ENTRY( deformW ) ) ) );
		gtk_spin_button_update ( GTK_SPIN_BUTTON( deformW ) );
		int deform = static_cast<int>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( deformW ) ) );
		int axis = 2; //Z
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( rndX ) ) ){
			axis = 0;
		}
		else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( rndY ) ) ){
			axis = 1;
		}
		Scene_PatchDeform( GlobalSceneGraph(), deform, axis );
	}
	gtk_widget_destroy( GTK_WIDGET( window ) );
}



EMessageBoxReturn DoCapDlg( ECapDialog* type ){
	ModalDialog dialog;
	ModalDialogButton ok_button( dialog, eIDOK );
	ModalDialogButton cancel_button( dialog, eIDCANCEL );
	GtkWidget* bevel;
	GtkWidget* ibevel;
	GtkWidget* endcap;
	GtkWidget* iendcap;
	GtkWidget* cylinder;

	GtkWindow* window = create_modal_dialog_window( MainFrame_getWindow(), "Cap", dialog );

	GtkAccelGroup *accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group( window, accel_group );

	{
		GtkHBox* hbox = create_dialog_hbox( 4, 4 );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( hbox ) );

		{
			// Gef: Added a vbox to contain the toggle buttons
			GtkVBox* radio_vbox = create_dialog_vbox( 4 );
			gtk_container_add( GTK_CONTAINER( hbox ), GTK_WIDGET( radio_vbox ) );

			{
				GtkTable* table = GTK_TABLE( gtk_table_new( 5, 2, FALSE ) );
				gtk_widget_show( GTK_WIDGET( table ) );
				gtk_box_pack_start( GTK_BOX( radio_vbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
				gtk_table_set_row_spacings( table, 5 );
				gtk_table_set_col_spacings( table, 5 );

				{
					GtkImage* image = new_local_image( "cap_bevel.png" );
					gtk_widget_show( GTK_WIDGET( image ) );
					gtk_table_attach( table, GTK_WIDGET( image ), 0, 1, 0, 1,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkImage* image = new_local_image( "cap_endcap.png" );
					gtk_widget_show( GTK_WIDGET( image ) );
					gtk_table_attach( table, GTK_WIDGET( image ), 0, 1, 1, 2,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkImage* image = new_local_image( "cap_ibevel.png" );
					gtk_widget_show( GTK_WIDGET( image ) );
					gtk_table_attach( table, GTK_WIDGET( image ), 0, 1, 2, 3,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkImage* image = new_local_image( "cap_iendcap.png" );
					gtk_widget_show( GTK_WIDGET( image ) );
					gtk_table_attach( table, GTK_WIDGET( image ), 0, 1, 3, 4,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkImage* image = new_local_image( "cap_cylinder.png" );
					gtk_widget_show( GTK_WIDGET( image ) );
					gtk_table_attach( table, GTK_WIDGET( image ), 0, 1, 4, 5,
									  (GtkAttachOptions) ( GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}

				GSList* group = 0;
				{
					GtkWidget* button = gtk_radio_button_new_with_label( group, "Bevel" );
					gtk_widget_show( button );
					gtk_table_attach( table, button, 1, 2, 0, 1,
									  (GtkAttachOptions) ( GTK_FILL | GTK_EXPAND ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					group = gtk_radio_button_group( GTK_RADIO_BUTTON( button ) );

					bevel = button;
				}
				{
					GtkWidget* button = gtk_radio_button_new_with_label( group, "Endcap" );
					gtk_widget_show( button );
					gtk_table_attach( table, button, 1, 2, 1, 2,
									  (GtkAttachOptions) ( GTK_FILL | GTK_EXPAND ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					group = gtk_radio_button_group( GTK_RADIO_BUTTON( button ) );

					endcap = button;
				}
				{
					GtkWidget* button = gtk_radio_button_new_with_label( group, "Inverted Bevel" );
					gtk_widget_show( button );
					gtk_table_attach( table, button, 1, 2, 2, 3,
									  (GtkAttachOptions) ( GTK_FILL | GTK_EXPAND ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					group = gtk_radio_button_group( GTK_RADIO_BUTTON( button ) );

					ibevel = button;
				}
				{
					GtkWidget* button = gtk_radio_button_new_with_label( group, "Inverted Endcap" );
					gtk_widget_show( button );
					gtk_table_attach( table, button, 1, 2, 3, 4,
									  (GtkAttachOptions) ( GTK_FILL | GTK_EXPAND ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					group = gtk_radio_button_group( GTK_RADIO_BUTTON( button ) );

					iendcap = button;
				}
				{
					GtkWidget* button = gtk_radio_button_new_with_label( group, "Cylinder" );
					gtk_widget_show( button );
					gtk_table_attach( table, button, 1, 2, 4, 5,
									  (GtkAttachOptions) ( GTK_FILL | GTK_EXPAND ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					group = gtk_radio_button_group( GTK_RADIO_BUTTON( button ) );

					cylinder = button;
				}
			}
		}

		{
			GtkVBox* vbox = create_dialog_vbox( 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), FALSE, FALSE, 0 );
			{
				GtkButton* button = create_modal_dialog_button( "OK", ok_button );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				widget_make_default( GTK_WIDGET( button ) );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel_group, GDK_Return, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
			{
				GtkButton* button = create_modal_dialog_button( "Cancel", cancel_button );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel_group, GDK_Escape, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
		}
	}

	// Initialize dialog
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bevel ), TRUE );

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );
	if ( ret == eIDOK ) {
		if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bevel ) ) ) {
			*type = PATCHCAP_BEVEL;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( endcap ) ) ) {
			*type = PATCHCAP_ENDCAP;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ibevel ) ) ) {
			*type = PATCHCAP_INVERTED_BEVEL;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( iendcap ) ) ) {
			*type = PATCHCAP_INVERTED_ENDCAP;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( cylinder ) ) ) {
			*type = PATCHCAP_CYLINDER;
		}
	}

	gtk_widget_destroy( GTK_WIDGET( window ) );

	return ret;
}


void DoPatchThickenDlg(){
	ModalDialog dialog;
	GtkWidget* thicknessW;
	GtkWidget* seamsW;
	GtkWidget* radX;
	GtkWidget* radY;
	GtkWidget* radZ;

	GtkWindow* window = create_dialog_window( MainFrame_getWindow(), "Patch thicken", G_CALLBACK( dialog_delete_callback ), &dialog );

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group( window, accel );

	{
		GtkHBox* hbox = create_dialog_hbox( 4, 4 );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( hbox ) );
		{
			GtkTable* table = create_dialog_table( 2, 4, 4, 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
			{
				GtkLabel* label = GTK_LABEL( gtk_label_new( "Thickness:" ) );
				gtk_widget_show( GTK_WIDGET( label ) );
				gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, 0, 1,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}
//			{
//				GtkWidget* entry = gtk_entry_new();
//				gtk_entry_set_text( GTK_ENTRY( entry ), "16" );
//				gtk_widget_set_size_request( entry, 40, -1 );
//				gtk_widget_show( entry );
//				gtk_table_attach( table, entry, 1, 2, 0, 1,
//								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
//								  (GtkAttachOptions) ( 0 ), 0, 0 );
//
//				thicknessW = entry;
//			}
			{
				GtkAdjustment* adj = GTK_ADJUSTMENT( gtk_adjustment_new( 16, -9999, 9999, 1, 10, 0 ) );
				GtkWidget* spin = gtk_spin_button_new( adj, 1, 0 );
				gtk_widget_show( spin );
				gtk_table_attach( table, spin, 1, 2, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_widget_set_size_request( spin, 48, -1 );
				gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );

				thicknessW = spin;
			}
			{
				// Create the "create seams" label
				GtkWidget* _seamsCheckBox = gtk_check_button_new_with_label( "Side walls" );
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( _seamsCheckBox ), TRUE );
				gtk_widget_show( _seamsCheckBox );
				gtk_table_attach( table, _seamsCheckBox, 2, 4, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				seamsW = _seamsCheckBox;

			}
			{
				// Create the radio button group for choosing the extrude axis
				GtkWidget* _radNormals = gtk_radio_button_new_with_label( NULL, "Normal" );
				GtkWidget* _radX = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_radNormals), "X" );
				GtkWidget* _radY = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_radNormals), "Y" );
				GtkWidget* _radZ = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_radNormals), "Z" );
				gtk_widget_show( _radNormals );
				gtk_widget_show( _radX );
				gtk_widget_show( _radY );
				gtk_widget_show( _radZ );


				// Pack the buttons into the table
				gtk_table_attach( table, _radNormals, 0, 1, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_table_attach( table, _radX, 1, 2, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_table_attach( table, _radY, 2, 3, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_table_attach( table, _radZ, 3, 4, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				radX = _radX;
				radY = _radY;
				radZ = _radZ;
			}
		}
		{
			GtkVBox* vbox = create_dialog_vbox( 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE, 0 );
			{
				GtkButton* button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				widget_make_default( GTK_WIDGET( button ) );
				gtk_widget_grab_focus( GTK_WIDGET( button ) );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
				GtkButton* button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		int axis = 3; // Extrude along normals
		bool seams;
		//float thickness = static_cast<float>( atoi( gtk_entry_get_text( GTK_ENTRY( thicknessW ) ) ) );
		gtk_spin_button_update ( GTK_SPIN_BUTTON( thicknessW ) );
		float thickness = static_cast<float>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( thicknessW ) ) );
		seams = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( seamsW )) ? true : false;

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radX))) {
			axis = 0;
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radY))) {
			axis = 1;
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radZ))) {
			axis = 2;
		}

		Scene_PatchThicken( GlobalSceneGraph(), thickness, seams, axis );
	}

	gtk_widget_destroy( GTK_WIDGET( window ) );
}

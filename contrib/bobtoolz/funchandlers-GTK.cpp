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

#include "funchandlers.h"

#include "dialogs/dialogs-gtk.h"

#include "string/string.h"

#include "DPoint.h"
#include "DPlane.h"
#include "DBrush.h"
#include "DEPair.h"
#include "DPatch.h"
#include "DEntity.h"
#include "DShape.h"
#include "DBobView.h"
#include "DVisDrawer.h"
#include "DTrainDrawer.h"

#include "misc.h"
#include "ScriptParser.h"
#include "DTreePlanter.h"

#include "shapes.h"
#include "lists.h"
#include "visfind.h"

#include "iundo.h"

#include <vector>
#include <list>
#include <algorithm>

#include "scenelib.h"

// for autocaulk
std::vector<CopiedString> exclusionList;       // whole brush exclusion
std::vector<CopiedString> exclusionList_Face;  // single face exclusion

bool el1Loaded =        false;
bool el2Loaded =        false;

std::unique_ptr<DBobView> g_PathView;
std::unique_ptr<DVisDrawer> g_VisView;
std::unique_ptr<DTrainDrawer> g_TrainView;
std::unique_ptr<DTreePlanter> g_TreePlanter;
// -------------

//========================//
//    Helper Functions    //
//========================//

void LoadLists(){
	char buffer[256];

	if ( !el1Loaded ) {
		el1Loaded = LoadExclusionList( GetFilename( buffer, "bt/bt-el1.txt" ), exclusionList );
	}
	if ( !el2Loaded ) {
		el2Loaded = LoadExclusionList( GetFilename( buffer, "bt/bt-el2.txt" ), exclusionList_Face );
	}
}


//========================//
//     Main Functions     //
//========================//

void DoIntersect(){
	IntersectRS rs;

	if ( !DoIntersectBox( &rs ) ) {
		return;
	}

	if ( rs.nBrushOptions == BRUSH_OPT_SELECTED ) {
		if ( GlobalSelectionSystem().countSelected() < 2 ) {
			//DoMessageBox( "Invalid number of brushes selected, choose at least 2", "Error", EMessageBoxType::Error );
			globalErrorStream() << "bobToolz Intersect: Invalid number of brushes selected, choose at least 2.\n";
			return;
		}
	}

	DEntity world;
	switch ( rs.nBrushOptions )
	{
	case BRUSH_OPT_SELECTED:
	{
		world.LoadSelectedBrushes();
		break;
	}
	case BRUSH_OPT_WHOLE_MAP:
	{
		world.LoadFromEntity( GlobalRadiant().getMapWorldEntity(), {.loadDetail = rs.bUseDetail} );
		break;
	}
	}
	world.RemoveNonCheckBrushes( exclusionList );

	bool* pbSelectList;
	if ( rs.bDuplicateOnly ) {
		pbSelectList = world.BuildDuplicateList();
	}
	else{
		pbSelectList = world.BuildIntersectList();
	}

	world.SelectBrushes( pbSelectList );
	int brushCount = GlobalSelectionSystem().countSelected();
	globalOutputStream() << "bobToolz Intersect: " << brushCount << " intersecting brushes found.\n";
	delete[] pbSelectList;
}

void DoFindDuplicates()
{
	DMap map;
	map.LoadAll( {.loadVisibleOnly = true} );

	std::vector<const DBrush *> brushes;

	for( const auto *e : map.entityList )
		for( const auto *b : e->brushList )
			brushes.push_back( b );

	GlobalSelectionSystem().setSelectedAll( false );

	for( auto b = brushes.begin(); b != brushes.end(); ++b ){
		if( *b != nullptr ){
			for( auto b2 = std::next( b ); b2 != brushes.end(); ++b2 ){
				if( *b2 != nullptr ){
					if( ( *b )->operator==( *b2 ) ){
						( *b2 )->selectInRadiant();
						*b2 = nullptr;
					}
				}
			}
		}
	}
	globalOutputStream() << "bobToolz Find Duplicates: " << (int)std::count( brushes.cbegin(), brushes.cend(), nullptr ) << " duplicate brushes found.\n";
}

void DoPolygonsTB(){
	DoPolygons();
}

void DoPolygons(){
	UndoableCommand undo( "bobToolz.polygons" );
	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of brushes selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz Polygons: Invalid number of brushes selected, choose 1 only.\n";
		return;
	}

	PolygonRS rs;
	scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
	if ( !Node_isBrush( instance.path().top() ) ) {
		//DoMessageBox( "No brush selected, select ONLY one brush", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz Polygons: No brush selected, select ONLY one brush.\n";
		return;
	}
	// ask user for type, size, etc....
	if ( DoPolygonBox( &rs ) ) {
		DShape poly;

		vec3_t vMin, vMax;

		{

			VectorSubtract( instance.worldAABB().origin, instance.worldAABB().extents, vMin );
			VectorAdd( instance.worldAABB().origin, instance.worldAABB().extents, vMax );

			Path_deleteTop( instance.path() );
		}

		if ( rs.bInverse ) {
			poly.BuildInversePrism( vMin, vMax, rs.nSides, rs.bAlignTop );
		}
		else
		{
			if ( rs.bUseBorder ) {
				poly.BuildBorderedPrism( vMin, vMax, rs.nSides, rs.nBorderWidth, rs.bAlignTop );
			}
			else{
				poly.BuildRegularPrism( vMin, vMax, rs.nSides, rs.bAlignTop );
			}

		}

		poly.Commit();
	}
}

void DoFixBrushes(){
	UndoableCommand undo( "bobToolz.fixBrushes" );
	DMap world;
	world.LoadAll();

	int count = world.FixBrushes();

	globalOutputStream() << "bobToolz FixBrushes: " << count << " invalid/duplicate planes removed.\n";
}

void DoResetTextures(){
	UndoableCommand undo( "bobToolz.resetTextures" );
	static ResetTextureRS rs;

	const char* texName;
	if ( 1 /*g_SelectedFaceTable.m_pfnGetSelectedFaceCount() != 1*/ ) {
		texName = NULL;
	}
	else
	{
		texName = GetCurrentTexture();
		strcpy( rs.textureName, GetCurrentTexture() );
	}

	const EMessageBoxReturn ret = DoResetTextureBox( &rs );

	if ( rs.bResetTextureName ) {
		texName = rs.textureName;
	}

	if ( ret == eIDOK ) {
		DEntity world;
		world.LoadSelectedBrushes();
		world.ResetTextures( texName,              rs.fScale,      rs.fShift,      rs.rotation, rs.newTextureName,
		                     rs.bResetTextureName, rs.bResetScale, rs.bResetShift, rs.bResetRotation, true );
	}
	else if ( ret == eIDYES )
	{
		DMap world;
		world.LoadAll( {.loadPatches = true} );
		world.ResetTextures( texName,              rs.fScale,      rs.fShift,      rs.rotation, rs.newTextureName,
		                     rs.bResetTextureName, rs.bResetScale, rs.bResetShift, rs.bResetRotation );
	}
}

void DoBuildStairs(){
	UndoableCommand undo( "bobToolz.buildStairs" );
	BuildStairsRS rs;

	strcpy( rs.mainTexture, GetCurrentTexture() );

	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of brushes selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz BuildStairs: Invalid number of brushes selected, choose 1 only.\n";
		return;
	}

	// ask user for type, size, etc....
	if ( DoBuildStairsBox( &rs ) ) {
		vec3_t vMin, vMax;

		{
			scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
			VectorSubtract( instance.worldAABB().origin, instance.worldAABB().extents, vMin );
			VectorAdd( instance.worldAABB().origin, instance.worldAABB().extents, vMax );
		}

		// calc brush size
		vec3_t size;
		VectorSubtract( vMax, vMin, size );

		if ( ( (int)size[2] % rs.stairHeight ) != 0 ) {
			// stairs must fit evenly into brush
			//DoMessageBox( "Invalid stair height\nHeight of block must be divisable by stair height", "Error", EMessageBoxType::Error );
			globalErrorStream() << "bobToolz BuildStairs: Invalid stair height. Height of block must be divisable by stair height.\n";
		}
		else
		{
			{
				scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
				Path_deleteTop( instance.path() );
			}

			// Get Step Count
			int numSteps = (int)size[2] / rs.stairHeight;

			if ( rs.style == STYLE_CORNER ) {
				BuildCornerStairs( vMin, vMax, numSteps, rs.mainTexture, rs.riserTexture );
			}
			else
			{

				// Get Step Dimensions
				float stairHeight = (float)rs.stairHeight;
				float stairWidth;
				if ( ( rs.direction == MOVE_EAST ) || ( rs.direction == MOVE_WEST ) ) {
					stairWidth = ( size[0] ) / numSteps;
				}
				else{
					stairWidth = ( size[1] ) / numSteps;
				}


				// Build Base For Stair (bob's style)
				if ( rs.style == STYLE_BOB ) {
					Build_Wedge( rs.direction, vMin, vMax, true );
				}


				// Set First Step Starting Position
				vMax[2] = vMin[2] + stairHeight;
				SetInitialStairPos( rs.direction, vMin, vMax, stairWidth );


				// Build The Steps
				for ( int i = 0; i < numSteps; i++ )
				{
					if ( rs.style == STYLE_BOB ) {
						Build_StairStep_Wedge( rs.direction, vMin, vMax, rs.mainTexture, rs.riserTexture, rs.bUseDetail );
					}
					else if ( rs.style == STYLE_ORIGINAL ) {
						Build_StairStep( vMin, vMax, rs.mainTexture, rs.riserTexture, rs.direction );
					}

					// get step into next position
					MoveBlock( rs.direction, vMin, vMax, stairWidth );
					vMax[2] += stairHeight;
					if ( rs.style == STYLE_BOB ) {
						vMin[2] += stairHeight; // wedge bottom must be raised
					}
				}
			}
		}
	}
}

void DoBuildDoors(){
	UndoableCommand undo( "bobToolz.buildDoors" );
	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of brushes selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz BuildDoors: Invalid number of brushes selected, choose 1 only.\n";
		return;
	}

	DoorRS rs;
	{
		const char *tex = GetCurrentTexture();
		strcpy( rs.mainTexture, tex + ( string_equal_prefix_nocase( tex, "textures/" )
									  ? strlen( "textures/" )
									  : 0 ) );
	}

	if ( DoDoorsBox( &rs ) ) {
		vec3_t vMin, vMax;

		{
			scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
			VectorSubtract( instance.worldAABB().origin, instance.worldAABB().extents, vMin );
			VectorAdd( instance.worldAABB().origin, instance.worldAABB().extents, vMax );
			Path_deleteTop( instance.path() );
		}

		BuildDoorsX2( vMin, vMax,
		              rs.bScaleMainH, rs.bScaleMainV,
		              rs.bScaleTrimH, rs.bScaleTrimV,
		              ( std::string( "textures/" ) + rs.mainTexture ).c_str(), ( std::string( "textures/" ) + rs.trimTexture ).c_str(),
		              rs.nOrientation ); // shapes.cpp
	}
}

void DoPathPlotter(){
	UndoableCommand undo( "bobToolz.pathPlotter" );
	PathPlotterRS rs;
	EMessageBoxReturn ret = DoPathPlotterBox( &rs );
	if ( ret == eIDCANCEL ) {
		return;
	}
	if ( ret == eIDNO ) {
		g_PathView.reset();
		SceneChangeNotify();
		return;
	}

	// ensure we have something selected
	if( GlobalSelectionSystem().countSelected() != 1 ){
		//DoMessageBox( "Invalid number of brushes selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz PathPlotter: Invalid number of entities selected, choose 1 trigger_push entity only.\n";
		return;
	}

	Entity* entity = Node_getEntity( GlobalSelectionSystem().ultimateSelected().path().top() );
	if ( entity != 0 || ( entity = Node_getEntity( GlobalSelectionSystem().ultimateSelected().path().parent() ) ) ){
		DBobView_setEntity( *entity, rs.fMultiplier, rs.nPoints, rs.fGravity, rs.bNoUpdate, rs.bShowExtra );
		SceneChangeNotify();
	}
	else
		globalErrorStream() << "bobToolz PathPlotter: No trigger_push entity selected, select 1 only (Use list to select it).\n";
}

void DoPitBuilder(){
	UndoableCommand undo( "bobToolz.pitBuilder" );
	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of brushes selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz PitBuilder: Invalid number of brushes selected, choose 1 only.\n";
		return;
	}

	vec3_t vMin, vMax;

	scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
	//seems it does this also with a patch with valid dimensions.. but probably better to enforce a brush.
	if ( !Node_isBrush( instance.path().top() ) ) {
		//DoMessageBox( "No brush selected, select ONLY one brush", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz PitBuilder: No brush selected, select ONLY 1 brush.\n";
		return;
	}

	VectorSubtract( instance.worldAABB().origin, instance.worldAABB().extents, vMin );
	VectorAdd( instance.worldAABB().origin, instance.worldAABB().extents, vMax );

	DShape pit;

	if ( pit.BuildPit( vMin, vMax ) ) {
		pit.Commit();
		Path_deleteTop( instance.path() );
	}
	else
	{
		//DoMessageBox( "Failed To Make Pit\nTry Making The Brush Bigger", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz PitBuilder: Failed to make Pit, try making the brush bigger.\n";
	}
}

void DoMergePatches(){
	UndoableCommand undo( "bobToolz.mergePatches" );

	class : public SelectionSystem::Visitor
	{
	public:
		void visit( scene::Instance& instance ) const override {
			if( Node_isPatch( instance.path().top() ) ){
				mrg.emplace_back().patch.LoadFromPatch( instance );
				mrg.back().instance = &instance;
			}
		}
		struct data{
			DPatch patch;
			scene::Instance* instance;
			bool merged;
		};
		mutable std::list<data> mrg;
		mutable std::vector<scene::Instance*> instances_erase;
	} patches;

	GlobalSelectionSystem().foreachSelected( patches );

	if( patches.mrg.size() < 2 ){
		globalErrorStream() << "bobToolz MergePatches: Invalid number of patches selected, choose 2 or more.\n";
		return;
	}

	const auto try_merge_one = [&patches](){
		for( auto one = patches.mrg.begin(); one != patches.mrg.end(); ++one )
		{
			for( auto two = std::next( one ); two != patches.mrg.end(); ++two )
			{
				patch_merge_t merge_info = one->patch.IsMergable( two->patch );
				if( merge_info.mergable ){
					DPatch* newPatch = one->patch.MergePatches( merge_info, one->patch, two->patch );
					if( newPatch != nullptr ){
						one->merged = true;
						one->patch = *newPatch;
						delete newPatch;
						patches.instances_erase.push_back( two->instance );
						patches.mrg.erase( two );
						return true;
					}
				}
			}
		}
		return false;
	};

	while( try_merge_one() ){}; /* merge one by one for the sake of holy laziness */

	if( patches.instances_erase.empty() ){
		globalErrorStream() << "bobToolz.mergePatch: The selected patches are not mergable.\n";
		return;
	}

	for( auto&& mrg : patches.mrg )
	{
		if( mrg.merged ){
			mrg.patch.BuildInRadiant( mrg.instance->path().parent().get_pointer() );
			Path_deleteTop( mrg.instance->path() );
		}
	}

	for( auto instance : patches.instances_erase )
	{
		scene::Instance* parent = instance->parent();
		Path_deleteTop( instance->path() );
		/* don't leave empty entities */
		if( Node_isEntity( parent->path().top() )
		 && Node_getTraversable( parent->path().top() )->empty() ){
			Path_deleteTop( parent->path() );
		}
	}
}

void DoSplitPatch() {
	UndoableCommand undo( "bobToolz.splitPatch" );

	DPatch patch;

	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of patches selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz SplitPatch: Invalid number of patches selected, choose only 1 patch.\n";
		return;
	}

	scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();

	if ( !Node_isPatch( instance.path().top() ) ) {
		//DoMessageBox( "No patch selected, select ONLY one patch", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz SplitPatch: No patch selected, select ONLY 1 patch.\n";
		return;
	}

	patch.LoadFromPatch( instance );

	for ( auto&& p : patch.Split() ) {
		p.BuildInRadiant( instance.path().parent().get_pointer() );
	}

	Path_deleteTop( instance.path() );
}

void DoSplitPatchCols() {
	UndoableCommand undo( "bobToolz.splitPatchCols" );

	DPatch patch;

	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of patches selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz SplitPatchCols: Invalid number of patches selected, choose 1 only.\n";
		return;
	}

	scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();

	if ( !Node_isPatch( instance.path().top() ) ) {
		//DoMessageBox( "No patch selected, select ONLY one patch", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz SplitPatchCols: No patch selected, select ONLY 1 patch.\n";
		return;
	}

	patch.LoadFromPatch( instance );

	for ( auto&& p : patch.SplitCols() ) {
		p.BuildInRadiant( instance.path().parent().get_pointer() );
	}

	Path_deleteTop( instance.path() );
}

void DoSplitPatchRows() {
	UndoableCommand undo( "bobToolz.splitPatchRows" );

	DPatch patch;

	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		//DoMessageBox( "Invalid number of patches selected, choose 1 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz SplitPatchRows: Invalid number of patches selected, choose 1 only.\n";
		return;
	}

	scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();

	if ( !Node_isPatch( instance.path().top() ) ) {
		//DoMessageBox( "No patch selected, select ONLY one patch", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz SplitPatchRows: No patch selected, select ONLY 1 patch.\n";
		return;
	}

	patch.LoadFromPatch( instance );

	for ( auto&& p : patch.SplitRows() ) {
		p.BuildInRadiant( instance.path().parent().get_pointer() );
	}

	Path_deleteTop( instance.path() );
}

void DoVisAnalyse(){
	const char* rad_filename = GlobalRadiant().getMapName();
	if ( !rad_filename ) {
		//DoMessageBox( "An ERROR occurred while trying\n to get the map filename", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz VisAnalyse: An ERROR occurred while trying to get the map filename.\n";
		return;
	}

	char filename[1024];
	strcpy( filename, rad_filename );

	char* ext = strrchr( filename, '.' ) + 1;
	strcpy( ext, "bsp" ); // rename the extension

	vec3_t origin;
	if ( GlobalSelectionSystem().countSelected() == 0 ) {
		memcpy( origin, GlobalRadiant().Camera_getOrigin().data(), 3 * sizeof( Vector3().x() ) );
	}
	else{
		memcpy( origin, GlobalSelectionSystem().getBoundsSelected().origin.data(), 3 * sizeof( Vector3().x() ) );
	}

	DMetaSurfaces* pointList = BuildTrace( filename, origin );

	if( pointList && pointList->size() )
		globalOutputStream() << "bobToolz VisAnalyse: " << pointList->size() << " drawsurfaces loaded\n";

	if ( !g_VisView ) {
		g_VisView = std::make_unique<DVisDrawer>();
	}

	g_VisView->SetList( pointList );
	SceneChangeNotify();
}

void DoTrainPathPlot() {
	g_TrainView.reset();
	g_TrainView.reset( new DTrainDrawer() );
}

void DoCaulkSelection() {
	UndoableCommand undo( "bobToolz.caulkSelection" );
	DEntity world;

	float fScale[2] = { 0.5f, 0.5f };
	float fShift[2] = { 0.0f, 0.0f };

	bool bResetScale[2] = { false, false };
	bool bResetShift[2] = { false, false };

	world.LoadSelectedBrushes();
	world.LoadSelectedPatches();
	world.ResetTextures( NULL, fScale, fShift, 0, "textures/common/caulk", true, bResetScale, bResetShift, false, true );
}

void DoTreePlanter() {
	if ( g_TreePlanter ) {
		g_TreePlanter.reset();
	}
	else{
		g_TreePlanter.reset( new DTreePlanter() );
	}
}

void DoDropEnts() {
	if ( g_TreePlanter ) {
		UndoableCommand undo( "bobToolz.dropEntities" );
		g_TreePlanter->DropEntsToGround();
	}
	else{
		globalErrorStream() << "bobToolz::DropEntity error: bobToolz::TreePlanter must be active\n";
	}
}

void DoMakeChain() {
	MakeChainRS rs;
	if ( DoMakeChainBox( &rs ) ) {
		if ( rs.linkNum > 1001 ) {
			globalErrorStream() << "bobToolz MakeChain: " << rs.linkNum << " to many Elemets, limited to 1000.\n";
			return;
		}
		UndoableCommand undo( "bobToolz.makeChain" );
		MakeChain( rs.linkNum, rs.linkName );
	}
}

typedef DPoint* pntTripple[3];

void DoFlipTerrain() {
	UndoableCommand undo( "bobToolz.flipTerrain" );
	vec3_t vUp = { 0.f, 0.f, 1.f };
	int i;

	// ensure we have something selected
	if ( GlobalSelectionSystem().countSelected() != 2 ) {
		//DoMessageBox( "Invalid number of objects selected, choose 2 only", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz FlipTerrain: Invalid number of objects selected, choose 2 only.\n";
		return;
	}

	scene::Instance* brushes[2];
	brushes[0] = &GlobalSelectionSystem().ultimateSelected();
	brushes[1] = &GlobalSelectionSystem().penultimateSelected();
	//ensure we have only Brushes selected.
	for ( i = 0; i < 2; i++ )
	{
		if ( !Node_isBrush( brushes[i]->path().top() ) ) {
			//DoMessageBox( "No brushes selected, select ONLY brushes", "Error", EMessageBoxType::Error );
			globalErrorStream() << "bobToolz FlipTerrain: No brushes selected, select ONLY 2 brushes.\n";
			return;
		}
	}
	DBrush Brushes[2];
	DPlane* Planes[2];
	pntTripple Points[2];
	for ( i = 0; i < 2; i++ ) {
		Brushes[i].LoadFromBrush( *brushes[i], false );
		if ( !( Planes[i] = Brushes[i].FindPlaneWithClosestNormal( vUp ) ) || Brushes[i].FindPointsForPlane( Planes[i], Points[i], 3 ) != 3 ) {
			//DoMessageBox( "Error", "Error", EMessageBoxType::Error );
			globalErrorStream() << "bobToolz FlipTerrain: ERROR (FindPlaneWithClosestNormal/FindPointsForPlane).\n";
			return;
		}
	}

	vec3_t mins1, mins2, maxs1, maxs2;
	Brushes[0].GetBounds( mins1, maxs1 );
	Brushes[1].GetBounds( mins2, maxs2 );



	int dontmatch[2] = { -1, -1 };
	bool found = false;
	for ( i = 0; i < 3; i++ ) {
		for ( int j = 0; j < 3 && !found; j++ ) {
			if ( VectorCompare( ( Points[0] )[i]->_pnt, ( Points[1] )[j]->_pnt ) ) {
				found = true;
				break;
			}
		}
		if ( !found ) {
			dontmatch[0] = i;
			break;
		}
		found = false;
	}
	if ( dontmatch[0] == -1 ) {
		//DoMessageBox( "Error", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz FlipTerrain: ERROR (dontmatch[0]).\n";
		return;
	}

	for ( i = 0; i < 3; i++ ) {
		for ( int j = 0; j < 3 && !found; j++ ) {
			if ( VectorCompare( ( Points[1] )[i]->_pnt, ( Points[0] )[j]->_pnt ) ) {
				found = true;
				break;
			}
		}
		if ( !found ) {
			dontmatch[1] = i;
			break;
		}
		found = false;
	}
	if ( dontmatch[1] == -1 ) {
		//DoMessageBox( "Error", "Error", EMessageBoxType::Error );
		globalErrorStream() << "bobToolz FlipTerrain: ERROR (dontmatch[1]).\n";
		return;
	}

	vec3_t plnpnts1[3];
	vec3_t plnpnts2[3];
	vec3_t plnpntsshr[3];

	VectorCopy( ( Points[0] )[dontmatch[0]]->_pnt, plnpnts1[0] );
	for ( i = 0; i < 3; i++ ) {
		if ( dontmatch[0] != i ) {
			VectorCopy( ( Points[0] )[i]->_pnt, plnpnts1[1] );
			break;
		}
	}
	VectorCopy( ( Points[1] )[dontmatch[1]]->_pnt, plnpnts1[2] );

	VectorCopy( ( Points[1] )[dontmatch[1]]->_pnt, plnpnts2[0] );
	for ( i = 0; i < 3; i++ ) {
		if ( dontmatch[1] != i && !VectorCompare( ( Points[1] )[i]->_pnt, plnpnts1[1] ) ) {
			VectorCopy( ( Points[1] )[i]->_pnt, plnpnts2[1] );
			break;
		}
	}
	VectorCopy( ( Points[0] )[dontmatch[0]]->_pnt, plnpnts2[2] );

	VectorCopy( ( Points[0] )[dontmatch[0]]->_pnt, plnpntsshr[0] );
	VectorCopy( ( Points[1] )[dontmatch[1]]->_pnt, plnpntsshr[1] );
	if ( ( Points[1] )[dontmatch[1]]->_pnt[2] < ( Points[0] )[dontmatch[0]]->_pnt[2] ) {
		VectorCopy( ( Points[1] )[dontmatch[1]]->_pnt, plnpntsshr[2] );
	}
	else {
		VectorCopy( ( Points[0] )[dontmatch[0]]->_pnt, plnpntsshr[2] );
	}
	plnpntsshr[2][2] -= 16;

	for ( i = 0; i < 3; i++ ) {
		if ( mins2[i] < mins1[i] ) {
			mins1[i] = mins2[i];
		}
		if ( maxs2[i] > maxs1[i] ) {
			maxs1[i] = maxs2[i];
		}
	}

	DBrush* newBrushes[2];
	newBrushes[0] = DShape::GetBoundingCube_Ext( mins1, maxs1, "textures/common/caulk", bFacesAll, true );
	newBrushes[1] = DShape::GetBoundingCube_Ext( mins1, maxs1, "textures/common/caulk", bFacesAll, true );

	vec3_t normal;
	MakeNormal( plnpnts1[0], plnpnts1[1], plnpnts1[2], normal );
	if ( normal[2] >= 0 ) {
		newBrushes[0]->AddFace( plnpnts1[0], plnpnts1[1], plnpnts1[2], "textures/common/terrain", true );
	}
	else {
		newBrushes[0]->AddFace( plnpnts1[2], plnpnts1[1], plnpnts1[0], "textures/common/terrain", true );
	}

	MakeNormal( plnpnts2[0], plnpnts2[1], plnpnts2[2], normal );
	if ( normal[2] >= 0 ) {
		newBrushes[1]->AddFace( plnpnts2[0], plnpnts2[1], plnpnts2[2], "textures/common/terrain", true );
	}
	else {
		newBrushes[1]->AddFace( plnpnts2[2], plnpnts2[1], plnpnts2[0], "textures/common/terrain", true );
	}

	vec3_t vec;
	MakeNormal( plnpntsshr[0], plnpntsshr[1], plnpntsshr[2], normal );

	VectorSubtract( plnpnts1[2], plnpnts1[1], vec );
	if ( DotProduct( vec, normal ) >= 0 ) {
		newBrushes[0]->AddFace( plnpntsshr[0], plnpntsshr[1], plnpntsshr[2], "textures/common/caulk", true );
	}
	else {
		newBrushes[0]->AddFace( plnpntsshr[2], plnpntsshr[1], plnpntsshr[0], "textures/common/caulk", true );
	}

	VectorSubtract( plnpnts2[2], plnpnts2[1], vec );
	if ( DotProduct( vec, normal ) >= 0 ) {
		newBrushes[1]->AddFace( plnpntsshr[0], plnpntsshr[1], plnpntsshr[2], "textures/common/caulk", true );
	}
	else {
		newBrushes[1]->AddFace( plnpntsshr[2], plnpntsshr[1], plnpntsshr[0], "textures/common/caulk", true );
	}

	for ( i = 0; i < 2; i++ ) {
		newBrushes[i]->RemoveRedundantPlanes();
		newBrushes[i]->BuildInRadiant( false, NULL, brushes[i]->path().parent().get_pointer() );
		Path_deleteTop( brushes[i]->path() );
		delete newBrushes[i];
	}
}


class ExplodeWalker : public scene::Traversable::Walker
{
	scene::Node* m_entitynode;
	scene::Cloneable* m_entitycloneable;
	scene::Traversable* m_traversableroot;
public:
	ExplodeWalker( scene::Node* entitynode, scene::Cloneable* entitycloneable, scene::Traversable* traversableroot )
	: m_entitynode( entitynode ), m_entitycloneable( entitycloneable ), m_traversableroot( traversableroot ){
	}
	bool pre( scene::Node& node ) const {
		return false;
	}
	void post( scene::Node& node ) const {
		NodeSmartReference clone( m_entitycloneable->clone() ); // duplicate entity
		m_traversableroot->insert( clone );

		NodeSmartReference brush( node ); // move primitive
		Node_getTraversable( *m_entitynode )->erase( node );
		Node_getTraversable( clone )->insert( node );
	}
};

void DoExplodeEntity(){
	scene::Node* entitynode = nullptr;
	scene::Node* rootnode = nullptr;
	{
		if( GlobalSelectionSystem().countSelected() == 0 ){
			globalErrorStream() << "nothing selected\n";
			return;
		}
		scene::Path path = GlobalSelectionSystem().ultimateSelected().path();
		Entity* entity = Node_getEntity( path.top() );
		if ( entity == nullptr && path.size() >= 3 ) { // path.size() = 3 as in root.entity.brush, guarantees getting `rootnode` later
			path.pop();
			entity = Node_getEntity( path.top() );
		}
		if( entity == nullptr ){
			globalErrorStream() << "no entity selected\n";
			return;
		}
		if( !entity->isContainer() ){
			globalErrorStream() << "!entity->isContainer()\n";
			return;
		}
		if( string_equal_nocase( entity->getClassName(), "worldspawn" ) ){
			globalErrorStream() << "can't explode worldspawn\n";
			return;
		}
		entitynode = path.top().get_pointer();
		rootnode = path.parent().get_pointer();
	}

	if( scene::Traversable* traversable = Node_getTraversable( *entitynode ) ){
		if( scene::Traversable* traversableroot = Node_getTraversable( *rootnode ) ){
			if( scene::Cloneable* entitycloneable = NodeTypeCast<scene::Cloneable>::cast( *entitynode ) ){
				UndoableCommand undo( "bobToolz.explodeEntity" );
				ExplodeWalker walker( entitynode, entitycloneable, traversableroot );
				traversable->traverse( walker );
				traversableroot->erase( *entitynode ); // remove original entity
			}
		}
	}
}

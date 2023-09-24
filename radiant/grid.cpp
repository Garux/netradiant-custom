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

#include "grid.h"

#include <cmath>
#include <vector>
#include <algorithm>

#include "preferencesystem.h"

#include "gtkutil/widget.h"
#include "signal/signal.h"
#include "stringio.h"

#include "gtkmisc.h"
#include "commands.h"
#include "preferences.h"



Signal0 g_gridChange_callbacks;

void AddGridChangeCallback( const SignalHandler& handler ){
	g_gridChange_callbacks.connectLast( handler );
	handler();
}

void GridChangeNotify(){
	g_gridChange_callbacks();
}

enum GridPower
{
	GRIDPOWER_0125 = -3,
	GRIDPOWER_025 = -2,
	GRIDPOWER_05 = -1,
	GRIDPOWER_1 = 0,
	GRIDPOWER_2 = 1,
	GRIDPOWER_4 = 2,
	GRIDPOWER_8 = 3,
	GRIDPOWER_16 = 4,
	GRIDPOWER_32 = 5,
	GRIDPOWER_64 = 6,
	GRIDPOWER_128 = 7,
	GRIDPOWER_256 = 8,
	GRIDPOWER_512 = 9,
	GRIDPOWER_1024 = 10,
};


// this must match the GridPower enumeration
const char *const g_gridnames[] = {
	"0.125",
	"0.25",
	"0.5",
	"1",
	"2",
	"4",
	"8",
	"16",
	"32",
	"64",
	"128",
	"256",
	"512",
	"1024",
};

std::array<QAction *, std::size( g_gridnames )> g_gridActions{};

inline GridPower GridPower_forGridDefault( int gridDefault ){
	return static_cast<GridPower>( gridDefault - 3 );
}

inline int GridDefault_forGridPower( GridPower gridPower ){
	return gridPower + 3;
}

inline void gridActions_setChecked( GridPower gridPower ){
	auto *action = g_gridActions[GridDefault_forGridPower( gridPower )];
	ASSERT_NOTNULL( action );
	action->setChecked( true );
}

int g_grid_default = GridDefault_forGridPower( GRIDPOWER_16 );

int g_grid_power = GridPower_forGridDefault( g_grid_default );

bool g_grid_snap = true;

int Grid_getPower(){
	return g_grid_power;
}

inline float GridSize_forGridPower( int gridPower ){
	return pow( 2.0f, gridPower );
}

float g_gridsize = GridSize_forGridPower( g_grid_power );

float GetSnapGridSize(){
	return g_grid_snap ? g_gridsize : 0;
}

float GetGridSize(){
	return g_gridsize;
}


void setGridPower( GridPower power ){
	g_grid_snap = true;
	g_gridsize = GridSize_forGridPower( power );
	GridChangeNotify();
}

template<GridPower power>
void setGridPower(){
	g_grid_power = power;
	setGridPower( power );
}

void GridPrev(){
	g_grid_snap = true;
	if ( g_grid_power > GRIDPOWER_0125 ) {
		setGridPower( static_cast<GridPower>( --g_grid_power ) );
		gridActions_setChecked( static_cast<GridPower>( g_grid_power ) );
	}
}

void GridNext(){
	g_grid_snap = true;
	if ( g_grid_power < GRIDPOWER_1024 ) {
		setGridPower( static_cast<GridPower>( ++g_grid_power ) );
		gridActions_setChecked( static_cast<GridPower>( g_grid_power ) );
	}
}

void ToggleGridSnap(){
	g_grid_snap = !g_grid_snap;
	GridChangeNotify();
}


int g_maxGridCoordPower = 4;
float g_maxGridCoord;
float GetMaxGridCoord(){
	return g_maxGridCoord;
}

void Region_defaultMinMax();
void maxGridCoordPowerImport( int value ){
	g_maxGridCoordPower = value;
	g_maxGridCoord = pow( 2.0, std::clamp( g_maxGridCoordPower, 0, 4 ) + 12 );
	Region_defaultMinMax();
	GridChangeNotify();
}
typedef FreeCaller1<int, maxGridCoordPowerImport> maxGridCoordPowerImportCaller;

void maxGridCoordPowerExport( const IntImportCallback& importer ){
	importer( std::clamp( g_maxGridCoordPower, 0, 4 ) );
}
typedef FreeCaller1<const IntImportCallback&, maxGridCoordPowerExport> maxGridCoordPowerExportCaller;


void Grid_registerCommands(){
	GlobalCommands_insert( "GridDown", FreeCaller<GridPrev>(), QKeySequence( "[" ) );
	GlobalCommands_insert( "GridUp", FreeCaller<GridNext>(), QKeySequence( "]" ) );

	GlobalCommands_insert( "ToggleGridSnap", FreeCaller<ToggleGridSnap>() );

	GlobalCommands_insert( "SetGrid0.125", FreeCaller<setGridPower<GRIDPOWER_0125>>() );
	GlobalCommands_insert( "SetGrid0.25", FreeCaller<setGridPower<GRIDPOWER_025>>() );
	GlobalCommands_insert( "SetGrid0.5", FreeCaller<setGridPower<GRIDPOWER_05>>() );
	GlobalCommands_insert( "SetGrid1", FreeCaller<setGridPower<GRIDPOWER_1>>(), QKeySequence( "1" ) );
	GlobalCommands_insert( "SetGrid2", FreeCaller<setGridPower<GRIDPOWER_2>>(), QKeySequence( "2" ) );
	GlobalCommands_insert( "SetGrid4", FreeCaller<setGridPower<GRIDPOWER_4>>(), QKeySequence( "3" ) );
	GlobalCommands_insert( "SetGrid8", FreeCaller<setGridPower<GRIDPOWER_8>>(), QKeySequence( "4" ) );
	GlobalCommands_insert( "SetGrid16", FreeCaller<setGridPower<GRIDPOWER_16>>(), QKeySequence( "5" ) );
	GlobalCommands_insert( "SetGrid32", FreeCaller<setGridPower<GRIDPOWER_32>>(), QKeySequence( "6" ) );
	GlobalCommands_insert( "SetGrid64", FreeCaller<setGridPower<GRIDPOWER_64>>(), QKeySequence( "7" ) );
	GlobalCommands_insert( "SetGrid128", FreeCaller<setGridPower<GRIDPOWER_128>>(), QKeySequence( "8" ) );
	GlobalCommands_insert( "SetGrid256", FreeCaller<setGridPower<GRIDPOWER_256>>(), QKeySequence( "9" ) );
	GlobalCommands_insert( "SetGrid512", FreeCaller<setGridPower<GRIDPOWER_512>>() );
	GlobalCommands_insert( "SetGrid1024", FreeCaller<setGridPower<GRIDPOWER_1024>>() );
}


void Grid_constructMenu( QMenu* menu ){
	g_gridActions = std::array{ // verify arrays size match this way
		create_menu_item_with_mnemonic( menu, "Grid0.125", "SetGrid0.125" ),
		create_menu_item_with_mnemonic( menu, "Grid0.25", "SetGrid0.25" ),
		create_menu_item_with_mnemonic( menu, "Grid0.5", "SetGrid0.5" ),
		create_menu_item_with_mnemonic( menu, "Grid1", "SetGrid1" ),
		create_menu_item_with_mnemonic( menu, "Grid2", "SetGrid2" ),
		create_menu_item_with_mnemonic( menu, "Grid4", "SetGrid4" ),
		create_menu_item_with_mnemonic( menu, "Grid8", "SetGrid8" ),
		create_menu_item_with_mnemonic( menu, "Grid16", "SetGrid16" ),
		create_menu_item_with_mnemonic( menu, "Grid32", "SetGrid32" ),
		create_menu_item_with_mnemonic( menu, "Grid64", "SetGrid64" ),
		create_menu_item_with_mnemonic( menu, "Grid128", "SetGrid128" ),
		create_menu_item_with_mnemonic( menu, "Grid256", "SetGrid256" ),
		create_menu_item_with_mnemonic( menu, "Grid512", "SetGrid512" ),
		create_menu_item_with_mnemonic( menu, "Grid1024", "SetGrid1024" )
	};
	// make them radio
	auto *group = new QActionGroup( menu );
	for( auto *action : g_gridActions ){
		action->setCheckable( true );
		action->setActionGroup( group );
	}
	// activate current
	gridActions_setChecked( static_cast<GridPower>( g_grid_power ) );
}

void Grid_registerShortcuts(){
//	command_connect_accelerator( "ToggleGrid" );
	command_connect_accelerator( "GridDown" );
	command_connect_accelerator( "GridUp" );
	command_connect_accelerator( "ToggleGridSnap" );
}

void Grid_constructPreferences( PreferencesPage& page ){
	page.appendCombo(
	    "Default grid spacing",
	    g_grid_default,
	    StringArrayRange( g_gridnames )
	);
	{
		const char* coords[] = { "4096", "8192", "16384", "32768", "65536" };

		page.appendCombo(
		    "Max grid coordinate",
		    StringArrayRange( coords ),
		    IntImportCallback( maxGridCoordPowerImportCaller() ),
		    IntExportCallback( maxGridCoordPowerExportCaller() )
		);
	}
}
void Grid_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Grid", "Grid Settings" ) );
	Grid_constructPreferences( page );
}
void Grid_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, Grid_constructPage>() );
}

void Grid_construct(){
	Grid_registerPreferencesPage();

	g_grid_default = GridDefault_forGridPower( GRIDPOWER_16 );

	GlobalPreferenceSystem().registerPreference( "GridDefault", IntImportStringCaller( g_grid_default ), IntExportStringCaller( g_grid_default ) );

	g_grid_power = GridPower_forGridDefault( g_grid_default );
	g_gridsize = GridSize_forGridPower( g_grid_power );

	GlobalPreferenceSystem().registerPreference( "GridMaxCoordPower", IntImportStringCaller( g_maxGridCoordPower ), IntExportStringCaller( g_maxGridCoordPower ) );
	maxGridCoordPowerImport( g_maxGridCoordPower ); // call manually to also work, when no preference was loaded (1st start)
}

void Grid_destroy(){
}

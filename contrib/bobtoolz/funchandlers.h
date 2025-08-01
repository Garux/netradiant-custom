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

#pragma once

#include <memory>

extern std::unique_ptr<class DBobView> g_PathView;
extern std::unique_ptr<class DVisDrawer> g_VisView;
extern std::unique_ptr<class DTrainDrawer> g_TrainView;
extern std::unique_ptr<class DTreePlanter> g_TreePlanter;

// intersect stuff
#define BRUSH_OPT_WHOLE_MAP 0
#define BRUSH_OPT_SELECTED  1

// defines for stairs
#define MOVE_NORTH  0
#define MOVE_SOUTH  1
#define MOVE_EAST   2
#define MOVE_WEST   3

#define STYLE_ORIGINAL  0
#define STYLE_BOB       1
#define STYLE_CORNER    2

// defines for doors
#define DIRECTION_NS    0
#define DIRECTION_EW    1

// help
void LoadLists();


// djbob
void DoIntersect();
void DoFindDuplicates();
void DoPolygonsTB();
void DoPolygons();
void DoFixBrushes();
void DoResetTextures();
void DoBuildStairs();
void DoBuildDoors();
void DoBuildApertureDoors();
void DoPathPlotter();
void DoPitBuilder();
void DoCTFColourChanger();
void DoMergePatches();
void DoSplitPatch();
void DoSplitPatchRows();
void DoSplitPatchCols();
void DoVisAnalyse();
void DoTrainThing();
void DoTrainPathPlot();
void DoCaulkSelection();
void DoTreePlanter();
void DoDropEnts();
void DoMakeChain();
void DoFlipTerrain();
void DoExplodeEntity();

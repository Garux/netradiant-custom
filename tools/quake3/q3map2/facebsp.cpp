/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"



static int c_faceLeafs;



/*
   SelectSplitPlaneNum()
   finds the best split plane for this node
 */

static void SelectSplitPlaneNum( const node_t *node, const facelist_t& list, int *splitPlaneNum, int *compileFlags ){
	//int frontC, backC, splitsC, facingC;


	/* ydnar: set some defaults */
	*splitPlaneNum = PLANENUM_LEAF; /* leaf */
	*compileFlags = 0;

	/* ydnar 2002-06-24: changed this to split on z-axis as well */
	/* ydnar 2002-09-21: changed blocksize to be a vector, so mappers can specify a 3 element value */

	/* if it is crossing a block boundary, force a split */
	for ( int i = 0; i < 3; i++ )
	{
		if ( blockSize[ i ] <= 0 ) {
			continue;
		}
		const float dist = blockSize[ i ] * ( floor( node->minmax.mins[ i ] / blockSize[ i ] ) + 1 );
		if ( node->minmax.maxs[ i ] > dist ) {
			*splitPlaneNum = FindFloatPlane( g_vector3_axes[i], dist, 0, NULL );
			return;
		}
	}

	/* pick one of the face planes */
	int bestValue = -99999;
	const face_t *bestSplit = nullptr;


	// div0: this check causes detail/structural mixes
	//for( face_t& split : list )
	//	split.checked = false;

	for ( const face_t& split : list )
	{
		//if ( split->checked )
		//	continue;

		const plane_t& plane = mapplanes[ split.planenum ];
		int splits = 0;
		int facing = 0;
		int front = 0;
		int back = 0;
		for ( const face_t& check : list ) {
			if ( check.planenum == split.planenum ) {
				facing++;
				//check->checked = true;	// won't need to test this plane again
				continue;
			}
			const EPlaneSide side = WindingOnPlaneSide( check.w, plane.plane );
			if ( side == eSideCross ) {
				splits++;
			}
			else if ( side == eSideFront ) {
				front++;
			}
			else if ( side == eSideBack ) {
				back++;
			}
		}

		int value;
		if ( bspAlternateSplitWeights ) {
			// from 27

			//Bigger is better
			const float sizeBias = WindingArea( split.w );

			//Base score = 20000 perfectly balanced
			value = 20000 - ( abs( front - back ) );
			value -= plane.counter; // If we've already used this plane sometime in the past try not to use it again
			value -= facing;        // if we're going to have alot of other surfs use this plane, we want to get it in quickly.
			value -= splits * 5;        //more splits = bad
			value +=  sizeBias * 10; //We want a huge score bias based on plane size
		}
		else
		{
			value =  5 * facing - 5 * splits; // - abs(front-back);
			if ( plane.type < ePlaneNonAxial ) {
				value += 5;       // axial is better
			}
		}

		value += split.priority;       // prioritize hints higher

		if ( value > bestValue ) {
			bestValue = value;
			bestSplit = &split;
			//frontC = front;
			//backC = back;
			//splitsC = splits;
			//facingC = facing;
		}
	}

	/* nothing, we have a leaf */
	if ( bestValue == -99999 ) {
		return;
	}

	//Sys_FPrintf( SYS_VRB, "F: %d B:%d S:%d FA:%ds\n", frontC, backC, splitsC, facingC );

	/* set best split data */
	*splitPlaneNum = bestSplit->planenum;
	*compileFlags = bestSplit->compileFlags;

#if 0
	if ( bestSplit->compileFlags & C_DETAIL ) {
		for ( const face_t& split : list )
			if ( !( split.compileFlags & C_DETAIL ) ) {
				Sys_FPrintf( SYS_ERR, "DON'T DO SUCH SPLITS (1)\n" );
			}
	}
	if ( ( node->compileFlags & C_DETAIL ) && !( bestSplit->compileFlags & C_DETAIL ) ) {
		Sys_FPrintf( SYS_ERR, "DON'T DO SUCH SPLITS (2)\n" );
	}
#endif

	if ( *splitPlaneNum > -1 ) {
		mapplanes[ *splitPlaneNum ].counter++;
	}
}



/*
   BuildFaceTree_r()
   recursively builds the bsp, splitting on face planes
 */

static void BuildFaceTree_r( node_t *node, facelist_t& list ){
	facelist_t childLists[2];
	int splitPlaneNum, compileFlags;
#if 0
	bool isstruct = false;
#endif


	/* select the best split plane */
	SelectSplitPlaneNum( node, list, &splitPlaneNum, &compileFlags );

	/* if we don't have any more faces, this is a node */
	if ( splitPlaneNum == PLANENUM_LEAF ) {
		node->planenum = PLANENUM_LEAF;
		node->has_structural_children = false;
		c_faceLeafs++;
		return;
	}

	/* partition the list */
	node->planenum = splitPlaneNum;
	node->compileFlags = compileFlags;
	node->has_structural_children = !( compileFlags & C_DETAIL ) && !node->opaque;
	const plane_t& plane = mapplanes[ splitPlaneNum ];

	while ( !list.empty() )
	{
		const face_t& split = list.front();
		/* don't split by identical plane */
		if ( split.planenum == node->planenum ) {
			list.pop_front();
			continue;
		}

#if 0
		if ( !( split.compileFlags & C_DETAIL ) ) {
			isstruct = true;
		}
#endif

		/* determine which side the face falls on */
		const EPlaneSide side = WindingOnPlaneSide( split.w, plane.plane );

		/* switch on side */
		if ( side == eSideCross ) {
			auto [frontWinding, backWinding] = ClipWindingEpsilonStrict( split.w, plane.plane, CLIP_EPSILON * 2 ); /* strict; if no winding is left, we have a "virtually identical" plane and don't want to split by it */
			if ( !frontWinding.empty() ) {
				face_t& newFace = childLists[0].emplace_front();
				newFace.w.swap( frontWinding );
				newFace.planenum = split.planenum;
				newFace.priority = split.priority;
				newFace.compileFlags = split.compileFlags;
			}
			if ( !backWinding.empty() ) {
				face_t& newFace = childLists[1].emplace_front();
				newFace.w.swap( backWinding );
				newFace.planenum = split.planenum;
				newFace.priority = split.priority;
				newFace.compileFlags = split.compileFlags;
			}
			list.pop_front();
		}
		else if ( side == eSideFront ) {
			childLists[0].splice_after( childLists[0].cbefore_begin(), list, list.cbefore_begin() );
		}
		else if ( side == eSideBack ) {
			childLists[1].splice_after( childLists[1].cbefore_begin(), list, list.cbefore_begin() );
		}
		else{ // eSideOn
			list.pop_front();
		}
	}


	// recursively process children
	for ( int i = 0; i < 2; i++ ) {
		node->children[i] = AllocNode();
		node->children[i]->parent = node;
		node->children[i]->minmax = node->minmax;
	}

	for ( int i = 0; i < 3; i++ ) {
		if ( plane.normal()[i] == 1 ) {
			node->children[0]->minmax.mins[i] = plane.dist();
			node->children[1]->minmax.maxs[i] = plane.dist();
			break;
		}
		if ( plane.normal()[i] == -1 ) {
			node->children[0]->minmax.maxs[i] = -plane.dist();
			node->children[1]->minmax.mins[i] = -plane.dist();
			break;
		}
	}

#if 0
	if ( ( node->compileFlags & C_DETAIL ) && isstruct ) {
		Sys_FPrintf( SYS_ERR, "I am detail, my child is structural, this is a wtf1\n", node->has_structural_children );
	}
#endif

	for ( int i = 0; i < 2; i++ ) {
		BuildFaceTree_r( node->children[i], childLists[i] );
		node->has_structural_children |= node->children[i]->has_structural_children;
	}

#if 0
	if ( ( node->compileFlags & C_DETAIL ) && !( node->children[0]->compileFlags & C_DETAIL ) && node->children[0]->planenum != PLANENUM_LEAF ) {
		Sys_FPrintf( SYS_ERR, "I am detail, my child is structural\n", node->has_structural_children );
	}
	if ( ( node->compileFlags & C_DETAIL ) && isstruct ) {
		Sys_FPrintf( SYS_ERR, "I am detail, my child is structural, this is a wtf2\n", node->has_structural_children );
	}
#endif
}


/*
   ================
   FaceBSP

   List will be freed before returning
   ================
 */
tree_t FaceBSP( facelist_t& list ) {
	Sys_FPrintf( SYS_VRB, "--- FaceBSP ---\n" );

	tree_t tree{};

	int count = 0;
	for ( const face_t& face : list )
	{
		WindingExtendBounds( face.w, tree.minmax );
		count++;
	}
	Sys_FPrintf( SYS_VRB, "%9d faces\n", count );

	for ( plane_t& plane : mapplanes )
	{
		plane.counter = 0;
	}

	tree.headnode = AllocNode();
	tree.headnode->minmax = tree.minmax;
	c_faceLeafs = 0;

	BuildFaceTree_r( tree.headnode, list );

	Sys_FPrintf( SYS_VRB, "%9d leafs\n", c_faceLeafs );

	return tree;
}



/*
   MakeStructuralBSPFaceList()
   get structural brush faces
 */

#define HINT_PRIORITY           1000        /* ydnar: force hint splits first and antiportal/areaportal splits last */
#define ANTIPORTAL_PRIORITY     -1000
#define AREAPORTAL_PRIORITY     -1000
#define DETAIL_PRIORITY         -3000


facelist_t MakeStructuralBSPFaceList( const brushlist_t& list ){
	facelist_t flist;

	for ( const brush_t &b : list )
	{
		if ( !deepBSP && b.detail ) {
			continue;
		}

		if( std::ranges::count_if( b.sides, []( const side_t& side ){ return !side.winding.empty(); } ) < 4 ){
			xml_Select( "ignoring malformed structural brush: sides < 4: would break bsp tree", b.entityNum, b.brushNum, false );
			const_cast<brush_t&>( b ).detail = true;
			continue;
		}

		for ( const side_t& s : b.sides )
		{
			/* get winding */
			const winding_t& w = s.winding;
			if ( w.empty() ) {
				continue;
			}

			/* ydnar: skip certain faces */
			if ( s.compileFlags & C_SKIP ) {
				continue;
			}

			/* allocate a face */
			face_t& f = flist.emplace_front();
			f.w = w;
			f.planenum = s.planenum & ~1;
			f.compileFlags = s.compileFlags;  /* ydnar */
			if ( b.detail ) {
				f.compileFlags |= C_DETAIL;
			}

			/* ydnar: set priority */
			f.priority = 0;
			if ( f.compileFlags & C_HINT ) {
				f.priority += HINT_PRIORITY;
			}
			if ( f.compileFlags & C_ANTIPORTAL ) {
				f.priority += ANTIPORTAL_PRIORITY;
			}
			if ( f.compileFlags & C_AREAPORTAL ) {
				f.priority += AREAPORTAL_PRIORITY;
			}
			if ( f.compileFlags & C_DETAIL ) {
				f.priority += DETAIL_PRIORITY;
			}
		}
	}

	return flist;
}



/*
   MakeVisibleBSPFaceList()
   get visible brush faces
 */

facelist_t MakeVisibleBSPFaceList( const brushlist_t& list ){
	facelist_t flist;

	for ( const brush_t& b : list )
	{
		if ( !deepBSP && b.detail ) {
			continue;
		}

		for ( const side_t& s : b.sides )
		{
			/* get winding */
			const winding_t& w = s.visibleHull;
			if ( w.empty() ) {
				continue;
			}

			/* ydnar: skip certain faces */
			if ( s.compileFlags & C_SKIP ) {
				continue;
			}

			/* allocate a face */
			face_t& f = flist.emplace_front();
			f.w = w;
			f.planenum = s.planenum & ~1;
			f.compileFlags = s.compileFlags;  /* ydnar */
			if ( b.detail ) {
				f.compileFlags |= C_DETAIL;
			}

			/* ydnar: set priority */
			f.priority = 0;
			if ( f.compileFlags & C_HINT ) {
				f.priority += HINT_PRIORITY;
			}
			if ( f.compileFlags & C_ANTIPORTAL ) {
				f.priority += ANTIPORTAL_PRIORITY;
			}
			if ( f.compileFlags & C_AREAPORTAL ) {
				f.priority += AREAPORTAL_PRIORITY;
			}
			if ( f.compileFlags & C_DETAIL ) {
				f.priority += DETAIL_PRIORITY;
			}
		}
	}

	return flist;
}

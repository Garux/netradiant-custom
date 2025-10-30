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


/*
   PortalPassable
   returns true if the portal has non-opaque leafs on both sides
 */

bool PortalPassable( const portal_t *p ){
	/* is this to global outside leaf? */
	if ( !p->onnode ) {
		return false;
	}

	/* this should never happen */
	if ( p->nodes[eFront]->planenum != PLANENUM_LEAF ||
	     p->nodes[eBack]->planenum != PLANENUM_LEAF ) {
		Error( "Portal_EntityFlood: not a leaf" );
	}

	/* ydnar: added antiportal to suppress portal generation for visibility blocking */
	if ( p->compileFlags & C_ANTIPORTAL ) {
		return false;
	}

	/* both leaves on either side of the portal must be passable */
	if ( !p->nodes[eFront]->opaque && !p->nodes[eBack]->opaque ) {
		return true;
	}

	/* otherwise this isn't a passable portal */
	return false;
}




static int c_tinyportals;
static int c_badportals;       /* ydnar */

/*
   =============
   AddPortalToNodes
   =============
 */
static void AddPortalToNodes( portal_t *p, node_t *front, node_t *back ){
	if ( p->nodes[eFront] || p->nodes[eBack] ) {
		Error( "AddPortalToNode: already included" );
	}

	p->nodes[eFront] = front;
	p->next[eFront] = front->portals;
	front->portals = p;

	p->nodes[eBack] = back;
	p->next[eBack] = back->portals;
	back->portals = p;
}


/*
   =============
   RemovePortalFromNode
   =============
 */
void RemovePortalFromNode( portal_t *portal, node_t *l ){
	portal_t    **pp, *t;

// remove reference to the current portal
	pp = &l->portals;
	while ( true )
	{
		t = *pp;
		if ( !t ) {
			Error( "RemovePortalFromNode: portal not in leaf" );
		}

		if ( t == portal ) {
			break;
		}

		if ( t->nodes[eFront] == l ) {
			pp = &t->next[eFront];
		}
		else if ( t->nodes[eBack] == l ) {
			pp = &t->next[eBack];
		}
		else{
			Error( "RemovePortalFromNode: portal not bounding leaf" );
		}
	}

	if ( portal->nodes[eFront] == l ) {
		*pp = portal->next[eFront];
		portal->nodes[eFront] = nullptr;
	}
	else if ( portal->nodes[eBack] == l ) {
		*pp = portal->next[eBack];
		portal->nodes[eBack] = nullptr;
	}
}

//============================================================================

static void PrintPortal( const portal_t *p ){
	for ( const Vector3& point : p->winding )
		Sys_Printf( "(%5.0f,%5.0f,%5.0f)\n", point[0], point[1], point[2] );
}

/*
   ================
   MakeHeadnodePortals

   The created portals will face the global outside_node
   ================
 */
#define SIDESPACE   8
static void MakeHeadnodePortals( tree_t& tree ){
	portal_t    *portals[6];

// pad with some space so there will never be null volume leafs
	const MinMax bounds( tree.minmax.mins - Vector3( SIDESPACE ),
	                     tree.minmax.maxs + Vector3( SIDESPACE ) );
	if ( !bounds.valid() ) {
		Error( "Backwards tree volume" );
	}

	tree.outside_node.planenum = PLANENUM_LEAF;
	tree.outside_node.brushlist.clear();
	tree.outside_node.portals = nullptr;
	tree.outside_node.opaque = false;

	for ( int i = 0; i < 3; ++i )
		for ( int j = 0; j < 2; ++j )
		{
			portal_t *p = portals[j * 3 + i] = AllocPortal();

			if ( j ) {
				p->plane.plane = Plane3f( -g_vector3_axes[i], -bounds.maxs[i] );
			}
			else
			{
				p->plane.plane = Plane3f( g_vector3_axes[i], bounds.mins[i] );
			}
			p->winding = BaseWindingForPlane( p->plane.plane );
			AddPortalToNodes( p, tree.headnode, &tree.outside_node );
		}

// clip the basewindings by all the other planes
	for ( int i = 0; i < 6; ++i )
	{
		for ( int j = 0; j < 6; ++j )
		{
			if ( j == i ) {
				continue;
			}
			ChopWindingInPlace( portals[i]->winding, portals[j]->plane.plane, ON_EPSILON );
		}
	}
}

//===================================================


/*
   ================
   BaseWindingForNode
   ================
 */
#define BASE_WINDING_EPSILON    0.001
#define SPLIT_WINDING_EPSILON   0.001

static winding_t   BaseWindingForNode( const node_t *node ){
	winding_t w = BaseWindingForPlane( mapplanes[node->planenum].plane );

	// clip by all the parents
	for ( const node_t *n = node->parent; n && !w.empty(); )
	{
		const plane_t& plane = mapplanes[n->planenum];

		if ( n->children[eFront] == node ) { // take front
			ChopWindingInPlace( w, plane.plane, BASE_WINDING_EPSILON );
		}
		else
		{	// take back
			ChopWindingInPlace( w, plane3_flipped( plane.plane ), BASE_WINDING_EPSILON );
		}
		node = n;
		n = n->parent;
	}

	return w;
}

//============================================================

/*
   ==================
   MakeNodePortal

   create the new portal by taking the full plane winding for the cutting plane
   and clipping it by all of parents of this node
   ==================
 */
static void MakeNodePortal( node_t *node ){
	winding_t w = BaseWindingForNode( node );

	// clip the portal by all the other portals in the node
	ESide side;
	for ( const portal_t *p = node->portals; p && !w.empty(); p = p->next[side] )
	{
		if ( p->nodes[eFront] == node ) {
			side = eFront;
			ChopWindingInPlace( w, p->plane.plane, CLIP_EPSILON );
		}
		else if ( p->nodes[eBack] == node ) {
			side = eBack;
			ChopWindingInPlace( w, plane3_flipped( p->plane.plane ), CLIP_EPSILON );
		}
		else{
			Error( "CutNodePortals_r: mislinked portal" );
		}
	}

	if ( w.empty() ) {
		return;
	}


	/* ydnar: adding this here to fix degenerate windings */
	#if 0
	if ( !FixWinding( w ) ) {
		c_badportals++;
		return;
	}
	#endif

	if ( WindingIsTiny( w ) ) {
		c_tinyportals++;
		return;
	}

	portal_t *new_portal = AllocPortal();
	new_portal->plane = mapplanes[node->planenum];
	new_portal->onnode = node;
	new_portal->winding.swap( w );
	new_portal->compileFlags = node->compileFlags;
	AddPortalToNodes( new_portal, node->children[eFront], node->children[eBack] );
}


/*
   ==============
   SplitNodePortals

   Move or split the portals that bound node so that the node's
   children have portals instead of node.
   ==============
 */
static void SplitNodePortals( node_t *node ){
	const plane_t& plane = mapplanes[node->planenum];
	node_t *front = node->children[eFront];
	node_t *back = node->children[eBack];

	for ( portal_t *next_portal, *p = node->portals; p; p = next_portal )
	{
		ESide side;
		if ( p->nodes[eFront] == node ) {
			side = eFront;
		}
		else if ( p->nodes[eBack] == node ) {
			side = eBack;
		}
		else{
			Error( "SplitNodePortals: mislinked portal" );
		}
		next_portal = p->next[side];

		node_t *other_node = p->nodes[!side];
		RemovePortalFromNode( p, p->nodes[eFront] );
		RemovePortalFromNode( p, p->nodes[eBack] );

//
// cut the portal into two portals, one on each side of the cut plane
//
		auto [frontwinding, backwinding] = ClipWindingEpsilon( p->winding, plane.plane, SPLIT_WINDING_EPSILON ); /* not strict, we want to always keep one of them even if coplanar */

		if ( !frontwinding.empty() && WindingIsTiny( frontwinding ) ) {
			if ( !front->tinyportals ) {
				front->referencepoint = frontwinding[0];
			}
			front->tinyportals++;
			if ( !other_node->tinyportals ) {
				other_node->referencepoint = frontwinding[0];
			}
			other_node->tinyportals++;

			frontwinding.clear();
			c_tinyportals++;
		}

		if ( !backwinding.empty() && WindingIsTiny( backwinding ) ) {
			if ( !back->tinyportals ) {
				back->referencepoint = backwinding[0];
			}
			back->tinyportals++;
			if ( !other_node->tinyportals ) {
				other_node->referencepoint = backwinding[0];
			}
			other_node->tinyportals++;

			backwinding.clear();
			c_tinyportals++;
		}

		if ( frontwinding.empty() && backwinding.empty() ) { // tiny windings on both sides
			continue;
		}

		if ( frontwinding.empty() ) {
			if ( side == eFront ) {
				AddPortalToNodes( p, back, other_node );
			}
			else{
				AddPortalToNodes( p, other_node, back );
			}
			continue;
		}
		if ( backwinding.empty() ) {
			if ( side == eFront ) {
				AddPortalToNodes( p, front, other_node );
			}
			else{
				AddPortalToNodes( p, other_node, front );
			}
			continue;
		}

		// the winding is split
		p->winding.clear();
		auto *new_portal = new portal_t( *p ); // AllocPortal()
		new_portal->winding.swap( backwinding );
		p->winding.swap( frontwinding );

		if ( side == eFront ) {
			AddPortalToNodes( p, front, other_node );
			AddPortalToNodes( new_portal, back, other_node );
		}
		else
		{
			AddPortalToNodes( p, other_node, front );
			AddPortalToNodes( new_portal, other_node, back );
		}
	}

	node->portals = nullptr;
}


/*
   ================
   CalcNodeBounds
   ================
 */
static void CalcNodeBounds( node_t *node ){
	// calc mins/maxs for both leafs and nodes
	node->minmax.clear();
	for ( const portal_t *p = node->portals; p; p = p->nextPortal( node ) )
	{
		WindingExtendBounds( p->winding, node->minmax );
	}
}

/*
   ==================
   MakeTreePortals_r
   ==================
 */
static void MakeTreePortals_r( node_t *node ){
	CalcNodeBounds( node );
	if ( !node->minmax.valid() ) {
		Sys_Warning( "node without a volume\n"
		             "node has %d tiny portals\n"
		             "node reference point %1.2f %1.2f %1.2f\n",
		             node->tinyportals,
		             node->referencepoint[0],
		             node->referencepoint[1],
		             node->referencepoint[2] );
	}

	if ( !c_worldMinmax.surrounds( node->minmax ) ) {
		if ( node->portals && !node->portals->winding.empty() ) {
			xml_Winding( "WARNING: Node With Unbounded Volume", node->portals->winding.data(), node->portals->winding.size(), false );
		}
	}
	if ( node->planenum == PLANENUM_LEAF ) {
		return;
	}

	MakeNodePortal( node );
	SplitNodePortals( node );

	MakeTreePortals_r( node->children[0] );
	MakeTreePortals_r( node->children[1] );
}

/*
   ==================
   MakeTreePortals
   ==================
 */
void MakeTreePortals( tree_t& tree ){
	Sys_FPrintf( SYS_VRB, "--- MakeTreePortals ---\n" );
	MakeHeadnodePortals( tree );
	MakeTreePortals_r( tree.headnode );
	Sys_FPrintf( SYS_VRB, "%9d tiny portals\n", c_tinyportals );
	Sys_FPrintf( SYS_VRB, "%9d bad portals\n", c_badportals );  /* ydnar */
}

/*
   =========================================================

   FLOOD ENTITIES

   =========================================================
 */

static int c_floodedleafs;

static void FloodPortals( node_t *startNode, bool skybox ){
	int dist = 1;
	std::vector<node_t*> nodes{ startNode }, nodes2;
	while( !nodes.empty() ){
		for( node_t *node : nodes ){
			node->skybox |= skybox;

			if ( node->opaque || ( node->occupied && node->occupied <= dist ) ) { // also reprocess occupied nodes for shorter leak line
				continue;
			}

			if( !node->occupied ){
				++c_floodedleafs;
			}

			node->occupied = dist;

			for ( const portal_t *p = node->portals; p; p = p->nextPortal( node ) )
			{
				nodes2.push_back( p->otherNode( node ) );
			}
		}
		nodes.swap( nodes2 );
		nodes2.clear();
		++dist;
	}
}



/*
   =============
   PlaceOccupant
   =============
 */

static bool PlaceOccupant( node_t *headnode, const Vector3& origin, const entity_t *occupant, bool skybox ){
	// find the leaf to start in
	node_t *node = headnode;
	while ( node->planenum != PLANENUM_LEAF )
	{
		if ( plane3_distance_to_point( mapplanes[ node->planenum ].plane, origin ) >= 0 ) {
			node = node->children[eFront];
		}
		else{
			node = node->children[eBack];
		}
	}

	if ( node->opaque ) {
		return false;
	}
	node->occupant = occupant;
	node->skybox |= skybox;

	FloodPortals( node, skybox );

	return true;
}

/*
   =============
   FloodEntities

   Marks all nodes that can be reached by entites
   =============
 */

EFloodEntities FloodEntities( tree_t& tree ){
	Sys_FPrintf( SYS_VRB, "--- FloodEntities ---\n" );

	bool inside = false;
	tree.outside_node.occupied = 0;
	c_floodedleafs = 0;

	for ( std::size_t i = 1; i < entities.size(); ++i )
	{
		/* get entity */
		const entity_t& e = entities[ i ];

		/* get origin */
		Vector3 origin( e.vectorForKey( "origin" ) );
#if 0 // 0 = allow maps with only point entity@( 0, 0, 0 ); assuming that entities, containing no primitives are point ones
		/* as a special case, allow origin-less entities */
		if ( VectorCompare( origin, g_vector3_identity ) ) {
			continue;
		}
#endif
		/* also allow bmodel entities outside, as they could be on a moving path that will go into the map */
		if ( !e.brushes.empty() || !e.patches.empty() || e.classname_is( "_decal" ) ) { //_decal primitive is freed at this point
			continue;
		}

		/* handle skybox entities */
		const bool skybox = e.classname_is( "_skybox" );
		if ( skybox ) {
			/* get scale */
			Vector3 scale( 64 );
			if( !e.read_keyvalue( scale, "_scale" ) )
				if( e.read_keyvalue( scale[0], "_scale" ) )
					scale[1] = scale[2] = scale[0];

			/* get "angle" (yaw) or "angles" (pitch yaw roll), store as (roll pitch yaw) */
			Vector3 angles( 0 );
			if ( e.read_keyvalue( angles, "angles" ) || e.read_keyvalue( angles.y(), "angle" ) )
				angles = angles_pyr2rpy( angles );

			/* set transform matrix (thanks spog) */
			skyboxTransform = g_matrix4_identity;
			matrix4_pivoted_transform_by_euler_xyz_degrees( skyboxTransform, -origin, angles, scale, origin );
		}

		/* nudge off floor */
		origin[ 2 ] += 1;

		/* debugging code */
		//%	if( i == 1 )
		//%		origin[ 2 ] += 4096;

		/* find leaf */
		if ( PlaceOccupant( tree.headnode, origin, &e, skybox ) )
			inside = true;
		else
			Sys_FPrintf( SYS_WRN, "Entity %i (%s): Entity in solid\n", e.mapEntityNum, e.classname() );
	}

	Sys_FPrintf( SYS_VRB, "%9d flooded leafs\n", c_floodedleafs );

	if ( !inside ) {
		Sys_FPrintf( SYS_WRN | SYS_VRBflag, "no entities in open -- no filling\n" );
		return EFloodEntities::Empty;
	}
	if ( tree.outside_node.occupied ) {
		Sys_FPrintf( SYS_WRN | SYS_VRBflag, "entity reached from outside -- leak detected\n" );
		return EFloodEntities::Leaked;
	}

	return EFloodEntities::Good;
}

/*
   =========================================================

   FLOOD AREAS

   =========================================================
 */

static int c_areas;



/*
   FloodAreas_r()
   floods through leaf portals to tag leafs with an area
 */

static void FloodAreas( node_t *startNode ){
	std::vector<node_t*> nodes{ startNode }, nodes2;
	while( !nodes.empty() ){
		for( node_t *node : nodes )
		{
			if ( node->area != AREA_INVALID ) {
				continue;
			}
			if ( node->cluster == CLUSTER_OPAQUE ) {
				continue;
			}

			node->area = c_areas;

			/* ydnar: skybox nodes set the skybox area */
			if ( node->skybox ) {
				skyboxArea = c_areas;
			}

			for ( const portal_t *p = node->portals; p; p = p->nextPortal( node ) )
			{
				/* ydnar: allow areaportal portals to block area flow */
				/* this check alone w/o node->areaportal path seems sufficient
				   besides when node->compileFlags are overriden by hint or struct split flush with areaportal
				   we make it persistent in FilterBrushIntoTree_r()
				   note: node->areaportal way fails for leafs with only opaque and areaportal portals */
				if ( p->compileFlags & C_AREAPORTAL ) {
					continue;
				}

				if ( !PortalPassable( p ) ) {
					continue;
				}

				nodes2.push_back( p->otherNode( node ) );
			}
		}
		nodes.swap( nodes2 );
		nodes2.clear();
	}
}

/*
   =============
   FindAreas_r

   Just decend the tree, and for each node that hasn't had an
   area set, flood fill out from there
   =============
 */
static void FindAreas_r( node_t *node ){
	if ( node->planenum != PLANENUM_LEAF ) {
		FindAreas_r( node->children[ 0 ] );
		FindAreas_r( node->children[ 1 ] );
		return;
	}

	if ( node->opaque || node->area != AREA_INVALID ) {
		return;
	}

	FloodAreas( node );
	c_areas++;
}

/*
   =============
   CheckAreas_r
   =============
 */
static void CheckAreas_r( const node_t *node ){
	if ( node->planenum != PLANENUM_LEAF ) {
		CheckAreas_r( node->children[0] );
		CheckAreas_r( node->children[1] );
		return;
	}

	if ( node->opaque ) {
		return;
	}

	if ( node->cluster != CLUSTER_OPAQUE ) {
		if ( node->area == AREA_INVALID ) {
			Sys_Warning( "cluster %d has area set to -1\n", node->cluster );
		}
	}
}



/*
   FloodSkyboxArea_r() - ydnar
   sets all nodes with the skybox area to skybox
 */

static void FloodSkyboxArea_r( node_t *node ){
	if ( node->planenum != PLANENUM_LEAF ) {
		FloodSkyboxArea_r( node->children[ 0 ] );
		FloodSkyboxArea_r( node->children[ 1 ] );
		return;
	}

	if ( node->opaque || node->area != skyboxArea ) {
		return;
	}

	node->skybox = true;
}



/*
   FloodAreas()
   mark each leaf with an area, bounded by C_AREAPORTAL
 */

void FloodAreas( tree_t& tree ){
	Sys_FPrintf( SYS_VRB, "--- FloodAreas ---\n" );
	FindAreas_r( tree.headnode );

	/* ydnar: flood all skybox nodes */
	if ( skyboxArea != AREA_INVALID )
		FloodSkyboxArea_r( tree.headnode );

	/* check for areaportal brushes that don't touch two areas */
	/* ydnar: fix this rather than just silence the warnings */
	//%	CheckAreas_r( tree.headnode );

	Sys_FPrintf( SYS_VRB, "%9d areas\n", c_areas );
}



//======================================================

static int c_outside;
static int c_inside;
static int c_solid;

static void FillOutside_r( node_t *node ){
	if ( node->planenum != PLANENUM_LEAF ) {
		FillOutside_r( node->children[0] );
		FillOutside_r( node->children[1] );
		return;
	}

	// anything not reachable by an entity
	// can be filled away
	if ( !node->occupied ) {
		if ( !node->opaque ) {
			c_outside++;
			node->opaque = true;
		}
		else {
			c_solid++;
		}
	}
	else {
		c_inside++;
	}
}

/*
   =============
   FillOutside

   Fill all nodes that can't be reached by entities
   =============
 */
void FillOutside( node_t *headnode ){
	c_outside = 0;
	c_inside = 0;
	c_solid = 0;
	Sys_FPrintf( SYS_VRB, "--- FillOutside ---\n" );
	FillOutside_r( headnode );
	Sys_FPrintf( SYS_VRB, "%9d solid leafs\n", c_solid );
	Sys_Printf( "%9d leafs filled\n", c_outside );
	Sys_FPrintf( SYS_VRB, "%9d inside leafs\n", c_inside );
}


//==============================================================

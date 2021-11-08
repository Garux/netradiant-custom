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
	if ( p->nodes[ 0 ]->planenum != PLANENUM_LEAF ||
	     p->nodes[ 1 ]->planenum != PLANENUM_LEAF ) {
		Error( "Portal_EntityFlood: not a leaf" );
	}

	/* ydnar: added antiportal to suppress portal generation for visibility blocking */
	if ( p->compileFlags & C_ANTIPORTAL ) {
		return false;
	}

	/* both leaves on either side of the portal must be passable */
	if ( !p->nodes[ 0 ]->opaque && !p->nodes[ 1 ]->opaque ) {
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
	if ( p->nodes[0] || p->nodes[1] ) {
		Error( "AddPortalToNode: already included" );
	}

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;

	p->nodes[1] = back;
	p->next[1] = back->portals;
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
	while ( 1 )
	{
		t = *pp;
		if ( !t ) {
			Error( "RemovePortalFromNode: portal not in leaf" );
		}

		if ( t == portal ) {
			break;
		}

		if ( t->nodes[0] == l ) {
			pp = &t->next[0];
		}
		else if ( t->nodes[1] == l ) {
			pp = &t->next[1];
		}
		else{
			Error( "RemovePortalFromNode: portal not bounding leaf" );
		}
	}

	if ( portal->nodes[0] == l ) {
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if ( portal->nodes[1] == l ) {
		*pp = portal->next[1];
		portal->nodes[1] = NULL;
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
	tree.outside_node.portals = NULL;
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

		if ( n->children[0] == node ) { // take front
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
	int side;

	winding_t w = BaseWindingForNode( node );

	// clip the portal by all the other portals in the node
	for ( const portal_t *p = node->portals; p && !w.empty(); p = p->next[side] )
	{
		if ( p->nodes[0] == node ) {
			side = 0;
			ChopWindingInPlace( w, p->plane.plane, CLIP_EPSILON );
		}
		else if ( p->nodes[1] == node ) {
			side = 1;
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
	AddPortalToNodes( new_portal, node->children[0], node->children[1] );
}


/*
   ==============
   SplitNodePortals

   Move or split the portals that bound node so that the node's
   children have portals instead of node.
   ==============
 */
static void SplitNodePortals( node_t *node ){
	node_t      *f, *b, *other_node;

	const plane_t& plane = mapplanes[node->planenum];
	f = node->children[0];
	b = node->children[1];

	for ( portal_t *next_portal, *p = node->portals; p; p = next_portal )
	{
		int side;
		if ( p->nodes[0] == node ) {
			side = 0;
		}
		else if ( p->nodes[1] == node ) {
			side = 1;
		}
		else{
			Error( "SplitNodePortals: mislinked portal" );
		}
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode( p, p->nodes[0] );
		RemovePortalFromNode( p, p->nodes[1] );

//
// cut the portal into two portals, one on each side of the cut plane
//
		auto [frontwinding, backwinding] = ClipWindingEpsilon( p->winding, plane.plane, SPLIT_WINDING_EPSILON ); /* not strict, we want to always keep one of them even if coplanar */

		if ( !frontwinding.empty() && WindingIsTiny( frontwinding ) ) {
			if ( !f->tinyportals ) {
				f->referencepoint = frontwinding[0];
			}
			f->tinyportals++;
			if ( !other_node->tinyportals ) {
				other_node->referencepoint = frontwinding[0];
			}
			other_node->tinyportals++;

			frontwinding.clear();
			c_tinyportals++;
		}

		if ( !backwinding.empty() && WindingIsTiny( backwinding ) ) {
			if ( !b->tinyportals ) {
				b->referencepoint = backwinding[0];
			}
			b->tinyportals++;
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
			if ( side == 0 ) {
				AddPortalToNodes( p, b, other_node );
			}
			else{
				AddPortalToNodes( p, other_node, b );
			}
			continue;
		}
		if ( backwinding.empty() ) {
			if ( side == 0 ) {
				AddPortalToNodes( p, f, other_node );
			}
			else{
				AddPortalToNodes( p, other_node, f );
			}
			continue;
		}

		// the winding is split
		p->winding.clear();
		portal_t *new_portal = new portal_t( *p ); // AllocPortal()
		new_portal->winding.swap( backwinding );
		p->winding.swap( frontwinding );

		if ( side == 0 ) {
			AddPortalToNodes( p, f, other_node );
			AddPortalToNodes( new_portal, b, other_node );
		}
		else
		{
			AddPortalToNodes( p, other_node, f );
			AddPortalToNodes( new_portal, other_node, b );
		}
	}

	node->portals = NULL;
}


/*
   ================
   CalcNodeBounds
   ================
 */
static void CalcNodeBounds( node_t *node ){
	portal_t    *p;
	int s;

	// calc mins/maxs for both leafs and nodes
	node->minmax.clear();
	for ( p = node->portals; p; p = p->next[s] )
	{
		s = ( p->nodes[1] == node );
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

static void FloodPortals( node_t *node, bool skybox ){
	int dist = 1;
	std::vector<node_t*> nodes{ node };
	while( !nodes.empty() ){
		std::vector<node_t*> nodes2;
		for( node_t *n : nodes ){
			if ( skybox ) {
				n->skybox = skybox;
			}

			if ( n->opaque || ( n->occupied && n->occupied <= dist ) ) { // also reprocess occupied nodes for shorter leak line
				continue;
			}

			if( !n->occupied ){
				++c_floodedleafs;
			}

			n->occupied = dist;

			int s;
			for ( portal_t *p = n->portals; p; p = p->next[ s ] )
			{
				s = ( p->nodes[ 1 ] == n );
				nodes2.push_back( p->nodes[ !s ] );
			}
		}
		nodes.swap( nodes2 );
		++dist;
	}
}



/*
   =============
   PlaceOccupant
   =============
 */

static bool PlaceOccupant( node_t *headnode, const Vector3& origin, const entity_t *occupant, bool skybox ){
	node_t  *node;

	// find the leaf to start in
	node = headnode;
	while ( node->planenum != PLANENUM_LEAF )
	{
		if ( plane3_distance_to_point( mapplanes[ node->planenum ].plane, origin ) >= 0 ) {
			node = node->children[ 0 ];
		}
		else{
			node = node->children[ 1 ];
		}
	}

	if ( node->opaque ) {
		return false;
	}
	node->occupant = occupant;
	node->skybox = skybox;

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
	bool r, inside, skybox;


	Sys_FPrintf( SYS_VRB, "--- FloodEntities ---\n" );
	inside = false;
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
		if ( !e.brushes.empty() || e.patches != NULL || e.classname_is( "_decal" ) ) { //_decal primitive is freed at this point
			continue;
		}

		/* handle skybox entities */
		if ( e.classname_is( "_skybox" ) ) {
			skybox = true;

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
		else{
			skybox = false;
		}

		/* nudge off floor */
		origin[ 2 ] += 1;

		/* debugging code */
		//%	if( i == 1 )
		//%		origin[ 2 ] += 4096;

		/* find leaf */
		r = PlaceOccupant( tree.headnode, origin, &e, skybox );
		if ( r ) {
			inside = true;
		}
		else {
			Sys_FPrintf( SYS_WRN, "Entity %i (%s): Entity in solid\n", e.mapEntityNum, e.classname() );
		}
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

static void FloodAreas_r( node_t *node ){
	int s;
	portal_t    *p;


	if ( node->areaportal ) {
		if ( node->area == -1 ) {
			node->area = c_areas;
		}

		/* this node is part of an area portal brush */
		brush_t *b = node->brushlist.front().original;

		/* if the current area has already touched this portal, we are done */
		if ( b->portalareas[ 0 ] == c_areas || b->portalareas[ 1 ] == c_areas ) {
			return;
		}

		// note the current area as bounding the portal
		if ( b->portalareas[ 1 ] != -1 ) {
			Sys_Warning( "areaportal brush %i touches > 2 areas\n", b->brushNum );
			return;
		}
		if ( b->portalareas[ 0 ] != -1 ) {
			b->portalareas[ 1 ] = c_areas;
		}
		else{
			b->portalareas[ 0 ] = c_areas;
		}

		return;
	}

	if ( node->area != -1 ) {
		return;
	}
	if ( node->cluster == -1 ) {
		return;
	}

	node->area = c_areas;

	/* ydnar: skybox nodes set the skybox area */
	if ( node->skybox ) {
		skyboxArea = c_areas;
	}

	for ( p = node->portals; p; p = p->next[ s ] )
	{
		s = ( p->nodes[1] == node );

		/* ydnar: allow areaportal portals to block area flow */
		if ( p->compileFlags & C_AREAPORTAL ) {
			continue;
		}

		if ( !PortalPassable( p ) ) {
			continue;
		}

		FloodAreas_r( p->nodes[ !s ] );
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

	if ( node->opaque || node->areaportal || node->area != -1 ) {
		return;
	}

	FloodAreas_r( node );
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

	if ( node->cluster != -1 ) {
		if ( node->area == -1 ) {
			Sys_Warning( "cluster %d has area set to -1\n", node->cluster );
		}
	}
	if ( node->areaportal ) {
		const brush_t *b = node->brushlist.front().original;

		// check if the areaportal touches two areas
		if ( b->portalareas[0] == -1 || b->portalareas[1] == -1 ) {
			Sys_Warning( "areaportal brush %i doesn't touch two areas\n", b->brushNum );
		}
	}
}



/*
   FloodSkyboxArea_r() - ydnar
   sets all nodes with the skybox area to skybox
 */

static void FloodSkyboxArea_r( node_t *node ){
	if ( skyboxArea < 0 ) {
		return;
	}

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

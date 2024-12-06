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
#include <libxml/tree.h>



/*
   ==============================================================================

   LEAK FILE GENERATION

   Save out name.line for qe3 to read
   ==============================================================================
 */


/*
   =============
   LeakFile

   Finds the shortest possible chain of portals
   that leads from the outside leaf to a specifically
   occupied leaf

   TTimo: builds a polyline xml node
   =============
 */
static xmlNodePtr LeakFile( const tree_t& tree ){
	Vector3 mid;
	FILE    *linefile;
	const node_t  *node;
	int count;
	xmlNodePtr xml_node, point;

	if ( !tree.outside_node.occupied ) {
		return NULL;
	}

	Sys_FPrintf( SYS_VRB, "--- LeakFile ---\n" );

	//
	// write the points to the file
	//
	const auto filename = StringStream( source, ".lin" );
	linefile = SafeOpenWrite( filename, "wt" );

	xml_node = xmlNewNode( NULL, (const xmlChar*)"polyline" );

	count = 0;
	node = &tree.outside_node;
	while ( node->occupied > 1 )
	{
		int next;
		const portal_t    *p, *nextportal = NULL;
		const node_t      *nextnode = NULL;
		int s;

		// find the best portal exit
		next = node->occupied;
		for ( p = node->portals; p; p = p->next[!s] )
		{
			s = ( p->nodes[0] == node );
			if ( p->nodes[s]->occupied
			     && p->nodes[s]->occupied < next ) {
				nextportal = p;
				nextnode = p->nodes[s];
				next = nextnode->occupied;
			}
		}
		node = nextnode;
		mid = WindingCenter( nextportal->winding );
		fprintf( linefile, "%f %f %f\n", mid[0], mid[1], mid[2] );
		point = xml_NodeForVec( mid );
		xmlAddChild( xml_node, point );
		count++;
	}
	// add the occupant center
	mid = node->occupant->vectorForKey( "origin" );

	fprintf( linefile, "%f %f %f\n", mid[0], mid[1], mid[2] );
	point = xml_NodeForVec( mid );
	xmlAddChild( xml_node, point );
	Sys_FPrintf( SYS_VRB, "%9d point linefile\n", count + 1 );

	fclose( linefile );

	xml_Select( "Entity leaked", node->occupant->mapEntityNum, 0, false );

	return xml_node;
}

void Leak_feedback( const tree_t& tree ){
	Sys_FPrintf( SYS_NOXMLflag | SYS_ERR, "**********************\n" );
	Sys_FPrintf( SYS_NOXMLflag | SYS_ERR, "******* leaked *******\n" );
	Sys_FPrintf( SYS_NOXMLflag | SYS_ERR, "**********************\n" );
	xmlNodePtr polyline = LeakFile( tree );
	xmlNodePtr leaknode = xmlNewNode( NULL, (const xmlChar*)"message" );
	xmlNodeAddContent( leaknode, (const xmlChar*)"MAP LEAKED\n" );
	xmlAddChild( leaknode, polyline );
	char level[ 2 ];
	level[0] = (int) '0' + SYS_ERR;
	level[1] = 0;
	xmlSetProp( leaknode, (const xmlChar*)"level", (const xmlChar*)level );
	xml_SendNode( leaknode );
}

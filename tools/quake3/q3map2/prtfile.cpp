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
   ==============================================================================

   PORTAL FILE GENERATION

   Save out name.prt for qvis to read
   ==============================================================================
 */

namespace
{
FILE    *pf;
int num_visclusters;                    // clusters the player can be in
int num_visportals;
int num_solidfaces;
}

inline void WriteFloat( FILE *f, float v ){
	if ( fabs( v - std::rint( v ) ) < 0.001 ) {
		fprintf( f, "%li ", std::lrint( v ) );
	}
	else{
		fprintf( f, "%f ", v );
	}
}

static void CountVisportals_r( const node_t *node ){
	int s;

	// decision node
	if ( node->planenum != PLANENUM_LEAF ) {
		CountVisportals_r( node->children[0] );
		CountVisportals_r( node->children[1] );
		return;
	}

	if ( node->opaque ) {
		return;
	}

	for ( const portal_t *p = node->portals; p; p = p->next[s] )
	{
		s = ( p->nodes[1] == node );
		if ( !p->winding.empty() && p->nodes[0] == node ) {
			if ( !PortalPassable( p ) ) {
				continue;
			}
			if ( p->nodes[0]->cluster == p->nodes[1]->cluster ) {
				continue;
			}
			++num_visportals;
		}
	}
}

/*
   =================
   WritePortalFile_r
   =================
 */
static void WritePortalFile_r( const node_t *node ){
	int s, flags;

	// decision node
	if ( node->planenum != PLANENUM_LEAF ) {
		WritePortalFile_r( node->children[0] );
		WritePortalFile_r( node->children[1] );
		return;
	}

	if ( node->opaque ) {
		return;
	}

	for ( const portal_t *p = node->portals; p; p = p->next[s] )
	{
		const winding_t& w = p->winding;
		s = ( p->nodes[1] == node );
		if ( !w.empty() && p->nodes[0] == node ) {
			if ( !PortalPassable( p ) ) {
				continue;
			}
			if ( p->nodes[0]->cluster == p->nodes[1]->cluster ) {
				continue;
			}
			--num_visportals;
			// write out to the file

			// sometimes planes get turned around when they are very near
			// the changeover point between different axis.  interpret the
			// plane the same way vis will, and flip the side orders if needed
			// FIXME: is this still relevant?
			if ( vector3_dot( p->plane.normal(), WindingPlane( w ).normal() ) < 0.99 ) { // backwards...
				fprintf( pf, "%zu %i %i ", w.size(), p->nodes[1]->cluster, p->nodes[0]->cluster );
			}
			else{
				fprintf( pf, "%zu %i %i ", w.size(), p->nodes[0]->cluster, p->nodes[1]->cluster );
			}

			flags = 0;

			/* ydnar: added this change to make antiportals work */
			if( p->compileFlags & C_HINT ) {
				flags |= 1;
			}

			/* divVerent: I want farplanedist to not kill skybox. So... */
			if( p->compileFlags & C_SKY ) {
				flags |= 2;
			}

			fprintf( pf, "%d ", flags );

			/* write the winding */
			for ( const Vector3 point : w )
			{
				fprintf( pf, "(" );
				WriteFloat( pf, point.x() );
				WriteFloat( pf, point.y() );
				WriteFloat( pf, point.z() );
				fprintf( pf, ") " );
			}
			fprintf( pf, "\n" );
		}
	}

}

static void CountSolidFaces_r( const node_t *node ){
	int s;

	// decision node
	if ( node->planenum != PLANENUM_LEAF ) {
		CountSolidFaces_r( node->children[0] );
		CountSolidFaces_r( node->children[1] );
		return;
	}

	if ( node->opaque ) {
		return;
	}

	for ( const portal_t *p = node->portals; p; p = p->next[s] )
	{
		s = ( p->nodes[1] == node );
		if ( !p->winding.empty() ) {
			if ( PortalPassable( p ) ) {
				continue;
			}
			if ( p->nodes[0]->cluster == p->nodes[1]->cluster ) {
				continue;
			}
			// write out to the file

			++num_solidfaces;
		}
	}
}

/*
   =================
   WriteFaceFile_r
   =================
 */
static void WriteFaceFile_r( const node_t *node ){
	int s;

	// decision node
	if ( node->planenum != PLANENUM_LEAF ) {
		WriteFaceFile_r( node->children[0] );
		WriteFaceFile_r( node->children[1] );
		return;
	}

	if ( node->opaque ) {
		return;
	}

	for ( const portal_t *p = node->portals; p; p = p->next[s] )
	{
		const winding_t& w = p->winding;
		s = ( p->nodes[1] == node );
		if ( !w.empty() ) {
			if ( PortalPassable( p ) ) {
				continue;
			}
			if ( p->nodes[0]->cluster == p->nodes[1]->cluster ) {
				continue;
			}
			// write out to the file

			if ( p->nodes[0] == node ) {
				fprintf( pf, "%zu %i ", w.size(), p->nodes[0]->cluster );
				for ( const Vector3& point : w )
				{
					fprintf( pf, "(" );
					WriteFloat( pf, point.x() );
					WriteFloat( pf, point.y() );
					WriteFloat( pf, point.z() );
					fprintf( pf, ") " );
				}
				fprintf( pf, "\n" );
			}
			else
			{
				fprintf( pf, "%zu %i ", w.size(), p->nodes[1]->cluster );
				for ( winding_t::const_reverse_iterator point = w.crbegin(); point != w.crend(); ++point )
				{
					fprintf( pf, "(" );
					WriteFloat( pf, point->x() );
					WriteFloat( pf, point->y() );
					WriteFloat( pf, point->z() );
					fprintf( pf, ") " );
				}
				fprintf( pf, "\n" );
			}
		}
	}
}

/*
   ================
   NumberLeafs_r
   ================
 */
static void NumberLeafs_r( node_t *node, int c ){
#if 0
	portal_t    *p;
#endif
	if ( node->planenum != PLANENUM_LEAF ) {
		// decision node
		node->cluster = -99;

		if ( node->has_structural_children ) {
#if 0
			if ( c >= 0 ) {
				Sys_FPrintf( SYS_ERR, "THIS CANNOT HAPPEN\n" );
			}
#endif
			NumberLeafs_r( node->children[0], c );
			NumberLeafs_r( node->children[1], c );
		}
		else
		{
			if ( c < 0 ) {
				c = num_visclusters++;
			}
			NumberLeafs_r( node->children[0], c );
			NumberLeafs_r( node->children[1], c );
		}
		return;
	}

	node->area = -1;

	if ( node->opaque ) {
		// solid block, viewpoint never inside
		node->cluster = -1;
		return;
	}

	if ( c < 0 ) {
		c = num_visclusters++;
	}

	node->cluster = c;

#if 0
	// count the portals
	for ( p = node->portals; p; )
	{
		if ( p->nodes[0] == node ) {      // only write out from first leaf
			if ( PortalPassable( p ) ) {
				num_visportals++;
			}
			else{
				num_solidfaces++;
			}
			p = p->next[0];
		}
		else
		{
			if ( !PortalPassable( p ) ) {
				num_solidfaces++;
			}
			p = p->next[1];
		}
	}
#endif
}


/*
   ================
   NumberClusters
   ================
 */
void NumberClusters( tree_t& tree ) {
	num_visclusters = 0;
	num_visportals = 0;
	num_solidfaces = 0;

	Sys_FPrintf( SYS_VRB, "--- NumberClusters ---\n" );

	// set the cluster field in every leaf and count the total number of portals
	NumberLeafs_r( tree.headnode, -1 );
	CountVisportals_r( tree.headnode );
	CountSolidFaces_r( tree.headnode );

	Sys_FPrintf( SYS_VRB, "%9d visclusters\n", num_visclusters );
	Sys_FPrintf( SYS_VRB, "%9d visportals\n", num_visportals );
	Sys_FPrintf( SYS_VRB, "%9d solidfaces\n", num_solidfaces );
}

/*
   ================
   WritePortalFile
   ================
 */
void WritePortalFile( const tree_t& tree ){
	Sys_FPrintf( SYS_VRB, "--- WritePortalFile ---\n" );

	// write the file
	const auto filename = StringStream( source, ".prt" );
	Sys_Printf( "writing %s\n", filename.c_str() );
	pf = SafeOpenWrite( filename, "wt" );

	fprintf( pf, "%s\n", PORTALFILE );
	fprintf( pf, "%i\n", num_visclusters );
	fprintf( pf, "%i\n", num_visportals );
	fprintf( pf, "%i\n", num_solidfaces );

	WritePortalFile_r( tree.headnode );
	WriteFaceFile_r( tree.headnode );

	fclose( pf );
}

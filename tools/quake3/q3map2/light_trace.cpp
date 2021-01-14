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



/* marker */
#define LIGHT_TRACE_C



/* dependencies */
#include "q3map2.h"


/* dependencies */
#include "q3map2.h"



#define Vector2Copy( a, b )     ( ( b )[ 0 ] = ( a )[ 0 ], ( b )[ 1 ] = ( a )[ 1 ] )
#define Vector4Copy( a, b )     ( ( b )[ 0 ] = ( a )[ 0 ], ( b )[ 1 ] = ( a )[ 1 ], ( b )[ 2 ] = ( a )[ 2 ], ( b )[ 3 ] = ( a )[ 3 ] )

#define MAX_NODE_ITEMS          5
#define MAX_NODE_TRIANGLES      5
#define MAX_TRACE_DEPTH         32
#define MIN_NODE_SIZE           32.0f

#define GROW_TRACE_INFOS        32768       //%	4096
#define GROW_TRACE_WINDINGS     65536       //%	32768
#define GROW_TRACE_TRIANGLES    131072      //%	32768
#define GROW_TRACE_NODES        16384       //%	16384
#define GROW_NODE_ITEMS         16          //%	256

#define MAX_TW_VERTS            24 // vortex: increased from 12 to 24 for ability co compile some insane maps with large curve count

#define TRACE_ON_EPSILON        0.1f

#define TRACE_LEAF              -1
#define TRACE_LEAF_SOLID        -2

typedef struct traceVert_s
{
	vec3_t xyz;
	float st[ 2 ];
}
traceVert_t;

typedef struct traceInfo_s
{
	shaderInfo_t                *si;
	int surfaceNum, castShadows;
	bool skipGrid;
}
traceInfo_t;

typedef struct traceWinding_s
{
	vec4_t plane;
	int infoNum, numVerts;
	traceVert_t v[ MAX_TW_VERTS ];
}
traceWinding_t;

typedef struct traceTriangle_s
{
	vec3_t edge1, edge2;
	int infoNum;
	traceVert_t v[ 3 ];
}
traceTriangle_t;

typedef struct traceNode_s
{
	int type;
	vec4_t plane;
	vec3_t mins, maxs;
	int children[ 2 ];
	int numItems, maxItems;
	int                         *items;
}
traceNode_t;


int noDrawContentFlags, noDrawSurfaceFlags, noDrawCompileFlags;

int numTraceInfos = 0, maxTraceInfos = 0, firstTraceInfo = 0;
traceInfo_t                     *traceInfos = NULL;

int numTraceWindings = 0, maxTraceWindings = 0, deadWinding = -1;
traceWinding_t                  *traceWindings = NULL;

int numTraceTriangles = 0, maxTraceTriangles = 0, deadTriangle = -1;
traceTriangle_t                 *traceTriangles = NULL;

int headNodeNum = 0, skyboxNodeNum = 0, maxTraceDepth = 0, numTraceLeafNodes = 0;
int numTraceNodes = 0, maxTraceNodes = 0;
traceNode_t                     *traceNodes = NULL;



/* -------------------------------------------------------------------------------

   allocation and list management

   ------------------------------------------------------------------------------- */

/*
   AddTraceInfo() - ydnar
   adds a trace info structure to the pool
 */

static int AddTraceInfo( traceInfo_t *ti ){
	int num;

	/* find an existing info */
	for ( num = firstTraceInfo; num < numTraceInfos; num++ )
	{
		if ( traceInfos[ num ].si == ti->si &&
			 traceInfos[ num ].surfaceNum == ti->surfaceNum &&
			 traceInfos[ num ].castShadows == ti->castShadows &&
			 traceInfos[ num ].skipGrid == ti->skipGrid ) {
			return num;
		}
	}

	/* enough space? */
	AUTOEXPAND_BY_REALLOC_ADD( traceInfos, numTraceInfos, maxTraceInfos, GROW_TRACE_INFOS );

	/* add the info */
	memcpy( &traceInfos[ num ], ti, sizeof( *traceInfos ) );
	if ( num == numTraceInfos ) {
		numTraceInfos++;
	}

	/* return the ti number */
	return num;
}



/*
   AllocTraceNode() - ydnar
   allocates a new trace node
 */

static int AllocTraceNode( void ){
	/* enough space? */
	AUTOEXPAND_BY_REALLOC_ADD( traceNodes, numTraceNodes, maxTraceNodes, GROW_TRACE_NODES );

	/* add the node */
	memset( &traceNodes[ numTraceNodes ], 0, sizeof( traceNode_t ) );
	traceNodes[ numTraceNodes ].type = TRACE_LEAF;
	ClearBounds( traceNodes[ numTraceNodes ].mins, traceNodes[ numTraceNodes ].maxs );

	/* Sys_Printf("alloc node %d\n", numTraceNodes); */

	numTraceNodes++;

	/* return the count */
	return ( numTraceNodes - 1 );
}



/*
   AddTraceWinding() - ydnar
   adds a winding to the raytracing pool
 */

static int AddTraceWinding( traceWinding_t *tw ){
	int num;

	/* check for a dead winding */
	if ( deadWinding >= 0 && deadWinding < numTraceWindings ) {
		num = deadWinding;
	}
	else
	{
		/* put winding at the end of the list */
		num = numTraceWindings;

		/* enough space? */
		AUTOEXPAND_BY_REALLOC_ADD( traceWindings, numTraceWindings, maxTraceWindings, GROW_TRACE_WINDINGS );
	}

	/* add the winding */
	memcpy( &traceWindings[ num ], tw, sizeof( *traceWindings ) );
	if ( num == numTraceWindings ) {
		numTraceWindings++;
	}
	deadWinding = -1;

	/* return the winding number */
	return num;
}



/*
   AddTraceTriangle() - ydnar
   adds a triangle to the raytracing pool
 */

static int AddTraceTriangle( traceTriangle_t *tt ){
	int num;

	/* check for a dead triangle */
	if ( deadTriangle >= 0 && deadTriangle < numTraceTriangles ) {
		num = deadTriangle;
	}
	else
	{
		/* put triangle at the end of the list */
		num = numTraceTriangles;

		/* enough space? */
		AUTOEXPAND_BY_REALLOC_ADD( traceTriangles, numTraceTriangles, maxTraceTriangles, GROW_TRACE_TRIANGLES );
	}

	/* find vectors for two edges sharing the first vert */
	VectorSubtract( tt->v[ 1 ].xyz, tt->v[ 0 ].xyz, tt->edge1 );
	VectorSubtract( tt->v[ 2 ].xyz, tt->v[ 0 ].xyz, tt->edge2 );

	/* add the triangle */
	memcpy( &traceTriangles[ num ], tt, sizeof( *traceTriangles ) );
	if ( num == numTraceTriangles ) {
		numTraceTriangles++;
	}
	deadTriangle = -1;

	/* return the triangle number */
	return num;
}



/*
   AddItemToTraceNode() - ydnar
   adds an item reference (winding or triangle) to a trace node
 */

static int AddItemToTraceNode( traceNode_t *node, int num ){
	/* dummy check */
	if ( num < 0 ) {
		return -1;
	}

	/* enough space? */
	if ( node->numItems >= node->maxItems ) {
		/* allocate more room */
		if ( node == traceNodes ) {
			node->maxItems *= 2;
		}
		else{
			node->maxItems += GROW_NODE_ITEMS;
		}
		if ( node->maxItems <= 0 ) {
			node->maxItems = GROW_NODE_ITEMS;
		}
		node->items = realloc( node->items, node->maxItems * sizeof( *node->items ) );
		if ( !node->items ) {
			Error( "node->items out of memory" );
		}
	}

	/* add the poly */
	node->items[ node->numItems ] = num;
	node->numItems++;

	/* return the count */
	return ( node->numItems - 1 );
}




/* -------------------------------------------------------------------------------

   trace node setup

   ------------------------------------------------------------------------------- */

/*
   SetupTraceNodes_r() - ydnar
   recursively create the initial trace node structure from the bsp tree
 */

static int SetupTraceNodes_r( int bspNodeNum ){
	int i, nodeNum, bspLeafNum, newNode;
	bspPlane_t      *plane;
	bspNode_t       *bspNode;


	/* get bsp node and plane */
	bspNode = &bspNodes[ bspNodeNum ];
	plane = &bspPlanes[ bspNode->planeNum ];

	/* allocate a new trace node */
	nodeNum = AllocTraceNode();

	/* setup trace node */
	traceNodes[ nodeNum ].type = PlaneTypeForNormal( plane->normal );
	VectorCopy( plane->normal, traceNodes[ nodeNum ].plane );
	traceNodes[ nodeNum ].plane[ 3 ] = plane->dist;

	/* setup children */
	for ( i = 0; i < 2; i++ )
	{
		/* leafnode */
		if ( bspNode->children[ i ] < 0 ) {
			bspLeafNum = -bspNode->children[ i ] - 1;

			/* new code */
			newNode = AllocTraceNode();
			traceNodes[ nodeNum ].children[ i ] = newNode;
			/* have to do this separately, as gcc first executes LHS, then RHS, and if a realloc took place, this fails */

			if ( bspLeafs[ bspLeafNum ].cluster == -1 ) {
				traceNodes[ traceNodes[ nodeNum ].children[ i ] ].type = TRACE_LEAF_SOLID;
			}
		}

		/* normal node */
		else
		{
			newNode = SetupTraceNodes_r( bspNode->children[ i ] );
			traceNodes[ nodeNum ].children[ i ] = newNode;
		}

		if ( traceNodes[ nodeNum ].children[ i ] == 0 ) {
			Error( "Invalid tracenode allocated" );
		}
	}

	/* Sys_Printf("node %d children: %d %d\n", nodeNum, traceNodes[ nodeNum ].children[0], traceNodes[ nodeNum ].children[1]); */

	/* return node number */
	return nodeNum;
}



/*
   ClipTraceWinding() - ydnar
   clips a trace winding against a plane into one or two parts
 */

#define TW_ON_EPSILON   0.25f

void ClipTraceWinding( traceWinding_t *tw, vec4_t plane, traceWinding_t *front, traceWinding_t *back ){
	int i, j, k;
	int sides[ MAX_TW_VERTS ], counts[ 3 ] = { 0, 0, 0 };
	float dists[ MAX_TW_VERTS ];
	float frac;
	traceVert_t     *a, *b, mid;


	/* clear front and back */
	front->numVerts = 0;
	back->numVerts = 0;

	/* classify points */
	for ( i = 0; i < tw->numVerts; i++ )
	{
		dists[ i ] = DotProduct( tw->v[ i ].xyz, plane ) - plane[ 3 ];
		if ( dists[ i ] < -TW_ON_EPSILON ) {
			sides[ i ] = SIDE_BACK;
		}
		else if ( dists[ i ] > TW_ON_EPSILON ) {
			sides[ i ] = SIDE_FRONT;
		}
		else{
			sides[ i ] = SIDE_ON;
		}
		counts[ sides[ i ] ]++;
	}

	/* entirely on front? */
	if ( counts[ SIDE_BACK ] == 0 ) {
		memcpy( front, tw, sizeof( *front ) );
	}

	/* entirely on back? */
	else if ( counts[ SIDE_FRONT ] == 0 ) {
		memcpy( back, tw, sizeof( *back ) );
	}

	/* straddles the plane */
	else
	{
		/* setup front and back */
		memcpy( front, tw, sizeof( *front ) );
		front->numVerts = 0;
		memcpy( back, tw, sizeof( *back ) );
		back->numVerts = 0;

		/* split the winding */
		for ( i = 0; i < tw->numVerts; i++ )
		{
			/* radix */
			j = ( i + 1 ) % tw->numVerts;

			/* get verts */
			a = &tw->v[ i ];
			b = &tw->v[ j ];

			/* handle points on the splitting plane */
			switch ( sides[ i ] )
			{
			case SIDE_FRONT:
				if ( front->numVerts >= MAX_TW_VERTS ) {
					Error( "MAX_TW_VERTS (%d) exceeded", MAX_TW_VERTS );
				}
				front->v[ front->numVerts++ ] = *a;
				break;

			case SIDE_BACK:
				if ( back->numVerts >= MAX_TW_VERTS ) {
					Error( "MAX_TW_VERTS (%d) exceeded", MAX_TW_VERTS );
				}
				back->v[ back->numVerts++ ] = *a;
				break;

			case SIDE_ON:
				if ( front->numVerts >= MAX_TW_VERTS || back->numVerts >= MAX_TW_VERTS ) {
					Error( "MAX_TW_VERTS (%d) exceeded", MAX_TW_VERTS );
				}
				front->v[ front->numVerts++ ] = *a;
				back->v[ back->numVerts++ ] = *a;
				continue;
			}

			/* check next point to see if we need to split the edge */
			if ( sides[ j ] == SIDE_ON || sides[ j ] == sides[ i ] ) {
				continue;
			}

			/* check limit */
			if ( front->numVerts >= MAX_TW_VERTS || back->numVerts >= MAX_TW_VERTS ) {
				Error( "MAX_TW_VERTS (%d) exceeded", MAX_TW_VERTS );
			}

			/* generate a split point */
			frac = dists[ i ] / ( dists[ i ] - dists[ j ] );
			for ( k = 0; k < 3; k++ )
			{
				/* minimize fp precision errors */
				if ( plane[ k ] == 1.0f ) {
					mid.xyz[ k ] = plane[ 3 ];
				}
				else if ( plane[ k ] == -1.0f ) {
					mid.xyz[ k ] = -plane[ 3 ];
				}
				else{
					mid.xyz[ k ] = a->xyz[ k ] + frac * ( b->xyz[ k ] - a->xyz[ k ] );
				}
			}
			/* set texture coordinates */
			mid.st[ 0 ] = a->st[ 0 ] + frac * ( b->st[ 0 ] - a->st[ 0 ] );
			mid.st[ 1 ] = a->st[ 1 ] + frac * ( b->st[ 1 ] - a->st[ 1 ] );

			/* copy midpoint to front and back polygons */
			front->v[ front->numVerts++ ] = mid;
			back->v[ back->numVerts++ ] = mid;
		}
	}
}



/*
   FilterTraceWindingIntoNodes_r() - ydnar
   filters a trace winding into the raytracing tree
 */

static void FilterTraceWindingIntoNodes_r( traceWinding_t *tw, int nodeNum ){
	int num;
	vec4_t plane1, plane2, reverse;
	traceNode_t     *node;
	traceWinding_t front, back;


	/* don't filter if passed a bogus node (solid, etc) */
	if ( nodeNum < 0 || nodeNum >= numTraceNodes ) {
		return;
	}

	/* get node */
	node = &traceNodes[ nodeNum ];

	/* is this a decision node? */
	if ( node->type >= 0 ) {
		/* create winding plane if necessary, filtering out bogus windings as well */
		if ( nodeNum == headNodeNum ) {
			if ( !PlaneFromPoints( tw->plane, tw->v[ 0 ].xyz, tw->v[ 1 ].xyz, tw->v[ 2 ].xyz ) ) {
				return;
			}
		}

		/* validate the node */
		if ( node->children[ 0 ] == 0 || node->children[ 1 ] == 0 ) {
			Error( "Invalid tracenode: %d", nodeNum );
		}

		/* get node plane */
		Vector4Copy( node->plane, plane1 );

		/* get winding plane */
		Vector4Copy( tw->plane, plane2 );

		/* invert surface plane */
		VectorSubtract( vec3_origin, plane2, reverse );
		reverse[ 3 ] = -plane2[ 3 ];

		/* front only */
		if ( DotProduct( plane1, plane2 ) > 0.999f && fabs( plane1[ 3 ] - plane2[ 3 ] ) < 0.001f ) {
			FilterTraceWindingIntoNodes_r( tw, node->children[ 0 ] );
			return;
		}

		/* back only */
		if ( DotProduct( plane1, reverse ) > 0.999f && fabs( plane1[ 3 ] - reverse[ 3 ] ) < 0.001f ) {
			FilterTraceWindingIntoNodes_r( tw, node->children[ 1 ] );
			return;
		}

		/* clip the winding by node plane */
		ClipTraceWinding( tw, plane1, &front, &back );

		/* filter by node plane */
		if ( front.numVerts >= 3 ) {
			FilterTraceWindingIntoNodes_r( &front, node->children[ 0 ] );
		}
		if ( back.numVerts >= 3 ) {
			FilterTraceWindingIntoNodes_r( &back, node->children[ 1 ] );
		}

		/* return to caller */
		return;
	}

	/* add winding to leaf node */
	num = AddTraceWinding( tw );
	AddItemToTraceNode( node, num );
}



/*
   SubdivideTraceNode_r() - ydnar
   recursively subdivides a tracing node until it meets certain size and complexity criteria
 */

static void SubdivideTraceNode_r( int nodeNum, int depth ){
	int i, j, count, num, frontNum, backNum, type;
	vec3_t size;
	float dist;
	double average[ 3 ];
	traceNode_t     *node, *frontNode, *backNode;
	traceWinding_t  *tw, front, back;


	/* dummy check */
	if ( nodeNum < 0 || nodeNum >= numTraceNodes ) {
		return;
	}

	/* get node */
	node = &traceNodes[ nodeNum ];

	/* runaway recursion check */
	if ( depth >= MAX_TRACE_DEPTH ) {
		//%	Sys_Printf( "Depth: (%d items)\n", node->numItems );
		numTraceLeafNodes++;
		return;
	}
	depth++;

	/* is this a decision node? */
	if ( node->type >= 0 ) {
		/* subdivide children */
		frontNum = node->children[ 0 ];
		backNum = node->children[ 1 ];
		SubdivideTraceNode_r( frontNum, depth );
		SubdivideTraceNode_r( backNum, depth );
		return;
	}

	/* bound the node */
	ClearBounds( node->mins, node->maxs );
	VectorClear( average );
	count = 0;
	for ( i = 0; i < node->numItems; i++ )
	{
		/* get winding */
		tw = &traceWindings[ node->items[ i ] ];

		/* walk its verts */
		for ( j = 0; j < tw->numVerts; j++ )
		{
			AddPointToBounds( tw->v[ j ].xyz, node->mins, node->maxs );
			average[ 0 ] += tw->v[ j ].xyz[ 0 ];
			average[ 1 ] += tw->v[ j ].xyz[ 1 ];
			average[ 2 ] += tw->v[ j ].xyz[ 2 ];
			count++;
		}
	}

	/* check triangle limit */
	//%	if( node->numItems <= MAX_NODE_ITEMS )
	if ( ( count - ( node->numItems * 2 ) ) < MAX_NODE_TRIANGLES ) {
		//%	Sys_Printf( "Limit: (%d triangles)\n", (count - (node->numItems * 2)) );
		numTraceLeafNodes++;
		return;
	}

	/* the largest dimension of the bounding box will be the split axis */
	VectorSubtract( node->maxs, node->mins, size );
	if ( size[ 0 ] >= size[ 1 ] && size[ 0 ] >= size[ 2 ] ) {
		type = PLANE_X;
	}
	else if ( size[ 1 ] >= size[ 0 ] && size[ 1 ] >= size[ 2 ] ) {
		type = PLANE_Y;
	}
	else{
		type = PLANE_Z;
	}

	/* don't split small nodes */
	if ( size[ type ] <= MIN_NODE_SIZE ) {
		//%	Sys_Printf( "Limit: %f %f %f (%d items)\n", size[ 0 ], size[ 1 ], size[ 2 ], node->numItems );
		numTraceLeafNodes++;
		return;
	}

	/* set max trace depth */
	if ( depth > maxTraceDepth ) {
		maxTraceDepth = depth;
	}

	/* snap the average */
	dist = floor( average[ type ] / count );

	/* dummy check it */
	if ( dist <= node->mins[ type ] || dist >= node->maxs[ type ] ) {
		dist = floor( 0.5f * ( node->mins[ type ] + node->maxs[ type ] ) );
	}

	/* allocate child nodes */
	frontNum = AllocTraceNode();
	backNum = AllocTraceNode();

	/* reset pointers */
	node = &traceNodes[ nodeNum ];
	frontNode = &traceNodes[ frontNum ];
	backNode = &traceNodes[ backNum ];

	/* attach children */
	node->type = type;
	node->plane[ type ] = 1.0f;
	node->plane[ 3 ] = dist;
	node->children[ 0 ] = frontNum;
	node->children[ 1 ] = backNum;

	/* setup front node */
	frontNode->maxItems = ( node->maxItems >> 1 );
	frontNode->items = safe_malloc( frontNode->maxItems * sizeof( *frontNode->items ) );

	/* setup back node */
	backNode->maxItems = ( node->maxItems >> 1 );
	backNode->items = safe_malloc( backNode->maxItems * sizeof( *backNode->items ) );

	/* filter windings into child nodes */
	for ( i = 0; i < node->numItems; i++ )
	{
		/* get winding */
		tw = &traceWindings[ node->items[ i ] ];

		/* clip the winding by the new split plane */
		ClipTraceWinding( tw, node->plane, &front, &back );

		/* kill the existing winding */
		if ( front.numVerts >= 3 || back.numVerts >= 3 ) {
			deadWinding = node->items[ i ];
		}

		/* add front winding */
		if ( front.numVerts >= 3 ) {
			num = AddTraceWinding( &front );
			AddItemToTraceNode( frontNode, num );
		}

		/* add back winding */
		if ( back.numVerts >= 3 ) {
			num = AddTraceWinding( &back );
			AddItemToTraceNode( backNode, num );
		}
	}

	/* free original node winding list */
	node->numItems = 0;
	node->maxItems = 0;
	free( node->items );
	node->items = NULL;

	/* check children */
	if ( frontNode->numItems <= 0 ) {
		frontNode->maxItems = 0;
		free( frontNode->items );
		frontNode->items = NULL;
	}

	if ( backNode->numItems <= 0 ) {
		backNode->maxItems = 0;
		free( backNode->items );
		backNode->items = NULL;
	}

	/* subdivide children */
	SubdivideTraceNode_r( frontNum, depth );
	SubdivideTraceNode_r( backNum, depth );
}



/*
   TriangulateTraceNode_r()
   optimizes the tracing data by changing trace windings into triangles
 */

static int TriangulateTraceNode_r( int nodeNum ){
	int i, j, num, frontNum, backNum, numWindings, *windings;
	traceNode_t     *node;
	traceWinding_t  *tw;
	traceTriangle_t tt;


	/* dummy check */
	if ( nodeNum < 0 || nodeNum >= numTraceNodes ) {
		return 0;
	}

	/* get node */
	node = &traceNodes[ nodeNum ];

	/* is this a decision node? */
	if ( node->type >= 0 ) {
		/* triangulate children */
		frontNum = node->children[ 0 ];
		backNum = node->children[ 1 ];
		node->numItems = TriangulateTraceNode_r( frontNum );
		node->numItems += TriangulateTraceNode_r( backNum );
		return node->numItems;
	}

	/* empty node? */
	if ( node->numItems == 0 ) {
		node->maxItems = 0;
		free( node->items );
		return node->numItems;
	}

	/* store off winding data */
	numWindings = node->numItems;
	windings = node->items;

	/* clear it */
	node->numItems = 0;
	node->maxItems = numWindings * 2;
	node->items = safe_malloc( node->maxItems * sizeof( tt ) );

	/* walk winding list */
	for ( i = 0; i < numWindings; i++ )
	{
		/* get winding */
		tw = &traceWindings[ windings[ i ] ];

		/* initial setup */
		tt.infoNum = tw->infoNum;
		tt.v[ 0 ] = tw->v[ 0 ];

		/* walk vertex list */
		for ( j = 1; j + 1 < tw->numVerts; j++ )
		{
			/* set verts */
			tt.v[ 1 ] = tw->v[ j ];
			tt.v[ 2 ] = tw->v[ j + 1 ];

			/* find vectors for two edges sharing the first vert */
			VectorSubtract( tt.v[ 1 ].xyz, tt.v[ 0 ].xyz, tt.edge1 );
			VectorSubtract( tt.v[ 2 ].xyz, tt.v[ 0 ].xyz, tt.edge2 );

			/* add it to the node */
			num = AddTraceTriangle( &tt );
			AddItemToTraceNode( node, num );
		}
	}

	/* free windings */
	free( windings );

	/* return item count */
	return node->numItems;
}



/* -------------------------------------------------------------------------------

   shadow casting item setup (triangles, patches, entities)

   ------------------------------------------------------------------------------- */

/*
   PopulateWithBSPModel() - ydnar
   filters a bsp model's surfaces into the raytracing tree
 */

static void PopulateWithBSPModel( bspModel_t *model, m4x4_t transform ){
	int i, j, x, y, pw[ 5 ], r, nodeNum;
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info;
	bspDrawVert_t       *verts;
	int                 *indexes;
	mesh_t srcMesh, *mesh, *subdivided;
	traceInfo_t ti;
	traceWinding_t tw;


	/* dummy check */
	if ( model == NULL || transform == NULL ) {
		return;
	}

	/* walk the list of surfaces in this model and fill out the info structs */
	for ( i = 0; i < model->numBSPSurfaces; i++ )
	{
		/* get surface and info */
		ds = &bspDrawSurfaces[ model->firstBSPSurface + i ];
		info = &surfaceInfos[ model->firstBSPSurface + i ];
		if ( info->si == NULL ) {
			continue;
		}

		/* no shadows */
		if ( !info->castShadows ) {
			continue;
		}

		/* patchshadows? */
		if ( ds->surfaceType == MST_PATCH && !patchShadows ) {
			continue;
		}

		/* some surfaces in the bsp might have been tagged as nodraw, with a bogus shader */
		if ( ( bspShaders[ ds->shaderNum ].contentFlags & noDrawContentFlags ) ||
			 ( bspShaders[ ds->shaderNum ].surfaceFlags & noDrawSurfaceFlags ) ) {
			continue;
		}

		/* translucent surfaces that are neither alphashadow or lightfilter don't cast shadows */
		if ( ( info->si->compileFlags & C_NODRAW ) ) {
			continue;
		}
		if ( ( info->si->compileFlags & C_TRANSLUCENT ) &&
			 !( info->si->compileFlags & C_ALPHASHADOW ) &&
			 !( info->si->compileFlags & C_LIGHTFILTER ) ) {
			continue;
		}

		/* setup trace info */
		ti.si = info->si;
		ti.castShadows = info->castShadows;
		ti.surfaceNum = model->firstBSPBrush + i;
		ti.skipGrid = ( ds->surfaceType == MST_PATCH );

		/* choose which node (normal or skybox) */
		if ( info->parentSurfaceNum >= 0 ) {
			nodeNum = skyboxNodeNum;

			/* sky surfaces in portal skies are ignored */
			if ( info->si->compileFlags & C_SKY ) {
				continue;
			}
		}
		else{
			nodeNum = headNodeNum;
		}

		/* setup trace winding */
		memset( &tw, 0, sizeof( tw ) );
		tw.infoNum = AddTraceInfo( &ti );
		tw.numVerts = 3;

		/* switch on type */
		switch ( ds->surfaceType )
		{
		/* handle patches */
		case MST_PATCH:
			/* subdivide the surface */
			srcMesh.width = ds->patchWidth;
			srcMesh.height = ds->patchHeight;
			srcMesh.verts = &bspDrawVerts[ ds->firstVert ];
			//%	subdivided = SubdivideMesh( srcMesh, 8, 512 );
			subdivided = SubdivideMesh2( srcMesh, info->patchIterations );

			/* fit it to the curve and remove colinear verts on rows/columns */
			PutMeshOnCurve( *subdivided );
			mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );

			/* set verts */
			verts = mesh->verts;

			/* subdivide each quad to place the models */
			for ( y = 0; y < ( mesh->height - 1 ); y++ )
			{
				for ( x = 0; x < ( mesh->width - 1 ); x++ )
				{
					/* set indexes */
					pw[ 0 ] = x + ( y * mesh->width );
					pw[ 1 ] = x + ( ( y + 1 ) * mesh->width );
					pw[ 2 ] = x + 1 + ( ( y + 1 ) * mesh->width );
					pw[ 3 ] = x + 1 + ( y * mesh->width );
					pw[ 4 ] = x + ( y * mesh->width );      /* same as pw[ 0 ] */

					/* set radix */
					r = ( x + y ) & 1;

					/* make first triangle */
					VectorCopy( verts[ pw[ r + 0 ] ].xyz, tw.v[ 0 ].xyz );
					Vector2Copy( verts[ pw[ r + 0 ] ].st, tw.v[ 0 ].st );
					VectorCopy( verts[ pw[ r + 1 ] ].xyz, tw.v[ 1 ].xyz );
					Vector2Copy( verts[ pw[ r + 1 ] ].st, tw.v[ 1 ].st );
					VectorCopy( verts[ pw[ r + 2 ] ].xyz, tw.v[ 2 ].xyz );
					Vector2Copy( verts[ pw[ r + 2 ] ].st, tw.v[ 2 ].st );
					m4x4_transform_point( transform, tw.v[ 0 ].xyz );
					m4x4_transform_point( transform, tw.v[ 1 ].xyz );
					m4x4_transform_point( transform, tw.v[ 2 ].xyz );
					FilterTraceWindingIntoNodes_r( &tw, nodeNum );

					/* make second triangle */
					VectorCopy( verts[ pw[ r + 0 ] ].xyz, tw.v[ 0 ].xyz );
					Vector2Copy( verts[ pw[ r + 0 ] ].st, tw.v[ 0 ].st );
					VectorCopy( verts[ pw[ r + 2 ] ].xyz, tw.v[ 1 ].xyz );
					Vector2Copy( verts[ pw[ r + 2 ] ].st, tw.v[ 1 ].st );
					VectorCopy( verts[ pw[ r + 3 ] ].xyz, tw.v[ 2 ].xyz );
					Vector2Copy( verts[ pw[ r + 3 ] ].st, tw.v[ 2 ].st );
					m4x4_transform_point( transform, tw.v[ 0 ].xyz );
					m4x4_transform_point( transform, tw.v[ 1 ].xyz );
					m4x4_transform_point( transform, tw.v[ 2 ].xyz );
					FilterTraceWindingIntoNodes_r( &tw, nodeNum );
				}
			}

			/* free the subdivided mesh */
			FreeMesh( mesh );
			break;

		/* handle triangle surfaces */
		case MST_TRIANGLE_SOUP:
		case MST_PLANAR:
			/* set verts and indexes */
			verts = &bspDrawVerts[ ds->firstVert ];
			indexes = &bspDrawIndexes[ ds->firstIndex ];

			/* walk the triangle list */
			for ( j = 0; j < ds->numIndexes; j += 3 )
			{
				VectorCopy( verts[ indexes[ j ] ].xyz, tw.v[ 0 ].xyz );
				Vector2Copy( verts[ indexes[ j ] ].st, tw.v[ 0 ].st );
				VectorCopy( verts[ indexes[ j + 1 ] ].xyz, tw.v[ 1 ].xyz );
				Vector2Copy( verts[ indexes[ j + 1 ] ].st, tw.v[ 1 ].st );
				VectorCopy( verts[ indexes[ j + 2 ] ].xyz, tw.v[ 2 ].xyz );
				Vector2Copy( verts[ indexes[ j + 2 ] ].st, tw.v[ 2 ].st );
				m4x4_transform_point( transform, tw.v[ 0 ].xyz );
				m4x4_transform_point( transform, tw.v[ 1 ].xyz );
				m4x4_transform_point( transform, tw.v[ 2 ].xyz );
				FilterTraceWindingIntoNodes_r( &tw, nodeNum );
			}
			break;

		/* other surface types do not cast shadows */
		default:
			break;
		}
	}
}



/*
   PopulateWithPicoModel() - ydnar
   filters a picomodel's surfaces into the raytracing tree
 */

static void PopulateWithPicoModel( int castShadows, picoModel_t *model, m4x4_t transform ){
	int i, j, k, numSurfaces, numIndexes;
	picoSurface_t       *surface;
	picoShader_t        *shader;
	picoVec_t           *xyz, *st;
	picoIndex_t         *indexes;
	traceInfo_t ti;
	traceWinding_t tw;


	/* dummy check */
	if ( model == NULL || transform == NULL ) {
		return;
	}

	/* get info */
	numSurfaces = PicoGetModelNumSurfaces( model );

	/* walk the list of surfaces in this model and fill out the info structs */
	for ( i = 0; i < numSurfaces; i++ )
	{
		/* get surface */
		surface = PicoGetModelSurface( model, i );
		if ( surface == NULL ) {
			continue;
		}

		/* only handle triangle surfaces initially (fixme: support patches) */
		if ( PicoGetSurfaceType( surface ) != PICO_TRIANGLES ) {
			continue;
		}

		/* get shader (fixme: support shader remapping) */
		shader = PicoGetSurfaceShader( surface );
		if ( shader == NULL ) {
			continue;
		}
		ti.si = ShaderInfoForShaderNull( PicoGetShaderName( shader ) );
		if ( ti.si == NULL ) {
			continue;
		}

		/* translucent surfaces that are neither alphashadow or lightfilter don't cast shadows */
		if ( ( ti.si->compileFlags & C_NODRAW ) ) {
			continue;
		}
		if ( ( ti.si->compileFlags & C_TRANSLUCENT ) &&
			 !( ti.si->compileFlags & C_ALPHASHADOW ) &&
			 !( ti.si->compileFlags & C_LIGHTFILTER ) ) {
			continue;
		}

		/* setup trace info */
		ti.castShadows = castShadows;
		ti.surfaceNum = -1;
		ti.skipGrid = true; // also ignore picomodels when skipping patches

		/* setup trace winding */
		memset( &tw, 0, sizeof( tw ) );
		tw.infoNum = AddTraceInfo( &ti );
		tw.numVerts = 3;

		/* get info */
		numIndexes = PicoGetSurfaceNumIndexes( surface );
		indexes = PicoGetSurfaceIndexes( surface, 0 );

		/* walk the triangle list */
		for ( j = 0; j < numIndexes; j += 3, indexes += 3 )
		{
			for ( k = 0; k < 3; k++ )
			{
				xyz = PicoGetSurfaceXYZ( surface, indexes[ k ] );
				st = PicoGetSurfaceST( surface, 0, indexes[ k ] );
				VectorCopy( xyz, tw.v[ k ].xyz );
				Vector2Copy( st, tw.v[ k ].st );
				m4x4_transform_point( transform, tw.v[ k ].xyz );
			}
			FilterTraceWindingIntoNodes_r( &tw, headNodeNum );
		}
	}
}



/*
   PopulateTraceNodes() - ydnar
   fills the raytracing tree with world and entity occluders
 */

static void PopulateTraceNodes( void ){
	int i, m;
	const char      *value;
	picoModel_t     *model;


	/* add worldspawn triangles */
	m4x4_t transform;
	m4x4_identity( transform );
	PopulateWithBSPModel( &bspModels[ 0 ], transform );

	/* walk each entity list */
	for ( i = 1; i < numEntities; i++ )
	{
		/* get entity */
		entity_t *e = &entities[ i ];

		/* get shadow flags */
		int castShadows = ENTITY_CAST_SHADOWS;
		GetEntityShadowFlags( e, NULL, &castShadows, NULL );

		/* early out? */
		if ( !castShadows ) {
			continue;
		}

		/* get entity origin */
		vec3_t origin;
		GetVectorForKey( e, "origin", origin );

		/* get scale */
		vec3_t scale = { 1.f, 1.f, 1.f };
		if( !ENT_READKV( &scale, e, "modelscale_vec" ) )
			if( ENT_READKV( &scale[0], e, "modelscale" ) )
				scale[1] = scale[2] = scale[0];

		/* get "angle" (yaw) or "angles" (pitch yaw roll), store as (roll pitch yaw) */
		vec3_t angles = { 0.f, 0.f, 0.f };
		if ( !ENT_READKV( &value, e, "angles" ) ||
			3 != sscanf( value, "%f %f %f", &angles[ 1 ], &angles[ 2 ], &angles[ 0 ] ) )
			ENT_READKV( &angles[ 2 ], e, "angle" );

		/* set transform matrix (thanks spog) */
		m4x4_identity( transform );
		m4x4_pivoted_transform_by_vec3( transform, origin, angles, eXYZ, scale, vec3_origin );

		/* hack: Stable-1_2 and trunk have differing row/column major matrix order
		   this transpose is necessary with Stable-1_2
		   uncomment the following line with old m4x4_t (non 1.3/spog_branch) code */
		//%	m4x4_transpose( transform );

		/* get model */
		value = ValueForKey( e, "model" );

		/* switch on model type */
		switch ( value[ 0 ] )
		{
		/* no model */
		case '\0':
			break;

		/* bsp model */
		case '*':
			m = atoi( &value[ 1 ] );
			if ( m <= 0 || m >= numBSPModels ) {
				continue;
			}
			PopulateWithBSPModel( &bspModels[ m ], transform );
			break;

		/* external model */
		default:
			{
				model = LoadModel( value, IntForKey( e, "_frame", "frame" ) );
				if ( model == NULL ) {
					continue;
				}
				PopulateWithPicoModel( castShadows, model, transform );
			}
			continue;
		}

		/* get model2 */
		value = ValueForKey( e, "model2" );

		/* switch on model type */
		switch ( value[ 0 ] )
		{
		/* no model */
		case '\0':
			break;

		/* bsp model */
		case '*':
			m = atoi( &value[ 1 ] );
			if ( m <= 0 || m >= numBSPModels ) {
				continue;
			}
			PopulateWithBSPModel( &bspModels[ m ], transform );
			break;

		/* external model */
		default:
			model = LoadModel( value, IntForKey( e, "_frame2" ) );
			if ( model == NULL ) {
				continue;
			}
			PopulateWithPicoModel( castShadows, model, transform );
			continue;
		}
	}
}




/* -------------------------------------------------------------------------------

   trace initialization

   ------------------------------------------------------------------------------- */

/*
   SetupTraceNodes() - ydnar
   creates a balanced bsp with axis-aligned splits for efficient raytracing
 */

void SetupTraceNodes( void ){
	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SetupTraceNodes ---\n" );

	/* find nodraw bit */
	noDrawContentFlags = noDrawSurfaceFlags = noDrawCompileFlags = 0;
	ApplySurfaceParm( "nodraw", &noDrawContentFlags, &noDrawSurfaceFlags, &noDrawCompileFlags );

	/* create the baseline raytracing tree from the bsp tree */
	headNodeNum = SetupTraceNodes_r( 0 );

	/* create outside node for skybox surfaces */
	skyboxNodeNum = AllocTraceNode();

	/* populate the tree with triangles from the world and shadow casting entities */
	PopulateTraceNodes();

	/* create the raytracing bsp */
	if ( !loMem ) {
		SubdivideTraceNode_r( headNodeNum, 0 );
		SubdivideTraceNode_r( skyboxNodeNum, 0 );
	}

	/* create triangles from the trace windings */
	TriangulateTraceNode_r( headNodeNum );
	TriangulateTraceNode_r( skyboxNodeNum );

	/* emit some stats */
	//%	Sys_FPrintf( SYS_VRB, "%9d original triangles\n", numOriginalTriangles );
	Sys_FPrintf( SYS_VRB, "%9d trace windings (%.2fMB)\n", numTraceWindings, (float) ( numTraceWindings * sizeof( *traceWindings ) ) / ( 1024.0f * 1024.0f ) );
	Sys_FPrintf( SYS_VRB, "%9d trace triangles (%.2fMB)\n", numTraceTriangles, (float) ( numTraceTriangles * sizeof( *traceTriangles ) ) / ( 1024.0f * 1024.0f ) );
	Sys_FPrintf( SYS_VRB, "%9d trace nodes (%.2fMB)\n", numTraceNodes, (float) ( numTraceNodes * sizeof( *traceNodes ) ) / ( 1024.0f * 1024.0f ) );
	Sys_FPrintf( SYS_VRB, "%9d leaf nodes (%.2fMB)\n", numTraceLeafNodes, (float) ( numTraceLeafNodes * sizeof( *traceNodes ) ) / ( 1024.0f * 1024.0f ) );
	//%	Sys_FPrintf( SYS_VRB, "%9d average triangles per leaf node\n", numTraceTriangles / numTraceLeafNodes );
	Sys_FPrintf( SYS_VRB, "%9d average windings per leaf node\n", numTraceWindings / ( numTraceLeafNodes + 1 ) );
	Sys_FPrintf( SYS_VRB, "%9d max trace depth\n", maxTraceDepth );

	/* free trace windings */
	free( traceWindings );
	numTraceWindings = 0;
	maxTraceWindings = 0;
	deadWinding = -1;

	/* debug code: write out trace triangles to an alias obj file */
	#if 0
	{
		int i, j;
		FILE            *file;
		char filename[ 1024 ];
		traceWinding_t  *tw;


		/* open the file */
		strcpy( filename, source );
		path_set_extension( filename, ".lin" );
		Sys_Printf( "Opening light trace file %s...\n", filename );
		file = fopen( filename, "w" );
		if ( file == NULL ) {
			Error( "Error opening %s for writing", filename );
		}

		/* walk node list */
		for ( i = 0; i < numTraceWindings; i++ )
		{
			tw = &traceWindings[ i ];
			for ( j = 0; j < tw->numVerts + 1; j++ )
				fprintf( file, "%f %f %f\n",
						 tw->v[ j % tw->numVerts ].xyz[ 0 ], tw->v[ j % tw->numVerts ].xyz[ 1 ], tw->v[ j % tw->numVerts ].xyz[ 2 ] );
		}

		/* close it */
		fclose( file );
	}
	#endif
}



/* -------------------------------------------------------------------------------

   raytracer

   ------------------------------------------------------------------------------- */

/*
   TraceTriangle()
   based on code written by william 'spog' joseph
   based on code originally written by tomas moller and ben trumbore, journal of graphics tools, 2(1):21-28, 1997
 */

#define BARY_EPSILON            0.01f
#define ASLF_EPSILON            0.0001f /* so to not get double shadows */
#define COPLANAR_EPSILON        0.25f   //%	0.000001f
#define NEAR_SHADOW_EPSILON     1.5f    //%	1.25f
#define SELF_SHADOW_EPSILON     0.5f

bool TraceTriangle( traceInfo_t *ti, traceTriangle_t *tt, trace_t *trace ){
	int i;
	float tvec[ 3 ], pvec[ 3 ], qvec[ 3 ];
	float det, invDet, depth;
	float u, v, w, s, t;
	int is, it;
	byte            *pixel;
	float shadow;
	shaderInfo_t    *si;


	/* don't double-trace against sky */
	si = ti->si;
	if ( trace->compileFlags & si->compileFlags & C_SKY ) {
		return false;
	}

	/* receive shadows from worldspawn group only */
	if ( trace->recvShadows == 1 ) {
		if ( ti->castShadows != 1 ) {
			return false;
		}
	}

	/* receive shadows from same group and worldspawn group */
	else if ( trace->recvShadows > 1 ) {
		if ( ti->castShadows != 1 && abs( ti->castShadows ) != abs( trace->recvShadows ) ) {
			return false;
		}
		//%	Sys_Printf( "%d:%d ", tt->castShadows, trace->recvShadows );
	}

	/* receive shadows from the same group only (< 0) */
	else
	{
		if ( abs( ti->castShadows ) != abs( trace->recvShadows ) ) {
			return false;
		}
	}

	/* skip patches when doing the grid (FIXME this is an ugly hack) */
	if ( inGrid ) {
		if ( ti->skipGrid ) {
			return false;
		}
	}

	/* begin calculating determinant - also used to calculate u parameter */
	CrossProduct( trace->direction, tt->edge2, pvec );

	/* if determinant is near zero, trace lies in plane of triangle */
	det = DotProduct( tt->edge1, pvec );

	/* the non-culling branch */
	if ( fabs( det ) < COPLANAR_EPSILON ) {
		return false;
	}
	invDet = 1.0f / det;

	/* calculate distance from first vertex to ray origin */
	VectorSubtract( trace->origin, tt->v[ 0 ].xyz, tvec );

	/* calculate u parameter and test bounds */
	u = DotProduct( tvec, pvec ) * invDet;
	if ( u < -BARY_EPSILON || u > ( 1.0f + BARY_EPSILON ) ) {
		return false;
	}

	/* prepare to test v parameter */
	CrossProduct( tvec, tt->edge1, qvec );

	/* calculate v parameter and test bounds */
	v = DotProduct( trace->direction, qvec ) * invDet;
	if ( v < -BARY_EPSILON || ( u + v ) > ( 1.0f + BARY_EPSILON ) ) {
		return false;
	}

	/* calculate t (depth) */
	depth = DotProduct( tt->edge2, qvec ) * invDet;
	if ( depth <= trace->inhibitRadius || depth >= trace->distance ) {
		return false;
	}

	/* if hitpoint is really close to trace origin (sample point), then check for self-shadowing */
	if ( depth <= SELF_SHADOW_EPSILON ) {
		/* don't self-shadow */
		for ( i = 0; i < trace->numSurfaces; i++ )
		{
			if ( ti->surfaceNum == trace->surfaces[ i ] ) {
				return false;
			}
		}
	}

	/* stack compile flags */
	trace->compileFlags |= si->compileFlags;

	/* don't trace against sky */
	if ( si->compileFlags & C_SKY ) {
		return false;
	}

	/* most surfaces are completely opaque */
	if ( !( si->compileFlags & ( C_ALPHASHADOW | C_LIGHTFILTER ) ) ||
		 si->lightImage == NULL || si->lightImage->pixels == NULL ) {
		VectorMA( trace->origin, depth, trace->direction, trace->hit );
		VectorClear( trace->color );
		trace->opaque = true;
		return true;
	}

	/* force subsampling because the lighting is texture dependent */
	trace->forceSubsampling = 1.0;

	/* try to avoid double shadows near triangle seams */
	if ( u < -ASLF_EPSILON || u > ( 1.0f + ASLF_EPSILON ) ||
		 v < -ASLF_EPSILON || ( u + v ) > ( 1.0f + ASLF_EPSILON ) ) {
		return false;
	}

	/* calculate w parameter */
	w = 1.0f - ( u + v );

	/* calculate st from uvw (barycentric) coordinates */
	s = w * tt->v[ 0 ].st[ 0 ] + u * tt->v[ 1 ].st[ 0 ] + v * tt->v[ 2 ].st[ 0 ];
	t = w * tt->v[ 0 ].st[ 1 ] + u * tt->v[ 1 ].st[ 1 ] + v * tt->v[ 2 ].st[ 1 ];
	s = s - floor( s );
	t = t - floor( t );
	is = s * si->lightImage->width;
	it = t * si->lightImage->height;
	if ( is < 0 ) {
		is = 0;
	}
	if ( is > si->lightImage->width - 1 ) {
		is = si->lightImage->width - 1;
	}
	if ( it < 0 ) {
		it = 0;
	}
	if ( it > si->lightImage->height - 1 ) {
		it = si->lightImage->height - 1;
	}

	/* get pixel */
	pixel = si->lightImage->pixels + 4 * ( it * si->lightImage->width + is );

	/* ydnar: color filter */
	if ( si->compileFlags & C_LIGHTFILTER ) {
		/* filter by texture color */
		trace->color[ 0 ] *= ( ( 1.0f / 255.0f ) * pixel[ 0 ] );
		trace->color[ 1 ] *= ( ( 1.0f / 255.0f ) * pixel[ 1 ] );
		trace->color[ 2 ] *= ( ( 1.0f / 255.0f ) * pixel[ 2 ] );
	}

	/* ydnar: alpha filter */
	if ( si->compileFlags & C_ALPHASHADOW ) {
		/* filter by inverse texture alpha */
		shadow = ( 1.0f / 255.0f ) * ( 255 - pixel[ 3 ] );
		trace->color[ 0 ] *= shadow;
		trace->color[ 1 ] *= shadow;
		trace->color[ 2 ] *= shadow;
	}

	/* check filter for opaque */
	if ( trace->color[ 0 ] <= 0.001f && trace->color[ 1 ] <= 0.001f && trace->color[ 2 ] <= 0.001f ) {
		VectorClear( trace->color );
		VectorMA( trace->origin, depth, trace->direction, trace->hit );
		trace->opaque = true;
		return true;
	}

	/* continue tracing */
	return false;
}



/*
   TraceWinding() - ydnar
   temporary hack
 */

bool TraceWinding( traceWinding_t *tw, trace_t *trace ){
	int i;
	traceTriangle_t tt;


	/* initial setup */
	tt.infoNum = tw->infoNum;
	tt.v[ 0 ] = tw->v[ 0 ];

	/* walk vertex list */
	for ( i = 1; i + 1 < tw->numVerts; i++ )
	{
		/* set verts */
		tt.v[ 1 ] = tw->v[ i ];
		tt.v[ 2 ] = tw->v[ i + 1 ];

		/* find vectors for two edges sharing the first vert */
		VectorSubtract( tt.v[ 1 ].xyz, tt.v[ 0 ].xyz, tt.edge1 );
		VectorSubtract( tt.v[ 2 ].xyz, tt.v[ 0 ].xyz, tt.edge2 );

		/* trace it */
		if ( TraceTriangle( &traceInfos[ tt.infoNum ], &tt, trace ) ) {
			return true;
		}
	}

	/* done */
	return false;
}




/*
   TraceLine_r()
   returns true if something is hit and tracing can stop

   SmileTheory: made half-iterative
 */

#define TRACELINE_R_HALF_ITERATIVE 1

#if TRACELINE_R_HALF_ITERATIVE
static bool TraceLine_r( int nodeNum, const vec3_t start, const vec3_t end, trace_t *trace )
#else
static bool TraceLine_r( int nodeNum, const vec3_t origin, const vec3_t end, trace_t *trace )
#endif
{
	traceNode_t     *node;
	int side;
	float front, back, frac;
	vec3_t mid;

#if TRACELINE_R_HALF_ITERATIVE
	vec3_t origin;
	VectorCopy( start, origin );

	while ( 1 )
#endif
	{
		/* bogus node number means solid, end tracing unless testing all */
		if ( nodeNum < 0 ) {
			VectorCopy( origin, trace->hit );
			trace->passSolid = true;
			return true;
		}

		/* get node */
		node = &traceNodes[ nodeNum ];

		/* solid? */
		if ( node->type == TRACE_LEAF_SOLID ) {
			VectorCopy( origin, trace->hit );
			trace->passSolid = true;
			return true;
		}

		/* leafnode? */
		if ( node->type < 0 ) {
			/* note leaf and return */
			if ( node->numItems > 0 && trace->numTestNodes < MAX_TRACE_TEST_NODES ) {
				trace->testNodes[ trace->numTestNodes++ ] = nodeNum;
			}
			return false;
		}

		/* ydnar 2003-09-07: don't test branches of the bsp with nothing in them when testall is enabled */
		if ( trace->testAll && node->numItems == 0 ) {
			return false;
		}

		/* classify beginning and end points */
		switch ( node->type )
		{
		case PLANE_X:
			front = origin[ 0 ] - node->plane[ 3 ];
			back = end[ 0 ] - node->plane[ 3 ];
			break;

		case PLANE_Y:
			front = origin[ 1 ] - node->plane[ 3 ];
			back = end[ 1 ] - node->plane[ 3 ];
			break;

		case PLANE_Z:
			front = origin[ 2 ] - node->plane[ 3 ];
			back = end[ 2 ] - node->plane[ 3 ];
			break;

		default:
			front = DotProduct( origin, node->plane ) - node->plane[ 3 ];
			back = DotProduct( end, node->plane ) - node->plane[ 3 ];
			break;
		}

		/* entirely in front side? */
		if ( front >= -TRACE_ON_EPSILON && back >= -TRACE_ON_EPSILON ) {
#if TRACELINE_R_HALF_ITERATIVE
			nodeNum = node->children[ 0 ];
			continue;
#else
			return TraceLine_r( node->children[ 0 ], origin, end, trace );
#endif
		}

		/* entirely on back side? */
		if ( front < TRACE_ON_EPSILON && back < TRACE_ON_EPSILON ) {
#if TRACELINE_R_HALF_ITERATIVE
			nodeNum = node->children[ 1 ];
			continue;
#else
			return TraceLine_r( node->children[ 1 ], origin, end, trace );
#endif
		}

		/* select side */
		side = front < 0;

		/* calculate intercept point */
		frac = front / ( front - back );
		mid[ 0 ] = origin[ 0 ] + ( end[ 0 ] - origin[ 0 ] ) * frac;
		mid[ 1 ] = origin[ 1 ] + ( end[ 1 ] - origin[ 1 ] ) * frac;
		mid[ 2 ] = origin[ 2 ] + ( end[ 2 ] - origin[ 2 ] ) * frac;

		/* fixme: check inhibit radius, then solid nodes and ignore */

		/* set trace hit here */
		//%	VectorCopy( mid, trace->hit );

		/* trace first side */
		if ( TraceLine_r( node->children[ side ], origin, mid, trace ) ) {
			return true;
		}

		/* trace other side */
#if TRACELINE_R_HALF_ITERATIVE
		nodeNum = node->children[ !side ];
		VectorCopy( mid, origin );
#else
		return TraceLine_r( node->children[ !side ], mid, end, trace );
#endif
	}
}



/*
   TraceLine() - ydnar
   rewrote this function a bit :)
 */

void TraceLine( trace_t *trace ){
	int i, j;
	traceNode_t     *node;
	traceTriangle_t *tt;
	traceInfo_t     *ti;


	/* setup output (note: this code assumes the input data is completely filled out) */
	trace->passSolid = false;
	trace->opaque = false;
	trace->compileFlags = 0;
	trace->numTestNodes = 0;

	/* early outs */
	if ( !trace->recvShadows || !trace->testOcclusion || trace->distance <= 0.00001f ) {
		return;
	}

	/* trace through nodes */
	TraceLine_r( headNodeNum, trace->origin, trace->end, trace );
	if ( trace->passSolid && !trace->testAll ) {
		trace->opaque = true;
		return;
	}

	/* skip surfaces? */
	if ( noSurfaces ) {
		return;
	}

	/* testall means trace through sky */
	if ( trace->testAll && trace->numTestNodes < MAX_TRACE_TEST_NODES &&
		 trace->compileFlags & C_SKY &&
		 ( trace->numSurfaces == 0 || surfaceInfos[ trace->surfaces[ 0 ] ].childSurfaceNum < 0 ) ) {
		//%	trace->testNodes[ trace->numTestNodes++ ] = skyboxNodeNum;
		TraceLine_r( skyboxNodeNum, trace->origin, trace->end, trace );
	}

	/* walk node list */
	for ( i = 0; i < trace->numTestNodes; i++ )
	{
		/* get node */
		node = &traceNodes[ trace->testNodes[ i ] ];

		/* walk node item list */
		for ( j = 0; j < node->numItems; j++ )
		{
			tt = &traceTriangles[ node->items[ j ] ];
			ti = &traceInfos[ tt->infoNum ];
			if ( TraceTriangle( ti, tt, trace ) ) {
				return;
			}
			//%	if( TraceWinding( &traceWindings[ node->items[ j ] ], trace ) )
			//%		return;
		}
	}
}



/*
   SetupTrace() - ydnar
   sets up certain trace values
 */

float SetupTrace( trace_t *trace ){
	VectorSubtract( trace->end, trace->origin, trace->displacement );
	trace->distance = VectorFastNormalize( trace->displacement, trace->direction );
	VectorCopy( trace->origin, trace->hit );
	return trace->distance;
}

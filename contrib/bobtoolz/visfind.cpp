// Requires parts of the q3 tools source to compile
// Date: Oct 5, 2001
// Written by: Brad Whitehead (whiteheb@gamerstv.net)

#include "visfind.h"
#include "dialogs/dialogs-gtk.h"
#include "bsploader.h"
#include "DVisDrawer.h"

typedef struct {
	int portalclusters;
	int leafbytes;           //leafbytes = ((portalclusters+63)&~63)>>3;
} vis_header;

// added because int shift = 32; i = 0xFFFFFFFF >> shift;
// then i = 0xFFFFFFFF, when it should = 0
const unsigned long bitmasks[33] =
{
	0x00000000,
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
	0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
	0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
	0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
	0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
	0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

int bsp_leafnumfororigin( vec3_t origin ){
	dnode_t     *node;
	dplane_t    *plane;
	float d;

	// TODO: check if origin is in the map??

	node = dnodes;
	while ( true )
	{
		plane = &dplanes[node->planeNum];
		d = DotProduct( origin, plane->normal ) - plane->dist;
		if ( d >= 0 ) {
			if ( node->children[0] < 0 ) {
				return -( node->children[0] + 1 );
			}
			else{
				node = &dnodes[node->children[0]];
			}
		}
		else if ( node->children[1] < 0 ) {
			return -( node->children[1] + 1 );
		}
		else{
			node = &dnodes[node->children[1]];
		}
	}
	return 0;
}

int bsp_leafnumforcluster( int cluster ){
	dleaf_t *l;
	int i;

	for ( i = 0, l = dleafs; i < numleafs; i++, l++ )
		if ( l->cluster == cluster ) {
			return( i );
		}
	return( 0 );
}

// leaf1 = origin leaf
// leaf2 = leaf to test for
/*int bsp_InPVS(int cluster1, int cluster2)
   {
    vis_header		*vheader;
    byte			*visdata;

    vheader = (vis_header *) visBytes;
    visdata = visBytes + VIS_HEADER_SIZE;

    return( *( visdata + ( cluster1 * vheader->leafbytes ) + (cluster2 / 8) ) & ( 1 << ( cluster2 % 8 ) ) );
   }*/

void bsp_setbitvectorlength( byte *v, int length_bits, int length_vector ){
	int i;

	i = length_bits / 8;

	*( v + i ) = (byte) bitmasks[length_bits % 8];

	memset( ( v + i + 1 ), 0, length_vector - i - 1 );
}


void bsp_bitvectorsubtract( byte *first, byte *second, byte *out, int length ){

	int i;

	for ( i = 0; i < length; i++ )
		*( out + i ) = *( first + i ) & ~( *( second + i ) );
}

int bsp_countclusters( byte *bitvector, int length ){
	int i, j, c;

	c = 0;
	for ( i = 0; i < length; i++ )
		for ( j = 0; j < 8; j++ )
			if ( ( *( bitvector + i ) & ( 1 << j ) ) ) {
				c++;
			}
	return( c );
}

int bsp_countclusters_mask( byte *bitvector, byte *maskvector, int length ){
	int i, j, c;

	c = 0;
	for ( i = 0; i < length; i++ )
		for ( j = 0; j < 8; j++ )
			if ( ( *( bitvector + i ) & ( 1 << j ) ) && ( *( maskvector + i ) & ( 1 << j ) ) ) {
				c++;
			}
	return( c );
}

void AddCluster( DMetaSurfaces* pointlist, dleaf_t    *cl, bool* repeatlist, const vec3_t clr ){
	int* leafsurf = &dleafsurfaces[cl->firstLeafSurface];
	for ( int k = 0; k < cl->numLeafSurfaces; k++, leafsurf++ )
	{
		if ( repeatlist[*leafsurf] ) {
			continue;
		}

		dsurface_t* surf = &drawSurfaces[*leafsurf];
		if ( surf->surfaceType != MST_PLANAR ) {
			continue;
		}

		qdrawVert_t* vert = &drawVerts[surf->firstVert];
		if ( surf->firstVert + surf->numVerts > numDrawVerts ) {
			DoMessageBox( "Warning", "Warning", EMessageBoxType::Warning );
		}

		DMetaSurf* meta = new DMetaSurf( surf->numVerts, surf->numIndexes );

		for ( int l = 0; l < surf->numIndexes; ++l )
			meta->indices[l] = drawVertsIndices[ surf->firstIndex + l ];
		for ( int l = 0; l < surf->numVerts; ++l, ++vert )
			VectorCopy( vert->xyz, meta->verts[l] );
		VectorCopy( clr, meta->colour );

		pointlist->push_back( meta );

		repeatlist[*leafsurf] = true;
	}
}

/*
   =============
   CreateTrace
   =============
 */
DMetaSurfaces *CreateTrace( dleaf_t *leaf, int c, vis_header *header, byte *visdata, byte *seen ){
	byte        *vis;
	int i, j, clusterNum;
	DMetaSurfaces* pointlist = new DMetaSurfaces;
	bool*   repeatlist = new bool[numDrawSurfaces];
	dleaf_t     *cl;

	const vec3_t clrGreen =   {0.f, 1.f, 0.f};

	memset( repeatlist, 0, sizeof( bool ) * numDrawSurfaces );

	vis = visdata + ( c * header->leafbytes );

	clusterNum = 0;

	AddCluster( pointlist, &( dleafs[bsp_leafnumforcluster( c )] ), repeatlist, clrGreen );

	for ( i = 0; i < header->leafbytes; i++ )
	{
		for ( j = 0; j < 8; j++ )
		{
			cl = &( dleafs[bsp_leafnumforcluster( clusterNum )] );

			if ( ( *( vis + i ) & ( 1 << j ) ) && ( *( seen + i ) & ( 1 << j ) ) && ( leaf->area == cl->area ) ) {
				vec3_t clr;
				do {
					VectorSet( clr,
					           ( rand() % 256 ) / 255.f,
					           ( rand() % 256 ) / 255.f,
					           ( rand() % 256 ) / 255.f );
				} while( ( clr[0] + clr[2] < clr[1] * 1.5f ) ||	//too green
				         ( clr[0] < .3 && clr[1] < .3 && clr[2] < .3 ) ); //too dark

				AddCluster( pointlist, cl, repeatlist, clr );
			}
			clusterNum++;
		}
	}

	delete [] repeatlist;

	return pointlist;
}

/*
   =============
   TraceCluster

   setup for CreateTrace
   =============
 */
DMetaSurfaces* TraceCluster( int leafnum ){
	byte seen[( MAX_MAP_LEAFS / 8 ) + 1];
	vis_header      *vheader;
	byte            *visdata;
	dleaf_t         *leaf;

	vheader = (vis_header *) visBytes;
	visdata = visBytes + sizeof( vis_header );

	memset( seen, 0xFF, sizeof( seen ) );
	bsp_setbitvectorlength( seen, vheader->portalclusters, sizeof( seen ) );

	leaf = &( dleafs[leafnum] );

	return CreateTrace( leaf, leaf->cluster, vheader, visdata, seen );
}

DMetaSurfaces* BuildTrace( char* filename, vec3_t v_origin ){
	if ( !LoadBSPFile( filename ) ) {
		return NULL;
	}

	if( numVisBytes == 0 ){
		FreeBSPData();
		globalErrorStream() << "bobToolz VisAnalyse: Bsp has no visibility data!\n";
		return 0;
	}

	int leafnum = bsp_leafnumfororigin( v_origin );

	if( dleafs[leafnum].cluster == -1 ){
		FreeBSPData();
		globalErrorStream() << "bobToolz VisAnalyse: Point of interest is in the void!\n";
		return 0;
	}

	DMetaSurfaces* pointlist = TraceCluster( leafnum );

	FreeBSPData();

	return pointlist;
}

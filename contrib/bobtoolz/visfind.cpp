// Requires parts of the q3 tools source to compile
// Date: Oct 5, 2001
// Written by: Brad Whitehead (whiteheb@gamerstv.net)

#include "visfind.h"
#include "funchandlers.h"
#include "DPlane.h"

typedef struct {
	int portalclusters;      //number of clusters
	int leafbytes;           //size of data per cluster //leafbytes = ((portalclusters+63)&~63)>>3; //multiple of 8
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

std::vector<dplane_t*> bsp_planesForLeaf( int leafnum ){
	std::vector<dplane_t*> planes;

	int findnum = -( leafnum + 1 );
	auto cmp = [&]( const dnode_t& node ){
		return
		  findnum == node.children[0]
		? ( planes.push_back( &dplanes[node.planeNum] ), true ) //inverse of plane, which would work for convex hull; to match ChopWindingInPlace 🤔
		: findnum == node.children[1]
		? ( planes.push_back( &dplanes[node.planeNum ^ 1] ), true ) //inverse of plane, which would work for convex hull; to match ChopWindingInPlace 🤔
		: false;
	};
	dnode_t *dnodesend = dnodes + numnodes;
	dnode_t *node = std::find_if( dnodes, dnodesend, cmp );

	while( node != dnodesend ){
		findnum = node - dnodes;
		node = std::find_if( dnodes, dnodesend, cmp );
	};

	return planes;
}

std::vector<DWinding> BuildLeafWindings( int leafnum ){
	std::vector<DWinding> windings;
	std::vector<dplane_t*> planes = bsp_planesForLeaf( leafnum );
	DPlane dplane;

	for( const dplane_t *plane : planes )
	{
		VectorCopy( plane->normal, dplane.normal );
		dplane._d = plane->dist;
		DWinding winding = dplane.BaseWindingForPlane();
		for( const dplane_t *cplane : planes )
		{
			if( winding.numpoints <= 2 ){
				break;
			}
			if( cplane != plane ){
				VectorCopy( cplane->normal, dplane.normal );
				dplane._d = cplane->dist;
				winding.ChopWindingInPlace( &dplane, .1f );
			}
		}
		if( winding.numpoints > 2 ){
			windings.push_back( std::move( winding ) );
		}
	}

	return windings;
}


int bsp_leafnumforcluster( int cluster ){
	for ( int i = 0; i < numleafs; ++i )
		if ( dleafs[ i ].cluster == cluster ) {
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

inline bool bit_is_enabled( const byte *bytes, int bit_index ){
	return ( bytes[bit_index >> 3] & ( 1 << ( bit_index & 7 ) ) ) != 0;
}

static void PutPointsOnCurve( vec3_t ctrl[], int width, int height ) {
	const auto lerp3 = []( const vec3_t prev, vec3_t mid, const vec3_t next ){
		mid[0] = ( prev[0] + mid[0] * 2 + next[0] ) * .25;
		mid[1] = ( prev[1] + mid[1] * 2 + next[1] ) * .25;
		mid[2] = ( prev[2] + mid[2] * 2 + next[2] ) * .25;
	};
	// put all the aproximating points on the curve
	for ( int i = 0; i < width; ++i )
		for ( int j = 1; j < height; j += 2 )
			lerp3( ctrl[( j - 1 ) * width + i],
			       ctrl[( j + 0 ) * width + i],
			       ctrl[( j + 1 ) * width + i] );

	for ( int j = 0; j < height; ++j )
		for ( int i = 1; i < width; i += 2 )
			lerp3( ctrl[j * width + i - 1],
			       ctrl[j * width + i + 0],
			       ctrl[j * width + i + 1] );
}


const vec3_t debugColorMain = { 0, 255, 0 };
const vec3_t debugColors[ 16 ] =
	{
		{ 1   , 0   , 0    },
		{ 0.75, 0.5 , 0.5  },
		{ 1   , 1   , 0    },
		{ 0.75, 0.75, 0.5  },
		{ 0   , 1   , 1    },
		{ 0.5 , 0.75, 0.75 },
		{ 0   , 0   , 1    },
		{ 0.5 , 0.5 , 0.75 },
		{ 1   , 0   , 1    },
		{ 0.75, 0.5 , 0.75 },
		{ 0.5 , 0.75, 0.5  },
		{ 0.75, 0   , 0.5  },
		{ 0.5 , 0   , 0.75 },
		{ 0   , 0.5 , 0.75 },
		{ 0.75, 0.5 , 0    },
		{ 1   , 0   , 0.5  },
	};
#if 0
			vec3_t clr;
			do {
				VectorSet( clr,
				           ( rand() % 256 ) / 255.f,
				           ( rand() % 256 ) / 255.f,
				           ( rand() % 256 ) / 255.f );
			} while( ( clr[0] + clr[2] < clr[1] * 1.5f ) ||	//too green
			         ( clr[0] < .3 && clr[1] < .3 && clr[2] < .3 ) ); //too dark
#endif


void AddSurface( const dsurface_t& surf, const vec3_t clr, DMetaSurfaces* pointlist ){
	if ( surf.surfaceType == MST_PLANAR || surf.surfaceType == MST_TRIANGLE_SOUP ) {
		qdrawVert_t* vert = &drawVerts[surf.firstVert];

		DMetaSurf& meta = pointlist->emplace_back( surf.numVerts, surf.numIndexes );

		for ( int l = 0; l < surf.numIndexes; ++l )
			meta.indices[l] = drawVertsIndices[ surf.firstIndex + l ];
		for ( int l = 0; l < surf.numVerts; ++l, ++vert )
			VectorCopy( vert->xyz, meta.verts[l] );
		VectorCopy( clr, meta.colour );
	}
	else if ( surf.surfaceType == MST_PATCH ) {
		qdrawVert_t* vert = &drawVerts[surf.firstVert];

		DMetaSurf& meta = pointlist->emplace_back( surf.numVerts, ( surf.patchWidth - 1 ) * ( surf.patchHeight - 1 ) * 2 * 3 );

		for ( int l = 0; l < surf.numVerts; ++l, ++vert )
			VectorCopy( vert->xyz, meta.verts[l] );
		VectorCopy( clr, meta.colour );

		PutPointsOnCurve( meta.verts, surf.patchWidth, surf.patchHeight );
		unsigned int *idx = meta.indices;
		for ( int i = 0; i < surf.patchWidth - 1; ++i ){
			for ( int j = 0; j < surf.patchHeight - 1; ++j ){
				idx[0] = ( j + 0 ) * surf.patchWidth + i;
				idx[1] = ( j + 1 ) * surf.patchWidth + i;
				idx[2] = ( j + 1 ) * surf.patchWidth + i + 1;
				idx[3] = idx[0];
				idx[4] = idx[2];
				idx[5] = ( j + 0 ) * surf.patchWidth + i + 1;
				idx += 6;
			}
		}
	}
};

void AddLeafSurfaces( const dleaf_t& leaf, const vec3_t clr, bool colorPerSurf, bool* repeatlist, DMetaSurfaces* pointlist ){
	for( int leafsurf : std::span( dleafsurfaces + leaf.firstLeafSurface, leaf.numLeafSurfaces ) )
	{
		if ( !repeatlist[leafsurf] ) {
			repeatlist[leafsurf] = true;
			AddSurface( drawSurfaces[leafsurf], colorPerSurf? debugColors[leafsurf & 15] : clr, pointlist );
		}
	}
}


#include "timer.h"
/*
   =============
   TraceCluster

   setup for CreateTrace
   =============
 */
class TraceCluster
{
	const vis_header vheader;
	byte            *visdata;
	bool            *repeatlist;
	void repeatlist_clear() const {
		memset( repeatlist, 0, sizeof( bool ) * numDrawSurfaces );
	}
public:
	TraceCluster()
	:	vheader( *(vis_header *) visBytes ),
		visdata( visBytes + sizeof( vis_header ) ),
		repeatlist( new bool[numDrawSurfaces] )
	{}
	~TraceCluster(){
		delete [] repeatlist;
	}
	TraceCluster( TraceCluster&& ) noexcept = delete;

	DMetaSurfaces* doTraceCluster( int leafnum, bool colorPerSurf ) const {
		DMetaSurfaces* pointlist = new DMetaSurfaces;

		repeatlist_clear();

		const dleaf_t& leaf = dleafs[leafnum];
		const byte *vis = visdata + ( leaf.cluster * vheader.leafbytes );

		AddLeafSurfaces( leaf, debugColorMain, false, repeatlist, pointlist );

		for ( const dleaf_t& cleaf : std::span( dleafs, numleafs ) )
		{
			if ( cleaf.cluster >= 0 && cleaf.cluster < vheader.portalclusters
			  && leaf.area == cleaf.area
			  && bit_is_enabled( vis, cleaf.cluster ) )
			{
				AddLeafSurfaces( cleaf, debugColors[( &cleaf - dleafs )%15], colorPerSurf, repeatlist, pointlist );
			}
		}

		return pointlist;
	}

	void traverseLeafs( DVisDrawer& drawer ) const {
		Timer timer;
		std::vector<int> shadernums;
		int nsurfs, nleafs;
		for( const dleaf_t& leaf : std::span( dleafs, numleafs ) ){
			if( leaf.cluster >= 0 && leaf.cluster < vheader.portalclusters ){
				nsurfs = nleafs = 0;
				shadernums.clear();

				auto countLeafStuff = [&nsurfs, &shadernums, this]( const dleaf_t& leaf ){
					for( int leafsurf : std::span( dleafsurfaces + leaf.firstLeafSurface, leaf.numLeafSurfaces ) )
					{
						if ( !repeatlist[leafsurf] ) {
							repeatlist[leafsurf] = true;

							const dsurface_t& surf = drawSurfaces[leafsurf];

							if( surf.surfaceType != MST_BAD )
								++nsurfs;
							if( std::ranges::find( shadernums, surf.shaderNum ) == shadernums.cend() )
								shadernums.push_back( surf.shaderNum );
						}
					}
				};

				repeatlist_clear();

				const byte *vis = visdata + ( leaf.cluster * vheader.leafbytes );

				countLeafStuff( leaf );

				for ( const dleaf_t& cleaf : std::span( dleafs, numleafs ) )
				{
					if ( cleaf.cluster >= 0 && cleaf.cluster < vheader.portalclusters
					  && leaf.area == cleaf.area
					  && bit_is_enabled( vis, cleaf.cluster ) )
					{
						countLeafStuff( cleaf );
						++nleafs;
					}
				}

				drawer.ui_leaf_add( &leaf - dleafs, nleafs, nsurfs, shadernums.size() );
			}
		}
		globalOutputStream() << timer.elapsed_msec() << " timer.elapsed_msec()\n";
	}
};


void SetupVisView( const char* filename, vec3_t v_origin ){
	/* g_VisView and LoadBSPFile must have same lifetime */
	if( !g_VisView ){
		if ( !LoadBSPFile( filename ) ) {
			return;
		}

		if( numVisBytes <= (int)sizeof( vis_header ) ){
			globalErrorStream() << "bobToolz VisAnalyse: Bsp has no visibility data!\n";

			int maxcluster = -1;
			std::for_each_n( dleafs, numleafs, [&maxcluster]( const dleaf_t& leaf ){ maxcluster = std::max( maxcluster, leaf.cluster ); } );
			if( maxcluster >= 0 ){
				globalErrorStream() << "bobToolz VisAnalyse: Setting up fake visibility data!\n";
				delete[] visBytes;
				vis_header vheader;
				vheader.portalclusters = maxcluster + 1;
				vheader.leafbytes = ( ( vheader.portalclusters + 63 ) & ~63 ) >> 3;
				numVisBytes = sizeof( vis_header ) + vheader.portalclusters * vheader.leafbytes;
				visBytes = new byte[ numVisBytes ];
				memset( visBytes, 0xFF, numVisBytes );
				*( vis_header* )visBytes = vheader;
			}
			else{
				FreeBSPData();
				return;
			}
		}

		g_VisView = std::make_unique<DVisDrawer>();
		TraceCluster traceCluster;
		traceCluster.traverseLeafs( *g_VisView );
		g_VisView->ui_show();
	}

	const int leafnum = bsp_leafnumfororigin( v_origin );

	if( dleafs[leafnum].cluster == -1 ){
		g_VisView->ClearPoints();
		globalErrorStream() << "bobToolz VisAnalyse: Point of interest is in the void!\n";
		return;
	}

	g_VisView->ui_leaf_show( leafnum );
}

DMetaSurfaces* BuildTrace( int leafnum, bool colorPerSurf ){
	TraceCluster traceCluster;
	return traceCluster.doTraceCluster( leafnum, colorPerSurf );
}

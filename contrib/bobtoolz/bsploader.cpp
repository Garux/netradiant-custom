#include "bsploader.h"
#include "dialogs/dialogs-gtk.h"
#include "commandlib.h"

int numnodes;
int numplanes;
int numleafs;
int numleafsurfaces;
int numVisBytes;
int numDrawVerts;
int numDrawVertsIndices;
int numDrawSurfaces;
int numbrushes;
int numbrushsides;
int numleafbrushes;

byte          *visBytes =         nullptr;
dnode_t       *dnodes =           nullptr;
dplane_t      *dplanes =          nullptr;
dleaf_t       *dleafs =           nullptr;
qdrawVert_t   *drawVerts =        nullptr;
int           *drawVertsIndices = nullptr;
dsurface_t    *drawSurfaces =     nullptr;
int           *dleafsurfaces =    nullptr;
dbrush_t      *dbrushes =         nullptr;
dbrushside_t  *dbrushsides =      nullptr;
int           *dleafbrushes =     nullptr;

#define IBSP_IDENT   ( ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'B' << 8 ) + 'I' )
#define IBSP_VERSION_Q3         46
#define IBSP_VERSION_WOLF       47   // also quakelive
// jka, jk2, sof2
#define RBSP_IDENT   ( ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'B' << 8 ) + 'R' )
#define RBSP_VERSION             1
// warsow
#define FBSP_IDENT   ( ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'B' << 8 ) + 'F' )
#define FBSP_VERSION             1

/*
   ================
   FileLength
   ================
 */
int FileLength( FILE *f ){
	int pos;
	int end;

	pos = ftell( f );
	fseek( f, 0, SEEK_END );
	end = ftell( f );
	fseek( f, pos, SEEK_SET );

	return end;
}

/*
   ==============
   LoadFile
   ==============
 */
bool    LoadFile( const char *filename, byte **bufferptr ){
	FILE    *f;
	int length;
	byte    *buffer;

	f = fopen( filename, "rb" );
	if ( !f ) {
		return false;
	}

	length = FileLength( f );
	buffer = new byte[length + 1];
	buffer[length] = 0;
	fread( buffer, 1, length, f );
	fclose( f );

	*bufferptr = buffer;
	return true;
}

int    LittleLong( int l ){
#if defined( __BIG_ENDIAN__ )
	std::reverse( reinterpret_cast<unsigned char*>( &l ), reinterpret_cast<unsigned char*>( &l ) + sizeof( int ) );
#endif
	return l;
}

float   LittleFloat( float l ){
#if defined( __BIG_ENDIAN__ )
	std::reverse( reinterpret_cast<unsigned char*>( &l ), reinterpret_cast<unsigned char*>( &l ) + sizeof( float ) );
#endif
	return l;
}

/*
   =============
   SwapBlock

   If all values are 32 bits, this can be used to swap everything
   =============
 */
void SwapBlock( int *block, int sizeOfBlock ) {
	int i;

	sizeOfBlock >>= 2;
	for ( i = 0; i < sizeOfBlock; ++i ) {
		block[i] = LittleLong( block[i] );
	}
}

/*
   =============
   SwapBSPFile

   Byte swaps all data in a bsp file.
   =============
 */
void SwapBSPFile() {
	int i;

	// models
//	SwapBlock( (int *)dmodels, nummodels * sizeof( dmodels[0] ) );

	// shaders (don't swap the name)
//	for ( i = 0; i < numShaders; ++i ) {
//		dshaders[i].contentFlags = LittleLong( dshaders[i].contentFlags );
//		dshaders[i].surfaceFlags = LittleLong( dshaders[i].surfaceFlags );
//	}

	// planes
	SwapBlock( (int *)dplanes, numplanes * sizeof( dplanes[0] ) );

	// nodes
	SwapBlock( (int *)dnodes, numnodes * sizeof( dnodes[0] ) );

	// leafs
	SwapBlock( (int *)dleafs, numleafs * sizeof( dleafs[0] ) );

	// leaffaces
	SwapBlock( (int *)dleafsurfaces, numleafsurfaces * sizeof( dleafsurfaces[0] ) );

	// leafbrushes
	SwapBlock( (int *)dleafbrushes, numleafbrushes * sizeof( dleafbrushes[0] ) );

	// brushes
	SwapBlock( (int *)dbrushes, numbrushes * sizeof( dbrushes[0] ) );

	// brushsides
	SwapBlock( (int *)dbrushsides, numbrushsides * sizeof( dbrushsides[0] ) );

	// vis
	( (int *)&visBytes )[0] = LittleLong( ( (int *)&visBytes )[0] );
	( (int *)&visBytes )[1] = LittleLong( ( (int *)&visBytes )[1] );

	// drawverts (don't swap colors )
	for ( i = 0; i < numDrawVerts; ++i ) {
		drawVerts[i].lightmap[0] = LittleFloat( drawVerts[i].lightmap[0] );
		drawVerts[i].lightmap[1] = LittleFloat( drawVerts[i].lightmap[1] );
		drawVerts[i].st[0] = LittleFloat( drawVerts[i].st[0] );
		drawVerts[i].st[1] = LittleFloat( drawVerts[i].st[1] );
		drawVerts[i].xyz[0] = LittleFloat( drawVerts[i].xyz[0] );
		drawVerts[i].xyz[1] = LittleFloat( drawVerts[i].xyz[1] );
		drawVerts[i].xyz[2] = LittleFloat( drawVerts[i].xyz[2] );
		drawVerts[i].normal[0] = LittleFloat( drawVerts[i].normal[0] );
		drawVerts[i].normal[1] = LittleFloat( drawVerts[i].normal[1] );
		drawVerts[i].normal[2] = LittleFloat( drawVerts[i].normal[2] );
	}

	// drawindexes
	SwapBlock( (int *)drawVertsIndices, numDrawVertsIndices * sizeof( drawVertsIndices[0] ) );

	// drawsurfs
	SwapBlock( (int *)drawSurfaces, numDrawSurfaces * sizeof( drawSurfaces[0] ) );

	// fogs
//	for ( i = 0; i < numFogs; ++i ) {
//		dfogs[i].brushNum = LittleLong( dfogs[i].brushNum );
//		dfogs[i].visibleSide = LittleLong( dfogs[i].visibleSide );
//	}
}

/*
   =============
   CopyLump
   =============
 */
int CopyLump( dheader_t *header, int lump, void **dest, int size ) {
	int length, ofs;

	length = header->lumps[lump].filelen;
	ofs = header->lumps[lump].fileofs;

	if ( length == 0 ) {
		return 0;
	}

	*dest = new byte[length];
	memcpy( *dest, (byte *)header + ofs, length );

	return length / size;
}

/*
   =============
   LoadBSPFile
   =============
 */
bool    LoadBSPFile( const char *filename ) {
	dheader_t   *header;

	// load the file header
	if ( !LoadFile( filename, (byte **)&header ) ) {
		DoMessageBox( "BSP file not found", "Error", EMessageBoxType::Error );
		return false;
	}

	// swap the header
	SwapBlock( (int *)header, sizeof( *header ) );

	if ( header->ident != IBSP_IDENT
	  && header->ident != RBSP_IDENT
	  && header->ident != FBSP_IDENT ) {
		DoMessageBox( "Cant find a valid IBSP/RBSP/FBSP file", "Error", EMessageBoxType::Error );
		delete[] header;
		return false;
	}

	if ( !( header->ident == IBSP_IDENT && ( header->version == IBSP_VERSION_Q3 || header->version == IBSP_VERSION_WOLF ) )
	  && !( header->ident == RBSP_IDENT && header->version == RBSP_VERSION )
	  && !( header->ident == FBSP_IDENT && header->version == FBSP_VERSION ) ) {
		DoMessageBox( "File is incorrect version", "Error", EMessageBoxType::Error );
		delete[] header;
		return false;
	}

if( header->ident == IBSP_IDENT )
	numbrushsides       = CopyLump( header, LUMP_BRUSHES,         (void**)&dbrushsides,      sizeof( dbrushside_t ) );
else{
	numbrushsides       = CopyLump( header, LUMP_BRUSHES,         (void**)&dbrushsides,      sizeof( rbspBrushSide_t ) );
	std::copy_n( (rbspBrushSide_t*)dbrushsides, numbrushes, dbrushsides );
}
	numbrushes          = CopyLump( header, LUMP_BRUSHES,         (void**)&dbrushes,         sizeof( dbrush_t ) );
	numplanes           = CopyLump( header, LUMP_PLANES,          (void**)&dplanes,          sizeof( dplane_t ) );
	numleafs            = CopyLump( header, LUMP_LEAFS,           (void**)&dleafs,           sizeof( dleaf_t ) );
	numnodes            = CopyLump( header, LUMP_NODES,           (void**)&dnodes,           sizeof( dnode_t ) );
if( header->ident == IBSP_IDENT )
	numDrawVerts        = CopyLump( header, LUMP_DRAWVERTS,       (void**)&drawVerts,        sizeof( qdrawVert_t ) );
else{
	numDrawVerts        = CopyLump( header, LUMP_DRAWVERTS,       (void**)&drawVerts,        sizeof( rbspDrawVert_t ) );
	std::copy_n( (rbspDrawVert_t*)drawVerts, numDrawVerts, drawVerts );
}
	numDrawVertsIndices = CopyLump( header, LUMP_DRAWINDEXES,     (void**)&drawVertsIndices, sizeof( int ) );
if( header->ident == IBSP_IDENT )
	numDrawSurfaces     = CopyLump( header, LUMP_SURFACES,        (void**)&drawSurfaces,     sizeof( dsurface_t ) );
else{
	numDrawSurfaces     = CopyLump( header, LUMP_SURFACES,        (void**)&drawSurfaces,     sizeof( rbspDrawSurface_t ) );
	std::copy_n( (rbspDrawSurface_t*)drawSurfaces, numDrawSurfaces, drawSurfaces );
}
	numleafsurfaces     = CopyLump( header, LUMP_LEAFSURFACES,    (void**)&dleafsurfaces,    sizeof( int ) );
	numVisBytes         = CopyLump( header, LUMP_VISIBILITY,      (void**)&visBytes,         1 );
	numleafbrushes      = CopyLump( header, LUMP_LEAFBRUSHES,     (void**)&dleafbrushes,     sizeof( int ) );

	delete[] header;      // everything has been copied out

	// swap everything
	SwapBSPFile();

	return true;
}

void FreeBSPData(){
#define DEL( a ) delete[] a; a = 0;
	DEL( visBytes );
	DEL( dnodes );
	DEL( dplanes );
	DEL( dleafs );
	DEL( drawVerts );
	DEL( drawVertsIndices );
	DEL( drawSurfaces );
	DEL( dleafsurfaces );
	DEL( dleafbrushes );
	DEL( dbrushes );
	DEL( dbrushsides );
}

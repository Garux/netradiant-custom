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




/* -------------------------------------------------------------------------------

   this file was copied out of the common directory in order to not break
   compatibility with the q3map 1.x tree. it was moved out in order to support
   the raven bsp format (RBSP) used in soldier of fortune 2 and jedi knight 2.

   since each game has its own set of particular features, the data structures
   below no longer directly correspond to the binary format of a particular game.

   the translation will be done at bsp load/save time to keep any sort of
   special-case code messiness out of the rest of the program.

   ------------------------------------------------------------------------------- */



/* FIXME: remove the functions below that handle memory management of bsp file chunks */

int numBSPDrawVertsBuffer = 0;
void IncDrawVerts(){
	numBSPDrawVerts++;

	if ( bspDrawVerts == 0 ) {
		numBSPDrawVertsBuffer = 1024;

		bspDrawVerts = safe_malloc_info( sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer, "IncDrawVerts" );

	}
	else if ( numBSPDrawVerts > numBSPDrawVertsBuffer ) {
		bspDrawVert_t *newBspDrawVerts;

		numBSPDrawVertsBuffer *= 3; // multiply by 1.5
		numBSPDrawVertsBuffer /= 2;

		newBspDrawVerts = void_ptr( realloc( bspDrawVerts, sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer ) );

		if ( !newBspDrawVerts ) {
			free (bspDrawVerts);
			Error( "realloc() failed (IncDrawVerts)" );
		}

		bspDrawVerts = newBspDrawVerts;
	}

	memset( bspDrawVerts + ( numBSPDrawVerts - 1 ), 0, sizeof( bspDrawVert_t ) );
}

void SetDrawVerts( int n ){
	free( bspDrawVerts );

	numBSPDrawVerts =
	numBSPDrawVertsBuffer = n;

	bspDrawVerts = safe_calloc_info( sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer, "IncDrawVerts" );
}

int numBSPDrawSurfacesBuffer = 0;
void SetDrawSurfacesBuffer(){
	free( bspDrawSurfaces );

	numBSPDrawSurfacesBuffer = MAX_MAP_DRAW_SURFS;

	bspDrawSurfaces = safe_calloc_info( sizeof( bspDrawSurface_t ) * numBSPDrawSurfacesBuffer, "IncDrawSurfaces" );
}

void SetDrawSurfaces( int n ){
	free( bspDrawSurfaces );

	numBSPDrawSurfaces =
	numBSPDrawSurfacesBuffer = n;

	bspDrawSurfaces = safe_calloc_info( sizeof( bspDrawSurface_t ) * numBSPDrawSurfacesBuffer, "IncDrawSurfaces" );
}

void BSPFilesCleanup(){
	free( bspDrawVerts );
	free( bspDrawSurfaces );
	free( bspLightBytes );
	free( bspGridPoints );
}






/*
   SwapBlock()
   if all values are 32 bits, this can be used to swap everything
 */

void SwapBlock( int *block, int size ){
	int i;


	/* dummy check */
	if ( block == NULL ) {
		return;
	}

	/* swap */
	size >>= 2;
	for ( i = 0; i < size; i++ )
		block[ i ] = LittleLong( block[ i ] );
}



/*
   SwapBSPFile()
   byte swaps all data in the abstract bsp
 */

void SwapBSPFile( void ){
	int i, j;
	shaderInfo_t    *si;

	/* models */
	SwapBlock( (int*) bspModels, numBSPModels * sizeof( bspModels[ 0 ] ) );

	/* shaders (don't swap the name) */
	for ( i = 0; i < numBSPShaders ; i++ )
	{
		if ( doingBSP ){
			si = ShaderInfoForShader( bspShaders[ i ].shader );
			if ( !strEmptyOrNull( si->remapShader ) ) {
				// copy and clear the rest of memory // check for overflow by String64
				const auto remap = String64()( si->remapShader );
				strncpy( bspShaders[ i ].shader, remap, sizeof( bspShaders[ i ].shader ) );
			}
		}
		bspShaders[ i ].contentFlags = LittleLong( bspShaders[ i ].contentFlags );
		bspShaders[ i ].surfaceFlags = LittleLong( bspShaders[ i ].surfaceFlags );
	}

	/* planes */
	SwapBlock( (int*) bspPlanes, numBSPPlanes * sizeof( bspPlanes[ 0 ] ) );

	/* nodes */
	SwapBlock( (int*) bspNodes, numBSPNodes * sizeof( bspNodes[ 0 ] ) );

	/* leafs */
	SwapBlock( (int*) bspLeafs, numBSPLeafs * sizeof( bspLeafs[ 0 ] ) );

	/* leaffaces */
	SwapBlock( (int*) bspLeafSurfaces, numBSPLeafSurfaces * sizeof( bspLeafSurfaces[ 0 ] ) );

	/* leafbrushes */
	SwapBlock( (int*) bspLeafBrushes, numBSPLeafBrushes * sizeof( bspLeafBrushes[ 0 ] ) );

	// brushes
	SwapBlock( (int*) bspBrushes, numBSPBrushes * sizeof( bspBrushes[ 0 ] ) );

	// brushsides
	SwapBlock( (int*) bspBrushSides, numBSPBrushSides * sizeof( bspBrushSides[ 0 ] ) );

	// vis
	( (int*) &bspVisBytes )[ 0 ] = LittleLong( ( (int*) &bspVisBytes )[ 0 ] );
	( (int*) &bspVisBytes )[ 1 ] = LittleLong( ( (int*) &bspVisBytes )[ 1 ] );

	/* drawverts (don't swap colors) */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[ i ].xyz[ 0 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 0 ] );
		bspDrawVerts[ i ].xyz[ 1 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 1 ] );
		bspDrawVerts[ i ].xyz[ 2 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 2 ] );
		bspDrawVerts[ i ].normal[ 0 ] = LittleFloat( bspDrawVerts[ i ].normal[ 0 ] );
		bspDrawVerts[ i ].normal[ 1 ] = LittleFloat( bspDrawVerts[ i ].normal[ 1 ] );
		bspDrawVerts[ i ].normal[ 2 ] = LittleFloat( bspDrawVerts[ i ].normal[ 2 ] );
		bspDrawVerts[ i ].st[ 0 ] = LittleFloat( bspDrawVerts[ i ].st[ 0 ] );
		bspDrawVerts[ i ].st[ 1 ] = LittleFloat( bspDrawVerts[ i ].st[ 1 ] );
		for ( j = 0; j < MAX_LIGHTMAPS; j++ )
		{
			bspDrawVerts[ i ].lightmap[ j ][ 0 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 0 ] );
			bspDrawVerts[ i ].lightmap[ j ][ 1 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 1 ] );
		}
	}

	/* drawindexes */
	SwapBlock( (int*) bspDrawIndexes, numBSPDrawIndexes * sizeof( bspDrawIndexes[0] ) );

	/* drawsurfs */
	/* note: rbsp files (and hence q3map2 abstract bsp) have byte lightstyles index arrays, this follows sof2map convention */
	SwapBlock( (int*) bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ) );

	/* fogs */
	for ( i = 0; i < numBSPFogs; i++ )
	{
		bspFogs[ i ].brushNum = LittleLong( bspFogs[ i ].brushNum );
		bspFogs[ i ].visibleSide = LittleLong( bspFogs[ i ].visibleSide );
	}

	/* advertisements */
	for ( i = 0; i < numBSPAds; i++ )
	{
		bspAds[ i ].cellId = LittleLong( bspAds[ i ].cellId );
		bspAds[ i ].normal[ 0 ] = LittleFloat( bspAds[ i ].normal[ 0 ] );
		bspAds[ i ].normal[ 1 ] = LittleFloat( bspAds[ i ].normal[ 1 ] );
		bspAds[ i ].normal[ 2 ] = LittleFloat( bspAds[ i ].normal[ 2 ] );

		for ( j = 0; j < 4; j++ )
		{
			bspAds[ i ].rect[j][ 0 ] = LittleFloat( bspAds[ i ].rect[j][ 0 ] );
			bspAds[ i ].rect[j][ 1 ] = LittleFloat( bspAds[ i ].rect[j][ 1 ] );
			bspAds[ i ].rect[j][ 2 ] = LittleFloat( bspAds[ i ].rect[j][ 2 ] );
		}

		//bspAds[ i ].model[ MAX_QPATH ];
	}
}

/*
   GetLumpElements()
   gets the number of elements in a bsp lump
 */

int GetLumpElements( bspHeader_t *header, int lump, int size ){
	/* check for odd size */
	if ( header->lumps[ lump ].length % size ) {
		if ( force ) {
			Sys_Warning( "GetLumpElements: odd lump size (%d) in lump %d\n", header->lumps[ lump ].length, lump );
			return 0;
		}
		else{
			Error( "GetLumpElements: odd lump size (%d) in lump %d", header->lumps[ lump ].length, lump );
		}
	}

	/* return element count */
	return header->lumps[ lump ].length / size;
}



/*
   GetLump()
   returns a pointer to the specified lump
 */

void_ptr GetLump( bspHeader_t *header, int lump ){
	return (void*)( (byte*) header + header->lumps[ lump ].offset );
}



/*
   CopyLump()
   copies a bsp file lump into a destination buffer
 */

int CopyLump( bspHeader_t *header, int lump, void *dest, int size ){
	int length, offset;


	/* get lump length and offset */
	length = header->lumps[ lump ].length;
	offset = header->lumps[ lump ].offset;

	/* handle erroneous cases */
	if ( length == 0 ) {
		return 0;
	}
	if ( length % size ) {
		if ( force ) {
			Sys_Warning( "CopyLump: odd lump size (%d) in lump %d\n", length, lump );
			return 0;
		}
		else{
			Error( "CopyLump: odd lump size (%d) in lump %d", length, lump );
		}
	}

	/* copy block of memory and return */
	memcpy( dest, (byte*) header + offset, length );
	return length / size;
}

int CopyLump_Allocate( bspHeader_t *header, int lump, void **dest, int size, int *allocationVariable ){
	/* get lump length and offset */
	*allocationVariable = header->lumps[ lump ].length / size;
	*dest = realloc( *dest, size * *allocationVariable );
	return CopyLump( header, lump, *dest, size );
}


/*
   AddLump()
   adds a lump to an outgoing bsp file
 */

void AddLump( FILE *file, bspHeader_t *header, int lumpNum, const void *data, int length ){
	bspLump_t   *lump;

	/* add lump to bsp file header */
	lump = &header->lumps[ lumpNum ];
	lump->offset = LittleLong( ftell( file ) );
	lump->length = LittleLong( length );

	/* write lump to file */
	SafeWrite( file, data, length );

	/* write padding zeros */
	SafeWrite( file, (const byte[3]){ 0, 0, 0 }, ( ( length + 3 ) & ~3 ) - length );
}



/*
   LoadBSPFile()
   loads a bsp file into memory
 */

void LoadBSPFile( const char *filename ){
	/* dummy check */
	if ( game == NULL || game->load == NULL ) {
		Error( "LoadBSPFile: unsupported BSP file format" );
	}

	/* load it, then byte swap the in-memory version */
	game->load( filename );
	SwapBSPFile();
}

/*
   PartialLoadBSPFile()
   partially loads a bsp file into memory
   for autopacker
 */

void PartialLoadBSPFile( const char *filename ){
	/* dummy check */
	if ( game == NULL || game->load == NULL ) {
		Error( "LoadBSPFile: unsupported BSP file format" );
	}

	/* load it, then byte swap the in-memory version */
	//game->load( filename );
	PartialLoadIBSPFile( filename );

	/* PartialSwapBSPFile() */
	int i;

	/* shaders (don't swap the name) */
	for ( i = 0; i < numBSPShaders ; i++ )
	{
		bspShaders[ i ].contentFlags = LittleLong( bspShaders[ i ].contentFlags );
		bspShaders[ i ].surfaceFlags = LittleLong( bspShaders[ i ].surfaceFlags );
	}

	/* drawsurfs */
	/* note: rbsp files (and hence q3map2 abstract bsp) have byte lightstyles index arrays, this follows sof2map convention */
	SwapBlock( (int*) bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ) );

	/* fogs */
	for ( i = 0; i < numBSPFogs; i++ )
	{
		bspFogs[ i ].brushNum = LittleLong( bspFogs[ i ].brushNum );
		bspFogs[ i ].visibleSide = LittleLong( bspFogs[ i ].visibleSide );
	}
}

/*
   WriteBSPFile()
   writes a bsp file
 */

void WriteBSPFile( const char *filename ){
	char tempname[ 1024 ];
	time_t tm;


	/* dummy check */
	if ( game == NULL || game->write == NULL ) {
		Error( "WriteBSPFile: unsupported BSP file format" );
	}

	/* make fake temp name so existing bsp file isn't damaged in case write process fails */
	time( &tm );
	sprintf( tempname, "%s.%08X", filename, (int) tm );

	/* byteswap, write the bsp, then swap back so it can be manipulated further */
	SwapBSPFile();
	game->write( tempname );
	SwapBSPFile();

	/* replace existing bsp file */
	remove( filename );
	rename( tempname, filename );
}



/*
   PrintBSPFileSizes()
   dumps info about current file
 */

void PrintBSPFileSizes( void ){
	/* parse entities first */
	if ( entities.empty() ) {
		ParseEntities();
	}
	int patchCount = 0;
	bspDrawSurface_t *s;
	for ( s = bspDrawSurfaces; s != bspDrawSurfaces + numBSPDrawSurfaces; ++s ){
		if ( s->surfaceType == MST_PATCH )
			++patchCount;
	}
	/* note that this is abstracted */
	Sys_Printf( "Abstracted BSP file components (*actual sizes may differ)\n" );

	/* print various and sundry bits */
	Sys_Printf( "%9d models        %9d\n",
	            numBSPModels, (int) ( numBSPModels * sizeof( bspModel_t ) ) );
	Sys_Printf( "%9d shaders       %9d\n",
	            numBSPShaders, (int) ( numBSPShaders * sizeof( bspShader_t ) ) );
	Sys_Printf( "%9d brushes       %9d\n",
	            numBSPBrushes, (int) ( numBSPBrushes * sizeof( bspBrush_t ) ) );
	Sys_Printf( "%9d brushsides    %9d *\n",
	            numBSPBrushSides, (int) ( numBSPBrushSides * sizeof( bspBrushSide_t ) ) );
	Sys_Printf( "%9d fogs          %9d\n",
	            numBSPFogs, (int) ( numBSPFogs * sizeof( bspFog_t ) ) );
	Sys_Printf( "%9d planes        %9d\n",
	            numBSPPlanes, (int) ( numBSPPlanes * sizeof( bspPlane_t ) ) );
	Sys_Printf( "%9zu entdata       %9d\n",
	            entities.size(), bspEntDataSize );
	Sys_Printf( "\n" );

	Sys_Printf( "%9d nodes         %9d\n",
	            numBSPNodes, (int) ( numBSPNodes * sizeof( bspNode_t ) ) );
	Sys_Printf( "%9d leafs         %9d\n",
	            numBSPLeafs, (int) ( numBSPLeafs * sizeof( bspLeaf_t ) ) );
	Sys_Printf( "%9d leafsurfaces  %9d\n",
	            numBSPLeafSurfaces, (int) ( numBSPLeafSurfaces * sizeof( *bspLeafSurfaces ) ) );
	Sys_Printf( "%9d leafbrushes   %9d\n",
	            numBSPLeafBrushes, (int) ( numBSPLeafBrushes * sizeof( *bspLeafBrushes ) ) );
	Sys_Printf( "\n" );

	Sys_Printf( "%9d drawsurfaces  %9d *\n",
	            numBSPDrawSurfaces, (int) ( numBSPDrawSurfaces * sizeof( *bspDrawSurfaces ) ) );
	Sys_Printf( "%9d patchsurfaces       \n",
	            patchCount );
	Sys_Printf( "%9d drawverts     %9d *\n",
	            numBSPDrawVerts, (int) ( numBSPDrawVerts * sizeof( *bspDrawVerts ) ) );
	Sys_Printf( "%9d drawindexes   %9d\n",
	            numBSPDrawIndexes, (int) ( numBSPDrawIndexes * sizeof( *bspDrawIndexes ) ) );
	Sys_Printf( "\n" );

	Sys_Printf( "%9d lightmaps     %9d\n",
	            numBSPLightBytes / ( game->lightmapSize * game->lightmapSize * 3 ), numBSPLightBytes );
	Sys_Printf( "%9d lightgrid     %9d *\n",
	            numBSPGridPoints, (int) ( numBSPGridPoints * sizeof( *bspGridPoints ) ) );
	Sys_Printf( "          visibility    %9d\n",
	            numBSPVisBytes );
}



/* -------------------------------------------------------------------------------

   entity data handling

   ------------------------------------------------------------------------------- */


/*
   StripTrailing()
   strips low byte chars off the end of a string
 */

StringRange StripTrailing( const char *string ){
	const char *end = string + strlen( string );
	while ( end != string && end[-1] <= 32 ){
		--end;
	}
	return StringRange( string, end );
}



/*
   ParseEpair()
   parses a single quoted "key" "value" pair into an epair struct
 */

void ParseEPair( std::list<epair_t>& epairs ){
	/* handle key */
	/* strip trailing spaces that sometimes get accidentally added in the editor */
	epair_t ep;
	ep.key = StripTrailing( token );

	/* handle value */
	GetToken( false );
	ep.value = StripTrailing( token );

	if( !ep.key.empty() && !ep.value.empty() )
		epairs.emplace_back( ep );
}



/*
   ParseEntity()
   parses an entity's epairs
 */

bool ParseEntity( void ){
	/* dummy check */
	if ( !GetToken( true ) ) {
		return false;
	}
	if ( !strEqual( token, "{" ) ) {
		Error( "ParseEntity: { not found" );
	}

	/* create new entity */
	entities.emplace_back();
	mapEnt = &entities.back();

	/* parse */
	while ( 1 )
	{
		if ( !GetToken( true ) ) {
			Error( "ParseEntity: EOF without closing brace" );
		}
		if ( strEqual( token, "}" ) ) {
			break;
		}
		ParseEPair( mapEnt->epairs );
	}

	/* return to sender */
	return true;
}



/*
   ParseEntities()
   parses the bsp entity data string into entities
 */

void ParseEntities( void ){
	entities.clear();
	ParseFromMemory( bspEntData, bspEntDataSize );
	while ( ParseEntity() ) ;

	/* ydnar: set number of bsp entities in case a map is loaded on top */
	numBSPEntities = entities.size();
}

/*
 * must be called before UnparseEntities
 */
void InjectCommandLine( char **argv, int beginArgs, int endArgs ){
	char newCommandLine[1024];
	const char *inpos;
	char *outpos = newCommandLine;
	char *sentinel = newCommandLine + sizeof( newCommandLine ) - 1;
	int i;

	if ( nocmdline ){
		return;
	}
	if ( entities[ 0 ].read_keyvalue( inpos, "_q3map2_cmdline" ) ) { // read previousCommandLine
		while ( outpos != sentinel && *inpos )
			*outpos++ = *inpos++;
		if ( outpos != sentinel ) {
			*outpos++ = ';';
		}
		if ( outpos != sentinel ) {
			*outpos++ = ' ';
		}
	}

	for ( i = beginArgs; i < endArgs; ++i )
	{
		if ( outpos != sentinel && i != beginArgs ) {
			*outpos++ = ' ';
		}
		inpos = argv[i];
		while ( outpos != sentinel && *inpos )
			if ( *inpos != '\\' && *inpos != '"' && *inpos != ';' && (unsigned char) *inpos >= ' ' ) {
				*outpos++ = *inpos++;
			}
	}

	*outpos = 0;
	entities[0].setKeyValue( "_q3map2_cmdline", newCommandLine );
	entities[0].setKeyValue( "_q3map2_version", Q3MAP_VERSION );
}

/*
   UnparseEntities()
   generates the entdata string from all the entities.
   this allows the utilities to add or remove key/value
   pairs to the data created by the map editor
 */

void UnparseEntities( void ){
	StringOutputStream data( 8192 );

	/* run through entity list */
	for ( std::size_t i = 0; i < numBSPEntities && i < entities.size(); i++ )
	{
		const entity_t& e = entities[ i ];
		/* get epair */
		if ( e.epairs.empty() ) {
			continue;   /* ent got removed */
		}
		/* ydnar: certain entities get stripped from bsp file */
		const char *classname = e.classname();
		if ( striEqual( classname, "misc_model" ) ||
		     striEqual( classname, "_decal" ) ||
		     striEqual( classname, "_skybox" ) ) {
			continue;
		}

		/* add beginning brace */
		data << "{\n";

		/* walk epair list */
		for ( const auto& ep : e.epairs )
		{
			/* copy and clean */
			data << '\"' << StripTrailing( ep.key.c_str() ) << "\" \"" << StripTrailing( ep.value.c_str() ) << "\"\n";
		}

		/* add trailing brace */
		data << "}\n";
	}

	/* save out */
	bspEntDataSize = data.end() - data.begin() + 1;
	AUTOEXPAND_BY_REALLOC( bspEntData, bspEntDataSize, allocatedBSPEntData, 1024 );
	strcpy( bspEntData, data );
}



/*
   PrintEntity()
   prints an entity's epairs to the console
 */

void PrintEntity( const entity_t *ent ){
	Sys_Printf( "------- entity %p -------\n", ent );
	for ( const auto& ep : ent->epairs )
		Sys_Printf( "%s = %s\n", ep.key.c_str(), ep.value.c_str() );
}



/*
   setKeyValue()
   sets an epair in an entity
 */

void entity_t::setKeyValue( const char *key, const char *value ){
	/* check for existing epair */
	for ( auto& ep : epairs )
	{
		if ( EPAIR_EQUAL( ep.key.c_str(), key ) ) {
			ep.value = value;
			return;
		}
	}

	/* create new epair */
	epairs.emplace_back( epair_t{ key, value } );
}


/*
   valueForKey()
   gets the value for an entity key
 */

const char *entity_t::valueForKey( const char *key ) const {
	/* walk epair list */
	for ( const auto& ep : epairs )
	{
		if ( EPAIR_EQUAL( ep.key.c_str(), key ) ) {
			return ep.value.c_str();
		}
	}

	/* if no match, return empty string */
	return "";
}

bool entity_t::read_keyvalue_( bool &bool_value, std::initializer_list<const char*>&& keys ) const {
	for( const char* key : keys ){
		const char* value = valueForKey( key );
		if( !strEmpty( value ) ){
			bool_value = ( value[0] == '1' );
			return true;
		}
	}
	return false;
}
bool entity_t::read_keyvalue_( int &int_value, std::initializer_list<const char*>&& keys ) const {
	for( const char* key : keys ){
		const char* value = valueForKey( key );
		if( !strEmpty( value ) ){
			int_value = atoi( value );
			return true;
		}
	}
	return false;
}
bool entity_t::read_keyvalue_( float &float_value, std::initializer_list<const char*>&& keys ) const {
	for( const char* key : keys ){
		const char* value = valueForKey( key );
		if( !strEmpty( value ) ){
			float_value = atof( value );
			return true;
		}
	}
	return false;
}
bool entity_t::read_keyvalue_( Vector3& vector3_value, std::initializer_list<const char*>&& keys ) const {
	for( const char* key : keys ){
		const char* value = valueForKey( key );
		if( !strEmpty( value ) ){
			float v0, v1, v2;
			if( 3 == sscanf( value, "%f %f %f", &v0, &v1, &v2 ) ){
				vector3_value[0] = v0;
				vector3_value[1] = v1;
				vector3_value[2] = v2;
				return true;
			}
		}
	}
	return false;
}
bool entity_t::read_keyvalue_( char (&string_value)[1024], std::initializer_list<const char*>&& keys ) const {
	for( const char* key : keys ){
		const char* value = valueForKey( key );
		if( !strEmpty( value ) ){
			strcpy( string_value, value );
			return true;
		}
	}
	return false;
}
bool entity_t::read_keyvalue_( const char *&string_ptr_value, std::initializer_list<const char*>&& keys ) const {
	for( const char* key : keys ){
		const char* value = valueForKey( key );
		if( !strEmpty( value ) ){
			string_ptr_value = value;
			return true;
		}
	}
	return false;
}


/*
   FindTargetEntity()
   finds an entity target
 */

entity_t *FindTargetEntity( const char *target ){
	/* walk entity list */
	for ( auto& e : entities )
	{
		if ( strEqual( e.valueForKey( "targetname" ), target ) ) {
			return &e;
		}
	}

	/* nada */
	return NULL;
}



/*
   GetEntityShadowFlags() - ydnar
   gets an entity's shadow flags
   note: does not set them to defaults if the keys are not found!
 */

void GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, int *castShadows, int *recvShadows ){
	/* get cast shadows */
	if ( castShadows != NULL ) {
		( ent != NULL && ent->read_keyvalue( *castShadows, "_castShadows", "_cs" ) ) ||
		( ent2 != NULL && ent2->read_keyvalue( *castShadows, "_castShadows", "_cs" ) );
	}

	/* receive */
	if ( recvShadows != NULL ) {
		( ent != NULL && ent->read_keyvalue( *recvShadows, "_receiveShadows", "_rs" ) ) ||
		( ent2 != NULL && ent2->read_keyvalue( *recvShadows, "_receiveShadows", "_rs" ) );
	}

	/* vortex: game-specific default entity keys */
	if ( striEqual( game->magic, "dq" ) || striEqual( game->magic, "prophecy" ) ) {
		/* vortex: deluxe quake default shadow flags */
		if ( ent->classname_is( "func_wall" ) ) {
			if ( recvShadows != NULL ) {
				*recvShadows = 1;
			}
			if ( castShadows != NULL ) {
				*castShadows = 1;
			}
		}
	}
}

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
#include "bspfile_ibsp.h"
#include <ctime>




/* -------------------------------------------------------------------------------

   this file was copied out of the common directory in order to not break
   compatibility with the q3map 1.x tree. it was moved out in order to support
   the raven bsp format (RBSP) used in soldier of fortune 2 and jedi knight 2.

   since each game has its own set of particular features, the data structures
   below no longer directly correspond to the binary format of a particular game.

   the translation will be done at bsp load/save time to keep any sort of
   special-case code messiness out of the rest of the program.

   ------------------------------------------------------------------------------- */



/*
   SwapBlock()
   if all values are 32 bits, this can be used to swap everything
 */

void SwapBlock( int *block, int size ){
	/* dummy check */
	if ( block == NULL ) {
		return;
	}

	/* swap */
	size >>= 2;
	for ( int i = 0; i < size; ++i )
		block[ i ] = LittleLong( block[ i ] );
}

/*
   SwapBlock()
   if all values are 32 bits, this can be used to swap everything
 */
template<typename T>
void SwapBlock( std::vector<T>& block ){
	const size_t size = ( sizeof( T ) * block.size() ) >> 2; // get size in integers
	/* swap */
	int *intptr = reinterpret_cast<int *>( block.data() );
	for ( size_t i = 0; i < size; ++i )
		intptr[ i ] = LittleLong( intptr[ i ] );
}



/*
   SwapBSPFile()
   byte swaps all data in the abstract bsp
 */

static void SwapBSPFile(){
	/* models */
	SwapBlock( bspModels );

	/* shaders (don't swap the name) */
	for ( bspShader_t& shader : bspShaders )
	{
		if ( doingBSP ){
			const shaderInfo_t *si = ShaderInfoForShader( shader.shader );
			if ( !strEmptyOrNull( si->remapShader ) ) {
				// copy and clear the rest of memory // check for overflow by String64
				const String64 remap( si->remapShader );
				strncpy( shader.shader, remap, sizeof( shader.shader ) );
			}
		}
		shader.contentFlags = LittleLong( shader.contentFlags );
		shader.surfaceFlags = LittleLong( shader.surfaceFlags );
	}

	/* planes */
	SwapBlock( bspPlanes );

	/* nodes */
	SwapBlock( bspNodes );

	/* leafs */
	SwapBlock( bspLeafs );

	/* leaffaces */
	SwapBlock( bspLeafSurfaces );

	/* leafbrushes */
	SwapBlock( bspLeafBrushes );

	// brushes
	SwapBlock( bspBrushes );

	// brushsides
	SwapBlock( bspBrushSides );

	// vis
	if( !bspVisBytes.empty() ){
		( (int*) bspVisBytes.data() )[ 0 ] = LittleLong( ( (int*) bspVisBytes.data() )[ 0 ] );
		( (int*) bspVisBytes.data() )[ 1 ] = LittleLong( ( (int*) bspVisBytes.data() )[ 1 ] );
	}

	/* drawverts (don't swap colors) */
	for ( bspDrawVert_t& v : bspDrawVerts )
	{
		v.xyz[ 0 ] = LittleFloat( v.xyz[ 0 ] );
		v.xyz[ 1 ] = LittleFloat( v.xyz[ 1 ] );
		v.xyz[ 2 ] = LittleFloat( v.xyz[ 2 ] );
		v.normal[ 0 ] = LittleFloat( v.normal[ 0 ] );
		v.normal[ 1 ] = LittleFloat( v.normal[ 1 ] );
		v.normal[ 2 ] = LittleFloat( v.normal[ 2 ] );
		v.st[ 0 ] = LittleFloat( v.st[ 0 ] );
		v.st[ 1 ] = LittleFloat( v.st[ 1 ] );
		for ( Vector2& lm : v.lightmap )
		{
			lm[ 0 ] = LittleFloat( lm[ 0 ] );
			lm[ 1 ] = LittleFloat( lm[ 1 ] );
		}
	}

	/* drawindexes */
	SwapBlock( bspDrawIndexes );

	/* drawsurfs */
	/* note: rbsp files (and hence q3map2 abstract bsp) have byte lightstyles index arrays, this follows sof2map convention */
	SwapBlock( bspDrawSurfaces );

	/* fogs */
	for ( bspFog_t& fog : bspFogs )
	{
		fog.brushNum = LittleLong( fog.brushNum );
		fog.visibleSide = LittleLong( fog.visibleSide );
	}

	/* advertisements */
	for ( bspAdvertisement_t& ad : bspAds )
	{
		ad.cellId = LittleLong( ad.cellId );
		ad.normal[ 0 ] = LittleFloat( ad.normal[ 0 ] );
		ad.normal[ 1 ] = LittleFloat( ad.normal[ 1 ] );
		ad.normal[ 2 ] = LittleFloat( ad.normal[ 2 ] );

		for ( Vector3& v : ad.rect )
		{
			v[ 0 ] = LittleFloat( v[ 0 ] );
			v[ 1 ] = LittleFloat( v[ 1 ] );
			v[ 2 ] = LittleFloat( v[ 2 ] );
		}
	}
}


/*
   LoadBSPFile()
   loads a bsp file into memory
 */

void LoadBSPFile( const char *filename ){
	/* dummy check */
	if ( g_game == NULL || g_game->load == NULL ) {
		Error( "LoadBSPFile: unsupported BSP file format" );
	}

	/* load it, then byte swap the in-memory version */
	g_game->load( filename );
	SwapBSPFile();
}

/*
   LoadBSPFilePartially()
   loads bsp file parts meaningful for autopacker
 */

void LoadBSPFilePartially( const char *filename ){
	/* dummy check */
	if ( g_game == NULL || g_game->load == NULL ) {
		Error( "LoadBSPFile: unsupported BSP file format" );
	}

	/* load it, then byte swap the in-memory version */
	//g_game->load( filename );
	LoadIBSPorRBSPFilePartially( filename );
	SwapBSPFile();
}

/*
   WriteBSPFile()
   writes a bsp file
 */

void WriteBSPFile( const char *filename ){
	char tempname[ 1024 ];
	time_t tm;

	Sys_Printf( "Writing %s\n", filename );

	/* dummy check */
	if ( g_game == NULL || g_game->write == NULL ) {
		Error( "WriteBSPFile: unsupported BSP file format" );
	}

	/* make fake temp name so existing bsp file isn't damaged in case write process fails */
	time( &tm );
	sprintf( tempname, "%s.%08X", filename, (int) tm );

	/* byteswap, write the bsp, then swap back so it can be manipulated further */
	SwapBSPFile();
	g_game->write( tempname );
	SwapBSPFile();

	/* replace existing bsp file */
	remove( filename );
	rename( tempname, filename );
}



/*
   PrintBSPFileSizes()
   dumps info about current file
 */

void PrintBSPFileSizes(){
	/* parse entities first */
	if ( entities.empty() ) {
		ParseEntities();
	}
	int patchCount = 0, planarCount = 0, trisoupCount = 0, flareCount = 0;
	for ( const bspDrawSurface_t& s : bspDrawSurfaces ){
		if ( s.surfaceType == MST_PATCH )
			++patchCount;
		else if ( s.surfaceType == MST_PLANAR )
			++planarCount;
		else if ( s.surfaceType == MST_TRIANGLE_SOUP )
			++trisoupCount;
		else if ( s.surfaceType == MST_FLARE )
			++flareCount;
	}
	/* note that this is abstracted */
	Sys_Printf( "Abstracted BSP file components (*actual sizes may differ)\n" );

	/* print various and sundry bits */
	Sys_Printf( "%9zu models        %9zu\n",
	            bspModels.size(), bspModels.size() * sizeof( bspModels[0] ) );
	Sys_Printf( "%9zu shaders       %9zu\n",
	            bspShaders.size(), bspShaders.size() * sizeof( bspShaders[0] ) );
	Sys_Printf( "%9zu brushes       %9zu\n",
	            bspBrushes.size(), bspBrushes.size() * sizeof( bspBrushes[0] ) );
	Sys_Printf( "%9zu brushsides    %9zu *\n",
	            bspBrushSides.size(), bspBrushSides.size() * sizeof( bspBrushSides[0] ) );
	Sys_Printf( "%9zu fogs          %9zu\n",
	            bspFogs.size(), bspFogs.size() * sizeof( bspFogs[0] ) );
	Sys_Printf( "%9zu planes        %9zu\n",
	            bspPlanes.size(), bspPlanes.size() * sizeof( bspPlanes[0] ) );
	Sys_Printf( "%9zu entdata       %9zu\n",
	            entities.size(), bspEntData.size() );
	Sys_Printf( "\n" );

	Sys_Printf( "%9zu nodes         %9zu\n",
	            bspNodes.size(), bspNodes.size() * sizeof( bspNodes[0] ) );
	Sys_Printf( "%9zu leafs         %9zu\n",
	            bspLeafs.size(), bspLeafs.size() * sizeof( bspLeafs[0] ) );
	Sys_Printf( "%9zu leafsurfaces  %9zu\n",
	            bspLeafSurfaces.size(), bspLeafSurfaces.size() * sizeof( bspLeafSurfaces[0] ) );
	Sys_Printf( "%9zu leafbrushes   %9zu\n",
	            bspLeafBrushes.size(), bspLeafBrushes.size() * sizeof( bspLeafBrushes[0] ) );
	Sys_Printf( "\n" );

	Sys_Printf( "%9zu drawsurfaces  %9zu *\n",
	            bspDrawSurfaces.size(), bspDrawSurfaces.size() * sizeof( bspDrawSurfaces[0] ) );
	Sys_Printf( "%9d   patch surfaces\n",
	            patchCount );
	Sys_Printf( "%9d   planar surfaces\n",
	            planarCount );
	Sys_Printf( "%9d   trisoup surfaces\n",
	            trisoupCount );
	if( flareCount != 0 )
		Sys_Printf( "%9d   flare surfaces\n",
	            flareCount );
	Sys_Printf( "%9zu drawverts     %9zu *\n",
	            bspDrawVerts.size(), bspDrawVerts.size() * sizeof( bspDrawVerts[0] ) );
	Sys_Printf( "%9zu drawindexes   %9zu\n",
	            bspDrawIndexes.size(), bspDrawIndexes.size() * sizeof( bspDrawIndexes[0] ) );
	Sys_Printf( "\n" );

	Sys_Printf( "%9zu lightmaps     %9zu\n",
	            bspLightBytes.size() / ( g_game->lightmapSize * g_game->lightmapSize * 3 ), bspLightBytes.size() );
	Sys_Printf( "%9zu lightgrid     %9zu *\n",
	            bspGridPoints.size(), bspGridPoints.size() * sizeof( bspGridPoints[0] ) );
	Sys_Printf( "          visibility    %9zu\n",
	            bspVisBytes.size() );
}



/* -------------------------------------------------------------------------------

   entity data handling

   ------------------------------------------------------------------------------- */


/*
   StripTrailing()
   strips low byte chars off the end of a string
 */

inline StringRange StripTrailing( const char *string ){
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
		epairs.push_back( std::move( ep ) );
}



/*
   ParseEntity()
   parses an entity's epairs
 */

static bool ParseEntity(){
	/* dummy check */
	if ( !GetToken( true ) ) {
		return false;
	}
	if ( !strEqual( token, "{" ) ) {
		Error( "ParseEntity: { not found" );
	}

	/* create new entity */
	entity_t& e = entities.emplace_back();

	/* parse */
	while ( 1 )
	{
		if ( !GetToken( true ) ) {
			Error( "ParseEntity: EOF without closing brace" );
		}
		if ( strEqual( token, "}" ) ) {
			break;
		}
		ParseEPair( e.epairs );
	}

	/* return to sender */
	return true;
}



/*
   ParseEntities()
   parses the bsp entity data string into entities
 */

void ParseEntities(){
	entities.clear();
	ParseFromMemory( bspEntData.data(), bspEntData.size() );
	while ( ParseEntity() ){};

	/* ydnar: set number of bsp entities in case a map is loaded on top */
	numBSPEntities = entities.size();
}

/*
 * must be called before UnparseEntities
 */
void InjectCommandLine( const char *stage, const std::vector<const char *>& args ){
	auto str = StringStream( entities[ 0 ].valueForKey( "_q3map2_cmdline" ) ); // read previousCommandLine
	if( !str.empty() )
		str << "; ";

	str << stage;

	for ( const char *c : args ) {
		str << ' ';
		for( ; !strEmpty( c ); ++c )
			if ( *c != '\\' && *c != '"' && *c != ';' && (unsigned char) *c >= ' ' )
				str << *c;
	}

	entities[0].setKeyValue( "_q3map2_cmdline", str );
	entities[0].setKeyValue( "_q3map2_version", Q3MAP_VERSION );
}

/*
   UnparseEntities()
   generates the entdata string from all the entities.
   this allows the utilities to add or remove key/value
   pairs to the data created by the map editor
 */

void UnparseEntities(){
	StringOutputStream data( 8192 );

	/* -keepmodels option: force misc_models to be kept and ignore what the map file says */
	if ( keepModels )
		entities[0].setKeyValue( "_keepModels", "1" ); // -keepmodels is -bsp option; save key in worldspawn to pass it to the next stages

	/* determine if we keep misc_models in the bsp */
	entities[ 0 ].read_keyvalue( keepModels, "_keepModels" );

	/* run through entity list */
	for ( std::size_t i = 0; i < numBSPEntities && i < entities.size(); ++i )
	{
		const entity_t& e = entities[ i ];
		/* get epair */
		if ( e.epairs.empty() ) {
			continue;   /* ent got removed */
		}
		/* ydnar: certain entities get stripped from bsp file */
		const char *classname = e.classname();
		if ( ( striEqual( classname, "misc_model" ) && !keepModels ) ||
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
	bspEntData = { data.cbegin(), data.cend() + 1 }; // include '\0'
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
	if ( striEqual( g_game->magic, "dq" ) || striEqual( g_game->magic, "prophecy" ) ) {
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

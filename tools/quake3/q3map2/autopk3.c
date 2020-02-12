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

   -------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */


#include "q3map2.h"
#include "autopk3.h"


typedef struct StrList_s
{
	int n;
	int max;
	char s[][MAX_QPATH];
}
StrList;

static inline StrList* StrList_allocate( size_t strNum ){
	StrList* ret = safe_calloc( offsetof( StrList, s[strNum] ) );
	memcpy( ret,
			&( StrList const ){ .n = 0, .max = strNum },
			sizeof( StrList ) );
	return ret;
}

static inline void StrList_append( StrList* list, const char* string ){
	if( list->n == list->max )
		Error( "StrList overflow" );

	const size_t size = sizeof( list->s[0] );
	if( strcpyQ( list->s[list->n], string, size ) >= size )
		Sys_FPrintf( SYS_WRN, "WARNING6: %s : string too long.\n", string ); // ( strlen( string ) > MAX_QPATH - 1 )

	list->n++;
}
/* returns index + 1 for boolean use */
static inline int StrList_find( const StrList* list, const char* string ){
	for ( int i = 0; i < list->n; ++i ){
		if ( striEqual( list->s[i], string ) )
			return i + 1;
	}
	return 0;
}

void pushStringCallback( StrList* list, const char* string ){
	if( !StrList_find( list, string ) )
		StrList_append( list, string );
}


/*
	Check if newcoming texture is unique and not excluded
*/
static inline void tex2list( StrList* texlist, StrList* EXtex, StrList* rEXtex ){
	if ( strEmpty( token ) )
		return;
	//StripExtension( token );
	char* dot = path_get_filename_base_end( token );
	if( striEqual( dot, ".tga" ) || striEqual( dot, ".jpg" ) || striEqual( dot, ".png" ) ){ //? might want to also warn on png in non png run
		strClear( dot );
	}
	else{
		Sys_FPrintf( SYS_WRN, "WARNING4: %s : weird or missing extension in shader image path\n", token );
	}

	FixDOSName( token );

	/* exclude */
	if( !StrList_find( texlist, token ) &&
		!StrList_find( EXtex, token ) &&
		( rEXtex == NULL ||
		!StrList_find( rEXtex, token ) ) ){
		StrList_append( texlist, token );
	}
	strcat( token, ".tga" ); // default extension for repacked shader text
}


/*
	Check if newcoming res is unique
*/
static inline void res2list( StrList* list, const char* res ){
	while ( path_separator( *res ) ){ // kill prepended slashes
		++res;
	}
	if ( strEmpty( res ) )
		return;
	if( !StrList_find( list, res ) )
		StrList_append( list, res );
}

static inline void parseEXblock( StrList* list, const char *exName ){
	if ( !GetToken( true ) || !strEqual( token, "{" ) ) {
		Error( "ReadExclusionsFile: %s, line %d: { not found", exName, scriptline );
	}
	while ( 1 )
	{
		if ( !GetToken( true ) ) {
			break;
		}
		if ( strEqual( token, "}" ) ) {
			break;
		}
		if ( strEqual( token, "{" ) ) {
			Error( "ReadExclusionsFile: %s, line %d: brace, opening twice in a row.", exName, scriptline );
		}

		/* add to list */
		StrList_append( list, token );
	}
	return;
}

static void parseEXfile( const char* filename, StrList* ExTextures, StrList* ExShaders, StrList* ExShaderfiles, StrList* ExSounds, StrList* ExVideos ){
	char exName[ 1024 ];
	ExtractFilePath( g_q3map2path, exName );
	strcat( exName, filename );
	Sys_Printf( "Loading %s\n", exName );

	byte *buffer;
	const int size = TryLoadFile( exName, (void**) &buffer );
	if ( size < 0 ) {
		Sys_Warning( "Unable to load exclusions file %s.\n", exName );
	}
	else{
		/* parse the file */
		ParseFromMemory( (char *) buffer, size );

		/* tokenize it */
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( true ) ) {
				break;
			}

			/* blocks */
			if ( striEqual( token, "textures" ) ){
				parseEXblock( ExTextures, exName );
			}
			else if ( striEqual( token, "shaders" ) ){
				parseEXblock( ExShaders, exName );
			}
			else if ( striEqual( token, "shaderfiles" ) ){
				parseEXblock( ExShaderfiles, exName );
			}
			else if ( striEqual( token, "sounds" ) ){
				parseEXblock( ExSounds, exName );
			}
			else if ( striEqual( token, "videos" ) ){
				parseEXblock( ExVideos, exName );
			}
			else{
				Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
			}
		}

		/* free the buffer */
		free( buffer );
	}
}



typedef struct
{
	int strlen;
	int max;
	char s[];
}
StrBuf;

static inline StrBuf* StrBuf_allocate( size_t strLen ){
	StrBuf* ret = safe_calloc( offsetof( StrBuf, s[strLen] ) );
	memcpy( ret,
			&( StrBuf const ){ .strlen = 0, .max = strLen },
			sizeof( StrBuf ) );
	return ret;
}

static inline void StrBuf_cat( StrBuf* buf, const char* string ){
	buf->strlen += strcpyQ( buf->s + buf->strlen, string, buf->max - buf->strlen );
	if( buf->strlen >= buf->max )
		Error( "StrBuf overflow" );
}
static inline void StrBuf_cat2( StrBuf* buf, const char* string, const char* string2 ){
	StrBuf_cat( buf, string );
	StrBuf_cat( buf, string2 );
}
static inline void StrBuf_cpy( StrBuf* buf, const char* string ){
	buf->strlen = 0;
	StrBuf_cat( buf, string );
}



static bool packResource( const char* resname, const char* packname, const int compLevel ){
	const bool ret = vfsPackFile( resname, packname, compLevel );
	if ( ret )
		Sys_Printf( "++%s\n", resname );
	return ret;
}
static bool packTexture( const char* texname, const char* packname, const int compLevel, const bool png ){
	const char* extensions[4] = { ".png", ".tga", ".jpg", 0 };
	for ( const char** ext = extensions + !png; *ext; ++ext ){
		char str[MAX_QPATH * 2];
		sprintf( str, "%s%s", texname, *ext );
		if( packResource( str, packname, compLevel ) ){
			return true;
		}
	}
	return false;
}




char g_q3map2path[1024];

/*
   pk3BSPMain()
   map autopackager, works for Q3 type of shaders and ents
 */

int pk3BSPMain( int argc, char **argv ){
	int i, compLevel = 10;
	bool dbg = false, png = false, packFAIL = false;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); ++i ){
		if ( strEqual( argv[ i ],  "-dbg" ) ) {
			dbg = true;
		}
		else if ( strEqual( argv[ i ],  "-png" ) ) {
			png = true;
		}
		else if ( strEqual( argv[ i ],  "-complevel" ) ) {
			compLevel = atoi( argv[ i + 1 ] );
			i++;
			if ( compLevel < -1 ) compLevel = -1;
			if ( compLevel > 10 ) compLevel = 10;
			Sys_Printf( "Compression level set to %i\n", compLevel );
		}
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	path_set_extension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();


	char nameOFmap[ 1024 ], str[ 1024 ];

	/* extract map name */
	ExtractFileBase( source, nameOFmap );

	bool drawsurfSHs[numBSPShaders];
	memset( drawsurfSHs, 0, sizeof( drawsurfSHs ) );

	for ( i = 0; i < numBSPDrawSurfaces; ++i ){
		/* can't exclude nodraw patches here (they want shaders :0!) */
		//if ( !( bspDrawSurfaces[i].surfaceType == 2 && bspDrawSurfaces[i].numIndexes == 0 ) ) drawsurfSHs[bspDrawSurfaces[i].shaderNum] = true;
		drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = true;
		//Sys_Printf( "%s\n", bspShaders[bspDrawSurfaces[i].shaderNum].shader );
	}

	StrList* pk3Shaders = StrList_allocate( 1024 );
	StrList* pk3Sounds = StrList_allocate( 1024 );
	StrList* pk3Shaderfiles = StrList_allocate( 1024 );
	StrList* pk3Textures = StrList_allocate( 1024 );
	StrList* pk3Videos = StrList_allocate( 1024 );

	for ( i = 0; i < numBSPShaders; ++i ){
		if ( drawsurfSHs[i] ){
			res2list( pk3Shaders, bspShaders[i].shader );
			//Sys_Printf( "%s\n", bspShaders[i].shader );
		}
	}

	/* Ent keys */
	epair_t *ep;
	for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
	{
		if ( striEqualPrefix( ep->key, "vertexremapshader" ) ) {
			sscanf( ep->value, "%*[^;] %*[;] %s", str ); // textures/remap/from;textures/remap/to
			res2list( pk3Shaders, str );
		}
	}

	if ( ENT_READKV( &str, &entities[0], "music" ) ){
		FixDOSName( str );
		DefaultExtension( str, ".wav" );
		res2list( pk3Sounds, str );
	}

	for ( i = 0; i < numBSPEntities && i < numEntities; ++i )
	{
		if ( ENT_READKV( &str, &entities[i], "noise" ) && str[0] != '*' ){
			FixDOSName( str );
			DefaultExtension( str, ".wav" );
			res2list( pk3Sounds, str );
		}

		if ( ent_class_is( &entities[i], "func_plat" ) ){
			res2list( pk3Sounds, "sound/movers/plats/pt1_strt.wav" );
			res2list( pk3Sounds, "sound/movers/plats/pt1_end.wav" );
		}
		if ( ent_class_is( &entities[i], "target_push" ) ){
			if ( !( IntForKey( &entities[i], "spawnflags") & 1 ) ){
				res2list( pk3Sounds, "sound/misc/windfly.wav" );
			}
		}
		res2list( pk3Shaders, ValueForKey( &entities[i], "targetShaderNewName" ) );
	}

	//levelshot
	sprintf( str, "levelshots/%s", nameOFmap );
	res2list( pk3Shaders, str );


	if( dbg ){
		Sys_Printf( "\n\tDrawsurface+ent calls....%i\n", pk3Shaders->n );
		for ( i = 0; i < pk3Shaders->n; ++i ){
			Sys_Printf( "%s\n", pk3Shaders->s[i] );
		}
		Sys_Printf( "\n\tSounds....%i\n", pk3Sounds->n );
		for ( i = 0; i < pk3Sounds->n; ++i ){
			Sys_Printf( "%s\n", pk3Sounds->s[i] );
		}
	}

	vfsListShaderFiles( pk3Shaderfiles, pushStringCallback );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%i\n", pk3Shaderfiles->n );
		for ( i = 0; i < pk3Shaderfiles->n; ++i ){
			Sys_Printf( "%s\n", pk3Shaderfiles->s[i] );
		}
	}


	/* load exclusions file */
	StrList* ExTextures = StrList_allocate( 4096 );
	StrList* ExShaders = StrList_allocate( 4096 );
	StrList* ExShaderfiles = StrList_allocate( 4096 );
	StrList* ExSounds = StrList_allocate( 4096 );
	StrList* ExVideos = StrList_allocate( 4096 );
	StrList* ExPureTextures = StrList_allocate( 4096 );

	char* ExReasonShader[4096] = { NULL };
	char* ExReasonShaderFile[4096] = { NULL };

	{
		sprintf( str, "%s%s", game->arg, ".exclude" );
		parseEXfile( str, ExTextures, ExShaders, ExShaderfiles, ExSounds, ExVideos );

		for ( i = 0; i < ExTextures->n; ++i ){
			if( !StrList_find( ExShaders, ExTextures->s[i] ) )
				StrList_append( ExPureTextures, ExTextures->s[i] );
		}
	}

	if( dbg ){
		Sys_Printf( "\n\tExTextures....%i\n", ExTextures->n );
		for ( i = 0; i < ExTextures->n; ++i )
			Sys_Printf( "%s\n", ExTextures->s[i] );
		Sys_Printf( "\n\tExPureTextures....%i\n", ExPureTextures->n );
		for ( i = 0; i < ExPureTextures->n; ++i )
			Sys_Printf( "%s\n", ExPureTextures->s[i] );
		Sys_Printf( "\n\tExShaders....%i\n", ExShaders->n );
		for ( i = 0; i < ExShaders->n; ++i )
			Sys_Printf( "%s\n", ExShaders->s[i] );
		Sys_Printf( "\n\tExShaderfiles....%i\n", ExShaderfiles->n );
		for ( i = 0; i < ExShaderfiles->n; ++i )
			Sys_Printf( "%s\n", ExShaderfiles->s[i] );
		Sys_Printf( "\n\tExSounds....%i\n", ExSounds->n );
		for ( i = 0; i < ExSounds->n; ++i )
			Sys_Printf( "%s\n", ExSounds->s[i] );
		Sys_Printf( "\n\tExVideos....%i\n", ExVideos->n );
		for ( i = 0; i < ExVideos->n; ++i )
			Sys_Printf( "%s\n", ExVideos->s[i] );
	}

	/* can exclude pure textures right now, shouldn't create shaders for them anyway */
	for ( i = 0; i < pk3Shaders->n; ++i ){
		if( StrList_find( ExPureTextures, pk3Shaders->s[i] ) )
			strClear( pk3Shaders->s[i] );
	}

	//Parse Shader Files
	 /* hack */
	endofscript = true;

	for ( i = 0; i < pk3Shaderfiles->n; ++i ){
		bool wantShader = false, wantShaderFile = false, ShaderFileExcluded = false;
		int shader, found;
		char* reasonShader = NULL;
		char* reasonShaderFile = NULL;

		/* load the shader */
		char scriptFile[128];
		sprintf( scriptFile, "%s/%s", game->shaderPath, pk3Shaderfiles->s[i] );
		SilentLoadScriptFile( scriptFile, 0 );
		if( dbg )
			Sys_Printf( "\n\tentering %s\n", pk3Shaderfiles->s[i] );

		/* do wanna le shader file? */
		if( ( found = StrList_find( ExShaderfiles, pk3Shaderfiles->s[i] ) ) ){
			ShaderFileExcluded = true;
			reasonShaderFile = ExShaderfiles->s[found - 1];
		}
		/* tokenize it */
		/* check if shader file has to be excluded */
		while ( !ShaderFileExcluded )
		{
			/* test for end of file */
			if ( !GetToken( true ) ) {
				break;
			}

			/* does it contain restricted shaders/textures? */
			if( ( found = StrList_find( ExShaders, token ) ) ){
				ShaderFileExcluded = true;
				reasonShader = ExShaders->s[found - 1];
				break;
			}
			else if( ( found = StrList_find( ExPureTextures, token ) ) ){
				ShaderFileExcluded = true;
				reasonShader = ExPureTextures->s[found - 1];
				break;
			}

			/* handle { } section */
			if ( !GetToken( true ) ) {
				break;
			}
			if ( !strEqual( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						scriptFile, scriptline, token, g_strLoadedFileLocation );
			}

			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( true ) ) {
					break;
				}
				if ( strEqual( token, "}" ) ) {
					break;
				}
				/* parse stage directives */
				if ( strEqual( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( true ) ) {
							break;
						}
						if ( strEqual( token, "}" ) ) {
							break;
						}
					}
				}
			}
		}

		/* tokenize it again */
		SilentLoadScriptFile( scriptFile, 0 );
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( true ) ) {
				break;
			}
			//dump shader names
			if( dbg ) Sys_Printf( "%s\n", token );

			/* do wanna le shader? */
			wantShader = false;
			if( ( found = StrList_find( pk3Shaders, token ) ) ){
				shader = found - 1;
				wantShader = true;
			}

			/* handle { } section */
			if ( !GetToken( true ) ) {
				break;
			}
			if ( !strEqual( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						scriptFile, scriptline, token, g_strLoadedFileLocation );
			}

			bool hasmap = false;
			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( true ) ) {
					break;
				}
				if ( strEqual( token, "}" ) ) {
					break;
				}


				/* -----------------------------------------------------------------
				shader stages (passes)
				----------------------------------------------------------------- */

				/* parse stage directives */
				if ( strEqual( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( true ) ) {
							break;
						}
						if ( strEqual( token, "}" ) ) {
							break;
						}
						if ( strEqual( token, "{" ) ) {
							Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", scriptFile, scriptline );
						}
						if ( striEqual( token, "mapComp" ) || striEqual( token, "mapNoComp" ) || striEqual( token, "animmapcomp" ) || striEqual( token, "animmapnocomp" ) ){
							Sys_FPrintf( SYS_WRN, "WARNING7: %s : line %d : unsupported '%s' map directive\n", scriptFile, scriptline, token );
						}
						/* skip the shader */
						if ( !wantShader )
							continue;

						/* digest any images */
						if ( striEqual( token, "map" ) ||
							striEqual( token, "clampMap" ) ) {
							hasmap = true;
							/* get an image */
							GetToken( false );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list( pk3Textures, ExTextures, NULL );
							}
						}
						else if ( striEqual( token, "animMap" ) ||
							striEqual( token, "clampAnimMap" ) ) {
							hasmap = true;
							GetToken( false );// skip num
							while ( TokenAvailable() ){
								GetToken( false );
								tex2list( pk3Textures, ExTextures, NULL );
							}
						}
						else if ( striEqual( token, "videoMap" ) ){
							hasmap = true;
							GetToken( false );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL ){
								sprintf( str, "video/%s", token );
								strcpy( token, str );
							}
							if( !StrList_find( pk3Videos, token ) &&
								!StrList_find( ExVideos, token ) )
								StrList_append( pk3Videos, token );
						}
					}
				}
				else if ( striEqualPrefix( token, "implicit" ) ){
					Sys_FPrintf( SYS_WRN, "WARNING5: %s : line %d : unsupported %s shader\n", scriptFile, scriptline, token );
				}
				/* skip the shader */
				else if ( !wantShader )
					continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( striEqual( token, "surfaceparm" ) ) {
					GetToken( false );
					if ( striEqual( token, "nodraw" ) ) {
						wantShader = false;
						strClear( pk3Shaders->s[shader] );
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( striEqual( token, "skyParms" ) ) {
					hasmap = true;
					/* get image base */
					GetToken( false );

					/* ignore bogus paths */
					if ( !strEqual( token, "-" ) && !striEqual( token, "full" ) ) {
						const char skysides[6][3] = { "up", "dn", "lf", "rt", "bk", "ft" };
						char* const skysidestring = token + strcatQ( token, "_@@.tga", sizeof( token ) ) - 6;
						for( size_t side = 0; side < 6; ++side ){
							memcpy( skysidestring, skysides[side], 2 );
							tex2list( pk3Textures, ExTextures, NULL );
						}
					}
					/* skip rest of line */
					GetToken( false );
					GetToken( false );
				}
				else if ( striEqual( token, "fogparms" ) ){
					hasmap = true;
				}
			}

			//exclude shader
			if ( wantShader ){
				if( StrList_find( ExShaders, pk3Shaders->s[shader] ) ){
					wantShader = false;
					strClear( pk3Shaders->s[shader] );
				}
				if ( !hasmap ){
					wantShader = false;
				}
				if ( wantShader ){
					if ( ShaderFileExcluded ){
						if ( reasonShaderFile != NULL ){
							ExReasonShaderFile[ shader ] = reasonShaderFile;
						}
						else{
							ExReasonShaderFile[ shader ] = copystring( pk3Shaderfiles->s[i] );
						}
						ExReasonShader[ shader ] = reasonShader;
					}
					else{
						wantShaderFile = true;
						strClear( pk3Shaders->s[shader] );
					}
				}
			}
		}
		if ( !wantShaderFile ){
			strClear( pk3Shaderfiles->s[i] );
		}
	}



/* exclude stuff */
//wanted shaders from excluded .shaders
	Sys_Printf( "\n" );
	for ( i = 0; i < pk3Shaders->n; ++i ){
		if ( !strEmpty( pk3Shaders->s[i] ) && ( ExReasonShader[i] != NULL || ExReasonShaderFile[i] != NULL ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders->s[i] );
			packFAIL = true;
			if ( ExReasonShader[i] != NULL ){
				Sys_Printf( "     reason: is located in %s,\n     containing restricted shader %s\n", ExReasonShaderFile[i], ExReasonShader[i] );
			}
			else{
				Sys_Printf( "     reason: is located in restricted %s\n", ExReasonShaderFile[i] );
			}
			strClear( pk3Shaders->s[i] );
		}
	}
//pure textures (shader ones are done)
	for ( i = 0; i < pk3Shaders->n; ++i ){
		if ( !strEmpty( pk3Shaders->s[i] ) ){
			FixDOSName( pk3Shaders->s[i] );
			if( StrList_find( pk3Textures, pk3Shaders->s[i] ) ||
				StrList_find( ExTextures, pk3Shaders->s[i] ) )
				strClear( pk3Shaders->s[i] );
		}
	}

//snds
	for ( i = 0; i < pk3Sounds->n; ++i ){
		if( StrList_find( ExSounds, pk3Sounds->s[i] ) )
			strClear( pk3Sounds->s[i] );
	}

	/* make a pack */
	char packname[ 1024 ], packFailName[ 1024 ];
	sprintf( packname, "%s/%s_autopacked.pk3", EnginePath, nameOFmap );
	remove( packname );
	sprintf( packFailName, "%s/%s_FAILEDpack.pk3", EnginePath, nameOFmap );
	remove( packFailName );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3Textures->n; ++i ){
		if( !packTexture( pk3Textures->s[i], packname, compLevel, png ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Textures->s[i] );
			packFAIL = true;
		}
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3Shaders->n; ++i ){
		if ( !strEmpty( pk3Shaders->s[i] ) ){
			if( !packTexture( pk3Shaders->s[i], packname, compLevel, png ) ){
				if ( i == pk3Shaders->n - 1 ){ //levelshot typically
					Sys_Printf( "  ~fail  %s\n", pk3Shaders->s[i] );
				}
				else{
					Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders->s[i] );
					packFAIL = true;
				}
			}
		}
	}

	Sys_Printf( "\n\tShaizers....\n" );

	for ( i = 0; i < pk3Shaderfiles->n; ++i ){
		if ( !strEmpty( pk3Shaderfiles->s[i] ) ){
			sprintf( str, "%s/%s", game->shaderPath, pk3Shaderfiles->s[i] );
			if ( !packResource( str, packname, compLevel ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders->s[i] );
				packFAIL = true;
			}
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3Sounds->n; ++i ){
		if ( !strEmpty( pk3Sounds->s[i] ) ){
			if ( !packResource( pk3Sounds->s[i], packname, compLevel ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Sounds->s[i] );
				packFAIL = true;
			}
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3Videos->n; ++i ){
		if ( !packResource( pk3Videos->s[i], packname, compLevel ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Videos->s[i] );
			packFAIL = true;
		}
	}

	Sys_Printf( "\n\t.bsp and stuff\n" );

	sprintf( str, "maps/%s.bsp", nameOFmap );
	//if ( vfsPackFile( str, packname, compLevel ) ){
	if ( vfsPackFile_Absolute_Path( source, str, packname, compLevel ) ){
		Sys_Printf( "++%s\n", str );
	}
	else{
		Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", str );
		packFAIL = true;
	}

	sprintf( str, "maps/%s.aas", nameOFmap );
	if ( !packResource( str, packname, compLevel ) )
		Sys_Printf( "  ~fail  %s\n", str );

	sprintf( str, "scripts/%s.arena", nameOFmap );
	if ( !packResource( str, packname, compLevel ) )
		Sys_Printf( "  ~fail  %s\n", str );

	sprintf( str, "scripts/%s.defi", nameOFmap );
	if ( !packResource( str, packname, compLevel ) )
		Sys_Printf( "  ~fail  %s\n", str );

	if ( !packFAIL ){
		Sys_Printf( "\nSaved to %s\n", packname );
	}
	else{
		rename( packname, packFailName );
		Sys_Printf( "\nSaved to %s\n", packFailName );
	}
	/* return to sender */
	return 0;
}


/*
   repackBSPMain()
   repack multiple maps, strip out only required shaders
   works for Q3 type of shaders and ents
 */

int repackBSPMain( int argc, char **argv ){
	int i, j, compLevel = 0;
	bool dbg = false, png = false;
	char str[ 1024 ];

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); ++i ){
		if ( strEqual( argv[ i ],  "-dbg" ) ) {
			dbg = true;
		}
		else if ( strEqual( argv[ i ],  "-png" ) ) {
			png = true;
		}
		else if ( strEqual( argv[ i ],  "-complevel" ) ) {
			compLevel = atoi( argv[ i + 1 ] );
			i++;
			if ( compLevel < -1 ) compLevel = -1;
			if ( compLevel > 10 ) compLevel = 10;
			Sys_Printf( "Compression level set to %i\n", compLevel );
		}
	}

/* load exclusions file */
	StrList* ExTextures = StrList_allocate( 4096 );
	StrList* ExShaders = StrList_allocate( 4096 );
	StrList* ExShaderfiles = StrList_allocate( 4096 );
	StrList* ExSounds = StrList_allocate( 4096 );
	StrList* ExVideos = StrList_allocate( 4096 );
	StrList* ExPureTextures = StrList_allocate( 4096 );

	{
		sprintf( str, "%s%s", game->arg, ".exclude" );
		parseEXfile( str, ExTextures, ExShaders, ExShaderfiles, ExSounds, ExVideos );

		for ( i = 0; i < ExTextures->n; ++i ){
			if( !StrList_find( ExShaders, ExTextures->s[i] ) )
				StrList_append( ExPureTextures, ExTextures->s[i] );
		}
	}

	if( dbg ){
		Sys_Printf( "\n\tExTextures....%i\n", ExTextures->n );
		for ( i = 0; i < ExTextures->n; ++i )
			Sys_Printf( "%s\n", ExTextures->s[i] );
		Sys_Printf( "\n\tExPureTextures....%i\n", ExPureTextures->n );
		for ( i = 0; i < ExPureTextures->n; ++i )
			Sys_Printf( "%s\n", ExPureTextures->s[i] );
		Sys_Printf( "\n\tExShaders....%i\n", ExShaders->n );
		for ( i = 0; i < ExShaders->n; ++i )
			Sys_Printf( "%s\n", ExShaders->s[i] );
		Sys_Printf( "\n\tExShaderfiles....%i\n", ExShaderfiles->n );
		for ( i = 0; i < ExShaderfiles->n; ++i )
			Sys_Printf( "%s\n", ExShaderfiles->s[i] );
		Sys_Printf( "\n\tExSounds....%i\n", ExSounds->n );
		for ( i = 0; i < ExSounds->n; ++i )
			Sys_Printf( "%s\n", ExSounds->s[i] );
		Sys_Printf( "\n\tExVideos....%i\n", ExVideos->n );
		for ( i = 0; i < ExVideos->n; ++i )
			Sys_Printf( "%s\n", ExVideos->s[i] );
	}


/* load repack.exclude */
	StrList* rExTextures = StrList_allocate( 65536 );
	StrList* rExShaders = StrList_allocate( 32768 );
	StrList* rExShaderfiles = StrList_allocate( 4096 );
	StrList* rExSounds = StrList_allocate( 8192 );
	StrList* rExVideos = StrList_allocate( 4096 );

	parseEXfile( "repack.exclude", rExTextures, rExShaders, rExShaderfiles, rExSounds, rExVideos );

	if( dbg ){
		Sys_Printf( "\n\trExTextures....%i\n", rExTextures->n );
		for ( i = 0; i < rExTextures->n; ++i )
			Sys_Printf( "%s\n", rExTextures->s[i] );
		Sys_Printf( "\n\trExShaders....%i\n", rExShaders->n );
		for ( i = 0; i < rExShaders->n; ++i )
			Sys_Printf( "%s\n", rExShaders->s[i] );
		Sys_Printf( "\n\trExShaderfiles....%i\n", rExShaderfiles->n );
		for ( i = 0; i < rExShaderfiles->n; ++i )
			Sys_Printf( "%s\n", rExShaderfiles->s[i] );
		Sys_Printf( "\n\trExSounds....%i\n", rExSounds->n );
		for ( i = 0; i < rExSounds->n; ++i )
			Sys_Printf( "%s\n", rExSounds->s[i] );
		Sys_Printf( "\n\trExVideos....%i\n", rExVideos->n );
		for ( i = 0; i < rExVideos->n; ++i )
			Sys_Printf( "%s\n", rExVideos->s[i] );
	}



	int bspListN = 0;
	const int bspListSize = 8192;
	char (*bspList)[1024] = safe_malloc( bspListSize * sizeof( bspList[0] ) );

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	if ( striEqual( path_get_filename_base_end( source ), ".bsp" ) ){
		strcpy( bspList[bspListN], source );
		bspListN++;
	}
	else{
		/* load bsps paths list */
		Sys_Printf( "Loading %s\n", source );
		byte *buffer;
		const int size = TryLoadFile( source, (void**) &buffer );
		if ( size <= 0 ) {
			Error( "Unable to open bsps paths list file %s.\n", source );
		}

		/* parse the file */
		ParseFromMemory( (char *) buffer, size );

		/* tokenize it */
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( true ) ) {
				break;
			}
			if( bspListSize == bspListN )
				Error( "bspList overflow" );
			strcpy( bspList[bspListN], token );
			bspListN++;
		}

		/* free the buffer */
		free( buffer );
	}

	/* extract input file name */
	char nameOFrepack[ 1024 ];
	ExtractFileBase( source, nameOFrepack );

/* load bsps */
	StrList* pk3Shaders = StrList_allocate( 65536 );
	StrList* pk3Sounds = StrList_allocate( 4096 );
	StrList* pk3Shaderfiles = StrList_allocate( 4096 );
	StrList* pk3Textures = StrList_allocate( 65536 );
	StrList* pk3Videos = StrList_allocate( 1024 );


	for( j = 0; j < bspListN; ++j ){

		int pk3SoundsNold = pk3Sounds->n;
		int pk3ShadersNold = pk3Shaders->n;

		strcpy( source, bspList[j] );
		path_set_extension( source, ".bsp" );

		/* load the bsp */
		Sys_Printf( "\nLoading %s\n", source );
		PartialLoadBSPFile( source );
		ParseEntities();

		/* extract map name */
		char nameOFmap[ 1024 ];
		ExtractFileBase( source, nameOFmap );

		bool drawsurfSHs[numBSPShaders];
		memset( drawsurfSHs, 0, sizeof( drawsurfSHs ) );

		for ( i = 0; i < numBSPDrawSurfaces; ++i ){
			drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = true;
		}

		for ( i = 0; i < numBSPShaders; ++i ){
			if ( drawsurfSHs[i] ){
				res2list( pk3Shaders, bspShaders[i].shader );
			}
		}

		/* Ent keys */
		epair_t *ep;
		for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
		{
			if ( striEqualPrefix( ep->key, "vertexremapshader" ) ) {
				sscanf( ep->value, "%*[^;] %*[;] %s", str ); // textures/remap/from;textures/remap/to
				res2list( pk3Shaders, str );
			}
		}
		if ( ENT_READKV( &str, &entities[0], "music" ) ){
			FixDOSName( str );
			DefaultExtension( str, ".wav" );
			res2list( pk3Sounds, str );
		}

		for ( i = 0; i < numBSPEntities && i < numEntities; ++i )
		{
			if ( ENT_READKV( &str, &entities[i], "noise" ) && str[0] != '*' ){
				FixDOSName( str );
				DefaultExtension( str, ".wav" );
				res2list( pk3Sounds, str );
			}

			if ( ent_class_is( &entities[i], "func_plat" ) ){
				res2list( pk3Sounds, "sound/movers/plats/pt1_strt.wav" );
				res2list( pk3Sounds, "sound/movers/plats/pt1_end.wav" );
			}
			if ( ent_class_is( &entities[i], "target_push" ) ){
				if ( !( IntForKey( &entities[i], "spawnflags") & 1 ) ){
					res2list( pk3Sounds, "sound/misc/windfly.wav" );
				}
			}
			res2list( pk3Shaders, ValueForKey( &entities[i], "targetShaderNewName" ) );
		}

		//levelshot
		sprintf( str, "levelshots/%s", nameOFmap );
		res2list( pk3Shaders, str );



		Sys_Printf( "\n\t+Drawsurface+ent calls....%i\n", pk3Shaders->n - pk3ShadersNold );
		for ( i = pk3ShadersNold; i < pk3Shaders->n; ++i ){
			Sys_Printf( "%s\n", pk3Shaders->s[i] );
		}
		Sys_Printf( "\n\t+Sounds....%i\n", pk3Sounds->n - pk3SoundsNold );
		for ( i = pk3SoundsNold; i < pk3Sounds->n; ++i ){
			Sys_Printf( "%s\n", pk3Sounds->s[i] );
		}
		/* free bsp data */
/*
		if ( bspDrawVerts != 0 ) {
			free( bspDrawVerts );
			bspDrawVerts = NULL;
			//numBSPDrawVerts = 0;
			Sys_Printf( "freed BSPDrawVerts\n" );
		}
		if ( bspDrawSurfaces != 0 ) {
			Sys_Printf( "freed bspDrawSurfaces\n" );
		}
*/		free( bspDrawSurfaces );
		bspDrawSurfaces = NULL;
		numBSPDrawSurfaces = 0;
/*		if ( bspLightBytes != 0 ) {
			free( bspLightBytes );
			bspLightBytes = NULL;
			//numBSPLightBytes = 0;
			Sys_Printf( "freed BSPLightBytes\n" );
		}
		if ( bspGridPoints != 0 ) {
			free( bspGridPoints );
			bspGridPoints = NULL;
			//numBSPGridPoints = 0;
			Sys_Printf( "freed BSPGridPoints\n" );
		}
		if ( bspPlanes != 0 ) {
			free( bspPlanes );
			bspPlanes = NULL;
			Sys_Printf( "freed bspPlanes\n" );
			//numBSPPlanes = 0;
			//allocatedBSPPlanes = 0;
		}
		if ( bspBrushes != 0 ) {
			free( bspBrushes );
			bspBrushes = NULL;
			Sys_Printf( "freed bspBrushes\n" );
			//numBSPBrushes = 0;
			//allocatedBSPBrushes = 0;
		}
*/		if ( entities != 0 ) {
			for ( i = 0; i < numBSPEntities && i < numEntities; ++i ){
				ep = entities[i].epairs;
				while( ep != NULL){
					epair_t *ep2free = ep;
					ep = ep->next;
					free( ep2free );
				}
			}
			free( entities );
			entities = NULL;
			//Sys_Printf( "freed entities\n" );
			numEntities = 0;
			numBSPEntities = 0;
			allocatedEntities = 0;
		}
/*		if ( bspModels != 0 ) {
			free( bspModels );
			bspModels = NULL;
			Sys_Printf( "freed bspModels\n" );
			//numBSPModels = 0;
			//allocatedBSPModels = 0;
		}
		if ( bspShaders != 0 ) {
			Sys_Printf( "freed bspShaders\n" );
		}
*/		free( bspShaders );
		bspShaders = NULL;
		numBSPShaders = 0;
		allocatedBSPShaders = 0;
/*		if ( bspEntData != 0 ) {
			Sys_Printf( "freed bspEntData\n" );
		}
*/		free( bspEntData );
		bspEntData = NULL;
		bspEntDataSize = 0;
		allocatedBSPEntData = 0;
/*		if ( bspNodes != 0 ) {
			free( bspNodes );
			bspNodes = NULL;
			Sys_Printf( "freed bspNodes\n" );
			//numBSPNodes = 0;
			//allocatedBSPNodes = 0;
		}
		if ( bspDrawIndexes != 0 ) {
			free( bspDrawIndexes );
			bspDrawIndexes = NULL;
			Sys_Printf( "freed bspDrawIndexes\n" );
			//numBSPDrawIndexes = 0;
			//allocatedBSPDrawIndexes = 0;
		}
		if ( bspLeafSurfaces != 0 ) {
			free( bspLeafSurfaces );
			bspLeafSurfaces = NULL;
			Sys_Printf( "freed bspLeafSurfaces\n" );
			//numBSPLeafSurfaces = 0;
			//allocatedBSPLeafSurfaces = 0;
		}
		if ( bspLeafBrushes != 0 ) {
			free( bspLeafBrushes );
			bspLeafBrushes = NULL;
			Sys_Printf( "freed bspLeafBrushes\n" );
			//numBSPLeafBrushes = 0;
			//allocatedBSPLeafBrushes = 0;
		}
		if ( bspBrushSides != 0 ) {
			free( bspBrushSides );
			bspBrushSides = NULL;
			Sys_Printf( "freed bspBrushSides\n" );
			numBSPBrushSides = 0;
			allocatedBSPBrushSides = 0;
		}
		if ( numBSPFogs != 0 ) {
			Sys_Printf( "freed numBSPFogs\n" );
			numBSPFogs = 0;
		}
		if ( numBSPAds != 0 ) {
			Sys_Printf( "freed numBSPAds\n" );
			numBSPAds = 0;
		}
		if ( numBSPLeafs != 0 ) {
			Sys_Printf( "freed numBSPLeafs\n" );
			numBSPLeafs = 0;
		}
		if ( numBSPVisBytes != 0 ) {
			Sys_Printf( "freed numBSPVisBytes\n" );
			numBSPVisBytes = 0;
		}
*/	}



	vfsListShaderFiles( pk3Shaderfiles, pushStringCallback );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%i\n", pk3Shaderfiles->n );
		for ( i = 0; i < pk3Shaderfiles->n; ++i ){
			Sys_Printf( "%s\n", pk3Shaderfiles->s[i] );
		}
	}



	/* can exclude pure *base* textures right now, shouldn't create shaders for them anyway */
	for ( i = 0; i < pk3Shaders->n; ++i ){
		if( StrList_find( ExPureTextures, pk3Shaders->s[i] ) )
			strClear( pk3Shaders->s[i] );
	}
	/* can exclude repack.exclude shaders, assuming they got all their images */
	for ( i = 0; i < pk3Shaders->n; ++i ){
		if( StrList_find( rExShaders, pk3Shaders->s[i] ) )
			strClear( pk3Shaders->s[i] );
	}

	//Parse Shader Files
	Sys_Printf( "\t\nParsing shaders....\n\n" );
	StrBuf* shaderText = StrBuf_allocate( 8192 );
	StrBuf* allShaders = StrBuf_allocate( 16777216 );
	 /* hack */
	endofscript = true;

	for ( i = 0; i < pk3Shaderfiles->n; ++i ){
		bool wantShader = false;
		int shader, found;

		/* load the shader */
		char scriptFile[128];
		sprintf( scriptFile, "%s/%s", game->shaderPath, pk3Shaderfiles->s[i] );
		if ( dbg )
			Sys_Printf( "\n\tentering %s\n", pk3Shaderfiles->s[i] );
		SilentLoadScriptFile( scriptFile, 0 );

		/* tokenize it */
		while ( 1 )
		{
			int line = scriptline;
			/* test for end of file */
			if ( !GetToken( true ) ) {
				break;
			}
			//dump shader names
			if( dbg )
				Sys_Printf( "%s\n", token );

			StrBuf_cpy( shaderText, token );

			if ( strchr( token, '\\') != NULL  ){
				Sys_FPrintf( SYS_WRN, "WARNING1: %s : %s : shader name with backslash\n", pk3Shaderfiles->s[i], token );
			}

			/* do wanna le shader? */
			wantShader = false;
			if( ( found = StrList_find( pk3Shaders, token ) ) ){
				shader = found - 1;
				wantShader = true;
				if( StrList_find( rExTextures, token ) )
					Sys_FPrintf( SYS_WRN, "WARNING3: %s : about to include shader for excluded texture\n", token );

			}

			/* handle { } section */
			if ( !GetToken( true ) ) {
				break;
			}
			if ( !strEqual( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						scriptFile, scriptline, token, g_strLoadedFileLocation );
			}
			StrBuf_cat( shaderText, "\n{" );
			bool hasmap = false;

			while ( 1 )
			{
				line = scriptline;
				/* get the next token */
				if ( !GetToken( true ) ) {
					break;
				}
				if ( strEqual( token, "}" ) ) {
					StrBuf_cat( shaderText, "\n}\n\n" );
					break;
				}
				/* parse stage directives */
				if ( strEqual( token, "{" ) ) {
					bool tokenready = false;
					StrBuf_cat( shaderText, "\n\t{" );
					while ( 1 )
					{
						/* detour of TokenAvailable() '~' */
						if ( tokenready )
							tokenready = false;
						else
							line = scriptline;
						if ( !GetToken( true ) ) {
							break;
						}
						if ( strEqual( token, "}" ) ) {
							StrBuf_cat( shaderText, "\n\t}" );
							break;
						}
						if ( strEqual( token, "{" ) ) {
							StrBuf_cat( shaderText, "\n\t{" );
							Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", scriptFile, scriptline );
						}
						/* skip the shader */
						if ( !wantShader )
							continue;

						/* digest any images */
						if ( striEqual( token, "map" ) ||
							striEqual( token, "clampMap" ) ) {
							StrBuf_cat2( shaderText, "\n\t\t", token );
							hasmap = true;

							/* get an image */
							GetToken( false );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list( pk3Textures, ExTextures, rExTextures );
							}
							StrBuf_cat2( shaderText, " ", token );
						}
						else if ( striEqual( token, "animMap" ) ||
							striEqual( token, "clampAnimMap" ) ) {
							StrBuf_cat2( shaderText, "\n\t\t", token );
							hasmap = true;

							GetToken( false );// skip num
							StrBuf_cat2( shaderText, " ", token );
							while ( TokenAvailable() ){
								GetToken( false );
								tex2list( pk3Textures, ExTextures, rExTextures );
								StrBuf_cat2( shaderText, " ", token );
							}
							tokenready = true;
						}
						else if ( striEqual( token, "videoMap" ) ){
							StrBuf_cat2( shaderText, "\n\t\t", token );
							hasmap = true;
							GetToken( false );
							StrBuf_cat2( shaderText, " ", token );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL ){
								sprintf( str, "video/%s", token );
								strcpy( token, str );
							}
							if( !StrList_find( pk3Videos, token ) &&
								!StrList_find( ExVideos, token ) &&
								!StrList_find( rExVideos, token ) )
								StrList_append( pk3Videos, token );
						}
						else if ( striEqual( token, "mapComp" ) || striEqual( token, "mapNoComp" ) || striEqual( token, "animmapcomp" ) || striEqual( token, "animmapnocomp" ) ){
							Sys_FPrintf( SYS_WRN, "WARNING7: %s : %s shader\n", pk3Shaders->s[shader], token );
							hasmap = true;
							if ( line == scriptline ){
								StrBuf_cat2( shaderText, " ", token );
							}
							else{
								StrBuf_cat2( shaderText, "\n\t\t", token );
							}
						}
						else if ( line == scriptline ){
							StrBuf_cat2( shaderText, " ", token );
						}
						else{
							StrBuf_cat2( shaderText, "\n\t\t", token );
						}
					}
				}
				/* skip the shader */
				else if ( !wantShader )
					continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( striEqual( token, "surfaceparm" ) ) {
					StrBuf_cat( shaderText, "\n\tsurfaceparm " );
					GetToken( false );
					StrBuf_cat( shaderText, token );
					if ( striEqual( token, "nodraw" ) ) {
						wantShader = false;
						strClear( pk3Shaders->s[shader] );
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( striEqual( token, "skyParms" ) ) {
					StrBuf_cat( shaderText, "\n\tskyParms " );
					hasmap = true;
					/* get image base */
					GetToken( false );
					StrBuf_cat( shaderText, token );

					/* ignore bogus paths */
					if ( !strEqual( token, "-" ) && !striEqual( token, "full" ) ) {
						const char skysides[6][3] = { "up", "dn", "lf", "rt", "bk", "ft" };
						char* const skysidestring = token + strcatQ( token, "_@@.tga", sizeof( token ) ) - 6;
						for( size_t side = 0; side < 6; ++side ){
							memcpy( skysidestring, skysides[side], 2 );
							tex2list( pk3Textures, ExTextures, rExTextures );
						}
					}
					/* skip rest of line */
					GetToken( false );
					StrBuf_cat2( shaderText, " ", token );
					GetToken( false );
					StrBuf_cat2( shaderText, " ", token );
				}
				else if ( striEqualPrefix( token, "implicit" ) ){
					Sys_FPrintf( SYS_WRN, "WARNING5: %s : %s shader\n", pk3Shaders->s[shader], token );
					hasmap = true;
					if ( line == scriptline ){
						StrBuf_cat2( shaderText, " ", token );
					}
					else{
						StrBuf_cat2( shaderText, "\n\t", token );
					}
				}
				else if ( striEqual( token, "fogparms" ) ){
					hasmap = true;
					if ( line == scriptline ){
						StrBuf_cat2( shaderText, " ", token );
					}
					else{
						StrBuf_cat2( shaderText, "\n\t", token );
					}
				}
				else if ( line == scriptline ){
					StrBuf_cat2( shaderText, " ", token );
				}
				else{
					StrBuf_cat2( shaderText, "\n\t", token );
				}
			}

			//exclude shader
			if ( wantShader ){
				if( StrList_find( ExShaders, pk3Shaders->s[shader] ) ){
					wantShader = false;
					strClear( pk3Shaders->s[shader] );
				}
				if ( wantShader && !hasmap ){
					Sys_FPrintf( SYS_WRN, "WARNING8: %s : shader has no known maps\n", pk3Shaders->s[shader] );
					wantShader = false;
					strClear( pk3Shaders->s[shader] );
				}
				if ( wantShader ){
					StrBuf_cat( allShaders, shaderText->s );
					strClear( pk3Shaders->s[shader] );
				}
			}
		}
	}
/* TODO: RTCW's mapComp, mapNoComp, animmapcomp, animmapnocomp; nocompress?; ET's implicitmap, implicitblend, implicitmask */


/* exclude stuff */

//pure textures (shader ones are done)
	for ( i = 0; i < pk3Shaders->n; ++i ){
		if ( !strEmpty( pk3Shaders->s[i] ) ){
			if ( strchr( pk3Shaders->s[i], '\\') != NULL  ){
				Sys_FPrintf( SYS_WRN, "WARNING2: %s : bsp shader path with backslash\n", pk3Shaders->s[i] );
				FixDOSName( pk3Shaders->s[i] );
				//what if theres properly slashed one in the list?
				for ( j = 0; j < pk3Shaders->n; ++j ){
					if ( striEqual( pk3Shaders->s[i], pk3Shaders->s[j] ) && ( i != j ) ){
						strClear( pk3Shaders->s[i] );
						break;
					}
				}
			}
			if ( !strEmpty( pk3Shaders->s[i] ) ){
				if( StrList_find( pk3Textures, pk3Shaders->s[i] ) ||
					StrList_find( ExTextures, pk3Shaders->s[i] ) ||
					StrList_find( rExTextures, pk3Shaders->s[i] ) )
					strClear( pk3Shaders->s[i] );
			}
		}
	}

//snds
	for ( i = 0; i < pk3Sounds->n; ++i ){
		if( StrList_find( ExSounds, pk3Sounds->s[i] ) ||
			StrList_find( rExSounds, pk3Sounds->s[i] ) )
			strClear( pk3Sounds->s[i] );
	}

	/* write shader */
	sprintf( str, "%s/%s_strippedBYrepacker.shader", EnginePath, nameOFrepack );
	FILE *f;
	f = fopen( str, "wb" );
	fwrite( allShaders->s, sizeof( char ), allShaders->strlen, f );
	fclose( f );
	Sys_Printf( "Shaders saved to %s\n", str );

	/* make a pack */
	char packname[ 1024 ];
	sprintf( packname, "%s/%s_repacked.pk3", EnginePath, nameOFrepack );
	remove( packname );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3Textures->n; ++i ){
		if( !packTexture( pk3Textures->s[i], packname, compLevel, png ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Textures->s[i] );
		}
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3Shaders->n; ++i ){
		if ( !strEmpty( pk3Shaders->s[i] ) ){
			if( !packTexture( pk3Shaders->s[i], packname, compLevel, png ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders->s[i] );
			}
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3Sounds->n; ++i ){
		if ( !strEmpty( pk3Sounds->s[i] ) ){
			if ( !packResource( pk3Sounds->s[i], packname, compLevel ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Sounds->s[i] );
			}
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3Videos->n; ++i ){
		if ( !packResource( pk3Videos->s[i], packname, compLevel ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Videos->s[i] );
		}
	}

	Sys_Printf( "\nSaved to %s\n", packname );

	/* return to sender */
	return 0;
}


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



/* marker */
#define MAIN_C



/* dependencies */
#include "q3map2.h"



/*
   Random()
   returns a pseudorandom number between 0 and 1
 */

vec_t Random( void ){
	return (vec_t) rand() / RAND_MAX;
}


char *Q_strncpyz( char *dst, const char *src, size_t len ) {
	if ( len == 0 ) {
		abort();
	}

	strncpy( dst, src, len );
	dst[ len - 1 ] = '\0';
	return dst;
}


char *Q_strcat( char *dst, size_t dlen, const char *src ) {
	size_t n = strlen( dst );

	if ( n > dlen ) {
		abort(); /* buffer overflow */
	}

	return Q_strncpyz( dst + n, src, dlen - n );
}


char *Q_strncat( char *dst, size_t dlen, const char *src, size_t slen ) {
	size_t n = strlen( dst );

	if ( n > dlen ) {
		abort(); /* buffer overflow */
	}

	return Q_strncpyz( dst + n, src, MIN( slen, dlen - n ) );
}


/*
   ExitQ3Map()
   cleanup routine
 */

static void ExitQ3Map( void ){
	/* flush xml send buffer, shut down connection */
	Broadcast_Shutdown();
	BSPFilesCleanup();
	if ( mapDrawSurfs != NULL ) {
		free( mapDrawSurfs );
	}
}


/*
   MD4BlockChecksum()
   calculates an md4 checksum for a block of data
 */

static int MD4BlockChecksum( void *buffer, int length ){
	return Com_BlockChecksum( buffer, length );
}

/*
   FixAAS()
   resets an aas checksum to match the given BSP
 */

int FixAAS( int argc, char **argv ){
	int length, checksum;
	void        *buffer;
	FILE        *file;
	char aas[ 1024 ], **ext;
	char        *exts[] =
	{
		".aas",
		"_b0.aas",
		"_b1.aas",
		NULL
	};


	/* arg checking */
	if ( argc < 2 ) {
		Sys_Printf( "Usage: q3map -fixaas [-v] <mapname>\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* note it */
	Sys_Printf( "--- FixAAS ---\n" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	length = LoadFile( source, &buffer );

	/* create bsp checksum */
	Sys_Printf( "Creating checksum...\n" );
	checksum = LittleLong( MD4BlockChecksum( buffer, length ) );

	/* write checksum to aas */
	ext = exts;
	while ( *ext )
	{
		/* mangle name */
		strcpy( aas, source );
		StripExtension( aas );
		strcat( aas, *ext );
		Sys_Printf( "Trying %s\n", aas );
		ext++;

		/* fix it */
		file = fopen( aas, "r+b" );
		if ( !file ) {
			continue;
		}
		if ( fwrite( &checksum, 4, 1, file ) != 1 ) {
			Error( "Error writing checksum to %s", aas );
		}
		fclose( file );
	}

	/* return to sender */
	return 0;
}



void FixDOSName( char *src ){
	if ( src == NULL ) {
		return;
	}

	while ( *src )
	{
		if ( *src == '\\' ) {
			*src = '/';
		}
		src++;
	}
}

/*
	Check if newcoming texture is unique and not excluded
*/
void tex2list( char* texlist, int *texnum, char* EXtex, int *EXtexnum ){
	int i;
	if ( token[0] == '\0') return;
	StripExtension( token );
	FixDOSName( token );
	for ( i = 0; i < *texnum; i++ ){
		if ( !Q_stricmp( texlist + i*65, token ) ) return;
	}
	for ( i = 0; i < *EXtexnum; i++ ){
		if ( !Q_stricmp( EXtex + i*65, token ) ) return;
	}
	strcpy ( texlist + (*texnum)*65, token );
	(*texnum)++;
	return;
}

/* 4 repack */
void tex2list2( char* texlist, int *texnum, char* EXtex, int *EXtexnum, char* rEXtex, int *rEXtexnum ){
	int i;
	if ( token[0] == '\0') return;
	//StripExtension( token );
	char* dot = strrchr( token, '.' );
	if ( dot != NULL){
		if ( Q_stricmp( dot, ".tga" ) && Q_stricmp( dot, ".jpg" ) && Q_stricmp( dot, ".png" ) ){
			Sys_FPrintf( SYS_WRN, "WARNING4: %s : weird or missing extension in shader\n", token );
		}
		else{
			*dot = '\0';
		}
	}
	FixDOSName( token );
	strcpy ( texlist + (*texnum)*65, token );
	strcat( token, ".tga" );

	/* exclude */
	for ( i = 0; i < *texnum; i++ ){
		if ( !Q_stricmp( texlist + i*65, texlist + (*texnum)*65 ) ) return;
	}
	for ( i = 0; i < *EXtexnum; i++ ){
		if ( !Q_stricmp( EXtex + i*65, texlist + (*texnum)*65 ) ) return;
	}
	for ( i = 0; i < *rEXtexnum; i++ ){
		if ( !Q_stricmp( rEXtex + i*65, texlist + (*texnum)*65 ) ) return;
	}
	(*texnum)++;
	return;
}


/*
	Check if newcoming res is unique
*/
void res2list( char* data, int *num ){
	int i;
	if ( strlen( data + (*num)*65 ) > 64 ){
		Sys_FPrintf( SYS_WRN, "WARNING6: %s : path too long.\n", data + (*num)*65 );
	}
	while ( *( data + (*num)*65 ) == '\\' || *( data + (*num)*65 ) == '/' ){
		char* cut = data + (*num)*65 + 1;
		strcpy( data + (*num)*65, cut );
	}
	if ( *( data + (*num)*65 ) == '\0') return;
	for ( i = 0; i < *num; i++ ){
		if ( !Q_stricmp( data + i*65, data + (*num)*65 ) ) return;
	}
	(*num)++;
	return;
}

void parseEXblock ( char* data, int *num, const char *exName ){
	if ( !GetToken( qtrue ) || strcmp( token, "{" ) ) {
		Error( "ReadExclusionsFile: %s, line %d: { not found", exName, scriptline );
	}
	while ( 1 )
	{
		if ( !GetToken( qtrue ) ) {
			break;
		}
		if ( !strcmp( token, "}" ) ) {
			break;
		}
		if ( token[0] == '{' ) {
			Error( "ReadExclusionsFile: %s, line %d: brace, opening twice in a row.", exName, scriptline );
		}

		/* add to list */
		strcpy( data + (*num)*65, token );
		(*num)++;
	}
	return;
}

char q3map2path[1024];
/*
   pk3BSPMain()
   map autopackager, works for Q3 type of shaders and ents
 */

int pk3BSPMain( int argc, char **argv ){
	int i, j, len;
	qboolean dbg = qfalse, png = qfalse, packFAIL = qfalse;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ ){
		if ( !strcmp( argv[ i ],  "-dbg" ) ) {
			dbg = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-png" ) ) {
			png = qtrue;
		}
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();


	char packname[ 1024 ], packFailName[ 1024 ], base[ 1024 ], nameOFmap[ 1024 ], temp[ 1024 ];

	/* copy map name */
	strcpy( base, source );
	StripExtension( base );

	/* extract map name */
	len = strlen( base ) - 1;
	while ( len > 0 && base[ len ] != '/' && base[ len ] != '\\' )
		len--;
	strcpy( nameOFmap, &base[ len + 1 ] );


	qboolean drawsurfSHs[1024] = { qfalse };

	for ( i = 0; i < numBSPDrawSurfaces; i++ ){
		/* can't exclude nodraw patches here (they want shaders :0!) */
		//if ( !( bspDrawSurfaces[i].surfaceType == 2 && bspDrawSurfaces[i].numIndexes == 0 ) ) drawsurfSHs[bspDrawSurfaces[i].shaderNum] = qtrue;
		drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = qtrue;
		//Sys_Printf( "%s\n", bspShaders[bspDrawSurfaces[i].shaderNum].shader );
	}

	int pk3ShadersN = 0;
	char* pk3Shaders = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3SoundsN = 0;
	char* pk3Sounds = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3ShaderfilesN = 0;
	char* pk3Shaderfiles = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3TexturesN = 0;
	char* pk3Textures = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3VideosN = 0;
	char* pk3Videos = (char *)calloc( 1024*65, sizeof( char ) );



	for ( i = 0; i < numBSPShaders; i++ ){
		if ( drawsurfSHs[i] ){
			strcpy( pk3Shaders + pk3ShadersN*65, bspShaders[i].shader );
			res2list( pk3Shaders, &pk3ShadersN );
			//pk3ShadersN++;
			//Sys_Printf( "%s\n", bspShaders[i].shader );
		}
	}

	/* Ent keys */
	epair_t *ep;
	for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
	{
		if ( !Q_strncasecmp( ep->key, "vertexremapshader", 17 ) ) {
			sscanf( ep->value, "%*[^;] %*[;] %s", pk3Shaders + pk3ShadersN*65 );
			res2list( pk3Shaders, &pk3ShadersN );
		}
	}
	strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[0], "music" ) );
	if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' ){
		FixDOSName( pk3Sounds + pk3SoundsN*65 );
		DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
		res2list( pk3Sounds, &pk3SoundsN );
	}

	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[i], "noise" ) );
		if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' && *( pk3Sounds + pk3SoundsN*65 ) != '*' ){
			FixDOSName( pk3Sounds + pk3SoundsN*65 );
			DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
			res2list( pk3Sounds, &pk3SoundsN );
		}

		if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "func_plat" ) ){
			strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_strt.wav");
			res2list( pk3Sounds, &pk3SoundsN );
			strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_end.wav");
			res2list( pk3Sounds, &pk3SoundsN );
		}
		if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "target_push" ) ){
			if ( !(IntForKey( &entities[i], "spawnflags") & 1) ){
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/misc/windfly.wav");
				res2list( pk3Sounds, &pk3SoundsN );
			}
		}
		strcpy( pk3Shaders + pk3ShadersN*65, ValueForKey( &entities[i], "targetShaderNewName" ) );
		res2list( pk3Shaders, &pk3ShadersN );
	}

	//levelshot
	sprintf( pk3Shaders + pk3ShadersN*65, "levelshots/%s", nameOFmap );
	res2list( pk3Shaders, &pk3ShadersN );


	if( dbg ){
		Sys_Printf( "\n\tDrawsurface+ent calls....%i\n", pk3ShadersN );
		for ( i = 0; i < pk3ShadersN; i++ ){
			Sys_Printf( "%s\n", pk3Shaders + i*65 );
		}
		Sys_Printf( "\n\tSounds....%i\n", pk3SoundsN );
		for ( i = 0; i < pk3SoundsN; i++ ){
			Sys_Printf( "%s\n", pk3Sounds + i*65 );
		}
	}

	vfsListShaderFiles( pk3Shaderfiles, &pk3ShaderfilesN );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%i\n", pk3ShaderfilesN );
		for ( i = 0; i < pk3ShaderfilesN; i++ ){
			Sys_Printf( "%s\n", pk3Shaderfiles + i*65 );
		}
	}


	/* load exclusions file */
	int ExTexturesN = 0;
	char* ExTextures = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShadersN = 0;
	char* ExShaders = (char *)calloc( 4096*65, sizeof( char ) );
	int ExSoundsN = 0;
	char* ExSounds = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShaderfilesN = 0;
	char* ExShaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int ExVideosN = 0;
	char* ExVideos = (char *)calloc( 4096*65, sizeof( char ) );
	int ExPureTexturesN = 0;
	char* ExPureTextures = (char *)calloc( 4096*65, sizeof( char ) );

	char* ExReasonShader[4096] = { NULL };
	char* ExReasonShaderFile[4096] = { NULL };

	char exName[ 1024 ];
	byte *buffer;
	int size;

	strcpy( exName, q3map2path );
	char *cut = strrchr( exName, '\\' );
	char *cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Warning( "Unable to load exclusions file.\n" );
		goto skipEXfile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, game->arg );
	strcat( exName, ".exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Warning( "Unable to find exclusions file %s.\n", exName );
		goto skipEXfile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( ExTextures, &ExTexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( ExShaders, &ExShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( ExShaderfiles, &ExShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( ExSounds, &ExSoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( ExVideos, &ExVideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

	for ( i = 0; i < ExTexturesN; i++ ){
		for ( j = 0; j < ExShadersN; j++ ){
			if ( !Q_stricmp( ExTextures + i*65, ExShaders + j*65 ) ){
				break;
			}
		}
		if ( j == ExShadersN ){
			strcpy ( ExPureTextures + ExPureTexturesN*65, ExTextures + i*65 );
			ExPureTexturesN++;
		}
	}

skipEXfile:

	if( dbg ){
		Sys_Printf( "\n\tExTextures....%i\n", ExTexturesN );
		for ( i = 0; i < ExTexturesN; i++ ) Sys_Printf( "%s\n", ExTextures + i*65 );
		Sys_Printf( "\n\tExPureTextures....%i\n", ExPureTexturesN );
		for ( i = 0; i < ExPureTexturesN; i++ ) Sys_Printf( "%s\n", ExPureTextures + i*65 );
		Sys_Printf( "\n\tExShaders....%i\n", ExShadersN );
		for ( i = 0; i < ExShadersN; i++ ) Sys_Printf( "%s\n", ExShaders + i*65 );
		Sys_Printf( "\n\tExShaderfiles....%i\n", ExShaderfilesN );
		for ( i = 0; i < ExShaderfilesN; i++ ) Sys_Printf( "%s\n", ExShaderfiles + i*65 );
		Sys_Printf( "\n\tExSounds....%i\n", ExSoundsN );
		for ( i = 0; i < ExSoundsN; i++ ) Sys_Printf( "%s\n", ExSounds + i*65 );
		Sys_Printf( "\n\tExVideos....%i\n", ExVideosN );
		for ( i = 0; i < ExVideosN; i++ ) Sys_Printf( "%s\n", ExVideos + i*65 );
	}

	/* can exclude pure textures right now, shouldn't create shaders for them anyway */
	for ( i = 0; i < pk3ShadersN ; i++ ){
		for ( j = 0; j < ExPureTexturesN ; j++ ){
			if ( !Q_stricmp( pk3Shaders + i*65, ExPureTextures + j*65 ) ){
				*( pk3Shaders + i*65 ) = '\0';
				break;
			}
		}
	}

	//Parse Shader Files
	 /* hack */
	endofscript = qtrue;

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		qboolean wantShader = qfalse, wantShaderFile = qfalse, ShaderFileExcluded = qfalse;
		int shader;
		char* reasonShader = NULL;
		char* reasonShaderFile = NULL;

		/* load the shader */
		sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
		SilentLoadScriptFile( temp, 0 );
		if( dbg ) Sys_Printf( "\n\tentering %s\n", pk3Shaderfiles + i*65 );

		/* do wanna le shader file? */
		for ( j = 0; j < ExShaderfilesN; j++ ){
			if ( !Q_stricmp( ExShaderfiles + j*65, pk3Shaderfiles + i*65 ) ){
				ShaderFileExcluded = qtrue;
				reasonShaderFile = ExShaderfiles + j*65;
				break;
			}
		}
		/* tokenize it */
		/* check if shader file has to be excluded */
		while ( !ShaderFileExcluded )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}

			/* does it contain restricted shaders/textures? */
			for ( j = 0; j < ExShadersN; j++ ){
				if ( !Q_stricmp( ExShaders + j*65, token ) ){
					ShaderFileExcluded = qtrue;
					reasonShader = ExShaders + j*65;
					break;
				}
			}
			if ( ShaderFileExcluded )
				break;
			for ( j = 0; j < ExPureTexturesN; j++ ){
				if ( !Q_stricmp( ExPureTextures + j*65, token ) ){
					ShaderFileExcluded = qtrue;
					reasonShader = ExPureTextures + j*65;
					break;
				}
			}
			if ( ShaderFileExcluded )
				break;

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						temp, scriptline, token, g_strLoadedFileLocation );
			}

			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					break;
				}
				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							break;
						}
					}
				}
			}
		}

		/* tokenize it again */
		SilentLoadScriptFile( temp, 0 );
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			//dump shader names
			if( dbg ) Sys_Printf( "%s\n", token );

			/* do wanna le shader? */
			wantShader = qfalse;
			for ( j = 0; j < pk3ShadersN; j++ ){
				if ( !Q_stricmp( pk3Shaders + j*65, token) ){
					shader = j;
					wantShader = qtrue;
					break;
				}
			}

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						temp, scriptline, token, g_strLoadedFileLocation );
			}

			qboolean hasmap = qfalse;
			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					break;
				}


				/* -----------------------------------------------------------------
				shader stages (passes)
				----------------------------------------------------------------- */

				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							break;
						}
						if ( !strcmp( token, "{" ) ) {
							Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", temp, scriptline );
						}
						if ( !Q_stricmp( token, "mapComp" ) || !Q_stricmp( token, "mapNoComp" ) || !Q_stricmp( token, "animmapcomp" ) || !Q_stricmp( token, "animmapnocomp" ) ){
							Sys_FPrintf( SYS_WRN, "WARNING7: %s : line %d : unsupported '%s' map directive\n", temp, scriptline, token );
						}
						/* skip the shader */
						if ( !wantShader ) continue;

						/* digest any images */
						if ( !Q_stricmp( token, "map" ) ||
							!Q_stricmp( token, "clampMap" ) ) {
							hasmap = qtrue;
							/* get an image */
							GetToken( qfalse );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
							}
						}
						else if ( !Q_stricmp( token, "animMap" ) ||
							!Q_stricmp( token, "clampAnimMap" ) ) {
							hasmap = qtrue;
							GetToken( qfalse );// skip num
							while ( TokenAvailable() ){
								GetToken( qfalse );
								tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
							}
						}
						else if ( !Q_stricmp( token, "videoMap" ) ){
							hasmap = qtrue;
							GetToken( qfalse );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL && strchr( token, '\\' ) == NULL ){
								sprintf( temp, "video/%s", token );
								strcpy( token, temp );
							}
							FixDOSName( token );
							for ( j = 0; j < pk3VideosN; j++ ){
								if ( !Q_stricmp( pk3Videos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < ExVideosN; j++ ){
								if ( !Q_stricmp( ExVideos + j*65, token ) ){
									goto away;
								}
							}
							strcpy ( pk3Videos + pk3VideosN*65, token );
							pk3VideosN++;
							away:
							j = 0;
						}
					}
				}
				else if ( !Q_strncasecmp( token, "implicit", 8 ) ){
					Sys_FPrintf( SYS_WRN, "WARNING5: %s : line %d : unsupported %s shader\n", temp, scriptline, token );
				}
				/* skip the shader */
				else if ( !wantShader ) continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( !Q_stricmp( token, "surfaceparm" ) ) {
					GetToken( qfalse );
					if ( !Q_stricmp( token, "nodraw" ) ) {
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( !Q_stricmp( token, "skyParms" ) ) {
					hasmap = qtrue;
					/* get image base */
					GetToken( qfalse );

					/* ignore bogus paths */
					if ( Q_stricmp( token, "-" ) && Q_stricmp( token, "full" ) ) {
						strcpy ( temp, token );
						sprintf( token, "%s_up", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_dn", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_lf", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_rt", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_bk", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
						sprintf( token, "%s_ft", temp );
						tex2list( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN );
					}
					/* skip rest of line */
					GetToken( qfalse );
					GetToken( qfalse );
				}
				else if ( !Q_stricmp( token, "fogparms" ) ){
					hasmap = qtrue;
				}
			}

			//exclude shader
			if ( wantShader ){
				for ( j = 0; j < ExShadersN; j++ ){
					if ( !Q_stricmp( ExShaders + j*65, pk3Shaders + shader*65 ) ){
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
						break;
					}
				}
				if ( !hasmap ){
					wantShader = qfalse;
				}
				if ( wantShader ){
					if ( ShaderFileExcluded ){
						if ( reasonShaderFile != NULL ){
							ExReasonShaderFile[ shader ] = reasonShaderFile;
						}
						else{
							ExReasonShaderFile[ shader ] = ( char* ) calloc( 65, sizeof( char ) );
							strcpy( ExReasonShaderFile[ shader ], pk3Shaderfiles + i*65 );
						}
						ExReasonShader[ shader ] = reasonShader;
					}
					else{
						wantShaderFile = qtrue;
						*( pk3Shaders + shader*65 ) = '\0';
					}
				}
			}
		}
		if ( !wantShaderFile ){
			*( pk3Shaderfiles + i*65 ) = '\0';
		}
	}



/* exclude stuff */
//wanted shaders from excluded .shaders
	Sys_Printf( "\n" );
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' && ( ExReasonShader[i] != NULL || ExReasonShaderFile[i] != NULL ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders + i*65 );
			packFAIL = qtrue;
			if ( ExReasonShader[i] != NULL ){
				Sys_Printf( "     reason: is located in %s,\n     containing restricted shader %s\n", ExReasonShaderFile[i], ExReasonShader[i] );
			}
			else{
				Sys_Printf( "     reason: is located in restricted %s\n", ExReasonShaderFile[i] );
			}
			*( pk3Shaders + i*65 ) = '\0';
		}
	}
//pure textures (shader ones are done)
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			FixDOSName( pk3Shaders + i*65 );
			for ( j = 0; j < pk3TexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, pk3Textures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < ExTexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, ExTextures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
		}
	}

//snds
	for ( i = 0; i < pk3SoundsN; i++ ){
		for ( j = 0; j < ExSoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, ExSounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
	}

	/* make a pack */
	sprintf( packname, "%s/%s_autopacked.pk3", EnginePath, nameOFmap );
	remove( packname );
	sprintf( packFailName, "%s/%s_FAILEDpack.pk3", EnginePath, nameOFmap );
	remove( packFailName );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3TexturesN; i++ ){
		if ( png ){
			sprintf( temp, "%s.png", pk3Textures + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
		}
		sprintf( temp, "%s.tga", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		sprintf( temp, "%s.jpg", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Textures + i*65 );
		packFAIL = qtrue;
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( png ){
				sprintf( temp, "%s.png", pk3Shaders + i*65 );
				if ( vfsPackFile( temp, packname, 10 ) ){
					Sys_Printf( "++%s\n", temp );
					continue;
				}
			}
			sprintf( temp, "%s.tga", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			sprintf( temp, "%s.jpg", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}

			if ( i == pk3ShadersN - 1 ){ //levelshot typically
				Sys_Printf( "  ~fail  %s\n", pk3Shaders + i*65 );
			}
			else{
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders + i*65 );
				packFAIL = qtrue;
			}
		}
	}

	Sys_Printf( "\n\tShaizers....\n" );

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		if ( *( pk3Shaderfiles + i*65 ) != '\0' ){
			sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
			if ( vfsPackFile( temp, packname, 10 ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders + i*65 );
			packFAIL = qtrue;
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3SoundsN; i++ ){
		if ( *( pk3Sounds + i*65 ) != '\0' ){
			if ( vfsPackFile( pk3Sounds + i*65, packname, 10 ) ){
				Sys_Printf( "++%s\n", pk3Sounds + i*65 );
				continue;
			}
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Sounds + i*65 );
			packFAIL = qtrue;
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3VideosN; i++ ){
		if ( vfsPackFile( pk3Videos + i*65, packname, 10 ) ){
			Sys_Printf( "++%s\n", pk3Videos + i*65 );
			continue;
		}
		Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Videos + i*65 );
		packFAIL = qtrue;
	}

	Sys_Printf( "\n\t.bsp and stuff\n" );

	sprintf( temp, "maps/%s.bsp", nameOFmap );
	//if ( vfsPackFile( temp, packname, 10 ) ){
	if ( vfsPackFile_Absolute_Path( source, temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", temp );
		packFAIL = qtrue;
	}

	sprintf( temp, "maps/%s.aas", nameOFmap );
	if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  ~fail  %s\n", temp );
	}

	sprintf( temp, "scripts/%s.arena", nameOFmap );
	if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  ~fail  %s\n", temp );
	}

	sprintf( temp, "scripts/%s.defi", nameOFmap );
	if ( vfsPackFile( temp, packname, 10 ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  ~fail  %s\n", temp );
	}

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
	int i, j, len, compLevel = 0;
	qboolean dbg = qfalse, png = qfalse;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ ){
		if ( !strcmp( argv[ i ],  "-dbg" ) ) {
			dbg = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-png" ) ) {
			png = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-complevel" ) ) {
			compLevel = atoi( argv[ i + 1 ] );
			i++;
			if ( compLevel < -1 ) compLevel = -1;
			if ( compLevel > 10 ) compLevel = 10;
			Sys_Printf( "Compression level set to %i\n", compLevel );
		}
	}

/* load exclusions file */
	int ExTexturesN = 0;
	char* ExTextures = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShadersN = 0;
	char* ExShaders = (char *)calloc( 4096*65, sizeof( char ) );
	int ExSoundsN = 0;
	char* ExSounds = (char *)calloc( 4096*65, sizeof( char ) );
	int ExShaderfilesN = 0;
	char* ExShaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int ExVideosN = 0;
	char* ExVideos = (char *)calloc( 4096*65, sizeof( char ) );
	int ExPureTexturesN = 0;
	char* ExPureTextures = (char *)calloc( 4096*65, sizeof( char ) );


	char exName[ 1024 ];
	byte *buffer;
	int size;

	strcpy( exName, q3map2path );
	char *cut = strrchr( exName, '\\' );
	char *cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Warning( "Unable to load exclusions file.\n" );
		goto skipEXfile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, game->arg );
	strcat( exName, ".exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Warning( "Unable to find exclusions file %s.\n", exName );
		goto skipEXfile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( ExTextures, &ExTexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( ExShaders, &ExShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( ExShaderfiles, &ExShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( ExSounds, &ExSoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( ExVideos, &ExVideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

	for ( i = 0; i < ExTexturesN; i++ ){
		for ( j = 0; j < ExShadersN; j++ ){
			if ( !Q_stricmp( ExTextures + i*65, ExShaders + j*65 ) ){
				break;
			}
		}
		if ( j == ExShadersN ){
			strcpy ( ExPureTextures + ExPureTexturesN*65, ExTextures + i*65 );
			ExPureTexturesN++;
		}
	}

skipEXfile:

	if( dbg ){
		Sys_Printf( "\n\tExTextures....%i\n", ExTexturesN );
		for ( i = 0; i < ExTexturesN; i++ ) Sys_Printf( "%s\n", ExTextures + i*65 );
		Sys_Printf( "\n\tExPureTextures....%i\n", ExPureTexturesN );
		for ( i = 0; i < ExPureTexturesN; i++ ) Sys_Printf( "%s\n", ExPureTextures + i*65 );
		Sys_Printf( "\n\tExShaders....%i\n", ExShadersN );
		for ( i = 0; i < ExShadersN; i++ ) Sys_Printf( "%s\n", ExShaders + i*65 );
		Sys_Printf( "\n\tExShaderfiles....%i\n", ExShaderfilesN );
		for ( i = 0; i < ExShaderfilesN; i++ ) Sys_Printf( "%s\n", ExShaderfiles + i*65 );
		Sys_Printf( "\n\tExSounds....%i\n", ExSoundsN );
		for ( i = 0; i < ExSoundsN; i++ ) Sys_Printf( "%s\n", ExSounds + i*65 );
		Sys_Printf( "\n\tExVideos....%i\n", ExVideosN );
		for ( i = 0; i < ExVideosN; i++ ) Sys_Printf( "%s\n", ExVideos + i*65 );
	}




/* load repack.exclude */
	int rExTexturesN = 0;
	char* rExTextures = (char *)calloc( 65536*65, sizeof( char ) );
	int rExShadersN = 0;
	char* rExShaders = (char *)calloc( 32768*65, sizeof( char ) );
	int rExSoundsN = 0;
	char* rExSounds = (char *)calloc( 8192*65, sizeof( char ) );
	int rExShaderfilesN = 0;
	char* rExShaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int rExVideosN = 0;
	char* rExVideos = (char *)calloc( 4096*65, sizeof( char ) );

	strcpy( exName, q3map2path );
	cut = strrchr( exName, '\\' );
	cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Warning( "Unable to load repack exclusions file.\n" );
		goto skipEXrefile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, "repack.exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Warning( "Unable to find repack exclusions file %s.\n", exName );
		goto skipEXrefile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( rExTextures, &rExTexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( rExShaders, &rExShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( rExShaderfiles, &rExShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( rExSounds, &rExSoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( rExVideos, &rExVideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

skipEXrefile:

	if( dbg ){
		Sys_Printf( "\n\trExTextures....%i\n", rExTexturesN );
		for ( i = 0; i < rExTexturesN; i++ ) Sys_Printf( "%s\n", rExTextures + i*65 );
		Sys_Printf( "\n\trExShaders....%i\n", rExShadersN );
		for ( i = 0; i < rExShadersN; i++ ) Sys_Printf( "%s\n", rExShaders + i*65 );
		Sys_Printf( "\n\trExShaderfiles....%i\n", rExShaderfilesN );
		for ( i = 0; i < rExShaderfilesN; i++ ) Sys_Printf( "%s\n", rExShaderfiles + i*65 );
		Sys_Printf( "\n\trExSounds....%i\n", rExSoundsN );
		for ( i = 0; i < rExSoundsN; i++ ) Sys_Printf( "%s\n", rExSounds + i*65 );
		Sys_Printf( "\n\trExVideos....%i\n", rExVideosN );
		for ( i = 0; i < rExVideosN; i++ ) Sys_Printf( "%s\n", rExVideos + i*65 );
	}




	int bspListN = 0;
	char* bspList = (char *)calloc( 8192*1024, sizeof( char ) );

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	if ( !Q_stricmp( strrchr( source, '.' ), ".bsp" ) ){
		strcpy( bspList, source );
		bspListN++;
	}
	else{
		/* load bsps paths list */
		Sys_Printf( "Loading %s\n", source );
		size = TryLoadFile( source, (void**) &buffer );
		if ( size <= 0 ) {
			Sys_Warning( "Unable to open bsps paths list file %s.\n", source );
		}

		/* parse the file */
		ParseFromMemory( (char *) buffer, size );

		/* tokenize it */
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			strcpy( bspList + bspListN * 1024 , token );
			bspListN++;
		}

		/* free the buffer */
		free( buffer );
	}

	char packname[ 1024 ], nameOFrepack[ 1024 ], nameOFmap[ 1024 ], temp[ 1024 ];

	/* copy input file name */
	strcpy( temp, source );
	StripExtension( temp );

	/* extract input file name */
	len = strlen( temp ) - 1;
	while ( len > 0 && temp[ len ] != '/' && temp[ len ] != '\\' )
		len--;
	strcpy( nameOFrepack, &temp[ len + 1 ] );


/* load bsps */
	int pk3ShadersN = 0;
	char* pk3Shaders = (char *)calloc( 65536*65, sizeof( char ) );
	int pk3SoundsN = 0;
	char* pk3Sounds = (char *)calloc( 4096*65, sizeof( char ) );
	int pk3ShaderfilesN = 0;
	char* pk3Shaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int pk3TexturesN = 0;
	char* pk3Textures = (char *)calloc( 65536*65, sizeof( char ) );
	int pk3VideosN = 0;
	char* pk3Videos = (char *)calloc( 1024*65, sizeof( char ) );

	for( j = 0; j < bspListN; j++ ){

		int pk3SoundsNold = pk3SoundsN;
		int pk3ShadersNold = pk3ShadersN;

		strcpy( source, bspList + j*1024 );
		StripExtension( source );
		DefaultExtension( source, ".bsp" );

		/* load the bsp */
		Sys_Printf( "\nLoading %s\n", source );
		PartialLoadBSPFile( source );
		ParseEntities();

		/* copy map name */
		strcpy( temp, source );
		StripExtension( temp );

		/* extract map name */
		len = strlen( temp ) - 1;
		while ( len > 0 && temp[ len ] != '/' && temp[ len ] != '\\' )
			len--;
		strcpy( nameOFmap, &temp[ len + 1 ] );


		qboolean drawsurfSHs[1024] = { qfalse };

		for ( i = 0; i < numBSPDrawSurfaces; i++ ){
			drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = qtrue;
		}

		for ( i = 0; i < numBSPShaders; i++ ){
			if ( drawsurfSHs[i] ){
				strcpy( pk3Shaders + pk3ShadersN*65, bspShaders[i].shader );
				res2list( pk3Shaders, &pk3ShadersN );
			}
		}

		/* Ent keys */
		epair_t *ep;
		for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
		{
			if ( !Q_strncasecmp( ep->key, "vertexremapshader", 17 ) ) {
				sscanf( ep->value, "%*[^;] %*[;] %s", pk3Shaders + pk3ShadersN*65 );
				res2list( pk3Shaders, &pk3ShadersN );
			}
		}
		strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[0], "music" ) );
		if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' ){
			FixDOSName( pk3Sounds + pk3SoundsN*65 );
			DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
			res2list( pk3Sounds, &pk3SoundsN );
		}

		for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
		{
			strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[i], "noise" ) );
			if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' && *( pk3Sounds + pk3SoundsN*65 ) != '*' ){
				FixDOSName( pk3Sounds + pk3SoundsN*65 );
				DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
				res2list( pk3Sounds, &pk3SoundsN );
			}

			if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "func_plat" ) ){
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_strt.wav");
				res2list( pk3Sounds, &pk3SoundsN );
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_end.wav");
				res2list( pk3Sounds, &pk3SoundsN );
			}
			if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "target_push" ) ){
				if ( !(IntForKey( &entities[i], "spawnflags") & 1) ){
					strcpy( pk3Sounds + pk3SoundsN*65, "sound/misc/windfly.wav");
					res2list( pk3Sounds, &pk3SoundsN );
				}
			}
			strcpy( pk3Shaders + pk3ShadersN*65, ValueForKey( &entities[i], "targetShaderNewName" ) );
			res2list( pk3Shaders, &pk3ShadersN );
		}

		//levelshot
		sprintf( pk3Shaders + pk3ShadersN*65, "levelshots/%s", nameOFmap );
		res2list( pk3Shaders, &pk3ShadersN );



		Sys_Printf( "\n\t+Drawsurface+ent calls....%i\n", pk3ShadersN - pk3ShadersNold );
		for ( i = pk3ShadersNold; i < pk3ShadersN; i++ ){
			Sys_Printf( "%s\n", pk3Shaders + i*65 );
		}
		Sys_Printf( "\n\t+Sounds....%i\n", pk3SoundsN - pk3SoundsNold );
		for ( i = pk3SoundsNold; i < pk3SoundsN; i++ ){
			Sys_Printf( "%s\n", pk3Sounds + i*65 );
		}
		/* free bsp data */
/*
		if ( bspDrawVerts != 0 ) {
			free( bspDrawVerts );
			bspDrawVerts = NULL;
			//numBSPDrawVerts = 0;
			Sys_Printf( "freed BSPDrawVerts\n" );
		}
*/		if ( bspDrawSurfaces != 0 ) {
			free( bspDrawSurfaces );
			bspDrawSurfaces = NULL;
			//numBSPDrawSurfaces = 0;
			//Sys_Printf( "freed bspDrawSurfaces\n" );
		}
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
			epair_t *ep2free;
			for ( i = 0; i < numBSPEntities && i < numEntities; i++ ){
				ep = entities[i].epairs;
				while( ep != NULL){
					ep2free = ep;
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
*/		if ( bspShaders != 0 ) {
			free( bspShaders );
			bspShaders = NULL;
			//Sys_Printf( "freed bspShaders\n" );
			//numBSPShaders = 0;
			//allocatedBSPShaders = 0;
		}
		if ( bspEntData != 0 ) {
			free( bspEntData );
			bspEntData = NULL;
			//Sys_Printf( "freed bspEntData\n" );
			//bspEntDataSize = 0;
			//allocatedBSPEntData = 0;
		}
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



	vfsListShaderFiles( pk3Shaderfiles, &pk3ShaderfilesN );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%i\n", pk3ShaderfilesN );
		for ( i = 0; i < pk3ShaderfilesN; i++ ){
			Sys_Printf( "%s\n", pk3Shaderfiles + i*65 );
		}
	}



	/* can exclude pure *base* textures right now, shouldn't create shaders for them anyway */
	for ( i = 0; i < pk3ShadersN ; i++ ){
		for ( j = 0; j < ExPureTexturesN ; j++ ){
			if ( !Q_stricmp( pk3Shaders + i*65, ExPureTextures + j*65 ) ){
				*( pk3Shaders + i*65 ) = '\0';
				break;
			}
		}
	}
	/* can exclude repack.exclude shaders, assuming they got all their images */
	for ( i = 0; i < pk3ShadersN ; i++ ){
		for ( j = 0; j < rExShadersN ; j++ ){
			if ( !Q_stricmp( pk3Shaders + i*65, rExShaders + j*65 ) ){
				*( pk3Shaders + i*65 ) = '\0';
				break;
			}
		}
	}

	//Parse Shader Files
	Sys_Printf( "\t\nParsing shaders....\n\n" );
	char shaderText[ 8192 ];
	char* allShaders = (char *)calloc( 16777216, sizeof( char ) );
	 /* hack */
	endofscript = qtrue;

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		qboolean wantShader = qfalse;
		int shader;

		/* load the shader */
		sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
		if ( dbg ) Sys_Printf( "\n\tentering %s\n", pk3Shaderfiles + i*65 );
		SilentLoadScriptFile( temp, 0 );

		/* tokenize it */
		while ( 1 )
		{
			int line = scriptline;
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			//dump shader names
			if( dbg ) Sys_Printf( "%s\n", token );

			strcpy( shaderText, token );

			if ( strchr( token, '\\') != NULL  ){
				Sys_FPrintf( SYS_WRN, "WARNING1: %s : %s : shader name with backslash\n", pk3Shaderfiles + i*65, token );
			}

			/* do wanna le shader? */
			wantShader = qfalse;
			for ( j = 0; j < pk3ShadersN; j++ ){
				if ( !Q_stricmp( pk3Shaders + j*65, token) ){
					shader = j;
					wantShader = qtrue;
					break;
				}
			}
			if ( wantShader ){
				for ( j = 0; j < rExTexturesN ; j++ ){
					if ( !Q_stricmp( pk3Shaders + shader*65, rExTextures + j*65 ) ){
						Sys_FPrintf( SYS_WRN, "WARNING3: %s : about to include shader for excluded texture\n", pk3Shaders + shader*65 );
						break;
					}
				}
			}

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
						temp, scriptline, token, g_strLoadedFileLocation );
			}
			strcat( shaderText, "\n{" );
			qboolean hasmap = qfalse;

			while ( 1 )
			{
				line = scriptline;
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					strcat( shaderText, "\n}\n\n" );
					break;
				}
				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					qboolean tokenready = qfalse;
					strcat( shaderText, "\n\t{" );
					while ( 1 )
					{
						/* detour of TokenAvailable() '~' */
						if ( tokenready ) tokenready = qfalse;
						else line = scriptline;
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							strcat( shaderText, "\n\t}" );
							break;
						}
						if ( !strcmp( token, "{" ) ) {
							strcat( shaderText, "\n\t{" );
							Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", temp, scriptline );
						}
						/* skip the shader */
						if ( !wantShader ) continue;

						/* digest any images */
						if ( !Q_stricmp( token, "map" ) ||
							!Q_stricmp( token, "clampMap" ) ) {
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
							hasmap = qtrue;

							/* get an image */
							GetToken( qfalse );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
							}
							strcat( shaderText, " " );
							strcat( shaderText, token );
						}
						else if ( !Q_stricmp( token, "animMap" ) ||
							!Q_stricmp( token, "clampAnimMap" ) ) {
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
							hasmap = qtrue;

							GetToken( qfalse );// skip num
							strcat( shaderText, " " );
							strcat( shaderText, token );
							while ( TokenAvailable() ){
								GetToken( qfalse );
								tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
								strcat( shaderText, " " );
								strcat( shaderText, token );
							}
							tokenready = qtrue;
						}
						else if ( !Q_stricmp( token, "videoMap" ) ){
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
							hasmap = qtrue;
							GetToken( qfalse );
							strcat( shaderText, " " );
							strcat( shaderText, token );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL && strchr( token, '\\' ) == NULL ){
								sprintf( temp, "video/%s", token );
								strcpy( token, temp );
							}
							FixDOSName( token );
							for ( j = 0; j < pk3VideosN; j++ ){
								if ( !Q_stricmp( pk3Videos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < ExVideosN; j++ ){
								if ( !Q_stricmp( ExVideos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < rExVideosN; j++ ){
								if ( !Q_stricmp( rExVideos + j*65, token ) ){
									goto away;
								}
							}
							strcpy ( pk3Videos + pk3VideosN*65, token );
							pk3VideosN++;
							away:
							j = 0;
						}
						else if ( !Q_stricmp( token, "mapComp" ) || !Q_stricmp( token, "mapNoComp" ) || !Q_stricmp( token, "animmapcomp" ) || !Q_stricmp( token, "animmapnocomp" ) ){
							Sys_FPrintf( SYS_WRN, "WARNING7: %s : %s shader\n", pk3Shaders + shader*65, token );
							hasmap = qtrue;
							if ( line == scriptline ){
								strcat( shaderText, " " );
								strcat( shaderText, token );
							}
							else{
								strcat( shaderText, "\n\t\t" );
								strcat( shaderText, token );
							}
						}
						else if ( line == scriptline ){
							strcat( shaderText, " " );
							strcat( shaderText, token );
						}
						else{
							strcat( shaderText, "\n\t\t" );
							strcat( shaderText, token );
						}
					}
				}
				/* skip the shader */
				else if ( !wantShader ) continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( !Q_stricmp( token, "surfaceparm" ) ) {
					strcat( shaderText, "\n\tsurfaceparm " );
					GetToken( qfalse );
					strcat( shaderText, token );
					if ( !Q_stricmp( token, "nodraw" ) ) {
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( !Q_stricmp( token, "skyParms" ) ) {
					strcat( shaderText, "\n\tskyParms " );
					hasmap = qtrue;
					/* get image base */
					GetToken( qfalse );
					strcat( shaderText, token );

					/* ignore bogus paths */
					if ( Q_stricmp( token, "-" ) && Q_stricmp( token, "full" ) ) {
						strcpy ( temp, token );
						sprintf( token, "%s_up", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_dn", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_lf", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_rt", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_bk", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
						sprintf( token, "%s_ft", temp );
						tex2list2( pk3Textures, &pk3TexturesN, ExTextures, &ExTexturesN, rExTextures, &rExTexturesN );
					}
					/* skip rest of line */
					GetToken( qfalse );
					strcat( shaderText, " " );
					strcat( shaderText, token );
					GetToken( qfalse );
					strcat( shaderText, " " );
					strcat( shaderText, token );
				}
				else if ( !Q_strncasecmp( token, "implicit", 8 ) ){
					Sys_FPrintf( SYS_WRN, "WARNING5: %s : %s shader\n", pk3Shaders + shader*65, token );
					hasmap = qtrue;
					if ( line == scriptline ){
						strcat( shaderText, " " );
						strcat( shaderText, token );
					}
					else{
						strcat( shaderText, "\n\t" );
						strcat( shaderText, token );
					}
				}
				else if ( !Q_stricmp( token, "fogparms" ) ){
					hasmap = qtrue;
					if ( line == scriptline ){
						strcat( shaderText, " " );
						strcat( shaderText, token );
					}
					else{
						strcat( shaderText, "\n\t" );
						strcat( shaderText, token );
					}
				}
				else if ( line == scriptline ){
					strcat( shaderText, " " );
					strcat( shaderText, token );
				}
				else{
					strcat( shaderText, "\n\t" );
					strcat( shaderText, token );
				}
			}

			//exclude shader
			if ( wantShader ){
				for ( j = 0; j < ExShadersN; j++ ){
					if ( !Q_stricmp( ExShaders + j*65, pk3Shaders + shader*65 ) ){
						wantShader = qfalse;
						*( pk3Shaders + shader*65 ) = '\0';
						break;
					}
				}
				if ( wantShader && !hasmap ){
					Sys_FPrintf( SYS_WRN, "WARNING8: %s : shader has no known maps\n", pk3Shaders + shader*65 );
					wantShader = qfalse;
					*( pk3Shaders + shader*65 ) = '\0';
				}
				if ( wantShader ){
					strcat( allShaders, shaderText );
					*( pk3Shaders + shader*65 ) = '\0';
				}
			}
		}
	}
/* TODO: RTCW's mapComp, mapNoComp, animmapcomp, animmapnocomp; nocompress?; ET's implicitmap, implicitblend, implicitmask */


/* exclude stuff */

//pure textures (shader ones are done)
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( strchr( pk3Shaders + i*65, '\\') != NULL  ){
				Sys_FPrintf( SYS_WRN, "WARNING2: %s : bsp shader path with backslash\n", pk3Shaders + i*65 );
				FixDOSName( pk3Shaders + i*65 );
				//what if theres properly slashed one in the list?
				for ( j = 0; j < pk3ShadersN; j++ ){
					if ( !Q_stricmp( pk3Shaders + i*65, pk3Shaders + j*65 ) && (i != j) ){
						*( pk3Shaders + i*65 ) = '\0';
						break;
					}
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < pk3TexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, pk3Textures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < ExTexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, ExTextures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < rExTexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, rExTextures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
		}
	}

//snds
	for ( i = 0; i < pk3SoundsN; i++ ){
		for ( j = 0; j < ExSoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, ExSounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
		if ( *( pk3Sounds + i*65 ) == '\0' ) continue;
		for ( j = 0; j < rExSoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, rExSounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
	}

	/* write shader */
	sprintf( temp, "%s/%s_strippedBYrepacker.shader", EnginePath, nameOFrepack );
	FILE *f;
	f = fopen( temp, "wb" );
	fwrite( allShaders, sizeof( char ), strlen( allShaders ), f );
	fclose( f );
	Sys_Printf( "Shaders saved to %s\n", temp );

	/* make a pack */
	sprintf( packname, "%s/%s_repacked.pk3", EnginePath, nameOFrepack );
	remove( packname );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3TexturesN; i++ ){
		if ( png ){
			sprintf( temp, "%s.png", pk3Textures + i*65 );
			if ( vfsPackFile( temp, packname, compLevel ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
		}
		sprintf( temp, "%s.tga", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, compLevel ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		sprintf( temp, "%s.jpg", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname, compLevel ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Textures + i*65 );
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( png ){
				sprintf( temp, "%s.png", pk3Shaders + i*65 );
				if ( vfsPackFile( temp, packname, compLevel ) ){
					Sys_Printf( "++%s\n", temp );
					continue;
				}
			}
			sprintf( temp, "%s.tga", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, compLevel ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			sprintf( temp, "%s.jpg", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname, compLevel ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Shaders + i*65 );
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3SoundsN; i++ ){
		if ( *( pk3Sounds + i*65 ) != '\0' ){
			if ( vfsPackFile( pk3Sounds + i*65, packname, compLevel ) ){
				Sys_Printf( "++%s\n", pk3Sounds + i*65 );
				continue;
			}
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Sounds + i*65 );
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3VideosN; i++ ){
		if ( vfsPackFile( pk3Videos + i*65, packname, compLevel ) ){
			Sys_Printf( "++%s\n", pk3Videos + i*65 );
			continue;
		}
		Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", pk3Videos + i*65 );
	}

	Sys_Printf( "\nSaved to %s\n", packname );

	/* return to sender */
	return 0;
}



/*
   main()
   q3map mojo...
 */

int main( int argc, char **argv ){
	int i, r;
	double start, end;

#ifdef WIN32
	_setmaxstdio(2048);
#endif

	/* we want consistent 'randomness' */
	srand( 0 );

	/* start timer */
	start = I_FloatTime();

	/* this was changed to emit version number over the network */
	printf( Q3MAP_VERSION "\n" );

	/* set exit call */
	atexit( ExitQ3Map );

	/* read general options first */
	for ( i = 1; i < argc; i++ )
	{
		/* -help */
		if ( !strcmp( argv[ i ], "-h" ) || !strcmp( argv[ i ], "--help" )
			|| !strcmp( argv[ i ], "-help" ) ) {
			HelpMain( argv[i+1] );
			return 0;
		}

		/* -connect */
		if ( !strcmp( argv[ i ], "-connect" ) ) {
			argv[ i ] = NULL;
			i++;
			Broadcast_Setup( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* verbose */
		else if ( !strcmp( argv[ i ], "-v" ) ) {
			if ( !verbose ) {
				verbose = qtrue;
				argv[ i ] = NULL;
			}
		}

		/* force */
		else if ( !strcmp( argv[ i ], "-force" ) ) {
			force = qtrue;
			argv[ i ] = NULL;
		}

		/* patch subdivisions */
		else if ( !strcmp( argv[ i ], "-subdivisions" ) ) {
			argv[ i ] = NULL;
			i++;
			patchSubdivisions = atoi( argv[ i ] );
			argv[ i ] = NULL;
			if ( patchSubdivisions <= 0 ) {
				patchSubdivisions = 1;
			}
		}

		/* threads */
		else if ( !strcmp( argv[ i ], "-threads" ) ) {
			argv[ i ] = NULL;
			i++;
			numthreads = atoi( argv[ i ] );
			argv[ i ] = NULL;
		}

		else if( !strcmp( argv[ i ], "-nocmdline" ) )
		{
			Sys_Printf( "noCmdLine\n" );
			nocmdline = qtrue;
			argv[ i ] = NULL;
		}

	}

	/* init model library */
	PicoInit();
	PicoSetMallocFunc( safe_malloc );
	PicoSetFreeFunc( free );
	PicoSetPrintFunc( PicoPrintFunc );
	PicoSetLoadFileFunc( PicoLoadFileFunc );
	PicoSetFreeFileFunc( free );

	/* set number of threads */
	ThreadSetDefault();

	/* generate sinusoid jitter table */
	for ( i = 0; i < MAX_JITTERS; i++ )
	{
		jitters[ i ] = sin( i * 139.54152147 );
		//%	Sys_Printf( "Jitter %4d: %f\n", i, jitters[ i ] );
	}

	/* we print out two versions, q3map's main version (since it evolves a bit out of GtkRadiant)
	   and we put the GtkRadiant version to make it easy to track with what version of Radiant it was built with */

	Sys_Printf( "Q3Map         - v1.0r (c) 1999 Id Software Inc.\n" );
	Sys_Printf( "Q3Map (ydnar) - v" Q3MAP_VERSION "\n" );
	Sys_Printf( "NetRadiant    - v" RADIANT_VERSION " " __DATE__ " " __TIME__ "\n" );
	Sys_Printf( "%s\n", Q3MAP_MOTD );
	Sys_Printf( "%s\n", argv[0] );

	strcpy( q3map2path, argv[0] );//fuer autoPack func

	/* ydnar: new path initialization */
	InitPaths( &argc, argv );

	/* set game options */
	if ( !patchSubdivisions ) {
		patchSubdivisions = game->patchSubdivisions;
	}

	/* check if we have enough options left to attempt something */
	if ( argc < 2 ) {
		Error( "Usage: %s [general options] [options] mapfile\n%s -help for help", argv[ 0 ] , argv[ 0 ] );
	}

	/* fixaas */
	if ( !strcmp( argv[ 1 ], "-fixaas" ) ) {
		r = FixAAS( argc - 1, argv + 1 );
	}

	/* analyze */
	else if ( !strcmp( argv[ 1 ], "-analyze" ) ) {
		r = AnalyzeBSP( argc - 1, argv + 1 );
	}

	/* info */
	else if ( !strcmp( argv[ 1 ], "-info" ) ) {
		r = BSPInfo( argc - 2, argv + 2 );
	}

	/* vis */
	else if ( !strcmp( argv[ 1 ], "-vis" ) ) {
		r = VisMain( argc - 1, argv + 1 );
	}

	/* light */
	else if ( !strcmp( argv[ 1 ], "-light" ) ) {
		r = LightMain( argc - 1, argv + 1 );
	}

	/* vlight */
	else if ( !strcmp( argv[ 1 ], "-vlight" ) ) {
		Sys_Warning( "VLight is no longer supported, defaulting to -light -fast instead\n\n" );
		argv[ 1 ] = "-fast";    /* eek a hack */
		r = LightMain( argc, argv );
	}

	/* QBall: export entities */
	else if ( !strcmp( argv[ 1 ], "-exportents" ) ) {
		r = ExportEntitiesMain( argc - 1, argv + 1 );
	}

	/* ydnar: lightmap export */
	else if ( !strcmp( argv[ 1 ], "-export" ) ) {
		r = ExportLightmapsMain( argc - 1, argv + 1 );
	}

	/* ydnar: lightmap import */
	else if ( !strcmp( argv[ 1 ], "-import" ) ) {
		r = ImportLightmapsMain( argc - 1, argv + 1 );
	}

	/* ydnar: bsp scaling */
	else if ( !strcmp( argv[ 1 ], "-scale" ) ) {
		r = ScaleBSPMain( argc - 1, argv + 1 );
	}

	/* bsp shifting */
	else if ( !strcmp( argv[ 1 ], "-shift" ) ) {
		r = ShiftBSPMain( argc - 1, argv + 1 );
	}

	/* autopacking */
	else if ( !strcmp( argv[ 1 ], "-pk3" ) ) {
		r = pk3BSPMain( argc - 1, argv + 1 );
	}

	/* repacker */
	else if ( !strcmp( argv[ 1 ], "-repack" ) ) {
		r = repackBSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: bsp conversion */
	else if ( !strcmp( argv[ 1 ], "-convert" ) ) {
		r = ConvertBSPMain( argc - 1, argv + 1 );
	}

	/* div0: minimap */
	else if ( !strcmp( argv[ 1 ], "-minimap" ) ) {
		r = MiniMapBSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: otherwise create a bsp */
	else{
		r = BSPMain( argc, argv );
	}

	/* emit time */
	end = I_FloatTime();
	Sys_Printf( "%9.0f seconds elapsed\n", end - start );

	/* return any error code */
	return r;
}

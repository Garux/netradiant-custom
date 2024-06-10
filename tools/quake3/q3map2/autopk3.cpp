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
#include "shaders.h"
#include <map>


using StrList = std::vector<String64>;

inline const String64 *StrList_find( const StrList& list, const char* string ){
	for ( auto&& s : list ){
		if ( striEqual( s, string ) )
			return &s;
	}
	return nullptr;
}
inline String64 *StrList_find( StrList& list, const char* string ){
	return const_cast<String64*>( StrList_find( const_cast<const StrList&>( list ), string ) );
}


/*
	Append newcoming texture, if it is unique and not excluded
*/
static inline void tex2list( StrList& texlist, const StrList& EXtex, const StrList* rEXtex ){
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
	      !StrList_find( *rEXtex, token ) ) ){
		texlist.emplace_back( token );
	}
	strcpy( dot, ".tga" ); // default extension for repacked shader text
}


/*
	Append newcoming resource, if it is unique
*/
inline void res2list( StrList& list, const char* res ){
	while ( path_separator( *res ) ){ // kill prepended slashes
		++res;
	}
	if( !strEmpty( res ) && !StrList_find( list, res ) )
		list.emplace_back( res );
}


static void parseBspFile( const char *bspPath, StrList& outShaders, StrList& outSounds, bool print ){
	const char *str_p;
	StringOutputStream stream( 256 );
	StrList pk3Shaders;
	StrList pk3Sounds;


	/* load the bsp */
	Sys_Printf( "Loading %s\n", bspPath );
	LoadBSPFilePartially( bspPath );
	ParseEntities();

	{ /* add visible bspShaders */
		std::vector<bool> drawsurfSHs( bspShaders.size(), false );

		for ( const bspDrawSurface_t& surf : bspDrawSurfaces ){
			drawsurfSHs[ surf.shaderNum ] = true;
		}

		for ( size_t i = 0; i < bspShaders.size(); ++i ){
			if ( drawsurfSHs[i] && !( bspShaders[i].surfaceFlags & GetRequiredSurfaceParm<"nodraw">().surfaceFlags ) ){ // also sort out nodraw patches
				res2list( pk3Shaders, bspShaders[i].shader );
			}
		}
	}
	/* Ent keys */
	for ( const auto& ep : entities[0].epairs )
	{
		if ( striEqualPrefix( ep.key.c_str(), "vertexremapshader" ) ) {
			char strbuf[ 1024 ];
			if( 1 == sscanf( ep.value.c_str(), "%*[^;] %*[;] %s", strbuf ) ) // textures/remap/from;textures/remap/to
				res2list( pk3Shaders, strbuf );
		}
	}

	if ( entities[ 0 ].read_keyvalue( str_p, "music" ) ){
		res2list( pk3Sounds, stream( PathDefaultExtension( PathCleaned( str_p ), ".wav" ) ) );
	}

	for ( const auto& e : entities )
	{
		if ( e.read_keyvalue( str_p, "noise" ) && str_p[0] != '*' ){
			res2list( pk3Sounds, stream( PathDefaultExtension( PathCleaned( str_p ), ".wav" ) ) );
		}
		/* these 3 sound files are missing in vanilla bundle */
		if ( e.classname_is( "func_plat" ) ){
			res2list( pk3Sounds, "sound/movers/plats/pt1_strt.wav" );
			res2list( pk3Sounds, "sound/movers/plats/pt1_end.wav" );
		}
		if ( e.classname_is( "target_push" ) ){
			if ( !( e.intForKey( "spawnflags") & 1 ) ){
				res2list( pk3Sounds, "sound/misc/windfly.wav" );
			}
		}
		res2list( pk3Shaders, e.valueForKey( "targetShaderNewName" ) );

		if ( e.read_keyvalue( str_p, "model2" ) ){
			Sys_Warning( "unhandled model2 key of %s: %s\n", e.classname(), str_p );
		}
	}

	for( const bspFog_t& fog : bspFogs ){
		res2list( pk3Shaders, fog.shader );
	}

	//levelshot
	res2list( pk3Shaders, stream( "levelshots/", PathFilename( bspPath ) ) );


	if( print ){
		Sys_Printf( "\n\tDrawsurface+ent calls....%zu\n", pk3Shaders.size() );
		for ( const auto& s : pk3Shaders ){
			Sys_Printf( "%s\n", s.c_str() );
		}
		Sys_Printf( "\n\tSounds....%zu\n", pk3Sounds.size() );
		for ( const auto& s : pk3Sounds ){
			Sys_Printf( "%s\n", s.c_str() );
		}
	}

	/* merge to out lists */
	for( const auto& s : pk3Shaders )
		res2list( outShaders, s );
	for( const auto& s : pk3Sounds )
		res2list( outSounds, s );
}



struct Exclusions
{
	StrList textures;
	StrList shaders;
	StrList shaderfiles;
	StrList sounds;
	StrList videos;
	StrList pureTextures;

	void print() const {
		Sys_Printf( "\n\tExTextures....%zu\n", textures.size() );
		for ( const auto& s : textures )
			Sys_Printf( "%s\n", s.c_str() );
		Sys_Printf( "\n\tExPureTextures....%zu\n", pureTextures.size() );
		for ( const auto& s : pureTextures )
			Sys_Printf( "%s\n", s.c_str() );
		Sys_Printf( "\n\tExShaders....%zu\n", shaders.size() );
		for ( const auto& s : shaders )
			Sys_Printf( "%s\n", s.c_str() );
		Sys_Printf( "\n\tExShaderfiles....%zu\n", shaderfiles.size() );
		for ( const auto& s : shaderfiles )
			Sys_Printf( "%s\n", s.c_str() );
		Sys_Printf( "\n\tExSounds....%zu\n", sounds.size() );
		for ( const auto& s : sounds )
			Sys_Printf( "%s\n", s.c_str() );
		Sys_Printf( "\n\tExVideos....%zu\n", videos.size() );
		for ( const auto& s : videos )
			Sys_Printf( "%s\n", s.c_str() );

	}
};

static inline void parseEXblock( StrList& list, const char *exName ){
	if ( !( GetToken( true ) && strEqual( token, "{" ) ) ) {
		Error( "ReadExclusionsFile: %s, line %d: { not found", exName, scriptline );
	}
	while ( GetToken( true ) && !strEqual( token, "}" ) )
	{
		if ( strEqual( token, "{" ) ) {
			Error( "ReadExclusionsFile: %s, line %d: brace, opening twice in a row.", exName, scriptline );
		}

		/* add to list */
		list.emplace_back( token );
	}
	return;
}

static const Exclusions parseEXfile( const char* filename ){
	Exclusions ex;
	if ( LoadScriptFile( filename, -1 ) ) {
		/* tokenize it */
		while ( GetToken( true ) ) /* test for end of file */
		{
			/* blocks */
			if ( striEqual( token, "textures" ) ){
				parseEXblock( ex.textures, filename );
			}
			else if ( striEqual( token, "shaders" ) ){
				parseEXblock( ex.shaders, filename );
			}
			else if ( striEqual( token, "shaderfiles" ) ){
				parseEXblock( ex.shaderfiles, filename );
			}
			else if ( striEqual( token, "sounds" ) ){
				parseEXblock( ex.sounds, filename );
			}
			else if ( striEqual( token, "videos" ) ){
				parseEXblock( ex.videos, filename );
			}
			else{
				Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", filename, scriptline );
			}
		}

		/* prepare ex.pureTextures */
		for ( const auto& s : ex.textures ){
			if( !StrList_find( ex.shaders, s ) )
				ex.pureTextures.emplace_back( s );
		}
	}
	else{
		Sys_Warning( "Unable to load exclusions file %s.\n", filename );
	}

	return ex;
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




/*
   pk3BSPMain()
   map autopackager, works for Q3 type of shaders and ents
 */

int pk3BSPMain( Args& args ){
	int compLevel = 9; // MZ_BEST_COMPRESSION; MZ_UBER_COMPRESSION : not zlib compatible, and may be very slow
	bool dbg = false, png = false, packFAIL = false;
	StringOutputStream stream( 256 );

	/* process arguments */
	const char *fileName = args.takeBack();
	{
		if ( args.takeArg( "-dbg" ) ) {
			dbg = true;
		}
		if ( args.takeArg( "-png" ) ) {
			png = true;
		}
		if ( args.takeArg( "-complevel" ) ) {
			compLevel = std::clamp( atoi( args.takeNext() ), -1, 10 );
			Sys_Printf( "Compression level set to %i\n", compLevel );
		}
	}

	/* extract pack name */
	const CopiedString nameOFpack( PathFilename( fileName ) );

	std::vector<CopiedString> bspList; // absolute bsp paths

	while( !args.empty() ){ // handle multiple bsps input
		bspList.emplace_back( stream( PathExtensionless( ExpandArg( args.takeFront() ) ), ".bsp" ) );
	}
	bspList.emplace_back( stream( PathExtensionless( ExpandArg( fileName ) ), ".bsp" ) );

	/* parse bsps */
	StrList pk3Shaders;
	StrList pk3Sounds;
	std::vector<CopiedString> pk3Shaderfiles;
	StrList pk3Textures;
	StrList pk3Videos;

	for( const auto& bsp : bspList ){
		parseBspFile( bsp.c_str(), pk3Shaders, pk3Sounds, dbg );
	}

	pk3Shaderfiles = vfsListShaderFiles( g_game->shaderPath );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%zu\n", pk3Shaderfiles.size() );
		for ( const CopiedString& file : pk3Shaderfiles ){
			Sys_Printf( "%s\n", file.c_str() );
		}
	}

	/* load exclusions file */
	const Exclusions ex = parseEXfile( stream( PathFilenameless( args.getArg0() ), g_game->arg, ".exclude" ) );
	/* key is pointer to pk3Shaders entry, thus latter shall not be reallocated! */
	std::map<const String64*, CopiedString> ExReasonShader;
	std::map<const String64*, CopiedString> ExReasonShaderFile;

	if( dbg )
		ex.print();

	/* can exclude pure textures right now, shouldn't create shaders for them anyway */
	for ( auto& s : pk3Shaders ){
		if( StrList_find( ex.pureTextures, s ) )
			s.clear();
	}

	//Parse Shader Files
	for ( CopiedString& file : pk3Shaderfiles ){
		bool wantShaderFile = false;
		const String64 *excludedByShader = nullptr;
		/* do wanna le shader file? */
		const bool excludedByShaderFile = StrList_find( ex.shaderfiles, file.c_str() );

		/* load the shader */
		const auto scriptFile = stream( g_game->shaderPath, '/', file );

		/* check if shader file has to be excluded */
		if( !excludedByShaderFile ){
			/* tokenize it */
			LoadScriptFile( scriptFile, 0, false );
			while ( GetToken( true ) )
			{
				/* does it contain restricted shaders/textures? */
				if( ( excludedByShader = StrList_find( ex.shaders, token ) )
				 || ( excludedByShader = StrList_find( ex.pureTextures, token ) ) ){
					break;
				}

				/* handle { } section */
				if ( !( GetToken( true ) && strEqual( token, "{" ) ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
					       scriptFile.c_str(), scriptline, token, g_strLoadedFileLocation );
				}

				while ( GetToken( true ) && !strEqual( token, "}" ) )
				{
					/* parse stage directives */
					if ( strEqual( token, "{" ) ) {
						while ( GetToken( true ) && !strEqual( token, "}" ) )
						{
						}
					}
				}
			}
		}

		/* tokenize it again */
		LoadScriptFile( scriptFile, 0, dbg );
		while ( GetToken( true ) )
		{
			//dump shader names
			if( dbg )
				Sys_Printf( "%s\n", token );

			/* do wanna le shader? */
			String64 *wantShader = StrList_find( pk3Shaders, token );

			/* handle { } section */
			if ( !( GetToken( true ) && strEqual( token, "{" ) ) ) {
				Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
				       scriptFile.c_str(), scriptline, token, g_strLoadedFileLocation );
			}

			bool hasmap = false;
			while ( GetToken( true ) && !strEqual( token, "}" ) )
			{

				/* -----------------------------------------------------------------
				shader stages (passes)
				----------------------------------------------------------------- */

				/* parse stage directives */
				if ( strEqual( token, "{" ) ) {
					while ( GetToken( true ) && !strEqual( token, "}" ) )
					{
						if ( strEqual( token, "{" ) ) {
							Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", scriptFile.c_str(), scriptline );
						}
						if ( striEqual( token, "mapComp" ) || striEqual( token, "mapNoComp" ) || striEqual( token, "animmapcomp" ) || striEqual( token, "animmapnocomp" ) ){
							Sys_FPrintf( SYS_WRN, "WARNING7: %s : line %d : unsupported '%s' map directive\n", scriptFile.c_str(), scriptline, token );
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
								tex2list( pk3Textures, ex.textures, NULL );
							}
						}
						else if ( striEqual( token, "animMap" ) ||
						          striEqual( token, "clampAnimMap" ) ) {
							hasmap = true;
							GetToken( false );// skip num
							while ( TokenAvailable() ){
								GetToken( false );
								tex2list( pk3Textures, ex.textures, NULL );
							}
						}
						else if ( striEqual( token, "videoMap" ) ){
							hasmap = true;
							GetToken( false );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL ){
								strcpy( token, stream( "video/", token ) );
							}
							if( !StrList_find( pk3Videos, token ) &&
							    !StrList_find( ex.videos, token ) )
								pk3Videos.emplace_back( token );
						}
					}
				}
				else if ( striEqualPrefix( token, "implicit" ) ){
					Sys_FPrintf( SYS_WRN, "WARNING5: %s : line %d : unsupported %s shader\n", scriptFile.c_str(), scriptline, token );
				}
				/* skip the shader */
				else if ( !wantShader )
					continue;

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( striEqual( token, "skyParms" ) ) {
					hasmap = true;
					/* get image base */
					GetToken( false );

					/* ignore bogus paths */
					if ( !strEqual( token, "-" ) && !striEqual( token, "full" ) ) {
						char* const skysidestring = token + strcatQ( token, "_@@.tga", std::size( token ) ) - 6;
						for( const auto side : { "up", "dn", "lf", "rt", "bk", "ft" } ){
							memcpy( skysidestring, side, 2 );
							tex2list( pk3Textures, ex.textures, NULL );
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
				if( StrList_find( ex.shaders, *wantShader ) ){
					wantShader->clear();
					wantShader = nullptr;
				}
				if ( wantShader && !hasmap ){
					Sys_FPrintf( SYS_WRN, "WARNING8: %s : visible shader has no known maps\n", wantShader->c_str() );
					wantShader = nullptr;
				}
				if ( wantShader ){
					if ( excludedByShader || excludedByShaderFile ){
						ExReasonShaderFile[ wantShader ] = file;
						if( excludedByShader != nullptr ){
							ExReasonShader[ wantShader ] = excludedByShader->c_str();
						}
					}
					else{
						wantShaderFile = true;
						wantShader->clear();
					}
				}
			}
		}
		if ( !wantShaderFile ){
			file = "";
		}
	}



	/* exclude stuff */
//wanted shaders from excluded .shaders
	Sys_Printf( "\n" );
	for ( auto& s : pk3Shaders ){
		if ( !s.empty() && ( ExReasonShader.contains( &s ) || ExReasonShaderFile.contains( &s ) ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
			packFAIL = true;
			if ( ExReasonShader.contains( &s ) ){
				Sys_Printf( "     reason: is located in %s,\n     containing restricted shader %s\n", ExReasonShaderFile[&s].c_str(), ExReasonShader[&s].c_str() );
			}
			else{
				Sys_Printf( "     reason: is located in restricted %s\n", ExReasonShaderFile[&s].c_str() );
			}
			s.clear();
		}
	}
//pure textures (shader ones are done)
	for ( auto& s : pk3Shaders ){
		if ( !s.empty() ){
			s = stream( PathCleaned( s ) );
			if( StrList_find( pk3Textures, s ) ||
			    StrList_find( ex.textures, s ) )
				s.clear();
		}
	}

//snds
	for ( auto& s : pk3Sounds ){
		if( StrList_find( ex.sounds, s ) )
			s.clear();
	}

	/* make a pack */
	const auto packname = stream( g_enginePath, nameOFpack, "_autopacked.pk3" );
	remove( packname );
	const auto packFailName = stream( g_enginePath, nameOFpack, "_FAILEDpack.pk3" );
	remove( packFailName );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( const auto& s : pk3Textures ){
		if( !packTexture( s, packname, compLevel, png ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
			packFAIL = true;
		}
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( const auto& s : pk3Shaders ){
		if ( !s.empty() ){
			if( !packTexture( s, packname, compLevel, png ) ){
				if ( &s == &pk3Shaders.back() ){ //levelshot typically
					Sys_Printf( "  ~fail  %s\n", s.c_str() );
				}
				else{
					Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
					packFAIL = true;
				}
			}
		}
	}

	Sys_Printf( "\n\tShaizers....\n" );

	for ( CopiedString& file : pk3Shaderfiles ){
		if ( !file.empty() ){
			stream( g_game->shaderPath, '/', file );
			if ( !packResource( stream, packname, compLevel ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", stream.c_str() );
				packFAIL = true;
			}
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( const auto& s : pk3Sounds ){
		if ( !s.empty() ){
			if ( !packResource( s, packname, compLevel ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
				packFAIL = true;
			}
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( const auto& s : pk3Videos ){
		if ( !packResource( s, packname, compLevel ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
			packFAIL = true;
		}
	}

	Sys_Printf( "\n\t.bsp and stuff\n" );

	for( const auto& bsp : bspList ){
		const auto mapname = PathFilename( bsp.c_str() );
		stream( "maps/", mapname, ".bsp" );
		//if ( vfsPackFile( stream, packname, compLevel ) ){
		if ( vfsPackFile_Absolute_Path( bsp.c_str(), stream, packname, compLevel ) ){
			Sys_Printf( "++%s\n", stream.c_str() );
		}
		else{
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", stream.c_str() );
			packFAIL = true;
		}

		stream( "maps/", mapname, ".aas" );
		if ( !packResource( stream, packname, compLevel ) )
			Sys_Printf( "  ~fail  %s\n", stream.c_str() );

		stream( "scripts/", mapname, ".arena" );
		if ( !packResource( stream, packname, compLevel ) )
			Sys_Printf( "  ~fail  %s\n", stream.c_str() );

		stream( "scripts/", mapname, ".defi" );
		if ( !packResource( stream, packname, compLevel ) )
			Sys_Printf( "  ~fail  %s\n", stream.c_str() );
	}

	if ( !packFAIL ){
		Sys_Printf( "\nSaved to %s\n", packname.c_str() );
	}
	else{
		rename( packname, packFailName );
		Sys_Printf( "\nSaved to %s\n", packFailName.c_str() );
	}
	/* return to sender */
	return 0;
}


/*
   repackBSPMain()
   repack multiple maps, strip out only required shaders
   works for Q3 type of shaders and ents
 */

int repackBSPMain( Args& args ){
	int compLevel = 1; // MZ_BEST_SPEED
	bool dbg = false, png = false, analyze = false;
	StringOutputStream stream( 256 );
	/* process arguments */
	const char *fileName = args.takeBack();
	{
		if ( args.takeArg( "-dbg" ) ) {
			dbg = true;
		}
		if ( args.takeArg( "-png" ) ) {
			png = true;
		}
		if ( args.takeArg( "-analyze" ) ) { // only analyze bsps and exit
			analyze = true;
		}
		if ( args.takeArg( "-complevel" ) ) {
			compLevel = std::clamp( atoi( args.takeNext() ), -1, 10 );
			Sys_Printf( "Compression level set to %i\n", compLevel );
		}
	}

	/* extract pack name */
	const CopiedString nameOFpack( PathFilename( fileName ) );

	/* load exclusions file */
	const Exclusions ex = parseEXfile( stream( PathFilenameless( args.getArg0() ), g_game->arg, ".exclude" ) );

	if( dbg )
		ex.print();

	/* load repack.exclude */ /* rex.pureTextures & rex.shaderfiles wont be used */
	const Exclusions rex = parseEXfile( stream( PathFilenameless( args.getArg0() ), "repack.exclude" ) );

	if( dbg )
		rex.print();


	std::vector<CopiedString> bspList; // absolute bsp paths

	if ( path_extension_is( fileName, "bsp" ) ){
		while( !args.empty() ){ // handle multiple bsps input
			bspList.emplace_back( stream( PathExtensionless( ExpandArg( args.takeFront() ) ), ".bsp" ) );
		}
		bspList.emplace_back( stream( PathExtensionless( ExpandArg( fileName ) ), ".bsp" ) );
	}
	else{
		/* load bsps paths list */
		/* do some path mangling */
		strcpy( source, ExpandArg( fileName ) );
		if ( !LoadScriptFile( source, -1 ) ) {
			Error( "Unable to open bsps paths list file %s.\n", source );
		}

		/* tokenize it */
		while ( GetToken( true ) )
		{
			bspList.emplace_back( stream( PathExtensionless( token ), ".bsp" ) );
		}
	}

	/* parse bsps */
	StrList pk3Shaders;
	StrList pk3Sounds;
	std::vector<CopiedString> pk3Shaderfiles;
	StrList pk3Textures;
	StrList pk3Videos;


	for( const auto& bsp : bspList ){
		parseBspFile( stream( PathExtensionless( bsp.c_str() ), ".bsp" ), pk3Shaders, pk3Sounds, true );
	}

	if( analyze )
		return 0;



	pk3Shaderfiles = vfsListShaderFiles( g_game->shaderPath );

	if( dbg ){
		Sys_Printf( "\n\tSchroider fileses.....%zu\n", pk3Shaderfiles.size() );
		for ( const CopiedString& file : pk3Shaderfiles ){
			Sys_Printf( "%s\n", file.c_str() );
		}
	}



	/* can exclude pure *base* textures right now, shouldn't create shaders for them anyway */
	for ( auto& s : pk3Shaders ){
		if( StrList_find( ex.pureTextures, s ) )
			s.clear();
	}
	/* can exclude repack.exclude shaders, assuming they got all their images */
	for ( auto& s : pk3Shaders ){
		if( StrList_find( rex.shaders, s ) )
			s.clear();
	}

	//Parse Shader Files
	Sys_Printf( "\t\nParsing shaders....\n\n" );
	ShaderTextCollector text;
	StringOutputStream allShaders( 1048576 );

	for ( const CopiedString& file : pk3Shaderfiles ){
		/* load the shader */
		const auto scriptFile = stream( g_game->shaderPath, '/', file );
		LoadScriptFile( scriptFile, 0, dbg );

		/* tokenize it */
		while ( text.GetToken( true ) ) /* test for end of file */
		{
			//dump shader names
			if( dbg )
				Sys_Printf( "%s\n", token );

			if ( strchr( token, '\\') != NULL  ){
				Sys_FPrintf( SYS_WRN, "WARNING1: %s : %s : shader name with backslash\n", file.c_str(), token );
			}

			/* do wanna le shader? */
			String64 *wantShader = StrList_find( pk3Shaders, token );

			/* handle { } section */
			if ( !( text.GetToken( true ) && strEqual( token, "{" ) ) ) {
				Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
				       scriptFile.c_str(), scriptline, token, g_strLoadedFileLocation );
			}

			bool hasmap = false;

			while ( text.GetToken( true ) && !strEqual( token, "}" ) )
			{
				/* parse stage directives */
				if ( strEqual( token, "{" ) ) {
					while ( text.GetToken( true ) && !strEqual( token, "}" ) )
					{
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
								tex2list( pk3Textures, ex.textures, &rex.textures );
							}
							text.tokenAppend(); // append token, modified by tex2list()
						}
						else if ( striEqual( token, "animMap" ) ||
						          striEqual( token, "clampAnimMap" ) ) {
							hasmap = true;

							text.GetToken( false );// skip num
							while ( TokenAvailable() ){
								tex2list( pk3Textures, ex.textures, &rex.textures );
								text.GetToken( false ); // append token, modified by tex2list()
							}
						}
						else if ( striEqual( token, "videoMap" ) ){
							hasmap = true;
							text.GetToken( false );
							FixDOSName( token );
							if ( strchr( token, '/' ) == NULL ){
								strcpy( token, stream( "video/", token ) );
							}
							if( !StrList_find( pk3Videos, token ) &&
							    !StrList_find( ex.videos, token ) &&
							    !StrList_find( rex.videos, token ) )
								pk3Videos.emplace_back( token );
						}
						else if ( striEqual( token, "mapComp" ) || striEqual( token, "mapNoComp" ) || striEqual( token, "animmapcomp" ) || striEqual( token, "animmapnocomp" ) ){
							Sys_FPrintf( SYS_WRN, "WARNING7: %s : %s shader\n", wantShader->c_str(), token );
							hasmap = true;
						}
					}
				}
				/* skip the shader */
				else if ( !wantShader )
					continue;

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( striEqual( token, "skyParms" ) ) {
					hasmap = true;
					/* get image base */
					text.GetToken( false );

					/* ignore bogus paths */
					if ( !strEqual( token, "-" ) && !striEqual( token, "full" ) ) {
						char* const skysidestring = token + strcatQ( token, "_@@.tga", std::size( token ) ) - 6;
						for( const auto side : { "up", "dn", "lf", "rt", "bk", "ft" } ){
							memcpy( skysidestring, side, 2 );
							tex2list( pk3Textures, ex.textures, &rex.textures );
						}
					}
					/* skip rest of line */
					text.GetToken( false );
					text.GetToken( false );
				}
				else if ( striEqualPrefix( token, "implicit" ) ){
					Sys_FPrintf( SYS_WRN, "WARNING5: %s : %s shader\n", wantShader->c_str(), token );
					hasmap = true;
				}
				else if ( striEqual( token, "fogparms" ) ){
					hasmap = true;
				}
			}

			//exclude shader
			if ( wantShader ){
				if( StrList_find( ex.shaders, *wantShader ) ){
					wantShader->clear();
					wantShader = nullptr;
				}
				if( wantShader && StrList_find( rex.textures, *wantShader ) )
					Sys_FPrintf( SYS_WRN, "WARNING3: %s : about to include shader for excluded texture\n", wantShader->c_str() );
				if ( wantShader && !hasmap ){
					Sys_FPrintf( SYS_WRN, "WARNING8: %s : visible shader has no known maps\n", wantShader->c_str() );
					wantShader = nullptr;
				}
				if ( wantShader ){
					allShaders << text.text << '\n';
					wantShader->clear();
				}
			}
			/* reset collector */
			text.clear();
		}
	}
	/* TODO: RTCW's mapComp, mapNoComp, animmapcomp, animmapnocomp; nocompress?; ET's implicitmap, implicitblend, implicitmask */


	/* exclude stuff */

//pure textures (shader ones are done)
	for ( auto& s : pk3Shaders ){
		if ( !s.empty() ){
			if ( strchr( s, '\\') != NULL  ){
				Sys_FPrintf( SYS_WRN, "WARNING2: %s : bsp shader path with backslash\n", s.c_str() );
				s = stream( PathCleaned( s ) );
				//what if theres properly slashed one in the list?
				for ( const auto& ss : pk3Shaders ){
					if ( striEqual( s, ss ) && ( &s != &ss ) ){
						s.clear();
						break;
					}
				}
			}
			if ( !s.empty() ){
				if( StrList_find( pk3Textures, s ) ||
				    StrList_find( ex.textures, s ) ||
				    StrList_find( rex.textures, s ) )
					s.clear();
			}
		}
	}

//snds
	for ( auto& s : pk3Sounds ){
		if( StrList_find( ex.sounds, s ) ||
		    StrList_find( rex.sounds, s ) )
			s.clear();
	}

	/* write shader */
	stream( g_enginePath, nameOFpack, "_strippedBYrepacker.shader" );
	SaveFile( stream, allShaders, allShaders.cend() - allShaders.cbegin() );
	Sys_Printf( "Shaders saved to %s\n", stream.c_str() );

	/* make a pack */
	stream( g_enginePath, nameOFpack, "_repacked.pk3" );
	remove( stream );

	Sys_Printf( "\n--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( const auto& s : pk3Textures ){
		if( !packTexture( s, stream, compLevel, png ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
		}
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( const auto& s : pk3Shaders ){
		if ( !s.empty() ){
			if( !packTexture( s, stream, compLevel, png ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
			}
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( const auto& s : pk3Sounds ){
		if ( !s.empty() ){
			if ( !packResource( s, stream, compLevel ) ){
				Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
			}
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( const auto& s : pk3Videos ){
		if ( !packResource( s, stream, compLevel ) ){
			Sys_FPrintf( SYS_WRN, "  !FAIL! %s\n", s.c_str() );
		}
	}

	Sys_Printf( "\nSaved to %s\n", stream.c_str() );

	/* return to sender */
	return 0;
}


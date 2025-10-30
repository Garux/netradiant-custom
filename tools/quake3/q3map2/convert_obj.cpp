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
   ConvertSurface()
   converts a bsp drawsurface to an obj chunk
 */

static int firstLightmap = 0;
static int lastLightmap = -1;

static int objVertexCount = 0;
static int objLastShaderNum = -1;

static void ConvertSurfaceToOBJ( FILE *f, int modelNum, int surfaceNum, const Vector3& origin, const std::vector<int>& lmIndices ){
	const bspDrawSurface_t& ds = bspDrawSurfaces[ surfaceNum ];

	/* ignore patches for now */
	if ( ds.surfaceType != MST_PLANAR && ds.surfaceType != MST_TRIANGLE_SOUP ) {
		return;
	}

	fprintf( f, "g mat%dmodel%dsurf%d\r\n", ds.shaderNum, modelNum, surfaceNum );
	switch ( ds.surfaceType )
	{
	case MST_PLANAR:
		fprintf( f, "# SURFACETYPE MST_PLANAR\r\n" );
		break;
	case MST_TRIANGLE_SOUP:
		fprintf( f, "# SURFACETYPE MST_TRIANGLE_SOUP\r\n" );
		break;
	}

	/* export shader */
	if ( lightmapsAsTexcoord ) {
		const int lmNum = ds.lightmapNum[0] >= 0? ds.lightmapNum[0]: lmIndices[ds.shaderNum] >= 0? lmIndices[ds.shaderNum] : ds.lightmapNum[0];
		if ( objLastShaderNum != lmNum ) {
			fprintf( f, "usemtl lm_%04d\r\n", lmNum + deluxemap );
			objLastShaderNum = lmNum + deluxemap;
		}
		if ( lmNum + (int)deluxemap < firstLightmap ) {
			Sys_Warning( "lightmap %d out of range (exporting anyway)\n", lmNum + deluxemap );
			firstLightmap = lmNum + deluxemap;
		}
		if ( lmNum > lastLightmap ) {
			Sys_Warning( "lightmap %d out of range (exporting anyway)\n", lmNum + deluxemap );
			lastLightmap = lmNum + deluxemap;
		}
	}
	else
	{
		if ( objLastShaderNum != ds.shaderNum ) {
			fprintf( f, "usemtl %s\r\n", bspShaders[ds.shaderNum].shader );
			objLastShaderNum = ds.shaderNum;
		}
	}

	/* export vertex */
	for ( int i = 0; i < ds.numVerts; ++i )
	{
		const bspDrawVert_t& dv = bspDrawVerts[ ds.firstVert + i ];
		fprintf( f, "# vertex %d\r\n", i + objVertexCount + 1 );
		fprintf( f, "v %f %f %f\r\n", dv.xyz[ 0 ], dv.xyz[ 2 ], -dv.xyz[ 1 ] );
		fprintf( f, "vn %f %f %f\r\n", dv.normal[ 0 ], dv.normal[ 2 ], -dv.normal[ 1 ] );
		if ( lightmapsAsTexcoord ) {
			fprintf( f, "vt %f %f\r\n", dv.lightmap[0][0], ( 1.0 - dv.lightmap[0][1] ) ); // dv.lightmap[0][1] internal, ( 1.0 - dv.lightmap[0][1] ) external
		}
		else{
			fprintf( f, "vt %f %f\r\n", dv.st[ 0 ], ( 1.0 - dv.st[ 1 ] ) );
		}
	}

	/* export faces */
	for ( int i = 0; i < ds.numIndexes; i += 3 )
	{
		const int a = bspDrawIndexes[ i + ds.firstIndex ];
		const int c = bspDrawIndexes[ i + ds.firstIndex + 1 ];
		const int b = bspDrawIndexes[ i + ds.firstIndex + 2 ];
		fprintf( f, "f %d/%d/%d %d/%d/%d %d/%d/%d\r\n",
		         a + objVertexCount + 1, a + objVertexCount + 1, a + objVertexCount + 1,
		         b + objVertexCount + 1, b + objVertexCount + 1, b + objVertexCount + 1,
		         c + objVertexCount + 1, c + objVertexCount + 1, c + objVertexCount + 1
		       );
	}

	objVertexCount += ds.numVerts;
}



/*
   ConvertModel()
   exports a bsp model to an ase chunk
 */

static void ConvertModelToOBJ( FILE *f, int modelNum, const Vector3& origin, const std::vector<int>& lmIndices ){
	const bspModel_t& model = bspModels[ modelNum ];

	/* go through each drawsurf in the model */
	for ( int i = 0; i < model.numBSPSurfaces; ++i )
	{
		ConvertSurfaceToOBJ( f, modelNum, model.firstBSPSurface + i, origin, lmIndices );
	}
}



/*
   ConvertShader()
   exports a bsp shader to an ase chunk
 */

static void ConvertShaderToMTL( FILE *f, const bspShader_t& shader ){
	/* get shader */
	shaderInfo_t& si = ShaderInfoForShader( shader.shader );

	/* set bitmap filename */
	auto filename = si.shaderImage->filename.c_str()[ 0 ] == '*'
	                ? StringStream<64>( si.shader, ".tga" )
	                : StringStream<64>( si.shaderImage->filename );

	/* blender hates this, so let's not do it
	for( char *c = filename; *c; ++c )
		if( *c == '/' )
			*c = '\\';
	*/

	/* print shader info */
	fprintf( f, "newmtl %s\r\n", shader.shader );
	fprintf( f, "Kd %f %f %f\r\n", si.color[ 0 ], si.color[ 1 ], si.color[ 2 ] );
	if ( shadersAsBitmap ) {
		fprintf( f, "map_Kd %s\r\n", shader.shader );
	}
	else{
		/* blender hates this, so let's not do it
		    fprintf( f, "map_Kd ..\\%s\r\n", filename );
		 */
		fprintf( f, "map_Kd ../%s\r\n", filename.c_str() );
	}
}

static void ConvertLightmapToMTL( FILE *f, const char *base, int lightmapNum ){
	/* print shader info */
	fprintf( f, "newmtl lm_%04d\r\n", lightmapNum );
	if ( lightmapNum >= 0 ) {
		/* blender hates this, so let's not do it
		    fprintf( f, "map_Kd %s\\" EXTERNAL_LIGHTMAP "\r\n", base, lightmapNum );
		 */
		if( shadersAsBitmap )
			fprintf( f, "map_Kd maps/%s/" EXTERNAL_LIGHTMAP "\r\n", base, lightmapNum );
		else
			fprintf( f, "map_Kd %s/" EXTERNAL_LIGHTMAP "\r\n", base, lightmapNum );
	}
}


int Convert_CountLightmaps( const char* dirname ){
	int lightmapCount;
	//FIXME numBSPLightmaps is 0, must be bspLightBytes / ( g_game->lightmapSize * g_game->lightmapSize * 3 )
	for ( lightmapCount = 0; lightmapCount < numBSPLightmaps; ++lightmapCount )
		;
	for ( ; ; ++lightmapCount )
	{
		char buf[1024];
		std::snprintf( buf, std::size( buf ), "%s/" EXTERNAL_LIGHTMAP, dirname, lightmapCount );
		if ( !FileExists( buf ) ) {
			break;
		}
	}
	return lightmapCount;
}

/* manage external lms, possibly referenced by q3map2_%mapname%.shader */
void Convert_ReferenceLightmaps( const char* base, std::vector<int>& lmIndices ){
	char shaderfile[256];
	sprintf( shaderfile, "%s/q3map2_%s.shader", g_game->shaderPath, base );
	LoadScriptFile( shaderfile );
	/* tokenize it */
	while ( GetToken( true ) ) /* test for end of file */
	{
		char shadername[256];
		strcpy( shadername, token );

		/* handle { } section */
		if ( !( GetToken( true ) && strEqual( token, "{" ) ) )
			Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
			       shaderfile, scriptline, token, g_loadedScriptLocation.c_str() );
		while ( GetToken( true ) && !strEqual( token, "}" ) )
		{
			/* parse stage directives */
			if ( strEqual( token, "{" ) ) {
				while ( GetToken( true ) && !strEqual( token, "}" ) )
				{
					if ( strEqual( token, "{" ) )
						Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", shaderfile, scriptline );

					/* digest any images */
					if ( striEqual( token, "map" ) ) {
						/* get an image */
						GetToken( false );
						if ( *token != '*' && *token != '$' ) {
							// map maps/bake_test_1/lm_0004.tga
							int lmindex;
							int okcount = 0;
							if( sscanf( token + strlen( token ) - ( strlen( EXTERNAL_LIGHTMAP ) + 1 ), "/" EXTERNAL_LIGHTMAP "%n", &lmindex, &okcount )
							    && okcount == ( strlen( EXTERNAL_LIGHTMAP ) + 1 ) ){
								for ( size_t i = 0; i < bspShaders.size(); ++i ){ // find bspShaders[i]<->lmindex pair
									if( strEqual( bspShaders[i].shader, shadername ) ){
										lmIndices[i] = lmindex;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}




/*
   ConvertBSPToASE()
   exports an 3d studio ase file from the bsp
 */

int ConvertBSPToOBJ( char *bspName ){
	int modelNum;
	FILE            *f, *fmtl;
	entity_t        *e;
	const char      *key;
	std::vector<int> lmIndices( bspShaders.size(), -1 );


	/* note it */
	Sys_Printf( "--- Convert BSP to OBJ ---\n" );

	/* create the ase filename from the bsp name */
	const auto dirname = StringStream( PathExtensionless( bspName ) );
	const auto name = StringStream( dirname, ".obj" );
	Sys_Printf( "writing %s\n", name.c_str() );
	const auto mtlname = StringStream( dirname, ".mtl" );
	Sys_Printf( "writing %s\n", mtlname.c_str() );
	const auto base = StringStream<64>( PathFilename( bspName ) );

	/* open it */
	f = SafeOpenWrite( name );
	fmtl = SafeOpenWrite( mtlname );

	/* print header */
	fprintf( f, "o %s\r\n", base.c_str() );
	fprintf( f, "# Generated by Q3Map2 (ydnar) -convert -format obj\r\n" );
	fprintf( f, "mtllib %s.mtl\r\n", base.c_str() );

	fprintf( fmtl, "# Generated by Q3Map2 (ydnar) -convert -format obj\r\n" );
	if ( lightmapsAsTexcoord ) {
		lastLightmap = Convert_CountLightmaps( dirname ) - 1;
		Convert_ReferenceLightmaps( base, lmIndices );
	}
	else
	{
		for ( const bspShader_t& shader : bspShaders )
		{
			ConvertShaderToMTL( fmtl, shader );
		}
	}

	/* walk entity list */
	for ( std::size_t i = 0; i < entities.size(); ++i )
	{
		/* get entity and model */
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = e->valueForKey( "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}

		/* convert model */
		ConvertModelToOBJ( f, modelNum, e->vectorForKey( "origin" ), lmIndices );
	}

	if ( lightmapsAsTexcoord ) {
		for ( int i = firstLightmap; i <= lastLightmap; ++i )
			ConvertLightmapToMTL( fmtl, base, i );
	}

	/* close the file and return */
	fclose( f );
	fclose( fmtl );

	/* return to sender */
	return 0;
}

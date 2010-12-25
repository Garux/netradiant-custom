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
#define CONVERT_ASE_C



/* dependencies */
#include "q3map2.h"



/*
ConvertSurface()
converts a bsp drawsurface to an obj chunk
*/

int firstLightmap = 0;
int lastLightmap = -1;
static void ConvertLightmapToMTL( FILE *f, const char *base, int lightmapNum );

int objVertexCount = 0;
int objLastShaderNum = -1;
static void ConvertSurfaceToOBJ( FILE *f, bspModel_t *model, int modelNum, bspDrawSurface_t *ds, int surfaceNum, vec3_t origin )
{
	int				i, v, face, a, b, c;
	bspDrawVert_t	*dv;
	
	/* ignore patches for now */
	if( ds->surfaceType != MST_PLANAR && ds->surfaceType != MST_TRIANGLE_SOUP )
		return;

	fprintf(f, "g mat%dmodel%dsurf%d\r\n", ds->shaderNum, modelNum, surfaceNum);
	switch( ds->surfaceType )
	{
		case MST_PLANAR:
			fprintf( f, "# SURFACETYPE MST_PLANAR\r\n" );
			break;
		case MST_TRIANGLE_SOUP:
			fprintf( f, "# SURFACETYPE MST_TRIANGLE_SOUP\r\n" );
			break;
	}

	/* export shader */
	if(lightmapsAsTexcoord)
	{
		if(objLastShaderNum != ds->lightmapNum[0])
		{
			fprintf(f, "usemtl lm_%04d\r\n", ds->lightmapNum[0] + deluxemap);
			objLastShaderNum = ds->lightmapNum[0] + deluxemap;
		}
		if(ds->lightmapNum[0] + deluxemap < firstLightmap)
		{
			Sys_Printf( "WARNING: lightmap %d out of range (exporting anyway)\n", ds->lightmapNum[0] + deluxemap );
			firstLightmap = ds->lightmapNum[0] + deluxemap;
		}
		if(ds->lightmapNum[0] > lastLightmap)
		{
			Sys_Printf( "WARNING: lightmap %d out of range (exporting anyway)\n", ds->lightmapNum[0] + deluxemap );
			lastLightmap = ds->lightmapNum[0] + deluxemap;
		}
	}
	else
	{
		if(objLastShaderNum != ds->shaderNum)
		{
			fprintf(f, "usemtl %s\r\n", bspShaders[ds->shaderNum].shader);
			objLastShaderNum = ds->shaderNum;
		}
	}
	
	/* export vertex */
	for( i = 0; i < ds->numVerts; i++ )
	{
		v = i + ds->firstVert;
		dv = &bspDrawVerts[ v ];
		fprintf(f, "# vertex %d\r\n", i + objVertexCount + 1);
		fprintf(f, "v %f %f %f\r\n", dv->xyz[ 0 ], dv->xyz[ 1 ], dv->xyz[ 2 ]);
		fprintf(f, "vn %f %f %f\r\n", dv->normal[ 0 ], dv->normal[ 1 ], dv->normal[ 2 ]);
		if(lightmapsAsTexcoord)
			fprintf(f, "vt %f %f\r\n", dv->lightmap[0][0], 1.0 - dv->lightmap[0][1]);
		else
			fprintf(f, "vt %f %f\r\n", dv->st[ 0 ], 1.0 - dv->st[ 1 ]);
	}

	/* export faces */
	for( i = 0; i < ds->numIndexes; i += 3 )
	{
		face = (i / 3);
		a = bspDrawIndexes[ i + ds->firstIndex ];
		c = bspDrawIndexes[ i + ds->firstIndex + 1 ];
		b = bspDrawIndexes[ i + ds->firstIndex + 2 ];
		fprintf(f, "f %d/%d/%d %d/%d/%d %d/%d/%d\r\n",
			a + objVertexCount + 1, a + objVertexCount + 1, a + objVertexCount + 1, 
			b + objVertexCount + 1, b + objVertexCount + 1, b + objVertexCount + 1, 
			c + objVertexCount + 1, c + objVertexCount + 1, c + objVertexCount + 1
		);
	}

	objVertexCount += ds->numVerts;
}



/*
ConvertModel()
exports a bsp model to an ase chunk
*/

static void ConvertModelToOBJ( FILE *f, bspModel_t *model, int modelNum, vec3_t origin )
{
	int					i, s;
	bspDrawSurface_t	*ds;
	
	
	/* go through each drawsurf in the model */
	for( i = 0; i < model->numBSPSurfaces; i++ )
	{
		s = i + model->firstBSPSurface;
		ds = &bspDrawSurfaces[ s ];
		ConvertSurfaceToOBJ( f, model, modelNum, ds, s, origin );
	}
}



/*
ConvertShader()
exports a bsp shader to an ase chunk
*/

static void ConvertShaderToMTL( FILE *f, bspShader_t *shader, int shaderNum )
{
	shaderInfo_t	*si;
	char			*c, filename[ 1024 ];
	
	
	/* get shader */
	si = ShaderInfoForShader( shader->shader );
	if( si == NULL )
	{
		Sys_Printf( "WARNING: NULL shader in BSP\n" );
		return;
	}
	
	/* set bitmap filename */
	if( si->shaderImage->filename[ 0 ] != '*' )
		strcpy( filename, si->shaderImage->filename );
	else
		sprintf( filename, "%s.tga", si->shader );
	for( c = filename; *c != '\0'; c++ )
		if( *c == '/' )
			*c = '\\';
	
	/* print shader info */
	fprintf( f, "newmtl %s\r\n", shader->shader );
	fprintf( f, "Kd %f %f %f\r\n", si->color[ 0 ], si->color[ 1 ], si->color[ 2 ] );
	if(shadersAsBitmap)
		fprintf( f, "map_Kd %s\r\n", shader->shader );
	else
		fprintf( f, "map_Kd ..\\%s\r\n", filename );
}

static void ConvertLightmapToMTL( FILE *f, const char *base, int lightmapNum )
{
	/* print shader info */
	fprintf( f, "newmtl lm_%04d\r\n", lightmapNum );
	if(lightmapNum >= 0)
		fprintf( f, "map_Kd %s\\lm_%04d.tga\r\n", base, lightmapNum );
}



/*
ConvertBSPToASE()
exports an 3d studio ase file from the bsp
*/

int ConvertBSPToOBJ( char *bspName )
{
	int				i, modelNum;
	FILE			*f, *fmtl;
	bspShader_t		*shader;
	bspModel_t		*model;
	entity_t		*e;
	vec3_t			origin;
	const char		*key;
	char			name[ 1024 ], base[ 1024 ], mtlname[ 1024 ], dirname[ 1024 ];
	
	
	/* note it */
	Sys_Printf( "--- Convert BSP to OBJ ---\n" );

	/* create the ase filename from the bsp name */
	strcpy( dirname, bspName );
	StripExtension( dirname );
	strcpy( name, bspName );
	StripExtension( name );
	strcat( name, ".obj" );
	Sys_Printf( "writing %s\n", name );
	strcpy( mtlname, bspName );
	StripExtension( mtlname );
	strcat( mtlname, ".mtl" );
	Sys_Printf( "writing %s\n", mtlname );
	
	ExtractFileBase( bspName, base );
	
	/* open it */
	f = fopen( name, "wb" );
	if( f == NULL )
		Error( "Open failed on %s\n", name );
	fmtl = fopen( mtlname, "wb" );
	if( fmtl == NULL )
		Error( "Open failed on %s\n", mtlname );
	
	/* print header */
	fprintf( f, "o %s\r\n", base );
	fprintf( f, "# Generated by Q3Map2 (ydnar) -convert -format obj\r\n" );
	fprintf( f, "mtllib %s.mtl\r\n", base );

	fprintf( fmtl, "# Generated by Q3Map2 (ydnar) -convert -format obj\r\n" );
	if(lightmapsAsTexcoord)
	{
		int lightmapCount;
		for( lightmapCount = 0; lightmapCount < numBSPLightmaps; lightmapCount++ )
			;
		for( ; ; lightmapCount++ )
		{
			char buf[1024];
			FILE *tmp;
			snprintf(buf, sizeof(buf), "%s/lm_%04d.tga", dirname, lightmapCount);
			buf[sizeof(buf) - 1] = 0;
			tmp = fopen(buf, "rb");
			if(!tmp)
				break;
			fclose(tmp);
		}
		lastLightmap = lightmapCount - 1;
	}
	else
	{
		for( i = 0; i < numBSPShaders; i++ )
		{
			shader = &bspShaders[ i ];
			ConvertShaderToMTL( fmtl, shader, i );
		}
	}
	
	/* walk entity list */
	for( i = 0; i < numEntities; i++ )
	{
		/* get entity and model */
		e = &entities[ i ];
		if( i == 0 )
			modelNum = 0;
		else
		{
			key = ValueForKey( e, "model" );
			if( key[ 0 ] != '*' )
				continue;
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];
		
		/* get entity origin */
		key = ValueForKey( e, "origin" );
		if( key[ 0 ] == '\0' )
			VectorClear( origin );
		else
			GetVectorForKey( e, "origin", origin );
		
		/* convert model */
		ConvertModelToOBJ( f, model, modelNum, origin );
	}
	
	if(lightmapsAsTexcoord)
	{
		for( i = firstLightmap; i <= lastLightmap; i++ )
			ConvertLightmapToMTL( fmtl, base, i );
	}

	/* close the file and return */
	fclose( f );
	fclose( fmtl );
	
	/* return to sender */
	return 0;
}




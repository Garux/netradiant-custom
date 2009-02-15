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

vec_t Random( void )
{
	return (vec_t) rand() / RAND_MAX;
}



/*
ExitQ3Map()
cleanup routine
*/

static void ExitQ3Map( void )
{
	BSPFilesCleanup();
	if( mapDrawSurfs != NULL )
		free( mapDrawSurfs );
}



/*
MD4BlockChecksum()
calculates an md4 checksum for a block of data
*/

static int MD4BlockChecksum( void *buffer, int length )
{
	return Com_BlockChecksum(buffer, length);
}



/*
FixAAS()
resets an aas checksum to match the given BSP
*/

int FixAAS( int argc, char **argv )
{
	int			length, checksum;
	void		*buffer;
	FILE		*file;
	char		aas[ 1024 ], **ext;
	char		*exts[] =
				{
					".aas",
					"_b0.aas",
					"_b1.aas",
					NULL
				};
	
	
	/* arg checking */
	if( argc < 2 )
	{
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
	while( *ext )
	{
		/* mangle name */
		strcpy( aas, source );
		StripExtension( aas );
		strcat( aas, *ext );
		Sys_Printf( "Trying %s\n", aas );
		ext++;
		
		/* fix it */
		file = fopen( aas, "r+b" );
		if( !file )
			continue;
		if( fwrite( &checksum, 4, 1, file ) != 1 )
			Error( "Error writing checksum to %s", aas );
		fclose( file );
	}
	
	/* return to sender */
	return 0;
}



/*
AnalyzeBSP() - ydnar
analyzes a Quake engine BSP file
*/

typedef struct abspHeader_s
{
	char			ident[ 4 ];
	int				version;
	
	bspLump_t		lumps[ 1 ];	/* unknown size */
}
abspHeader_t;

typedef struct abspLumpTest_s
{
	int				radix, minCount;
	char			*name;
}
abspLumpTest_t;

int AnalyzeBSP( int argc, char **argv )
{
	abspHeader_t			*header;
	int						size, i, version, offset, length, lumpInt, count;
	char					ident[ 5 ];
	void					*lump;
	float					lumpFloat;
	char					lumpString[ 1024 ], source[ 1024 ];
	qboolean				lumpSwap = qfalse;
	abspLumpTest_t			*lumpTest;
	static abspLumpTest_t	lumpTests[] =
							{
								{ sizeof( bspPlane_t ),			6,		"IBSP LUMP_PLANES" },
								{ sizeof( bspBrush_t ),			1,		"IBSP LUMP_BRUSHES" },
								{ 8,							6,		"IBSP LUMP_BRUSHSIDES" },
								{ sizeof( bspBrushSide_t ),		6,		"RBSP LUMP_BRUSHSIDES" },
								{ sizeof( bspModel_t ),			1,		"IBSP LUMP_MODELS" },
								{ sizeof( bspNode_t ),			2,		"IBSP LUMP_NODES" },
								{ sizeof( bspLeaf_t ),			1,		"IBSP LUMP_LEAFS" },
								{ 104,							3,		"IBSP LUMP_DRAWSURFS" },
								{ 44,							3,		"IBSP LUMP_DRAWVERTS" },
								{ 4,							6,		"IBSP LUMP_DRAWINDEXES" },
								{ 128 * 128 * 3,				1,		"IBSP LUMP_LIGHTMAPS" },
								{ 256 * 256 * 3,				1,		"IBSP LUMP_LIGHTMAPS (256 x 256)" },
								{ 512 * 512 * 3,				1,		"IBSP LUMP_LIGHTMAPS (512 x 512)" },
								{ 0, 0, NULL }
							};
	
	
	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -analyze [-lumpswap] [-v] <mapname>\n" );
		return 0;
	}
	
	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
	{
		/* -format map|ase|... */
		if( !strcmp( argv[ i ],  "-lumpswap" ) )
		{
			Sys_Printf( "Swapped lump structs enabled\n" );
 			lumpSwap = qtrue;
 		}
	}
	
	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	Sys_Printf( "Loading %s\n", source );
	
	/* load the file */
	size = LoadFile( source, (void**) &header );
	if( size == 0 || header == NULL )
	{
		Sys_Printf( "Unable to load %s.\n", source );
		return -1;
	}
	
	/* analyze ident/version */
	memcpy( ident, header->ident, 4 );
	ident[ 4 ] = '\0';
	version = LittleLong( header->version );
	
	Sys_Printf( "Identity:      %s\n", ident );
	Sys_Printf( "Version:       %d\n", version );
	Sys_Printf( "---------------------------------------\n" );
	
	/* analyze each lump */
	for( i = 0; i < 100; i++ )
	{
		/* call of duty swapped lump pairs */
		if( lumpSwap )
		{
			offset = LittleLong( header->lumps[ i ].length );
			length = LittleLong( header->lumps[ i ].offset );
		}
		
		/* standard lump pairs */
		else
		{
			offset = LittleLong( header->lumps[ i ].offset );
			length = LittleLong( header->lumps[ i ].length );
		}
		
		/* extract data */
		lump = (byte*) header + offset;
		lumpInt = LittleLong( (int) *((int*) lump) );
		lumpFloat = LittleFloat( (float) *((float*) lump) );
		memcpy( lumpString, (char*) lump, (length < 1024 ? length : 1024) );
		lumpString[ 1024 ] = '\0';
		
		/* print basic lump info */
		Sys_Printf( "Lump:          %d\n", i );
		Sys_Printf( "Offset:        %d bytes\n", offset );
		Sys_Printf( "Length:        %d bytes\n", length );
		
		/* only operate on valid lumps */
		if( length > 0 )
		{
			/* print data in 4 formats */
			Sys_Printf( "As hex:        %08X\n", lumpInt );
			Sys_Printf( "As int:        %d\n", lumpInt );
			Sys_Printf( "As float:      %f\n", lumpFloat );
			Sys_Printf( "As string:     %s\n", lumpString );
			
			/* guess lump type */
			if( lumpString[ 0 ] == '{' && lumpString[ 2 ] == '"' )
				Sys_Printf( "Type guess:    IBSP LUMP_ENTITIES\n" );
			else if( strstr( lumpString, "textures/" ) )
				Sys_Printf( "Type guess:    IBSP LUMP_SHADERS\n" );
			else
			{
				/* guess based on size/count */
				for( lumpTest = lumpTests; lumpTest->radix > 0; lumpTest++ )
				{
					if( (length % lumpTest->radix) != 0 )
						continue;
					count = length / lumpTest->radix;
					if( count < lumpTest->minCount )
						continue;
					Sys_Printf( "Type guess:    %s (%d x %d)\n", lumpTest->name, count, lumpTest->radix );
				}
			}
		}
		
		Sys_Printf( "---------------------------------------\n" );
		
		/* end of file */
		if( offset + length >= size )
			break;
	}
	
	/* last stats */
	Sys_Printf( "Lump count:    %d\n", i + 1 );
	Sys_Printf( "File size:     %d bytes\n", size );
	
	/* return to caller */
	return 0;
}



/*
BSPInfo()
emits statistics about the bsp file
*/

int BSPInfo( int count, char **fileNames )
{
	int			i;
	char		source[ 1024 ], ext[ 64 ];
	int			size;
	FILE		*f;
	
	
	/* dummy check */
	if( count < 1 )
	{
		Sys_Printf( "No files to dump info for.\n");
		return -1;
	}
	
	/* enable info mode */
	infoMode = qtrue;
	
	/* walk file list */
	for( i = 0; i < count; i++ )
	{
		Sys_Printf( "---------------------------------\n" );
		
		/* mangle filename and get size */
		strcpy( source, fileNames[ i ] );
		ExtractFileExtension( source, ext );
		if( !Q_stricmp( ext, "map" ) )
			StripExtension( source );
		DefaultExtension( source, ".bsp" );
		f = fopen( source, "rb" );
		if( f )
		{
			size = Q_filelength (f);
			fclose( f );
		}
		else
			size = 0;
		
		/* load the bsp file and print lump sizes */
		Sys_Printf( "%s\n", source );
		LoadBSPFile( source );		
		PrintBSPFileSizes();
		
		/* print sizes */
		Sys_Printf( "\n" );
		Sys_Printf( "          total         %9d\n", size );
		Sys_Printf( "                        %9d KB\n", size / 1024 );
		Sys_Printf( "                        %9d MB\n", size / (1024 * 1024) );
		
		Sys_Printf( "---------------------------------\n" );
	}
	
	/* return count */
	return i;
}



/*
ScaleBSPMain()
amaze and confuse your enemies with wierd scaled maps!
*/

int ScaleBSPMain( int argc, char **argv )
{
	int			i;
	float		f, a;
	vec3_t scale;
	vec3_t		vec;
	char		str[ 1024 ];
	int uniform, axis;
	
	
	/* arg checking */
	if( argc < 2 )
	{
		Sys_Printf( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		return 0;
	}
	
	/* get scale */
	scale[2] = scale[1] = scale[0] = atof( argv[ argc - 2 ] );
	if(argc >= 3)
		scale[1] = scale[0] = atof( argv[ argc - 3 ] );
	if(argc >= 4)
		scale[0] = atof( argv[ argc - 4 ] );
	
	uniform = ((scale[0] == scale[1]) && (scale[1] == scale[2]));

	if( scale[0] == 0.0f || scale[1] == 0.0f || scale[2] == 0.0f )
	{
		Sys_Printf( "Usage: q3map -scale <value> <mapname>\n" );
		Sys_Printf( "Non-zero scale value required.\n" );
		return 0;
	}
	
	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();
	
	/* note it */
	Sys_Printf( "--- ScaleBSP ---\n" );
	Sys_FPrintf( SYS_VRB, "%9d entities\n", numEntities );
	
	/* scale entity keys */
	for( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* scale origin */
		GetVectorForKey( &entities[ i ], "origin", vec );
		if( (vec[ 0 ] || vec[ 1 ] || vec[ 2 ]) )
		{
			vec[0] *= scale[0];
			vec[1] *= scale[1];
			vec[2] *= scale[2];
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			SetKeyValue( &entities[ i ], "origin", str );
		}

		a = FloatForKey( &entities[ i ], "angle" );
		if(a == -1 || a == -2) // z scale
			axis = 2;
		else if(fabs(sin(DEG2RAD(a))) < 0.707)
			axis = 0;
		else
			axis = 1;
		
		/* scale door lip */
		f = FloatForKey( &entities[ i ], "lip" );
		if( f )
		{
			f *= scale[axis];
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "lip", str );
		}
		
		/* scale plat height */
		f = FloatForKey( &entities[ i ], "height" );
		if( f )
		{
			f *= scale[2];
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "height", str );
		}

		// TODO maybe allow a definition file for entities to specify which values are scaled how?
	}
	
	/* scale models */
	for( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].mins[0] *= scale[0];
		bspModels[ i ].mins[1] *= scale[1];
		bspModels[ i ].mins[2] *= scale[2];
		bspModels[ i ].maxs[0] *= scale[0];
		bspModels[ i ].maxs[1] *= scale[1];
		bspModels[ i ].maxs[2] *= scale[2];
	}
	
	/* scale nodes */
	for( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].mins[0] *= scale[0];
		bspNodes[ i ].mins[1] *= scale[1];
		bspNodes[ i ].mins[2] *= scale[2];
		bspNodes[ i ].maxs[0] *= scale[0];
		bspNodes[ i ].maxs[1] *= scale[1];
		bspNodes[ i ].maxs[2] *= scale[2];
	}
	
	/* scale leafs */
	for( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].mins[0] *= scale[0];
		bspLeafs[ i ].mins[1] *= scale[1];
		bspLeafs[ i ].mins[2] *= scale[2];
		bspLeafs[ i ].maxs[0] *= scale[0];
		bspLeafs[ i ].maxs[1] *= scale[1];
		bspLeafs[ i ].maxs[2] *= scale[2];
	}
	
	/* scale drawverts */
	for( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz[0] *= scale[0];
		bspDrawVerts[i].xyz[1] *= scale[1];
		bspDrawVerts[i].xyz[2] *= scale[2];
		bspDrawVerts[i].normal[0] /= scale[0];
		bspDrawVerts[i].normal[1] /= scale[1];
		bspDrawVerts[i].normal[2] /= scale[2];
		VectorNormalize(bspDrawVerts[i].normal, bspDrawVerts[i].normal);
	}
	
	/* scale planes */
	if(uniform)
	{
		for( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].dist *= scale[0];
		}
	}
	else
	{
		for( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].normal[0] /= scale[0];
			bspPlanes[ i ].normal[1] /= scale[1];
			bspPlanes[ i ].normal[2] /= scale[2];
			f = 1/VectorLength(bspPlanes[i].normal);
			VectorScale(bspPlanes[i].normal, f, bspPlanes[i].normal);
			bspPlanes[ i ].dist *= f;
		}
	}
	
	/* scale gridsize */
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if( (vec[ 0 ] + vec[ 1 ] + vec[ 2 ]) == 0.0f )
		VectorCopy( gridSize, vec );
	vec[0] *= scale[0];
	vec[1] *= scale[1];
	vec[2] *= scale[2];
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );

	/* inject command line parameters */
	InjectCommandLine(argv, 0, argc - 1);
	
	/* write the bsp */
	UnparseEntities();
	StripExtension( source );
	DefaultExtension( source, "_s.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );
	
	/* return to sender */
	return 0;
}



/*
ConvertBSPMain()
main argument processing function for bsp conversion
*/

int ConvertBSPMain( int argc, char **argv )
{
	int		i;
	int		(*convertFunc)( char * );
	game_t	*convertGame;
	
	
	/* set default */
	convertFunc = ConvertBSPToASE;
	convertGame = NULL;
	
	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		return 0;
	}
	
	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
	{
		/* -format map|ase|... */
		if( !strcmp( argv[ i ],  "-format" ) )
 		{
			i++;
			if( !Q_stricmp( argv[ i ], "ase" ) )
				convertFunc = ConvertBSPToASE;
			else if( !Q_stricmp( argv[ i ], "map" ) )
				convertFunc = ConvertBSPToMap;
			else
			{
				convertGame = GetGame( argv[ i ] );
				if( convertGame == NULL )
					Sys_Printf( "Unknown conversion format \"%s\". Defaulting to ASE.\n", argv[ i ] );
			}
 		}
		else if( !strcmp( argv[ i ],  "-ne" ) )
 		{
			normalEpsilon = atof( argv[ i + 1 ] );
 			i++;
			Sys_Printf( "Normal epsilon set to %f\n", normalEpsilon );
 		}
		else if( !strcmp( argv[ i ],  "-de" ) )
 		{
			distanceEpsilon = atof( argv[ i + 1 ] );
 			i++;
			Sys_Printf( "Distance epsilon set to %f\n", distanceEpsilon );
 		}
		else if( !strcmp( argv[ i ],  "-shadersasbitmap" ) )
			shadersAsBitmap = qtrue;
	}
	
	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	LoadShaderInfo();
	
	Sys_Printf( "Loading %s\n", source );
	
	/* ydnar: load surface file */
	//%	LoadSurfaceExtraFile( source );
	
	LoadBSPFile( source );
	
	/* parse bsp entities */
	ParseEntities();
	
	/* bsp format convert? */
	if( convertGame != NULL )
	{
		/* set global game */
		game = convertGame;
		
		/* write bsp */
		StripExtension( source );
		DefaultExtension( source, "_c.bsp" );
		Sys_Printf( "Writing %s\n", source );
		WriteBSPFile( source );
		
		/* return to sender */
		return 0;
	}
	
	/* normal convert */
	return convertFunc( source );
}



/*
main()
q3map mojo...
*/

int main( int argc, char **argv )
{
	int		i, r;
	double	start, end;
	
	
	/* we want consistent 'randomness' */
	srand( 0 );
	
	/* start timer */
	start = I_FloatTime();

	/* this was changed to emit version number over the network */
	printf( Q3MAP_VERSION "\n" );
	
	/* set exit call */
	atexit( ExitQ3Map );
	
	/* read general options first */
	for( i = 1; i < argc; i++ )
	{
		/* -connect */
		if( !strcmp( argv[ i ], "-connect" ) )
		{
			argv[ i ] = NULL;
			i++;
			Broadcast_Setup( argv[ i ] );
			argv[ i ] = NULL;
		}
		
		/* verbose */
		else if( !strcmp( argv[ i ], "-v" ) )
		{
			verbose = qtrue;
			argv[ i ] = NULL;
		}
		
		/* force */
		else if( !strcmp( argv[ i ], "-force" ) )
		{
			force = qtrue;
			argv[ i ] = NULL;
		}
		
		/* patch subdivisions */
		else if( !strcmp( argv[ i ], "-subdivisions" ) )
		{
			argv[ i ] = NULL;
			i++;
			patchSubdivisions = atoi( argv[ i ] );
			argv[ i ] = NULL;
			if( patchSubdivisions <= 0 )
				patchSubdivisions = 1;
		}
		
		/* threads */
		else if( !strcmp( argv[ i ], "-threads" ) )
		{
			argv[ i ] = NULL;
			i++;
			numthreads = atoi( argv[ i ] );
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
	for( i = 0; i < MAX_JITTERS; i++ )
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
	
	/* ydnar: new path initialization */
	InitPaths( &argc, argv );
	
	/* check if we have enough options left to attempt something */
	if( argc < 2 )
		Error( "Usage: %s [general options] [options] mapfile", argv[ 0 ] );
	
	/* fixaas */
	if( !strcmp( argv[ 1 ], "-fixaas" ) )
		r = FixAAS( argc - 1, argv + 1 );
	
	/* analyze */
	else if( !strcmp( argv[ 1 ], "-analyze" ) )
		r = AnalyzeBSP( argc - 1, argv + 1 );
	
	/* info */
	else if( !strcmp( argv[ 1 ], "-info" ) )
		r = BSPInfo( argc - 2, argv + 2 );
	
	/* vis */
	else if( !strcmp( argv[ 1 ], "-vis" ) )
		r = VisMain( argc - 1, argv + 1 );
	
	/* light */
	else if( !strcmp( argv[ 1 ], "-light" ) )
		r = LightMain( argc - 1, argv + 1 );
	
	/* vlight */
	else if( !strcmp( argv[ 1 ], "-vlight" ) )
	{
		Sys_Printf( "WARNING: VLight is no longer supported, defaulting to -light -fast instead\n\n" );
		argv[ 1 ] = "-fast";	/* eek a hack */
		r = LightMain( argc, argv );
	}
	
	/* ydnar: lightmap export */
	else if( !strcmp( argv[ 1 ], "-export" ) )
		r = ExportLightmapsMain( argc - 1, argv + 1 );
	
	/* ydnar: lightmap import */
	else if( !strcmp( argv[ 1 ], "-import" ) )
		r = ImportLightmapsMain( argc - 1, argv + 1 );
	
	/* ydnar: bsp scaling */
	else if( !strcmp( argv[ 1 ], "-scale" ) )
		r = ScaleBSPMain( argc - 1, argv + 1 );
	
	/* ydnar: bsp conversion */
	else if( !strcmp( argv[ 1 ], "-convert" ) )
		r = ConvertBSPMain( argc - 1, argv + 1 );
	
	/* ydnar: otherwise create a bsp */
	else
		r = BSPMain( argc, argv );
	
	/* emit time */
	end = I_FloatTime();
	Sys_Printf( "%9.0f seconds elapsed\n", end - start );
	
	/* shut down connection */
	Broadcast_Shutdown();
	
	/* return any error code */
	return r;
}

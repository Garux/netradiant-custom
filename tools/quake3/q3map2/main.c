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
	size_t n = strlen( dst  );

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



/*
   AnalyzeBSP() - ydnar
   analyzes a Quake engine BSP file
 */

typedef struct abspHeader_s
{
	char ident[ 4 ];
	int version;

	bspLump_t lumps[ 1 ];       /* unknown size */
}
abspHeader_t;

typedef struct abspLumpTest_s
{
	int radix, minCount;
	char            *name;
}
abspLumpTest_t;

int AnalyzeBSP( int argc, char **argv ){
	abspHeader_t            *header;
	int size, i, version, offset, length, lumpInt, count;
	char ident[ 5 ];
	void                    *lump;
	float lumpFloat;
	char lumpString[ 1024 ], source[ 1024 ];
	qboolean lumpSwap = qfalse;
	abspLumpTest_t          *lumpTest;
	static abspLumpTest_t lumpTests[] =
	{
		{ sizeof( bspPlane_t ),         6,      "IBSP LUMP_PLANES" },
		{ sizeof( bspBrush_t ),         1,      "IBSP LUMP_BRUSHES" },
		{ 8,                            6,      "IBSP LUMP_BRUSHSIDES" },
		{ sizeof( bspBrushSide_t ),     6,      "RBSP LUMP_BRUSHSIDES" },
		{ sizeof( bspModel_t ),         1,      "IBSP LUMP_MODELS" },
		{ sizeof( bspNode_t ),          2,      "IBSP LUMP_NODES" },
		{ sizeof( bspLeaf_t ),          1,      "IBSP LUMP_LEAFS" },
		{ 104,                          3,      "IBSP LUMP_DRAWSURFS" },
		{ 44,                           3,      "IBSP LUMP_DRAWVERTS" },
		{ 4,                            6,      "IBSP LUMP_DRAWINDEXES" },
		{ 128 * 128 * 3,                1,      "IBSP LUMP_LIGHTMAPS" },
		{ 256 * 256 * 3,                1,      "IBSP LUMP_LIGHTMAPS (256 x 256)" },
		{ 512 * 512 * 3,                1,      "IBSP LUMP_LIGHTMAPS (512 x 512)" },
		{ 0, 0, NULL }
	};


	/* arg checking */
	if ( argc < 1 ) {
		Sys_Printf( "Usage: q3map -analyze [-lumpswap] [-v] <mapname>\n" );
		return 0;
	}

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* -format map|ase|... */
		if ( !strcmp( argv[ i ],  "-lumpswap" ) ) {
			Sys_Printf( "Swapped lump structs enabled\n" );
			lumpSwap = qtrue;
		}
	}

	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	Sys_Printf( "Loading %s\n", source );

	/* load the file */
	size = LoadFile( source, (void**) &header );
	if ( size == 0 || header == NULL ) {
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
	for ( i = 0; i < 100; i++ )
	{
		/* call of duty swapped lump pairs */
		if ( lumpSwap ) {
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
		lumpInt = LittleLong( (int) *( (int*) lump ) );
		lumpFloat = LittleFloat( (float) *( (float*) lump ) );
		memcpy( lumpString, (char*) lump, ( (size_t)length < sizeof( lumpString ) ? (size_t)length : sizeof( lumpString ) - 1 ) );
		lumpString[ sizeof( lumpString ) - 1 ] = '\0';

		/* print basic lump info */
		Sys_Printf( "Lump:          %d\n", i );
		Sys_Printf( "Offset:        %d bytes\n", offset );
		Sys_Printf( "Length:        %d bytes\n", length );

		/* only operate on valid lumps */
		if ( length > 0 ) {
			/* print data in 4 formats */
			Sys_Printf( "As hex:        %08X\n", lumpInt );
			Sys_Printf( "As int:        %d\n", lumpInt );
			Sys_Printf( "As float:      %f\n", lumpFloat );
			Sys_Printf( "As string:     %s\n", lumpString );

			/* guess lump type */
			if ( lumpString[ 0 ] == '{' && lumpString[ 2 ] == '"' ) {
				Sys_Printf( "Type guess:    IBSP LUMP_ENTITIES\n" );
			}
			else if ( strstr( lumpString, "textures/" ) ) {
				Sys_Printf( "Type guess:    IBSP LUMP_SHADERS\n" );
			}
			else
			{
				/* guess based on size/count */
				for ( lumpTest = lumpTests; lumpTest->radix > 0; lumpTest++ )
				{
					if ( ( length % lumpTest->radix ) != 0 ) {
						continue;
					}
					count = length / lumpTest->radix;
					if ( count < lumpTest->minCount ) {
						continue;
					}
					Sys_Printf( "Type guess:    %s (%d x %d)\n", lumpTest->name, count, lumpTest->radix );
				}
			}
		}

		Sys_Printf( "---------------------------------------\n" );

		/* end of file */
		if ( offset + length >= size ) {
			break;
		}
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

int BSPInfo( int count, char **fileNames ){
	int i;
	char source[ 1024 ], ext[ 64 ];
	int size;
	FILE        *f;


	/* dummy check */
	if ( count < 1 ) {
		Sys_Printf( "No files to dump info for.\n" );
		return -1;
	}

	/* enable info mode */
	infoMode = qtrue;

	/* walk file list */
	for ( i = 0; i < count; i++ )
	{
		Sys_Printf( "---------------------------------\n" );

		/* mangle filename and get size */
		strcpy( source, fileNames[ i ] );
		ExtractFileExtension( source, ext );
		if ( !Q_stricmp( ext, "map" ) ) {
			StripExtension( source );
		}
		DefaultExtension( source, ".bsp" );
		f = fopen( source, "rb" );
		if ( f ) {
			size = Q_filelength( f );
			fclose( f );
		}
		else{
			size = 0;
		}

		/* load the bsp file and print lump sizes */
		Sys_Printf( "%s\n", source );
		LoadBSPFile( source );
		PrintBSPFileSizes();

		/* print sizes */
		Sys_Printf( "\n" );
		Sys_Printf( "          total         %9d\n", size );
		Sys_Printf( "                        %9d KB\n", size / 1024 );
		Sys_Printf( "                        %9d MB\n", size / ( 1024 * 1024 ) );

		Sys_Printf( "---------------------------------\n" );
	}

	/* return count */
	return i;
}


static void ExtrapolateTexcoords( const float *axyz, const float *ast, const float *bxyz, const float *bst, const float *cxyz, const float *cst, const float *axyz_new, float *ast_out, const float *bxyz_new, float *bst_out, const float *cxyz_new, float *cst_out ){
	vec4_t scoeffs, tcoeffs;
	float md;
	m4x4_t solvematrix;

	vec3_t norm;
	vec3_t dab, dac;
	VectorSubtract( bxyz, axyz, dab );
	VectorSubtract( cxyz, axyz, dac );
	CrossProduct( dab, dac, norm );

	// assume:
	//   s = f(x, y, z)
	//   s(v + norm) = s(v) when n ortho xyz

	// s(v) = DotProduct(v, scoeffs) + scoeffs[3]

	// solve:
	//   scoeffs * (axyz, 1) == ast[0]
	//   scoeffs * (bxyz, 1) == bst[0]
	//   scoeffs * (cxyz, 1) == cst[0]
	//   scoeffs * (norm, 0) == 0
	// scoeffs * [axyz, 1 | bxyz, 1 | cxyz, 1 | norm, 0] = [ast[0], bst[0], cst[0], 0]
	solvematrix[0] = axyz[0];
	solvematrix[4] = axyz[1];
	solvematrix[8] = axyz[2];
	solvematrix[12] = 1;
	solvematrix[1] = bxyz[0];
	solvematrix[5] = bxyz[1];
	solvematrix[9] = bxyz[2];
	solvematrix[13] = 1;
	solvematrix[2] = cxyz[0];
	solvematrix[6] = cxyz[1];
	solvematrix[10] = cxyz[2];
	solvematrix[14] = 1;
	solvematrix[3] = norm[0];
	solvematrix[7] = norm[1];
	solvematrix[11] = norm[2];
	solvematrix[15] = 0;

	md = m4_det( solvematrix );
	if ( md * md < 1e-10 ) {
		Sys_Printf( "Cannot invert some matrix, some texcoords aren't extrapolated!" );
		return;
	}

	m4x4_invert( solvematrix );

	scoeffs[0] = ast[0];
	scoeffs[1] = bst[0];
	scoeffs[2] = cst[0];
	scoeffs[3] = 0;
	m4x4_transform_vec4( solvematrix, scoeffs );
	tcoeffs[0] = ast[1];
	tcoeffs[1] = bst[1];
	tcoeffs[2] = cst[1];
	tcoeffs[3] = 0;
	m4x4_transform_vec4( solvematrix, tcoeffs );

	ast_out[0] = scoeffs[0] * axyz_new[0] + scoeffs[1] * axyz_new[1] + scoeffs[2] * axyz_new[2] + scoeffs[3];
	ast_out[1] = tcoeffs[0] * axyz_new[0] + tcoeffs[1] * axyz_new[1] + tcoeffs[2] * axyz_new[2] + tcoeffs[3];
	bst_out[0] = scoeffs[0] * bxyz_new[0] + scoeffs[1] * bxyz_new[1] + scoeffs[2] * bxyz_new[2] + scoeffs[3];
	bst_out[1] = tcoeffs[0] * bxyz_new[0] + tcoeffs[1] * bxyz_new[1] + tcoeffs[2] * bxyz_new[2] + tcoeffs[3];
	cst_out[0] = scoeffs[0] * cxyz_new[0] + scoeffs[1] * cxyz_new[1] + scoeffs[2] * cxyz_new[2] + scoeffs[3];
	cst_out[1] = tcoeffs[0] * cxyz_new[0] + tcoeffs[1] * cxyz_new[1] + tcoeffs[2] * cxyz_new[2] + tcoeffs[3];
}

/*
   ScaleBSPMain()
   amaze and confuse your enemies with wierd scaled maps!
 */

int ScaleBSPMain( int argc, char **argv ){
	int i, j;
	float f, a;
	vec3_t scale;
	vec3_t vec;
	char str[ 1024 ];
	int uniform, axis;
	qboolean texscale;
	float *old_xyzst = NULL;
	float spawn_ref = 0;


	/* arg checking */
	if ( argc < 3 ) {
		Sys_Printf( "Usage: q3map [-v] -scale [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		return 0;
	}

	texscale = qfalse;
	for ( i = 1; i < argc - 2; ++i )
	{
		if ( !strcmp( argv[i], "-tex" ) ) {
			texscale = qtrue;
		}
		else if ( !strcmp( argv[i], "-spawn_ref" ) ) {
			spawn_ref = atof( argv[i + 1] );
			++i;
		}
		else{
			break;
		}
	}

	/* get scale */
	// if(argc-2 >= i) // always true
	scale[2] = scale[1] = scale[0] = atof( argv[ argc - 2 ] );
	if ( argc - 3 >= i ) {
		scale[1] = scale[0] = atof( argv[ argc - 3 ] );
	}
	if ( argc - 4 >= i ) {
		scale[0] = atof( argv[ argc - 4 ] );
	}

	uniform = ( ( scale[0] == scale[1] ) && ( scale[1] == scale[2] ) );

	if ( scale[0] == 0.0f || scale[1] == 0.0f || scale[2] == 0.0f ) {
		Sys_Printf( "Usage: q3map [-v] -scale [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
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
	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* scale origin */
		GetVectorForKey( &entities[ i ], "origin", vec );
		if ( ( vec[ 0 ] || vec[ 1 ] || vec[ 2 ] ) ) {
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] += spawn_ref;
			}
			vec[0] *= scale[0];
			vec[1] *= scale[1];
			vec[2] *= scale[2];
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] -= spawn_ref;
			}
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			SetKeyValue( &entities[ i ], "origin", str );
		}

		a = FloatForKey( &entities[ i ], "angle" );
		if ( a == -1 || a == -2 ) { // z scale
			axis = 2;
		}
		else if ( fabs( sin( DEG2RAD( a ) ) ) < 0.707 ) {
			axis = 0;
		}
		else{
			axis = 1;
		}

		/* scale door lip */
		f = FloatForKey( &entities[ i ], "lip" );
		if ( f ) {
			f *= scale[axis];
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "lip", str );
		}

		/* scale plat height */
		f = FloatForKey( &entities[ i ], "height" );
		if ( f ) {
			f *= scale[2];
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "height", str );
		}

		// TODO maybe allow a definition file for entities to specify which values are scaled how?
	}

	/* scale models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].mins[0] *= scale[0];
		bspModels[ i ].mins[1] *= scale[1];
		bspModels[ i ].mins[2] *= scale[2];
		bspModels[ i ].maxs[0] *= scale[0];
		bspModels[ i ].maxs[1] *= scale[1];
		bspModels[ i ].maxs[2] *= scale[2];
	}

	/* scale nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].mins[0] *= scale[0];
		bspNodes[ i ].mins[1] *= scale[1];
		bspNodes[ i ].mins[2] *= scale[2];
		bspNodes[ i ].maxs[0] *= scale[0];
		bspNodes[ i ].maxs[1] *= scale[1];
		bspNodes[ i ].maxs[2] *= scale[2];
	}

	/* scale leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].mins[0] *= scale[0];
		bspLeafs[ i ].mins[1] *= scale[1];
		bspLeafs[ i ].mins[2] *= scale[2];
		bspLeafs[ i ].maxs[0] *= scale[0];
		bspLeafs[ i ].maxs[1] *= scale[1];
		bspLeafs[ i ].maxs[2] *= scale[2];
	}

	if ( texscale ) {
		Sys_Printf( "Using texture unlocking (and probably breaking texture alignment a lot)\n" );
		old_xyzst = safe_malloc( sizeof( *old_xyzst ) * numBSPDrawVerts * 5 );
		for ( i = 0; i < numBSPDrawVerts; i++ )
		{
			old_xyzst[5 * i + 0] = bspDrawVerts[i].xyz[0];
			old_xyzst[5 * i + 1] = bspDrawVerts[i].xyz[1];
			old_xyzst[5 * i + 2] = bspDrawVerts[i].xyz[2];
			old_xyzst[5 * i + 3] = bspDrawVerts[i].st[0];
			old_xyzst[5 * i + 4] = bspDrawVerts[i].st[1];
		}
	}

	/* scale drawverts */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz[0] *= scale[0];
		bspDrawVerts[i].xyz[1] *= scale[1];
		bspDrawVerts[i].xyz[2] *= scale[2];
		bspDrawVerts[i].normal[0] /= scale[0];
		bspDrawVerts[i].normal[1] /= scale[1];
		bspDrawVerts[i].normal[2] /= scale[2];
		VectorNormalize( bspDrawVerts[i].normal, bspDrawVerts[i].normal );
	}

	if ( texscale ) {
		for ( i = 0; i < numBSPDrawSurfaces; i++ )
		{
			switch ( bspDrawSurfaces[i].surfaceType )
			{
			case SURFACE_FACE:
			case SURFACE_META:
				if ( bspDrawSurfaces[i].numIndexes % 3 ) {
					Error( "Not a triangulation!" );
				}
				for ( j = bspDrawSurfaces[i].firstIndex; j < bspDrawSurfaces[i].firstIndex + bspDrawSurfaces[i].numIndexes; j += 3 )
				{
					int ia = bspDrawIndexes[j] + bspDrawSurfaces[i].firstVert, ib = bspDrawIndexes[j + 1] + bspDrawSurfaces[i].firstVert, ic = bspDrawIndexes[j + 2] + bspDrawSurfaces[i].firstVert;
					bspDrawVert_t *a = &bspDrawVerts[ia], *b = &bspDrawVerts[ib], *c = &bspDrawVerts[ic];
					float *oa = &old_xyzst[ia * 5], *ob = &old_xyzst[ib * 5], *oc = &old_xyzst[ic * 5];
					// extrapolate:
					//   a->xyz -> oa
					//   b->xyz -> ob
					//   c->xyz -> oc
					ExtrapolateTexcoords(
						&oa[0], &oa[3],
						&ob[0], &ob[3],
						&oc[0], &oc[3],
						a->xyz, a->st,
						b->xyz, b->st,
						c->xyz, c->st );
				}
				break;
			}
		}
	}

	/* scale planes */
	if ( uniform ) {
		for ( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].dist *= scale[0];
		}
	}
	else
	{
		for ( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].normal[0] /= scale[0];
			bspPlanes[ i ].normal[1] /= scale[1];
			bspPlanes[ i ].normal[2] /= scale[2];
			f = 1 / VectorLength( bspPlanes[i].normal );
			VectorScale( bspPlanes[i].normal, f, bspPlanes[i].normal );
			bspPlanes[ i ].dist *= f;
		}
	}

	/* scale gridsize */
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if ( ( vec[ 0 ] + vec[ 1 ] + vec[ 2 ] ) == 0.0f ) {
		VectorCopy( gridSize, vec );
	}
	vec[0] *= scale[0];
	vec[1] *= scale[1];
	vec[2] *= scale[2];
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );

	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

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
   PseudoCompileBSP()
   a stripped down ProcessModels
 */
void PseudoCompileBSP( qboolean need_tree ){
	int models;
	char modelValue[10];
	entity_t *entity;
	face_t *faces;
	tree_t *tree;
	node_t *node;
	brush_t *brush;
	side_t *side;
	int i;

	SetDrawSurfacesBuffer();
	mapDrawSurfs = safe_malloc( sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	memset( mapDrawSurfs, 0, sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	numMapDrawSurfs = 0;

	BeginBSPFile();
	models = 1;
	for ( mapEntityNum = 0; mapEntityNum < numEntities; mapEntityNum++ )
	{
		/* get entity */
		entity = &entities[ mapEntityNum ];
		if ( entity->brushes == NULL && entity->patches == NULL ) {
			continue;
		}

		if ( mapEntityNum != 0 ) {
			sprintf( modelValue, "*%d", models++ );
			SetKeyValue( entity, "model", modelValue );
		}

		/* process the model */
		Sys_FPrintf( SYS_VRB, "############### model %i ###############\n", numBSPModels );
		BeginModel();

		entity->firstDrawSurf = numMapDrawSurfs;

		ClearMetaTriangles();
		PatchMapDrawSurfs( entity );

		if ( mapEntityNum == 0 && need_tree ) {
			faces = MakeStructuralBSPFaceList( entities[0].brushes );
			tree = FaceBSP( faces );
			node = tree->headnode;
		}
		else
		{
			node = AllocNode();
			node->planenum = PLANENUM_LEAF;
			tree = AllocTree();
			tree->headnode = node;
		}

		/* a minimized ClipSidesIntoTree */
		for ( brush = entity->brushes; brush; brush = brush->next )
		{
			/* walk the brush sides */
			for ( i = 0; i < brush->numsides; i++ )
			{
				/* get side */
				side = &brush->sides[ i ];
				if ( side->winding == NULL ) {
					continue;
				}
				/* shader? */
				if ( side->shaderInfo == NULL ) {
					continue;
				}
				/* save this winding as a visible surface */
				DrawSurfaceForSide( entity, brush, side, side->winding );
			}
		}

		if ( meta ) {
			ClassifyEntitySurfaces( entity );
			MakeEntityDecals( entity );
			MakeEntityMetaTriangles( entity );
			SmoothMetaTriangles();
			MergeMetaTriangles();
		}
		FilterDrawsurfsIntoTree( entity, tree );

		FilterStructuralBrushesIntoTree( entity, tree );
		FilterDetailBrushesIntoTree( entity, tree );

		EmitBrushes( entity->brushes, &entity->firstBrush, &entity->numBrushes );
		EndModel( entity, node );
	}
	EndBSPFile( qfalse );
}

/*
   ConvertBSPMain()
   main argument processing function for bsp conversion
 */

int ConvertBSPMain( int argc, char **argv ){
	int i;
	int ( *convertFunc )( char * );
	game_t  *convertGame;
	char ext[1024];
	qboolean map_allowed, force_bsp, force_map;


	/* set default */
	convertFunc = ConvertBSPToASE;
	convertGame = NULL;
	map_allowed = qfalse;
	force_bsp = qfalse;
	force_map = qfalse;

	/* arg checking */
	if ( argc < 1 ) {
		Sys_Printf( "Usage: q3map -convert [-format <ase|obj|map_bp|map>] [-shadersasbitmap|-lightmapsastexcoord|-deluxemapsastexcoord] [-readbsp|-readmap [-meta|-patchmeta]] [-v] <mapname>\n" );
		return 0;
	}

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* -format map|ase|... */
		if ( !strcmp( argv[ i ],  "-format" ) ) {
			i++;
			if ( !Q_stricmp( argv[ i ], "ase" ) ) {
				convertFunc = ConvertBSPToASE;
				map_allowed = qfalse;
			}
			else if ( !Q_stricmp( argv[ i ], "obj" ) ) {
				convertFunc = ConvertBSPToOBJ;
				map_allowed = qfalse;
			}
			else if ( !Q_stricmp( argv[ i ], "map_bp" ) ) {
				convertFunc = ConvertBSPToMap_BP;
				map_allowed = qtrue;
			}
			else if ( !Q_stricmp( argv[ i ], "map" ) ) {
				convertFunc = ConvertBSPToMap;
				map_allowed = qtrue;
			}
			else
			{
				convertGame = GetGame( argv[ i ] );
				map_allowed = qfalse;
				if ( convertGame == NULL ) {
					Sys_Printf( "Unknown conversion format \"%s\". Defaulting to ASE.\n", argv[ i ] );
				}
			}
		}
		else if ( !strcmp( argv[ i ],  "-ne" ) ) {
			normalEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Normal epsilon set to %f\n", normalEpsilon );
		}
		else if ( !strcmp( argv[ i ],  "-de" ) ) {
			distanceEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Distance epsilon set to %f\n", distanceEpsilon );
		}
		else if ( !strcmp( argv[ i ],  "-shaderasbitmap" ) || !strcmp( argv[ i ],  "-shadersasbitmap" ) ) {
			shadersAsBitmap = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-lightmapastexcoord" ) || !strcmp( argv[ i ],  "-lightmapsastexcoord" ) ) {
			lightmapsAsTexcoord = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-deluxemapastexcoord" ) || !strcmp( argv[ i ],  "-deluxemapsastexcoord" ) ) {
			lightmapsAsTexcoord = qtrue;
			deluxemap = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-readbsp" ) ) {
			force_bsp = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-readmap" ) ) {
			force_map = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-meta" ) ) {
			meta = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-patchmeta" ) ) {
			meta = qtrue;
			patchMeta = qtrue;
		}
	}

	LoadShaderInfo();

	/* clean up map name */
	strcpy( source, ExpandArg( argv[i] ) );
	ExtractFileExtension( source, ext );

	if ( !map_allowed && !force_map ) {
		force_bsp = qtrue;
	}

	if ( force_map || ( !force_bsp && !Q_stricmp( ext, "map" ) && map_allowed ) ) {
		if ( !map_allowed ) {
			Sys_Printf( "WARNING: the requested conversion should not be done from .map files. Compile a .bsp first.\n" );
		}
		StripExtension( source );
		DefaultExtension( source, ".map" );
		Sys_Printf( "Loading %s\n", source );
		LoadMapFile( source, qfalse, convertGame == NULL );
		PseudoCompileBSP( convertGame != NULL );
	}
	else
	{
		StripExtension( source );
		DefaultExtension( source, ".bsp" );
		Sys_Printf( "Loading %s\n", source );
		LoadBSPFile( source );
		ParseEntities();
	}

	/* bsp format convert? */
	if ( convertGame != NULL ) {
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


struct HelpOption
{
	const char* name;
	const char* description;
};

void HelpOptions(const char* group_name, int indentation, int width, struct HelpOption* options, int count)
{
	indentation *= 2;
	char* indent = malloc(indentation+1);
	memset(indent, ' ', indentation);
	indent[indentation] = 0;
	printf("%s%s:\n", indent, group_name);
	indentation += 2;
	indent = realloc(indent, indentation+1);
	memset(indent, ' ', indentation);
	indent[indentation] = 0;

	int i;
	for ( i = 0; i < count; i++ )
	{
		int printed = printf("%s%-24s  ", indent, options[i].name);
		int descsz = strlen(options[i].description);
		int j = 0;
		while ( j < descsz && descsz-j > width - printed )
		{
			if ( j != 0 )
				printf("%s%26c",indent,' ');
			int fragment = width - printed;
			while ( fragment > 0 && options[i].description[j+fragment-1] != ' ')
					fragment--;
			j += fwrite(options[i].description+j, sizeof(char), fragment, stdout);
			putchar('\n');
			printed = indentation+26;
		}
		if ( j == 0 )
		{
			printf("%s\n",options[i].description+j);
		}
		else if ( j < descsz )
		{
			printf("%s%26c%s\n",indent,' ',options[i].description+j);
		}
	}

	putchar('\n');

	free(indent);
}

void HelpBsp()
{
	struct HelpOption bsp[] = {
		{"-bsp <filename.map>", "Switch that enters this stage"},
		{"-altsplit", "Alternate BSP tree splitting weights (should give more fps)"},
		{"-celshader <shadername>", "Sets a global cel shader name"},
		{"-custinfoparms", "Read scripts/custinfoparms.txt"},
		{"-debuginset", "Push all triangle vertexes towards the triangle center"},
		{"-debugportals", "Make BSP portals visible in the map"},
		{"-debugsurfaces", "Color the vertexes according to the index of the surface"},
		{"-deep", "Use detail brushes in the BSP tree, but at lowest priority (should give more fps)"},
		{"-de <F>", "Distance epsilon for plane snapping etc."},
		{"-fakemap", "Write fakemap.map containing all world brushes"},
		{"-flares", "Turn on support for flares (TEST?)"},
		{"-flat", "Enable flat shading (good for combining with -celshader)"},
		{"-fulldetail", "Treat detail brushes as structural ones"},
		{"-leaktest", "Abort if a leak was found"},
		{"-meta", "Combine adjacent triangles of the same texture to surfaces (ALWAYS USE THIS)"},
		{"-minsamplesize <N>", "Sets minimum lightmap resolution in luxels/qu"},
		{"-mi <N>", "Sets the maximum number of indexes per surface"},
		{"-mv <N>", "Sets the maximum number of vertices of a lightmapped surface"},
		{"-ne <F>", "Normal epsilon for plane snapping etc."},
		{"-nocurves", "Turn off support for patches"},
		{"-nodetail", "Leave out detail brushes"},
		{"-noflares", "Turn off support for flares"},
		{"-nofog", "Turn off support for fog volumes"},
		{"-nohint", "Turn off support for hint brushes"},
		{"-nosubdivide", "Turn off support for `q3map_tessSize` (breaks water vertex deforms)"},
		{"-notjunc", "Do not fix T-junctions (causes cracks between triangles, do not use)"},
		{"-nowater", "Turn off support for water, slime or lava (Stef, this is for you)"},
		{"-np <A>", "Force all surfaces to be nonplanar with a given shade angle"},
		{"-onlyents", "Only update entities in the BSP"},
		{"-patchmeta", "Turn patches into triangle meshes for display"},
		{"-rename", "Append â€œbspâ€ suffix to miscmodel shaders (needed for SoF2)"},
		{"-samplesize <N>", "Sets default lightmap resolution in luxels/qu"},
		{"-skyfix", "Turn sky box into six surfaces to work around ATI problems"},
		{"-snap <N>", "Snap brush bevel planes to the given number of units"},
		{"-tempname <filename.map>", "Read the MAP file from the given file name"},
		{"-texrange <N>", "Limit per-surface texture range to the given number of units, and subdivide surfaces like with `q3map_tessSize` if this is not met"},
		{"-tmpout", "Write the BSP file to /tmp"},
		{"-verboseentities", "Enable `-v` only for map entities, not for the world"},
	};
	HelpOptions("BSP Stage", 0, 80, bsp, sizeof(bsp)/sizeof(struct HelpOption));
}
void HelpVis()
{
	struct HelpOption vis[] = {
		{"-vis <filename.map>", "Switch that enters this stage"},
		{"-fast", "Very fast and crude vis calculation"},
		{"-mergeportals", "The less crude half of `-merge`, makes vis sometimes much faster but doesn't hurt fps usually"},
		{"-merge", "Faster but still okay vis calculation"},
		{"-nopassage", "Just use PortalFlow vis (usually less fps)"},
		{"-nosort", "Do not sort the portals before calculating vis (usually slower)"},
		{"-passageOnly", "Just use PassageFlow vis (usually less fps)"},
		{"-saveprt", "Keep the PRT file after running vis (so you can run vis again)"},
		{"-tmpin", "Use /tmp folder for input"},
		{"-tmpout", "Use /tmp folder for output"},
	};
	HelpOptions("VIS Stage", 0, 80, vis, sizeof(vis)/sizeof(struct HelpOption));
}
void HelpLight()
{
	struct HelpOption light[] = {
		{"-light <filename.map>", "Switch that enters this stage"},
		{"-vlight <filename.map>", "Deprecated alias for `-light -fast` ... filename.map"},
		{"-approx <N>", "Vertex light approximation tolerance (never use in conjunction with deluxemapping)"},
		{"-areascale <F, `-area` F>", "Scaling factor for area lights (surfacelight)"},
		{"-border", "Add a red border to lightmaps for debugging"},
		{"-bouncegrid", "Also compute radiosity on the light grid"},
		{"-bounceonly", "Only compute radiosity"},
		{"-bouncescale <F>", "Scaling factor for radiosity"},
		{"-bounce <N>", "Number of bounces for radiosity"},
		{"-cheapgrid", "Use `-cheap` style lighting for radiosity"},
		{"-cheap", "Abort vertex light calculations when white is reached"},
		{"-compensate <F>", "Lightmap compensate (darkening factor applied after everything else)"},
		{"-cpma", "CPMA vertex lighting mode"},
		{"-custinfoparms", "Read scripts/custinfoparms.txt"},
		{"-dark", "Darken lightmap seams"},
		{"-debugaxis", "Color the lightmaps according to the lightmap axis"},
		{"-debugcluster", "Color the lightmaps according to the index of the cluster"},
		{"-debugdeluxe", "Show deluxemaps on the lightmap"},
		{"-debugnormals", "Color the lightmaps according to the direction of the surface normal"},
		{"-debugorigin", "Color the lightmaps according to the origin of the luxels"},
		{"-debugsurfaces, -debugsurface", "Color the lightmaps according to the index of the surface"},
		{"-debugunused", "This option does nothing"},
		{"-debug", "Mark the lightmaps according to the cluster: unmapped clusters get yellow, occluded ones get pink, flooded ones get blue overlay color, otherwise red"},
		{"-deluxemode 0", "Use modelspace deluxemaps (DarkPlaces)"},
		{"-deluxemode 1", "Use tangentspace deluxemaps"},
		{"-deluxe, -deluxemap", "Enable deluxemapping (light direction maps)"},
		{"-dirtdebug, -debugdirt", "Store the dirtmaps as lightmaps for debugging"},
		{"-dirtdepth", "Dirtmapping depth"},
		{"-dirtgain", "Dirtmapping exponent"},
		{"-dirtmode 0", "Ordered direction dirtmapping"},
		{"-dirtmode 1", "Randomized direction dirtmapping"},
		{"-dirtscale", "Dirtmapping scaling factor"},
		{"-dirty", "Enable dirtmapping"},
		{"-dump", "Dump radiosity from `-bounce` into numbered MAP file prefabs"},
		{"-export", "Export lightmaps when compile finished (like `-export` mode)"},
		{"-exposure <F>", "Lightmap exposure to better support overbright spots"},
		{"-external", "Force external lightmaps even if at size of internal lightmaps"},
		{"-extravisnudge", "Broken feature to nudge the luxel origin to a better vis cluster"},
		{"-extrawide", "Deprecated alias for `-super 2 -filter`"},
		{"-extra", "Deprecated alias for `-super 2`"},
		{"-fastbounce", "Use `-fast` style lighting for radiosity"},
		{"-faster", "Use a faster falloff curve for lighting; also implies `-fast`"},
		{"-fastgrid", "Use `-fast` style lighting for the light grid"},
		{"-fast", "Ignore tiny light contributions"},
		{"-filter", "Lightmap filtering"},
		{"-floodlight", "Enable floodlight (zero-effort somewhat decent lighting)"},
		{"-gamma <F>", "Lightmap gamma"},
		{"-gridambientscale <F>", "Scaling factor for the light grid ambient components only"},
		{"-gridscale <F>", "Scaling factor for the light grid only"},
		{"-keeplights", "Keep light entities in the BSP file after compile"},
		{"-lightmapdir <directory>", "Directory to store external lightmaps (default: same as map name without extension)"},
		{"-lightmapsize <N>", "Size of lightmaps to generate (must be a power of two)"},
		{"-lomem", "Low memory but slower lighting mode"},
		{"-lowquality", "Low quality floodlight (appears to currently break floodlight)"},
		{"-minsamplesize <N>", "Sets minimum lightmap resolution in luxels/qu"},
		{"-nocollapse", "Do not collapse identical lightmaps"},
		{"-nodeluxe, -nodeluxemap", "Disable deluxemapping"},
		{"-nogrid", "Disable grid light calculation (makes all entities fullbright)"},
		{"-nolightmapsearch", "Do not optimize lightmap packing for GPU memory usage (as doing so costs fps)"},
		{"-normalmap", "Color the lightmaps according to the direction of the surface normal (TODO is this identical to `-debugnormals`?)"},
		{"-nostyle, -nostyles", "Disable support for light styles"},
		{"-nosurf", "Disable tracing against surfaces (only uses BSP nodes then)"},
		{"-notrace", "Disable shadow occlusion"},
		{"-novertex", "Disable vertex lighting"},
		{"-patchshadows", "Cast shadows from patches"},
		{"-pointscale <F, `-point` F>", "Scaling factor for point lights (light entities)"},
		{"-q3", "Use nonlinear falloff curve by default (like Q3A)"},
		{"-samplescale <F>", "Scales all lightmap resolutions"},
		{"-samplesize <N>", "Sets default lightmap resolution in luxels/qu"},
		{"-samples <N>", "Adaptive supersampling quality"},
		{"-scale <F>", "Scaling factor for all light types"},
		{"-shadeangle <A>", "Angle for phong shading"},
		{"-shade", "Enable phong shading at default shade angle"},
		{"-skyscale <F, `-sky` F>", "Scaling factor for sky and sun light"},
		{"-smooth", "Deprecated alias for `-samples 2`"},
		{"-style, -styles", "Enable support for light styles"},
		{"-sunonly", "Only compute sun light"},
		{"-super <N, `-supersample` N>", "Ordered grid supersampling quality"},
		{"-thresh <F>", "Triangle subdivision threshold"},
		{"-trianglecheck", "Broken check that should ensure luxels apply to the right triangle"},
		{"-trisoup", "Convert brush faces to triangle soup"},
		{"-wolf", "Use linear falloff curve by default (like W:ET)"},
	};

	HelpOptions("Light Stage", 0, 80, light, sizeof(light)/sizeof(struct HelpOption));
}

void HelpAnalize()
{
	struct HelpOption analize[] = {
		{"-analyze <filename.bsp>", "Switch that enters this mode"},
		{"-lumpswap", "Swap byte order in the lumps"},
	};

	HelpOptions("Analyzing BSP-like file structure", 0, 80, analize, sizeof(analize)/sizeof(struct HelpOption));
}
void HelpScale()
{
	struct HelpOption scale[] = {
		{"-scale <S filename.bsp>", "Scale uniformly"},
		{"-scale <SX SY SZ filename.bsp>", "Scale non-uniformly"},
		{"-scale -tex <S filename.bsp>", "Scale uniformly without texture lock"},
		{"-scale -tex <SX SY SZ filename.bsp>", "Scale non-uniformly without texture lock"},
	};
	HelpOptions("Scaling", 0, 80, scale, sizeof(scale)/sizeof(struct HelpOption));
}
void HelpConvert()
{
	struct HelpOption convert[] = {
		{"-convert <filename.bsp>", "Switch that enters this mode"},
		{"-de <number>", "Distance epsilon for the conversion"},
		{"-format <converter>", "Select the converter (available: map, ase, or game names)"},
		{"-ne <F>", "Normal epsilon for the conversion"},
		{"-shadersasbitmap", "(only for ase) use the shader names as \\*BITMAP key so they work as prefabs"},
	};

	HelpOptions("Converting & Decompiling", 0, 80, convert, sizeof(convert)/sizeof(struct HelpOption));
}

void HelpExport()
{
	struct HelpOption exportl[] = {
		{"-export <filename.bsp>", "Copies lightmaps from the BSP to `filename/lightmap_0000.tga` ff"}
	};

	HelpOptions("Exporting lightmaps", 0, 80, exportl, sizeof(exportl)/sizeof(struct HelpOption));
}

void HelpFixaas()
{
	struct HelpOption fixaas[] = {
		{"-fixaas <filename.bsp>", "Switch that enters this mode"},
	};

	HelpOptions("Fixing AAS checksum", 0, 80, fixaas, sizeof(fixaas)/sizeof(struct HelpOption));
}

void HelpInfo()
{
	struct HelpOption info[] = {
		{"-info <filename.bsp>", "Switch that enters this mode"},
	};

	HelpOptions("Get info about BSP file", 0, 80, info, sizeof(info)/sizeof(struct HelpOption));
}

void HelpImport()
{
	struct HelpOption import[] = {
		{"-import <filename.bsp>", "Copies lightmaps from `filename/lightmap_0000.tga` ff into the BSP"},
	};

	HelpOptions("Importing lightmaps", 0, 80, import, sizeof(import)/sizeof(struct HelpOption));
}

void HelpMinimap()
{
	struct HelpOption minimap[] = {
		{"-minimap <filename.bsp>", "Creates a minimap of the BSP, by default writes to `../gfx/filename_mini.tga`"},
		{"-black", "Write the minimap as a black-on-transparency RGBA32 image"},
		{"-boost <F>", "Sets the contrast boost value (higher values make a brighter image); contrast boost is somewhat similar to gamma, but continuous even at zero"},
		{"-border <F>", "Sets the amount of border pixels relative to the total image size"},
		{"-gray", "Write the minimap as a white-on-black GRAY8 image"},
		{"-keepaspect", "Ensure the aspect ratio is kept (the minimap is then letterboxed to keep aspect)"},
		{"-minmax <xmin ymin zmin xmax ymax zmax>", "Forces specific map dimensions (note: the minimap actually uses these dimensions, scaled to the target size while keeping aspect with centering, and 1/64 of border appended to all sides)"},
		{"-nokeepaspect", "Do not ensure the aspect ratio is kept (makes it easier to use the image in your code, but looks bad together with sharpening)"},
		{"-o <filename.tga>", "Sets the output file name"},
		{"-random <N>", "Sets the randomized supersampling count (cannot be combined with `-samples`)"},
		{"-samples <N>", "Sets the ordered supersampling count (cannot be combined with `-random`)"},
		{"-sharpen <F>", "Sets the sharpening coefficient"},
		{"-size <N>", "Sets the width and height of the output image"},
		{"-white", "Write the minimap as a white-on-transparency RGBA32 image"},
	};

	HelpOptions("MiniMap", 0, 80, minimap, sizeof(minimap)/sizeof(struct HelpOption));
}

void HelpCommon()
{
	struct HelpOption common[] = {
		{"-connect <address>", "Talk to a NetRadiant instance using a specific XML based protocol"},
		{"-force", "Allow reading some broken/unsupported BSP files e.g. when decompiling, may also crash"},
		{"-fs_basepath <path>", "Sets the given path as main directory of the game (can be used more than once to look in multiple paths)"},
		{"-fs_game <gamename>", "Sets a different game directory name (default for Q3A: baseq3)"},
		{"-fs_homebase <dir>", "Specifies where the user home directory name is on Linux (default for Q3A: .q3a)"},
		{"-game <gamename>", "Load settings for the given game (default: quake3)"},
		{"-subdivisions <F>", "multiplier for patch subdivisions quality"},
		{"-threads <N>", "number of threads to use"},
		{"-v", "Verbose mode"}
	};

	HelpOptions("Common Options", 0, 80, common, sizeof(common)/sizeof(struct HelpOption));

}

void Help(const char* arg)
{
	printf("Usage: q3map2 [stage] [common options...] [stage options...] [stage source file]\n");
	printf("       q3map2 -help [stage]\n\n");

	HelpCommon();

	struct HelpOption stages[] = {
		{"-bsp", "BSP Stage"},
		{"-vis", "VIS Stage"},
		{"-light", "Light Stage"},
		{"-analize", "Analyzing BSP-like file structure"},
		{"-scale", "Scaling"},
		{"-convert", "Converting & Decompiling"},
		{"-export", "Exporting lightmaps"},
		{"-fixaas", "Fixing AAS checksum"},
		{"-info", "Get info about BSP file"},
		{"-import", "Importing lightmaps"},
		{"-minimap", "MiniMap"},
	};
	void(*help_funcs[])() = {
		HelpBsp,
		HelpVis,
		HelpLight,
		HelpAnalize,
		HelpScale,
		HelpConvert,
		HelpExport,
		HelpFixaas,
		HelpInfo,
		HelpImport,
		HelpMinimap
	};

	if ( arg && strlen(arg) > 0 )
	{
		if ( arg[0] == '-' )
			arg++;

		unsigned i;
		for ( i = 0; i < sizeof(stages)/sizeof(struct HelpOption); i++ )
			if ( strcmp(arg, stages[i].name+1) == 0 )
			{
				help_funcs[i]();
				return;
			}
	}

	HelpOptions("Stages", 0, 80, stages, sizeof(stages)/sizeof(struct HelpOption));
}

/*
   main()
   q3map mojo...
 */

int main( int argc, char **argv ){
	int i, r;
	double start, end;


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
			Help(argv[i+1]);
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

	/* ydnar: new path initialization */
	InitPaths( &argc, argv );

	/* set game options */
	if ( !patchSubdivisions ) {
		patchSubdivisions = game->patchSubdivisions;
	}

	/* check if we have enough options left to attempt something */
	if ( argc < 2 ) {
		Error( "Usage: %s [general options] [options] mapfile", argv[ 0 ] );
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
		Sys_Printf( "WARNING: VLight is no longer supported, defaulting to -light -fast instead\n\n" );
		argv[ 1 ] = "-fast";    /* eek a hack */
		r = LightMain( argc, argv );
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

	/* shut down connection */
	Broadcast_Shutdown();

	/* return any error code */
	return r;
}

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



/* dependencies */
#include "q3map2.h"



static void AAS_DData( unsigned char *data, int size ){
	for ( int i = 0; i < size; i++ )
		data[i] ^= (unsigned char) i * 119;
}

/*
   FixAAS()
   resets an aas checksum to match the given BSP
 */

int FixAAS( int argc, char **argv ){
	int length, checksum;
	void        *buffer;
	FILE        *file;
	char aas[ 1024 ];
	const char **ext;
	const char  *exts[] =
	{
		".aas",
		"_b0.aas",
		"_b1.aas",
		NULL
	};


	/* arg checking */
	if ( argc < 2 ) {
		Sys_Printf( "Usage: q3map2 -fixaas [-v] <mapname>\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	path_set_extension( source, ".bsp" );

	/* note it */
	Sys_Printf( "--- FixAAS ---\n" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	length = LoadFile( source, &buffer );

	/* create bsp checksum */
	Sys_Printf( "Creating checksum...\n" );
	checksum = LittleLong( (int)Com_BlockChecksum( buffer, length ) ); // md4 checksum for a block of data
	AAS_DData( (unsigned char *) &checksum, 4 );

	/* write checksum to aas */
	ext = exts;
	while ( *ext )
	{
		/* mangle name */
		strcpy( aas, source );
		path_set_extension( aas, *ext );
		Sys_Printf( "Trying %s\n", aas );
		ext++;

		/* fix it */
		file = fopen( aas, "r+b" );
		if ( !file ) {
			continue;
		}
		if ( fseek( file, 8, SEEK_SET ) != 0 || fwrite( &checksum, 4, 1, file ) != 1 ) {
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

struct abspHeader_t
{
	char ident[ 4 ];
	int version;

	bspLump_t lumps[ 1 ];       /* unknown size */
};

struct abspLumpTest_t
{
	int radix, minCount;
	const char     *name;
};

int AnalyzeBSP( int argc, char **argv ){
	abspHeader_t            *header;
	int size, i, version, offset, length, lumpInt, count;
	char ident[ 5 ];
	void                    *lump;
	float lumpFloat;
	char lumpString[ 1024 ], source[ 1024 ];
	bool lumpSwap = false;
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
	if ( argc < 2 ) {
		Sys_Printf( "Usage: q3map2 -analyze [-lumpswap] [-v] <mapname>\n" );
		return 0;
	}

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* -format map|ase|... */
		if ( striEqual( argv[ i ], "-lumpswap" ) ) {
			Sys_Printf( "Swapped lump structs enabled\n" );
			lumpSwap = true;
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
		memcpy( lumpString, (char*) lump, std::min( (size_t)length, std::size( lumpString ) - 1 ) );
		lumpString[ std::size( lumpString ) - 1 ] = '\0';

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
	char source[ 1024 ];
	int size;
	FILE        *f;


	/* dummy check */
	if ( count < 1 ) {
		Sys_Printf( "No files to dump info for.\n" );
		return -1;
	}

	/* walk file list */
	for ( i = 0; i < count; i++ )
	{
		Sys_Printf( "---------------------------------\n" );

		/* mangle filename and get size */
		strcpy( source, fileNames[ i ] );
		path_set_extension( source, ".bsp" );
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


static void ExtrapolateTexcoords( const float *axyz, const float *ast,
                                  const float *bxyz, const float *bst,
                                  const float *cxyz, const float *cst,
                                  const Vector3& axyz_new, Vector2& ast_out,
                                  const Vector3& bxyz_new, Vector2& bst_out,
                                  const Vector3& cxyz_new, Vector2& cst_out ){
	Vector4 scoeffs, tcoeffs;
	Matrix4 solvematrix;

	const Vector3 norm = vector3_cross( vector3_from_array( bxyz ) - vector3_from_array( axyz ), vector3_from_array( cxyz ) - vector3_from_array( axyz ) );

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

	const double md = matrix4_determinant( solvematrix );
	if ( md * md < 1e-10 ) {
		Sys_Printf( "Cannot invert some matrix, some texcoords aren't extrapolated!" );
		return;
	}

	matrix4_full_invert( solvematrix );

	scoeffs[0] = ast[0];
	scoeffs[1] = bst[0];
	scoeffs[2] = cst[0];
	scoeffs[3] = 0;
	matrix4_transform_vector4( solvematrix, scoeffs );
	tcoeffs[0] = ast[1];
	tcoeffs[1] = bst[1];
	tcoeffs[2] = cst[1];
	tcoeffs[3] = 0;
	matrix4_transform_vector4( solvematrix, tcoeffs );

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
	Vector3 scale;
	Vector3 vec;
	char str[ 1024 ];
	int uniform, axis;
	bool texscale;
	float *old_xyzst = NULL;
	float spawn_ref = 0;


	/* arg checking */
	if ( argc < 3 ) {
		Sys_Printf( "Usage: q3map2 [-v] -scale [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		return 0;
	}

	texscale = false;
	for ( i = 1; i < argc - 2; ++i )
	{
		if ( striEqual( argv[i], "-tex" ) ) {
			texscale = true;
		}
		else if ( striEqual( argv[i], "-spawn_ref" ) ) {
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

	if ( scale == g_vector3_identity ) {
		Sys_Printf( "Usage: q3map2 [-v] -scale [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		Sys_Printf( "Non-zero scale value required.\n" );
		return 0;
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	path_set_extension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();

	/* note it */
	Sys_Printf( "--- ScaleBSP ---\n" );
	Sys_FPrintf( SYS_VRB, "%9zu entities\n", entities.size() );

	/* scale entity keys */
	for ( auto& e : entities )
	{
		/* scale origin */
		if ( e.read_keyvalue( vec, "origin" ) ) {
			if ( entities[i].classname_prefixed( "info_player_" ) ) {
				vec[2] += spawn_ref;
			}
			vec *= scale;
			if ( entities[i].classname_prefixed( "info_player_" ) ) {
				vec[2] -= spawn_ref;
			}
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			e.setKeyValue( "origin", str );
		}

		a = e.floatForKey( "angle" );
		if ( a == -1 || a == -2 ) { // z scale
			axis = 2;
		}
		else if ( fabs( sin( degrees_to_radians( a ) ) ) < 0.707 ) {
			axis = 0;
		}
		else{
			axis = 1;
		}

		/* scale door lip */
		if ( e.read_keyvalue( f, "lip" ) ) {
			f *= scale[axis];
			sprintf( str, "%f", f );
			e.setKeyValue( "lip", str );
		}

		/* scale plat height */
		if ( e.read_keyvalue( f, "height" ) ) {
			f *= scale[2];
			sprintf( str, "%f", f );
			e.setKeyValue( "height", str );
		}

		// TODO maybe allow a definition file for entities to specify which values are scaled how?
	}

	/* scale models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].minmax.mins *= scale;
		bspModels[ i ].minmax.maxs *= scale;
	}

	/* scale nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].minmax.mins *= scale;
		bspNodes[ i ].minmax.maxs *= scale;
	}

	/* scale leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].minmax.mins *= scale;
		bspLeafs[ i ].minmax.maxs *= scale;
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
		bspDrawVerts[i].xyz *= scale;
		bspDrawVerts[i].normal /= scale;
		VectorNormalize( bspDrawVerts[i].normal );
	}

	if ( texscale ) {
		for ( i = 0; i < numBSPDrawSurfaces; i++ )
		{
			switch ( bspDrawSurfaces[i].surfaceType )
			{
			case MST_PLANAR:
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
			bspPlanes[ i ].dist() *= scale[0];
		}
	}
	else
	{
		for ( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[i].normal() /= scale;
			const double len = vector3_length( bspPlanes[i].normal() );
			bspPlanes[i].normal() /= len;
			bspPlanes[i].dist() /= len;
		}
	}

	/* scale gridsize */
	if ( !entities[ 0 ].read_keyvalue( vec, "gridsize" ) ) {
		vec = gridSize;
	}
	vec *= scale;
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	entities[ 0 ].setKeyValue( "gridsize", str );

	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* write the bsp */
	UnparseEntities();
	path_set_extension( source, "_s.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}


/*
   ShiftBSPMain()
   shifts a map: for testing physics with huge coordinates
 */

int ShiftBSPMain( int argc, char **argv ){
	int i;
	Vector3 shift;
	Vector3 vec;
	char str[ 1024 ];


	/* arg checking */
	if ( argc < 3 ) {
		Sys_Printf( "Usage: q3map2 [-v] -shift <value> <mapname>\n" );
		return 0;
	}

	/* get shift */
	shift[2] = shift[1] = shift[0] = atof( argv[ argc - 2 ] );
	if ( argc - 3 >= 1 ) {
		shift[1] = shift[0] = atof( argv[ argc - 3 ] );
	}
	if ( argc - 4 >= 1 ) {
		shift[0] = atof( argv[ argc - 4 ] );
	}


	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	path_set_extension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();

	/* note it */
	Sys_Printf( "--- ShiftBSP ---\n" );
	Sys_FPrintf( SYS_VRB, "%9zu entities\n", entities.size() );

	/* shift entity keys */
	for ( auto& e : entities )
	{
		/* shift origin */
		if ( e.read_keyvalue( vec, "origin" ) ) { // fixme: this doesn't consider originless point entities; group entities with origin will be wrong too
			vec += shift;
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			e.setKeyValue( "origin", str );
		}

	}

	/* shift models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].minmax.mins += shift;
		bspModels[ i ].minmax.maxs += shift;
	}

	/* shift nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].minmax.mins += shift;
		bspNodes[ i ].minmax.maxs += shift;
	}

	/* shift leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].minmax.mins += shift;
		bspLeafs[ i ].minmax.maxs += shift;
	}

	/* shift drawverts */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz += shift;
	}

	/* shift planes */
	for ( i = 0; i < numBSPPlanes; i++ )
	{
		bspPlanes[i].dist() = vector3_dot( bspPlanes[i].normal(), bspPlanes[i].normal() * bspPlanes[i].dist() + shift );
	}

	// fixme: engine says 'light grid mismatch', unless translation is multiple of grid size

	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* write the bsp */
	UnparseEntities();
	path_set_extension( source, "_sh.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}


/*
   PseudoCompileBSP()
   a stripped down ProcessModels
 */
void PseudoCompileBSP( bool need_tree ){
	int models;
	char modelValue[16];
	entity_t *entity;
	face_t *faces;
	tree_t *tree;
	node_t *node;
	brush_t *brush;
	side_t *side;

	SetDrawSurfacesBuffer();
	mapDrawSurfs = safe_calloc( sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	numMapDrawSurfs = 0;

	BeginBSPFile();
	models = 1;
	for ( mapEntityNum = 0; mapEntityNum < entities.size(); mapEntityNum++ )
	{
		/* get entity */
		entity = &entities[ mapEntityNum ];
		if ( entity->brushes == NULL && entity->patches == NULL ) {
			continue;
		}

		if ( mapEntityNum != 0 ) {
			sprintf( modelValue, "*%d", models++ );
			entity->setKeyValue( "model", modelValue );
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
			for ( int j = 0; j < brush->numsides; j++ )
			{
				/* get side */
				side = &brush->sides[ j ];
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
	EndBSPFile( false );
}

/*
   ConvertBSPMain()
   main argument processing function for bsp conversion
 */

int ConvertBSPMain( int argc, char **argv ){
	int i;
	int ( *convertFunc )( char * );
	game_t  *convertGame;
	bool map_allowed, force_bsp, force_map;


	/* set default */
	convertFunc = ConvertBSPToASE;
	convertGame = NULL;
	map_allowed = false;
	force_bsp = false;
	force_map = false;

	/* arg checking */
	if ( argc < 2 ) {
		Sys_Printf( "Usage: q3map2 -convert [-format <ase|obj|map_bp|map>] [-shadersasbitmap|-lightmapsastexcoord|-deluxemapsastexcoord] [-readbsp|-readmap [-meta|-patchmeta]] [-v] <mapname>\n" );
		return 0;
	}

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		/* -format map|ase|... */
		if ( striEqual( argv[ i ], "-format" ) ) {
			i++;
			if ( striEqual( argv[ i ], "ase" ) ) {
				convertFunc = ConvertBSPToASE;
				map_allowed = false;
			}
			else if ( striEqual( argv[ i ], "obj" ) ) {
				convertFunc = ConvertBSPToOBJ;
				map_allowed = false;
			}
			else if ( striEqual( argv[ i ], "map_bp" ) ) {
				convertFunc = ConvertBSPToMap_BP;
				map_allowed = true;
			}
			else if ( striEqual( argv[ i ], "map" ) ) {
				convertFunc = ConvertBSPToMap;
				map_allowed = true;
			}
			else
			{
				convertGame = GetGame( argv[ i ] );
				map_allowed = false;
				if ( convertGame == NULL ) {
					Sys_Printf( "Unknown conversion format \"%s\". Defaulting to ASE.\n", argv[ i ] );
				}
			}
		}
		else if ( striEqual( argv[ i ], "-ne" ) ) {
			normalEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Normal epsilon set to %lf\n", normalEpsilon );
		}
		else if ( striEqual( argv[ i ], "-de" ) ) {
			distanceEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Distance epsilon set to %lf\n", distanceEpsilon );
		}
		else if ( striEqual( argv[ i ], "-shaderasbitmap" ) || striEqual( argv[ i ], "-shadersasbitmap" ) ) {
			shadersAsBitmap = true;
		}
		else if ( striEqual( argv[ i ], "-lightmapastexcoord" ) || striEqual( argv[ i ], "-lightmapsastexcoord" ) ) {
			lightmapsAsTexcoord = true;
		}
		else if ( striEqual( argv[ i ], "-deluxemapastexcoord" ) || striEqual( argv[ i ], "-deluxemapsastexcoord" ) ) {
			lightmapsAsTexcoord = true;
			deluxemap = true;
		}
		else if ( striEqual( argv[ i ], "-readbsp" ) ) {
			force_bsp = true;
		}
		else if ( striEqual( argv[ i ], "-readmap" ) ) {
			force_map = true;
		}
		else if ( striEqual( argv[ i ], "-meta" ) ) {
			meta = true;
		}
		else if ( striEqual( argv[ i ], "-patchmeta" ) ) {
			meta = true;
			patchMeta = true;
		}
		else if ( striEqual( argv[ i ], "-fast" ) ) {
			fast = true;
		}
	}

	LoadShaderInfo();

	/* clean up map name */
	strcpy( source, ExpandArg( argv[i] ) );

	if ( !map_allowed && !force_map ) {
		force_bsp = true;
	}

	if ( force_map || ( !force_bsp && striEqual( path_get_extension( source ), "map" ) && map_allowed ) ) {
		if ( !map_allowed ) {
			Sys_Warning( "the requested conversion should not be done from .map files. Compile a .bsp first.\n" );
		}
		path_set_extension( source, ".map" );
		Sys_Printf( "Loading %s\n", source );
		LoadMapFile( source, false, convertGame == NULL );
		PseudoCompileBSP( convertGame != NULL );
	}
	else
	{
		path_set_extension( source, ".bsp" );
		Sys_Printf( "Loading %s\n", source );
		LoadBSPFile( source );
		ParseEntities();
	}

	/* bsp format convert? */
	if ( convertGame != NULL ) {
		/* set global game */
		game = convertGame;

		/* write bsp */
		path_set_extension( source, "_c.bsp" );
		Sys_Printf( "Writing %s\n", source );
		WriteBSPFile( source );

		/* return to sender */
		return 0;
	}

	/* normal convert */
	return convertFunc( source );
}

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



static bool g_autocaulk = false;

static void autocaulk_write(){
	Sys_FPrintf( SYS_VRB, "--- autocaulk_write ---\n" );
	auto filename = StringOutputStream( 256 )( source, ".caulk" );
	Sys_Printf( "writing %s\n", filename.c_str() );

	FILE* file = SafeOpenWrite( filename, "wt" );

	int fslime = 16;
	ApplySurfaceParm( "slime", &fslime, NULL, NULL );
	int flava = 8;
	ApplySurfaceParm( "lava", &flava, NULL, NULL );

	for ( brush_t* b = entities[0].brushes; b; b = b->next ) {
		fprintf( file, "%i ", b->brushNum );
		shaderInfo_t* contentShader = b->contentShader;
		for( int i = 0; i < b->numsides; ++i ){
			if( b->sides[i].visibleHull || ( b->sides[i].compileFlags & C_NODRAW ) ){
				fprintf( file, "-" );
			}
			else if( contentShader->compileFlags & C_LIQUID ){
				if( contentShader->contentFlags & flava )
					fprintf( file, "l" );
				else if( contentShader->contentFlags & fslime )
					fprintf( file, "s" );
				else
					fprintf( file, "w" );
			}
			else if( b->compileFlags & C_TRANSLUCENT ){
				if( contentShader->compileFlags & C_SOLID )
					fprintf( file, "N" );
				else
					fprintf( file, "n" );
			}
			else{
				fprintf( file, "c" );
			}
		}
		fprintf( file, "\n" );
	}

	fclose( file );
}

/* -------------------------------------------------------------------------------

   functions

   ------------------------------------------------------------------------------- */

/*
   ProcessAdvertisements()
   copies advertisement info into the BSP structures
 */

static void ProcessAdvertisements( void ) {
	const char*         modelKey;
	int modelNum;
	bspModel_t*         adModel;
	bspDrawSurface_t*   adSurface;

	Sys_FPrintf( SYS_VRB, "--- ProcessAdvertisements ---\n" );

	for ( const auto& e : entities ) {

		/* is an advertisement? */
		if ( e.classname_is( "advertisement" ) ) {

			modelKey = e.valueForKey( "model" );

			if ( strlen( modelKey ) > MAX_QPATH - 1 ) {
				Error( "Model Key for entity exceeds ad struct string length." );
			}
			else {
				if ( numBSPAds < MAX_MAP_ADVERTISEMENTS ) {
					bspAds[numBSPAds].cellId = e.intForKey( "cellId" );
					strncpy( bspAds[numBSPAds].model, modelKey, sizeof( bspAds[numBSPAds].model ) );

					modelKey++;
					modelNum = atoi( modelKey );
					adModel = &bspModels[modelNum];

					if ( adModel->numBSPSurfaces != 1 ) {
						Error( "Ad cell id %d has more than one surface.", bspAds[numBSPAds].cellId );
					}

					adSurface = &bspDrawSurfaces[adModel->firstBSPSurface];

					// store the normal for use at run time.. all ad verts are assumed to
					// have identical normals (because they should be a simple rectangle)
					// so just use the first vert's normal
					bspAds[numBSPAds].normal = bspDrawVerts[adSurface->firstVert].normal;

					// store the ad quad for quick use at run time
					if ( adSurface->surfaceType == MST_PATCH ) {
						int v0 = adSurface->firstVert + adSurface->patchHeight - 1;
						int v1 = adSurface->firstVert + adSurface->numVerts - 1;
						int v2 = adSurface->firstVert + adSurface->numVerts - adSurface->patchWidth;
						int v3 = adSurface->firstVert;
						bspAds[numBSPAds].rect[0] = bspDrawVerts[v0].xyz;
						bspAds[numBSPAds].rect[1] = bspDrawVerts[v1].xyz;
						bspAds[numBSPAds].rect[2] = bspDrawVerts[v2].xyz;
						bspAds[numBSPAds].rect[3] = bspDrawVerts[v3].xyz;
					}
					else {
						Error( "Ad cell %d has an unsupported Ad Surface type.", bspAds[numBSPAds].cellId );
					}

					numBSPAds++;
				}
				else {
					Error( "Maximum number of map advertisements exceeded." );
				}
			}
		}
	}

	Sys_FPrintf( SYS_VRB, "%9d in-game advertisements\n", numBSPAds );
}

/*
   SetCloneModelNumbers() - ydnar
   sets the model numbers for brush entities
 */

static void SetCloneModelNumbers( void ){
	int models;
	char modelValue[ 16 ];
	const char  *value, *value2, *value3;


	/* start with 1 (worldspawn is model 0) */
	models = 1;
	for ( std::size_t i = 1; i < entities.size(); ++i )
	{
		/* only entities with brushes or patches get a model number */
		if ( entities[ i ].brushes == NULL && entities[ i ].patches == NULL ) {
			continue;
		}

		/* is this a clone? */
		if( entities[ i ].read_keyvalue( value, "_ins", "_instance", "_clone" ) )
			continue;

		/* add the model key */
		sprintf( modelValue, "*%d", models );
		entities[ i ].setKeyValue( "model", modelValue );

		/* increment model count */
		models++;
	}

	/* fix up clones */
	for ( std::size_t i = 1; i < entities.size(); ++i )
	{
		/* only entities with brushes or patches get a model number */
		if ( entities[ i ].brushes == NULL && entities[ i ].patches == NULL ) {
			continue;
		}

		/* isn't this a clone? */
		if( !entities[ i ].read_keyvalue( value, "_ins", "_instance", "_clone" ) )
			continue;

		/* find an entity with matching clone name */
		for ( std::size_t j = 0; j < entities.size(); ++j )
		{
			/* is this a clone parent? */
			if ( !entities[ j ].read_keyvalue( value2, "_clonename" ) ) {
				continue;
			}

			/* do they match? */
			if ( strEqual( value, value2 ) ) {
				/* get the model num */
				if ( !entities[ j ].read_keyvalue( value3, "model" ) ) {
					Sys_Warning( "Cloned entity %s referenced entity without model\n", value2 );
					continue;
				}
				models = atoi( &value3[ 1 ] );

				/* add the model key */
				sprintf( modelValue, "*%d", models );
				entities[ i ].setKeyValue( "model", modelValue );

				/* nuke the brushes/patches for this entity (fixme: leak!) */
				entities[ i ].brushes = NULL;
				entities[ i ].patches = NULL;
			}
		}
	}
}



/*
   FixBrushSides() - ydnar
   matches brushsides back to their appropriate drawsurface and shader
 */

static void FixBrushSides( entity_t *e ){
	int i;
	mapDrawSurface_t    *ds;
	sideRef_t           *sideRef;
	bspBrushSide_t      *side;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- FixBrushSides ---\n" );

	/* walk list of drawsurfaces */
	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ )
	{
		/* get surface and try to early out */
		ds = &mapDrawSurfs[ i ];
		if ( ds->outputNum < 0 ) {
			continue;
		}

		/* walk sideref list */
		for ( sideRef = ds->sideRef; sideRef != NULL; sideRef = sideRef->next )
		{
			/* get bsp brush side */
			if ( sideRef->side == NULL || sideRef->side->outputNum < 0 ) {
				continue;
			}
			side = &bspBrushSides[ sideRef->side->outputNum ];

			/* set drawsurface */
			side->surfaceNum = ds->outputNum;
			//%	Sys_FPrintf( SYS_VRB, "DS: %7d Side: %7d     ", ds->outputNum, sideRef->side->outputNum );

			/* set shader */
			if ( !strEqual( bspShaders[ side->shaderNum ].shader, ds->shaderInfo->shader ) ) {
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", bspShaders[ side->shaderNum ].shader, ds->shaderInfo->shader );
				side->shaderNum = EmitShader( ds->shaderInfo->shader, &ds->shaderInfo->contentFlags, &ds->shaderInfo->surfaceFlags );
			}
		}
	}
}



/*
   ProcessWorldModel()
   creates a full bsp + surfaces for the worldspawn entity
 */

void ProcessWorldModel( void ){
	entity_t    *e;
	tree_t      *tree;
	face_t      *faces;
	xmlNodePtr polyline, leaknode;
	char level[ 2 ];
	const char  *value;

	/* sets integer blockSize from worldspawn "_blocksize" key if it exists */
	if( entities[ 0 ].read_keyvalue( value, "_blocksize", "blocksize", "chopsize" ) ) {  /* "chopsize" : sof2 */
		/* scan 3 numbers */
		const int s = sscanf( value, "%d %d %d", &blockSize[ 0 ], &blockSize[ 1 ], &blockSize[ 2 ] );

		/* handle legacy case */
		if ( s == 1 || s == 2 ) {
			blockSize[ 1 ] = blockSize[ 2 ] = blockSize[ 0 ];
		}
	}
	Sys_Printf( "block size = { %d %d %d }\n", blockSize[ 0 ], blockSize[ 1 ], blockSize[ 2 ] );

	/* sof2: ignore leaks? */
	const bool ignoreLeaks = entities[ 0 ].boolForKey( "_ignoreleaks", "ignoreleaks" );

	/* begin worldspawn model */
	BeginModel();
	e = &entities[ 0 ];
	e->firstDrawSurf = 0;

	/* ydnar: gs mods */
	ClearMetaTriangles();

	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );

	if ( debugClip ) {
		AddTriangleModels( e );
	}

	/* build an initial bsp tree using all of the sides of all of the structural brushes */
	faces = MakeStructuralBSPFaceList( entities[ 0 ].brushes );
	tree = FaceBSP( faces );
	MakeTreePortals( tree );
	FilterStructuralBrushesIntoTree( e, tree );

	/* see if the bsp is completely enclosed */
	EFloodEntities leakStatus = FloodEntities( tree );
	if ( ignoreLeaks && leakStatus == EFloodEntities::Leaked ) {
		leakStatus = EFloodEntities::Good;
	}

	const bool leaked = ( leakStatus != EFloodEntities::Good );
	if( leaked ){
		Sys_FPrintf( SYS_NOXMLflag | SYS_ERR, "**********************\n" );
		Sys_FPrintf( SYS_NOXMLflag | SYS_ERR, "******* leaked *******\n" );
		Sys_FPrintf( SYS_NOXMLflag | SYS_ERR, "**********************\n" );
		polyline = LeakFile( tree );
		leaknode = xmlNewNode( NULL, (const xmlChar*)"message" );
		xmlNodeAddContent( leaknode, (const xmlChar*)"MAP LEAKED\n" );
		xmlAddChild( leaknode, polyline );
		level[0] = (int) '0' + SYS_ERR;
		level[1] = 0;
		xmlSetProp( leaknode, (const xmlChar*)"level", (const xmlChar*)level );
		xml_SendNode( leaknode );
		if ( leaktest ) {
			Sys_FPrintf( SYS_WRN, "--- MAP LEAKED, ABORTING LEAKTEST ---\n" );
			exit( 0 );
		}
	}

	if ( leakStatus != EFloodEntities::Empty ) { /* if no entities exist, this would accidentally the whole map, and that IS bad */
		/* rebuild a better bsp tree using only the sides that are visible from the inside */
		FillOutside( tree->headnode );

		/* chop the sides to the convex hull of their visible fragments, giving us the smallest polygons */
		ClipSidesIntoTree( e, tree );

		/* build a visible face tree (same thing as the initial bsp tree but after reducing the faces) */
		faces = MakeVisibleBSPFaceList( entities[ 0 ].brushes );
		FreeTree( tree );
		tree = FaceBSP( faces );
		MakeTreePortals( tree );
		FilterStructuralBrushesIntoTree( e, tree );

		if( g_autocaulk ){
			autocaulk_write();
			exit( 0 );
		}

		/* flood again to discard portals in the void (also required for _skybox) */
		FloodEntities( tree );
		FillOutside( tree->headnode );
	}

	/* save out information for visibility processing */
	NumberClusters( tree );
	if ( !leaked ) {
		WritePortalFile( tree );
	}

	/* flood from entities */
	FloodAreas( tree );

	/* create drawsurfs for triangle models */
	if ( !debugClip ) {
		AddTriangleModels( e );
	}

	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );

	/* generate bsp brushes from map brushes */
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );

	/* add references to the detail brushes */
	FilterDetailBrushesIntoTree( e, tree );

	/* drawsurfs that cross fog boundaries will need to be split along the fog boundary */
	if ( !nofog ) {
		FogDrawSurfaces( e );
	}

	/* subdivide each drawsurf as required by shader tesselation */
	if ( !nosubdivide ) {
		SubdivideFaceSurfaces( e, tree );
	}

	/* add in any vertexes required to fix t-junctions */
	if ( !notjunc ) {
		FixTJunctions( e );
	}

	/* ydnar: classify the surfaces */
	ClassifyEntitySurfaces( e );

	/* ydnar: project decals */
	MakeEntityDecals( e );

	/* ydnar: meta surfaces */
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();

	/* ydnar: debug portals */
	if ( debugPortals ) {
		MakeDebugPortalSurfs( tree );
	}

	/* ydnar: fog hull */
	if ( entities[ 0 ].read_keyvalue( value, "_foghull" ) ) {
		const auto shader = String64()( "textures/", value );
		MakeFogHullSurfs( e, tree, shader );
	}

	/* ydnar: bug 645: do flares for lights */
	if( emitFlares ){
		for ( const auto& light : entities )
		{
			/* get light */
			if ( light.classname_is( "light" ) ) {
				/* get flare shader */
				const char *flareShader = NULL;
				if ( light.read_keyvalue( flareShader, "_flareshader" ) || light.boolForKey( "_flare" ) ) {
					/* get specifics */
					const Vector3 origin( light.vectorForKey( "origin" ) );
					Vector3 color( light.vectorForKey( "_color" ) );
					const int lightStyle = light.intForKey( "_style", "style" );
					Vector3 normal;

					/* handle directional spotlights */
					if ( light.read_keyvalue( value, "target" ) ) {
						/* get target light */
						const entity_t *target = FindTargetEntity( value );
						if ( target != NULL ) {
							normal = VectorNormalized( target->vectorForKey( "origin" ) - origin );
						}
					}
					else{
						//%	normal.set( 0 );
						normal = -g_vector3_axis_z;
					}

					if ( colorsRGB ) {
						color[0] = Image_LinearFloatFromsRGBFloat( color[0] );
						color[1] = Image_LinearFloatFromsRGBFloat( color[1] );
						color[2] = Image_LinearFloatFromsRGBFloat( color[2] );
					}

					/* create the flare surface (note shader defaults automatically) */
					DrawSurfaceForFlare( mapEntityNum, origin, normal, color, flareShader, lightStyle );
				}
			}
		}
	}

	/* add references to the final drawsurfs in the apropriate clusters */
	FilterDrawsurfsIntoTree( e, tree );

	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );

	/* finish */
	EndModel( e, tree->headnode );
	FreeTree( tree );
}



/*
   ProcessSubModel()
   creates bsp + surfaces for other brush models
 */

void ProcessSubModel( void ){
	entity_t    *e;
	tree_t      *tree;
	brush_t     *b, *bc;
	node_t      *node;


	/* start a brush model */
	BeginModel();
	e = &entities[ mapEntityNum ];
	e->firstDrawSurf = numMapDrawSurfs;

	/* ydnar: gs mods */
	ClearMetaTriangles();

	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );

	/* allocate a tree */
	node = AllocNode();
	node->planenum = PLANENUM_LEAF;
	tree = AllocTree();
	tree->headnode = node;

	/* add the sides to the tree */
	ClipSidesIntoTree( e, tree );

	/* ydnar: create drawsurfs for triangle models */
	AddTriangleModels( e );

	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );

	/* generate bsp brushes from map brushes */
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );

	/* just put all the brushes in headnode */
	for ( b = e->brushes; b; b = b->next )
	{
		bc = CopyBrush( b );
		bc->next = node->brushlist;
		node->brushlist = bc;
	}

	/* subdivide each drawsurf as required by shader tesselation */
	if ( !nosubdivide ) {
		SubdivideFaceSurfaces( e, tree );
	}

	/* add in any vertexes required to fix t-junctions */
	if ( !notjunc ) {
		FixTJunctions( e );
	}

	/* ydnar: classify the surfaces and project lightmaps */
	ClassifyEntitySurfaces( e );

	/* ydnar: project decals */
	MakeEntityDecals( e );

	/* ydnar: meta surfaces */
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();

	/* add references to the final drawsurfs in the apropriate clusters */
	FilterDrawsurfsIntoTree( e, tree );

	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );

	/* finish */
	EndModel( e, node );
	FreeTree( tree );
}



/*
   ProcessModels()
   process world + other models into the bsp
 */

void ProcessModels( void ){
	bool oldVerbose;
	entity_t    *entity;


	/* preserve -v setting */
	oldVerbose = verbose;

	/* start a new bsp */
	BeginBSPFile();

	/* create map fogs */
	CreateMapFogs();

	/* walk entity list */
	for ( mapEntityNum = 0; mapEntityNum < entities.size(); mapEntityNum++ )
	{
		/* get entity */
		entity = &entities[ mapEntityNum ];
		if ( entity->brushes == NULL && entity->patches == NULL ) {
			continue;
		}

		/* process the model */
		Sys_FPrintf( SYS_VRB, "############### model %i ###############\n", numBSPModels );
		if ( mapEntityNum == 0 ) {
			ProcessWorldModel();
		}
		else{
			ProcessSubModel();
		}

		/* potentially turn off the deluge of text */
		verbose = verboseEntities;
	}

	/* restore -v setting */
	verbose = oldVerbose;

	Sys_FPrintf( SYS_VRB, "%9i bspModels in total\n", numBSPModels );

	/* write fogs */
	EmitFogs();

	/* vortex: emit meta stats */
	EmitMetaStats();
}



/*
   OnlyEnts()
   this is probably broken unless teamed with a radiant version that preserves entity order
 */

void OnlyEnts( void ){
	/* note it */
	Sys_Printf( "--- OnlyEnts ---\n" );

	auto out = StringOutputStream( 256 )( source, ".bsp" );
	LoadBSPFile( out );

	ParseEntities();
	const CopiedString save_cmdline( entities[ 0 ].valueForKey( "_q3map2_cmdline" ) );
	const CopiedString save_version( entities[ 0 ].valueForKey( "_q3map2_version" ) );
	const CopiedString save_gridsize( entities[ 0 ].valueForKey( "gridsize" ) );

	entities.clear();

	LoadShaderInfo();
	LoadMapFile( name, false, false );
	SetModelNumbers();
	SetLightStyles();

	if ( !save_cmdline.empty() ) {
		entities[0].setKeyValue( "_q3map2_cmdline", save_cmdline.c_str() );
	}
	if ( !save_version.empty() ) {
		entities[0].setKeyValue( "_q3map2_version", save_version.c_str() );
	}
	if ( !save_gridsize.empty() ) {
		entities[0].setKeyValue( "gridsize", save_gridsize.c_str() );
	}

	numBSPEntities = entities.size();
	UnparseEntities();

	WriteBSPFile( out );
}



/*
   BSPMain() - ydnar
   handles creation of a bsp from a map file
 */

int BSPMain( int argc, char **argv ){
	int i;
	char tempSource[ 1024 ];
	bool onlyents = false;

	if ( argc >= 2 && striEqual( argv[ 1 ], "-bsp" ) ) {
		Sys_Printf( "-bsp argument unnecessary\n" );
		argv++;
		argc--;
	}

	/* note it */
	Sys_Printf( "--- BSP ---\n" );

	doingBSP = true;
	SetDrawSurfacesBuffer();
	mapDrawSurfs = safe_calloc( sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	numMapDrawSurfs = 0;

	strClear( tempSource );

	/* set standard game flags */
	maxSurfaceVerts = game->maxSurfaceVerts;
	maxSurfaceIndexes = game->maxSurfaceIndexes;
	emitFlares = game->emitFlares;
	texturesRGB = game->texturesRGB;
	colorsRGB = game->colorsRGB;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ )
	{
		if ( striEqual( argv[ i ], "-onlyents" ) ) {
			Sys_Printf( "Running entity-only compile\n" );
			onlyents = true;
		}
		else if ( striEqual( argv[ i ], "-tempname" ) ) {
			strcpy( tempSource, argv[ ++i ] );
		}
		else if ( striEqual( argv[ i ],  "-nowater" ) ) {
			Sys_Printf( "Disabling water\n" );
			nowater = true;
		}
		else if ( striEqual( argv[ i ], "-keeplights" ) ) {
			keepLights = true;
			Sys_Printf( "Leaving light entities on map after compile\n" );
		}
		else if ( striEqual( argv[ i ],  "-nodetail" ) ) {
			Sys_Printf( "Ignoring detail brushes\n" ) ;
			nodetail = true;
		}
		else if ( striEqual( argv[ i ],  "-fulldetail" ) ) {
			Sys_Printf( "Turning detail brushes into structural brushes\n" );
			fulldetail = true;
		}
		else if ( striEqual( argv[ i ],  "-nofog" ) ) {
			Sys_Printf( "Fog volumes disabled\n" );
			nofog = true;
		}
		else if ( striEqual( argv[ i ],  "-nosubdivide" ) ) {
			Sys_Printf( "Disabling brush face subdivision\n" );
			nosubdivide = true;
		}
		else if ( striEqual( argv[ i ],  "-leaktest" ) ) {
			Sys_Printf( "Leaktest enabled\n" );
			leaktest = true;
		}
		else if ( striEqual( argv[ i ],  "-verboseentities" ) ) {
			Sys_Printf( "Verbose entities enabled\n" );
			verboseEntities = true;
		}
		else if ( striEqual( argv[ i ], "-nocurves" ) ) {
			Sys_Printf( "Ignoring curved surfaces (patches)\n" );
			noCurveBrushes = true;
		}
		else if ( striEqual( argv[ i ], "-notjunc" ) ) {
			Sys_Printf( "T-junction fixing disabled\n" );
			notjunc = true;
		}
		else if ( striEqual( argv[ i ], "-fakemap" ) ) {
			Sys_Printf( "Generating fakemap.map\n" );
			fakemap = true;
		}
		else if ( striEqual( argv[ i ],  "-samplesize" ) ) {
			sampleSize = std::max( 1, atoi( argv[ i + 1 ] ) );
			i++;
			Sys_Printf( "Lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
		}
		else if ( striEqual( argv[ i ], "-minsamplesize" ) ) {
			minSampleSize = std::max( 1, atoi( argv[ i + 1 ] ) );
			i++;
			Sys_Printf( "Minimum lightmap sample size set to %dx%d units\n", minSampleSize, minSampleSize );
		}
		else if ( striEqual( argv[ i ],  "-custinfoparms" ) ) {
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = true;
		}

		/* sof2 args */
		else if ( striEqual( argv[ i ], "-rename" ) ) {
			Sys_Printf( "Appending _bsp suffix to misc_model shaders (SOF2)\n" );
			renameModelShaders = true;
		}

		/* ydnar args */
		else if ( striEqual( argv[ i ],  "-ne" ) ) {
			normalEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Normal epsilon set to %f\n", normalEpsilon );
		}
		else if ( striEqual( argv[ i ],  "-de" ) ) {
			distanceEpsilon = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Distance epsilon set to %f\n", distanceEpsilon );
		}
		else if ( striEqual( argv[ i ],  "-mv" ) ) {
			maxLMSurfaceVerts = std::max( 3, atoi( argv[ i + 1 ] ) );
			value_maximize( maxSurfaceVerts, maxLMSurfaceVerts );
			i++;
			Sys_Printf( "Maximum lightmapped surface vertex count set to %d\n", maxLMSurfaceVerts );
		}
		else if ( striEqual( argv[ i ],  "-mi" ) ) {
			maxSurfaceIndexes = std::max( 3, atoi( argv[ i + 1 ] ) );
			Sys_Printf( "Maximum per-surface index count set to %d\n", maxSurfaceIndexes );
			i++;
		}
		else if ( striEqual( argv[ i ], "-np" ) ) {
			npDegrees = std::max( 0.0, atof( argv[ i + 1 ] ) );
			if ( npDegrees > 0.0f ) {
				Sys_Printf( "Forcing nonplanar surfaces with a breaking angle of %f degrees\n", npDegrees );
			}
			i++;
		}
		else if ( striEqual( argv[ i ],  "-snap" ) ) {
			bevelSnap = std::max( 0, atoi( argv[ i + 1 ] ) );
			if ( bevelSnap > 0 ) {
				Sys_Printf( "Snapping brush bevel planes to %d units\n", bevelSnap );
			}
			i++;
		}
		else if ( striEqual( argv[ i ],  "-texrange" ) ) {
			texRange = std::max( 0, atoi( argv[ i + 1 ] ) );
			i++;
			Sys_Printf( "Limiting per-surface texture range to %d texels\n", texRange );
		}
		else if ( striEqual( argv[ i ], "-nohint" ) ) {
			Sys_Printf( "Hint brushes disabled\n" );
			noHint = true;
		}
		else if ( striEqual( argv[ i ], "-flat" ) ) {
			Sys_Printf( "Flatshading enabled\n" );
			flat = true;
		}
		else if ( striEqual( argv[ i ], "-celshader" ) ) {
			++i;
			if ( !strEmpty( argv[ i ] ) ) {
				globalCelShader( "textures/", argv[ i ] );
			}
			else{
				globalCelShader.clear();
			}
			Sys_Printf( "Global cel shader set to \"%s\"\n", globalCelShader.c_str() );
		}
		else if ( striEqual( argv[ i ], "-meta" ) ) {
			Sys_Printf( "Creating meta surfaces from brush faces\n" );
			meta = true;
		}
		else if ( striEqual( argv[ i ], "-metaadequatescore" ) ) {
			metaAdequateScore = std::max( -1, atoi( argv[ i + 1 ] ) );
			i++;
			if ( metaAdequateScore >= 0 ) {
				Sys_Printf( "Setting ADEQUATE meta score to %d (see surface_meta.c)\n", metaAdequateScore );
			}
		}
		else if ( striEqual( argv[ i ], "-metagoodscore" ) ) {
			metaGoodScore = std::max( -1, atoi( argv[ i + 1 ] ) );
			i++;
			if ( metaGoodScore >= 0 ) {
				Sys_Printf( "Setting GOOD meta score to %d (see surface_meta.c)\n", metaGoodScore );
			}
		}
		else if ( striEqual( argv[ i ], "-metamaxbboxdistance" ) ) {
			metaMaxBBoxDistance = atof( argv[ i + 1 ] );
			if ( metaMaxBBoxDistance < 0 ) {
				metaMaxBBoxDistance = -1;
			}
			i++;
			if ( metaMaxBBoxDistance >= 0 ) {
				Sys_Printf( "Setting meta maximum bounding box distance to %f\n", metaMaxBBoxDistance );
			}
		}
		else if ( striEqual( argv[ i ], "-patchmeta" ) ) {
			Sys_Printf( "Creating meta surfaces from patches\n" );
			patchMeta = true;
		}
		else if ( striEqual( argv[ i ], "-flares" ) ) {
			Sys_Printf( "Flare surfaces enabled\n" );
			emitFlares = true;
		}
		else if ( striEqual( argv[ i ], "-noflares" ) ) {
			Sys_Printf( "Flare surfaces disabled\n" );
			emitFlares = false;
		}
		else if ( striEqual( argv[ i ], "-skyfix" ) ) {
			Sys_Printf( "GL_CLAMP sky fix/hack/workaround enabled\n" );
			skyFixHack = true;
		}
		else if ( striEqual( argv[ i ], "-debugsurfaces" ) ) {
			Sys_Printf( "emitting debug surfaces\n" );
			debugSurfaces = true;
		}
		else if ( striEqual( argv[ i ], "-debuginset" ) ) {
			Sys_Printf( "Debug surface triangle insetting enabled\n" );
			debugInset = true;
		}
		else if ( striEqual( argv[ i ], "-debugportals" ) ) {
			Sys_Printf( "Debug portal surfaces enabled\n" );
			debugPortals = true;
		}
		else if ( striEqual( argv[ i ], "-debugclip" ) ) {
			Sys_Printf( "Debug model clip enabled\n" );
			debugClip = true;
		}
		else if ( striEqual( argv[ i ],  "-clipdepth" ) ) {
			clipDepthGlobal = atof( argv[ i + 1 ] );
			i++;
			Sys_Printf( "Model autoclip thickness set to %.3f\n", clipDepthGlobal );
		}
		else if ( striEqual( argv[ i ], "-sRGBtex" ) ) {
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
		}
		else if ( striEqual( argv[ i ], "-nosRGBtex" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
		}
		else if ( striEqual( argv[ i ], "-sRGBcolor" ) ) {
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}
		else if ( striEqual( argv[ i ], "-nosRGBcolor" ) ) {
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}
		else if ( striEqual( argv[ i ], "-nosRGB" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}
		else if ( striEqual( argv[ i ], "-altsplit" ) ) {
			Sys_Printf( "Alternate BSP splitting (by 27) enabled\n" );
			bspAlternateSplitWeights = true;
		}
		else if ( striEqual( argv[ i ], "-deep" ) ) {
			Sys_Printf( "Deep BSP tree generation enabled\n" );
			deepBSP = true;
		}
		else if ( striEqual( argv[ i ], "-maxarea" ) ) {
			Sys_Printf( "Max Area face surface generation enabled\n" );
			maxAreaFaceSurface = true;
		}
		else if ( striEqual( argv[ i ], "-noob" ) ) {
			Sys_Printf( "No oBs!\n" );
			noob = true;
		}
		else if ( striEqual( argv[ i ], "-autocaulk" ) ) {
			Sys_Printf( "\trunning in autocaulk mode\n" );
			g_autocaulk = true;
		}
		else
		{
			Sys_Warning( "Unknown option \"%s\"\n", argv[ i ] );
		}
	}

	/* fixme: print more useful usage here */
	if ( i != ( argc - 1 ) ) {
		Error( "usage: q3map2 [options] mapfile" );
	}

	/* copy source name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );

	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );

	/* delete portal, line and surface files */
	remove( StringOutputStream( 256 )( source, ".prt" ) );
	remove( StringOutputStream( 256 )( source, ".lin" ) );
	//%	remove( StringOutputStream( 256 )( source, ".srf" ) );	/* ydnar */

	/* expand mapname */
	strcpy( name, ExpandArg( argv[ i ] ) );
	if ( !striEqual( path_get_filename_base_end( name ), ".reg" ) ) { /* not .reg */
		/* if we are doing a full map, delete the last saved region map */
		remove( StringOutputStream( 256 )( source, ".reg" ) );
		if ( !onlyents || !striEqual( path_get_filename_base_end( name ), ".ent" ) ) {
			path_set_extension( name, ".map" );   /* .reg and .ent are ok too */
		}
	}

	/* if onlyents, just grab the entites and resave */
	if ( onlyents ) {
		OnlyEnts();
		return 0;
	}

	/* load shaders */
	LoadShaderInfo();

	/* load original file from temp spot in case it was renamed by the editor on the way in */
	if ( !strEmpty( tempSource ) ) {
		LoadMapFile( tempSource, false, g_autocaulk );
	}
	else{
		LoadMapFile( name, false, g_autocaulk );
	}

	/* div0: inject command line parameters */
	InjectCommandLine( argv, 1, argc - 1 );

	/* ydnar: decal setup */
	ProcessDecals();

	/* ydnar: cloned brush model entities */
	SetCloneModelNumbers();

	/* process world and submodels */
	ProcessModels();

	/* set light styles from targetted light entities */
	SetLightStyles();

	/* process in game advertisements */
	ProcessAdvertisements();

	/* finish and write bsp */
	EndBSPFile( true );

	/* remove temp map source file if appropriate */
	if ( !strEmpty( tempSource ) ) {
		remove( tempSource );
	}

	/* return to sender */
	return 0;
}

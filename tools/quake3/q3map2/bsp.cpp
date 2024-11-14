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
	const auto filename = StringStream( source, ".caulk" );
	Sys_Printf( "writing %s\n", filename.c_str() );

	FILE* file = SafeOpenWrite( filename, "wt" );

	int fslime = 0;
	ApplySurfaceParm( "slime", &fslime, NULL, NULL );
	int flava = 0;
	ApplySurfaceParm( "lava", &flava, NULL, NULL );
	// many setups have nodraw shader nonsolid, including vQ3; and nondrawnonsolid also... fall back to caulk in such case
	// it would be better to decide in Radiant, as it has configurable per game common shaders, but it has no solidity info
	const bool nodraw_is_solid = ShaderInfoForShader( "textures/common/nodraw" )->compileFlags & C_SOLID;

	for ( const brush_t& b : entities[0].brushes ) {
		fprintf( file, "%i ", b.brushNum );
		const shaderInfo_t* contentShader = b.contentShader;
		const bool globalFog = ( contentShader->compileFlags & C_FOG )
			&& std::all_of( b.sides.cbegin(), b.sides.cend(), []( const side_t& side ){ return side.visibleHull.empty(); } );
		for( const side_t& side : b.sides ){
			if( !side.visibleHull.empty() || ( side.compileFlags & C_NODRAW ) || globalFog ){
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
			else if( b.compileFlags & C_TRANSLUCENT ){
				if( contentShader->compileFlags & C_SOLID )
					fprintf( file, nodraw_is_solid? "N" : "c" );
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

static void ProcessAdvertisements() {
	Sys_FPrintf( SYS_VRB, "--- ProcessAdvertisements ---\n" );

	for ( const auto& e : entities ) {

		/* is an advertisement? */
		if ( e.classname_is( "advertisement" ) ) {
			bspAdvertisement_t& ad = bspAds.emplace_back();
			ad.cellId = e.intForKey( "cellId" );
			// copy and clear the rest of memory // check for overflow by String64
			const String64 modelKey( e.valueForKey( "model" ) );
			strncpy( ad.model, modelKey, sizeof( ad.model ) );

			const bspModel_t& adModel = bspModels[atoi( modelKey.c_str() + 1 )];

			if ( adModel.numBSPSurfaces != 1 ) {
				Error( "Ad cell id %d has more than one surface.", ad.cellId );
			}

			const bspDrawSurface_t& adSurface = bspDrawSurfaces[adModel.firstBSPSurface];

			// store the normal for use at run time.. all ad verts are assumed to
			// have identical normals (because they should be a simple rectangle)
			// so just use the first vert's normal
			ad.normal = bspDrawVerts[adSurface.firstVert].normal;

			// store the ad quad for quick use at run time
			if ( adSurface.surfaceType == MST_PATCH ) {
				const int v0 = adSurface.firstVert + adSurface.patchHeight - 1;
				const int v1 = adSurface.firstVert + adSurface.numVerts - 1;
				const int v2 = adSurface.firstVert + adSurface.numVerts - adSurface.patchWidth;
				const int v3 = adSurface.firstVert;
				ad.rect[0] = bspDrawVerts[v0].xyz;
				ad.rect[1] = bspDrawVerts[v1].xyz;
				ad.rect[2] = bspDrawVerts[v2].xyz;
				ad.rect[3] = bspDrawVerts[v3].xyz;
			}
			else {
				Error( "Ad cell %d has an unsupported Ad Surface type.", ad.cellId );
			}
		}
	}

	Sys_FPrintf( SYS_VRB, "%9zu in-game advertisements\n", bspAds.size() );
}

/*
   SetCloneModelNumbers() - ydnar
   sets the model numbers for brush entities
 */

static void SetCloneModelNumbers(){
	int models;
	char modelValue[ 16 ];
	const char  *value, *value2, *value3;


	/* start with 1 (worldspawn is model 0) */
	models = 1;
	for ( std::size_t i = 1; i < entities.size(); ++i )
	{
		/* only entities with brushes or patches get a model number */
		if ( entities[ i ].brushes.empty() && entities[ i ].patches == NULL ) {
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
		if ( entities[ i ].brushes.empty() && entities[ i ].patches == NULL ) {
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
				brushlist_t *leak = new brushlist_t( std::move( entities[ i ].brushes ) ); // are brushes referenced elsewhere, so we do not nuke them really?
				entities[ i ].patches = NULL;
			}
		}
	}
}



/*
   FixBrushSides() - ydnar
   matches brushsides back to their appropriate drawsurface and shader
 */

static void FixBrushSides( const entity_t& e ){
	/* note it */
	Sys_FPrintf( SYS_VRB, "--- FixBrushSides ---\n" );

	/* walk list of drawsurfaces */
	for ( int i = e.firstDrawSurf; i < numMapDrawSurfs; ++i )
	{
		/* get surface and try to early out */
		const mapDrawSurface_t& ds = mapDrawSurfs[ i ];
		if ( ds.outputNum < 0 ) {
			continue;
		}

		/* walk sideref list */
		for ( const sideRef_t *sideRef = ds.sideRef; sideRef != NULL; sideRef = sideRef->next )
		{
			/* get bsp brush side */
			if ( sideRef->side == NULL || sideRef->side->outputNum < 0 ) {
				continue;
			}
			bspBrushSide_t& side = bspBrushSides[ sideRef->side->outputNum ];

			/* set drawsurface */
			side.surfaceNum = ds.outputNum;
			//%	Sys_FPrintf( SYS_VRB, "DS: %7d Side: %7d     ", ds.outputNum, sideRef->side->outputNum );

			/* set shader */
			if ( !strEqual( bspShaders[ side.shaderNum ].shader, ds.shaderInfo->shader ) ) {
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", bspShaders[ side.shaderNum ].shader, ds.shaderInfo->shader );
				side.shaderNum = EmitShader( ds.shaderInfo->shader, &ds.shaderInfo->contentFlags, &ds.shaderInfo->surfaceFlags );
			}
		}
	}
}



/*
   ProcessWorldModel()
   creates a full bsp + surfaces for the worldspawn entity
 */

static void ProcessWorldModel( entity_t& e ){
	const char  *value;

	/* sets integer blockSize from worldspawn "_blocksize" key if it exists */
	if( e.read_keyvalue( value, "_blocksize", "blocksize", "chopsize" ) ) {  /* "chopsize" : sof2 */
		/* scan 3 numbers */
		const int s = sscanf( value, "%d %d %d", &blockSize[ 0 ], &blockSize[ 1 ], &blockSize[ 2 ] );

		/* handle legacy case */
		if ( s == 1 || s == 2 ) {
			blockSize[ 1 ] = blockSize[ 2 ] = blockSize[ 0 ];
		}
	}
	Sys_Printf( "block size = { %d %d %d }\n", blockSize[ 0 ], blockSize[ 1 ], blockSize[ 2 ] );

	/* sof2: ignore leaks? */
	const bool ignoreLeaks = e.boolForKey( "_ignoreleaks", "ignoreleaks" );

	/* begin worldspawn model */
	BeginModel( e );
	e.firstDrawSurf = 0;

	/* ydnar: gs mods */
	ClearMetaTriangles();

	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );

	if ( debugClip ) {
		AddTriangleModels( e );
	}

	/* build an initial bsp tree using all of the sides of all of the structural brushes */
	facelist_t faces = MakeStructuralBSPFaceList( e.brushes );
	tree_t tree = FaceBSP( faces );
	MakeTreePortals( tree );
	FilterStructuralBrushesIntoTree( e, tree );

	/* see if the bsp is completely enclosed */
	EFloodEntities leakStatus = FloodEntities( tree );
	if ( ignoreLeaks && leakStatus == EFloodEntities::Leaked ) {
		leakStatus = EFloodEntities::Good;
	}

	const bool leaked = ( leakStatus != EFloodEntities::Good );
	if( leaked ){
		Leak_feedback( tree );
		if ( leaktest ) {
			Sys_FPrintf( SYS_WRN, "--- MAP LEAKED, ABORTING LEAKTEST ---\n" );
			exit( 0 );
		}
	}

	if ( leakStatus != EFloodEntities::Empty ) { /* if no entities exist, this would accidentally the whole map, and that IS bad */
		/* rebuild a better bsp tree using only the sides that are visible from the inside */
		FillOutside( tree.headnode );

		/* chop the sides to the convex hull of their visible fragments, giving us the smallest polygons */
		ClipSidesIntoTree( e, tree );

		/* build a visible face tree (same thing as the initial bsp tree but after reducing the faces) */
		faces = MakeVisibleBSPFaceList( e.brushes );
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
		FillOutside( tree.headnode );
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
	EmitBrushes( e.brushes, &e.firstBrush, &e.numBrushes );

	/* add references to the detail brushes */
	FilterDetailBrushesIntoTree( e, tree );

	/* drawsurfs that cross fog boundaries will need to be split along the fog boundary */
	if ( !nofog ) {
		FogDrawSurfaces( e );
	}

	/* subdivide each drawsurf as required by shader tesselation */
	if ( !nosubdivide ) {
		SubdivideFaceSurfaces( e );
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
	if ( e.read_keyvalue( value, "_foghull" ) ) {
		MakeFogHullSurfs( String64( "textures/", value ) );
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
					DrawSurfaceForFlare( e.mapEntityNum, origin, normal, color, flareShader, lightStyle );
				}
			}
		}
	}

	/* add references to the final drawsurfs in the appropriate clusters */
	FilterDrawsurfsIntoTree( e, tree );

	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );

	/* finish */
	EndModel( e, tree.headnode );
	FreeTree( tree );
}



/*
   ProcessSubModel()
   creates bsp + surfaces for other brush models
 */

static void ProcessSubModel( entity_t& e ){
	/* start a brush model */
	BeginModel( e );
	e.firstDrawSurf = numMapDrawSurfs;

	/* ydnar: gs mods */
	ClearMetaTriangles();

	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );

	/* allocate a tree */
	tree_t tree{};
	tree.headnode = AllocNode();
	tree.headnode->planenum = PLANENUM_LEAF;

	/* add the sides to the tree */
	ClipSidesIntoTree( e, tree );

	/* ydnar: create drawsurfs for triangle models */
	AddTriangleModels( e );

	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );

	/* generate bsp brushes from map brushes */
	EmitBrushes( e.brushes, &e.firstBrush, &e.numBrushes );

	/* just put all the brushes in headnode */
	tree.headnode->brushlist = e.brushes;

	/* subdivide each drawsurf as required by shader tesselation */
	if ( !nosubdivide ) {
		SubdivideFaceSurfaces( e );
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

	/* add references to the final drawsurfs in the appropriate clusters */
	FilterDrawsurfsIntoTree( e, tree );

	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );

	/* finish */
	EndModel( e, tree.headnode );
	FreeTree( tree );
}



/*
   ProcessModels()
   process world + other models into the bsp
 */

static void ProcessModels(){
	/* preserve -v setting */
	const bool oldVerbose = verbose;

	/* start a new bsp */
	BeginBSPFile();

	/* create map fogs */
	CreateMapFogs();

	/* walk entity list */
	for ( size_t entityNum = 0; entityNum < entities.size(); ++entityNum )
	{
		/* get entity */
		entity_t& entity = entities[ entityNum ];
		if ( entity.brushes.empty() && entity.patches == NULL ) {
			continue;
		}

		/* process the model */
		Sys_FPrintf( SYS_VRB, "############### model %zu ###############\n", bspModels.size() );
		if ( entityNum == 0 ) {
			ProcessWorldModel( entity );
		}
		else{
			ProcessSubModel( entity );
		}

		/* potentially turn off the deluge of text */
		verbose = verboseEntities;
	}

	/* restore -v setting */
	verbose = oldVerbose;

	Sys_FPrintf( SYS_VRB, "%9zu bspModels in total\n", bspModels.size() );

	/* write fogs */
	EmitFogs();

	/* vortex: emit meta stats */
	EmitMetaStats();
}



/*
   OnlyEnts()
   this is probably broken unless teamed with a radiant version that preserves entity order
 */

static void OnlyEnts( const char *filename ){
	/* note it */
	Sys_Printf( "--- OnlyEnts ---\n" );

	const auto out = StringStream( source, ".bsp" );
	LoadBSPFile( out );

	ParseEntities();
	const CopiedString save_cmdline( entities[ 0 ].valueForKey( "_q3map2_cmdline" ) );
	const CopiedString save_version( entities[ 0 ].valueForKey( "_q3map2_version" ) );
	const CopiedString save_gridsize( entities[ 0 ].valueForKey( "gridsize" ) );

	entities.clear();

	LoadShaderInfo();
	LoadMapFile( filename, false, false );
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

int BSPMain( Args& args ){
	char tempSource[ 1024 ];
	bool onlyents = false;

	if ( args.takeFront( "-bsp" ) ) {
		Sys_Printf( "-bsp argument unnecessary\n" );
	}

	/* note it */
	Sys_Printf( "--- BSP ---\n" );

	doingBSP = true;
	mapDrawSurfs = safe_calloc( sizeof( mapDrawSurface_t ) * max_map_draw_surfs );
	numMapDrawSurfs = 0;

	strClear( tempSource );

	/* set standard game flags */
	maxLMSurfaceVerts = g_game->maxLMSurfaceVerts;
	maxSurfaceVerts = g_game->maxSurfaceVerts;
	maxSurfaceIndexes = g_game->maxSurfaceIndexes;
	emitFlares = g_game->emitFlares;
	texturesRGB = g_game->texturesRGB;
	colorsRGB = g_game->colorsRGB;
	keepLights = g_game->keepLights;

	/* process arguments */
	/* fixme: print more useful usage here */
	if ( args.empty() ) {
		Error( "usage: q3map2 [options] mapfile" );
	}

	const char *fileName = args.takeBack();
	auto argsToInject = args.getVector();
	{
		while ( args.takeArg( "-onlyents" ) ) {
			Sys_Printf( "Running entity-only compile\n" );
			onlyents = true;
		}
		while ( args.takeArg( "-tempname" ) ) {
			strcpy( tempSource, args.takeNext() );
		}
		while ( args.takeArg( "-nowater" ) ) {
			Sys_Printf( "Disabling water\n" );
			nowater = true;
		}
		while ( args.takeArg( "-keeplights" ) ) {
			keepLights = true;
			Sys_Printf( "Leaving light entities on map after compile\n" );
		}
		while ( args.takeArg( "-keepmodels" ) ) {
			keepModels = true;
			Sys_Printf( "Leaving misc_model entities on map after compile\n" );
		}
		while ( args.takeArg( "-nodetail" ) ) {
			Sys_Printf( "Ignoring detail brushes\n" );
			nodetail = true;
		}
		while ( args.takeArg( "-fulldetail" ) ) {
			Sys_Printf( "Turning detail brushes into structural brushes\n" );
			fulldetail = true;
		}
		while ( args.takeArg( "-nofog" ) ) {
			Sys_Printf( "Fog volumes disabled\n" );
			nofog = true;
		}
		while ( args.takeArg( "-nosubdivide" ) ) {
			Sys_Printf( "Disabling brush face subdivision\n" );
			nosubdivide = true;
		}
		while ( args.takeArg( "-leaktest" ) ) {
			Sys_Printf( "Leaktest enabled\n" );
			leaktest = true;
		}
		while ( args.takeArg( "-verboseentities" ) ) {
			Sys_Printf( "Verbose entities enabled\n" );
			verboseEntities = true;
		}
		while ( args.takeArg( "-nocurves" ) ) {
			Sys_Printf( "Ignoring curved surfaces (patches)\n" );
			noCurveBrushes = true;
		}
		while ( args.takeArg( "-notjunc" ) ) {
			Sys_Printf( "T-junction fixing disabled\n" );
			notjunc = true;
		}
		while ( args.takeArg( "-fakemap" ) ) {
			Sys_Printf( "Generating fakemap.map\n" );
			fakemap = true;
		}
		while ( args.takeArg( "-samplesize" ) ) {
			sampleSize = std::max( 1, atoi( args.takeNext() ) );
			Sys_Printf( "Lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
		}
		while ( args.takeArg( "-minsamplesize" ) ) {
			minSampleSize = std::max( 1, atoi( args.takeNext() ) );
			Sys_Printf( "Minimum lightmap sample size set to %dx%d units\n", minSampleSize, minSampleSize );
		}
		while ( args.takeArg( "-custinfoparms" ) ) {
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = true;
		}

		/* sof2 args */
		while ( args.takeArg( "-rename" ) ) {
			Sys_Printf( "Appending _bsp suffix to misc_model shaders (SOF2)\n" );
			renameModelShaders = true;
		}

		/* ydnar args */
		while ( args.takeArg( "-ne" ) ) {
			normalEpsilon = atof( args.takeNext() );
			Sys_Printf( "Normal epsilon set to %f\n", normalEpsilon );
		}
		while ( args.takeArg( "-de" ) ) {
			distanceEpsilon = atof( args.takeNext() );
			Sys_Printf( "Distance epsilon set to %f\n", distanceEpsilon );
		}
		while ( args.takeArg( "-mv" ) ) {
			maxLMSurfaceVerts = std::max( 3, atoi( args.takeNext() ) );
			value_maximize( maxSurfaceVerts, maxLMSurfaceVerts );
			Sys_Printf( "Maximum lightmapped surface vertex count set to %d\n", maxLMSurfaceVerts );
		}
		while ( args.takeArg( "-mi" ) ) {
			maxSurfaceIndexes = std::max( 3, atoi( args.takeNext() ) );
			Sys_Printf( "Maximum per-surface index count set to %d\n", maxSurfaceIndexes );
		}
		while ( args.takeArg( "-np" ) ) {
			npDegrees = std::max( 0.0, atof( args.takeNext() ) );
			if ( npDegrees > 0.0f ) {
				Sys_Printf( "Forcing nonplanar surfaces with a breaking angle of %f degrees\n", npDegrees );
			}
		}
		while ( args.takeArg( "-snap" ) ) {
			bevelSnap = std::max( 0, atoi( args.takeNext() ) );
			if ( bevelSnap > 0 ) {
				Sys_Printf( "Snapping brush bevel planes to %d units\n", bevelSnap );
			}
		}
		while ( args.takeArg( "-nobrushsnap" ) ) {
			Sys_Printf( "Brush vertices snapping disabled\n" );
			g_brushSnap = false;
		}
		while ( args.takeArg( "-nohint" ) ) {
			Sys_Printf( "Hint brushes disabled\n" );
			noHint = true;
		}
		while ( args.takeArg( "-flat" ) ) {
			Sys_Printf( "Flatshading enabled\n" );
			flat = true;
		}
		while ( args.takeArg( "-celshader" ) ) {
			globalCelShader( "textures/", args.takeNext() );
			Sys_Printf( "Global cel shader set to \"%s\"\n", globalCelShader.c_str() );
		}
		while ( args.takeArg( "-meta" ) ) {
			Sys_Printf( "Creating meta surfaces from brush faces\n" );
			meta = true;
		}
		while ( args.takeArg( "-metaadequatescore" ) ) {
			metaAdequateScore = std::max( -1, atoi( args.takeNext() ) );
			if ( metaAdequateScore >= 0 ) {
				Sys_Printf( "Setting ADEQUATE meta score to %d (see surface_meta.c)\n", metaAdequateScore );
			}
		}
		while ( args.takeArg( "-metagoodscore" ) ) {
			metaGoodScore = std::max( -1, atoi( args.takeNext() ) );
			if ( metaGoodScore >= 0 ) {
				Sys_Printf( "Setting GOOD meta score to %d (see surface_meta.c)\n", metaGoodScore );
			}
		}
		while ( args.takeArg( "-patchmeta" ) ) {
			Sys_Printf( "Creating meta surfaces from patches\n" );
			patchMeta = true;
		}
		while ( args.takeArg( "-flares" ) ) {
			Sys_Printf( "Flare surfaces enabled\n" );
			emitFlares = true;
		}
		while ( args.takeArg( "-noflares" ) ) {
			Sys_Printf( "Flare surfaces disabled\n" );
			emitFlares = false;
		}
		while ( args.takeArg( "-skyfix" ) ) {
			Sys_Printf( "GL_CLAMP sky fix/hack/workaround enabled\n" );
			skyFixHack = true;
		}
		while ( args.takeArg( "-debugsurfaces" ) ) {
			Sys_Printf( "emitting debug surfaces\n" );
			debugSurfaces = true;
		}
		while ( args.takeArg( "-debuginset" ) ) {
			Sys_Printf( "Debug surface triangle insetting enabled\n" );
			debugInset = true;
		}
		while ( args.takeArg( "-debugportals" ) ) {
			Sys_Printf( "Debug portal surfaces enabled\n" );
			debugPortals = true;
		}
		while ( args.takeArg( "-debugclip" ) ) {
			Sys_Printf( "Debug model clip enabled\n" );
			debugClip = true;
		}
		while ( args.takeArg(  "-clipdepth" ) ) {
			clipDepthGlobal = atof( args.takeNext() );
			Sys_Printf( "Model autoclip thickness set to %.3f\n", clipDepthGlobal );
		}
		while ( args.takeArg( "-sRGBtex" ) ) {
			texturesRGB = true;
			Sys_Printf( "Textures are in sRGB\n" );
		}
		while ( args.takeArg( "-nosRGBtex" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
		}
		while ( args.takeArg( "-sRGBcolor" ) ) {
			colorsRGB = true;
			Sys_Printf( "Colors are in sRGB\n" );
		}
		while ( args.takeArg( "-nosRGBcolor" ) ) {
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}
		while ( args.takeArg( "-nosRGB" ) ) {
			texturesRGB = false;
			Sys_Printf( "Textures are linear\n" );
			colorsRGB = false;
			Sys_Printf( "Colors are linear\n" );
		}
		while ( args.takeArg( "-altsplit" ) ) {
			Sys_Printf( "Alternate BSP splitting (by 27) enabled\n" );
			bspAlternateSplitWeights = true;
		}
		while ( args.takeArg( "-deep" ) ) {
			Sys_Printf( "Deep BSP tree generation enabled\n" );
			deepBSP = true;
		}
		while ( args.takeArg( "-maxarea" ) ) {
			Sys_Printf( "Max Area face surface generation enabled\n" );
			maxAreaFaceSurface = true;
		}
		while ( args.takeArg( "-noob" ) ) {
			Sys_Printf( "No oBs!\n" );
			g_noob = true;
		}
		while ( args.takeArg( "-globalflag" ) ) {
			ApplySurfaceParm( args.takeNext(), nullptr, &g_globalSurfaceFlags, nullptr );
			Sys_Printf( "g_globalSurfaceFlags: 0x%.8X\n", g_globalSurfaceFlags );
		}
		while ( args.takeArg( "-autocaulk" ) ) {
			Sys_Printf( "\trunning in autocaulk mode\n" );
			g_autocaulk = true;
		}
		while( !args.empty() )
		{
			Sys_Warning( "Unknown option \"%s\"\n", args.takeFront() );
		}
	}

	/* copy source name */
	strcpy( source, ExpandArg( fileName ) );
	StripExtension( source );

	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );

	/* delete portal, line and surface files */
	remove( StringStream( source, ".prt" ) );
	remove( StringStream( source, ".lin" ) );
	//%	remove( StringStream( source, ".srf" ) );	/* ydnar */

	/* if we are doing a full map, delete the last saved region map */
	if ( !path_extension_is( fileName, "reg" ) )
		remove( StringStream( source, ".reg" ) );

	/* expand mapname */
	StringOutputStream mapFileName( 256 );
	if ( path_extension_is( fileName, "reg" ) || ( onlyents && path_extension_is( fileName, "ent" ) ) )
		mapFileName << ExpandArg( fileName );
	else
		mapFileName << PathExtensionless( ExpandArg( fileName ) ) << ".map";

	/* if onlyents, just grab the entites and resave */
	if ( onlyents ) {
		OnlyEnts( mapFileName );
		return 0;
	}

	/* load shaders */
	LoadShaderInfo();

	/* load original file from temp spot in case it was renamed by the editor on the way in */
	if ( !strEmpty( tempSource ) ) {
		LoadMapFile( tempSource, false, g_autocaulk );
	}
	else{
		LoadMapFile( mapFileName, false, g_autocaulk );
	}

	/* div0: inject command line parameters */
	InjectCommandLine( "-bsp", argsToInject );

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

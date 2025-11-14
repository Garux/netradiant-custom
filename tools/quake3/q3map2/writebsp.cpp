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
#include "bspfile_rbsp.h"



/*
   EmitShader()
   emits a bsp shader entry
 */

int EmitShader( const char *shader, const int *contentFlags, const int *surfaceFlags ){
	/* handle special cases */
	if ( shader == nullptr ) {
		shader = "noshader";
	}

	/* try to find an existing shader */
	for ( size_t i = 0; i < bspShaders.size(); ++i )
	{
		/* ydnar: handle custom surface/content flags */
		if ( surfaceFlags != nullptr && bspShaders[ i ].surfaceFlags != *surfaceFlags ) {
			continue;
		}
		if ( contentFlags != nullptr && bspShaders[ i ].contentFlags != *contentFlags ) {
			continue;
		}
		if ( !doingBSP ){
			const shaderInfo_t& si = ShaderInfoForShader( shader );
			if ( !strEmptyOrNull( si.remapShader ) ) {
				shader = si.remapShader;
			}
		}
		/* compare name */
		if ( striEqual( shader, bspShaders[ i ].shader ) ) {
			return i;
		}
	}

	/* get shaderinfo */
	const shaderInfo_t& si = ShaderInfoForShader( shader );

	/* emit a new shader */
	const int i = bspShaders.size(); // store index
	bspShader_t& bspShader = bspShaders.emplace_back();

	strcpy( bspShader.shader, si.shader );
	/* handle custom content/surface flags */
	bspShader.surfaceFlags = ( surfaceFlags != nullptr )? *surfaceFlags : si.surfaceFlags;
	bspShader.contentFlags = ( contentFlags != nullptr )? *contentFlags : si.contentFlags;

	/* recursively emit any damage shaders */
	if ( !strEmptyOrNull( si.damageShader ) ) {
		Sys_FPrintf( SYS_VRB, "Shader %s has damage shader %s\n", si.shader.c_str(), si.damageShader );
		EmitShader( si.damageShader, nullptr, nullptr );
	}

	/* return index */
	return i;
}



/*
   EmitPlanes()
   there is no opportunity to discard planes, because all of the original
   brushes will be saved in the map
 */

static void EmitPlanes(){
	bspPlanes.reserve( mapplanes.size() );
	/* walk plane list */
	for ( const plane_t& plane : mapplanes )
	{
		bspPlanes.push_back( plane.plane );
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9zu BSP planes\n", bspPlanes.size() );
}



/*
   EmitLeaf()
   emits a leafnode to the bsp file
 */

static void EmitLeaf( node_t *node ){
	bspLeaf_t& leaf = bspLeafs.emplace_back();

	leaf.cluster = node->cluster;
	leaf.area = node->area;

	/* emit bounding box */
	leaf.minmax.maxs = node->minmax.maxs;
	leaf.minmax.mins = node->minmax.mins;

	/* emit leaf brushes */
	leaf.firstBSPLeafBrush = bspLeafBrushes.size();
	for ( const brush_t& b : node->brushlist )
	{
		bspLeafBrushes.push_back( b.original->outputNum );
	}

	leaf.numBSPLeafBrushes = bspLeafBrushes.size() - leaf.firstBSPLeafBrush;

	/* emit leaf surfaces */
	if ( node->opaque ) {
		return;
	}

	/* add the drawSurfRef_t drawsurfs */
	leaf.firstBSPLeafSurface = bspLeafSurfaces.size();
	for ( const drawSurfRef_t *dsr = node->drawSurfReferences; dsr; dsr = dsr->nextRef )
	{
		bspLeafSurfaces.push_back( dsr->outputNum );
	}

	leaf.numBSPLeafSurfaces = bspLeafSurfaces.size() - leaf.firstBSPLeafSurface;
}


/*
   EmitDrawNode_r()
   recursively emit the bsp nodes
 */

static int EmitDrawNode_r( node_t *node ){
	/* check for leafnode */
	if ( node->planenum == PLANENUM_LEAF ) {
		EmitLeaf( node );
		return -int( bspLeafs.size() );
	}

	/* emit a node */
	const int id = bspNodes.size();
	{
		bspNode_t& bnode = bspNodes.emplace_back();

		bnode.minmax.mins = node->minmax.mins;
		bnode.minmax.maxs = node->minmax.maxs;

		if ( node->planenum & 1 ) {
			Error( "WriteDrawNodes_r: odd planenum" );
		}
		bnode.planeNum = node->planenum;
	}

	//
	// recursively output the other nodes
	//
	for ( int i = 0; i < 2; ++i )
	{
		// reference node by id, as it may be reallocated
		if ( node->children[i]->planenum == PLANENUM_LEAF ) {
			bspNodes[id].children[i] = -int( bspLeafs.size() + 1 );
			EmitLeaf( node->children[i] );
		}
		else
		{
			bspNodes[id].children[i] = bspNodes.size();
			EmitDrawNode_r( node->children[i] );
		}
	}

	return id;
}



/*
   ============
   SetModelNumbers
   ============
 */
void SetModelNumbers(){
	int models = 1;
	for ( std::size_t i = 1; i < entities.size(); ++i ) {
		if ( !entities[i].brushes.empty() || !entities[i].patches.empty() ) {
			entities[i].setKeyValue( "model", models++, "*%i" );
		}
	}
}




/*
   SetLightStyles()
   sets style keys for entity lights
 */

void SetLightStyles(){
	int j, numStyles;
	char lightTargets[ MAX_SWITCHED_LIGHTS ][ 64 ];
	int lightStyles[ MAX_SWITCHED_LIGHTS ];
	int numStrippedLights = 0;

	/* -keeplights option: force lights to be kept and ignore what the map file says */
	if ( keepLights ) {
		entities[0].setKeyValue( "_keepLights", "1" ); // -keeplights is -bsp option; save key in worldspawn to pass it to the next stages
	}

	/* ydnar: determine if we keep lights in the bsp */
	entities[ 0 ].read_keyvalue( keepLights, "_keepLights" );

	/* any light that is controlled (has a targetname) must have a unique style number generated for it in RBSP */
	numStyles = 0;
	for ( std::size_t i = 1; i < entities.size(); ++i )
	{
		entity_t& e = entities[ i ];

		if ( !e.classname_prefixed( "light" ) ) {
			continue;
		}
		const char *t;
		if ( !( e.read_keyvalue( t, "targetname" ) && g_game->load == LoadRBSPFile ) ) { // only RBSP has switchable light styles
			/* ydnar: strip the light from the BSP file */
			if ( !keepLights ) {
				e.epairs.clear();
				numStrippedLights++;
			}

			/* next light */
			continue;
		}

		/* get existing style */
		const int style = e.intForKey( "style" );
		if ( !style_is_valid( style ) ) {
			Error( "Invalid lightstyle (%d) on entity %zu", style, i );
		}

		/* find this targetname */
		for ( j = 0; j < numStyles; ++j )
			if ( lightStyles[ j ] == style && strEqual( lightTargets[ j ], t ) ) {
				break;
			}

		/* add a new style */
		if ( j >= numStyles ) {
			if ( numStyles == MAX_SWITCHED_LIGHTS ) {
				Error( "MAX_SWITCHED_LIGHTS (%d) exceeded, reduce the number of lights with targetnames", MAX_SWITCHED_LIGHTS );
			}
			strcpy( lightTargets[ j ], t );
			lightStyles[ j ] = style;
			numStyles++;
		}

		/* set explicit style */
		e.setKeyValue( "style", MAX_SWITCHED_LIGHTS + j );

		/* set old style */
		if ( style != LS_NORMAL ) {
			e.setKeyValue( "switch_style", style );
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d light entities stripped\n", numStrippedLights );
}

// reverts SetLightStyles() effect for decompilation purposes
void UnSetLightStyles(){
	for ( entity_t& e : entities ){
		if ( e.classname_prefixed( "light" ) && !strEmpty( e.valueForKey( "targetname" ) ) && !strEmpty( e.valueForKey( "style" ) ) ) {
			e.setKeyValue( "style", e.intForKey( "switch_style" ) ); // value or 0, latter is fine too
		}
	}
}



/*
   BeginBSPFile()
   starts a new bsp file
 */

void BeginBSPFile(){
	/* these values may actually be initialized if the file existed when loaded, so clear them explicitly */
	bspModels.clear();
	bspNodes.clear();
	bspBrushSides.clear();
	bspLeafSurfaces.clear();
	bspLeafBrushes.clear();

	/* leave leaf 0 as an error, because leafs are referenced as negative number nodes */
	bspLeafs.resize( 1 );

	/* ydnar: gs mods: set the first 6 drawindexes to 0 1 2 2 1 3 for triangles and quads */
	bspDrawIndexes = { 0, 1, 2, 0, 2, 3 };
}



/*
   EndBSPFile()
   finishes a new bsp and writes to disk
 */

void EndBSPFile( bool do_write ){
	Sys_FPrintf( SYS_VRB, "--- EndBSPFile ---\n" );

	EmitPlanes();

	numBSPEntities = entities.size();
	UnparseEntities();

	if ( do_write ) {
		/* write the surface extra file */
		WriteSurfaceExtraFile( source );

		/* write the bsp */
		WriteBSPFile( StringStream( source, ".bsp" ) );
	}
}



/*
   EmitBrushes()
   writes the entity brush list to the bsp
 */

void EmitBrushes( entity_t& e ){
	/* set initial brush */
	e.firstBrush = bspBrushes.size();
	e.numBrushes = e.brushes.size();

	/* walk list of brushes */
	for ( brush_t& b : e.brushes )
	{
		/* get bsp brush */
		b.outputNum = bspBrushes.size();
		bspBrush_t& db = bspBrushes.emplace_back();

		db.shaderNum = EmitShader( b.contentShader->shader, &b.contentShader->contentFlags, &b.contentShader->surfaceFlags );
		db.firstSide = bspBrushSides.size();
		db.numSides = b.sides.size();

		/* walk sides */
		for ( side_t& side : b.sides )
		{
			/* emit side */
			side.outputNum = bspBrushSides.size();
			bspBrushSide_t& cp = bspBrushSides.emplace_back();
			cp.planeNum = side.planenum;

			/* emit shader */
			if ( side.shaderInfo ) {
				cp.shaderNum = EmitShader( side.shaderInfo->shader, &side.shaderInfo->contentFlags, &side.shaderInfo->surfaceFlags );
			}
			else if( side.bevel ) { /* emit surfaceFlags for bevels to get correct physics at walkable brush edges and vertices */
				cp.shaderNum = EmitShader( nullptr, nullptr, &side.surfaceFlags );
			}
			else{
				cp.shaderNum = EmitShader( nullptr, nullptr, nullptr );
			}
		}
	}
}



/*
   EmitFogs() - ydnar
   turns map fogs into bsp fogs
 */

void EmitFogs(){
	/* walk list */
	for ( size_t i = 0; i < mapFogs.size(); ++i )
	{
		const fog_t& fog = mapFogs[i];
		bspFog_t& bspFog = bspFogs.emplace_back();
		/* set shader */
		strcpy( bspFog.shader, fog.si->shader );

		/* global fog doesn't have an associated brush */
		if ( fog.brush == nullptr ) {
			bspFog.brushNum = -1;
			bspFog.visibleSide = -1;
		}
		else
		{
			/* set brush */
			bspFog.brushNum = fog.brush->outputNum;
			bspFog.visibleSide = -1; // default to something sensible, not just zero index

			/* try to use forced visible side */
			if ( fog.visibleSide >= 0 ) {
				bspFog.visibleSide = fog.visibleSide;
				continue;
			}

			/* find visible axial side */
			for ( size_t j = 6; j-- > 0; ) // prioritize +Z (index 5) then -Z (index 4) in ambiguous case; fogged pit is assumed as most likely case
			{
				if ( !fog.brush->sides[ j ].visibleHull.empty() && !( fog.brush->sides[ j ].compileFlags & C_NODRAW ) ) {
					bspFog.visibleSide = j;
					break;
				}
			}
			/* try other sides */
			if( bspFog.visibleSide < 0 ){
				for ( size_t j = 6; j < fog.brush->sides.size(); ++j )
				{
					if ( !fog.brush->sides[ j ].visibleHull.empty() && !( fog.brush->sides[ j ].compileFlags & C_NODRAW ) ) {
						bspFog.visibleSide = j;
						break;
					}
				}
			}
			Sys_Printf( "Fog %zu has visible side %i\n", i, bspFog.visibleSide );
		}
	}

	/* warn about overflow */
	if( g_game->load == LoadRBSPFile ){
		if( mapFogs.size() > MAX_RBSP_FOGS )
			Sys_Warning( "MAX_RBSP_FOGS (%i) exceeded (%zu). Visual inconsistencies are expected.\n", MAX_RBSP_FOGS, mapFogs.size() );
	}
	else if( mapFogs.size() > MAX_IBSP_FOGS )
		Sys_Warning( "MAX_IBSP_FOGS (%i) exceeded (%zu). Visual inconsistencies are expected.\n", MAX_IBSP_FOGS, mapFogs.size() );
}



/*
   BeginModel()
   sets up a new brush model
 */

void BeginModel( const entity_t& e ){
	MinMax minmax;
	MinMax lgMinmax;          /* ydnar: lightgrid mins/maxs */

	/* bound the brushes */
	for ( const brush_t& b : e.brushes )
	{
		/* ignore non-real brushes (origin, etc) */
		if ( b.sides.empty() ) {
			continue;
		}
		minmax.extend( b.minmax );

		/* ydnar: lightgrid bounds */
		if ( b.compileFlags & C_LIGHTGRID ) {
			lgMinmax.extend( b.minmax );
		}
	}

	/* bound patches */
	for ( const parseMesh_t& p : e.patches )
	{
		for ( const bspDrawVert_t& vert : Span( p.mesh.verts, p.mesh.numVerts() ) )
			minmax.extend( vert.xyz );
	}

	/* get model */
	bspModel_t& mod = bspModels.emplace_back();

	/* ydnar: lightgrid mins/maxs */
	if ( lgMinmax.valid() ) {
		/* use lightgrid bounds */
		mod.minmax = lgMinmax;
	}
	else
	{
		/* use brush/patch bounds */
		mod.minmax = minmax;
	}

	/* note size */
	Sys_FPrintf( SYS_VRB, "BSP bounds: { %f %f %f } { %f %f %f }\n", minmax.mins[0], minmax.mins[1], minmax.mins[2], minmax.maxs[0], minmax.maxs[1], minmax.maxs[2] );
	if ( lgMinmax.valid() )
		Sys_FPrintf( SYS_VRB, "Lightgrid bounds: { %f %f %f } { %f %f %f }\n", lgMinmax.mins[0], lgMinmax.mins[1], lgMinmax.mins[2], lgMinmax.maxs[0], lgMinmax.maxs[1], lgMinmax.maxs[2] );

	/* set firsts */
	mod.firstBSPSurface = bspDrawSurfaces.size();
	mod.firstBSPBrush = bspBrushes.size();
}




/*
   EndModel()
   finish a model's processing
 */

void EndModel( const entity_t& e, node_t *headnode ){
	/* note it */
	Sys_FPrintf( SYS_VRB, "--- EndModel ---\n" );

	/* emit the bsp */
	bspModel_t& mod = bspModels.back();
	EmitDrawNode_r( headnode );

	/* set surfaces and brushes */
	mod.numBSPSurfaces = bspDrawSurfaces.size() - mod.firstBSPSurface;
	mod.firstBSPBrush = e.firstBrush;
	mod.numBSPBrushes = e.numBrushes;
}

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



int numFogFragments;
int numFogPatchFragments;



/*
   DrawSurfToMesh()
   converts a patch drawsurface to a mesh_t
 */

mesh_t *DrawSurfToMesh( mapDrawSurface_t *ds ){
	mesh_t      *m;


	m = safe_malloc( sizeof( *m ) );
	m->width = ds->patchWidth;
	m->height = ds->patchHeight;
	m->verts = safe_malloc( sizeof( m->verts[ 0 ] ) * m->width * m->height );
	memcpy( m->verts, ds->verts, sizeof( m->verts[ 0 ] ) * m->width * m->height );

	return m;
}



/*
   SplitMeshByPlane()
   chops a mesh by a plane
 */

void SplitMeshByPlane( mesh_t *in, const Plane3f& plane, mesh_t **front, mesh_t **back ){
	int w, h, split;
	float d[MAX_PATCH_SIZE][MAX_PATCH_SIZE];
	bspDrawVert_t   *dv, *v1, *v2;
	int c_front, c_back, c_on;
	mesh_t  *f, *b;
	int i;
	float frac;
	int frontAprox, backAprox;

	for ( i = 0 ; i < 2 ; i++ ) {
		dv = in->verts;
		c_front = 0;
		c_back = 0;
		c_on = 0;
		for ( h = 0 ; h < in->height ; h++ ) {
			for ( w = 0 ; w < in->width ; w++, dv++ ) {
				d[h][w] = plane3_distance_to_point( plane, dv->xyz );
				if ( d[h][w] > ON_EPSILON ) {
					c_front++;
				}
				else if ( d[h][w] < -ON_EPSILON ) {
					c_back++;
				}
				else {
					c_on++;
				}
			}
		}

		*front = NULL;
		*back = NULL;

		if ( !c_front ) {
			*back = in;
			return;
		}
		if ( !c_back ) {
			*front = in;
			return;
		}

		// find a split point
		split = -1;
		for ( w = 0 ; w < in->width - 1 ; w++ ) {
			if ( ( d[0][w] < 0 ) != ( d[0][w + 1] < 0 ) ) {
				if ( split == -1 ) {
					split = w;
					break;
				}
			}
		}

		if ( split == -1 ) {
			if ( i == 1 ) {
				Sys_FPrintf( SYS_WRN | SYS_VRBflag, "No crossing points in patch\n" );
				*front = in;
				return;
			}

			in = TransposeMesh( in );
			InvertMesh( in );
			continue;
		}

		// make sure the split point stays the same for all other rows
		for ( h = 1 ; h < in->height ; h++ ) {
			for ( w = 0 ; w < in->width - 1 ; w++ ) {
				if ( ( d[h][w] < 0 ) != ( d[h][w + 1] < 0 ) ) {
					if ( w != split ) {
						Sys_Printf( "multiple crossing points for patch -- can't clip\n" );
						*front = in;
						return;
					}
				}
			}
			if ( ( d[h][split] < 0 ) == ( d[h][split + 1] < 0 ) ) {
				Sys_Printf( "differing crossing points for patch -- can't clip\n" );
				*front = in;
				return;
			}
		}

		break;
	}


	// create two new meshes
	f = safe_malloc( sizeof( *f ) );
	f->width = split + 2;
	if ( !( f->width & 1 ) ) {
		f->width++;
		frontAprox = 1;
	}
	else {
		frontAprox = 0;
	}
	if ( f->width > MAX_PATCH_SIZE ) {
		Error( "MAX_PATCH_SIZE after split" );
	}
	f->height = in->height;
	f->verts = safe_malloc( sizeof( f->verts[0] ) * f->width * f->height );

	b = safe_malloc( sizeof( *b ) );
	b->width = in->width - split;
	if ( !( b->width & 1 ) ) {
		b->width++;
		backAprox = 1;
	}
	else {
		backAprox = 0;
	}
	if ( b->width > MAX_PATCH_SIZE ) {
		Error( "MAX_PATCH_SIZE after split" );
	}
	b->height = in->height;
	b->verts = safe_malloc( sizeof( b->verts[0] ) * b->width * b->height );

	if ( d[0][0] > 0 ) {
		*front = f;
		*back = b;
	}
	else {
		*front = b;
		*back = f;
	}

	// distribute the points
	for ( w = 0 ; w < in->width ; w++ ) {
		for ( h = 0 ; h < in->height ; h++ ) {
			if ( w <= split ) {
				f->verts[ h * f->width + w ] = in->verts[ h * in->width + w ];
			}
			else {
				b->verts[ h * b->width + w - split + backAprox ] = in->verts[ h * in->width + w ];
			}
		}
	}

	// clip the crossing line
	for ( h = 0; h < in->height; h++ )
	{
		dv = &f->verts[ h * f->width + split + 1 ];
		v1 = &in->verts[ h * in->width + split ];
		v2 = &in->verts[ h * in->width + split + 1 ];

		frac = d[h][split] / ( d[h][split] - d[h][split + 1] );

		/* interpolate */
		//%	for( i = 0; i < 10; i++ )
		//%		dv->xyz[ i ] = v1->xyz[ i ] + frac * (v2->xyz[ i ] - v1->xyz[ i ]);
		//%	dv->xyz[10] = 0;	// set all 4 colors to 0
		LerpDrawVertAmount( v1, v2, frac, dv );

		if ( frontAprox ) {
			f->verts[ h * f->width + split + 2 ] = *dv;
		}
		b->verts[ h * b->width ] = *dv;
		if ( backAprox ) {
			b->verts[ h * b->width + 1 ] = *dv;
		}
	}

	/*
	   PrintMesh( in );
	   Sys_Printf("\n");
	   PrintMesh( f );
	   Sys_Printf("\n");
	   PrintMesh( b );
	   Sys_Printf("\n");
	 */

	FreeMesh( in );
}


/*
   ChopPatchSurfaceByBrush()
   chops a patch up by a fog brush
 */

bool ChopPatchSurfaceByBrush( entity_t *e, mapDrawSurface_t *ds, brush_t *b ){
	int i, j;
	side_t      *s;
	mesh_t      *outside[MAX_BRUSH_SIDES];
	int numOutside;
	mesh_t      *m, *front, *back;
	mapDrawSurface_t    *newds;

	m = DrawSurfToMesh( ds );
	numOutside = 0;

	// only split by the top and bottom planes to avoid
	// some messy patch clipping issues

	for ( i = 4 ; i <= 5 ; i++ ) {
		s = &b->sides[ i ];
		const plane_t& plane = mapplanes[ s->planenum ];

		SplitMeshByPlane( m, plane.plane, &front, &back );

		if ( !back ) {
			// nothing actually contained inside
			for ( j = 0 ; j < numOutside ; j++ ) {
				FreeMesh( outside[j] );
			}
			return false;
		}
		m = back;

		if ( front ) {
			if ( numOutside == MAX_BRUSH_SIDES ) {
				Error( "MAX_BRUSH_SIDES" );
			}
			outside[ numOutside ] = front;
			numOutside++;
		}
	}

	/* all of outside fragments become seperate drawsurfs */
	numFogPatchFragments += numOutside;
	for ( i = 0; i < numOutside; i++ )
	{
		/* transpose and invert the chopped patch (fixes potential crash. fixme: why?) */
		outside[ i ] = TransposeMesh( outside[ i ] );
		InvertMesh( outside[ i ] );

		/* ydnar: do this the hacky right way */
		newds = AllocDrawSurface( ESurfaceType::Patch );
		memcpy( newds, ds, sizeof( *ds ) );
		newds->patchWidth = outside[ i ]->width;
		newds->patchHeight = outside[ i ]->height;
		newds->numVerts = outside[ i ]->width * outside[ i ]->height;
		newds->verts = safe_malloc( newds->numVerts * sizeof( *newds->verts ) );
		memcpy( newds->verts, outside[ i ]->verts, newds->numVerts * sizeof( *newds->verts ) );

		/* free the source mesh */
		FreeMesh( outside[ i ] );
	}

	/* only rejigger this patch if it was chopped */
	//%	Sys_Printf( "Inside: %d x %d\n", m->width, m->height );
	if ( numOutside > 0 ) {
		/* transpose and invert the chopped patch (fixes potential crash. fixme: why?) */
		m = TransposeMesh( m );
		InvertMesh( m );

		/* replace ds with m */
		ds->patchWidth = m->width;
		ds->patchHeight = m->height;
		ds->numVerts = m->width * m->height;
		free( ds->verts );
		ds->verts = safe_malloc( ds->numVerts * sizeof( *ds->verts ) );
		memcpy( ds->verts, m->verts, ds->numVerts * sizeof( *ds->verts ) );
	}

	/* free the source mesh and return */
	FreeMesh( m );
	return true;
}



/*
   WindingFromDrawSurf()
   creates a winding from a surface's verts
 */

winding_t *WindingFromDrawSurf( mapDrawSurface_t *ds ){
	winding_t   *w;
	int i;

	// we use the first point of the surface, maybe something more clever would be useful
	// (actually send the whole draw surface would be cool?)
	if ( ds->numVerts >= MAX_POINTS_ON_WINDING ) {
		const int max = std::min( ds->numVerts, 256 );
		Vector3 p[256];

		for ( i = 0; i < max; i++ ) {
			p[i] = ds->verts[i].xyz;
		}

		xml_Winding( "WindingFromDrawSurf failed: MAX_POINTS_ON_WINDING exceeded", p, max, true );
	}

	w = AllocWinding( ds->numVerts );
	w->numpoints = ds->numVerts;
	for ( i = 0 ; i < ds->numVerts ; i++ ) {
		w->p[i] = ds->verts[i].xyz;
	}
	return w;
}



/*
   ChopFaceSurfaceByBrush()
   chops up a face drawsurface by a fog brush, with a potential fragment left inside
 */

bool ChopFaceSurfaceByBrush( entity_t *e, mapDrawSurface_t *ds, brush_t *b ){
	int i, j;
	side_t              *s;
	winding_t           *w;
	winding_t           *front, *back;
	winding_t           *outside[ MAX_BRUSH_SIDES ];
	int numOutside;
	mapDrawSurface_t    *newds;


	/* dummy check */
	if ( ds->sideRef == NULL || ds->sideRef->side == NULL ) {
		return false;
	}

	/* initial setup */
	w = WindingFromDrawSurf( ds );
	numOutside = 0;

	/* chop by each brush side */
	for ( i = 0; i < b->numsides; i++ )
	{
		/* get brush side and plane */
		s = &b->sides[ i ];
		const plane_t& plane = mapplanes[ s->planenum ];

		/* handle coplanar outfacing (don't fog) */
		if ( ds->sideRef->side->planenum == s->planenum ) {
			return false;
		}

		/* handle coplanar infacing (keep inside) */
		if ( ( ds->sideRef->side->planenum ^ 1 ) == s->planenum ) {
			continue;
		}

		/* general case */
		ClipWindingEpsilonStrict( w, plane.plane, ON_EPSILON, &front, &back ); /* strict; if plane is "almost identical" to face, both ways to continue can be wrong, so we better not fog it */
		FreeWinding( w );

		if ( back == NULL ) {
			/* nothing actually contained inside */
			for ( j = 0; j < numOutside; j++ )
				FreeWinding( outside[ j ] );
			return false;
		}

		if ( front != NULL ) {
			if ( numOutside == MAX_BRUSH_SIDES ) {
				Error( "MAX_BRUSH_SIDES" );
			}
			outside[ numOutside ] = front;
			numOutside++;
		}

		w = back;
	}

	/* fixme: celshaded surface fragment errata */

	/* all of outside fragments become seperate drawsurfs */
	numFogFragments += numOutside;
	s = ds->sideRef->side;
	for ( i = 0; i < numOutside; i++ )
	{
		newds = DrawSurfaceForSide( e, ds->mapBrush, s, outside[ i ] );
		newds->fogNum = ds->fogNum;
		FreeWinding( outside[ i ] );
	}

	/* ydnar: the old code neglected to snap to 0.125 for the fragment
	          inside the fog brush, leading to sparklies. this new code does
	          the right thing and uses the original surface's brush side */

	/* build a drawsurf for it */
	newds = DrawSurfaceForSide( e, ds->mapBrush, s, w );
	if ( newds == NULL ) {
		return false;
	}

	/* copy new to original */
	ClearSurface( ds );
	memcpy( ds, newds, sizeof( mapDrawSurface_t ) );

	/* didn't really add a new drawsurface... :) */
	numMapDrawSurfs--;

	/* return ok */
	return true;
}



/*
   FogDrawSurfaces()
   call after the surface list has been pruned, before tjunction fixing
 */

void FogDrawSurfaces( entity_t *e ){
	int i, j, fogNum;
	fog_t               *fog;
	mapDrawSurface_t    *ds;
	int fogged, numFogged;
	int numBaseDrawSurfs;


	/* note it */
	Sys_FPrintf( SYS_VRB, "----- FogDrawSurfs -----\n" );

	/* reset counters */
	numFogged = 0;
	numFogFragments = 0;

	/* walk fog list */
	for ( fogNum = 0; fogNum < numMapFogs; fogNum++ )
	{
		/* get fog */
		fog = &mapFogs[ fogNum ];

		/* clip each surface into this, but don't clip any of the resulting fragments to the same brush */
		numBaseDrawSurfs = numMapDrawSurfs;
		for ( i = 0; i < numBaseDrawSurfs; i++ )
		{
			/* get the drawsurface */
			ds = &mapDrawSurfs[ i ];

			/* no fog? */
			if ( ds->shaderInfo->noFog ) {
				continue;
			}

			/* global fog doesn't have a brush */
			if ( fog->brush == NULL ) {
				/* don't re-fog already fogged surfaces */
				if ( ds->fogNum >= 0 ) {
					continue;
				}
				fogged = 1;
			}
			else
			{
				/* find drawsurface bounds */
				MinMax minmax;
				for ( j = 0; j < ds->numVerts; j++ )
					minmax.extend( ds->verts[ j ].xyz );

				/* check against the fog brush */
				if( !minmax.test( fog->brush->minmax ) ){
					continue; /* no intersection */
				}

				/* ydnar: gs mods: handle the various types of surfaces */
				switch ( ds->type )
				{
				/* handle brush faces */
				case ESurfaceType::Face:
					fogged = ChopFaceSurfaceByBrush( e, ds, fog->brush );
					break;

				/* handle patches */
				case ESurfaceType::Patch:
					fogged = ChopPatchSurfaceByBrush( e, ds, fog->brush );
					break;

				/* handle triangle surfaces (fixme: split triangle surfaces) */
				case ESurfaceType::Triangles:
				case ESurfaceType::ForcedMeta:
				case ESurfaceType::Meta:
					fogged = 1;
					break;

				/* no fogging */
				default:
					fogged = 0;
					break;
				}
			}

			/* is this surface fogged? */
			if ( fogged ) {
				numFogged += fogged;
				ds->fogNum = fogNum;
			}
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d fog polygon fragments\n", numFogFragments );
	Sys_FPrintf( SYS_VRB, "%9d fog patch fragments\n", numFogPatchFragments );
	Sys_FPrintf( SYS_VRB, "%9d fogged drawsurfs\n", numFogged );
}



/*
   FogForPoint() - ydnar
   gets the fog number for a point in space
 */

int FogForPoint( const Vector3& point, float epsilon ){
	int fogNum, i, j;
	bool inside;
	brush_t         *brush;


	/* start with bogus fog num */
	fogNum = defaultFogNum;

	/* walk the list of fog volumes */
	for ( i = 0; i < numMapFogs; i++ )
	{
		/* sof2: global fog doesn't reference a brush */
		if ( mapFogs[ i ].brush == NULL ) {
			fogNum = i;
			continue;
		}

		/* get fog brush */
		brush = mapFogs[ i ].brush;

		/* check point against all planes */
		inside = true;
		for ( j = 0; j < brush->numsides && inside; j++ )
		{
			if ( plane3_distance_to_point( mapplanes[ brush->sides[ j ].planenum ].plane, point ) > epsilon ) {
				inside = false;
			}
		}

		/* if inside, return the fog num */
		if ( inside ) {
			//%	Sys_Printf( "FogForPoint: %f, %f, %f in fog %d\n", point[ 0 ], point[ 1 ], point[ 2 ], i );
			return i;
		}
	}

	/* if the point made it this far, it's not inside any fog volumes (or inside global fog) */
	return fogNum;
}



/*
   FogForBounds() - ydnar
   gets the fog number for a bounding box
 */

int FogForBounds( const MinMax& minmax, float epsilon ){
	int fogNum, i;

	/* start with bogus fog num */
	fogNum = defaultFogNum;

	/* init */
	float bestVolume = 0.0f;

	/* walk the list of fog volumes */
	for ( i = 0; i < numMapFogs; i++ )
	{
		/* sof2: global fog doesn't reference a brush */
		if ( mapFogs[ i ].brush == NULL ) {
			fogNum = i;
			continue;
		}

		/* get fog brush */
		brush_t *brush = mapFogs[ i ].brush;

		/* get bounds */
		const MinMax fogMinmax( brush->minmax.mins - Vector3( epsilon ),
		                        brush->minmax.maxs + Vector3( epsilon ) );
		/* check against bounds */
		if( !minmax.test( fogMinmax ) ){
			continue; /* no overlap */
		}
		const Vector3 overlap( std::max( 1.f, std::min( minmax.maxs[0], fogMinmax.maxs[0] ) - std::max( minmax.mins[0], fogMinmax.mins[0] ) ),
		                       std::max( 1.f, std::min( minmax.maxs[1], fogMinmax.maxs[1] ) - std::max( minmax.mins[1], fogMinmax.mins[1] ) ),
		                       std::max( 1.f, std::min( minmax.maxs[2], fogMinmax.maxs[2] ) - std::max( minmax.mins[2], fogMinmax.mins[2] ) ) );

		/* get volume */
		const float volume = overlap[0] * overlap[1] * overlap[2];

		/* test against best volume */
		if ( volume > bestVolume ) {
			bestVolume = volume;
			fogNum = i;
		}
	}

	/* if the point made it this far, it's not inside any fog volumes (or inside global fog) */
	return fogNum;
}



/*
   CreateMapFogs() - ydnar
   generates a list of map fogs
 */

void CreateMapFogs( void ){
	int j;
	brush_t     *brush;
	fog_t       *fog;


	/* skip? */
	if ( nofog ) {
		return;
	}

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- CreateMapFogs ---\n" );

	/* walk entities */
	for ( const auto& e : entities )
	{
		/* walk entity brushes */
		for ( brush = e.brushes; brush != NULL; brush = brush->next )
		{
			/* ignore non-fog brushes */
			if ( !brush->contentShader->fogParms ) {
				continue;
			}

			/* test limit */
			if ( numMapFogs >= MAX_MAP_FOGS ) {
				Error( "Exceeded MAX_MAP_FOGS (%d)", MAX_MAP_FOGS );
			}

			/* set up fog */
			fog = &mapFogs[ numMapFogs++ ];
			fog->si = brush->contentShader;
			fog->brush = brush;
			fog->visibleSide = -1;

			/* if shader specifies an explicit direction, then find a matching brush side with an opposed normal */
			if ( vector3_length( fog->si->fogDir ) ) {
				/* flip it */
				const Vector3 invFogDir = -fog->si->fogDir;

				/* find the brush side */
				for ( j = 0; j < brush->numsides; j++ )
				{
					if ( VectorCompare( invFogDir, mapplanes[ brush->sides[ j ].planenum ].normal() ) ) {
						fog->visibleSide = j;
						//%	Sys_Printf( "Brush num: %d Side num: %d\n", fog->brushNum, fog->visibleSide );
						break;
					}
				}
			}
		}
	}

	/* ydnar: global fog */
	const char  *globalFog;
	if ( entities[ 0 ].read_keyvalue( globalFog, "_fog", "fog" ) ) {
		/* test limit */
		if ( numMapFogs >= MAX_MAP_FOGS ) {
			Error( "Exceeded MAX_MAP_FOGS (%d) trying to add global fog", MAX_MAP_FOGS );
		}

		/* note it */
		Sys_FPrintf( SYS_VRB, "Map has global fog shader %s\n", globalFog );

		/* set up fog */
		fog = &mapFogs[ numMapFogs++ ];
		fog->si = ShaderInfoForShaderNull( globalFog );
		if ( fog->si == NULL ) {
			Error( "Invalid shader \"%s\" referenced trying to add global fog", globalFog );
		}
		fog->brush = NULL;
		fog->visibleSide = -1;

		/* set as default fog */
		defaultFogNum = numMapFogs - 1;

		/* mark all worldspawn brushes as fogged */
		for ( brush = entities[ 0 ].brushes; brush != NULL; brush = brush->next )
			ApplySurfaceParm( "fog", &brush->contentFlags, NULL, &brush->compileFlags );
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d fogs\n", numMapFogs );
}

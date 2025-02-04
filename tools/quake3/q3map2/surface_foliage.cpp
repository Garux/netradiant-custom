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

   Foliage code for Wolfenstein: Enemy Territory by ydnar@splashdamage.com

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"



#define MAX_FOLIAGE_INSTANCES   8192

static int numFoliageInstances;
static foliageInstance_t foliageInstances[ MAX_FOLIAGE_INSTANCES ];



/*
   SubdivideFoliageTriangle_r()
   recursively subdivides a triangle until the triangle is smaller than
   the desired density, then pseudo-randomly sets a point
 */

static void SubdivideFoliageTriangle_r( mapDrawSurface_t *ds, const foliage_t& foliage, const TriRef& tri ){
	int max;


	/* limit test */
	if ( numFoliageInstances >= MAX_FOLIAGE_INSTANCES ) {
		return;
	}

	/* plane test */
	{
		Plane3f plane;

		/* make a plane */
		if ( !PlaneFromPoints( plane, tri[ 0 ]->xyz, tri[ 1 ]->xyz, tri[ 2 ]->xyz ) ) {
			return;
		}

		/* if normal is too far off vertical, then don't place an instance */
		if ( plane.normal().z() < 0.5f ) {
			return;
		}
	}

	/* subdivide calc */
	{
		/* get instance */
		foliageInstance_t *fi = &foliageInstances[ numFoliageInstances ];

		/* find the longest edge and split it */
		max = -1;
		float maxDist = 0.0f;
		fi->xyz.set( 0 );
		fi->normal.set( 0 );
		for ( int i = 0; i < 3; i++ )
		{
			const float dist = vector3_length_squared( tri[ i ]->xyz - tri[ ( i + 1 ) % 3 ]->xyz );

			/* longer? */
			if ( dist > maxDist ) {
				maxDist = dist;
				max = i;
			}

			/* add to centroid */
			fi->xyz += tri[ i ]->xyz;
			fi->normal += tri[ i ]->normal;
		}

		/* is the triangle small enough? */
		if ( maxDist <= ( foliage.density * foliage.density ) ) {
			float alpha, odds, r;


			/* get average alpha */
			if ( foliage.inverseAlpha == 2 ) {
				alpha = 1.0f;
			}
			else
			{
				alpha = ( tri[ 0 ]->color[ 0 ].alpha() + tri[ 1 ]->color[ 0 ].alpha() + tri[ 2 ]->color[ 0 ].alpha() ) / 765.0f;
				if ( foliage.inverseAlpha == 1 ) {
					alpha = 1.0f - alpha;
				}
				if ( alpha < 0.75f ) {
					return;
				}
			}

			/* roll the dice */
			odds = foliage.odds * alpha;
			r = Random();
			if ( r > odds ) {
				return;
			}

			/* scale centroid */
			fi->xyz *= 0.33333333f;
			if ( VectorNormalize( fi->normal ) == 0.0f ) {
				return;
			}

			/* add to count and return */
			numFoliageInstances++;
			return;
		}
	}

	/* split the longest edge and map it */
	const bspDrawVert_t mid = LerpDrawVert( *tri[ max ], *tri[ ( max + 1 ) % 3 ] );

	/* recurse to first triangle */
	TriRef tri2 = tri;
	tri2[ max ] = &mid;
	SubdivideFoliageTriangle_r( ds, foliage, tri2 );

	/* recurse to second triangle */
	tri2 = tri;
	tri2[ ( max + 1 ) % 3 ] = &mid;
	SubdivideFoliageTriangle_r( ds, foliage, tri2 );
}



/*
   GenFoliage()
   generates a foliage file for a bsp
 */

void Foliage( mapDrawSurface_t *src, entity_t& entity ){
	/* get shader */
	shaderInfo_t *si = src->shaderInfo;
	if ( si == NULL || si->foliage.empty() ) {
		return;
	}

	/* do every foliage */
	for ( const auto& foliage : si->foliage )
	{
		/* zero out */
		numFoliageInstances = 0;

		/* map the surface onto the lightmap origin/cluster/normal buffers */
		switch ( src->type )
		{
		case ESurfaceType::Meta:
		case ESurfaceType::ForcedMeta:
		case ESurfaceType::Triangles:
		{
			/* get verts */
			const bspDrawVert_t *verts = src->verts;

			/* map the triangles */
			for ( int i = 0; i < src->numIndexes; i += 3 )
				SubdivideFoliageTriangle_r( src, foliage, TriRef{
					&verts[ src->indexes[ i + 0 ] ],
					&verts[ src->indexes[ i + 1 ] ],
					&verts[ src->indexes[ i + 2 ] ]
				} );
			break;
		}
		case ESurfaceType::Patch:
		{
			/* make a mesh from the drawsurf */
			mesh_t srcMesh;
			srcMesh.width = src->patchWidth;
			srcMesh.height = src->patchHeight;
			srcMesh.verts = src->verts;
			mesh_t *subdivided = SubdivideMesh( srcMesh, 8, 512 );

			/* fit it to the curve and remove colinear verts on rows/columns */
			PutMeshOnCurve( *subdivided );
			mesh_t *mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );

			/* get verts */
			const bspDrawVert_t *verts = mesh->verts;

			/* map the mesh quads */
			for ( int y = 0; y < ( mesh->height - 1 ); y++ )
			{
				for ( int x = 0; x < ( mesh->width - 1 ); x++ )
				{
					/* set indexes */
					const int pw[ 5 ] = {
						x + ( y * mesh->width ),
						x + ( ( y + 1 ) * mesh->width ),
						x + 1 + ( ( y + 1 ) * mesh->width ),
						x + 1 + ( y * mesh->width ),
						x + ( y * mesh->width )      /* same as pw[ 0 ] */
					};
					/* set radix */
					const int r = ( x + y ) & 1;

					/* get drawverts and map first triangle */
					SubdivideFoliageTriangle_r( src, foliage, TriRef{
						&verts[ pw[ r + 0 ] ],
						&verts[ pw[ r + 1 ] ],
						&verts[ pw[ r + 2 ] ]
					} );
					/* get drawverts and map second triangle */
					SubdivideFoliageTriangle_r( src, foliage, TriRef{
						&verts[ pw[ r + 0 ] ],
						&verts[ pw[ r + 2 ] ],
						&verts[ pw[ r + 3 ] ]
					} );
				}
			}

			/* free the mesh */
			FreeMesh( mesh );
			break;
		}
		default:
			break;
		}

		/* any origins? */
		if ( numFoliageInstances < 1 ) {
			continue;
		}

		/* remember surface count */
		const int oldNumMapDrawSurfs = numMapDrawSurfs;

		/* add the model to the bsp */
		InsertModel( foliage.model.c_str(), NULL, 0, matrix4_scale_for_vec3( Vector3( foliage.scale ) ), NULL, NULL, entity, src->castShadows, src->recvShadows, 0, src->lightmapScale, 0, 0, clipDepthGlobal );

		/* walk each new surface */
		for ( int i = oldNumMapDrawSurfs; i < numMapDrawSurfs; i++ )
		{
			/* get surface */
			mapDrawSurface_t& ds = mapDrawSurfs[ i ];

			/* set up */
			ds.type = ESurfaceType::Foliage;
			ds.numFoliageInstances = numFoliageInstances;

			/* a wee hack */
			ds.patchWidth = ds.numFoliageInstances;
			ds.patchHeight = ds.numVerts;

			/* set fog to be same as source surface */
			ds.fogNum = src->fogNum;

			/* add a drawvert for every instance */
			bspDrawVert_t *verts = safe_calloc( ( ds.numVerts + ds.numFoliageInstances ) * sizeof( *verts ) );
			memcpy( verts, ds.verts, ds.numVerts * sizeof( *verts ) );
			free( ds.verts );
			ds.verts = verts;

			/* copy the verts */
			for ( int j = 0; j < ds.numFoliageInstances; j++ )
			{
				/* get vert (foliage instance) */
				bspDrawVert_t& fi = ds.verts[ ds.numVerts + j ];

				/* copy xyz and normal */
				fi.xyz = foliageInstances[ j ].xyz;
				fi.normal = foliageInstances[ j ].normal;

				/* ydnar: set color */
				for ( auto& color : fi.color )
				{
					color.set( 255 );
				}
			}

			/* increment */
			ds.numVerts += ds.numFoliageInstances;
		}
	}
}

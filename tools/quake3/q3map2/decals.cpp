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
#include "timer.h"



#define MAX_PROJECTORS      1024

struct decalProjector_t
{
	shaderInfo_t            *si;
	MinMax minmax;
	Vector3 center;
	float radius, radius2;
	int numPlanes;                      /* either 5 or 6, for quad or triangle projectors */
	Plane3f planes[ 6 ];
	Vector4 texMat[ 2 ];
};

static int numProjectors = 0;
static decalProjector_t projectors[ MAX_PROJECTORS ];

static int numDecalSurfaces = 0;

static Vector3 entityOrigin;




/*
   MakeTextureMatrix()
   generates a texture projection matrix for a triangle
   returns false if a texture matrix cannot be created
 */

static bool MakeTextureMatrix( decalProjector_t *dp, const Plane3f& projection, const bspDrawVert_t *a, const bspDrawVert_t *b, const bspDrawVert_t *c ){
	int i, j;
	double bb, s, t;
	DoubleVector3 pa, pb, pc;
	DoubleVector3 bary, xyz;
	DoubleVector3 vecs[ 3 ], axis[ 3 ], lengths;


	/* project triangle onto plane of projection */
	pa = plane3_project_point( projection, a->xyz );
	pb = plane3_project_point( projection, b->xyz );
	pc = plane3_project_point( projection, c->xyz );

	/* two methods */
	#if 1
	{
		/* old code */

		/* calculate barycentric basis for the triangle */
		bb = ( b->st[ 0 ] - a->st[ 0 ] ) * ( c->st[ 1 ] - a->st[ 1 ] ) - ( c->st[ 0 ] - a->st[ 0 ] ) * ( b->st[ 1 ] - a->st[ 1 ] );
		if ( fabs( bb ) < 0.00000001 ) {
			return false;
		}

		/* calculate texture origin */
		#if 0
		s = 0.0;
		t = 0.0;
		bary[ 0 ] = ( ( b->st[ 0 ] - s ) * ( c->st[ 1 ] - t ) - ( c->st[ 0 ] - s ) * ( b->st[ 1 ] - t ) ) / bb;
		bary[ 1 ] = ( ( c->st[ 0 ] - s ) * ( a->st[ 1 ] - t ) - ( a->st[ 0 ] - s ) * ( c->st[ 1 ] - t ) ) / bb;
		bary[ 2 ] = ( ( a->st[ 0 ] - s ) * ( b->st[ 1 ] - t ) - ( b->st[ 0 ] - s ) * ( a->st[ 1 ] - t ) ) / bb;

		origin[ 0 ] = bary[ 0 ] * pa[ 0 ] + bary[ 1 ] * pb[ 0 ] + bary[ 2 ] * pc[ 0 ];
		origin[ 1 ] = bary[ 0 ] * pa[ 1 ] + bary[ 1 ] * pb[ 1 ] + bary[ 2 ] * pc[ 1 ];
		origin[ 2 ] = bary[ 0 ] * pa[ 2 ] + bary[ 1 ] * pb[ 2 ] + bary[ 2 ] * pc[ 2 ];
		#endif

		/* calculate s vector */
		s = a->st[ 0 ] + 1.0;
		t = a->st[ 1 ] + 0.0;
		bary[ 0 ] = ( ( b->st[ 0 ] - s ) * ( c->st[ 1 ] - t ) - ( c->st[ 0 ] - s ) * ( b->st[ 1 ] - t ) ) / bb;
		bary[ 1 ] = ( ( c->st[ 0 ] - s ) * ( a->st[ 1 ] - t ) - ( a->st[ 0 ] - s ) * ( c->st[ 1 ] - t ) ) / bb;
		bary[ 2 ] = ( ( a->st[ 0 ] - s ) * ( b->st[ 1 ] - t ) - ( b->st[ 0 ] - s ) * ( a->st[ 1 ] - t ) ) / bb;

		xyz[ 0 ] = bary[ 0 ] * pa[ 0 ] + bary[ 1 ] * pb[ 0 ] + bary[ 2 ] * pc[ 0 ];
		xyz[ 1 ] = bary[ 0 ] * pa[ 1 ] + bary[ 1 ] * pb[ 1 ] + bary[ 2 ] * pc[ 1 ];
		xyz[ 2 ] = bary[ 0 ] * pa[ 2 ] + bary[ 1 ] * pb[ 2 ] + bary[ 2 ] * pc[ 2 ];

		//%	vecs[ 0 ] = xyz - origin;
		vecs[ 0 ] = xyz - pa;

		/* calculate t vector */
		s = a->st[ 0 ] + 0.0;
		t = a->st[ 1 ] + 1.0;
		bary[ 0 ] = ( ( b->st[ 0 ] - s ) * ( c->st[ 1 ] - t ) - ( c->st[ 0 ] - s ) * ( b->st[ 1 ] - t ) ) / bb;
		bary[ 1 ] = ( ( c->st[ 0 ] - s ) * ( a->st[ 1 ] - t ) - ( a->st[ 0 ] - s ) * ( c->st[ 1 ] - t ) ) / bb;
		bary[ 2 ] = ( ( a->st[ 0 ] - s ) * ( b->st[ 1 ] - t ) - ( b->st[ 0 ] - s ) * ( a->st[ 1 ] - t ) ) / bb;

		xyz[ 0 ] = bary[ 0 ] * pa[ 0 ] + bary[ 1 ] * pb[ 0 ] + bary[ 2 ] * pc[ 0 ];
		xyz[ 1 ] = bary[ 0 ] * pa[ 1 ] + bary[ 1 ] * pb[ 1 ] + bary[ 2 ] * pc[ 1 ];
		xyz[ 2 ] = bary[ 0 ] * pa[ 2 ] + bary[ 1 ] * pb[ 2 ] + bary[ 2 ] * pc[ 2 ];

		//%	vecs[ 1 ] = xyz - origin;
		vecs[ 1 ] = xyz - pa;

		/* calcuate r vector */
		vecs[ 2 ] = -projection.normal();

		/* calculate transform axis */
		for ( i = 0; i < 3; i++ ){
			axis[ i ] = vecs[ i ];
			lengths[ i ] = VectorNormalize( axis[ i ] );
		}
		for ( i = 0; i < 2; i++ )
			for ( j = 0; j < 3; j++ )
				dp->texMat[ i ][ j ] = lengths[ i ] != 0.0 ? ( axis[ i ][ j ] / lengths[ i ] ) : 0.0;
		//%	dp->texMat[ i ][ j ] = fabs( vecs[ i ][ j ] ) > 0.0 ? ( 1.0 / vecs[ i ][ j ] ) : 0.0;
		//%	dp->texMat[ i ][ j ] = axis[ i ][ j ] > 0.0 ? ( 1.0 / axis[ i ][ j ] ) : 0.0;

		/* calculalate translation component */
		dp->texMat[ 0 ][ 3 ] = a->st[ 0 ] - vector3_dot( a->xyz, dp->texMat[ 0 ].vec3() );
		dp->texMat[ 1 ][ 3 ] = a->st[ 1 ] - vector3_dot( a->xyz, dp->texMat[ 1 ].vec3() );
	}
	#else
	{
		int k;
		DoubleVector3 deltas[ 3 ];
		BasicVector2<double> texDeltas[ 3 ];
		double delta, texDelta;


		/* new code */

		/* calculate deltas */
		deltas[ 0 ] = pa - pb;
		deltas[ 1 ] = pa - pc;
		deltas[ 2 ] = pb - pc;
		texDeltas[ 0 ] = a->st - b->st;
		texDeltas[ 1 ] = a->st - c->st;
		texDeltas[ 2 ] = b->st - c->st;

		/* walk st */
		for ( i = 0; i < 2; i++ )
		{
			/* walk xyz */
			for ( j = 0; j < 3; j++ )
			{
				/* clear deltas */
				delta = 0.0;
				texDelta = 0.0;

				/* walk deltas */
				for ( k = 0; k < 3; k++ )
				{
					if ( fabs( deltas[ k ][ j ] ) > delta &&
					     fabs( texDeltas[ k ][ i ] ) > texDelta  ) {
						delta = deltas[ k ][ j ];
						texDelta = texDeltas[ k ][ i ];
					}
				}

				/* set texture matrix component */
				if ( fabs( delta ) > 0.0 ) {
					dp->texMat[ i ][ j ] = texDelta / delta;
				}
				else{
					dp->texMat[ i ][ j ] = 0.0;
				}
			}

			/* set translation component */
			dp->texMat[ i ][ 3 ] = a->st[ i ] - vector3_dot( pa, dp->texMat[ i ].vec3() );
		}
	}
	#endif

	/* debug code */
	#if 1
	Sys_Printf( "Mat: [ %f %f %f %f ] [ %f %f %f %f ] Theta: %lf (%lf)\n",
	            dp->texMat[ 0 ][ 0 ], dp->texMat[ 0 ][ 1 ], dp->texMat[ 0 ][ 2 ], dp->texMat[ 0 ][ 3 ],
	            dp->texMat[ 1 ][ 0 ], dp->texMat[ 1 ][ 1 ], dp->texMat[ 1 ][ 2 ], dp->texMat[ 1 ][ 3 ],
	            radians_to_degrees( acos( vector3_dot( dp->texMat[ 0 ].vec3(), dp->texMat[ 1 ].vec3() ) ) ),
	            radians_to_degrees( acos( vector3_dot( axis[ 0 ], axis[ 1 ] ) ) ) );

	Sys_Printf( "XYZ: %f %f %f ST: %f %f ST(t): %lf %lf\n",
	            a->xyz[ 0 ], a->xyz[ 1 ], a->xyz[ 2 ],
	            a->st[ 0 ], a->st[ 1 ],
	            vector3_dot( a->xyz, dp->texMat[ 0 ].vec3() ) + dp->texMat[ 0 ][ 3 ], vector3_dot( a->xyz, dp->texMat[ 1 ].vec3() ) + dp->texMat[ 1 ][ 3 ] );
	#endif

	/* test texture matrix */
	s = vector3_dot( a->xyz, dp->texMat[ 0 ].vec3() ) + dp->texMat[ 0 ][ 3 ];
	t = vector3_dot( a->xyz, dp->texMat[ 1 ].vec3() ) + dp->texMat[ 1 ][ 3 ];
	if ( !float_equal_epsilon( s, a->st[ 0 ], 0.01 ) || !float_equal_epsilon( t, a->st[ 1 ], 0.01 ) ) {
		Sys_Printf( "Bad texture matrix! (A) (%f, %f) != (%f, %f)\n",
		            s, t, a->st[ 0 ], a->st[ 1 ] );
		//%	return false;
	}
	s = vector3_dot( b->xyz, dp->texMat[ 0 ].vec3() ) + dp->texMat[ 0 ][ 3 ];
	t = vector3_dot( b->xyz, dp->texMat[ 1 ].vec3() ) + dp->texMat[ 1 ][ 3 ];
	if ( !float_equal_epsilon( s, b->st[ 0 ], 0.01 ) || !float_equal_epsilon( t, b->st[ 1 ], 0.01 ) ) {
		Sys_Printf( "Bad texture matrix! (B) (%f, %f) != (%f, %f)\n",
		            s, t, b->st[ 0 ], b->st[ 1 ] );
		//%	return false;
	}
	s = vector3_dot( c->xyz, dp->texMat[ 0 ].vec3() ) + dp->texMat[ 0 ][ 3 ];
	t = vector3_dot( c->xyz, dp->texMat[ 1 ].vec3() ) + dp->texMat[ 1 ][ 3 ];
	if ( !float_equal_epsilon( s, c->st[ 0 ], 0.01 ) || !float_equal_epsilon( t, c->st[ 1 ], 0.01 ) ) {
		Sys_Printf( "Bad texture matrix! (C) (%f, %f) != (%f, %f)\n",
		            s, t, c->st[ 0 ], c->st[ 1 ] );
		//%	return false;
	}

	/* disco */
	return true;
}



/*
   TransformDecalProjector()
   transforms a decal projector
   note: non-normalized axes will screw up the plane transform
 */

static void TransformDecalProjector( decalProjector_t *in, const Vector3 (&axis)[ 3 ], const Vector3& origin, decalProjector_t *out ){
	/* copy misc stuff */
	out->si = in->si;
	out->numPlanes = in->numPlanes;

	/* translate bounding box and sphere (note: rotated projector bounding box will be invalid!) */
	out->minmax.mins = in->minmax.mins - origin;
	out->minmax.maxs = in->minmax.maxs - origin;
	out->center = in->center - origin;
	out->radius = in->radius;
	out->radius2 = in->radius2;

	/* translate planes */
	for ( int i = 0; i < in->numPlanes; i++ )
	{
		out->planes[ i ].a = vector3_dot( in->planes[ i ].normal(), axis[ 0 ] );
		out->planes[ i ].b = vector3_dot( in->planes[ i ].normal(), axis[ 1 ] );
		out->planes[ i ].c = vector3_dot( in->planes[ i ].normal(), axis[ 2 ] );
		out->planes[ i ].d = in->planes[ i ].dist() - vector3_dot( out->planes[ i ].normal(), origin );
	}

	/* translate texture matrix */
	for ( int i = 0; i < 2; i++ )
	{
		out->texMat[ i ][ 0 ] = vector3_dot( in->texMat[ i ].vec3(), axis[ 0 ] );
		out->texMat[ i ][ 1 ] = vector3_dot( in->texMat[ i ].vec3(), axis[ 1 ] );
		out->texMat[ i ][ 2 ] = vector3_dot( in->texMat[ i ].vec3(), axis[ 2 ] );
		out->texMat[ i ][ 3 ] = vector3_dot( out->texMat[ i ].vec3(), origin ) + in->texMat[ i ][ 3 ];
	}
}



/*
   MakeDecalProjector()
   creates a new decal projector from a triangle
 */

static int MakeDecalProjector( shaderInfo_t *si, const Plane3f& projection, float distance, int numVerts, const bspDrawVert_t **dv ){
	int i, j;
	decalProjector_t    *dp;


	/* dummy check */
	if ( numVerts != 3 && numVerts != 4 ) {
		return -1;
	}

	/* limit check */
	if ( numProjectors >= MAX_PROJECTORS ) {
		Sys_Warning( "MAX_PROJECTORS (%d) exceeded, no more decal projectors available.\n", MAX_PROJECTORS );
		return -2;
	}

	/* create a new projector */
	dp = &projectors[ numProjectors ];
	memset( dp, 0, sizeof( *dp ) );

	/* basic setup */
	dp->si = si;
	dp->numPlanes = numVerts + 2;

	/* make texture matrix */
	if ( !MakeTextureMatrix( dp, projection, dv[ 0 ], dv[ 1 ], dv[ 2 ] ) ) {
		return -1;
	}

	/* bound the projector */
	dp->minmax.clear();
	for ( i = 0; i < numVerts; i++ )
	{
		dp->minmax.extend( dv[ i ]->xyz );
		dp->minmax.extend( dv[ i ]->xyz + projection.normal() * distance );
	}

	/* make bouding sphere */
	dp->center = dp->minmax.origin();
	dp->radius = vector3_length( dp->minmax.maxs - dp->center );
	dp->radius2 = dp->radius * dp->radius;

	/* make the front plane */
	if ( !PlaneFromPoints( dp->planes[ 0 ], dv[ 0 ]->xyz, dv[ 1 ]->xyz, dv[ 2 ]->xyz ) ) {
		return -1;
	}

	/* make the back plane */
	dp->planes[ 1 ].normal() = -dp->planes[ 0 ].normal();
	dp->planes[ 1 ].dist() = vector3_dot( dv[ 0 ]->xyz + projection.normal() * distance, dp->planes[ 1 ].normal() );

	/* make the side planes */
	for ( i = 0; i < numVerts; i++ )
	{
		j = ( i + 1 ) % numVerts;
		if ( !PlaneFromPoints( dp->planes[ i + 2 ], dv[ j ]->xyz, dv[ i ]->xyz, dv[ i ]->xyz + projection.normal() * distance ) ) {
			return -1;
		}
	}

	/* return ok */
	return numProjectors++;
}



/*
   ProcessDecals()
   finds all decal entities and creates decal projectors
 */

#define PLANAR_EPSILON  0.5f

void ProcessDecals(){
	float distance;
	Plane3f projection, plane;
	entity_t            *e2;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- ProcessDecals ---\n" );

	/* walk entity list */
	for ( auto& e : entities )
	{
		if ( !e.classname_is( "_decal" ) ) {
			continue;
		}

		/* any patches? */
		if ( e.patches == NULL ) {
			Sys_Warning( "Decal entity without any patch meshes, ignoring.\n" );
			e.epairs.clear();
			continue;
		}

		/* find target */
		e2 = FindTargetEntity( e.valueForKey( "target" ) );

		/* no target? */
		if ( e2 == NULL ) {
			Sys_Warning( "Decal entity without a valid target, ignoring.\n" );
			continue;
		}

		/* walk entity patches */
		for ( parseMesh_t *p = e.patches; p != NULL; p = e.patches )
		{
			/* setup projector */
			Vector3 origin;
			if ( VectorCompare( e.origin, g_vector3_identity ) ) {
				origin = p->eMinmax.origin();
			}
			else{
				origin = e.origin;
			}

			/* setup projection plane */
			projection.normal() = e2->origin - origin;
			distance = VectorNormalize( projection.normal() );
			projection.dist() = vector3_dot( origin, projection.normal() );

			/* create projectors */
			if ( distance > 0.125f ) {
				/* tesselate the patch */
				const int iterations = IterationsForCurve( p->longestCurve, patchSubdivisions );
				mesh_t *subdivided = SubdivideMesh2( p->mesh, iterations );

				/* fit it to the curve and remove colinear verts on rows/columns */
				PutMeshOnCurve( *subdivided );
				mesh_t *mesh = RemoveLinearMeshColumnsRows( subdivided );
				FreeMesh( subdivided );

				/* offset by projector origin */
				for ( bspDrawVert_t& vert : Span( mesh->verts, mesh->width * mesh->height ) )
					vert.xyz += e.origin;

				/* iterate through the mesh quads */
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
							x + ( y * mesh->width )    /* same as pw[ 0 ] */
						};
						/* set radix */
						const int r = ( x + y ) & 1;

						/* get drawverts */
						const bspDrawVert_t *dv[ 4 ] = {
							&mesh->verts[ pw[ r + 0 ] ],
							&mesh->verts[ pw[ r + 1 ] ],
							&mesh->verts[ pw[ r + 2 ] ],
							&mesh->verts[ pw[ r + 3 ] ]
						};
						/* planar? (nuking this optimization as it doesn't work on non-rectangular quads) */
						if ( 0 && PlaneFromPoints( plane, dv[ 0 ]->xyz, dv[ 1 ]->xyz, dv[ 2 ]->xyz ) &&
						     fabs( plane3_distance_to_point( plane, dv[ 1 ]->xyz ) ) <= PLANAR_EPSILON ) {
							/* make a quad projector */
							MakeDecalProjector( p->shaderInfo, projection, distance, 4, dv );
						}
						else
						{
							/* make first triangle */
							MakeDecalProjector( p->shaderInfo, projection, distance, 3, dv );

							/* make second triangle */
							dv[ 1 ] = dv[ 2 ];
							dv[ 2 ] = dv[ 3 ];
							MakeDecalProjector( p->shaderInfo, projection, distance, 3, dv );
						}
					}
				}

				/* clean up */
				free( mesh );
			}

			/* remove patch from entity (fixme: leak!) */
			e.patches = p->next;

			/* push patch to worldspawn (enable this to debug projectors) */
			#if 0
			p->next = entities[ 0 ].patches;
			entities[ 0 ].patches = p;
			#endif
		}
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d decal projectors\n", numProjectors );
}



/*
   ProjectDecalOntoWinding()
   projects a decal onto a winding
 */

static void ProjectDecalOntoWinding( decalProjector_t *dp, mapDrawSurface_t *ds, winding_t& w ){
	int i, j;
	mapDrawSurface_t    *ds2;
	bspDrawVert_t       *dv;
	Plane3f plane;


	/* dummy check */
	if ( w.size() < 3 ) {
		return;
	}

	/* offset by entity origin */
	for ( Vector3& p : w )
		p += entityOrigin;

	/* make a plane from the winding */
	if ( !PlaneFromPoints( plane, w.data() ) ) {
		return;
	}

	/* backface check */
	if ( vector3_dot( dp->planes[ 0 ].normal(), plane.normal() ) < -0.0001f ) {
		return;
	}

	/* walk list of planes */
	for ( i = 0; i < dp->numPlanes; i++ )
	{
		/* chop winding by the plane */
		auto [front, back] = ClipWindingEpsilonStrict( w, dp->planes[ i ], 0.0625f ); /* strict, if identical plane we don't want to keep it */

		/* lose the front fragment */
		/* if nothing left in back, then bail */
		if ( back.empty() ) {
			return;
		}

		/* reset winding */
		w.swap( back );
	}

	/* nothing left? */
	if ( w.size() < 3 ) {
		return;
	}

	/* add to counts */
	numDecalSurfaces++;

	/* make a new surface */
	ds2 = AllocDrawSurface( ESurfaceType::Decal );

	/* set it up */
	ds2->entityNum = ds->entityNum;
	ds2->castShadows = ds->castShadows;
	ds2->recvShadows = ds->recvShadows;
	ds2->shaderInfo = dp->si;
	ds2->fogNum = ds->fogNum;   /* why was this -1? */
	ds2->lightmapScale = ds->lightmapScale;
	ds2->shadeAngleDegrees = ds->shadeAngleDegrees;
	ds2->numVerts = w.size();
	ds2->verts = safe_calloc( ds2->numVerts * sizeof( *ds2->verts ) );

	/* set vertexes */
	for ( i = 0; i < ds2->numVerts; i++ )
	{
		/* get vertex */
		dv = &ds2->verts[ i ];

		/* set alpha */
		const float d = plane3_distance_to_point( dp->planes[ 0 ], w[ i ] );
		const float d2 = plane3_distance_to_point( dp->planes[ 1 ], w[ i ] );
		const float alpha = 255.0f * d2 / ( d + d2 );

		/* set misc */
		dv->xyz = w[ i ] - entityOrigin;
		dv->normal = plane.normal();
		dv->st[ 0 ] = vector3_dot( dv->xyz, dp->texMat[ 0 ].vec3() ) + dp->texMat[ 0 ][ 3 ];
		dv->st[ 1 ] = vector3_dot( dv->xyz, dp->texMat[ 1 ].vec3() ) + dp->texMat[ 1 ][ 3 ];

		/* set color */
		for ( j = 0; j < MAX_LIGHTMAPS; j++ )
		{
			dv->color[ j ] = { 255, 255, 255, color_to_byte( alpha ) };
		}
	}
}



/*
   ProjectDecalOntoFace()
   projects a decal onto a brushface surface
 */

static void ProjectDecalOntoFace( decalProjector_t *dp, mapDrawSurface_t *ds ){
	/* dummy check */
	if ( ds->sideRef == NULL || ds->sideRef->side == NULL ) {
		return;
	}

	/* backface check */
	if ( ds->planar ) {
		if ( vector3_dot( dp->planes[ 0 ].normal(), mapplanes[ ds->planeNum ].normal() ) < -0.0001f ) {
			return;
		}
	}

	/* generate decal */
	winding_t w = WindingFromDrawSurf( ds );
	ProjectDecalOntoWinding( dp, ds, w );
}



/*
   ProjectDecalOntoPatch()
   projects a decal onto a patch surface
 */

static void ProjectDecalOntoPatch( decalProjector_t *dp, mapDrawSurface_t *ds ){
	/* backface check */
	if ( ds->planar )
		if ( vector3_dot( dp->planes[ 0 ].normal(), mapplanes[ ds->planeNum ].normal() ) < -0.0001f )
			return;

	/* tesselate the patch */
	mesh_t src;
	src.width = ds->patchWidth;
	src.height = ds->patchHeight;
	src.verts = ds->verts;
	const int iterations = IterationsForCurve( ds->longestCurve, patchSubdivisions );
	mesh_t *subdivided = SubdivideMesh2( src, iterations );

	/* fit it to the curve and remove colinear verts on rows/columns */
	PutMeshOnCurve( *subdivided );
	mesh_t *mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );

	/* iterate through the mesh quads */
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
				x + ( y * mesh->width )    /* same as pw[ 0 ] */
			};
			/* set radix */
			const int r = ( x + y ) & 1;

			/* generate decal for first triangle */
			winding_t w{
				mesh->verts[ pw[ r + 0 ] ].xyz,
				mesh->verts[ pw[ r + 1 ] ].xyz,
				mesh->verts[ pw[ r + 2 ] ].xyz };
			ProjectDecalOntoWinding( dp, ds, w );

			/* generate decal for second triangle */
			winding_t w2{
				mesh->verts[ pw[ r + 0 ] ].xyz,
				mesh->verts[ pw[ r + 2 ] ].xyz,
				mesh->verts[ pw[ r + 3 ] ].xyz };
			ProjectDecalOntoWinding( dp, ds, w2 );
		}
	}

	/* clean up */
	free( mesh );
}



/*
   ProjectDecalOntoTriangles()
   projects a decal onto a triangle surface
 */

static void ProjectDecalOntoTriangles( decalProjector_t *dp, mapDrawSurface_t *ds ){

	/* triangle surfaces without shaders don't get marks by default */
	if ( ds->type == ESurfaceType::Triangles && ds->shaderInfo->shaderText == NULL ) {
		return;
	}

	/* backface check */
	if ( ds->planar ) {
		if ( vector3_dot( dp->planes[ 0 ].normal(), mapplanes[ ds->planeNum ].normal() ) < -0.0001f ) {
			return;
		}
	}

	/* iterate through triangles */
	for ( int i = 0; i < ds->numIndexes; i += 3 )
	{
		/* generate decal */
		winding_t w{
			ds->verts[ ds->indexes[ i + 0 ] ].xyz,
			ds->verts[ ds->indexes[ i + 1 ] ].xyz,
			ds->verts[ ds->indexes[ i + 2 ] ].xyz };
		ProjectDecalOntoWinding( dp, ds, w );
	}
}



/*
   MakeEntityDecals()
   projects decals onto world surfaces
 */

void MakeEntityDecals( const entity_t& e ){
	int i, j, fOld;
	decalProjector_t dp;
	mapDrawSurface_t    *ds;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- MakeEntityDecals ---\n" );

	/* transform projector instead of geometry */
	entityOrigin.set( 0 );

	/* init pacifier */
	fOld = -1;
	Timer timer;

	/* walk the list of decal projectors */
	for ( i = 0; i < numProjectors; i++ )
	{
		/* print pacifier */
		if ( const int f = 10 * i / numProjectors; f != fOld ) {
			fOld = f;
			Sys_FPrintf( SYS_VRB, "%d...", f );
		}

		/* get projector */
		TransformDecalProjector( &projectors[ i ], g_vector3_axes, e.origin, &dp );

		/* walk the list of surfaces in the entity */
		for ( j = e.firstDrawSurf; j < numMapDrawSurfs; ++j )
		{
			/* get surface */
			ds = &mapDrawSurfs[ j ];
			if ( ds->numVerts <= 0 ) {
				continue;
			}

			/* ignore autosprite or nomarks */
			if ( ds->shaderInfo->autosprite || ( ds->shaderInfo->compileFlags & C_NOMARKS ) ) {
				continue;
			}

			/* bounds check */
			if ( !ds->minmax.test( dp.center, dp.radius ) ) {
				continue;
			}

			/* switch on type */
			switch ( ds->type )
			{
			case ESurfaceType::Face:
				ProjectDecalOntoFace( &dp, ds );
				break;

			case ESurfaceType::Patch:
				ProjectDecalOntoPatch( &dp, ds );
				break;

			case ESurfaceType::Triangles:
			case ESurfaceType::ForcedMeta:
			case ESurfaceType::Meta:
				ProjectDecalOntoTriangles( &dp, ds );
				break;

			default:
				break;
			}
		}
	}

	/* print time */
	Sys_FPrintf( SYS_VRB, " (%d)\n", int( timer.elapsed_sec() ) );

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d decal surfaces\n", numDecalSurfaces );
}

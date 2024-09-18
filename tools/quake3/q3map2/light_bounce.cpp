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


/* functions */


/*
   RadClipWindingEpsilon()
   clips a rad winding by a plane
   based off the regular clip winding code
 */

static void RadClipWindingEpsilon( radWinding_t *in, const Vector3& normal, float dist,
                                   float epsilon, radWinding_t *front, radWinding_t *back, clipWork_t *cw ){
	float           *dists;
	EPlaneSide      *sides;
	int counts[ 3 ];
	float dot;                  /* ydnar: changed from static b/c of threading */ /* VC 4.2 optimizer bug if not static? */
	int i, k;
	int maxPoints;


	/* crutch */
	dists = cw->dists;
	sides = cw->sides;

	/* clear counts */
	counts[ 0 ] = counts[ 1 ] = counts[ 2 ] = 0;

	/* determine sides for each point */
	for ( i = 0; i < in->numVerts; i++ )
	{
		dists[ i ] = vector3_dot( in->verts[ i ].xyz, normal ) - dist;
		if ( dists[ i ] > epsilon ) {
			sides[ i ] = eSideFront;
		}
		else if ( dists[ i ] < -epsilon ) {
			sides[ i ] = eSideBack;
		}
		else{
			sides[ i ] = eSideOn;
		}
		counts[ sides[ i ] ]++;
	}
	sides[ i ] = sides[ 0 ];
	dists[ i ] = dists[ 0 ];

	/* clear front and back */
	front->numVerts = back->numVerts = 0;

	/* handle all on one side cases */
	if ( counts[ 0 ] == 0 ) {
		memcpy( back, in, sizeof( radWinding_t ) );
		return;
	}
	if ( counts[ 1 ] == 0 ) {
		memcpy( front, in, sizeof( radWinding_t ) );
		return;
	}

	/* setup windings */
	maxPoints = in->numVerts + 4;

	/* do individual verts */
	for ( i = 0; i < in->numVerts; i++ )
	{
		/* do simple vertex copies first */
		const radVert_t& v1 = in->verts[ i ];

		if ( sides[ i ] == eSideOn ) {
			front->verts[ front->numVerts++ ] = v1;
			back->verts[ back->numVerts++ ] = v1;
			continue;
		}

		if ( sides[ i ] == eSideFront ) {
			front->verts[ front->numVerts++ ] = v1;
		}

		if ( sides[ i ] == eSideBack ) {
			back->verts[ back->numVerts++ ] = v1;
		}

		if ( sides[ i + 1 ] == eSideOn || sides[ i + 1 ] == sides[ i ] ) {
			continue;
		}

		/* generate a split vertex */
		const radVert_t& v2 = in->verts[ ( i + 1 ) % in->numVerts ];

		dot = dists[ i ] / ( dists[ i ] - dists[ i + 1 ] );

		/* average vertex values */
		radVert_t mid;
		/* color */
		for ( k = 0; k < MAX_LIGHTMAPS; k++ ){
			mid.color[ k ] = v1.color[ k ] + ( v2.color[ k ] - v1.color[ k ] ) * dot;
		}
		/* xyz, normal */
		mid.xyz = v1.xyz + ( v2.xyz - v1.xyz ) * dot;
		mid.normal = v1.normal + ( v2.normal - v1.normal ) * dot;
		/* st, lightmap */
		mid.st = v1.st + ( v2.st - v1.st ) * dot;
		for ( k = 0; k < MAX_LIGHTMAPS; k++ )
			mid.lightmap[ k ] = v1.lightmap[ k ] + ( v2.lightmap[ k ] - v1.lightmap[ k ] ) * dot;

		/* normalize the averaged normal */
		VectorNormalize( mid.normal );

		/* copy the midpoint to both windings */
		front->verts[ front->numVerts++ ] = mid;
		back->verts[ back->numVerts++ ] = mid;
	}

	/* error check */
	if ( front->numVerts > maxPoints ) {
		Error( "RadClipWindingEpsilon: points exceeded estimate" );
	}
	if ( front->numVerts > MAX_POINTS_ON_WINDING ) {
		Error( "RadClipWindingEpsilon: MAX_POINTS_ON_WINDING" );
	}
}



inline float Modulo1IfNegative( float f ){
	return f < 0.0f ? f - floor( f ) : f;
}


/*
   RadSampleImage()
   samples a texture image for a given color
   returns false if pixels are bad
 */

bool RadSampleImage( const byte *pixels, int width, int height, const Vector2& st, Color4f& color ){
	int x, y;

	/* clear color first */
	color.set( 255 );

	/* dummy check */
	if ( pixels == NULL || width < 1 || height < 1 ) {
		return false;
	}

	/* get offsets */
	x = ( (float) width * Modulo1IfNegative( st[ 0 ] ) ) + 0.5f;
	x %= width;
	y = ( (float) height * Modulo1IfNegative( st[ 1 ] ) ) + 0.5f;
	y %= height;

	/* get pixel */
	pixels += ( y * width * 4 ) + ( x * 4 );
	VectorCopy( pixels, color.rgb() );
	color.alpha() = pixels[ 3 ];

	if ( texturesRGB ) {
		color[0] = Image_LinearFloatFromsRGBFloat( color[0] * ( 1.0 / 255.0 ) ) * 255.0;
		color[1] = Image_LinearFloatFromsRGBFloat( color[1] * ( 1.0 / 255.0 ) ) * 255.0;
		color[2] = Image_LinearFloatFromsRGBFloat( color[2] * ( 1.0 / 255.0 ) ) * 255.0;
	}

	return true;
}



/*
   RadSample()
   samples a fragment's lightmap or vertex color and returns an
   average color and a color gradient for the sample
 */

#define MAX_SAMPLES         150
#define SAMPLE_GRANULARITY  6

static void RadSample( int lightmapNum, bspDrawSurface_t *ds, rawLightmap_t *lm, const shaderInfo_t *si, radWinding_t *rw, Vector3& average, Vector3& gradient, int *style ){
	int i, j, k, l, v, samples;
	Vector3 color;
	MinMax minmax;
	Color4f textureColor;
	float alpha, alphaI;
	radVert_t   *rv[ 3 ];

	if ( !bouncing )
		Sys_Printf( "BUG: RadSample: !bouncing shouldn't happen\n" );

	/* initial setup */
	average.set( 0 );
	gradient.set( 0 );
	alpha = 0;

	/* dummy check */
	if ( rw == NULL || rw->numVerts < 3 ) {
		return;
	}

	/* start sampling */
	samples = 0;

	/* sample vertex colors if no lightmap or this is the initial pass */
	if ( lm == NULL || lm->radLuxels[ lightmapNum ] == NULL || !bouncing ) {
		for ( samples = 0; samples < rw->numVerts; samples++ )
		{
			/* multiply by texture color */
			if ( !RadSampleImage( si->lightImage->pixels, si->lightImage->width, si->lightImage->height, rw->verts[ samples ].st, textureColor ) ) {
				textureColor.rgb() = si->averageColor.rgb();
				textureColor.alpha() = 255.0f;
			}
			const float avgcolor = ( textureColor[ 0 ] + textureColor[ 1 ] + textureColor[ 2 ] ) / 3;
			color = ( ( textureColor.rgb() * bounceColorRatio + Vector3( avgcolor * ( 1 - bounceColorRatio ) ) ) / 255 ) * ( rw->verts[ samples ].color[ lightmapNum ].rgb() / 255.0f );
//			color = ( textureColor.rgb / 255 ) * ( rw->verts[ samples ].color[ lightmapNum ].rgb / 255.0f );

			minmax.extend( color );
			average += color;

			/* get alpha */
			alpha += ( textureColor.alpha() / 255.0f ) * ( rw->verts[ samples ].color[ lightmapNum ].alpha() / 255.0f );
		}

		/* set style */
		*style = ds->vertexStyles[ lightmapNum ];
	}

	/* sample lightmap */
	else
	{
		/* fracture the winding into a fan (including degenerate tris) */
		for ( v = 1; v < ( rw->numVerts - 1 ) && samples < MAX_SAMPLES; v++ )
		{
			/* get a triangle */
			rv[ 0 ] = &rw->verts[ 0 ];
			rv[ 1 ] = &rw->verts[ v ];
			rv[ 2 ] = &rw->verts[ v + 1 ];

			/* this code is embarrassing (really should just rasterize the triangle) */
			for ( i = 1; i < SAMPLE_GRANULARITY && samples < MAX_SAMPLES; i++ )
			{
				for ( j = 1; j < SAMPLE_GRANULARITY && samples < MAX_SAMPLES; j++ )
				{
					for ( k = 1; k < SAMPLE_GRANULARITY && samples < MAX_SAMPLES; k++ )
					{
						/* create a blend vector (barycentric coordinates) */
						DoubleVector3 blend( i, j, k );
						blend *= 1.0 / ( blend[ 0 ] + blend[ 1 ] + blend[ 2 ] );

						/* create a blended sample */
						Vector2 st( 0, 0 );
						Vector2 lightmap( 0, 0 );
						alphaI = 0.0f;
						for ( l = 0; l < 3; l++ )
						{
							st += rv[ l ]->st * blend[ l ];
							lightmap += rv[ l ]->lightmap[ lightmapNum ] * blend[ l ];
							alphaI += rv[ l ]->color[ lightmapNum ].alpha() * blend[ l ];
						}

						/* get lightmap xy coords */
						const int x = std::clamp( int( lightmap[ 0 ] / superSample ), 0, lm->w - 1 );
						const int y = std::clamp( int( lightmap[ 1 ] / superSample ), 0, lm->h - 1 );

						/* get radiosity luxel */
						const Vector3& radLuxel = lm->getRadLuxel( lightmapNum, x, y );

						/* ignore unlit/unused luxels */
						if ( radLuxel[ 0 ] < 0.0f ) {
							continue;
						}

						/* inc samples */
						samples++;

						/* multiply by texture color */
						if ( !RadSampleImage( si->lightImage->pixels, si->lightImage->width, si->lightImage->height, st, textureColor ) ) {
							textureColor.rgb() = si->averageColor.rgb();
							textureColor.alpha() = 255;
						}
						const float avgcolor = ( textureColor[ 0 ] + textureColor[ 1 ] + textureColor[ 2 ] ) / 3;
						color = ( ( textureColor.rgb() * bounceColorRatio + Vector3( avgcolor * ( 1 - bounceColorRatio ) ) ) / 255 ) * ( radLuxel / 255 );
						//Sys_Printf( "%i %i %i %i %i \n", (int) textureColor.rgb[ 0 ], (int) textureColor.rgb[ 1 ], (int) textureColor.rgb[ 2 ], (int) avgcolor, (int) color[ i ] );
						minmax.extend( color );
						average += color;

						/* get alpha */
						alpha += ( textureColor.alpha() / 255 ) * ( alphaI / 255 );
					}
				}
			}
		}

		/* set style */
		*style = ds->lightmapStyles[ lightmapNum ];
	}

	/* any samples? */
	if ( samples <= 0 ) {
		return;
	}

	/* average the color */
	average *= ( 1.0 / samples );

	/* create the color gradient */
	//%	VectorSubtract( minmax.maxs, minmax.mins, delta );

	/* new: color gradient will always be 0-1.0, expressed as the range of light relative to overall light */
	//%	gradient[ 0 ] = minmax.maxs[ 0 ] > 0.0f ? (minmax.maxs[ 0 ] - minmax.mins[ 0 ]) / minmax.maxs[ 0 ] : 0.0f;
	//%	gradient[ 1 ] = minmax.maxs[ 1 ] > 0.0f ? (minmax.maxs[ 1 ] - minmax.mins[ 1 ]) / minmax.maxs[ 1 ] : 0.0f;
	//%	gradient[ 2 ] = minmax.maxs[ 2 ] > 0.0f ? (minmax.maxs[ 2 ] - minmax.mins[ 2 ]) / minmax.maxs[ 2 ] : 0.0f;

	/* newer: another contrast function */
	gradient = ( minmax.maxs - minmax.mins ) * minmax.maxs;
}



/*
   RadSubdivideDiffuseLight()
   subdivides a radiosity winding until it is smaller than subdivide, then generates an area light
 */

#define RADIOSITY_MAX_GRADIENT      0.75f   //%	0.25f
#define RADIOSITY_VALUE             500.0f
#define RADIOSITY_MIN               0.0001f
#define RADIOSITY_CLIP_EPSILON      0.125f



static void RadSubdivideDiffuseLight( int lightmapNum, bspDrawSurface_t *ds, rawLightmap_t *lm, const shaderInfo_t *si,
                                      float scale, float subdivide, radWinding_t *rw, clipWork_t *cw ){
	int i, style = 0;
	float dist, area, value;
	Vector3 normal, color, gradient;


	/* dummy check */
	if ( rw == NULL || rw->numVerts < 3 ) {
		return;
	}

	/* get bounds for winding */
	MinMax minmax;
	for ( const radVert_t& vert : Span( rw->verts, rw->numVerts ) )
		minmax.extend( vert.xyz );

	/* subdivide if necessary */
	for ( i = 0; i < 3; i++ )
	{
		if ( minmax.maxs[ i ] - minmax.mins[ i ] > subdivide ) {
			auto front = std::make_unique<radWinding_t>();
			auto back = std::make_unique<radWinding_t>();

			/* make axial plane */
			dist = ( minmax.maxs[ i ] + minmax.mins[ i ] ) * 0.5f;

			/* clip the winding */
			RadClipWindingEpsilon( rw, g_vector3_axes[i], dist, RADIOSITY_CLIP_EPSILON, front.get(), back.get(), cw );

			/* recurse */
			RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, front.get(), cw );
			RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, back.get(), cw );
			return;
		}
	}

	/* check area */
	area = 0.0f;
	for ( i = 2; i < rw->numVerts; i++ )
	{
		area += 0.5f * vector3_length( vector3_cross( rw->verts[ i - 1 ].xyz - rw->verts[ 0 ].xyz, rw->verts[ i ].xyz - rw->verts[ 0 ].xyz ) );
	}
	if ( area < 1.0f || area > 20000000.0f ) {
		return;
	}

	/* more subdivision may be necessary */
	if ( bouncing ) {
		/* get color sample for the surface fragment */
		RadSample( lightmapNum, ds, lm, si, rw, color, gradient, &style );

		/* if color gradient is too high, subdivide again */
		if ( subdivide > minDiffuseSubdivide &&
		     ( gradient[ 0 ] > RADIOSITY_MAX_GRADIENT || gradient[ 1 ] > RADIOSITY_MAX_GRADIENT || gradient[ 2 ] > RADIOSITY_MAX_GRADIENT ) ) {
			RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, ( subdivide / 2.0f ), rw, cw );
			return;
		}
	}

	/* create an average normal */
	normal.set( 0 );
	for ( const radVert_t& vert : Span( rw->verts, rw->numVerts ) )
	{
		normal += vert.normal;
	}
	normal /= rw->numVerts;
	if ( VectorNormalize( normal ) == 0.0f ) {
		return;
	}

	/* early out? */
	if ( bouncing && vector3_length( color ) < RADIOSITY_MIN ) {
		return;
	}

	/* debug code */
	//%	Sys_Printf( "Size: %d %d %d\n", (int) (minmax.maxs[ 0 ] - minmax.mins[ 0 ]), (int) (minmax.maxs[ 1 ] - minmax.mins[ 1 ]), (int) (minmax.maxs[ 2 ] - minmax.mins[ 2 ]) );
	//%	Sys_Printf( "Grad: %f %f %f\n", gradient[ 0 ], gradient[ 1 ], gradient[ 2 ] );

	/* increment counts */
	numDiffuseLights++;
	switch ( ds->surfaceType )
	{
	case MST_PLANAR:
		numBrushDiffuseLights++;
		break;

	case MST_TRIANGLE_SOUP:
		numTriangleDiffuseLights++;
		break;

	case MST_PATCH:
		numPatchDiffuseLights++;
		break;
	}


	/* create a light */
	ThreadLock();
	light_t& light = lights.emplace_front();
	ThreadUnlock();

	/* initialize the light */
	light.flags = LightFlags::DefaultArea;
	light.type = ELightType::Area;
	light.si = si;
	light.fade = 1.0f;
	/* create a regular winding */
	light.w = AllocWinding( rw->numVerts );
	for ( const radVert_t& vert : Span( rw->verts, rw->numVerts ) )
	{
		light.w.push_back( vert.xyz );
	}

	/* set falloff threshold */
	light.falloffTolerance = falloffTolerance;

	/* bouncing light? */
	if ( !bouncing ) {
		/* This is weird. This actually handles surfacelight and not
		 * bounces. */

		/* handle first-pass lights in normal q3a style */
		value = si->value;
		light.photons = value * area * areaScale;
		light.add = value * formFactorValueScale * areaScale;
		light.color = si->color;
		light.style = noStyles || !style_is_valid( si->lightStyle )? LS_NORMAL : si->lightStyle;

		/* set origin */
		light.origin = minmax.origin();

		/* nudge it off the plane a bit */
		light.normal = normal;
		light.origin += light.normal;
		light.dist = vector3_dot( light.origin, normal );

#if 0
		/* optionally create a point backsplash light */
		if ( si->backsplashFraction > 0 ) {
			/* allocate a new point light */
			light_t& splash = lights.emplace_front();

			/* set it up */
			splash.flags = LightFlags::DefaultQ3A;
			splash.type = ELightType::Point;
			splash.photons = light.photons * si->backsplashFraction;

			splash.fade = 1.0f;
			splash.si = si;
			splash.origin = normal * si->backsplashDistance + light.origin;
			splash.color = si->color;

			splash.falloffTolerance = falloffTolerance;
			splash.style = noStyles ? LS_NORMAL : light.style;

			/* add to counts */
			numPointLights++;
		}
#endif

#if 1
		/* optionally create area backsplash light */
		//if ( original && si->backsplashFraction > 0 ) {
		if ( si->backsplashFraction > 0 && !( si->compileFlags & C_SKY ) ) {
			/* allocate a new area light */
			ThreadLock();
			light_t& splash = lights.emplace_front();
			ThreadUnlock();

			/* set it up */
			splash.flags = LightFlags::DefaultArea;
			splash.type = ELightType::Area;
			splash.photons = light.photons * 7.0f * si->backsplashFraction;
			splash.add = light.add * 7.0f * si->backsplashFraction;
			splash.fade = 1.0f;
			splash.si = si;
			splash.color = si->color;
			splash.falloffTolerance = falloffTolerance;
			splash.style = noStyles || !style_is_valid( si->lightStyle )? LS_NORMAL : si->lightStyle;

			/* create a regular winding */
			splash.w = AllocWinding( rw->numVerts );
			for ( i = 0; i < rw->numVerts; i++ )
				splash.w.push_back( rw->verts[rw->numVerts - 1 - i].xyz + normal * si->backsplashDistance );

			splash.origin = normal * si->backsplashDistance + light.origin;
			splash.normal = -normal;
			splash.dist = vector3_dot( splash.origin, splash.normal );

//			splash.flags |= LightFlags::Twosided;
		}
#endif

	}
	else
	{
		/* handle bounced light (radiosity) a little differently */
		value = RADIOSITY_VALUE * si->bounceScale * 0.375f;
		light.photons = value * area * bounceScale;
		light.add = value * formFactorValueScale * bounceScale;
		light.color = color;
		light.style = noStyles || !style_is_valid( style )? LS_NORMAL : style;

		/* set origin */
		light.origin = WindingCenter( light.w );

		/* nudge it off the plane a bit */
		light.normal = normal;
		light.origin += light.normal;
		light.dist = vector3_dot( light.origin, normal );
	}

	if (light.photons < 0 || light.add < 0 || light.color[0] < 0 || light.color[1] < 0 || light.color[2] < 0)
		Sys_Printf( "BUG: RadSubdivideDiffuseLight created a darkbulb\n" );

	/* emit light from both sides? */
	if ( si->compileFlags & C_FOG || si->twoSided ) {
		light.flags |= LightFlags::Twosided;
	}

	//%	Sys_Printf( "\nAL: C: (%6f, %6f, %6f) [%6f] N: (%6f, %6f, %6f) %s\n",
	//%		light.color[ 0 ], light.color[ 1 ], light.color[ 2 ], light.add,
	//%		light.normal[ 0 ], light.normal[ 1 ], light.normal[ 2 ],
	//%		light.si->shader );
}


/*
   RadLightForTriangles()
   creates unbounced diffuse lights for triangle soup (misc_models, etc)
 */

void RadLightForTriangles( int num, int lightmapNum, rawLightmap_t *lm, const shaderInfo_t *si, float scale, float subdivide, clipWork_t *cw ){
	int i, j, k, v;
	bspDrawSurface_t    *ds;
	radWinding_t rw;


	/* get surface */
	ds = &bspDrawSurfaces[ num ];

	/* each triangle is a potential emitter */
	rw.numVerts = 3;
	for ( i = 0; i < ds->numIndexes; i += 3 )
	{
		/* copy each vert */
		for ( j = 0; j < 3; j++ )
		{
			/* get vertex index and rad vertex luxel */
			v = ds->firstVert + bspDrawIndexes[ ds->firstIndex + i + j ];

			/* get most everything */
			memcpy( &rw.verts[ j ], &yDrawVerts[ v ], sizeof( bspDrawVert_t ) );

			/* fix colors */
			for ( k = 0; k < MAX_LIGHTMAPS; k++ )
			{
				rw.verts[ j ].color[ k ].rgb() = getRadVertexLuxel( k, ds->firstVert + bspDrawIndexes[ ds->firstIndex + i + j ] );
				rw.verts[ j ].color[ k ].alpha() = yDrawVerts[ v ].color[ k ].alpha();
			}
		}

		/* subdivide into area lights */
		RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, &rw, cw );
	}
}



/*
   RadLightForPatch()
   creates unbounced diffuse lights for patches
 */

#define PLANAR_EPSILON  0.1f

void RadLightForPatch( int num, int lightmapNum, rawLightmap_t *lm, const shaderInfo_t *si, float scale, float subdivide, clipWork_t *cw ){
	int i, x, y, v, t, pw[ 5 ], r;
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info;
	bspDrawVert_t       *bogus;
	bspDrawVert_t       *dv[ 4 ];
	mesh_t src, *subdivided, *mesh;
	bool planar;
	radWinding_t rw;


	/* get surface */
	ds = &bspDrawSurfaces[ num ];
	info = &surfaceInfos[ num ];

	/* construct a bogus vert list with color index stuffed into color[ 0 ] */
	bogus = safe_malloc( ds->numVerts * sizeof( bspDrawVert_t ) );
	memcpy( bogus, &yDrawVerts[ ds->firstVert ], ds->numVerts * sizeof( bspDrawVert_t ) );
	for ( i = 0; i < ds->numVerts; i++ )
		bogus[ i ].color[ 0 ][ 0 ] = i;

	/* build a subdivided mesh identical to shadow facets for this patch */
	/* this MUST MATCH FacetsForPatch() identically! */
	src.width = ds->patchWidth;
	src.height = ds->patchHeight;
	src.verts = bogus;
	//%	subdivided = SubdivideMesh( src, 8, 512 );
	subdivided = SubdivideMesh2( src, info->patchIterations );
	PutMeshOnCurve( *subdivided );
	//%	MakeMeshNormals( *subdivided );
	mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );
	free( bogus );

	/* FIXME: build interpolation table into color[ 1 ] */

	/* fix up color indexes */
	for ( bspDrawVert_t& vert : Span( mesh->verts, mesh->width * mesh->height ) )
	{
		if ( vert.color[ 0 ][ 0 ] >= ds->numVerts ) {
			vert.color[ 0 ][ 0 ] = ds->numVerts - 1;
		}
	}

	/* iterate through the mesh quads */
	for ( y = 0; y < ( mesh->height - 1 ); y++ )
	{
		for ( x = 0; x < ( mesh->width - 1 ); x++ )
		{
			/* set indexes */
			pw[ 0 ] = x + ( y * mesh->width );
			pw[ 1 ] = x + ( ( y + 1 ) * mesh->width );
			pw[ 2 ] = x + 1 + ( ( y + 1 ) * mesh->width );
			pw[ 3 ] = x + 1 + ( y * mesh->width );
			pw[ 4 ] = x + ( y * mesh->width );    /* same as pw[ 0 ] */

			/* set radix */
			r = ( x + y ) & 1;

			/* get drawverts */
			dv[ 0 ] = &mesh->verts[ pw[ r + 0 ] ];
			dv[ 1 ] = &mesh->verts[ pw[ r + 1 ] ];
			dv[ 2 ] = &mesh->verts[ pw[ r + 2 ] ];
			dv[ 3 ] = &mesh->verts[ pw[ r + 3 ] ];

			/* planar? */
			Plane3f plane;
			planar = PlaneFromPoints( plane, dv[ 0 ]->xyz, dv[ 1 ]->xyz, dv[ 2 ]->xyz );
			if ( planar ) {
				if ( fabs( plane3_distance_to_point( plane, dv[ 1 ]->xyz ) ) > PLANAR_EPSILON ) {
					planar = false;
				}
			}

			/* generate a quad */
			if ( planar ) {
				rw.numVerts = 4;
				for ( v = 0; v < 4; v++ )
				{
					/* get most everything */
					memcpy( &rw.verts[ v ], dv[ v ], sizeof( bspDrawVert_t ) );

					/* fix colors */
					for ( i = 0; i < MAX_LIGHTMAPS; i++ )
					{
						rw.verts[ v ].color[ i ].rgb() = getRadVertexLuxel( i, ds->firstVert + dv[ v ]->color[ 0 ][ 0 ] );
						rw.verts[ v ].color[ i ].alpha() = dv[ v ]->color[ i ].alpha();
					}
				}

				/* subdivide into area lights */
				RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, &rw, cw );
			}

			/* generate 2 tris */
			else
			{
				rw.numVerts = 3;
				for ( t = 0; t < 2; t++ )
				{
					for ( v = 0; v < 3 + t; v++ )
					{
						/* get "other" triangle (stupid hacky logic, but whatevah) */
						if ( v == 1 && t == 1 ) {
							v++;
						}

						/* get most everything */
						memcpy( &rw.verts[ v ], dv[ v ], sizeof( bspDrawVert_t ) );

						/* fix colors */
						for ( i = 0; i < MAX_LIGHTMAPS; i++ )
						{
							rw.verts[ v ].color[ i ].rgb() = getRadVertexLuxel( i, ds->firstVert + dv[ v ]->color[ 0 ][ 0 ] );
							rw.verts[ v ].color[ i ].alpha() = dv[ v ]->color[ i ].alpha();
						}
					}

					/* subdivide into area lights */
					RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, &rw, cw );
				}
			}
		}
	}

	/* free the mesh */
	FreeMesh( mesh );
}




/*
   RadLight()
   creates unbounced diffuse lights for a given surface
 */

static void RadLight( int num ){
	int lightmapNum;
	float scale, subdivide;
	int contentFlags, surfaceFlags, compileFlags;
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info;
	rawLightmap_t       *lm;
	const shaderInfo_t  *si;
	clipWork_t cw;


	/* get drawsurface, lightmap, and shader info */
	ds = &bspDrawSurfaces[ num ];
	info = &surfaceInfos[ num ];
	lm = info->lm;
	si = info->si;
	scale = si->bounceScale;

	/* find nodraw bit */
	contentFlags = surfaceFlags = compileFlags = 0;
	ApplySurfaceParm( "nodraw", &contentFlags, &surfaceFlags, &compileFlags );

	// jal : avoid bouncing on trans surfaces
	ApplySurfaceParm( "trans", &contentFlags, &surfaceFlags, &compileFlags );

	/* early outs? */
	if ( scale <= 0.0f || ( si->compileFlags & C_SKY ) || si->autosprite ||
	     ( bspShaders[ ds->shaderNum ].contentFlags & contentFlags ) || ( bspShaders[ ds->shaderNum ].surfaceFlags & surfaceFlags ) ||
	     ( si->compileFlags & compileFlags ) ) {
		return;
	}

	/* determine how much we need to chop up the surface */
	if ( si->lightSubdivide ) {
		subdivide = si->lightSubdivide;
	}
	else{
		subdivide = diffuseSubdivide;
	}

	/* inc counts */
	numDiffuseSurfaces++;

	/* iterate through styles (this could be more efficient, yes) */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
	{
		/* switch on type */
		if ( ds->lightmapStyles[ lightmapNum ] != LS_NONE && ds->lightmapStyles[ lightmapNum ] != LS_UNUSED ) {
			switch ( ds->surfaceType )
			{
			case MST_PLANAR:
			case MST_TRIANGLE_SOUP:
				RadLightForTriangles( num, lightmapNum, lm, si, scale, subdivide, &cw );
				break;

			case MST_PATCH:
				RadLightForPatch( num, lightmapNum, lm, si, scale, subdivide, &cw );
				break;

			default:
				break;
			}
		}
	}
}



/*
   RadCreateDiffuseLights()
   creates lights for unbounced light on surfaces in the bsp
 */

void RadCreateDiffuseLights(){
	/* startup */
	Sys_FPrintf( SYS_VRB, "--- RadCreateDiffuseLights ---\n" );
	numDiffuseSurfaces = 0;
	numDiffuseLights = 0;
	numBrushDiffuseLights = 0;
	numTriangleDiffuseLights = 0;
	numPatchDiffuseLights = 0;
	static int iterations = 0;

	/* hit every surface (threaded) */
	RunThreadsOnIndividual( bspDrawSurfaces.size(), true, RadLight );

	/* dump the lights generated to a file */
	if ( dump && !lights.empty() ) {
		char dumpName[ 1024 ], ext[ 64 ];

		strcpy( dumpName, source );
		sprintf( ext, "_bounce_%03d.map", iterations );
		path_set_extension( dumpName, ext );
		FILE *file = fopen( dumpName, "wb" );
		Sys_Printf( "Writing %s...\n", dumpName );
		if ( file ) {
			for ( const light_t& light : lights )
			{
				fprintf( file,
				         "{\n"
				         "\"classname\" \"light\"\n"
				         "\"light\" \"%d\"\n"
				         "\"origin\" \"%.0f %.0f %.0f\"\n"
				         "\"_color\" \"%.3f %.3f %.3f\"\n"
				         "}\n",

				         (int) light.add,

				         light.origin[ 0 ],
				         light.origin[ 1 ],
				         light.origin[ 2 ],

				         light.color[ 0 ],
				         light.color[ 1 ],
				         light.color[ 2 ] );
			}
			fclose( file );
		}
	}

	/* increment */
	iterations++;

	/* print counts */
	Sys_Printf( "%8d diffuse surfaces\n", numDiffuseSurfaces );
	Sys_FPrintf( SYS_VRB, "%8d total diffuse lights\n", numDiffuseLights );
	Sys_FPrintf( SYS_VRB, "%8d brush diffuse lights\n", numBrushDiffuseLights );
	Sys_FPrintf( SYS_VRB, "%8d patch diffuse lights\n", numPatchDiffuseLights );
	Sys_FPrintf( SYS_VRB, "%8d triangle diffuse lights\n", numTriangleDiffuseLights );
}

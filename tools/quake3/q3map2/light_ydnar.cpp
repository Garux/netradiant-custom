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


// http://www.graficaobscura.com/matrix/index.html
static void color_saturate( Vector3& color, float saturation ){
	/* This is the luminance vector. Notice here that we do not use the standard NTSC weights of 0.299, 0.587, and 0.114.
	The NTSC weights are only applicable to RGB colors in a gamma 2.2 color space. For linear RGB colors these values are better. */
	const Vector3 rgb2gray( 0.3086, 0.6094, 0.0820 );
	Matrix4 tra( g_matrix4_identity );
	tra.x().vec3().set( rgb2gray.x() * ( 1 - saturation ) );
	tra.y().vec3().set( rgb2gray.y() * ( 1 - saturation ) );
	tra.z().vec3().set( rgb2gray.z() * ( 1 - saturation ) );
	tra.xx() += saturation;
	tra.yy() += saturation;
	tra.zz() += saturation;
	matrix4_transform_direction( tra, color );
}



/*
   ColorToBytes()
   ydnar: moved to here 2001-02-04
 */

Vector3b ColorToBytes( const Vector3& color, float scale ){
	int i;
	float max, gamma;
	float inv, dif;


	/* ydnar: scaling necessary for simulating r_overbrightBits on external lightmaps */
	if ( scale <= 0.0f ) {
		scale = 1.0f;
	}

	/* make a local copy */
	Vector3 sample = color * scale;

	if( g_lightmapSaturation != 1 )
		color_saturate( sample, g_lightmapSaturation );

	/* muck with it */
	gamma = 1.0f / lightmapGamma;
	for ( i = 0; i < 3; i++ )
	{
		/* handle negative light */
		if ( sample[ i ] < 0.0f ) {
			sample[ i ] = 0.0f;
			continue;
		}

		/* gamma */
		sample[ i ] = pow( sample[ i ] / 255.0f, gamma ) * 255.0f;
	}

	if ( lightmapExposure == 0 ) {
		/* clamp with color normalization */
		max = vector3_max_component( sample );
		if ( max > maxLight ) {
			sample *= ( maxLight / max );
		}
	}
	else
	{
		inv = 1.f / lightmapExposure;
		//Exposure

		max = vector3_max_component( sample );

		dif = ( 1 -  exp( -max * inv ) )  *  255;

		if ( max > 0 ) {
			dif = dif / max;
		}
		else
		{
			dif = 0;
		}

		sample *= dif;
	}


	/* compensate for ingame overbrighting/bitshifting */
	sample *= ( 1.0f / lightmapCompensate );

	/* contrast */
	if ( lightmapContrast != 1.0f ){
		for ( i = 0; i < 3; i++ ){
			sample[i] = std::max( 0.f, lightmapContrast * ( sample[i] - 128 ) + 128 );
		}
		/* clamp with color normalization */
		max = vector3_max_component( sample );
		if ( max > 255.0f ) {
			sample *= ( 255.0f / max );
		}
	}

	/* sRGB lightmaps */
	if ( lightmapsRGB ) {
		sample[0] = floor( Image_sRGBFloatFromLinearFloat( sample[0] * ( 1.0 / 255.0 ) ) * 255.0 + 0.5 );
		sample[1] = floor( Image_sRGBFloatFromLinearFloat( sample[1] * ( 1.0 / 255.0 ) ) * 255.0 + 0.5 );
		sample[2] = floor( Image_sRGBFloatFromLinearFloat( sample[2] * ( 1.0 / 255.0 ) ) * 255.0 + 0.5 );
	}

	/* store it off */
	return sample;
}



/* -------------------------------------------------------------------------------

   this section deals with phong shading (normal interpolation across brush faces)

   ------------------------------------------------------------------------------- */

/*
   SmoothNormals()
   smooths together coincident vertex normals across the bsp
 */

#define MAX_SAMPLES             256
#define THETA_EPSILON           0.000001
#define EQUAL_NORMAL_EPSILON    0.01f

void SmoothNormals(){
	int fOld;
	float shadeAngle, defaultShadeAngle, maxShadeAngle;
	int indexes[ MAX_SAMPLES ];
	Vector3 votes[ MAX_SAMPLES ];
	const int numBSPDrawVerts = bspDrawVerts.size();


	/* allocate shade angle table */
	std::vector<float> shadeAngles( numBSPDrawVerts, 0 );

	/* allocate smoothed table */
	std::vector<std::uint8_t> smoothed( numBSPDrawVerts, false );

	/* set default shade angle */
	defaultShadeAngle = degrees_to_radians( shadeAngleDegrees );
	maxShadeAngle = 0;

	/* run through every surface and flag verts belonging to non-lightmapped surfaces
	   and set per-vertex smoothing angle */
	for ( size_t i = 0; i < bspDrawSurfaces.size(); ++i )
	{
		/* get drawsurf */
		bspDrawSurface_t& ds = bspDrawSurfaces[ i ];

		/* get shader for shade angle */
		const shaderInfo_t *si = surfaceInfos[ i ].si;
		if ( si->shadeAngleDegrees ) {
			shadeAngle = degrees_to_radians( si->shadeAngleDegrees );
		}
		else{
			shadeAngle = defaultShadeAngle;
		}
		value_maximize( maxShadeAngle, shadeAngle );

		/* flag its verts */
		for ( int j = 0; j < ds.numVerts; j++ )
		{
			const int f = ds.firstVert + j;
			shadeAngles[ f ] = shadeAngle;
			if ( ds.surfaceType == MST_TRIANGLE_SOUP ) {
				smoothed[ f ] = true;
			}
		}
	}

	/* bail if no surfaces have a shade angle */
	if ( maxShadeAngle == 0 ) {
		return;
	}

	/* init pacifier */
	fOld = -1;
	Timer timer;

	/* go through the list of vertexes */
	for ( int i = 0; i < numBSPDrawVerts; i++ )
	{
		/* print pacifier */
		if ( const int f = 10 * i / numBSPDrawVerts; f != fOld ) {
			fOld = f;
			Sys_Printf( "%i...", f );
		}

		/* already smoothed? */
		if ( smoothed[ i ] ) {
			continue;
		}

		/* clear */
		Vector3 average( 0 );
		int numVerts = 0;
		int numVotes = 0;

		/* build a table of coincident vertexes */
		for ( int j = i; j < numBSPDrawVerts && numVerts < MAX_SAMPLES; j++ )
		{
			/* already smoothed? */
			if ( smoothed[ j ] ) {
				continue;
			}

			/* test vertexes */
			if ( !VectorCompare( yDrawVerts[ i ].xyz, yDrawVerts[ j ].xyz ) ) {
				continue;
			}

			/* use smallest shade angle */
			shadeAngle = std::min( shadeAngles[ i ], shadeAngles[ j ] );

			/* check shade angle */
			const double dot = std::clamp( vector3_dot( bspDrawVerts[ i ].normal, bspDrawVerts[ j ].normal ), -1.0, 1.0 );
			if ( acos( dot ) + THETA_EPSILON >= shadeAngle ) {
				//Sys_Printf( "F(%3.3f >= %3.3f) ", RAD2DEG( testAngle ), RAD2DEG( shadeAngle ) );
				continue;
			}
			//Sys_Printf( "P(%3.3f < %3.3f) ", RAD2DEG( testAngle ), RAD2DEG( shadeAngle ) );

			/* add to the list */
			indexes[ numVerts++ ] = j;

			/* flag vertex */
			smoothed[ j ] = true;

			/* see if this normal has already been voted */
			int k;
			for ( k = 0; k < numVotes; k++ )
			{
				if ( vector3_equal_epsilon( bspDrawVerts[ j ].normal, votes[ k ], EQUAL_NORMAL_EPSILON ) ) {
					break;
				}
			}

			/* add a new vote? */
			if ( k == numVotes && numVotes < MAX_SAMPLES ) {
				average += bspDrawVerts[ j ].normal;
				votes[ numVotes ] = bspDrawVerts[ j ].normal;
				numVotes++;
			}
		}

		/* don't average for less than 2 verts */
		if ( numVerts < 2 ) {
			continue;
		}

		/* average normal */
		if ( VectorNormalize( average ) != 0 ) {
			/* smooth */
			for ( int j = 0; j < numVerts; j++ )
				yDrawVerts[ indexes[ j ] ].normal = average;
		}
	}

	/* print time */
	Sys_Printf( " (%i)\n", int( timer.elapsed_sec() ) );
}



/* ------------------------------------------------------------------------------- */

/*
   ClusterVisible()
   determines if two clusters are visible to each other using the PVS
 */

bool ClusterVisible( int a, int b ){
	/* dummy check */
	if ( a < 0 || b < 0 ) {
		return false;
	}

	/* early out */
	if ( a == b ) {
		return true;
	}

	/* not vised? */
	if ( bspVisBytes.size() <= 8 ) {
		return true;
	}

	/* get pvs data */
	/* portalClusters = ( (int *) bspVisBytes )[ 0 ]; */
	const int leafBytes = ( (int*) bspVisBytes.data() )[ 1 ];
	const byte *pvs = bspVisBytes.data() + VIS_HEADER_SIZE + ( a * leafBytes );

	/* check */
	return bit_is_enabled( pvs, b );
}



/*
   PointInLeafNum_r()
   borrowed from vlight.c
 */

static int PointInLeafNum_r( const Vector3& point, int nodenum ){
	int leafnum;

	while ( nodenum >= 0 )
	{
		const bspNode_t& node = bspNodes[ nodenum ];
		const bspPlane_t& plane = bspPlanes[ node.planeNum ];
		const double dist = plane3_distance_to_point( plane, point );
		if ( dist > 0.1 ) {
			nodenum = node.children[ 0 ];
		}
		else if ( dist < -0.1 ) {
			nodenum = node.children[ 1 ];
		}
		else
		{
			leafnum = PointInLeafNum_r( point, node.children[ 0 ] );
			if ( bspLeafs[ leafnum ].cluster != -1 ) {
				return leafnum;
			}
			nodenum = node.children[ 1 ];
		}
	}

	leafnum = -nodenum - 1;
	return leafnum;
}



/*
   PointInLeafnum()
   borrowed from vlight.c
 */

static int PointInLeafNum( const Vector3& point ){
	return PointInLeafNum_r( point, 0 );
}



/*
   ClusterForPoint() - ydnar
   returns the pvs cluster for point
 */

static int ClusterForPoint( const Vector3& point ){
	int leafNum;


	/* get leafNum for point */
	leafNum = PointInLeafNum( point );
	if ( leafNum < 0 ) {
		return -1;
	}

	/* return the cluster */
	return bspLeafs[ leafNum ].cluster;
}



/*
   ClusterVisibleToPoint() - ydnar
   returns true if point can "see" cluster
 */

static bool ClusterVisibleToPoint( const Vector3& point, int cluster ){
	int pointCluster;


	/* get leafNum for point */
	pointCluster = ClusterForPoint( point );
	if ( pointCluster < 0 ) {
		return false;
	}

	/* check pvs */
	return ClusterVisible( pointCluster, cluster );
}



/*
   ClusterForPointExt() - ydnar
   also takes brushes into account for occlusion testing
 */

int ClusterForPointExt( const Vector3& point, float epsilon ){
	/* get leaf for point */
	const int leafNum = PointInLeafNum( point );
	if ( leafNum < 0 ) {
		return -1;
	}
	const bspLeaf_t& leaf = bspLeafs[ leafNum ];

	/* get the cluster */
	const int cluster = leaf.cluster;
	if ( cluster < 0 ) {
		return -1;
	}

	/* transparent leaf, so check point against all brushes in the leaf */
	const int *brushes = &bspLeafBrushes[ leaf.firstBSPLeafBrush ];
	const int numBSPBrushes = leaf.numBSPLeafBrushes;
	for ( int i = 0; i < numBSPBrushes; i++ )
	{
		/* get parts */
		const int b = brushes[ i ];
		if ( b > maxOpaqueBrush ) {
			continue;
		}
		if ( !opaqueBrushes[ b ] ) {
			continue;
		}

		const bspBrush_t& brush = bspBrushes[ b ];
		/* check point against all planes */
		bool inside = true;
		for ( int j = 0; j < brush.numSides && inside; j++ )
		{
			const bspPlane_t& plane = bspPlanes[ bspBrushSides[ brush.firstSide + j ].planeNum ];
			if ( plane3_distance_to_point( plane, point ) > epsilon ) {
				inside = false;
			}
		}

		/* if inside, return bogus cluster */
		if ( inside ) {
			return -1 - b;
		}
	}

	/* if the point made it this far, it's not inside any opaque brushes */
	return cluster;
}



/*
   ClusterForPointExtFilter() - ydnar
   adds cluster checking against a list of known valid clusters
 */

static int ClusterForPointExtFilter( const Vector3& point, float epsilon, int numClusters, int *clusters ){
	int i, cluster;


	/* get cluster for point */
	cluster = ClusterForPointExt( point, epsilon );

	/* check if filtering is necessary */
	if ( cluster < 0 || numClusters <= 0 || clusters == NULL ) {
		return cluster;
	}

	/* filter */
	for ( i = 0; i < numClusters; i++ )
	{
		if ( cluster == clusters[ i ] || ClusterVisible( cluster, clusters[ i ] ) ) {
			return cluster;
		}
	}

	/* failed */
	return -1;
}



/*
   ShaderForPointInLeaf() - ydnar
   checks a point against all brushes in a leaf, returning the shader of the brush
   also sets the cumulative surface and content flags for the brush hit
 */

static int ShaderForPointInLeaf( const Vector3& point, int leafNum, float epsilon, int wantContentFlags, int wantSurfaceFlags, int *contentFlags, int *surfaceFlags ){
	int allSurfaceFlags, allContentFlags;


	/* clear things out first */
	*surfaceFlags = 0;
	*contentFlags = 0;

	/* get leaf */
	if ( leafNum < 0 ) {
		return -1;
	}
	const bspLeaf_t& leaf = bspLeafs[ leafNum ];

	/* transparent leaf, so check point against all brushes in the leaf */
	const int *brushes = &bspLeafBrushes[ leaf.firstBSPLeafBrush ];
	const int numBSPBrushes = leaf.numBSPLeafBrushes;
	for ( int i = 0; i < numBSPBrushes; i++ )
	{
		/* get parts */
		const bspBrush_t& brush = bspBrushes[ brushes[ i ] ];

		/* check point against all planes */
		bool inside = true;
		allSurfaceFlags = 0;
		allContentFlags = 0;
		for ( int j = 0; j < brush.numSides && inside; j++ )
		{
			const bspBrushSide_t& side = bspBrushSides[ brush.firstSide + j ];
			const bspPlane_t& plane = bspPlanes[ side.planeNum ];
			if ( plane3_distance_to_point( plane, point ) > epsilon ) {
				inside = false;
			}
			else
			{
				const bspShader_t& shader = bspShaders[ side.shaderNum ];
				allSurfaceFlags |= shader.surfaceFlags;
				allContentFlags |= shader.contentFlags;
			}
		}

		/* handle if inside */
		if ( inside ) {
			/* if there are desired flags, check for same and continue if they aren't matched */
			if ( wantContentFlags && !( wantContentFlags & allContentFlags ) ) {
				continue;
			}
			if ( wantSurfaceFlags && !( wantSurfaceFlags & allSurfaceFlags ) ) {
				continue;
			}

			/* store the cumulative flags and return the brush shader (which is mostly useless) */
			*surfaceFlags = allSurfaceFlags;
			*contentFlags = allContentFlags;
			return brush.shaderNum;
		}
	}

	/* if the point made it this far, it's not inside any brushes */
	return -1;
}



/* -------------------------------------------------------------------------------

   this section deals with phong shaded lightmap tracing

   ------------------------------------------------------------------------------- */

/* 9th rewrite (recursive subdivision of a lightmap triangle) */

/*
   CalcTangentVectors()
   calculates the st tangent vectors for normalmapping
 */

template<std::size_t numVerts>
bool CalcTangentVectors( const std::array<const bspDrawVert_t *, numVerts>& dv, Vector3 (&stv)[numVerts], Vector3 (&ttv)[numVerts] ){
	float bb, s, t;
	Vector3 bary;


	/* calculate barycentric basis for the triangle */
	bb = ( dv[ 1 ]->st[ 0 ] - dv[ 0 ]->st[ 0 ] ) * ( dv[ 2 ]->st[ 1 ] - dv[ 0 ]->st[ 1 ] ) - ( dv[ 2 ]->st[ 0 ] - dv[ 0 ]->st[ 0 ] ) * ( dv[ 1 ]->st[ 1 ] - dv[ 0 ]->st[ 1 ] );
	if ( fabs( bb ) < 0.00000001f ) {
		return false;
	}

	/* do each vertex */
	for ( std::size_t i = 0; i < numVerts; ++i )
	{
		/* calculate s tangent vector */
		s = dv[ i ]->st[ 0 ] + 10.0f;
		t = dv[ i ]->st[ 1 ];
		bary[ 0 ] = ( ( dv[ 1 ]->st[ 0 ] - s ) * ( dv[ 2 ]->st[ 1 ] - t ) - ( dv[ 2 ]->st[ 0 ] - s ) * ( dv[ 1 ]->st[ 1 ] - t ) ) / bb;
		bary[ 1 ] = ( ( dv[ 2 ]->st[ 0 ] - s ) * ( dv[ 0 ]->st[ 1 ] - t ) - ( dv[ 0 ]->st[ 0 ] - s ) * ( dv[ 2 ]->st[ 1 ] - t ) ) / bb;
		bary[ 2 ] = ( ( dv[ 0 ]->st[ 0 ] - s ) * ( dv[ 1 ]->st[ 1 ] - t ) - ( dv[ 1 ]->st[ 0 ] - s ) * ( dv[ 0 ]->st[ 1 ] - t ) ) / bb;

		stv[ i ][ 0 ] = bary[ 0 ] * dv[ 0 ]->xyz[ 0 ] + bary[ 1 ] * dv[ 1 ]->xyz[ 0 ] + bary[ 2 ] * dv[ 2 ]->xyz[ 0 ];
		stv[ i ][ 1 ] = bary[ 0 ] * dv[ 0 ]->xyz[ 1 ] + bary[ 1 ] * dv[ 1 ]->xyz[ 1 ] + bary[ 2 ] * dv[ 2 ]->xyz[ 1 ];
		stv[ i ][ 2 ] = bary[ 0 ] * dv[ 0 ]->xyz[ 2 ] + bary[ 1 ] * dv[ 1 ]->xyz[ 2 ] + bary[ 2 ] * dv[ 2 ]->xyz[ 2 ];

		stv[ i ] -= dv[ i ]->xyz;
		VectorNormalize( stv[ i ] );

		/* calculate t tangent vector */
		s = dv[ i ]->st[ 0 ];
		t = dv[ i ]->st[ 1 ] + 10.0f;
		bary[ 0 ] = ( ( dv[ 1 ]->st[ 0 ] - s ) * ( dv[ 2 ]->st[ 1 ] - t ) - ( dv[ 2 ]->st[ 0 ] - s ) * ( dv[ 1 ]->st[ 1 ] - t ) ) / bb;
		bary[ 1 ] = ( ( dv[ 2 ]->st[ 0 ] - s ) * ( dv[ 0 ]->st[ 1 ] - t ) - ( dv[ 0 ]->st[ 0 ] - s ) * ( dv[ 2 ]->st[ 1 ] - t ) ) / bb;
		bary[ 2 ] = ( ( dv[ 0 ]->st[ 0 ] - s ) * ( dv[ 1 ]->st[ 1 ] - t ) - ( dv[ 1 ]->st[ 0 ] - s ) * ( dv[ 0 ]->st[ 1 ] - t ) ) / bb;

		ttv[ i ][ 0 ] = bary[ 0 ] * dv[ 0 ]->xyz[ 0 ] + bary[ 1 ] * dv[ 1 ]->xyz[ 0 ] + bary[ 2 ] * dv[ 2 ]->xyz[ 0 ];
		ttv[ i ][ 1 ] = bary[ 0 ] * dv[ 0 ]->xyz[ 1 ] + bary[ 1 ] * dv[ 1 ]->xyz[ 1 ] + bary[ 2 ] * dv[ 2 ]->xyz[ 1 ];
		ttv[ i ][ 2 ] = bary[ 0 ] * dv[ 0 ]->xyz[ 2 ] + bary[ 1 ] * dv[ 1 ]->xyz[ 2 ] + bary[ 2 ] * dv[ 2 ]->xyz[ 2 ];

		ttv[ i ] -= dv[ i ]->xyz;
		VectorNormalize( ttv[ i ] );

		/* debug code */
		//%	Sys_FPrintf( SYS_VRB, "%d S: (%f %f %f) T: (%f %f %f)\n", i,
		//%		stv[ i ][ 0 ], stv[ i ][ 1 ], stv[ i ][ 2 ], ttv[ i ][ 0 ], ttv[ i ][ 1 ], ttv[ i ][ 2 ] );
	}

	/* return to caller */
	return true;
}




/*
   PerturbNormal()
   perterbs the normal by the shader's normalmap in tangent space
 */

static void PerturbNormal( const bspDrawVert_t& dv, const shaderInfo_t *si, Vector3& pNormal, const Vector3 stv[ 3 ], const Vector3 ttv[ 3 ] ){
	/* passthrough */
	pNormal = dv.normal;

	/* sample normalmap */
	Color4f bump;
	if ( !RadSampleImage( si->normalImage->pixels, si->normalImage->width, si->normalImage->height, dv.st, bump ) ) {
		return;
	}

	/* remap sampled normal from [0,255] to [-1,-1] */
	bump.rgb() = ( bump.rgb() - Vector3( 127.0f ) ) * ( 1.0f / 127.5f );

	/* scale tangent vectors and add to original normal */
	pNormal = dv.normal + stv[ 0 ] * bump[ 0 ] + ttv[ 0 ] * bump[ 1 ] + dv.normal * bump[ 2 ];

	/* renormalize and return */
	VectorNormalize( pNormal );
}



/*
   MapSingleLuxel()
   maps a luxel for triangle bv at
 */

#define NUDGE           0.5f
#define BOGUS_NUDGE     -99999.0f

static int MapSingleLuxel( rawLightmap_t *lm, const surfaceInfo_t *info, const bspDrawVert_t& dv, const Plane3f* plane, float pass, const Vector3 stv[ 3 ], const Vector3 ttv[ 3 ], const Vector3 worldverts[ 3 ] ){
	int i, numClusters, *clusters, pointCluster;
	float           lightmapSampleOffset;
	const shaderInfo_t    *si;
	Vector3 pNormal;
	Vector3 vecs[ 3 ];
	Vector3 nudged;
	Vector3 origintwo;
	int j;
	float           *nudge;
	static float nudges[][ 2 ] =
	{
		//%{ 0, 0 },		/* try center first */
		{ -NUDGE, 0 },                      /* left */
		{ NUDGE, 0 },                       /* right */
		{ 0, NUDGE },                       /* up */
		{ 0, -NUDGE },                      /* down */
		{ -NUDGE, NUDGE },                  /* left/up */
		{ NUDGE, -NUDGE },                  /* right/down */
		{ NUDGE, NUDGE },                   /* right/up */
		{ -NUDGE, -NUDGE },                 /* left/down */
		{ BOGUS_NUDGE, BOGUS_NUDGE }
	};


	/* find luxel xy coords (fixme: subtract 0.5?) */
	const int x = std::clamp( int( dv.lightmap[ 0 ][ 0 ] ), 0, lm->sw - 1 );
	const int y = std::clamp( int( dv.lightmap[ 0 ][ 1 ] ), 0, lm->sh - 1 );

	/* set shader and cluster list */
	if ( info != NULL ) {
		si = info->si;
		numClusters = info->numSurfaceClusters;
		clusters = &surfaceClusters[ info->firstSurfaceCluster ];
	}
	else
	{
		si = NULL;
		numClusters = 0;
		clusters = NULL;
	}

	/* get luxel, origin, cluster, and normal */
	SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );
	Vector3& origin = lm->getSuperOrigin( x, y );
	Vector3& normal = lm->getSuperNormal( x, y );
	int& cluster = lm->getSuperCluster( x, y );

	/* don't attempt to remap occluded luxels for planar surfaces */
	if ( cluster == CLUSTER_OCCLUDED && lm->plane != NULL ) {
		return cluster;
	}

	/* only average the normal for premapped luxels */
	else if ( cluster >= 0 ) {
		/* do bumpmap calculations */
		if ( stv != NULL ) {
			PerturbNormal( dv, si, pNormal, stv, ttv );
		}
		else{
			pNormal = dv.normal;
		}

		/* add the additional normal data */
		normal += pNormal;
		luxel.count += 1.0f;
		return cluster;
	}

	/* otherwise, unmapped luxels (*cluster == CLUSTER_UNMAPPED) will have their full attributes calculated */

	/* get origin */

	/* axial lightmap projection */
	if ( lm->vecs != NULL ) {
		/* calculate an origin for the sample from the lightmap vectors */
		origin = lm->origin;
		for ( i = 0; i < 3; i++ )
		{
			/* add unless it's the axis, which is taken care of later */
			if ( i == lm->axisNum ) {
				continue;
			}
			origin[ i ] += ( x * lm->vecs[ 0 ][ i ] ) + ( y * lm->vecs[ 1 ][ i ] );
		}

		/* project the origin onto the plane */
		origin[ lm->axisNum ] -= plane3_distance_to_point( *plane, origin ) / plane->normal()[ lm->axisNum ];
	}

	/* non axial lightmap projection (explicit xyz) */
	else{
		origin = dv.xyz;
	}

	//////////////////////
	//27's test to make sure samples stay within the triangle boundaries
	//1) Test the sample origin to see if it lays on the wrong side of any edge (x/y)
	//2) if it does, nudge it onto the correct side.

	if ( worldverts != NULL && lightmapTriangleCheck ) {
		Plane3f hostplane;
		PlaneFromPoints( hostplane, worldverts[0], worldverts[1], worldverts[2] );

		for ( j = 0; j < 3; j++ )
		{
			for ( i = 0; i < 3; i++ )
			{
				Plane3f sideplane;
				//build plane using 2 edges and a normal
				const int next = ( i + 1 ) % 3;
				PlaneFromPoints( sideplane, worldverts[i], worldverts[ next ], worldverts[ next ] + hostplane.normal() );

				//planetest sample point
				const float e = plane3_distance_to_point( sideplane, origin );
				if ( e > -LUXEL_EPSILON ) {
					//we're bad.
					//Move the sample point back inside triangle bounds
					origin -= sideplane.normal() * ( e + 1 );
#ifdef DEBUG_27_1
					origin.set( 0 );
#endif
				}
			}
		}
	}

	////////////////////////

	/* planar surfaces have precalculated lightmap vectors for nudging */
	if ( lm->plane != NULL ) {
		vecs[ 0 ] = lm->vecs[ 0 ];
		vecs[ 1 ] = lm->vecs[ 1 ];
		vecs[ 2 ] = lm->plane->normal();
	}

	/* non-planar surfaces must calculate them */
	else
	{
		if ( plane != NULL ) {
			vecs[ 2 ] = plane->normal();
		}
		else{
			vecs[ 2 ] = dv.normal;
		}
		MakeNormalVectors( vecs[ 2 ], vecs[ 0 ], vecs[ 1 ] );
	}

	/* push the origin off the surface a bit */
	if ( si != NULL ) {
		lightmapSampleOffset = si->lightmapSampleOffset;
	}
	else{
		lightmapSampleOffset = DEFAULT_LIGHTMAP_SAMPLE_OFFSET;
	}
	if ( lm->axisNum < 0 ) {
		origin += vecs[ 2 ] * lightmapSampleOffset;
	}
	else if ( vecs[ 2 ][ lm->axisNum ] < 0.0f ) {
		origin[ lm->axisNum ] -= lightmapSampleOffset;
	}
	else{
		origin[ lm->axisNum ] += lightmapSampleOffset;
	}

	origintwo = origin;
	if ( lightmapExtraVisClusterNudge ) {
		origintwo += vecs[2];
	}

	/* get cluster */
	pointCluster = ClusterForPointExtFilter( origintwo, LUXEL_EPSILON, numClusters, clusters );

	/* another retarded hack, storing nudge count in luxel[ 1 ] */
	luxel.value[ 1 ] = 0.0f;

	/* point in solid? (except in dark mode) */
	if ( pointCluster < 0 && !dark ) {
		/* nudge the the location around */
		nudge = nudges[ 0 ];
		while ( nudge[ 0 ] > BOGUS_NUDGE && pointCluster < 0 )
		{
			/* nudge the vector around a bit */
			/* set nudged point*/
			nudged = origintwo + vecs[ 0 ] * nudge[ 0 ] + vecs[ 1 ] * nudge[ 1 ];
			nudge += 2;

			/* get pvs cluster */
			pointCluster = ClusterForPointExtFilter( nudged, LUXEL_EPSILON, numClusters, clusters ); //% + 0.625 );
			if ( pointCluster >= 0 ) {
				origin = nudged;
			}
			luxel.value[ 1 ] += 1.0f;
		}
	}

	/* as a last resort, if still in solid, try drawvert origin offset by normal (except in dark mode) */
	if ( pointCluster < 0 && si != NULL && !dark ) {
		nudged = dv.xyz + dv.normal * lightmapSampleOffset;
		pointCluster = ClusterForPointExtFilter( nudged, LUXEL_EPSILON, numClusters, clusters );
		if ( pointCluster >= 0 ) {
			origin = nudged;
		}
		luxel.value[ 1 ] += 1.0f;
	}

	/* valid? */
	if ( pointCluster < 0 ) {
		cluster = CLUSTER_OCCLUDED;
		origin.set( 0 );
		normal.set( 0 );
		numLuxelsOccluded++;
		return cluster;
	}

	/* debug code */
	//%	Sys_Printf( "%f %f %f\n", origin[ 0 ], origin[ 1 ], origin[ 2 ] );

	/* do bumpmap calculations */
	if ( stv ) {
		PerturbNormal( dv, si, pNormal, stv, ttv );
	}
	else{
		pNormal = dv.normal;
	}

	/* store the cluster and normal */
	cluster = pointCluster;
	normal = pNormal;

	/* store explicit mapping pass and implicit mapping pass */
	luxel.value[ 0 ] = pass;
	luxel.count = 1.0f;

	/* add to count */
	numLuxelsMapped++;

	/* return ok */
	return cluster;
}



/*
   MapTriangle_r()
   recursively subdivides a triangle until its edges are shorter
   than the distance between two luxels (thanks jc :)
 */

static void MapTriangle_r( rawLightmap_t *lm, const surfaceInfo_t *info, const TriRef& tri, Plane3f *plane, const Vector3 stv[ 3 ], const Vector3 ttv[ 3 ], const Vector3 worldverts[ 3 ] ){
	/* map the vertexes */
	#if 0
	MapSingleLuxel( lm, info, *tri[ 0 ], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, *tri[ 1 ], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, *tri[ 2 ], plane, 1, stv, ttv );
	#endif

	/* subdivide calc */
	int max = -1;
	{
		/* find the longest edge and split it */
		float maxDist = 0;
		for ( int i = 0; i < 3; i++ )
		{
			/* get dist */
			const float dist = vector2_length_squared( tri[ i ]->lightmap[ 0 ] - tri[ ( i + 1 ) % 3 ]->lightmap[ 0 ] );
			/* longer? */
			if ( dist > maxDist ) {
				maxDist = dist;
				max = i;
			}
		}

		/* try to early out */
		if ( max < 0 || maxDist <= subdivideThreshold ) { /* ydnar: was i < 0 instead of max < 0 (?) */
			return;
		}
	}

	/* split the longest edge and map it */
	const bspDrawVert_t mid = LerpDrawVert( *tri[ max ], *tri[ ( max + 1 ) % 3 ] );
	MapSingleLuxel( lm, info, mid, plane, 1, stv, ttv, worldverts );

	/* push the point up a little bit to account for fp creep (fixme: revisit this) */
	//%	VectorMA( mid.xyz, 2.0f, mid.normal, mid.xyz );

	/* recurse to first triangle */
	TriRef tri2 = tri;
	tri2[ max ] = &mid;
	MapTriangle_r( lm, info, tri2, plane, stv, ttv, worldverts );

	/* recurse to second triangle */
	tri2 = tri;
	tri2[ ( max + 1 ) % 3 ] = &mid;
	MapTriangle_r( lm, info, tri2, plane, stv, ttv, worldverts );
}



/*
   MapTriangle()
   seed function for MapTriangle_r()
   requires a cw ordered triangle
 */

static bool MapTriangle( rawLightmap_t *lm, const surfaceInfo_t *info, const TriRef& tri, bool mapNonAxial ){
	Plane3f plane;
	/* get plane if possible */
	if ( lm->plane != NULL ) {
		plane = *lm->plane;
	}
	/* otherwise make one from the points */
	else if ( !PlaneFromPoints( plane, tri[ 0 ]->xyz, tri[ 1 ]->xyz, tri[ 2 ]->xyz ) ) {
		return false;
	}

	/* this must not happen in the first place, but it does and spreads result of division by zero in MapSingleLuxel all over the map during -bounce */
	if( lm->vecs != NULL && plane.normal()[lm->axisNum] == 0 ){
		Sys_Warning( "plane[lm->axisNum] == 0\n" );
		return false;
	}

	Vector3          *stv, *ttv, stvStatic[ 3 ], ttvStatic[ 3 ];
	/* check to see if we need to calculate texture->world tangent vectors */
	if ( info->si->normalImage != NULL && CalcTangentVectors( tri, stvStatic, ttvStatic ) ) {
		stv = stvStatic;
		ttv = ttvStatic;
	}
	else
	{
		stv = NULL;
		ttv = NULL;
	}

	const Vector3 worldverts[ 3 ] = { tri[ 0 ]->xyz, tri[ 1 ]->xyz, tri[ 2 ]->xyz };

	/* map the vertexes */
	MapSingleLuxel( lm, info, *tri[ 0 ], &plane, 1, stv, ttv, worldverts );
	MapSingleLuxel( lm, info, *tri[ 1 ], &plane, 1, stv, ttv, worldverts );
	MapSingleLuxel( lm, info, *tri[ 2 ], &plane, 1, stv, ttv, worldverts );

	/* 2002-11-20: prefer axial triangle edges */
	if ( mapNonAxial ) {
		/* subdivide the triangle */
		MapTriangle_r( lm, info, tri, &plane, stv, ttv, worldverts );
		return true;
	}

	for ( int i = 0; i < 3; i++ )
	{
		/* get verts */
		const Vector2& a = tri[ i ]->lightmap[ 0 ];
		const Vector2& b = tri[ ( i + 1 ) % 3 ]->lightmap[ 0 ];

		/* make degenerate triangles for mapping edges */
		if ( float_equal_epsilon( a[ 0 ], b[ 0 ], 0.01f ) || float_equal_epsilon( a[ 1 ], b[ 1 ], 0.01f ) )
			/* map the degenerate triangle */
			MapTriangle_r( lm, info, TriRef{
				tri[ i ],
				tri[ ( i + 1 ) % 3 ],
				tri[ ( i + 1 ) % 3 ] }, &plane, stv, ttv, worldverts );
	}

	return true;
}



/*
   MapQuad_r()
   recursively subdivides a quad until its edges are shorter
   than the distance between two luxels
 */

static void MapQuad_r( rawLightmap_t *lm, const surfaceInfo_t *info, const QuadRef& quad, Plane3f *plane, const Vector3 stv[ 4 ], const Vector3 ttv[ 4 ] ){
	/* subdivide calc */
	int max = -1;
	{
		/* find the longest edge and split it */
		float maxDist = 0;
		for ( int i = 0; i < 4; i++ )
		{
			/* get dist */
			const float dist = vector2_length_squared( quad[ i ]->lightmap[ 0 ] - quad[ ( i + 1 ) % 4 ]->lightmap[ 0 ] );
			/* longer? */
			if ( dist > maxDist ) {
				maxDist = dist;
				max = i;
			}
		}

		/* try to early out */
		if ( max < 0 || maxDist <= subdivideThreshold ) {
			return;
		}
	}

	/* we only care about even/odd edges */
	max &= 1;

	/* split the longest edges */
	const bspDrawVert_t mid[ 2 ] = {
		LerpDrawVert( *quad[ max + 0 ], *quad[ ( max + 1 ) % 4 ] ),
		LerpDrawVert( *quad[ max + 2 ], *quad[ ( max + 3 ) % 4 ] ) };

	/* map the vertexes */
	MapSingleLuxel( lm, info, mid[ 0 ], plane, 1, stv, ttv, NULL );
	MapSingleLuxel( lm, info, mid[ 1 ], plane, 1, stv, ttv, NULL );

	/* 0 and 2 */
	if ( max == 0 ) {
		/* recurse to first quad */
		MapQuad_r( lm, info, QuadRef{ quad[ 0 ], &mid[ 0 ], &mid[ 1 ], quad[ 3 ] }, plane, stv, ttv );

		/* recurse to second quad */
		MapQuad_r( lm, info, QuadRef{ &mid[ 0 ], quad[ 1 ], quad[ 2 ], &mid[ 1 ] }, plane, stv, ttv );
	}

	/* 1 and 3 */
	else
	{
		/* recurse to first quad */
		MapQuad_r( lm, info, QuadRef{ quad[ 0 ], quad[ 1 ], &mid[ 0 ], &mid[ 1 ] }, plane, stv, ttv );

		/* recurse to second quad */
		MapQuad_r( lm, info, QuadRef{ &mid[ 1 ], &mid[ 0 ], quad[ 2 ], quad[ 3 ] }, plane, stv, ttv );
	}
}



/*
   MapQuad()
   seed function for MapQuad_r()
   requires a cw ordered triangle quad
 */

#define QUAD_PLANAR_EPSILON     0.5f

static bool MapQuad( rawLightmap_t *lm, const surfaceInfo_t *info, const QuadRef& quad ){
	Plane3f plane;
	/* get plane if possible */
	if ( lm->plane != NULL ) {
		plane = *lm->plane;
	}
	/* otherwise make one from the points */
	else if ( !PlaneFromPoints( plane, quad[ 0 ]->xyz, quad[ 1 ]->xyz, quad[ 2 ]->xyz ) ) {
		return false;
	}

	/* 4th point must fall on the plane */
	if ( fabs( plane3_distance_to_point( plane, quad[ 3 ]->xyz ) ) > QUAD_PLANAR_EPSILON ) {
		return false;
	}

	Vector3          *stv, *ttv, stvStatic[ 4 ], ttvStatic[ 4 ];
	/* check to see if we need to calculate texture->world tangent vectors */
	if ( info->si->normalImage != NULL && CalcTangentVectors( quad, stvStatic, ttvStatic ) ) {
		stv = stvStatic;
		ttv = ttvStatic;
	}
	else
	{
		stv = NULL;
		ttv = NULL;
	}

	/* map the vertexes */
	MapSingleLuxel( lm, info, *quad[ 0 ], &plane, 1, stv, ttv, NULL );
	MapSingleLuxel( lm, info, *quad[ 1 ], &plane, 1, stv, ttv, NULL );
	MapSingleLuxel( lm, info, *quad[ 2 ], &plane, 1, stv, ttv, NULL );
	MapSingleLuxel( lm, info, *quad[ 3 ], &plane, 1, stv, ttv, NULL );

	/* subdivide the quad */
	MapQuad_r( lm, info, quad, &plane, stv, ttv );
	return true;
}



/*
   MapRawLightmap()
   maps the locations, normals, and pvs clusters for a raw lightmap
 */

void MapRawLightmap( int rawLightmapNum ){
	int n, i, x, y, sx, sy, mapNonAxial;
	float               samples, radius, pass;
	rawLightmap_t       *lm;
	bspDrawVert_t fake;


	/* bail if this number exceeds the number of raw lightmaps */
	if ( rawLightmapNum >= numRawLightmaps ) {
		return;
	}

	/* get lightmap */
	lm = &rawLightmaps[ rawLightmapNum ];

	/* -----------------------------------------------------------------
	   map referenced surfaces onto the raw lightmap
	   ----------------------------------------------------------------- */

	/* walk the list of surfaces on this raw lightmap */
	for ( n = 0; n < lm->numLightSurfaces; n++ )
	{
		/* with > 1 surface per raw lightmap, clear occluded */
		if ( n > 0 ) {
			for ( y = 0; y < lm->sh; y++ )
			{
				for ( x = 0; x < lm->sw; x++ )
				{
					/* get cluster */
					int& cluster = lm->getSuperCluster( x, y );
					if ( cluster < 0 ) {
						cluster = CLUSTER_UNMAPPED;
					}
				}
			}
		}

		/* get surface */
		const int num = lightSurfaces[ lm->firstLightSurface + n ];
		const bspDrawSurface_t& ds = bspDrawSurfaces[ num ];
		const surfaceInfo_t *info = &surfaceInfos[ num ];

		/* bail if no lightmap to calculate */
		if ( info->lm != lm ) {
			Sys_Printf( "!" );
			continue;
		}

		/* map the surface onto the lightmap origin/cluster/normal buffers */
		switch ( ds.surfaceType )
		{
		case MST_PLANAR:
		{
			/* get verts */
			const bspDrawVert_t *verts = &yDrawVerts[ ds.firstVert ];

			/* map the triangles */
			for ( mapNonAxial = 0; mapNonAxial < 2; mapNonAxial++ )
				for ( i = 0; i < ds.numIndexes; i += 3 )
					MapTriangle( lm, info, TriRef{
						&verts[ bspDrawIndexes[ ds.firstIndex + i ] ],
						&verts[ bspDrawIndexes[ ds.firstIndex + i + 1 ] ],
						&verts[ bspDrawIndexes[ ds.firstIndex + i + 2 ] ] }, mapNonAxial );
			break;
		}
		case MST_PATCH:
		{
			/* make a mesh from the drawsurf */
			mesh_t src;
			src.width = ds.patchWidth;
			src.height = ds.patchHeight;
			src.verts = &yDrawVerts[ ds.firstVert ];
			//%	mesh_t *subdivided = SubdivideMesh( src, 8, 512 );
			mesh_t *subdivided = SubdivideMesh2( src, info->patchIterations );

			/* fit it to the curve and remove colinear verts on rows/columns */
			PutMeshOnCurve( *subdivided );
			mesh_t *mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );

			/* get verts */
			const bspDrawVert_t *verts = mesh->verts;

			/* debug code */
#if 0
			if ( lm->plane ) {
				Sys_Printf( "Planar patch: [%1.3f %1.3f %1.3f] [%1.3f %1.3f %1.3f] [%1.3f %1.3f %1.3f]\n",
				            lm->plane[ 0 ], lm->plane[ 1 ], lm->plane[ 2 ],
				            lm->vecs[ 0 ][ 0 ], lm->vecs[ 0 ][ 1 ], lm->vecs[ 0 ][ 2 ],
				            lm->vecs[ 1 ][ 0 ], lm->vecs[ 1 ][ 1 ], lm->vecs[ 1 ][ 2 ] );
			}
#endif

			/* map the mesh quads */
#if 0

			for ( mapNonAxial = 0; mapNonAxial < 2; mapNonAxial++ )
			{
				for ( y = 0; y < ( mesh->height - 1 ); y++ )
				{
					for ( x = 0; x < ( mesh->width - 1 ); x++ )
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
						MapTriangle( lm, info, TriRef{
							&verts[ pw[ r + 0 ] ],
							&verts[ pw[ r + 1 ] ],
							&verts[ pw[ r + 2 ] ] }, mapNonAxial );

						/* get drawverts and map second triangle */
						MapTriangle( lm, info, TriRef{
							&verts[ pw[ r + 0 ] ],
							&verts[ pw[ r + 2 ] ],
							&verts[ pw[ r + 3 ] ] }, mapNonAxial );
					}
				}
			}

#else

			for ( y = 0; y < ( mesh->height - 1 ); y++ )
			{
				for ( x = 0; x < ( mesh->width - 1 ); x++ )
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

					/* attempt to map quad first */
					if ( MapQuad( lm, info, QuadRef{
						&verts[ pw[ r + 0 ] ],
						&verts[ pw[ r + 1 ] ],
						&verts[ pw[ r + 2 ] ],
						&verts[ pw[ r + 3 ] ] } ) ) {
						continue;
					}

					for ( mapNonAxial = 0; mapNonAxial < 2; mapNonAxial++ )
					{
						/* get drawverts and map first triangle */
						MapTriangle( lm, info, TriRef{
							&verts[ pw[ r + 0 ] ],
							&verts[ pw[ r + 1 ] ],
							&verts[ pw[ r + 2 ] ] }, mapNonAxial );

						/* get drawverts and map second triangle */
						MapTriangle( lm, info, TriRef{
							&verts[ pw[ r + 0 ] ],
							&verts[ pw[ r + 2 ] ],
							&verts[ pw[ r + 3 ] ] }, mapNonAxial );
					}
				}
			}

#endif

			/* free the mesh */
			FreeMesh( mesh );
			break;
		}
		default:
			break;
		}
	}

	/* -----------------------------------------------------------------
	   average and clean up luxel normals
	   ----------------------------------------------------------------- */

	/* walk the luxels */
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			/* get luxel */
			SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );

			/* only look at mapped luxels */
			if ( lm->getSuperCluster( x, y ) < 0 ) {
				continue;
			}

			/* the normal data could be the sum of multiple samples */
			if ( luxel.count > 1.0f ) {
				VectorNormalize( lm->getSuperNormal( x, y ) );
			}

			/* mark this luxel as having only one normal */
			luxel.count = 1.0f;
		}
	}

	/* non-planar surfaces stop here */
	if ( lm->plane == NULL ) {
		return;
	}

	/* -----------------------------------------------------------------
	   map occluded or unuxed luxels
	   ----------------------------------------------------------------- */

	/* walk the luxels */
	radius = std::max( 1, superSample / 2 ) + 1;
	for ( pass = 2.0f; pass <= radius; pass += 1.0f )
	{
		for ( y = 0; y < lm->sh; y++ )
		{
			for ( x = 0; x < lm->sw; x++ )
			{
				/* only look at unmapped luxels */
				if ( lm->getSuperCluster( x, y ) != CLUSTER_UNMAPPED ) {
					continue;
				}

				/* divine a normal and origin from neighboring luxels */
				fake.xyz.set( 0 );
				fake.normal.set( 0 );
				fake.lightmap[ 0 ] = Vector2( x, y );    //% 0.0001 + x; //% 0.0001 + y;
				samples = 0.0f;
				for ( sy = ( y - 1 ); sy <= ( y + 1 ); sy++ )
				{
					if ( sy < 0 || sy >= lm->sh ) {
						continue;
					}

					for ( sx = ( x - 1 ); sx <= ( x + 1 ); sx++ )
					{
						if ( sx < 0 || sx >= lm->sw || ( sx == x && sy == y ) ) {
							continue;
						}

						/* get neighboring luxel */
						const SuperLuxel& luxel = lm->getSuperLuxel( 0, sx, sy );

						/* only consider luxels mapped in previous passes */
						if ( lm->getSuperCluster( sx, sy ) < 0 || luxel.value[ 0 ] >= pass ) {
							continue;
						}

						/* add its distinctiveness to our own */
						fake.xyz += lm->getSuperOrigin( sx, sy );
						fake.normal += lm->getSuperNormal( sx, sy );
						samples += luxel.count;
					}
				}

				/* any samples? */
				if ( samples == 0.0f ) {
					continue;
				}

				/* average */
				fake.xyz *= ( 1.f / samples );
				//%	fake.normal *= ( 1.f / samples );
				if ( VectorNormalize( fake.normal ) == 0.0f ) {
					continue;
				}

				/* map the fake vert */
				MapSingleLuxel( lm, NULL, fake, lm->plane, pass, NULL, NULL, NULL );
			}
		}
	}

	/* -----------------------------------------------------------------
	   average and clean up luxel normals
	   ----------------------------------------------------------------- */

	/* walk the luxels */
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			/* get luxel */
			SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );

			/* only look at mapped luxels */
			if ( lm->getSuperCluster( x, y ) < 0 ) {
				continue;
			}

			/* the normal data could be the sum of multiple samples */
			if ( luxel.count > 1.0f ) {
				VectorNormalize( lm->getSuperNormal( x, y ) );
			}

			/* mark this luxel as having only one normal */
			luxel.count = 1.0f;
		}
	}

	/* debug code */
	#if 0
	Sys_Printf( "\n" );
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			const int cluster = lm->getSuperCluster( x, y );
			const Vector3& origin = lm->getSuperOrigin( x, y );
			const SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );

			if ( cluster < 0 ) {
				continue;
			}

			/* check if within the bounding boxes of all surfaces referenced */
			MinMax minmax;
			for ( n = 0; n < lm->numLightSurfaces; n++ )
			{
				info = &surfaceInfos[ lightSurfaces[ lm->firstLightSurface + n ] ];
				minmax.extend( info->minmax );
				if( info->minmax.test( origin, info->sampleSize + 2 ) ){
					break;
				}
			}

			/* inside? */
			if ( n < lm->numLightSurfaces ) {
				continue;
			}

			/* report bogus origin */
			Sys_Printf( "%6d [%2d,%2d] (%4d): XYZ(%+4.1f %+4.1f %+4.1f) LO(%+4.1f %+4.1f %+4.1f) HI(%+4.1f %+4.1f %+4.1f) <%3.0f>\n",
			            rawLightmapNum, x, y, cluster,
			            origin[ 0 ], origin[ 1 ], origin[ 2 ],
			            minmax.mins[ 0 ], minmax.mins[ 1 ], minmax.mins[ 2 ],
			            minmax.maxs[ 0 ], minmax.maxs[ 1 ], minmax.maxs[ 2 ],
			            luxel.count );
		}
	}
	#endif
}



/*
   SetupDirt()
   sets up dirtmap (ambient occlusion)
 */

#define DIRT_CONE_ANGLE             88  /* degrees */
#define DIRT_NUM_ANGLE_STEPS        16
#define DIRT_NUM_ELEVATION_STEPS    3
#define DIRT_NUM_VECTORS            ( DIRT_NUM_ANGLE_STEPS * DIRT_NUM_ELEVATION_STEPS )

static Vector3 dirtVectors[ DIRT_NUM_VECTORS ];
static int numDirtVectors = 0;

void SetupDirt(){
	int i, j;
	float angle, elevation, angleStep, elevationStep;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SetupDirt ---\n" );

	/* calculate angular steps */
	angleStep = degrees_to_radians( 360.0f / DIRT_NUM_ANGLE_STEPS );
	elevationStep = degrees_to_radians( DIRT_CONE_ANGLE / DIRT_NUM_ELEVATION_STEPS );

	/* iterate angle */
	angle = 0.0f;
	for ( i = 0, angle = 0.0f; i < DIRT_NUM_ANGLE_STEPS; i++, angle += angleStep )
	{
		/* iterate elevation */
		for ( j = 0, elevation = elevationStep * 0.5f; j < DIRT_NUM_ELEVATION_STEPS; j++, elevation += elevationStep )
		{
			dirtVectors[ numDirtVectors ][ 0 ] = sin( elevation ) * cos( angle );
			dirtVectors[ numDirtVectors ][ 1 ] = sin( elevation ) * sin( angle );
			dirtVectors[ numDirtVectors ][ 2 ] = cos( elevation );
			numDirtVectors++;
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d dirtmap vectors\n", numDirtVectors );
}


/*
   DirtForSample()
   calculates dirt value for a given sample
 */

static float DirtForSample( trace_t *trace ){
	int i;
	float gatherDirt, outDirt, angle, elevation, ooDepth;
	Vector3 myUp, myRt;


	/* dummy check */
	if ( !dirty ) {
		return 1.0f;
	}
	if ( trace == NULL || trace->cluster < 0 ) {
		return 0.0f;
	}

	/* setup */
	gatherDirt = 0.0f;
	ooDepth = 1.0f / dirtDepth;
	const Vector3 normal( trace->normal );

	/* check if the normal is aligned to the world-up */
	if ( normal[ 0 ] == 0.0f && normal[ 1 ] == 0.0f && ( normal[ 2 ] == 1.0f || normal[ 2 ] == -1.0f ) ) {
		if ( normal[ 2 ] == 1.0f ) {
			myRt = g_vector3_axis_x;
			myUp = g_vector3_axis_y;
		}
		else if ( normal[ 2 ] == -1.0f ) {
			myRt = -g_vector3_axis_x;
			myUp = g_vector3_axis_y;
		}
	}
	else
	{
		myRt = VectorNormalized( vector3_cross( normal, g_vector3_axis_z ) );
		myUp = VectorNormalized( vector3_cross( myRt, normal ) );
	}

	/* 1 = random mode, 0 (well everything else) = non-random mode */
	if ( dirtMode == 1 ) {
		/* iterate */
		for ( i = 0; i < numDirtVectors; i++ )
		{
			/* get random vector */
			angle = Random() * degrees_to_radians( 360.0f );
			elevation = Random() * degrees_to_radians( DIRT_CONE_ANGLE );
			const Vector3 temp( cos( angle ) * sin( elevation ),
			                    sin( angle ) * sin( elevation ),
			                    cos( elevation ) );

			/* transform into tangent space */
			const Vector3 direction = myRt * temp[ 0 ] + myUp * temp[ 1 ] + normal * temp[ 2 ];

			/* set endpoint */
			trace->end = trace->origin + direction * dirtDepth;
			SetupTrace( trace );
			trace->color.set( 1 );

			/* trace */
			TraceLine( trace );
			if ( trace->opaque && !( trace->compileFlags & C_SKY ) ) {
				gatherDirt += 1.0f - ooDepth * vector3_length( trace->hit - trace->origin );
			}
		}
	}
	else
	{
		/* iterate through ordered vectors */
		for ( i = 0; i < numDirtVectors; i++ )
		{
			/* transform vector into tangent space */
			const Vector3 direction = myRt * dirtVectors[ i ][ 0 ] + myUp * dirtVectors[ i ][ 1 ] + normal * dirtVectors[ i ][ 2 ];

			/* set endpoint */
			trace->end = trace->origin + direction * dirtDepth;
			SetupTrace( trace );
			trace->color.set( 1 );

			/* trace */
			TraceLine( trace );
			if ( trace->opaque ) {
				gatherDirt += 1.0f - ooDepth * vector3_length( trace->hit - trace->origin );
			}
		}
	}

	/* direct ray */
	trace->end = trace->origin + normal * dirtDepth;
	SetupTrace( trace );
	trace->color.set( 1 );

	/* trace */
	TraceLine( trace );
	if ( trace->opaque ) {
		gatherDirt += 1.0f - ooDepth * vector3_length( trace->hit - trace->origin );
	}

	/* early out */
	if ( gatherDirt <= 0.0f ) {
		return 1.0f;
	}

	/* apply gain (does this even do much? heh) */
	outDirt = std::min( 1.f, std::pow( gatherDirt / ( numDirtVectors + 1 ), dirtGain ) );

	/* apply scale */
	outDirt *= dirtScale;
	value_minimize( outDirt, 1.0f );

	/* return to sender */
	return 1.0f - outDirt;
}



/*
   DirtyRawLightmap()
   calculates dirty fraction for each luxel
 */

void DirtyRawLightmap( int rawLightmapNum ){
	int i, x, y, sx, sy;
	float               average, samples;
	rawLightmap_t       *lm;
	surfaceInfo_t       *info;
	trace_t trace;
	bool noDirty;


	/* bail if this number exceeds the number of raw lightmaps */
	if ( rawLightmapNum >= numRawLightmaps ) {
		return;
	}

	/* get lightmap */
	lm = &rawLightmaps[ rawLightmapNum ];

	/* setup trace */
	trace.testOcclusion = true;
	trace.forceSunlight = false;
	trace.recvShadows = lm->recvShadows;
	trace.numSurfaces = lm->numLightSurfaces;
	trace.surfaces = &lightSurfaces[ lm->firstLightSurface ];
	trace.inhibitRadius = 0.0f;
	trace.testAll = false;

	/* twosided lighting (may or may not be a good idea for lightmapped stuff) */
	trace.twoSided = false;
	for ( i = 0; i < trace.numSurfaces; i++ )
	{
		/* get surface */
		info = &surfaceInfos[ trace.surfaces[ i ] ];

		/* check twosidedness */
		if ( info->si->twoSided ) {
			trace.twoSided = true;
			break;
		}
	}

	noDirty = false;
	for ( i = 0; i < trace.numSurfaces; i++ )
	{
		/* get surface */
		info = &surfaceInfos[ trace.surfaces[ i ] ];

		/* check twosidedness */
		if ( info->si->noDirty ) {
			noDirty = true;
			break;
		}
	}

	/* gather dirt */
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			/* get luxel */
			const int cluster = lm->getSuperCluster( x, y );
			float& dirt = lm->getSuperDirt( x, y );

			/* set default dirt */
			dirt = 0.0f;

			/* only look at mapped luxels */
			if ( cluster < 0 ) {
				continue;
			}

			/* don't apply dirty on this surface */
			if ( noDirty ) {
				dirt = 1.0f;
				continue;
			}

			/* copy to trace */
			trace.cluster = cluster;
			trace.origin = lm->getSuperOrigin( x, y );
			trace.normal = lm->getSuperNormal( x, y );

			/* get dirt */
			dirt = DirtForSample( &trace );
		}
	}

	/* testing no filtering */
	//%	return;

	/* filter dirt */
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			/* get luxel */
			float& dirt = lm->getSuperDirt( x, y );

			/* filter dirt by adjacency to unmapped luxels */
			average = dirt;
			samples = 1.0f;
			for ( sy = ( y - 1 ); sy <= ( y + 1 ); sy++ )
			{
				if ( sy < 0 || sy >= lm->sh ) {
					continue;
				}

				for ( sx = ( x - 1 ); sx <= ( x + 1 ); sx++ )
				{
					if ( sx < 0 || sx >= lm->sw || ( sx == x && sy == y ) ) {
						continue;
					}

					/* get neighboring luxel */
					const float dirt2 = lm->getSuperDirt( sx, sy );
					if ( lm->getSuperCluster( sx, sy ) < 0 || dirt2 <= 0.0f ) {
						continue;
					}

					/* add it */
					average += dirt2;
					samples += 1.0f;
				}

				/* bail */
				if ( samples <= 0.0f ) {
					break;
				}
			}

			/* bail */
			if ( samples <= 0.0f ) {
				continue;
			}

			/* scale dirt */
			dirt = average / samples;
		}
	}
}



/*
   SubmapRawLuxel()
   calculates the pvs cluster, origin, normal of a sub-luxel
 */

static bool SubmapRawLuxel( const rawLightmap_t *lm, int x, int y, float bx, float by, int& sampleCluster, Vector3& sampleOrigin, Vector3& sampleNormal ){
	const Vector3       *origin, *origin2;
	Vector3 originVecs[ 2 ];


	/* calculate x vector */
	if ( ( x < ( lm->sw - 1 ) && bx >= 0.0f ) || ( x == 0 && bx <= 0.0f ) ) {
		origin = &lm->getSuperOrigin( x, y );
		//%	normal = SUPER_NORMAL( x, y );
		origin2 = lm->getSuperCluster( x + 1, y ) < 0 ? &lm->getSuperOrigin( x, y ) : &lm->getSuperOrigin( x + 1, y );
		//%	normal2 = *cluster2 < 0 ? SUPER_NORMAL( x, y ) : SUPER_NORMAL( x + 1, y );
	}
	else if ( ( x > 0 && bx <= 0.0f ) || ( x == ( lm->sw - 1 ) && bx >= 0.0f ) ) {
		origin = lm->getSuperCluster( x - 1, y ) < 0 ? &lm->getSuperOrigin( x, y ) : &lm->getSuperOrigin( x - 1, y );
		//%	normal = *cluster < 0 ? SUPER_NORMAL( x, y ) : SUPER_NORMAL( x - 1, y );
		origin2 = &lm->getSuperOrigin( x, y );
		//%	normal2 = SUPER_NORMAL( x, y );
	}
	else
	{
		Error( "Spurious lightmap S vector\n" );
	}

	originVecs[ 0 ] = *origin2 - *origin;
	//%	VectorSubtract( normal2, normal, normalVecs[ 0 ] );

	/* calculate y vector */
	if ( ( y < ( lm->sh - 1 ) && bx >= 0.0f ) || ( y == 0 && bx <= 0.0f ) ) {
		origin = &lm->getSuperOrigin( x, y );
		//%	normal = SUPER_NORMAL( x, y );
		origin2 = lm->getSuperCluster( x, y + 1 ) < 0 ? &lm->getSuperOrigin( x, y ) : &lm->getSuperOrigin( x, y + 1 );
		//%	normal2 = *cluster2 < 0 ? SUPER_NORMAL( x, y ) : SUPER_NORMAL( x, y + 1 );
	}
	else if ( ( y > 0 && bx <= 0.0f ) || ( y == ( lm->sh - 1 ) && bx >= 0.0f ) ) {
		origin = lm->getSuperCluster( x, y - 1 ) < 0 ? &lm->getSuperOrigin( x, y ) : &lm->getSuperOrigin( x, y - 1 );
		//%	normal = *cluster < 0 ? SUPER_NORMAL( x, y ) : SUPER_NORMAL( x, y - 1 );
		origin2 = &lm->getSuperOrigin( x, y );
		//%	normal2 = SUPER_NORMAL( x, y );
	}
	else{
		Sys_Warning( "Spurious lightmap T vector\n" );
	}

	originVecs[ 1 ] = *origin2 - *origin;
	//%	VectorSubtract( normal2, normal, normalVecs[ 1 ] );

	/* calculate new origin */
	//%	VectorMA( origin, bx, originVecs[ 0 ], sampleOrigin );
	//%	VectorMA( sampleOrigin, by, originVecs[ 1 ], sampleOrigin );
	sampleOrigin += ( originVecs[ 0 ] * bx ) + ( originVecs[ 1 ] * by );

	/* get cluster */
	sampleCluster = ClusterForPointExtFilter( sampleOrigin, ( LUXEL_EPSILON * 2 ), lm->numLightClusters, lm->lightClusters );
	if ( sampleCluster < 0 ) {
		return false;
	}

	/* calculate new normal */
	//%	VectorMA( normal, bx, normalVecs[ 0 ], sampleNormal );
	//%	VectorMA( sampleNormal, by, normalVecs[ 1 ], sampleNormal );
	//%	if( VectorNormalize( sampleNormal, sampleNormal ) <= 0.0f )
	//%		return false;
	sampleNormal = lm->getSuperNormal( x, y );

	/* return ok */
	return true;
}


/*
   SubsampleRawLuxel_r()
   recursively subsamples a luxel until its color gradient is low enough or subsampling limit is reached
 */

static void SubsampleRawLuxel_r( rawLightmap_t *lm, trace_t *trace, const Vector3& sampleOrigin, int x, int y, float bias, SuperLuxel& lightLuxel, Vector3 *lightDeluxel ){
	int b, samples, mapped, lighted;
	int cluster[ 4 ];
	SuperLuxel luxel[ 4 ];
	Vector3 deluxel[ 4 ];
	Vector3 origin[ 4 ], normal[ 4 ];
	float biasDirs[ 4 ][ 2 ] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
	Vector3 color, direction( 0 ), total( 0 );


	/* limit check */
	if ( lightLuxel.count >= lightSamples ) {
		return;
	}

	/* setup */
	mapped = 0;
	lighted = 0;

	/* make 2x2 subsample stamp */
	for ( b = 0; b < 4; b++ )
	{
		/* set origin */
		origin[ b ] = sampleOrigin;

		/* calculate position */
		if ( !SubmapRawLuxel( lm, x, y, ( bias * biasDirs[ b ][ 0 ] ), ( bias * biasDirs[ b ][ 1 ] ), cluster[ b ], origin[ b ], normal[ b ] ) ) {
			cluster[ b ] = -1;
			continue;
		}
		mapped++;

		/* increment sample count */
		luxel[ b ].count = lightLuxel.count + 1.0f;

		/* setup trace */
		trace->cluster = *cluster;
		trace->origin = origin[ b ];
		trace->normal = normal[ b ];

		/* sample light */

		LightContributionToSample( trace );
		if ( trace->forceSubsampling > 1.0f ) {
			/* alphashadow: we subsample as deep as we can */
			++lighted;
			++mapped;
			++mapped;
		}

		/* add to totals (fixme: make contrast function) */
		luxel[ b ].value = trace->color;
		if ( lightDeluxel ) {
			deluxel[ b ] = trace->directionContribution;
		}
		total += trace->color;
		if ( ( luxel[ b ].value[ 0 ] + luxel[ b ].value[ 1 ] + luxel[ b ].value[ 2 ] ) > 0.0f ) {
			lighted++;
		}
	}

	/* subsample further? */
	if ( ( lightLuxel.count + 1.0f ) < lightSamples &&
	     ( total[ 0 ] > 4.0f || total[ 1 ] > 4.0f || total[ 2 ] > 4.0f ) &&
	     lighted != 0 && lighted != mapped ) {
		for ( b = 0; b < 4; b++ )
		{
			if ( cluster[ b ] < 0 ) {
				continue;
			}
			SubsampleRawLuxel_r( lm, trace, origin[ b ], x, y, ( bias * 0.5f ), luxel[ b ], lightDeluxel ? &deluxel[ b ] : NULL );
		}
	}

	/* average */
	//%	color.set( 0 );
	//%	samples = 0;
	color = lightLuxel.value;
	if ( lightDeluxel ) {
		direction = *lightDeluxel;
	}
	samples = 1;
	for ( b = 0; b < 4; b++ )
	{
		if ( cluster[ b ] < 0 ) {
			continue;
		}
		color += luxel[ b ].value;
		if ( lightDeluxel ) {
			direction += deluxel[ b ];
		}
		samples++;
	}

	/* add to luxel */
	if ( samples > 0 ) {
		/* average */
		color /= samples;

		/* add to color */
		lightLuxel.value = color;
		lightLuxel.count += 1.0f;

		if ( lightDeluxel ) {
			direction /= samples;
			*lightDeluxel = direction;
		}
	}
}

/* A mostly Gaussian-like bounded random distribution (sigma is expected standard deviation) */
static void GaussLikeRandom( float sigma, float *x, float *y ){
	float r;
	r = Random() * 2 * c_pi;
	*x = sigma * 2.73861278752581783822 * cos( r );
	*y = sigma * 2.73861278752581783822 * sin( r );
	r = Random();
	r = 1 - sqrt( r );
	r = 1 - sqrt( r );
	*x *= r;
	*y *= r;
}
static void RandomSubsampleRawLuxel( rawLightmap_t *lm, trace_t *trace, const Vector3& sampleOrigin, int x, int y, float bias, SuperLuxel& lightLuxel, Vector3 *lightDeluxel ){
	int b, mapped = 0;
	int cluster;
	Vector3 origin, normal;
	Vector3 total( 0 ), totaldirection( 0 );
	float dx, dy;

	for ( b = 0; b < lightSamples; ++b )
	{
		/* set origin */
		origin = sampleOrigin;
		GaussLikeRandom( bias, &dx, &dy );

		/* calculate position */
		if ( !SubmapRawLuxel( lm, x, y, dx, dy, cluster, origin, normal ) ) {
			cluster = -1;
			continue;
		}
		mapped++;

		trace->cluster = cluster;
		trace->origin = origin;
		trace->normal = normal;

		LightContributionToSample( trace );
		total += trace->color;
		if ( lightDeluxel ) {
			totaldirection += trace->directionContribution;
		}
	}

	/* add to luxel */
	if ( mapped > 0 ) {
		/* average */
		lightLuxel.value = total / mapped;

		if ( lightDeluxel ) {
			*lightDeluxel = totaldirection / mapped;
		}
	}
}



/*
   CreateTraceLightsForBounds()
   creates a list of lights that affect the given bounding box and pvs clusters (bsp leaves)
 */

static void CreateTraceLightsForBounds( const MinMax& minmax, const Vector3 *normal, int numClusters, int *clusters, LightFlags flags, trace_t *trace ){
	int i;
	float length;


	/* debug code */
	//% Sys_Printf( "CTWLFB: (%4.1f %4.1f %4.1f) (%4.1f %4.1f %4.1f)\n", minmax.mins[ 0 ], minmax.mins[ 1 ], minmax.mins[ 2 ], minmax.maxs[ 0 ], minmax.maxs[ 1 ], minmax.maxs[ 2 ] );

	/* allocate the light list */
	trace->lights = safe_malloc( sizeof( light_t* ) * ( lights.size() + 1 ) );
	trace->numLights = 0;

	/* calculate spherical bounds */
	const Vector3 origin = minmax.origin();
	const float radius = vector3_length( minmax.maxs - origin );

	/* get length of normal vector */
	if ( normal != NULL ) {
		length = vector3_length( *normal );
	}
	else
	{
		normal = &g_vector3_identity;
		length = 0;
	}

	/* test each light and see if it reaches the sphere */
	/* note: the attenuation code MUST match LightingAtSample() */
	for ( const light_t& light : lights )
	{
		/* check zero sized envelope */
		if ( light.envelope <= 0 ) {
			lightsEnvelopeCulled++;
			continue;
		}

		/* check flags */
		if ( !( light.flags & flags ) ) {
			continue;
		}

		/* sunlight skips all this nonsense */
		if ( light.type != ELightType::Sun ) {
			/* sun only? */
			if ( sunOnly ) {
				continue;
			}

			/* check against pvs cluster */
			if ( numClusters > 0 && clusters != NULL ) {
				for ( i = 0; i < numClusters; i++ )
				{
					if ( ClusterVisible( light.cluster, clusters[ i ] ) ) {
						break;
					}
				}

				/* fixme! */
				if ( i == numClusters ) {
					lightsClusterCulled++;
					continue;
				}
			}

			/* if the light's bounding sphere intersects with the bounding sphere then this light needs to be tested */
			if ( vector3_length( light.origin - origin ) - light.envelope - radius > 0 ) {
				lightsEnvelopeCulled++;
				continue;
			}

			/* check bounding box against light's pvs envelope (note: this code never eliminated any lights, so disabling it) */
			#if 0
			if( !minmax.test( light.minmax ) ){
				lightsBoundsCulled++;
				continue;
			}
			#endif
		}

		/* planar surfaces (except twosided surfaces) have a couple more checks */
		if ( length > 0.0f && !trace->twoSided ) {
			/* lights coplanar with a surface won't light it */
			if ( !( light.flags & LightFlags::Twosided ) && vector3_dot( light.normal, *normal ) > 0.999f ) {
				lightsPlaneCulled++;
				continue;
			}

			/* check to see if light is behind the plane */
			if ( vector3_dot( light.origin, *normal ) - vector3_dot( origin, *normal ) < -1.0f ) {
				lightsPlaneCulled++;
				continue;
			}
		}

		/* add this light */
		trace->lights[ trace->numLights++ ] = &light;
	}

	/* make last night null */
	trace->lights[ trace->numLights ] = NULL;
}



/*
   CreateTraceLightsForSurface()
   creates a list of lights that can potentially affect a drawsurface
 */

static void CreateTraceLightsForSurface( int num, trace_t *trace ){
	/* dummy check */
	if ( num < 0 ) {
		return;
	}

	/* get drawsurface and info */
	const bspDrawSurface_t& ds = bspDrawSurfaces[ num ];
	const surfaceInfo_t& info = surfaceInfos[ num ];

	/* get the mins/maxs for the dsurf */
	MinMax minmax;
	Vector3 normal = bspDrawVerts[ ds.firstVert ].normal;
	for ( int i = 0; i < ds.numVerts; i++ )
	{
		const bspDrawVert_t& dv = yDrawVerts[ ds.firstVert + i ];
		minmax.extend( dv.xyz );
		if ( !VectorCompare( dv.normal, normal ) ) {
			normal.set( 0 );
		}
	}

	/* create the lights for the bounding box */
	CreateTraceLightsForBounds( minmax, &normal, info.numSurfaceClusters, &surfaceClusters[ info.firstSurfaceCluster ], LightFlags::Surfaces, trace );
}

inline void FreeTraceLights( trace_t *trace ){
	free( trace->lights );
}



/*
   IlluminateRawLightmap()
   illuminates the luxels
 */
static void FloodlightIlluminateLightmap( rawLightmap_t *lm );

void IlluminateRawLightmap( int rawLightmapNum ){
	int i, t, x, y, sx, sy, size, luxelFilterRadius, lightmapNum;
	int                 mapped, lighted, totalLighted;
	rawLightmap_t       *lm;
	surfaceInfo_t       *info;
	bool filterColor, filterDir;
	float               samples, filterRadius, weight;
	Vector3 averageColor, averageDir;
	float tests[ 4 ][ 2 ] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	trace_t trace;
	SuperLuxel stackLightLuxels[ 64 * 64 ];


	/* bail if this number exceeds the number of raw lightmaps */
	if ( rawLightmapNum >= numRawLightmaps ) {
		return;
	}

	/* get lightmap */
	lm = &rawLightmaps[ rawLightmapNum ];

	/* setup trace */
	trace.testOcclusion = !noTrace;
	trace.forceSunlight = false;
	trace.recvShadows = lm->recvShadows;
	trace.numSurfaces = lm->numLightSurfaces;
	trace.surfaces = &lightSurfaces[ lm->firstLightSurface ];
	trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;

	/* twosided lighting (may or may not be a good idea for lightmapped stuff) */
	trace.twoSided = false;
	for ( i = 0; i < trace.numSurfaces; i++ )
	{
		/* get surface */
		info = &surfaceInfos[ trace.surfaces[ i ] ];

		/* check twosidedness */
		if ( info->si->twoSided ) {
			trace.twoSided = true;
			break;
		}
	}

	/* create a culled light list for this raw lightmap */
	CreateTraceLightsForBounds( lm->minmax, ( lm->plane == NULL? NULL : &lm->plane->normal() ), lm->numLightClusters, lm->lightClusters, LightFlags::Surfaces, &trace );

	/* -----------------------------------------------------------------
	   fill pass
	   ----------------------------------------------------------------- */

	/* set counts */
	numLuxelsIlluminated += ( lm->sw * lm->sh );

	/* test debugging state */
	if ( debugSurfaces || debugAxis || debugCluster || debugOrigin || dirtDebug || normalmap ) {
		/* debug fill the luxels */
		for ( y = 0; y < lm->sh; y++ )
		{
			for ( x = 0; x < lm->sw; x++ )
			{
				/* get cluster */
				const int cluster = lm->getSuperCluster( x, y );

				/* only fill mapped luxels */
				if ( cluster < 0 ) {
					continue;
				}

				/* get particulars */
				SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );

				/* color the luxel with raw lightmap num? */
				if ( debugSurfaces ) {
					luxel.value = debugColors[ rawLightmapNum % 12 ];
				}

				/* color the luxel with lightmap axis? */
				else if ( debugAxis ) {
					luxel.value = ( lm->axis + Vector3( 1 ) ) * 127.5f;
				}

				/* color the luxel with luxel cluster? */
				else if ( debugCluster ) {
					luxel.value = debugColors[ cluster % 12 ];
				}

				/* color the luxel with luxel origin? */
				else if ( debugOrigin ) {
					const Vector3 temp = ( lm->minmax.maxs - lm->minmax.mins ) * ( 1.0f / 255.0f );
					const Vector3 temp2 = lm->getSuperOrigin( x, y ) - lm->minmax.mins;
					luxel.value = lm->minmax.mins + ( temp * temp2 );
				}

				/* color the luxel with the normal */
				else if ( normalmap ) {
					luxel.value = ( lm->getSuperNormal( x, y ) + Vector3( 1 ) ) * 127.5f;
				}

				/* otherwise clear it */
				else{
					luxel.value.set( 0 );
				}

				/* add to counts */
				luxel.count = 1.0f;
			}
		}
	}
	else
	{
		/* allocate temporary per-light luxel storage */
		rawLightmap_t tmplm = *lm;
		const size_t llSize = lm->sw * lm->sh * sizeof( *lm->superLuxels[0] );
		const size_t ldSize = lm->sw * lm->sh * sizeof( *lm->superDeluxels );
		if ( llSize <= sizeof( stackLightLuxels ) ) {
			tmplm.superLuxels[0] = stackLightLuxels;
		}
		else{
			tmplm.superLuxels[0] = safe_malloc( llSize );
		}
		if ( deluxemap ) {
			tmplm.superDeluxels = safe_malloc( ldSize );
		}
		else{
			tmplm.superDeluxels = NULL;
		}

		/* clear luxels */
		//%	memset( lm->superLuxels[ 0 ], 0, llSize );

		/* set ambient color */
		for ( y = 0; y < lm->sh; y++ )
		{
			for ( x = 0; x < lm->sw; x++ )
			{
				/* get cluster */
				SuperLuxel& luxel = lm->getSuperLuxel( 0, x, y );

				/* blacken unmapped clusters */
				if ( lm->getSuperCluster( x, y ) < 0 ) {
					luxel.value.set( 0 );
				}
				/* set ambient */
				else
				{
					luxel.value = ambientColor;
					if ( deluxemap ) {
						// use AT LEAST this amount of contribution from ambient for the deluxemap, fixes points that receive ZERO light
						const float brightness = std::max( 0.00390625f, RGBTOGRAY( ambientColor ) * ( 1.0f / 255.0f ) );

						lm->getSuperDeluxel( x, y ) = lm->getSuperNormal( x, y ) * brightness;
					}
					luxel.count = 1.0f;
				}
			}
		}

		/* clear styled lightmaps */
		size = lm->sw * lm->sh * sizeof( *lm->superLuxels[0] );
		for ( lightmapNum = 1; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			if ( lm->superLuxels[ lightmapNum ] != NULL ) {
				memset( lm->superLuxels[ lightmapNum ], 0, size );
			}
		}

		/* debugging code */
		//%	if( trace.numLights <= 0 )
		//%		Sys_Printf( "Lightmap %9d: 0 lights, axis: %.2f, %.2f, %.2f\n", rawLightmapNum, lm->axis[ 0 ], lm->axis[ 1 ], lm->axis[ 2 ] );

		/* walk light list */
		for ( i = 0; i < trace.numLights; i++ )
		{
			/* setup trace */
			trace.light = trace.lights[ i ];

			/* style check */
			for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
			{
				if ( lm->styles[ lightmapNum ] == trace.light->style ||
				     lm->styles[ lightmapNum ] == LS_NONE ) {
					break;
				}
			}

			/* max of MAX_LIGHTMAPS (4) styles allowed to hit a surface/lightmap */
			if ( lightmapNum >= MAX_LIGHTMAPS ) {
				Sys_Warning( "Hit per-surface style limit (%d)\n", MAX_LIGHTMAPS );
				continue;
			}

			/* setup */
			memset( tmplm.superLuxels[0], 0, llSize );
			if ( deluxemap ) {
				memset( tmplm.superDeluxels, 0, ldSize );
			}
			totalLighted = 0;

			/* determine filter radius */
			filterRadius = std::max( { 0.f, lm->filterRadius, trace.light->filterRadius } );

			/* set luxel filter radius */
			luxelFilterRadius = lm->sampleSize != 0 ? superSample * filterRadius / lm->sampleSize : 0;
			if ( luxelFilterRadius == 0 && ( filterRadius > 0.0f || filter ) ) {
				luxelFilterRadius = 1;
			}

			/* allocate sampling flags storage */
			if ( lightSamples > 1 || lightRandomSamples ) {
				size = lm->sw * lm->sh * sizeof( *lm->superFlags );
				if ( lm->superFlags == NULL ) {
					lm->superFlags = safe_malloc( size );
				}
				memset( lm->superFlags, 0, size );
			}

			/* initial pass, one sample per luxel */
			for ( y = 0; y < lm->sh; y++ )
			{
				for ( x = 0; x < lm->sw; x++ )
				{
					/* get cluster */
					const int cluster = lm->getSuperCluster( x, y );
					if ( cluster < 0 ) {
						continue;
					}

					/* get particulars */
					SuperLuxel& lightLuxel = tmplm.getSuperLuxel( 0, x, y );

#if 0
					////////// 27's temp hack for testing edge clipping ////
					if ( lm->getSuperOrigin( x, y ) == g_vector3_identity ) {
						lightLuxel.value[ 1 ] = 255;
						lightLuxel.count = 1.0f;
						totalLighted++;
					}
					else
#endif
					{
						/* set contribution count */
						lightLuxel.count = 1.0f;

						/* setup trace */
						trace.cluster = cluster;
						trace.origin = lm->getSuperOrigin( x, y );
						trace.normal = lm->getSuperNormal( x, y );

						/* get light for this sample */
						LightContributionToSample( &trace );
						lightLuxel.value = trace.color;

						/* add the contribution to the deluxemap */
						if ( deluxemap ) {
							tmplm.getSuperDeluxel( x, y ) = trace.directionContribution;
						}

						/* check for evilness */
						if ( trace.forceSubsampling > 1.0f && ( lightSamples > 1 || lightRandomSamples ) ) {
							totalLighted++;
							lm->getSuperFlag( x, y ) |= FLAG_FORCE_SUBSAMPLING; /* force */
						}
						/* add to count */
						else if ( trace.color != g_vector3_identity ) {
							totalLighted++;
						}
					}
				}
			}

			/* don't even bother with everything else if nothing was lit */
			if ( totalLighted == 0 ) {
				continue;
			}

			/* secondary pass, adaptive supersampling (fixme: use a contrast function to determine if subsampling is necessary) */
			/* 2003-09-27: changed it so filtering disamples supersampling, as it would waste time */
			if ( lightSamples > 1 || lightRandomSamples ) {
				/* walk luxels */
				for ( y = 0; y < ( lm->sh - 1 ); y++ )
				{
					for ( x = 0; x < ( lm->sw - 1 ); x++ )
					{
						/* setup */
						mapped = 0;
						lighted = 0;
						Vector3 total( 0 );

						/* test 2x2 stamp */
						for ( t = 0; t < 4; t++ )
						{
							/* set sample coords */
							sx = x + tests[ t ][ 0 ];
							sy = y + tests[ t ][ 1 ];

							/* get cluster */
							if ( lm->getSuperCluster( sx, sy ) < 0 ) {
								continue;
							}
							mapped++;

							/* get luxel */
							if ( lm->getSuperFlag( sx, sy ) & FLAG_FORCE_SUBSAMPLING ) {
								/* force a lighted/mapped discrepancy so we subsample */
								++lighted;
								++mapped;
								++mapped;
							}
							const SuperLuxel& lightLuxel = tmplm.getSuperLuxel( 0, sx, sy );
							total += lightLuxel.value;
							if ( ( lightLuxel.value[ 0 ] + lightLuxel.value[ 1 ] + lightLuxel.value[ 2 ] ) > 0.0f ) {
								lighted++;
							}
						}

						/* if total color is under a certain amount, then don't bother subsampling */
						if ( total[ 0 ] <= 4.0f && total[ 1 ] <= 4.0f && total[ 2 ] <= 4.0f ) {
							continue;
						}

						/* if all 4 pixels are either in shadow or light, then don't subsample */
						if ( lighted != 0 && lighted != mapped ) {
							for ( t = 0; t < 4; t++ )
							{
								/* set sample coords */
								sx = x + tests[ t ][ 0 ];
								sy = y + tests[ t ][ 1 ];

								/* get luxel */
								if ( lm->getSuperCluster( sx, sy ) < 0 ) {
									continue;
								}
								byte& flag = lm->getSuperFlag( sx, sy );
								if ( flag & FLAG_ALREADY_SUBSAMPLED ) { // already subsampled
									continue;
								}
								SuperLuxel& lightLuxel = tmplm.getSuperLuxel( 0, sx, sy );
								Vector3* lightDeluxel = &tmplm.getSuperDeluxel( sx, sy );
								const Vector3& origin = lm->getSuperOrigin( sx, sy );

								/* only subsample shadowed luxels */
								//%	if( ( lightLuxel[ 0 ] + lightLuxel[ 1 ] + lightLuxel[ 2 ] ) <= 0.0f )
								//%		continue;

								/* subsample it */
								if ( lightRandomSamples ) {
									RandomSubsampleRawLuxel( lm, &trace, origin, sx, sy, 0.5f * lightSamplesSearchBoxSize, lightLuxel, deluxemap ? lightDeluxel : NULL );
								}
								else{
									SubsampleRawLuxel_r( lm, &trace, origin, sx, sy, 0.25f * lightSamplesSearchBoxSize, lightLuxel, deluxemap ? lightDeluxel : NULL );
								}

								flag |= FLAG_ALREADY_SUBSAMPLED;

								/* debug code to colorize subsampled areas to yellow */
								//%	lm->getSuperLuxel( lightmapNum, sx, sy ).value = { 255, 204, 0 };
							}
						}
					}
				}
			}

			/* tertiary pass, apply dirt map (ambient occlusion) */
			if ( 0 && dirty ) {
				/* walk luxels */
				for ( y = 0; y < lm->sh; y++ )
				{
					for ( x = 0; x < lm->sw; x++ )
					{
						/* get cluster  */
						if ( lm->getSuperCluster( x, y ) < 0 ) {
							continue;
						}

						/* scale light value */
						tmplm.getSuperLuxel( 0, x, y ).value *= lm->getSuperDirt( x, y );
					}
				}
			}

			/* allocate sampling lightmap storage */
			if ( lm->superLuxels[ lightmapNum ] == NULL ) {
				/* allocate sampling lightmap storage */
				size = lm->sw * lm->sh * sizeof( *lm->superLuxels[0] );
				lm->superLuxels[ lightmapNum ] = safe_calloc( size );
			}

			/* set style */
			if ( lightmapNum > 0 ) {
				lm->styles[ lightmapNum ] = trace.light->style;
				//%	Sys_Printf( "Surface %6d has lightstyle %d\n", rawLightmapNum, trace.light->style );
			}

			/* copy to permanent luxels */
			for ( y = 0; y < lm->sh; y++ )
			{
				for ( x = 0; x < lm->sw; x++ )
				{
					/* get cluster and origin */
					if ( lm->getSuperCluster( x, y ) < 0 ) {
						continue;
					}

					/* filter? */
					if ( luxelFilterRadius ) {
						/* setup */
						averageColor.set( 0 );
						averageDir.set( 0 );
						samples = 0.0f;

						/* cheaper distance-based filtering */
						for ( sy = ( y - luxelFilterRadius ); sy <= ( y + luxelFilterRadius ); sy++ )
						{
							if ( sy < 0 || sy >= lm->sh ) {
								continue;
							}

							for ( sx = ( x - luxelFilterRadius ); sx <= ( x + luxelFilterRadius ); sx++ )
							{
								if ( sx < 0 || sx >= lm->sw ) {
									continue;
								}

								/* get particulars */
								if ( lm->getSuperCluster( sx, sy ) < 0 ) {
									continue;
								}

								/* create weight */
								weight = ( abs( sx - x ) == luxelFilterRadius ? 0.5f : 1.0f );
								weight *= ( abs( sy - y ) == luxelFilterRadius ? 0.5f : 1.0f );

								/* scale luxel by filter weight */
								averageColor += tmplm.getSuperLuxel( 0, sx, sy ).value * weight;
								if ( deluxemap ) {
									averageDir += tmplm.getSuperDeluxel( sx, sy ) * weight;
								}
								samples += weight;
							}
						}

						/* any samples? */
						if ( samples <= 0.0f ) {
							continue;
						}

						/* scale into luxel */
						SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );
						luxel.count = 1.0f;

						/* handle negative light */
						if ( trace.light->flags & LightFlags::Negative ) {
							luxel.value -= averageColor / samples;
						}
						/* handle normal light */
						else
						{
							luxel.value += averageColor / samples;
						}

						if ( deluxemap ) {
							/* scale into luxel */
							lm->getSuperDeluxel( x, y ) += averageDir / samples;
						}
					}

					/* single sample */
					else
					{
						/* get particulars */
						const SuperLuxel& lightLuxel = tmplm.getSuperLuxel( 0, x, y );
						SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );

						/* handle negative light */
						if ( trace.light->flags & LightFlags::Negative ) {
							vector3_negate( averageColor );
						}

						/* add color */
						luxel.count = 1.0f;

						/* handle negative light */
						if ( trace.light->flags & LightFlags::Negative ) {
							luxel.value -= lightLuxel.value;
						}
						/* handle normal light */
						else{
							luxel.value += lightLuxel.value;
						}

						if ( deluxemap ) {
							lm->getSuperDeluxel( x, y ) += tmplm.getSuperDeluxel( x, y );
						}
					}
				}
			}
		}

		/* free temporary luxels */
		if ( tmplm.superLuxels[0] != stackLightLuxels ) {
			free( tmplm.superLuxels[0] );
		}

		if ( deluxemap ) {
			free( tmplm.superDeluxels );
		}
	}

	/* free light list */
	FreeTraceLights( &trace );

	/* floodlight pass */
	if ( floodlighty ) {
		FloodlightIlluminateLightmap( lm );
	}

	if ( debugnormals ) {
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			/* early out */
			if ( lm->superLuxels[ lightmapNum ] == NULL ) {
				continue;
			}

			for ( y = 0; y < lm->sh; y++ )
			{
				for ( x = 0; x < lm->sw; x++ )
				{
					/* get cluster */
					//%	if( lm->getSuperCluster( x, y ) < 0 )
					//%		continue;

					lm->getSuperLuxel( lightmapNum, x, y ).value = lm->getSuperNormal( x, y ) * 127 + Vector3( 127 );
				}
			}
		}
	}

	/*	-----------------------------------------------------------------
	    dirt pass
	    ----------------------------------------------------------------- */

	if ( dirty ) {
		/* walk lightmaps */
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			/* early out */
			if ( lm->superLuxels[ lightmapNum ] == NULL ) {
				continue;
			}

			/* apply dirt to each luxel */
			for ( y = 0; y < lm->sh; y++ )
			{
				for ( x = 0; x < lm->sw; x++ )
				{
					/* get cluster */
					//%	if( lm->getSuperCluster( x, y ) < 0 ) // TODO why not do this check? These pixels should be zero anyway
					//%		continue;

					/* get particulars */
					SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );
					const float dirt = lm->getSuperDirt( x, y );

					/* apply dirt */
					luxel.value *= dirt;

					/* debugging */
					if ( dirtDebug ) {
						luxel.value.set( dirt * 255.0f );
					}
				}
			}
		}
	}

	/* -----------------------------------------------------------------
	   filter pass
	   ----------------------------------------------------------------- */

	/* walk lightmaps */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
	{
		/* early out */
		if ( lm->superLuxels[ lightmapNum ] == NULL ) {
			continue;
		}

		/* average occluded luxels from neighbors */
		for ( y = 0; y < lm->sh; y++ )
		{
			for ( x = 0; x < lm->sw; x++ )
			{
				/* get particulars */
				int& cluster = lm->getSuperCluster( x, y );
				SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );

				/* determine if filtering is necessary */
				filterColor = false;
				filterDir = false;
				if ( cluster < 0 ||
				     ( lm->splotchFix && ( luxel.value[ 0 ] <= ambientColor[ 0 ]
				                        || luxel.value[ 1 ] <= ambientColor[ 1 ]
				                        || luxel.value[ 2 ] <= ambientColor[ 2 ] ) ) ) {
					filterColor = true;
				}

				if ( deluxemap && lightmapNum == 0 && ( cluster < 0 || filter ) ) {
					filterDir = true;
				}

				if ( !filterColor && !filterDir ) {
					continue;
				}

				/* choose seed amount */
				averageColor.set( 0 );
				averageDir.set( 0 );
				samples = 0.0f;

				/* walk 3x3 matrix */
				for ( sy = ( y - 1 ); sy <= ( y + 1 ); sy++ )
				{
					if ( sy < 0 || sy >= lm->sh ) {
						continue;
					}

					for ( sx = ( x - 1 ); sx <= ( x + 1 ); sx++ )
					{
						if ( sx < 0 || sx >= lm->sw || ( sx == x && sy == y ) ) {
							continue;
						}

						/* get neighbor's particulars */
						const SuperLuxel& luxel2 = lm->getSuperLuxel( lightmapNum, sx, sy );

						/* ignore unmapped/unlit luxels */
						if ( lm->getSuperCluster( sx, sy ) < 0 || luxel2.count == 0.0f ||
						     ( lm->splotchFix && VectorCompare( luxel2.value, ambientColor ) ) ) {
							continue;
						}

						/* add its distinctiveness to our own */
						averageColor += luxel2.value;
						samples += luxel2.count;
						if ( filterDir ) {
							averageDir += lm->getSuperDeluxel( sx, sy );
						}
					}
				}

				/* fall through */
				if ( samples <= 0.0f ) {
					continue;
				}

				/* dark lightmap seams */
				if ( dark ) {
					if ( lightmapNum == 0 ) {
						averageColor += ambientColor * 2;
					}
					samples += 2.0f;
				}

				/* average it */
				if ( filterColor ) {
					luxel.value = averageColor * ( 1.f / samples );
					luxel.count = 1.0f;
				}
				if ( filterDir ) {
					lm->getSuperDeluxel( x, y ) = averageDir * ( 1.f / samples );
				}

				/* set cluster to -3 */
				if ( cluster < 0 ) {
					cluster = CLUSTER_FLOODED;
				}
			}
		}
	}


#if 0
	// audit pass
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
	{
		/* early out */
		if ( lm->superLuxels[ lightmapNum ] == NULL ) {
			continue;
		}
		for ( y = 0; y < lm->sh; y++ )
			for ( x = 0; x < lm->sw; x++ )
			{
				/* get cluster */
				cluster = SUPER_CLUSTER( x, y );
				luxel = SUPER_LUXEL( lightmapNum, x, y );
				deluxel = SUPER_DELUXEL( x, y );
				if ( !luxel || !deluxel || !cluster ) {
					Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: I got NULL'd.\n" );
					continue;
				}
				else if ( *cluster < 0 ) {
					// unmapped pixel
					// should have neither deluxemap nor lightmap
					if ( deluxel[3] ) {
						Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: I have written deluxe to an unmapped luxel. Sorry.\n" );
					}
				}
				else
				{
					// mapped pixel
					// should have both deluxemap and lightmap
					if ( deluxel[3] ) {
						Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: I forgot to write deluxe to a mapped luxel. Sorry.\n" );
					}
				}
			}
	}
#endif
}



/*
   IlluminateVertexes()
   light the surface vertexes
 */

#define VERTEX_NUDGE    4.0f

void IlluminateVertexes( int num ){
	int i, x, y, z, x1, y1, z1, sx, sy, radius, maxRadius;
	int lightmapNum, numAvg;
	float samples, dirt;
	Vector3 colors[ MAX_LIGHTMAPS ], avgColors[ MAX_LIGHTMAPS ];
	bspDrawSurface_t    *ds;
	surfaceInfo_t       *info;
	rawLightmap_t       *lm;
	bspDrawVert_t       *verts;
	trace_t trace;
	float floodLightAmount;
	Vector3 floodColor;


	/* get surface, info, and raw lightmap */
	ds = &bspDrawSurfaces[ num ];
	info = &surfaceInfos[ num ];
	lm = info->lm;

	/* -----------------------------------------------------------------
	   illuminate the vertexes
	   ----------------------------------------------------------------- */

	/* calculate vertex lighting for surfaces without lightmaps */
	if ( lm == NULL || cpmaHack ) {
		/* setup trace */
		trace.testOcclusion = ( cpmaHack && lm != NULL ) ? false : !noTrace;
		trace.forceSunlight = info->si->forceSunlight;
		trace.recvShadows = info->recvShadows;
		trace.numSurfaces = 1;
		trace.surfaces = &num;
		trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;

		/* twosided lighting */
		trace.twoSided = info->si->twoSided;

		/* make light list for this surface */
		CreateTraceLightsForSurface( num, &trace );

		/* setup */
		verts = &yDrawVerts[ ds->firstVert ];
		numAvg = 0;
		memset( avgColors, 0, sizeof( avgColors ) );

		/* walk the surface verts */
		for ( i = 0; i < ds->numVerts; i++ )
		{
			/* get vertex luxel */
			Vector3& radVertLuxel = getRadVertexLuxel( 0, ds->firstVert + i );

			/* color the luxel with raw lightmap num? */
			if ( debugSurfaces ) {
				radVertLuxel = debugColors[ num % 12 ];
			}

			/* color the luxel with luxel origin? */
			else if ( debugOrigin ) {
				const Vector3 temp = ( info->minmax.maxs - info->minmax.mins ) * ( 1.0f / 255.0f );
				const Vector3 temp2 = verts[ i ].xyz - info->minmax.mins;
				radVertLuxel = info->minmax.mins + ( temp * temp2 );
			}

			/* color the luxel with the normal */
			else if ( normalmap ) {
				radVertLuxel = ( verts[ i ].normal + Vector3( 1 ) ) * 127.5f;
			}

			else if ( info->si->noVertexLight ) {
				radVertLuxel.set( 127.5f );
			}

			else if ( noVertexLighting > 0 ) {
				radVertLuxel.set( 127.5f * noVertexLighting );
			}

			/* illuminate the vertex */
			else
			{
				/* clear vertex luxel */
				radVertLuxel.set( -1.0f );
				/* setup trace */
				trace.normal = verts[ i ].normal;
				/* try at initial origin */
				trace.cluster = ClusterForPointExtFilter( verts[ i ].xyz, VERTEX_EPSILON, info->numSurfaceClusters, &surfaceClusters[ info->firstSurfaceCluster ] );
				if ( trace.cluster >= 0 ) {
					/* setup trace */
					trace.origin = verts[ i ].xyz;

					/* r7 dirt */
					if ( dirty && !bouncing ) {
						dirt = DirtForSample( &trace );
					}
					else{
						dirt = 1.0f;
					}

					/* jal: floodlight */
					floodLightAmount = 0.0f;
					floodColor.set( 0 );
					if ( floodlighty && !bouncing ) {
						floodLightAmount = floodlightIntensity * FloodLightForSample( &trace, floodlightDistance, floodlight_lowquality );
						floodColor = floodlightRGB * floodLightAmount;
					}

					/* trace */
					LightingAtSample( &trace, ds->vertexStyles, colors );

					/* store */
					for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
					{
						/* r7 dirt */
						colors[ lightmapNum ] *= dirt;

						/* jal: floodlight */
						colors[ lightmapNum ] += floodColor;

						/* store */
						getRadVertexLuxel( lightmapNum, ds->firstVert + i ) = colors[ lightmapNum ];
						colors[ lightmapNum ] += avgColors[ lightmapNum ];
					}
				}

				/* is this sample bright enough? */
				const auto vector3_component_greater = []( const Vector3& greater, const Vector3& lesser ){
					return greater[0] > lesser[0] || greater[1] > lesser[1] || greater[2] > lesser[2];
				};
				if ( !vector3_component_greater( getRadVertexLuxel( 0, ds->firstVert + i ), ambientColor ) ) {
					/* nudge the sample point around a bit */
					for ( x = 0; x < 5; x++ )
					{
						/* two's complement 0, 1, -1, 2, -2, etc */
						x1 = ( ( x >> 1 ) ^ ( x & 1 ? -1 : 0 ) ) + ( x & 1 );

						for ( y = 0; y < 5; y++ )
						{
							y1 = ( ( y >> 1 ) ^ ( y & 1 ? -1 : 0 ) ) + ( y & 1 );

							for ( z = 0; z < 5; z++ )
							{
								z1 = ( ( z >> 1 ) ^ ( z & 1 ? -1 : 0 ) ) + ( z & 1 );

								/* nudge origin */
								trace.origin = verts[ i ].xyz + Vector3( x1, y1, z1 ) * VERTEX_NUDGE;

								/* try at nudged origin */
								trace.cluster = ClusterForPointExtFilter( trace.origin, VERTEX_EPSILON, info->numSurfaceClusters, &surfaceClusters[ info->firstSurfaceCluster ] );
								if ( trace.cluster < 0 ) {
									continue;
								}

								/* r7 dirt */
								if ( dirty && !bouncing ) {
									dirt = DirtForSample( &trace );
								}
								else{
									dirt = 1.0f;
								}

								/* jal: floodlight */
								floodLightAmount = 0.0f;
								floodColor.set( 0 );
								if ( floodlighty && !bouncing ) {
									floodLightAmount = floodlightIntensity * FloodLightForSample( &trace, floodlightDistance, floodlight_lowquality );
									floodColor = floodlightRGB * floodLightAmount;
								}

								/* trace */
								LightingAtSample( &trace, ds->vertexStyles, colors );

								/* store */
								for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
								{
									/* r7 dirt */
									colors[ lightmapNum ] *= dirt;

									/* jal: floodlight */
									colors[ lightmapNum ] += floodColor;

									/* store */
									getRadVertexLuxel( lightmapNum, ds->firstVert + i ) = colors[ lightmapNum ];
								}

								/* bright enough? */
								if ( vector3_component_greater( getRadVertexLuxel( 0, ds->firstVert + i ), ambientColor ) ) {
									x = y = z = 1000;
								}
							}
						}
					}
				}

				/* add to average? */
				if ( vector3_component_greater( getRadVertexLuxel( 0, ds->firstVert + i ), ambientColor ) ) {
					numAvg++;
					for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
					{
						avgColors[ lightmapNum ] += getRadVertexLuxel( lightmapNum, ds->firstVert + i );
					}
				}
			}

			/* another happy customer */
			numVertsIlluminated++;
		}

		/* set average color */
		if ( numAvg > 0 ) {
			for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
				avgColors[ lightmapNum ] *= ( 1.0f / numAvg );
		}
		else
		{
			avgColors[ 0 ] = ambientColor;
		}

		/* clean up and store vertex color */
		for ( i = 0; i < ds->numVerts; i++ )
		{
			/* store average in occluded vertexes */
			if ( getRadVertexLuxel( 0, ds->firstVert + i )[ 0 ] < 0.0f ) {
				for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
				{
					getRadVertexLuxel( lightmapNum, ds->firstVert + i ) = avgColors[ lightmapNum ];

					/* debug code */
					//%	getRadVertexLuxel( lightmapNum, ds->firstVert + i ) = { 255.0f, 0.0f, 0.0f };
				}
			}

			/* store it */
			for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
			{
				/* get luxels */
				Vector3& vertLuxel = getVertexLuxel( lightmapNum, ds->firstVert + i );
				const Vector3& radVertLuxel = getRadVertexLuxel( lightmapNum, ds->firstVert + i );

				/* store */
				if ( bouncing || bounce == 0 || !bounceOnly ) {
					vertLuxel += radVertLuxel;
				}
				if ( !info->si->noVertexLight ) {
					verts[ i ].color[ lightmapNum ].rgb() = ColorToBytes( vertLuxel, info->si->vertexScale );
				}
			}
		}

		/* free light list */
		FreeTraceLights( &trace );

		/* return to sender */
		return;
	}

	/* -----------------------------------------------------------------
	   reconstitute vertex lighting from the luxels
	   ----------------------------------------------------------------- */

	/* set styles from lightmap */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		ds->vertexStyles[ lightmapNum ] = lm->styles[ lightmapNum ];

	/* get max search radius */
	maxRadius = std::max( lm->sw, lm->sh );

	/* walk the surface verts */
	verts = &yDrawVerts[ ds->firstVert ];
	for ( i = 0; i < ds->numVerts; i++ )
	{
		/* do each lightmap */
		for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			/* early out */
			if ( lm->superLuxels[ lightmapNum ] == NULL ) {
				continue;
			}

			/* get luxel coords */
			x = std::clamp( int( verts[ i ].lightmap[ lightmapNum ][ 0 ] ), 0, lm->sw - 1 );
			y = std::clamp( int( verts[ i ].lightmap[ lightmapNum ][ 1 ] ), 0, lm->sh - 1 );

			/* get vertex luxels */
			Vector3& vertLuxel = getVertexLuxel( lightmapNum, ds->firstVert + i );
			Vector3& radVertLuxel = getRadVertexLuxel( lightmapNum, ds->firstVert + i );

			/* color the luxel with the normal? */
			if ( normalmap ) {
				radVertLuxel = ( verts[ i ].normal + Vector3( 1 ) ) * 127.5f;
			}

			/* color the luxel with surface num? */
			else if ( debugSurfaces ) {
				radVertLuxel = debugColors[ num % 12 ];
			}

			else if ( info->si->noVertexLight ) {
				radVertLuxel.set( 127.5f );
			}

			else if ( noVertexLighting > 0 ) {
				radVertLuxel.set( 127.5f * noVertexLighting );
			}

			/* divine color from the superluxels */
			else
			{
				/* increasing radius */
				radVertLuxel.set( 0 );
				samples = 0.0f;
				for ( radius = 0; radius < maxRadius && samples <= 0.0f; radius++ )
				{
					/* sample within radius */
					for ( sy = ( y - radius ); sy <= ( y + radius ); sy++ )
					{
						if ( sy < 0 || sy >= lm->sh ) {
							continue;
						}

						for ( sx = ( x - radius ); sx <= ( x + radius ); sx++ )
						{
							if ( sx < 0 || sx >= lm->sw ) {
								continue;
							}

							/* get luxel particulars */
							const SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, sx, sy );
							if ( lm->getSuperCluster( sx, sy ) < 0 ) {
								continue;
							}

							/* testing: must be brigher than ambient color */
							//%	if( luxel[ 0 ] <= ambientColor[ 0 ] || luxel[ 1 ] <= ambientColor[ 1 ] || luxel[ 2 ] <= ambientColor[ 2 ] )
							//%		continue;

							/* add its distinctiveness to our own */
							radVertLuxel += luxel.value;
							samples += luxel.count;
						}
					}
				}

				/* any color? */
				if ( samples > 0.0f ) {
					radVertLuxel *= ( 1.f / samples );
				}
				else{
					radVertLuxel = ambientColor;
				}
			}

			/* store into floating point storage */
			vertLuxel += radVertLuxel;
			numVertsIlluminated++;

			/* store into bytes (for vertex approximation) */
			if ( !info->si->noVertexLight ) {
				verts[ i ].color[ lightmapNum ].rgb() = ColorToBytes( vertLuxel, 1.0f );
			}
		}
	}
}



/* -------------------------------------------------------------------------------

   light optimization (-fast)

   creates a list of lights that will affect a surface and stores it in tw
   this is to optimize surface lighting by culling out as many of the
   lights in the world as possible from further calculation

   ------------------------------------------------------------------------------- */

/*
   SetupBrushes()
   determines opaque brushes in the world and find sky shaders for sunlight calculations
 */

void SetupBrushesFlags( int mask_any, int test_any, int mask_all, int test_all ){
	int compileFlags, allCompileFlags;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SetupBrushes ---\n" );

	/* clear */
	opaqueBrushes = decltype( opaqueBrushes )( bspBrushes.size(), false );
	int numOpaqueBrushes = 0;

	/* walk the list of worldspawn brushes */
	for ( int i = 0; i < bspModels[ 0 ].numBSPBrushes; i++ )
	{
		/* get brush */
		const int b = bspModels[ 0 ].firstBSPBrush + i;
		const bspBrush_t& brush = bspBrushes[ b ];

		/* check all sides */
		compileFlags = 0;
		allCompileFlags = ~( 0 );
		for ( int j = 0; j < brush.numSides; j++ )
		{
			/* do bsp shader calculations */
			const bspBrushSide_t& side = bspBrushSides[ brush.firstSide + j ];

			/* get shader info */
			const shaderInfo_t *si = ShaderInfoForShaderNull( bspShaders[ side.shaderNum ].shader );
			if ( si == NULL ) {
				continue;
			}

			/* or together compile flags */
			compileFlags |= si->compileFlags;
			allCompileFlags &= si->compileFlags;
		}

		/* determine if this brush is opaque to light */
		if ( ( compileFlags & mask_any ) == test_any && ( allCompileFlags & mask_all ) == test_all ) {
			opaqueBrushes[ b ] = true;
			numOpaqueBrushes++;
			maxOpaqueBrush = i;
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d opaque brushes\n", numOpaqueBrushes );
}
void SetupBrushes(){
	SetupBrushesFlags( C_TRANSLUCENT, 0, 0, 0 );
}



/*
   ChopBounds()
   chops a bounding box by the plane defined by origin and normal
   returns false if the bounds is entirely clipped away

   this is not exactly the fastest way to do this...
 */

inline bool ChopBounds( MinMax& minmax, const Vector3& origin, const Vector3& normal ){
	/* FIXME: rewrite this so it doesn't use bloody brushes */
	return true;
}



/*
   SetupEnvelopes()
   calculates each light's effective envelope,
   taking into account brightness, type, and pvs.
 */

#define LIGHT_EPSILON   0.125f
#define LIGHT_NUDGE     2.0f

void SetupEnvelopes( bool forGrid, bool fastFlag ){
	float radius, intensity;


	/* early out for weird cases where there are no lights */
	if ( lights.empty() ) {
		return;
	}

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SetupEnvelopes%s ---\n", fastFlag ? " (fast)" : "" );

	/* count lights */
	int numCulledLights = 0;
	for( auto light = lights.begin(); light != lights.end(); )
	{
		/* handle negative lights */
		if ( light->photons < 0.0f || light->add < 0.0f ) {
			light->photons *= -1.0f;
			light->add *= -1.0f;
			light->flags |= LightFlags::Negative;
		}

		/* sunlight? */
		if ( light->type == ELightType::Sun ) {
			/* special cased */
			light->cluster = 0;
			light->envelope = MAX_WORLD_COORD * 8.0f;
			light->minmax.mins.set( MIN_WORLD_COORD * 8.0f );
			light->minmax.maxs.set( MAX_WORLD_COORD * 8.0f );
		}

		/* everything else */
		else
		{
			/* get pvs cluster for light */
			light->cluster = ClusterForPointExt( light->origin, LIGHT_EPSILON );

			/* invalid cluster? */
			if ( light->cluster < 0 ) {
				/* nudge the sample point around a bit */
				for ( int x = 0; x < 4; x++ )
				{
					/* two's complement 0, 1, -1, 2, -2, etc */
					const int x1 = ( ( x >> 1 ) ^ ( x & 1 ? -1 : 0 ) ) + ( x & 1 );

					for ( int y = 0; y < 4; y++ )
					{
						const int y1 = ( ( y >> 1 ) ^ ( y & 1 ? -1 : 0 ) ) + ( y & 1 );

						for ( int z = 0; z < 4; z++ )
						{
							const int z1 = ( ( z >> 1 ) ^ ( z & 1 ? -1 : 0 ) ) + ( z & 1 );

							/* nudge origin */
							const Vector3 origin = light->origin + Vector3( x1, y1, z1 ) * LIGHT_NUDGE;

							/* try at nudged origin */
							light->cluster = ClusterForPointExt( origin, LIGHT_EPSILON );
							if ( light->cluster < 0 ) {
								continue;
							}

							/* set origin */
							light->origin = origin;
						}
					}
				}
			}

			/* only calculate for lights in pvs and outside of opaque brushes */
			if ( light->cluster >= 0 ) {
				/* set light fast flag */
				if ( fastFlag ) {
					light->flags |= LightFlags::FastTemp;
				}
				else{
					light->flags &= ~LightFlags::FastTemp;
				}
				if ( fastpoint && ( light->type != ELightType::Area ) ) {
					light->flags |= LightFlags::FastTemp;
				}
				if ( light->si && light->si->noFast ) {
					light->flags &= ~( LightFlags::FastActual );
				}

				/* clear light envelope */
				light->envelope = 0;

				/* handle area lights */
				if ( exactPointToPolygon && light->type == ELightType::Area && !light->w.empty() ) {
					light->envelope = MAX_WORLD_COORD * 8.0f;

					/* check for fast mode */
					if ( light->flags & LightFlags::FastActual ) {
						/* ugly hack to calculate extent for area lights, but only done once */
						const Vector3 dir = -light->normal;
						for ( radius = 100.0f; radius < MAX_WORLD_COORD * 8.0f; radius += 10.0f )
						{
							const Vector3 origin = light->origin + light->normal * radius;
							const float factor = std::abs( PointToPolygonFormFactor( origin, dir, light->w ) );
							if ( ( factor * light->add ) <= light->falloffTolerance ) {
								light->envelope = radius;
								break;
							}
						}
					}

					intensity = light->photons; /* hopefully not used */
				}
				else
				{
					radius = 0.0f;
					intensity = light->photons;
				}

				/* other calcs */
				if ( light->envelope <= 0.0f ) {
					/* solve distance for non-distance lights */
					if ( !( light->flags & LightFlags::AttenDistance ) ) {
						light->envelope = MAX_WORLD_COORD * 8.0f;
					}

					else if ( light->flags & LightFlags::FastActual ) {
						/* solve distance for linear lights */
						if ( ( light->flags & LightFlags::AttenLinear ) ) {
							light->envelope = ( ( intensity * linearScale ) - light->falloffTolerance ) / light->fade;
						}

						/*
						   add = angle * light->photons * linearScale - (dist * light->fade);
						   T = (light->photons * linearScale) - (dist * light->fade);
						   T + (dist * light->fade) = (light->photons * linearScale);
						   dist * light->fade = (light->photons * linearScale) - T;
						   dist = ((light->photons * linearScale) - T) / light->fade;
						 */

						/* solve for inverse square falloff */
						else{
							light->envelope = sqrt( intensity / light->falloffTolerance ) + radius;
						}

						/*
						   add = light->photons / (dist * dist);
						   T = light->photons / (dist * dist);
						   T * (dist * dist) = light->photons;
						   dist = sqrt( light->photons / T );
						 */
					}
					else
					{
						/* solve distance for linear lights */
						if ( ( light->flags & LightFlags::AttenLinear ) ) {
							light->envelope = ( intensity * linearScale ) / light->fade;
						}

						/* can't cull these */
						else{
							light->envelope = MAX_WORLD_COORD * 8.0f;
						}
					}
				}

				/* chop radius against pvs */
				{
					/* clear bounds */
					MinMax minmax;

					/* check all leaves */
					for ( const bspLeaf_t& leaf : bspLeafs )
					{
						/* in pvs? */
						if ( leaf.cluster < 0 ) {
							continue;
						}
						if ( !ClusterVisible( light->cluster, leaf.cluster ) ) { /* ydnar: thanks Arnout for exposing my stupid error (this never failed before) */
							continue;
						}

						/* add this leafs bbox to the bounds */
						minmax.extend( leaf.minmax );
					}

					/* test to see if bounds encompass light */
					if ( !minmax.test( light->origin ) ) {
						//% Sys_Warning( "Light PVS bounds (%.0f, %.0f, %.0f) -> (%.0f, %.0f, %.0f)\ndo not encompass light %d (%f, %f, %f)\n",
						//%     minmax.mins[ 0 ], minmax.mins[ 1 ], minmax.mins[ 2 ],
						//%     minmax.maxs[ 0 ], minmax.maxs[ 1 ], minmax.maxs[ 2 ],
						//%     numLights, light->origin[ 0 ], light->origin[ 1 ], light->origin[ 2 ] );
						minmax.extend( light->origin );
					}

					/* chop the bounds by a plane for area lights and spotlights */
					if ( light->type == ELightType::Area || light->type == ELightType::Spot ) {
						ChopBounds( minmax, light->origin, light->normal );
					}

					/* copy bounds */
					light->minmax = minmax;

					/* reflect bounds around light origin */
					//%	VectorMA( light->origin, -1.0f, origin, origin );
					minmax.extend( light->origin * 2 - minmax.maxs );
					//%	VectorMA( light->origin, -1.0f, mins, origin );
					minmax.extend( light->origin * 2 - minmax.mins );

					/* calculate spherical bounds */
					radius = vector3_length( minmax.maxs - light->origin );

					/* if this radius is smaller than the envelope, then set the envelope to it */
					//% if ( radius < light->envelope ) Sys_FPrintf( SYS_VRB, "PVS Cull (%d): culled\n", numLights );
					//%	else Sys_FPrintf( SYS_VRB, "PVS Cull (%d): failed (%8.0f > %8.0f)\n", numLights, radius, light->envelope );
					value_minimize( light->envelope, radius );
				}

				/* add grid/surface only check */
				if ( forGrid ) {
					if ( !( light->flags & LightFlags::Grid ) ) {
						light->envelope = 0.0f;
					}
				}
				else
				{
					if ( !( light->flags & LightFlags::Surfaces ) ) {
						light->envelope = 0.0f;
					}
				}
			}

			/* culled? */
			if ( light->cluster < 0 || light->envelope <= 0.0f ) {
				/* debug code */
				//%	Sys_Printf( "Culling light: Cluster: %d Envelope: %f\n", light->cluster, light->envelope );

				/* delete the light */
				numCulledLights++;
				light = lights.erase( light );
				continue;
			}
		}

		/* square envelope */
		light->envelope2 = ( light->envelope * light->envelope );

		/* set next light */
		++light;
	}

	/* sort lights by style */
	lights.sort( []( const light_t& a, const light_t& b ){ return a.style < b.style; } );
	/* if any styled light is present, automatically set nocollapse */
	if ( !lights.empty() && lights.back().style != LS_NORMAL ) {
		noCollapse = true;
	}

	/* emit some statistics */
	Sys_Printf( "%9zu total lights\n", lights.size() );
	Sys_Printf( "%9d culled lights\n", numCulledLights );
}



/////////////////////////////////////////////////////////////

#define FLOODLIGHT_CONE_ANGLE           88  /* degrees */
#define FLOODLIGHT_NUM_ANGLE_STEPS      16
#define FLOODLIGHT_NUM_ELEVATION_STEPS  4
#define FLOODLIGHT_NUM_VECTORS          ( FLOODLIGHT_NUM_ANGLE_STEPS * FLOODLIGHT_NUM_ELEVATION_STEPS )

static Vector3 floodVectors[ FLOODLIGHT_NUM_VECTORS ];
static int numFloodVectors = 0;

void SetupFloodLight(){
	int i, j;
	float angle, elevation, angleStep, elevationStep;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- SetupFloodLight ---\n" );

	/* calculate angular steps */
	angleStep = degrees_to_radians( 360.0f / FLOODLIGHT_NUM_ANGLE_STEPS );
	elevationStep = degrees_to_radians( FLOODLIGHT_CONE_ANGLE / FLOODLIGHT_NUM_ELEVATION_STEPS );

	/* iterate angle */
	angle = 0.0f;
	for ( i = 0, angle = 0.0f; i < FLOODLIGHT_NUM_ANGLE_STEPS; i++, angle += angleStep )
	{
		/* iterate elevation */
		for ( j = 0, elevation = elevationStep * 0.5f; j < FLOODLIGHT_NUM_ELEVATION_STEPS; j++, elevation += elevationStep )
		{
			floodVectors[ numFloodVectors ][ 0 ] = sin( elevation ) * cos( angle );
			floodVectors[ numFloodVectors ][ 1 ] = sin( elevation ) * sin( angle );
			floodVectors[ numFloodVectors ][ 2 ] = cos( elevation );
			numFloodVectors++;
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d numFloodVectors\n", numFloodVectors );

	/* floodlight */
	const char  *value;
	if ( entities[ 0 ].read_keyvalue( value, "_floodlight" ) ) {
		double v1, v2, v3, v4, v5, v6;
		v1 = v2 = v3 = 0;
		v4 = floodlightDistance;
		v5 = floodlightIntensity;
		v6 = floodlightDirectionScale;

		sscanf( value, "%lf %lf %lf %lf %lf %lf", &v1, &v2, &v3, &v4, &v5, &v6 );

		floodlightRGB = Vector3( v1, v2, v3 );

		if ( vector3_length( floodlightRGB ) == 0 ) {
			floodlightRGB = { 0.94, 0.94, 1.0 };
		}

		if ( v4 < 1 ) {
			v4 = 1024;
		}
		if ( v5 < 1 ) {
			v5 = 128;
		}
		if ( v6 < 0 ) {
			v6 = 1;
		}

		floodlightDistance = v4;
		floodlightIntensity = v5;
		floodlightDirectionScale = v6;

		floodlighty = true;
		Sys_Printf( "FloodLighting enabled via worldspawn _floodlight key.\n" );
	}
	else
	{
		floodlightRGB = { 0.94, 0.94, 1.0 };
	}
	if ( colorsRGB ) {
		floodlightRGB[0] = Image_LinearFloatFromsRGBFloat( floodlightRGB[0] );
		floodlightRGB[1] = Image_LinearFloatFromsRGBFloat( floodlightRGB[1] );
		floodlightRGB[2] = Image_LinearFloatFromsRGBFloat( floodlightRGB[2] );
	}
	ColorNormalize( floodlightRGB );
}

/*
   FloodLightForSample()
   calculates floodlight value for a given sample
   once again, kudos to the dirtmapping coder
 */

float FloodLightForSample( trace_t *trace, float floodLightDistance, bool floodLightLowQuality ){
	int i;
	float contribution;
	float gatherLight, outLight;
	Vector3 myUp, myRt;
	int vecs = 0;

	gatherLight = 0;
	/* dummy check */
	//if( !dirty )
	//	return 1.0f;
	if ( trace == NULL || trace->cluster < 0 ) {
		return 0.0f;
	}


	/* setup */
	const float dd = floodLightDistance;
	const Vector3 normal( trace->normal );

	/* check if the normal is aligned to the world-up */
	if ( normal[ 0 ] == 0.0f && normal[ 1 ] == 0.0f && ( normal[ 2 ] == 1.0f || normal[ 2 ] == -1.0f ) ) {
		if ( normal[ 2 ] == 1.0f ) {
			myRt = g_vector3_axis_x;
			myUp = g_vector3_axis_y;
		}
		else if ( normal[ 2 ] == -1.0f ) {
			myRt = -g_vector3_axis_x;
			myUp = g_vector3_axis_y;
		}
	}
	else
	{
		myRt = VectorNormalized( vector3_cross( normal, g_vector3_axis_z ) );
		myUp = VectorNormalized( vector3_cross( myRt, normal ) );
	}

	/* vortex: optimise floodLightLowQuality a bit */
	if ( floodLightLowQuality ) {
		/* iterate through ordered vectors */
		for ( i = 0; i < numFloodVectors; i++ )
			if ( rand() % 10 != 0 ) {
				continue;
			}
	}
	else
	{
		/* iterate through ordered vectors */
		for ( i = 0; i < numFloodVectors; i++ )
		{
			vecs++;

			/* transform vector into tangent space */
			const Vector3 direction = myRt * floodVectors[ i ][ 0 ] + myUp * floodVectors[ i ][ 1 ] + normal * floodVectors[ i ][ 2 ];

			/* set endpoint */
			trace->end = trace->origin + direction * dd;

			// trace->origin += direction;

			SetupTrace( trace );
			trace->color.set( 1 );
			/* trace */
			TraceLine( trace );
			contribution = 1;

			if ( trace->compileFlags & C_SKY || trace->compileFlags & C_TRANSLUCENT ) {
				contribution = 1.0f;
			}
			else if ( trace->opaque ) {
				const float d = vector3_length( trace->hit - trace->origin );

				// d = trace->distance;
				//if ( d > 256 ) gatherDirt += 1;
				contribution = std::min( 1.f, d / dd );

				//gatherDirt += 1.0f - ooDepth * VectorLength( displacement );
			}

			gatherLight += contribution;
		}
	}

	/* early out */
	if ( gatherLight <= 0.0f ) {
		return 0.0f;
	}

	gatherLight /= std::max( 1, vecs );

	outLight = std::min( 1.f, gatherLight );

	/* return to sender */
	return outLight;
}

/*
   FloodLightRawLightmap
   lighttracer style ambient occlusion light hack.
   Kudos to the dirtmapping author for most of this source.
   VorteX: modified to floodlight up custom surfaces (q3map_floodLight)
   VorteX: fixed problems with deluxemapping
 */

// floodlight pass on a lightmap
static void FloodLightRawLightmapPass( rawLightmap_t *lm, Vector3& lmFloodLightRGB, float lmFloodLightIntensity, float lmFloodLightDistance, bool lmFloodLightLowQuality, float floodlightDirectionScale ){
	int i, x, y;
	surfaceInfo_t       *info;
	trace_t trace;
	// int sx, sy;
	// float samples, average, *floodlight2;

	memset( &trace, 0, sizeof( trace_t ) );

	/* setup trace */
	trace.testOcclusion = true;
	trace.forceSunlight = false;
	trace.twoSided = true;
	trace.recvShadows = lm->recvShadows;
	trace.numSurfaces = lm->numLightSurfaces;
	trace.surfaces = &lightSurfaces[ lm->firstLightSurface ];
	trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
	trace.testAll = false;
	trace.distance = 1024;

	/* twosided lighting (may or may not be a good idea for lightmapped stuff) */
	//trace.twoSided = false;
	for ( i = 0; i < trace.numSurfaces; i++ )
	{
		/* get surface */
		info = &surfaceInfos[ trace.surfaces[ i ] ];

		/* check twosidedness */
		if ( info->si->twoSided ) {
			trace.twoSided = true;
			break;
		}
	}

	/* gather floodlight */
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			/* get luxel */
			const int cluster = lm->getSuperCluster( x, y );
			SuperFloodLight& floodlight = lm->getSuperFloodLight( x, y );

			/* set default dirt */
			floodlight.value[0] = 0.0f;

			/* only look at mapped luxels */
			if ( cluster < 0 ) {
				continue;
			}

			/* copy to trace */
			trace.cluster = cluster;
			trace.origin = lm->getSuperOrigin( x, y );
			trace.normal = lm->getSuperNormal( x, y );

			/* get floodlight */
			const float floodLightAmount = FloodLightForSample( &trace, lmFloodLightDistance, lmFloodLightLowQuality ) * lmFloodLightIntensity;

			/* add floodlight */
			floodlight.value += lmFloodLightRGB * floodLightAmount;
			floodlight.scale += floodlightDirectionScale;
		}
	}

	/* testing no filtering */
	return;

#if 0

	/* filter "dirt" */
	for ( y = 0; y < lm->sh; y++ )
	{
		for ( x = 0; x < lm->sw; x++ )
		{
			/* get luxel */
			SuperFloodLight& floodlight = lm->getSuperFloodLight( x, y );

			/* filter dirt by adjacency to unmapped luxels */
			average = floodlight.value[0];
			samples = 1.0f;
			for ( sy = ( y - 1 ); sy <= ( y + 1 ); sy++ )
			{
				if ( sy < 0 || sy >= lm->sh ) {
					continue;
				}

				for ( sx = ( x - 1 ); sx <= ( x + 1 ); sx++ )
				{
					if ( sx < 0 || sx >= lm->sw || ( sx == x && sy == y ) ) {
						continue;
					}

					/* get neighboring luxel */
					const SuperFloodLight& floodlight2 = lm->getSuperFloodLight( sx, sy );
					if ( lm->getSuperCluster( sx, sy ) < 0 || floodlight2.value[0] <= 0.0f ) {
						continue;
					}

					/* add it */
					average += floodlight2.value[0];
					samples += 1.0f;
				}

				/* bail */
				if ( samples <= 0.0f ) {
					break;
				}
			}

			/* bail */
			if ( samples <= 0.0f ) {
				continue;
			}

			/* scale dirt */
			floodlight.value[0] = average / samples;
		}
	}
#endif
}

static int numSurfacesFloodlighten;

static void FloodLightRawLightmap( int rawLightmapNum ){
	rawLightmap_t       *lm;

	/* bail if this number exceeds the number of raw lightmaps */
	if ( rawLightmapNum >= numRawLightmaps ) {
		return;
	}
	/* get lightmap */
	lm = &rawLightmaps[ rawLightmapNum ];

	/* global pass */
	if ( floodlighty && floodlightIntensity ) {
		FloodLightRawLightmapPass( lm, floodlightRGB, floodlightIntensity, floodlightDistance, floodlight_lowquality, floodlightDirectionScale );
	}

	/* custom pass */
	if ( lm->floodlightIntensity ) {
		FloodLightRawLightmapPass( lm, lm->floodlightRGB, lm->floodlightIntensity, lm->floodlightDistance, false, lm->floodlightDirectionScale );
		numSurfacesFloodlighten += 1;
	}
}

void FloodlightRawLightmaps(){
	Sys_Printf( "--- FloodlightRawLightmap ---\n" );
	numSurfacesFloodlighten = 0;
	RunThreadsOnIndividual( numRawLightmaps, true, FloodLightRawLightmap );
	Sys_Printf( "%9d custom lightmaps floodlighted\n", numSurfacesFloodlighten );
}

/*
   FloodLightIlluminate()
   illuminate floodlight into lightmap luxels
 */

static void FloodlightIlluminateLightmap( rawLightmap_t *lm ){
	int x, y, lightmapNum;

	/* walk lightmaps */
	for ( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
	{
		/* early out */
		if ( lm->superLuxels[ lightmapNum ] == NULL ) {
			continue;
		}

		if( lm->styles[lightmapNum] != LS_NORMAL && lm->styles[lightmapNum] != LS_NONE ) // isStyleLight
			continue;

		/* apply floodlight to each luxel */
		for ( y = 0; y < lm->sh; y++ )
		{
			for ( x = 0; x < lm->sw; x++ )
			{
				/* get floodlight */
				const SuperFloodLight& floodlight = lm->getSuperFloodLight( x, y );
				if ( floodlight.value == g_vector3_identity ) {
					continue;
				}

				/* only process mapped luxels */
				if ( lm->getSuperCluster( x, y ) < 0 ) {
					continue;
				}

				/* get particulars */
				SuperLuxel& luxel = lm->getSuperLuxel( lightmapNum, x, y );

				/* add to lightmap */
				luxel.value += floodlight.value;

				if ( luxel.count == 0 ) {
					luxel.count = 1;
				}

				/* add to deluxemap */
				if ( deluxemap && floodlight.scale > 0 ) {
					// use AT LEAST this amount of contribution from ambient for the deluxemap, fixes points that receive ZERO light
					const float brightness = std::max( 0.00390625f, RGBTOGRAY( floodlight.value ) * ( 1.0f / 255.0f ) * floodlight.scale );

					const Vector3 lightvector = lm->getSuperNormal( x, y ) * brightness;
					lm->getSuperDeluxel( x, y ) += lightvector;
				}
			}
		}
	}
}

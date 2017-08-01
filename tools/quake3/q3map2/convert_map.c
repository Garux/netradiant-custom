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



/* marker */
#define CONVERT_MAP_C



/* dependencies */
#include "q3map2.h"



/*
   ConvertBrush()
   exports a map brush
 */

#define SNAP_FLOAT_TO_INT   4
#define SNAP_INT_TO_FLOAT   ( 1.0 / SNAP_FLOAT_TO_INT )

typedef vec_t vec2_t[2];

static vec_t Det3x3( vec_t a00, vec_t a01, vec_t a02,
					 vec_t a10, vec_t a11, vec_t a12,
					 vec_t a20, vec_t a21, vec_t a22 ){
	return
		a00 * ( a11 * a22 - a12 * a21 )
		-   a01 * ( a10 * a22 - a12 * a20 )
		+   a02 * ( a10 * a21 - a11 * a20 );
}

void GetBestSurfaceTriangleMatchForBrushside( side_t *buildSide, bspDrawVert_t *bestVert[3] ){
	bspDrawSurface_t *s;
	int i;
	int t;
	vec_t best = 0;
	vec_t thisarea;
	vec3_t normdiff;
	vec3_t v1v0, v2v0, norm;
	bspDrawVert_t *vert[3];
	winding_t *polygon;
	plane_t *buildPlane = &mapplanes[buildSide->planenum];
	int matches = 0;

	// first, start out with NULLs
	bestVert[0] = bestVert[1] = bestVert[2] = NULL;

	// brute force through all surfaces
	for ( s = bspDrawSurfaces; s != bspDrawSurfaces + numBSPDrawSurfaces; ++s )
	{
		if ( s->surfaceType != MST_PLANAR && s->surfaceType != MST_TRIANGLE_SOUP ) {
			continue;
		}
		if ( strcmp( buildSide->shaderInfo->shader, bspShaders[s->shaderNum].shader ) ) {
			continue;
		}
		for ( t = 0; t + 3 <= s->numIndexes; t += 3 )
		{
			vert[0] = &bspDrawVerts[s->firstVert + bspDrawIndexes[s->firstIndex + t + 0]];
			vert[1] = &bspDrawVerts[s->firstVert + bspDrawIndexes[s->firstIndex + t + 1]];
			vert[2] = &bspDrawVerts[s->firstVert + bspDrawIndexes[s->firstIndex + t + 2]];
			if ( s->surfaceType == MST_PLANAR && VectorCompare( vert[0]->normal, vert[1]->normal ) && VectorCompare( vert[1]->normal, vert[2]->normal ) ) {
				VectorSubtract( vert[0]->normal, buildPlane->normal, normdiff );
				if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
				VectorSubtract( vert[1]->normal, buildPlane->normal, normdiff );
				if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
				VectorSubtract( vert[2]->normal, buildPlane->normal, normdiff );
				if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
			}
			else
			{
				// this is more prone to roundoff errors, but with embedded
				// models, there is no better way
				VectorSubtract( vert[1]->xyz, vert[0]->xyz, v1v0 );
				VectorSubtract( vert[2]->xyz, vert[0]->xyz, v2v0 );
				CrossProduct( v2v0, v1v0, norm );
				VectorNormalize( norm, norm );
				VectorSubtract( norm, buildPlane->normal, normdiff );
				if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
			}
			if ( abs( DotProduct( vert[0]->xyz, buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			if ( abs( DotProduct( vert[1]->xyz, buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			if ( abs( DotProduct( vert[2]->xyz, buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			// Okay. Correct surface type, correct shader, correct plane. Let's start with the business...
			polygon = CopyWinding( buildSide->winding );
			for ( i = 0; i < 3; ++i )
			{
				// 0: 1, 2
				// 1: 2, 0
				// 2; 0, 1
				vec3_t *v1 = &vert[( i + 1 ) % 3]->xyz;
				vec3_t *v2 = &vert[( i + 2 ) % 3]->xyz;
				vec3_t triNormal;
				vec_t triDist;
				vec3_t sideDirection;
				// we now need to generate triNormal and triDist so that they represent the plane spanned by normal and (v2 - v1).
				VectorSubtract( *v2, *v1, sideDirection );
				CrossProduct( sideDirection, buildPlane->normal, triNormal );
				triDist = DotProduct( *v1, triNormal );
				ChopWindingInPlace( &polygon, triNormal, triDist, distanceEpsilon );
				if ( !polygon ) {
					goto exwinding;
				}
			}
			thisarea = WindingArea( polygon );
			if ( thisarea > 0 ) {
				++matches;
			}
			if ( thisarea > best ) {
				best = thisarea;
				bestVert[0] = vert[0];
				bestVert[1] = vert[1];
				bestVert[2] = vert[2];
			}
			FreeWinding( polygon );
exwinding:
			;
		}
	}
	//if(strncmp(buildSide->shaderInfo->shader, "textures/common/", 16))
	//	fprintf(stderr, "brushside with %s: %d matches (%f area)\n", buildSide->shaderInfo->shader, matches, best);
}

#define FRAC( x ) ( ( x ) - floor( x ) )
static void ConvertOriginBrush( FILE *f, int num, vec3_t origin, qboolean brushPrimitives ){
	int originSize = 256;

	char pattern[6][7][3] = {
		{ "+++", "+-+", "-++", "-  ", " + ", " - ", "-  " },
		{ "+++", "-++", "++-", "-  ", "  +", "+  ", "  +" },
		{ "+++", "++-", "+-+", " - ", "  +", " - ", "  +" },
		{ "---", "+--", "-+-", "-  ", " + ", " - ", "+  " },
		{ "---", "--+", "+--", "-  ", "  +", "-  ", "  +" },
		{ "---", "-+-", "--+", " - ", "  +", " + ", "  +" }
	};
	int i;
#define S( a,b,c ) ( pattern[a][b][c] == '+' ? +1 : pattern[a][b][c] == '-' ? -1 : 0 )

	/* start brush */
	fprintf( f, "\t// brush %d\n", num );
	fprintf( f, "\t{\n" );
	if ( brushPrimitives ) {
		fprintf( f, "\tbrushDef\n" );
		fprintf( f, "\t{\n" );
	}
	/* print brush side */
	/* ( 640 24 -224 ) ( 448 24 -224 ) ( 448 -232 -224 ) common/caulk 0 48 0 0.500000 0.500000 0 0 0 */

	for ( i = 0; i < 6; ++i )
	{
		if ( brushPrimitives ) {
			fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( ( %.8f %.8f %.8f ) ( %.8f %.8f %.8f ) ) %s %d 0 0\n",
					 origin[0] + 8 * S( i,0,0 ), origin[1] + 8 * S( i,0,1 ), origin[2] + 8 * S( i,0,2 ),
					 origin[0] + 8 * S( i,1,0 ), origin[1] + 8 * S( i,1,1 ), origin[2] + 8 * S( i,1,2 ),
					 origin[0] + 8 * S( i,2,0 ), origin[1] + 8 * S( i,2,1 ), origin[2] + 8 * S( i,2,2 ),
					 1.0f / 16.0f, 0.0f, FRAC( ( S( i,5,0 ) * origin[0] + S( i,5,1 ) * origin[1] + S( i,5,2 ) * origin[2] ) / 16.0 + 0.5 ),
					 0.0f, 1.0f / 16.0f, FRAC( ( S( i,6,0 ) * origin[0] + S( i,6,1 ) * origin[1] + S( i,6,2 ) * origin[2] ) / 16.0 + 0.5 ),
					 "common/origin",
					 0
					 );
		}
		else
		{
			fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s %.8f %.8f %.8f %.8f %.8f %d 0 0\n",
					 origin[0] + 8 * S( i,0,0 ), origin[1] + 8 * S( i,0,1 ), origin[2] + 8 * S( i,0,2 ),
					 origin[0] + 8 * S( i,1,0 ), origin[1] + 8 * S( i,1,1 ), origin[2] + 8 * S( i,1,2 ),
					 origin[0] + 8 * S( i,2,0 ), origin[1] + 8 * S( i,2,1 ), origin[2] + 8 * S( i,2,2 ),
					 "common/origin",
					 FRAC( ( S( i,3,0 ) * origin[0] + S( i,3,1 ) * origin[1] + S( i,3,2 ) * origin[2] ) / 16.0 + 0.5 ) * originSize,
					 FRAC( ( S( i,4,0 ) * origin[0] + S( i,4,1 ) * origin[1] + S( i,4,2 ) * origin[2] ) / 16.0 + 0.5 ) * originSize,
					 0.0f, 16.0 / originSize, 16.0 / originSize,
					 0
					 );
		}
	}
#undef S

	/* end brush */
	if ( brushPrimitives ) {
		fprintf( f, "\t}\n" );
	}
	fprintf( f, "\t}\n\n" );
}

static void ConvertBrushFast( FILE *f, int num, bspBrush_t *brush, vec3_t origin, qboolean brushPrimitives ){
	int i;
	bspBrushSide_t  *side;
	side_t          *buildSide;
	bspShader_t     *shader;
	char            *texture;
	plane_t         *buildPlane;
	vec3_t pts[ 3 ];


	/* clear out build brush */
	for ( i = 0; i < buildBrush->numsides; i++ )
	{
		buildSide = &buildBrush->sides[ i ];
		if ( buildSide->winding != NULL ) {
			FreeWinding( buildSide->winding );
			buildSide->winding = NULL;
		}
	}
	buildBrush->numsides = 0;

	qboolean modelclip = qfalse;
	/* try to guess if thats model clip */
	if ( force ){
		int notNoShader = 0;
		modelclip = qtrue;
		for ( i = 0; i < brush->numSides; i++ )
		{
			/* get side */
			side = &bspBrushSides[ brush->firstSide + i ];

			/* get shader */
			if ( side->shaderNum < 0 || side->shaderNum >= numBSPShaders ) {
				continue;
			}
			shader = &bspShaders[ side->shaderNum ];
			//"noshader" happens on modelclip and unwanted sides ( usually breaking complex brushes )
			if( Q_stricmp( shader->shader, "noshader" ) ){
				notNoShader++;
			}
			if( notNoShader > 1 ){
				modelclip = qfalse;
				break;
			}
		}
	}

	/* iterate through bsp brush sides */
	for ( i = 0; i < brush->numSides; i++ )
	{
		/* get side */
		side = &bspBrushSides[ brush->firstSide + i ];

		/* get shader */
		if ( side->shaderNum < 0 || side->shaderNum >= numBSPShaders ) {
			continue;
		}
		shader = &bspShaders[ side->shaderNum ];
		//"noshader" happens on modelclip and unwanted sides ( usually breaking complex brushes )
		if( !Q_stricmp( shader->shader, "default" ) || ( !Q_stricmp( shader->shader, "noshader" ) && !modelclip ) )
			continue;

		/* add build side */
		buildSide = &buildBrush->sides[ buildBrush->numsides ];
		buildBrush->numsides++;

		/* tag it */
		buildSide->shaderInfo = ShaderInfoForShader( shader->shader );
		buildSide->planenum = side->planeNum;
		buildSide->winding = NULL;
	}

	if ( !CreateBrushWindings( buildBrush ) ) {
		//Sys_Printf( "CreateBrushWindings failed\n" );
		return;
	}

	/* start brush */
	fprintf( f, "\t// brush %d\n", num );
	fprintf( f, "\t{\n" );
	if ( brushPrimitives ) {
		fprintf( f, "\tbrushDef\n" );
		fprintf( f, "\t{\n" );
	}

	/* iterate through build brush sides */
	for ( i = 0; i < buildBrush->numsides; i++ )
	{
		/* get build side */
		buildSide = &buildBrush->sides[ i ];

		/* get plane */
		buildPlane = &mapplanes[ buildSide->planenum ];

		/* dummy check */
		if ( buildSide->shaderInfo == NULL || buildSide->winding == NULL ) {
			continue;
		}

		/* get texture name */
		if ( !Q_strncasecmp( buildSide->shaderInfo->shader, "textures/", 9 ) ) {
			texture = buildSide->shaderInfo->shader + 9;
		}
		else{
			texture = buildSide->shaderInfo->shader;
		}

		{
			vec3_t vecs[ 2 ];
			MakeNormalVectors( buildPlane->normal, vecs[ 0 ], vecs[ 1 ] );
			VectorMA( vec3_origin, buildPlane->dist, buildPlane->normal, pts[ 0 ] );
			VectorAdd( pts[ 0 ], origin, pts[ 0 ] );
			VectorMA( pts[ 0 ], 256.0f, vecs[ 0 ], pts[ 1 ] );
			VectorMA( pts[ 0 ], 256.0f, vecs[ 1 ], pts[ 2 ] );
		}

		{
			if ( brushPrimitives ) {
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( ( %.8f %.8f %.8f ) ( %.8f %.8f %.8f ) ) %s %d 0 0\n",
						 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
						 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
						 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
						 1.0f / 32.0f, 0.0f, 0.0f,
						 0.0f, 1.0f / 32.0f, 0.0f,
						 texture,
						 0
						 );
			}
			else
			{
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s %.8f %.8f %.8f %.8f %.8f %d 0 0\n",
						 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
						 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
						 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
						 texture,
						 0.0f, 0.0f, 0.0f, 0.5f, 0.5f,
						 0
						 );
			}
		}
	}

	/* end brush */
	if ( brushPrimitives ) {
		fprintf( f, "\t}\n" );
	}
	fprintf( f, "\t}\n\n" );
}

static void ConvertBrush( FILE *f, int num, bspBrush_t *brush, vec3_t origin, qboolean brushPrimitives ){
	int i, j;
	bspBrushSide_t  *side;
	side_t          *buildSide;
	bspShader_t     *shader;
	char            *texture;
	plane_t         *buildPlane;
	vec3_t pts[ 3 ];
	bspDrawVert_t   *vert[3];


	/* clear out build brush */
	for ( i = 0; i < buildBrush->numsides; i++ )
	{
		buildSide = &buildBrush->sides[ i ];
		if ( buildSide->winding != NULL ) {
			FreeWinding( buildSide->winding );
			buildSide->winding = NULL;
		}
	}
	buildBrush->numsides = 0;

	qboolean modelclip = qfalse;
	/* try to guess if thats model clip */
	if ( force ){
		int notNoShader = 0;
		modelclip = qtrue;
		for ( i = 0; i < brush->numSides; i++ )
		{
			/* get side */
			side = &bspBrushSides[ brush->firstSide + i ];

			/* get shader */
			if ( side->shaderNum < 0 || side->shaderNum >= numBSPShaders ) {
				continue;
			}
			shader = &bspShaders[ side->shaderNum ];
			//"noshader" happens on modelclip and unwanted sides ( usually breaking complex brushes )
			if( Q_stricmp( shader->shader, "noshader" ) ){
				notNoShader++;
			}
			if( notNoShader > 1 ){
				modelclip = qfalse;
				break;
			}
		}
	}

	/* iterate through bsp brush sides */
	for ( i = 0; i < brush->numSides; i++ )
	{
		/* get side */
		side = &bspBrushSides[ brush->firstSide + i ];

		/* get shader */
		if ( side->shaderNum < 0 || side->shaderNum >= numBSPShaders ) {
			continue;
		}
		shader = &bspShaders[ side->shaderNum ];
		//"noshader" happens on modelclip and unwanted sides ( usually breaking complex brushes )
		if( !Q_stricmp( shader->shader, "default" ) || ( !Q_stricmp( shader->shader, "noshader" ) && !modelclip ) )
			continue;

		/* add build side */
		buildSide = &buildBrush->sides[ buildBrush->numsides ];
		buildBrush->numsides++;

		/* tag it */
		buildSide->shaderInfo = ShaderInfoForShader( shader->shader );
		buildSide->planenum = side->planeNum;
		buildSide->winding = NULL;
	}

	/* make brush windings */
	if ( !CreateBrushWindings( buildBrush ) ) {
		//Sys_Printf( "CreateBrushWindings failed\n" );
		return;
	}

	/* start brush */
	fprintf( f, "\t// brush %d\n", num );
	fprintf( f, "\t{\n" );
	if ( brushPrimitives ) {
		fprintf( f, "\tbrushDef\n" );
		fprintf( f, "\t{\n" );
	}

	/* iterate through build brush sides */
	for ( i = 0; i < buildBrush->numsides; i++ )
	{
		/* get build side */
		buildSide = &buildBrush->sides[ i ];

		/* get plane */
		buildPlane = &mapplanes[ buildSide->planenum ];

		/* dummy check */
		if ( buildSide->shaderInfo == NULL || buildSide->winding == NULL ) {
			continue;
		}

		// st-texcoords -> texMat block
		// start out with dummy
		VectorSet( buildSide->texMat[0], 1 / 32.0, 0, 0 );
		VectorSet( buildSide->texMat[1], 0, 1 / 32.0, 0 );

		// find surface for this side (by brute force)
		// surface format:
		//   - meshverts point in pairs of three into verts
		//   - (triangles)
		//   - find the triangle that has most in common with our side
		GetBestSurfaceTriangleMatchForBrushside( buildSide, vert );

		/* get texture name */
		if ( !Q_strncasecmp( buildSide->shaderInfo->shader, "textures/", 9 ) ) {
			texture = buildSide->shaderInfo->shader + 9;
		}
		else{
			texture = buildSide->shaderInfo->shader;
		}

		/* recheck and fix winding points, fails occur somehow */
		int match = 0;
		for ( j = 0; j < buildSide->winding->numpoints; j++ ){
			if ( fabs( DotProduct( buildSide->winding->p[ j ], buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			else{
				VectorCopy( buildSide->winding->p[ j ], pts[ match ] );
				match++;
				/* got 3 fine points? */
				if( match > 2 )
					break;
			}
		}

		if( match > 2 ){
			//Sys_Printf( "pointsKK " );
			vec4_t testplane;
			if ( PlaneFromPoints( testplane, pts[0], pts[1], pts[2] ) ){
				if( !PlaneEqual( buildPlane, testplane, testplane[3] ) ){
					//Sys_Printf( "1: %f %f %f %f\n2: %f %f %f %f\n", buildPlane->normal[0], buildPlane->normal[1], buildPlane->normal[2], buildPlane->dist, testplane[0], testplane[1], testplane[2], testplane[3] );
					match--;
					//Sys_Printf( "planentEQ " );
				}
			}
			else{
				match--;
			}
		}


		if( match > 2 ){
			//Sys_Printf( "ok " );
			/* offset by origin */
			for ( j = 0; j < 3; j++ )
				VectorAdd( pts[ j ], origin, pts[ j ] );
		}
		else{
			vec3_t vecs[ 2 ];
			MakeNormalVectors( buildPlane->normal, vecs[ 0 ], vecs[ 1 ] );
			VectorMA( vec3_origin, buildPlane->dist, buildPlane->normal, pts[ 0 ] );
			VectorAdd( pts[ 0 ], origin, pts[ 0 ] );
			VectorMA( pts[ 0 ], 256.0f, vecs[ 0 ], pts[ 1 ] );
			VectorMA( pts[ 0 ], 256.0f, vecs[ 1 ], pts[ 2 ] );
			//Sys_Printf( "not\n" );
		}

		if ( vert[0] && vert[1] && vert[2] ) {
			if ( brushPrimitives ) {
				int i;
				vec3_t texX, texY;
				vec2_t xyI, xyJ, xyK;
				vec2_t stI, stJ, stK;
				vec_t D, D0, D1, D2;

				ComputeAxisBase( buildPlane->normal, texX, texY );

				xyI[0] = DotProduct( vert[0]->xyz, texX );
				xyI[1] = DotProduct( vert[0]->xyz, texY );
				xyJ[0] = DotProduct( vert[1]->xyz, texX );
				xyJ[1] = DotProduct( vert[1]->xyz, texY );
				xyK[0] = DotProduct( vert[2]->xyz, texX );
				xyK[1] = DotProduct( vert[2]->xyz, texY );
				stI[0] = vert[0]->st[0]; stI[1] = vert[0]->st[1];
				stJ[0] = vert[1]->st[0]; stJ[1] = vert[1]->st[1];
				stK[0] = vert[2]->st[0]; stK[1] = vert[2]->st[1];

				//   - solve linear equations:
				//     - (x, y) := xyz . (texX, texY)
				//     - st[i] = texMat[i][0]*x + texMat[i][1]*y + texMat[i][2]
				//       (for three vertices)
				D = Det3x3(
					xyI[0], xyI[1], 1,
					xyJ[0], xyJ[1], 1,
					xyK[0], xyK[1], 1
					);
				if ( D != 0 ) {
					for ( i = 0; i < 2; ++i )
					{
						D0 = Det3x3(
							stI[i], xyI[1], 1,
							stJ[i], xyJ[1], 1,
							stK[i], xyK[1], 1
							);
						D1 = Det3x3(
							xyI[0], stI[i], 1,
							xyJ[0], stJ[i], 1,
							xyK[0], stK[i], 1
							);
						D2 = Det3x3(
							xyI[0], xyI[1], stI[i],
							xyJ[0], xyJ[1], stJ[i],
							xyK[0], xyK[1], stK[i]
							);
						VectorSet( buildSide->texMat[i], D0 / D, D1 / D, D2 / D );
					}
				}
				else{
					fprintf( stderr, "degenerate triangle found when solving texMat equations for\n(%f %f %f) (%f %f %f) (%f %f %f)\n( %f %f %f )\n( %f %f %f ) -> ( %f %f )\n( %f %f %f ) -> ( %f %f )\n( %f %f %f ) -> ( %f %f )\n",
							 buildPlane->normal[0], buildPlane->normal[1], buildPlane->normal[2],
							 vert[0]->normal[0], vert[0]->normal[1], vert[0]->normal[2],
							 texX[0], texX[1], texX[2], texY[0], texY[1], texY[2],
							 vert[0]->xyz[0], vert[0]->xyz[1], vert[0]->xyz[2], xyI[0], xyI[1],
							 vert[1]->xyz[0], vert[1]->xyz[1], vert[1]->xyz[2], xyJ[0], xyJ[1],
							 vert[2]->xyz[0], vert[2]->xyz[1], vert[2]->xyz[2], xyK[0], xyK[1]
							 );
				}

				/* print brush side */
				/* ( 640 24 -224 ) ( 448 24 -224 ) ( 448 -232 -224 ) common/caulk 0 48 0 0.500000 0.500000 0 0 0 */
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( ( %.8f %.8f %.8f ) ( %.8f %.8f %.8f ) ) %s %d 0 0\n",
						 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
						 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
						 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
						 buildSide->texMat[0][0], buildSide->texMat[0][1], FRAC( buildSide->texMat[0][2] ),
						 buildSide->texMat[1][0], buildSide->texMat[1][1], FRAC( buildSide->texMat[1][2] ),
						 texture,
						 0
						 );
			}
			else
			{
				// invert QuakeTextureVecs
				int i;
				vec3_t vecs[2];
				int sv, tv;
				vec2_t stI, stJ, stK;
				vec3_t sts[2];
				vec2_t shift, scale;
				vec_t rotate;
				vec_t D, D0, D1, D2;

				TextureAxisFromPlane( buildPlane, vecs[0], vecs[1] );
				if ( vecs[0][0] ) {
					sv = 0;
				}
				else if ( vecs[0][1] ) {
					sv = 1;
				}
				else{
					sv = 2;
				}
				if ( vecs[1][0] ) {
					tv = 0;
				}
				else if ( vecs[1][1] ) {
					tv = 1;
				}
				else{
					tv = 2;
				}

				stI[0] = vert[0]->st[0] * buildSide->shaderInfo->shaderWidth; stI[1] = vert[0]->st[1] * buildSide->shaderInfo->shaderHeight;
				stJ[0] = vert[1]->st[0] * buildSide->shaderInfo->shaderWidth; stJ[1] = vert[1]->st[1] * buildSide->shaderInfo->shaderHeight;
				stK[0] = vert[2]->st[0] * buildSide->shaderInfo->shaderWidth; stK[1] = vert[2]->st[1] * buildSide->shaderInfo->shaderHeight;

				D = Det3x3(
					vert[0]->xyz[sv], vert[0]->xyz[tv], 1,
					vert[1]->xyz[sv], vert[1]->xyz[tv], 1,
					vert[2]->xyz[sv], vert[2]->xyz[tv], 1
					);
				if ( D != 0 ) {
					for ( i = 0; i < 2; ++i )
					{
						D0 = Det3x3(
							stI[i], vert[0]->xyz[tv], 1,
							stJ[i], vert[1]->xyz[tv], 1,
							stK[i], vert[2]->xyz[tv], 1
							);
						D1 = Det3x3(
							vert[0]->xyz[sv], stI[i], 1,
							vert[1]->xyz[sv], stJ[i], 1,
							vert[2]->xyz[sv], stK[i], 1
							);
						D2 = Det3x3(
							vert[0]->xyz[sv], vert[0]->xyz[tv], stI[i],
							vert[1]->xyz[sv], vert[1]->xyz[tv], stJ[i],
							vert[2]->xyz[sv], vert[2]->xyz[tv], stK[i]
							);
						VectorSet( sts[i], D0 / D, D1 / D, D2 / D );
					}
				}
				else{
					fprintf( stderr, "degenerate triangle found when solving texDef equations\n" ); // FIXME add stuff here

				}
				// now we must solve:
				//	// now we must invert:
				//	ang = rotate / 180 * Q_PI;
				//	sinv = sin(ang);
				//	cosv = cos(ang);
				//	ns = cosv * vecs[0][sv];
				//	nt = sinv * vecs[0][sv];
				//	vecsrotscaled[0][sv] = ns / scale[0];
				//	vecsrotscaled[0][tv] = nt / scale[0];
				//	ns = -sinv * vecs[1][tv];
				//	nt =  cosv * vecs[1][tv];
				//	vecsrotscaled[1][sv] = ns / scale[1];
				//	vecsrotscaled[1][tv] = nt / scale[1];
				scale[0] = 1.0 / sqrt( sts[0][0] * sts[0][0] + sts[0][1] * sts[0][1] );
				scale[1] = 1.0 / sqrt( sts[1][0] * sts[1][0] + sts[1][1] * sts[1][1] );
				rotate = atan2( sts[0][1] * vecs[0][sv] - sts[1][0] * vecs[1][tv], sts[0][0] * vecs[0][sv] + sts[1][1] * vecs[1][tv] ) * ( 180.0f / Q_PI );
				shift[0] = buildSide->shaderInfo->shaderWidth * FRAC( sts[0][2] / buildSide->shaderInfo->shaderWidth );
				shift[1] = buildSide->shaderInfo->shaderHeight * FRAC( sts[1][2] / buildSide->shaderInfo->shaderHeight );

				/* print brush side */
				/* ( 640 24 -224 ) ( 448 24 -224 ) ( 448 -232 -224 ) common/caulk 0 48 0 0.500000 0.500000 0 0 0 */
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s %.8f %.8f %.8f %.8f %.8f %d 0 0\n",
						 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
						 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
						 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
						 texture,
						 shift[0], shift[1], rotate, scale[0], scale[1],
						 0
						 );
			}
		}
		else
		{
			//vec3_t vecs[ 2 ];
			if ( strncmp( buildSide->shaderInfo->shader, "textures/common/", 16 ) ) {
				if ( strcmp( buildSide->shaderInfo->shader, "noshader" ) ) {
					if ( strcmp( buildSide->shaderInfo->shader, "default" ) ) {
						//fprintf( stderr, "no matching triangle for brushside using %s (hopefully nobody can see this side anyway)\n", buildSide->shaderInfo->shader );
						texture = "common/WTF";
					}
				}
			}
/*
			MakeNormalVectors( buildPlane->normal, vecs[ 0 ], vecs[ 1 ] );
			VectorMA( vec3_origin, buildPlane->dist, buildPlane->normal, pts[ 0 ] );
			VectorMA( pts[ 0 ], 256.0f, vecs[ 1 ], pts[ 2 ] );
			VectorMA( pts[ 0 ], 256.0f, vecs[ 0 ], pts[ 1 ] );
*/
			if ( brushPrimitives ) {
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( ( %.8f %.8f %.8f ) ( %.8f %.8f %.8f ) ) %s %d 0 0\n",
						 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
						 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
						 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
						 1.0f / 16.0f, 0.0f, 0.0f,
						 0.0f, 1.0f / 16.0f, 0.0f,
						 texture,
						 0
						 );
			}
			else
			{
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s %.8f %.8f %.8f %.8f %.8f %d 0 0\n",
						 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
						 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
						 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
						 texture,
						 0.0f, 0.0f, 0.0f, 0.25f, 0.25f,
						 0
						 );
			}
		}
	}

	/* end brush */
	if ( brushPrimitives ) {
		fprintf( f, "\t}\n" );
	}
	fprintf( f, "\t}\n\n" );
}
#undef FRAC

#if 0
/* iterate through the brush sides (ignore the first 6 bevel planes) */
for ( i = 0; i < brush->numSides; i++ )
{
	/* get side */
	side = &bspBrushSides[ brush->firstSide + i ];

	/* get shader */
	if ( side->shaderNum < 0 || side->shaderNum >= numBSPShaders ) {
		continue;
	}
	shader = &bspShaders[ side->shaderNum ];
	if ( !Q_stricmp( shader->shader, "default" ) || !Q_stricmp( shader->shader, "noshader" ) ) {
		continue;
	}

	/* get texture name */
	if ( !Q_strncasecmp( shader->shader, "textures/", 9 ) ) {
		texture = shader->shader + 9;
	}
	else{
		texture = shader->shader;
	}

	/* get plane */
	plane = &bspPlanes[ side->planeNum ];

	/* make plane points */
	{
		vec3_t vecs[ 2 ];


		MakeNormalVectors( plane->normal, vecs[ 0 ], vecs[ 1 ] );
		VectorMA( vec3_origin, plane->dist, plane->normal, pts[ 0 ] );
		VectorMA( pts[ 0 ], 256.0f, vecs[ 0 ], pts[ 1 ] );
		VectorMA( pts[ 0 ], 256.0f, vecs[ 1 ], pts[ 2 ] );
	}

	/* offset by origin */
	for ( j = 0; j < 3; j++ )
		VectorAdd( pts[ j ], origin, pts[ j ] );

	/* print brush side */
	/* ( 640 24 -224 ) ( 448 24 -224 ) ( 448 -232 -224 ) common/caulk 0 48 0 0.500000 0.500000 0 0 0 */
	fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s 0 0 0 0.5 0.5 0 0 0\n",
			 pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
			 pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
			 pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
			 texture );
}
#endif



/*
   ConvertPatch()
   converts a bsp patch to a map patch

    {
        patchDef2
        {
            base_wall/concrete
            ( 9 3 0 0 0 )
            (
                ( ( 168 168 -192 0 2 ) ( 168 168 -64 0 1 ) ( 168 168 64 0 0 ) ... )
                ...
            )
        }
    }

 */

static void ConvertPatch( FILE *f, int num, bspDrawSurface_t *ds, vec3_t origin ){
	int x, y;
	bspShader_t     *shader;
	char            *texture;
	bspDrawVert_t   *dv;
	vec3_t xyz;


	/* only patches */
	if ( ds->surfaceType != MST_PATCH ) {
		return;
	}

	/* get shader */
	if ( ds->shaderNum < 0 || ds->shaderNum >= numBSPShaders ) {
		return;
	}
	shader = &bspShaders[ ds->shaderNum ];

	/* get texture name */
	if ( !Q_strncasecmp( shader->shader, "textures/", 9 ) ) {
		texture = shader->shader + 9;
	}
	else{
		texture = shader->shader;
	}

	/* start patch */
	fprintf( f, "\t// patch %d\n", num );
	fprintf( f, "\t{\n" );
	fprintf( f, "\t\tpatchDef2\n" );
	fprintf( f, "\t\t{\n" );
	fprintf( f, "\t\t\t%s\n", texture );
	fprintf( f, "\t\t\t( %d %d 0 0 0 )\n", ds->patchWidth, ds->patchHeight );
	fprintf( f, "\t\t\t(\n" );

	/* iterate through the verts */
	for ( x = 0; x < ds->patchWidth; x++ )
	{
		/* start row */
		fprintf( f, "\t\t\t\t(" );

		/* iterate through the row */
		for ( y = 0; y < ds->patchHeight; y++ )
		{
			/* get vert */
			dv = &bspDrawVerts[ ds->firstVert + ( y * ds->patchWidth ) + x ];

			/* offset it */
			VectorAdd( origin, dv->xyz, xyz );

			/* print vertex */
			fprintf( f, " ( %f %f %f %f %f )", xyz[ 0 ], xyz[ 1 ], xyz[ 2 ], dv->st[ 0 ], dv->st[ 1 ] );
		}

		/* end row */
		fprintf( f, " )\n" );
	}

	/* end patch */
	fprintf( f, "\t\t\t)\n" );
	fprintf( f, "\t\t}\n" );
	fprintf( f, "\t}\n\n" );
}



/*
   ConvertModel()
   exports a bsp model to a map file
 */

static void ConvertModel( FILE *f, bspModel_t *model, int modelNum, vec3_t origin, qboolean brushPrimitives ){
	int i, num;
	bspBrush_t          *brush;
	bspDrawSurface_t    *ds;


	/* convert bsp planes to map planes */
	nummapplanes = numBSPPlanes;
	AUTOEXPAND_BY_REALLOC( mapplanes, nummapplanes, allocatedmapplanes, 1024 );
	for ( i = 0; i < numBSPPlanes; i++ )
	{
		VectorCopy( bspPlanes[ i ].normal, mapplanes[ i ].normal );
		mapplanes[ i ].dist = bspPlanes[ i ].dist;
		mapplanes[ i ].type = PlaneTypeForNormal( mapplanes[ i ].normal );
		mapplanes[ i ].hash_chain = 0;
	}

	/* allocate a build brush */
	buildBrush = AllocBrush( 512 );
	buildBrush->entityNum = 0;
	buildBrush->original = buildBrush;

	if ( origin[0] != 0 || origin[1] != 0 || origin[2] != 0 ) {
		ConvertOriginBrush( f, -1, origin, brushPrimitives );
	}

	/* go through each brush in the model */
	for ( i = 0; i < model->numBSPBrushes; i++ )
	{
		num = i + model->firstBSPBrush;
		brush = &bspBrushes[ num ];
		if( fast ){
			ConvertBrushFast( f, num, brush, origin, brushPrimitives );
		}
		else{
			ConvertBrush( f, num, brush, origin, brushPrimitives );
		}
	}

	/* free the build brush */
	free( buildBrush );

	/* go through each drawsurf in the model */
	for ( i = 0; i < model->numBSPSurfaces; i++ )
	{
		num = i + model->firstBSPSurface;
		ds = &bspDrawSurfaces[ num ];

		/* we only love patches */
		if ( ds->surfaceType == MST_PATCH ) {
			ConvertPatch( f, num, ds, origin );
		}
	}
}



/*
   ConvertEPairs()
   exports entity key/value pairs to a map file
 */

static void ConvertEPairs( FILE *f, entity_t *e, qboolean skip_origin ){
	epair_t *ep;


	/* walk epairs */
	for ( ep = e->epairs; ep != NULL; ep = ep->next )
	{
		/* ignore empty keys/values */
		if ( ep->key[ 0 ] == '\0' || ep->value[ 0 ] == '\0' ) {
			continue;
		}

		/* ignore model keys with * prefixed values */
		if ( !Q_stricmp( ep->key, "model" ) && ep->value[ 0 ] == '*' ) {
			continue;
		}

		/* ignore origin keys if skip_origin is set */
		if ( skip_origin && !Q_stricmp( ep->key, "origin" ) ) {
			continue;
		}

		/* emit the epair */
		fprintf( f, "\t\"%s\" \"%s\"\n", ep->key, ep->value );
	}
}



/*
   ConvertBSPToMap()
   exports an quake map file from the bsp
 */

int ConvertBSPToMap_Ext( char *bspName, qboolean brushPrimitives ){
	int i, modelNum;
	FILE            *f;
	bspModel_t      *model;
	entity_t        *e;
	vec3_t origin;
	const char      *value;
	char name[ 1024 ], base[ 1024 ];


	/* note it */
	Sys_Printf( "--- Convert BSP to MAP ---\n" );

	/* create the bsp filename from the bsp name */
	strcpy( name, bspName );
	StripExtension( name );
	strcat( name, "_converted.map" );
	Sys_Printf( "writing %s\n", name );

	ExtractFileBase( bspName, base );
	strcat( base, ".bsp" );

	/* open it */
	f = fopen( name, "wb" );
	if ( f == NULL ) {
		Error( "Open failed on %s\n", name );
	}

	/* print header */
	fprintf( f, "// Generated by Q3Map2 (ydnar) -convert -format map\n" );

	/* walk entity list */
	for ( i = 0; i < numEntities; i++ )
	{
		/* get entity */
		e = &entities[ i ];

		/* start entity */
		fprintf( f, "// entity %d\n", i );
		fprintf( f, "{\n" );

		/* get model num */
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			value = ValueForKey( e, "model" );
			if ( value[ 0 ] == '*' ) {
				modelNum = atoi( value + 1 );
			}
			else{
				modelNum = -1;
			}
		}

		/* export keys */
		ConvertEPairs( f, e, modelNum >= 0 );
		fprintf( f, "\n" );

		/* only handle bsp models */
		if ( modelNum >= 0 ) {
			/* get model */
			model = &bspModels[ modelNum ];

			/* get entity origin */
			value = ValueForKey( e, "origin" );
			if ( value[ 0 ] == '\0' ) {
				VectorClear( origin );
			}
			else{
				GetVectorForKey( e, "origin", origin );
			}

			/* convert model */
			ConvertModel( f, model, modelNum, origin, brushPrimitives );
		}

		/* end entity */
		fprintf( f, "}\n\n" );
	}

	/* close the file and return */
	fclose( f );

	/* return to sender */
	return 0;
}

int ConvertBSPToMap( char *bspName ){
	return ConvertBSPToMap_Ext( bspName, qfalse );
}

int ConvertBSPToMap_BP( char *bspName ){
	return ConvertBSPToMap_Ext( bspName, qtrue );
}

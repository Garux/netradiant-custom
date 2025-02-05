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
   ConvertBrush()
   exports a map brush
 */

inline float Det3x3( float a00, float a01, float a02,
                     float a10, float a11, float a12,
                     float a20, float a21, float a22 ){
	return
	        a00 * ( a11 * a22 - a12 * a21 )
	    -   a01 * ( a10 * a22 - a12 * a20 )
	    +   a02 * ( a10 * a21 - a11 * a20 );
}

static TriRef GetBestSurfaceTriangleMatchForBrushside( const side_t& buildSide ){
	float best = 0;
	float thisarea;
	const plane_t& buildPlane = mapplanes[buildSide.planenum];
	int matches = 0;

	// first, start out with NULLs
	TriRef bestVert{ nullptr };

	// brute force through all surfaces
	for ( const bspDrawSurface_t& s : bspDrawSurfaces )
	{
		if ( s.surfaceType != MST_PLANAR && s.surfaceType != MST_TRIANGLE_SOUP ) {
			continue;
		}
		if ( !strEqual( buildSide.shaderInfo->shader, bspShaders[s.shaderNum].shader ) ) {
			continue;
		}
		for ( int t = 0; t + 3 <= s.numIndexes; t += 3 )
		{
			const TriRef vert{
				&bspDrawVerts[s.firstVert + bspDrawIndexes[s.firstIndex + t + 0]],
				&bspDrawVerts[s.firstVert + bspDrawIndexes[s.firstIndex + t + 1]],
				&bspDrawVerts[s.firstVert + bspDrawIndexes[s.firstIndex + t + 2]]
			};
			if ( s.surfaceType == MST_PLANAR && VectorCompare( vert[0]->normal, vert[1]->normal ) && VectorCompare( vert[1]->normal, vert[2]->normal ) ) {
				if ( vector3_length( vert[0]->normal - buildPlane.normal() ) >= normalEpsilon
				  || vector3_length( vert[1]->normal - buildPlane.normal() ) >= normalEpsilon
				  || vector3_length( vert[2]->normal - buildPlane.normal() ) >= normalEpsilon ) {
					continue;
				}
			}
			else
			{
				// this is more prone to roundoff errors, but with embedded
				// models, there is no better way
				Plane3f plane;
				PlaneFromPoints( plane, vert[0]->xyz, vert[1]->xyz, vert[2]->xyz );
				if ( vector3_length( plane.normal() - buildPlane.normal() ) >= normalEpsilon ) {
					continue;
				}
			}
			// fixme? better distance epsilon
			if ( abs( plane3_distance_to_point( buildPlane.plane, vert[0]->xyz ) ) > 1
			  || abs( plane3_distance_to_point( buildPlane.plane, vert[1]->xyz ) ) > 1
			  || abs( plane3_distance_to_point( buildPlane.plane, vert[2]->xyz ) ) > 1 ) {
				continue;
			}
			// Okay. Correct surface type, correct shader, correct plane. Let's start with the business...
			winding_t polygon( buildSide.winding );
			for ( int i = 0; i < 3; ++i )
			{
				// 0: 1, 2
				// 1: 2, 0
				// 2; 0, 1
				const Vector3& v1 = vert[( i + 1 ) % 3]->xyz;
				const Vector3& v2 = vert[( i + 2 ) % 3]->xyz;
				// we now need to generate the plane spanned by normal and (v2 - v1).
				Plane3f plane( vector3_cross( v2 - v1, buildPlane.normal() ), 0 );
				plane.dist() = vector3_dot( v1, plane.normal() );
				ChopWindingInPlace( polygon, plane, distanceEpsilon );
				if ( polygon.empty() ) {
					goto exwinding;
				}
			}
			thisarea = WindingArea( polygon );
			if ( thisarea > 0 ) {
				++matches;
			}
			if ( thisarea > best ) {
				best = thisarea;
				bestVert = vert;
			}
exwinding:
			;
		}
	}
	//if( !striEqualPrefix( buildSide.shaderInfo->shader, "textures/common/" ) )
	//	fprintf( stderr, "brushside with %s: %d matches (%f area)\n", buildSide.shaderInfo->shader, matches, best );
	return bestVert;
}

#define FRAC( x ) ( ( x ) - floor( x ) )
static void ConvertOriginBrush( FILE *f, int num, const Vector3& origin, bool brushPrimitives ){
	int originSize = 256;

	char pattern[6][7][4] = {
		{ "+++", "+-+", "-++", "-  ", " + ", " - ", "-  " },
		{ "+++", "-++", "++-", "-  ", "  +", "+  ", "  +" },
		{ "+++", "++-", "+-+", " - ", "  +", " - ", "  +" },
		{ "---", "+--", "-+-", "-  ", " + ", " - ", "+  " },
		{ "---", "--+", "+--", "-  ", "  +", "-  ", "  +" },
		{ "---", "-+-", "--+", " - ", "  +", " + ", "  +" }
	};
#define S( a, b, c ) ( pattern[a][b][c] == '+' ? +1 : pattern[a][b][c] == '-' ? -1 : 0 )

	/* start brush */
	fprintf( f, "\t// brush %d\n", num );
	fprintf( f, "\t{\n" );
	if ( brushPrimitives ) {
		fprintf( f, "\tbrushDef\n" );
		fprintf( f, "\t{\n" );
	}
	/* print brush side */
	/* ( 640 24 -224 ) ( 448 24 -224 ) ( 448 -232 -224 ) common/caulk 0 48 0 0.500000 0.500000 0 0 0 */

	for ( int i = 0; i < 6; ++i )
	{
		if ( brushPrimitives ) {
			fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( ( %.8f %.8f %.8f ) ( %.8f %.8f %.8f ) ) %s %d 0 0\n",
			         origin[0] + 8 * S( i, 0, 0 ), origin[1] + 8 * S( i, 0, 1 ), origin[2] + 8 * S( i, 0, 2 ),
			         origin[0] + 8 * S( i, 1, 0 ), origin[1] + 8 * S( i, 1, 1 ), origin[2] + 8 * S( i, 1, 2 ),
			         origin[0] + 8 * S( i, 2, 0 ), origin[1] + 8 * S( i, 2, 1 ), origin[2] + 8 * S( i, 2, 2 ),
			         1.0f / 16.0f, 0.0f, FRAC( ( S( i, 5, 0 ) * origin[0] + S( i, 5, 1 ) * origin[1] + S( i, 5, 2 ) * origin[2] ) / 16.0 + 0.5 ),
			         0.0f, 1.0f / 16.0f, FRAC( ( S( i, 6, 0 ) * origin[0] + S( i, 6, 1 ) * origin[1] + S( i, 6, 2 ) * origin[2] ) / 16.0 + 0.5 ),
			         "common/origin",
			         0
			       );
		}
		else
		{
			fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s %.8f %.8f %.8f %.8f %.8f %d 0 0\n",
			         origin[0] + 8 * S( i, 0, 0 ), origin[1] + 8 * S( i, 0, 1 ), origin[2] + 8 * S( i, 0, 2 ),
			         origin[0] + 8 * S( i, 1, 0 ), origin[1] + 8 * S( i, 1, 1 ), origin[2] + 8 * S( i, 1, 2 ),
			         origin[0] + 8 * S( i, 2, 0 ), origin[1] + 8 * S( i, 2, 1 ), origin[2] + 8 * S( i, 2, 2 ),
			         "common/origin",
			         FRAC( ( S( i, 3, 0 ) * origin[0] + S( i, 3, 1 ) * origin[1] + S( i, 3, 2 ) * origin[2] ) / 16.0 + 0.5 ) * originSize,
			         FRAC( ( S( i, 4, 0 ) * origin[0] + S( i, 4, 1 ) * origin[1] + S( i, 4, 2 ) * origin[2] ) / 16.0 + 0.5 ) * originSize,
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

static void bspBrush_to_buildBrush( const bspBrush_t& brush ){
	/* clear out build brush */
	buildBrush.sides.clear();

	bool modelclip = false;
	/* try to guess if thats model clip */
	if ( force ){
		int notNoShader = 0;
		modelclip = true;
		for ( int i = 0; i < brush.numSides; i++ )
		{
			/* get side */
			const bspBrushSide_t& side = bspBrushSides[ brush.firstSide + i ];

			/* get shader */
			if ( side.shaderNum < 0 || side.shaderNum >= int( bspShaders.size() ) ) {
				continue;
			}
			const bspShader_t& shader = bspShaders[ side.shaderNum ];
			//"noshader" happens on modelclip and unwanted sides ( usually breaking complex brushes )
			if( !striEqual( shader.shader, "noshader" ) ){
				notNoShader++;
			}
			if( notNoShader > 1 ){
				modelclip = false;
				break;
			}
		}
	}

	/* iterate through bsp brush sides */
	for ( int i = 0; i < brush.numSides; i++ )
	{
		/* get side */
		const bspBrushSide_t& side = bspBrushSides[ brush.firstSide + i ];

		/* get shader */
		if ( side.shaderNum < 0 || side.shaderNum >= int( bspShaders.size() ) ) {
			continue;
		}
		const bspShader_t& shader = bspShaders[ side.shaderNum ];
		//"noshader" happens on modelclip and unwanted sides ( usually breaking complex brushes )
		if( striEqual( shader.shader, "default" ) || ( striEqual( shader.shader, "noshader" ) && !modelclip ) )
			continue;

		/* add build side */
		buildBrush.sides.emplace_back();

		/* tag it */
		buildBrush.sides.back().shaderInfo = ShaderInfoForShader( shader.shader );
		buildBrush.sides.back().planenum = side.planeNum;
	}
}

static void ConvertBrushFast( FILE *f, int bspBrushNum, const Vector3& origin, bool brushPrimitives ){

	bspBrush_to_buildBrush( bspBrushes[bspBrushNum] );

	if ( !CreateBrushWindings( buildBrush ) ) {
		//Sys_Printf( "CreateBrushWindings failed\n" );
		return;
	}

	/* start brush */
	fprintf( f, "\t// brush %d\n", bspBrushNum );
	fprintf( f, "\t{\n" );
	if ( brushPrimitives ) {
		fprintf( f, "\tbrushDef\n" );
		fprintf( f, "\t{\n" );
	}

	/* iterate through build brush sides */
	for ( side_t& buildSide : buildBrush.sides )
	{
		/* get plane */
		const plane_t& buildPlane = mapplanes[ buildSide.planenum ];

		/* dummy check */
		if ( buildSide.shaderInfo == NULL || buildSide.winding.empty() ) {
			continue;
		}

		/* get texture name */
		const char *texture = striEqualPrefix( buildSide.shaderInfo->shader, "textures/" )
		                      ? buildSide.shaderInfo->shader + 9
		                      : buildSide.shaderInfo->shader;

		Vector3 pts[ 3 ];
		{
			Vector3 vecs[ 2 ];
			MakeNormalVectors( buildPlane.normal(), vecs[ 0 ], vecs[ 1 ] );
			pts[ 0 ] = buildPlane.normal() * buildPlane.dist() + origin;
			pts[ 1 ] = pts[ 0 ] + vecs[ 0 ] * 256.0f;
			pts[ 2 ] = pts[ 0 ] + vecs[ 1 ] * 256.0f;
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

static void ConvertBrush( FILE *f, int bspBrushNum, const Vector3& origin, bool brushPrimitives ){

	bspBrush_to_buildBrush( bspBrushes[bspBrushNum] );

	/* make brush windings */
	if ( !CreateBrushWindings( buildBrush ) ) {
		//Sys_Printf( "CreateBrushWindings failed\n" );
		return;
	}

	/* start brush */
	fprintf( f, "\t// brush %d\n", bspBrushNum );
	fprintf( f, "\t{\n" );
	if ( brushPrimitives ) {
		fprintf( f, "\tbrushDef\n" );
		fprintf( f, "\t{\n" );
	}

	/* find out if brush is detail */
	int contentFlag = 0;
	if( !( bspShaders[bspBrushes[bspBrushNum].shaderNum].contentFlags & GetRequiredSurfaceParm<"structural">().contentFlags ) ){ // sort out structural transparent brushes, e.g. hints
		for( const auto& leaf : bspLeafs ){
			if( leaf.cluster >= 0 )
				for( auto id = bspLeafBrushes.cbegin() + leaf.firstBSPLeafBrush, end = id + leaf.numBSPLeafBrushes; id != end; ++id ){
					if( *id == bspBrushNum ){
						contentFlag = C_DETAIL;
						break;
					}
				}
			if( contentFlag == C_DETAIL)
				break;
		}
	}

	/* iterate through build brush sides */
	for ( side_t& buildSide : buildBrush.sides )
	{
		/* get plane */
		const plane_t& buildPlane = mapplanes[ buildSide.planenum ];

		/* dummy check */
		if ( buildSide.shaderInfo == NULL || buildSide.winding.empty() ) {
			continue;
		}

		// st-texcoords -> texMat block
		// start out with dummy
		buildSide.texMat[0] = { 1 / 32.0, 0, 0 };
		buildSide.texMat[1] = { 0, 1 / 32.0, 0 };

		// find surface for this side (by brute force)
		// surface format:
		//   - meshverts point in pairs of three into verts
		//   - (triangles)
		//   - find the triangle that has most in common with our
		const TriRef vert = GetBestSurfaceTriangleMatchForBrushside( buildSide );

		/* get texture name */
		const char *texture = striEqualPrefix( buildSide.shaderInfo->shader, "textures/" )
		                      ? buildSide.shaderInfo->shader + 9
		                      : buildSide.shaderInfo->shader;

		Vector3 pts[ 3 ];
		/* recheck and fix winding points, fails occur somehow */
		int match = 0;
		for ( const Vector3& p : buildSide.winding ){
			if ( fabs( plane3_distance_to_point( buildPlane.plane, p ) ) < distanceEpsilon ) {
				pts[ match ] = p;
				match++;
				/* got 3 fine points? */
				if( match > 2 )
					break;
			}
		}

		if( match > 2 ){
			//Sys_Printf( "pointsKK " );
			if ( Plane3f testplane; PlaneFromPoints( testplane, pts ) ){
				if( !PlaneEqual( buildPlane, testplane ) ){
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
			for ( Vector3& pt : pts )
				pt += origin;
		}
		else{
			Vector3 vecs[ 2 ];
			MakeNormalVectors( buildPlane.normal(), vecs[ 0 ], vecs[ 1 ] );
			pts[ 0 ] = buildPlane.normal() * buildPlane.dist() + origin;
			pts[ 1 ] = pts[ 0 ] + vecs[ 0 ] * 256.0f;
			pts[ 2 ] = pts[ 0 ] + vecs[ 1 ] * 256.0f;
			//Sys_Printf( "not\n" );
		}

		if ( vert[0] != nullptr && vert[1] != nullptr && vert[2] != nullptr ) {
			if ( brushPrimitives ) {
				int i;
				Vector3 texX, texY;
				Vector2 xyI, xyJ, xyK;
				Vector2 stI, stJ, stK;
				float D, D0, D1, D2;

				ComputeAxisBase( buildPlane.normal(), texX, texY );

				xyI[0] = vector3_dot( vert[0]->xyz, texX );
				xyI[1] = vector3_dot( vert[0]->xyz, texY );
				xyJ[0] = vector3_dot( vert[1]->xyz, texX );
				xyJ[1] = vector3_dot( vert[1]->xyz, texY );
				xyK[0] = vector3_dot( vert[2]->xyz, texX );
				xyK[1] = vector3_dot( vert[2]->xyz, texY );
				stI = vert[0]->st;
				stJ = vert[1]->st;
				stK = vert[2]->st;

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
						buildSide.texMat[i] = { D0 / D, D1 / D, D2 / D };
					}
				}
				else{
					fprintf( stderr, "degenerate triangle found when solving texMat equations for\n(%f %f %f) (%f %f %f) (%f %f %f)\n( %f %f %f )\n( %f %f %f ) -> ( %f %f )\n( %f %f %f ) -> ( %f %f )\n( %f %f %f ) -> ( %f %f )\n",
					         buildPlane.normal()[0], buildPlane.normal()[1], buildPlane.normal()[2],
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
				         buildSide.texMat[0][0], buildSide.texMat[0][1], FRAC( buildSide.texMat[0][2] ),
				         buildSide.texMat[1][0], buildSide.texMat[1][1], FRAC( buildSide.texMat[1][2] ),
				         texture,
				         contentFlag
				       );
			}
			else
			{
				// invert QuakeTextureVecs
				int i;
				int sv, tv;
				float stI[2], stJ[2], stK[2];
				Vector3 sts[2];
				float shift[2], scale[2];
				float rotate;
				float D, D0, D1, D2;

				const auto vecs = TextureAxisFromPlane( buildPlane );
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

				stI[0] = vert[0]->st[0] * buildSide.shaderInfo->shaderWidth;
				stI[1] = vert[0]->st[1] * buildSide.shaderInfo->shaderHeight;
				stJ[0] = vert[1]->st[0] * buildSide.shaderInfo->shaderWidth;
				stJ[1] = vert[1]->st[1] * buildSide.shaderInfo->shaderHeight;
				stK[0] = vert[2]->st[0] * buildSide.shaderInfo->shaderWidth;
				stK[1] = vert[2]->st[1] * buildSide.shaderInfo->shaderHeight;

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
						sts[i] = { D0 / D, D1 / D, D2 / D };
						//Sys_Printf( "%.3f %.3f %.3f \n", sts[i][0], sts[i][1], sts[i][2] );
					}
				}
				else{
					fprintf( stderr, "degenerate triangle found when solving texDef equations\n" ); // FIXME add stuff here
					sts[0] = { 2.f, 0.f, 0.f };
					sts[1] = { 0.f, -2.f, 0.f };
				}
				// now we must solve:
				//	// now we must invert:
				//	ang = degrees_to_radians( rotate );
				//	sinv = sin( ang );
				//	cosv = cos( ang );
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
				rotate = radians_to_degrees( atan2( sts[0][1] * vecs[0][sv] - sts[1][0] * vecs[1][tv], sts[0][0] * vecs[0][sv] + sts[1][1] * vecs[1][tv] ) );
				shift[0] = buildSide.shaderInfo->shaderWidth * FRAC( sts[0][2] / buildSide.shaderInfo->shaderWidth );
				shift[1] = buildSide.shaderInfo->shaderHeight * FRAC( sts[1][2] / buildSide.shaderInfo->shaderHeight );

				/* print brush side */
				/* ( 640 24 -224 ) ( 448 24 -224 ) ( 448 -232 -224 ) common/caulk 0 48 0 0.500000 0.500000 0 0 0 */
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) %s %.8f %.8f %.8f %.8f %.8f %d 0 0\n",
				         pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
				         pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
				         pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
				         texture,
				         shift[0], shift[1], rotate, scale[0], scale[1],
				         contentFlag
				       );
			}
		}
		else
		{
			if ( !striEqualPrefix( buildSide.shaderInfo->shader, "textures/common/" )
			  && !striEqualPrefix( buildSide.shaderInfo->shader, "textures/system/" )
			  &&        !strEqual( buildSide.shaderInfo->shader, "noshader" )
			  &&        !strEqual( buildSide.shaderInfo->shader, "default" ) ) {
				//fprintf( stderr, "no matching triangle for brushside using %s (hopefully nobody can see this side anyway)\n", buildSide.shaderInfo->shader );
				texture = "common/WTF";
			}

			if ( brushPrimitives ) {
				fprintf( f, "\t\t( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( %.3f %.3f %.3f ) ( ( %.8f %.8f %.8f ) ( %.8f %.8f %.8f ) ) %s %d 0 0\n",
				         pts[ 0 ][ 0 ], pts[ 0 ][ 1 ], pts[ 0 ][ 2 ],
				         pts[ 1 ][ 0 ], pts[ 1 ][ 1 ], pts[ 1 ][ 2 ],
				         pts[ 2 ][ 0 ], pts[ 2 ][ 1 ], pts[ 2 ][ 2 ],
				         1.0f / 16.0f, 0.0f, 0.0f,
				         0.0f, 1.0f / 16.0f, 0.0f,
				         texture,
				         contentFlag
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
				         contentFlag
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
	if ( side->shaderNum < 0 || side->shaderNum >= int( bspShaders.size() ) ) {
		continue;
	}
	shader = &bspShaders[ side->shaderNum ];
	if ( striEqual( shader->shader, "default" ) || striEqual( shader->shader, "noshader" ) ) {
		continue;
	}

	/* get texture name */
	if ( striEqualPrefix( shader->shader, "textures/" ) ) {
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

static void ConvertPatch( FILE *f, int num, const bspDrawSurface_t& ds, const Vector3& origin ){
	/* only patches */
	if ( ds.surfaceType != MST_PATCH ) {
		return;
	}

	/* get shader */
	if ( ds.shaderNum < 0 || ds.shaderNum >= int( bspShaders.size() ) ) {
		return;
	}

	/* get texture name */
	const char      *texture;
	if ( const bspShader_t& shader = bspShaders[ ds.shaderNum ];
		striEqualPrefix( shader.shader, "textures/" ) ) {
		texture = shader.shader + 9;
	}
	else{
		texture = shader.shader;
	}

	/* start patch */
	fprintf( f, "\t// patch %d\n", num );
	fprintf( f, "\t{\n" );
	fprintf( f, "\t\tpatchDef2\n" );
	fprintf( f, "\t\t{\n" );
	fprintf( f, "\t\t\t%s\n", texture );
	fprintf( f, "\t\t\t( %d %d 0 0 0 )\n", ds.patchWidth, ds.patchHeight );
	fprintf( f, "\t\t\t(\n" );

	/* iterate through the verts */
	for ( int x = 0; x < ds.patchWidth; x++ )
	{
		/* start row */
		fprintf( f, "\t\t\t\t(" );

		/* iterate through the row */
		for ( int y = 0; y < ds.patchHeight; y++ )
		{
			/* get vert */
			const bspDrawVert_t& dv = bspDrawVerts[ ds.firstVert + ( y * ds.patchWidth ) + x ];

			/* offset it */
			const Vector3 xyz = dv.xyz + origin;

			/* print vertex */
			fprintf( f, " ( %f %f %f %f %f )", xyz[ 0 ], xyz[ 1 ], xyz[ 2 ], dv.st[ 0 ], dv.st[ 1 ] );
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

static void ConvertModel( FILE *f, const bspModel_t& model, const Vector3& origin, bool brushPrimitives ){
	if ( origin != g_vector3_identity ) {
		ConvertOriginBrush( f, -1, origin, brushPrimitives );
	}

	/* go through each brush in the model */
	for ( int i = 0; i < model.numBSPBrushes; i++ )
	{
		if( fast )
			ConvertBrushFast( f, model.firstBSPBrush + i, origin, brushPrimitives );
		else
			ConvertBrush( f, model.firstBSPBrush + i, origin, brushPrimitives );
	}

	/* go through each drawsurf in the model */
	for ( int i = 0; i < model.numBSPSurfaces; i++ )
	{
		const int num = i + model.firstBSPSurface;
		const bspDrawSurface_t& ds = bspDrawSurfaces[ num ];

		/* we only love patches */
		if ( ds.surfaceType == MST_PATCH ) {
			ConvertPatch( f, num, ds, origin );
		}
	}
}



/*
   ConvertEPairs()
   exports entity key/value pairs to a map file
 */

static void ConvertEPairs( FILE *f, const entity_t& e, bool skip_origin ){
	/* walk epairs */
	for ( const auto& ep : e.epairs )
	{
		/* ignore empty keys/values */
		if ( ep.key.empty() || ep.value.empty() ) {
			continue;
		}

		/* ignore model keys with * prefixed values */
		if ( striEqual( ep.key.c_str(), "model" ) && ep.value.c_str()[ 0 ] == '*' ) {
			continue;
		}

		/* ignore origin keys if skip_origin is set */
		if ( skip_origin && striEqual( ep.key.c_str(), "origin" ) ) {
			continue;
		}

		/* emit the epair */
		fprintf( f, "\t\"%s\" \"%s\"\n", ep.key.c_str(), ep.value.c_str() );
	}
}



/*
   ConvertBSPToMap()
   exports an quake map file from the bsp
 */

static int ConvertBSPToMap_Ext( char *bspName, bool brushPrimitives ){
	/* setup brush conversion prerequisites */
	{
		/* convert bsp planes to map planes */
		mapplanes.resize( bspPlanes.size() );
		for ( size_t i = 0; i < bspPlanes.size(); ++i )
		{
			plane_t& plane = mapplanes[i];
			plane.plane = bspPlanes[ i ];
			plane.type = PlaneTypeForNormal( plane.normal() );
			plane.hash_chain = 0;
		}

		/* allocate a build brush */
		buildBrush.sides.reserve( MAX_BUILD_SIDES );
		buildBrush.entityNum = 0;
		buildBrush.original = &buildBrush;
	}

	if( g_game->load == LoadRBSPFile )
		UnSetLightStyles();

	/* note it */
	Sys_Printf( "--- Convert BSP to MAP ---\n" );

	/* create map filename from the bsp name */
	const auto name = StringStream( PathExtensionless( bspName ), "_converted.map" );
	Sys_Printf( "writing %s\n", name.c_str() );

	/* open it */
	FILE *f = SafeOpenWrite( name );

	/* print header */
	fprintf( f, "// Generated by Q3Map2 (ydnar) -convert -format map\n" );

	/* walk entity list */
	for ( std::size_t i = 0; i < entities.size(); ++i )
	{
		/* get entity */
		const entity_t& e = entities[ i ];

		/* start entity */
		fprintf( f, "// entity %zu\n", i );
		fprintf( f, "{\n" );

		/* get model num */
		int modelNum;
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			const char *value = e.valueForKey( "model" );
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
			/* convert model */
			ConvertModel( f, bspModels[ modelNum ], e.vectorForKey( "origin" ), brushPrimitives );
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
	return ConvertBSPToMap_Ext( bspName, false );
}

int ConvertBSPToMap_BP( char *bspName ){
	return ConvertBSPToMap_Ext( bspName, true );
}

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



/*
   LerpDrawVert()
   returns an 50/50 interpolated vert
 */

void LerpDrawVert( const bspDrawVert_t *a, const bspDrawVert_t *b, bspDrawVert_t *out ){

	out->xyz = vector3_mid( a->xyz, b->xyz );
	out->st = vector2_mid( a->st, b->st );

	for ( int k = 0; k < MAX_LIGHTMAPS; k++ )
	{
		out->lightmap[ k ] = vector2_mid( a->lightmap[ k ], b->lightmap[ k ] );
		for( int i = 0; i < 4; ++i )
			out->color[ k ][ i ] = ( a->color[ k ][ i ] + b->color[ k ][ i ] ) >> 1;
	}

	/* ydnar: added normal interpolation */
	out->normal = vector3_mid( a->normal, b->normal );

	/* if the interpolant created a bogus normal, just copy the normal from a */
	if ( VectorNormalize( out->normal ) == 0 ) {
		out->normal = a->normal;
	}
}



/*
   LerpDrawVertAmount()
   returns a biased interpolated vert
 */

void LerpDrawVertAmount( bspDrawVert_t *a, bspDrawVert_t *b, float amount, bspDrawVert_t *out ){

	out->xyz = a->xyz + ( b->xyz - a->xyz ) * amount;

	out->st = a->st + ( b->st - a->st ) * amount;

	for ( int k = 0; k < MAX_LIGHTMAPS; k++ )
	{
		out->lightmap[ k ] = a->lightmap[ k ] + ( b->lightmap[ k ] - a->lightmap[ k ] ) * amount;
		for( int i = 0; i < 4; ++i )
			out->color[ k ][ i ] = a->color[ k ][ i ] + amount * ( b->color[ k ][ i ] - a->color[ k ][ i ] );
	}

	out->normal = a->normal + ( b->normal - a->normal ) * amount;

	/* if the interpolant created a bogus normal, just copy the normal from a */
	if ( VectorNormalize( out->normal ) == 0 ) {
		out->normal = a->normal;
	}
}


void FreeMesh( mesh_t *m ) {
	free( m->verts );
	free( m );
}

void PrintMesh( mesh_t *m ) {
	int i, j;

	for ( i = 0 ; i < m->height ; i++ ) {
		for ( j = 0 ; j < m->width ; j++ ) {
			Sys_Printf( "(%5.2f %5.2f %5.2f) "
			            , m->verts[i * m->width + j].xyz[0]
			            , m->verts[i * m->width + j].xyz[1]
			            , m->verts[i * m->width + j].xyz[2] );
		}
		Sys_Printf( "\n" );
	}
}


mesh_t *CopyMesh( mesh_t *mesh ) {
	mesh_t  *out;
	int size;

	out = safe_malloc( sizeof( *out ) );
	out->width = mesh->width;
	out->height = mesh->height;

	size = out->width * out->height * sizeof( *out->verts );
	out->verts = safe_malloc( size );
	memcpy( out->verts, mesh->verts, size );

	return out;
}


/*
   TransposeMesh()
   returns a transposed copy of the mesh, freeing the original
 */

mesh_t *TransposeMesh( mesh_t *in ) {
	int w, h;
	mesh_t      *out;

	out = safe_malloc( sizeof( *out ) );
	out->width = in->height;
	out->height = in->width;
	out->verts = safe_malloc( out->width * out->height * sizeof( bspDrawVert_t ) );

	for ( h = 0 ; h < in->height ; h++ ) {
		for ( w = 0 ; w < in->width ; w++ ) {
			out->verts[ w * in->height + h ] = in->verts[ h * in->width + w ];
		}
	}

	FreeMesh( in );

	return out;
}

void InvertMesh( mesh_t *in ) {
	int w, h;
	bspDrawVert_t temp;

	for ( h = 0 ; h < in->height ; h++ ) {
		for ( w = 0 ; w < in->width / 2 ; w++ ) {
			temp = in->verts[ h * in->width + w ];
			in->verts[ h * in->width + w ] = in->verts[ h * in->width + in->width - 1 - w ];
			in->verts[ h * in->width + in->width - 1 - w ] = temp;
		}
	}
}

/*
   =================
   MakeMeshNormals

   =================
 */
void MakeMeshNormals( mesh_t in ){
	int i, j, k, dist;
	int count;
	int x, y;
	bspDrawVert_t   *dv;
	Vector3 around[8];
	bool good[8];
	bool wrapWidth, wrapHeight;
	int neighbors[8][2] =
	{
		{0,1}, {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}
	};


	wrapWidth = false;
	for ( i = 0 ; i < in.height ; i++ ) {
		if ( vector3_length( in.verts[i * in.width].xyz - in.verts[i * in.width + in.width - 1].xyz ) > 1.0 ) {
			break;
		}
	}
	if ( i == in.height ) {
		wrapWidth = true;
	}

	wrapHeight = false;
	for ( i = 0 ; i < in.width ; i++ ) {
		if ( vector3_length( in.verts[i].xyz - in.verts[i + ( in.height - 1 ) * in.width].xyz ) > 1.0 ) {
			break;
		}
	}
	if ( i == in.width ) {
		wrapHeight = true;
	}


	for ( i = 0 ; i < in.width ; i++ ) {
		for ( j = 0 ; j < in.height ; j++ ) {
			count = 0;
			dv = &in.verts[j * in.width + i];
			const Vector3 base( dv->xyz );
			for ( k = 0 ; k < 8 ; k++ ) {
				around[k].set( 0 );
				good[k] = false;

				for ( dist = 1 ; dist <= 3 ; dist++ ) {
					x = i + neighbors[k][0] * dist;
					y = j + neighbors[k][1] * dist;
					if ( wrapWidth ) {
						if ( x < 0 ) {
							x = in.width - 1 + x;
						}
						else if ( x >= in.width ) {
							x = 1 + x - in.width;
						}
					}
					if ( wrapHeight ) {
						if ( y < 0 ) {
							y = in.height - 1 + y;
						}
						else if ( y >= in.height ) {
							y = 1 + y - in.height;
						}
					}

					if ( x < 0 || x >= in.width || y < 0 || y >= in.height ) {
						break;                  // edge of patch
					}
					Vector3 temp = in.verts[y * in.width + x].xyz - base;
					if ( VectorNormalize( temp ) == 0 ) {
						continue;               // degenerate edge, get more dist
					}
					else {
						good[k] = true;
						around[k] = temp;
						break;                  // good edge
					}
				}
			}

			Vector3 sum( 0 );
			for ( k = 0 ; k < 8 ; k++ ) {
				if ( !good[k] || !good[( k + 1 ) & 7] ) {
					continue;   // didn't get two points
				}
				Vector3 normal = vector3_cross( around[( k + 1 ) & 7], around[k] );
				if ( VectorNormalize( normal ) == 0 ) {
					continue;
				}
				sum += normal;
				count++;
			}
			if ( count == 0 ) {
//Sys_Printf("bad normal\n");
				count = 1;
			}
			dv->normal = VectorNormalized( sum );
		}
	}
}

/*
   PutMeshOnCurve()
   drops the aproximating points onto the curve
   ydnar: fixme: make this use LerpDrawVert() rather than this complicated mess
 */

void PutMeshOnCurve( mesh_t in ) {
	int i, j, m;


	// put all the aproximating points on the curve
	for ( i = 0 ; i < in.width ; i++ ) {
		for ( j = 1 ; j < in.height ; j += 2 ) {
			const int idx = j * in.width + i;
			const int idprev = ( j + 1 ) * in.width + i;
			const int idnext = ( j - 1 ) * in.width + i;
			in.verts[idx].xyz = vector3_mid( vector3_mid( in.verts[idx].xyz, in.verts[idprev].xyz ),
			                                 vector3_mid( in.verts[idx].xyz, in.verts[idnext].xyz ) );
			/* ydnar: interpolating st coords */
			in.verts[idx].st = vector2_mid( vector2_mid( in.verts[idx].st, in.verts[idprev].st ),
			                                vector2_mid( in.verts[idx].st, in.verts[idnext].st ) );
			for ( m = 0; m < MAX_LIGHTMAPS; m++ )
			{
				in.verts[idx].lightmap[ m ] = vector2_mid( vector2_mid( in.verts[idx].lightmap[ m ], in.verts[idprev].lightmap[ m ] ),
				                                           vector2_mid( in.verts[idx].lightmap[ m ], in.verts[idnext].lightmap[ m ] ) );
			}
		}
	}

	for ( j = 0 ; j < in.height ; j++ ) {
		for ( i = 1 ; i < in.width ; i += 2 ) {
			const int idx = j * in.width + i;
			in.verts[idx].xyz = vector3_mid( vector3_mid( in.verts[idx].xyz, in.verts[idx + 1].xyz ),
			                                 vector3_mid( in.verts[idx].xyz, in.verts[idx - 1].xyz ) );
			/* ydnar: interpolating st coords */
			in.verts[idx].st = vector2_mid( vector2_mid( in.verts[idx].st, in.verts[idx + 1].st ),
			                                vector2_mid( in.verts[idx].st, in.verts[idx - 1].st ) );
			for ( m = 0; m < MAX_LIGHTMAPS; m++ )
			{
				in.verts[idx].lightmap[ m ] = vector2_mid( vector2_mid( in.verts[idx].lightmap[ m ], in.verts[idx + 1].lightmap[ m ] ),
				                                           vector2_mid( in.verts[idx].lightmap[ m ], in.verts[idx - 1].lightmap[ m ] ) );
			}
		}
	}
}


/*
   =================
   SubdivideMesh

   =================
 */
mesh_t *SubdivideMesh( mesh_t in, float maxError, float minLength ){
	int i, j, k;
	bspDrawVert_t prev, next, mid;
	Vector3 prevxyz, nextxyz, midxyz;
	mesh_t out;

	bspDrawVert_t expand[MAX_EXPANDED_AXIS][MAX_EXPANDED_AXIS];


	out.width = in.width;
	out.height = in.height;

	for ( i = 0 ; i < in.width ; i++ ) {
		for ( j = 0 ; j < in.height ; j++ ) {
			expand[j][i] = in.verts[j * in.width + i];
		}
	}

	// horizontal subdivisions
	for ( j = 0 ; j + 2 < out.width ; j += 2 ) {
		// check subdivided midpoints against control points
		for ( i = 0 ; i < out.height ; i++ ) {
			prevxyz = expand[i][j + 1].xyz - expand[i][j].xyz;
			nextxyz = expand[i][j + 2].xyz - expand[i][j + 1].xyz;
			midxyz = ( expand[i][j].xyz + expand[i][j + 1].xyz * 2 + expand[i][j + 2].xyz ) * 0.25;

			// if the span length is too long, force a subdivision
			if ( vector3_length( prevxyz ) > minLength
			  || vector3_length( nextxyz ) > minLength ) {
				break;
			}

			// see if this midpoint is off far enough to subdivide
			if ( vector3_length( expand[i][j + 1].xyz - midxyz ) > maxError ) {
				break;
			}
		}

		if ( out.width + 2 >= MAX_EXPANDED_AXIS ) {
			break;  // can't subdivide any more
		}

		if ( i == out.height ) {
			continue;   // didn't need subdivision
		}

		// insert two columns and replace the peak
		out.width += 2;

		for ( i = 0 ; i < out.height ; i++ ) {
			LerpDrawVert( &expand[i][j], &expand[i][j + 1], &prev );
			LerpDrawVert( &expand[i][j + 1], &expand[i][j + 2], &next );
			LerpDrawVert( &prev, &next, &mid );

			for ( k = out.width - 1 ; k > j + 3 ; k-- ) {
				expand[i][k] = expand[i][k - 2];
			}
			expand[i][j + 1] = prev;
			expand[i][j + 2] = mid;
			expand[i][j + 3] = next;
		}

		// back up and recheck this set again, it may need more subdivision
		j -= 2;

	}

	// vertical subdivisions
	for ( j = 0 ; j + 2 < out.height ; j += 2 ) {
		// check subdivided midpoints against control points
		for ( i = 0 ; i < out.width ; i++ ) {
			prevxyz = expand[j + 1][i].xyz - expand[j][i].xyz;
			nextxyz = expand[j + 2][i].xyz - expand[j + 1][i].xyz;
			midxyz = ( expand[j][i].xyz + expand[j + 1][i].xyz * 2 + expand[j + 2][i].xyz ) * 0.25;

			// if the span length is too long, force a subdivision
			if ( vector3_length( prevxyz ) > minLength
			  || vector3_length( nextxyz ) > minLength ) {
				break;
			}
			// see if this midpoint is off far enough to subdivide
			if ( vector3_length( expand[j + 1][i].xyz - midxyz ) > maxError ) {
				break;
			}
		}

		if ( out.height + 2 >= MAX_EXPANDED_AXIS ) {
			break;  // can't subdivide any more
		}

		if ( i == out.width ) {
			continue;   // didn't need subdivision
		}

		// insert two columns and replace the peak
		out.height += 2;

		for ( i = 0 ; i < out.width ; i++ ) {
			LerpDrawVert( &expand[j][i], &expand[j + 1][i], &prev );
			LerpDrawVert( &expand[j + 1][i], &expand[j + 2][i], &next );
			LerpDrawVert( &prev, &next, &mid );

			for ( k = out.height - 1 ; k > j + 3 ; k-- ) {
				expand[k][i] = expand[k - 2][i];
			}
			expand[j + 1][i] = prev;
			expand[j + 2][i] = mid;
			expand[j + 3][i] = next;
		}

		// back up and recheck this set again, it may need more subdivision
		j -= 2;

	}

	// collapse the verts

	out.verts = &expand[0][0];
	for ( i = 1 ; i < out.height ; i++ ) {
		memmove( &out.verts[i * out.width], expand[i], out.width * sizeof( bspDrawVert_t ) );
	}

	return CopyMesh( &out );
}



/*
   IterationsForCurve() - ydnar
   given a curve of a certain length, return the number of subdivision iterations
   note: this is affected by subdivision amount
 */

int IterationsForCurve( float len, int subdivisions ){
	int iterations, facets;


	/* calculate the number of subdivisions */
	for ( iterations = 0; iterations < 3; iterations++ )
	{
		facets = subdivisions * 16 * pow( 2, iterations );
		if ( facets >= len ) {
			break;
		}
	}

	/* return to caller */
	return iterations;
}


/*
   SubdivideMesh2() - ydnar
   subdivides each mesh quad a specified number of times
 */

mesh_t *SubdivideMesh2( mesh_t in, int iterations ){
	int i, j, k;
	bspDrawVert_t prev, next, mid;
	mesh_t out;

	bspDrawVert_t expand[ MAX_EXPANDED_AXIS ][ MAX_EXPANDED_AXIS ];


	/* initial setup */
	out.width = in.width;
	out.height = in.height;
	for ( i = 0; i < in.width; i++ )
	{
		for ( j = 0; j < in.height; j++ )
			expand[ j ][ i ] = in.verts[ j * in.width + i ];
	}

	/* keep chopping */
	for ( ; iterations > 0; iterations-- )
	{
		/* horizontal subdivisions */
		for ( j = 0; j + 2 < out.width; j += 4 )
		{
			/* check size limit */
			if ( out.width + 2 >= MAX_EXPANDED_AXIS ) {
				break;
			}

			/* insert two columns and replace the peak */
			out.width += 2;
			for ( i = 0; i < out.height; i++ )
			{
				LerpDrawVert( &expand[ i ][ j ], &expand[ i ][ j + 1 ], &prev );
				LerpDrawVert( &expand[ i ][ j + 1 ], &expand[ i ][ j + 2 ], &next );
				LerpDrawVert( &prev, &next, &mid );

				for ( k = out.width - 1 ; k > j + 3; k-- )
					expand [ i ][ k ] = expand[ i ][ k - 2 ];
				expand[ i ][ j + 1 ] = prev;
				expand[ i ][ j + 2 ] = mid;
				expand[ i ][ j + 3 ] = next;
			}

		}

		/* vertical subdivisions */
		for ( j = 0; j + 2 < out.height; j += 4 )
		{
			/* check size limit */
			if ( out.height + 2 >= MAX_EXPANDED_AXIS ) {
				break;
			}

			/* insert two columns and replace the peak */
			out.height += 2;
			for ( i = 0; i < out.width; i++ )
			{
				LerpDrawVert( &expand[ j ][ i ], &expand[ j + 1 ][ i ], &prev );
				LerpDrawVert( &expand[ j + 1 ][ i ], &expand[ j + 2 ][ i ], &next );
				LerpDrawVert( &prev, &next, &mid );

				for ( k = out.height - 1; k > j  +  3; k-- )
					expand[ k ][ i ] = expand[ k - 2 ][ i ];
				expand[ j + 1 ][ i ] = prev;
				expand[ j + 2 ][ i ] = mid;
				expand[ j + 3 ][ i ] = next;
			}
		}
	}

	/* collapse the verts */
	out.verts = &expand[ 0 ][ 0 ];
	for ( i = 1; i < out.height; i++ )
		memmove( &out.verts[ i * out.width ], expand[ i ], out.width * sizeof( bspDrawVert_t ) );

	/* return to sender */
	return CopyMesh( &out );
}







/*
   ================
   ProjectPointOntoVector
   ================
 */
Vector3 ProjectPointOntoVector( const Vector3& point, const Vector3& vStart, const Vector3& vEnd ){
	const Vector3 pVec = point - vStart;
	const Vector3 vec = VectorNormalized( vEnd - vStart );
	// project onto the directional vector for this segment
	return ( vStart + vec * vector3_dot( pVec, vec ) );
}

/*
   ================
   RemoveLinearMeshColumsRows
   ================
 */
mesh_t *RemoveLinearMeshColumnsRows( mesh_t *in ) {
	int i, j, k;
	mesh_t out;

	bspDrawVert_t expand[MAX_EXPANDED_AXIS][MAX_EXPANDED_AXIS];


	out.width = in->width;
	out.height = in->height;

	for ( i = 0 ; i < in->width ; i++ ) {
		for ( j = 0 ; j < in->height ; j++ ) {
			expand[j][i] = in->verts[j * in->width + i];
		}
	}

	for ( j = 1 ; j < out.width - 1; j++ ) {
		double maxLength = 0;
		for ( i = 0 ; i < out.height ; i++ ) {
			value_maximize( maxLength, vector3_length( expand[i][j].xyz - ProjectPointOntoVector( expand[i][j].xyz, expand[i][j - 1].xyz, expand[i][j + 1].xyz ) ) );
		}
		if ( maxLength < 0.1 ) {
			out.width--;
			for ( i = 0 ; i < out.height ; i++ ) {
				for ( k = j; k < out.width; k++ ) {
					expand[i][k] = expand[i][k + 1];
				}
			}
			j--;
		}
	}
	for ( j = 1 ; j < out.height - 1; j++ ) {
		double maxLength = 0;
		for ( i = 0 ; i < out.width ; i++ ) {
			value_maximize( maxLength, vector3_length( expand[j][i].xyz - ProjectPointOntoVector( expand[j][i].xyz, expand[j - 1][i].xyz, expand[j + 1][i].xyz ) ) );
		}
		if ( maxLength < 0.1 ) {
			out.height--;
			for ( i = 0 ; i < out.width ; i++ ) {
				for ( k = j; k < out.height; k++ ) {
					expand[k][i] = expand[k + 1][i];
				}
			}
			j--;
		}
	}
	// collapse the verts
	out.verts = &expand[0][0];
	for ( i = 1 ; i < out.height ; i++ ) {
		memmove( &out.verts[i * out.width], expand[i], out.width * sizeof( bspDrawVert_t ) );
	}

	return CopyMesh( &out );
}



/*
   =================
   SubdivideMeshQuads
   =================
 */
mesh_t *SubdivideMeshQuads( mesh_t *in, float minLength, int maxsize, int *widthtable, int *heighttable ){
	int i, j, k, w, h, maxsubdivisions, subdivisions;
	mesh_t out;
	bspDrawVert_t expand[MAX_EXPANDED_AXIS][MAX_EXPANDED_AXIS];

	out.width = in->width;
	out.height = in->height;

	for ( i = 0 ; i < in->width ; i++ ) {
		for ( j = 0 ; j < in->height ; j++ ) {
			expand[j][i] = in->verts[j * in->width + i];
		}
	}

	if ( maxsize > MAX_EXPANDED_AXIS ) {
		Error( "SubdivideMeshQuads: maxsize > MAX_EXPANDED_AXIS" );
	}

	// horizontal subdivisions

	maxsubdivisions = ( maxsize - in->width ) / ( in->width - 1 );

	for ( w = 0, j = 0 ; w < in->width - 1; w++, j += subdivisions + 1 ) {
		double maxLength = 0;
		for ( i = 0 ; i < out.height ; i++ ) {
			value_maximize( maxLength, vector3_length( expand[i][j + 1].xyz - expand[i][j].xyz ) );
		}

		subdivisions = std::min( (int) ( maxLength / minLength ), maxsubdivisions );

		widthtable[w] = subdivisions + 1;
		if ( subdivisions <= 0 ) {
			continue;
		}

		out.width += subdivisions;

		for ( i = 0 ; i < out.height ; i++ ) {
			for ( k = out.width - 1 ; k > j + subdivisions; k-- ) {
				expand[i][k] = expand[i][k - subdivisions];
			}
			for ( k = 1; k <= subdivisions; k++ )
			{
				const float amount = (float) k / ( subdivisions + 1 );
				LerpDrawVertAmount( &expand[i][j], &expand[i][j + subdivisions + 1], amount, &expand[i][j + k] );
			}
		}
	}

	maxsubdivisions = ( maxsize - in->height ) / ( in->height - 1 );

	for ( h = 0, j = 0 ; h < in->height - 1; h++, j += subdivisions + 1 ) {
		double maxLength = 0;
		for ( i = 0 ; i < out.width ; i++ ) {
			value_maximize( maxLength, vector3_length( expand[j + 1][i].xyz - expand[j][i].xyz ) );
		}

		subdivisions = std::min( (int) ( maxLength / minLength ), maxsubdivisions );

		heighttable[h] = subdivisions + 1;
		if ( subdivisions <= 0 ) {
			continue;
		}

		out.height += subdivisions;

		for ( i = 0 ; i < out.width ; i++ ) {
			for ( k = out.height - 1 ; k > j + subdivisions; k-- ) {
				expand[k][i] = expand[k - subdivisions][i];
			}
			for ( k = 1; k <= subdivisions; k++ )
			{
				const float amount = (float) k / ( subdivisions + 1 );
				LerpDrawVertAmount( &expand[j][i], &expand[j + subdivisions + 1][i], amount, &expand[j + k][i] );
			}
		}
	}

	// collapse the verts
	out.verts = &expand[0][0];
	for ( i = 1 ; i < out.height ; i++ ) {
		memmove( &out.verts[i * out.width], expand[i], out.width * sizeof( bspDrawVert_t ) );
	}

	return CopyMesh( &out );
}

/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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
 */

///\file
///\brief Represents any light entity (e.g. light).
///
/// This entity dislays a special 'light' model.
/// The "origin" key directly controls the position of the light model in local space.
/// The "_color" key controls the colour of the light model.
/// The "light" key is visualised with a sphere representing the approximate coverage of the light (except Doom3).
/// Doom3 special behaviour:
/// The entity behaves as a group.
/// The "origin" key is the translation to be applied to all brushes (not patches) grouped under this entity.
/// The "light_center" and "light_radius" keys are visualised with a point and a box when the light is selected.
/// The "rotation" key directly controls the orientation of the light bounding box in local space.
/// The "light_origin" key controls the position of the light independently of the "origin" key if it is specified.
/// The "light_rotation" key duplicates the behaviour of the "rotation" key if it is specified. This appears to be an unfinished feature in Doom3.

#include "light.h"

#include <stdlib.h>

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "math/frustum.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "eclasslib.h"
#include "render.h"
#include "stringio.h"
#include "traverselib.h"
#include "dragplanes.h"

#include "targetable.h"
#include "origin.h"
#include "colour.h"
#include "filters.h"
#include "namedentity.h"
#include "keyobservers.h"
#include "namekeys.h"
#include "rotation.h"

#include "entity.h"


#if 0 //old straight calculations + render
void sphere_draw_fill( const Vector3& origin, float radius, int sides ){
	if ( radius <= 0 ) {
		return;
	}

	const double dt = c_2pi / static_cast<double>( sides );
	const double dp = c_pi / static_cast<double>( sides );

	glBegin( GL_TRIANGLES );
	for ( int i = 0; i <= sides - 1; ++i )
	{
		for ( int j = 0; j <= sides - 2; ++j )
		{
			const double t = i * dt;
			const double p = ( j * dp ) - ( c_pi / 2.0 );

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t, p ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t, p + dp ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t + dt, p + dp ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t, p ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t + dt, p + dp ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t + dt, p ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}
		}
	}

	{
		const double p = ( sides - 1 ) * dp - ( c_pi / 2.0 );
		for ( int i = 0; i <= sides - 1; ++i )
		{
			const double t = i * dt;

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t, p ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t + dt, p + dp ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}

			{
				Vector3 v( vector3_added( origin, vector3_scaled( vector3_for_spherical( t + dt, p ), radius ) ) );
				glVertex3fv( vector3_to_array( v ) );
			}
		}
	}
	glEnd();
}

void light_draw_radius_fill( const Vector3& origin, const float envelope[3] ){
	if ( envelope[0] > 0 ) {
		sphere_draw_fill( origin, envelope[0], 16 );
	}
	if ( envelope[1] > 0 ) {
		sphere_draw_fill( origin, envelope[1], 16 );
	}
	if ( envelope[2] > 0 ) {
		sphere_draw_fill( origin, envelope[2], 16 );
	}
}
#elif 1 //precalculated sphere

	#if 1 //GL_TRIANGLE_STRIP

#define SPHERE_FILL_SIDES 12 //must be n*2

#if 0
	#define SPHERE_FILL_POINTS 1261
	#define SPHERE_FILL_STEP 10.f
#elif 0
	#define SPHERE_FILL_POINTS 241
	#define SPHERE_FILL_STEP 22.5f
#else
	//#define SPHERE_FILL_POINTS 553 // ( (12 - 1)*2 + 1)*2*12 + 1
	#define SPHERE_FILL_POINTS ((SPHERE_FILL_SIDES - 1)*2 + 1)*2*SPHERE_FILL_SIDES + 1
	#define SPHERE_FILL_STEP 15.f
#endif



void cartesian( const double Long, const double Lat, float cart[3] ) {
	cart[0] = cos( Long ) * fabs( cos( Lat ) );
	cart[1] = sin( Long ) * fabs( cos( Lat ) );
	cart[2] = sin( Lat );
}

void sphere_construct_fill( Vector3 radiiPoints[SPHERE_FILL_POINTS] ){
	float cart[3];
	int k = 0;
	const double step = c_pi / static_cast<double>( SPHERE_FILL_SIDES );

	radiiPoints[k++] = Vector3( 0.0, 0.0, -1.0 );
	for( int i = 0; i < SPHERE_FILL_SIDES * 2; i+=2 ) {
		for( int j = -SPHERE_FILL_SIDES / 2 + 1; j < SPHERE_FILL_SIDES / 2; ++j ) {
			cartesian( step * i, step * j, cart );
			radiiPoints[k++] = Vector3( cart[0], cart[1], cart[2] );
			cartesian( step * ( i + 1 ), step * j, cart );
			radiiPoints[k++] = Vector3( cart[0], cart[1], cart[2] );
		}
		radiiPoints[k++] = Vector3( 0.0, 0.0, 1.0 );
		for( int j = SPHERE_FILL_SIDES / 2 - 1; j > -SPHERE_FILL_SIDES / 2; --j ) {
			cartesian( step * ( i + 1 ), step * j, cart );
			radiiPoints[k++] = Vector3( cart[0], cart[1], cart[2] );
			cartesian( step * ( i + 2 ), step * j, cart );
			radiiPoints[k++] = Vector3( cart[0], cart[1], cart[2] );
		}
		radiiPoints[k++] = Vector3( 0.0, 0.0, -1.0 );
	}
	//globalOutputStream() << k << "!!!!!!!!!!!!\n";
}

void sphere_draw_fill( const Vector3& origin, float radius, const Vector3 radiiPoints[SPHERE_FILL_POINTS] ){
	glBegin( GL_TRIANGLE_STRIP );
	for ( int i = 0; i < SPHERE_FILL_POINTS; ++i )
	{
		glVertex3fv( vector3_to_array( vector3_added( origin, vector3_scaled( radiiPoints[i], radius ) ) ) );
	}
	glEnd();
}

	#elif 0 // triangles
		#if 1 // icosahedron

#define SPHERE_FILL_SIDES 16
#define SPHERE_FILL_DIV 2
#define SPHERE_FILL_POINTS 20 * ( 4 << SPHERE_FILL_DIV ) * 3

#define X .525731112119133606
#define Z .850650808352039932

static float vdata[12][3] = {
	{ -X, 0.0, Z}, {X, 0.0, Z}, { -X, 0.0, -Z}, {X, 0.0, -Z},
	{0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},
	{Z, X, 0.0}, { -Z, X, 0.0}, {Z, -X, 0.0}, { -Z, -X, 0.0}
};
static unsigned int tindices[20][3] = {
	{0, 4, 1}, {0, 9, 4}, {9, 5, 4}, {4, 5, 8}, {4, 8, 1},
	{8, 10, 1}, {8, 3, 10}, {5, 3, 8}, {5, 2, 3}, {2, 7, 3},
	{7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
	{6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5}, {7, 2, 11}
};

void normalize( float * a ) {
	float d = sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] );
	a[0] /= d;
	a[1] /= d;
	a[2] /= d;
}

void drawtri( float * a, float * b, float * c, int div, Vector3 radiiPoints[SPHERE_FILL_POINTS], int& k ) {
	if( div <= 0 ) {
		radiiPoints[k++] = Vector3( a[0], a[1], a[2] );
		radiiPoints[k++] = Vector3( b[0], b[1], b[2] );
		radiiPoints[k++] = Vector3( c[0], c[1], c[2] );
	} else {
		float ab[3], ac[3], bc[3];
		for( int i = 0; i < 3; i++ ) {
			ab[i] = ( a[i] + b[i] ) / 2;
			ac[i] = ( a[i] + c[i] ) / 2;
			bc[i] = ( b[i] + c[i] ) / 2;
		}
		normalize( ab );
		normalize( ac );
		normalize( bc );
		drawtri( a, ab, ac, div - 1, radiiPoints, k );
		drawtri( b, bc, ab, div - 1, radiiPoints, k );
		drawtri( c, ac, bc, div - 1, radiiPoints, k );
		drawtri( ab, bc, ac, div - 1, radiiPoints, k ); //<--Comment this line and sphere looks really cool!
	}
}

void sphere_construct_fill( Vector3 radiiPoints[SPHERE_FILL_POINTS] ){
	int k = 0;
	for( int i = 0; i < 20; i++ )
		drawtri( vdata[tindices[i][0]], vdata[tindices[i][1]], vdata[tindices[i][2]], SPHERE_FILL_DIV, radiiPoints, k );
}

		#elif 0 //old precalculated

#define SPHERE_FILL_SIDES 16
#define SPHERE_FILL_POINTS SPHERE_FILL_SIDES * (SPHERE_FILL_SIDES - 1) * 6 + SPHERE_FILL_SIDES * 3

void sphere_construct_fill( Vector3 radiiPoints[SPHERE_FILL_POINTS] ){

	const double dt = c_2pi / static_cast<double>( SPHERE_FILL_SIDES );
	const double dp = c_pi / static_cast<double>( SPHERE_FILL_SIDES );
	int k = 0;

	for ( int i = 0; i <= SPHERE_FILL_SIDES - 1; ++i )
	{
		for ( int j = 0; j <= SPHERE_FILL_SIDES - 2; ++j )
		{
			const double t = i * dt;
			const double p = ( j * dp ) - ( c_pi / 2.0 );

			radiiPoints[k++] = vector3_for_spherical( t, p );
			radiiPoints[k++] = vector3_for_spherical( t, p + dp );
			radiiPoints[k++] = vector3_for_spherical( t + dt, p + dp );
			radiiPoints[k++] = vector3_for_spherical( t, p );
			radiiPoints[k++] = vector3_for_spherical( t + dt, p + dp );
			radiiPoints[k++] = vector3_for_spherical( t + dt, p );
		}
	}

	{
		const double p = ( SPHERE_FILL_SIDES - 1 ) * dp - ( c_pi / 2.0 );
		for ( int i = 0; i <= SPHERE_FILL_SIDES - 1; ++i )
		{
			const double t = i * dt;

			radiiPoints[k++] = vector3_for_spherical( t, p );
			radiiPoints[k++] = vector3_for_spherical( t + dt, p + dp );
			radiiPoints[k++] = vector3_for_spherical( t + dt, p );
		}
	}
}

		#endif

void sphere_draw_fill( const Vector3& origin, float radius, const Vector3 radiiPoints[SPHERE_FILL_POINTS] ){
	glBegin( GL_TRIANGLES );
	for ( int i = 0; i < SPHERE_FILL_POINTS; ++i )
	{
		glVertex3fv( vector3_to_array( vector3_added( origin, vector3_scaled( radiiPoints[i], radius ) ) ) );
	}
	glEnd();
}

	#endif

void light_draw_radius_fill( const Vector3& origin, const float envelope[3], const Vector3 radiiPoints[SPHERE_FILL_POINTS] ){
	if ( envelope[0] > 0 ) {
		sphere_draw_fill( origin, envelope[0], radiiPoints );
	}
	if ( envelope[1] > 0 ) {
		sphere_draw_fill( origin, envelope[1], radiiPoints );
	}
	if ( envelope[2] > 0 ) {
		sphere_draw_fill( origin, envelope[2], radiiPoints );
	}
}
#endif


#if 0 //old straight calculations + render
void sphere_draw_wire( const Vector3& origin, float radius, int sides ){
	{
		glBegin( GL_LINE_LOOP );

		for ( int i = 0; i <= sides; i++ )
		{
			double ds = sin( ( i * 2 * c_pi ) / sides );
			double dc = cos( ( i * 2 * c_pi ) / sides );

			glVertex3f(
				static_cast<float>( origin[0] + radius * dc ),
				static_cast<float>( origin[1] + radius * ds ),
				origin[2]
				);
		}

		glEnd();
	}

	{
		glBegin( GL_LINE_LOOP );

		for ( int i = 0; i <= sides; i++ )
		{
			double ds = sin( ( i * 2 * c_pi ) / sides );
			double dc = cos( ( i * 2 * c_pi ) / sides );

			glVertex3f(
				static_cast<float>( origin[0] + radius * dc ),
				origin[1],
				static_cast<float>( origin[2] + radius * ds )
				);
		}

		glEnd();
	}

	{
		glBegin( GL_LINE_LOOP );

		for ( int i = 0; i <= sides; i++ )
		{
			double ds = sin( ( i * 2 * c_pi ) / sides );
			double dc = cos( ( i * 2 * c_pi ) / sides );

			glVertex3f(
				origin[0],
				static_cast<float>( origin[1] + radius * dc ),
				static_cast<float>( origin[2] + radius * ds )
				);
		}

		glEnd();
	}
}

void light_draw_radius_wire( const Vector3& origin, const float envelope[3] ){
	if ( envelope[0] > 0 ) {
		sphere_draw_wire( origin, envelope[0], 24 );
	}
	if ( envelope[1] > 0 ) {
		sphere_draw_wire( origin, envelope[1], 24 );
	}
	if ( envelope[2] > 0 ) {
		sphere_draw_wire( origin, envelope[2], 24 );
	}
}

#elif 1 //precalculated circle

#define SPHERE_WIRE_SIDES 24
#define SPHERE_WIRE_POINTS SPHERE_WIRE_SIDES * 3

void sphere_construct_wire( Vector3 radiiPoints[SPHERE_WIRE_POINTS] ){
	int k = 0;

	for ( int i = 0; i < SPHERE_WIRE_SIDES; i++ )
	{
		double ds = sin( ( i * 2 * c_pi ) / SPHERE_WIRE_SIDES );
		double dc = cos( ( i * 2 * c_pi ) / SPHERE_WIRE_SIDES );

		radiiPoints[k++] =
		Vector3(
			static_cast<float>( dc ),
			static_cast<float>( ds ),
			0.f
			);
	}

	for ( int i = 0; i < SPHERE_WIRE_SIDES; i++ )
	{
		double ds = sin( ( i * 2 * c_pi ) / SPHERE_WIRE_SIDES );
		double dc = cos( ( i * 2 * c_pi ) / SPHERE_WIRE_SIDES );

		radiiPoints[k++] =
		Vector3(
			static_cast<float>( dc ),
			0.f,
			static_cast<float>( ds )
			);
	}

	for ( int i = 0; i < SPHERE_WIRE_SIDES; i++ )
	{
		double ds = sin( ( i * 2 * c_pi ) / SPHERE_WIRE_SIDES );
		double dc = cos( ( i * 2 * c_pi ) / SPHERE_WIRE_SIDES );

		radiiPoints[k++] =
		Vector3(
			0.f,
			static_cast<float>( dc ),
			static_cast<float>( ds )
			);
	}

}

void sphere_draw_wire( const Vector3& origin, float radius, const Vector3 radiiPoints[SPHERE_WIRE_POINTS] ){
	int k = 0;
	for( int j = 0; j < 3; j++ )
	{
		glBegin( GL_LINE_LOOP );

		for ( int i = 0; i < SPHERE_WIRE_SIDES; i++ )
		{
			glVertex3fv( vector3_to_array( vector3_added( origin, vector3_scaled( radiiPoints[k++], radius ) ) ) );
		}

		glEnd();
	}
}

void light_draw_radius_wire( const Vector3& origin, const float envelope[3], const Vector3 radiiPoints[SPHERE_WIRE_POINTS] ){
	if ( envelope[0] > 0 ) {
		sphere_draw_wire( origin, envelope[0], radiiPoints );
	}
	if ( envelope[1] > 0 ) {
		sphere_draw_wire( origin, envelope[1], radiiPoints );
	}
	if ( envelope[2] > 0 ) {
		sphere_draw_wire( origin, envelope[2], radiiPoints );
	}
}
#endif


void light_draw_box_lines( const Vector3& origin, const Vector3 points[8] ){
	//draw lines from the center of the bbox to the corners
	glBegin( GL_LINES );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[1] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[5] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[2] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[6] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[0] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[4] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[3] ) );

	glVertex3fv( vector3_to_array( origin ) );
	glVertex3fv( vector3_to_array( points[7] ) );

	glEnd();
}

void light_vertices( const AABB& aabb_light, Vector3 points[6] ){
	Vector3 max( vector3_added( aabb_light.origin, aabb_light.extents ) );
	Vector3 min( vector3_subtracted( aabb_light.origin, aabb_light.extents ) );
	Vector3 mid( aabb_light.origin );

	// top, bottom, middle-up, middle-right, middle-down, middle-left
	points[0] = Vector3( mid[0], mid[1], max[2] );
	points[1] = Vector3( mid[0], mid[1], min[2] );
	points[2] = Vector3( mid[0], max[1], mid[2] );
	points[3] = Vector3( max[0], mid[1], mid[2] );
	points[4] = Vector3( mid[0], min[1], mid[2] );
	points[5] = Vector3( min[0], mid[1], mid[2] );
}

inline void light_testselect( const AABB& aabb, SelectionTest& test, SelectionIntersection& best ){
	const IndexPointer::index_type indices[24] = {
		0, 2, 3,
		0, 3, 4,
		0, 4, 5,
		0, 5, 2,
		1, 2, 5,
		1, 5, 4,
		1, 4, 3,
		1, 3, 2
	};
	Vector3 points[6];
	light_vertices( aabb, points );
	test.TestTriangles( VertexPointer( reinterpret_cast<VertexPointer::pointer>( points ), sizeof( Vector3 ) ), IndexPointer( indices, 24 ), best );
}

void light_draw( const AABB& aabb_light, RenderStateFlags state ){
	Vector3 points[6];
	light_vertices( aabb_light, points );

	if ( state & RENDER_LIGHTING ) {
		const float f = 0.70710678f;
		// North, East, South, West
		const Vector3 normals[8] = {
			Vector3( 0, f, f ),
			Vector3( f, 0, f ),
			Vector3( 0,-f, f ),
			Vector3( -f, 0, f ),
			Vector3( 0, f,-f ),
			Vector3( f, 0,-f ),
			Vector3( 0,-f,-f ),
			Vector3( -f, 0,-f ),
		};

#if !defined( USE_TRIANGLE_FAN )
		glBegin( GL_TRIANGLES );
#else
		glBegin( GL_TRIANGLE_FAN );
#endif
		glVertex3fv( vector3_to_array( points[0] ) );
		glVertex3fv( vector3_to_array( points[2] ) );
		glNormal3fv( vector3_to_array( normals[0] ) );
		glVertex3fv( vector3_to_array( points[3] ) );

#if !defined( USE_TRIANGLE_FAN )
		glVertex3fv( vector3_to_array( points[0] ) );
		glVertex3fv( vector3_to_array( points[3] ) );
#endif
		glNormal3fv( vector3_to_array( normals[1] ) );
		glVertex3fv( vector3_to_array( points[4] ) );

#if !defined( USE_TRIANGLE_FAN )
		glVertex3fv( vector3_to_array( points[0] ) );
		glVertex3fv( vector3_to_array( points[4] ) );
#endif
		glNormal3fv( vector3_to_array( normals[2] ) );
		glVertex3fv( vector3_to_array( points[5] ) );
#if !defined( USE_TRIANGLE_FAN )
		glVertex3fv( vector3_to_array( points[0] ) );
		glVertex3fv( vector3_to_array( points[5] ) );
#endif
		glNormal3fv( vector3_to_array( normals[3] ) );
		glVertex3fv( vector3_to_array( points[2] ) );
#if defined( USE_TRIANGLE_FAN )
		glEnd();
		glBegin( GL_TRIANGLE_FAN );
#endif

		glVertex3fv( vector3_to_array( points[1] ) );
		glVertex3fv( vector3_to_array( points[2] ) );
		glNormal3fv( vector3_to_array( normals[7] ) );
		glVertex3fv( vector3_to_array( points[5] ) );

#if !defined( USE_TRIANGLE_FAN )
		glVertex3fv( vector3_to_array( points[1] ) );
		glVertex3fv( vector3_to_array( points[5] ) );
#endif
		glNormal3fv( vector3_to_array( normals[6] ) );
		glVertex3fv( vector3_to_array( points[4] ) );

#if !defined( USE_TRIANGLE_FAN )
		glVertex3fv( vector3_to_array( points[1] ) );
		glVertex3fv( vector3_to_array( points[4] ) );
#endif
		glNormal3fv( vector3_to_array( normals[5] ) );
		glVertex3fv( vector3_to_array( points[3] ) );

#if !defined( USE_TRIANGLE_FAN )
		glVertex3fv( vector3_to_array( points[1] ) );
		glVertex3fv( vector3_to_array( points[3] ) );
#endif
		glNormal3fv( vector3_to_array( normals[4] ) );
		glVertex3fv( vector3_to_array( points[2] ) );

		glEnd();
	}
	else
	{
		typedef unsigned int index_t;
		const index_t indices[24] = {
			0, 2, 3,
			0, 3, 4,
			0, 4, 5,
			0, 5, 2,
			1, 2, 5,
			1, 5, 4,
			1, 4, 3,
			1, 3, 2
		};
#if 1
		glVertexPointer( 3, GL_FLOAT, 0, points );
		glDrawElements( GL_TRIANGLES, sizeof( indices ) / sizeof( index_t ), RenderIndexTypeID, indices );
#else
		glBegin( GL_TRIANGLES );
		for ( unsigned int i = 0; i < sizeof( indices ) / sizeof( index_t ); ++i )
		{
			glVertex3fv( points[indices[i]] );
		}
		glEnd();
#endif
	}


	// NOTE: prolly not relevant until some time..
	// check for DOOM lights
#if 0
	if ( strlen( ValueForKey( e, "light_right" ) ) > 0 ) {
		vec3_t vRight, vUp, vTarget, vTemp;
		GetVectorForKey( e, "light_right", vRight );
		GetVectorForKey( e, "light_up", vUp );
		GetVectorForKey( e, "light_target", vTarget );

		glColor3f( 0, 1, 0 );
		glBegin( GL_LINE_LOOP );
		VectorAdd( vTarget, e->origin, vTemp );
		VectorAdd( vTemp, vRight, vTemp );
		VectorAdd( vTemp, vUp, vTemp );
		glVertex3fv( e->origin );
		glVertex3fv( vTemp );
		VectorAdd( vTarget, e->origin, vTemp );
		VectorAdd( vTemp, vUp, vTemp );
		VectorSubtract( vTemp, vRight, vTemp );
		glVertex3fv( e->origin );
		glVertex3fv( vTemp );
		VectorAdd( vTarget, e->origin, vTemp );
		VectorAdd( vTemp, vRight, vTemp );
		VectorSubtract( vTemp, vUp, vTemp );
		glVertex3fv( e->origin );
		glVertex3fv( vTemp );
		VectorAdd( vTarget, e->origin, vTemp );
		VectorSubtract( vTemp, vUp, vTemp );
		VectorSubtract( vTemp, vRight, vTemp );
		glVertex3fv( e->origin );
		glVertex3fv( vTemp );
		glEnd();

	}
#endif
}

inline void write_intensity( const float intensity, Entity* entity ){
	char value[64];
	sprintf( value, "%g", intensity );
	if( !string_empty( entity->getKeyValue( "_light" ) ) ) //primaryIntensity //if set or default is set in .ent
		entity->setKeyValue( "_light", value );
	else //secondaryIntensity
		entity->setKeyValue( "light", value ); //otherwise default to "light", which is understood by both q3 and q1
}

// These variables are tweakable on the q3map2 console, setting to q3map2
// default here as there is no way to find out what the user actually uses
// right now. Maybe move them to worldspawn?
float fPointScale = 7500.f;
float fLinearScale = 1.f / 8000.f;

float light_radius_linear( float fIntensity, float fFalloffTolerance ){
	return ( ( fIntensity * fPointScale * fLinearScale ) - fFalloffTolerance );
}

float light_radius( float fIntensity, float fFalloffTolerance ){
	return sqrt( fIntensity * fPointScale / fFalloffTolerance );
}


LightType g_lightType = LIGHTTYPE_DEFAULT;


bool spawnflags_linear( int flags ){
	if ( g_lightType == LIGHTTYPE_RTCW ) {
		// Spawnflags :
		// 1: nonlinear
		// 2: angle

		return !( flags & 1 );
	}
	else
	{
		// Spawnflags :
		// 1: linear
		// 2: no angle

		return ( flags & 1 );
	}
}

class LightRadii
{
public:
float m_radii[3];
float m_radii_transformed[3];

private:
float m_primaryIntensity;
float m_secondaryIntensity;
int m_flags;
float m_fade;
float m_scale;

void calculateRadii(){
	float intensity = 300.0f;

	if ( m_primaryIntensity != 0.0f ) {
		intensity = m_primaryIntensity;
	}
	else if ( m_secondaryIntensity != 0.0f ) {
		intensity = m_secondaryIntensity;
	}

	intensity *= m_scale;

	if ( spawnflags_linear( m_flags ) ) {
		m_radii_transformed[0] = m_radii[0] = light_radius_linear( intensity, 1.0f ) / m_fade;
		m_radii_transformed[1] = m_radii[1] = light_radius_linear( intensity, 48.0f ) / m_fade;
		m_radii_transformed[2] = m_radii[2] = light_radius_linear( intensity, 255.0f ) / m_fade;
	}
	else
	{
		m_radii_transformed[0] = m_radii[0] = light_radius( intensity, 1.0f );
		m_radii_transformed[1] = m_radii[1] = light_radius( intensity, 48.0f );
		m_radii_transformed[2] = m_radii[2] = light_radius( intensity, 255.0f );
	}
}

public:
LightRadii() : m_primaryIntensity( 0 ), m_secondaryIntensity( 0 ), m_flags( 0 ), m_fade( 1 ), m_scale( 1 ){
	calculateRadii();
}


void primaryIntensityChanged( const char* value ){
	m_primaryIntensity = string_read_float( value );
	calculateRadii();
}
typedef MemberCaller1<LightRadii, const char*, &LightRadii::primaryIntensityChanged> PrimaryIntensityChangedCaller;
void secondaryIntensityChanged( const char* value ){
	m_secondaryIntensity = string_read_float( value );
	calculateRadii();
}
typedef MemberCaller1<LightRadii, const char*, &LightRadii::secondaryIntensityChanged> SecondaryIntensityChangedCaller;
void scaleChanged( const char* value ){
	m_scale = string_read_float( value );
	if ( m_scale <= 0.0f ) {
		m_scale = 1.0f;
	}
	calculateRadii();
}
typedef MemberCaller1<LightRadii, const char*, &LightRadii::scaleChanged> ScaleChangedCaller;
void fadeChanged( const char* value ){
	m_fade = string_read_float( value );
	if ( m_fade <= 0.0f ) {
		m_fade = 1.0f;
	}
	calculateRadii();
}
typedef MemberCaller1<LightRadii, const char*, &LightRadii::fadeChanged> FadeChangedCaller;
void flagsChanged( const char* value ){
	m_flags = string_read_int( value );
	calculateRadii();
}
typedef MemberCaller1<LightRadii, const char*, &LightRadii::flagsChanged> FlagsChangedCaller;

void calculateTransformedRadii( float radius ){
	float (&r)[3] = m_radii_transformed;
	if ( spawnflags_linear( m_flags ) ) {
		r[0] = radius + 47.0f / m_fade;
		if( r[0] <= 1.f ){
			r[0] = r[1] = r[2] = 1.f;
			return;
		}
		r[1] = radius;
		r[2] = radius - 207.0f / m_fade;;
	}
	else
	{
		r[0] = radius * sqrt( 48.f );
		if( r[0] <= 1.f ){
			r[0] = r[1] = r[2] = 1.f;
			return;
		}
		r[1] = radius;
		r[2] = r[0] / sqrt( 255.f );
	}
	//globalOutputStream() << r[0] << " " << r[1] << " " << r[2] << " \n";
}
float calculateIntensityFromRadii(){
	float intensity;
	if ( spawnflags_linear( m_flags ) )
		intensity = ( m_radii_transformed[0] * m_fade + 1.f ) / ( fPointScale * fLinearScale );
	else
		intensity = m_radii_transformed[0] * m_radii_transformed[0] * 1.f / fPointScale;
	return intensity / m_scale;
}
};

class Doom3LightRadius
{
public:
Vector3 m_defaultRadius;
Vector3 m_radius;
Vector3 m_radiusTransformed;
Vector3 m_center;
Callback m_changed;
bool m_useCenterKey;

Doom3LightRadius( const char* defaultRadius ) : m_defaultRadius( 300, 300, 300 ), m_center( 0, 0, 0 ), m_useCenterKey( false ){
	if ( g_lightType == LIGHTTYPE_DOOM3 ){
		if ( !string_parse_vector3( defaultRadius, m_defaultRadius ) ) {
			globalErrorStream() << "Doom3LightRadius: failed to parse default light radius\n";
		}
	m_radius = m_defaultRadius;
	}
}

void lightRadiusChanged( const char* value ){
	if ( !string_parse_vector3( value, m_radius ) ) {
		m_radius = m_defaultRadius;
	}
	m_radiusTransformed = m_radius;
	m_changed();
	SceneChangeNotify();
}
typedef MemberCaller1<Doom3LightRadius, const char*, &Doom3LightRadius::lightRadiusChanged> LightRadiusChangedCaller;

void lightCenterChanged( const char* value ){
	m_useCenterKey = string_parse_vector3( value, m_center );
	if ( !m_useCenterKey ) {
		m_center = Vector3( 0, 0, 0 );
	}
	SceneChangeNotify();
}
typedef MemberCaller1<Doom3LightRadius, const char*, &Doom3LightRadius::lightCenterChanged> LightCenterChangedCaller;
};

class RenderLightRadiiWire : public OpenGLRenderable
{
LightRadii& m_radii;
const Vector3& m_origin;
public:
static Vector3 m_radiiPoints[SPHERE_WIRE_POINTS];

RenderLightRadiiWire( LightRadii& radii, const Vector3& origin ) : m_radii( radii ), m_origin( origin ){
}
void render( RenderStateFlags state ) const {
	//light_draw_radius_wire( m_origin, m_radii.m_radii );
	light_draw_radius_wire( m_origin, m_radii.m_radii_transformed, m_radiiPoints );
}
};
Vector3 RenderLightRadiiWire::m_radiiPoints[SPHERE_WIRE_POINTS] = {g_vector3_identity};

class RenderLightRadiiFill : public OpenGLRenderable
{
LightRadii& m_radii;
const Vector3& m_origin;
public:
//static Shader* m_state;
static Vector3 m_radiiPoints[SPHERE_FILL_POINTS];

RenderLightRadiiFill( LightRadii& radii, const Vector3& origin ) : m_radii( radii ), m_origin( origin ){
}
void render( RenderStateFlags state ) const {
	//light_draw_radius_fill( m_origin, m_radii.m_radii );
	light_draw_radius_fill( m_origin, m_radii.m_radii_transformed, m_radiiPoints );
}
};
//Shader* RenderLightRadiiFill::m_state = 0;
Vector3 RenderLightRadiiFill::m_radiiPoints[SPHERE_FILL_POINTS] = {g_vector3_identity};

class RenderLightRadiiBox : public OpenGLRenderable
{
const Vector3& m_origin;
public:
mutable Vector3 m_points[8];
//static Shader* m_state;

RenderLightRadiiBox( const Vector3& origin ) : m_origin( origin ){
}
void render( RenderStateFlags state ) const {
	//draw the bounding box of light based on light_radius key
	if ( ( state & RENDER_FILL ) != 0 ) {
		aabb_draw_flatshade( m_points );
	}
	else
	{
		aabb_draw_wire( m_points );
	}

  #if 1    //disable if you dont want lines going from the center of the light bbox to the corners
	light_draw_box_lines( m_origin, m_points );
  #endif
}
};

class RenderLightCenter : public OpenGLRenderable
{
const Vector3& m_center;
EntityClass& m_eclass;
public:
static Shader* m_state;

RenderLightCenter( const Vector3& center, EntityClass& eclass ) : m_center( center ), m_eclass( eclass ){
}
void render( RenderStateFlags state ) const {
	glBegin( GL_POINTS );
	glColor3fv( vector3_to_array( m_eclass.color ) );
	glVertex3fv( vector3_to_array( m_center ) );
	glEnd();
}
};

Shader* RenderLightCenter::m_state = 0;

class RenderLightProjection : public OpenGLRenderable
{
const Matrix4& m_projection;
public:

RenderLightProjection( const Matrix4& projection ) : m_projection( projection ){
}
void render( RenderStateFlags state ) const {
	Matrix4 unproject( matrix4_full_inverse( m_projection ) );
	Vector3 points[8];
	aabb_corners( AABB( Vector3( 0.5f, 0.5f, 0.5f ), Vector3( 0.5f, 0.5f, 0.5f ) ), points );
	points[0] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[0], 1 ) ) );
	points[1] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[1], 1 ) ) );
	points[2] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[2], 1 ) ) );
	points[3] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[3], 1 ) ) );
	points[4] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[4], 1 ) ) );
	points[5] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[5], 1 ) ) );
	points[6] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[6], 1 ) ) );
	points[7] = vector4_projected( matrix4_transformed_vector4( unproject, Vector4( points[7], 1 ) ) );
	//Vector4 test1 = matrix4_transformed_vector4( unproject, Vector4( 0.5f, 0.5f, 0.5f, 1 ) );
	//Vector3 test2 = vector4_projected( test1 );
	aabb_draw_wire( points );
}
};
/*
inline void default_extents( Vector3& extents ){
	extents = Vector3( 12, 12, 12 );
}
*/
class ShaderRef
{
CopiedString m_name;
Shader* m_shader;
void capture(){
	m_shader = GlobalShaderCache().capture( m_name.c_str() );
}
void release(){
	GlobalShaderCache().release( m_name.c_str() );
}
public:
ShaderRef(){
	capture();
}
~ShaderRef(){
	release();
}
void setName( const char* name ){
	release();
	m_name = name;
	capture();
}
Shader* get() const {
	return m_shader;
}
};

class LightShader
{
ShaderRef m_shader;
void setDefault(){
	m_shader.setName( m_defaultShader );
}
public:
static const char* m_defaultShader;

LightShader(){
	setDefault();
}
void valueChanged( const char* value ){
	if ( string_empty( value ) ) {
		setDefault();
	}
	else
	{
		m_shader.setName( value );
	}
	SceneChangeNotify();
}
typedef MemberCaller1<LightShader, const char*, &LightShader::valueChanged> ValueChangedCaller;

Shader* get() const {
	return m_shader.get();
}
};

const char* LightShader::m_defaultShader = "";

inline const BasicVector4<double>& plane3_to_vector4( const Plane3& self ){
	return reinterpret_cast<const BasicVector4<double>&>( self );
}

inline BasicVector4<double>& plane3_to_vector4( Plane3& self ){
	return reinterpret_cast<BasicVector4<double>&>( self );
}

inline Matrix4 matrix4_from_planes( const Plane3& left, const Plane3& right, const Plane3& bottom, const Plane3& top, const Plane3& front, const Plane3& back ){
	return Matrix4(
			   ( right.a - left.a ) / 2,
			   ( top.a - bottom.a ) / 2,
			   ( back.a - front.a ) / 2,
			   right.a - ( right.a - left.a ) / 2,
			   ( right.b - left.b ) / 2,
			   ( top.b - bottom.b ) / 2,
			   ( back.b - front.b ) / 2,
			   right.b - ( right.b - left.b ) / 2,
			   ( right.c - left.c ) / 2,
			   ( top.c - bottom.c ) / 2,
			   ( back.c - front.c ) / 2,
			   right.c - ( right.c - left.c ) / 2,
			   ( right.d - left.d ) / 2,
			   ( top.d - bottom.d ) / 2,
			   ( back.d - front.d ) / 2,
			   right.d - ( right.d - left.d ) / 2
			   );
}

const char EXCLUDE_NAME[] = "light";

class Light :
	public OpenGLRenderable,
	public Cullable,
	public Bounded,
	public Editable,
	public Snappable
{
EntityKeyValues m_entity;
KeyObserverMap m_keyObservers;
TraversableNodeSet m_traverse;
IdentityTransform m_transform;

OriginKey m_originKey;
RotationKey m_rotationKey;
Float9 m_rotation;
Colour m_colour;

ClassnameFilter m_filter;
NamedEntity m_named;
NameKeys m_nameKeys;
TraversableObserverPairRelay m_traverseObservers;
Doom3GroupOrigin m_funcStaticOrigin;

LightRadii m_radii;
Doom3LightRadius m_doom3Radius;

AABB m_aabb_light;

RenderLightRadiiWire m_radii_wire;
RenderLightRadiiFill m_radii_fill;
RenderLightRadiiBox m_radii_box;
RenderLightCenter m_render_center;
RenderableNamedEntity m_renderName;

Vector3 m_lightOrigin;
bool m_useLightOrigin;
Float9 m_lightRotation;
bool m_useLightRotation;

Vector3 m_lightTarget;
bool m_useLightTarget;
Vector3 m_lightUp;
bool m_useLightUp;
Vector3 m_lightRight;
bool m_useLightRight;
Vector3 m_lightStart;
bool m_useLightStart;
Vector3 m_lightEnd;
bool m_useLightEnd;

mutable AABB m_doom3AABB;
mutable Matrix4 m_doom3Rotation;
mutable Matrix4 m_doom3Projection;
mutable Frustum m_doom3Frustum;
mutable bool m_doom3ProjectionChanged;

RenderLightProjection m_renderProjection;

LightShader m_shader;

Callback m_transformChanged;
Callback m_boundsChanged;
Callback m_evaluateTransform;

void construct(){
	default_rotation( m_rotation );
	//m_aabb_light.origin = Vector3( 0, 0, 0 );
	//default_extents( m_aabb_light.extents );

	m_keyObservers.insert( "classname", ClassnameFilter::ClassnameChangedCaller( m_filter ) );
	m_keyObservers.insert( Static<KeyIsName>::instance().m_nameKey, NamedEntity::IdentifierChangedCaller( m_named ) );
	m_keyObservers.insert( "_color", Colour::ColourChangedCaller( m_colour ) );
	m_keyObservers.insert( "origin", OriginKey::OriginChangedCaller( m_originKey ) );
	m_keyObservers.insert( "_light", LightRadii::PrimaryIntensityChangedCaller( m_radii ) );
	m_keyObservers.insert( "light", LightRadii::SecondaryIntensityChangedCaller( m_radii ) );
	m_keyObservers.insert( "fade", LightRadii::FadeChangedCaller( m_radii ) );
	m_keyObservers.insert( "scale", LightRadii::ScaleChangedCaller( m_radii ) );
	m_keyObservers.insert( "spawnflags", LightRadii::FlagsChangedCaller( m_radii ) );

	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_keyObservers.insert( "angle", RotationKey::AngleChangedCaller( m_rotationKey ) );
		m_keyObservers.insert( "rotation", RotationKey::RotationChangedCaller( m_rotationKey ) );
		m_keyObservers.insert( "light_radius", Doom3LightRadius::LightRadiusChangedCaller( m_doom3Radius ) );
		m_keyObservers.insert( "light_center", Doom3LightRadius::LightCenterChangedCaller( m_doom3Radius ) );
		m_keyObservers.insert( "light_origin", Light::LightOriginChangedCaller( *this ) );
		m_keyObservers.insert( "light_rotation", Light::LightRotationChangedCaller( *this ) );
		m_keyObservers.insert( "light_target", Light::LightTargetChangedCaller( *this ) );
		m_keyObservers.insert( "light_up", Light::LightUpChangedCaller( *this ) );
		m_keyObservers.insert( "light_right", Light::LightRightChangedCaller( *this ) );
		m_keyObservers.insert( "light_start", Light::LightStartChangedCaller( *this ) );
		m_keyObservers.insert( "light_end", Light::LightEndChangedCaller( *this ) );
		m_keyObservers.insert( "texture", LightShader::ValueChangedCaller( m_shader ) );
		m_useLightTarget = m_useLightUp = m_useLightRight = m_useLightStart = m_useLightEnd = false;
		m_doom3ProjectionChanged = true;
	}

	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_traverse.attach( &m_traverseObservers );
		m_traverseObservers.attach( m_funcStaticOrigin );

		m_entity.m_isContainer = true;
	}
}
void destroy(){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_traverseObservers.detach( m_funcStaticOrigin );
		m_traverse.detach( &m_traverseObservers );
	}
}

// vc 2k5 compiler fix
#if _MSC_VER >= 1400
public:
#endif

void updateOrigin(){
	m_boundsChanged();

	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_funcStaticOrigin.originChanged();
	}

	m_doom3Radius.m_changed();

	GlobalSelectionSystem().pivotChanged();
}

void originChanged(){
	m_aabb_light.origin = m_useLightOrigin ? m_lightOrigin : m_originKey.m_origin;
	updateOrigin();
}
typedef MemberCaller<Light, &Light::originChanged> OriginChangedCaller;

void lightOriginChanged( const char* value ){
	m_useLightOrigin = !string_empty( value );
	if ( m_useLightOrigin ) {
		read_origin( m_lightOrigin, value );
	}
	originChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightOriginChanged> LightOriginChangedCaller;

void lightTargetChanged( const char* value ){
	m_useLightTarget = !string_empty( value );
	if ( m_useLightTarget ) {
		read_origin( m_lightTarget, value );
	}
	projectionChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightTargetChanged> LightTargetChangedCaller;
void lightUpChanged( const char* value ){
	m_useLightUp = !string_empty( value );
	if ( m_useLightUp ) {
		read_origin( m_lightUp, value );
	}
	projectionChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightUpChanged> LightUpChangedCaller;
void lightRightChanged( const char* value ){
	m_useLightRight = !string_empty( value );
	if ( m_useLightRight ) {
		read_origin( m_lightRight, value );
	}
	projectionChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightRightChanged> LightRightChangedCaller;
void lightStartChanged( const char* value ){
	m_useLightStart = !string_empty( value );
	if ( m_useLightStart ) {
		read_origin( m_lightStart, value );
	}
	projectionChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightStartChanged> LightStartChangedCaller;
void lightEndChanged( const char* value ){
	m_useLightEnd = !string_empty( value );
	if ( m_useLightEnd ) {
		read_origin( m_lightEnd, value );
	}
	projectionChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightEndChanged> LightEndChangedCaller;

void writeLightOrigin(){
	write_origin( m_lightOrigin, &m_entity, "light_origin" );
}

void updateLightRadiiBox() const {
	const Matrix4& rotation = rotation_toMatrix( m_rotation );
	aabb_corners( AABB( Vector3( 0, 0, 0 ), m_doom3Radius.m_radiusTransformed ), m_radii_box.m_points );
	matrix4_transform_point( rotation, m_radii_box.m_points[0] );
	vector3_add( m_radii_box.m_points[0], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[1] );
	vector3_add( m_radii_box.m_points[1], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[2] );
	vector3_add( m_radii_box.m_points[2], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[3] );
	vector3_add( m_radii_box.m_points[3], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[4] );
	vector3_add( m_radii_box.m_points[4], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[5] );
	vector3_add( m_radii_box.m_points[5], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[6] );
	vector3_add( m_radii_box.m_points[6], m_aabb_light.origin );
	matrix4_transform_point( rotation, m_radii_box.m_points[7] );
	vector3_add( m_radii_box.m_points[7], m_aabb_light.origin );
}

void rotationChanged(){
	rotation_assign( m_rotation, m_useLightRotation ? m_lightRotation : m_rotationKey.m_rotation );
	GlobalSelectionSystem().pivotChanged();
}
typedef MemberCaller<Light, &Light::rotationChanged> RotationChangedCaller;

void lightRotationChanged( const char* value ){
	m_useLightRotation = !string_empty( value );
	if ( m_useLightRotation ) {
		read_rotation( m_lightRotation, value );
	}
	rotationChanged();
}
typedef MemberCaller1<Light, const char*, &Light::lightRotationChanged> LightRotationChangedCaller;

public:

Light( EntityClass* eclass, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged, const Callback& evaluateTransform ) :
	m_entity( eclass ),
	m_originKey( OriginChangedCaller( *this ) ),
	m_rotationKey( RotationChangedCaller( *this ) ),
	m_colour( Callback() ),
	m_filter( m_entity, node ),
	m_named( m_entity ),
	m_nameKeys( m_entity ),
	m_funcStaticOrigin( m_traverse, m_originKey.m_origin ),
	m_doom3Radius( EntityClass_valueForKey( m_entity.getEntityClass(), "light_radius" ) ),
	m_aabb_light( Vector3( 0, 0, 0 ), Vector3( 12, 12, 12 ) ),
	m_radii_wire( m_radii, m_aabb_light.origin ),
	m_radii_fill( m_radii, m_aabb_light.origin ),
	m_radii_box( m_aabb_light.origin ),
	m_render_center( m_doom3Radius.m_center, m_entity.getEntityClass() ),
	m_renderName( m_named, m_aabb_light.origin, EXCLUDE_NAME ),
	m_useLightOrigin( false ),
	m_useLightRotation( false ),
	m_renderProjection( m_doom3Projection ),
	m_transformChanged( transformChanged ),
	m_boundsChanged( boundsChanged ),
	m_evaluateTransform( evaluateTransform ){
	construct();
}
Light( const Light& other, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged, const Callback& evaluateTransform ) :
	m_entity( other.m_entity ),
	m_originKey( OriginChangedCaller( *this ) ),
	m_rotationKey( RotationChangedCaller( *this ) ),
	m_colour( Callback() ),
	m_filter( m_entity, node ),
	m_named( m_entity ),
	m_nameKeys( m_entity ),
	m_funcStaticOrigin( m_traverse, m_originKey.m_origin ),
	m_doom3Radius( EntityClass_valueForKey( m_entity.getEntityClass(), "light_radius" ) ),
	m_aabb_light( Vector3( 0, 0, 0 ), Vector3( 12, 12, 12 ) ),
	m_radii_wire( m_radii, m_aabb_light.origin ),
	m_radii_fill( m_radii, m_aabb_light.origin ),
	m_radii_box( m_aabb_light.origin ),
	m_render_center( m_doom3Radius.m_center, m_entity.getEntityClass() ),
	m_renderName( m_named, m_aabb_light.origin, EXCLUDE_NAME ),
	m_useLightOrigin( false ),
	m_useLightRotation( false ),
	m_renderProjection( m_doom3Projection ),
	m_transformChanged( transformChanged ),
	m_boundsChanged( boundsChanged ),
	m_evaluateTransform( evaluateTransform ){
	construct();
}
~Light(){
	destroy();
}

InstanceCounter m_instanceCounter;
void instanceAttach( const scene::Path& path ){
	if ( ++m_instanceCounter.m_count == 1 ) {
		m_filter.instanceAttach();
		m_entity.instanceAttach( path_find_mapfile( path.begin(), path.end() ) );
		if ( g_lightType == LIGHTTYPE_DOOM3 ) {
			m_traverse.instanceAttach( path_find_mapfile( path.begin(), path.end() ) );
		}
		m_entity.attach( m_keyObservers );

		if ( g_lightType == LIGHTTYPE_DOOM3 ) {
			m_funcStaticOrigin.enable();
		}
	}
}
void instanceDetach( const scene::Path& path ){
	if ( --m_instanceCounter.m_count == 0 ) {
		if ( g_lightType == LIGHTTYPE_DOOM3 ) {
			m_funcStaticOrigin.disable();
		}

		m_entity.detach( m_keyObservers );
		if ( g_lightType == LIGHTTYPE_DOOM3 ) {
			m_traverse.instanceDetach( path_find_mapfile( path.begin(), path.end() ) );
		}
		m_entity.instanceDetach( path_find_mapfile( path.begin(), path.end() ) );
		m_filter.instanceDetach();
	}
}

EntityKeyValues& getEntity(){
	return m_entity;
}
const EntityKeyValues& getEntity() const {
	return m_entity;
}

scene::Traversable& getTraversable(){
	return m_traverse;
}
Namespaced& getNamespaced(){
	return m_nameKeys;
}
Nameable& getNameable(){
	return m_named;
}
TransformNode& getTransformNode(){
	return m_transform;
}

void attach( scene::Traversable::Observer* observer ){
	m_traverseObservers.attach( *observer );
}
void detach( scene::Traversable::Observer* observer ){
	m_traverseObservers.detach( *observer );
}

void render( RenderStateFlags state ) const {
	light_draw( m_aabb_light, state );
}

VolumeIntersectionValue intersectVolume( const VolumeTest& volume, const Matrix4& localToWorld ) const {
	return volume.TestAABB( m_aabb_light, localToWorld );
}

// cache
const AABB& localAABB() const {
	return m_aabb_light;
}


mutable Matrix4 m_projectionOrientation;

void renderSolid( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected ) const {
	renderer.SetState( m_colour.state(), Renderer::eWireframeOnly );
	renderer.SetState( m_colour.state(), Renderer::eFullMaterials );
	renderer.addRenderable( *this, localToWorld );

	if( selected ){
		if ( g_lightType != LIGHTTYPE_DOOM3 ) {
			if ( g_lightRadii && string_empty( m_entity.getKeyValue( "target" ) ) ) {
				if ( renderer.getStyle() == Renderer::eFullMaterials ) {
					renderer.SetState( m_colour.state_additive(), Renderer::eFullMaterials );
					renderer.Highlight( Renderer::ePrimitive, false );
					renderer.Highlight( Renderer::eFace, false );
					renderer.addRenderable( m_radii_fill, localToWorld );
				}
				else
				{
					renderer.Highlight( Renderer::ePrimitive, false );
					renderer.addRenderable( m_radii_wire, localToWorld );
				}
			}
		}
		else{
			renderer.SetState( m_entity.getEntityClass().m_state_wire, Renderer::eFullMaterials );
			if ( isProjected() ) {
				projection();
				m_projectionOrientation = rotation();
				vector4_to_vector3( m_projectionOrientation.t() ) = localAABB().origin;
				renderer.addRenderable( m_renderProjection, m_projectionOrientation );
			}
			else
			{
				updateLightRadiiBox();
				renderer.addRenderable( m_radii_box, localToWorld );
			}

			//draw the center of the light
			if ( m_doom3Radius.m_useCenterKey ) {
				renderer.Highlight( Renderer::ePrimitive, false );
				renderer.Highlight( Renderer::eFace, false );
				renderer.SetState( m_render_center.m_state, Renderer::eFullMaterials );
				renderer.SetState( m_render_center.m_state, Renderer::eWireframeOnly );
				renderer.addRenderable( m_render_center, localToWorld );
			}
		}
	}

	if ( m_renderName.excluded_not()
		&& ( selected || ( g_showNames && ( volume.fill() || aabb_fits_view( m_aabb_light, volume.GetModelview(), volume.GetViewport(), g_showNamesRatio ) ) ) ) ) {
		m_renderName.render( renderer, volume, localToWorld, selected );
	}
}
void renderWireframe( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected ) const {
	renderSolid( renderer, volume, localToWorld, selected );
}

void testSelect( Selector& selector, SelectionTest& test, const Matrix4& localToWorld ){
	test.BeginMesh( localToWorld );

	SelectionIntersection best;
	light_testselect( m_aabb_light, test, best );
	if ( best.valid() ) {
		selector.addIntersection( best );
	}
}

void translate( const Vector3& translation ){
	m_aabb_light.origin = origin_translated( m_aabb_light.origin, translation );
}
void rotate( const Quaternion& rotation ){
	rotation_rotate( m_rotation, rotation );
}
void snapto( float snap ){
	if ( g_lightType == LIGHTTYPE_DOOM3 && !m_useLightOrigin && !m_traverse.empty() ) {
		m_useLightOrigin = true;
		m_lightOrigin = m_originKey.m_origin;
	}

	if ( m_useLightOrigin ) {
		m_lightOrigin = origin_snapped( m_lightOrigin, snap );
		writeLightOrigin();
	}
	else
	{
		m_originKey.m_origin = origin_snapped( m_originKey.m_origin, snap );
		m_originKey.write( &m_entity );
	}
}
void setLightRadii( float radius ){
	m_radii.calculateTransformedRadii( radius );
}
void setLightRadius( const AABB& aabb ){
	m_aabb_light.origin = aabb.origin;
	m_doom3Radius.m_radiusTransformed = aabb.extents;
}
void transformLightRadius( const Matrix4& transform ){
	matrix4_transform_point( transform, m_aabb_light.origin );
}
void revertTransform(){
	m_aabb_light.origin = m_useLightOrigin ? m_lightOrigin : m_originKey.m_origin;
	rotation_assign( m_rotation, m_useLightRotation ? m_lightRotation : m_rotationKey.m_rotation );
	m_doom3Radius.m_radiusTransformed = m_doom3Radius.m_radius;
	m_radii.m_radii_transformed[0] = m_radii.m_radii[0];
	m_radii.m_radii_transformed[1] = m_radii.m_radii[1];
	m_radii.m_radii_transformed[2] = m_radii.m_radii[2];
}
void freezeTransform(){
	if ( g_lightType == LIGHTTYPE_DOOM3 && !m_useLightOrigin && !m_traverse.empty() ) {
		m_useLightOrigin = true;
	}

	if ( m_useLightOrigin ) {
		m_lightOrigin = m_aabb_light.origin;
		writeLightOrigin();
	}
	else
	{
		m_originKey.m_origin = m_aabb_light.origin;
		m_originKey.write( &m_entity );
	}

	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		if ( !m_useLightRotation && !m_traverse.empty() ) {
			m_useLightRotation = true;
		}

		if ( m_useLightRotation ) {
			rotation_assign( m_lightRotation, m_rotation );
			write_rotation( m_lightRotation, &m_entity, "light_rotation" );
		}

		rotation_assign( m_rotationKey.m_rotation, m_rotation );
		write_rotation( m_rotationKey.m_rotation, &m_entity );

		m_doom3Radius.m_radius = m_doom3Radius.m_radiusTransformed;
		write_origin( m_doom3Radius.m_radius, &m_entity, "light_radius" );
	}
	else{
		write_intensity( m_radii.calculateIntensityFromRadii(), &m_entity );
		m_radii.m_radii[0] = m_radii.m_radii_transformed[0];
		m_radii.m_radii[1] = m_radii.m_radii_transformed[1];
		m_radii.m_radii[2] = m_radii.m_radii_transformed[2];
	}
}
void transformChanged(){
	revertTransform();
	m_evaluateTransform();
	updateOrigin();
}
typedef MemberCaller<Light, &Light::transformChanged> TransformChangedCaller;

mutable Matrix4 m_localPivot;
const Matrix4& getLocalPivot() const {
	m_localPivot = rotation_toMatrix( m_rotation );
	vector4_to_vector3( m_localPivot.t() ) = m_aabb_light.origin;
	return m_localPivot;
}

void setLightChangedCallback( const Callback& callback ){
	m_doom3Radius.m_changed = callback;
}

const AABB& aabb() const {
	m_doom3AABB = AABB( m_aabb_light.origin, m_doom3Radius.m_radiusTransformed );
	return m_doom3AABB;
}
bool testAABB( const AABB& other ) const {
	if ( isProjected() ) {
		Matrix4 transform = rotation();
		vector4_to_vector3( transform.t() ) = localAABB().origin;
		projection();
		Frustum frustum( frustum_transformed( m_doom3Frustum, transform ) );
		return frustum_test_aabb( frustum, other ) != c_volumeOutside;
	}
	// test against an AABB which contains the rotated bounds of this light.
	const AABB& bounds = aabb();
	return aabb_intersects_aabb( other, AABB(
									 bounds.origin,
									 Vector3(
										 static_cast<float>( fabs( m_rotation[0] * bounds.extents[0] )
															 + fabs( m_rotation[3] * bounds.extents[1] )
															 + fabs( m_rotation[6] * bounds.extents[2] ) ),
										 static_cast<float>( fabs( m_rotation[1] * bounds.extents[0] )
															 + fabs( m_rotation[4] * bounds.extents[1] )
															 + fabs( m_rotation[7] * bounds.extents[2] ) ),
										 static_cast<float>( fabs( m_rotation[2] * bounds.extents[0] )
															 + fabs( m_rotation[5] * bounds.extents[1] )
															 + fabs( m_rotation[8] * bounds.extents[2] ) )
										 )
									 ) );
}

const Matrix4& rotation() const {
	m_doom3Rotation = rotation_toMatrix( m_rotation );
	return m_doom3Rotation;
}
const Vector3& offset() const {
	return m_doom3Radius.m_center;
}
const Vector3& colour() const {
	return m_colour.m_colour;
}

bool isProjected() const {
	return m_useLightTarget && m_useLightUp && m_useLightRight;
}
void projectionChanged(){
	m_doom3ProjectionChanged = true;
	m_doom3Radius.m_changed();
	SceneChangeNotify();
}

const Matrix4& projection() const {
	if ( !m_doom3ProjectionChanged ) {
		return m_doom3Projection;
	}
	m_doom3ProjectionChanged = false;
	m_doom3Projection = g_matrix4_identity;
	matrix4_translate_by_vec3( m_doom3Projection, Vector3( 0.5f, 0.5f, 0 ) );
	matrix4_scale_by_vec3( m_doom3Projection, Vector3( 0.5f, 0.5f, 1 ) );

#if 0
	Vector3 right = vector3_cross( m_lightUp, vector3_normalised( m_lightTarget ) );
	Vector3 up = vector3_cross( vector3_normalised( m_lightTarget ), m_lightRight );
	Vector3 target = m_lightTarget;
	Matrix4 test(
		-right.x(), -right.y(), -right.z(), 0,
		-up.x(), -up.y(), -up.z(), 0,
		-target.x(), -target.y(), -target.z(), 0,
		0, 0, 0, 1
		);
	Matrix4 frustum = matrix4_frustum( -0.01, 0.01, -0.01, 0.01, 0.01, 1.0 );
	test = matrix4_full_inverse( test );
	matrix4_premultiply_by_matrix4( test, frustum );
	matrix4_multiply_by_matrix4( m_doom3Projection, test );
#elif 0
	const float nearFar = 1 / 49.5f;
	Vector3 right = vector3_cross( m_lightUp, vector3_normalised( m_lightTarget + m_lightRight ) );
	Vector3 up = vector3_cross( vector3_normalised( m_lightTarget + m_lightUp ), m_lightRight );
	Vector3 target = vector3_negated( m_lightTarget * ( 1 + nearFar ) );
	float scale = -1 / vector3_length( m_lightTarget );
	Matrix4 test(
		-inverse( right.x() ), -inverse( up.x() ), -inverse( target.x() ), 0,
		-inverse( right.y() ), -inverse( up.y() ), -inverse( target.y() ), 0,
		-inverse( right.z() ), -inverse( up.z() ), -inverse( target.z() ), scale,
		0, 0, -nearFar, 0
		);
	matrix4_multiply_by_matrix4( m_doom3Projection, test );
#elif 0
	Vector3 leftA( m_lightTarget - m_lightRight );
	Vector3 leftB( m_lightRight + m_lightUp );
	Plane3 left( vector3_normalised( vector3_cross( leftA, leftB ) ) * ( 1.0 / 128 ), 0 );
	Vector3 rightA( m_lightTarget + m_lightRight );
	Vector3 rightB( vector3_cross( rightA, m_lightTarget ) );
	Plane3 right( vector3_normalised( vector3_cross( rightA, rightB ) ) * ( 1.0 / 128 ), 0 );
	Vector3 bottomA( m_lightTarget - m_lightUp );
	Vector3 bottomB( vector3_cross( bottomA, m_lightTarget ) );
	Plane3 bottom( vector3_normalised( vector3_cross( bottomA, bottomB ) ) * ( 1.0 / 128 ), 0 );
	Vector3 topA( m_lightTarget + m_lightUp );
	Vector3 topB( vector3_cross( topA, m_lightTarget ) );
	Plane3 top( vector3_normalised( vector3_cross( topA, topB ) ) * ( 1.0 / 128 ), 0 );
	Plane3 front( vector3_normalised( m_lightTarget ) * ( 1.0 / 128 ), 1 );
	Plane3 back( vector3_normalised( vector3_negated( m_lightTarget ) ) * ( 1.0 / 128 ), 0 );
	Matrix4 test( matrix4_from_planes( plane3_flipped( left ), plane3_flipped( right ), plane3_flipped( bottom ), plane3_flipped( top ), plane3_flipped( front ), plane3_flipped( back ) ) );
	matrix4_multiply_by_matrix4( m_doom3Projection, test );
#else

	Plane3 lightProject[4];

	Vector3 start = m_useLightStart && m_useLightEnd ? m_lightStart : vector3_normalised( m_lightTarget );
	Vector3 stop = m_useLightStart && m_useLightEnd ? m_lightEnd : m_lightTarget;

	float rLen = vector3_length( m_lightRight );
	Vector3 right = vector3_divided( m_lightRight, rLen );
	float uLen = vector3_length( m_lightUp );
	Vector3 up = vector3_divided( m_lightUp, uLen );
	Vector3 normal = vector3_normalised( vector3_cross( up, right ) );

	float dist = vector3_dot( m_lightTarget, normal );
	if ( dist < 0 ) {
		dist = -dist;
		normal = vector3_negated( normal );
	}

	right *= ( 0.5f * dist ) / rLen;
	up *= -( 0.5f * dist ) / uLen;

	lightProject[2] = Plane3( normal, 0 );
	lightProject[0] = Plane3( right, 0 );
	lightProject[1] = Plane3( up, 0 );

	// now offset to center
	Vector4 targetGlobal( m_lightTarget, 1 );
	{
		float a = vector4_dot( targetGlobal, plane3_to_vector4( lightProject[0] ) );
		float b = vector4_dot( targetGlobal, plane3_to_vector4( lightProject[2] ) );
		float ofs = 0.5f - a / b;
		plane3_to_vector4( lightProject[0] ) += plane3_to_vector4( lightProject[2] ) * ofs;
	}
	{
		float a = vector4_dot( targetGlobal, plane3_to_vector4( lightProject[1] ) );
		float b = vector4_dot( targetGlobal, plane3_to_vector4( lightProject[2] ) );
		float ofs = 0.5f - a / b;
		plane3_to_vector4( lightProject[1] ) += plane3_to_vector4( lightProject[2] ) * ofs;
	}

	// set the falloff vector
	Vector3 falloff = stop - start;
	float length = vector3_length( falloff );
	falloff = vector3_divided( falloff, length );
	if ( length <= 0 ) {
		length = 1;
	}
	falloff *= ( 1.0f / length );
	lightProject[3] = Plane3( falloff, -vector3_dot( start, falloff ) );

	// we want the planes of s=0, s=q, t=0, and t=q
	m_doom3Frustum.left = lightProject[0];
	m_doom3Frustum.bottom = lightProject[1];
	m_doom3Frustum.right = Plane3( lightProject[2].normal() - lightProject[0].normal(), lightProject[2].dist() - lightProject[0].dist() );
	m_doom3Frustum.top = Plane3( lightProject[2].normal() - lightProject[1].normal(), lightProject[2].dist() - lightProject[1].dist() );

	// we want the planes of s=0 and s=1 for front and rear clipping planes
	m_doom3Frustum.front = lightProject[3];

	m_doom3Frustum.back = lightProject[3];
	m_doom3Frustum.back.dist() -= 1.0f;
	m_doom3Frustum.back = plane3_flipped( m_doom3Frustum.back );

	Matrix4 test( matrix4_from_planes( m_doom3Frustum.left, m_doom3Frustum.right, m_doom3Frustum.bottom, m_doom3Frustum.top, m_doom3Frustum.front, m_doom3Frustum.back ) );
	matrix4_multiply_by_matrix4( m_doom3Projection, test );

	m_doom3Frustum.left = plane3_normalised( m_doom3Frustum.left );
	m_doom3Frustum.right = plane3_normalised( m_doom3Frustum.right );
	m_doom3Frustum.bottom = plane3_normalised( m_doom3Frustum.bottom );
	m_doom3Frustum.top = plane3_normalised( m_doom3Frustum.top );
	m_doom3Frustum.back = plane3_normalised( m_doom3Frustum.back );
	m_doom3Frustum.front = plane3_normalised( m_doom3Frustum.front );
#endif
	//matrix4_scale_by_vec3(m_doom3Projection, Vector3(1.0 / 128, 1.0 / 128, 1.0 / 128));
	return m_doom3Projection;
}

const LightRadii& getLightRadii() const {
	return m_radii;
}

Shader* getShader() const {
	return m_shader.get();
}
};

class LightInstance :
	public TargetableInstance,
	public TransformModifier,
	public Renderable,
	public SelectionTestable,
	public RendererLight,
	public PlaneSelectable,
	public ComponentSelectionTestable
{
class TypeCasts
{
InstanceTypeCastTable m_casts;
public:
TypeCasts(){
	m_casts = TargetableInstance::StaticTypeCasts::instance().get();
	InstanceContainedCast<LightInstance, Bounded>::install( m_casts );
	//InstanceContainedCast<LightInstance, Cullable>::install(m_casts);
	InstanceStaticCast<LightInstance, Renderable>::install( m_casts );
	InstanceStaticCast<LightInstance, SelectionTestable>::install( m_casts );
	InstanceStaticCast<LightInstance, Transformable>::install( m_casts );
	InstanceStaticCast<LightInstance, PlaneSelectable>::install( m_casts );
	InstanceStaticCast<LightInstance, ComponentSelectionTestable>::install( m_casts );
	InstanceIdentityCast<LightInstance>::install( m_casts );
}
InstanceTypeCastTable& get(){
	return m_casts;
}
};

Light& m_contained;
DragPlanes m_dragPlanes;  // dragplanes for lightresizing using mousedrag
ScaleRadius m_scaleRadius;
public:
typedef LazyStatic<TypeCasts> StaticTypeCasts;

Bounded& get( NullType<Bounded>){
	return m_contained;
}

STRING_CONSTANT( Name, "LightInstance" );

LightInstance( const scene::Path& path, scene::Instance* parent, Light& contained ) :
	TargetableInstance( path, parent, this, StaticTypeCasts::instance().get(), contained.getEntity(), *this ),
	TransformModifier( Light::TransformChangedCaller( contained ), ApplyTransformCaller( *this ) ),
	m_contained( contained ),
	m_dragPlanes( SelectedChangedComponentCaller( *this ) ),
	m_scaleRadius( SelectedChangedComponentCaller( *this ) ){
	m_contained.instanceAttach( Instance::path() );

	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		GlobalShaderCache().attach( *this );
		m_contained.setLightChangedCallback( LightChangedCaller( *this ) );
	}

	StaticRenderableConnectionLines::instance().attach( *this );
}
~LightInstance(){
	StaticRenderableConnectionLines::instance().detach( *this );

	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_contained.setLightChangedCallback( Callback() );
		GlobalShaderCache().detach( *this );
	}

	m_contained.instanceDetach( Instance::path() );
}
void renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
	m_contained.renderSolid( renderer, volume, Instance::localToWorld(), getSelectable().isSelected() );
}
void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
	m_contained.renderWireframe( renderer, volume, Instance::localToWorld(), getSelectable().isSelected() );
}
void testSelect( Selector& selector, SelectionTest& test ){
	m_contained.testSelect( selector, test, Instance::localToWorld() );
}

void selectPlanes( Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback ){
	test.BeginMesh( localToWorld() );
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_dragPlanes.selectPlanes( m_contained.aabb(), selector, test, selectedPlaneCallback, rotation() );
	}
	else{
		m_scaleRadius.selectPlanes( selector, test, selectedPlaneCallback );
	}
}
void selectReversedPlanes( Selector& selector, const SelectedPlanes& selectedPlanes ){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_dragPlanes.selectReversedPlanes( m_contained.aabb(), selector, selectedPlanes, rotation() );
	}
}

void bestPlaneDirect( SelectionTest& test, Plane3& plane, SelectionIntersection& intersection ){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		test.BeginMesh( localToWorld() );
		m_dragPlanes.bestPlaneDirect( m_contained.aabb(), test, plane, intersection, rotation() );
	}
}
void bestPlaneIndirect( SelectionTest& test, Plane3& plane, Vector3& intersection, float& dist ){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		test.BeginMesh( localToWorld() );
		m_dragPlanes.bestPlaneIndirect( m_contained.aabb(), test, plane, intersection, dist, rotation() );
	}
}
void selectByPlane( const Plane3& plane ){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_dragPlanes.selectByPlane( m_contained.aabb(), plane, rotation() );
	}
}


bool isSelectedComponents() const {
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		return m_dragPlanes.isSelected();
	}
	else{
		return m_scaleRadius.isSelected();
	}
}
void setSelectedComponents( bool select, SelectionSystem::EComponentMode mode ){
	if ( mode == SelectionSystem::eFace ) {
		if ( g_lightType == LIGHTTYPE_DOOM3 ) {
			m_dragPlanes.setSelected( false );
		}
		else{
			m_scaleRadius.setSelected( false );
		}
	}
}
void testSelectComponents( Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode ){
}

void selectedChangedComponent( const Selectable& selectable ){
	GlobalSelectionSystem().getObserver ( SelectionSystem::eComponent )( selectable );
	GlobalSelectionSystem().onComponentSelection( *this, selectable );
}
typedef MemberCaller1<LightInstance, const Selectable&, &LightInstance::selectedChangedComponent> SelectedChangedComponentCaller;

void evaluateTransform(){
	if ( getType() == TRANSFORM_PRIMITIVE ) {
		m_contained.translate( getTranslation() );
		m_contained.rotate( getRotation() );
	}
	else
	{
		//globalOutputStream() << getTranslation() << "\n";
		if ( g_lightType == LIGHTTYPE_DOOM3 ) {
			m_dragPlanes.m_bounds = m_contained.aabb();
			m_contained.setLightRadius( m_dragPlanes.evaluateResize( getTranslation(), rotation() ) );
		}
		else{
			m_contained.setLightRadii( m_contained.getLightRadii().m_radii[1] + m_scaleRadius.evaluateResize( getTranslation() ) );
		}
	}
}
void applyTransform(){
	m_contained.revertTransform();
	evaluateTransform();
	m_contained.freezeTransform();
}
typedef MemberCaller<LightInstance, &LightInstance::applyTransform> ApplyTransformCaller;

void lightChanged(){
	GlobalShaderCache().changed( *this );
}
typedef MemberCaller<LightInstance, &LightInstance::lightChanged> LightChangedCaller;

Shader* getShader() const {
	return m_contained.getShader();
}
const AABB& aabb() const {
	return m_contained.aabb();
}
bool testAABB( const AABB& other ) const {
	return m_contained.testAABB( other );
}
const Matrix4& rotation() const {
	return m_contained.rotation();
}
const Vector3& offset() const {
	return m_contained.offset();
}
const Vector3& colour() const {
	return m_contained.colour();
}

bool isProjected() const {
	return m_contained.isProjected();
}
const Matrix4& projection() const {
	return m_contained.projection();
}
};

class LightNode :
	public scene::Node::Symbiot,
	public scene::Instantiable,
	public scene::Cloneable,
	public scene::Traversable::Observer
{
class TypeCasts
{
NodeTypeCastTable m_casts;
public:
TypeCasts(){
	NodeStaticCast<LightNode, scene::Instantiable>::install( m_casts );
	NodeStaticCast<LightNode, scene::Cloneable>::install( m_casts );
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		NodeContainedCast<LightNode, scene::Traversable>::install( m_casts );
	}
	NodeContainedCast<LightNode, Editable>::install( m_casts );
	NodeContainedCast<LightNode, Snappable>::install( m_casts );
	NodeContainedCast<LightNode, TransformNode>::install( m_casts );
	NodeContainedCast<LightNode, Entity>::install( m_casts );
	NodeContainedCast<LightNode, Nameable>::install( m_casts );
	NodeContainedCast<LightNode, Namespaced>::install( m_casts );
}
NodeTypeCastTable& get(){
	return m_casts;
}
};


scene::Node m_node;
InstanceSet m_instances;
Light m_contained;

void construct(){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_contained.attach( this );
	}
}
void destroy(){
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		m_contained.detach( this );
	}
}
public:
typedef LazyStatic<TypeCasts> StaticTypeCasts;

scene::Traversable& get( NullType<scene::Traversable>){
	return m_contained.getTraversable();
}
Editable& get( NullType<Editable>){
	return m_contained;
}
Snappable& get( NullType<Snappable>){
	return m_contained;
}
TransformNode& get( NullType<TransformNode>){
	return m_contained.getTransformNode();
}
Entity& get( NullType<Entity>){
	return m_contained.getEntity();
}
Nameable& get( NullType<Nameable>){
	return m_contained.getNameable();
}
Namespaced& get( NullType<Namespaced>){
	return m_contained.getNamespaced();
}

LightNode( EntityClass* eclass ) :
	m_node( this, this, StaticTypeCasts::instance().get() ),
	m_contained( eclass, m_node, InstanceSet::TransformChangedCaller( m_instances ), InstanceSet::BoundsChangedCaller( m_instances ), InstanceSetEvaluateTransform<LightInstance>::Caller( m_instances ) ){
	construct();
}
LightNode( const LightNode& other ) :
	scene::Node::Symbiot( other ),
	scene::Instantiable( other ),
	scene::Cloneable( other ),
	scene::Traversable::Observer( other ),
	m_node( this, this, StaticTypeCasts::instance().get() ),
	m_contained( other.m_contained, m_node, InstanceSet::TransformChangedCaller( m_instances ), InstanceSet::BoundsChangedCaller( m_instances ), InstanceSetEvaluateTransform<LightInstance>::Caller( m_instances ) ){
	construct();
}
~LightNode(){
	destroy();
}

void release(){
	delete this;
}
scene::Node& node(){
	return m_node;
}

scene::Node& clone() const {
	return ( new LightNode( *this ) )->node();
}

void insert( scene::Node& child ){
	m_instances.insert( child );
}
void erase( scene::Node& child ){
	m_instances.erase( child );
}

scene::Instance* create( const scene::Path& path, scene::Instance* parent ){
	return new LightInstance( path, parent, m_contained );
}
void forEachInstance( const scene::Instantiable::Visitor& visitor ){
	m_instances.forEachInstance( visitor );
}
void insert( scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance ){
	m_instances.insert( observer, path, instance );
}
scene::Instance* erase( scene::Instantiable::Observer* observer, const scene::Path& path ){
	return m_instances.erase( observer, path );
}
};

void Light_Construct( LightType lightType ){
	g_lightType = lightType;
	if ( g_lightType == LIGHTTYPE_DOOM3 ) {
		LightShader::m_defaultShader = "lights/defaultPointLight";
#if 0
		LightShader::m_defaultShader = "lights/defaultProjectedLight";
#endif
	}
	//RenderLightRadiiFill::m_state = GlobalShaderCache().capture( "$Q3MAP2_LIGHT_SPHERE" );
	RenderLightCenter::m_state = GlobalShaderCache().capture( "$BIGPOINT" );
	sphere_construct_fill( RenderLightRadiiFill::m_radiiPoints );
	sphere_construct_wire( RenderLightRadiiWire::m_radiiPoints );
}
void Light_Destroy(){
	//GlobalShaderCache().release( "$Q3MAP2_LIGHT_SPHERE" );
	GlobalShaderCache().release( "$BIGPOINT" );
}

scene::Node& New_Light( EntityClass* eclass ){
	return ( new LightNode( eclass ) )->node();
}

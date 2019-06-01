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

#include "selection.h"

#include "debugging/debugging.h"

#include <map>
#include <list>
#include <set>

#include "windowobserver.h"
#include "iundo.h"
#include "ientity.h"
#include "cullable.h"
#include "renderable.h"
#include "selectable.h"
#include "editable.h"

#include "math/frustum.h"
#include "signal/signal.h"
#include "generic/object.h"
#include "selectionlib.h"
#include "render.h"
#include "view.h"
#include "renderer.h"
#include "stream/stringstream.h"
#include "eclasslib.h"
#include "generic/bitfield.h"
#include "generic/static.h"
#include "pivot.h"
#include "stringio.h"
#include "container/container.h"

#include "grid.h"

int g_SELECT_EPSILON = 12;

struct Pivot2World
{
	Matrix4 m_worldSpace;
	Matrix4 m_viewpointSpace;
	Matrix4 m_viewplaneSpace;
	Vector3 m_axis_screen;

	void update( const Matrix4& pivot2world, const Matrix4& modelview, const Matrix4& projection, const Matrix4& viewport ){
		Pivot2World_worldSpace( m_worldSpace, pivot2world, modelview, projection, viewport );
		Pivot2World_viewpointSpace( m_viewpointSpace, m_axis_screen, pivot2world, modelview, projection, viewport );
		Pivot2World_viewplaneSpace( m_viewplaneSpace, pivot2world, modelview, projection, viewport );
	}
};


inline Vector3 point_for_device_point( const Matrix4& device2object, const float x, const float y, const float z ){
	// transform from normalised device coords to object coords
	return vector4_projected( matrix4_transformed_vector4( device2object, Vector4( x, y, z, 1 ) ) );
}

inline Ray ray_for_device_point( const Matrix4& device2object, const float x, const float y ){
	return ray_for_points( point_for_device_point( device2object, x, y, -1 ),	// point at x, y, zNear
						 point_for_device_point( device2object, x, y, 0 )		// point at x, y, zFar
						 //point_for_device_point( device2object, x, y, 1 ) //sometimes is inaccurate up to negative ray direction
						);
}

inline Vector3 sphere_intersect_ray( const Vector3& origin, float radius, const Ray& ray ){
	const Vector3 intersection = vector3_subtracted( origin, ray.origin );
	const double a = vector3_dot( intersection, ray.direction );
	const double d = radius * radius - ( vector3_dot( intersection, intersection ) - a * a );

	if ( d > 0 ) {
		return vector3_added( ray.origin, vector3_scaled( ray.direction, a - sqrt( d ) ) );
//		return true;
	}
	else
	{
		return vector3_added( ray.origin, vector3_scaled( ray.direction, a ) );
//		return false;
	}
}

inline Vector3 ray_intersect_ray( const Ray& ray, const Ray& other ){
	const Vector3 intersection = vector3_subtracted( ray.origin, other.origin );
	//float a = 1;//vector3_dot(ray.direction, ray.direction);        // always >= 0
	const double dot = vector3_dot( ray.direction, other.direction );
	//float c = 1;//vector3_dot(other.direction, other.direction);        // always >= 0
	const double d = vector3_dot( ray.direction, intersection );
	const double e = vector3_dot( other.direction, intersection );
	const double D = 1 - dot * dot; //a*c - dot*dot;       // always >= 0

	if ( D < 0.000001 ) {
		// the lines are almost parallel
		return vector3_added( other.origin, vector3_scaled( other.direction, e ) );
	}
	else
	{
		return vector3_added( other.origin, vector3_scaled( other.direction, ( e - dot * d ) / D ) );
	}
}

const Vector3 g_origin( 0, 0, 0 );
const float g_radius = 64;

inline Vector3 point_on_sphere( const Matrix4& device2object, const float x, const float y, const float radius = g_radius ){
	return sphere_intersect_ray( g_origin,
								radius,
								ray_for_device_point( device2object, x, y ) );
}

inline Vector3 point_on_axis( const Vector3& axis, const Matrix4& device2object, const float x, const float y ){
	return ray_intersect_ray( ray_for_device_point( device2object, x, y ),
								Ray( Vector3( 0, 0, 0 ), axis ) );
}

inline Vector3 point_on_plane( const Matrix4& device2object, const float x, const float y ){
	const Matrix4 object2device( matrix4_full_inverse( device2object ) );
	return vector4_projected( matrix4_transformed_vector4( device2object, Vector4( x, y, object2device[14] / object2device[15], 1 ) ) );
}

inline Vector3 point_on_plane( const Plane3& plane, const Matrix4& object2device, const float x, const float y ){
	return ray_intersect_plane( ray_for_device_point( matrix4_full_inverse( object2device ), x, y ),
								plane );
}

//! a and b are unit vectors .. returns angle in radians
inline float angle_between( const Vector3& a, const Vector3& b ){
	return static_cast<float>( 2.0 * atan2(
								   vector3_length( vector3_subtracted( a, b ) ),
								   vector3_length( vector3_added( a, b ) )
								   ) );
}


#if defined( _DEBUG ) && !defined( _DEBUG_QUICKER )
class test_quat
{
public:
test_quat( const Vector3& from, const Vector3& to ){
	Vector4 quaternion( quaternion_for_unit_vectors( from, to ) );
	Matrix4 matrix( matrix4_rotation_for_quaternion( quaternion_multiplied_by_quaternion( quaternion, c_quaternion_identity ) ) );
}
private:
};

static test_quat bleh( g_vector3_axis_x, g_vector3_axis_y );
#endif

//! axis is a unit vector
inline void constrain_to_axis( Vector3& vec, const Vector3& axis ){
	vec = vector3_normalised( vector3_added( vec, vector3_scaled( axis, -vector3_dot( vec, axis ) ) ) );
}

//! a and b are unit vectors .. a and b must be orthogonal to axis .. returns angle in radians
inline float angle_for_axis( const Vector3& a, const Vector3& b, const Vector3& axis ){
	if ( vector3_dot( axis, vector3_cross( a, b ) ) > 0.0 ) {
		return angle_between( a, b );
	}
	else{
		return -angle_between( a, b );
	}
}

inline float distance_for_axis( const Vector3& a, const Vector3& b, const Vector3& axis ){
	return static_cast<float>( vector3_dot( b, axis ) - vector3_dot( a, axis ) );
}

class Manipulatable
{
public:
virtual void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ) = 0;
virtual void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ) = 0;
static const View* m_view;
static float m_device_point[2];
static float m_device_epsilon[2];
static void assign_static( const View& view, const float device_point[2], const float device_epsilon[2] ){
	m_view = &view;
	m_device_point[0] = device_point[0];
	m_device_point[1] = device_point[1];
	m_device_epsilon[0] = device_epsilon[0];
	m_device_epsilon[1] = device_epsilon[1];

}
};
const View* Manipulatable::m_view = 0;
float Manipulatable::m_device_point[2];
float Manipulatable::m_device_epsilon[2];

void transform_local2object( Matrix4& object, const Matrix4& local, const Matrix4& local2object ){
	object = matrix4_multiplied_by_matrix4(
		matrix4_multiplied_by_matrix4( local2object, local ),
		matrix4_full_inverse( local2object )
		);
}

class Rotatable
{
public:
virtual void rotate( const Quaternion& rotation ) = 0;
};

class RotateFree : public Manipulatable
{
Vector3 m_start;
Rotatable& m_rotatable;
public:
RotateFree( Rotatable& rotatable )
	: m_rotatable( rotatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_start = point_on_sphere( device2manip, x, y );
	vector3_normalise( m_start );
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current = point_on_sphere( device2manip, x, y );
	vector3_normalise( current );

	if( snap )
		for( std::size_t i = 0; i < 3; ++i )
			if( current[i] == 0.f )
				return m_rotatable.rotate( quaternion_for_axisangle( g_vector3_axes[i], float_snapped( angle_for_axis( m_start, current, g_vector3_axes[i] ), static_cast<float>( c_pi / 12.0 ) ) ) );

	m_rotatable.rotate( quaternion_for_unit_vectors( m_start, current ) );
//	m_rotatable.rotate( quaternion_for_sphere_vectors( m_start, current ) ); //wrong math, 2x more sensitive
}
};

class RotateAxis : public Manipulatable
{
Vector3 m_axis;
Vector3 m_start;
float m_radius;
bool m_plane_way;
Plane3 m_plane;
Vector3 m_origin;
Rotatable& m_rotatable;
public:
RotateAxis( Rotatable& rotatable )
	: m_radius( g_radius ), m_rotatable( rotatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	const float dot = vector3_dot( m_axis, m_view->fill()? vector3_normalised( m_view->getViewer() - transform_origin ) : m_view->getViewDir() );
	m_plane_way = fabs( dot ) > 0.1;

	if( m_plane_way ){
		m_origin = transform_origin;
		m_plane = Plane3( m_axis, vector3_dot( m_axis, m_origin ) );
		m_start = point_on_plane( m_plane, m_view->GetViewMatrix(), x, y ) - m_origin;
		vector3_normalise( m_start );
	}
	else{
		m_start = point_on_sphere( device2manip, x, y, m_radius );
		constrain_to_axis( m_start, m_axis );
	}
}
/// \brief Converts current position to a normalised vector orthogonal to axis.
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current;
	if( m_plane_way ){
		current = point_on_plane( m_plane, m_view->GetViewMatrix(), x, y ) - m_origin;
		vector3_normalise( current );
	}
	else{
		current = point_on_sphere( device2manip, x, y, m_radius );
		constrain_to_axis( current, m_axis );
	}

	if( snap ){
		m_rotatable.rotate( quaternion_for_axisangle( m_axis, float_snapped( angle_for_axis( m_start, current, m_axis ), static_cast<float>( c_pi / 12.0 ) ) ) );
	}
	else{
		m_rotatable.rotate( quaternion_for_axisangle( m_axis, angle_for_axis( m_start, current, m_axis ) ) );
	}
}

void SetAxis( const Vector3& axis ){
	m_axis = axis;
}
void SetRadius( const float radius ){
	m_radius = radius;
}
};


/// \brief snaps changed axes of \p move so that \p bounds stick to closest grid lines.
void aabb_snap_translation( Vector3& move, const AABB& bounds ){
	const Vector3 maxs( bounds.origin + bounds.extents );
	const Vector3 mins( bounds.origin - bounds.extents );
//	globalOutputStream() << "move: " << move << "\n";
	for( std::size_t i = 0; i < 3; ++i ){
		if( fabs( move[i] ) > 1e-2f ){
			const float snapto1 = float_snapped( maxs[i] + move[i] , GetSnapGridSize() );
			const float snapto2 = float_snapped( mins[i] + move[i] , GetSnapGridSize() );

			const float dist1 = fabs( fabs( maxs[i] + move[i] ) - fabs( snapto1 ) );
			const float dist2 = fabs( fabs( mins[i] + move[i] ) - fabs( snapto2 ) );

//			globalOutputStream() << "maxs[i] + move[i]: " << maxs[i] + move[i]  << "    snapto1: " << snapto1 << "   dist1: " << dist1 << "\n";
//			globalOutputStream() << "mins[i] + move[i]: " << mins[i] + move[i]  << "    snapto2: " << snapto2 << "   dist2: " << dist2 << "\n";
			move[i] = dist2 > dist1 ? snapto1 - maxs[i] : snapto2 - mins[i];
		}
	}
}

void translation_local2object( Vector3& object, const Vector3& local, const Matrix4& local2object ){
	object = matrix4_get_translation_vec3(
		matrix4_multiplied_by_matrix4(
			matrix4_translated_by_vec3( local2object, local ),
			matrix4_full_inverse( local2object )
			)
		);
}

class Translatable
{
public:
virtual void translate( const Vector3& translation ) = 0;
};

class TranslateAxis : public Manipulatable
{
Vector3 m_start;
Vector3 m_axis;
Translatable& m_translatable;
AABB m_bounds;
public:
TranslateAxis( Translatable& translatable )
	: m_translatable( translatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_start = point_on_axis( m_axis, device2manip, x, y );
	m_bounds = bounds;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current = point_on_axis( m_axis, device2manip, x, y );
	current = vector3_scaled( m_axis, distance_for_axis( m_start, current, m_axis ) );

	translation_local2object( current, current, manip2object );
	if( snapbbox )
		aabb_snap_translation( current, m_bounds );
	else
		vector3_snap( current, GetSnapGridSize() );

	m_translatable.translate( current );
}

void SetAxis( const Vector3& axis ){
	m_axis = axis;
}
};

class TranslateAxis2 : public Manipulatable
{
private:
Vector3 m_0;
std::size_t m_axisZ;
Plane3 m_planeZ;
Vector3 m_startZ;
Translatable& m_translatable;
AABB m_bounds;
public:
TranslateAxis2( Translatable& translatable )
	: m_translatable( translatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	Vector3 xydir( m_view->getViewer() - m_0 );
	xydir[m_axisZ] = 0;
	vector3_normalise( xydir );
	m_planeZ = Plane3( xydir, vector3_dot( xydir, m_0 ) );
	m_startZ = point_on_plane( m_planeZ, m_view->GetViewMatrix(), x, y );
	m_bounds = bounds;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current = ( point_on_plane( m_planeZ, m_view->GetViewMatrix(), x, y ) - m_startZ ) * g_vector3_axes[m_axisZ];

	if( snapbbox )
		aabb_snap_translation( current, m_bounds );
	else
		vector3_snap( current, GetSnapGridSize() );

	m_translatable.translate( current );
}
void set0( const Vector3& start, std::size_t axis ){
	m_0 = start;
	m_axisZ = axis;
}
};

class TranslateFree : public Manipulatable
{
private:
Vector3 m_start;
Translatable& m_translatable;
AABB m_bounds;
public:
TranslateFree( Translatable& translatable )
	: m_translatable( translatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_start = point_on_plane( device2manip, x, y );
	m_bounds = bounds;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current = point_on_plane( device2manip, x, y );
	current = vector3_subtracted( current, m_start );

	if( snap )
		current *= g_vector3_axes[vector3_max_abs_component_index( current )];

	translation_local2object( current, current, manip2object );

	if( snapbbox )
		aabb_snap_translation( current, m_bounds );
	else
		vector3_snap( current, GetSnapGridSize() );

	m_translatable.translate( current );
}
};

class TranslateFreeXY_Z : public Manipulatable
{
private:
Vector3 m_0;
std::size_t m_axisZ;
Plane3 m_planeXY;
Plane3 m_planeZ;
Vector3 m_startXY;
Vector3 m_startZ;
Translatable& m_translatable;
AABB m_bounds;
public:
static int m_viewdependent;
TranslateFreeXY_Z( Translatable& translatable )
	: m_translatable( translatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_axisZ = ( m_viewdependent || !m_view->fill() )? vector3_max_abs_component_index( m_view->getViewDir() ) : 2;
	if( m_0 == g_vector3_identity ) /* special value to indicate missing good point to start with, i.e. while dragging components by clicking anywhere; m_startXY, m_startZ != m_0 in this case */
		m_0 = transform_origin;
	m_planeXY = Plane3( g_vector3_axes[m_axisZ], m_0[m_axisZ] );
#if 0
	Vector3 xydir( m_view->getViewDir() );
#else
	Vector3 xydir( m_view->getViewer() - m_0 );
#endif
	xydir[m_axisZ] = 0;
	vector3_normalise( xydir );
	m_planeZ = Plane3( xydir, vector3_dot( xydir, m_0 ) );
	m_startXY = point_on_plane( m_planeXY, m_view->GetViewMatrix(), x, y );
	m_startZ = point_on_plane( m_planeZ, m_view->GetViewMatrix(), x, y );
	m_bounds = bounds;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current;
	if( alt && m_view->fill() )
		current = ( point_on_plane( m_planeZ, m_view->GetViewMatrix(), x, y ) - m_startZ ) * g_vector3_axes[m_axisZ];
	else{
		current = point_on_plane( m_planeXY, m_view->GetViewMatrix(), x, y ) - m_startXY;
		current[m_axisZ] = 0;
	}

	if( snap )
		current *= g_vector3_axes[vector3_max_abs_component_index( current )];

	if( snapbbox )
		aabb_snap_translation( current, m_bounds );
	else
		vector3_snap( current, GetSnapGridSize() );

	m_translatable.translate( current );
}
void set0( const Vector3& start ){
	m_0 = start;
}
};
int TranslateFreeXY_Z::m_viewdependent = 0;

class Scalable
{
public:
virtual void scale( const Vector3& scaling ) = 0;
};


class ScaleAxis : public Manipulatable
{
private:
Vector3 m_start;
Vector3 m_axis;
Scalable& m_scalable;

Vector3 m_choosen_extent;
AABB m_bounds;

public:
ScaleAxis( Scalable& scalable )
	: m_scalable( scalable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_start = point_on_axis( m_axis, device2manip, x, y );

	m_choosen_extent = Vector3(
					std::max( bounds.origin[0] + bounds.extents[0] - transform_origin[0], - bounds.origin[0] + bounds.extents[0] + transform_origin[0] ),
					std::max( bounds.origin[1] + bounds.extents[1] - transform_origin[1], - bounds.origin[1] + bounds.extents[1] + transform_origin[1] ),
					std::max( bounds.origin[2] + bounds.extents[2] - transform_origin[2], - bounds.origin[2] + bounds.extents[2] + transform_origin[2] )
							);
	m_bounds = bounds;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	//globalOutputStream() << "manip2object: " << manip2object << "  device2manip: " << device2manip << "  x: " << x << "  y:" << y <<"\n";
	Vector3 current = point_on_axis( m_axis, device2manip, x, y );
	Vector3 delta = vector3_subtracted( current, m_start );

	translation_local2object( delta, delta, manip2object );
	vector3_snap( delta, GetSnapGridSize() );
	vector3_scale( delta, m_axis );

	Vector3 start( vector3_snapped( m_start, GetSnapGridSize() != 0.f ? GetSnapGridSize() : 1e-3f ) );
	for ( std::size_t i = 0; i < 3 ; ++i ){ //prevent snapping to 0 with big gridsize
		if( float_snapped( m_start[i], 1e-3f ) != 0.f && start[i] == 0.f ){
			start[i] = GetSnapGridSize();
		}
	}
	//globalOutputStream() << "m_start: " << m_start << "   start: " << start << "   delta: " << delta <<"\n";
	/* boundless way */
	Vector3 scale(
		start[0] == 0 ? 1 : 1 + delta[0] / start[0],
		start[1] == 0 ? 1 : 1 + delta[1] / start[1],
		start[2] == 0 ? 1 : 1 + delta[2] / start[2]
		);
	/* try bbox way */
	for( std::size_t i = 0; i < 3; i++ ){
		if( m_choosen_extent[i] > 0.0625f && m_axis[i] != 0.f ){ //epsilon to prevent super high scale for set of models, having really small extent, formed by origins
			scale[i] = ( m_choosen_extent[i] + delta[i] ) / m_choosen_extent[i];
			if( snapbbox ){
				const float snappdwidth = float_snapped( scale[i] * m_bounds.extents[i] * 2.f, GetSnapGridSize() );
				scale[i] = snappdwidth / ( m_bounds.extents[i] * 2.f );
			}
		}
	}
	if( snap ){
		for( std::size_t i = 0; i < 3; i++ ){
			if( m_axis[i] == 0.f ){
				scale[i] = vector3_dot( scale, vector3_scaled( m_axis, m_axis ) );
			}
		}
	}
	//globalOutputStream() << "scale: " << scale <<"\n";
	m_scalable.scale( scale );
}

void SetAxis( const Vector3& axis ){
	m_axis = axis;
}
};

class ScaleFree : public Manipulatable
{
private:
Vector3 m_start;
Vector3 m_axis;
Vector3 m_axis2;
Scalable& m_scalable;

Vector3 m_choosen_extent;
AABB m_bounds;

public:
ScaleFree( Scalable& scalable )
	: m_scalable( scalable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_start = point_on_plane( device2manip, x, y );

	m_choosen_extent = Vector3(
					std::max( bounds.origin[0] + bounds.extents[0] - transform_origin[0], -( bounds.origin[0] - bounds.extents[0] - transform_origin[0] ) ),
					std::max( bounds.origin[1] + bounds.extents[1] - transform_origin[1], -( bounds.origin[1] - bounds.extents[1] - transform_origin[1] ) ),
					std::max( bounds.origin[2] + bounds.extents[2] - transform_origin[2], -( bounds.origin[2] - bounds.extents[2] - transform_origin[2] ) )
							);
	m_bounds = bounds;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current = point_on_plane( device2manip, x, y );
	Vector3 delta = vector3_subtracted( current, m_start );

	translation_local2object( delta, delta, manip2object );
	vector3_snap( delta, GetSnapGridSize() );
	if( m_axis != g_vector3_identity )
		delta = vector3_scaled( delta, m_axis ) + vector3_scaled( delta, m_axis2 );

	Vector3 start( vector3_snapped( m_start, GetSnapGridSize() != 0.f ? GetSnapGridSize() : 1e-3f ) );
	for ( std::size_t i = 0; i < 3 ; ++i ){ //prevent snapping to 0 with big gridsize
		if( float_snapped( m_start[i], 1e-3f ) != 0.f && start[i] == 0.f ){
			start[i] = GetSnapGridSize();
		}
	}

	const std::size_t ignore_axis = vector3_min_abs_component_index( m_start );
	if( snap )
		start[ignore_axis] = 0.f;

	Vector3 scale(
		start[0] == 0 ? 1 : 1 + delta[0] / start[0],
		start[1] == 0 ? 1 : 1 + delta[1] / start[1],
		start[2] == 0 ? 1 : 1 + delta[2] / start[2]
		);

	//globalOutputStream() << "m_start: " << m_start << "   start: " << start << "   delta: " << delta <<"\n";
	for( std::size_t i = 0; i < 3; i++ ){
		if( m_choosen_extent[i] > 0.0625f && start[i] != 0.f ){
			scale[i] = ( m_choosen_extent[i] + delta[i] ) / m_choosen_extent[i];
			if( snapbbox ){
				const float snappdwidth = float_snapped( scale[i] * m_bounds.extents[i] * 2.f, GetSnapGridSize() );
				scale[i] = snappdwidth / ( m_bounds.extents[i] * 2.f );
			}
		}
	}
	//globalOutputStream() << "pre snap scale: " << scale <<"\n";
	if( snap ){
		float bestscale = ignore_axis != 0 ? scale[0] : scale[1];
		for( std::size_t i = ignore_axis != 0 ? 1 : 2; i < 3; i++ ){
			if( ignore_axis != i && fabs( scale[i] ) < fabs( bestscale ) ){
				bestscale = scale[i];
			}
			//globalOutputStream() << "bestscale: " << bestscale <<"\n";
		}
		for( std::size_t i = 0; i < 3; i++ ){
			if( ignore_axis != i ){
				scale[i] = ( scale[i] < 0.f ) ? -fabs( bestscale ) : fabs( bestscale );
			}
		}
	}
	//globalOutputStream() << "scale: " << scale <<"\n";
	m_scalable.scale( scale );
}
void SetAxes( const Vector3& axis, const Vector3& axis2 ){
	m_axis = axis;
	m_axis2 = axis2;
}
};


class Skewable
{
public:
virtual void skew( const Skew& skew ) = 0;
};


class SkewAxis : public Manipulatable
{
private:
Vector3 m_0;
Plane3 m_planeZ;

int m_axis_which;
int m_axis_by;
int m_axis_by_sign;
Skewable& m_skewable;

float m_axis_by_extent;
AABB m_bounds;

public:
SkewAxis( Skewable& skewable )
	: m_skewable( skewable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	Vector3 xydir( m_view->getViewer() - m_0 );
	xydir[m_axis_which] = 0;
//	xydir *= g_vector3_axes[vector3_max_abs_component_index( xydir )];
	vector3_normalise( xydir );
	m_planeZ = Plane3( xydir, vector3_dot( xydir, m_0 ) );

	m_bounds = bounds;
	m_axis_by_extent = bounds.origin[m_axis_by] + bounds.extents[m_axis_by] * m_axis_by_sign - transform_origin[m_axis_by];
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	const Vector3 current = point_on_plane( m_planeZ, m_view->GetViewMatrix(), x, y ) - m_0;
//	globalOutputStream() << m_axis_which << " by axis " << m_axis_by << "\n";
	m_skewable.skew( Skew( m_axis_by * 4 + m_axis_which, m_axis_by_extent != 0.f? float_snapped( current[m_axis_which], GetSnapGridSize() ) / m_axis_by_extent : 0 ) );
}
void SetAxes( int axis_which, int axis_by, int axis_by_sign ){
	m_axis_which = axis_which;
	m_axis_by = axis_by;
	m_axis_by_sign = axis_by_sign;
}
void set0( const Vector3& start ){
	m_0 = start;
}
};

#include "brushmanip.h"

class DragNewBrush : public Manipulatable
{
private:
Vector3 m_0;
Vector3 m_size;
float m_setSizeZ; /* store separately for fine square/cube modes handling */
scene::Node* m_newBrushNode;
public:
DragNewBrush(){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_setSizeZ = m_size[0] = m_size[1] = m_size[2] = GetGridSize();
	m_newBrushNode = 0;
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 diff_raw = point_on_plane( Plane3( g_vector3_axis_z, vector3_dot( g_vector3_axis_z, Vector3( m_size.x(), m_size.y(), m_setSizeZ ) + m_0 ) ), m_view->GetViewMatrix(), x, y ) - m_0;
	const Vector3 xydir( vector3_normalised( Vector3( m_view->GetModelview()[2], m_view->GetModelview()[6], 0 ) ) );
	diff_raw.z() = ( point_on_plane( Plane3( xydir, vector3_dot( xydir, Vector3( m_size.x(), m_size.y(), m_setSizeZ ) + m_0 ) ), m_view->GetViewMatrix(), x, y ) - m_0 ).z();
	Vector3 diff = vector3_snapped( diff_raw, GetSnapGridSize() );

	for ( std::size_t i = 0; i < 3; ++i )
		if( diff[i] == 0 )
			diff[i] = diff_raw[i] < 0? -GetGridSize() : GetGridSize();

	if( alt ){
		diff.x() = m_size.x();
		diff.y() = m_size.y();
	}
	else{
		diff.z() = m_size.z();
	}

	const float z = vector4_projected( matrix4_transformed_vector4( m_view->GetViewMatrix(), Vector4( diff + m_0, 1 ) ) ).z();
	if( z != z || z > 1 ) //catch NAN and behind near, far planes cases
		return;

	if( snap || snapbbox ){
		const float squaresize = std::max( fabs( diff.x() ), fabs( diff.y() ) );
		diff.x() = diff.x() > 0? squaresize : -squaresize; //square
		diff.y() = diff.y() > 0? squaresize : -squaresize;
		if( snapbbox && !alt ) //cube
			diff.z() = diff.z() > 0? squaresize : -squaresize;
	}

	m_size = diff;
	if( alt )
		m_setSizeZ = diff.z();

	Vector3 mins( m_0 );
	Vector3 maxs( m_0 + diff );
	for ( std::size_t i = 0; i < 3; ++i )
		if( mins[i] > maxs[i] )
			std::swap( mins[i], maxs[i] );

	Scene_BrushResize_Cuboid( m_newBrushNode, aabb_for_minmax( mins, maxs ) );
}
void set0( const Vector3& start ){
	m_0 = start;
}
};







class RenderableClippedPrimitive : public OpenGLRenderable
{
struct primitive_t
{
	PointVertex m_points[9];
	std::size_t m_count;
};
Matrix4 m_inverse;
std::vector<primitive_t> m_primitives;
public:
Matrix4 m_world;

void render( RenderStateFlags state ) const {
	for ( std::size_t i = 0; i < m_primitives.size(); ++i )
	{
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_primitives[i].m_points[0].colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_primitives[i].m_points[0].vertex );
		switch ( m_primitives[i].m_count )
		{
		case 1: break;
		case 2: glDrawArrays( GL_LINES, 0, GLsizei( m_primitives[i].m_count ) ); break;
		default: glDrawArrays( GL_POLYGON, 0, GLsizei( m_primitives[i].m_count ) ); break;
		}
	}
}

void construct( const Matrix4& world2device ){
	m_inverse = matrix4_full_inverse( world2device );
	m_world = g_matrix4_identity;
}

void insert( const Vector4 clipped[9], std::size_t count ){
	add_one();

	m_primitives.back().m_count = count;
	for ( std::size_t i = 0; i < count; ++i )
	{
		Vector3 world_point( vector4_projected( matrix4_transformed_vector4( m_inverse, clipped[i] ) ) );
		m_primitives.back().m_points[i].vertex = vertex3f_for_vector3( world_point );
	}
}

void destroy(){
	m_primitives.clear();
}
private:
void add_one(){
	m_primitives.push_back( primitive_t() );

	const Colour4b colour_clipped( 255, 127, 0, 255 );

	for ( std::size_t i = 0; i < 9; ++i )
		m_primitives.back().m_points[i].colour = colour_clipped;
}
};

#if defined( _DEBUG ) && !defined( _DEBUG_QUICKER )
#define DEBUG_SELECTION
#endif

#if defined( DEBUG_SELECTION )
Shader* g_state_clipped;
RenderableClippedPrimitive g_render_clipped;
#endif

typedef Vector3 point_t;
typedef const Vector3* point_iterator_t;

// crossing number test for a point in a polygon
// This code is patterned after [Franklin, 2000]
bool point_test_polygon_2d( const point_t& P, point_iterator_t start, point_iterator_t finish ){
	std::size_t crossings = 0;

	// loop through all edges of the polygon
	for ( point_iterator_t prev = finish - 1, cur = start; cur != finish; prev = cur, ++cur )
	{  // edge from (*prev) to (*cur)
		if ( ( ( ( *prev )[1] <= P[1] ) && ( ( *cur )[1] > P[1] ) ) // an upward crossing
			 || ( ( ( *prev )[1] > P[1] ) && ( ( *cur )[1] <= P[1] ) ) ) { // a downward crossing
			                                                              // compute the actual edge-ray intersect x-coordinate
			float vt = (float)( P[1] - ( *prev )[1] ) / ( ( *cur )[1] - ( *prev )[1] );
			if ( P[0] < ( *prev )[0] + vt * ( ( *cur )[0] - ( *prev )[0] ) ) { // P[0] < intersect
				++crossings; // a valid crossing of y=P[1] right of P[0]
			}
		}
	}
	return ( crossings & 0x1 ) != 0; // 0 if even (out), and 1 if odd (in)
}

inline double triangle_signed_area_XY( const Vector3& p0, const Vector3& p1, const Vector3& p2 ){
	return ( ( p1[0] - p0[0] ) * ( p2[1] - p0[1] ) ) - ( ( p2[0] - p0[0] ) * ( p1[1] - p0[1] ) );
}

enum clipcull_t
{
	eClipCullNone,
	eClipCullCW,
	eClipCullCCW,
};


inline SelectionIntersection select_point_from_clipped( Vector4& clipped ){
	return SelectionIntersection( clipped[2] / clipped[3], static_cast<float>( vector3_length_squared( Vector3( clipped[0] / clipped[3], clipped[1] / clipped[3], 0 ) ) ) );
}

void BestPoint( std::size_t count, Vector4 clipped[9], SelectionIntersection& best, clipcull_t cull, const Plane3* plane = 0 ){
	Vector3 normalised[9];

	{
		for ( std::size_t i = 0; i < count; ++i )
		{
			normalised[i][0] = clipped[i][0] / clipped[i][3];
			normalised[i][1] = clipped[i][1] / clipped[i][3];
			normalised[i][2] = clipped[i][2] / clipped[i][3];
		}
	}

	if ( cull != eClipCullNone && count > 2 ) {
		double signed_area = triangle_signed_area_XY( normalised[0], normalised[1], normalised[2] );

		if ( ( cull == eClipCullCW && signed_area > 0 )
			 || ( cull == eClipCullCCW && signed_area < 0 ) ) {
			return;
		}
	}

	if ( count == 2 ) {
		const Vector3 point = line_closest_point( Line( normalised[0], normalised[1] ), Vector3( 0, 0, 0 ) );
		assign_if_closer( best, SelectionIntersection( point.z(), vector3_length_squared( Vector3( point.x(), point.y(), 0 ) ) ) );
	}
	else if ( count > 2 && !point_test_polygon_2d( Vector3( 0, 0, 0 ), normalised, normalised + count ) ) {
		Plane3 plaine;
		if( !plane ){
			plaine = plane3_for_points( normalised[0], normalised[1], normalised[2] );
			plane = &plaine;
		}
//globalOutputStream() << plane.a << " " << plane.b << " " << plane.c << " " << "\n";
		const point_iterator_t end = normalised + count;
		for ( point_iterator_t previous = end - 1, current = normalised; current != end; previous = current, ++current )
		{
			Vector3 point = line_closest_point( Line( *previous, *current ), Vector3( 0, 0, 0 ) );
			float depth = point.z();
			point.z() = 0;
			float distance = static_cast<float>( vector3_length_squared( point ) );

			if( plane->c == 0 ){
				assign_if_closer( best, SelectionIntersection( depth, distance ) );
			}
			else{
				assign_if_closer( best, SelectionIntersection( depth, distance, ray_distance_to_plane(
										Ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 1 ) ),
										*plane
										) ) );
//										globalOutputStream() << static_cast<float>( ray_distance_to_plane(
//										Ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 1 ) ),
//										plane
//										) ) << "\n";
			}
		}
	}
	else if ( count > 2 ) {
		Plane3 plaine;
		if( !plane ){
			plaine = plane3_for_points( normalised[0], normalised[1], normalised[2] );
			plane = &plaine;
		}
		assign_if_closer(
			best,
			SelectionIntersection(
				ray_distance_to_plane(
										Ray( Vector3( 0, 0, 0 ), Vector3( 0, 0, 1 ) ),
										*plane
										),
				0,
				ray_distance_to_plane(
										Ray( Vector3( 10, 8, 0 ), Vector3( 0, 0, 1 ) ),
										*plane
										)
				)
			);
	}

#if defined( DEBUG_SELECTION )
	if ( count >= 2 ) {
		g_render_clipped.insert( clipped, count );
	}
#endif
}

void Point_BestPoint( const Matrix4& local2view, const PointVertex& vertex, SelectionIntersection& best ){
	Vector4 clipped;
	if ( matrix4_clip_point( local2view, vertex3f_to_vector3( vertex.vertex ), clipped ) == c_CLIP_PASS ) {
		assign_if_closer( best, select_point_from_clipped( clipped ) );
	}
}

void LineStrip_BestPoint( const Matrix4& local2view, const PointVertex* vertices, const std::size_t size, SelectionIntersection& best ){
	Vector4 clipped[2];
	for ( std::size_t i = 0; ( i + 1 ) < size; ++i )
	{
		const std::size_t count = matrix4_clip_line( local2view, vertex3f_to_vector3( vertices[i].vertex ), vertex3f_to_vector3( vertices[i + 1].vertex ), clipped );
		BestPoint( count, clipped, best, eClipCullNone );
	}
}

void LineLoop_BestPoint( const Matrix4& local2view, const PointVertex* vertices, const std::size_t size, SelectionIntersection& best ){
	Vector4 clipped[2];
	for ( std::size_t i = 0; i < size; ++i )
	{
		const std::size_t count = matrix4_clip_line( local2view, vertex3f_to_vector3( vertices[i].vertex ), vertex3f_to_vector3( vertices[( i + 1 ) % size].vertex ), clipped );
		BestPoint( count, clipped, best, eClipCullNone );
	}
}

void Line_BestPoint( const Matrix4& local2view, const PointVertex vertices[2], SelectionIntersection& best ){
	Vector4 clipped[2];
	const std::size_t count = matrix4_clip_line( local2view, vertex3f_to_vector3( vertices[0].vertex ), vertex3f_to_vector3( vertices[1].vertex ), clipped );
	BestPoint( count, clipped, best, eClipCullNone );
}

void Circle_BestPoint( const Matrix4& local2view, clipcull_t cull, const PointVertex* vertices, const std::size_t size, SelectionIntersection& best ){
	Vector4 clipped[9];
	for ( std::size_t i = 0; i < size; ++i )
	{
		const std::size_t count = matrix4_clip_triangle( local2view, g_vector3_identity, vertex3f_to_vector3( vertices[i].vertex ), vertex3f_to_vector3( vertices[( i + 1 ) % size].vertex ), clipped );
		BestPoint( count, clipped, best, cull );
	}
}

void Quad_BestPoint( const Matrix4& local2view, clipcull_t cull, const PointVertex* vertices, SelectionIntersection& best ){
	Vector4 clipped[9];
	{
		const std::size_t count = matrix4_clip_triangle( local2view, vertex3f_to_vector3( vertices[0].vertex ), vertex3f_to_vector3( vertices[1].vertex ), vertex3f_to_vector3( vertices[3].vertex ), clipped );
		BestPoint( count, clipped, best, cull );
	}
	{
		const std::size_t count = matrix4_clip_triangle( local2view, vertex3f_to_vector3( vertices[1].vertex ), vertex3f_to_vector3( vertices[2].vertex ), vertex3f_to_vector3( vertices[3].vertex ), clipped );
		BestPoint( count, clipped, best, cull );
	}
}

void AABB_BestPoint( const Matrix4& local2view, clipcull_t cull, const AABB& aabb, SelectionIntersection& best ){
	const IndexPointer::index_type indices_[24] = {
		2, 1, 5, 6,
		1, 0, 4, 5,
		0, 1, 2, 3,
		3, 7, 4, 0,
		3, 2, 6, 7,
		7, 6, 5, 4,
	};

	Vector3 points[8];
	aabb_corners( aabb, points );

	const IndexPointer indices( indices_, 24 );

	Vector4 clipped[9];
	for ( IndexPointer::iterator i( indices.begin() ); i != indices.end(); i += 4 )
	{
		BestPoint(
			matrix4_clip_triangle(
				local2view,
				points[*i],
				points[*( i + 1 )],
				points[*( i + 3 )],
				clipped
				),
			clipped,
			best,
			cull
			);
		BestPoint(
			matrix4_clip_triangle(
				local2view,
				points[*( i + 1 )],
				points[*( i + 2 )],
				points[*( i + 3 )],
				clipped
				),
			clipped,
			best,
			cull
			);
	}
}

struct FlatShadedVertex
{
	Vertex3f vertex;
	Colour4b colour;
	Normal3f normal;

	FlatShadedVertex(){
	}
};


typedef FlatShadedVertex* FlatShadedVertexIterator;
void Triangles_BestPoint( const Matrix4& local2view, clipcull_t cull, FlatShadedVertexIterator first, FlatShadedVertexIterator last, SelectionIntersection& best ){
	for ( FlatShadedVertexIterator x( first ), y( first + 1 ), z( first + 2 ); x != last; x += 3, y += 3, z += 3 )
	{
		Vector4 clipped[9];
		BestPoint(
			matrix4_clip_triangle(
				local2view,
				reinterpret_cast<const Vector3&>( ( *x ).vertex ),
				reinterpret_cast<const Vector3&>( ( *y ).vertex ),
				reinterpret_cast<const Vector3&>( ( *z ).vertex ),
				clipped
				),
			clipped,
			best,
			cull
			);
	}
}


typedef std::multimap<SelectionIntersection, Selectable*> SelectableSortedSet;

class SelectionPool : public Selector
{
SelectableSortedSet m_pool;
SelectionIntersection m_intersection;
Selectable* m_selectable;

public:
void pushSelectable( Selectable& selectable ){
	m_intersection = SelectionIntersection();
	m_selectable = &selectable;
}
void popSelectable(){
	addSelectable( m_intersection, m_selectable );
	m_intersection = SelectionIntersection();
}
void addIntersection( const SelectionIntersection& intersection ){
	assign_if_closer( m_intersection, intersection );
}
void addSelectable( const SelectionIntersection& intersection, Selectable* selectable ){
	if ( intersection.valid() ) {
		m_pool.insert( SelectableSortedSet::value_type( intersection, selectable ) );
	}
}

typedef SelectableSortedSet::iterator iterator;

iterator begin(){
	return m_pool.begin();
}
iterator end(){
	return m_pool.end();
}

bool failed(){
	return m_pool.empty();
}
};


const Colour4b g_colour_sphere( 0, 0, 0, 255 );
const Colour4b g_colour_screen( 0, 255, 255, 255 );
const Colour4b g_colour_selected( 255, 255, 0, 255 );

inline const Colour4b& colourSelected( const Colour4b& colour, bool selected ){
	return ( selected ) ? g_colour_selected : colour;
}

template<typename remap_policy>
inline void draw_semicircle( const std::size_t segments, const float radius, PointVertex* vertices, remap_policy remap ){
	const double increment = c_pi / double(segments << 2);

	std::size_t count = 0;
	float x = radius;
	float y = 0;
	remap_policy::set( vertices[segments << 2].vertex, -radius, 0, 0 );
	while ( count < segments )
	{
		PointVertex* i = vertices + count;
		PointVertex* j = vertices + ( ( segments << 1 ) - ( count + 1 ) );

		PointVertex* k = i + ( segments << 1 );
		PointVertex* l = j + ( segments << 1 );

#if 0
		PointVertex* m = i + ( segments << 2 );
		PointVertex* n = j + ( segments << 2 );
		PointVertex* o = k + ( segments << 2 );
		PointVertex* p = l + ( segments << 2 );
#endif

		remap_policy::set( i->vertex, x,-y, 0 );
		remap_policy::set( k->vertex,-y,-x, 0 );
#if 0
		remap_policy::set( m->vertex,-x, y, 0 );
		remap_policy::set( o->vertex, y, x, 0 );
#endif

		++count;

		{
			const double theta = increment * count;
			x = static_cast<float>( radius * cos( theta ) );
			y = static_cast<float>( radius * sin( theta ) );
		}

		remap_policy::set( j->vertex, y,-x, 0 );
		remap_policy::set( l->vertex,-x,-y, 0 );
#if 0
		remap_policy::set( n->vertex,-y, x, 0 );
		remap_policy::set( p->vertex, x, y, 0 );
#endif
	}
}

class Manipulator
{
public:
virtual Manipulatable* GetManipulatable() = 0;
virtual void testSelect( const View& view, const Matrix4& pivot2world ) = 0;
virtual void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ){
}
virtual void setSelected( bool select ) = 0;
virtual bool isSelected() const = 0;
};


inline Vector3 normalised_safe( const Vector3& self ){
	if ( vector3_equal( self, g_vector3_identity ) ) {
		return g_vector3_identity;
	}
	return vector3_normalised( self );
}


class RotateManipulator : public Manipulator
{
struct RenderableCircle : public OpenGLRenderable
{
	Array<PointVertex> m_vertices;

	RenderableCircle( std::size_t size ) : m_vertices( size ){
	}
	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_vertices.data()->colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_vertices.data()->vertex );
		glDrawArrays( GL_LINE_LOOP, 0, GLsizei( m_vertices.size() ) );
	}
	void setColour( const Colour4b& colour ){
		for ( Array<PointVertex>::iterator i = m_vertices.begin(); i != m_vertices.end(); ++i )
		{
			( *i ).colour = colour;
		}
	}
};

struct RenderableSemiCircle : public OpenGLRenderable
{
	Array<PointVertex> m_vertices;

	RenderableSemiCircle( std::size_t size ) : m_vertices( size ){
	}
	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_vertices.data()->colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_vertices.data()->vertex );
		glDrawArrays( GL_LINE_STRIP, 0, GLsizei( m_vertices.size() ) );
	}
	void setColour( const Colour4b& colour ){
		for ( Array<PointVertex>::iterator i = m_vertices.begin(); i != m_vertices.end(); ++i )
		{
			( *i ).colour = colour;
		}
	}
};

RotateFree m_free;
RotateAxis m_axis;
Vector3 m_axis_screen;
RenderableSemiCircle m_circle_x;
RenderableSemiCircle m_circle_y;
RenderableSemiCircle m_circle_z;
RenderableCircle m_circle_screen;
RenderableCircle m_circle_sphere;
SelectableBool m_selectable_x;
SelectableBool m_selectable_y;
SelectableBool m_selectable_z;
SelectableBool m_selectable_screen;
SelectableBool m_selectable_sphere;
Selectable* m_selectable_prev_ptr;
Pivot2World m_pivot;
Matrix4 m_local2world_x;
Matrix4 m_local2world_y;
Matrix4 m_local2world_z;
bool m_circle_x_visible;
bool m_circle_y_visible;
bool m_circle_z_visible;
public:
static Shader* m_state_outer;

RotateManipulator( Rotatable& rotatable, std::size_t segments, float radius ) :
	m_free( rotatable ),
	m_axis( rotatable ),
	m_circle_x( ( segments << 2 ) + 1 ),
	m_circle_y( ( segments << 2 ) + 1 ),
	m_circle_z( ( segments << 2 ) + 1 ),
	m_circle_screen( segments << 3 ),
	m_circle_sphere( segments << 3 ),
	m_selectable_prev_ptr( 0 ){
	draw_semicircle( segments, radius, m_circle_x.m_vertices.data(), RemapYZX() );
	draw_semicircle( segments, radius, m_circle_y.m_vertices.data(), RemapZXY() );
	draw_semicircle( segments, radius, m_circle_z.m_vertices.data(), RemapXYZ() );

	draw_circle( segments, radius * 1.15f, m_circle_screen.m_vertices.data(), RemapXYZ() );
	draw_circle( segments, radius, m_circle_sphere.m_vertices.data(), RemapXYZ() );
}


void UpdateColours(){
	m_circle_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
	m_circle_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
	m_circle_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
	m_circle_screen.setColour( colourSelected( g_colour_screen, m_selectable_screen.isSelected() ) );
	m_circle_sphere.setColour( colourSelected( g_colour_sphere, false ) );
}

void updateCircleTransforms(){
	Vector3 localViewpoint( matrix4_transformed_direction( matrix4_transposed( m_pivot.m_worldSpace ), vector4_to_vector3( m_pivot.m_viewpointSpace.z() ) ) );

	m_circle_x_visible = !vector3_equal_epsilon( g_vector3_axis_x, localViewpoint, 1e-6f );
	if ( m_circle_x_visible ) {
		m_local2world_x = g_matrix4_identity;
		vector4_to_vector3( m_local2world_x.y() ) = normalised_safe(
			vector3_cross( g_vector3_axis_x, localViewpoint )
			);
		vector4_to_vector3( m_local2world_x.z() ) = normalised_safe(
			vector3_cross( vector4_to_vector3( m_local2world_x.x() ), vector4_to_vector3( m_local2world_x.y() ) )
			);
		matrix4_premultiply_by_matrix4( m_local2world_x, m_pivot.m_worldSpace );
	}

	m_circle_y_visible = !vector3_equal_epsilon( g_vector3_axis_y, localViewpoint, 1e-6f );
	if ( m_circle_y_visible ) {
		m_local2world_y = g_matrix4_identity;
		vector4_to_vector3( m_local2world_y.z() ) = normalised_safe(
			vector3_cross( g_vector3_axis_y, localViewpoint )
			);
		vector4_to_vector3( m_local2world_y.x() ) = normalised_safe(
			vector3_cross( vector4_to_vector3( m_local2world_y.y() ), vector4_to_vector3( m_local2world_y.z() ) )
			);
		matrix4_premultiply_by_matrix4( m_local2world_y, m_pivot.m_worldSpace );
	}

	m_circle_z_visible = !vector3_equal_epsilon( g_vector3_axis_z, localViewpoint, 1e-6f );
	if ( m_circle_z_visible ) {
		m_local2world_z = g_matrix4_identity;
		vector4_to_vector3( m_local2world_z.x() ) = normalised_safe(
			vector3_cross( g_vector3_axis_z, localViewpoint )
			);
		vector4_to_vector3( m_local2world_z.y() ) = normalised_safe(
			vector3_cross( vector4_to_vector3( m_local2world_z.z() ), vector4_to_vector3( m_local2world_z.x() ) )
			);
		matrix4_premultiply_by_matrix4( m_local2world_z, m_pivot.m_worldSpace );
	}
}

void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ){
	m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
	updateCircleTransforms();

	// temp hack
	UpdateColours();

	renderer.SetState( m_state_outer, Renderer::eWireframeOnly );
	renderer.SetState( m_state_outer, Renderer::eFullMaterials );

	renderer.addRenderable( m_circle_screen, m_pivot.m_viewpointSpace );
	renderer.addRenderable( m_circle_sphere, m_pivot.m_viewpointSpace );

	if ( m_circle_x_visible ) {
		renderer.addRenderable( m_circle_x, m_local2world_x );
	}
	if ( m_circle_y_visible ) {
		renderer.addRenderable( m_circle_y, m_local2world_y );
	}
	if ( m_circle_z_visible ) {
		renderer.addRenderable( m_circle_z, m_local2world_z );
	}
}
void testSelect( const View& view, const Matrix4& pivot2world ){
	m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );
	updateCircleTransforms();

	SelectionPool selector;

	{
		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_local2world_x ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif

			SelectionIntersection best;
			LineStrip_BestPoint( local2view, m_circle_x.m_vertices.data(), m_circle_x.m_vertices.size(), best );
			selector.addSelectable( best, &m_selectable_x );
		}

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_local2world_y ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif

			SelectionIntersection best;
			LineStrip_BestPoint( local2view, m_circle_y.m_vertices.data(), m_circle_y.m_vertices.size(), best );
			selector.addSelectable( best, &m_selectable_y );
		}

		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_local2world_z ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif

			SelectionIntersection best;
			LineStrip_BestPoint( local2view, m_circle_z.m_vertices.data(), m_circle_z.m_vertices.size(), best );
			selector.addSelectable( best, &m_selectable_z );
		}
	}

	{
		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_viewpointSpace ) );

		{
			SelectionIntersection best;
			LineLoop_BestPoint( local2view, m_circle_screen.m_vertices.data(), m_circle_screen.m_vertices.size(), best );
			selector.addSelectable( best, &m_selectable_screen );
		}

//		{
//			SelectionIntersection best;
//			Circle_BestPoint( local2view, eClipCullCW, m_circle_sphere.m_vertices.data(), m_circle_sphere.m_vertices.size(), best );
//			selector.addSelectable( best, &m_selectable_sphere );
//		}
	}

	m_axis_screen = m_pivot.m_axis_screen;

	if ( !selector.failed() ) {
		( *selector.begin() ).second->setSelected( true );
		if( m_selectable_prev_ptr != ( *selector.begin() ).second ){
			m_selectable_prev_ptr = ( *selector.begin() ).second;
			SceneChangeNotify();
		}
	}
	else{
		m_selectable_sphere.setSelected( true );
		if( m_selectable_prev_ptr != &m_selectable_sphere ){
			m_selectable_prev_ptr = &m_selectable_sphere;
			SceneChangeNotify();
		}
	}
}

Manipulatable* GetManipulatable(){
	if ( m_selectable_x.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_x );
		return &m_axis;
	}
	else if ( m_selectable_y.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_y );
		return &m_axis;
	}
	else if ( m_selectable_z.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_z );
		return &m_axis;
	}
	else if ( m_selectable_screen.isSelected() ) {
		m_axis.SetAxis( m_axis_screen );
		return &m_axis;
	}
	else{
		return &m_free;
	}
}

void setSelected( bool select ){
	m_selectable_x.setSelected( select );
	m_selectable_y.setSelected( select );
	m_selectable_z.setSelected( select );
	m_selectable_screen.setSelected( select );
	m_selectable_sphere.setSelected( select );
}
bool isSelected() const {
	return m_selectable_x.isSelected()
		   | m_selectable_y.isSelected()
		   | m_selectable_z.isSelected()
		   | m_selectable_screen.isSelected()
		   | m_selectable_sphere.isSelected();
}
};

Shader* RotateManipulator::m_state_outer;


const float arrowhead_length = 16;
const float arrowhead_radius = 4;

inline void draw_arrowline( const float length, PointVertex* line, const std::size_t axis ){
	( *line++ ).vertex = vertex3f_identity;
	( *line ).vertex = vertex3f_identity;
	vertex3f_to_array( ( *line ).vertex )[axis] = length - arrowhead_length;
}

template<typename VertexRemap, typename NormalRemap>
inline void draw_arrowhead( const std::size_t segments, const float length, FlatShadedVertex* vertices, VertexRemap, NormalRemap ){
	std::size_t head_tris = ( segments << 3 );
	const double head_segment = c_2pi / head_tris;
	for ( std::size_t i = 0; i < head_tris; ++i )
	{
		{
			FlatShadedVertex& point = vertices[i * 6 + 0];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * static_cast<float>( cos( i * head_segment ) );
			VertexRemap::z( point.vertex ) = arrowhead_radius * static_cast<float>( sin( i * head_segment ) );
			NormalRemap::x( point.normal ) = arrowhead_radius / arrowhead_length;
			NormalRemap::y( point.normal ) = static_cast<float>( cos( i * head_segment ) );
			NormalRemap::z( point.normal ) = static_cast<float>( sin( i * head_segment ) );
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 1];
			VertexRemap::x( point.vertex ) = length;
			VertexRemap::y( point.vertex ) = 0;
			VertexRemap::z( point.vertex ) = 0;
			NormalRemap::x( point.normal ) = arrowhead_radius / arrowhead_length;
			NormalRemap::y( point.normal ) = static_cast<float>( cos( ( i + 0.5 ) * head_segment ) );
			NormalRemap::z( point.normal ) = static_cast<float>( sin( ( i + 0.5 ) * head_segment ) );
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 2];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * static_cast<float>( cos( ( i + 1 ) * head_segment ) );
			VertexRemap::z( point.vertex ) = arrowhead_radius * static_cast<float>( sin( ( i + 1 ) * head_segment ) );
			NormalRemap::x( point.normal ) = arrowhead_radius / arrowhead_length;
			NormalRemap::y( point.normal ) = static_cast<float>( cos( ( i + 1 ) * head_segment ) );
			NormalRemap::z( point.normal ) = static_cast<float>( sin( ( i + 1 ) * head_segment ) );
		}

		{
			FlatShadedVertex& point = vertices[i * 6 + 3];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = 0;
			VertexRemap::z( point.vertex ) = 0;
			NormalRemap::x( point.normal ) = -1;
			NormalRemap::y( point.normal ) = 0;
			NormalRemap::z( point.normal ) = 0;
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 4];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * static_cast<float>( cos( i * head_segment ) );
			VertexRemap::z( point.vertex ) = arrowhead_radius * static_cast<float>( sin( i * head_segment ) );
			NormalRemap::x( point.normal ) = -1;
			NormalRemap::y( point.normal ) = 0;
			NormalRemap::z( point.normal ) = 0;
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 5];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * static_cast<float>( cos( ( i + 1 ) * head_segment ) );
			VertexRemap::z( point.vertex ) = arrowhead_radius * static_cast<float>( sin( ( i + 1 ) * head_segment ) );
			NormalRemap::x( point.normal ) = -1;
			NormalRemap::y( point.normal ) = 0;
			NormalRemap::z( point.normal ) = 0;
		}
	}
}

template<typename Triple>
class TripleRemapXYZ
{
public:
static float& x( Triple& triple ){
	return triple.x();
}
static float& y( Triple& triple ){
	return triple.y();
}
static float& z( Triple& triple ){
	return triple.z();
}
};

template<typename Triple>
class TripleRemapYZX
{
public:
static float& x( Triple& triple ){
	return triple.y();
}
static float& y( Triple& triple ){
	return triple.z();
}
static float& z( Triple& triple ){
	return triple.x();
}
};

template<typename Triple>
class TripleRemapZXY
{
public:
static float& x( Triple& triple ){
	return triple.z();
}
static float& y( Triple& triple ){
	return triple.x();
}
static float& z( Triple& triple ){
	return triple.y();
}
};

class ManipulatorSelectionChangeable
{
	Selectable* m_selectable_prev_ptr;
public:
	ManipulatorSelectionChangeable() : m_selectable_prev_ptr( 0 ){
	}
	void selectionChange( SelectionPool& selector ){
		if ( !selector.failed() ) {
			( *selector.begin() ).second->setSelected( true );
			if( m_selectable_prev_ptr != ( *selector.begin() ).second ){
				m_selectable_prev_ptr = ( *selector.begin() ).second;
				SceneChangeNotify();
			}
		}
		else if( m_selectable_prev_ptr ){
			m_selectable_prev_ptr = 0;
			SceneChangeNotify();
		}
	}
};



class TranslateManipulator : public Manipulator, public ManipulatorSelectionChangeable
{
struct RenderableArrowLine : public OpenGLRenderable
{
	PointVertex m_line[2];

	RenderableArrowLine(){
	}
	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_line[0].colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_line[0].vertex );
		glDrawArrays( GL_LINES, 0, 2 );
	}
	void setColour( const Colour4b& colour ){
		m_line[0].colour = colour;
		m_line[1].colour = colour;
	}
};
struct RenderableArrowHead : public OpenGLRenderable
{
	Array<FlatShadedVertex> m_vertices;

	RenderableArrowHead( std::size_t size )
		: m_vertices( size ){
	}
	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( FlatShadedVertex ), &m_vertices.data()->colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( FlatShadedVertex ), &m_vertices.data()->vertex );
		glNormalPointer( GL_FLOAT, sizeof( FlatShadedVertex ), &m_vertices.data()->normal );
		glDrawArrays( GL_TRIANGLES, 0, GLsizei( m_vertices.size() ) );
	}
	void setColour( const Colour4b& colour ){
		for ( Array<FlatShadedVertex>::iterator i = m_vertices.begin(); i != m_vertices.end(); ++i )
		{
			( *i ).colour = colour;
		}
	}
};
struct RenderableQuad : public OpenGLRenderable
{
	PointVertex m_quad[4];
	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_quad[0].colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_quad[0].vertex );
		glDrawArrays( GL_LINE_LOOP, 0, 4 );
	}
	void setColour( const Colour4b& colour ){
		m_quad[0].colour = colour;
		m_quad[1].colour = colour;
		m_quad[2].colour = colour;
		m_quad[3].colour = colour;
	}
};

TranslateFree m_free;
TranslateAxis m_axis;
RenderableArrowLine m_arrow_x;
RenderableArrowLine m_arrow_y;
RenderableArrowLine m_arrow_z;
RenderableArrowHead m_arrow_head_x;
RenderableArrowHead m_arrow_head_y;
RenderableArrowHead m_arrow_head_z;
RenderableQuad m_quad_screen;
SelectableBool m_selectable_x;
SelectableBool m_selectable_y;
SelectableBool m_selectable_z;
SelectableBool m_selectable_screen;
Pivot2World m_pivot;
public:
static Shader* m_state_wire;
static Shader* m_state_fill;

TranslateManipulator( Translatable& translatable, std::size_t segments, float length ) :
	m_free( translatable ),
	m_axis( translatable ),
	m_arrow_head_x( 3 * 2 * ( segments << 3 ) ),
	m_arrow_head_y( 3 * 2 * ( segments << 3 ) ),
	m_arrow_head_z( 3 * 2 * ( segments << 3 ) ){
	draw_arrowline( length, m_arrow_x.m_line, 0 );
	draw_arrowhead( segments, length, m_arrow_head_x.m_vertices.data(), TripleRemapXYZ<Vertex3f>(), TripleRemapXYZ<Normal3f>() );
	draw_arrowline( length, m_arrow_y.m_line, 1 );
	draw_arrowhead( segments, length, m_arrow_head_y.m_vertices.data(), TripleRemapYZX<Vertex3f>(), TripleRemapYZX<Normal3f>() );
	draw_arrowline( length, m_arrow_z.m_line, 2 );
	draw_arrowhead( segments, length, m_arrow_head_z.m_vertices.data(), TripleRemapZXY<Vertex3f>(), TripleRemapZXY<Normal3f>() );

	draw_quad( 16, m_quad_screen.m_quad );
}

void UpdateColours(){
	m_arrow_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
	m_arrow_head_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
	m_arrow_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
	m_arrow_head_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
	m_arrow_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
	m_arrow_head_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
	m_quad_screen.setColour( colourSelected( g_colour_screen, m_selectable_screen.isSelected() ) );
}

bool manipulator_show_axis( const Pivot2World& pivot, const Vector3& axis ){
	return fabs( vector3_dot( pivot.m_axis_screen, axis ) ) < 0.95;
}

void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ){
	m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

	// temp hack
	UpdateColours();

	Vector3 x = vector3_normalised( vector4_to_vector3( m_pivot.m_worldSpace.x() ) );
	bool show_x = manipulator_show_axis( m_pivot, x );

	Vector3 y = vector3_normalised( vector4_to_vector3( m_pivot.m_worldSpace.y() ) );
	bool show_y = manipulator_show_axis( m_pivot, y );

	Vector3 z = vector3_normalised( vector4_to_vector3( m_pivot.m_worldSpace.z() ) );
	bool show_z = manipulator_show_axis( m_pivot, z );

	renderer.SetState( m_state_wire, Renderer::eWireframeOnly );
	renderer.SetState( m_state_wire, Renderer::eFullMaterials );

	if ( show_x ) {
		renderer.addRenderable( m_arrow_x, m_pivot.m_worldSpace );
	}
	if ( show_y ) {
		renderer.addRenderable( m_arrow_y, m_pivot.m_worldSpace );
	}
	if ( show_z ) {
		renderer.addRenderable( m_arrow_z, m_pivot.m_worldSpace );
	}

	renderer.addRenderable( m_quad_screen, m_pivot.m_viewplaneSpace );

	renderer.SetState( m_state_fill, Renderer::eWireframeOnly );
	renderer.SetState( m_state_fill, Renderer::eFullMaterials );

	if ( show_x ) {
		renderer.addRenderable( m_arrow_head_x, m_pivot.m_worldSpace );
	}
	if ( show_y ) {
		renderer.addRenderable( m_arrow_head_y, m_pivot.m_worldSpace );
	}
	if ( show_z ) {
		renderer.addRenderable( m_arrow_head_z, m_pivot.m_worldSpace );
	}
}
void testSelect( const View& view, const Matrix4& pivot2world ){
	m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );

	SelectionPool selector;

	Vector3 x = vector3_normalised( vector4_to_vector3( m_pivot.m_worldSpace.x() ) );
	bool show_x = manipulator_show_axis( m_pivot, x );

	Vector3 y = vector3_normalised( vector4_to_vector3( m_pivot.m_worldSpace.y() ) );
	bool show_y = manipulator_show_axis( m_pivot, y );

	Vector3 z = vector3_normalised( vector4_to_vector3( m_pivot.m_worldSpace.z() ) );
	bool show_z = manipulator_show_axis( m_pivot, z );

	{
		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_viewpointSpace ) );

		{
			SelectionIntersection best;
			Quad_BestPoint( local2view, eClipCullCW, m_quad_screen.m_quad, best );
			if ( best.valid() ) {
				best = SelectionIntersection( 0, 0 );
				selector.addSelectable( best, &m_selectable_screen );
			}
		}
	}

	{
		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_worldSpace ) );

#if defined( DEBUG_SELECTION )
		g_render_clipped.construct( view.GetViewMatrix() );
#endif

		if ( show_x ) {
			SelectionIntersection best;
			Line_BestPoint( local2view, m_arrow_x.m_line, best );
			Triangles_BestPoint( local2view, eClipCullCW, m_arrow_head_x.m_vertices.begin(), m_arrow_head_x.m_vertices.end(), best );
			selector.addSelectable( best, &m_selectable_x );
		}

		if ( show_y ) {
			SelectionIntersection best;
			Line_BestPoint( local2view, m_arrow_y.m_line, best );
			Triangles_BestPoint( local2view, eClipCullCW, m_arrow_head_y.m_vertices.begin(), m_arrow_head_y.m_vertices.end(), best );
			selector.addSelectable( best, &m_selectable_y );
		}

		if ( show_z ) {
			SelectionIntersection best;
			Line_BestPoint( local2view, m_arrow_z.m_line, best );
			Triangles_BestPoint( local2view, eClipCullCW, m_arrow_head_z.m_vertices.begin(), m_arrow_head_z.m_vertices.end(), best );
			selector.addSelectable( best, &m_selectable_z );
		}
	}

	selectionChange( selector );
}

Manipulatable* GetManipulatable(){
	if ( m_selectable_x.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_x );
		return &m_axis;
	}
	else if ( m_selectable_y.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_y );
		return &m_axis;
	}
	else if ( m_selectable_z.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_z );
		return &m_axis;
	}
	else
	{
		return &m_free;
	}
}

void setSelected( bool select ){
	m_selectable_x.setSelected( select );
	m_selectable_y.setSelected( select );
	m_selectable_z.setSelected( select );
	m_selectable_screen.setSelected( select );
}
bool isSelected() const {
	return m_selectable_x.isSelected()
		   | m_selectable_y.isSelected()
		   | m_selectable_z.isSelected()
		   | m_selectable_screen.isSelected();
}
};

Shader* TranslateManipulator::m_state_wire;
Shader* TranslateManipulator::m_state_fill;

class ScaleManipulator : public Manipulator, public ManipulatorSelectionChangeable
{
struct RenderableArrow : public OpenGLRenderable
{
	PointVertex m_line[2];

	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_line[0].colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_line[0].vertex );
		glDrawArrays( GL_LINES, 0, 2 );
	}
	void setColour( const Colour4b& colour ){
		m_line[0].colour = colour;
		m_line[1].colour = colour;
	}
};
struct RenderableQuad : public OpenGLRenderable
{
	PointVertex m_quad[4];
	void render( RenderStateFlags state ) const {
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_quad[0].colour );
		glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_quad[0].vertex );
		glDrawArrays( GL_QUADS, 0, 4 );
	}
	void setColour( const Colour4b& colour ){
		m_quad[0].colour = colour;
		m_quad[1].colour = colour;
		m_quad[2].colour = colour;
		m_quad[3].colour = colour;
	}
};

ScaleFree m_free;
ScaleAxis m_axis;
RenderableArrow m_arrow_x;
RenderableArrow m_arrow_y;
RenderableArrow m_arrow_z;
RenderableQuad m_quad_screen;
SelectableBool m_selectable_x;
SelectableBool m_selectable_y;
SelectableBool m_selectable_z;
SelectableBool m_selectable_screen;
Pivot2World m_pivot;
public:
ScaleManipulator( Scalable& scalable, std::size_t segments, float length ) :
	m_free( scalable ),
	m_axis( scalable ){
	draw_arrowline( length, m_arrow_x.m_line, 0 );
	draw_arrowline( length, m_arrow_y.m_line, 1 );
	draw_arrowline( length, m_arrow_z.m_line, 2 );

	draw_quad( 16, m_quad_screen.m_quad );
}

void UpdateColours(){
	m_arrow_x.setColour( colourSelected( g_colour_x, m_selectable_x.isSelected() ) );
	m_arrow_y.setColour( colourSelected( g_colour_y, m_selectable_y.isSelected() ) );
	m_arrow_z.setColour( colourSelected( g_colour_z, m_selectable_z.isSelected() ) );
	m_quad_screen.setColour( colourSelected( g_colour_screen, m_selectable_screen.isSelected() ) );
}

void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ){
	m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

	// temp hack
	UpdateColours();

	renderer.addRenderable( m_arrow_x, m_pivot.m_worldSpace );
	renderer.addRenderable( m_arrow_y, m_pivot.m_worldSpace );
	renderer.addRenderable( m_arrow_z, m_pivot.m_worldSpace );

	renderer.addRenderable( m_quad_screen, m_pivot.m_viewpointSpace );
}
void testSelect( const View& view, const Matrix4& pivot2world ){
	m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );

	SelectionPool selector;

	{
		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_worldSpace ) );

#if defined( DEBUG_SELECTION )
		g_render_clipped.construct( view.GetViewMatrix() );
#endif

		{
			SelectionIntersection best;
			Line_BestPoint( local2view, m_arrow_x.m_line, best );
			selector.addSelectable( best, &m_selectable_x );
		}

		{
			SelectionIntersection best;
			Line_BestPoint( local2view, m_arrow_y.m_line, best );
			selector.addSelectable( best, &m_selectable_y );
		}

		{
			SelectionIntersection best;
			Line_BestPoint( local2view, m_arrow_z.m_line, best );
			selector.addSelectable( best, &m_selectable_z );
		}
	}

	{
		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_viewpointSpace ) );

		{
			SelectionIntersection best;
			Quad_BestPoint( local2view, eClipCullCW, m_quad_screen.m_quad, best );
			selector.addSelectable( best, &m_selectable_screen );
		}
	}

	selectionChange( selector );
}

Manipulatable* GetManipulatable(){
	if ( m_selectable_x.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_x );
		return &m_axis;
	}
	else if ( m_selectable_y.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_y );
		return &m_axis;
	}
	else if ( m_selectable_z.isSelected() ) {
		m_axis.SetAxis( g_vector3_axis_z );
		return &m_axis;
	}
	else{
		m_free.SetAxes( g_vector3_identity, g_vector3_identity );
		return &m_free;
	}
}

void setSelected( bool select ){
	m_selectable_x.setSelected( select );
	m_selectable_y.setSelected( select );
	m_selectable_z.setSelected( select );
	m_selectable_screen.setSelected( select );
}
bool isSelected() const {
	return m_selectable_x.isSelected()
		   | m_selectable_y.isSelected()
		   | m_selectable_z.isSelected()
		   | m_selectable_screen.isSelected();
}
};


class SkewManipulator : public Manipulator
{
	struct RenderableLine : public OpenGLRenderable {
		PointVertex m_line[2];

		RenderableLine() {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_line[0].colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_line[0].vertex );
			glDrawArrays( GL_LINES, 0, 2 );
		}
		void setColour( const Colour4b& colour ) {
			m_line[0].colour = colour;
			m_line[1].colour = colour;
		}
	};
	struct RenderableArrowHead : public OpenGLRenderable
	{
		Array<FlatShadedVertex> m_vertices;

		RenderableArrowHead( std::size_t size )
			: m_vertices( size ) {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( FlatShadedVertex ), &m_vertices.data()->colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( FlatShadedVertex ), &m_vertices.data()->vertex );
			glNormalPointer( GL_FLOAT, sizeof( FlatShadedVertex ), &m_vertices.data()->normal );
			glDrawArrays( GL_TRIANGLES, 0, GLsizei( m_vertices.size() ) );
		}
		void setColour( const Colour4b & colour ) {
			for( Array<FlatShadedVertex>::iterator i = m_vertices.begin(); i != m_vertices.end(); ++i ) {
				( *i ).colour = colour;
			}
		}
	};
	struct RenderablePoint : public OpenGLRenderable
	{
		PointVertex m_point;
		RenderablePoint():
			m_point( vertex3f_identity ) {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_point.colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_point.vertex );
			glDrawArrays( GL_POINTS, 0, 1 );
		}
		void setColour( const Colour4b & colour ) {
			m_point.colour = colour;
		}
	};

	SkewAxis m_skew;
	TranslateFreeXY_Z m_translateFreeXY_Z;
	ScaleAxis m_scaleAxis;
	ScaleFree m_scaleFree;
	RotateAxis m_rotateAxis;
	AABB m_bounds_draw;
	const AABB& m_bounds;
	Matrix4& m_pivot2world;
	const bool& m_pivotIsCustom;
/*
	RenderableLine m_lineXy_;
	RenderableLine m_lineXy;
	RenderableLine m_lineXz_;
	RenderableLine m_lineXz;
	RenderableLine m_lineYz_;
	RenderableLine m_lineYz;
	RenderableLine m_lineYx_;
	RenderableLine m_lineYx;
	RenderableLine m_lineZx_;
	RenderableLine m_lineZx;
	RenderableLine m_lineZy_;
	RenderableLine m_lineZy;
*/
	RenderableLine m_lines[3][2][2];
	SelectableBool m_selectables[3][2][2];	//[X][YZ][-+]
	SelectableBool m_selectable_translateFree;
	SelectableBool m_selectables_scale[3][2];	//[X][-+]
	SelectableBool m_selectables_rotate[3][2][2];	//[X][-+Y][-+Z]
	Selectable* m_selectable_prev_ptr;
	Selectable* m_selectable_prev_ptr2;
	Pivot2World m_pivot;
	Matrix4 m_worldSpace;
	RenderableArrowHead m_arrow;
	Matrix4 m_arrow_modelview;
	Matrix4 m_arrow_modelview2;
	RenderablePoint m_point;
public:
	static Shader* m_state_wire;
	static Shader* m_state_fill;
	static Shader* m_state_point;
	SkewManipulator( Skewable& skewable, Translatable& translatable, Scalable& scalable, Rotatable& rotatable, const AABB& bounds, Matrix4& pivot2world, const bool& pivotIsCustom, const std::size_t segments = 2 ) :
		m_skew( skewable ),
		m_translateFreeXY_Z( translatable ),
		m_scaleAxis( scalable ),
		m_scaleFree( scalable ),
		m_rotateAxis( rotatable ),
		m_bounds( bounds ),
		m_pivot2world( pivot2world ),
		m_pivotIsCustom( pivotIsCustom ),
		m_selectable_prev_ptr( 0 ),
		m_selectable_prev_ptr2( 0 ),
		m_arrow( 3 * 2 * ( segments << 3 ) ) {
		for ( int i = 0; i < 3; ++i ){
			for ( int j = 0; j < 2; ++j ){
				const int x = i;
				const int y = ( i + j + 1 )%3;
				Vertex3f& xy_ = m_lines[i][j][0].m_line[0].vertex;
				Vertex3f& x_y_ = m_lines[i][j][0].m_line[1].vertex;
				Vertex3f& xy = m_lines[i][j][1].m_line[0].vertex;
				Vertex3f& x_y = m_lines[i][j][1].m_line[1].vertex;
				xy = x_y = xy_ = x_y_ = vertex3f_identity;
				xy[x] = xy_[x] = 1;
				x_y[x] = x_y_[x] = -1;
				xy[y] = x_y[y] = 1;
				xy_[y] = x_y_[y] = -1;
			}
		}
		draw_arrowhead( segments, 0, m_arrow.m_vertices.data(), TripleRemapXYZ<Vertex3f>(), TripleRemapXYZ<Normal3f>() );
		m_arrow.setColour( g_colour_selected );
		m_point.setColour( g_colour_selected );
	}

	void UpdateColours() {
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					m_lines[i][j][k].setColour( colourSelected( g_colour_screen, m_selectables[i][j][k].isSelected() ) );
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				if( m_selectables_scale[i][j].isSelected() ){
					m_lines[(i + 1)%3][1][j].setColour( g_colour_z );
					m_lines[(i + 2)%3][0][j].setColour( g_colour_z );
				}
	}

	void updateModelview( const VolumeTest& volume, const Matrix4& pivot2world ){
		//m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		//m_pivot.update( matrix4_translation_for_vec3( matrix4_get_translation_vec3( pivot2world ) ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		m_pivot.update( matrix4_translation_for_vec3( m_bounds.origin ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
		//m_pivot.update( g_matrix4_identity, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() ); //no shaking in cam due to low precision this way; smooth and sometimes very incorrect result
//		globalOutputStream() << m_pivot.m_worldSpace << "\n";
		Matrix4& m = m_pivot.m_worldSpace; /* go affine to increase precision */
		m[1] = m[2] = m[3] = m[4] = m[6] = m[7] = m[8] = m[9] = m[11] = 0;
		m[15] = 1;
		m_bounds_draw = aabb_for_oriented_aabb( m_bounds, matrix4_affine_inverse( m_pivot.m_worldSpace ) ); //screen scale
		for ( int i = 0; i < 3; ++i ){
			if( m_bounds_draw.extents[i] < 16 )
				m_bounds_draw.extents[i] = 18;
			else
				m_bounds_draw.extents[i] += 2.0f;
		}
		m_bounds_draw = aabb_for_oriented_aabb( m_bounds_draw, m_pivot.m_worldSpace ); //world scale
		m_bounds_draw.origin = m_bounds.origin;

		m_worldSpace = matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( m_bounds_draw.origin ), matrix4_scale_for_vec3( m_bounds_draw.extents ) );
		matrix4_premultiply_by_matrix4( m_worldSpace, matrix4_translation_for_vec3( -matrix4_get_translation_vec3( pivot2world ) ) );
		matrix4_premultiply_by_matrix4( m_worldSpace, pivot2world );

//		globalOutputStream() << m_worldSpace << "\n";
//		globalOutputStream() << pivot2world << "\n";
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) {
		updateModelview( volume, pivot2world );

		// temp hack
		UpdateColours();

		renderer.SetState( m_state_wire, Renderer::eWireframeOnly );
		renderer.SetState( m_state_wire, Renderer::eFullMaterials );

		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j ){
#if 0
				const Vector3 dir = ( m_lines[i][j][0].m_line[0].vertex - m_lines[i][j][1].m_line[0].vertex ) / 2;
				const float dot = vector3_dot( dir, m_pivot.m_axis_screen );
				if( dot > 0.9999f )
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
				else if( dot < -0.9999f )
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
				else{
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
				}
#else
				if( m_selectables[i][j][0].isSelected() ){ /* add selected last to get highlighted one rendered on top in 2d */
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
				}
				else{
					renderer.addRenderable( m_lines[i][j][0], m_worldSpace );
					renderer.addRenderable( m_lines[i][j][1], m_worldSpace );
				}
#endif
			}

		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					if( m_selectables[i][j][k].isSelected() ){
						Vector3 origin = matrix4_transformed_point( m_worldSpace, m_lines[i][j][k].m_line[0].vertex );
						Vector3 origin2 = matrix4_transformed_point( m_worldSpace, m_lines[i][j][k].m_line[1].vertex );

						Pivot2World_worldSpace( m_arrow_modelview, matrix4_translation_for_vec3( origin ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
						Pivot2World_worldSpace( m_arrow_modelview2, matrix4_translation_for_vec3( origin2 ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

						const Matrix4 rot( i == 0? g_matrix4_identity: i == 1? matrix4_rotation_for_sincos_z( 1, 0 ): matrix4_rotation_for_sincos_y( -1, 0 ) );
						matrix4_multiply_by_matrix4( m_arrow_modelview, rot );
						matrix4_multiply_by_matrix4( m_arrow_modelview2, rot );
						const float x = 0.7f;
						matrix4_multiply_by_matrix4( m_arrow_modelview, matrix4_scale_for_vec3( Vector3( x, x, x ) ) );
						matrix4_multiply_by_matrix4( m_arrow_modelview2, matrix4_scale_for_vec3( Vector3( -x, x, x ) ) );

						renderer.SetState( m_state_fill, Renderer::eWireframeOnly );
						renderer.SetState( m_state_fill, Renderer::eFullMaterials );
						renderer.addRenderable( m_arrow, m_arrow_modelview );
						renderer.addRenderable( m_arrow, m_arrow_modelview2 );
						return;
					}

		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					if( m_selectables_rotate[i][j][k].isSelected() ){
						renderer.SetState( m_state_point, Renderer::eWireframeOnly );
						renderer.SetState( m_state_point, Renderer::eFullMaterials );
						renderer.addRenderable( m_point, m_worldSpace );
						renderer.addRenderable( m_point, m_worldSpace );
						return;
					}
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) {
		updateModelview( view, pivot2world );

		SelectionPool selector;

		const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_worldSpace ) );
		/* try corner points to rotate */
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k ){
					m_point.m_point.vertex[i] = 0;
					m_point.m_point.vertex[(i + 1)%3] = j? 1 : -1;
					m_point.m_point.vertex[(i + 2)%3] = k? 1 : -1;
					SelectionIntersection best;
					Point_BestPoint( local2view, m_point.m_point, best );
					selector.addSelectable( best, &m_selectables_rotate[i][j][k] );
				}
		if( !selector.failed() ) {
			( *selector.begin() ).second->setSelected( true );
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					for ( int k = 0; k < 2; ++k )
						if( m_selectables_rotate[i][j][k].isSelected() ){
							m_point.m_point.vertex[i] = 0;
							m_point.m_point.vertex[(i + 1)%3] = j? 1 : -1;
							m_point.m_point.vertex[(i + 2)%3] = k? 1 : -1;
							if( !m_pivotIsCustom ){
								const Vector3 origin = m_bounds.origin + m_point.m_point.vertex * -1 * m_bounds.extents;
								m_pivot2world = matrix4_translation_for_vec3( origin );
							}
							/* set radius */
							if( fabs( vector3_dot( m_pivot.m_axis_screen, g_vector3_axes[i] ) ) < 0.2 ){
								Vector3 origin = matrix4_get_translation_vec3( m_pivot2world );
								Vector3 point = m_bounds_draw.origin + m_point.m_point.vertex * m_bounds_draw.extents;
								const Matrix4 inv = matrix4_affine_inverse( m_pivot.m_worldSpace );
								matrix4_transform_point( inv, origin );
								matrix4_transform_point( inv, point );
								point -= origin;
								point = vector3_added( point, vector3_scaled( m_pivot.m_axis_screen, -vector3_dot( point, m_pivot.m_axis_screen ) ) ); //constrain_to_axis
								m_rotateAxis.SetRadius( vector3_length( point ) - g_SELECT_EPSILON / 2.0 - 1.0 ); /* use smaller radius to constrain to one rotation direction in 2D */
								//globalOutputStream() << "radius " << ( vector3_length( point ) - g_SELECT_EPSILON / 2.0 - 1.0 ) << "\n";
							}
							else{
								m_rotateAxis.SetRadius( g_radius );
								//globalOutputStream() << "g_radius\n";
							}
						}
		}
		else{
			/* try lines to skew */
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					for ( int k = 0; k < 2; ++k ){
						SelectionIntersection best;
						Line_BestPoint( local2view, m_lines[i][j][k].m_line, best );
						selector.addSelectable( best, &m_selectables[i][j][k] );
					}

			if( !selector.failed() ) {
				( *selector.begin() ).second->setSelected( true );
				m_skew.set0( vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( view.GetViewMatrix() ), Vector4( 0, 0, selector.begin()->first.depth(), 1 ) ) ) );
				if( !m_pivotIsCustom )
					for ( int i = 0; i < 3; ++i )
						for ( int j = 0; j < 2; ++j )
							for ( int k = 0; k < 2; ++k )
								if( m_selectables[i][j][k].isSelected() ){
									const int axis_by = ( i + j + 1 ) % 3;
									Vector3 origin = m_bounds.origin;
									origin[axis_by] += k? -m_bounds.extents[axis_by] : m_bounds.extents[axis_by];
									m_pivot2world = matrix4_translation_for_vec3( origin );
								}
			}
			else{ /* try bbox to translate */
				SelectionIntersection best;
				AABB_BestPoint( local2view, eClipCullCW, AABB( Vector3( 0, 0, 0 ), Vector3( 1, 1, 1 ) ), best );
				selector.addSelectable( best, &m_selectable_translateFree );
				if( !selector.failed() )
					m_translateFreeXY_Z.set0( vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( view.GetViewMatrix() ), Vector4( 0, 0, selector.begin()->first.depth(), 1 ) ) ) );
			}
		}

		/* try bbox planes to scale*/
		if( selector.failed() ){
			const Matrix4 screen2world( matrix4_full_inverse( view.GetViewMatrix() ) );

			Vector3 corners[8];
			aabb_corners( m_bounds_draw, corners );

			const int indices[24] = {
				3, 7, 4, 0, //-x
				2, 1, 5, 6, //+x
				3, 2, 6, 7, //-y
				1, 0, 4, 5, //+y
				7, 6, 5, 4, //-z
				0, 1, 2, 3, //+z
			};

			Selectable* selectable = 0;
			Selectable* selectable2 = 0;
			double bestDot = 1;
			const Vector3 viewdir( view.getViewDir() );
			for ( int i = 0; i < 3; ++i ){
				for ( int j = 0; j < 2; ++j ){
					const Vector3 normal = j? g_vector3_axes[i] : -g_vector3_axes[i];
					const Vector3 centroid = m_bounds.origin + m_bounds.extents * normal;
					const Vector3 projected = vector4_projected( matrix4_transformed_vector4( view.GetViewMatrix(), Vector4( centroid, 1 ) ) );
					const Vector3 closest_point = vector4_projected( matrix4_transformed_vector4( screen2world, Vector4( 0, 0, projected[2], 1 ) ) );

					const int index = i * 8 + j * 4;
					if( vector3_dot( normal, closest_point - corners[indices[index]] ) > 0
						&& vector3_dot( normal, closest_point - corners[indices[index + 1]] ) > 0
						&& vector3_dot( normal, closest_point - corners[indices[index + 2]] ) > 0
						&& vector3_dot( normal, closest_point - corners[indices[index + 3]] ) > 0 )
					{
						const double dot = fabs( vector3_dot( normal, viewdir ) );
						const double diff = bestDot - dot;
						if( diff > 0.03 ){
							bestDot = dot;
							selectable = &m_selectables_scale[i][j];
							selectable2 = 0;
						}
						else if( fabs( diff ) <= 0.03 ){
							selectable2 = &m_selectables_scale[i][j];
						}
					}
				}
			}
//			if( view.GetViewMatrix().xw() != 0 || view.GetViewMatrix().yw() != 0 )
			if( view.fill() ) // select only plane in camera
				selectable2 = 0;
			if( selectable ){
				Vector3 origin = m_bounds.origin;
				for ( int i = 0; i < 3; ++i )
					for ( int j = 0; j < 2; ++j )
						if( &m_selectables_scale[i][j] == selectable || &m_selectables_scale[i][j] == selectable2 ){
							m_selectables_scale[i][j].setSelected( true );
							origin[i] += j? -m_bounds.extents[i] : m_bounds.extents[i];
						}
				if( !m_pivotIsCustom )
					m_pivot2world = matrix4_translation_for_vec3( origin );
				if( m_selectable_prev_ptr != selectable || m_selectable_prev_ptr2 != selectable2 ){
					m_selectable_prev_ptr = selectable;
					m_selectable_prev_ptr2 = selectable2;
					SceneChangeNotify();
				}
				return;
			}

		}

		if( !selector.failed() ) {
			( *selector.begin() ).second->setSelected( true );
			if( m_selectable_prev_ptr != ( *selector.begin() ).second ) {
				m_selectable_prev_ptr = ( *selector.begin() ).second;
				m_selectable_prev_ptr2 = 0;
				SceneChangeNotify();
			}
		}
		else if( m_selectable_prev_ptr ) {
			m_selectable_prev_ptr = 0;
			m_selectable_prev_ptr2 = 0;
			SceneChangeNotify();
		}
	}

	Manipulatable* GetManipulatable() {
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k )
					if( m_selectables[i][j][k].isSelected() ){
						m_skew.SetAxes( i, ( i + j + 1 ) % 3, k? 1 : -1 );
						return &m_skew;
					}
					else if( m_selectables_rotate[i][j][k].isSelected() ){
						m_rotateAxis.SetAxis( g_vector3_axes[i] );
						return &m_rotateAxis;
					}
		{
			Vector3 axes[2] = { g_vector3_identity, g_vector3_identity };
			Vector3* axis = axes;
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 2; ++j )
					if( m_selectables_scale[i][j].isSelected() )
						(*axis++)[i] = j? 1 : -1;
			if( m_selectable_prev_ptr2 ){
				m_scaleFree.SetAxes( axes[0], axes[1] );
				return &m_scaleFree;
			}
			else if( axis != axes ){
				m_scaleAxis.SetAxis( axes[0] );
				return &m_scaleAxis;
			}
		}
		return &m_translateFreeXY_Z;
	}

	void setSelected( bool select ) {
		m_selectable_translateFree.setSelected( select );
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k ){
					m_selectables[i][j][k].setSelected( select );
					m_selectables_rotate[i][j][k].setSelected( select );
				}
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				 m_selectables_scale[i][j].setSelected( select );
	}
	bool isSelected() const {
		bool selected = false;
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				for ( int k = 0; k < 2; ++k ){
					selected |= m_selectables[i][j][k].isSelected();
					selected |= m_selectables_rotate[i][j][k].isSelected();
				}
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 2; ++j )
				 selected |= m_selectables_scale[i][j].isSelected();
		return selected | m_selectable_translateFree.isSelected();
	}
};

Shader* SkewManipulator::m_state_wire;
Shader* SkewManipulator::m_state_fill;
Shader* SkewManipulator::m_state_point;



inline PlaneSelectable* Instance_getPlaneSelectable( scene::Instance& instance ){
	return InstanceTypeCast<PlaneSelectable>::cast( instance );
}

class PlaneSelectableSelectPlanes : public scene::Graph::Walker
{
Selector& m_selector;
SelectionTest& m_test;
PlaneCallback m_selectedPlaneCallback;
public:
PlaneSelectableSelectPlanes( Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback )
	: m_selector( selector ), m_test( test ), m_selectedPlaneCallback( selectedPlaneCallback ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->selectPlanes( m_selector, m_test, m_selectedPlaneCallback );
			}
		}
	}
	return true;
}
};

class PlaneSelectableSelectReversedPlanes : public scene::Graph::Walker
{
Selector& m_selector;
const SelectedPlanes& m_selectedPlanes;
public:
PlaneSelectableSelectReversedPlanes( Selector& selector, const SelectedPlanes& selectedPlanes )
	: m_selector( selector ), m_selectedPlanes( selectedPlanes ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->selectReversedPlanes( m_selector, m_selectedPlanes );
			}
		}
	}
	return true;
}
};

void Scene_forEachPlaneSelectable_selectPlanes( scene::Graph& graph, Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback ){
	graph.traverse( PlaneSelectableSelectPlanes( selector, test, selectedPlaneCallback ) );
}

void Scene_forEachPlaneSelectable_selectReversedPlanes( scene::Graph& graph, Selector& selector, const SelectedPlanes& selectedPlanes ){
	graph.traverse( PlaneSelectableSelectReversedPlanes( selector, selectedPlanes ) );
}


class PlaneLess
{
public:
bool operator()( const Plane3& plane, const Plane3& other ) const {
	if ( plane.a < other.a ) {
		return true;
	}
	if ( other.a < plane.a ) {
		return false;
	}

	if ( plane.b < other.b ) {
		return true;
	}
	if ( other.b < plane.b ) {
		return false;
	}

	if ( plane.c < other.c ) {
		return true;
	}
	if ( other.c < plane.c ) {
		return false;
	}

	if ( plane.d < other.d ) {
		return true;
	}
	if ( other.d < plane.d ) {
		return false;
	}

	return false;
}
};

typedef std::set<Plane3, PlaneLess> PlaneSet;

inline void PlaneSet_insert( PlaneSet& self, const Plane3& plane ){
	self.insert( plane );
}

inline bool PlaneSet_contains( const PlaneSet& self, const Plane3& plane ){
	return self.find( plane ) != self.end();
}


class SelectedPlaneSet : public SelectedPlanes
{
PlaneSet m_selectedPlanes;
public:
bool empty() const {
	return m_selectedPlanes.empty();
}

void insert( const Plane3& plane ){
	PlaneSet_insert( m_selectedPlanes, plane );
}
bool contains( const Plane3& plane ) const {
	return PlaneSet_contains( m_selectedPlanes, plane );
}
typedef MemberCaller1<SelectedPlaneSet, const Plane3&, &SelectedPlaneSet::insert> InsertCaller;
};


bool Scene_forEachPlaneSelectable_selectPlanes( scene::Graph& graph, Selector& selector, SelectionTest& test ){
	SelectedPlaneSet selectedPlanes;

	Scene_forEachPlaneSelectable_selectPlanes( graph, selector, test, SelectedPlaneSet::InsertCaller( selectedPlanes ) );
	Scene_forEachPlaneSelectable_selectReversedPlanes( graph, selector, selectedPlanes );

	return !selectedPlanes.empty();
}




class PlaneSelectable_bestPlaneDirect : public scene::Graph::Walker
{
SelectionTest& m_test;
Plane3& m_plane;
mutable SelectionIntersection m_intersection;
public:
PlaneSelectable_bestPlaneDirect( SelectionTest& test, Plane3& plane )
	: m_test( test ), m_plane( plane ), m_intersection(){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->bestPlaneDirect( m_test, m_plane, m_intersection );
			}
		}
	}
	return true;
}
};
class PlaneSelectable_bestPlaneIndirect : public scene::Graph::Walker
{
SelectionTest& m_test;
Plane3& m_plane;
Vector3& m_intersection;
mutable float m_dist;
public:
PlaneSelectable_bestPlaneIndirect( SelectionTest& test, Plane3& plane, Vector3& intersection )
	: m_test( test ), m_plane( plane ), m_intersection( intersection ), m_dist( FLT_MAX ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->bestPlaneIndirect( m_test, m_plane, m_intersection, m_dist );
			}
		}
	}
	return true;
}
};

class PlaneSelectable_selectByPlane : public scene::Graph::Walker
{
const Plane3 m_plane;
public:
PlaneSelectable_selectByPlane( const Plane3& plane )
	: m_plane( plane ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->selectByPlane( m_plane );
			}
		}
	}
	return true;
}
};

bool Scene_forEachPlaneSelectable_selectPlanes2( scene::Graph& graph, SelectionTest& test, TranslateAxis2& translateAxis ){
	Plane3 plane( 0, 0, 0, 0 );
	graph.traverse( PlaneSelectable_bestPlaneDirect( test, plane ) );
	if( plane3_valid( plane ) ){
		test.BeginMesh( g_matrix4_identity );
		translateAxis.set0( point_on_plane( plane, test.getVolume().GetViewMatrix(), 0, 0 ), vector3_max_abs_component_index( plane.normal() ) );
	}
	else{
		Vector3 intersection;
		graph.traverse( PlaneSelectable_bestPlaneIndirect( test, plane, intersection ) );
		if( plane3_valid( plane ) ){
			test.BeginMesh( g_matrix4_identity );
			/* may introduce some screen space offset in manipulatable to handle far-from-edge clicks perfectly; thought clicking not so far isn't too nasty, right? */
			translateAxis.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( intersection, 1 ) ) ), vector3_max_abs_component_index( plane.normal() ) );
		}
	}
	if( plane3_valid( plane ) ){
		graph.traverse( PlaneSelectable_selectByPlane( plane ) );
	}
	return plane3_valid( plane );
}


#include "brush.h"

class TestedBrushFacesSelectVeritces : public scene::Graph::Walker
{
SelectionTest& m_test;
public:
TestedBrushFacesSelectVeritces( SelectionTest& test )
	: m_test( test ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			BrushInstance* brushInstance = Instance_getBrush( instance );
			if ( brushInstance != 0 ) {
				brushInstance->selectVerticesOnTestedFaces( m_test );
			}
		}
	}
	return true;
}
};

void Scene_forEachTestedBrushFace_selectVertices( scene::Graph& graph, SelectionTest& test ){
	graph.traverse( TestedBrushFacesSelectVeritces( test ) );
}

class BrushPlanesSelectVeritces : public scene::Graph::Walker
{
SelectionTest& m_test;
public:
BrushPlanesSelectVeritces( SelectionTest& test )
	: m_test( test ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 && selectable->isSelected() ) {
			BrushInstance* brushInstance = Instance_getBrush( instance );
			if ( brushInstance != 0 ) {
				brushInstance->selectVerticesOnPlanes( m_test );
			}
		}
	}
	return true;
}
};

void Scene_forEachBrushPlane_selectVertices( scene::Graph& graph, SelectionTest& test ){
	graph.traverse( BrushPlanesSelectVeritces( test ) );
}

void Scene_Translate_Component_Selected( scene::Graph& graph, const Vector3& translation );
void Scene_Translate_Selected( scene::Graph& graph, const Vector3& translation );
void Scene_TestSelect_Primitive( Selector& selector, SelectionTest& test, const VolumeTest& volume );
void Scene_TestSelect_Component( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode );
void Scene_TestSelect_Component_Selected( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode );
void Scene_SelectAll_Component( bool select, SelectionSystem::EComponentMode componentMode );

class ResizeTranslatable : public Translatable
{
void translate( const Vector3& translation ){
	Scene_Translate_Component_Selected( GlobalSceneGraph(), translation );
}
};


class SelectionVolume : public SelectionTest
{
Matrix4 m_local2view;
const View& m_view;
clipcull_t m_cull;
#if 0
Vector3 m_near;
Vector3 m_far;
#endif
Matrix4 m_screen2world;
public:
SelectionVolume( const View& view )
	: m_view( view ){
}

const VolumeTest& getVolume() const {
	return m_view;
}
#if 0
const Vector3& getNear() const {
	return m_near;
}
const Vector3& getFar() const {
	return m_far;
}
#endif
const Matrix4& getScreen2world() const {
	return m_screen2world;
}

void BeginMesh( const Matrix4& localToWorld, bool twoSided ){
	m_local2view = matrix4_multiplied_by_matrix4( m_view.GetViewMatrix(), localToWorld );

	// Cull back-facing polygons based on winding being clockwise or counter-clockwise.
	// Don't cull if the view is wireframe and the polygons are two-sided.
	m_cull = twoSided && !m_view.fill() ? eClipCullNone : ( matrix4_handedness( localToWorld ) == MATRIX4_RIGHTHANDED ) ? eClipCullCW : eClipCullCCW;

	{
		m_screen2world = matrix4_full_inverse( m_local2view );
#if 0
		m_near = vector4_projected(
			matrix4_transformed_vector4(
				m_screen2world,
				Vector4( 0, 0, -1, 1 )
				)
			);

		m_far = vector4_projected(
			matrix4_transformed_vector4(
				m_screen2world,
				Vector4( 0, 0, 1, 1 )
				)
			);
#endif
	}

#if defined( DEBUG_SELECTION )
	g_render_clipped.construct( m_view.GetViewMatrix() );
#endif
}
void TestPoint( const Vector3& point, SelectionIntersection& best ){
	Vector4 clipped;
	if ( matrix4_clip_point( m_local2view, point, clipped ) == c_CLIP_PASS ) {
		best = select_point_from_clipped( clipped );
	}
}
void TestPolygon( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best, const DoubleVector3 planepoints[3] ){
	DoubleVector3 pts[3];
	pts[0] = vector4_projected( matrix4_transformed_vector4( m_local2view, BasicVector4<double>( planepoints[0], 1 ) ) );
	pts[1] = vector4_projected( matrix4_transformed_vector4( m_local2view, BasicVector4<double>( planepoints[1], 1 ) ) );
	pts[2] = vector4_projected( matrix4_transformed_vector4( m_local2view, BasicVector4<double>( planepoints[2], 1 ) ) );
	const Plane3 planeTransformed( plane3_for_points( pts ) );

	Vector4 clipped[9];
	for ( std::size_t i = 0; i + 2 < count; ++i )
	{
		BestPoint(
			matrix4_clip_triangle(
				m_local2view,
				reinterpret_cast<const Vector3&>( vertices[0] ),
				reinterpret_cast<const Vector3&>( vertices[i + 1] ),
				reinterpret_cast<const Vector3&>( vertices[i + 2] ),
				clipped
				),
			clipped,
			best,
			m_cull,
			&planeTransformed
			);
	}
}
void TestLineLoop( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best ){
	if ( count == 0 ) {
		return;
	}
	Vector4 clipped[9];
	for ( VertexPointer::iterator i = vertices.begin(), end = i + count, prev = i + ( count - 1 ); i != end; prev = i, ++i )
	{
		BestPoint(
			matrix4_clip_line(
				m_local2view,
				reinterpret_cast<const Vector3&>( ( *prev ) ),
				reinterpret_cast<const Vector3&>( ( *i ) ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
	}
}
void TestLineStrip( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best ){
	if ( count == 0 ) {
		return;
	}
	Vector4 clipped[9];
	for ( VertexPointer::iterator i = vertices.begin(), end = i + count, next = i + 1; next != end; i = next, ++next )
	{
		BestPoint(
			matrix4_clip_line(
				m_local2view,
				reinterpret_cast<const Vector3&>( ( *i ) ),
				reinterpret_cast<const Vector3&>( ( *next ) ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
	}
}
void TestLines( const VertexPointer& vertices, std::size_t count, SelectionIntersection& best ){
	if ( count == 0 ) {
		return;
	}
	Vector4 clipped[9];
	for ( VertexPointer::iterator i = vertices.begin(), end = i + count; i != end; i += 2 )
	{
		BestPoint(
			matrix4_clip_line(
				m_local2view,
				reinterpret_cast<const Vector3&>( ( *i ) ),
				reinterpret_cast<const Vector3&>( ( *( i + 1 ) ) ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
	}
}
void TestTriangles( const VertexPointer& vertices, const IndexPointer& indices, SelectionIntersection& best ){
	Vector4 clipped[9];
	for ( IndexPointer::iterator i( indices.begin() ); i != indices.end(); i += 3 )
	{
		BestPoint(
			matrix4_clip_triangle(
				m_local2view,
				reinterpret_cast<const Vector3&>( vertices[*i] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
	}
}
void TestQuads( const VertexPointer& vertices, const IndexPointer& indices, SelectionIntersection& best ){
	Vector4 clipped[9];
	for ( IndexPointer::iterator i( indices.begin() ); i != indices.end(); i += 4 )
	{
		BestPoint(
			matrix4_clip_triangle(
				m_local2view,
				reinterpret_cast<const Vector3&>( vertices[*i] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 3 )] ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
		BestPoint(
			matrix4_clip_triangle(
				m_local2view,
				reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 3 )] ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
	}
}
void TestQuadStrip( const VertexPointer& vertices, const IndexPointer& indices, SelectionIntersection& best ){
	Vector4 clipped[9];
	for ( IndexPointer::iterator i( indices.begin() ); i + 2 != indices.end(); i += 2 )
	{
		BestPoint(
			matrix4_clip_triangle(
				m_local2view,
				reinterpret_cast<const Vector3&>( vertices[*i] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
		BestPoint(
			matrix4_clip_triangle(
				m_local2view,
				reinterpret_cast<const Vector3&>( vertices[*( i + 2 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 1 )] ),
				reinterpret_cast<const Vector3&>( vertices[*( i + 3 )] ),
				clipped
				),
			clipped,
			best,
			m_cull
			);
	}
}
};

class SelectionCounter
{
public:
typedef const Selectable& first_argument_type;

SelectionCounter( const SelectionChangeCallback& onchanged )
	: m_count( 0 ), m_onchanged( onchanged ){
}
void operator()( const Selectable& selectable ){
	if ( selectable.isSelected() ) {
		++m_count;
	}
	else
	{
		ASSERT_MESSAGE( m_count != 0, "selection counter underflow" );
		--m_count;
	}

	m_onchanged( selectable );
}
bool empty() const {
	return m_count == 0;
}
std::size_t size() const {
	return m_count;
}
private:
std::size_t m_count;
SelectionChangeCallback m_onchanged;
};

class SelectedStuffCounter
{
public:
	std::size_t m_brushcount;
	std::size_t m_patchcount;
	std::size_t m_entitycount;
	SelectedStuffCounter() : m_brushcount( 0 ), m_patchcount( 0 ), m_entitycount( 0 ){
	}
	void increment( scene::Node& node ) {
		if( Node_isBrush( node ) )
			++m_brushcount;
		else if( Node_isPatch( node ) )
			++m_patchcount;
		else if( Node_isEntity( node ) )
			++m_entitycount;
	}
	void decrement( scene::Node& node ) {
		if( Node_isBrush( node ) )
			--m_brushcount;
		else if( Node_isPatch( node ) )
			--m_patchcount;
		else if( Node_isEntity( node ) )
			--m_entitycount;
	}
	void get( std::size_t& brushes, std::size_t& patches, std::size_t& entities ) const {
		 brushes = m_brushcount;
		 patches = m_patchcount;
		 entities = m_entitycount;
	}
};

inline void ConstructSelectionTest( View& view, const rect_t selection_box ){
	view.EnableScissor( selection_box.min[0], selection_box.max[0], selection_box.min[1], selection_box.max[1] );
}

inline const rect_t SelectionBoxForPoint( const float device_point[2], const float device_epsilon[2] ){
	rect_t selection_box;
	selection_box.min[0] = device_point[0] - device_epsilon[0];
	selection_box.min[1] = device_point[1] - device_epsilon[1];
	selection_box.max[0] = device_point[0] + device_epsilon[0];
	selection_box.max[1] = device_point[1] + device_epsilon[1];
	return selection_box;
}

inline const rect_t SelectionBoxForArea( const float device_point[2], const float device_delta[2] ){
	rect_t selection_box;
	selection_box.min[0] = ( device_delta[0] < 0 ) ? ( device_point[0] + device_delta[0] ) : ( device_point[0] );
	selection_box.min[1] = ( device_delta[1] < 0 ) ? ( device_point[1] + device_delta[1] ) : ( device_point[1] );
	selection_box.max[0] = ( device_delta[0] > 0 ) ? ( device_point[0] + device_delta[0] ) : ( device_point[0] );
	selection_box.max[1] = ( device_delta[1] > 0 ) ? ( device_point[1] + device_delta[1] ) : ( device_point[1] );
	selection_box.modifier = device_delta[0] * device_delta[1] < 0?
								rect_t::eToggle
								: device_delta[0] < 0 ?
								rect_t::eDeselect
								: rect_t::eSelect;
	return selection_box;
}
#if 0
Quaternion construct_local_rotation( const Quaternion& world, const Quaternion& localToWorld ){
	return quaternion_normalised( quaternion_multiplied_by_quaternion(
									  quaternion_normalised( quaternion_multiplied_by_quaternion(
																 quaternion_inverse( localToWorld ),
																 world
																 ) ),
									  localToWorld
									  ) );
}
#endif
inline void matrix4_assign_rotation( Matrix4& matrix, const Matrix4& other ){
	matrix[0] = other[0];
	matrix[1] = other[1];
	matrix[2] = other[2];
	matrix[4] = other[4];
	matrix[5] = other[5];
	matrix[6] = other[6];
	matrix[8] = other[8];
	matrix[9] = other[9];
	matrix[10] = other[10];
}
#define SELECTIONSYSTEM_AXIAL_PIVOTS
void matrix4_assign_rotation_for_pivot( Matrix4& matrix, scene::Instance& instance ){
#ifndef SELECTIONSYSTEM_AXIAL_PIVOTS
	Editable* editable = Node_getEditable( instance.path().top() );
	if ( editable != 0 ) {
		matrix4_assign_rotation( matrix, matrix4_multiplied_by_matrix4( instance.localToWorld(), editable->getLocalPivot() ) );
	}
	else
	{
		matrix4_assign_rotation( matrix, instance.localToWorld() );
	}
#endif
}

inline bool Instance_isSelectedComponents( scene::Instance& instance ){
	ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
	return componentSelectionTestable != 0
		   && componentSelectionTestable->isSelectedComponents();
}

class TranslateSelected : public SelectionSystem::Visitor
{
const Vector3& m_translate;
public:
TranslateSelected( const Vector3& translate )
	: m_translate( translate ){
}
void visit( scene::Instance& instance ) const {
	Transformable* transform = Instance_getTransformable( instance );
	if ( transform != 0 ) {
		transform->setType( TRANSFORM_PRIMITIVE );
		transform->setTranslation( m_translate );
	}
}
};

void Scene_Translate_Selected( scene::Graph& graph, const Vector3& translation ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( TranslateSelected( translation ) );
	}
}

Vector3 get_local_pivot( const Vector3& world_pivot, const Matrix4& localToWorld ){
	return Vector3(
			   matrix4_transformed_point(
				   matrix4_full_inverse( localToWorld ),
				   world_pivot
				   )
			   );
}

void translation_for_pivoted_matrix_transform( Vector3& parent_translation, const Matrix4& local_transform, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	// we need a translation inside the parent system to move the origin of this object to the right place

	// mathematically, it must fulfill:
	//
	//   local_translation local_transform local_pivot = local_pivot
	//   local_translation = local_pivot - local_transform local_pivot
	//
	//   or maybe?
	//   local_transform local_translation local_pivot = local_pivot
	//                   local_translation local_pivot = local_transform^-1 local_pivot
	//                 local_translation + local_pivot = local_transform^-1 local_pivot
	//                   local_translation             = local_transform^-1 local_pivot - local_pivot

	Vector3 local_pivot( get_local_pivot( world_pivot, localToWorld ) );

	Vector3 local_translation(
		vector3_subtracted(
			local_pivot,
			matrix4_transformed_point(
				local_transform,
				local_pivot
				)
	        /*
	            matrix4_transformed_point(
	                matrix4_full_inverse(local_transform),
	                local_pivot
	            ),
	            local_pivot
	         */
			)
		);

	translation_local2object( parent_translation, local_translation, localToParent );

	/*
	   // verify it!
	   globalOutputStream() << "World pivot is at " << world_pivot << "\n";
	   globalOutputStream() << "Local pivot is at " << local_pivot << "\n";
	   globalOutputStream() << "Transformation " << local_transform << " moves it to: " << matrix4_transformed_point(local_transform, local_pivot) << "\n";
	   globalOutputStream() << "Must move by " << local_translation << " in the local system" << "\n";
	   globalOutputStream() << "Must move by " << parent_translation << " in the parent system" << "\n";
	 */
}

void translation_for_pivoted_rotation( Vector3& parent_translation, const Quaternion& local_rotation, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	translation_for_pivoted_matrix_transform( parent_translation, matrix4_rotation_for_quaternion_quantised( local_rotation ), world_pivot, localToWorld, localToParent );
}

void translation_for_pivoted_scale( Vector3& parent_translation, const Vector3& world_scale, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	Matrix4 local_transform(
		matrix4_multiplied_by_matrix4(
			matrix4_full_inverse( localToWorld ),
			matrix4_multiplied_by_matrix4(
				matrix4_scale_for_vec3( world_scale ),
				localToWorld
				)
			)
		);
	local_transform.tx() = local_transform.ty() = local_transform.tz() = 0; // cancel translation parts
	translation_for_pivoted_matrix_transform( parent_translation, local_transform, world_pivot, localToWorld, localToParent );
}

void translation_for_pivoted_skew( Vector3& parent_translation, const Skew& local_skew, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	Matrix4 local_transform( g_matrix4_identity );
	local_transform[local_skew.index] = local_skew.amount;
	translation_for_pivoted_matrix_transform( parent_translation, local_transform, world_pivot, localToWorld, localToParent );
}

class rotate_selected : public SelectionSystem::Visitor
{
const Quaternion& m_rotate;
const Vector3& m_world_pivot;
public:
rotate_selected( const Quaternion& rotation, const Vector3& world_pivot )
	: m_rotate( rotation ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
	if ( transformNode != 0 ) {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setScale( c_scale_identity );
			transform->setTranslation( c_translation_identity );

			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setRotation( m_rotate );

			{
				Editable* editable = Node_getEditable( instance.path().top() );
				const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

				Vector3 parent_translation;
				translation_for_pivoted_rotation(
					parent_translation,
					m_rotate,
					m_world_pivot,
#ifdef SELECTIONSYSTEM_AXIAL_PIVOTS
					matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( instance.localToWorld() ) ), localPivot ),
					matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( transformNode->localToParent() ) ), localPivot )
#else
					matrix4_multiplied_by_matrix4( instance.localToWorld(), localPivot ),
					matrix4_multiplied_by_matrix4( transformNode->localToParent(), localPivot )
#endif
					);

				transform->setTranslation( parent_translation );
			}
		}
	}
}
};

void Scene_Rotate_Selected( scene::Graph& graph, const Quaternion& rotation, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( rotate_selected( rotation, world_pivot ) );
	}
}

class scale_selected : public SelectionSystem::Visitor
{
const Vector3& m_scale;
const Vector3& m_world_pivot;
public:
scale_selected( const Vector3& scaling, const Vector3& world_pivot )
	: m_scale( scaling ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
	if ( transformNode != 0 ) {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setScale( c_scale_identity );
			transform->setTranslation( c_translation_identity );

			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setScale( m_scale );
			{
				Editable* editable = Node_getEditable( instance.path().top() );
				const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

				Vector3 parent_translation;
				translation_for_pivoted_scale(
					parent_translation,
					m_scale,
					m_world_pivot,
					matrix4_multiplied_by_matrix4( instance.localToWorld(), localPivot ),
					matrix4_multiplied_by_matrix4( transformNode->localToParent(), localPivot )
					);

				transform->setTranslation( parent_translation );
			}
		}
	}
}
};

void Scene_Scale_Selected( scene::Graph& graph, const Vector3& scaling, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( scale_selected( scaling, world_pivot ) );
	}
}

class skew_selected : public SelectionSystem::Visitor
{
const Skew& m_skew;
const Vector3& m_world_pivot;
public:
skew_selected( const Skew& skew, const Vector3& world_pivot )
	: m_skew( skew ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
	if ( transformNode != 0 ) {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setScale( c_scale_identity );
			transform->setTranslation( c_translation_identity );

			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setSkew( m_skew );
			{
				Editable* editable = Node_getEditable( instance.path().top() );
				const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

				Vector3 parent_translation;
				translation_for_pivoted_skew(
					parent_translation,
					m_skew,
					m_world_pivot,
					matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( instance.localToWorld() ) ), localPivot ),
					matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( transformNode->localToParent() ) ), localPivot )
					);

				transform->setTranslation( parent_translation );
			}
		}
	}
}
};

void Scene_Skew_Selected( scene::Graph& graph, const Skew& skew, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( skew_selected( skew, world_pivot ) );
	}
}


class RepeatableTransforms
{
public:
Translation m_translation;
Rotation m_rotation;
Scale m_scale;
Skew m_skew;
/* next aren't used; TODO: think if unique origin per transform is needed, and how to implement this correctly for entities, having transform keys */
Vector3 m_rotationOrigin;
Vector3 m_scaleOrigin;
Vector3 m_skewOrigin;

bool m_rotationOriginSet;
bool m_scaleOriginSet;
bool m_skewOriginSet;

RepeatableTransforms(){
	setIdentity();
}

bool isIdentity() const {
	return m_translation == c_translation_identity
		 && m_rotation == c_rotation_identity
		 && m_scale == c_scale_identity
		 && m_skew == c_skew_identity;
}
void setIdentity(){
	m_translation 	= c_translation_identity;
	m_rotation 		= c_quaternion_identity;
	m_scale 		= c_scale_identity;
	m_skew 			= c_skew_identity;

	m_rotationOrigin =
	m_scaleOrigin 	=
	m_skewOrigin 	= g_vector3_identity;

	m_rotationOriginSet =
	m_scaleOriginSet 	=
	m_skewOriginSet 	= false;
}
};

class transform_selected : public SelectionSystem::Visitor
{
const RepeatableTransforms& m_transforms;
const Vector3& m_world_pivot;
public:
transform_selected( const RepeatableTransforms& transforms, const Vector3& world_pivot )
	: m_transforms( transforms ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
	if ( transformNode != 0 ) {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setRotation( m_transforms.m_rotation );
			transform->setScale( m_transforms.m_scale );
			transform->setSkew( m_transforms.m_skew );
			{
				Editable* editable = Node_getEditable( instance.path().top() );
				const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

				const Matrix4 local_transform = matrix4_transform_for_components( c_translation_identity, m_transforms.m_rotation, m_transforms.m_scale, m_transforms.m_skew );
				Vector3 parent_translation;
				translation_for_pivoted_matrix_transform(
					parent_translation,
					local_transform,
					m_world_pivot,
					matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( instance.localToWorld() ) ), localPivot ),
					matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( transformNode->localToParent() ) ), localPivot )
					);

				transform->setTranslation( parent_translation + m_transforms.m_translation );
			}
		}
	}
}
};


class translate_component_selected : public SelectionSystem::Visitor
{
const Vector3& m_translate;
public:
translate_component_selected( const Vector3& translate )
	: m_translate( translate ){
}
void visit( scene::Instance& instance ) const {
	Transformable* transform = Instance_getTransformable( instance );
	if ( transform != 0 ) {
		transform->setType( TRANSFORM_COMPONENT );
		transform->setTranslation( m_translate );
	}
}
};

void Scene_Translate_Component_Selected( scene::Graph& graph, const Vector3& translation ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( translate_component_selected( translation ) );
	}
}

class rotate_component_selected : public SelectionSystem::Visitor
{
const Quaternion& m_rotate;
const Vector3& m_world_pivot;
public:
rotate_component_selected( const Quaternion& rotation, const Vector3& world_pivot )
	: m_rotate( rotation ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	Transformable* transform = Instance_getTransformable( instance );
	if ( transform != 0 ) {
		Vector3 parent_translation;
		translation_for_pivoted_rotation( parent_translation, m_rotate, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

		transform->setType( TRANSFORM_COMPONENT );
		transform->setRotation( m_rotate );
		transform->setTranslation( parent_translation );
	}
}
};

void Scene_Rotate_Component_Selected( scene::Graph& graph, const Quaternion& rotation, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( rotate_component_selected( rotation, world_pivot ) );
	}
}

class scale_component_selected : public SelectionSystem::Visitor
{
const Vector3& m_scale;
const Vector3& m_world_pivot;
public:
scale_component_selected( const Vector3& scaling, const Vector3& world_pivot )
	: m_scale( scaling ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	Transformable* transform = Instance_getTransformable( instance );
	if ( transform != 0 ) {
		Vector3 parent_translation;
		translation_for_pivoted_scale( parent_translation, m_scale, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

		transform->setType( TRANSFORM_COMPONENT );
		transform->setScale( m_scale );
		transform->setTranslation( parent_translation );
	}
}
};

void Scene_Scale_Component_Selected( scene::Graph& graph, const Vector3& scaling, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( scale_component_selected( scaling, world_pivot ) );
	}
}

class skew_component_selected : public SelectionSystem::Visitor
{
const Skew& m_skew;
const Vector3& m_world_pivot;
public:
skew_component_selected( const Skew& skew, const Vector3& world_pivot )
	: m_skew( skew ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	Transformable* transform = Instance_getTransformable( instance );
	if ( transform != 0 ) {
		Vector3 parent_translation;
		translation_for_pivoted_skew( parent_translation, m_skew, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

		transform->setType( TRANSFORM_COMPONENT );
		transform->setSkew( m_skew );
		transform->setTranslation( parent_translation );
	}
}
};

void Scene_Skew_Component_Selected( scene::Graph& graph, const Skew& skew, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( skew_component_selected( skew, world_pivot ) );
	}
}


class transform_component_selected : public SelectionSystem::Visitor
{
const RepeatableTransforms& m_transforms;
const Vector3& m_world_pivot;
public:
transform_component_selected( const RepeatableTransforms& transforms, const Vector3& world_pivot )
	: m_transforms( transforms ), m_world_pivot( world_pivot ){
}
void visit( scene::Instance& instance ) const {
	Transformable* transform = Instance_getTransformable( instance );
	if ( transform != 0 ) {
		const Matrix4 local_transform = matrix4_transform_for_components( c_translation_identity, m_transforms.m_rotation, m_transforms.m_scale, m_transforms.m_skew );
		Vector3 parent_translation;
		translation_for_pivoted_matrix_transform( parent_translation, local_transform, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

		transform->setType( TRANSFORM_COMPONENT );
		transform->setRotation( m_transforms.m_rotation );
		transform->setScale( m_transforms.m_scale );
		transform->setSkew( m_transforms.m_skew );
		transform->setTranslation( parent_translation + m_transforms.m_translation );
	}
}
};


class BooleanSelector : public Selector
{
SelectionIntersection m_bestIntersection;
Selectable* m_selectable;
public:
BooleanSelector() : m_bestIntersection( SelectionIntersection() ){
}

void pushSelectable( Selectable& selectable ){
	m_selectable = &selectable;
}
void popSelectable(){
}
void addIntersection( const SelectionIntersection& intersection ){
	if ( m_selectable->isSelected() ) {
		assign_if_closer( m_bestIntersection, intersection );
	}
}

bool isSelected(){
	return m_bestIntersection.valid();
}
const SelectionIntersection& bestIntersection() const {
	return m_bestIntersection;
}
};

class BestSelector : public Selector
{
SelectionIntersection m_intersection;
Selectable* m_selectable;
SelectionIntersection m_bestIntersection;
std::list<Selectable*> m_bestSelectable;
public:
BestSelector() : m_bestIntersection( SelectionIntersection() ), m_bestSelectable( 0 ){
}

void pushSelectable( Selectable& selectable ){
	m_intersection = SelectionIntersection();
	m_selectable = &selectable;
}
void popSelectable(){
	if ( m_intersection.equalEpsilon( m_bestIntersection, 0.25f, 2e-6f ) ) {
		m_bestSelectable.push_back( m_selectable );
		m_bestIntersection = m_intersection;
	}
	else if ( m_intersection < m_bestIntersection ) {
		m_bestSelectable.clear();
		m_bestSelectable.push_back( m_selectable );
		m_bestIntersection = m_intersection;
	}
	m_intersection = SelectionIntersection();
}
void addIntersection( const SelectionIntersection& intersection ){
	assign_if_closer( m_intersection, intersection );
}

std::list<Selectable*>& best(){
	return m_bestSelectable;
}
const SelectionIntersection& bestIntersection() const {
	return m_bestIntersection;
}
};

class DeepBestSelector : public Selector
{
SelectionIntersection m_intersection;
Selectable* m_selectable;
SelectionIntersection m_bestIntersection;
std::list<Selectable*> m_bestSelectable;
public:
DeepBestSelector() : m_bestIntersection( SelectionIntersection() ), m_bestSelectable( 0 ){
}

void pushSelectable( Selectable& selectable ){
	m_intersection = SelectionIntersection();
	m_selectable = &selectable;
}
void popSelectable(){
	if ( m_intersection.equalEpsilon( m_bestIntersection, 0.25f, 2.f ) ) {
		m_bestSelectable.push_back( m_selectable );
		m_bestIntersection = m_intersection;
	}
	else if ( m_intersection < m_bestIntersection ) {
		m_bestSelectable.clear();
		m_bestSelectable.push_back( m_selectable );
		m_bestIntersection = m_intersection;
	}
	m_intersection = SelectionIntersection();
}
void addIntersection( const SelectionIntersection& intersection ){
	assign_if_closer( m_intersection, intersection );
}

std::list<Selectable*>& best(){
	return m_bestSelectable;
}
};

class BestPointSelector : public Selector
{
SelectionIntersection m_bestIntersection;
public:
BestPointSelector() : m_bestIntersection( SelectionIntersection() ){
}

void pushSelectable( Selectable& selectable ){
}
void popSelectable(){
}
void addIntersection( const SelectionIntersection& intersection ){
	assign_if_closer( m_bestIntersection, intersection );
}

bool isSelected(){
	return m_bestIntersection.valid();
}
const SelectionIntersection& best() const {
	return m_bestIntersection;
}
};





class ClipperSelector : public Selector {
	SelectionIntersection m_bestIntersection;
	Face* m_face;
public:
	ClipperSelector() : m_bestIntersection( SelectionIntersection() ), m_face( 0 ) {
	}

	void pushSelectable( Selectable& selectable ) {
	}
	void popSelectable() {
	}
	void addIntersection( const SelectionIntersection& intersection ) {
		if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
			m_bestIntersection = intersection;
			m_face = 0;
		}
	}

	void addIntersection( const SelectionIntersection& intersection, Face* face ) {
		if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
			m_bestIntersection = intersection;
			m_face = face;
		}
	}
	bool isSelected() {
		return m_bestIntersection.valid();
	}
	const SelectionIntersection& best() {
		return m_bestIntersection;
	}
	const Face* face() {
		return m_face;
	}
};

class testselect_scene_4clipper : public scene::Graph::Walker {
	ClipperSelector& m_clipperSelector;
	SelectionTest& m_test;
public:
	testselect_scene_4clipper( ClipperSelector& clipperSelector, SelectionTest& test ) : m_clipperSelector( clipperSelector ), m_test( test ) {
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		BrushInstance* brush = Instance_getBrush( instance );
		if( brush != 0 ) {
			m_test.BeginMesh( brush->localToWorld() );
			for( Brush::const_iterator i = brush->getBrush().begin(); i != brush->getBrush().end(); ++i ) {
				Face* face = *i;
				if( !face->isFiltered() ) {
					SelectionIntersection intersection;
					face->testSelect( m_test, intersection );
					m_clipperSelector.addIntersection( intersection, face );
				}
			}
		}
		else {
			SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance );
			if( selectionTestable ) {
				selectionTestable->testSelect( m_clipperSelector, m_test );
			}
		}
		return true;
	}
};

class testselect_scene_4clipper_selected : public scene::Graph::Walker {
	ClipperSelector& m_clipperSelector;
	SelectionTest& m_test;
public:
	testselect_scene_4clipper_selected( ClipperSelector& clipperSelector, SelectionTest& test ) : m_clipperSelector( clipperSelector ), m_test( test ) {
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		BrushInstance* brush = Instance_getBrush( instance );
		if( brush != 0 && brush->isSelected() ) {
			m_test.BeginMesh( brush->localToWorld() );
			for( Brush::const_iterator i = brush->getBrush().begin(); i != brush->getBrush().end(); ++i ) {
				Face* face = *i;
				if( !face->isFiltered() ) {
					SelectionIntersection intersection;
					face->testSelect( m_test, intersection );
					m_clipperSelector.addIntersection( intersection, face );
				}
			}
		}
		return true;
	}
};

Vector3 testSelected_scene_snapped_point( const SelectionVolume& test, ClipperSelector& clipperSelector ){
	Vector3 point = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, clipperSelector.best().depth(), 1 ) ) );
	if( clipperSelector.face() ){
		const Face& face = *clipperSelector.face();
		float bestDist = FLT_MAX;
		Vector3 wannabePoint;
		for ( Winding::const_iterator prev = face.getWinding().end() - 1, curr = face.getWinding().begin(); curr != face.getWinding().end(); prev = curr, ++curr ){
			{ /* try vertices */
				const float dist = vector3_length_squared( ( *curr ).vertex - point );
				if( dist < bestDist ){
					wannabePoint = ( *curr ).vertex;
					bestDist = dist;
				}
			}
			{ /* try edges */
				Vector3 edgePoint = line_closest_point( Line( ( *prev ).vertex, ( *curr ).vertex ), point );
				if( edgePoint != ( *prev ).vertex && edgePoint != ( *curr ).vertex ){
					const Vector3 edgedir = vector3_normalised( ( *curr ).vertex - ( *prev ).vertex );
					const std::size_t maxi = vector3_max_abs_component_index( edgedir );
					// ( *prev ).vertex[maxi] + edgedir[maxi] * coef = float_snapped( point[maxi], GetSnapGridSize() )
					const float coef = ( float_snapped( point[maxi], GetSnapGridSize() ) - ( *prev ).vertex[maxi] ) / edgedir[maxi];
					edgePoint = ( *prev ).vertex + edgedir * coef;
					const float dist = vector3_length_squared( edgePoint - point );
					if( dist < bestDist ){
						wannabePoint = edgePoint;
						bestDist = dist;
					}
				}
			}
		}
		if( clipperSelector.best().distance() == 0.f ){ /* try plane, if pointing inside of polygon */
			const std::size_t maxi = vector3_max_abs_component_index( face.plane3().normal() );
			Vector3 planePoint( vector3_snapped( point, GetSnapGridSize() ) );
			// face.plane3().normal().dot( point snapped ) = face.plane3().dist()
			planePoint[maxi] = ( face.plane3().dist()
									- face.plane3().normal()[( maxi + 1 ) % 3] * planePoint[( maxi + 1 ) % 3]
									- face.plane3().normal()[( maxi + 2 ) % 3] * planePoint[( maxi + 2 ) % 3] ) / face.plane3().normal()[maxi];
			const float dist = vector3_length_squared( planePoint - point );
			if( dist < bestDist ){
				wannabePoint = planePoint;
				bestDist = dist;
			}
		}
		point = wannabePoint;
	}
	else{
		vector3_snap( point, GetSnapGridSize() );
	}
	return point;
}

class Scene_insert_brush_vertices
{
	const Brush::VertexModeVertices& m_vertexModeVertices;
public:
	Scene_insert_brush_vertices( const Brush::VertexModeVertices& vertexModeVertices ) : m_vertexModeVertices( vertexModeVertices ) {
	}
	void operator()( BrushInstance& brush ) const {
		brush.insert_vertices( m_vertexModeVertices );
	}
};

bool scene_insert_brush_vertices( const View& view, TranslateFreeXY_Z& freeDragXY_Z ){
	SelectionVolume test( view );
	ClipperSelector clipperSelector;
	if( view.fill() )
		Scene_forEachVisible( GlobalSceneGraph(), view, testselect_scene_4clipper( clipperSelector, test ) );
	else
		Scene_forEachVisible( GlobalSceneGraph(), view, testselect_scene_4clipper_selected( clipperSelector, test ) );
	test.BeginMesh( g_matrix4_identity, true );
	if( clipperSelector.isSelected() ){
		freeDragXY_Z.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, clipperSelector.best().depth(), 1 ) ) ) );
		Vector3 point = testSelected_scene_snapped_point( test, clipperSelector );
		if( !view.fill() ){
			point -= view.getViewDir() * GetGridSize();
		}
		Brush::VertexModeVertices vertexModeVertices;
		vertexModeVertices.push_back( Brush::VertexModeVertex( point, true ) );
		if( clipperSelector.face() )
			vertexModeVertices.back().m_faces.push_back( clipperSelector.face() );

		UndoableCommand undo( "InsertBrushVertices" );
		Scene_forEachSelectedBrush( Scene_insert_brush_vertices( vertexModeVertices ) );
		return true;
	}
	else if( !view.fill() ){ //+two points
		freeDragXY_Z.set0( g_vector3_identity );
		const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
		if( aabb_valid( bounds ) ){
			Vector3 xy = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, 0, 1 ) ) );
			vector3_snap( xy, GetSnapGridSize() );
			Vector3 a( xy ), b( xy );
			const std::size_t max = vector3_max_abs_component_index( view.getViewDir() );
			a[max] = bounds.origin[max] + bounds.extents[max];
			b[max] = bounds.origin[max] - bounds.extents[max];
			Brush::VertexModeVertices vertexModeVertices;
			vertexModeVertices.push_back( Brush::VertexModeVertex( a, true ) );
			vertexModeVertices.push_back( Brush::VertexModeVertex( b, true ) );

			UndoableCommand undo( "InsertBrushVertices" );
			Scene_forEachSelectedBrush( Scene_insert_brush_vertices( vertexModeVertices ) );
			return true;
		}
	}
	return false;
}


bool g_bAltResize_AltSelect = false; //AltDragManipulatorResize + select primitives in component modes
bool g_bTmpComponentMode = false;

class DragManipulator : public Manipulator
{
TranslateFree m_freeResize;
TranslateAxis2 m_axisResize;
TranslateFreeXY_Z m_freeDragXY_Z;
ResizeTranslatable m_resize;
DragNewBrush m_dragNewBrush;
bool m_dragSelected; //drag selected primitives or components
bool m_selected; //components selected temporally for drag
bool m_selected2; //planeselectables in cam with alt
bool m_newBrush;

public:

DragManipulator( Translatable& translatable ) : m_freeResize( m_resize ), m_axisResize( m_resize ), m_freeDragXY_Z( translatable ), m_dragSelected( false ), m_selected( false ), m_selected2( false ), m_newBrush( false ){
}

Manipulatable* GetManipulatable(){
	if( m_newBrush )
		return &m_dragNewBrush;
	else if( m_selected )
		return &m_freeResize;
	else if( m_selected2 )
		return &m_axisResize;
	else
		return &m_freeDragXY_Z;
}

void testSelect( const View& view, const Matrix4& pivot2world ){
	SelectionPool selector;
	SelectionVolume test( view );

	if( GlobalSelectionSystem().countSelected() != 0 ){
		if ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ){
			if( g_bAltResize_AltSelect && view.fill() ){
				m_selected2 = Scene_forEachPlaneSelectable_selectPlanes2( GlobalSceneGraph(), test, m_axisResize );
			}
			else{
				BooleanSelector booleanSelector;
				Scene_TestSelect_Primitive( booleanSelector, test, view );

				if ( booleanSelector.isSelected() ) { /* hit a primitive */
					if( g_bAltResize_AltSelect ){
						DeepBestSelector deepSelector;
						Scene_TestSelect_Component_Selected( deepSelector, test, view, SelectionSystem::eVertex ); /* try to quickly select hit vertices */
						for ( std::list<Selectable*>::iterator i = deepSelector.best().begin(); i != deepSelector.best().end(); ++i )
							selector.addSelectable( SelectionIntersection( 0, 0 ), ( *i ) );
						if( deepSelector.best().empty() ) /* otherwise drag clicked face's vertices */
							Scene_forEachTestedBrushFace_selectVertices( GlobalSceneGraph(), test );
						m_selected = true;
					}
					else{ /* drag a primitive */
						m_dragSelected = true;
						test.BeginMesh( g_matrix4_identity, true );
						m_freeDragXY_Z.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, booleanSelector.bestIntersection().depth(), 1 ) ) ) );
					}
				}
				else{ /* haven't hit a primitive */
					if( g_bAltResize_AltSelect ){
						Scene_forEachBrushPlane_selectVertices( GlobalSceneGraph(), test ); /* select vertices on planeSelectables */
						m_selected = true;
					}
					else{
						m_selected = Scene_forEachPlaneSelectable_selectPlanes( GlobalSceneGraph(), selector, test ); /* select faces on planeSelectables */
					}
				}
			}
		}
		else{
			BestSelector bestSelector;
			Scene_TestSelect_Component_Selected( bestSelector, test, view, GlobalSelectionSystem().ComponentMode() ); /* drag components */
			for ( std::list<Selectable*>::iterator i = bestSelector.best().begin(); i != bestSelector.best().end(); ++i ){
				if ( !( *i )->isSelected() )
					GlobalSelectionSystem().setSelectedAllComponents( false );
				selector.addSelectable( SelectionIntersection( 0, 0 ), ( *i ) );
				m_dragSelected = true;
			}
			if( bestSelector.bestIntersection().valid() ){
				test.BeginMesh( g_matrix4_identity, true );
				m_freeDragXY_Z.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, bestSelector.bestIntersection().depth(), 1 ) ) ) );
			}
			else{
				if( GlobalSelectionSystem().countSelectedComponents() != 0 ){ /* even if hit nothing, but got selected */
					m_dragSelected = true;
					m_freeDragXY_Z.set0( g_vector3_identity );
				}
				else if( GlobalSelectionSystem().ComponentMode() == SelectionSystem::eVertex ){
					m_dragSelected = g_bTmpComponentMode = scene_insert_brush_vertices( view, m_freeDragXY_Z ); //hack: indicating not a tmp mode
					return;
				}
			}
		}

		for ( SelectionPool::iterator i = selector.begin(); i != selector.end(); ++i )
			( *i ).second->setSelected( true );
		g_bTmpComponentMode = m_selected | m_selected2;
	}
	else if( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ){
		m_newBrush = true;
		BestPointSelector bestPointSelector;
		Scene_TestSelect_Primitive( bestPointSelector, test, view );
		Vector3 start;
		test.BeginMesh( g_matrix4_identity, true );
		if( bestPointSelector.isSelected() ){
			start = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, bestPointSelector.best().depth(), 1 ) ) );
		}
		else{
			const Vector3 near = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, -1, 1 ) ) );
			const Vector3 far = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, 1, 1 ) ) );
			start = vector3_normalised( far - near ) * ( 256.f + GetGridSize() * sqrt( 3.0 ) ) + near;
		}
		vector3_snap( start, GetSnapGridSize() );
		m_dragNewBrush.set0( start );
	}
}

void setSelected( bool select ){
	m_dragSelected = select;
	m_selected = select;
	m_selected2 = select;
	m_newBrush = select;
}
bool isSelected() const {
	return m_dragSelected || m_selected || m_selected2 || m_newBrush;
}
};



#include "clippertool.h"

class ClipManipulator : public Manipulator, public ManipulatorSelectionChangeable, public Translatable, public Manipulatable
{
	struct ClipperPoint : public OpenGLRenderable, public SelectableBool
	{
		PointVertex m_p; //for render
		ClipperPoint():
			m_p( vertex3f_identity ), m_set( false ) {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_p.colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_p.vertex );
			glDrawArrays( GL_POINTS, 0, 1 );

			glColor4ub( m_p.colour.r, m_p.colour.g, m_p.colour.b, m_p.colour.a ); ///?
			glRasterPos3f( m_namePos.x(), m_namePos.y(), m_namePos.z() );
			GlobalOpenGL().drawChar( m_name );
		}
		void setColour( const Colour4b& colour ) {
			m_p.colour = colour;
		}
		bool m_set;
		Vector3 m_point;
		Vector3 m_pointNonTransformed;
		char m_name;
		Vector3 m_namePos;
	};
	Matrix4& m_pivot2world;
	ClipperPoint m_points[3];
	TranslateFreeXY_Z m_dragXY_Z;
	const AABB& m_bounds;
	Vector3 m_viewdir;
public:
	static Shader* m_state;

	ClipManipulator( Matrix4& pivot2world, const AABB& bounds ) : m_pivot2world( pivot2world ), m_dragXY_Z( *this ), m_bounds( bounds ){
		m_points[0].m_name = '1';
		m_points[1].m_name = '2';
		m_points[2].m_name = '3';
	}

	void UpdateColours() {
		for( std::size_t i = 0; i < 3; ++i )
			m_points[i].setColour( colourSelected( g_colour_screen, m_points[i].isSelected() ) );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) {
		// temp hack
		UpdateColours();

		renderer.SetState( m_state, Renderer::eWireframeOnly );
		renderer.SetState( m_state, Renderer::eFullMaterials );

		const Matrix4 proj( matrix4_multiplied_by_matrix4( volume.GetViewport(), volume.GetViewMatrix() ) );
		const Matrix4 proj_inv( matrix4_full_inverse( proj ) );
		for( std::size_t i = 0; i < 3; ++i )
			if( m_points[i].m_set ){
				m_points[i].m_p.vertex = vertex3f_for_vector3( m_points[i].m_point );
				renderer.addRenderable( m_points[i], g_matrix4_identity );
				const Vector3 pos = vector4_projected( matrix4_transformed_vector4( proj, Vector4( m_points[i].m_point, 1 ) ) ) + Vector3( 3, 4, 0 );
				m_points[i].m_namePos = vector4_projected( matrix4_transformed_vector4( proj_inv, Vector4( pos, 1 ) ) );
			}
	}
	/* these three functions and m_viewdir for 2 points only */
	void viewdir_set( const Vector3 viewdir ){
		const std::size_t maxi = vector3_max_abs_component_index( viewdir );
		m_viewdir = ( viewdir[maxi] > 0 )? g_vector3_axes[maxi] : -g_vector3_axes[maxi];
	}
	void viewdir_fixup(){
		if( fabs( vector3_length( m_points[1].m_point - m_points[0].m_point ) ) > 1e-3 //two non coincident points
			&& fabs( vector3_dot( m_viewdir, vector3_normalised( m_points[1].m_point - m_points[0].m_point ) ) ) > 0.999 ){ //on axis = m_viewdir
			viewdir_set( m_view->getViewDir() );
			if( fabs( vector3_dot( m_viewdir, vector3_normalised( m_points[1].m_point - m_points[0].m_point ) ) ) > 0.999 ){
				const Matrix4 screen2world( matrix4_full_inverse( m_view->GetViewMatrix() ) );
				Vector3 p[2];
				for( std::size_t i = 0; i < 2; ++i ){
					p[i] = vector4_projected( matrix4_transformed_vector4( m_view->GetViewMatrix(), Vector4( m_points[i].m_point, 1 ) ) );
				}
				const float depthdir = p[1].z() > p[0].z()? -1 : 1;
				for( std::size_t i = 0; i < 2; ++i ){
					p[i].z() = -1;
					p[i] = vector4_projected( matrix4_transformed_vector4( screen2world, Vector4( p[i], 1 ) ) );
				}
				viewdir_set( ( p[1] - p[0] ) * depthdir );
			}
		}
	}
	void viewdir_make_cut_worthy( const Plane3& plane ){
		const std::size_t maxi = vector3_max_abs_component_index( plane.normal() );
		if( plane3_valid( plane )
			&& aabb_valid( m_bounds )
			&& fabs( plane.normal()[maxi] ) > 0.999 ){ //axial plane
			const double anchor = plane.normal()[maxi] * plane.dist();
			if( anchor > m_bounds.origin[maxi] ){
				if( ( anchor - ( m_bounds.origin[maxi] + m_bounds.extents[maxi] ) ) > -0.1 )
					viewdir_set( -g_vector3_axes[maxi] );
			}
			else{
				if( ( -anchor + ( m_bounds.origin[maxi] - m_bounds.extents[maxi] ) ) > -0.1 )
					viewdir_set( g_vector3_axes[maxi] );
			}
		}
	}
	void updatePlane(){
		std::size_t npoints = 0;
		for(; npoints < 3; )
			if( m_points[npoints].m_set )
				++npoints;
			else
				break;

		switch ( npoints )
		{
		case 1:
			Clipper_setPlanePoints( ClipperPoints( m_points[0].m_point, m_points[0].m_point, m_points[0].m_point, npoints ) );
			break;
		case 2:
		{
			if( m_view->fill() ){ //3d
				viewdir_fixup();
				m_points[2].m_point = m_points[0].m_point - m_viewdir * vector3_length( m_points[0].m_point - m_points[1].m_point );
				viewdir_make_cut_worthy( plane3_for_points( m_points[0].m_point, m_points[1].m_point, m_points[2].m_point ) );
			}
			m_points[2].m_point = m_points[0].m_point - m_viewdir * vector3_length( m_points[0].m_point - m_points[1].m_point );
		} // fall through
		case 3:
			Clipper_setPlanePoints( ClipperPoints( m_points[0].m_point, m_points[1].m_point, m_points[2].m_point, npoints ) );
			break;

		default:
			Clipper_setPlanePoints( ClipperPoints() );
			break;
		}
	}
	std::size_t newPointIndex( bool viewfill ) const {
		const std::size_t maxi = ( !viewfill && Clipper_get2pointsIn2d() )? 2 : 3;
		std::size_t i;
		for( i = 0; i < maxi; ++i )
			if( !m_points[i].m_set )
				break;
		return i % maxi;
	}
	void newPoint( const Vector3& point, const View& view ){
		const std::size_t i = newPointIndex( view.fill() );
		if( i == 0 )
			m_points[1].m_set = m_points[2].m_set = false;
		m_points[i].m_set = true;
		m_points[i].m_point = point;

		SelectionPool selector;
		selector.addSelectable( SelectionIntersection( 0, 0 ), &m_points[i] );
		selectionChange( selector );

		if( i == 1 )
			viewdir_set( m_view->getViewDir() );

		updatePlane();
	}
	bool testSelect_scene( const View& view, Vector3& point ){
		SelectionVolume test( view );
		ClipperSelector clipperSelector;
		Scene_forEachVisible( GlobalSceneGraph(), view, testselect_scene_4clipper( clipperSelector, test ) );
		test.BeginMesh( g_matrix4_identity, true );
		if( clipperSelector.isSelected() ){
			point = testSelected_scene_snapped_point( test, clipperSelector );
			return true;
		}
		return false;
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) {
		testSelect_points( view );
		if( !isSelected() ){
			if( view.fill() ){
				Vector3 point;
				if( testSelect_scene( view, point ) )
					newPoint( point, view );
			}
			else{
				Vector3 point = vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( view.GetViewMatrix() ), Vector4( 0, 0, 0, 1 ) ) );
				vector3_snap( point, GetSnapGridSize() );
				{
					const std::size_t maxi = vector3_max_abs_component_index( view.getViewDir() );
					const std::size_t i = newPointIndex( false );
					point[maxi] = m_bounds.origin[maxi] + ( i == 2? -1 : 1 ) * m_bounds.extents[maxi];
				}
				newPoint( point, view );
			}
		}
		for( std::size_t i = 0; i < 3; ++i )
			if( m_points[i].isSelected() ){
				m_points[i].m_pointNonTransformed = m_points[i].m_point;
				m_pivot2world = matrix4_translation_for_vec3( m_points[i].m_pointNonTransformed );
				break;
			}
	}
	void testSelect_points( const View& view ) {
		SelectionPool selector;
		{
			const Matrix4 local2view( view.GetViewMatrix() );

			for( std::size_t i = 0; i < 3; ++i ){
				if( m_points[i].m_set ){
					SelectionIntersection best;
					Point_BestPoint( local2view, PointVertex( vertex3f_for_vector3( m_points[i].m_point ) ), best );
					selector.addSelectable( best, &m_points[i] );
				}
			}
		}
		selectionChange( selector );
	}
	void reset(){
		for( std::size_t i = 0; i < 3; ++i ){
			m_points[i].m_set = false;
			m_points[i].setSelected( false ); ///?
		}
		updatePlane();
	}
	/* Translatable */
	void translate( const Vector3& translation ){ //in 2d and ( 3d + m_dragXY_Z )
		for( std::size_t i = 0; i < 3; ++i )
			if( m_points[i].isSelected() ){
				m_points[i].m_point = m_points[i].m_pointNonTransformed + translation;
				updatePlane();
				break;
			}
	}
	/* Manipulatable */
	void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
		m_dragXY_Z.set0( transform_origin );
		m_dragXY_Z.Construct( device2manip, x, y, AABB( transform_origin, g_vector3_identity ), transform_origin );
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
		if( snap || snapbbox || alt || !m_view->fill() )
			return m_dragXY_Z.Transform( manip2object, device2manip, x, y, snap, snapbbox, alt );

		View scissored( *m_view );
		const float device_point[2] = { x, y };
		ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, m_device_epsilon ) );

		Vector3 point;
		if( testSelect_scene( scissored, point ) )
			for( std::size_t i = 0; i < 3; ++i )
				if( m_points[i].isSelected() ){
					m_points[i].m_point = point;
					updatePlane();
					break;
				}
	}

	Manipulatable* GetManipulatable() {
		return this;
	}

	void setSelected( bool select ) {
		for( std::size_t i = 0; i < 3; ++i )
			m_points[i].setSelected( select );
	}
	bool isSelected() const {
		return m_points[0].isSelected() || m_points[1].isSelected() || m_points[2].isSelected();
	}
};
Shader* ClipManipulator::m_state;




class BuildManipulator : public Manipulator, public Manipulatable
{
	struct RenderableLine : public OpenGLRenderable {
		PointVertex m_line[2];

		RenderableLine() {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_line[0].colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_line[0].vertex );
			glDrawArrays( GL_LINES, 0, 2 );
		}
		void setColour( const Colour4b& colour ) {
			m_line[0].colour = colour;
			m_line[1].colour = colour;
		}
	};
	struct RenderablePoint : public OpenGLRenderable
	{
		PointVertex m_point;
		RenderablePoint():
			m_point( vertex3f_identity ) {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_point.colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_point.vertex );
			glDrawArrays( GL_POINTS, 0, 1 );
		}
		void setColour( const Colour4b & colour ) {
			m_point.colour = colour;
		}
	};
	bool m_isSelected;
	bool m_isInitialised;
	RenderablePoint m_point;
	RenderableLine m_line;
	RenderableLine m_midline;
public:
	static Shader* m_state_point;
	static Shader* m_state_line;

	BuildManipulator() : m_isSelected( false ), m_isInitialised( false ) {
		m_point.setColour( g_colour_selected );
		m_line.setColour( g_colour_selected );
		m_midline.setColour( g_colour_screen );
	}
	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) {
		renderer.SetState( m_state_point, Renderer::eWireframeOnly );
		renderer.SetState( m_state_point, Renderer::eFullMaterials );
		renderer.addRenderable( m_point, g_matrix4_identity );
		renderer.SetState( m_state_line, Renderer::eWireframeOnly );
		renderer.SetState( m_state_line, Renderer::eFullMaterials );
		renderer.addRenderable( m_line, g_matrix4_identity );
		renderer.addRenderable( m_midline, g_matrix4_identity );
	}
	void initialise(){
	}
	void highlight( const View& view ){
		SceneChangeNotify();
	}

	void testSelect( const View& view, const Matrix4& pivot2world ) {
		m_isSelected = true;
	}
	/* Manipulatable */
	void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
		//do things with undo
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	}

	Manipulatable* GetManipulatable() {
		m_isSelected = false; //don't handle the manipulator move part void MoveSelected()
		return this;
	}

	void setSelected( bool select ) {
		m_isSelected = select;
	}
	bool isSelected() const {
		return m_isSelected;
	}
};
Shader* BuildManipulator::m_state_point;
Shader* BuildManipulator::m_state_line;




class UVManipulator : public Manipulator, public Manipulatable
{
	struct RenderablePoint : public OpenGLRenderable
	{
		PointVertex m_point;
		RenderablePoint():
			m_point( vertex3f_identity ) {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_point.colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_point.vertex );
			glDrawArrays( GL_POINTS, 0, 1 );
		}
		void setColour( const Colour4b & colour ) {
			m_point.colour = colour;
		}
	};
	struct RenderableLines : public OpenGLRenderable
	{
		std::vector<PointVertex> m_lines;
		RenderableLines(){
		}
		void render( RenderStateFlags state ) const {
			if( m_lines.size() != 0 ){
				glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_lines[0].colour );
				glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_lines[0].vertex );
				glDrawArrays( GL_LINES, 0, m_lines.size() );
			}
		}
	};
	struct RenderableCircle : public OpenGLRenderable
	{
		Array<PointVertex> m_vertices;

		RenderableCircle( std::size_t size ) : m_vertices( size ){
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_vertices.data()->colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_vertices.data()->vertex );
			glDrawArrays( GL_LINE_LOOP, 0, GLsizei( m_vertices.size() ) );
		}
		void setColour( const Colour4b& colour ){
			for ( Array<PointVertex>::iterator i = m_vertices.begin(); i != m_vertices.end(); ++i )
			{
				( *i ).colour = colour;
			}
		}
	};
	class UVSelector : public Selector {
		SelectionIntersection m_bestIntersection;
	public:
		PointVertex* m_pointVertex;
		UVSelector() : m_bestIntersection( SelectionIntersection() ) {
		}
		void pushSelectable( Selectable& selectable ) {
		}
		void popSelectable() {
			m_bestIntersection = SelectionIntersection();
		}
		void addIntersection( const SelectionIntersection& intersection ) {
			if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
				m_bestIntersection = intersection;
			}
		}
		void addIntersection( const SelectionIntersection& intersection, PointVertex* pointVertex ) {
			if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
				m_bestIntersection = intersection;
				m_pointVertex = pointVertex;
			}
		}
		bool isSelected() {
			return m_bestIntersection.valid();
		}
	};
	const Colour4b m_cWhite{ 255, 255, 255, 255 };
	const Colour4b m_cGray{ 255, 255, 255, 125 };
	const Colour4b m_cGrayer{ 100, 100, 100, 150 };
	const Colour4b m_cRed{ 255, 0, 0, 255 };
	const Colour4b m_cGreen{ 0, 255, 0, 255 };

	enum EUVSelection{
		eNone,
		ePivot,
		eGridU,
		eGridV,
		eCircle,
		ePivotU,
		ePivotV,
		eU,
		eV,
		eUV,
		eSkewU,
		eSkewV,
		eTex,
	} m_selection;
	PointVertex* m_selectedU = 0; // must nullify this on m_Ulines, m_Vlines change
	PointVertex* m_selectedV = 0;
	bool m_isSelected = false;

	Face* m_face = 0;
	Plane3 m_plane;
	std::size_t m_width, m_height;
	TextureProjection m_projection;

	Matrix4 m_local2tex; //real projection
	Matrix4 m_tex2local; //real unprojection aka projection space basis aka texture axes
	Matrix4 m_faceLocal2tex; //x,y projected to the face for z = const
	Matrix4 m_faceTex2local;
	Vector3 m_origin;

	RenderablePivot m_pivot;
	Matrix4 m_pivot2world0; // original
	Matrix4 m_pivot2world; // transformed during transformation
	RenderablePoint m_pivotPoint;
	RenderableLines m_pivotLines;
	Matrix4 m_pivotLines2world;
	/* lines in uv space */
	RenderableLines m_Ulines;
	RenderableLines m_Vlines;
	Matrix4 m_lines2world; // line * ( transform during transformation ) * m_faceTex2local = world

	unsigned int m_gridU = 1; // n - 1 of U directed sub lines, 1-16
	unsigned int m_gridV = 1;
	RenderablePoint m_gridPointU; // control of U grid lines density, rendered on V axis
	RenderablePoint m_gridPointV;
	Vector2 m_gridSign; // orientation of controls relative to origin

	RenderableCircle m_circle;
	Matrix4 m_circle2world;

public:
	static Shader* m_state_line;
	static Shader* m_state_point;
	UVManipulator() : m_pivot( 32 ), m_circle( 8 << 3 ) {
		draw_circle( 8, 1, m_circle.m_vertices.data(), RemapXYZ() );
		m_circle.setColour( m_cGray );
		m_pivotPoint.setColour( m_cWhite );
		m_gridPointU.setColour( m_cWhite );
		m_gridPointV.setColour( m_cWhite );
		m_pivotLines.m_lines.resize( 4, PointVertex( vertex3f_identity, m_cWhite ) );
	}

private:

	bool UpdateFaceData( bool updateOrigin, bool updateLines = true ) {
		//!? todo fewer outer quads for large textures
		//!? todo auto subdivisions num, based on tex size and world scale
		//! todo update on undo/redo, when face stays the same, but transformed
		//! todo update on nudgeSelectedLeft and the rest, qe tool move w/o projection change or with tex lock off
		//+ todo put default origin to winding's UV aabb corner
		//+ todo disable 3d workzone in this manipulator mode
		m_plane = m_face->getPlane().plane3();
		m_width = m_face->getShader().width();
		m_height = m_face->getShader().height();
//		m_face->GetTexdef( m_projection );
		m_projection = m_face->getTexdef().m_projection;

		const Winding& winding = m_face->getWinding();

		Texdef_Construct_local2tex( m_projection, m_width, m_height, m_plane.normal(), m_local2tex );
		m_tex2local = matrix4_affine_inverse( m_local2tex );
//		globalOutputStream() << m_local2tex << " m_local2tex\n";
//		globalOutputStream() << m_tex2local << " m_tex2local\n";
		/* error checking */
		if( !std::isfinite( m_local2tex[0] ) //nan
			|| fabs( vector3_dot( m_plane.normal(), vector4_to_vector3( m_tex2local.z() ) ) ) < 1e-6 //projected along face
			|| vector3_length_squared( vector4_to_vector3( m_tex2local.x() ) ) < .01 //srsly scaled down, limit at max 10 textures per world unit
			|| vector3_length_squared( vector4_to_vector3( m_tex2local.y() ) ) < .01 ){
			m_selectedU = m_selectedV = 0;
			m_Ulines.m_lines.clear();
			m_Vlines.m_lines.clear();
			return false;
		}

		m_faceTex2local = m_tex2local;
		vector4_to_vector3( m_faceTex2local.x() ) = plane3_project_point( Plane3( m_plane.normal(), 0 ), vector4_to_vector3( m_tex2local.x() ), vector4_to_vector3( m_tex2local.z() ) );
		vector4_to_vector3( m_faceTex2local.y() ) = plane3_project_point( Plane3( m_plane.normal(), 0 ), vector4_to_vector3( m_tex2local.y() ), vector4_to_vector3( m_tex2local.z() ) );
		m_faceTex2local = matrix4_multiplied_by_matrix4( // adjust to have UV's z = 0: move the plane along m_tex2local.z() so that plane.dist() = 0
							matrix4_translation_for_vec3(
								vector4_to_vector3( m_tex2local.z() ) * ( m_plane.dist() - vector3_dot( m_plane.normal(), vector4_to_vector3( m_tex2local.t() ) ) )
								/ vector3_dot( m_plane.normal(), vector4_to_vector3( m_tex2local.z() ) )
							),
							m_faceTex2local );
		m_faceLocal2tex = matrix4_affine_inverse( m_faceTex2local );

		Vector2 min( FLT_MAX, FLT_MAX );
		Vector2 max( -FLT_MAX, -FLT_MAX );
		for( const auto& v : winding ){
			const Vector3 p = matrix4_transformed_point( m_faceLocal2tex, v.vertex );
			min.x() = std::min( min.x(), p.x() );
			max.x() = std::max( max.x(), p.x() );
			min.y() = std::min( min.y(), p.y() );
			max.y() = std::max( max.y(), p.y() );
		}

		if( updateOrigin )
			m_origin =  matrix4_transformed_point( m_faceTex2local, Vector3( min.x(), min.y(), 0 ) ); //winding.points[0].vertex;

		m_pivot2world = m_tex2local;
		vector3_normalise( vector4_to_vector3( m_pivot2world.x() ) );
		vector3_normalise( vector4_to_vector3( m_pivot2world.y() ) );
		vector4_to_vector3( m_pivot2world.t() ) = m_origin;
		m_pivot2world0 = m_pivot2world;

		{
			float bestDist = 0;
			for( const auto& v : winding ){
				const float dist = vector3_length_squared( v.vertex - m_origin );
				if( dist > bestDist ){
					bestDist = dist;
				}
			}
			bestDist = sqrt( bestDist );
			m_circle2world = g_matrix4_identity;
			//! todo gcc 7 declines this template use w/o cast, wth
			ComputeAxisBase( static_cast<Vector3>( m_plane.normal() ), vector4_to_vector3( m_circle2world.x() ), vector4_to_vector3( m_circle2world.y() ) );
			vector4_to_vector3( m_circle2world.x() ) *= bestDist;
			vector4_to_vector3( m_circle2world.y() ) *= bestDist;
			vector4_to_vector3( m_circle2world.z() ) = m_plane.normal();
			vector4_to_vector3( m_circle2world.t() ) = m_origin;
		}

		min -= Vector2( 5, 5 );
		max += Vector2( 5, 5 );
		min.x() = float_to_integer( min.x() );
		min.y() = float_to_integer( min.y() );
		max.x() = float_to_integer( max.x() );
		max.y() = float_to_integer( max.y() );

		m_selectedU = m_selectedV = 0;
		m_lines2world = m_faceTex2local;
		m_pivotLines2world = m_faceTex2local;
		if( updateLines ){
			const int imax = float_to_integer( max.y() - min.y() ) + 1;
			m_Ulines.m_lines.clear();
			m_Ulines.m_lines.reserve( ( imax + ( m_gridU - 1 ) * ( imax - 1 ) ) * 2 );
			for( int i = 0; i < imax; ++i ){
				if( i != 0 ){
					for( std::size_t j = m_gridU - 1; j != 0; --j ){ //subgrid lines
						m_Ulines.m_lines.emplace_back( Vertex3f( min.x(), min.y() + i - static_cast<float>( j ) / m_gridU, 0 ), m_cGrayer );
						m_Ulines.m_lines.emplace_back( Vertex3f( max.x(), min.y() + i - static_cast<float>( j ) / m_gridU, 0 ), m_cGrayer );
					}
				}
				m_Ulines.m_lines.emplace_back( Vertex3f( min.x(), min.y() + i, 0 ), m_cGray );
				m_Ulines.m_lines.emplace_back( Vertex3f( max.x(), min.y() + i, 0 ), m_cGray );
			}
		}
		if( updateLines ){
			const int imax = float_to_integer( max.x() - min.x() ) + 1;
			m_Vlines.m_lines.clear();
			m_Vlines.m_lines.reserve( ( imax + ( m_gridV - 1 ) * ( imax - 1 ) ) * 2 );
			for( int i = 0; i < imax; ++i ){
				if( i != 0 ){
					for( std::size_t j = m_gridV - 1; j != 0; --j ){
						m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i - static_cast<float>( j ) / m_gridV, min.y(), 0 ), m_cGrayer );
						m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i - static_cast<float>( j ) / m_gridV, max.y(), 0 ), m_cGrayer );
					}
				}
				m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i, min.y(), 0 ), m_cGray );
				m_Vlines.m_lines.emplace_back( Vertex3f( min.x() + i, max.y(), 0 ), m_cGray );
			}
		}
		{
			const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
			{ // u pivot line
				m_pivotLines.m_lines[0].vertex = Vertex3f( min.x(), uv_origin.y(), 0 );
				m_pivotLines.m_lines[1].vertex = Vertex3f( max.x(), uv_origin.y(), 0 );
			}
			{ // v pivot line
				m_pivotLines.m_lines[2].vertex = Vertex3f( uv_origin.x(), min.y(), 0 );
				m_pivotLines.m_lines[3].vertex = Vertex3f( uv_origin.x(), max.y(), 0 );
			}
			// grid grain controls, on the polygon side of origin
			m_gridSign.x() = max.y() - uv_origin.y() >=  uv_origin.y() - min.y()? 1 : -1;
			m_gridSign.y() = max.x() - uv_origin.x() >=  uv_origin.x() - min.x()? 1 : -1;
			m_gridPointU.m_point.vertex = Vertex3f( uv_origin.x(), float_to_integer( uv_origin.y() + m_gridSign.x() * .25 ) + m_gridSign.x() * ( 1 - 1.0 / std::max( float( m_gridU ), 1.8f ) ), 0 );
			m_gridPointV.m_point.vertex = Vertex3f( float_to_integer( uv_origin.x() + m_gridSign.y() * .25 ) + m_gridSign.y() * ( 1 - 1.0 / std::max( float( m_gridV ), 1.8f ) ), uv_origin.y(), 0 );
		}
		return true;
	}
	bool UpdateData() {
		if( !g_SelectedFaceInstances.empty() ){
			Face* face = &g_SelectedFaceInstances.last().getFace();
			if( m_face != face ){
				m_face = face;
				return UpdateFaceData( true );
			}
			else if( memcmp( &m_projection, &m_face->getTexdef().m_projection, sizeof( TextureProjection ) ) != 0
					|| m_width != m_face->getShader().width()
					|| m_height != m_face->getShader().height() ) {
				return UpdateFaceData( false );
			}
			return true;
		}
		return false;

	}
public:
	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) {
		if( volume.fill() && UpdateData() ){
			renderer.SetState( m_state_line, Renderer::eFullMaterials );
			renderer.addRenderable( m_Ulines, m_lines2world );
			renderer.addRenderable( m_Vlines, m_lines2world );
			renderer.addRenderable( m_pivotLines, m_pivotLines2world );
			//fix pivot position for better visibility
			m_pivot.render( renderer, volume, matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( vector3_normalised( volume.getViewer() - m_origin ) ), m_pivot2world ) );

			renderer.addRenderable( m_circle, m_circle2world );

			renderer.SetState( m_state_point, Renderer::eFullMaterials );
			renderer.addRenderable( m_pivotPoint, m_pivot2world );
			renderer.addRenderable( m_gridPointU, m_pivotLines2world );
			renderer.addRenderable( m_gridPointV, m_pivotLines2world );
		}
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) {
		//!? todo fix: eUV selection possibility may be blocked by the circle
		if( !view.fill() || !UpdateData() ){
			m_isSelected = false;
			return;
		}

		EUVSelection selection = eNone;

		UVSelector selector;
		{ // try pivot point
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot2world ) );
			SelectionIntersection best;
			Point_BestPoint( local2view, m_pivotPoint.m_point.vertex, best );
			selector.addIntersection( best );
			if( selector.isSelected() ){
				selection = ePivot;
			}
		}

		if( !selector.isSelected() ){ // try grid points
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			SelectionIntersection best;
			Point_BestPoint( local2view, m_gridPointU.m_point.vertex, best );
			selector.addIntersection( best );
			if( selector.isSelected() ){
				selection = eGridU;
			}
			else{
				Point_BestPoint( local2view, m_gridPointV.m_point.vertex, best );
				selector.addIntersection( best );
				if( selector.isSelected() ){
					selection = eGridV;
				}
			}
		}

		if( !selector.isSelected() ){ // try circle
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_circle2world ) );
			SelectionIntersection best;
			LineLoop_BestPoint( local2view, m_circle.m_vertices.data(), m_circle.m_vertices.size(), best );
			selector.addIntersection( best );
			if( selector.isSelected() ){
				selection = eCircle;
			}
		}

		if( !selector.isSelected() ){ // try pivot lines
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			SelectionIntersection best;
			Line_BestPoint( local2view, &m_pivotLines.m_lines[0], best );
			selector.addIntersection( best );
			if( selector.isSelected() ){
				selection = ePivotU;
			}
			else{
				Line_BestPoint( local2view, &m_pivotLines.m_lines[2], best );
				selector.addIntersection( best );
				if( selector.isSelected() ){
					selection = ePivotV;
				}
			}
		}

		PointVertex* selectedU = 0;
		PointVertex* selectedV = 0;
		if( !selector.isSelected() ){ // try UV lines
#if 1
/*
            -|------
             |
             |
V line center| - -  tex U center - -
             |         tex
             |          V
      -cross-|-----U line center-----|
             |
*/
			// special fuckage with the grid for better distinguishing of user's intentions
			// better picking of tex, only line for skew or scale with dense grid
			const Matrix4 screen2world( matrix4_full_inverse( view.GetViewMatrix() ) );
			const DoubleRay ray = ray_for_points( vector4_projected( matrix4_transformed_vector4( screen2world, BasicVector4<double>( 0, 0, -1, 1 ) ) ),
												vector4_projected( matrix4_transformed_vector4( screen2world, BasicVector4<double>( 0, 0, 1, 1 ) ) ) );
			const DoubleVector3 hit = ray_intersect_plane( ray, m_plane );
			const Vector3 uvhit = matrix4_transformed_point( m_faceLocal2tex, hit );
			if( fabs( vector3_dot( ray.direction, m_plane.normal() ) ) > 1e-6
				&& !m_Ulines.m_lines.empty()
				&& !m_Vlines.m_lines.empty()
				&& matrix4_transformed_vector4( view.GetViewMatrix(), Vector4( hit, 1 ) ).w() > 0 ){
				PointVertex* closestU = &m_Ulines.m_lines[std::min( m_Ulines.m_lines.size() - 2,
														static_cast<std::size_t>( float_to_integer( std::max( 0.f, uvhit.y() - m_Ulines.m_lines.front().vertex.y() ) * m_gridU ) * 2 ) )];
				PointVertex* closestV = &m_Vlines.m_lines[std::min( m_Vlines.m_lines.size() - 2,
														static_cast<std::size_t>( float_to_integer( std::max( 0.f, uvhit.x() - m_Vlines.m_lines.front().vertex.x() ) * m_gridV ) * 2 ) )];
				const Vector2 sign( uvhit.y() > closestU->vertex.y()? 1 : -1, uvhit.x() > closestV->vertex.x()? 1 : -1 ); //hit in positive or negative part of lines u, v
				const PointVertex pCross( Vertex3f( closestV->vertex.x(), closestU->vertex.y(), 0 ) );
				const PointVertex pUcenter( Vertex3f( closestV->vertex.x() + sign.y() / ( m_gridV * 2 ), closestU->vertex.y(), 0 ) );
				const PointVertex pVcenter( Vertex3f( closestV->vertex.x(), closestU->vertex.y() + sign.x() / ( m_gridU * 2 ), 0 ) );

				PointVertex pTexUcenter[2]{ *closestU, *( closestU + 1 ) };
				pTexUcenter[0].vertex.y() = pTexUcenter[1].vertex.y() = pVcenter.vertex.y();
				PointVertex pTexVcenter[2]{ *closestV, *( closestV + 1 ) };
				pTexVcenter[0].vertex.x() = pTexVcenter[1].vertex.x() = pUcenter.vertex.x();

				SelectionIntersection iCross, iUcenter, iVcenter, iTexUcenter, iTexVcenter, iU, iV, iNull;

				const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
#if defined( DEBUG_SELECTION )
				g_render_clipped.construct( view.GetViewMatrix() );
#endif
				Line_BestPoint( local2view, closestU, iU );
				Line_BestPoint( local2view, closestV, iV );
				Line_BestPoint( local2view, pTexUcenter, iTexUcenter );
				Line_BestPoint( local2view, pTexVcenter, iTexVcenter );
				const bool uselected = iU < iNull;
				const bool vselected = iV < iNull;
				if( !uselected && !vselected ){ //no lines hit, definitely tex
					selection = eTex;
				}
				else if( ( !uselected || iTexUcenter < iU ) && ( !vselected || iTexVcenter < iV ) ){ //yes lines, but tex ones are closer
					selection = eTex;
				}
				else if( uselected != vselected ){ //only line selected
					if( uselected ){
						selection = g_bAltResize_AltSelect? eSkewU : eU;
						selectedU = closestU;
					}
					else{
						selection = g_bAltResize_AltSelect? eSkewV : eV;
						selectedV = closestV;
					}
				}
				else{ //two lines hit
					if( g_bAltResize_AltSelect ){ //pick only line for skew
						if( iU < iV ){
							selection = eSkewU;
							selectedU = closestU;
						}
						else{
							selection = eSkewV;
							selectedV = closestV;
						}
					}
					else{
						Point_BestPoint( local2view, pUcenter, iUcenter );
						Point_BestPoint( local2view, pVcenter, iVcenter );
						Point_BestPoint( local2view, pCross, iCross );
						const bool ucenter = iUcenter < iNull;
						const bool vcenter = iVcenter < iNull;
						if( !ucenter && !vcenter ){ // no centers, definitely two lines
							selection = eUV;
							selectedU = closestU;
							selectedV = closestV;
						}
						else if( iCross < iUcenter && iCross < iVcenter ){ // some center(s), cross is closer = two lines
							selection = eUV;
							selectedU = closestU;
							selectedV = closestV;
						}
						else{ // some center(s), pick closest line
							if( iUcenter < iVcenter ){
								selection = eU;
								selectedU = closestU;
							}
							else{
								selection = eV;
								selectedV = closestV;
							}
						}
					}
				}
			}
#else
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_faceTex2local ) );
			{
				SelectionIntersection best;
				for( std::vector<PointVertex>::iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
					Line_BestPoint( local2view, &( *i ), best );
					selector.addIntersection( best, &( *i ) );
				}
				if( selector.isSelected() ){
					selection = g_bAltResize_AltSelect? eSkewU : eU;
					selectedU = selector.m_pointVertex;
				}
			}
			{
				selector.popSelectable();
				SelectionIntersection best;
				for( std::vector<PointVertex>::iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
					Line_BestPoint( local2view, &( *i ), best );
					selector.addIntersection( best, &( *i ) );
				}
				if( selector.isSelected() ){
					selection = g_bAltResize_AltSelect? eSkewV : eV;
					selectedV = selector.m_pointVertex;
				}
			}
			if( selectedU && selectedV ){
				selection = eUV;
				if( g_bAltResize_AltSelect ){
					selection = eSkewU;
					selectedV = 0;
				}
			}
#endif
		}

		applySelection( selection, selectedU, selectedV );
	}
private:
	void applySelection( EUVSelection selection, PointVertex* selectedU, PointVertex* selectedV ){
		if( m_selection != selection
			|| m_selectedU != selectedU
			|| m_selectedV != selectedV ){
			if( m_selection != selection ){
				switch ( m_selection )
				{
				case ePivot:
					m_pivotPoint.m_point.colour = m_cWhite;
					break;
				case eGridU:
					m_gridPointU.m_point.colour = m_cWhite;
					break;
				case eGridV:
					m_gridPointV.m_point.colour = m_cWhite;
					break;
				case eCircle:
					m_circle.setColour( m_cGray );
					break;
				case ePivotU:
					m_pivotLines.m_lines[0].colour = m_cWhite;
					m_pivotLines.m_lines[1].colour = m_cWhite;
					break;
				case ePivotV:
					m_pivotLines.m_lines[2].colour = m_cWhite;
					m_pivotLines.m_lines[3].colour = m_cWhite;
					break;
				default:
					break;
				}
				switch ( selection )
				{
				case ePivot:
					m_pivotPoint.m_point.colour = m_cRed;
					break;
				case eGridU:
					m_gridPointU.m_point.colour = m_cRed;
					break;
				case eGridV:
					m_gridPointV.m_point.colour = m_cRed;
					break;
				case eCircle:
					m_circle.setColour( g_colour_selected );
					break;
				case ePivotU:
					m_pivotLines.m_lines[0].colour = m_cRed;
					m_pivotLines.m_lines[1].colour = m_cRed;
					break;
				case ePivotV:
					m_pivotLines.m_lines[2].colour = m_cRed;
					m_pivotLines.m_lines[3].colour = m_cRed;
					break;
				default:
					break;
				}
			}

			const Colour4b colour_selected = g_bAltResize_AltSelect? m_cGreen : g_colour_selected;
			if( m_selectedU != selectedU || m_selection != selection ){ // selected line changed or not, but scale<->skew modes exchanged
				if( m_selectedU )
					m_selectedU->colour =
					( m_selectedU + 1 )->colour = ( ( m_selectedU - &m_Ulines.m_lines[0] ) / 2 ) % m_gridU == 0? m_cGray : m_cGrayer;
				if( selectedU )
					selectedU->colour =
					( selectedU + 1 )->colour = colour_selected;
			}
			if( m_selectedV != selectedV || m_selection != selection ){
				if( m_selectedV )
					m_selectedV->colour =
					( m_selectedV + 1 )->colour = ( ( m_selectedV - &m_Vlines.m_lines[0] ) / 2 ) % m_gridV == 0? m_cGray : m_cGrayer;
				if( selectedV )
					selectedV->colour =
					( selectedV + 1 )->colour = colour_selected;
			}

			m_selection = selection;
			m_selectedU = selectedU;
			m_selectedV = selectedV;
			SceneChangeNotify();
		}
		m_isSelected = ( selection != eNone );
	}
	/* Manipulatable */
	Vector3 m_start;
public:
	void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
		m_start = point_on_plane( m_plane, m_view->GetViewMatrix(), x, y );
	}
	//!? todo change snap dist measurement from world to screenspace
	//! fix meaningless undo on grid/origin change, then click tex or lines
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapHard, const bool alt ){
		Vector3 current = point_on_plane( m_plane, m_view->GetViewMatrix(), x, y );
		switch ( m_selection )
		{
		case ePivot:
			{
				const Vector3 uv_origin_start = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, current );
				float bestDistU = FLT_MAX;
				float bestDistV = FLT_MAX;
				float snapToU = 0;
				float snapToV = 0;
				for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
					const float dist = fabs( ( *i ).vertex.y() - uv_origin.y() );
					if( dist < bestDistU ){
						bestDistU = dist;
						snapToU = ( *i ).vertex.y();
					}
				}
				for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
					const float dist = fabs( ( *i ).vertex.x() - uv_origin.x() );
					if( dist < bestDistV ){
						bestDistV = dist;
						snapToV = ( *i ).vertex.x();
					}
				}
				for( const auto& v : m_face->getWinding() ){
					const Vector3 p = matrix4_transformed_point( m_faceLocal2tex, v.vertex );
					const float distU = fabs( p.y() - uv_origin.y() );
					if( distU < bestDistU ){
						bestDistU = distU;
						snapToU = p.y();
					}
					const float distV = fabs( p.x() - uv_origin.x() );
					if( distV < bestDistV ){
						bestDistV = distV;
						snapToV = p.x();
					}
				}
				Vector3 result( uv_origin_start );
				if( bestDistU * vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ) < 3 || snapHard ){
					result.y() = snapToU;
				}
				else{
					result.y() = uv_origin.y();
				}
				if( bestDistV * vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) < 3 || snapHard ){
					result.x() = snapToV;
				}
				else{
					result.x() = uv_origin.x();
				}
				m_origin = matrix4_transformed_point( m_faceTex2local, result );
				UpdateFaceData( false, false );
				SceneChangeNotify();
			}
			break;
		case ePivotU:
			{
				const Vector3 uv_origin_start = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, current );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
					const float dist = fabs( ( *i ).vertex.y() - uv_origin.y() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = ( *i ).vertex.y();
					}
				}
				for( const auto& v : m_face->getWinding() ){
					const Vector3 p = matrix4_transformed_point( m_faceLocal2tex, v.vertex );
					const float dist = fabs( p.y() - uv_origin.y() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = p.y();
					}
				}
				Vector3 result( uv_origin_start );
				if( bestDist * vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ) < 3 || snapHard ){
					result.y() = snapTo;
				}
				else{
					result.y() = uv_origin.y();
				}
				m_origin = matrix4_transformed_point( m_faceTex2local, result );
				UpdateFaceData( false, false );
				SceneChangeNotify();
			}
			break;
		case ePivotV:
			{
				const Vector3 uv_origin_start = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, current );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
					const float dist = fabs( ( *i ).vertex.x() - uv_origin.x() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = ( *i ).vertex.x();
					}
				}
				for( const auto& v : m_face->getWinding() ){
					const Vector3 p = matrix4_transformed_point( m_faceLocal2tex, v.vertex );
					const float dist = fabs( p.x() - uv_origin.x() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = p.x();
					}
				}
				Vector3 result( uv_origin_start );
				if( bestDist * vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) < 3 || snapHard ){
					result.x() = snapTo;
				}
				else{
					result.x() = uv_origin.x();
				}
				m_origin = matrix4_transformed_point( m_faceTex2local, result );
				UpdateFaceData( false, false );
				SceneChangeNotify();
			}
			break;
		case eGridU:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_current = matrix4_transformed_point( m_faceLocal2tex, current );

				const float dist = std::max( ( float_to_integer( uv_origin.y() + m_gridSign.x() * .25 ) + m_gridSign.x() - uv_current.y() ) * m_gridSign.x(), .01f );
				unsigned int grid = std::max( 1, std::min( 16, int( 1 / dist ) ) );

				if( snapHard ){ // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
					grid--;
					grid |= grid >> 1;
					grid |= grid >> 2;
					grid |= grid >> 4;
					grid |= grid >> 8;
					grid |= grid >> 16;
					grid++;
				}

				if( m_gridU != grid || ( snap && m_gridV != grid ) ){
					m_gridU = grid;
					if( snap )
						m_gridV = grid;
					UpdateFaceData( false );
					SceneChangeNotify();
				}
			}
			break;
		case eGridV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_current = matrix4_transformed_point( m_faceLocal2tex, current );

				const float dist = std::max( ( float_to_integer( uv_origin.x() + m_gridSign.y() * .25 ) + m_gridSign.y() - uv_current.x() ) * m_gridSign.y(), .01f );
				unsigned int grid = std::max( 1, std::min( 16, int( 1 / dist ) ) );

				if( snapHard ){ // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
					grid--;
					grid |= grid >> 1;
					grid |= grid >> 2;
					grid |= grid >> 4;
					grid |= grid >> 8;
					grid |= grid >> 16;
					grid++;
				}

				if( m_gridV != grid || ( snap && m_gridU != grid ) ){
					m_gridV = grid;
					if( snap )
						m_gridU = grid;
					UpdateFaceData( false );
					SceneChangeNotify();
				}
			}
			break;
		case eCircle:
			{
				Vector3 from = m_start - m_origin;
				constrain_to_axis( from, vector4_to_vector3( m_tex2local.z() ) );
				Vector3 to = current - m_origin;
				constrain_to_axis( to, vector4_to_vector3( m_tex2local.z() ) );
				Matrix4 rot = g_matrix4_identity;
				if( snap ){
					matrix4_pivoted_rotate_by_axisangle( rot,
														vector4_to_vector3( m_tex2local.z() ),
														float_snapped( angle_for_axis( from, to, vector4_to_vector3( m_tex2local.z() ) ), static_cast<float>( c_pi / 12.0 ) ),
														m_origin );
				}
				else{
					matrix4_pivoted_rotate_by_axisangle( rot,
														vector4_to_vector3( m_tex2local.z() ),
														angle_for_axis( from, to, vector4_to_vector3( m_tex2local.z() ) ),
														m_origin );
				}
				{ // snap
					const Winding& winding = m_face->getWinding();
					const Vector3 uvec = vector3_normalised( matrix4_transformed_direction( rot, vector4_to_vector3( m_tex2local.x() ) ) );
					const Vector3 vvec = vector3_normalised( matrix4_transformed_direction( rot, vector4_to_vector3( m_tex2local.y() ) ) );
					float bestDot = 0;
					Vector3 bestTo;
					bool V = false;
					for( Winding::const_iterator next = winding.begin(), i = winding.end() - 1; next != winding.end(); i = next, ++next ){
						Vector3 vec( ( *next ).vertex - ( *i ).vertex );
						constrain_to_axis( vec, vector4_to_vector3( m_tex2local.z() ) );
						const float dotU = fabs( vector3_dot( uvec, vec ) );
						if( dotU > bestDot ){
							bestDot = dotU;
							bestTo = vector3_dot( uvec, vec ) > 0? vec : -vec;
							V = false;
						}
						const float dotV = fabs( vector3_dot( vvec, vec ) );
						if( dotV > bestDot ){
							bestDot = dotV;
							bestTo = vector3_dot( vvec, vec ) > 0? vec : -vec;
							V = true;
						}
					}
					if( bestDot > 0.9994f || snapHard ){
						const Vector3 bestFrom = vector3_normalised( vector4_to_vector3( V? m_tex2local.y() : m_tex2local.x() ) );
						rot = g_matrix4_identity;
						matrix4_pivoted_rotate_by_axisangle( rot,
															vector4_to_vector3( m_tex2local.z() ),
															angle_for_axis( bestFrom, bestTo, vector4_to_vector3( m_tex2local.z() ) ),
															m_origin );
					}
				}

				Matrix4 faceTex2local = matrix4_multiplied_by_matrix4( rot, m_tex2local );
				vector4_to_vector3( faceTex2local.x() ) = plane3_project_point( Plane3( m_plane.normal(), 0 ), vector4_to_vector3( faceTex2local.x() ), vector4_to_vector3( m_tex2local.z() ) );
				vector4_to_vector3( faceTex2local.y() ) = plane3_project_point( Plane3( m_plane.normal(), 0 ), vector4_to_vector3( faceTex2local.y() ), vector4_to_vector3( m_tex2local.z() ) );
				faceTex2local = matrix4_multiplied_by_matrix4( // adjust to have UV's z = 0: move the plane along m_tex2local.z() so that plane.dist() = 0
							matrix4_translation_for_vec3(
								vector4_to_vector3( m_tex2local.z() ) * ( m_plane.dist() - vector3_dot( m_plane.normal(), vector4_to_vector3( faceTex2local.t() ) ) )
								/ vector3_dot( m_plane.normal(), vector4_to_vector3( m_tex2local.z() ) )
							),
							faceTex2local );
				m_lines2world = m_pivotLines2world = faceTex2local;

				m_pivot2world = matrix4_multiplied_by_matrix4( rot, m_pivot2world0 );

				m_face->transform_texdef( rot, m_origin ); //! todo make SI update after Brush_textureChanged(); same problem after brush moved with tex lock
				SceneChangeNotify();
			}
			break;
		case eU:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_local2tex, m_origin );
				const Vector3 uv_start = m_selectedU->vertex;
				const Vector3 uv_current = m_selectedU->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				for( const auto& v : m_face->getWinding() ){
					const Vector3 p = matrix4_transformed_point( m_local2tex, v.vertex );
					const float dist = fabs( p.y() - uv_current.y() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = p.y();
					}
				}
				Vector3 result( 1, uv_current.y(), 1 );
				if( bestDist * vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ) < 3 || snapHard ){
					result.y() = snapTo;
				}
				result.y() = ( result.y() - uv_origin.y() ) / ( uv_start.y() - uv_origin.y() );

				if( snap )
					result.x() = fabs( result.y() );

				/* prevent scaling to 0, limit at max 10 textures per world unit */
				if( vector3_length_squared( vector4_to_vector3( m_tex2local.y() ) * result.y() ) < .01 )
					return;

				Matrix4 scale = g_matrix4_identity;
				matrix4_pivoted_scale_by_vec3( scale, result, uv_origin );
				// transform_local2object
				scale = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_tex2local, scale ),
						m_local2tex
						);
				{
					Matrix4 linescale = g_matrix4_identity;
					matrix4_pivoted_scale_by_vec3( linescale, result, matrix4_transformed_point( m_faceLocal2tex, m_origin ) );
					m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, linescale );

					m_pivot2world = matrix4_multiplied_by_matrix4( m_pivot2world0, matrix4_scale_for_vec3( result ) );
				}
				m_face->transform_texdef( scale, m_origin );
				SceneChangeNotify();
			}
			break;
		case eV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_local2tex, m_origin );
				const Vector3 uv_start = m_selectedV->vertex;
				const Vector3 uv_current = m_selectedV->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start );
				float bestDist = FLT_MAX;
				float snapTo = 0;
				for( const auto& v : m_face->getWinding() ){
					const Vector3 p = matrix4_transformed_point( m_local2tex, v.vertex );
					const float dist = fabs( p.x() - uv_current.x() );
					if( dist < bestDist ){
						bestDist = dist;
						snapTo = p.x();
					}
				}
				Vector3 result( uv_current.x(), 1, 1 );
				if( bestDist * vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) < 3 || snapHard ){
					result.x() = snapTo;
				}
				result.x() = ( result.x() - uv_origin.x() ) / ( uv_start.x() - uv_origin.x() );

				if( snap )
					result.y() = fabs( result.x() );

				/* prevent scaling to 0, limit at max 10 textures per world unit */
				if( vector3_length_squared( vector4_to_vector3( m_tex2local.x() ) * result.x() ) < .01 )
					return;

				Matrix4 scale = g_matrix4_identity;
				matrix4_pivoted_scale_by_vec3( scale, result, uv_origin );
				// transform_local2object
				scale = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_tex2local, scale ),
						m_local2tex
						);
				{
					Matrix4 linescale = g_matrix4_identity;
					matrix4_pivoted_scale_by_vec3( linescale, result, matrix4_transformed_point( m_faceLocal2tex, m_origin ) );
					m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, linescale );

					m_pivot2world = matrix4_multiplied_by_matrix4( m_pivot2world0, matrix4_scale_for_vec3( result ) );
				}
				m_face->transform_texdef( scale, m_origin );
				SceneChangeNotify();
			}
			break;
		case eUV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_local2tex, m_origin );
				const Vector3 uv_start{ m_selectedV->vertex.x(), m_selectedU->vertex.y(), 0 } ;
				const Vector3 uv_current{ ( m_selectedV->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start ) ).x(),
											( m_selectedU->vertex + matrix4_transformed_point( m_local2tex, current ) - matrix4_transformed_point( m_local2tex, m_start ) ).y(),
											0 };
				float bestDistU = FLT_MAX;
				float snapToU = 0;
				float bestDistV = FLT_MAX;
				float snapToV = 0;
				for( const auto& v : m_face->getWinding() ){
					const Vector3 p = matrix4_transformed_point( m_local2tex, v.vertex );
					const float distU = fabs( p.y() - uv_current.y() );
					if( distU < bestDistU ){
						bestDistU = distU;
						snapToU = p.y();
					}
					const float distV = fabs( p.x() - uv_current.x() );
					if( distV < bestDistV ){
						bestDistV = distV;
						snapToV = p.x();
					}
				}

				Vector3 result( uv_current.x(), uv_current.y(), 1 );
				if( bestDistU * vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ) < 3 || snapHard ){
					result.y() = snapToU;
				}
				result.y() = ( result.y() - uv_origin.y() ) / ( uv_start.y() - uv_origin.y() );

				if( bestDistV * vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) < 3 || snapHard ){
					result.x() = snapToV;
				}
				result.x() = ( result.x() - uv_origin.x() ) / ( uv_start.x() - uv_origin.x() );

				if( snap ){
					const std::size_t best = fabs( result.x() ) > fabs( result.y() )? 0 : 1;
					result[( best + 1 ) % 2] = std::copysign( result[best], result[( best + 1 ) % 2] );
				}

				/* prevent scaling to 0, limit at max 10 textures per world unit */
				if( vector3_length_squared( vector4_to_vector3( m_tex2local.x() ) * result.x() ) < .01 ||
					vector3_length_squared( vector4_to_vector3( m_tex2local.y() ) * result.y() ) < .01 )
					return;

				Matrix4 scale = g_matrix4_identity;
				matrix4_pivoted_scale_by_vec3( scale, result, uv_origin );
				// transform_local2object
				scale = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_tex2local, scale ),
						m_local2tex
						);
				{
					Matrix4 linescale = g_matrix4_identity;
					matrix4_pivoted_scale_by_vec3( linescale, result, matrix4_transformed_point( m_faceLocal2tex, m_origin ) );
					m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, linescale );

					m_pivot2world = matrix4_multiplied_by_matrix4( m_pivot2world0, matrix4_scale_for_vec3( result ) );
				}
				m_face->transform_texdef( scale, m_origin );
				SceneChangeNotify();
			}
			break;
		case eSkewU:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_move = matrix4_transformed_point( m_faceLocal2tex, current ) - matrix4_transformed_point( m_faceLocal2tex, m_start );
				Matrix4 skew( g_matrix4_identity );
				skew[4] = uv_move.x() / ( m_selectedU->vertex - uv_origin ).y();

				Matrix4 scale = matrix4_scale_for_vec3( // scale snap measurement space so that x/y = 1
									Vector3( vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) / vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ),
												1, 1 ) );
				const Vector3 skewed = vector3_normalised( matrix4_transformed_direction( matrix4_multiplied_by_matrix4( scale, skew ), g_vector3_axis_y ) );
				matrix4_multiply_by_matrix4( scale, m_faceLocal2tex );
				float bestDot = 0;
				Vector3 bestTo;
				const Winding& winding = m_face->getWinding();
				for( Winding::const_iterator next = winding.begin(), i = winding.end() - 1; next != winding.end(); i = next, ++next ){
					const Vector3 vec( vector3_normalised( matrix4_transformed_point( scale, ( *next ).vertex ) -
															matrix4_transformed_point( scale, ( *i ).vertex ) ) );
					const float dot = fabs( vector3_dot( skewed, vec ) );
					if( dot > bestDot
						&& fabs( vector3_dot( vec, g_vector3_axis_x ) ) < 0.99999 ){ // don't snap so, that one axis = the other
						bestDot = dot;
						const Vector3 vecTo( vector3_normalised( matrix4_transformed_point( m_faceLocal2tex, ( *next ).vertex ) -
																matrix4_transformed_point( m_faceLocal2tex, ( *i ).vertex ) ) );
						bestTo = vector3_dot( skewed, vec ) > 0? vecTo : -vecTo;
					}
				}
				if( bestDot > 0.9994f || snapHard ){
					skew[4] = bestTo.x() / bestTo.y();
				}

				{
					Matrix4 mat( g_matrix4_identity );
					matrix4_translate_by_vec3( mat, uv_origin );
					matrix4_multiply_by_matrix4( mat, skew );
					matrix4_translate_by_vec3( mat, -uv_origin );
					skew = mat;
				}

				m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, skew );
				// transform_local2object
				m_pivot2world = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_tex2local, skew ),
						m_local2tex
						);
				matrix4_multiply_by_matrix4( m_pivot2world, m_pivot2world0 );

				// transform_local2object
				skew = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_faceTex2local, skew ),
						m_faceLocal2tex
						);
				m_face->transform_texdef( skew, m_origin );
				SceneChangeNotify();
			}
			break;
		case eSkewV:
			{
				const Vector3 uv_origin = matrix4_transformed_point( m_faceLocal2tex, m_origin );
				const Vector3 uv_move = matrix4_transformed_point( m_faceLocal2tex, current ) - matrix4_transformed_point( m_faceLocal2tex, m_start );
				Matrix4 skew( g_matrix4_identity );
				skew[1] = uv_move.y() / ( m_selectedV->vertex - uv_origin ).x();

				Matrix4 scale = matrix4_scale_for_vec3( // scale snap measurement space so that x/y = 1
									Vector3( vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) / vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ),
												1, 1 ) );
				const Vector3 skewed = vector3_normalised( matrix4_transformed_direction( matrix4_multiplied_by_matrix4( scale, skew ), g_vector3_axis_x ) );
				matrix4_multiply_by_matrix4( scale, m_faceLocal2tex );
				float bestDot = 0;
				Vector3 bestTo;
				const Winding& winding = m_face->getWinding();
				for( Winding::const_iterator next = winding.begin(), i = winding.end() - 1; next != winding.end(); i = next, ++next ){
					const Vector3 vec( vector3_normalised( matrix4_transformed_point( scale, ( *next ).vertex ) -
															matrix4_transformed_point( scale, ( *i ).vertex ) ) );
					const float dot = fabs( vector3_dot( skewed, vec ) );
					if( dot > bestDot
						&& fabs( vector3_dot( vec, g_vector3_axis_y ) ) < 0.99999 ){ // don't snap so, that one axis = the other
						bestDot = dot;
						const Vector3 vecTo( vector3_normalised( matrix4_transformed_point( m_faceLocal2tex, ( *next ).vertex ) -
																matrix4_transformed_point( m_faceLocal2tex, ( *i ).vertex ) ) );
						bestTo = vector3_dot( skewed, vec ) > 0? vecTo : -vecTo;
					}
				}
				if( bestDot > 0.9994f || snapHard ){
					skew[1] = bestTo.y() / bestTo.x();
				}

				{
					Matrix4 mat( g_matrix4_identity );
					matrix4_translate_by_vec3( mat, uv_origin );
					matrix4_multiply_by_matrix4( mat, skew );
					matrix4_translate_by_vec3( mat, -uv_origin );
					skew = mat;
				}

				m_lines2world = m_pivotLines2world = matrix4_multiplied_by_matrix4( m_faceTex2local, skew );
				// transform_local2object
				m_pivot2world = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_tex2local, skew ),
						m_local2tex
						);
				matrix4_multiply_by_matrix4( m_pivot2world, m_pivot2world0 );

				// transform_local2object
				skew = matrix4_multiplied_by_matrix4(
						matrix4_multiplied_by_matrix4( m_faceTex2local, skew ),
						m_faceLocal2tex
						);
				m_face->transform_texdef( skew, m_origin );
				SceneChangeNotify();
			}
			break;
		case eTex:
			{
				const Vector3 uvstart = matrix4_transformed_point( m_faceLocal2tex, m_start );
				const Vector3 uvcurrent = matrix4_transformed_point( m_faceLocal2tex, current );
				const Vector3 uvmove = uvcurrent - uvstart;
				float bestDistU = FLT_MAX;
				float bestDistV = FLT_MAX;
				float snapMoveU = 0;
				float snapMoveV = 0;
				const Winding& winding = m_face->getWinding();
				// snap uvmove
				for( const auto& v : winding ){
					const Vector3 p = matrix4_transformed_point( m_faceLocal2tex, v.vertex );
					for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
						const float dist = p.y() - ( ( *i ).vertex.y() + uvmove.y() );
						if( fabs( dist ) < bestDistU ){
							bestDistU = fabs( dist );
							snapMoveU = uvmove.y() + dist;
						}
					}
					for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
						const float dist = p.x() - ( ( *i ).vertex.x() + uvmove.x() );
						if( fabs( dist ) < bestDistV ){
							bestDistV = fabs( dist );
							snapMoveV = uvmove.x() + dist;
						}
					}
				}
				{
					const Vector3 p = matrix4_transformed_point( m_faceLocal2tex, m_origin ); // uvorigin
					for( std::vector<PointVertex>::const_iterator i = m_Ulines.m_lines.begin(); i != m_Ulines.m_lines.end(); ++++i ){
						const float dist = p.y() - ( ( *i ).vertex.y() + uvmove.y() );
						if( fabs( dist ) < bestDistU ){
							bestDistU = fabs( dist );
							snapMoveU = uvmove.y() + dist;
						}
					}
					for( std::vector<PointVertex>::const_iterator i = m_Vlines.m_lines.begin(); i != m_Vlines.m_lines.end(); ++++i ){
						const float dist = p.x() - ( ( *i ).vertex.x() + uvmove.x() );
						if( fabs( dist ) < bestDistV ){
							bestDistV = fabs( dist );
							snapMoveV = uvmove.x() + dist;
						}
					}
				}

				Vector3 result( uvmove );
				if( bestDistU * vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ) < 3 || snapHard ){
					result.y() = snapMoveU;
				}
				if( bestDistV * vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) < 3 || snapHard ){
					result.x() = snapMoveV;
				}

				if( snap ){
					auto& smaller = fabs( uvmove.x() * vector3_length( vector4_to_vector3( m_faceTex2local.x() ) ) ) <
									fabs( uvmove.y() * vector3_length( vector4_to_vector3( m_faceTex2local.y() ) ) )? result.x() : result.y();
					smaller = 0;
				}

				// translation_local2object
				result = matrix4_get_translation_vec3(
					matrix4_multiplied_by_matrix4(
						matrix4_translated_by_vec3( m_faceTex2local, result ),
						m_faceLocal2tex
						)
					);

				const Matrix4 translation = matrix4_translation_for_vec3( result );

				m_lines2world = matrix4_multiplied_by_matrix4( translation, m_faceTex2local );

				m_face->transform_texdef( translation );
				SceneChangeNotify();
			}
			break;
		default:
			break;
		}
	}

	void freezeTransform(){
		if( m_face &&
			( m_selection == eCircle
			|| m_selection == eU
			|| m_selection == eV
			|| m_selection == eUV
			|| m_selection == eSkewU
			|| m_selection == eSkewV
			|| m_selection == eTex ) )
			m_face->freezeTransform();
	}

	Manipulatable* GetManipulatable() {
		return this;
	}

	void setSelected( bool select ) {
		m_isSelected = select;
	}
	bool isSelected() const {
		return m_isSelected;
	}
};

Shader* UVManipulator::m_state_line;
Shader* UVManipulator::m_state_point;




class TransformOriginTranslatable
{
public:
virtual void transformOriginTranslate( const Vector3& translation, const bool set[3] ) = 0;
};

class TransformOriginTranslate : public Manipulatable
{
private:
Vector3 m_start;
TransformOriginTranslatable& m_transformOriginTranslatable;
public:
TransformOriginTranslate( TransformOriginTranslatable& transformOriginTranslatable )
	: m_transformOriginTranslatable( transformOriginTranslatable ){
}
void Construct( const Matrix4& device2manip, const float x, const float y, const AABB& bounds, const Vector3& transform_origin ){
	m_start = point_on_plane( device2manip, x, y );
}
void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const float x, const float y, const bool snap, const bool snapbbox, const bool alt ){
	Vector3 current = point_on_plane( device2manip, x, y );
	current = vector3_subtracted( current, m_start );

	if( snap ){
		for ( std::size_t i = 0; i < 3 ; ++i ){
			if( fabs( current[i] ) >= fabs( current[(i + 1) % 3] ) ){
				current[(i + 1) % 3] = 0.f;
			}
			else{
				current[i] = 0.f;
			}
		}
	}

	bool set[3] = { true, true, true };
	for ( std::size_t i = 0; i < 3 ; ++i ){
		if( fabs( current[i] ) < 1e-3f ){
			set[i] = false;
		}
	}

	translation_local2object( current, current, manip2object );

	m_transformOriginTranslatable.transformOriginTranslate( current, set );
}
};

class TransformOriginManipulator : public Manipulator, public ManipulatorSelectionChangeable
{
	struct RenderablePoint : public OpenGLRenderable
	{
		PointVertex m_point;
		RenderablePoint():
			m_point( vertex3f_identity ) {
		}
		void render( RenderStateFlags state ) const {
			glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_point.colour );
			glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_point.vertex );
			glDrawArrays( GL_POINTS, 0, 1 );
		}
		void setColour( const Colour4b & colour ) {
			m_point.colour = colour;
		}
	};

	TransformOriginTranslate m_translate;
	const bool& m_pivotIsCustom;
	RenderablePoint m_point;
	SelectableBool m_selectable;
	Pivot2World m_pivot;
public:
	static Shader* m_state;

	TransformOriginManipulator( TransformOriginTranslatable& transformOriginTranslatable, const bool& pivotIsCustom ) :
		m_translate( transformOriginTranslatable ),
		m_pivotIsCustom( pivotIsCustom ){
	}

	void UpdateColours() {
		m_point.setColour(
			m_selectable.isSelected()?
				m_pivotIsCustom? Colour4b( 255, 232, 0, 255 )
				: g_colour_selected
			:	m_pivotIsCustom? Colour4b( 0, 125, 255, 255 )
				: g_colour_screen );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) {
		m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

		// temp hack
		UpdateColours();

		renderer.SetState( m_state, Renderer::eWireframeOnly );
		renderer.SetState( m_state, Renderer::eFullMaterials );

		renderer.addRenderable( m_point, m_pivot.m_worldSpace );
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) {
		m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );

		SelectionPool selector;
		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_worldSpace ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif
			{
				SelectionIntersection best;

				Point_BestPoint( local2view, m_point.m_point, best );
				selector.addSelectable( best, &m_selectable );
			}
		}

		selectionChange( selector );
	}

	Manipulatable* GetManipulatable() {
		return &m_translate;
	}

	void setSelected( bool select ) {
		m_selectable.setSelected( select );
	}
	bool isSelected() const {
		return m_selectable.isSelected();
	}
};
Shader* TransformOriginManipulator::m_state;

class select_all : public scene::Graph::Walker
{
bool m_select;
public:
select_all( bool select )
	: m_select( select ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0 ) {
		selectable->setSelected( m_select );
	}
	return true;
}
};

class select_all_component : public scene::Graph::Walker
{
bool m_select;
SelectionSystem::EComponentMode m_mode;
public:
select_all_component( bool select, SelectionSystem::EComponentMode mode )
	: m_select( select ), m_mode( mode ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
	if ( componentSelectionTestable ) {
		componentSelectionTestable->setSelectedComponents( m_select, m_mode );
	}
	return true;
}
};

void Scene_SelectAll_Component( bool select, SelectionSystem::EComponentMode componentMode ){
	GlobalSceneGraph().traverse( select_all_component( select, componentMode ) );
}

void Scene_BoundsSelected( scene::Graph& graph, AABB& bounds );
class LazyBounds
{
	AABB m_bounds;
	bool m_valid;
public:
	LazyBounds() : m_valid( false ){
	}
	void setInvalid(){
		m_valid = false;
	}
	const AABB& getBounds(){
		if( !m_valid ){
			Scene_BoundsSelected( GlobalSceneGraph(), m_bounds );
			m_valid = true;
		}
		return m_bounds;
	}
};


// RadiantSelectionSystem
class RadiantSelectionSystem final :
	public SelectionSystem,
	public Translatable,
	public Rotatable,
	public Scalable,
	public Skewable,
	public TransformOriginTranslatable,
	public Renderable
{
mutable Matrix4 m_pivot2world;
mutable AABB m_bounds;
mutable LazyBounds m_lazy_bounds;
Matrix4 m_pivot2world_start;
Matrix4 m_manip2pivot_start;
Translation m_translation;
Rotation m_rotation;
Scale m_scale;
Skew m_skew;
public:
static Shader* m_state;
bool m_bPreferPointEntsIn2D;
private:
EManipulatorMode m_manipulator_mode;
Manipulator* m_manipulator;

// state
bool m_undo_begun;
EMode m_mode;
EComponentMode m_componentmode;

SelectionCounter m_count_primitive;
SelectionCounter m_count_component;
SelectedStuffCounter m_count_stuff;

TranslateManipulator m_translate_manipulator;
RotateManipulator m_rotate_manipulator;
ScaleManipulator m_scale_manipulator;
SkewManipulator m_skew_manipulator;
DragManipulator m_drag_manipulator;
ClipManipulator m_clip_manipulator;
BuildManipulator m_build_manipulator;
UVManipulator m_uv_manipulator;
mutable TransformOriginManipulator m_transformOrigin_manipulator;

typedef SelectionList<scene::Instance> selection_t;
selection_t m_selection;
selection_t m_component_selection;

Signal1<const Selectable&> m_selectionChanged_callbacks;

void ConstructPivot() const;
void ConstructPivotRotation() const;
void setCustomTransformOrigin( const Vector3& origin, const bool set[3] ) const;
AABB getSelectionAABB() const;
mutable bool m_pivotChanged;
bool m_pivot_moving;
mutable bool m_pivotIsCustom;

void Scene_TestSelect( Selector& selector, SelectionTest& test, const View& view, SelectionSystem::EMode mode, SelectionSystem::EComponentMode componentMode );

bool nothingSelected() const {
	return ( Mode() == eComponent && m_count_component.empty() )
		   || ( Mode() == ePrimitive && m_count_primitive.empty() );
}


public:
enum EModifier
{
	eManipulator,
	eReplace,
	eCycle,
	eSelect,
	eDeselect,
};

RadiantSelectionSystem() :
	m_bPreferPointEntsIn2D( true ),
	m_undo_begun( false ),
	m_mode( ePrimitive ),
	m_componentmode( eDefault ),
	m_count_primitive( SelectionChangedCaller( *this ) ),
	m_count_component( SelectionChangedCaller( *this ) ),
	m_translate_manipulator( *this, 2, 64 ),
	m_rotate_manipulator( *this, 8, 64 ),
	m_scale_manipulator( *this, 0, 64 ),
	m_skew_manipulator( *this, *this, *this, *this, m_bounds, m_pivot2world, m_pivotIsCustom ),
	m_drag_manipulator( *this ),
	m_clip_manipulator( m_pivot2world, m_bounds ),
	m_transformOrigin_manipulator( *this, m_pivotIsCustom ),
	m_pivotChanged( false ),
	m_pivot_moving( false ),
	m_pivotIsCustom( false ){
	SetManipulatorMode( eTranslate );
	pivotChanged();
	addSelectionChangeCallback( PivotChangedSelectionCaller( *this ) );
	AddGridChangeCallback( PivotChangedCaller( *this ) );
}
void pivotChanged() const {
	m_pivotChanged = true;
	m_lazy_bounds.setInvalid();
	SceneChangeNotify();
}
typedef ConstMemberCaller<RadiantSelectionSystem, &RadiantSelectionSystem::pivotChanged> PivotChangedCaller;
void pivotChangedSelection( const Selectable& selectable ){
	pivotChanged();
}
typedef MemberCaller1<RadiantSelectionSystem, const Selectable&, &RadiantSelectionSystem::pivotChangedSelection> PivotChangedSelectionCaller;

const AABB& getBoundsSelected() const {
	return m_lazy_bounds.getBounds();
}

void SetMode( EMode mode ){
	if ( m_mode != mode ) {
		m_mode = mode;
		pivotChanged();
	}
}
EMode Mode() const {
	return m_mode;
}
void SetComponentMode( EComponentMode mode ){
	m_componentmode = mode;
}
EComponentMode ComponentMode() const {
	return m_componentmode;
}
void SetManipulatorMode( EManipulatorMode mode ){
	if( ( mode == eClip ) || ( ManipulatorMode() == eClip ) ){
		m_clip_manipulator.reset();
		if( ( mode == eClip ) != ( ManipulatorMode() == eClip ) )
			Clipper_modeChanged( mode == eClip );
	}

	m_pivotIsCustom = false;
	m_manipulator_mode = mode;
	switch ( m_manipulator_mode )
	{
	case eTranslate: m_manipulator = &m_translate_manipulator; break;
	case eRotate: m_manipulator = &m_rotate_manipulator; break;
	case eScale: m_manipulator = &m_scale_manipulator; break;
	case eSkew: m_manipulator = &m_skew_manipulator; break;
	case eDrag: m_manipulator = &m_drag_manipulator; break;
	case eClip: m_manipulator = &m_clip_manipulator; m_repeatableTransforms.setIdentity(); break;
	case eBuild:
		{
			m_build_manipulator.initialise();
			m_manipulator = &m_build_manipulator; break;
		}
	case eUV: m_manipulator = &m_uv_manipulator; break;
	}
	pivotChanged();
}
EManipulatorMode ManipulatorMode() const {
	return m_manipulator_mode;
}

SelectionChangeCallback getObserver( EMode mode ){
	if ( mode == ePrimitive ) {
		return makeCallback1( m_count_primitive );
	}
	else
	{
		return makeCallback1( m_count_component );
	}
}
std::size_t countSelected() const {
	return m_count_primitive.size();
}
std::size_t countSelectedComponents() const {
	return m_count_component.size();
}
void countSelectedStuff( std::size_t& brushes, std::size_t& patches, std::size_t& entities ) const {
	m_count_stuff.get( brushes, patches, entities );
}
void onSelectedChanged( scene::Instance& instance, const Selectable& selectable ){
	if ( selectable.isSelected() ) {
		m_selection.append( instance );
		m_count_stuff.increment( instance.path().top() );
	}
	else
	{
		m_selection.erase( instance );
		m_count_stuff.decrement( instance.path().top() );
	}

	ASSERT_MESSAGE( m_selection.size() == m_count_primitive.size(), "selection-tracking error" );
}
void onComponentSelection( scene::Instance& instance, const Selectable& selectable ){
	if ( selectable.isSelected() ) {
		m_component_selection.append( instance );
	}
	else
	{
		m_component_selection.erase( instance );
	}

	ASSERT_MESSAGE( m_component_selection.size() == m_count_component.size(), "selection-tracking error" );
}
scene::Instance& firstSelected() const {
	ASSERT_MESSAGE( m_selection.size() > 0, "no instance selected" );
	return **m_selection.begin();
}
scene::Instance& ultimateSelected() const {
	ASSERT_MESSAGE( m_selection.size() > 0, "no instance selected" );
	return m_selection.back();
}
scene::Instance& penultimateSelected() const {
	ASSERT_MESSAGE( m_selection.size() > 1, "only one instance selected" );
	return *( *( --( --m_selection.end() ) ) );
}
void setSelectedAll( bool selected ){
	GlobalSceneGraph().traverse( select_all( selected ) );

	m_manipulator->setSelected( selected );
}
void setSelectedAllComponents( bool selected ){
	Scene_SelectAll_Component( selected, SelectionSystem::eVertex );
	Scene_SelectAll_Component( selected, SelectionSystem::eEdge );
	Scene_SelectAll_Component( selected, SelectionSystem::eFace );

	m_manipulator->setSelected( selected );
}

void foreachSelected( const Visitor& visitor ) const {
	selection_t::const_iterator i = m_selection.begin();
	while ( i != m_selection.end() )
	{
		visitor.visit( *( *( i++ ) ) );
	}
}
void foreachSelectedComponent( const Visitor& visitor ) const {
	selection_t::const_iterator i = m_component_selection.begin();
	while ( i != m_component_selection.end() )
	{
		visitor.visit( *( *( i++ ) ) );
	}
}

void addSelectionChangeCallback( const SelectionChangeHandler& handler ){
	m_selectionChanged_callbacks.connectLast( handler );
}
void selectionChanged( const Selectable& selectable ){
	m_selectionChanged_callbacks( selectable );
}
typedef MemberCaller1<RadiantSelectionSystem, const Selectable&, &RadiantSelectionSystem::selectionChanged> SelectionChangedCaller;


void startMove(){
	m_pivot2world_start = GetPivot2World();
}

bool SelectManipulator( const View& view, const float device_point[2], const float device_epsilon[2] ){
	bool movingOrigin = false;

	if ( !nothingSelected() || ManipulatorMode() == eDrag || ManipulatorMode() == eClip || ManipulatorMode() == eBuild || ManipulatorMode() == eUV ) {
#if defined ( DEBUG_SELECTION )
		g_render_clipped.destroy();
#endif
		Manipulatable::assign_static( view, device_point, device_epsilon ); //this b4 m_manipulator calls!

		m_transformOrigin_manipulator.setSelected( false );
		m_manipulator->setSelected( false );

		{
			View scissored( view );
			ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

			if( transformOrigin_isTranslatable() ){
				m_transformOrigin_manipulator.testSelect( scissored, GetPivot2World() );
				movingOrigin = m_transformOrigin_manipulator.isSelected();
			}

			if( !movingOrigin )
				m_manipulator->testSelect( scissored, GetPivot2World() );
		}

		startMove();

		m_pivot_moving = m_manipulator->isSelected();

		if ( m_pivot_moving || movingOrigin ) {
			Pivot2World pivot;
			pivot.update( GetPivot2World(), view.GetModelview(), view.GetProjection(), view.GetViewport() );

			m_manip2pivot_start = matrix4_multiplied_by_matrix4( matrix4_full_inverse( m_pivot2world_start ), pivot.m_worldSpace );

			Matrix4 device2manip;
			ConstructDevice2Manip( device2manip, m_pivot2world_start, view.GetModelview(), view.GetProjection(), view.GetViewport() );
			if( m_pivot_moving ){
				m_manipulator->GetManipulatable()->Construct( device2manip, device_point[0], device_point[1], m_bounds, vector4_to_vector3( GetPivot2World().t() ) );
				m_undo_begun = false;
			}
			else if( movingOrigin ){
				m_transformOrigin_manipulator.GetManipulatable()->Construct( device2manip, device_point[0], device_point[1], m_bounds, vector4_to_vector3( GetPivot2World().t() ) );
			}
		}

		SceneChangeNotify();
	}

	return m_pivot_moving || movingOrigin;
}

void HighlightManipulator( const View& view, const float device_point[2], const float device_epsilon[2] ){
	Manipulatable::assign_static( view, device_point, device_epsilon ); //this b4 m_manipulator calls!

	if ( ( !nothingSelected() && transformOrigin_isTranslatable() ) || ManipulatorMode() == eClip || ManipulatorMode() == eBuild || ManipulatorMode() == eUV ) {
#if defined ( DEBUG_SELECTION )
		g_render_clipped.destroy();
#endif

		m_transformOrigin_manipulator.setSelected( false );
		m_manipulator->setSelected( false );

		View scissored( view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

		if( transformOrigin_isTranslatable() ){
			m_transformOrigin_manipulator.testSelect( scissored, GetPivot2World() );

			if( !m_transformOrigin_manipulator.isSelected() )
				m_manipulator->testSelect( scissored, GetPivot2World() );
		}
		else if( ManipulatorMode() == eClip ){
			m_clip_manipulator.testSelect_points( scissored );
		}
		else if( ManipulatorMode() == eBuild ){
			m_build_manipulator.highlight( scissored );
		}
		else if( ManipulatorMode() == eUV ){
			m_manipulator->testSelect( scissored, GetPivot2World() );
		}
	}
}

void deselectAll(){
	if ( Mode() == eComponent ) {
		setSelectedAllComponents( false );
	}
	else
	{
		setSelectedAll( false );
	}
}

void deselectComponentsOrAll( bool components ){
	if ( components ) {
		setSelectedAllComponents( false );
	}
	else
	{
		deselectAll();
	}
}
#define SELECT_MATCHING
#define SELECT_MATCHING_DEPTH 1e-6f
#define SELECT_MATCHING_DIST 1e-6f
#define SELECT_MATCHING_COMPONENTS_DIST .25f
void SelectionPool_Select( SelectionPool& pool, bool select, float dist_epsilon ){
	SelectionPool::iterator best = pool.begin();
	if( ( *best ).second->isSelected() != select ){
		( *best ).second->setSelected( select );
	}
#ifdef SELECT_MATCHING
	SelectionPool::iterator i = best;
	++i;
	while ( i != pool.end() )
	{
		if( ( *i ).first.equalEpsilon( ( *best ).first, dist_epsilon, SELECT_MATCHING_DEPTH ) ){
			//if( ( *i ).second->isSelected() != select ){
				( *i ).second->setSelected( select );
			//}
		}
		else{
			break;
		}
		++i;
	}
#endif // SELECT_MATCHING
}

void SelectPoint( const View& view, const float device_point[2], const float device_epsilon[2], RadiantSelectionSystem::EModifier modifier, bool face ){
	//globalOutputStream() << device_point[0] << "   " << device_point[1] << "\n";
	ASSERT_MESSAGE( fabs( device_point[0] ) <= 1.f && fabs( device_point[1] ) <= 1.f, "point-selection error" );

	if ( modifier == eReplace ) {
		deselectComponentsOrAll( face );
	}
/*
//nothingSelected() doesn't consider faces, selected in non-component mode, m
	if ( modifier == eCycle && nothingSelected() ){
		modifier = eReplace;
	}
*/
  #if defined ( DEBUG_SELECTION )
	g_render_clipped.destroy();
  #endif

	{
		View scissored( view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

		SelectionVolume volume( scissored );
		SelectionPool selector;
		SelectionPool selector_point_ents;
		const bool prefer_point_ents = m_bPreferPointEntsIn2D && Mode() == ePrimitive && !view.fill() && !face
			&& ( modifier == RadiantSelectionSystem::eReplace || modifier == RadiantSelectionSystem::eSelect || modifier == RadiantSelectionSystem::eDeselect );

		if( prefer_point_ents ){
			Scene_TestSelect( selector_point_ents, volume, scissored, eEntity, ComponentMode() );
		}
		if( prefer_point_ents && !selector_point_ents.failed() ){
			switch ( modifier )
			{
			// if cycle mode not enabled, enable it
			case RadiantSelectionSystem::eReplace:
			{
				// select closest
				( *selector_point_ents.begin() ).second->setSelected( true );
			}
			break;
			case RadiantSelectionSystem::eSelect:
			{
				SelectionPool_Select( selector_point_ents, true, SELECT_MATCHING_DIST );
			}
			break;
			case RadiantSelectionSystem::eDeselect:
			{
				SelectionPool_Select( selector_point_ents, false, SELECT_MATCHING_DIST );
			}
			break;
			default:
				break;
			}
		}
		else{
			if ( face ){
				Scene_TestSelect_Component( selector, volume, scissored, eFace );
			}
			else{
				Scene_TestSelect( selector, volume, scissored, g_bAltResize_AltSelect ? ePrimitive : Mode(), ComponentMode() );
			}

			if ( !selector.failed() ) {
				switch ( modifier )
				{
				// if cycle mode not enabled, enable it
				case RadiantSelectionSystem::eReplace:
				{
					// select closest
					( *selector.begin() ).second->setSelected( true );
				}
				break;
				// select the next object in the list from the one already selected
				case RadiantSelectionSystem::eCycle:
				{
					bool cycleSelectionOccured = false;
					SelectionPool::iterator i = selector.begin();
					while ( i != selector.end() )
					{
						if ( ( *i ).second->isSelected() ) {
							deselectComponentsOrAll( face );
							++i;
							if ( i != selector.end() ) {
								i->second->setSelected( true );
							}
							else
							{
								selector.begin()->second->setSelected( true );
							}
							cycleSelectionOccured = true;
							break;
						}
						++i;
					}
					if( !cycleSelectionOccured ){
						deselectComponentsOrAll( face );
						( *selector.begin() ).second->setSelected( true );
					}
				}
				break;
				case RadiantSelectionSystem::eSelect:
				{
					SelectionPool_Select( selector, true, ( Mode() == eComponent && !g_bAltResize_AltSelect )? SELECT_MATCHING_COMPONENTS_DIST : SELECT_MATCHING_DIST );
				}
				break;
				case RadiantSelectionSystem::eDeselect:
				{
					SelectionPool_Select( selector, false, ( Mode() == eComponent && !g_bAltResize_AltSelect )? SELECT_MATCHING_COMPONENTS_DIST : SELECT_MATCHING_DIST );
				}
				break;
				default:
					break;
				}
			}
			else if( modifier == eCycle ){
				deselectComponentsOrAll( face );
			}
		}
	}
}

bool SelectPoint_InitPaint( const View& view, const float device_point[2], const float device_epsilon[2], bool face ){
	ASSERT_MESSAGE( fabs( device_point[0] ) <= 1.f && fabs( device_point[1] ) <= 1.f, "point-selection error" );
  #if defined ( DEBUG_SELECTION )
	g_render_clipped.destroy();
  #endif

	{
		View scissored( view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

		SelectionVolume volume( scissored );
		SelectionPool selector;
		SelectionPool selector_point_ents;
		const bool prefer_point_ents = m_bPreferPointEntsIn2D && Mode() == ePrimitive && !view.fill() && !face;

		if( prefer_point_ents ){
			Scene_TestSelect( selector_point_ents, volume, scissored, eEntity, ComponentMode() );
		}
		if( prefer_point_ents && !selector_point_ents.failed() ){
			const bool wasSelected = ( *selector_point_ents.begin() ).second->isSelected();
			SelectionPool_Select( selector_point_ents, !wasSelected, SELECT_MATCHING_DIST );
			return !wasSelected;
		}
		else{//do primitives, if ents failed
			if ( face ){
				Scene_TestSelect_Component( selector, volume, scissored, eFace );
			}
			else{
				Scene_TestSelect( selector, volume, scissored, g_bAltResize_AltSelect ? ePrimitive : Mode(), ComponentMode() );
			}
			if ( !selector.failed() ){
				const bool wasSelected = ( *selector.begin() ).second->isSelected();
				SelectionPool_Select( selector, !wasSelected, ( Mode() == eComponent && !g_bAltResize_AltSelect )? SELECT_MATCHING_COMPONENTS_DIST : SELECT_MATCHING_DIST );

				#if 0
				SelectionPool::iterator best = selector.begin();
				SelectionPool::iterator i = best;
				globalOutputStream() << "\n\n\n===========\n";
				while ( i != selector.end() )
				{
					globalOutputStream() << "depth:" << ( *i ).first.m_depth << " dist:" << ( *i ).first.m_distance << " depth2:" << ( *i ).first.m_depth2 << "\n";
					globalOutputStream() << "depth - best depth:" << ( *i ).first.m_depth - ( *best ).first.m_depth << "\n";
					++i;
				}
				#endif

				return !wasSelected;
			}
			else{
				return true;
			}
		}
	}
}

void SelectArea( const View& view, const rect_t rect, bool face ){
  #if defined ( DEBUG_SELECTION )
	g_render_clipped.destroy();
  #endif
	View scissored( view );
	ConstructSelectionTest( scissored, rect );

	SelectionVolume volume( scissored );
	SelectionPool pool;
	if ( face ) {
		Scene_TestSelect_Component( pool, volume, scissored, eFace );
	}
	else
	{
		Scene_TestSelect( pool, volume, scissored, Mode(), ComponentMode() );
	}

	for ( SelectionPool::iterator i = pool.begin(); i != pool.end(); ++i )
	{
		( *i ).second->setSelected( rect.modifier == rect_t::eSelect? true : rect.modifier == rect_t::eDeselect? false : !( *i ).second->isSelected() );
	}
}


void translate( const Vector3& translation ){
	if ( !nothingSelected() ) {
		//ASSERT_MESSAGE(!m_pivotChanged, "pivot is invalid");

		m_translation = translation;
		m_repeatableTransforms.m_translation = translation;

		m_pivot2world = m_pivot2world_start;
		matrix4_translate_by_vec3( m_pivot2world, translation );

		if ( Mode() == eComponent ) {
			Scene_Translate_Component_Selected( GlobalSceneGraph(), m_translation );
		}
		else
		{
			Scene_Translate_Selected( GlobalSceneGraph(), m_translation );
		}

		SceneChangeNotify();
	}
}
void outputTranslation( TextOutputStream& ostream ){
	ostream << " -xyz " << m_translation.x() << " " << m_translation.y() << " " << m_translation.z();
}
void rotate( const Quaternion& rotation ){
	if ( !nothingSelected() ) {
		//ASSERT_MESSAGE(!m_pivotChanged, "pivot is invalid");

		m_rotation = rotation;
		m_repeatableTransforms.m_rotation = rotation;
		if( ( m_repeatableTransforms.m_rotationOriginSet = m_pivotIsCustom ) )
			m_repeatableTransforms.m_rotationOrigin = vector4_to_vector3( m_pivot2world.t() );

		if ( Mode() == eComponent ) {
			Scene_Rotate_Component_Selected( GlobalSceneGraph(), m_rotation, vector4_to_vector3( m_pivot2world.t() ) );

			matrix4_assign_rotation_for_pivot( m_pivot2world, m_component_selection.back() );
		}
		else
		{
			Scene_Rotate_Selected( GlobalSceneGraph(), m_rotation, vector4_to_vector3( m_pivot2world.t() ) );

			matrix4_assign_rotation_for_pivot( m_pivot2world, m_selection.back() );
		}
#ifdef SELECTIONSYSTEM_AXIAL_PIVOTS
		matrix4_assign_rotation( m_pivot2world, matrix4_rotation_for_quaternion_quantised( m_rotation ) );
#endif

		SceneChangeNotify();
	}
}
void outputRotation( TextOutputStream& ostream ){
	ostream << " -eulerXYZ " << m_rotation.x() << " " << m_rotation.y() << " " << m_rotation.z();
}
void scale( const Vector3& scaling ){
	if ( !nothingSelected() ) {
		m_scale = scaling;
		m_repeatableTransforms.m_scale = scaling;
		if( ( m_repeatableTransforms.m_scaleOriginSet = m_pivotIsCustom ) )
			m_repeatableTransforms.m_scaleOrigin = vector4_to_vector3( m_pivot2world.t() );

		if ( Mode() == eComponent ) {
			Scene_Scale_Component_Selected( GlobalSceneGraph(), m_scale, vector4_to_vector3( m_pivot2world.t() ) );
		}
		else
		{
			Scene_Scale_Selected( GlobalSceneGraph(), m_scale, vector4_to_vector3( m_pivot2world.t() ) );
		}

		if( ManipulatorMode() == eSkew ){
			m_pivot2world[0] = scaling[0];
			m_pivot2world[5] = scaling[1];
			m_pivot2world[10] = scaling[2];
		}

		SceneChangeNotify();
	}
}
void outputScale( TextOutputStream& ostream ){
	ostream << " -scale " << m_scale.x() << " " << m_scale.y() << " " << m_scale.z();
}

void skew( const Skew& skew ){
	if ( !nothingSelected() ) {
		m_skew = skew;
		m_repeatableTransforms.m_skew = skew;
		if( ( m_repeatableTransforms.m_skewOriginSet = m_pivotIsCustom ) )
			m_repeatableTransforms.m_skewOrigin = vector4_to_vector3( m_pivot2world.t() );

		if ( Mode() == eComponent ) {
			Scene_Skew_Component_Selected( GlobalSceneGraph(), m_skew, vector4_to_vector3( m_pivot2world.t() ) );
		}
		else
		{
			Scene_Skew_Selected( GlobalSceneGraph(), m_skew, vector4_to_vector3( m_pivot2world.t() ) );
		}
		m_pivot2world[skew.index] = skew.amount;
		SceneChangeNotify();
	}
}

void rotateSelected( const Quaternion& rotation, bool snapOrigin = false ){
	if( snapOrigin && !m_pivotIsCustom )
		vector3_snap( vector4_to_vector3( m_pivot2world.t() ), GetSnapGridSize() );
	startMove();
	rotate( rotation );
	freezeTransforms();
}
void translateSelected( const Vector3& translation ){
	startMove();
	translate( translation );
	freezeTransforms();
}
void scaleSelected( const Vector3& scaling, bool snapOrigin = false ){
	if( snapOrigin && !m_pivotIsCustom )
		vector3_snap( vector4_to_vector3( m_pivot2world.t() ), GetSnapGridSize() );
	startMove();
	scale( scaling );
	freezeTransforms();
}

RepeatableTransforms m_repeatableTransforms;

void repeatTransforms( const Callback& clone ){
	if ( countSelected() != 0 && !m_repeatableTransforms.isIdentity() ) {
		startMove();
		UndoableCommand undo( "repeatTransforms" );
		if( Mode() == ePrimitive )
			clone();
		if ( Mode() == eComponent ) {
			GlobalSelectionSystem().foreachSelectedComponent( transform_component_selected( m_repeatableTransforms, vector4_to_vector3( m_pivot2world.t() ) ) );
		}
		else
		{
			GlobalSelectionSystem().foreachSelected( transform_selected( m_repeatableTransforms, vector4_to_vector3( m_pivot2world.t() ) ) );
		}
		freezeTransforms();
	}
}

bool transformOrigin_isTranslatable() const{
	return ManipulatorMode() == eScale
		|| ManipulatorMode() == eSkew
		|| ManipulatorMode() == eRotate
		|| ManipulatorMode() == eTranslate;
}

void transformOriginTranslate( const Vector3& translation, const bool set[3] ){
	m_pivot2world = m_pivot2world_start;
	setCustomTransformOrigin( translation + vector4_to_vector3( m_pivot2world_start.t() ), set );
	SceneChangeNotify();
}

void MoveSelected( const View& view, const float device_point[2], bool snap, bool snapbbox, bool alt ){
	if ( m_manipulator->isSelected() ) {
		if ( !m_undo_begun ) {
			m_undo_begun = true;
			GlobalUndoSystem().start();
		}

		Matrix4 device2manip;
		ConstructDevice2Manip( device2manip, m_pivot2world_start, view.GetModelview(), view.GetProjection(), view.GetViewport() );
		m_manipulator->GetManipulatable()->Transform( m_manip2pivot_start, device2manip, device_point[0], device_point[1], snap, snapbbox, alt );
	}
	else if( m_transformOrigin_manipulator.isSelected() ){
		Matrix4 device2manip;
		ConstructDevice2Manip( device2manip, m_pivot2world_start, view.GetModelview(), view.GetProjection(), view.GetViewport() );
		m_transformOrigin_manipulator.GetManipulatable()->Transform( m_manip2pivot_start, device2manip, device_point[0], device_point[1], snap, snapbbox, alt );
	}
}

/// \todo Support view-dependent nudge.
void NudgeManipulator( const Vector3& nudge, const Vector3& view ){
//	if ( ManipulatorMode() == eTranslate || ManipulatorMode() == eDrag ) {
		translateSelected( nudge );
//	}
}

bool endMove();
void freezeTransforms();

void renderSolid( Renderer& renderer, const VolumeTest& volume ) const;
void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
	renderSolid( renderer, volume );
}

const Matrix4& GetPivot2World() const {
	ConstructPivot();
	return m_pivot2world;
}

static void constructStatic(){
  #if defined( DEBUG_SELECTION )
	g_state_clipped = GlobalShaderCache().capture( "$DEBUG_CLIPPED" );
  #endif
	m_state = GlobalShaderCache().capture( "$POINT" );
	TranslateManipulator::m_state_wire =
	RotateManipulator::m_state_outer =
	SkewManipulator::m_state_wire =
	BuildManipulator::m_state_line = GlobalShaderCache().capture( "$WIRE_OVERLAY" );
	TranslateManipulator::m_state_fill =
	SkewManipulator::m_state_fill = GlobalShaderCache().capture( "$FLATSHADE_OVERLAY" );
	TransformOriginManipulator::m_state =
	ClipManipulator::m_state =
	SkewManipulator::m_state_point =
	BuildManipulator::m_state_point =
	UVManipulator::m_state_point = GlobalShaderCache().capture( "$BIGPOINT" );
	RenderablePivot::StaticShader::instance() = GlobalShaderCache().capture( "$PIVOT" );
	UVManipulator::m_state_line = GlobalShaderCache().capture( "$BLENDLINE" );
}

static void destroyStatic(){
  #if defined( DEBUG_SELECTION )
	GlobalShaderCache().release( "$DEBUG_CLIPPED" );
  #endif
	GlobalShaderCache().release( "$BLENDLINE" );
	GlobalShaderCache().release( "$PIVOT" );
	GlobalShaderCache().release( "$BIGPOINT" );
	GlobalShaderCache().release( "$FLATSHADE_OVERLAY" );
	GlobalShaderCache().release( "$WIRE_OVERLAY" );
	GlobalShaderCache().release( "$POINT" );
}
};

Shader* RadiantSelectionSystem::m_state = 0;


namespace
{
RadiantSelectionSystem* g_RadiantSelectionSystem;

inline RadiantSelectionSystem& getSelectionSystem(){
	return *g_RadiantSelectionSystem;
}
}

#include "map.h"

class testselect_entity_visible : public scene::Graph::Walker
{
Selector& m_selector;
SelectionTest& m_test;
public:
testselect_entity_visible( Selector& selector, SelectionTest& test )
	: m_selector( selector ), m_test( test ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if( path.top().get_pointer() == Map_GetWorldspawn( g_map ) ||
		node_is_group( path.top().get() ) ){
		return false;
	}
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0
		 && Node_isEntity( path.top() ) ) {
		m_selector.pushSelectable( *selectable );
	}

	SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance );
	if ( selectionTestable ) {
		selectionTestable->testSelect( m_selector, m_test );
	}

	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0
		 && Node_isEntity( path.top() ) ) {
		m_selector.popSelectable();
	}
}
};

class testselect_primitive_visible : public scene::Graph::Walker
{
Selector& m_selector;
SelectionTest& m_test;
public:
testselect_primitive_visible( Selector& selector, SelectionTest& test )
	: m_selector( selector ), m_test( test ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0 ) {
		m_selector.pushSelectable( *selectable );
	}

	SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance );
	if ( selectionTestable ) {
		selectionTestable->testSelect( m_selector, m_test );
	}

	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0 ) {
		m_selector.popSelectable();
	}
}
};

class testselect_component_visible : public scene::Graph::Walker
{
Selector& m_selector;
SelectionTest& m_test;
SelectionSystem::EComponentMode m_mode;
public:
testselect_component_visible( Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode )
	: m_selector( selector ), m_test( test ), m_mode( mode ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
	if ( componentSelectionTestable ) {
		componentSelectionTestable->testSelectComponents( m_selector, m_test, m_mode );
	}

	return true;
}
};


class testselect_component_visible_selected : public scene::Graph::Walker
{
Selector& m_selector;
SelectionTest& m_test;
SelectionSystem::EComponentMode m_mode;
public:
testselect_component_visible_selected( Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode )
	: m_selector( selector ), m_test( test ), m_mode( mode ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0 && selectable->isSelected() ) {
		ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
		if ( componentSelectionTestable ) {
			componentSelectionTestable->testSelectComponents( m_selector, m_test, m_mode );
		}
	}

	return true;
}
};

void Scene_TestSelect_Primitive( Selector& selector, SelectionTest& test, const VolumeTest& volume ){
	Scene_forEachVisible( GlobalSceneGraph(), volume, testselect_primitive_visible( selector, test ) );
}

void Scene_TestSelect_Component_Selected( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode ){
	Scene_forEachVisible( GlobalSceneGraph(), volume, testselect_component_visible_selected( selector, test, componentMode ) );
}

void Scene_TestSelect_Component( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode ){
	Scene_forEachVisible( GlobalSceneGraph(), volume, testselect_component_visible( selector, test, componentMode ) );
}

void RadiantSelectionSystem::Scene_TestSelect( Selector& selector, SelectionTest& test, const View& view, SelectionSystem::EMode mode, SelectionSystem::EComponentMode componentMode ){
	switch ( mode )
	{
	case eEntity:
		Scene_forEachVisible( GlobalSceneGraph(), view, testselect_entity_visible( selector, test ) );
		break;
	case ePrimitive:
		Scene_TestSelect_Primitive( selector, test, view );
		break;
	case eComponent:
		Scene_TestSelect_Component_Selected( selector, test, view, componentMode );
		break;
	}
}


void Scene_Intersect( const View& view, const float device_point[2], const float device_epsilon[2], Vector3& intersection ){
	View scissored( view );
	ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );
	SelectionVolume test( scissored );

	BestPointSelector bestPointSelector;
	Scene_TestSelect_Primitive( bestPointSelector, test, scissored );

	test.BeginMesh( g_matrix4_identity, true );
	if( bestPointSelector.isSelected() ){
		intersection = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, bestPointSelector.best().depth(), 1 ) ) );
	}
	else{
		const Vector3 near = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, -1, 1 ) ) );
		const Vector3 far = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, 1, 1 ) ) );
		intersection = vector3_normalised( far - near ) * 256.f + near;
	}
}

class FreezeTransforms : public scene::Graph::Walker
{
public:
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	TransformNode* transformNode = Node_getTransformNode( path.top() );
	if ( transformNode != 0 ) {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->freezeTransform();
		}
	}
	return true;
}
};

void RadiantSelectionSystem::freezeTransforms(){
	GlobalSceneGraph().traverse( FreezeTransforms() );
}


bool RadiantSelectionSystem::endMove(){
	if( m_transformOrigin_manipulator.isSelected() ){
		if( m_pivot2world == m_pivot2world_start ){
			m_pivotIsCustom = !m_pivotIsCustom;
			pivotChanged();
		}
		return true;
	}

	if ( ManipulatorMode() == eUV )
		m_uv_manipulator.freezeTransform();
	else
		freezeTransforms();

//	if ( Mode() == ePrimitive && ManipulatorMode() == eDrag ) {
//		g_bTmpComponentMode = false;
//		Scene_SelectAll_Component( false, g_bAltResize_AltSelect? SelectionSystem::eVertex : SelectionSystem::eFace );
//	}
	if( g_bTmpComponentMode ){
		g_bTmpComponentMode = false;
		setSelectedAllComponents( false );
	}

	m_pivot_moving = false;
	pivotChanged();

	SceneChangeNotify();

	if ( m_undo_begun ) {
		StringOutputStream command;

		if ( ManipulatorMode() == eTranslate ) {
			command << "translateTool";
			outputTranslation( command );
		}
		else if ( ManipulatorMode() == eRotate ) {
			command << "rotateTool";
			outputRotation( command );
		}
		else if ( ManipulatorMode() == eScale ) {
			command << "scaleTool";
			outputScale( command );
		}
		else if ( ManipulatorMode() == eSkew ) {
			command << "transformTool";
//			outputScale( command );
		}
		else if ( ManipulatorMode() == eDrag ) {
			command << "dragTool";
		}
		else if ( ManipulatorMode() == eUV ) {
			command << "UVTool";
		}

		GlobalUndoSystem().finish( command.c_str() );
	}
	return false;
}

inline AABB Instance_getPivotBounds( scene::Instance& instance ){
	Entity* entity = Node_getEntity( instance.path().top() );
	if ( entity != 0
		 && ( entity->getEntityClass().fixedsize
			  || !node_is_group( instance.path().top() ) ) ) {
		Editable* editable = Node_getEditable( instance.path().top() );
		if ( editable != 0 ) {
			return AABB( vector4_to_vector3( matrix4_multiplied_by_matrix4( instance.localToWorld(), editable->getLocalPivot() ).t() ), Vector3( 0, 0, 0 ) );
		}
		else
		{
			return AABB( vector4_to_vector3( instance.localToWorld().t() ), Vector3( 0, 0, 0 ) );
		}
	}

	return instance.worldAABB();
}

class bounds_selected : public scene::Graph::Walker
{
AABB& m_bounds;
public:
bounds_selected( AABB& bounds )
	: m_bounds( bounds ){
	m_bounds = AABB();
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0
		 && selectable->isSelected() ) {
		aabb_extend_by_aabb_safe( m_bounds, Instance_getPivotBounds( instance ) );
	}
	return true;
}
};

class bounds_selected_component : public scene::Graph::Walker
{
AABB& m_bounds;
public:
bounds_selected_component( AABB& bounds )
	: m_bounds( bounds ){
	m_bounds = AABB();
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable != 0
		 && selectable->isSelected() ) {
		ComponentEditable* componentEditable = Instance_getComponentEditable( instance );
		if ( componentEditable ) {
			aabb_extend_by_aabb_safe( m_bounds, aabb_for_oriented_aabb_safe( componentEditable->getSelectedComponentsBounds(), instance.localToWorld() ) );
		}
	}
	return true;
}
};

void Scene_BoundsSelected( scene::Graph& graph, AABB& bounds ){
	graph.traverse( bounds_selected( bounds ) );
}

void Scene_BoundsSelectedComponent( scene::Graph& graph, AABB& bounds ){
	graph.traverse( bounds_selected_component( bounds ) );
}

#if 0
inline void pivot_for_node( Matrix4& pivot, scene::Node& node, scene::Instance& instance ){
	ComponentEditable* componentEditable = Instance_getComponentEditable( instance );
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent
		 && componentEditable != 0 ) {
		pivot = matrix4_translation_for_vec3( componentEditable->getSelectedComponentsBounds().origin );
	}
	else
	{
		Bounded* bounded = Instance_getBounded( instance );
		if ( bounded != 0 ) {
			pivot = matrix4_translation_for_vec3( bounded->localAABB().origin );
		}
		else
		{
			pivot = g_matrix4_identity;
		}
	}
}
#endif

void RadiantSelectionSystem::ConstructPivotRotation() const {
	switch ( m_manipulator_mode )
	{
	case eTranslate:
		break;
	case eRotate:
		if ( Mode() == eComponent ) {
			matrix4_assign_rotation_for_pivot( m_pivot2world, m_component_selection.back() );
		}
		else
		{
			matrix4_assign_rotation_for_pivot( m_pivot2world, m_selection.back() );
		}
		break;
	case eScale:
		if ( Mode() == eComponent ) {
			matrix4_assign_rotation_for_pivot( m_pivot2world, m_component_selection.back() );
		}
		else
		{
			matrix4_assign_rotation_for_pivot( m_pivot2world, m_selection.back() );
		}
		break;
	default:
		break;
	}
}

void RadiantSelectionSystem::ConstructPivot() const {
	if ( !m_pivotChanged || m_pivot_moving ) {
		return;
	}
	m_pivotChanged = false;

	if ( !nothingSelected() ) {
		m_bounds = getSelectionAABB();
		if( !m_pivotIsCustom ){
			Vector3 object_pivot = m_bounds.origin;

			//vector3_snap( object_pivot, GetSnapGridSize() );
			//globalOutputStream() << object_pivot << "\n";
			m_pivot2world = matrix4_translation_for_vec3( object_pivot );
		}
		else{
//			m_pivot2world = matrix4_translation_for_vec3( vector4_to_vector3( m_pivot2world.t() ) );
			matrix4_assign_rotation( m_pivot2world, g_matrix4_identity );
		}

		ConstructPivotRotation();
	}
}

void RadiantSelectionSystem::setCustomTransformOrigin( const Vector3& origin, const bool set[3] ) const {
	if ( !nothingSelected() && transformOrigin_isTranslatable() ) {

		//globalOutputStream() << origin << "\n";
		for( std::size_t i = 0; i < 3; i++ ){
			float value = origin[i];
			if( set[i] ){
				float bestsnapDist = fabs( m_bounds.origin[i] - value );
				float bestsnapTo = m_bounds.origin[i];
				float othersnapDist = fabs( m_bounds.origin[i] + m_bounds.extents[i] - value );
				if( othersnapDist < bestsnapDist ){
					bestsnapDist = othersnapDist;
					bestsnapTo = m_bounds.origin[i] + m_bounds.extents[i];
				}
				othersnapDist = fabs( m_bounds.origin[i] - m_bounds.extents[i] - value );
				if( othersnapDist < bestsnapDist ){
					bestsnapDist = othersnapDist;
					bestsnapTo = m_bounds.origin[i] - m_bounds.extents[i];
				}
				othersnapDist = fabs( float_snapped( value, GetSnapGridSize() ) - value );
				if( othersnapDist < bestsnapDist ){
					bestsnapDist = othersnapDist;
					bestsnapTo = float_snapped( value, GetSnapGridSize() );
				}
				value = bestsnapTo;

				m_pivot2world[i + 12] = value; //m_pivot2world.tx() .ty() .tz()
			}
		}
		m_pivotIsCustom = true;

		ConstructPivotRotation();
	}
}

AABB RadiantSelectionSystem::getSelectionAABB() const {
	AABB bounds;
	if ( !nothingSelected() ) {
		if ( Mode() == eComponent || g_bTmpComponentMode ) {
			Scene_BoundsSelectedComponent( GlobalSceneGraph(), bounds );
			if( !aabb_valid( bounds ) ) /* selecting PlaneSelectables sets g_bTmpComponentMode, but only brushes return correct componentEditable->getSelectedComponentsBounds() */
				bounds = getBoundsSelected();
		}
		else
		{
			bounds = getBoundsSelected();
		}
	}
	return bounds;
}

void RadiantSelectionSystem::renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
	//if(view->TestPoint(m_object_pivot))
	if ( !nothingSelected() || ManipulatorMode() == eClip || ManipulatorMode() == eBuild || ManipulatorMode() == eUV ) {
		renderer.Highlight( Renderer::ePrimitive, false );
		renderer.Highlight( Renderer::eFace, false );

		renderer.SetState( m_state, Renderer::eWireframeOnly );
		renderer.SetState( m_state, Renderer::eFullMaterials );

		if( transformOrigin_isTranslatable() )
			m_transformOrigin_manipulator.render( renderer, volume, GetPivot2World() );

		m_manipulator->render( renderer, volume, GetPivot2World() );
	}

#if defined( DEBUG_SELECTION )
	renderer.SetState( g_state_clipped, Renderer::eWireframeOnly );
	renderer.SetState( g_state_clipped, Renderer::eFullMaterials );
	renderer.addRenderable( g_render_clipped, g_render_clipped.m_world );
#endif
}

#include "preferencesystem.h"
#include "preferences.h"

bool g_bLeftMouseClickSelector = true;

void SelectionSystem_constructPreferences( PreferencesPage& page ){
	page.appendSpinner( "Selector size (pixels)", g_SELECT_EPSILON, 8, 2, 64 );
	page.appendCheckBox( "", "Prefer point entities in 2D", getSelectionSystem().m_bPreferPointEntsIn2D );
	page.appendCheckBox( "", "Left mouse click tunnel selector", g_bLeftMouseClickSelector );
	{
		const char* styles[] = { "XY plane + Z with Alt", "View plane + Forward with Alt", };
		page.appendCombo(
			"Move style in 3D",
			STRING_ARRAY_RANGE( styles ),
			IntImportCaller( TranslateFreeXY_Z::m_viewdependent ),
			IntExportCaller( TranslateFreeXY_Z::m_viewdependent )
			);
	}
}
void SelectionSystem_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Selection", "Selection System Settings" ) );
	SelectionSystem_constructPreferences( page );
}
void SelectionSystem_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, SelectionSystem_constructPage>() );
}



void SelectionSystem_OnBoundsChanged(){
	getSelectionSystem().pivotChanged();
}

SignalHandlerId SelectionSystem_boundsChanged;

void SelectionSystem_Construct(){
	RadiantSelectionSystem::constructStatic();

	g_RadiantSelectionSystem = new RadiantSelectionSystem;

	SelectionSystem_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback( FreeCaller<SelectionSystem_OnBoundsChanged>() );

	GlobalShaderCache().attachRenderable( getSelectionSystem() );

	GlobalPreferenceSystem().registerPreference( "SELECT_EPSILON", IntImportStringCaller( g_SELECT_EPSILON ), IntExportStringCaller( g_SELECT_EPSILON ) );
	GlobalPreferenceSystem().registerPreference( "PreferPointEntsIn2D", BoolImportStringCaller( getSelectionSystem().m_bPreferPointEntsIn2D ), BoolExportStringCaller( getSelectionSystem().m_bPreferPointEntsIn2D ) );
	GlobalPreferenceSystem().registerPreference( "LeftMouseClickSelector", BoolImportStringCaller( g_bLeftMouseClickSelector ), BoolExportStringCaller( g_bLeftMouseClickSelector ) );
	GlobalPreferenceSystem().registerPreference( "3DMoveStyle", IntImportStringCaller( TranslateFreeXY_Z::m_viewdependent ), IntExportStringCaller( TranslateFreeXY_Z::m_viewdependent ) );
	SelectionSystem_registerPreferencesPage();
}

void SelectionSystem_Destroy(){
	GlobalShaderCache().detachRenderable( getSelectionSystem() );

	GlobalSceneGraph().removeBoundsChangedCallback( SelectionSystem_boundsChanged );

	delete g_RadiantSelectionSystem;

	RadiantSelectionSystem::destroyStatic();
}




inline float screen_normalised( float pos, std::size_t size ){
	return ( ( 2.0f * pos ) / size ) - 1.0f;
}

typedef Vector2 DeviceVector;

inline DeviceVector window_to_normalised_device( WindowVector window, std::size_t width, std::size_t height ){
	return DeviceVector( screen_normalised( window.x(), width ), screen_normalised( height - 1 - window.y(), height ) );
}

inline float device_constrained( float pos ){
	return std::min( 1.0f, std::max( -1.0f, pos ) );
}

inline DeviceVector device_constrained( DeviceVector device ){
	return DeviceVector( device_constrained( device.x() ), device_constrained( device.y() ) );
}

inline float window_constrained( float pos, std::size_t origin, std::size_t size ){
	return std::min( static_cast<float>( origin + size ), std::max( static_cast<float>( origin ), pos ) );
}

inline WindowVector window_constrained( WindowVector window, std::size_t x, std::size_t y, std::size_t width, std::size_t height ){
	return WindowVector( window_constrained( window.x(), x, width ), window_constrained( window.y(), y, height ) );
}

typedef Callback1<DeviceVector> MouseEventCallback;

Single<MouseEventCallback> g_mouseMovedCallback;
Single<MouseEventCallback> g_mouseUpCallback;

#if 1
const ButtonIdentifier c_button_select = c_buttonLeft;
const ButtonIdentifier c_button_select2 = c_buttonRight;
const ModifierFlags c_modifier_manipulator = c_modifierNone;
const ModifierFlags c_modifier_toggle = c_modifierShift;
const ModifierFlags c_modifier_replace = c_modifierShift | c_modifierAlt;
const ModifierFlags c_modifier_face = c_modifierControl;
#else
const ButtonIdentifier c_button_select = c_buttonLeft;
const ModifierFlags c_modifier_manipulator = c_modifierNone;
const ModifierFlags c_modifier_toggle = c_modifierControl;
const ModifierFlags c_modifier_replace = c_modifierNone;
const ModifierFlags c_modifier_face = c_modifierShift;
#endif
const ModifierFlags c_modifier_toggle_face = c_modifier_toggle | c_modifier_face;
const ModifierFlags c_modifier_replace_face = c_modifier_replace | c_modifier_face;

const ButtonIdentifier c_button_texture = c_buttonMiddle;
const ModifierFlags c_modifier_apply_texture1_project = c_modifierControl | c_modifierShift;
const ModifierFlags c_modifier_apply_texture2_seamless = c_modifierControl;
const ModifierFlags c_modifier_apply_texture3 =                     c_modifierShift;
const ModifierFlags c_modifier_copy_texture = c_modifierNone;



void Scene_copyClosestTexture( SelectionTest& test );
void Scene_applyClosestTexture( SelectionTest& test, bool shift, bool ctrl, bool alt, bool texturize_selected = false );
const char* Scene_applyClosestTexture_getUndoName( bool shift, bool ctrl, bool alt );

class TexManipulator_
{
const DeviceVector& m_epsilon;
const ModifierFlags& m_state;
public:
const View* m_view;
bool m_undo_begun;

TexManipulator_( const DeviceVector& epsilon, const ModifierFlags& state ) :
		m_epsilon( epsilon ),
		m_state( state ),
		m_undo_begun( false ){
}

void mouseDown( DeviceVector position ){
	View scissored( *m_view );
	ConstructSelectionTest( scissored, SelectionBoxForPoint( &position[0], &m_epsilon[0] ) );
	SelectionVolume volume( scissored );

	if( m_state == c_modifier_copy_texture ) {
		Scene_copyClosestTexture( volume );
	}
	else{
		m_undo_begun = true;
		GlobalUndoSystem().start();
		Scene_applyClosestTexture( volume, bitfield_enabled( m_state, c_modifierShift ),
											bitfield_enabled( m_state, c_modifierControl ),
											bitfield_enabled( m_state, c_modifierAlt ), true );
	}
}

void mouseMoved( DeviceVector position ){
	if( m_undo_begun ){
		View scissored( *m_view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( &device_constrained( position )[0], &m_epsilon[0] ) );
		SelectionVolume volume( scissored );

		Scene_applyClosestTexture( volume, bitfield_enabled( m_state, c_modifierShift ),
											bitfield_enabled( m_state, c_modifierControl ),
											bitfield_enabled( m_state, c_modifierAlt ) );
	}
}
typedef MemberCaller1<TexManipulator_, DeviceVector, &TexManipulator_::mouseMoved> MouseMovedCaller;

void mouseUp( DeviceVector position ){
	if( m_undo_begun ){
		GlobalUndoSystem().finish( Scene_applyClosestTexture_getUndoName( bitfield_enabled( m_state, c_modifierShift ),
															bitfield_enabled( m_state, c_modifierControl ),
															bitfield_enabled( m_state, c_modifierAlt ) ) );
		m_undo_begun = false;
	}
	g_mouseMovedCallback.clear();
	g_mouseUpCallback.clear();
}
typedef MemberCaller1<TexManipulator_, DeviceVector, &TexManipulator_::mouseUp> MouseUpCaller;
};


class Selector_
{
RadiantSelectionSystem::EModifier modifier_for_state( ModifierFlags state ){
	if ( ( state == c_modifier_toggle || state == c_modifier_toggle_face || state == c_modifier_face )
			&& m_mouse2 )
		return RadiantSelectionSystem::eReplace;
	else
		return RadiantSelectionSystem::eManipulator;
}

rect_t getDeviceArea() const {
	const DeviceVector delta( m_current - m_start );
	if ( m_mouseMovedWhilePressed && selecting() && delta.x() != 0 && delta.y() != 0 )
		return SelectionBoxForArea( &m_start[0], &delta[0] );
	else
		return rect_t();
}

const DeviceVector& m_epsilon;
ModifierFlags m_state;
public:
DeviceVector m_start;
DeviceVector m_current;
bool m_mouse2;
bool m_mouseMoved;
bool m_mouseMovedWhilePressed;
bool m_paintSelect;
const View* m_view;
RectangleCallback m_window_update;

Selector_( const DeviceVector& epsilon ) :
		m_epsilon( epsilon ),
		m_state( c_modifierNone ),
		m_start( 0.f, 0.f ),
		m_current( 0.f, 0.f ),
		m_mouse2( false ),
		m_mouseMoved( false ),
		m_mouseMovedWhilePressed( false ){
}

void draw_area(){
	m_window_update( getDeviceArea() );
}

void testSelect( DeviceVector position ){
	RadiantSelectionSystem::EModifier modifier = modifier_for_state( m_state );
	if ( modifier != RadiantSelectionSystem::eManipulator ) {
		const DeviceVector delta( position - m_start );
		if ( m_mouseMovedWhilePressed && delta.x() != 0 && delta.y() != 0 ) {
			getSelectionSystem().SelectArea( *m_view, SelectionBoxForArea( &m_start[0], &delta[0] ), ( m_state & c_modifier_face ) != c_modifierNone );
		}
		else if( !m_mouseMovedWhilePressed ){
			if ( modifier == RadiantSelectionSystem::eReplace && !m_mouseMoved ) {
				modifier = RadiantSelectionSystem::eCycle;
			}
			getSelectionSystem().SelectPoint( *m_view, &position[0], &m_epsilon[0], modifier, ( m_state & c_modifier_face ) != c_modifierNone );
		}
	}

	m_start = m_current = DeviceVector( 0.f, 0.f );
	draw_area();
}

void testSelect_simpleM1( DeviceVector position ){
	if( g_bLeftMouseClickSelector )
		getSelectionSystem().SelectPoint( *m_view, &device_constrained( position )[0], &m_epsilon[0], m_mouseMoved ? RadiantSelectionSystem::eReplace : RadiantSelectionSystem::eCycle, false );
}


bool selecting() const {
	return m_state != c_modifier_manipulator && m_mouse2;
}

void setState( ModifierFlags state ){
	const bool was_selecting = selecting();
	m_state = state;
	if ( was_selecting ^ selecting() ) {
		draw_area();
	}
}

void mouseDown( DeviceVector position ){
	m_start = m_current = device_constrained( position );
	if( !m_mouse2 && m_state != c_modifierNone ){
		m_paintSelect = getSelectionSystem().SelectPoint_InitPaint( *m_view, &position[0], &m_epsilon[0], ( m_state & c_modifier_face ) != c_modifierNone );
	}
}

void mouseMoved( DeviceVector position ){
	m_current = device_constrained( position );
	if( m_mouse2 ){
		draw_area();
	}
	else if( m_state != c_modifier_manipulator ){
		getSelectionSystem().SelectPoint( *m_view, &m_current[0], &m_epsilon[0],
										m_paintSelect ? RadiantSelectionSystem::eSelect : RadiantSelectionSystem::eDeselect,
										( m_state & c_modifier_face ) != c_modifierNone );
	}
}
typedef MemberCaller1<Selector_, DeviceVector, &Selector_::mouseMoved> MouseMovedCaller;

void mouseUp( DeviceVector position ){
	if( m_mouse2 ){
		testSelect( device_constrained( position ) );
	}
	else{
		m_start = m_current = DeviceVector( 0.0f, 0.0f );
	}

	g_mouseMovedCallback.clear();
	g_mouseUpCallback.clear();
}
typedef MemberCaller1<Selector_, DeviceVector, &Selector_::mouseUp> MouseUpCaller;
};


class Manipulator_
{
DeviceVector getEpsilon(){
	switch ( getSelectionSystem().ManipulatorMode() )
	{
	case SelectionSystem::eClip:
		return m_epsilon / g_SELECT_EPSILON * ( g_SELECT_EPSILON + 4 );
	case SelectionSystem::eDrag:
	case SelectionSystem::eUV:
		return m_epsilon;
	default: //getSelectionSystem().transformOrigin_isTranslatable()
		return m_epsilon / g_SELECT_EPSILON * 8;
	}
}
const DeviceVector& m_epsilon;
const ModifierFlags& m_state;

public:
const View* m_view;

bool m_moving_transformOrigin;
bool m_mouseMovedWhilePressed;

Manipulator_( const DeviceVector& epsilon, const ModifierFlags& state ) :
		m_epsilon( epsilon ),
		m_state( state ),
		m_moving_transformOrigin( false ),
		m_mouseMovedWhilePressed( false ) {
}

bool mouseDown( DeviceVector position ){
	if( getSelectionSystem().ManipulatorMode() == SelectionSystem::eClip )
		Clipper_tryDoubleclick();
	return getSelectionSystem().SelectManipulator( *m_view, &position[0], &getEpsilon()[0] );
}

void mouseMoved( DeviceVector position ){
	if( m_mouseMovedWhilePressed )
		getSelectionSystem().MoveSelected( *m_view, &position[0], bitfield_enabled( m_state, c_modifierShift ),
																	bitfield_enabled( m_state, c_modifierControl ),
																	bitfield_enabled( m_state, c_modifierAlt ) );
}
typedef MemberCaller1<Manipulator_, DeviceVector, &Manipulator_::mouseMoved> MouseMovedCaller;

void mouseUp( DeviceVector position ){
	m_moving_transformOrigin = getSelectionSystem().endMove();
	g_mouseMovedCallback.clear();
	g_mouseUpCallback.clear();
}
typedef MemberCaller1<Manipulator_, DeviceVector, &Manipulator_::mouseUp> MouseUpCaller;

void highlight( DeviceVector position ){
	getSelectionSystem().HighlightManipulator( *m_view, &position[0], &getEpsilon()[0] );
}
};




class RadiantWindowObserver final : public SelectionSystemWindowObserver
{
DeviceVector m_epsilon;
ModifierFlags m_state;

int m_width;
int m_height;

bool m_mouse_down;

const float m_moveEpsilon;
float m_move; /* released move after m_moveEnd, for tunnel selector decision: eReplace or eCycle */
float m_movePressed; /* pressed move after m_moveStart, for decision: m1 tunnel selector or manipulate and if to do tunnel selector at all */
DeviceVector m_moveStart;
DeviceVector m_moveEnd;

Selector_ m_selector;
Manipulator_ m_manipulator;
TexManipulator_ m_texmanipulator;
public:

RadiantWindowObserver() :
		m_state( c_modifierNone ),
		m_mouse_down( false ),
		m_moveEpsilon( .01f ),
		m_selector( m_epsilon ),
		m_manipulator( m_epsilon, m_state ),
		m_texmanipulator( m_epsilon, m_state ){
}
void release(){
	delete this;
}
void setView( const View& view ){
	m_selector.m_view = &view;
	m_manipulator.m_view = &view;
	m_texmanipulator.m_view = &view;
}
void setRectangleDrawCallback( const RectangleCallback& callback ){
	m_selector.m_window_update = callback;
}
void updateEpsilon(){
	m_epsilon = DeviceVector( g_SELECT_EPSILON / static_cast<float>( m_width ), g_SELECT_EPSILON / static_cast<float>( m_height ) );
}
void onSizeChanged( int width, int height ){
	m_width = width;
	m_height = height;
	updateEpsilon();
}
void onMouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ){
	updateEpsilon(); /* could have changed, as it is user setting */

	const DeviceVector devicePosition( device( position ) );

	if ( button == c_button_select || ( button == c_button_select2 && modifiers != c_modifierNone ) ) {
		m_mouse_down = true;
		g_bAltResize_AltSelect = ( modifiers == c_modifierAlt );

		const bool clipper2d( !m_manipulator.m_view->fill() && button == c_button_select && modifiers == c_modifierControl );
		if( clipper2d && getSelectionSystem().ManipulatorMode() != SelectionSystem::eClip )
			ClipperModeQuick();

		if ( ( modifiers == c_modifier_manipulator
					|| clipper2d
					|| ( modifiers == c_modifierAlt && getSelectionSystem().Mode() == SelectionSystem::ePrimitive ) /* AltResize */
				) && m_manipulator.mouseDown( devicePosition ) ) {
			g_mouseMovedCallback.insert( MouseEventCallback( Manipulator_::MouseMovedCaller( m_manipulator ) ) );
			g_mouseUpCallback.insert( MouseEventCallback( Manipulator_::MouseUpCaller( m_manipulator ) ) );
		}
		else
		{
			m_selector.m_mouse2 = ( button != c_button_select );
			m_selector.mouseDown( devicePosition );
			g_mouseMovedCallback.insert( MouseEventCallback( Selector_::MouseMovedCaller( m_selector ) ) );
			g_mouseUpCallback.insert( MouseEventCallback( Selector_::MouseUpCaller( m_selector ) ) );
		}
	}
	else if ( button == c_button_texture ) {
		m_mouse_down = true;
		m_texmanipulator.mouseDown( devicePosition );
		g_mouseMovedCallback.insert( MouseEventCallback( TexManipulator_::MouseMovedCaller( m_texmanipulator ) ) );
		g_mouseUpCallback.insert( MouseEventCallback( TexManipulator_::MouseUpCaller( m_texmanipulator ) ) );
	}

	m_moveStart = devicePosition;
	m_movePressed = 0.f;
}
void onMouseMotion( const WindowVector& position, ModifierFlags modifiers ){
	m_selector.m_mouseMoved = mouse_moved_epsilon( position, m_moveEnd, m_move );
	if ( m_mouse_down && !g_mouseMovedCallback.empty() ) {
		m_manipulator.m_mouseMovedWhilePressed = m_selector.m_mouseMovedWhilePressed = mouse_moved_epsilon( position, m_moveStart, m_movePressed );
		g_mouseMovedCallback.get() ( device( position ) );
	}
	else{
		m_manipulator.highlight( device( position ) );
	}
}
void onMouseUp( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ){
	if ( ( button == c_button_select || button == c_button_select2 || button == c_button_texture ) && !g_mouseUpCallback.empty() ) {
		g_mouseUpCallback.get() ( device( position ) );
		m_mouse_down = false;
	}
	if( button == c_button_select	/* L button w/o mouse moved = tunnel selection */
			&& modifiers == c_modifierNone
			&& !m_selector.m_mouseMovedWhilePressed
			&& !m_manipulator.m_moving_transformOrigin
			&& !( getSelectionSystem().Mode() == SelectionSystem::eComponent && getSelectionSystem().ManipulatorMode() == SelectionSystem::eDrag )
			&& getSelectionSystem().ManipulatorMode() != SelectionSystem::eClip
			&& getSelectionSystem().ManipulatorMode() != SelectionSystem::eBuild ){
		m_selector.testSelect_simpleM1( device( position ) );
	}
	if( getSelectionSystem().ManipulatorMode() == SelectionSystem::eClip )
		Clipper_tryDoubleclickedCut();

	m_manipulator.m_moving_transformOrigin = false;
	m_selector.m_mouseMoved = false;
	m_selector.m_mouseMovedWhilePressed = false;
	m_manipulator.m_mouseMovedWhilePressed = false;
	m_moveEnd = device( position );
	m_move = 0.f;
}
void onModifierDown( ModifierFlags type ){
	m_state = bitfield_enable( m_state, type );
	m_selector.setState( m_state );
	g_bAltResize_AltSelect = ( m_state == c_modifierAlt );
}
void onModifierUp( ModifierFlags type ){
	m_state = bitfield_disable( m_state, type );
	m_selector.setState( m_state );
	g_bAltResize_AltSelect = ( m_state == c_modifierAlt );
}
DeviceVector device( WindowVector window ) const {
	return window_to_normalised_device( window, m_width, m_height );
}
bool mouse_moved_epsilon( const WindowVector& position, const DeviceVector& moveStart, float& move ){
	if( move > m_moveEpsilon )
		return true;
	const DeviceVector devicePosition( device( position ) );
	const float currentMove = std::max( fabs( devicePosition.x() - moveStart.x() ), fabs( devicePosition.y() - moveStart.y() ) );
	move = std::max( move, currentMove );
//	globalOutputStream() << move << " move\n";
	return move > m_moveEpsilon;
}
/* support mouse_moved_epsilon with frozen pointer (camera freelook) */
void incMouseMove( const WindowVector& delta ){
	const WindowVector normalized_delta( delta.x() * 2.f / m_width, delta.y() * 2.f / m_height );
	m_moveEnd -= normalized_delta;
	if( m_mouse_down )
		m_moveStart -= normalized_delta;
}
};



SelectionSystemWindowObserver* NewWindowObserver(){
	return new RadiantWindowObserver;
}



#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class SelectionDependencies :
	public GlobalSceneGraphModuleRef,
	public GlobalShaderCacheModuleRef,
	public GlobalOpenGLModuleRef
{
};

class SelectionAPI : public TypeSystemRef
{
SelectionSystem* m_selection;
public:
typedef SelectionSystem Type;
STRING_CONSTANT( Name, "*" );

SelectionAPI(){
	SelectionSystem_Construct();

	m_selection = &getSelectionSystem();
}
~SelectionAPI(){
	SelectionSystem_Destroy();
}
SelectionSystem* getTable(){
	return m_selection;
}
};

typedef SingletonModule<SelectionAPI, SelectionDependencies> SelectionModule;
typedef Static<SelectionModule> StaticSelectionModule;
StaticRegisterModule staticRegisterSelection( StaticSelectionModule::instance() );

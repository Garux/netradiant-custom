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

#pragma once

#include "pivot.h"
#include "selectable.h"
#include "view.h"
#include "windowobserver.h"
#include "rect_t.h"

inline int g_SELECT_EPSILON = 12;

using DeviceVector = Vector2;

class Manipulatable
{
public:
	virtual void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) = 0;
	virtual void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) = 0;

	inline static const View* m_view = 0;
	inline static DeviceVector m_device_point;
	inline static DeviceVector m_device_epsilon;
	static void assign_static( const View& view, const DeviceVector& device_point, const DeviceVector& device_epsilon ){
		m_view = &view;
		m_device_point = device_point;
		m_device_epsilon = device_epsilon;
	}
};

class Manipulator
{
public:
	virtual Manipulatable* GetManipulatable() = 0;
	virtual void testSelect( const View& view, const Matrix4& pivot2world ) = 0;
	virtual void highlight( const View& view, const Matrix4& pivot2world ){
		testSelect( view, pivot2world );
	}
	virtual void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ){
	}
	virtual void setSelected( bool select ) = 0;
	virtual bool isSelected() const = 0;
};


class ModifierFlagsExt : public ModifierFlags
{
public:
	ModifierFlagsExt( const ModifierFlags& other ) : ModifierFlags( other ){}
	bool shift() const {
		return bitfield_enabled( *this, c_modifierShift );
	}
	bool ctrl() const {
		return bitfield_enabled( *this, c_modifierControl );
	}
	bool alt() const {
		return bitfield_enabled( *this, c_modifierAlt );
	}
};
inline ModifierFlagsExt g_modifiers = c_modifierNone;


class Translatable
{
public:
	virtual void translate( const Vector3& translation ) = 0;
};

class Rotatable
{
public:
	virtual void rotate( const Quaternion& rotation ) = 0;
};

class Scalable
{
public:
	virtual void scale( const Vector3& scaling ) = 0;
};

class Skewable
{
public:
	virtual void skew( const Skew& skew ) = 0;
};

class AllTransformable
{
public:
	virtual void alltransform( const Transforms& transforms, const Vector3& world_pivot ) = 0;
};


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


inline void ConstructSelectionTest( View& view, const rect_t selection_box ){
	view.EnableScissor( selection_box.min[0], selection_box.max[0], selection_box.min[1], selection_box.max[1] );
}

inline const rect_t SelectionBoxForPoint( const DeviceVector& device_point, const DeviceVector& device_epsilon ){
	rect_t selection_box;
	selection_box.min[0] = device_point[0] - device_epsilon[0];
	selection_box.min[1] = device_point[1] - device_epsilon[1];
	selection_box.max[0] = device_point[0] + device_epsilon[0];
	selection_box.max[1] = device_point[1] + device_epsilon[1];
	return selection_box;
}

inline const rect_t SelectionBoxForArea( const DeviceVector& device_point, const DeviceVector& device_delta ){
	rect_t selection_box;
	selection_box.min[0] = device_point[0] + std::min( device_delta[0], 0.f );
	selection_box.min[1] = device_point[1] + std::min( device_delta[1], 0.f );
	selection_box.max[0] = device_point[0] + std::max( device_delta[0], 0.f );
	selection_box.max[1] = device_point[1] + std::max( device_delta[1], 0.f );
	selection_box.modifier = device_delta[0] * device_delta[1] < 0
	                         ? rect_t::eToggle
	                         : device_delta[0] < 0
	                         ? rect_t::eDeselect
	                         : rect_t::eSelect;
	return selection_box;
}


void Scene_forEachVisible_testselect_scene_point( const View& view, class ScenePointSelector& selector, SelectionTest& test );
void Scene_forEachVisible_testselect_scene_point_selected_brushes( const View& view, ScenePointSelector& selector, SelectionTest& test );
void Scene_TestSelect_Primitive( Selector& selector, SelectionTest& test, const VolumeTest& volume );
void Scene_TestSelect_Component_Selected( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode );
DoubleVector3 testSelected_scene_snapped_point( const class SelectionVolume& test, ScenePointSelector& selector );
void Scene_Translate_Component_Selected( scene::Graph& graph, const Vector3& translation );


inline Vector3 translation_local2object( const Vector3& local, const Matrix4& local2object ){
	return matrix4_get_translation_vec3(
	           matrix4_multiplied_by_matrix4(
	               matrix4_translated_by_vec3( local2object, local ),
	               matrix4_full_inverse( local2object )
	           )
	       );
}
inline Vector3 translation_local2object( const Vector3& localTranslation, const Matrix4& local2parent, const Matrix4& parent2local ){
	return matrix4_get_translation_vec3(
	           matrix4_multiplied_by_matrix4(
	               matrix4_translated_by_vec3( local2parent, localTranslation ),
	               parent2local
	           )
	       );
}

inline Matrix4 transform_local2object( const Matrix4& local, const Matrix4& local2object ){
	return matrix4_multiplied_by_matrix4(
	           matrix4_multiplied_by_matrix4( local2object, local ),
	           matrix4_full_inverse( local2object )
	       );
}
inline Matrix4 transform_local2object( const Matrix4& localTransform, const Matrix4& local2parent, const Matrix4& parent2local ){
	return matrix4_multiplied_by_matrix4(
	           matrix4_multiplied_by_matrix4( local2parent, localTransform ),
	           parent2local
	       );
}


inline Vector3 point_for_device_point( const Matrix4& device2object, const DeviceVector xy, const float z ){
	// transform from normalised device coords to object coords
	return vector4_projected( matrix4_transformed_vector4( device2object, Vector4( xy.x(), xy.y(), z, 1 ) ) );
}

inline Ray ray_for_device_point( const Matrix4& device2object, const DeviceVector xy ){
	return ray_for_points( point_for_device_point( device2object, xy, -1 ), // point at x, y, zNear
	                       point_for_device_point( device2object, xy, 0 )   // point at x, y, zFar
	                       //point_for_device_point( device2object, xy, 1 ) //sometimes is inaccurate up to negative ray direction
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
	//float a = 1;//vector3_dot( ray.direction, ray.direction );        // always >= 0
	const double dot = vector3_dot( ray.direction, other.direction );
	//float c = 1;//vector3_dot( other.direction, other.direction );        // always >= 0
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

inline Vector3 point_on_sphere( const Matrix4& device2object, const DeviceVector xy, const float radius = g_radius ){
	return sphere_intersect_ray( g_origin,
	                             radius,
	                             ray_for_device_point( device2object, xy ) );
}

inline Vector3 point_on_axis( const Vector3& axis, const Matrix4& device2object, const DeviceVector xy ){
	return ray_intersect_ray( ray_for_device_point( device2object, xy ),
	                          Ray( Vector3( 0, 0, 0 ), axis ) );
}

inline Vector3 point_on_plane( const Matrix4& device2object, const DeviceVector xy ){
	const Matrix4 object2device( matrix4_full_inverse( device2object ) );
	return vector4_projected( matrix4_transformed_vector4( device2object, Vector4( xy.x(), xy.y(), object2device[14] / object2device[15], 1 ) ) );
}

inline Vector3 point_on_plane( const Plane3& plane, const Matrix4& object2device, const DeviceVector xy ){
	return ray_intersect_plane( ray_for_device_point( matrix4_full_inverse( object2device ), xy ),
	                            plane );
}

//! a and b are unit vectors .. returns angle in radians
inline float angle_between( const Vector3& a, const Vector3& b ){
	return 2.0 * atan2(
	           vector3_length( vector3_subtracted( a, b ) ),
	           vector3_length( vector3_added( a, b ) )
	       );
}

//! axis is a unit vector
inline void constrain_to_axis( Vector3& vec, const Vector3& axis ){
	vec = vector3_normalised( vector3_added( vec, vector3_scaled( axis, -vector3_dot( vec, axis ) ) ) );
}

//! a and b are unit vectors .. a and b must be orthogonal to axis .. returns angle in radians
inline float angle_for_axis( const Vector3& a, const Vector3& b, const Vector3& axis ){
	if ( vector3_dot( axis, vector3_cross( a, b ) ) > 0 ) {
		return angle_between( a, b );
	}
	else{
		return -angle_between( a, b );
	}
}

inline float distance_for_axis( const Vector3& a, const Vector3& b, const Vector3& axis ){
	return vector3_dot( b, axis ) - vector3_dot( a, axis );
}

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

#include "selection_.h"
#include "grid.h"

/// \brief snaps changed axes of \p move so that \p bounds stick to closest grid lines.
inline void aabb_snap_translation( Vector3& move, const AABB& bounds ){
	const Vector3 maxs( bounds.origin + bounds.extents );
	const Vector3 mins( bounds.origin - bounds.extents );
//	globalOutputStream() << "move: " << move << '\n';
	for( std::size_t i = 0; i < 3; ++i ){
		if( std::fabs( move[i] ) > 1e-2f ){
			const float snapto1 = float_snapped( maxs[i] + move[i], GetSnapGridSize() );
			const float snapto2 = float_snapped( mins[i] + move[i], GetSnapGridSize() );

			const float dist1 = std::fabs( std::fabs( maxs[i] + move[i] ) - std::fabs( snapto1 ) );
			const float dist2 = std::fabs( std::fabs( mins[i] + move[i] ) - std::fabs( snapto2 ) );

//			globalOutputStream() << "maxs[i] + move[i]: " << maxs[i] + move[i]  << "    snapto1: " << snapto1 << "   dist1: " << dist1 << '\n';
//			globalOutputStream() << "mins[i] + move[i]: " << mins[i] + move[i]  << "    snapto2: " << snapto2 << "   dist2: " << dist2 << '\n';
			move[i] = dist2 > dist1 ? snapto1 - maxs[i] : snapto2 - mins[i];
		}
	}
}


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
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_axis( m_axis, device2manip, device_point );
		m_bounds = bounds;
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = point_on_axis( m_axis, device2manip, device_point );
		current = vector3_scaled( m_axis, distance_for_axis( m_start, current, m_axis ) );

		current = translation_local2object( current, manip2object );
		if( g_modifiers.ctrl() )
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
	Vector3 m_0;
	Plane3 m_planeSelected;
	std::size_t m_axisZ;
	Plane3 m_planeZ;
	Vector3 m_startZ;
	Translatable& m_translatable;
	AABB m_bounds;
public:
	TranslateAxis2( Translatable& translatable )
		: m_translatable( translatable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_axisZ = vector3_max_abs_component_index( m_planeSelected.normal() );
		Vector3 xydir( m_view->getViewer() - m_0 );
		xydir[m_axisZ] = 0;
		vector3_normalise( xydir );
		m_planeZ = Plane3( xydir, vector3_dot( xydir, m_0 ) );
		m_startZ = point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point );
		m_bounds = bounds;
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = g_vector3_axes[m_axisZ] * vector3_dot( m_planeSelected.normal(), ( point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point ) - m_startZ ) )
		                  * ( m_planeSelected.normal()[m_axisZ] >= 0? 1 : -1 );

		if( !std::isfinite( current[0] ) || !std::isfinite( current[1] ) || !std::isfinite( current[2] ) ) // catch INF case, is likely with top of the box in 2D
			return;

		if( g_modifiers.ctrl() )
			aabb_snap_translation( current, m_bounds );
		else
			vector3_snap( current, GetSnapGridSize() );

		m_translatable.translate( current );
	}
	void set0( const Vector3& start, const Plane3& planeSelected ){
		m_0 = start;
		m_planeSelected = planeSelected;
	}
};

class TranslateFree : public Manipulatable
{
	Vector3 m_start;
	Translatable& m_translatable;
	AABB m_bounds;
public:
	TranslateFree( Translatable& translatable )
		: m_translatable( translatable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_plane( device2manip, device_point );
		m_bounds = bounds;
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = point_on_plane( device2manip, device_point );
		current = vector3_subtracted( current, m_start );

		if( g_modifiers.shift() ) // snap to axis
			current *= g_vector3_axes[vector3_max_abs_component_index( current )];

		current = translation_local2object( current, manip2object );

		if( g_modifiers.ctrl() ) // snap aabb
			aabb_snap_translation( current, m_bounds );
		else
			vector3_snap( current, GetSnapGridSize() );

		m_translatable.translate( current );
	}
};



/// \brief constructs Quaternion so that rotated box geometry ends up aligned to one or more axes (depends on how much axial \p to is).
inline Quaternion quaternion_for_unit_vectors_for_bounds( const Vector3& axialfrom, const Vector3& to ){
	// do step by step from the larger component to the smaller one
	size_t ids[3] = { vector3_max_abs_component_index( to ), ( ids[0] + 1 ) %3, ( ids[0] + 2 ) %3 };
	if( std::fabs( to[ids[2]] ) > std::fabs( to[ids[1]] ) )
		std::swap( ids[2], ids[1] );

	Vector3 steps[3] = { g_vector3_axes[ids[0]] * std::copysign( 1.f, to[ids[0]] ), to, to };

	Quaternion rotation = quaternion_for_unit_vectors_safe( axialfrom, steps[0] );
	if( std::fabs( to[ids[1]] ) > 1e-6f ){
		steps[1][ids[2]] = 0;
		vector3_normalise( steps[1] );
		rotation = quaternion_multiplied_by_quaternion( quaternion_for_unit_vectors( steps[0], steps[1] ), rotation );
		if( std::fabs( to[ids[2]] ) > 1e-6f ){
			rotation = quaternion_multiplied_by_quaternion( quaternion_for_unit_vectors( steps[1], to ), rotation );
		}
	}
	return rotation;
}


#include <optional>
struct testSelect_unselected_scene_point_return_t{ DoubleVector3 point; std::optional<Plane3> plane; };
std::optional<testSelect_unselected_scene_point_return_t>
testSelect_unselected_scene_point( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon );

void Scene_BoundsSelected_withEntityBounds( scene::Graph& graph, AABB& bounds );

inline std::optional<Vector3> AABB_TestPoint( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon, const AABB& aabb ){
	View scissored( view );
	ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

	SelectionIntersection best;
	AABB_BestPoint( scissored.GetViewMatrix(), EClipCull::CW, aabb, best );
	if( best.valid() ){
		return vector4_projected( matrix4_transformed_vector4( matrix4_full_inverse( scissored.GetViewMatrix() ), Vector4( 0, 0, best.depth(), 1 ) ) );
	}
	return {};
}

class SnapBounds : public Manipulatable
{
	Translatable& m_translatable;
	AllTransformable& m_transformable;
	AABB m_bounds;
	Vector3 m_0;
	// rotate-snap axis and sign of aabb
	size_t m_roatateAxis = 0;
	int m_rotateSign = 1;

	std::optional<Plane3> m_along_plane;
	Vector3 m_along_plane_start_point;
public:
	SnapBounds( Translatable& translatable, AllTransformable& transformable )
		: m_translatable( translatable ), m_transformable( transformable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		if( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive )
			Scene_BoundsSelected_withEntityBounds( GlobalSceneGraph(), m_bounds );
		else
			m_bounds = bounds;

		// for rotate-snap deduce aabb side opposite to clicked
		if( const auto point = AABB_TestPoint( *m_view, device_point, m_device_epsilon, m_bounds ) ){
			m_0 = point.value(); // original m_0 is less reliable fallback
		}
		m_roatateAxis = 0;
		m_rotateSign = 1;
		float bestDist = FLT_MAX;
		for( size_t axis : { 0, 1, 2 } )
			for( int sign : { -1, 1 } )
				if( const float dist = std::fabs( m_0[axis] - ( m_bounds.origin[axis] + std::copysign( m_bounds.extents[axis], sign ) ) ); dist < bestDist ){
					bestDist = dist;
					m_roatateAxis = axis;
					m_rotateSign = sign;
				}

		m_along_plane.reset();
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current( g_vector3_identity );
		if( g_modifiers.shift() ){ // move along plane
			if( !m_along_plane ){ // try to initialize plane from original cursor position
				if( const auto test = testSelect_unselected_scene_point( *m_view, m_device_point, m_device_epsilon );
					test && test->plane ){
					m_along_plane = test->plane;
					m_along_plane_start_point = point_on_plane( *m_along_plane, m_view->GetViewMatrix(), m_device_point );
				}
				else if( const auto test = testSelect_unselected_scene_point( *m_view, device_point, m_device_epsilon );
					test && test->plane ){ // init cursor pos was not on plane, try to fallback to current pos
					m_along_plane = test->plane;
					m_along_plane_start_point = point_on_plane( *m_along_plane, m_view->GetViewMatrix(), device_point );
				}
			}
			if( m_along_plane ){ // got plane, lez go
				current = point_on_plane( *m_along_plane, m_view->GetViewMatrix(), device_point ) - m_along_plane_start_point;
				const size_t maxi = vector3_max_abs_component_index( m_along_plane->normal() );
				vector3_snap( current, GetSnapGridSize() );
				// snap move on two axes with least normal component -> need to find out 3rd move component
				// it equals to point snap to plane with dist=0
				// normal.dot( snapped move ) = 0
				current[maxi] = -( m_along_plane->normal()[( maxi + 1 ) % 3] * current[( maxi + 1 ) % 3]
				                 + m_along_plane->normal()[( maxi + 2 ) % 3] * current[( maxi + 2 ) % 3] )
				                 / m_along_plane->normal()[maxi];
				return m_translatable.translate( current );
			}
		}
		else if( const auto test = testSelect_unselected_scene_point( *m_view, device_point, m_device_epsilon ) ){
			const auto choose_aabb_corner = []( const AABB& bounds, const size_t axis, const Vector3& nrm, const Vector3& ray ){
				Vector3 extents = bounds.extents;
				extents[axis] = std::copysign( extents[axis], nrm[axis] );
				extents[( axis + 1 ) % 3] = std::copysign( extents[( axis + 1 ) % 3], ray[( axis + 1 ) % 3] );
				extents[( axis + 2 ) % 3] = std::copysign( extents[( axis + 2 ) % 3], ray[( axis + 2 ) % 3] );
				return bounds.origin - extents;
			};
			const Ray ray = ray_for_device_point( matrix4_full_inverse( m_view->GetViewMatrix() ), device_point );
			const Vector3 nrm = test->plane? Vector3( test->plane->normal() ) : -ray.direction;
			if( g_modifiers.alt() ){ // rotate-snap
				const Quaternion rotation = quaternion_for_unit_vectors_for_bounds( g_vector3_axes[m_roatateAxis] * m_rotateSign, nrm );
				const Matrix4 unrot = matrix4_rotation_for_quaternion( quaternion_inverse( rotation ) );
				const Vector3 unray = matrix4_transformed_direction( unrot,
					test->plane
					? ray.direction
					// when test point has no plane data we rotate exactly to test ray... tweak ray to deduce distinct aabb corner
					: ray_for_device_point( matrix4_full_inverse( m_view->GetViewMatrix() ), device_point * 1.1f ).direction );
				const Vector3 corner = choose_aabb_corner( m_bounds, m_roatateAxis, -unray, unray );

				Transforms transforms;
				transforms.setRotation( rotation );
				transforms.setTranslation( test->point - corner );
				return m_transformable.alltransform( transforms, corner );
			}
			else{ // move-snap
				const std::size_t axis = vector3_max_abs_component_index( nrm ); // snap bbox along this axis
				current = test->point - choose_aabb_corner( m_bounds, axis, nrm, ray.direction );
				return m_translatable.translate( current );
			}
		}

		m_translatable.translate( current ); // fallback to move to original position
	}
	void set0( const Vector3& start ){
		m_0 = start;
	}
	static bool useCondition( const ModifierFlagsExt& modifiers, const View& view ){
		return modifiers.ctrl() && view.fill();
	}
};

class TranslateFreeXY_Z : public Manipulatable
{
	Vector3 m_0;
	std::size_t m_axisZ;
	Plane3 m_planeXY;
	Plane3 m_planeZ;
	Vector3 m_startXY;
	Vector3 m_startZ;
	Translatable& m_translatable;
	AABB m_bounds;
	SnapBounds m_snapBounds;
public:
	inline static int m_viewdependent = 0;
	TranslateFreeXY_Z( Translatable& translatable, AllTransformable& transformable )
		: m_translatable( translatable ), m_snapBounds( translatable, transformable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
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
		m_startXY = point_on_plane( m_planeXY, m_view->GetViewMatrix(), device_point );
		m_startZ = point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point );
		m_bounds = bounds;

		m_snapBounds.Construct( device2manip, device_point, bounds, transform_origin );
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		if( SnapBounds::useCondition( g_modifiers, *m_view ) ){
			m_snapBounds.Transform( manip2object, device2manip, device_point );
			return;
		}

		Vector3 current;
		if( g_modifiers.alt() && m_view->fill() ) // Z only
			current = ( point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point ) - m_startZ ) * g_vector3_axes[m_axisZ];
		else{
			current = point_on_plane( m_planeXY, m_view->GetViewMatrix(), device_point ) - m_startXY;
			current[m_axisZ] = 0;
		}

		if( g_modifiers.shift() ) // snap to axis
			current *= g_vector3_axes[vector3_max_abs_component_index( current )];

		if( g_modifiers.ctrl() ) // snap aabb
			aabb_snap_translation( current, m_bounds );
		else
			vector3_snap( current, GetSnapGridSize() );

		m_translatable.translate( current );
	}
	void set0( const Vector3& start ){
		m_0 = start;
		m_snapBounds.set0( start );
	}
};

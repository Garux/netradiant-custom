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

class RotateFree : public Manipulatable
{
	Vector3 m_start;
	Rotatable& m_rotatable;
public:
	RotateFree( Rotatable& rotatable )
		: m_rotatable( rotatable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_sphere( device2manip, device_point );
		vector3_normalise( m_start );
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = point_on_sphere( device2manip, device_point );
		vector3_normalise( current );

		if( g_modifiers.shift() )
			for( std::size_t i = 0; i < 3; ++i )
				if( current[i] == 0 )
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
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		const float dot = vector3_dot( m_axis, m_view->fill()? vector3_normalised( m_view->getViewer() - transform_origin ) : m_view->getViewDir() );
		m_plane_way = std::fabs( dot ) > 0.1f;

		if( m_plane_way ){
			m_origin = transform_origin;
			m_plane = Plane3( m_axis, vector3_dot( m_axis, m_origin ) );
			m_start = point_on_plane( m_plane, m_view->GetViewMatrix(), device_point ) - m_origin;
			vector3_normalise( m_start );
		}
		else{
			m_start = point_on_sphere( device2manip, device_point, m_radius );
			constrain_to_axis( m_start, m_axis );
		}
	}
	/// \brief Converts current position to a normalised vector orthogonal to axis.
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current;
		if( m_plane_way ){
			current = point_on_plane( m_plane, m_view->GetViewMatrix(), device_point ) - m_origin;
			vector3_normalise( current );
		}
		else{
			current = point_on_sphere( device2manip, device_point, m_radius );
			constrain_to_axis( current, m_axis );
		}

		if( g_modifiers.shift() ){
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

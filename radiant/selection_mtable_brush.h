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
#include "brushmanip.h"

class DragNewBrush : public Manipulatable
{
	Vector3 m_0;
	Vector3 m_size;
	float m_setSizeZ; /* store separately for fine square/cube modes handling */
	scene::Node* m_newBrushNode;
public:
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_setSizeZ = m_size[0] = m_size[1] = m_size[2] = GetGridSize();
		m_newBrushNode = 0;
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 diff_raw = point_on_plane( Plane3( g_vector3_axis_z, vector3_dot( g_vector3_axis_z, Vector3( m_size.x(), m_size.y(), m_setSizeZ ) + m_0 ) ), m_view->GetViewMatrix(), device_point ) - m_0;
		const Vector3 xydir( vector3_normalised( Vector3( m_view->GetModelview()[2], m_view->GetModelview()[6], 0 ) ) );
		diff_raw.z() = ( point_on_plane( Plane3( xydir, vector3_dot( xydir, Vector3( m_size.x(), m_size.y(), m_setSizeZ ) + m_0 ) ), m_view->GetViewMatrix(), device_point ) - m_0 ).z();
		Vector3 diff = vector3_snapped( diff_raw, GetSnapGridSize() );

		for ( std::size_t i = 0; i < 3; ++i )
			if( diff[i] == 0 )
				diff[i] = diff_raw[i] < 0? -GetGridSize() : GetGridSize();

		if( g_modifiers.alt() ){ // height adjustment
			diff.x() = m_size.x();
			diff.y() = m_size.y();
		}
		else{
			diff.z() = m_size.z();
		}

		const float z = vector4_projected( matrix4_transformed_vector4( m_view->GetViewMatrix(), Vector4( diff + m_0, 1 ) ) ).z();
		if( z != z || z > 1 ) //catch NAN and behind near, far planes cases
			return;

		if( g_modifiers.shift() || g_modifiers.ctrl() ){ // square or cube
			const float squaresize = std::max( std::fabs( diff.x() ), std::fabs( diff.y() ) );
			diff.x() = std::copysign( squaresize, diff.x() ); //square
			diff.y() = std::copysign( squaresize, diff.y() );
			if( g_modifiers.ctrl() && !g_modifiers.alt() ) //cube
				diff.z() = std::copysign( squaresize, diff.z() );
		}

		m_size = diff;
		if( g_modifiers.alt() )
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

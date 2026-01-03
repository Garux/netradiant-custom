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

class SkewAxis : public Manipulatable
{
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
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		Vector3 xydir( m_view->getViewer() - m_0 );
		xydir[m_axis_which] = 0;
	//	xydir *= g_vector3_axes[vector3_max_abs_component_index( xydir )];
		vector3_normalise( xydir );
		m_planeZ = Plane3( xydir, vector3_dot( xydir, m_0 ) );

		m_bounds = bounds;
		m_axis_by_extent = bounds.origin[m_axis_by] + bounds.extents[m_axis_by] * m_axis_by_sign - transform_origin[m_axis_by];
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		const Vector3 current = point_on_plane( m_planeZ, m_view->GetViewMatrix(), device_point ) - m_0;
	//	globalOutputStream() << m_axis_which << " by axis " << m_axis_by << '\n';
		m_skewable.skew( Skew( m_axis_by * 4 + m_axis_which, m_axis_by_extent != 0? float_snapped( current[m_axis_which], GetSnapGridSize() ) / m_axis_by_extent : 0 ) );
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

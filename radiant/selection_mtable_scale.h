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

class ScaleAxis : public Manipulatable
{
	Vector3 m_start;
	Vector3 m_axis;
	Scalable& m_scalable;

	Vector3 m_chosen_extent;
	AABB m_bounds;
public:
	ScaleAxis( Scalable& scalable )
		: m_scalable( scalable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_axis( m_axis, device2manip, device_point );

		m_chosen_extent = Vector3(
		                       std::max( bounds.origin[0] + bounds.extents[0] - transform_origin[0], - bounds.origin[0] + bounds.extents[0] + transform_origin[0] ),
		                       std::max( bounds.origin[1] + bounds.extents[1] - transform_origin[1], - bounds.origin[1] + bounds.extents[1] + transform_origin[1] ),
		                       std::max( bounds.origin[2] + bounds.extents[2] - transform_origin[2], - bounds.origin[2] + bounds.extents[2] + transform_origin[2] )
		                   );
		m_bounds = bounds;
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		//globalOutputStream() << "manip2object: " << manip2object << "  device2manip: " << device2manip << "  x: " << x << "  y:" << y << '\n';
		Vector3 current = point_on_axis( m_axis, device2manip, device_point );
		Vector3 delta = vector3_subtracted( current, m_start );

		delta = translation_local2object( delta, manip2object );
		vector3_snap( delta, GetSnapGridSize() );
		vector3_scale( delta, m_axis );

		Vector3 start( vector3_snapped( m_start, GetSnapGridSize() != 0 ? GetSnapGridSize() : 1e-3f ) );
		for ( std::size_t i = 0; i < 3; ++i ){ //prevent snapping to 0 with big gridsize
			if( float_snapped( m_start[i], 1e-3f ) != 0 && start[i] == 0 ){
				start[i] = GetSnapGridSize();
			}
		}
		//globalOutputStream() << "m_start: " << m_start << "   start: " << start << "   delta: " << delta << '\n';
		/* boundless way */
		Vector3 scale(
		    start[0] == 0 ? 1 : 1 + delta[0] / start[0],
		    start[1] == 0 ? 1 : 1 + delta[1] / start[1],
		    start[2] == 0 ? 1 : 1 + delta[2] / start[2]
		);
		/* try bbox way */
		for( std::size_t i = 0; i < 3; ++i ){
			if( m_chosen_extent[i] > 0.0625f && m_axis[i] != 0 ){ //epsilon to prevent super high scale for set of models, having really small extent, formed by origins
				scale[i] = ( m_chosen_extent[i] + delta[i] ) / m_chosen_extent[i];
				if( g_modifiers.ctrl() ){ // snap bbox dimension size to grid
					const float snappdwidth = float_snapped( scale[i] * m_bounds.extents[i] * 2.f, GetSnapGridSize() );
					scale[i] = snappdwidth / ( m_bounds.extents[i] * 2.f );
				}
			}
		}
		if( g_modifiers.shift() ){ // scale all axes equally
			for( std::size_t i = 0; i < 3; ++i ){
				if( m_axis[i] == 0 ){
					scale[i] = vector3_dot( scale, vector3_scaled( m_axis, m_axis ) );
				}
			}
		}
		//globalOutputStream() << "scale: " << scale << '\n';
		m_scalable.scale( scale );
	}

	void SetAxis( const Vector3& axis ){
		m_axis = axis;
	}
};

class ScaleFree : public Manipulatable
{
	Vector3 m_start;
	Vector3 m_axis;
	Vector3 m_axis2;
	Scalable& m_scalable;

	Vector3 m_chosen_extent;
	AABB m_bounds;
public:
	ScaleFree( Scalable& scalable )
		: m_scalable( scalable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_plane( device2manip, device_point );

		m_chosen_extent = Vector3(
		                       std::max( bounds.origin[0] + bounds.extents[0] - transform_origin[0], -( bounds.origin[0] - bounds.extents[0] - transform_origin[0] ) ),
		                       std::max( bounds.origin[1] + bounds.extents[1] - transform_origin[1], -( bounds.origin[1] - bounds.extents[1] - transform_origin[1] ) ),
		                       std::max( bounds.origin[2] + bounds.extents[2] - transform_origin[2], -( bounds.origin[2] - bounds.extents[2] - transform_origin[2] ) )
		                   );
		m_bounds = bounds;
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = point_on_plane( device2manip, device_point );
		Vector3 delta = vector3_subtracted( current, m_start );

		delta = translation_local2object( delta, manip2object );
		vector3_snap( delta, GetSnapGridSize() );
		if( m_axis != g_vector3_identity )
			delta = vector3_scaled( delta, m_axis ) + vector3_scaled( delta, m_axis2 );

		Vector3 start( vector3_snapped( m_start, GetSnapGridSize() != 0 ? GetSnapGridSize() : 1e-3f ) );
		for ( std::size_t i = 0; i < 3; ++i ){ //prevent snapping to 0 with big gridsize
			if( float_snapped( m_start[i], 1e-3f ) != 0 && start[i] == 0 ){
				start[i] = GetSnapGridSize();
			}
		}

		const std::size_t ignore_axis = vector3_min_abs_component_index( m_start );
		if( g_modifiers.shift() )
			start[ignore_axis] = 0;

		Vector3 scale(
		    start[0] == 0 ? 1 : 1 + delta[0] / start[0],
		    start[1] == 0 ? 1 : 1 + delta[1] / start[1],
		    start[2] == 0 ? 1 : 1 + delta[2] / start[2]
		);

		//globalOutputStream() << "m_start: " << m_start << "   start: " << start << "   delta: " << delta << '\n';
		for( std::size_t i = 0; i < 3; ++i ){
			if( m_chosen_extent[i] > 0.0625f && start[i] != 0 ){
				scale[i] = ( m_chosen_extent[i] + delta[i] ) / m_chosen_extent[i];
				if( g_modifiers.ctrl() ){ // snap bbox dimension size to grid
					const float snappdwidth = float_snapped( scale[i] * m_bounds.extents[i] * 2.f, GetSnapGridSize() );
					scale[i] = snappdwidth / ( m_bounds.extents[i] * 2.f );
				}
			}
		}
		//globalOutputStream() << "pre snap scale: " << scale << '\n';
		if( g_modifiers.shift() ){ // snap 2 axes equally
			float bestscale = ignore_axis != 0 ? scale[0] : scale[1];
			for( std::size_t i = ignore_axis != 0 ? 1 : 2; i < 3; ++i ){
				if( ignore_axis != i && std::fabs( scale[i] ) < std::fabs( bestscale ) ){
					bestscale = scale[i];
				}
				//globalOutputStream() << "bestscale: " << bestscale << '\n';
			}
			for( std::size_t i = 0; i < 3; ++i ){
				if( ignore_axis != i ){
					scale[i] = ( scale[i] < 0 ) ? -std::fabs( bestscale ) : fabs( bestscale );
				}
			}
		}
		//globalOutputStream() << "scale: " << scale << '\n';
		m_scalable.scale( scale );
	}
	void SetAxes( const Vector3& axis, const Vector3& axis2 ){
		m_axis = axis;
		m_axis2 = axis2;
	}
};

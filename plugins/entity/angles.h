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

#include "ientity.h"

#include "math/quaternion.h"
#include "generic/callback.h"
#include "stringio.h"

#include "angle.h"

#include "entity.h"

const Vector3 ANGLESKEY_IDENTITY = Vector3( 0, 0, 0 );

inline void default_angles( Vector3& angles ){
	angles = ANGLESKEY_IDENTITY;
}
inline void normalise_angles( Vector3& angles ){
	angles[0] = static_cast<float>( float_mod( angles[0], 360 ) );
	angles[1] = static_cast<float>( float_mod( angles[1], 360 ) );
	angles[2] = static_cast<float>( float_mod( angles[2], 360 ) );
}
inline void read_angle( Vector3& angles, const char* value ){
	if ( !string_parse_float( value, angles[2] ) ) {
		default_angles( angles );
	}
	else
	{
		angles[0] = 0;
		angles[1] = 0;
		normalise_angles( angles );
	}
}
inline void read_group_angle( Vector3& angles, const char* value ){
	if( string_equal( value, "-1" ) )
		angles = Vector3( 0, -90, 0 );
	else if( string_equal( value, "-2" ) )
		angles = Vector3( 0, 90, 0 );
	else
		read_angle( angles, value );
}
inline void read_angles( Vector3& angles, const char* value ){
	if ( !string_parse_vector3( value, angles ) ) {
		default_angles( angles );
	}
	else
	{
		angles = Vector3( angles[2], g_stupidQuakeBug? -angles[0] : angles[0], angles[1] );
		normalise_angles( angles );
	}
}
inline void write_angles( const Vector3& angles, Entity* entity ){
	if ( angles == ANGLESKEY_IDENTITY ) {
		entity->setKeyValue( "angle", "" );
		entity->setKeyValue( "angles", "" );
	}
	else
	{
		if ( angles[0] == 0 && angles[1] == 0 ) {
			const float yaw = angles[2];
			entity->setKeyValue( "angles", "" );
			write_angle( yaw, entity );
		}
		else
		{
			char value[64];
			sprintf( value, "%g %g %g", g_stupidQuakeBug? -angles[1] : angles[1], angles[2], angles[0] );
			entity->setKeyValue( "angle", "" );
			entity->setKeyValue( "angles", value );
		}
	}
}

inline Matrix4 matrix4_rotation_for_euler_xyz_degrees_quantised( const Vector3& angles ){
	if( angles[0] == 0.f && angles[1] == 0.f ){
		return matrix4_rotation_for_z_degrees( angles[2] );
	}
	else if( angles[0] == 0.f && angles[2] == 0.f ){
		return matrix4_rotation_for_y_degrees( angles[1] );
	}
	else if( angles[1] == 0.f && angles[2] == 0.f ){
		return matrix4_rotation_for_x_degrees( angles[0] );
	}
	return matrix4_rotation_for_euler_xyz_degrees( angles );
}

inline Vector3 angles_snapped_to_zero( const Vector3& angles ){
	const float epsilon = ( fabs( angles[0] ) > 0.001f || fabs( angles[1] ) > 0.001f || fabs( angles[2] ) > 0.001f ) ? 5e-5 : 1e-6;
	return Vector3( fabs( angles[0] ) < epsilon ? 0.f : angles[0],
	                fabs( angles[1] ) < epsilon ? 0.f : angles[1],
	                fabs( angles[2] ) < epsilon ? 0.f : angles[2]
	              );
}

inline Vector3 angles_rotated( const Vector3& angles, const Quaternion& rotation ){
	return angles_snapped_to_zero(
	           matrix4_get_rotation_euler_xyz_degrees(
	               matrix4_multiplied_by_matrix4(
	                   matrix4_rotation_for_quaternion_quantised( rotation ),
	                   matrix4_rotation_for_euler_xyz_degrees_quantised( angles )
	               )
	           )
	       );
}
#if 0
inline Vector3 angles_rotated_for_rotated_pivot( const Vector3& angles, const Quaternion& rotation ){
	return angles_snapped_to_zero(
	           matrix4_get_rotation_euler_xyz_degrees(
	               matrix4_multiplied_by_matrix4(
	                   matrix4_rotation_for_euler_xyz_degrees_quantised( angles ),
	                   matrix4_rotation_for_quaternion_quantised( rotation )
	               )
	           )
	       );
}
#endif
class AnglesKey
{
	Callback<void()> m_anglesChanged;
	KeyObserver m_angleCB;
	KeyObserver m_anglesCB;
	const Entity& m_entity;
public:
	Vector3 m_angles;


	AnglesKey( const Callback<void()>& anglesChanged, const Entity& entity )
		: m_anglesChanged( anglesChanged ), m_angleCB(), m_anglesCB(), m_entity( entity ), m_angles( ANGLESKEY_IDENTITY ){
	}

	void angleChanged( const char* value ){
		if( !m_entity.hasKeyValue( "angles" ) || m_anglesCB == KeyObserver() ){ // no "angles" set or supported
			read_angle( m_angles, value );
			m_anglesChanged();
		}
	}
	KeyObserver getAngleChangedCallback(){
		return m_angleCB = MemberCaller<AnglesKey, void(const char*), &AnglesKey::angleChanged>( *this );
	}

	void groupAngleChanged( const char* value ){
		if( !m_entity.hasKeyValue( "angles" ) || m_anglesCB == KeyObserver() ){ // no "angles" set or supported
			read_group_angle( m_angles, value );
			m_anglesChanged();
		}
	}
	KeyObserver getGroupAngleChangedCallback(){
		return m_angleCB = MemberCaller<AnglesKey, void(const char*), &AnglesKey::groupAngleChanged>( *this );
	}

	void anglesChanged( const char* value ){
		if( m_entity.hasKeyValue( "angles" ) ){ // check actual key presence, as this may be notified by default value on key removal
			read_angles( m_angles, value );
			m_anglesChanged();
		}
		else // "angles" key removed // improvable: also do this on invalid "angles" key
			m_angleCB( m_entity.getKeyValue( "angle" ) );
	}
	KeyObserver getAnglesChangedCallback(){
		return m_anglesCB = MemberCaller<AnglesKey, void(const char*), &AnglesKey::anglesChanged>( *this );
	}

	void write( Entity* entity ) const {
		write_angles( m_angles, entity );
	}
};

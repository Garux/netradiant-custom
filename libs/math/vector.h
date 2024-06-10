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

/// \file
/// \brief Vector data types and related operations.

#include "generic/vector.h"

#if defined ( _MSC_VER )

// Apparently modern MSC comes with lrint, but I don't know since when. 1940 is almost certainly too much.
#if ( _MSC_VER ) < 1940
inline int lrint( double flt ){
	int i;

	_asm
	{
		fld flt
		fistp i
	};

	return i;
}
#endif

inline __int64 llrint( double f ){
	return static_cast<__int64>( f + 0.5 );
}

#elif defined( __FreeBSD__ )

inline long lrint( double f ){
	return static_cast<long>( f + 0.5 );
}

inline long long llrint( double f ){
	return static_cast<long long>( f + 0.5 );
}

#elif defined( __GNUC__ )

// lrint is part of ISO C99
#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1

#define __USE_ISOC9X    1
#define __USE_ISOC99    1

#else
#error "unsupported platform"
#endif

#include <cmath>
#include <cfloat>
#include <algorithm>


//#include "debugging/debugging.h"

/// \brief Returns true if \p self is equal to other \p other within \p epsilon.
template<typename Element, typename OtherElement>
inline bool float_equal_epsilon( const Element& self, const OtherElement& other, const Element& epsilon ){
	return fabs( other - self ) < epsilon;
}

/// \brief Returns the value midway between \p self and \p other.
template<typename Element>
inline Element float_mid( const Element& self, const Element& other ){
	return Element( ( self + other ) * 0.5 );
}

/// \brief Returns \p f rounded to the nearest integer. Note that this is not the same behaviour as casting from float to int.
template<typename Element>
inline int float_to_integer( const Element& f ){
	return lrint( f );
}

/// \brief Returns \p f rounded to the nearest multiple of \p snap.
template<typename Element, typename OtherElement>
inline Element float_snapped( const Element& f, const OtherElement& snap ){
	//return Element(float_to_integer(f / snap) * snap);
	if ( snap == 0 ) {
		return f;
	}
	return Element( llrint( f / snap ) * snap ); // llrint has more significant bits
}

/// \brief Returns true if \p f has no decimal fraction part.
template<typename Element>
inline bool float_is_integer( const Element& f ){
	return f == Element( float_to_integer( f ) );
}

/// \brief Returns \p self modulated by the range [0, \p modulus)
/// \p self must be in the range [\p -modulus, \p modulus)
template<typename Element, typename ModulusElement>
inline Element float_mod_range( const Element& self, const ModulusElement& modulus ){
	return Element( ( self < 0.0 ) ? self + modulus : self );
}

/// \brief Returns \p self modulated by the range [0, \p modulus)
template<typename Element, typename ModulusElement>
inline Element float_mod( const Element& self, const ModulusElement& modulus ){
	return float_mod_range( Element( fmod( static_cast<double>( self ), static_cast<double>( modulus ) ) ), modulus );
}


template<typename Element, typename OtherElement>
inline bool vector2_equal( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return self.x() == other.x() && self.y() == other.y();
}
template<typename Element, typename OtherElement>
inline bool operator==( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return vector2_equal( self, other );
}
template<typename Element, typename OtherElement>
inline bool operator!=( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return !vector2_equal( self, other );
}

template<typename Element, typename OtherElement, typename Epsilon>
inline bool vector2_equal_epsilon( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other, Epsilon epsilon ){
	return float_equal_epsilon( self.x(), other.x(), epsilon )
	    && float_equal_epsilon( self.y(), other.y(), epsilon );
}

template<typename Element, typename OtherElement>
inline BasicVector2<Element> vector2_added( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return BasicVector2<Element>(
	           Element( self.x() + other.x() ),
	           Element( self.y() + other.y() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector2<Element> operator+( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return vector2_added( self, other );
}
template<typename Element, typename OtherElement>
inline void vector2_add( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	self.x() += Element( other.x() );
	self.y() += Element( other.y() );
}
template<typename Element, typename OtherElement>
inline void operator+=( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	vector2_add( self, other );
}


template<typename Element, typename OtherElement>
inline BasicVector2<Element> vector2_subtracted( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return BasicVector2<Element>(
	           Element( self.x() - other.x() ),
	           Element( self.y() - other.y() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector2<Element> operator-( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return vector2_subtracted( self, other );
}
template<typename Element, typename OtherElement>
inline void vector2_subtract( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	self.x() -= Element( other.x() );
	self.y() -= Element( other.y() );
}
template<typename Element, typename OtherElement>
inline void operator-=( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	vector2_subtract( self, other );
}


template<typename Element, typename OtherElement>
inline BasicVector2<Element> vector2_scaled( const BasicVector2<Element>& self, OtherElement other ){
	return BasicVector2<Element>(
	           Element( self.x() * other ),
	           Element( self.y() * other )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector2<Element> operator*( const BasicVector2<Element>& self, OtherElement other ){
	return vector2_scaled( self, other );
}
template<typename Element, typename OtherElement>
inline void vector2_scale( BasicVector2<Element>& self, OtherElement other ){
	self.x() *= Element( other );
	self.y() *= Element( other );
}
template<typename Element, typename OtherElement>
inline void operator*=( BasicVector2<Element>& self, OtherElement other ){
	vector2_scale( self, other );
}


template<typename Element, typename OtherElement>
inline BasicVector2<Element> vector2_scaled( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return BasicVector2<Element>(
	           Element( self.x() * other.x() ),
	           Element( self.y() * other.y() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector2<Element> operator*( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return vector2_scaled( self, other );
}
template<typename Element, typename OtherElement>
inline void vector2_scale( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	self.x() *= Element( other.x() );
	self.y() *= Element( other.y() );
}
template<typename Element, typename OtherElement>
inline void operator*=( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	vector2_scale( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector2<Element> vector2_divided( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return BasicVector2<Element>(
	           Element( self.x() / other.x() ),
	           Element( self.y() / other.y() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector2<Element> operator/( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return vector2_divided( self, other );
}
template<typename Element, typename OtherElement>
inline void vector2_divide( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	self.x() /= Element( other.x() );
	self.y() /= Element( other.y() );
}
template<typename Element, typename OtherElement>
inline void operator/=( BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	vector2_divide( self, other );
}


template<typename Element, typename OtherElement>
inline BasicVector2<Element> vector2_divided( const BasicVector2<Element>& self, OtherElement other ){
	return BasicVector2<Element>(
	           Element( self.x() / other ),
	           Element( self.y() / other )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector2<Element> operator/( const BasicVector2<Element>& self, OtherElement other ){
	return vector2_divided( self, other );
}
template<typename Element, typename OtherElement>
inline void vector2_divide( BasicVector2<Element>& self, OtherElement other ){
	self.x() /= Element( other );
	self.y() /= Element( other );
}
template<typename Element, typename OtherElement>
inline void operator/=( BasicVector2<Element>& self, OtherElement other ){
	vector2_divide( self, other );
}

template<typename Element, typename OtherElement>
inline double vector2_dot( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return self.x() * other.x() + self.y() * other.y();
}

template<typename Element>
inline double vector2_length_squared( const BasicVector2<Element>& self ){
	return vector2_dot( self, self );
}

template<typename Element>
inline double vector2_length( const BasicVector2<Element>& self ){
	return sqrt( vector2_length_squared( self ) );
}

template<typename Element, typename OtherElement>
inline double vector2_cross( const BasicVector2<Element>& self, const BasicVector2<OtherElement>& other ){
	return self.x() * other.y() - self.y() * other.x();
}

template<typename Element>
inline Element float_divided( Element f, Element other ){
	//ASSERT_MESSAGE(other != 0, "float_divided: invalid divisor");
	return f / other;
}

template<typename Element>
inline BasicVector2<Element> vector2_normalised( const BasicVector2<Element>& self ){
	return vector2_scaled( self, float_divided( 1.0, vector2_length( self ) ) );
}

template<typename Element>
inline void vector2_normalise( BasicVector2<Element>& self ){
	self = vector2_normalised( self );
}

template<typename Element>
inline BasicVector2<Element> vector2_mid( const BasicVector2<Element>& begin, const BasicVector2<Element>& end ){
	return vector2_scaled( vector2_added( begin, end ), 0.5 );
}

const Vector3 g_vector3_identity( 0, 0, 0 );
const Vector3 g_vector3_max = Vector3( FLT_MAX, FLT_MAX, FLT_MAX );
const Vector3 g_vector3_axis_x( 1, 0, 0 );
const Vector3 g_vector3_axis_y( 0, 1, 0 );
const Vector3 g_vector3_axis_z( 0, 0, 1 );

const Vector3 g_vector3_axes[3] = { g_vector3_axis_x, g_vector3_axis_y, g_vector3_axis_z };

template<typename Element, typename OtherElement>
inline void vector3_swap( BasicVector3<Element>& self, BasicVector3<OtherElement>& other ){
	std::swap( self.x(), other.x() );
	std::swap( self.y(), other.y() );
	std::swap( self.z(), other.z() );
}

template<typename Element, typename OtherElement>
inline bool vector3_equal( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return self.x() == other.x() && self.y() == other.y() && self.z() == other.z();
}
template<typename Element, typename OtherElement>
inline bool operator==( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return vector3_equal( self, other );
}
template<typename Element, typename OtherElement>
inline bool operator!=( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return !vector3_equal( self, other );
}


template<typename Element, typename OtherElement, typename Epsilon>
inline bool vector3_equal_epsilon( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other, Epsilon epsilon ){
	return float_equal_epsilon( self.x(), other.x(), epsilon )
	    && float_equal_epsilon( self.y(), other.y(), epsilon )
	    && float_equal_epsilon( self.z(), other.z(), epsilon );
}



template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_added( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return BasicVector3<Element>(
	           Element( self.x() + other.x() ),
	           Element( self.y() + other.y() ),
	           Element( self.z() + other.z() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> operator+( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return vector3_added( self, other );
}
template<typename Element, typename OtherElement>
inline void vector3_add( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	self.x() += static_cast<Element>( other.x() );
	self.y() += static_cast<Element>( other.y() );
	self.z() += static_cast<Element>( other.z() );
}
template<typename Element, typename OtherElement>
inline void operator+=( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	vector3_add( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_subtracted( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return BasicVector3<Element>(
	           Element( self.x() - other.x() ),
	           Element( self.y() - other.y() ),
	           Element( self.z() - other.z() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> operator-( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return vector3_subtracted( self, other );
}
template<typename Element, typename OtherElement>
inline void vector3_subtract( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	self.x() -= static_cast<Element>( other.x() );
	self.y() -= static_cast<Element>( other.y() );
	self.z() -= static_cast<Element>( other.z() );
}
template<typename Element, typename OtherElement>
inline void operator-=( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	vector3_subtract( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_scaled( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return BasicVector3<Element>(
	           Element( self.x() * other.x() ),
	           Element( self.y() * other.y() ),
	           Element( self.z() * other.z() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> operator*( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return vector3_scaled( self, other );
}
template<typename Element, typename OtherElement>
inline void vector3_scale( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	self.x() *= static_cast<Element>( other.x() );
	self.y() *= static_cast<Element>( other.y() );
	self.z() *= static_cast<Element>( other.z() );
}
template<typename Element, typename OtherElement>
inline void operator*=( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	vector3_scale( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_scaled( const BasicVector3<Element>& self, const OtherElement& scale ){
	return BasicVector3<Element>(
	           Element( self.x() * scale ),
	           Element( self.y() * scale ),
	           Element( self.z() * scale )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> operator*( const BasicVector3<Element>& self, const OtherElement& scale ){
	return vector3_scaled( self, scale );
}
template<typename Element, typename OtherElement>
inline void vector3_scale( BasicVector3<Element>& self, const OtherElement& scale ){
	self.x() *= static_cast<Element>( scale );
	self.y() *= static_cast<Element>( scale );
	self.z() *= static_cast<Element>( scale );
}
template<typename Element, typename OtherElement>
inline void operator*=( BasicVector3<Element>& self, const OtherElement& scale ){
	vector3_scale( self, scale );
}

template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_divided( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return BasicVector3<Element>(
	           Element( self.x() / other.x() ),
	           Element( self.y() / other.y() ),
	           Element( self.z() / other.z() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> operator/( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return vector3_divided( self, other );
}
template<typename Element, typename OtherElement>
inline void vector3_divide( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	self.x() /= static_cast<Element>( other.x() );
	self.y() /= static_cast<Element>( other.y() );
	self.z() /= static_cast<Element>( other.z() );
}
template<typename Element, typename OtherElement>
inline void operator/=( BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	vector3_divide( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_divided( const BasicVector3<Element>& self, const OtherElement& divisor ){
	return BasicVector3<Element>(
	           Element( self.x() / divisor ),
	           Element( self.y() / divisor ),
	           Element( self.z() / divisor )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> operator/( const BasicVector3<Element>& self, const OtherElement& divisor ){
	return vector3_divided( self, divisor );
}
template<typename Element, typename OtherElement>
inline void vector3_divide( BasicVector3<Element>& self, const OtherElement& divisor ){
	self.x() /= static_cast<Element>( divisor );
	self.y() /= static_cast<Element>( divisor );
	self.z() /= static_cast<Element>( divisor );
}
template<typename Element, typename OtherElement>
inline void operator/=( BasicVector3<Element>& self, const OtherElement& divisor ){
	vector3_divide( self, divisor );
}

template<typename Element, typename OtherElement>
inline double vector3_dot( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return self.x() * other.x() + self.y() * other.y() + self.z() * other.z();
}

template<typename Element>
inline BasicVector3<Element> vector3_mid( const BasicVector3<Element>& begin, const BasicVector3<Element>& end ){
	return vector3_scaled( vector3_added( begin, end ), 0.5 );
}

template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_cross( const BasicVector3<Element>& self, const BasicVector3<OtherElement>& other ){
	return BasicVector3<Element>(
	           Element( self.y() * other.z() - self.z() * other.y() ),
	           Element( self.z() * other.x() - self.x() * other.z() ),
	           Element( self.x() * other.y() - self.y() * other.x() )
	       );
}

template<typename Element>
inline BasicVector3<Element> vector3_negated( const BasicVector3<Element>& self ){
	return BasicVector3<Element>( -self.x(), -self.y(), -self.z() );
}
template<typename Element>
inline BasicVector3<Element> operator-( const BasicVector3<Element>& self ){
	return vector3_negated( self );
}

template<typename Element>
inline void vector3_negate( BasicVector3<Element>& self ){
	self = vector3_negated( self );
}

template<typename Element>
inline double vector3_length_squared( const BasicVector3<Element>& self ){
	return vector3_dot( self, self );
}

template<typename Element>
inline double vector3_length( const BasicVector3<Element>& self ){
	return sqrt( vector3_length_squared( self ) );
}

template<typename Element>
inline BasicVector3<Element> vector3_normalised( const BasicVector3<Element>& self ){
	return vector3_scaled( self, float_divided( 1.0, vector3_length( self ) ) );
}

template<typename Element>
inline void vector3_normalise( BasicVector3<Element>& self ){
	self = vector3_normalised( self );
}


template<typename Element>
inline BasicVector3<Element> vector3_snapped( const BasicVector3<Element>& self ){
	return BasicVector3<Element>(
	           Element( float_to_integer( self.x() ) ),
	           Element( float_to_integer( self.y() ) ),
	           Element( float_to_integer( self.z() ) )
	       );
}
template<typename Element>
inline void vector3_snap( BasicVector3<Element>& self ){
	self = vector3_snapped( self );
}
template<typename Element, typename OtherElement>
inline BasicVector3<Element> vector3_snapped( const BasicVector3<Element>& self, const OtherElement& snap ){
	return BasicVector3<Element>(
	           Element( float_snapped( self.x(), snap ) ),
	           Element( float_snapped( self.y(), snap ) ),
	           Element( float_snapped( self.z(), snap ) )
	       );
}
template<typename Element, typename OtherElement>
inline void vector3_snap( BasicVector3<Element>& self, const OtherElement& snap ){
	self = vector3_snapped( self, snap );
}

inline Vector3 vector3_for_spherical( double theta, double phi ){
	return Vector3(
	           static_cast<float>( cos( theta ) * cos( phi ) ),
	           static_cast<float>( sin( theta ) * cos( phi ) ),
	           static_cast<float>( sin( phi ) )
	       );
}

template<typename Element>
inline std::size_t vector3_max_abs_component_index( const BasicVector3<Element>& self ){
	const std::size_t maxi = ( fabs( self[1] ) > fabs( self[0] ) )? 1 : 0;
	return ( fabs( self[2] ) > fabs( self[maxi] ) )? 2 : maxi;;
}

template<typename Element>
inline std::size_t vector3_min_abs_component_index( const BasicVector3<Element>& self ){
	const std::size_t mini = ( fabs( self[1] ) < fabs( self[0] ) )? 1 : 0;
	return ( fabs( self[2] ) < fabs( self[mini] ) )? 2 : mini;
}

template<typename Element>
inline Element vector3_max_component( const BasicVector3<Element>& v ){
	return ( v[0] > v[1] ) ? ( ( v[0] > v[2] ) ? v[0] : v[2] ) : ( ( v[1] > v[2] ) ? v[1] : v[2] );
}

template<typename Element>
inline Element vector3_min_component( const BasicVector3<Element>& v ){
	return ( v[0] < v[1] ) ? ( ( v[0] < v[2] ) ? v[0] : v[2] ) : ( ( v[1] < v[2] ) ? v[1] : v[2] );
}




template<typename Element, typename OtherElement>
inline bool vector4_equal( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return self.x() == other.x() && self.y() == other.y() && self.z() == other.z() && self.w() == other.w();
}
template<typename Element, typename OtherElement>
inline bool operator==( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return vector4_equal( self, other );
}
template<typename Element, typename OtherElement>
inline bool operator!=( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return !vector4_equal( self, other );
}

template<typename Element, typename OtherElement>
inline bool vector4_equal_epsilon( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other, Element epsilon ){
	return float_equal_epsilon( self.x(), other.x(), epsilon )
	    && float_equal_epsilon( self.y(), other.y(), epsilon )
	    && float_equal_epsilon( self.z(), other.z(), epsilon )
	    && float_equal_epsilon( self.w(), other.w(), epsilon );
}

template<typename Element, typename OtherElement>
inline BasicVector4<Element> vector4_added( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return BasicVector4<Element>(
	           float(self.x() + other.x() ),
	           float(self.y() + other.y() ),
	           float(self.z() + other.z() ),
	           float(self.w() + other.w() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector4<Element> operator+( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return vector4_added( self, other );
}
template<typename Element, typename OtherElement>
inline void vector4_add( BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	self.x() += static_cast<float>( other.x() );
	self.y() += static_cast<float>( other.y() );
	self.z() += static_cast<float>( other.z() );
	self.w() += static_cast<float>( other.w() );
}
template<typename Element, typename OtherElement>
inline void operator+=( BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	vector4_add( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector4<Element> vector4_subtracted( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return BasicVector4<Element>(
	           float(self.x() - other.x() ),
	           float(self.y() - other.y() ),
	           float(self.z() - other.z() ),
	           float(self.w() - other.w() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector4<Element> operator-( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return vector4_subtracted( self, other );
}
template<typename Element, typename OtherElement>
inline void vector4_subtract( BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	self.x() -= static_cast<float>( other.x() );
	self.y() -= static_cast<float>( other.y() );
	self.z() -= static_cast<float>( other.z() );
	self.w() -= static_cast<float>( other.w() );
}
template<typename Element, typename OtherElement>
inline void operator-=( BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	vector4_subtract( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector4<Element> vector4_scaled( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return BasicVector4<Element>(
	           float(self.x() * other.x() ),
	           float(self.y() * other.y() ),
	           float(self.z() * other.z() ),
	           float(self.w() * other.w() )
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector4<Element> operator*( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return vector4_scaled( self, other );
}
template<typename Element, typename OtherElement>
inline void vector4_scale( BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	self.x() *= static_cast<float>( other.x() );
	self.y() *= static_cast<float>( other.y() );
	self.z() *= static_cast<float>( other.z() );
	self.w() *= static_cast<float>( other.w() );
}
template<typename Element, typename OtherElement>
inline void operator*=( BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	vector4_scale( self, other );
}

template<typename Element, typename OtherElement>
inline BasicVector4<Element> vector4_scaled( const BasicVector4<Element>& self, OtherElement scale ){
	return BasicVector4<Element>(
	           float(self.x() * scale),
	           float(self.y() * scale),
	           float(self.z() * scale),
	           float(self.w() * scale)
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector4<Element> operator*( const BasicVector4<Element>& self, OtherElement scale ){
	return vector4_scaled( self, scale );
}
template<typename Element, typename OtherElement>
inline void vector4_scale( BasicVector4<Element>& self, OtherElement scale ){
	self.x() *= static_cast<float>( scale );
	self.y() *= static_cast<float>( scale );
	self.z() *= static_cast<float>( scale );
	self.w() *= static_cast<float>( scale );
}
template<typename Element, typename OtherElement>
inline void operator*=( BasicVector4<Element>& self, OtherElement scale ){
	vector4_scale( self, scale );
}

template<typename Element, typename OtherElement>
inline BasicVector4<Element> vector4_divided( const BasicVector4<Element>& self, OtherElement divisor ){
	return BasicVector4<Element>(
	           float(self.x() / divisor),
	           float(self.y() / divisor),
	           float(self.z() / divisor),
	           float(self.w() / divisor)
	       );
}
template<typename Element, typename OtherElement>
inline BasicVector4<Element> operator/( const BasicVector4<Element>& self, OtherElement divisor ){
	return vector4_divided( self, divisor );
}
template<typename Element, typename OtherElement>
inline void vector4_divide( BasicVector4<Element>& self, OtherElement divisor ){
	self.x() /= divisor;
	self.y() /= divisor;
	self.z() /= divisor;
	self.w() /= divisor;
}
template<typename Element, typename OtherElement>
inline void operator/=( BasicVector4<Element>& self, OtherElement divisor ){
	vector4_divide( self, divisor );
}

template<typename Element, typename OtherElement>
inline double vector4_dot( const BasicVector4<Element>& self, const BasicVector4<OtherElement>& other ){
	return self.x() * other.x() + self.y() * other.y() + self.z() * other.z() + self.w() * other.w();
}

template<typename Element>
inline BasicVector3<Element> vector4_projected( const BasicVector4<Element>& self ){
	return vector3_scaled( vector4_to_vector3( self ), 1.0 / self[3] );
}

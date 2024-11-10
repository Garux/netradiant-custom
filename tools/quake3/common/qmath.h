#pragma once

#include "bytebool.h"
#include "math/vector.h"
#include "math/plane.h"


#define VectorCopy( a,b ) ( ( b )[0] = ( a )[0],( b )[1] = ( a )[1],( b )[2] = ( a )[2] )

#define RGBTOGRAY( x ) ( (float)( ( x )[0] ) * 0.2989f + (float)( ( x )[1] ) * 0.5870f + (float)( ( x )[2] ) * 0.1140f )

#define VectorFastNormalize VectorNormalize

template<typename T>
inline void value_maximize( T& value, const T& other ){
	value = std::max( value, other );
}

template<typename T>
inline void value_minimize( T& value, const T& other ){
	value = std::min( value, other );
}


inline bool bit_is_enabled( const byte *bytes, int bit_index ){
	return ( bytes[bit_index >> 3] & ( 1 << ( bit_index & 7 ) ) ) != 0;
}
inline void bit_enable( byte *bytes, int bit_index ){
	bytes[bit_index >> 3] |= ( 1 << ( bit_index & 7 ) );
}


template<typename T>
struct MinMax___
{
	BasicVector3<T> mins;
	BasicVector3<T> maxs;
	MinMax___() : mins( std::numeric_limits<T>::max() ), maxs( std::numeric_limits<T>::lowest() ) {
	}
	template<typename U>
	MinMax___( const BasicVector3<U>& min, const BasicVector3<U>& max ) : mins( min ), maxs( max ){
	}
	void clear(){
		*this = MinMax___();
	}
	bool valid() const {
		return mins.x() < maxs.x() && mins.y() < maxs.y() && mins.z() < maxs.z();
	}
	template<typename U>
	void extend( const BasicVector3<U>& point ){
		for ( size_t i = 0; i < 3; ++i ){
			const auto val = point[i];
			if ( val < mins[i] ) {
				mins[i] = val;
			}
			if ( val > maxs[i] ) {
				maxs[i] = val;
			}
		}
	}
	template<typename U>
	void extend( const MinMax___<U>& other ){
		extend( other.mins );
		extend( other.maxs );
	}
	// true, if point is within the bounds
	template<typename U>
	bool test( const BasicVector3<U>& point ) const {
		return point.x() >= mins.x() && point.y() >= mins.y() && point.z() >= mins.z()
		    && point.x() <= maxs.x() && point.y() <= maxs.y() && point.z() <= maxs.z();
	}
	// true, if point is within the bounds expanded by epsilon
	template<typename U, typename E>
	bool test( const BasicVector3<U>& point, const E epsilon ) const {
		return point.x() >= mins.x() - epsilon && point.y() >= mins.y() - epsilon && point.z() >= mins.z() - epsilon
		    && point.x() <= maxs.x() + epsilon && point.y() <= maxs.y() + epsilon && point.z() <= maxs.z() + epsilon;
	}
	// true, if there is an intersection
	template<typename U>
	bool test( const MinMax___<U>& other ) const {
		return other.maxs.x() >= mins.x() && other.maxs.y() >= mins.y() && other.maxs.z() >= mins.z()
		    && other.mins.x() <= maxs.x() && other.mins.y() <= maxs.y() && other.mins.z() <= maxs.z();
	}
	// true, if other is completely enclosed by this
	template<typename U>
	bool surrounds( const MinMax___<U>& other ) const {
		return other.mins.x() >= mins.x() && other.mins.y() >= mins.y() && other.mins.z() >= mins.z()
		    && other.maxs.x() <= maxs.x() && other.maxs.y() <= maxs.y() && other.maxs.z() <= maxs.z();
	}
	BasicVector3<T> origin() const {
		return ( mins + maxs ) * 0.5;
	}
};

using MinMax = MinMax___<float>;



template<typename T>
struct Color4___ : public BasicVector4<T>
{
	using BasicVector4<T>::BasicVector4;

	Color4___( const BasicVector4<T>& vector ) : BasicVector4<T>( vector ){
	}
	BasicVector3<T>& rgb(){
		return this->vec3();
	}
	const BasicVector3<T>& rgb() const {
		return this->vec3();
	}
	T& alpha(){
		return this->w();
	}
	const T& alpha() const {
		return this->w();
	}
};

using Color4f = Color4___<float>;
using Color4b = Color4___<byte>;
using Vector3b = BasicVector3<byte>;


inline byte color_to_byte( float color ){
	return std::clamp( color, 0.f, 255.f );
}
inline Vector3b color_to_byte( const Vector3& color ){
	return Vector3b( color_to_byte( color.x() ), color_to_byte( color.y() ), color_to_byte( color.z() ) );
}
inline Color4b color_to_byte( const Color4f& color ){
	return Color4b( color_to_byte( color.rgb() ), color_to_byte( color.alpha() ) );
}



template<typename T>
T VectorNormalize( BasicVector3<T>& vector ) {
	const DoubleVector3 v( vector ); // intermediate vector to be sure to do in double
	const double length = vector3_length( v );

	if ( length == 0 ) {
		vector.set( 0 );
		return 0;
	}

	vector = v / length;

	return length;
}

template<typename T>
BasicVector3<T> VectorNormalized( const BasicVector3<T>& vector ) {
	BasicVector3<T> vec( vector );
	VectorNormalize( vec );
	return vec;
}

const float EQUAL_EPSILON = 0.001;

inline bool VectorCompare( const Vector3& v1, const Vector3& v2 ){
	return vector3_equal_epsilon( v1, v2, EQUAL_EPSILON );
}

inline bool VectorIsOnAxis( const Vector3& v ){
	int zeroComponentCount = 0;
	for ( int i = 0; i < 3; ++i )
	{
		if ( v[i] == 0.0 ) {
			zeroComponentCount++;
		}
	}

	return zeroComponentCount > 1; // The zero vector will be on axis.
}

/* (pitch yaw roll) -> (roll pitch yaw) */
inline Vector3 angles_pyr2rpy( const Vector3& angles ){
	return Vector3( angles.z(), angles.x(), angles.y() );
}

/*
   =====================
   PlaneFromPoints

   Returns false if the triangle is degenrate.
   The normal will point out of the clock for clockwise ordered points
   =====================
 */
template<typename P, typename V>
bool PlaneFromPoints( Plane3___<P>& plane, const BasicVector3<V>& p0, const BasicVector3<V>& p1, const BasicVector3<V>& p2 ) {
	plane.normal() = vector3_cross( p2 - p0, p1 - p0 );
	if ( VectorNormalize( plane.normal() ) == 0 ) {
		plane.dist() = 0;
		return false;
	}

	plane.dist() = vector3_dot( p0, plane.normal() );
	return true;
}

template<typename P, typename V>
bool PlaneFromPoints( Plane3___<P>& plane, const BasicVector3<V> planepts[3] ) {
	return PlaneFromPoints( plane, planepts[0], planepts[1], planepts[2] );
}


/*
   ComputeAxisBase()
   computes the base texture axis for brush primitive texturing
   note: ComputeAxisBase here and in editor code must always BE THE SAME!
   warning: special case behaviour of atan2( y, x ) <-> atan( y / x ) might not be the same everywhere when x == 0
   rotation by (0,RotY,RotZ) assigns X to normal
 */

template <typename Element, typename OtherElement>
inline void ComputeAxisBase( const BasicVector3<Element>& normal, BasicVector3<OtherElement>& texS, BasicVector3<OtherElement>& texT ){
#if 1
	const BasicVector3<Element> up( 0, 0, 1 );
	const BasicVector3<Element> down( 0, 0, -1 );

	if ( vector3_equal_epsilon( normal, up, Element(1e-6) ) ) {
		texS = BasicVector3<OtherElement>( 0, 1, 0 );
		texT = BasicVector3<OtherElement>( 1, 0, 0 );
	}
	else if ( vector3_equal_epsilon( normal, down, Element(1e-6) ) ) {
		texS = BasicVector3<OtherElement>( 0, 1, 0 );
		texT = BasicVector3<OtherElement>( -1, 0, 0 );
	}
	else
	{
		texS = vector3_normalised( vector3_cross( normal, up ) );
		texT = vector3_normalised( vector3_cross( normal, texS ) );
		vector3_negate( texS );
	}
#else
	/* do some cleaning */
	if ( fabs( normal[ 0 ] ) < 1e-6 ) {
		normal[ 0 ] = 0.0f;
	}
	if ( fabs( normal[ 1 ] ) < 1e-6 ) {
		normal[ 1 ] = 0.0f;
	}
	if ( fabs( normal[ 2 ] ) < 1e-6 ) {
		normal[ 2 ] = 0.0f;
	}

	/* compute the two rotations around y and z to rotate x to normal */
	const float RotY = -atan2( normal[ 2 ], sqrt( normal[ 1 ] * normal[ 1 ] + normal[ 0 ] * normal[ 0 ] ) );
	const float RotZ = atan2( normal[ 1 ], normal[ 0 ] );

	/* rotate (0,1,0) and (0,0,1) to compute texS and texT */
	texS[ 0 ] = -sin( RotZ );
	texS[ 1 ] = cos( RotZ );
	texS[ 2 ] = 0;

	/* the texT vector is along -z (t texture coorinates axis) */
	texT[ 0 ] = -sin( RotY ) * cos( RotZ );
	texT[ 1 ] = -sin( RotY ) * sin( RotZ );
	texT[ 2 ] = -cos( RotY );
#endif
}


/*
   ================
   MakeNormalVectors

   Given a normalized forward vector, create two
   other perpendicular vectors
   ================
 */
inline void MakeNormalVectors( const Vector3& forward, Vector3& right, Vector3& up ){
#if 0
	// this rotate and negate guarantees a vector
	// not colinear with the original
	//! fails with forward( -0.577350259 -0.577350259 0.577350259 )
	//! colinear right( 0.577350259 0.577350259 -0.577350259 )
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	right = VectorNormalized( right - forward * vector3_dot( right, forward ) );
	up = vector3_cross( right, forward );
#else
	right = VectorNormalized( vector3_cross( g_vector3_axes[ vector3_min_abs_component_index( forward ) ], forward ) );
	up = vector3_cross( right, forward );
#endif
}


/*
** NormalToLatLong
**
** We use two byte encoded normals in some space critical applications.
** Lat = 0 at (1,0,0) to 360 (-1,0,0), encoded in 8-bit sine table format
** Lng = 0 at (0,0,1) to 180 (0,0,-1), encoded in 8-bit sine table format
**
*/
inline void NormalToLatLong( const Vector3& normal, byte bytes[2] ) {
	// check for singularities
	if ( normal[0] == 0 && normal[1] == 0 ) {
		if ( normal[2] > 0 ) {
			bytes[0] = 0;
			bytes[1] = 0;       // lat = 0, long = 0
		}
		else {
			bytes[0] = 128;
			bytes[1] = 0;       // lat = 0, long = 128
		}
	}
	else {
		const int a = radians_to_degrees( atan2( normal[1], normal[0] ) ) * ( 255.0 / 360.0 );
		const int b = radians_to_degrees( acos( normal[2] ) ) * ( 255.0 / 360.0 );

		bytes[0] = b & 0xff;   // longitude
		bytes[1] = a & 0xff;   // latitude
	}
}

// plane types are used to speed some tests
// 0-2 are axial planes
enum EPlaneType : int
{
	ePlaneX = 0,
	ePlaneY = 1,
	ePlaneZ = 2,
	ePlaneNonAxial = 3
};

inline EPlaneType PlaneTypeForNormal( const Vector3& normal ) {
	if ( normal[0] == 1.0 || normal[0] == -1.0 ) {
		return ePlaneX;
	}
	if ( normal[1] == 1.0 || normal[1] == -1.0 ) {
		return ePlaneY;
	}
	if ( normal[2] == 1.0 || normal[2] == -1.0 ) {
		return ePlaneZ;
	}

	return ePlaneNonAxial;
}


inline void ColorNormalize( Vector3& color ) {
	const float max = vector3_max_component( color );

	if ( max == 0 ) {
		color.set( 1 );
	}
	else{
		color *= ( 1.f / max );
	}
}


inline double angle_squared_sin( const Vector3& a, const Vector3& b, const Vector3& c ){
	const Vector3 d1 = b - a;
	const Vector3 d2 = c - a;
	const Vector3 normal = vector3_cross( d2, d1 );
	/* https://en.wikipedia.org/wiki/Cross_product#Geometric_meaning
		cross( a, b ).length = a.length b.length sin( angle ) */
	const double lengthsSquared = vector3_length_squared( d1 ) * vector3_length_squared( d2 );
	return lengthsSquared == 0? 0 : ( vector3_length_squared( normal ) / lengthsSquared );
}

inline double triangle_min_angle_squared_sin( const Vector3& a, const Vector3& b, const Vector3& c ){
	const Vector3 d[3] = { b - a, c - a, c - b };
	const double l[3] = { vector3_length_squared( d[0] ), vector3_length_squared( d[1] ), vector3_length_squared( d[2] ) };
	const size_t mini = ( l[0] < l[1] ) ? ( ( l[0] < l[2] ) ? 0 : 2 ) : ( ( l[1] < l[2] ) ? 1 : 2 );
	if( l[mini] == 0 )
		return 0;

	const size_t minj = mini == 2? 0 : mini + 1;
	const size_t mink = minj == 2? 0 : minj + 1;
	return vector3_length_squared( vector3_cross( d[minj], d[mink] ) ) / ( l[minj] * l[mink] );
}


inline double triangle_area2x( const Vector3& a, const Vector3& b, const Vector3& c ){
	return vector3_length( vector3_cross( b - a, c - a ) );
}

#pragma once

#include "qmath.h"


const Vector3 c_spatial_sort_direction( 0.786868, 0.316861, 0.529564 );
const float c_spatial_EQUAL_EPSILON = EQUAL_EPSILON * 2;

inline float spatial_distance( const Vector3& point ){
	return vector3_dot( c_spatial_sort_direction, point );
}


struct MinMax1D
{
	float min, max;
	MinMax1D() : min( std::numeric_limits<float>::max() ), max( std::numeric_limits<float>::lowest() ){}
	void extend( float val ){
		value_minimize( min, val );
		value_maximize( max, val );
	}
};

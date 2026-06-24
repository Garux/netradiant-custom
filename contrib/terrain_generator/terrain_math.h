#pragma once

#include <cmath>

// Named Vec3d (double precision) to avoid conflict with Radiant's Vec3d (float)
struct Vec3d
{
	double x, y, z;

	Vec3d() : x( 0 ), y( 0 ), z( 0 ) {}
	Vec3d( double x, double y, double z ) : x( x ), y( y ), z( z ) {}

	Vec3d operator+( const Vec3d& b ) const { return { x + b.x, y + b.y, z + b.z }; }
	Vec3d operator-( const Vec3d& b ) const { return { x - b.x, y - b.y, z - b.z }; }
	Vec3d operator*( double d )        const { return { x * d,   y * d,   z * d   }; }
	Vec3d operator/( double d )        const { return { x / d,   y / d,   z / d   }; }

	static double dot( const Vec3d& a, const Vec3d& b ){
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
	static Vec3d cross( const Vec3d& a, const Vec3d& b ){
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}
};

struct BrushData
{
	double min_x, max_x;
	double min_y, max_y;
	double min_z, max_z;
	double width_x, length_y, height_z;
};

class Plane
{
public:
	Vec3d normal;
	double  d;

	Plane( const Vec3d& p1, const Vec3d& p2, const Vec3d& p3 ){
		Vec3d v1 = p1 - p2;
		Vec3d v2 = p3 - p2;
		normal = Vec3d::cross( v1, v2 );
		double len = std::sqrt( Vec3d::dot( normal, normal ) );
		if ( len > 1e-10 ) normal = normal / len;
		d = Vec3d::dot( normal, p1 );
	}

	double distance_to_point( const Vec3d& p ) const {
		return Vec3d::dot( normal, p ) - d;
	}

	static bool try_get_intersection( const Plane& p1, const Plane& p2, const Plane& p3, Vec3d& out ){
		double det = Vec3d::dot( p1.normal, Vec3d::cross( p2.normal, p3.normal ) );
		if ( std::abs( det ) < 0.0001 ) return false;
		Vec3d v1 = Vec3d::cross( p2.normal, p3.normal ) * p1.d;
		Vec3d v2 = Vec3d::cross( p3.normal, p1.normal ) * p2.d;
		Vec3d v3 = Vec3d::cross( p1.normal, p2.normal ) * p3.d;
		out = ( v1 + v2 + v3 ) / det;
		return true;
	}
};

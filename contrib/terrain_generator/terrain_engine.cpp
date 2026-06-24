#include "terrain_engine.h"
#include "noise.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <numbers>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static double round2( double v ){
	return std::round( v * 100.0 ) / 100.0;
}

static double random_double(){
	return (double)std::rand() / (double)RAND_MAX;
}

static double sample_noise( NoiseType noise_type, double x, double y ){
	switch ( noise_type ) {
	case NoiseType::Perlin:  return Perlin::noise( x, y );
	case NoiseType::Simplex: return Simplex::noise( x, y );
	default:                 return random_double() * 2.0 - 1.0;
	}
}

// ---------------------------------------------------------------------------

BrushData make_manual_brush_data( double width, double length, double height ){
	BrushData b;
	b.min_x   = -width  / 2.0;
	b.max_x   =  width  / 2.0;
	b.min_y   = -length / 2.0;
	b.max_y   =  length / 2.0;
	b.min_z   = 0.0;
	b.max_z   = height;
	b.width_x  = width;
	b.length_y = length;
	b.height_z = height;
	return b;
}

void adjust_bounds_to_fit_grid( BrushData& target, double step_x, double step_y ){
	double new_width  = std::max( step_x, std::round( target.width_x  / step_x ) * step_x );
	double new_length = std::max( step_y, std::round( target.length_y / step_y ) * step_y );

	if ( std::abs( target.width_x  - new_width  ) > 0.001 ||
	     std::abs( target.length_y - new_length ) > 0.001 ) {
		double diff_x = new_width  - target.width_x;
		double diff_y = new_length - target.length_y;

		target.min_x   = std::round( target.min_x - diff_x / 2.0 );
		target.max_x   = target.min_x + new_width;
		target.min_y   = std::round( target.min_y - diff_y / 2.0 );
		target.max_y   = target.min_y + new_length;
		target.width_x  = new_width;
		target.length_y = new_length;
	}
}

// ---------------------------------------------------------------------------
// Standard heightmap
// ---------------------------------------------------------------------------

HeightMap generate_height_map( const BrushData& target, double step_x, double step_y,
                                ShapeType shape_type, double shape_height,
                                double variance, double frequency,
                                NoiseType noise_type, double terrace_step ){
	HeightMap height_map;

	double seed_x = random_double() * 10000.0;
	double seed_y = random_double() * 10000.0;

	for ( double x = target.min_x; x <= target.max_x + 0.01; x += step_x ) {
		for ( double y = target.min_y; y <= target.max_y + 0.01; y += step_y ) {
			double nx = target.width_x  > 0 ? ( x - target.min_x ) / target.width_x  : 0.0;
			double ny = target.length_y > 0 ? ( y - target.min_y ) / target.length_y : 0.0;

			double center_dist = std::min( 1.0, std::sqrt(
				( nx - 0.5 ) * ( nx - 0.5 ) + ( ny - 0.5 ) * ( ny - 0.5 ) ) / 0.5 );

			double base_z = 0.0;
			switch ( shape_type ) {
			case ShapeType::Hill: {
				base_z = shape_height * 0.5 * ( 1.0 + std::cos( center_dist * std::numbers::pi ) );
				break;
			}
			case ShapeType::Crater: {
				base_z = shape_height * 0.5 * ( 1.0 - std::cos( center_dist * std::numbers::pi ) );
				break;
			}
			case ShapeType::Ridge: {
				double dist = std::min( 1.0, std::abs( nx - 0.5 ) / 0.5 );
				base_z = shape_height * 0.5 * ( 1.0 + std::cos( dist * std::numbers::pi ) );
				break;
			}
			case ShapeType::Slope: {
				base_z = shape_height * nx;
				break;
			}
			case ShapeType::Volcano: {
				double mountain   = shape_height * 0.5 * ( 1.0 + std::cos( center_dist * std::numbers::pi ) );
				double crater_dist = std::min( 1.0, center_dist / 0.35 );
				double crater     = ( shape_height * 0.7 ) * 0.5 * ( 1.0 + std::cos( crater_dist * std::numbers::pi ) );
				base_z = mountain - crater;
				break;
			}
			case ShapeType::Valley: {
				double dist = std::min( 1.0, std::abs( nx - 0.5 ) / 0.5 );
				base_z = shape_height * 0.5 * ( 1.0 - std::cos( dist * std::numbers::pi ) );
				break;
			}
			default:
				break;
			}

			double noise_z = 0.0;
			if ( variance > 0.0 ) {
				if ( noise_type == NoiseType::Random ) {
					noise_z = ( random_double() * ( variance * 2.0 ) ) - variance;
				} else {
					noise_z = sample_noise( noise_type,
					                        ( x + seed_x ) * frequency,
					                        ( y + seed_y ) * frequency ) * variance;
				}
			}

			double final_z = target.max_z + base_z + noise_z;
			if ( shape_type != ShapeType::Flat && terrace_step > 0.0 )
				final_z = std::floor( final_z / terrace_step ) * terrace_step;

			height_map[{ round2( x ), round2( y ) }] = std::round( final_z );
		}
	}

	return height_map;
}

// ---------------------------------------------------------------------------
// Tunnel heightmaps
// ---------------------------------------------------------------------------

TunnelMaps generate_tunnel_height_maps( const BrushData& target, double step_x, double step_y,
                                        double cave_height, double slope_height,
                                        double variance, double frequency,
                                        NoiseType noise_type, double terrace_step ){
	TunnelMaps result;

	double seed_floor_x = random_double() * 10000.0;
	double seed_floor_y = random_double() * 10000.0;
	double seed_ceil_x  = random_double() * 10000.0;
	double seed_ceil_y  = random_double() * 10000.0;
	double seed_wall_l  = random_double() * 10000.0;
	double seed_wall_r  = random_double() * 10000.0;

	double center_x  = ( target.min_x + target.max_x ) / 2.0;
	double half_width = target.width_x / 2.0;

	// Floor and ceiling
	for ( double x = target.min_x; x <= target.max_x + 0.01; x += step_x ) {
		for ( double y = target.min_y; y <= target.max_y + 0.01; y += step_y ) {
			double t = half_width > 0 ? std::min( 1.0, std::abs( x - center_x ) / half_width ) : 0.0;
			double blend = 1.0 - std::sqrt( std::max( 0.0, 1.0 - t * t ) );

			double floor_noise = 0.0, ceil_noise = 0.0;
			if ( variance > 0.0 ) {
				if ( noise_type == NoiseType::Random ) {
					floor_noise = random_double() * variance;
					ceil_noise  = random_double() * variance;
				} else {
					floor_noise = std::abs( sample_noise( noise_type,
					    ( x + seed_floor_x ) * frequency, ( y + seed_floor_y ) * frequency ) ) * variance;
					ceil_noise  = std::abs( sample_noise( noise_type,
					    ( x + seed_ceil_x  ) * frequency, ( y + seed_ceil_y  ) * frequency ) ) * variance;
				}
			}

			double ny    = target.length_y > 0 ? ( y - target.min_y ) / target.length_y : 0.0;
			double base_z = target.max_z + slope_height * ny;
			double floor_z = base_z + blend * ( cave_height * 0.25 ) + floor_noise;
			double ceil_z  = base_z + cave_height - blend * ( cave_height * 0.25 ) - ceil_noise;

			if ( floor_z > ceil_z ) {
				double mid = ( floor_z + ceil_z ) / 2.0;
				floor_z = mid;
				ceil_z  = mid;
			}

			if ( terrace_step > 0.0 ) {
				floor_z = std::floor( floor_z / terrace_step ) * terrace_step;
				ceil_z  = std::ceil(  ceil_z  / terrace_step ) * terrace_step;
			}

			result.floor_map[   { round2( x ), round2( y ) }] = std::round( floor_z );
			result.ceiling_map[ { round2( x ), round2( y ) }] = std::round( ceil_z  );
		}
	}

	// Wall step in Z — walls must cover the full slope range regardless of direction.
	// slope_height may be negative (downward slope), so use abs for the span.
	double total_wall_height = cave_height + std::abs( slope_height );
	int    num_z_steps = std::max( 1, (int)std::round( total_wall_height / step_x ) );
	double step_z      = total_wall_height / num_z_steps;
	double wall_min_z  = target.max_z + std::min( 0.0, slope_height );
	double wall_max_z  = wall_min_z + total_wall_height;
	result.step_z      = step_z;

	// Left and right walls — grid over (Y, Z)
	for ( double y = target.min_y; y <= target.max_y + 0.01; y += step_y ) {
		for ( double z = wall_min_z; z <= wall_max_z + 0.01; z += step_z ) {
			double ry = round2( y );
			double rz = round2( z );

			double wall_noise = 0.0;
			if ( variance > 0.0 ) {
				if ( noise_type == NoiseType::Random ) {
					wall_noise = random_double() * variance;
				} else {
					wall_noise = std::abs( sample_noise( noise_type,
					    ( y + seed_wall_l ) * frequency, ( z + seed_wall_l ) * frequency ) ) * variance;
				}
			}
			result.left_wall_map[{ ry, rz }] = std::round( target.min_x + wall_noise );

			wall_noise = 0.0;
			if ( variance > 0.0 ) {
				if ( noise_type == NoiseType::Random ) {
					wall_noise = random_double() * variance;
				} else {
					wall_noise = std::abs( sample_noise( noise_type,
					    ( y + seed_wall_r ) * frequency, ( z + seed_wall_r ) * frequency ) ) * variance;
				}
			}
			result.right_wall_map[{ ry, rz }] = std::round( target.max_x - wall_noise );
		}
	}

	return result;
}

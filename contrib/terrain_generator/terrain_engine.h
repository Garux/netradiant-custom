#pragma once

#include "terrain_math.h"

#include <map>
#include <utility>
#include <tuple>

using HeightMap   = std::map<std::pair<double, double>, double>;
using WallMap     = std::map<std::pair<double, double>, double>;

struct TunnelMaps
{
	HeightMap floor_map;
	HeightMap ceiling_map;
	WallMap   left_wall_map;
	WallMap   right_wall_map;
	double    step_z;
};

enum class ShapeType {
	Flat        = 0,
	Hill        = 1,
	Crater      = 2,
	Ridge       = 3,
	Slope       = 4,
	Volcano     = 5,
	Valley      = 6,
	Tunnel      = 7,
	SlopeTunnel = 8
};

enum class NoiseType {
	Perlin  = 0,
	Simplex = 1,
	Random  = 2
};

BrushData make_manual_brush_data( double width, double length, double height );

void adjust_bounds_to_fit_grid( BrushData& target, double step_x, double step_y );

HeightMap generate_height_map( const BrushData& target, double step_x, double step_y,
                                ShapeType shape_type, double shape_height,
                                double variance, double frequency,
                                NoiseType noise_type, double terrace_step );

TunnelMaps generate_tunnel_height_maps( const BrushData& target, double step_x, double step_y,
                                        double cave_height, double slope_height,
                                        double variance, double frequency,
                                        NoiseType noise_type, double terrace_step );

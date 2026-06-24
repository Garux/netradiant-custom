#pragma once

#include "terrain_engine.h"

// Generates terrain brushes into func_group entities and inserts them into
// the scene graph. For standard terrain, one func_group is created.
// For tunnel terrain, four func_groups are created (floor, ceiling, left wall, right wall).
void build_terrain_brushes( const BrushData& target, double step_x, double step_y,
                             const HeightMap& height_map, const char* top_texture,
                             bool split_diagonally );

void build_tunnel_brushes( const BrushData& target, double step_x, double step_y,
                            const TunnelMaps& maps, const char* top_texture,
                            double cave_height, double slope_height );

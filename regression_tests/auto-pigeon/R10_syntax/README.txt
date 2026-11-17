R10_syntax — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R10.classic: classic brush syntax: geometry, texture mapping and material contents
  map: maps/r10_classic.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  One room, written three ways. Classic and Valve 220 describe the same nearest-axial mapping (Valve
  220 spells the axes out); brush primitives carry a texture matrix in each face's own basis
  (ComputeAxisBase). Each must compile to the same solids, with the water box CONTENTS_WATER and not
  solid, the clip box player clip and not drawn, the caulk face not drawn, and the box's texture
  coordinates exactly what its syntax describes.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [-96, 8, 64]: the textured box, classic
  - drawn [-64, 8, 40]: the textured box, classic
  - drawn [-100, -32, 24]: the textured box, classic
  - not_drawn [-96, 8, 0]: the box's caulk bottom: nodraw
  - solid [-96, 0, 32]: inside the textured box
  - contents [96, 0, 24]: inside the water box
  - contents [176, 0, 48]: inside the clip box
  - not_drawn [176, 0, 96]: the clip box's top: nodraw

R10.valve220: valve220 brush syntax: geometry, texture mapping and material contents
  map: maps/r10_valve220.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  One room, written three ways. Classic and Valve 220 describe the same nearest-axial mapping (Valve
  220 spells the axes out); brush primitives carry a texture matrix in each face's own basis
  (ComputeAxisBase). Each must compile to the same solids, with the water box CONTENTS_WATER and not
  solid, the clip box player clip and not drawn, the caulk face not drawn, and the box's texture
  coordinates exactly what its syntax describes.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [-96, 8, 64]: the textured box, valve220
  - drawn [-64, 8, 40]: the textured box, valve220
  - drawn [-100, -32, 24]: the textured box, valve220
  - not_drawn [-96, 8, 0]: the box's caulk bottom: nodraw
  - solid [-96, 0, 32]: inside the textured box
  - contents [96, 0, 24]: inside the water box
  - contents [176, 0, 48]: inside the clip box
  - not_drawn [176, 0, 96]: the clip box's top: nodraw

R10.brushprim: brushprim brush syntax: geometry, texture mapping and material contents
  map: maps/r10_brushprim.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  One room, written three ways. Classic and Valve 220 describe the same nearest-axial mapping (Valve
  220 spells the axes out); brush primitives carry a texture matrix in each face's own basis
  (ComputeAxisBase). Each must compile to the same solids, with the water box CONTENTS_WATER and not
  solid, the clip box player clip and not drawn, the caulk face not drawn, and the box's texture
  coordinates exactly what its syntax describes.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [-96, 8, 64]: the textured box, brushprim
  - drawn [-64, 8, 40]: the textured box, brushprim
  - drawn [-100, -32, 24]: the textured box, brushprim
  - not_drawn [-96, 8, 0]: the box's caulk bottom: nodraw
  - solid [-96, 0, 32]: inside the textured box
  - contents [96, 0, 24]: inside the water box
  - contents [176, 0, 48]: inside the clip box
  - not_drawn [176, 0, 96]: the clip box's top: nodraw

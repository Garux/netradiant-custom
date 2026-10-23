R09_patch — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R09.patch: a patch with authored UV checker beside an ordinary brush
  map: maps/r09_patch.map
  mechanism: none (control)
  the source is sealed
  stages: bsp + vis + light

  A 3 x 3 patchDef2 bowed over the floor, every control point with its own st, and a box with a
  non-default classic texture shift and scale. The BSP must keep the patch's control points and st
  exactly as authored (the engine tessellates them) and draw the box with the texture coordinates
  Q3Map2's nearest-axial projection gives for that shift and scale.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - patch : the authored control points and st, unchanged
  - drawn [96, 0, 64]: the box's top, classic projection
  - drawn [128, 8, 40]: the box's +x side
  - open [-64, -32, 64]: air above the patch

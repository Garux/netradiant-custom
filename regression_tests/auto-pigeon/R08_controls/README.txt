R08_controls — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R08.slit.0125: a 0.125 slot through a wall to the void, centred on a block split
  map: maps/r08_slit_0125.map
  mechanism: block_split
  the source leaks
  stages: bsp
  also: S01, S03

  The same slot 0.125 wide, centred on the block split y = 0. The source describes an opening, so the
  honest verdict is a leak, and the route must cross the slot. Upstream closed it: the forced block
  split consumed both slot faces lying within ON_EPSILON of it, and no leaf held the slot's air (the
  portal stage dropped nothing here). Repaired with S03/S05 (NEW_340A).


R08.touch: touch
  map: maps/r08_touch.map
  mechanism: none (control)
  the source is sealed
  stages: bsp
  also: G06

  Two boxes share the face x = 0, which is also a block split plane. Both stay solid and both tops
  drawn.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [-0.5, 0, 16]: box A at the shared face
  - solid [0.5, 0, 16]: box B at the shared face
  - drawn [-32, 0, 32]: top of A
  - drawn [32, 0, 32]: top of B
  - open [0, 0, 40]: air above the seam

R08.overlap: overlap
  map: maps/r08_overlap.map
  mechanism: none (control)
  the source is sealed
  stages: bsp
  also: G06

  Box B overlaps box A by 16 units and rises above it. The overlap is solid, A's top is drawn up to B,
  B's side is drawn above A.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [0, 0, 16]: inside both boxes
  - drawn [-32, 0, 32]: top of A
  - drawn [32, 0, 48]: top of B
  - drawn [-8, 0, 40]: B's -x face above A
  - open [-20, 0, 40]: air above A, beside B

R08.nearparallel: near-parallel
  map: maps/r08_nearparallel.map
  mechanism: none (control)
  the source is sealed
  stages: bsp
  also: G07

  Box A has a flat top at z = 32 for x in [-64, 0]; box B, from x = 0 to 64, has a top that starts at
  z = 32 at x = 0 and rises 0.2 degrees. The two top planes differ by 0.2 degrees and meet along x =
  0: a near-parallel pair of planes that are NOT the same plane. Both tops must be drawn.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [-32, 0, 32]: flat top of A
  - drawn [32, 0, 32.1117]: tilted top of B
  - solid [32, 0, 16]: inside B
  - open [32, 0, 40]: air above B
  - open [-32, 0, 40]: air above A

R08.zeroarea: zero-area
  map: maps/r08_zeroarea.map
  mechanism: none (control)
  the source is sealed
  stages: bsp
  also: G07

  A brush whose two x faces coincide encloses no volume. The compiler must not crash, must not seal or
  fill the room with it, and leaves its place air.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [32, 0, 32]: where the zero-thickness brush is: it has no volume
  - open [40, 0, 32]: air beside it

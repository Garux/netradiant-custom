R02_rod — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R02.rod.split: 0.25 x 0.25 x 24 rod across the y = 0 split
  map: maps/r02_rod_split.map
  mechanism: M1
  the source is sealed
  stages: bsp + vis + light

  The rod is 0.25 x 0.25 x 24 = 1.5 cubic units. Across y = 0 (a block split plane) it is cut into two
  halves of 0.75 cubic units each; upstream SplitBrush discards a fragment below 1 cubic unit, so both
  halves vanish from the tree: the leaves list no brush, the rod has no collision and the player walks
  through it. The uncut control (one fragment of 1.5) keeps it.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [64.125, 0.0625, 12]: inside the rod, +y half
  - solid [64.125, -0.0625, 12]: inside the rod, -y half
  - solid [64.125, 0.0625, 1]: inside the rod near its foot
  - solid [64.125, -0.0625, 23]: inside the rod near its top
  - open [64.125, 0.0, 30]: air 6 units above the rod
  - open [66, 0.0, 12]: air 1.75 units beside the rod
  - drawn [64.25, 0.0, 12]: the rod's +x face

R02.rod.uncut: the same rod moved 0.25 to +y, wholly on one side of y = 0
  map: maps/r02_rod_uncut.map
  mechanism: none (control)
  the source is sealed
  stages: bsp + vis + light

  Control: the same rod wholly on +y of the y = 0 split is never cut, so no fragment is discarded and
  every compiler keeps it solid.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [64.125, 0.3125, 12]: inside the rod, +y half
  - solid [64.125, 0.1875, 12]: inside the rod, -y half
  - solid [64.125, 0.3125, 1]: inside the rod near its foot
  - solid [64.125, 0.1875, 23]: inside the rod near its top
  - open [64.125, 0.25, 30]: air 6 units above the rod
  - open [66, 0.25, 12]: air 1.75 units beside the rod
  - drawn [64.25, 0.25, 12]: the rod's +x face

G02_G07_small_sections — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

G02.m3.rod_along: M3: a 3/16 rod lying along y across the y = 0 split
  map: maps/g02_rod_along_01875.map
  mechanism: M3
  the source is sealed
  stages: bsp

  The rod is 0.1875 x 0.1875 in section and runs from y = -4 to y = 8 across the block split y = 0.
  Its section by y = 0 has only two edges longer than 0.2, so upstream SplitBrush called the brush
  'not really split' and sent it whole to the side it reaches furthest into: the 4 units on -y kept no
  brush in their leaves (no collision, and contents read as air). The whole rod must be solid; a 1/4
  rod (four long edges) was always split and is the control.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [64.09375, -2, 8.09375]: the rod's -y part, 2 units from the split
  - solid [64.09375, -3.5, 8.09375]: the rod's -y part near its end
  - solid [64.09375, 4, 8.09375]: the rod's +y part, 4 units from the split
  - open [64.09375, -2, 12.09375]: air 4 above the rod's -y part
  - open [64.09375, -6, 8.09375]: air past the rod's -y end

G02.rod_along.025: control: the same rod 1/4 thick (its section is not small)
  map: maps/g02_rod_along_025.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  The rod is 0.25 x 0.25 in section and runs from y = -4 to y = 8 across the block split y = 0. Its
  section by y = 0 has only two edges longer than 0.2, so upstream SplitBrush called the brush 'not
  really split' and sent it whole to the side it reaches furthest into: the 4 units on -y kept no
  brush in their leaves (no collision, and contents read as air). The whole rod must be solid; a 1/4
  rod (four long edges) was always split and is the control.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [64.125, -2, 8.125]: the rod's -y part, 2 units from the split
  - solid [64.125, -3.5, 8.125]: the rod's -y part near its end
  - solid [64.125, 4, 8.125]: the rod's +y part, 4 units from the split
  - open [64.125, -2, 12.125]: air 4 above the rod's -y part
  - open [64.125, -6, 8.125]: air past the rod's -y end

G07.m3.cube.025: M3: a 1/4 cube across x = 0 and y = 0 (four fragments)
  map: maps/g07_cube_025.map
  mechanism: M3
  the source is sealed
  stages: bsp

  A cube of 0.25 centred on the two block split planes x = 0 and y = 0 is cut into four quarters of
  positive volume. After the first split each half's section by the second plane is 0.125 x 0.25;
  below 0.2 on a side, upstream sent each half whole to one side of the second plane and two quarters
  kept no brush. Every quarter must be solid.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [-0.0625, -0.0625, 8.125]: the cube's -x-y quarter
  - solid [-0.0625, 0.0625, 8.125]: the cube's -x+y quarter
  - solid [0.0625, -0.0625, 8.125]: the cube's +x-y quarter
  - solid [0.0625, 0.0625, 8.125]: the cube's +x+y quarter
  - open [0, 0, 9.25]: air 1 above the cube

G07.cube.05: control: a 1/2 cube across x = 0 and y = 0
  map: maps/g07_cube_05.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A cube of 0.5 centred on the two block split planes x = 0 and y = 0 is cut into four quarters of
  positive volume. After the first split each half's section by the second plane is 0.25 x 0.5; below
  0.2 on a side, upstream sent each half whole to one side of the second plane and two quarters kept
  no brush. Every quarter must be solid.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [-0.125, -0.125, 8.25]: the cube's -x-y quarter
  - solid [-0.125, 0.125, 8.25]: the cube's -x+y quarter
  - solid [0.125, -0.125, 8.25]: the cube's +x-y quarter
  - solid [0.125, 0.125, 8.25]: the cube's +x+y quarter
  - open [0, 0, 9.5]: air 1 above the cube

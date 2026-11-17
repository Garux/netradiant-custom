R03_crossing_face — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R03.cross.0148: a step whose top face crosses x = 0 by 0.148: between ON_EPSILON and CLIP_EPSILON*2
  map: maps/r03_cross_0148.map
  mechanism: M2
  the source is sealed
  stages: bsp

  x = 0 is a block split plane, so the step's top face (z = 32, x from -64 to 0.148) crosses it.
  BuildFaceTree_r classifies the face as crossing when its points lie more than ON_EPSILON (0.1) on
  both sides, then clips it with CLIP_EPSILON*2 (0.2): a part between 0.1 and 0.2 wide is classified
  crossing yet clips to nothing, so the whole face goes to the back. The front subtree then has no
  plane at z = 32, one leaf straddles the step's strip x in [0, 0.148] and the air above it, the
  step's fragment makes that leaf opaque, and the room air above the strip is filled. At 0.148 this
  happens; at 0.25 the clip keeps a front part, at 0.05 the face is not crossing, and both controls
  stay correct on every build.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [0.074, 0, 96]: air 64 above the strip of the step past x = 0
  - open [0.074, 0, 40]: air 8 above the strip of the step past x = 0
  - open [2, 0, 96]: air 2 units past the end of the step
  - solid [-32, 0, 16]: inside the step
  - drawn [-32, 0, 32]: the step's top face
  - drawn [0.148, 0, 16]: the step's end face

R03.cross.025: control: crossing by 0.25, past CLIP_EPSILON*2
  map: maps/r03_cross_025.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  x = 0 is a block split plane, so the step's top face (z = 32, x from -64 to 0.25) crosses it.
  BuildFaceTree_r classifies the face as crossing when its points lie more than ON_EPSILON (0.1) on
  both sides, then clips it with CLIP_EPSILON*2 (0.2): a part between 0.1 and 0.2 wide is classified
  crossing yet clips to nothing, so the whole face goes to the back. The front subtree then has no
  plane at z = 32, one leaf straddles the step's strip x in [0, 0.25] and the air above it, the step's
  fragment makes that leaf opaque, and the room air above the strip is filled. At 0.148 this happens;
  at 0.25 the clip keeps a front part, at 0.05 the face is not crossing, and both controls stay
  correct on every build.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [0.125, 0, 96]: air 64 above the strip of the step past x = 0
  - open [0.125, 0, 40]: air 8 above the strip of the step past x = 0
  - open [2, 0, 96]: air 2 units past the end of the step
  - solid [-32, 0, 16]: inside the step
  - drawn [-32, 0, 32]: the step's top face
  - drawn [0.25, 0, 16]: the step's end face

R03.cross.005: control: crossing by 0.05, inside ON_EPSILON
  map: maps/r03_cross_005.map
  mechanism: none (control)
  the source is sealed
  stages: bsp
  also: G04

  x = 0 is a block split plane, so the step's top face (z = 32, x from -64 to 0.05) crosses it.
  BuildFaceTree_r classifies the face as crossing when its points lie more than ON_EPSILON (0.1) on
  both sides, then clips it with CLIP_EPSILON*2 (0.2): a part between 0.1 and 0.2 wide is classified
  crossing yet clips to nothing, so the whole face goes to the back. The front subtree then has no
  plane at z = 32, one leaf straddles the step's strip x in [0, 0.05] and the air above it, the step's
  fragment makes that leaf opaque, and the room air above the strip is filled. At 0.148 this happens;
  at 0.25 the clip keeps a front part, at 0.05 the face is not crossing, and both controls stay
  correct on every build.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [0.025, 0, 96]: air 64 above the strip of the step past x = 0
  - open [0.025, 0, 40]: air 8 above the strip of the step past x = 0
  - open [2, 0, 96]: air 2 units past the end of the step
  - solid [-32, 0, 16]: inside the step
  - drawn [-32, 0, 32]: the step's top face
  - drawn [0.05, 0, 16]: the step's end face

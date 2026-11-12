G01_G05_near_parallel — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

G01.m4.inset: M4: an inset face 0.0885 behind a near-coplanar wall, its corners snapped further behind
  map: maps/g01_m4_inset.map
  mechanism: M4
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.78447508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.483050173072385, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.60016551429065, 64]: the west wall piece's face

G05.m4.phase0547: the inset at snap phase 0.0547 (the snap leaves it within 0.1)
  map: maps/g05_m4_phase0547.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0547), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.753275086536185, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.753275086536185, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.753275086536185, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.45185017307237, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.45185017307237, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.45185017307237, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.15042525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.15042525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.15042525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.45185017307237, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.568965514290646, 64]: the west wall piece's face

G05.m4.phase0625: the inset at snap phase 0.0625 (first phase the snap carries past 0.1)
  map: maps/g05_m4_phase0625.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0625), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.761075086536195, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.761075086536195, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.761075086536195, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.45965017307238, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.45965017307238, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.45965017307238, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.15822525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.15822525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.15822525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.45965017307238, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.576765514290656, 64]: the west wall piece's face

G05.m4.phase1094: the inset at snap phase 0.1094 (last such phase)
  map: maps/g05_m4_phase1094.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.1094), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.80797508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.80797508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.80797508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.50655017307238, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.50655017307238, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.50655017307238, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.20512525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.20512525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.20512525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.50655017307238, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.62366551429065, 64]: the west wall piece's face

G05.m4.phase1172: the inset at snap phase 0.1172 (the snap leaves it within 0.1)
  map: maps/g05_m4_phase1172.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.1172), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.81577508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.81577508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.81577508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.51435017307238, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.51435017307238, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.51435017307238, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.21292525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.21292525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.21292525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.51435017307238, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.63146551429065, 64]: the west wall piece's face

G05.m4.recess0050: the inset 0.05 behind the wall's plane
  map: maps/g05_m4_recess0050.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.05 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.78447508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.483050173072385, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.63870219266945, 64]: the west wall piece's face

G05.m4.recess0095: the inset 0.095 behind the wall's plane
  map: maps/g05_m4_recess0095.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.095 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.78447508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.483050173072385, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.593659321837094, 64]: the west wall piece's face

G05.m4.recess0110: the inset 0.11 behind: past ON_EPSILON, a plane of its own
  map: maps/g05_m4_recess0110.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.11 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.78447508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.483050173072385, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.578645031559645, 64]: the west wall piece's face

G05.m4.proud0885: the inset 0.0885 in FRONT of the wall's plane
  map: maps/g05_m4_proud0885.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face -0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [112.0, 61.78447508653619, 48]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 64]: the inset's face, seen from the room
  - drawn [112.0, 61.78447508653619, 80]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 48]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 64]: the inset's face, seen from the room
  - drawn [128.0, 62.483050173072385, 80]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 48]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 64]: the inset's face, seen from the room
  - drawn [144.0, 63.18162525960857, 80]: the inset's face, seen from the room
  - solid [128.0, 100.0, 64]: inside the inset
  - open [128.0, 61.483050173072385, 64]: air 1 unit in front of the inset
  - drawn [64.0, 59.777334139564566, 64]: the west wall piece's face

G05.m4.moved: G01 moved by (-37.3, -21.45, 8.2)
  map: maps/g05_m4_moved.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [74.7, 40.33447508653619, 56.2]: the inset's face, seen from the room
  - drawn [74.7, 40.33447508653619, 72.2]: the inset's face, seen from the room
  - drawn [74.7, 40.33447508653619, 88.2]: the inset's face, seen from the room
  - drawn [90.7, 41.03305017307238, 56.2]: the inset's face, seen from the room
  - drawn [90.7, 41.03305017307238, 72.2]: the inset's face, seen from the room
  - drawn [90.7, 41.03305017307238, 88.2]: the inset's face, seen from the room
  - drawn [106.7, 41.73162525960858, 56.2]: the inset's face, seen from the room
  - drawn [106.7, 41.73162525960858, 72.2]: the inset's face, seen from the room
  - drawn [106.7, 41.73162525960858, 88.2]: the inset's face, seen from the room
  - solid [90.7, 78.55, 72.2]: inside the inset
  - open [90.7, 40.03305017307238, 72.2]: air 1 unit in front of the inset
  - drawn [26.700000000000003, 38.15016551429065, 72.2]: the west wall piece's face

G05.m4.turn15: G01 turned 15 degrees about z
  map: maps/g05_m4_turn15.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [120.36554008474903, 58.67293940956421, 48]: the inset's face, seen from the room
  - drawn [120.36554008474903, 58.67293940956421, 64]: the inset's face, seen from the room
  - drawn [120.36554008474903, 58.67293940956421, 80]: the inset's face, seen from the room
  - drawn [135.63954876854442, 63.488815848891974, 48]: the inset's face, seen from the room
  - drawn [135.63954876854442, 63.488815848891974, 64]: the inset's face, seen from the room
  - drawn [135.63954876854442, 63.488815848891974, 80]: the inset's face, seen from the room
  - drawn [150.9135574523398, 68.30469228821973, 48]: the inset's face, seen from the room
  - drawn [150.9135574523398, 68.30469228821973, 64]: the inset's face, seen from the room
  - drawn [150.9135574523398, 68.30469228821973, 80]: the inset's face, seen from the room
  - solid [125.92944763917984, 99.72740661031254, 64]: inside the inset
  - open [135.89836781364693, 62.52289002260291, 64]: air 1 unit in front of the inset
  - drawn [74.56644134057065, 44.139744216200825, 64]: the west wall piece's face

G05.m4.turn45: G01 turned 45 degrees about z
  map: maps/g05_m4_turn45.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [138.05189406443657, 59.3206889375939, 48]: the inset's face, seen from the room
  - drawn [138.05189406443657, 59.3206889375939, 64]: the inset's face, seen from the room
  - drawn [138.05189406443657, 59.3206889375939, 80]: the inset's face, seen from the room
  - drawn [148.8716353825636, 71.12836461743639, 48]: the inset's face, seen from the room
  - drawn [148.8716353825636, 71.12836461743639, 64]: the inset's face, seen from the room
  - drawn [148.8716353825636, 71.12836461743639, 80]: the inset's face, seen from the room
  - drawn [159.69137670069065, 82.93604029727888, 48]: the inset's face, seen from the room
  - drawn [159.69137670069065, 82.93604029727888, 64]: the inset's face, seen from the room
  - drawn [159.69137670069065, 82.93604029727888, 80]: the inset's face, seen from the room
  - solid [122.34314575050762, 97.65685424949238, 64]: inside the inset
  - open [149.57874216375015, 70.42125783624985, 64]: air 1 unit in front of the inset
  - drawn [105.6553086782278, 23.835023329894124, 64]: the west wall piece's face

G05.m4.turn15.moved: G01 turned 15 degrees and moved
  map: maps/g05_m4_turn15_moved.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [83.06554008474903, 37.222939409564205, 56.2]: the inset's face, seen from the room
  - drawn [83.06554008474903, 37.222939409564205, 72.2]: the inset's face, seen from the room
  - drawn [83.06554008474903, 37.222939409564205, 88.2]: the inset's face, seen from the room
  - drawn [98.33954876854442, 42.03881584889197, 56.2]: the inset's face, seen from the room
  - drawn [98.33954876854442, 42.03881584889197, 72.2]: the inset's face, seen from the room
  - drawn [98.33954876854442, 42.03881584889197, 88.2]: the inset's face, seen from the room
  - drawn [113.6135574523398, 46.854692288219724, 56.2]: the inset's face, seen from the room
  - drawn [113.6135574523398, 46.854692288219724, 72.2]: the inset's face, seen from the room
  - drawn [113.6135574523398, 46.854692288219724, 88.2]: the inset's face, seen from the room
  - solid [88.62944763917984, 78.27740661031254, 72.2]: inside the inset
  - open [98.59836781364693, 41.072890022602905, 72.2]: air 1 unit in front of the inset
  - drawn [37.26644134057065, 22.689744216200825, 72.2]: the west wall piece's face

G05.m4.turn45.moved: G01 turned 45 degrees and moved
  map: maps/g05_m4_turn45_moved.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Four wall pieces share one face plane, tilted 2.5 degrees from the y axis; an inset piece B between
  them has a parallel face 0.0885 behind that plane. When the visible-face tree splits on the wall's
  plane, B's face is within ON_EPSILON (0.1) of it and is consumed there: B's own plane never splits
  the leaves behind. B's drawn triangles are then filtered with their 1/8-snapped vertices; when the
  snap carries every vertex more than 0.1 behind the wall's plane (B's corner phase 0.0859), the
  triangles reach only the opaque leaf behind it, no visible leaf references the surface and it is not
  emitted. The face must be drawn at every phase, offset and orientation.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - drawn [100.75189406443657, 37.8706889375939, 56.2]: the inset's face, seen from the room
  - drawn [100.75189406443657, 37.8706889375939, 72.2]: the inset's face, seen from the room
  - drawn [100.75189406443657, 37.8706889375939, 88.2]: the inset's face, seen from the room
  - drawn [111.5716353825636, 49.67836461743639, 56.2]: the inset's face, seen from the room
  - drawn [111.5716353825636, 49.67836461743639, 72.2]: the inset's face, seen from the room
  - drawn [111.5716353825636, 49.67836461743639, 88.2]: the inset's face, seen from the room
  - drawn [122.39137670069066, 61.486040297278876, 56.2]: the inset's face, seen from the room
  - drawn [122.39137670069066, 61.486040297278876, 72.2]: the inset's face, seen from the room
  - drawn [122.39137670069066, 61.486040297278876, 88.2]: the inset's face, seen from the room
  - solid [85.04314575050762, 76.20685424949238, 72.2]: inside the inset
  - open [112.27874216375015, 48.97125783624985, 72.2]: air 1 unit in front of the inset
  - drawn [68.3553086782278, 2.385023329894125, 72.2]: the west wall piece's face

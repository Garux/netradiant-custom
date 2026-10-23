G06_openings — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

G06.slit.1: a reachable slit 1 wide between two blocks inside the room
  map: maps/g06_slit_1.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Two blocks 1 apart inside the sealed room; the slit is open to the room above. Its air must stay
  open and both faces in it drawn: no closure of a reachable slit, no fill.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [96.5, 0, 32]: air in the slit
  - solid [64, 0, 32]: the west block
  - solid [129.0, 0, 32]: the east block
  - drawn [96, 0, 32]: the west block's face in the slit
  - drawn [97.0, 0, 32]: the east block's face in the slit

G06.slit.05: a reachable slit 0.5 wide between two blocks inside the room
  map: maps/g06_slit_05.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Two blocks 0.5 apart inside the sealed room; the slit is open to the room above. Its air must stay
  open and both faces in it drawn: no closure of a reachable slit, no fill.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [96.25, 0, 32]: air in the slit
  - solid [64, 0, 32]: the west block
  - solid [128.5, 0, 32]: the east block
  - drawn [96, 0, 32]: the west block's face in the slit
  - drawn [96.5, 0, 32]: the east block's face in the slit

G06.pocket: an enclosed pocket: a hollow block sealed from the room
  map: maps/g06_pocket.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Six 8-unit walls around 80 x 80 x 80 of air no entity can reach. The map is sealed, the walls solid
  and the room around them open. Nothing is asserted about the pocket's own air: it is unreachable,
  and Q3Map2 may fill it; a player-sized passage is the slit case, not this one.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [52, 0, 48]: the pocket's -x wall
  - drawn [144, 0, 48]: the block's outside face
  - open [152, 0, 48]: room air beside the block

G06.outer.025: a 0.25 slot to the void at y = 100: the narrowest opening Q3Map2 keeps
  map: maps/g06_outer_025.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.25 wide through the +x wall, centred on y = 100 (no split plane through it).
  The map leaks. Portals with fewer than three edges longer than 0.2 are dropped (portals.cpp,
  EDGE_LENGTH), so 0.25 is the narrowest opening measured to stay open; R08.slit.0125 is the known
  failure below that limit.

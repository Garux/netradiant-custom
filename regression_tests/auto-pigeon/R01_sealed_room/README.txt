R01_sealed_room — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R01.room: sealed room baseline
  map: maps/r01_room.map
  mechanism: none (control)
  the source is sealed
  stages: bsp + vis + light

  Six 16-unit walls around 512 x 512 x 256 of air with a player start and a light. Every compiler must
  seal it, keep its air open, its walls solid and its six inner faces drawn. It is the positive
  control the other cases are variations of.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [0, 0, 128]: the middle of the room, on both block split planes

P02_P05_portal_controls — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

P02.mitre.overlap: control: two boards of a wall overlap at a near-parallel seam: closed
  map: maps/p02_overlap.map
  mechanism: none (control)
  the source is sealed
  stages: bsp + vis + light

  The seam faces are near-parallel and the boards overlap 0.12 to 0.17 along it. Nothing is open.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P02.mitre.shared: control: two boards share one tilted face: closed
  map: maps/p02_shared.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  One seam plane, 0.09 degrees off the axis, shared by both boards.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P02.mitre.open_014: a real near-parallel seam, closed at the floor and 0.14 wide at the ceiling, leaks
  map: maps/p02_open_014.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The opening traced on a real map: seam faces a fraction of a degree apart, opening with height. A
  slot this shape is real, so the map leaks through the seam's wide end. Upstream drops the portals
  and reads it sealed; that disagreement is the fork being right. The route must cross the seam.


P02.mitre.open_02: a real near-parallel seam, closed at the floor and 0.2 wide at the ceiling, leaks
  map: maps/p02_open_02.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The opening traced on a real map: seam faces a fraction of a degree apart, opening with height. A
  slot this shape is real, so the map leaks through the seam's wide end. Upstream drops the portals
  and reads it sealed; that disagreement is the fork being right. The route must cross the seam.


P03.needle.closed_00005: a sealed room with a needle portal 0.0005 wide stays sealed
  map: maps/p03_closed_00005.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A portal 240 long is cut to a needle around the degeneracy tolerance (portals.cpp, about 0.001 plus
  float rounding). A rejected piece folds the whole portal into the other child: if a live piece past
  the fold could join leaves that do not touch, as EricW's folded portal did at 0.12, this sealed room
  would leak.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P03.needle.closed_0001: a sealed room with a needle portal 0.001 wide stays sealed
  map: maps/p03_closed_0001.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A portal 240 long is cut to a needle around the degeneracy tolerance (portals.cpp, about 0.001 plus
  float rounding). A rejected piece folds the whole portal into the other child: if a live piece past
  the fold could join leaves that do not touch, as EricW's folded portal did at 0.12, this sealed room
  would leak.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P03.needle.closed_00015: a sealed room with a needle portal 0.0015 wide stays sealed
  map: maps/p03_closed_00015.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A portal 240 long is cut to a needle around the degeneracy tolerance (portals.cpp, about 0.001 plus
  float rounding). A rejected piece folds the whole portal into the other child: if a live piece past
  the fold could join leaves that do not touch, as EricW's folded portal did at 0.12, this sealed room
  would leak.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P03.needle.closed_0002: a sealed room with a needle portal 0.002 wide stays sealed
  map: maps/p03_closed_0002.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A portal 240 long is cut to a needle around the degeneracy tolerance (portals.cpp, about 0.001 plus
  float rounding). A rejected piece folds the whole portal into the other child: if a live piece past
  the fold could join leaves that do not touch, as EricW's folded portal did at 0.12, this sealed room
  would leak.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P03.needle.closed_0003: a sealed room with a needle portal 0.003 wide stays sealed
  map: maps/p03_closed_0003.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A portal 240 long is cut to a needle around the degeneracy tolerance (portals.cpp, about 0.001 plus
  float rounding). A rejected piece folds the whole portal into the other child: if a live piece past
  the fold could join leaves that do not touch, as EricW's folded portal did at 0.12, this sealed room
  would leak.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P03.needle.closed_005: a sealed room with a needle portal 0.05 wide stays sealed
  map: maps/p03_closed_005.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A portal 240 long is cut to a needle around the degeneracy tolerance (portals.cpp, about 0.001 plus
  float rounding). A rejected piece folds the whole portal into the other child: if a live piece past
  the fold could join leaves that do not touch, as EricW's folded portal did at 0.12, this sealed room
  would leak.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall

P03.needle.open_0001: the same needle 0.001 wide in a room with a real slot: the slot leaks
  map: maps/p03_open_0001.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The negative control of the sealed needles: the leak goes through the real slot.


P03.needle.open_0002: the same needle 0.002 wide in a room with a real slot: the slot leaks
  map: maps/p03_open_0002.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The negative control of the sealed needles: the leak goes through the real slot.


P04.degenerate.repeated: a floor side with two equal points: the outcome is a leak, never a sealed pass
  map: maps/p04_repeated.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The side makes no plane, so the brush cannot enclose anything and the room is open. The compiler
  must finish and report that leak: an input it cannot use must not read sealed. Upstream's base
  crashes on the repeated, collinear and huge forms.


P04.degenerate.collinear: a floor side with three collinear points: the outcome is a leak, never a sealed pass
  map: maps/p04_collinear.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The side makes no plane, so the brush cannot enclose anything and the room is open. The compiler
  must finish and report that leak: an input it cannot use must not read sealed. Upstream's base
  crashes on the repeated, collinear and huge forms.


P04.degenerate.nan: a floor side with a coordinate that is not a number: the outcome is a leak, never a sealed pass
  map: maps/p04_nan.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The side makes no plane, so the brush cannot enclose anything and the room is open. The compiler
  must finish and report that leak: an input it cannot use must not read sealed. Upstream's base
  crashes on the repeated, collinear and huge forms.


P04.degenerate.inf: a floor side with an infinite coordinate: the outcome is a leak, never a sealed pass
  map: maps/p04_inf.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The side makes no plane, so the brush cannot enclose anything and the room is open. The compiler
  must finish and report that leak: an input it cannot use must not read sealed. Upstream's base
  crashes on the repeated, collinear and huge forms.


P04.degenerate.huge: a floor side with a coordinate far past any world: the outcome is a leak, never a sealed pass
  map: maps/p04_huge.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The side makes no plane, so the brush cannot enclose anything and the room is open. The compiler
  must finish and report that leak: an input it cannot use must not read sealed. Upstream's base
  crashes on the repeated, collinear and huge forms.


P04.degenerate.garbage: a floor side with text where a number belongs: the outcome is a leak, never a sealed pass
  map: maps/p04_garbage.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The side makes no plane, so the brush cannot enclose anything and the room is open. The compiler
  must finish and report that leak: an input it cannot use must not read sealed. Upstream's base
  crashes on the repeated, collinear and huge forms.


P05.wedge.through_wall: two near-parallel hint planes cross a wall: closed, read as leaking
  map: maps/p05_through_wall.map
  mechanism: sliver
  the source is sealed
  stages: bsp

  The planes are 0.05 to 0.08 apart inside the wall. The sliver mechanism of P01 on the smallest map
  that shows it, except that here no plane of the wall bounds the wedge: the wall's faces, reaching
  less than 0.1 past the second plane, do not enter it either, so one leaf runs from the room through
  the wall to the void.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -100, 128]: inside the +x wall
  - solid [264, 90, 128]: inside the +x wall
  - solid [264, 110, 128]: inside the +x wall
  - solid [264, 100, 128]: the wall, between the two planes

P05.wedge.slot_through: the same wedge with a real slot through the wedge: the slot leaks
  map: maps/p05_slot_through.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The negative control of the repair for P01 and P05, where it works. A brush is only ever taken for
  solid where it is: the slot is not brush, so it stays open.


P05.wedge.slot_beside: the same wedge with a real slot beside the wedge: the slot leaks
  map: maps/p05_slot_beside.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The negative control of the repair for P01 and P05, where it works. A brush is only ever taken for
  solid where it is: the slot is not brush, so it stays open.


P05.wedge.slot_band: the same wedge with a real slot 0.08 wide, through the wedge: the slot leaks
  map: maps/p05_slot_band.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The negative control of the repair for P01 and P05, where it works. A brush is only ever taken for
  solid where it is: the slot is not brush, so it stays open.

S02_S12_narrow_openings — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

S02.off.125: a 0.125 slot to the void at y = 100, off every split
  map: maps/s02_off_0125.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.125 wide through the +x wall, centred on y = 100 where no block split passes.
  The map leaks and the route must cross the slot. Before NEW_340A the slot's leaf existed but its
  portals to the room and to the void (0.125 x 256 rectangles) were dropped as 'tiny' (fewer than
  three edges longer than 0.2): the map read sealed below 0.25.


S02.off.1875: a 0.1875 slot to the void at y = 100, off every split
  map: maps/s02_off_01875.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.1875 wide through the +x wall, centred on y = 100 where no block split passes.
  The map leaks and the route must cross the slot. Before NEW_340A the slot's leaf existed but its
  portals to the room and to the void (0.1875 x 256 rectangles) were dropped as 'tiny' (fewer than
  three edges longer than 0.2): the map read sealed below 0.25.


S02.off.5: a 0.5 slot to the void at y = 100, off every split
  map: maps/s02_off_05.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.5 wide through the +x wall, centred on y = 100 where no block split passes. The
  map leaks and the route must cross the slot. Before NEW_340A the slot's leaf existed but its portals
  to the room and to the void (0.5 x 256 rectangles) were dropped as 'tiny' (fewer than three edges
  longer than 0.2): the map read sealed below 0.25.


S03.split.1875: a 0.1875 slot to the void centred on the block split y = 0
  map: maps/s03_split_01875.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.1875 wide centred on y = 0, a block split plane. The map leaks through the
  slot. Before NEW_340A the forced block split consumed both slot faces lying within ON_EPSILON of it
  (a half up to 0.1), or split the slot into halves whose portals were dropped as 'tiny' (a half up to
  0.2): sealed up to 0.375.


S03.split.25: a 0.25 slot to the void centred on the block split y = 0
  map: maps/s03_split_025.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.25 wide centred on y = 0, a block split plane. The map leaks through the slot.
  Before NEW_340A the forced block split consumed both slot faces lying within ON_EPSILON of it (a
  half up to 0.1), or split the slot into halves whose portals were dropped as 'tiny' (a half up to
  0.2): sealed up to 0.375.


S03.split.5: a 0.5 slot to the void centred on the block split y = 0
  map: maps/s03_split_05.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.5 wide centred on y = 0, a block split plane. The map leaks through the slot.
  Before NEW_340A the forced block split consumed both slot faces lying within ON_EPSILON of it (a
  half up to 0.1), or split the slot into halves whose portals were dropped as 'tiny' (a half up to
  0.2): sealed up to 0.375.


S04.turned.0: a 0.125 slot through two plugs in a window, turned 0 degrees
  map: maps/s04_turned_0.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The +x wall has a 128 x 128 window centred on (y, z) = (100, 128); two plugs fill it except a slot
  0.125 wide through its centre, turned 0 degrees about x (0 is horizontal). The map leaks through the
  slot, with no grid or epsilon change: the verdict follows the opening.


S04.turned.15: a 0.125 slot through two plugs in a window, turned 15 degrees
  map: maps/s04_turned_15.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The +x wall has a 128 x 128 window centred on (y, z) = (100, 128); two plugs fill it except a slot
  0.125 wide through its centre, turned 15 degrees about x (0 is horizontal). The map leaks through
  the slot, with no grid or epsilon change: the verdict follows the opening.


S04.turned.45: a 0.125 slot through two plugs in a window, turned 45 degrees
  map: maps/s04_turned_45.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The +x wall has a 128 x 128 window centred on (y, z) = (100, 128); two plugs fill it except a slot
  0.125 wide through its centre, turned 45 degrees about x (0 is horizontal). The map leaks through
  the slot, with no grid or epsilon change: the verdict follows the opening.


S05.moved.on_split: a 0.125 slot with one face ON the block split y = 0
  map: maps/s05_moved_on_split.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.125 wide from y = 0 to 0.125: one face ON the block split y = 0. The map leaks
  through the slot wherever the world's block boundary falls.


S05.moved.straddle: a 0.125 slot with both faces within ON_EPSILON of the block split, unevenly
  map: maps/s05_moved_straddle.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A full-height slot 0.125 wide from y = -0.09375 to 0.03125: both faces within ON_EPSILON of the
  block split, unevenly. The map leaks through the slot wherever the world's block boundary falls.


S05.moved.turned45: the 45-degree 0.125 slot moved by (0, 0.3125, 0.1875)
  map: maps/s05_moved_turned45.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  S04.turned.45 with its window, plugs and slot moved 0.3125 in y and 0.1875 in z, off the 1/8 grid
  phase it had.


S06.touch.split: the +x wall as two slabs touching at y = 0
  map: maps/s06_touch_split.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  The +x wall is two brushes sharing the face y = 0 exactly: no opening, so the map is sealed and the
  seam solid. A portal predicate that kept a zero-width crack would leak here.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, -0.03, 128]: south slab at the seam
  - solid [264, 0.03, 128]: north slab at the seam
  - drawn [256, 4.0, 128]: +x wall beside the seam, seen from the room

S06.touch.off: the +x wall as two slabs touching at y = 100
  map: maps/s06_touch_off.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  The +x wall is two brushes sharing the face y = 100 exactly: no opening, so the map is sealed and
  the seam solid. A portal predicate that kept a zero-width crack would leak here.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, 99.97, 128]: south slab at the seam
  - solid [264, 100.03, 128]: north slab at the seam
  - drawn [256, 104.0, 128]: +x wall beside the seam, seen from the room

S07.overlap: the +x wall as two slabs overlapping by 0.125 at y = 0
  map: maps/s07_overlap.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  Two wall slabs overlap by 0.125 across the block split: sealed, no invented route outside.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [264, 0, 128]: inside the overlap

S07.pocket: a 0.125 slot closed at both ends inside the +x wall
  map: maps/s07_pocket.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A 0.125 slot inside the +x wall, capped on the room side and on the void side: a closed thin pocket
  nothing can reach. Sealed; the pocket is not a leak and nothing is asserted about its own air.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - solid [258, 100.0, 128]: the room-side cap
  - solid [270, 100.0, 128]: the void-side cap

S07.blind: a 0.125 slot from the room into the +x wall, closed on the void side
  map: maps/s07_blind.map
  mechanism: none (control)
  the source is sealed
  stages: bsp

  A 0.125 slot from the room 12 units into the +x wall, capped on the void side. Sealed, and the
  slot's air is reachable from the room, so it must stay open (before NEW_340A its portal to the room
  was dropped and the slot filled as unreachable).

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [262, 100.0, 128]: air in the slot, reached from the room
  - solid [270, 100.0, 128]: the void-side cap

S08.band.00625: a 0.0625 slot to the void: inside the ON_EPSILON band
  map: maps/s08_band_00625.map
  mechanism: on_epsilon_band
  the source leaks
  stages: bsp

  A slot 0.0625 wide, off every split. The source leaks. Its two faces lie closer than ON_EPSILON
  (0.1): the face tree consumes the second one as ON the first one's plane, so no leaf lies between
  them. The portal stage is not involved. Kept as a KNOWN FAILURE naming that band, not rewritten as a
  seal.


S11.detail_over: a 0.125 slot behind a detail brush
  map: maps/s11_detail_over.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A 0.125 slot whose outer end is covered by a DETAIL brush. Detail does not seal: the map leaks.


S11.patch_over: a 0.125 slot behind a patch
  map: maps/s11_patch_over.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  A 0.125 slot whose outer end is covered by a flat patch. A patch does not seal: the map leaks.


S12.passage: a sealed room in two halves joined by a 0.125 passage
  map: maps/s12_passage.map
  mechanism: none (control)
  the source is sealed
  stages: bsp + vis + light

  A sealed room split by a full partition at x = 64..80 with one full-height passage 0.125 wide at y =
  100. The player and the light are in the west half; the east half is reached through the passage, so
  its air stays open, its walls are drawn and NORMAL -vis and -light finish. Before NEW_340A the
  passage's portals were dropped and the east half was filled as unreachable.

  - open [-192, -192, 128]: room air beside the player start
  - solid [-264, -192, 128]: inside the -x wall
  - solid [-192, -192, -8]: inside the floor
  - drawn [-200, -200, 0]: floor, seen from the room
  - drawn [-200, -200, 256]: ceiling, seen from the room
  - drawn [-256, -200, 64]: -x wall, seen from the room
  - drawn [-200, 256, 64]: +y wall, seen from the room
  - open [160, -100, 128]: air in the east half, reached through the passage
  - drawn [256, -100, 128]: +x wall, seen from the east half
  - drawn [80, -100, 128]: partition, seen from the east half
  - open [72, 100.0, 128]: air in the passage
  - solid [72, 99.0, 128]: the partition beside the passage

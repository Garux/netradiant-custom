R04_m4_near_parallel — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R04.m4.p08_nearby: M4: a near-parallel face discarded as on another plane (pipe P08 with a nearby pull)
  map: maps/r04_p08_nearby.map
  mechanism: M4
  the source is sealed
  stages: bsp

  No source-independent reduction of M4 exists yet, so this is the shape that shows it: a 354-brush
  pipe whose one segment is pulled 3 units, from Auto-Pigeon's own pipe test generator (no corpus, no
  game data). The face of brush 101 at these points is nearly parallel to a neighbouring segment's
  face; BuildFaceTree_r treats it as ON that plane and discards it, and its drawn triangles filter
  into the neighbour's solid leaves (traced in Auto-Pigeon's compiler investigation). KNOWN FAILURE
  until NEW_340 repairs it: P1+P2 does not touch it, and the two attempted repairs (P3, P4) broke R05
  and R06.

  - drawn [-58.6043605, 54.64965349999999, 0]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-61.918280375, 54.504964724999994, 5.371289086187509]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-65.23220025, 54.360275949999995, 10.742578172375017]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-61.918280375, 54.504964724999994, -5.371289086187509]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-65.23220025, 54.360275949999995, -10.742578172375017]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-55.290440625, 54.79434227499999, -5.371288999999997]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-51.97652075, 54.93903104999999, -10.742577999999995]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-55.290440625, 54.79434227499999, 5.371288999999997]: the bore face of segment brush 101 (map line 919), seen from the bore
  - drawn [-51.97652075, 54.93903104999999, 10.742577999999995]: the bore face of segment brush 101 (map line 919), seen from the bore

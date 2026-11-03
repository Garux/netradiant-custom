P01_bend_slivers — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

P01.sliver.bend_a: air inside solid: a first BEND, closed in the source, read as leaking
  map: maps/p01_bend_a.map
  mechanism: sliver
  the source is sealed
  stages: bsp + vis + light

  A corridor from Auto-Pigeon's own BEND test generator (no corpus, no game data), reduced brush by
  brush while the release still leaked, upstream sealed and an independent solver of the source
  brushes found no air path: the source is closed, to the 1e-6 the map text is written with. Two
  near-parallel node planes from neighbouring pieces cross a brush and bound a wedge thinner than 0.1.
  SplitBrush keeps a fragment reaching less than 0.1 past a plane whole on its other side, so every
  fragment stays out of the wedge and a leaf inside the brush is left non-opaque: air inside solid.
  Upstream reads this sealed only because it drops the narrow portals into that leaf, and with them
  real openings.

  - open [418.0, -169.0, -173.0]: air on the interior side of the wall
  - open [500.658, -312.235, -309.483]: air on the interior side of the wall
  - open [530.438, -265.31, -276.911]: air on the interior side of the wall
  - solid [428.711, -443.931, -246.313]: inside a declared brush
  - solid [429.4, -514.212, -514.257]: inside a declared brush
  - solid [545.247, -378.25, -274.702]: inside a declared brush
  - solid [527.793, -415.308, -312.194]: inside a declared brush

P01.sliver.bend_b: air inside solid: a first BEND, a second shape, closed in the source, read as leaking
  map: maps/p01_bend_b.map
  mechanism: sliver
  the source is sealed
  stages: bsp + vis + light

  A corridor from Auto-Pigeon's own BEND test generator (no corpus, no game data), reduced brush by
  brush while the release still leaked, upstream sealed and an independent solver of the source
  brushes found no air path: the source is closed, to the 1e-6 the map text is written with. Two
  near-parallel node planes from neighbouring pieces cross a brush and bound a wedge thinner than 0.1.
  SplitBrush keeps a fragment reaching less than 0.1 past a plane whole on its other side, so every
  fragment stays out of the wedge and a leaf inside the brush is left non-opaque: air inside solid.
  Upstream reads this sealed only because it drops the narrow portals into that leaf, and with them
  real openings.

  - open [207.0, -82.0, -86.0]: air on the interior side of the wall
  - open [222.352, -222.407, -218.694]: air on the interior side of the wall
  - open [240.063, -214.73, -198.517]: air on the interior side of the wall
  - solid [212.495, -243.527, -247.406]: inside a declared brush
  - solid [391.293, -62.55, -142.996]: inside a declared brush
  - solid [373.906, -34.75, -124.531]: inside a declared brush

P01.sliver.rotate: air inside solid: a rotated BEND, closed in the source, read as leaking
  map: maps/p01_rotate.map
  mechanism: sliver
  the source is sealed
  stages: bsp + vis + light

  A corridor from Auto-Pigeon's own BEND test generator (no corpus, no game data), reduced brush by
  brush while the release still leaked, upstream sealed and an independent solver of the source
  brushes found no air path: the source is closed, to the 1e-6 the map text is written with. Two
  near-parallel node planes from neighbouring pieces cross a brush and bound a wedge thinner than 0.1.
  SplitBrush keeps a fragment reaching less than 0.1 past a plane whole on its other side, so every
  fragment stays out of the wedge and a leaf inside the brush is left non-opaque: air inside solid.
  Upstream reads this sealed only because it drops the narrow portals into that leaf, and with them
  real openings.

  - open [26.0, -59.0, 376.0]: air on the interior side of the wall
  - open [-35.995, -85.867, 459.737]: air on the interior side of the wall
  - open [0.0, -92.095, 406.668]: air on the interior side of the wall
  - solid [-7.357, -235.59, 430.336]: inside a declared brush
  - solid [167.646, 25.092, 617.647]: inside a declared brush
  - solid [233.841, -190.074, 495.944]: inside a declared brush
  - solid [127.685, 27.617, 583.416]: inside a declared brush

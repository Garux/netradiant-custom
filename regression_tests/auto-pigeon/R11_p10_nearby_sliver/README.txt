R11_p10_nearby_sliver — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R11.m3.p10_nearby: M3: a split sliver the face tree has no plane for fills a pipe bore (P10, nearby pull)
  map: maps/r11_p10_nearby.map
  mechanism: M3
  the source is sealed
  stages: bsp

  A 630-brush pipe from the same generator. The visible-face tree consumes the bore face of brush 324
  (map line 2878) as ON a neighbouring segment's face plane, every point of its VISIBLE part being
  within ON_EPSILON of it. SplitBrush then decides the same question from the brush's full,
  1/8-snapped windings, finds a vertex 0.104 past that plane and splits for real: the 0.1-thick sliver
  goes to the bore side, where the tree has no plane bounding it (the face that would have been one
  was consumed), lands in one leaf that holds most of the bore and makes it opaque. Five bore points
  read solid and three bore faces lose their drawn surfaces.

  - open [23.650396, 380.542256, -20.127506]: declared bore air of the pipe
  - open [44.572602, 366.388518, -15.415647]: declared bore air of the pipe
  - open [121.599811, 260.853132, -42.55114]: declared bore air of the pipe
  - open [350.118658, 145.887534, -43.12829]: declared bore air of the pipe
  - open [372.514138, 133.520196, -43.12829]: declared bore air of the pipe
  - drawn [65.049, 314.197, -83.871]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [52.921, 302.645, -83.779]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [40.793, 291.092, -83.687]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [73.23, 318.143, -84.022]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [81.41, 322.089, -84.173]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [68.996, 321.804, -83.813]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [72.944, 329.411, -83.755]: the floor face of brush 307 (map line 2739), seen from the bore
  - drawn [53.773, 292.462, -83.973]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [45.591, 288.517, -83.845]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [37.41, 284.572, -83.717]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [49.826, 284.855, -83.985]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [45.88, 277.247, -83.998]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [65.9, 304.016, -84.088]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [78.027, 315.569, -84.203]: the floor face of brush 308 (map line 2747), seen from the bore
  - drawn [102.874, 357.645, -50.327]: a slanted bore face (map line 2769), seen from the bore
  - drawn [93.583, 350.045, -62.009]: a slanted bore face (map line 2769), seen from the bore
  - drawn [84.291, 342.445, -73.691]: a slanted bore face (map line 2769), seen from the bore
  - drawn [109.652, 359.633, -44.615]: a slanted bore face (map line 2769), seen from the bore
  - drawn [116.429, 361.622, -38.903]: a slanted bore face (map line 2769), seen from the bore
  - drawn [105.388, 363.256, -44.357]: a slanted bore face (map line 2769), seen from the bore

---
title: A route depth or a shared polygon width does not prove a leak is real
author: Andrea-dintino
date: 2026-11-03
branch: main
cost: { minutes: 3, tokens: 36611 }
id: l-4b1b11b1
scope: repo
status: active
src: agent
touches:
  - regression_tests/auto-pigeon/generate.py
  - tools/quake3/q3map2/brush.cpp
---

## Decided
To judge a leak against the source, inflate every brush side outward by r and ask whether an air path from the player to the void survives: it is then at least 2r wide along all of its length. Use that clearance, calibrated on known slots such as those of regression_tests/auto-pigeon/generate.py, as ground truth. A route walking through a brush is only a lead.

## Tried and ruled out
Widest-path over the width of the polygon two cells share: an oblique cut across a hairline makes a wide polygon, so a closed seam read 1.19 wide. A first oracle also dropped a degenerate face and stopped clipping by its plane, which joined distant cells: it reported 29 openings no compiler saw, and it made the leaf cut in tools/quake3/q3map2/brush.cpp look as if it had sealed a real opening. A crossing outside the world box is now an abort.

## Watch out
Upstream sealing proves nothing, and neither does a leak on both compilers: on the BEND rows some both-leak maps are closed in the source. A seam that is wide at one end can be closed at the other.

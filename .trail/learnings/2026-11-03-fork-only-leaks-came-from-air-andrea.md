---
title: "Fork-only leaks came from air inside solid brushes, not from real openings"
author: Andrea-dintino
date: 2026-11-03
branch: main
cost: { minutes: 96, tokens: 1028766 }
id: l-817ce405
scope: repo
status: active
src: agent
touches:
  - regression_tests/auto-pigeon/generate.py
  - README.md
  - regression_tests/auto-pigeon/known-failures.json
  - tools/quake3/q3map2/portals.cpp
  - regression_tests/auto-pigeon/P02_P05_portal_controls/README.txt
  - regression_tests/auto-pigeon/P02_P05_portal_controls/maps/p06_corner_a.map
  - regression_tests/auto-pigeon/P02_P05_portal_controls/maps/p06_corner_b.map
  - regression_tests/auto-pigeon/P02_P05_portal_controls/maps/p06_corner_c.map
  - regression_tests/auto-pigeon/P02_P05_portal_controls/maps/p06_corner_open.map
  - regression_tests/auto-pigeon/P02_P05_portal_controls/maps/p06_corner_residual.map
---

## Decided
A brush fragment reaching under 0.1 past a node plane stays whole on the other side, so a wedge between two near-parallel planes inside a brush gets no fragment and its leaf stays air. Upstream only escaped this by dropping the narrow portals into that leaf. Keep the fork's portal rule; the repair belongs where the brush is lost. Fill a leaf only when the slivers that reached it cover its exact volume; cut leaves along brush planes only on a map that would otherwise leak, so a sealed map keeps its tree.

## Tried and ruled out
Blaming the consumed-side keep from the earlier sliver repair: its reach is 0.03 and disabling it changes nothing. Cutting every thin leaf unconditionally: it fixes the same rows but grows clean pipe maps up to 4.6 times. Reading a leaked BSP as evidence of the flood: a leak still builds a second tree, which is not the tree that was flooded.

## Watch out
A portal chopped by MakeNodePortal at CLIP_EPSILON empties across a leaf thinner than that, which seals a passage by accident: a green verdict can be a lost portal. Check that a leaf was actually filled.

In the code: tools/quake3/q3map2/brush.cpp KeepSliver, FillCoveredLeaves, CutSliverLeaves; portals.cpp SplitLeafOnPlane, BaseWindingForNode; cases P01, P05, P06.

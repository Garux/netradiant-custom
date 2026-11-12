---
title: "Don't blindly apply per-game settings in the map compiler without checking effects"
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-f190a641
scope: repo
status: active
src: init-review
touches:
  - tools/quake3/q3map2/bsp.cpp
---

## Watch out
- Game descriptor values may have been added only for reference, not real use. Applying them changes behavior for many games, so the maintainer merged it only as an experiment, based on one engine's known setting.
- In the code: Before wiring `game->maxLMSurfaceVerts` or `keepLights` (bsp.cpp) into behavior, check practical effects. The -light stage can't know if -keeplights was used in -bsp, so it must load lights anyway.

_From PR #74 here, Jun 3._

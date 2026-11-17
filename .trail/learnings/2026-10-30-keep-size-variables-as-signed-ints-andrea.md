---
title: "Keep size variables as signed ints, not unsigned, until a global refactor"
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-380890c8
scope: repo
status: active
src: init-review
touches:
  - tools/quake3/q3map2/q3map2.h
---

## Tried and ruled out
- Switching a few size settings to unsigned adds warnings against the rest of the code. It is also dangerous because user input is parsed with atoi, so negative values wrap.
- In the code: Keep maxLMSurfaceVerts, maxSurfaceVerts, maxSurfaceIndexes as `int` in tools/quake3/q3map2/q3map2.h. Don't change signedness piecemeal.

_From PR #74 here, Jun 3._

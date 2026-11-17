---
title: Lightmap coordinates for styled lights exist only at index zero during compile
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-d5216088
scope: repo
status: active
src: init-review
---

## Watch out
- Two bugs (vertex lights and bounce lighting) came from reading internal lightmap coordinates for styles above zero. Final BSP coordinates are only computed when saving, so other indices are uninitialized garbage.
- In the code: In q3map2 light code, don't index raw lightmap coords (yDrawVerts) with lightmapNum > 0. Use index 0 for internal sampling (see RadSample and vertex light code).

_From PRs #243, #238 here, Jul 31._

---
title: "Fix wrong path slashes once in the file loader, not per caller"
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-72f54f9e
scope: repo
status: active
src: init-review
touches:
  - tools/quake3/common/vfs.cpp
  - tools/quake3/q3map2/model.cpp
  - "tools/quake3/common/vfs.cpp#vfsLoadFile"
  - "tools/quake3/q3map2/model.cpp#LoadModel"
---

## Decided
- A proposed fix normalized slashes inside one model loader with hand-written copy code. The maintainer preferred fixing it centrally where files are loaded, since that already cleans slashes for most callers.
- In the code: Fix path separator issues in `vfsLoadFile` (tools/quake3/common/vfs.cpp) rather than in `LoadModel` in tools/quake3/q3map2/model.cpp. Avoid fixed-size char buffers and hardcoded key lists.

_From PR #200 here, Jun 25._

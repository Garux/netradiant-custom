---
title: Linux build monitoring needs address reuse and clean socket shutdown together
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-1a996e6a
scope: repo
status: active
src: init-review
---

## Watch out
- Starting a build with monitoring often failed because the listening port could not be rebound. Only setting both the reuse option and a full shutdown fixed it.
- In the code: In libs/l_net/l_net_berkley.c, use SO_REUSEADDR and shutdown(SHUT_RDWR) before close. Both are required. Port 39000.

_From PR #205 here, Jun 23._

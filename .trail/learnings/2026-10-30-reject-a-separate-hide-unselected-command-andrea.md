---
title: "Reject a separate 'hide unselected' command; region selected already covers it"
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-bff30701
scope: repo
status: active
src: init-review
---

## Tried and ruled out
- The proposed command duplicated an existing feature, and it would have added toolbar clutter. It also used a different flag that affects map building.
- In the code: Point users to the existing 'Region selected' command instead of adding a hide-unselected action or toolbar button.

_From PR #276 here, Dec 18._

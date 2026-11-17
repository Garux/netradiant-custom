---
title: No UI translations; English-only interface is deliberate
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-38505998
scope: repo
status: active
src: init-review
---

## Tried and ruled out
- A language support PR was closed. The maintainer argued a single shared language guarantees mutual understanding, and showing translations next to English would clutter the interface.
- In the code: Don't add interface translation machinery. If ever needed, tooltips are the suggested place for translated text.

_From PR #32 here, Jul 14._

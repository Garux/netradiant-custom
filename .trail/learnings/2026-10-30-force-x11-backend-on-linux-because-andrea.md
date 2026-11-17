---
title: Force X11 backend on Linux because mouse capture fails on Wayland
author: Andrea-dintino
date: 2026-10-30
standing: true
id: l-8bedb2e2
scope: repo
status: active
src: init-review
---

## Decided
- Viewports were unusable on Wayland since the cursor position cannot be set. The app now defaults to the X11/Xwayland backend automatically.
- In the code: Don't remove the default that selects Qt's xcb platform on Linux (equivalent to QT_QPA_PLATFORM=xcb) in the app's main startup code.

_From PR #208 here, Jul 23._

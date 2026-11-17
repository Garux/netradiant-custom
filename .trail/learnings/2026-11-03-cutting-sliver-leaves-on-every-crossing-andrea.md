---
title: Cutting sliver leaves on every crossing brush plane sealed a real opening
author: Andrea-dintino
date: 2026-11-03
branch: main
cost: { minutes: 20, tokens: 787826 }
id: l-d5f10715
scope: repo
status: active
src: agent
touches:
  - tools/quake3/q3map2/brush.cpp
---

## Decided
Keep CutSliverLeaves in tools/quake3/q3map2/brush.cpp conservative: cut a leaf only where a brush separates it (two opposed sides it reaches past) or where the leaf is thinner than the band. Two closed second-BEND corridors still leak with that rule; that is the accepted cost.

## Tried and ruled out
Cutting on any side of a brush that reached the leaf, with leaf volumes in double. It sealed three more false leaks, and it sealed a real opening at least 0.1 wide along its whole length (a second-BEND corridor): repeated cuts through large air leaves lost the connection between two of them, most likely SplitNodePortals folding a portal whose piece on one side is degenerate into the other child. Found only because every changed verdict was checked against source clearance.

## Watch out
A repair that seals more rows is not better until each newly sealed row is shown closed in the source. Re-run the clearance table over every row whose verdict moved.

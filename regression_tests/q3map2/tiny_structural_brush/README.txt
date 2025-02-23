DESCRIPTION OF PROBLEM:
=======================

Tiny brush windings are removed by FixWindingAccu(), which may result in
non convex set of faces in a brush. Using such set in FaceBSP() produces
fucked up tree with unexpected behavior. In sample map half of the box
is invisible, as if there is antiportal brush.

To trigger the bug, compile the map with -bsp -meta args.


SOLUTION TO PROBLEM:
====================

For now basic test 'non-empty windings number < 4' catches all reported cases,
while, of course, it's not sufficient to detect all possible cases.
Catch in MakeStructuralBSPFaceList(), report to user, mark brush detail
to be also skipped in MakeVisibleBSPFaceList() later.

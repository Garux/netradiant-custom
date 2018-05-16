NetRadiant-custom
=================

The open-source, cross-platform level editor for id Tech based games.

NetRadiant-custom is a fork of NetRadiant (GtkRadiant 1.4&rarr;massive rewrite&rarr;1.5&rarr;NetRadiant&rarr;this)

---
![screenshot](/../readme_files/radDarkShot.png?raw=true)
---

## Downloads

Ready-to-use packages are available in the [Releases section](/../../releases).

## Supported games

Main focus is on Quake, Quake3 and Quake Live.

Though other normally supported games should work too; See [unverified game configs](/../readme_files/unverified_gamepacks.7z "darkplaces&NewLine;doom3&NewLine;et&NewLine;heretic2&NewLine;hl&NewLine;ja&NewLine;jk2&NewLine;neverball&NewLine;nexuiz&NewLine;oa&NewLine;osirion&NewLine;prey&NewLine;q2&NewLine;q4&NewLine;quetoo&NewLine;sof2&NewLine;stvef&NewLine;trem&NewLine;turtlearena&NewLine;ufoai&NewLine;unvanquished&NewLine;warsow&NewLine;wolf&NewLine;xonotic").

## Features

Development is focused on smoothing and tweaking editing process.

#### Random feature highlights

* WASD camera binds
* Clipper tool, brush and entity creation, working in camera
* left mouse button click tunnel selector, paint selector
* numerous mouse shortcuts (see help->General->Mouse Shortcuts)
* focus camera on selected (Tab)
* snapped modes of manipulators
* draggable renderable transform origin for manipulators
* quick vertices drag / brush faces shear
* shader editor
* texture painting by drag
* seamless brush face to face texture paste
* keyboard shortcuts are customizable
* GUI themes, fonts are customizable
* meshTex plugin
* patch thicken
* all patch prefabs are created aligned to active projection
* filters toolbar with extra functions on right mouse button click
* viewports zoom in to pointer
* \'all Supported formats\' default option in open dialogs
* opening *.map, sent via cmd line (can assign *.map files in OS to be opened with radiant)
* texture browser: show alpha transparency option
* texture browser: gtk search in directories and tags trees
* texture browser: search in currently shown textures
* CSG Tool (aka shell modifier)
* working region compilations (build a map with region enabled = compile regioned part only)
* QE tool in a component mode: perform drag w/o hitting any handle too
* map info dialog: + Total patches, Ingame entities, Group entities, Ingame group entities counts
* connected entities walker
* build->customize: list available build variables
* 50x faster light radius rendering
* light power is adjustable by mouse drag
* anisotropic textures filtering
* optional MSAA in viewports
* new very fast entity names rendering system
* support \'stupid quake bug\'
* arbitrary texture projections for brushes and curves
* fully working texture lock, supporting any affine transformation
* texture locking during vertex and edge manipulations
* brush resize (QE tool): reduce selected faces amount to most wanted ones
* support brush formats, as toggleable preference: Axial projection, Brush primitives, Valve 220
* autodetect brush type on map opening
* automatic AP, BP and Valve220 brush types conversion on map Import and Paste
* new bbox styled manipulator, allowing any affine transform (move, rotate, scale, skew)
* incredible number of fixes and options


#### Q3Map2:

* allowed samples+filter, makes sense
* -vertexscale
* -novertex works, (0..1) sets globally
* fixed _clone _ins _instance (_clonename) functionality
* -nolm - no lightmaps
* -bouncecolorratio 0..1 (ratio of colorizing light sample by texture)
* q3map_remapshader remaps anything fine, on all stages
* fixed model autoclip, added 20 new modes
* automatic map packager (complete Q3 support)
* -brightness 0..alot, def 1: mimics q3map_lightmapBrightness, but globally + affects vertexlight
* -contrast -255..255, def 0: lighting contrast
* report full / full pk3 path on file syntax errors
* new area lights backsplash algorithm (utilizing area lights instead of point ones)
* -backsplash (float)scale (float)distance: adjust area lights globally (real area lights have no backsplash)
* new slightly less careful, but much faster lightmaps packing algorithm (allocating... process)
* Valve220 mapformat autodetection and support

###### see changelog-custom.txt for more

## [COMPILING](/COMPILING)
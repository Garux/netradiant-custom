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
* Fully supported editing in 3D view (brush and entity creation, all manipulating tools)
* Uniform merge algorithm, merging selected brushes, components and clipper points
* Free and robust vertex editing, also providing abilities to remove and insert vertices
* UV Tool (edits texture alignment of selected face or patch)
* Autocaulk
* Left mouse button click tunnel selector, paint selector
* Numerous mouse shortcuts (see help->General->Mouse Shortcuts)
* Focus camera on selected (Tab)
* Snapped modes of manipulators
* Draggable renderable transform origin for manipulators
* Quick vertices drag / brush faces shear shortcut
* Simple shader editor
* Texture painting by drag
* Seamless brush face to face texture paste
* Customizable keyboard shortcuts
* Customizable GUI themes, fonts
* MeshTex plugin
* Patch thicken
* All patch prefabs are created aligned to active projection
* Filters toolbar with extra functions on right mouse button click
* Viewports zoom in to mouse pointer
* \'all Supported formats\' default option in open dialogs
* Opening *.map, sent via cmd line (can assign *.map files in OS to be opened with radiant)
* Texture browser: show alpha transparency option
* Texture browser: gtk search in directories and tags trees
* Texture browser: search in currently shown textures
* CSG Tool (aka shell modifier)
* Working region compilations (build a map with region enabled = compile regioned part only)
* QE tool in a component mode: perform drag w/o hitting any handle too
* Map info dialog: + Total patches, Ingame entities, Group entities, Ingame group entities counts
* Connected entities selector/walker
* Build->customize: list available build variables
* 50x faster light radius rendering
* Light power is adjustable by mouse drag
* Anisotropic textures filtering
* Optional MSAA in viewports
* New very fast entity names rendering system
* Support \'stupid quake bug\'
* Arbitrary texture projections for brushes and curves
* Fully working texture lock, supporting any affine transformation
* Texture locking during vertex and edge manipulations
* Brush resize (QE tool): reduce selected faces amount to most wanted ones
* Support brush formats, as toggleable preference: Axial projection, Brush primitives, Valve 220
* Autodetect brush type on map opening
* Automatic AP, BP and Valve220 brush types conversion on map Import and Paste
* New bbox styled manipulator, allowing any affine transform (move, rotate, scale, skew)
* Incredible number of fixes and options


#### Q3Map2:

* Allowed simultaneous samples+filter use, makes sense
* -vertexscale
* -novertex works, (0..1) sets globally
* Fixed _clone _ins _instance (_clonename) functionality
* -nolm - no lightmaps
* -bouncecolorratio 0..1 (ratio of colorizing light sample by texture)
* q3map_remapshader remaps anything fine, on all stages
* Fixed model autoclip, added 20 new modes
* Automatic map packager (complete Q3 support)
* -brightness 0..alot, def 1: mimics q3map_lightmapBrightness, but globally + affects vertexlight
* -contrast -255..255, def 0: lighting contrast
* Report full / full pk3 path on file syntax errors
* New area lights backsplash algorithm (utilizing area lights instead of point ones)
* -backsplash (float)scale (float)distance: adjust area lights globally (real area lights have no backsplash)
* New slightly less careful, but much faster lightmaps packing algorithm (allocating... process)
* Valve220 mapformat autodetection and support
* Correct .obj and .mtl loading
* Guessing model shaders paths

###### see changelog-custom.txt for more

## [COMPILING](/COMPILING)

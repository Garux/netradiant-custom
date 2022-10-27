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

Though other normally supported games should work too. Releases include configs for the following games: Alien Arena, Darkplaces, Doom 3, Doombringer, Wolfenstein: Enemy Territory, Heretic II, Half-Life, Jedi Knight Jedi Academy, Jedi Knight II: Jedi Outcast, Kingpin, Neverball, Nexuiz, Open Arena, Project::OSiRiON, Prey, Quake II, Q3Rally, Quake 4, Quetoo, Smokin' Guns, Soldier of Fortune II - Double Helix, Star Trek Voyager : Elite Force, Tremulous, Turtle Arena, UFO:Alien Invasion, Unreal Arena, Unvanquished, Urban Terror, Warfork, Warsow, Return To Castle Wolfenstein, World of Padman, Xonotic, ZEQ2 Lite.

## Features

Development is focused on smoothing and tweaking editing process.

#### Random feature highlights

* WASD camera binds
* Fully supported editing in 3D view (brush and entity creation, all manipulating tools)
* Uniform merge algorithm, merging selected brushes, components and clipper points
* Free and robust vertex editing, also providing abilities to remove and insert vertices
* UV Tool (edits texture alignment of selected face or patch)
* Autocaulk
* Model browser
* Brush faces extrusion
* Left mouse button click tunnel selector, paint selector
* Numerous mouse shortcuts (see help->General->Mouse Shortcuts)
* Focus camera on selected (Tab)
* Snapped modes of manipulators
* Draggable renderable transform origin for manipulators
* Quick vertices drag / brush faces shear shortcut
* Simple shader editor
* Texture painting by drag
* Seamless brush face<->face, patch<->face texture paste
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
* Texture browser: search in directories and tags trees
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
* rendering of Q3 shader based skyboxes
* Incredible number of fixes and options


#### Q3Map2:

* q3map_remapshader remaps anything fine, on all stages
* Automatic map packager (complete Q3 support)
* Report full / full pk3 path on file syntax errors
* Allowed simultaneous samples+filter use, makes sense
* -brightness 0..alot, def 1: mimics q3map_lightmapBrightness globally
* -contrast -255..255, def 0: lighting contrast
* -saturation light option
* -bouncecolorratio 0..1 (ratio of colorizing light sample by texture)
* -nolm - no lightmaps
* -novertex works, (0..1) sets globally
* -vertexscale
* New area lights backsplash algorithm (utilizing area lights instead of point ones)
* -backsplash (float)scale (float)distance: adjust area lights globally (real area lights have no backsplash)
* New slightly less careful, but much faster lightmaps packing algorithm (allocating... process)
* -extlmhacksize zero effort external lightmaps for Q3
* Valve220 mapformat autodetection and support
* Consistent brush content deduction with mixed face parameters
* Model shaders paths deduction
* Fixed model autoclip, added 20 new clipping modes
* Support negative misc_model scale
* Assimp model loading library (40+ formats)
* -json bsp export/import
* -mergebsp injects one bsp to another
* No shaderlist.txt mode: load all shaders

###### see changelog-custom.txt for more

## [COMPILING](/COMPILING)

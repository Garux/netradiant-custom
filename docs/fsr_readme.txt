////////////////////////////////////////
// Q3Map2 - FS20 - R5
// programming: Pavel P. [VorteX] Timofeyev
// e-mail: paul.vortex@gmail.com
//////////

========================================
 ABOUT
==========

 The FS-R releases are modifications of Q3map2 FS releases (see fs_readme.txt) by TwentySeven.

 This software is licensed under the GPL.

========================================
 VERSION HISTORY
========================================

--------------------
----------------------------------------
- VERSION R5
--------------------
----------

- added "-gridscale X" and "-gridambientscale X" to scale grid lightning, note that ambient grid receive
both "-gridscale" and "-gridambientscale". For -darkplaces, -dq and -prophecy game mode added
default game-specific values: -gridscale 0.3 -gridambientscale 0.4

- modified game-specific options prints at the begin of light stage

--------------------
----------------------------------------
- VERSION R4
--------------------
----------

- added spawnflag 32 "unnormalized" - it means that light color will not be normalized on parse

- added spawnflag 64 "distance_falloff" to gain access to light distance attenuation used for sun and area lights

- to make things easy, behavior of _deviance/_samples on lights is changed - now when "_deviance" detected, "_samples" get same start value. So in most cases we need to set 1 key instead of 2.

--------------------
----------------------------------------
- VERSION R3
--------------------
----------

- fixed tangentspace deluxemaps with radiocity (was broken)

- fixed bug with overbrights on shadow edges in deluxemapping. This bug was here because shadowed luxels does not receive light directions leading to very contrast light direction vectors in light/shadow zones, then texture filtering introduces a bug when interpolating this values. Now deluxels calculated with no shadows (shadowing work is done in lightmap already).

- added "-keeplights" switch into light phase, this works like "_keeplights 1" world key. World key has greater priority (so if you set -keeplights and _keeplights 0 light entites won't be keeped). Also Per-game "keepLights" setting added, it defaults to True in "dq", "darkplaces" and "prophecy" games.

- Global floodlight code reverted back to have no effect on deluxemap (it don't add anything and deluxemaps looks blurry). q3map_floodlight shader keyword is changed to have "<light_direction_power>" parameter in the end unless "low_quality", so now it is q3map_floodlight <r> <g> <b> <dist> <intentity> <light_direction_power>. Use 0 in to have no effect on deluxemap, 1 to have standart effect like all lights do and 200 and more to make floodlight override deluxemap on this surface (it would be useful for floodlighted water since if some lightsource will be too close it will make dark zone on deluxemap).

- added printing "entity has vertex normal smoothing with..." when _smoothnormals/_sn/_smooth keys is found, just like _lightmapscale did

--------------------
----------------------------------------
- VERSION R2
--------------------
----------

- added "_smoothnormals" or "_sn" or "_smooth" to set nonplanar normal smoothing on entities. This works exactly as -shadeangle and overrides shader-set normal smoothing (q3map_shadeangle).

- reorganized floodlighting code to be more clean

- fixed bug with floodlight not adding a light direction to deluxemaps

- removed "q3map_minlight" test code that was added in previous version

- added "-deluxemode 1" switch to generate tangentspace deluxemaps instead of modelspace.
Actually deluxemaps being converted to tangentspace in StoreLigtmaps phase. "-deluxemode 0" will switch back to modelspace deluxemaps.

- added game-specific setting of deluxemode. "darkplaces", "dq" and "prophecy" games still have 0 because darkplaces engine can't detect tangentspace deluxemaps on Q3BSP yet (probably detection can be done by setting of custom world field?).

--------------------
----------------------------------------
- VERSION R1
--------------------
----------

- added shader deprecation keyword "q3map_deprecateShader <shader>", a global variant of q3map_baseShader/q3map_remapShader. Replacing is done in early load stage, so all q3map2 keyworlds are supported (instead of q3map_remapShader which only remaps rendering part of shader so surfaceparms won't work). Maximum of 16 chained deprecations are allowed. In other words if you deprecate the shader by this keyword, it will be showed in map with another name.

 Example deprecated shader:

 // this shader will appear as "textures/#water0" in map
 textures/test/test_deprecate_water
 {
 	q3map_deprecateShader textures/#water0
 }

- added "_patchMeta 1" (or "patchMeta") entity keyword to force entity patch surfaces to be converted to planar brush at compile. This works exactly as "-patchmeta" bsp switch, but only for user customized entities. Additional 2 new keys: "_patchQuality" ("patchQuality") and "_patchSubdivide" ("patchSubdivide") are added. _PatchQuality divides the default patch subdivisions by it's value (2 means 2x more detailed patch). _patchSubdivide overrides patch subdivisions for that entity (use 4 to match Quake3) and ignores _patchQuality. Note: using "_patchMeta" on world makes all world patches to be triangulated, but other entities will remain same.

- MAX_TW_VERTS increased from 12 to 24 for ability co compile some insane maps with large curve count.

- added "EmitMetaStats" printing in the end of BSP stage to show full meta statistics, not only for world. So all "_patchMeta 1" surfaces will be in it.

- added gametype-controlled structure fields for "-deluxe", "-subdivisions", "-nostyles", "-patchshadows". New "dq" (deluxequake), "prophecy" and "darkplaces" games uses them. Additionally added "-nodeluxe", "-nopatchshadows", "-styles" to negate positive defaults.

- Floodligting code is changed to handle custom surfaces. And "q3map_floodlight <red> <green> <blue> <distance> <brightness> <low_quality>" shader keyword was added. Per-surfaces floodlight code does not intersect with global floodlight done by -floodlight of _floodlight worldspawn key, their effects get summarized. This is good way to light up surfacelight surfaces, such as water.

- added printing of game-specific options (default light gamma, exposure and such) to light phase

- "func_wall" entities now have _castShadows default to 1. This works only for "dq" and "prophecy" games.

- added "-samplesize" switch to light phase, this scales samplesizes for all lightmaps, useful to compile map with different lightmap quality.

- added "_ls" key which duplicates "_lightmapscale" but have short name (order of checking is lightmapscale->_lightmapscale->_ls)


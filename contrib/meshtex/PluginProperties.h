/**
 * @file PluginProperties.h
 * Information about this plugin used in areas like plugin registration and
 * generating the About dialog.
 * @ingroup meshtex-plugin
 */

/*
 * Copyright 2012 Joel Baxter
 *
 * This file is part of MeshTex.
 *
 * MeshTex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * MeshTex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshTex.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(INCLUDED_PLUGINPROPERTIES_H)
#define INCLUDED_PLUGINPROPERTIES_H

#include "UtilityMacros.h"
//#include "CodeVersion.h"

#define PLUGIN_NAME "MeshTex"
#define PLUGIN_VERSION_MAJOR_NUMERIC 3
#define PLUGIN_VERSION_MINOR_NUMERIC 0
#define PLUGIN_VERSION STRINGIFY_MACRO(PLUGIN_VERSION_MAJOR_NUMERIC) "." STRINGIFY_MACRO(PLUGIN_VERSION_MINOR_NUMERIC) " beta" \
            ", commit "
#define PLUGIN_AUTHOR "Joel Baxter"
#define PLUGIN_AUTHOR_EMAIL "jl@neogeographica.com"
#define PLUGIN_COPYRIGHT_DATE "2012"
#define PLUGIN_DESCRIPTION "Align and scale textures on patch meshes"
#define PLUGIN_FILE_BASENAME "meshtex"
#define PLUGIN_FILE_DESCRIPTION "patch mesh texturing plugin for GtkRadiant 1.5"

#endif // #if !defined(INCLUDED_PLUGINPROPERTIES_H)

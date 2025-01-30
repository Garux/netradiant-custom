/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "string/stringfwd.h"
#include "math/vectorfwd.h"

void Patch_registerCommands();
void Patch_constructToolbar( class QToolBar* toolbar );
void Patch_constructMenu( class QMenu* menu );

namespace scene
{
class Graph;
}

void Scene_PatchSetShader_Selected( scene::Graph& graph, const char* name );
void Scene_PatchGetShader_Selected( scene::Graph& graph, CopiedString& name );
void Scene_PatchSelectByShader( scene::Graph& graph, const char* name );
void Scene_PatchFindReplaceShader( scene::Graph& graph, const char* find, const char* replace );
void Scene_PatchFindReplaceShader_Selected( scene::Graph& graph, const char* find, const char* replace );

void Scene_PatchGetTexdef_Selected( scene::Graph& graph, class TextureProjection &projection );
bool Scene_PatchGetShaderTexdef_Selected( scene::Graph& graph, CopiedString& name, TextureProjection &projection );
void Patch_SetTexdef( const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation );

void Scene_PatchCapTexture_Selected( scene::Graph& graph );

void Scene_PatchProjectTexture_Selected( scene::Graph& graph, const class texdef_t& texdef, const Vector3* direction );
void Scene_PatchProjectTexture_Selected( scene::Graph& graph, const TextureProjection& projection, const Vector3& normal );
void Scene_PatchNaturalTexture_Selected( scene::Graph& graph );
void Scene_PatchTileTexture_Selected( scene::Graph& graph, float s, float t );

void PatchFilters_construct();

void PatchPreferences_construct();

void Patch_registerPreferencesPage();

void Patch_NaturalTexture();
void Patch_CapTexture();
void Scene_PatchFlipTexture_Selected( scene::Graph& graph, int axis );
void Patch_FlipTextureX();
void Patch_FlipTextureY();

extern class PatchCreator* g_patchCreator;

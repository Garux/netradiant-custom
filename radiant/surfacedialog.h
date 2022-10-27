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


void SurfaceInspector_Construct();
void SurfaceInspector_Destroy();

void SurfaceInspector_constructWindow( class QWidget* widget );
void SurfaceInspector_destroyWindow();

bool SelectedFaces_empty();
void SelectedFaces_copyTexture();
void SelectedFaces_pasteTexture();
void FaceTextureClipboard_setDefault();


// the increment we are using for the surface inspector (this is saved in the prefs)
struct si_globals_t
{
	float shift[2] = { 8.0f, 8.0f };
	float scale[2] = { 0.5f, 0.5f };
	float rotate = 45.0f;

	bool m_bSnapTToGrid = false;
};
extern si_globals_t g_si_globals;

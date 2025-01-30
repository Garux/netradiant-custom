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

#include "generic/callback.h"
#include "signal/signalfwd.h"
#include "math/vectorfwd.h"

class QWidget;
QWidget* TextureBrowser_constructWindow( QWidget* toplevel );
void TextureBrowser_destroyWindow();


const char* TextureBrowser_GetSelectedShader();

void TextureBrowser_Construct();
void TextureBrowser_Destroy();


typedef Callback<void(const char*)> StringImportCallback;

extern QWidget* g_page_textures;
void TextureBrowser_exportTitle( const StringImportCallback& importer );
typedef FreeCaller<void(const StringImportCallback&), TextureBrowser_exportTitle> TextureBrowserExportTitleCaller;


const Vector3& TextureBrowser_getBackgroundColour();
void TextureBrowser_setBackgroundColour( const Vector3& colour );

void TextureBrowser_addActiveShadersChangedCallback( const SignalHandler& handler );
void TextureBrowser_addShadersRealiseCallback( const SignalHandler& handler );

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

class QWidget;

void GroupDialog_Construct();
void GroupDialog_Destroy();

void GroupDialog_constructWindow( QWidget* main_window );
void GroupDialog_destroyWindow();
QWidget* GroupDialog_getWindow();
void GroupDialog_show();

inline void RawStringExport( const char* string, const StringImportCallback& importer ){
	importer( string );
}
typedef ConstPointerCaller<char, void(const StringImportCallback&), RawStringExport> RawStringExportCaller;
QWidget* GroupDialog_addPage( const char* tabLabel, QWidget* widget, const StringExportCallback& title );

void GroupDialog_showPage( QWidget* page );
void GroupDialog_updatePageTitle( QWidget* page );

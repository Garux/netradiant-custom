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

#if !defined( INCLUDED_GROUPDIALOG_H )
#define INCLUDED_GROUPDIALOG_H

#include <uilib/uilib.h>
#include "generic/callback.h"

void GroupDialog_Construct();
void GroupDialog_Destroy();

void GroupDialog_constructWindow( ui::Window main_window );
void GroupDialog_destroyWindow();
ui::Window GroupDialog_getWindow();
void GroupDialog_show();

inline void RawStringExport( const char* string, const StringImportCallback& importer ){
	importer( string );
}
typedef ConstPointerCaller1<char, const StringImportCallback&, RawStringExport> RawStringExportCaller;
ui::Widget GroupDialog_addPage( const char* tabLabel, ui::Widget widget, const StringExportCallback& title );

void GroupDialog_showPage( ui::Widget page );
void GroupDialog_updatePageTitle( ui::Widget page );
bool GroupDialog_isShown();
ui::Widget GroupDialog_getPage();

#endif

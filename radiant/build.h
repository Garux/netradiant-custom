/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#include <cstddef>
#include <vector>
#include "string/string.h"

void build_set_variable( const char* name, const char* value );
void build_clear_variables();

std::vector<CopiedString> build_construct_commands( size_t buildIdx );

void DoBuildMenu();

void BuildMenu_Construct();
void BuildMenu_Destroy();

void Build_constructMenu( class QMenu* menu );
extern QMenu* g_bsp_menu;

void Build_runRecentExecutedBuild();

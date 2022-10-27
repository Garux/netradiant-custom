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

/// \file
/// Qt file-chooser dialogs.

#include <QString>

const char* file_dialog( class QWidget *parent, bool open, const char* title, const char* path = nullptr, const char* pattern = nullptr, bool want_load = false, bool want_import = false, bool want_save = false );


/// \brief Prompts the user to browse for a directory.
/// The prompt window will be transient to \p parent.
/// The directory will initially default to \p path, which must be an absolute path.
QString dir_dialog( class QWidget *parent, const QString& path = {} );

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

#include <vector>
#include <QList>

/// @brief Controls default/save/restore gui settings.
/// Path strings must be unique and presumably string literals, since they aren't stored.
/// Widget pointers must stay valid till save().
/// QCoreApplication::organizationName(), QCoreApplication::applicationName() must be set before first use.
/// \todo  //. ?use QPointer for sanity
class GuiSettings
{
	std::vector<const class GuiSetting*> m_settings;
public:
	~GuiSettings();
	void save();
	// zeros are meant to use defaults
	void addWindow( class QWidget *window, const char *path, int w = 0, int h = 0, int x = 0, int y = 0 );
	void addMainWindow( class QMainWindow *window, const char *path );
	void addSplitter( class QSplitter *splitter, const char *path, const QList<int> &sizes );
};

inline GuiSettings g_guiSettings;

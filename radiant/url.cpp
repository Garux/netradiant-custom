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

#include "url.h"

#include "mainframe.h"
#include "gtkutil/messagebox.h"
#include <QDesktopServices>
#include <QUrl>

void OpenURL( const char *url ){
	// let's put a little comment
	globalOutputStream() << "OpenURL: " << url << '\n';
	// QUrl::fromUserInput appears to work well for urls and local paths with spaces
	// alternatively can prepend file:/// to the latter and use default QUrl contructor
	if ( !QDesktopServices::openUrl( QUrl::fromUserInput( url ) ) ) {
		qt_MessageBox( MainFrame_getWindow(), "Failed to launch browser!" );
	}
}

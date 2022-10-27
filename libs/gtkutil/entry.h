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

#include <cstdio>
#include <cstdlib>
#include <QLineEdit>

inline void entry_set_int( QLineEdit* entry, int i ){
	char buf[32];
	sprintf( buf, "%d", i );
	entry->setText( buf );
}

inline void entry_set_float( QLineEdit* entry, float f ){
	char buf[32];
	sprintf( buf, "%g", f );
	entry->setText( buf );
}

inline int entry_get_int( QLineEdit* entry ){
	return atoi( entry->text().toLatin1().constData() );
}

inline double entry_get_float( QLineEdit* entry ){
	return atof( entry->text().toLatin1().constData() );
}

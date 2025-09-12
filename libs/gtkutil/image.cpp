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

#include "image.h"

#include "os/file.h"
#include "os/path.h"
#include "string/string.h"
#include "stream/stringstream.h"


namespace
{
CopiedString g_bitmapsPath;
}

void BitmapsPath_set( const char* path ){
	g_bitmapsPath = path;
}

QPixmap new_local_image( const char* filename ){
	StringOutputStream fullpath( 256 );

	for( const auto *ext : { ".svg", ".png" } )
		if( file_exists( fullpath( g_bitmapsPath, PathExtensionless( filename ), ext ) ) )
			return QPixmap( fullpath.c_str() );

	return {};
}

QIcon new_local_icon( const char* filename ){
	StringOutputStream fullpath( 256 );

	for( const auto *ext : { ".svg", ".png", ".ico" } )
		if( file_exists( fullpath( g_bitmapsPath, PathExtensionless( filename ), ext ) ) )
			return QIcon( fullpath.c_str() );

	return {};
}

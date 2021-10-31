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

#include "os/path.h"
#include "qstringops.h"

/*
   ====================
   Extract file parts
   ====================
 */

/// \brief Returns a pointer to the last slash or to terminating null character if not found.
inline const char* path_get_last_separator( const char* path ){
	const char *end = path + strlen( path );
	const char *src = end;

	while ( src != path ){
		if( path_separator( *--src ) )
			return src;
	}
	return end;
}

inline char* path_get_last_separator( char* path ){
	return const_cast<char*>( path_get_last_separator( const_cast<const char*>( path ) ) );
}

/// \brief Appends trailing slash, unless \p path is empty or already has slash.
inline void path_add_slash( char *path ){
	char* end = path + strlen( path );
	if ( end != path && !path_separator( end[-1] ) )
		strcat( end, "/" );
}

/// \brief Appends or replaces .EXT part of \p path with \p extension.
inline void path_set_extension( char *path, const char *extension ){
	strcpy( path_get_filename_base_end( path ), extension );
}

//
// if path doesnt have a .EXT, append extension
// (extension should include the .)
//
inline void DefaultExtension( char *path, const char *extension ){
	char* ext = path_get_filename_base_end( path );
	if( strEmpty( ext ) )
		strcpy( ext, extension );
}


inline void StripFilename( char *path ){
	strClear( path_get_filename_start( path ) );
}

inline void StripExtension( char *path ){
	strClear( path_get_filename_base_end( path ) );
}


inline void FixDOSName( char *src ){
	for ( ; *src; ++src )
		if ( *src == '\\' )
			*src = '/';
}

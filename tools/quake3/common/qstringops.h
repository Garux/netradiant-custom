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

#include <cstring>
#include <cstdlib>
#include <cctype>

inline bool strEmpty( const char* string ){
	return *string == '\0';
}
inline bool strEmptyOrNull( const char* string ){
	return string == nullptr || *string == '\0';
}
inline void strClear( char* string ){
	*string = '\0';
}
inline char *strLower( char *string ){
	for( char *in = string; *in; ++in )
		*in = tolower( *in );
	return string;
}
inline char *copystring( const char *src ){	// version of strdup() with malloc()
	const size_t size = strlen( src ) + 1;
	return static_cast<char*>( memcpy( malloc( size ), src, size ) );
}
#ifdef WIN32
	#define Q_stricmp           stricmp
	#define Q_strnicmp          strnicmp
#else
	#define Q_stricmp           strcasecmp
	#define Q_strnicmp          strncasecmp
#endif
inline bool strEqual( const char* string, const char* other ){
	return strcmp( string, other ) == 0;
}
inline bool strnEqual( const char* string, const char* other, size_t n ){
	return strncmp( string, other, n ) == 0;
}
inline bool striEqual( const char* string, const char* other ){
	return Q_stricmp( string, other ) == 0;
}
inline bool strniEqual( const char* string, const char* other, size_t n ){
	return Q_strnicmp( string, other, n ) == 0;
}

inline bool strEqualPrefix( const char* string, const char* prefix ){
	return strnEqual( string, prefix, strlen( prefix ) );
}
inline bool striEqualPrefix( const char* string, const char* prefix ){
	return strniEqual( string, prefix, strlen( prefix ) );
}
inline bool strEqualSuffix( const char* string, const char* suffix ){
	const size_t stringLength = strlen( string );
	const size_t suffixLength = strlen( suffix );
	return ( suffixLength > stringLength )? false : strnEqual( string + stringLength - suffixLength, suffix, suffixLength );
}
inline bool striEqualSuffix( const char* string, const char* suffix ){
	const size_t stringLength = strlen( string );
	const size_t suffixLength = strlen( suffix );
	return ( suffixLength > stringLength )? false : strniEqual( string + stringLength - suffixLength, suffix, suffixLength );
}

//http://stackoverflow.com/questions/27303062/strstr-function-like-that-ignores-upper-or-lower-case
//chux: Somewhat tricky to match the corner cases of strstr() with inputs like "x","", "","x", "",""
inline const char *strIstr( const char* haystack, const char* needle ) {
	do {
		const char* h = haystack;
		const char* n = needle;
		while ( tolower( (unsigned char)*h ) == tolower( (unsigned char)*n ) && *n != '\0' ) {
			h++;
			n++;
		}
		if ( *n == '\0' ) {
			return haystack;
		}
	} while ( *haystack++ );
	return nullptr;
}

inline char *strIstr( char* haystack, const char* needle ) {
	return const_cast<char*>( strIstr( const_cast<const char*>( haystack ), needle ) );
}

/* strlcpy, strlcat versions */

/*
 * Copy src to string dst of size size. At most size-1 characters
 * will be copied. Always NUL terminates (unless size == 0).
 * Returns strlen(src); if retval >= size, truncation occurred.
 */
inline size_t strcpyQ( char* dest, const char* src, const size_t dest_size ) {
	const size_t src_len = strlen( src );
	if( src_len < dest_size )
		memcpy( dest, src, src_len + 1 );
	else if( dest_size != 0 ){
		memcpy( dest, src, dest_size - 1 );
		dest[dest_size - 1] = '\0';
	}
	return src_len;
}

inline size_t strcatQ( char* dest, const char* src, const size_t dest_size ) {
	const size_t dest_len = strlen( dest );
	return dest_len + strcpyQ( dest + dest_len, src, dest_size > dest_len? dest_size - dest_len : 0 );
}

inline size_t strncatQ( char* dest, const char* src, const size_t dest_size, const size_t src_len ) {
	const size_t dest_len = strlen( dest );
	const size_t ds_len = dest_len + src_len;
	if( ds_len < dest_size ){
		memcpy( dest + dest_len, src, src_len );
		dest[ds_len] = '\0';
	}
	else if( dest_len < dest_size ){
		memcpy( dest + dest_len, src, dest_size - dest_len - 1 );
		dest[dest_size - 1] = '\0';
	}
	return ds_len;
}

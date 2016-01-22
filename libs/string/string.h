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

#if !defined( INCLUDED_STRING_STRING_H )
#define INCLUDED_STRING_STRING_H

/// \file
/// C-style null-terminated-character-array string library.

#include <cstring>
#include <cctype>
#include <algorithm>

#include "memory/allocator.h"
#include "generic/arrayrange.h"

/// \brief Returns true if \p string length is zero.
/// O(1)
inline bool string_empty( const char* string ){
	return *string == '\0';
}

/// \brief Returns true if \p string length is not zero.
/// O(1)
inline bool string_not_empty( const char* string ){
	return !string_empty( string );
}

/// \brief Returns <0 if \p string is lexicographically less than \p other.
/// Returns >0 if \p string is lexicographically greater than \p other.
/// Returns 0 if \p string is lexicographically equal to \p other.
/// O(n)
inline int string_compare( const char* string, const char* other ){
	return std::strcmp( string, other );
}

/// \brief Returns true if \p string is lexicographically equal to \p other.
/// O(n)
inline bool string_equal( const char* string, const char* other ){
	return string_compare( string, other ) == 0;
}

/// \brief Returns true if [\p string, \p string + \p n) is lexicographically equal to [\p other, \p other + \p n).
/// O(n)
inline bool string_equal_n( const char* string, const char* other, std::size_t n ){
	return std::strncmp( string, other, n ) == 0;
}

/// \brief Returns true if \p string is lexicographically less than \p other.
/// O(n)
inline bool string_less( const char* string, const char* other ){
	return string_compare( string, other ) < 0;
}

/// \brief Returns <0 if \p string is lexicographically less than \p other after converting both to lower-case.
/// Returns >0 if \p string is lexicographically greater than \p other after converting both to lower-case.
/// Returns 0 if \p string is lexicographically equal to \p other after converting both to lower-case.
/// O(n)
inline int string_compare_nocase( const char* string, const char* other ){
#ifdef WIN32
	return _stricmp( string, other );
#else
	return strcasecmp( string, other );
#endif
}

/// \brief Returns <0 if [\p string, \p string + \p n) is lexicographically less than [\p other, \p other + \p n).
/// Returns >0 if [\p string, \p string + \p n) is lexicographically greater than [\p other, \p other + \p n).
/// Returns 0 if [\p string, \p string + \p n) is lexicographically equal to [\p other, \p other + \p n).
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline int string_compare_nocase_n( const char* string, const char* other, std::size_t n ){
#ifdef WIN32
	return _strnicmp( string, other, n );
#else
	return strncasecmp( string, other, n );
#endif
}

/// \brief Returns true if \p string is lexicographically equal to \p other.
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline bool string_equal_nocase( const char* string, const char* other ){
	return string_compare_nocase( string, other ) == 0;
}

/// \brief Returns true if [\p string, \p string + \p n) is lexicographically equal to [\p other, \p other + \p n).
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline bool string_equal_nocase_n( const char* string, const char* other, std::size_t n ){
	return string_compare_nocase_n( string, other, n ) == 0;
}

/// \brief Returns true if \p string is lexicographically less than \p other.
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline bool string_less_nocase( const char* string, const char* other ){
	return string_compare_nocase( string, other ) < 0;
}

/// \brief Returns the number of non-null characters in \p string.
/// O(n)
inline std::size_t string_length( const char* string ){
	return std::strlen( string );
}

/// \brief Returns true if the beginning of \p string is equal to \p prefix.
/// O(n)
inline bool string_equal_prefix( const char* string, const char* prefix ){
	return string_equal_n( string, prefix, string_length( prefix ) );
}

/// \brief Returns true if the ending of \p string is equal to \p suffix.
/// O(n)
inline bool string_equal_suffix( const char* string, const char* suffix){
	const char *s = string + string_length( string ) - string_length( suffix );
	return string_equal_n( s , suffix, string_length( suffix ) );
}

/// \brief Copies \p other into \p string and returns \p string.
/// Assumes that the space allocated for \p string is at least string_length(other) + 1.
/// O(n)
inline char* string_copy( char* string, const char* other ){
	return std::strcpy( string, other );
}

/// \brief Allocates a string buffer large enough to hold \p length characters, using \p allocator.
/// The returned buffer must be released with \c string_release using a matching \p allocator.
template<typename Allocator>
inline char* string_new( std::size_t length, Allocator& allocator ){
	return allocator.allocate( length + 1 );
}

/// \brief Deallocates the \p buffer large enough to hold \p length characters, using \p allocator.
template<typename Allocator>
inline void string_release( char* buffer, std::size_t length, Allocator& allocator ){
	allocator.deallocate( buffer, length + 1 );
}

/// \brief Returns a newly-allocated string which is a clone of \p other, using \p allocator.
/// The returned buffer must be released with \c string_release using a matching \p allocator.
template<typename Allocator>
inline char* string_clone( const char* other, Allocator& allocator ){
	char* copied = string_new( string_length( other ), allocator );
	std::strcpy( copied, other );
	return copied;
}


/// \brief Allocates a string buffer large enough to hold \p length characters.
/// The returned buffer must be released with \c string_release.
inline char* string_new( std::size_t length ){
	DefaultAllocator<char> allocator;
	return string_new( length, allocator );
}

/// \brief Deallocates the \p buffer large enough to hold \p length characters.
inline void string_release( char* string, std::size_t length ){
	DefaultAllocator<char> allocator;
	string_release( string, length, allocator );
}

/// \brief Returns a newly-allocated string which is a clone of \p other.
/// The returned buffer must be released with \c string_release.
inline char* string_clone( const char* other ){
	DefaultAllocator<char> allocator;
	return string_clone( other, allocator );
}

/// \brief A re-entrant string tokeniser similar to strchr.
class StringTokeniser
{
bool istoken( char c ) const {
	if ( strchr( m_delimiters, c ) != 0 ) {
		return false;
	}
	return true;
}
const char* advance(){
	const char* token = m_pos;
	bool intoken = true;
	while ( !string_empty( m_pos ) )
	{
		if ( !istoken( *m_pos ) ) {
			*m_pos = '\0';
			intoken = false;
		}
		else if ( !intoken ) {
			return token;
		}
		++m_pos;
	}
	return token;
}
std::size_t m_length;
char* m_string;
char* m_pos;
const char* m_delimiters;
public:
StringTokeniser( const char* string, const char* delimiters = " \n\r\t\v" ) :
	m_length( string_length( string ) ),
	m_string( string_copy( string_new( m_length ), string ) ),
	m_pos( m_string ),
	m_delimiters( delimiters ){
	while ( !string_empty( m_pos ) && !istoken( *m_pos ) )
	{
		++m_pos;
	}
}
~StringTokeniser(){
	string_release( m_string, m_length );
}
/// \brief Returns the next token or "" if there are no more tokens available.
const char* getToken(){
	return advance();
}
};

struct StringLessNoCase
{
	bool operator()( const std::string& x, const std::string& y ) const {
		return string_less_nocase( x.c_str(), y.c_str() );
	}

	bool operator()( const char* x, const char* y ) const {
		return string_less_nocase( x, y );
	}
};

typedef StringLessNoCase RawStringLessNoCase;

struct RawStringEqual
{
	bool operator()( const char* x, const char* y ) const {
		return string_equal( x, y );
	}
};

struct RawStringLess
{
	bool operator()( const char* x, const char* y ) const {
		return string_less( x, y );
	}
};
#endif

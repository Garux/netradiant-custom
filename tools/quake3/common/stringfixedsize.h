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

#if !defined( INCLUDED_STRINGFIXESIZE_H )
#define INCLUDED_STRINGFIXESIZE_H

#include "stream/textstream.h"
#include "string/string.h"
#include "cmdlib.h"


/// \brief A TextOutputStream which writes to a null terminated fixed lenght char array.
/// Similar to std::stringstream.
template<std::size_t SIZE>
class StringFixedSize : public TextOutputStream
{
	char m_string[SIZE];
	std::size_t m_length;
public:
	StringFixedSize() {
		clear();
	}
	std::size_t write( const char* buffer, std::size_t length ) override {
		if( m_length + length < SIZE ){
			for( auto i = length; i != 0; --i )
				m_string[m_length++] = *buffer++;
			strClear( &m_string[m_length] );
		}
		else{
			Error( "String '%s%.*s' overflow: length >= %d.", m_string, length, buffer, SIZE );
		}

		return length;
	}

	StringFixedSize& operator=( const char* string ){
		return operator()( string );
	}

	template<typename ... Args>
	StringFixedSize& operator()( Args&& ... args ){
		clear();
		( *this << ... << std::forward<Args>( args ) );
		return *this;
	}

	operator const char*() const {
		return c_str();
	}

	bool empty() const {
		return strEmpty( m_string );
	}
	const char* c_str() const {
		return m_string;
	}
	void clear() {
		strClear( m_string );
		m_length = 0;
	}
};

template<std::size_t SIZE, typename T>
inline StringFixedSize<SIZE>& operator<<( StringFixedSize<SIZE>& ostream, const T& t ) {
	return ostream_write( ostream, t );
}

using String64 = StringFixedSize<64>;


#endif

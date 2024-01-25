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

#include "stream/textstream.h"
#include "string/string.h"
#include "inout.h"


/// \brief A TextOutputStream which writes to a null terminated fixed length char array.
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
	template<typename ... Args, typename = std::enable_if_t<sizeof...(Args) != 1 || //prevent override of copy constructor
	             !std::is_same_v<StringFixedSize,
	                             std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>>>>
	explicit StringFixedSize( Args&& ... args ){
		operator()( std::forward<Args>( args ) ... );
	}
	std::size_t write( const char* buffer, std::size_t length ) override {
		if( m_length + length < SIZE ){
			std::copy_n( buffer, length, m_string + m_length );
			m_length += length;
			strClear( &m_string[m_length] );
		}
		else{
			Error( "String '%s%.*s' overflow: length >= %d.", m_string, length, buffer, SIZE );
		}

		return length;
	}

	void operator=( const char* string ){
		operator()( string );
	}

	template<typename ... Args>
	void operator()( Args&& ... args ){
		clear();
		( *this << ... << std::forward<Args>( args ) );
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

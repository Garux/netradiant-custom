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

#include "bytebool.h"

#include <cstdio>
#include <cstdlib>
#include <utility>


class void_ptr
{
	void *ptr;
public:
	void_ptr( void *p ) : ptr( p ) {}
	template<typename T>
	operator T*() const {
		return static_cast<T*>( ptr );
	}
};


class MemBuffer
{
	byte *m_data;
	size_t m_size;
public:
	MemBuffer() : m_data( nullptr ), m_size( 0 ) {}
	explicit MemBuffer( size_t size ) : m_data( new byte[ size + 1 ] ), m_size( size ) {
		m_data[m_size] = '\0';         // NOTE: when loading a file, you have to allocate one extra byte and set it to \0
	}
	MemBuffer( MemBuffer&& other ) noexcept : m_data( std::exchange( other.m_data, nullptr ) ), m_size( other.m_size ) {}
	MemBuffer& operator=( MemBuffer&& other ) noexcept {
		std::swap( m_data, other.m_data );
		std::swap( m_size, other.m_size );
		return *this;
	}
	~MemBuffer(){
		delete[] m_data;
	}
	void_ptr data() const {
		return m_data;
	}
	/// \return correct buffer size in bytes, if it's not empty. May be not used for validity check!
	size_t size() const {
		return m_size;
	}
	/// \return true, if there is managed buffer
	operator bool() const {
		return m_data != nullptr;
	}
	/// \brief Delegates the ownership. Obtained buffer must be deallocated by \c delete[]
	void_ptr release(){
		return std::exchange( m_data, nullptr );
	}
};

void_ptr safe_malloc( size_t size );
void_ptr safe_calloc( size_t size );

#define offsetof_array( TYPE, ARRAY_MEMBER, ARRAY_SIZE ) ( offsetof( TYPE, ARRAY_MEMBER[0] ) + sizeof( TYPE::ARRAY_MEMBER[0] ) * ARRAY_SIZE )

void Q_getwd( char *out );

int Q_filelength( FILE *f );

void    Q_mkdir( const char *path );

char *ExpandArg( const char *path );    // from cmd line


FILE    *SafeOpenWrite( const char *filename, const char *mode = "wb" );
FILE    *SafeOpenRead( const char *filename, const char *mode = "rb" );
void    SafeRead( FILE *f, MemBuffer& buffer );
void    SafeWrite( FILE *f, const void *buffer, int count );

/// \brief loads file from absolute \p filename path or emits \c Error
MemBuffer LoadFile( const char *filename );
void    SaveFile( const char *filename, const void *buffer, int count );
bool    FileExists( const char *filename );


short   BigShort( short l );
short   LittleShort( short l );
int     BigLong( int l );
int     LittleLong( int l );
float   BigFloat( float l );
float   LittleFloat( float l );

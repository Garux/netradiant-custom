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

// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__

#include "bytebool.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)     // truncate from double to float

#pragma check_stack(off)

#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#include "os/path.h"

#ifdef _MSC_VER

#pragma intrinsic( memset, memcpy )

#endif


#ifdef PATH_MAX
#define MAX_OS_PATH     PATH_MAX
#else
#define MAX_OS_PATH     4096
#endif
#define MEM_BLOCKSIZE 4096

#define SAFE_MALLOC
#ifdef SAFE_MALLOC

class void_ptr
{
private:
	void *ptr;
public:
	void_ptr() = delete;
	void_ptr( void *p ) : ptr( p ) {}
	template<typename T>
	operator T*() const {
		return static_cast<T*>( ptr );
	}
};

void_ptr safe_malloc( size_t size );
void_ptr safe_malloc_info( size_t size, const char* info );
void_ptr safe_calloc( size_t size );
void_ptr safe_calloc_info( size_t size, const char* info );
#else
#define safe_malloc( size ) malloc( size )
#define safe_malloc_info( size, info ) malloc( size )
#define safe_calloc( size ) calloc( 1, size )
#define safe_calloc_info( size, info ) calloc( 1, size )
#endif /* SAFE_MALLOC */

#define offsetof_array( TYPE, ARRAY_MEMBER, ARRAY_SIZE ) ( offsetof( TYPE, ARRAY_MEMBER[0] ) + sizeof( TYPE::ARRAY_MEMBER[0] ) * ARRAY_SIZE )

void Q_getwd( char *out );

int Q_filelength( FILE *f );
int FileTime( const char *path );

void    Q_mkdir( const char *path );

extern char qdir[1024];
extern char gamedir[1024];
extern char writedir[1024];
void SetQdirFromPath( const char *path );
char *ExpandArg( const char *path );    // from cmd line
char *ExpandPath( const char *path );   // from scripts
void ExpandWildcards( int *argc, char ***argv );


double I_FloatTime( void );

FILE    *SafeOpenWrite( const char *filename, const char *mode = "wb" );
FILE    *SafeOpenRead( const char *filename, const char *mode = "rb" );
void    SafeRead( FILE *f, void *buffer, int count );
void    SafeWrite( FILE *f, const void *buffer, int count );

int     LoadFile( const char *filename, void **bufferptr );
int   LoadFileBlock( const char *filename, void **bufferptr );
int     TryLoadFile( const char *filename, void **bufferptr );
void    SaveFile( const char *filename, const void *buffer, int count );
bool    FileExists( const char *filename );


const char* path_get_last_separator( const char* path );
      char* path_get_last_separator(       char* path );
void path_add_slash( char *path );
void path_set_extension( char *path, const char *extension );
void    DefaultExtension( char *path, const char *extension );
void    DefaultPath( char *path, const char *basepath );
void    StripFilename( char *path );
void    StripExtension( char *path );

static inline void FixDOSName( char *src ){
	for ( ; *src; ++src )
		if ( *src == '\\' )
			*src = '/';
}

void    ExtractFilePath( const char *path, char *dest );		// file directory with trailing slash
void    ExtractFileBase( const char *path, char *dest );		// file name w/o extension
void    ExtractFileExtension( const char *path, char *dest );

int     ParseNum( const char *str );

short   BigShort( short l );
short   LittleShort( short l );
int     BigLong( int l );
int     LittleLong( int l );
float   BigFloat( float l );
float   LittleFloat( float l );


void CRC_Init( unsigned short *crcvalue );
void CRC_ProcessByte( unsigned short *crcvalue, byte data );
unsigned short CRC_Value( unsigned short crcvalue );

void    CreatePath( const char *path );
void    QCopyFile( const char *from, const char *to );

// sleep for the given amount of milliseconds
void Sys_Sleep( int n );

// for compression routines
typedef struct
{
	void    *data;
	int count, width, height;
} cblock_t;


#endif

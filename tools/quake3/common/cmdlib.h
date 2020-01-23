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
void *safe_malloc( size_t size );
void *safe_malloc_info( size_t size, const char* info );
void *safe_calloc( size_t size );
void *safe_calloc_info( size_t size, const char* info );
#else
#define safe_malloc( size ) malloc( size )
#define safe_malloc_info( size, info ) malloc( size )
#define safe_calloc( size ) calloc( 1, size )
#define safe_calloc_info( size, info ) calloc( 1, size )
#endif /* SAFE_MALLOC */


static inline bool strEmpty( const char* string ){
	return *string == '\0';
}
static inline bool strEmptyOrNull( const char* string ){
	return string == NULL || *string == '\0';
}
static inline void strClear( char* string ){
	*string = '\0';
}
static inline char *strLower( char *string ){
	for( char *in = string; *in; ++in )
		*in = tolower( *in );
	return string;
}
static inline char *copystring( const char *src ){	// version of strdup() with safe_malloc()
	const size_t size = strlen( src ) + 1;
	return memcpy( safe_malloc( size ), src, size );
}
char* strIstr( const char* haystack, const char* needle );
#ifdef WIN32
	#define Q_stricmp           stricmp
	#define Q_strnicmp          strnicmp
#else
	#define Q_stricmp           strcasecmp
	#define Q_strnicmp          strncasecmp
#endif
static inline bool strEqual( const char* string, const char* other ){
	return strcmp( string, other ) == 0;
}
static inline bool strnEqual( const char* string, const char* other, size_t n ){
	return strncmp( string, other, n ) == 0;
}
static inline bool striEqual( const char* string, const char* other ){
	return Q_stricmp( string, other ) == 0;
}
static inline bool strniEqual( const char* string, const char* other, size_t n ){
	return Q_strnicmp( string, other, n ) == 0;
}

static inline bool strEqualPrefix( const char* string, const char* prefix ){
	return strnEqual( string, prefix, strlen( prefix ) );
}
static inline bool striEqualPrefix( const char* string, const char* prefix ){
	return strniEqual( string, prefix, strlen( prefix ) );
}
static inline bool strEqualSuffix( const char* string, const char* suffix ){
	const size_t stringLength = strlen( string );
	const size_t suffixLength = strlen( suffix );
	return ( suffixLength > stringLength )? false : strnEqual( string + stringLength - suffixLength, suffix, suffixLength );
}
static inline bool striEqualSuffix( const char* string, const char* suffix ){
	const size_t stringLength = strlen( string );
	const size_t suffixLength = strlen( suffix );
	return ( suffixLength > stringLength )? false : strniEqual( string + stringLength - suffixLength, suffix, suffixLength );
}
/* strlcpy, strlcat versions */
size_t strcpyQ( char* dest, const char* src, const size_t dest_size );
size_t strcatQ( char* dest, const char* src, const size_t dest_size );
size_t strncatQ( char* dest, const char* src, const size_t dest_size, const size_t src_len );

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

void    Error( const char *error, ... )
#ifdef __GNUC__
__attribute__( ( noreturn ) )
#endif
;

FILE    *SafeOpenWrite( const char *filename );
FILE    *SafeOpenRead( const char *filename );
void    SafeRead( FILE *f, void *buffer, int count );
void    SafeWrite( FILE *f, const void *buffer, int count );

int     LoadFile( const char *filename, void **bufferptr );
int   LoadFileBlock( const char *filename, void **bufferptr );
int     TryLoadFile( const char *filename, void **bufferptr );
void    SaveFile( const char *filename, const void *buffer, int count );
bool    FileExists( const char *filename );


static inline bool path_separator( const char c ){
	return c == '/' || c == '\\';
}
bool path_is_absolute( const char* path );
char* path_get_last_separator( const char* path );
char* path_get_filename_start( const char* path );
char* path_get_filename_base_end( const char* path );
char* path_get_extension( const char* path );
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

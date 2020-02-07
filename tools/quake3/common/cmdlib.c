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

// cmdlib.c
// TTimo 09/30/2000
// from an intial copy of common/cmdlib.c
// stripped out the Sys_Printf Sys_Printf stuff

// SPoG 05/27/2001
// merging alpha branch into trunk
// replaced qprintf with Sys_Printf

#include "cmdlib.h"
#include "mathlib.h"
#include "inout.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <direct.h>
#include <windows.h>
#endif

#if defined ( __linux__ ) || defined ( __APPLE__ )
#include <unistd.h>
#endif

#define BASEDIRNAME "quake"     // assumed to have a 2 or 3 following

#ifdef SAFE_MALLOC
// FIXME switch to -std=c99 or above to use proper %zu format specifier for size_t
void *safe_malloc( size_t size ){
	void *p = malloc( size );
	if ( !p ) {
		Error( "safe_malloc failed on allocation of %lu bytes", (unsigned long)size );
	}
	return p;
}

void *safe_malloc_info( size_t size, const char* info ){
	void *p = malloc( size );
	if ( !p ) {
		Error( "%s: safe_malloc failed on allocation of %lu bytes", info, (unsigned long)size );
	}
	return p;
}

void *safe_calloc( size_t size ){
	void *p = calloc( 1, size );
	if ( !p ) {
		Error( "safe_calloc failed on allocation of %lu bytes", (unsigned long)size );
	}
	return p;
}

void *safe_calloc_info( size_t size, const char* info ){
	void *p = calloc( 1, size );
	if ( !p ) {
		Error( "%s: safe_calloc failed on allocation of %lu bytes", info, (unsigned long)size );
	}
	return p;
}
#endif


/*
   ===================
   ExpandWildcards

   Mimic unix command line expansion
   ===================
 */
#define MAX_EX_ARGC 1024
int ex_argc;
char    *ex_argv[MAX_EX_ARGC];
#ifdef _WIN32
#include "io.h"
void ExpandWildcards( int *argc, char ***argv ){
	struct _finddata_t fileinfo;
	int handle;
	int i;
	char filename[1024];
	char filepath[1024];
	char    *path;

	ex_argc = 0;
	for ( i = 0 ; i < *argc ; i++ )
	{
		path = ( *argv )[i];
		if ( path[0] == '-'
			 || ( !strchr( path, '*' ) && !strchr( path, '?' ) ) ) {
			ex_argv[ex_argc++] = path;
			continue;
		}

		handle = _findfirst( path, &fileinfo );
		if ( handle == -1 ) {
			return;
		}

		ExtractFilePath( path, filepath );

		do
		{
			sprintf( filename, "%s%s", filepath, fileinfo.name );
			ex_argv[ex_argc++] = copystring( filename );
		} while ( _findnext( handle, &fileinfo ) != -1 );

		_findclose( handle );
	}

	*argc = ex_argc;
	*argv = ex_argv;
}
#else
void ExpandWildcards( int *argc, char ***argv ){
}
#endif

/*

   qdir will hold the path up to the quake directory, including the slash

   f:\quake\
   /raid/quake/

   gamedir will hold qdir + the game directory (id1, id2, etc)

 */

char qdir[1024];
char gamedir[1024];
char writedir[1024];

void SetQdirFromPath( const char *path ){
	const char  *c;
	const char *sep;
	int len, count;

	path = ExpandArg( path );

	// search for "quake2" in path

	len = strlen( BASEDIRNAME );
	for ( c = path + strlen( path ) - 1 ; c != path ; c-- )
	{
		if ( strniEqual( c, BASEDIRNAME, len ) ) {
			//
			//strncpy (qdir, path, c+len+2-path);
			// the +2 assumes a 2 or 3 following quake which is not the
			// case with a retail install
			// so we need to add up how much to the next separator
			sep = c + len;
			count = 1;
			while ( !strEmpty( sep ) && !path_separator( *sep ) )
			{
				sep++;
				count++;
			}
			strncpy( qdir, path, c + len + count - path );
			Sys_Printf( "qdir: %s\n", qdir );
			FixDOSName( qdir );

			c += len + count;
			while ( *c )
			{
				if ( path_separator( *c ) ) {
					strncpy( gamedir, path, c + 1 - path );
					FixDOSName( gamedir );
					Sys_Printf( "gamedir: %s\n", gamedir );

					if ( strEmpty( writedir ) ) {
						strcpy( writedir, gamedir );
					}
					else{
						path_add_slash( writedir );
					}

					return;
				}
				c++;
			}
			Error( "No gamedir in %s", path );
			return;
		}
	}
	Error( "SetQdirFromPath: no '%s' in %s", BASEDIRNAME, path );
}

char *ExpandArg( const char *path ){
	static char full[1024];

	if ( path_is_absolute( path ) ) {
		strcpy( full, path );
	}
	else{
		Q_getwd( full );
		strcat( full, path );
	}
	return full;
}

char *ExpandPath( const char *path ){
	static char full[1024];
	if ( path_is_absolute( path ) ) {
		strcpy( full, path );
	}
	else{
		sprintf( full, "%s%s", qdir, path );
	}
	return full;
}



/*
   ================
   I_FloatTime
   ================
 */
double I_FloatTime( void ){
	time_t t;

	time( &t );

	return t;
#if 0
// more precise, less portable
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday( &tp, &tzp );

	if ( !secbase ) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return ( tp.tv_sec - secbase ) + tp.tv_usec / 1000000.0;
#endif
}

void Q_getwd( char *out ){
#ifdef WIN32
	_getcwd( out, 256 );
#else
	// Gef: Changed from getwd() to getcwd() to avoid potential buffer overflow
	if ( !getcwd( out, 256 ) ) {
		strClear( out );
	}
#endif
	path_add_slash( out );
	FixDOSName( out );
}


void Q_mkdir( const char *path ){
	char parentbuf[256];
	const char *p = NULL;
	int retry = 2;
	while ( retry-- )
	{
#ifdef WIN32
		if ( _mkdir( path ) != -1 ) {
			return;
		}
#else
		if ( mkdir( path, 0777 ) != -1 ) {
			return;
		}
#endif
		if ( errno == ENOENT ) {
			p = path_get_last_separator( path );
		}
		if ( !strEmptyOrNull( p ) ) {
			strcpyQ( parentbuf, path, p - path + 1 );
			if ( ( p - path ) < (ptrdiff_t) sizeof( parentbuf ) ) {
				Sys_Printf( "mkdir: %s: creating parent %s first\n", path, parentbuf );
				Q_mkdir( parentbuf );
				continue;
			}
		}
		break;
	}
	if ( errno != EEXIST ) {
		Error( "mkdir %s: %s", path, strerror( errno ) );
	}
}

/*
   ============
   FileTime

   returns -1 if not present
   ============
 */
int FileTime( const char *path ){
	struct  stat buf;

	if ( stat( path,&buf ) == -1 ) {
		return -1;
	}

	return buf.st_mtime;
}



//http://stackoverflow.com/questions/27303062/strstr-function-like-that-ignores-upper-or-lower-case
//chux: Somewhat tricky to match the corner cases of strstr() with inputs like "x","", "","x", "",""
char *strIstr( const char* haystack, const char* needle ) {
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
	return NULL;
}

/*
 * Copy src to string dst of size size. At most size-1 characters
 * will be copied. Always NUL terminates (unless size == 0).
 * Returns strlen(src); if retval >= size, truncation occurred.
 */
size_t strcpyQ( char* dest, const char* src, const size_t dest_size ) {
	const size_t src_len = strlen( src );
	if( src_len < dest_size )
		memcpy( dest, src, src_len + 1 );
	else if( dest_size != 0 ){
		memcpy( dest, src, dest_size - 1 );
		dest[dest_size - 1] = '\0';
	}
	return src_len;
}

size_t strcatQ( char* dest, const char* src, const size_t dest_size ) {
	const size_t dest_len = strlen( dest );
	return dest_len + strcpyQ( dest + dest_len, src, dest_size > dest_len? dest_size - dest_len : 0 );
}

size_t strncatQ( char* dest, const char* src, const size_t dest_size, const size_t src_len ) {
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


/*
   =============================================================================

                        MISC FUNCTIONS

   =============================================================================
 */


/*
   ================
   Q_filelength
   ================
 */
int Q_filelength( FILE *f ){
	int pos;
	int end;

	pos = ftell( f );
	fseek( f, 0, SEEK_END );
	end = ftell( f );
	fseek( f, pos, SEEK_SET );

	return end;
}


FILE *SafeOpenWrite( const char *filename ){
	FILE    *f;

	f = fopen( filename, "wb" );

	if ( !f ) {
		Error( "Error opening %s: %s",filename,strerror( errno ) );
	}

	return f;
}

FILE *SafeOpenRead( const char *filename ){
	FILE    *f;

	f = fopen( filename, "rb" );

	if ( !f ) {
		Error( "Error opening %s: %s",filename,strerror( errno ) );
	}

	return f;
}


void SafeRead( FILE *f, void *buffer, int count ){
	if ( fread( buffer, 1, count, f ) != (size_t)count ) {
		Error( "File read failure" );
	}
}


void SafeWrite( FILE *f, const void *buffer, int count ){
	if ( fwrite( buffer, 1, count, f ) != (size_t)count ) {
		Error( "File write failure" );
	}
}


/*
   ==============
   FileExists
   ==============
 */
bool    FileExists( const char *filename ){
	FILE    *f;

	f = fopen( filename, "r" );
	if ( !f ) {
		return false;
	}
	fclose( f );
	return true;
}

/*
   ==============
   LoadFile
   ==============
 */
int    LoadFile( const char *filename, void **bufferptr ){
	FILE    *f;
	int length;
	void    *buffer;

	f = SafeOpenRead( filename );
	length = Q_filelength( f );
	buffer = safe_malloc( length + 1 );
	( (char *)buffer )[length] = 0;
	SafeRead( f, buffer, length );
	fclose( f );

	*bufferptr = buffer;
	return length;
}


/*
   ==============
   LoadFileBlock
   -
   rounds up memory allocation to 4K boundry
   -
   ==============
 */
int    LoadFileBlock( const char *filename, void **bufferptr ){
	FILE    *f;
	int length, nBlock, nAllocSize;
	void    *buffer;

	f = SafeOpenRead( filename );
	length = Q_filelength( f );
	nAllocSize = length;
	nBlock = nAllocSize % MEM_BLOCKSIZE;
	if ( nBlock > 0 ) {
		nAllocSize += MEM_BLOCKSIZE - nBlock;
	}
	buffer = safe_calloc( nAllocSize + 1 );
	SafeRead( f, buffer, length );
	fclose( f );

	*bufferptr = buffer;
	return length;
}


/*
   ==============
   TryLoadFile

   Allows failure
   ==============
 */
int    TryLoadFile( const char *filename, void **bufferptr ){
	FILE    *f;
	int length;
	void    *buffer;

	*bufferptr = NULL;

	f = fopen( filename, "rb" );
	if ( !f ) {
		return -1;
	}
	length = Q_filelength( f );
	buffer = safe_malloc( length + 1 );
	( (char *)buffer )[length] = 0;
	SafeRead( f, buffer, length );
	fclose( f );

	*bufferptr = buffer;
	return length;
}


/*
   ==============
   SaveFile
   ==============
 */
void    SaveFile( const char *filename, const void *buffer, int count ){
	FILE    *f;

	f = SafeOpenWrite( filename );
	SafeWrite( f, buffer, count );
	fclose( f );
}


/*
   ====================
   Extract file parts
   ====================
 */

/// \brief Returns true if \p path is a fully qualified file-system path.
bool path_is_absolute( const char* path ){
#if defined( WIN32 )
	return path[0] == '/'
		   || ( path[0] != '\0' && path[1] == ':' ); // local drive
#elif defined( POSIX )
	return path[0] == '/';
#endif
}

/// \brief Returns a pointer to the last slash or to terminating null character if not found.
char* path_get_last_separator( const char* path ){
	const char *end = path + strlen( path );
	const char *src = end;

	while ( src != path ){
		if( path_separator( *--src ) )
			return src;
	}
	return end;
}

/// \brief Returns a pointer to the first character of the filename component of \p path.
char* path_get_filename_start( const char* path ){
	const char *src = path + strlen( path );

	while ( src != path && !path_separator( src[-1] ) ){
		--src;
	}
	return src;
}

/// \brief Returns a pointer to the character after the end of the filename component of \p path - either the extension separator or the terminating null character.
char* path_get_filename_base_end( const char* path ){
	const char *end = path + strlen( path );
	const char *src = end;

	while ( src != path && !path_separator( *--src ) ){
		if( *src == '.' )
			return src;
	}
	return end;
}


/// \brief Returns a pointer to the first character of the file extension of \p path, or to terminating null character if not found.
char* path_get_extension( const char* path ){
	const char *end = path + strlen( path );
	const char *src = end;

	while ( src != path && !path_separator( *--src ) ){
		if( *src == '.' )
			return src + 1;
	}
	return end;
}

/// \brief Appends trailing slash, unless \p path is empty or already has slash.
void path_add_slash( char *path ){
	char* end = path + strlen( path );
	if ( end != path && !path_separator( end[-1] ) )
		strcat( end, "/" );
}

/// \brief Appends or replaces .EXT part of \p path with \p extension.
void path_set_extension( char *path, const char *extension ){
	strcpy( path_get_filename_base_end( path ), extension );
}

//
// if path doesnt have a .EXT, append extension
// (extension should include the .)
//
void DefaultExtension( char *path, const char *extension ){
	char* ext = path_get_filename_base_end( path );
	if( strEmpty( ext ) )
		strcpy( ext, extension );
}


void DefaultPath( char *path, const char *basepath ){
	if( !path_is_absolute( path ) ){
		char* temp = strdup( path );
		sprintf( path, "%s%s", basepath, temp );
		free( temp );
	}
}


void    StripFilename( char *path ){
	strClear( path_get_filename_start( path ) );
}

void    StripExtension( char *path ){
	strClear( path_get_filename_base_end( path ) );
}


// NOTE: includes the slash, otherwise
// backing to an empty path will be wrong when appending a slash
void ExtractFilePath( const char *path, char *dest ){
	strcpyQ( dest, path, path_get_filename_start( path ) - path + 1 ); // +1 for '\0'
}

void ExtractFileBase( const char *path, char *dest ){
	const char* start = path_get_filename_start( path );
	const char* end = path_get_filename_base_end( start );
	strcpyQ( dest, start, end - start + 1 ); // +1 for '\0'
}

void ExtractFileExtension( const char *path, char *dest ){
	strcpy( dest, path_get_extension( path ) );
}


/*
   ==============
   ParseNum / ParseHex
   ==============
 */
int ParseHex( const char *hex ){
	const char    *str;
	int num;

	num = 0;
	str = hex;

	while ( *str )
	{
		num <<= 4;
		if ( *str >= '0' && *str <= '9' ) {
			num += *str - '0';
		}
		else if ( *str >= 'a' && *str <= 'f' ) {
			num += 10 + *str - 'a';
		}
		else if ( *str >= 'A' && *str <= 'F' ) {
			num += 10 + *str - 'A';
		}
		else{
			Error( "Bad hex number: %s",hex );
		}
		str++;
	}

	return num;
}


int ParseNum( const char *str ){
	if ( str[0] == '$' ) {
		return ParseHex( str + 1 );
	}
	if ( str[0] == '0' && str[1] == 'x' ) {
		return ParseHex( str + 2 );
	}
	return atol( str );
}



/*
   ============================================================================

                    BYTE ORDER FUNCTIONS

   ============================================================================
 */

#ifdef _SGI_SOURCE
#define __BIG_ENDIAN__
#endif

#ifdef __BIG_ENDIAN__

short   LittleShort( short l ){
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short   BigShort( short l ){
	return l;
}


int    LittleLong( int l ){
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int    BigLong( int l ){
	return l;
}


float   LittleFloat( float l ){
	union {byte b[4]; float f; } in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float   BigFloat( float l ){
	return l;
}


#else


short   BigShort( short l ){
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short   LittleShort( short l ){
	return l;
}


int    BigLong( int l ){
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int    LittleLong( int l ){
	return l;
}

float   BigFloat( float l ){
	union {byte b[4]; float f; } in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float   LittleFloat( float l ){
	return l;
}


#endif


//=======================================================


// FIXME: byte swap?

// this is a 16 bit, non-reflected CRC using the polynomial 0x1021
// and the initial and final xor values shown below...  in other words, the
// CCITT standard CRC used by XMODEM

#define CRC_INIT_VALUE  0xffff
#define CRC_XOR_VALUE   0x0000

static unsigned short crctable[256] =
{
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

void CRC_Init( unsigned short *crcvalue ){
	*crcvalue = CRC_INIT_VALUE;
}

void CRC_ProcessByte( unsigned short *crcvalue, byte data ){
	*crcvalue = ( *crcvalue << 8 ) ^ crctable[( *crcvalue >> 8 ) ^ data];
}

unsigned short CRC_Value( unsigned short crcvalue ){
	return crcvalue ^ CRC_XOR_VALUE;
}
//=============================================================================

/*
   ============
   CreatePath
   ============
 */
void    CreatePath( const char *path ){
	const char  *ofs;
	char dir[1024];

#ifdef _WIN32
	int olddrive = -1;

	if ( path[1] == ':' ) {
		olddrive = _getdrive();
		_chdrive( toupper( path[0] ) - 'A' + 1 );
	}
#endif

	if ( path[1] == ':' ) {
		path += 2;
	}

	for ( ofs = path + 1 ; *ofs ; ofs++ )
	{
		if ( path_separator( *ofs ) ) { // create the directory
			memcpy( dir, path, ofs - path );
			dir[ ofs - path ] = 0;
			Q_mkdir( dir );
		}
	}

#ifdef _WIN32
	if ( olddrive != -1 ) {
		_chdrive( olddrive );
	}
#endif
}


/*
   ============
   QCopyFile

   Used to archive source files
   ============
 */
void QCopyFile( const char *from, const char *to ){
	void    *buffer;
	int length;

	length = LoadFile( from, &buffer );
	CreatePath( to );
	SaveFile( to, buffer, length );
	free( buffer );
}

void Sys_Sleep( int n ){
#ifdef WIN32
	Sleep( n );
#endif
#if defined ( __linux__ ) || defined ( __APPLE__ )
	usleep( n * 1000 );
#endif
}

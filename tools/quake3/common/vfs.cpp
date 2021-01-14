/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Rules:
//
// - Directories should be searched in the following order: ~/.q3a/baseq3,
//   install dir (/usr/local/games/quake3/baseq3) and cd_path (/mnt/cdrom/baseq3).
//
// - Pak files are searched first inside the directories.
// - Case insensitive.
// - Unix-style slashes (/) (windows is backwards .. everyone knows that)
//
// Leonardo Zide (leo@lokigames.com)
//

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "cmdlib.h"
#include "filematch.h"
#include "mathlib.h"
#include "inout.h"
#include "vfs.h"
#include "unzip.h"
#include "miniz.h"

typedef struct
{
	char* unzFilePath;
	char*   name;
	unz_s zipinfo;
	unzFile zipfile;
	guint32 size;
} VFS_PAKFILE;

// =============================================================================
// Global variables

static GSList*  g_unzFiles;
static GSList*  g_pakFiles;
static char g_strDirs[VFS_MAXDIRS][PATH_MAX + 1];
static int g_numDirs;
char g_strForbiddenDirs[VFS_MAXDIRS][PATH_MAX + 1];
int g_numForbiddenDirs = 0;
static gboolean g_bUsePak = TRUE;
char g_strLoadedFileLocation[1024];

// =============================================================================
// Static functions

//!\todo Define globally or use heap-allocated string.
#define NAME_MAX 255

static void vfsInitPakFile( const char *filename ){
	unz_global_info gi;
	unzFile uf;
	guint32 i;
	int err;

	uf = unzOpen( filename );
	if ( uf == NULL ) {
		return;
	}

	g_unzFiles = g_slist_append( g_unzFiles, uf );

	err = unzGetGlobalInfo( uf,&gi );
	if ( err != UNZ_OK ) {
		return;
	}
	unzGoToFirstFile( uf );

	char* unzFilePath = strdup( filename );

	for ( i = 0; i < gi.number_entry; i++ )
	{
		char filename_inzip[NAME_MAX];
		unz_file_info file_info;

		err = unzGetCurrentFileInfo( uf, &file_info, filename_inzip, sizeof( filename_inzip ), NULL, 0, NULL, 0 );
		if ( err != UNZ_OK ) {
			break;
		}

		VFS_PAKFILE* file = safe_malloc( sizeof( VFS_PAKFILE ) );
		g_pakFiles = g_slist_append( g_pakFiles, file );

		FixDOSName( filename_inzip );
		g_strdown( filename_inzip );

		file->name = strdup( filename_inzip );
		file->size = file_info.uncompressed_size;
		file->zipfile = uf;
		file->unzFilePath = unzFilePath;
		memcpy( &file->zipinfo, uf, sizeof( unz_s ) );

		if ( ( i + 1 ) < gi.number_entry ) {
			err = unzGoToNextFile( uf );
			if ( err != UNZ_OK ) {
				break;
			}
		}
	}
}

// =============================================================================
// Global functions

// reads all pak files from a dir
void vfsInitDirectory( const char *path ){
	char filename[PATH_MAX];
	GDir *dir;
	int j;

	for ( j = 0; j < g_numForbiddenDirs; ++j )
	{
		char* dbuf = strdup( path );
		if ( !strEmpty( dbuf ) && path_separator( dbuf[strlen( dbuf ) - 1] ) ) // del trailing slash
			strClear( &dbuf[strlen( dbuf ) - 1] );
		if ( matchpattern( path_get_filename_start( dbuf ), g_strForbiddenDirs[j], TRUE ) ) {
			free( dbuf );
			return;
		}
		free( dbuf );
	}

	if ( g_numDirs == VFS_MAXDIRS ) {
		return;
	}

	Sys_Printf( "VFS Init: %s\n", path );

	strncpy( g_strDirs[g_numDirs], path, PATH_MAX );
	g_strDirs[g_numDirs][PATH_MAX] = 0;
	FixDOSName( g_strDirs[g_numDirs] );
	path_add_slash( g_strDirs[g_numDirs] );
	g_numDirs++;

	if ( g_bUsePak ) {
		dir = g_dir_open( path, 0, NULL );

		if ( dir != NULL ) {
			const char* name;
			while ( ( name = g_dir_read_name( dir ) ) )
			{
				for ( j = 0; j < g_numForbiddenDirs; ++j )
				{
					if ( matchpattern( path_get_filename_start( name ), g_strForbiddenDirs[j], TRUE ) ) {
						break;
					}
				}
				if ( j < g_numForbiddenDirs ) {
					continue;
				}

				const char *ext = path_get_filename_base_end( name );

				if ( striEqual( ext, ".pk3" ) ) {
					sprintf( filename, "%s/%s", path, name );
					vfsInitPakFile( filename );
				}
				else if ( striEqual( ext, ".pk3dir" ) ) {
					if ( g_numDirs == VFS_MAXDIRS ) {
						continue;
					}
					snprintf( g_strDirs[g_numDirs], PATH_MAX, "%s/%s", path, name );
					FixDOSName( g_strDirs[g_numDirs] );
					path_add_slash( g_strDirs[g_numDirs] );
					++g_numDirs;
				}
			}
			g_dir_close( dir );
		}
	}
}


// lists all .shader files
void vfsListShaderFiles( StrList* list, void pushStringCallback( StrList* list, const char* string ) ){
	GDir *dir;
	char path[NAME_MAX];
/* search in dirs */
	for ( int i = 0; i < g_numDirs; i++ ){
		snprintf( path, NAME_MAX, "%sscripts/", g_strDirs[ i ] );

		dir = g_dir_open( path, 0, NULL );

		if ( dir != NULL ) {
			const char* name;
			while ( ( name = g_dir_read_name( dir ) ) )
			{
				if ( striEqual( path_get_filename_base_end( name ), ".shader" ) ) {
					pushStringCallback( list, name );
				}
			}
			g_dir_close( dir );
		}
	}
	/* search in packs */
	GSList *lst;

	for ( lst = g_pakFiles; lst != NULL; lst = g_slist_next( lst ) )
	{
		VFS_PAKFILE* file = (VFS_PAKFILE*)lst->data;
		if ( striEqual( path_get_filename_base_end( file->name ), ".shader" ) ) {
			pushStringCallback( list, path_get_filename_start( file->name ) );
		}
	}
}

// frees all memory that we allocated
void vfsShutdown(){
	while ( g_unzFiles )
	{
		unzClose( (unzFile)g_unzFiles->data );
		g_unzFiles = g_slist_remove( g_unzFiles, g_unzFiles->data );
	}

	while ( g_pakFiles )
	{
		VFS_PAKFILE* file = (VFS_PAKFILE*)g_pakFiles->data;
		free( file->unzFilePath );
		free( file->name );
		free( file );
		g_pakFiles = g_slist_remove( g_pakFiles, file );
	}
}

// return the number of files that match
int vfsGetFileCount( const char *filename ){
	int i, count = 0;
	char fixed[NAME_MAX], tmp[NAME_MAX];
	GSList *lst;

	strcpy( fixed, filename );
	FixDOSName( fixed );
	g_strdown( fixed );

	for ( lst = g_pakFiles; lst != NULL; lst = g_slist_next( lst ) )
	{
		VFS_PAKFILE* file = (VFS_PAKFILE*)lst->data;

		if ( strEqual( file->name, fixed ) ) {
			count++;
		}
	}

	for ( i = 0; i < g_numDirs; i++ )
	{
		strcpy( tmp, g_strDirs[i] );
		strcat( tmp, fixed );
		if ( access( tmp, R_OK ) == 0 ) {
			count++;
		}
	}

	return count;
}

// NOTE: when loading a file, you have to allocate one extra byte and set it to \0
int vfsLoadFile( const char *filename, void **bufferptr, int index ){
	int i, count = 0;
	char tmp[NAME_MAX], fixed[NAME_MAX];
	GSList *lst;

	// filename is a full path
	if ( index == -1 ) {
		strcpy( g_strLoadedFileLocation, filename );

		FILE *f = fopen( filename, "rb" );
		if ( f == NULL ) {
			return -1;
		}

		fseek( f, 0, SEEK_END );
		const long len = ftell( f );
		rewind( f );

		*bufferptr = safe_malloc( len + 1 );

		if ( fread( *bufferptr, 1, len, f ) != (size_t) len ) {
			fclose( f );
			return -1;
		}
		fclose( f );

		// we need to end the buffer with a 0
		( (char*) ( *bufferptr ) )[len] = 0;

		return len;
	}

	*bufferptr = NULL;
	strcpy( fixed, filename );
	FixDOSName( fixed );
	g_strdown( fixed );

	for ( i = 0; i < g_numDirs; i++ )
	{
		strcpy( tmp, g_strDirs[i] );
		strcat( tmp, filename );
		if ( access( tmp, R_OK ) == 0 ) {
			if ( count == index ) {
				strcpy( g_strLoadedFileLocation, tmp );

				FILE *f = fopen( tmp, "rb" );
				if ( f == NULL ) {
					return -1;
				}

				fseek( f, 0, SEEK_END );
				const long len = ftell( f );
				rewind( f );

				*bufferptr = safe_malloc( len + 1 );

				if ( fread( *bufferptr, 1, len, f ) != (size_t) len ) {
					fclose( f );
					return -1;
				}
				fclose( f );

				// we need to end the buffer with a 0
				( (char*) ( *bufferptr ) )[len] = 0;

				return len;
			}

			count++;
		}
	}

	for ( lst = g_pakFiles; lst != NULL; lst = g_slist_next( lst ) )
	{
		VFS_PAKFILE* file = (VFS_PAKFILE*)lst->data;

		if ( !strEqual( file->name, fixed ) ) {
			continue;
		}

		if ( count == index ) {
			snprintf( g_strLoadedFileLocation, sizeof( g_strLoadedFileLocation ), "%s :: %s", file->unzFilePath, filename );

			memcpy( file->zipfile, &file->zipinfo, sizeof( unz_s ) );

			if ( unzOpenCurrentFile( file->zipfile ) != UNZ_OK ) {
				return -1;
			}

			*bufferptr = safe_malloc( file->size + 1 );
			// we need to end the buffer with a 0
			( (char*) ( *bufferptr ) )[file->size] = 0;

			i = unzReadCurrentFile( file->zipfile, *bufferptr, file->size );
			unzCloseCurrentFile( file->zipfile );
			if ( i < 0 ) {
				return -1;
			}
			else{
				return file->size;
			}
		}

		count++;
	}

	return -1;
}




bool vfsPackFile( const char *filename, const char *packname, const int compLevel ){
	int i;
	char tmp[NAME_MAX], fixed[NAME_MAX];
	GSList *lst;

	byte *bufferptr = NULL;
	strcpy( fixed, filename );
	FixDOSName( fixed );
	g_strdown( fixed );

	for ( i = 0; i < g_numDirs; i++ )
	{
		strcpy( tmp, g_strDirs[i] );
		strcat( tmp, filename );
		if ( access( tmp, R_OK ) == 0 ) {
			if ( access( packname, R_OK ) == 0 ) {
				mz_zip_archive zip;
				memset( &zip, 0, sizeof(zip) );
				mz_zip_reader_init_file( &zip, packname, 0 );
				mz_zip_writer_init_from_reader( &zip, packname );

				mz_bool success = MZ_TRUE;
				success &= mz_zip_writer_add_file( &zip, filename, tmp, 0, 0, compLevel );
				if ( !success || !mz_zip_writer_finalize_archive( &zip ) ){
					Error( "Failed creating zip archive \"%s\"!\n", packname );
				}
				mz_zip_reader_end( &zip);
				mz_zip_writer_end( &zip );
			}
			else{
				mz_zip_archive zip;
				memset( &zip, 0, sizeof(zip) );
				if( !mz_zip_writer_init_file( &zip, packname, 0 ) ){
					Error( "Failed creating zip archive \"%s\"!\n", packname );
				}
				mz_bool success = MZ_TRUE;
				success &= mz_zip_writer_add_file( &zip, filename, tmp, 0, 0, compLevel );
				if ( !success || !mz_zip_writer_finalize_archive( &zip ) ){
					Error( "Failed creating zip archive \"%s\"!\n", packname );
				}
				mz_zip_writer_end( &zip );
			}

			return true;
		}
	}

	for ( lst = g_pakFiles; lst != NULL; lst = g_slist_next( lst ) )
	{
		VFS_PAKFILE* file = (VFS_PAKFILE*)lst->data;

		if ( !strEqual( file->name, fixed ) ) {
			continue;
		}

		memcpy( file->zipfile, &file->zipinfo, sizeof( unz_s ) );

		if ( unzOpenCurrentFile( file->zipfile ) != UNZ_OK ) {
			return false;
		}

		bufferptr = safe_malloc( file->size + 1 );
		// we need to end the buffer with a 0
		bufferptr[file->size] = 0;

		i = unzReadCurrentFile( file->zipfile, bufferptr, file->size );
		unzCloseCurrentFile( file->zipfile );
		if ( i < 0 ) {
			return false;
		}
		else{
			mz_bool success = MZ_TRUE;
			success &= mz_zip_add_mem_to_archive_file_in_place_with_time( packname, filename, bufferptr, i, 0, 0, compLevel, file->zipinfo.cur_file_info.dosDate );
				if ( !success ){
					Error( "Failed creating zip archive \"%s\"!\n", packname );
				}
			free( bufferptr );
			return true;
		}
	}

	return false;
}

bool vfsPackFile_Absolute_Path( const char *filepath, const char *filename, const char *packname, const int compLevel ){
	char tmp[NAME_MAX];
	strcpy( tmp, filepath );
	if ( access( tmp, R_OK ) == 0 ) {
		if ( access( packname, R_OK ) == 0 ) {
			mz_zip_archive zip;
			memset( &zip, 0, sizeof(zip) );
			mz_zip_reader_init_file( &zip, packname, 0 );
			mz_zip_writer_init_from_reader( &zip, packname );

			mz_bool success = MZ_TRUE;
			success &= mz_zip_writer_add_file( &zip, filename, tmp, 0, 0, compLevel );
			if ( !success || !mz_zip_writer_finalize_archive( &zip ) ){
				Error( "Failed creating zip archive \"%s\"!\n", packname );
			}
			mz_zip_reader_end( &zip);
			mz_zip_writer_end( &zip );
		}
		else{
			mz_zip_archive zip;
			memset( &zip, 0, sizeof(zip) );
			if( !mz_zip_writer_init_file( &zip, packname, 0 ) ){
				Error( "Failed creating zip archive \"%s\"!\n", packname );
			}
			mz_bool success = MZ_TRUE;
			success &= mz_zip_writer_add_file( &zip, filename, tmp, 0, 0, compLevel );
			if ( !success || !mz_zip_writer_finalize_archive( &zip ) ){
				Error( "Failed creating zip archive \"%s\"!\n", packname );
			}
			mz_zip_writer_end( &zip );
		}

		return true;
	}

	return false;
}

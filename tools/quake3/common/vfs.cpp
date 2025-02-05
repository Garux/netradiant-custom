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
   DIRECT INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
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

#if defined ( __linux__ ) || defined ( __APPLE__ )
#include <dirent.h>
#include <unistd.h>
#else
#include <io.h>

#ifndef R_OK
#define R_OK 04
#endif

#endif

#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <glib.h>

#include "cmdlib.h"
#include "qstringops.h"
#include "qpathops.h"
#include "filematch.h"
#include "inout.h"
#include "vfs.h"
#include "unzip.h"
#include "miniz.h"

#include "stream/stringstream.h"
#include "stream/textstream.h"
#include <forward_list>

struct VFS_PAK
{
	unzFile zipfile;
	const CopiedString unzFilePath;
	VFS_PAK( unzFile zipfile, const char *unzFilePath ) : zipfile( zipfile ), unzFilePath( unzFilePath ) {};
	VFS_PAK( VFS_PAK&& ) noexcept = delete;
	~VFS_PAK(){
		unzClose( zipfile );
	}
};

struct VFS_PAKFILE
{
	const CopiedString   name;
	const unz_s zipinfo;
	VFS_PAK& pak;
	const unsigned long size;
};

// =============================================================================
// Global variables

static std::forward_list<VFS_PAK>  g_paks;
static std::forward_list<VFS_PAKFILE>  g_pakFiles;
static std::vector<CopiedString> g_strDirs;
std::vector<CopiedString> g_strForbiddenDirs;
static constexpr bool g_bUsePak = true;
char g_strLoadedFileLocation[1024];

// =============================================================================
// Static functions

static void vfsInitPakFile( const char *filename ){
	unzFile uf = unzOpen( filename );
	if ( uf != NULL ) {
		VFS_PAK& pak = g_paks.emplace_front( uf, filename );

		if ( unzGoToFirstFile( uf ) == UNZ_OK ) {
			do {
				char filename_inzip[256];
				unz_file_info file_info;

				if ( unzGetCurrentFileInfo( uf, &file_info, filename_inzip, std::size( filename_inzip ), NULL, 0, NULL, 0 ) != UNZ_OK ) {
					break;
				}

				if( file_info.size_filename < std::size( filename_inzip ) ) {
					FixDOSName( filename_inzip );
					strLower( filename_inzip );

					g_pakFiles.emplace_front( VFS_PAKFILE{
						filename_inzip,
						*(unz_s*)uf,
						pak,
						file_info.uncompressed_size
					} );
				}
			} while( unzGoToNextFile( uf ) == UNZ_OK );
		}
	}
}

// =============================================================================
// Global functions

// reads all pak files from a dir
void vfsInitDirectory( const char *path ){
	GDir *dir;

	const auto path_is_forbidden = []( const char *path ){
		for ( const auto& forbidden : g_strForbiddenDirs )
			if ( matchpattern( path_get_filename_start( path ), forbidden.c_str(), TRUE ) )
				return true;
		return false;
	};

	{
		StringBuffer buf( path );
		if ( !buf.empty() && path_separator( buf.back() ) ) // del trailing slash
			buf.pop_back();
		if ( path_is_forbidden( buf.c_str() ) )
			return;
	}

	Sys_Printf( "VFS Init: %s\n", path );

	// clean and store copy to be safe of original's reallocation
	const CopiedString pathCleaned = g_strDirs.emplace_back( StringStream( DirectoryCleaned( path ) ) );

	if ( g_bUsePak ) {
		dir = g_dir_open( path, 0, NULL );

		if ( dir != NULL ) {
			std::vector<StringOutputStream> paks;
			const char* name;
			while ( ( name = g_dir_read_name( dir ) ) )
			{
				if ( path_is_forbidden( name ) )
					continue;

				if ( path_extension_is( name, "pk3" ) ) {
					paks.push_back( StringStream( pathCleaned, name ) );
				}
				else if ( path_extension_is( name, "pk3dir" ) ) {
					g_strDirs.emplace_back( StringStream( pathCleaned, name, '/' ) );
				}
			}
			g_dir_close( dir );

			// sort paks in ascending order
			// pakFiles are then prepended to the list, reversing the order
			// thus later (zzz) pak content have priority over earlier, just like in engine
			std::sort( paks.begin(), paks.end(),
			[]( const char* a, const char* b ){
				return string_compare_nocase_upper( a, b ) < 0;
			} );
			for( const char* pak : paks ){
				vfsInitPakFile( pak );
			}
		}
	}
}


// lists all unique .shader files with extension and w/o path
std::vector<CopiedString> vfsListShaderFiles( const char *shaderPath ){
	std::vector<CopiedString> list;
	const auto insert = [&list]( const char *name ){
		for( const CopiedString& str : list )
			if( striEqual( str.c_str(), name ) )
				return;
		list.emplace_back( name );
	};
	/* search in dirs */
	for ( const auto& strdir : g_strDirs ){
		GDir *dir = g_dir_open( StringStream( strdir, shaderPath, '/' ), 0, NULL );

		if ( dir != NULL ) {
			const char* name;
			while ( ( name = g_dir_read_name( dir ) ) )
			{
				if ( path_extension_is( name, "shader" ) ) {
					insert( name );
				}
			}
			g_dir_close( dir );
		}
	}
	/* search in packs */
	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		const char *name = file.name.c_str();
		if ( path_extension_is( name, "shader" )
		  && strniEqual( name, shaderPath, path_get_last_separator( name ) - name ) ) {
			insert( path_get_filename_start( name ) );
		}
	}

	return list;
}

// frees all memory that we allocated
void vfsShutdown(){
	g_paks.clear();
	g_pakFiles.clear();
}

// return the number of files that match
int vfsGetFileCount( const char *filename ){
	int count = 0;
	auto fixedname = StringStream<64>( PathCleaned( filename ) );

	for ( const auto& dir : g_strDirs )
	{
		if ( FileExists( StringStream( dir, fixedname ) ) ) {
			++count;
		}
	}

	strLower( fixedname.c_str() );

	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		if ( strEqual( file.name.c_str(), fixedname ) ) {
			++count;
		}
	}

	return count;
}

// NOTE: when loading a file, you have to allocate one extra byte and set it to \0
MemBuffer vfsLoadFile( const char *filename, int index /* = 0 */ ){

	const auto load_full_path = [] ( const char *filename ) -> MemBuffer
	{
		strcpy( g_strLoadedFileLocation, filename );

		MemBuffer buffer;

		FILE *f = fopen( filename, "rb" );
		if ( f != NULL ) {
			fseek( f, 0, SEEK_END );
			buffer = MemBuffer( ftell( f ) );
			rewind( f );

			if ( fread( buffer.data(), 1, buffer.size(), f ) != buffer.size() ) {
				 buffer = MemBuffer();
			}
			fclose( f );
		}

		return buffer;
	};

	auto fixedname = StringStream<64>( PathCleaned( filename ) );

	// filename is a full path
	if ( index == -1 ) {
		return load_full_path( fixedname );
	}

	for ( const auto& dir : g_strDirs )
	{
		const auto fullpath = StringStream( dir, fixedname );
		if ( FileExists( fullpath ) && 0 == index-- ) {
			return load_full_path( fullpath );
		}
	}

	strLower( fixedname.c_str() );

	MemBuffer buffer;

	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		if ( strEqual( file.name.c_str(), fixedname ) && 0 == index-- )
		{
			std::snprintf( g_strLoadedFileLocation, std::size( g_strLoadedFileLocation ), "%s :: %s", file.pak.unzFilePath.c_str(), filename );

			unzFile zipfile = file.pak.zipfile;
			*(unz_s*)zipfile = file.zipinfo;

			if ( unzOpenCurrentFile( zipfile ) == UNZ_OK ) {
				buffer = MemBuffer( file.size );

				if ( unzReadCurrentFile( zipfile, buffer.data(), file.size ) < 0 ) {
					buffer = MemBuffer();
				}
				unzCloseCurrentFile( zipfile );
			}
			return buffer;
		}
	}

	return buffer;
}




bool vfsPackFile( const char *filename, const char *packname, const int compLevel ){
	for ( const auto& dir : g_strDirs )
	{
		if( vfsPackFile_Absolute_Path( StringStream( dir, filename ), filename, packname, compLevel ) )
			return true;
	}

	auto fixed = StringStream<64>( PathCleaned( filename ) );
	strLower( fixed.c_str() );

	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		if ( strEqual( file.name.c_str(), fixed ) )
		{
			unzFile zipfile = file.pak.zipfile;
			*(unz_s*)zipfile = file.zipinfo;

			if ( unzOpenCurrentFile( zipfile ) == UNZ_OK ) {
				MemBuffer buffer( file.size );

				const int size = unzReadCurrentFile( zipfile, buffer.data(), file.size );
				unzCloseCurrentFile( zipfile );
				if ( size >= 0 ) {
					if ( !mz_zip_add_mem_to_archive_file_in_place_with_time( packname, filename, buffer.data(), size, 0, 0, compLevel, file.zipinfo.cur_file_info.dosDate ) ){
						Error( "Failed creating zip archive \"%s\"!\n", packname );
					}
					return true;
				}
			}
			return false;
		}
	}

	return false;
}

bool vfsPackFile_Absolute_Path( const char *filepath, const char *filename, const char *packname, const int compLevel ){
	if ( FileExists( filepath ) ) {
		if ( FileExists( packname ) ) {
			mz_zip_archive zip;
			memset( &zip, 0, sizeof( zip ) );

			if ( !mz_zip_reader_init_file( &zip, packname, 0 )
			  || !mz_zip_writer_init_from_reader( &zip, packname )
			  || !mz_zip_writer_add_file( &zip, filename, filepath, 0, 0, compLevel )
			  || !mz_zip_writer_finalize_archive( &zip ) ){
				Error( "Failed creating zip archive \"%s\"!\n", packname );
			}
			mz_zip_reader_end( &zip);
			mz_zip_writer_end( &zip );
		}
		else{
			mz_zip_archive zip;
			memset( &zip, 0, sizeof( zip ) );

			if ( !mz_zip_writer_init_file( &zip, packname, 0 )
			  || !mz_zip_writer_add_file( &zip, filename, filepath, 0, 0, compLevel )
			  || !mz_zip_writer_finalize_archive( &zip ) ){
				Error( "Failed creating zip archive \"%s\"!\n", packname );
			}
			mz_zip_writer_end( &zip );
		}

		return true;
	}

	return false;
}

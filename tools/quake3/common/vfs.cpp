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
	// unzFile zipfile;
	const CopiedString unzFilePath;
	VFS_PAK( const char *unzFilePath ) : unzFilePath( unzFilePath ) {};
	VFS_PAK( VFS_PAK&& ) noexcept = delete;
	~VFS_PAK(){
		// unzClose( zipfile );
	}
};

struct VFS_PAKFILE
{
	const CopiedString   name;
	const unz_s zipinfo;
	VFS_PAK& pak;
	const guint32 size;
};


static MemBuffer read_file_from_pak( const VFS_PAKFILE& file ){
	MemBuffer buffer;
	unzFile zipfile = unzOpen( file.pak.unzFilePath.c_str() );
	if( zipfile != nullptr ){
		*(unz_s*)zipfile = file.zipinfo;

		if ( unzOpenCurrentFile( zipfile ) == UNZ_OK ) {
			buffer = MemBuffer( file.size );

			if ( unzReadCurrentFile( zipfile, buffer.data(), file.size ) < 0 ) {
				buffer = MemBuffer();
			}
			unzCloseCurrentFile( zipfile );
		}
		unzClose( zipfile );
	}
	return buffer;
}


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
		VFS_PAK& pak = g_paks.emplace_front( filename );

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
			unzClose( uf );
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
	const CopiedString pathCleaned = g_strDirs.emplace_back( StringOutputStream( 256 )( DirectoryCleaned( path ) ) );

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
					paks.push_back( StringOutputStream( 256 )( pathCleaned, name ) );
				}
				else if ( path_extension_is( name, "pk3dir" ) ) {
					g_strDirs.emplace_back( StringOutputStream( 256 )( pathCleaned, name, '/' ) );
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


#include <set>

void vfsFindSkyFiles( const std::vector<CopiedString>& present_list ){
	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		const char *name = file.name.c_str();
		if ( path_extension_is( name, "tga" ) || path_extension_is( name, "jpg" ) || path_extension_is( name, "png" ) ) {
			auto n = StringOutputStream( 64 )( PathExtensionless( name ) );
			if( string_equal_suffix_nocase( n, "_bk" )
			 || string_equal_suffix_nocase( n, "_rt" )
			 || string_equal_suffix_nocase( n, "_ft" )
			 || string_equal_suffix_nocase( n, "_lf" )
			 || string_equal_suffix_nocase( n, "_up" )
			 || string_equal_suffix_nocase( n, "_dn" )
			 ){
				bool found = false;
				for( const auto& pre : present_list )
					if( striEqual( pre.c_str(), n ) )
						found = true;
				if( !found ){
					auto add_crc = [crcs = std::set<unsigned long>()]( const MemBuffer& boo )mutable->bool{
						return crcs.insert( crc32( 0, boo.data(), boo.size() ) ).second;
					};

					struct FTYPE
					{
						const char *ext;
						int idx;
					};
					FTYPE ftypes[3]{ { ".png", 1 }, { ".tga", 1 }, { ".jpg", 1 } };

					for( auto& ftype : ftypes )
					{
						const StringOutputStream fullnames[] = { StringOutputStream( 256 )( "c:/qsky/__/skpk/", n, ftype.ext ),
						StringOutputStream( 256 )( "c:/qsky/__/dups/", n, ftype.ext ),
						StringOutputStream( 256 )( "c:/qsky/__/dups/", n, " (2)", ftype.ext ),
						StringOutputStream( 256 )( "c:/qsky/__/dups/", n, " (3)", ftype.ext ),
						StringOutputStream( 256 )( "c:/qsky/__/dups/", n, " (4)", ftype.ext ),
						StringOutputStream( 256 )( "c:/qsky/__/dups/", n, " (5)", ftype.ext ) };
						for( const auto& fullname : fullnames )
						{
							if( FileExists( fullname ) ){
								++ftype.idx;
								add_crc( LoadFile( fullname ) );
							}
						}
					}


					if( MemBuffer buffer = read_file_from_pak( file ) ){
						if( add_crc( buffer ) ){
							Sys_Printf( "WARNING777: %s :: %s\n", name, file.pak.unzFilePath.c_str() );
							if ( !mz_zip_add_mem_to_archive_file_in_place_with_time( "c:/qsky/test_bigbox_repacked_noShader.pk3", name, buffer.data(), buffer.size(), 0, 0, 9, file.zipinfo.cur_file_info.dosDate ) ){
								Error( "Failed creating zip archive \"%s\"!\n", "c:/qsky/test_bigbox_repacked_noShader.pk3" );
							}
						}
					}
				}
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
		GDir *dir = g_dir_open( StringOutputStream( 256 )( strdir, shaderPath, "/" ), 0, NULL );

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
#if 0
	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		const char *name = file.name.c_str();
		if ( path_extension_is( name, "shader" )
		  && strniEqual( name, shaderPath, path_get_last_separator( name ) - name ) ) {
			insert( path_get_filename_start( name ) );
		}
	}
#endif
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

	auto fixed = StringOutputStream( 64 )( PathCleaned( filename ) );
	strLower( fixed.c_str() );

	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		if ( strEqual( file.name.c_str(), fixed ) ) {
			++count;
		}
	}

	for ( const auto& dir : g_strDirs )
	{
		if ( FileExists( StringOutputStream( 256 )( dir, filename ) ) ) {
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

	// filename is a full path
	if ( index == -1 ) {
		return load_full_path( filename );
	}

	for ( const auto& dir : g_strDirs )
	{
		const auto fullpath = StringOutputStream( 256 )( dir, filename );
		if ( FileExists( fullpath ) && 0 == index-- ) {
			return load_full_path( fullpath );
		}
	}

	auto fixed = StringOutputStream( 64 )( PathCleaned( filename ) );
	strLower( fixed.c_str() );

	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		if ( strEqual( file.name.c_str(), fixed ) && 0 == index-- )
		{
			snprintf( g_strLoadedFileLocation, sizeof( g_strLoadedFileLocation ), "%s :: %s", file.pak.unzFilePath.c_str(), filename );

			// unzFile zipfile = file.pak.zipfile;
			return read_file_from_pak( file );
		}
	}

	return {};
}




#include <filesystem>

bool vfsPackSkyImage( const char *filename, const char *packname, const int compLevel ){
	bool packed = false;
	bool resInMain = false;

	auto add_crc = [crcs = std::set<unsigned long>()]( const MemBuffer& boo )mutable->bool{
		return crcs.insert( crc32( 0, boo.data(), boo.size() ) ).second;
	};

	struct FTYPE
	{
		const char *ext;
		int idx;
	};
	FTYPE ftypes[3]{ { ".png", 1 }, { ".tga", 1 }, { ".jpg", 1 } };

	for( auto& ftype : ftypes )
	{
		const StringOutputStream fullnames[] = { StringOutputStream( 256 )( "c:/qsky/__/skpk/", filename, ftype.ext ),
		StringOutputStream( 256 )( "c:/qsky/__/dups/", filename, ftype.ext ),
		StringOutputStream( 256 )( "c:/qsky/__/dups/", filename, " (2)", ftype.ext ),
		StringOutputStream( 256 )( "c:/qsky/__/dups/", filename, " (3)", ftype.ext ),
		StringOutputStream( 256 )( "c:/qsky/__/dups/", filename, " (4)", ftype.ext ),
		StringOutputStream( 256 )( "c:/qsky/__/dups/", filename, " (5)", ftype.ext ) };
		for( const auto& fullname : fullnames )
		{
			if( FileExists( fullname ) ){
				resInMain = packed = true;
				++ftype.idx;
				add_crc( LoadFile( fullname ) );
			}
		}
	}

	for( auto& ftype : ftypes )
	{
		for ( const auto& dir : g_strDirs )
		{
			const auto fullname = StringOutputStream( 256 )( dir, filename, ftype.ext );
			if( FileExists( fullname ) && add_crc( LoadFile( fullname ) ) ){
				if( !packed ){
					packed = vfsPackFile_Absolute_Path( fullname, StringOutputStream( 256 )( filename, ftype.ext ), packname, compLevel );
				}
				else{
					#if 0
					auto toname = StringOutputStream( 256 )( "c:/qsky/m_dups/", filename );
					if( idx > 1 )
						toname << " (" << idx << ")";
					toname << ext;
					std::filesystem::create_directories( std::filesystem::path( toname.c_str() ).remove_filename() );
					std::filesystem::copy_file( fullname.c_str(), toname.c_str() );
					#else
					auto toname = StringOutputStream( 256 )( filename );
					if( ftype.idx > 1 ) // original name for files in alt format, (2) etc suffix for alt versions in the same format
						toname << " (" << ftype.idx << ")";
					toname << ftype.ext;
					if( resInMain )
						vfsPackFile_Absolute_Path( fullname, toname, StringOutputStream( 256 )( PathExtensionless( packname ), "_altMain.", path_get_extension( packname ) ), compLevel );
					else
						vfsPackFile_Absolute_Path( fullname, toname, StringOutputStream( 256 )( PathExtensionless( packname ), "_altNew.", path_get_extension( packname ) ), compLevel );
					#endif
				}
				++ftype.idx;
			}
		}
	}

	auto fixed = StringOutputStream( 64 )( PathCleaned( filename ) );
	strLower( fixed.c_str() );

	for( auto& ftype : ftypes )
	{
		const auto fullfixed = StringOutputStream( 256 )( fixed, ftype.ext );
		for ( const VFS_PAKFILE& file : g_pakFiles )
		{
			if ( strEqual( file.name.c_str(), fullfixed ) )
			{
				// unzFile zipfile = file.pak.zipfile;
				if ( MemBuffer buffer = read_file_from_pak( file ) ) {
					if ( add_crc( buffer ) ) {
						if( !packed ){
							packed = true;
							if ( !mz_zip_add_mem_to_archive_file_in_place_with_time( packname, StringOutputStream( 256 )( filename, ftype.ext ), buffer.data(), buffer.size(), 0, 0, compLevel, file.zipinfo.cur_file_info.dosDate ) ){
								Error( "Failed creating zip archive \"%s\"!\n", packname );
							}
						}
						else{
							auto toname = StringOutputStream( 256 )( filename );
							if( ftype.idx > 1 ) // original name for files in alt format, (2) etc suffix for alt versions in the same format
								toname << " (" << ftype.idx << ")";
							toname << ftype.ext;
							if( resInMain ){
								if ( !mz_zip_add_mem_to_archive_file_in_place_with_time( StringOutputStream( 256 )( PathExtensionless( packname ), "_altMain.", path_get_extension( packname ) ), toname, buffer.data(), buffer.size(), 0, 0, compLevel, file.zipinfo.cur_file_info.dosDate ) ){
									Error( "Failed creating zip archive \"%s\"!\n", packname );
								}
							}
							else{
								if ( !mz_zip_add_mem_to_archive_file_in_place_with_time( StringOutputStream( 256 )( PathExtensionless( packname ), "_altNew.", path_get_extension( packname ) ), toname, buffer.data(), buffer.size(), 0, 0, compLevel, file.zipinfo.cur_file_info.dosDate ) ){
									Error( "Failed creating zip archive \"%s\"!\n", packname );
								}
							}
						}
						++ftype.idx;
					}
				}
			}
		}
	}
	return packed;
}

bool vfsPackFile( const char *filename, const char *packname_in, const int compLevel ){
	static int packNum = 0;
	auto packname = StringOutputStream( 256 )( PathExtensionless( packname_in ), packNum, '.', path_get_extension( packname_in ) );
	if( std::filesystem::exists( packname.c_str() ) && std::filesystem::file_size( packname.c_str() ) > 2147483648 - 10485760 ){
		++packNum;
		packname( PathExtensionless( packname_in ), packNum, '.', path_get_extension( packname_in ) );
	}

	for ( const auto& dir : g_strDirs )
	{
		if( vfsPackFile_Absolute_Path( StringOutputStream( 256 )( dir, filename ), filename, packname, compLevel ) )
			return true;
	}

	auto fixed = StringOutputStream( 64 )( PathCleaned( filename ) );
	strLower( fixed.c_str() );

	for ( const VFS_PAKFILE& file : g_pakFiles )
	{
		if ( strEqual( file.name.c_str(), fixed ) )
		{
			// unzFile zipfile = file.pak.zipfile;
			if( MemBuffer buffer = read_file_from_pak( file ) ){
				if ( !mz_zip_add_mem_to_archive_file_in_place_with_time( packname, filename, buffer.data(), buffer.size(), 0, 0, compLevel, file.zipinfo.cur_file_info.dosDate ) ){
					Error( "Failed creating zip archive \"%s\"!\n", packname.c_str() );
				}
				return true;
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
			memset( &zip, 0, sizeof(zip) );

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
			memset( &zip, 0, sizeof(zip) );

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

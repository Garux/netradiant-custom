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
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
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

#include "vfs.h"

#include <cstdio>
#include <cstdlib>
#include <glib.h>

#include "qerplugin.h"
#include "idatastream.h"
#include "iarchive.h"
ArchiveModules& FileSystemQ3API_getArchiveModules();
#include "ifilesystem.h"

#include "generic/callback.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "os/path.h"
#include "moduleobservers.h"
#include "filematch.h"
#include <list>



// =============================================================================
// Global variables

Archive* OpenArchive( const char* name );

struct archive_entry_t
{
	CopiedString name;
	Archive* archive;
	bool is_pakfile;
};

using archives_t = std::list<archive_entry_t>;

static archives_t g_archives;
static constexpr bool g_bUsePak = true;

ModuleObservers g_observers;

using StrList = std::vector<CopiedString>;

// =============================================================================
// Static functions

const _QERArchiveTable* GetArchiveTable( ArchiveModules& archiveModules, const char* ext ){
	return archiveModules.findModule( StringStream<16>( LowerCase( ext ) ) );
}
static void InitPakFile( ArchiveModules& archiveModules, const char *filename ){
	const _QERArchiveTable* table = GetArchiveTable( archiveModules, path_get_extension( filename ) );

	if ( table != 0 ) {
		g_archives.push_back( archive_entry_t{ filename, table->m_pfnOpenArchive( filename ), true } );
		globalOutputStream() << "  pak file: " << filename << '\n';
	}
}

inline void pathlist_append_unique( StrList& pathlist, CopiedString path ){
	if( std::ranges::none_of( pathlist, [&path]( const CopiedString& str ){ return path_equal( str.c_str(), path.c_str() ); } ) )
		pathlist.emplace_back( std::move( path ) );
}

class DirectoryListVisitor : public Archive::Visitor
{
	StrList& m_matches;
	const char* m_directory;
public:
	DirectoryListVisitor( StrList& matches, const char* directory )
		: m_matches( matches ), m_directory( directory )
	{}
	void visit( const char* name ) override {
		const char* subname = path_make_relative( name, m_directory );
		if ( subname != name ) {
			if ( subname[0] == '/' ) {
				++subname;
			}
			const char* last_char = subname + strlen( subname );
			if ( last_char != subname && *( last_char - 1 ) == '/' ) {
				--last_char;
			}
			pathlist_append_unique( m_matches, StringRange( subname, last_char ) );
		}
	}
};

class FileListVisitor : public Archive::Visitor
{
	StrList& m_matches;
	const char* m_directory;
	const char* m_extension;
public:
	FileListVisitor( StrList& matches, const char* directory, const char* extension )
		: m_matches( matches ), m_directory( directory ), m_extension( extension )
	{}
	void visit( const char* name ) override {
		const char* subname = path_make_relative( name, m_directory );
		if ( subname != name ) {
			if ( subname[0] == '/' ) {
				++subname;
			}
			if ( m_extension[0] == '*' || path_extension_is( subname, m_extension ) ) {
				pathlist_append_unique( m_matches, subname );
			}
		}
	}
};

static StrList GetListInternal( const char *refdir, const char *ext, bool directories, std::size_t depth ){
	StrList files;

	ASSERT_MESSAGE( refdir[strlen( refdir ) - 1] == '/', "search path does not end in '/'" );

	if ( directories ) {
		for ( archive_entry_t& arch : g_archives )
		{
			DirectoryListVisitor visitor( files, refdir );
			arch.archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eDirectories, depth ), refdir );
		}
	}
	else
	{
		for ( archive_entry_t& arch : g_archives )
		{
			FileListVisitor visitor( files, refdir, ext );
			arch.archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eFiles, depth ), refdir );
		}
	}

	return files;
}

// Arnout: note - sort pakfiles in reverse order. This ensures that
// later pakfiles override earlier ones. This because the vfs module
// returns a filehandle to the first file it can find (while it should
// return the filehandle to the file in the most overriding pakfile, the
// last one in the list that is).

//!\todo Analyse the code in rtcw/q3 to see which order it sorts pak files.
class PakLess
{
public:
	bool operator()( const CopiedString& self, const CopiedString& other ) const {
		return string_compare_nocase_upper( self.c_str(), other.c_str() ) > 0;
	}
};

typedef std::set<CopiedString, PakLess> Archives;

// =============================================================================
// Global functions

// reads all pak files from a dir
void InitDirectory( const char* directory, ArchiveModules& archiveModules ){
	std::vector<CopiedString> strForbiddenDirs;
	StringTokeniser st( GlobalRadiant().getGameDescriptionKeyValue( "forbidden_paths" ), " " );
	for ( const char *t; !string_empty( t = st.getToken() ); )
	{
		strForbiddenDirs.push_back( t );
	}

	const auto path_is_forbidden = [&strForbiddenDirs]( const char *path ){
		return std::ranges::any_of( strForbiddenDirs, [name = path_get_filename_start( path )]( const CopiedString& forbidden ){
			return matchpattern( name, forbidden.c_str(), TRUE );
		} );
	};

	{
		StringBuffer buf( directory );
		if ( !buf.empty() && path_separator( buf.back() ) ) // del trailing slash
			buf.pop_back();
		if ( path_is_forbidden( buf.c_str() ) ){
			printf( "Directory %s matched by forbidden dirs, removed\n", directory );
			return;
		}
	}


	auto stream = StringStream( DirectoryCleaned( directory ) );
	const char *path = g_archives.emplace_back( archive_entry_t{ stream.c_str(), OpenArchive( stream ), false } ).name.c_str();

	if ( g_bUsePak ) {
		if ( GDir* dir = g_dir_open( path, 0, 0 ) ) {
			globalOutputStream() << "vfs directory: " << path << '\n';

			const char* ignore_prefix = "";
			const char* override_prefix = "";
			{
				// See if we are in "sp" or "mp" mapping mode
				const char* gamemode = GlobalRadiant().getGameMode();

				if ( string_equal( gamemode, "sp" ) ) {
					ignore_prefix = "mp_";
					override_prefix = "sp_";
				}
				else if ( string_equal( gamemode, "mp" ) ) {
					ignore_prefix = "sp_";
					override_prefix = "mp_";
				}
			}

			Archives archives;
			Archives archivesOverride;
			while ( const char* name = g_dir_read_name( dir ) )
			{
				if ( path_is_forbidden( name ) ) {
					continue;
				}

				const char *ext = path_get_extension( name );
				/* .pk3dir / .pk4dir / .dpkdir / .pakdir / .waddir */
				if ( string_equal_suffix_nocase( ext, "dir" ) && GetArchiveTable( archiveModules, ( const char[] ){ ext[0], ext[1], ext[2], '\0' } ) != nullptr ) {
					stream( path, name, '/' );
					g_archives.push_back( archive_entry_t{ stream.c_str(), OpenArchive( stream ), false } );
				}

				if ( GetArchiveTable( archiveModules, ext ) == nullptr ) {
					continue;
				}

				// using the same kludge as in engine to ensure consistency
				if ( !string_empty( ignore_prefix ) && string_equal_prefix( name, ignore_prefix ) ) {
					continue;
				}
				if ( !string_empty( override_prefix ) && string_equal_prefix( name, override_prefix ) ) {
					archivesOverride.insert( name );
					continue;
				}

				archives.insert( name );
			}

			g_dir_close( dir );

			// add the entries to the vfs
			for ( const auto& archive : archivesOverride )
			{
				InitPakFile( archiveModules, stream( path, archive.c_str() ) );
			}
			for ( const auto& archive : archives )
			{
				InitPakFile( archiveModules, stream( path, archive.c_str() ) );
			}
		}
		else
		{
			globalErrorStream() << "vfs directory not found: " << path << '\n';
		}
	}
}

// frees all memory that we allocated
// FIXME TTimo this should be improved so that we can shutdown and restart the VFS without exiting Radiant?
//   (for instance when modifying the project settings)
void Shutdown(){
	for ( archive_entry_t& arch : g_archives )
	{
		arch.archive->release();
	}
	g_archives.clear();
}

#define VFS_SEARCH_PAK 0x1
#define VFS_SEARCH_DIR 0x2

int GetFileCount( const char *filename, int flag ){
	int count = 0;
	const auto fixed = StringStream( PathCleaned( filename ) );

	if ( !flag ) {
		flag = VFS_SEARCH_PAK | VFS_SEARCH_DIR;
	}

	for ( archive_entry_t& arch : g_archives )
	{
		if ( ( arch.is_pakfile && ( flag & VFS_SEARCH_PAK ) != 0 )
		  || ( !arch.is_pakfile && ( flag & VFS_SEARCH_DIR ) != 0 ) ) {
			if ( arch.archive->containsFile( fixed ) ) {
				++count;
			}
		}
	}

	return count;
}

ArchiveFile* OpenFile( const char* filename ){
	ASSERT_MESSAGE( strchr( filename, '\\' ) == 0, "path contains invalid separator '\\': " << Quoted( filename ) );
	for ( archive_entry_t& arch : g_archives )
	{
		ArchiveFile* file = arch.archive->openFile( filename );
		if ( file != 0 ) {
			return file;
		}
	}

	return 0;
}

ArchiveTextFile* OpenTextFile( const char* filename ){
	ASSERT_MESSAGE( strchr( filename, '\\' ) == 0, "path contains invalid separator '\\': " << Quoted( filename ) );
	for ( archive_entry_t& arch : g_archives )
	{
		ArchiveTextFile* file = arch.archive->openTextFile( filename );
		if ( file != 0 ) {
			return file;
		}
	}

	return 0;
}

// NOTE: when loading a file, you have to allocate one extra byte and set it to \0
std::size_t LoadFile( const char *filename, void **bufferptr, int index ){
	const auto fixed = StringStream( PathCleaned( filename ) );

	if ( ArchiveFile* file = OpenFile( fixed ) ) {
		*bufferptr = malloc( file->size() + 1 );
		// we need to end the buffer with a 0
		( (char*) ( *bufferptr ) )[file->size()] = 0;

		std::size_t length = file->getInputStream().read( (InputStream::byte_type*)*bufferptr, file->size() );
		file->release();
		return length;
	}

	*bufferptr = 0;
	return 0;
}

void FreeFile( void *p ){
	free( p );
}

StrList GetFileList( const char *dir, const char *ext, std::size_t depth ){
	return GetListInternal( dir, ext, false, depth );
}

StrList GetDirList( const char *dir, std::size_t depth ){
	return GetListInternal( dir, 0, true, depth );
}

const char* FindFile( const char* relative ){
	for ( const archive_entry_t& arch : g_archives )
	{
		if ( arch.archive->containsFile( relative ) ) {
			return arch.name.c_str();
		}
	}

	return "";
}

const char* FindPath( const char* absolute ){
	const char *best = "";
	for ( const archive_entry_t& arch : g_archives )
	{
		if ( string_length( arch.name.c_str() ) > string_length( best ) ) {
			if ( path_equal_n( absolute, arch.name.c_str(), string_length( arch.name.c_str() ) ) ) {
				best = arch.name.c_str();
			}
		}
	}

	return best;
}


class Quake3FileSystem : public VirtualFileSystem
{
public:
	void initDirectory( const char *path ) override {
		InitDirectory( path, FileSystemQ3API_getArchiveModules() );
	}
	void initialise() override {
		globalOutputStream() << "filesystem initialised\n";
		g_observers.realise();
	}
	void shutdown() override {
		g_observers.unrealise();
		globalOutputStream() << "filesystem shutdown\n";
		Shutdown();
	}

	int getFileCount( const char *filename, int flags ){
		return GetFileCount( filename, flags );
	}
	ArchiveFile* openFile( const char* filename ) override {
		return OpenFile( filename );
	}
	ArchiveTextFile* openTextFile( const char* filename ) override {
		return OpenTextFile( filename );
	}
	std::size_t loadFile( const char *filename, void **buffer ) override {
		return LoadFile( filename, buffer, 0 );
	}
	void freeFile( void *p ) override {
		FreeFile( p );
	}

	void forEachDirectory( const char* basedir, const FileNameCallback& callback, std::size_t depth ) override {
		StrList list = GetDirList( basedir, depth );

		for ( const CopiedString& str : list )
		{
			callback( str.c_str() );
		}
	}
	void forEachFile( const char* basedir, const char* extension, const FileNameCallback& callback, std::size_t depth ) override {
		StrList list = GetFileList( basedir, extension, depth );

		for ( const CopiedString& str : list )
		{
			callback( str.c_str() );
		}
	}
	/// \brief Returns a list containing the relative names of all the directories under \p basedir.
	/// \deprecated Deprecated - use \c forEachDirectory.
	StrList getDirList( const char *basedir ){
		return GetDirList( basedir, 1 );
	}
	/// \brief Returns a list containing the relative names of the files under \p basedir ( \p extension can be "*" for all files ).
	/// \deprecated Deprecated - use \c forEachFile.
	StrList getFileList( const char *basedir, const char *extension ){
		return GetFileList( basedir, extension, 1 );
	}

	const char* findFile( const char *name ) override {
		return FindFile( name );
	}
	const char* findRoot( const char *name ) override {
		return FindPath( name );
	}

	void attach( ModuleObserver& observer ) override {
		g_observers.attach( observer );
	}
	void detach( ModuleObserver& observer ) override {
		g_observers.detach( observer );
	}

	Archive* getArchive( const char* archiveName, bool pakonly ) override {
		for ( archive_entry_t& arch : g_archives )
		{
			if ( pakonly && !arch.is_pakfile ) {
				continue;
			}

			if ( path_equal( arch.name.c_str(), archiveName ) ) {
				return arch.archive;
			}
		}
		return 0;
	}
	void forEachArchive( const ArchiveNameCallback& callback, bool pakonly, bool reverse ) override {
		if ( reverse ) {
			g_archives.reverse();
		}

		for ( const archive_entry_t& arch : g_archives )
		{
			if ( pakonly && !arch.is_pakfile ) {
				continue;
			}

			callback( arch.name.c_str() );
		}

		if ( reverse ) {
			g_archives.reverse();
		}
	}
};

Quake3FileSystem g_Quake3FileSystem;

void FileSystem_Init(){
}

void FileSystem_Shutdown(){
}

VirtualFileSystem& GetFileSystem(){
	return g_Quake3FileSystem;
}

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


#define VFS_MAXDIRS 64

#if defined( WIN32 )
#define PATH_MAX 260
#endif

#define gamemode_get GlobalRadiant().getGameMode



// =============================================================================
// Global variables

Archive* OpenArchive( const char* name );

struct archive_entry_t
{
	CopiedString name;
	Archive* archive;
	bool is_pakfile;
};

#include <list>

using archives_t = std::list<archive_entry_t>;

static archives_t g_archives;
static char g_strDirs[VFS_MAXDIRS][PATH_MAX + 1];
static int g_numDirs;
static char g_strForbiddenDirs[VFS_MAXDIRS][PATH_MAX + 1];
static int g_numForbiddenDirs = 0;
static constexpr bool g_bUsePak = true;

ModuleObservers g_observers;

using StrList = std::vector<CopiedString>;

// =============================================================================
// Static functions

static void AddSlash( char *str ){
	std::size_t n = strlen( str );
	if ( n > 0 ) {
		if ( str[n - 1] != '\\' && str[n - 1] != '/' ) {
			globalWarningStream() << "WARNING: directory path does not end with separator: " << str << '\n';
			strcat( str, "/" );
		}
	}
}

static void FixDOSName( char *src ){
	if ( src == 0 || strchr( src, '\\' ) == 0 ) {
		return;
	}

	globalWarningStream() << "WARNING: invalid path separator '\\': " << src << '\n';

	while ( *src )
	{
		if ( *src == '\\' ) {
			*src = '/';
		}
		src++;
	}
}



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
	if( std::none_of( pathlist.cbegin(), pathlist.cend(),
	[&path]( const CopiedString& str ){ return path_compare( str.c_str(), path.c_str() ) == 0; } ) )
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
	void visit( const char* name ){
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
	void visit( const char* name ){
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
	int j;

	g_numForbiddenDirs = 0;
	StringTokeniser st( GlobalRadiant().getGameDescriptionKeyValue( "forbidden_paths" ), " " );
	for ( j = 0; j < VFS_MAXDIRS; ++j )
	{
		const char *t = st.getToken();
		if ( string_empty( t ) ) {
			break;
		}
		strncpy( g_strForbiddenDirs[g_numForbiddenDirs], t, PATH_MAX );
		g_strForbiddenDirs[g_numForbiddenDirs][PATH_MAX] = '\0';
		++g_numForbiddenDirs;
	}

	for ( j = 0; j < g_numForbiddenDirs; ++j )
	{
		char* dbuf = g_strdup( directory );
		if ( *dbuf && dbuf[strlen( dbuf ) - 1] == '/' ) {
			dbuf[strlen( dbuf ) - 1] = 0;
		}
		const char *p = strrchr( dbuf, '/' );
		p = ( p ? ( p + 1 ) : dbuf );
		if ( matchpattern( p, g_strForbiddenDirs[j], TRUE ) ) {
			g_free( dbuf );
			break;
		}
		g_free( dbuf );
	}
	if ( j < g_numForbiddenDirs ) {
		printf( "Directory %s matched by forbidden dirs, removed\n", directory );
		return;
	}

	if ( g_numDirs == VFS_MAXDIRS ) {
		return;
	}

	strncpy( g_strDirs[g_numDirs], directory, PATH_MAX );
	g_strDirs[g_numDirs][PATH_MAX] = '\0';
	FixDOSName( g_strDirs[g_numDirs] );
	AddSlash( g_strDirs[g_numDirs] );

	const char* path = g_strDirs[g_numDirs];

	g_numDirs++;

	g_archives.push_back( archive_entry_t{ path, OpenArchive( path ), false } );

	if ( g_bUsePak ) {
		GDir* dir = g_dir_open( path, 0, 0 );

		if ( dir != 0 ) {
			globalOutputStream() << "vfs directory: " << path << '\n';

			const char* ignore_prefix = "";
			const char* override_prefix = "";

			{
				// See if we are in "sp" or "mp" mapping mode
				const char* gamemode = gamemode_get();

				if ( strcmp( gamemode, "sp" ) == 0 ) {
					ignore_prefix = "mp_";
					override_prefix = "sp_";
				}
				else if ( strcmp( gamemode, "mp" ) == 0 ) {
					ignore_prefix = "sp_";
					override_prefix = "mp_";
				}
			}

			Archives archives;
			Archives archivesOverride;
			for (;; )
			{
				const char* name = g_dir_read_name( dir );
				if ( name == 0 ) {
					break;
				}

				for ( j = 0; j < g_numForbiddenDirs; ++j )
				{
					const char *p = strrchr( name, '/' );
					p = ( p ? ( p + 1 ) : name );
					if ( matchpattern( p, g_strForbiddenDirs[j], TRUE ) ) {
						break;
					}
				}
				if ( j < g_numForbiddenDirs ) {
					continue;
				}

				const char *ext = strrchr( name, '.' );

				if ( ext && !string_compare_nocase_upper( ext, ".pk3dir" ) ) {
					if ( g_numDirs == VFS_MAXDIRS ) {
						continue;
					}
					std::snprintf( g_strDirs[g_numDirs], PATH_MAX, "%s%s/", path, name );
					FixDOSName( g_strDirs[g_numDirs] );
					AddSlash( g_strDirs[g_numDirs] );
					g_numDirs++;

					g_archives.push_back( archive_entry_t{ g_strDirs[g_numDirs - 1], OpenArchive( g_strDirs[g_numDirs - 1] ), false } );
				}

				if ( ( ext == 0 ) || *( ++ext ) == '\0' || GetArchiveTable( archiveModules, ext ) == 0 ) {
					continue;
				}

				// using the same kludge as in engine to ensure consistency
				if ( !string_empty( ignore_prefix ) && strncmp( name, ignore_prefix, strlen( ignore_prefix ) ) == 0 ) {
					continue;
				}
				if ( !string_empty( override_prefix ) && strncmp( name, override_prefix, strlen( override_prefix ) ) == 0 ) {
					archivesOverride.insert( name );
					continue;
				}

				archives.insert( name );
			}

			g_dir_close( dir );

			// add the entries to the vfs
			for ( Archives::iterator i = archivesOverride.begin(); i != archivesOverride.end(); ++i )
			{
				char filename[PATH_MAX];
				strcpy( filename, path );
				strcat( filename, ( *i ).c_str() );
				InitPakFile( archiveModules, filename );
			}
			for ( Archives::iterator i = archives.begin(); i != archives.end(); ++i )
			{
				char filename[PATH_MAX];
				strcpy( filename, path );
				strcat( filename, ( *i ).c_str() );
				InitPakFile( archiveModules, filename );
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

	g_numDirs = 0;
	g_numForbiddenDirs = 0;
}

#define VFS_SEARCH_PAK 0x1
#define VFS_SEARCH_DIR 0x2

int GetFileCount( const char *filename, int flag ){
	int count = 0;
	char fixed[PATH_MAX + 1];

	strncpy( fixed, filename, PATH_MAX );
	fixed[PATH_MAX] = '\0';
	FixDOSName( fixed );

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
	ASSERT_MESSAGE( strchr( filename, '\\' ) == 0, "path contains invalid separator '\\': " << makeQuoted( filename ) );
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
	ASSERT_MESSAGE( strchr( filename, '\\' ) == 0, "path contains invalid separator '\\': " << makeQuoted( filename ) );
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
	char fixed[PATH_MAX + 1];

	strncpy( fixed, filename, PATH_MAX );
	fixed[PATH_MAX] = '\0';
	FixDOSName( fixed );

	ArchiveFile* file = OpenFile( fixed );

	if ( file != 0 ) {
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
	void initDirectory( const char *path ){
		InitDirectory( path, FileSystemQ3API_getArchiveModules() );
	}
	void initialise(){
		globalOutputStream() << "filesystem initialised\n";
		g_observers.realise();
	}
	void shutdown(){
		g_observers.unrealise();
		globalOutputStream() << "filesystem shutdown\n";
		Shutdown();
	}

	int getFileCount( const char *filename, int flags ){
		return GetFileCount( filename, flags );
	}
	ArchiveFile* openFile( const char* filename ){
		return OpenFile( filename );
	}
	ArchiveTextFile* openTextFile( const char* filename ){
		return OpenTextFile( filename );
	}
	std::size_t loadFile( const char *filename, void **buffer ){
		return LoadFile( filename, buffer, 0 );
	}
	void freeFile( void *p ){
		FreeFile( p );
	}

	void forEachDirectory( const char* basedir, const FileNameCallback& callback, std::size_t depth ){
		StrList list = GetDirList( basedir, depth );

		for ( const CopiedString& str : list )
		{
			callback( str.c_str() );
		}
	}
	void forEachFile( const char* basedir, const char* extension, const FileNameCallback& callback, std::size_t depth ){
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

	const char* findFile( const char *name ){
		return FindFile( name );
	}
	const char* findRoot( const char *name ){
		return FindPath( name );
	}

	void attach( ModuleObserver& observer ){
		g_observers.attach( observer );
	}
	void detach( ModuleObserver& observer ){
		g_observers.detach( observer );
	}

	Archive* getArchive( const char* archiveName, bool pakonly ){
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
	void forEachArchive( const ArchiveNameCallback& callback, bool pakonly, bool reverse ){
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

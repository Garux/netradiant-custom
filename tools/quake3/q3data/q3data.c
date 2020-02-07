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

#include "q3data.h"
#include "md3lib.h"

#include "vfs.h"

bool g_verbose;
bool g_stripify = true;
bool g_release;             // don't grab, copy output data to new tree
char g_releasedir[1024];        // c:\quake2\baseq2, etc
bool g_archive;             // don't grab, copy source data to new tree
char g_only[256];               // if set, only grab this cd
bool g_skipmodel;           // set true when a cd is not g_only

// bogus externs for some TA hacks (common/ using them against q3map)
char *moddir = NULL;

/*
   =======================================================

   PAK FILES

   =======================================================
 */

unsigned Com_BlockChecksum( void *buffer, int length );

typedef struct
{
	char name[56];
	int filepos, filelen;
} packfile_t;

typedef struct
{
	char id[4];
	int dirofs;
	int dirlen;
} packheader_t;

packfile_t pfiles[16384];
FILE            *pakfile;
packfile_t      *pf;
packheader_t pakheader;

/*
   ==============
   ReleaseFile

   Filename should be gamedir relative.
   Either copies the file to the release dir, or adds it to
   the pak file.
   ==============
 */
void ReleaseFile( char *filename ){
	char source[1024];
	char dest[1024];

	if ( !g_release ) {
		return;
	}

	sprintf( source, "%s%s", gamedir, filename );
	sprintf( dest, "%s/%s", g_releasedir, filename );
	printf( "copying to %s\n", dest );
	QCopyFile( source, dest );
	return;
}

typedef struct
{
	// shader
	// opaque
	// opaque 2
	// blend
	// blend 2
	char names[5][1024];
	int num;
} ShaderFiles_t;

ShaderFiles_t s_shaderFiles;

void FindShaderFiles( char *filename ){
	char buffer[1024];
	char stripped[1024];
	char linebuffer[1024];
	int len, i;
	char *buf;
	char *diffuseExtensions[] =
	{
		".TGA",
		".WAL",
		".PCX",
		0
	};
	char *otherExtensions[] =
	{
		".specular.TGA",
		".blend.TGA",
		".alpha.TGA",
		0
	};

	s_shaderFiles.num = 0;

	strcpy( stripped, filename );
	path_set_extension( stripped, ".shader" );

	if ( FileExists( stripped ) ) {
		char *p;
		char mapa[512], mapb[512];

		strcpy( s_shaderFiles.names[s_shaderFiles.num], stripped );
		s_shaderFiles.num++;

		// load and parse
		len = LoadFile( stripped, (void **)&buf );

		p = buf;

		while ( p - buf < len )
		{
			i = 0;

			// skip spaces
			while ( *p == ' ' || *p == '\n' || *p == '\t' )
				p++;

			// grab rest of the line
			while ( *p != 0 && *p != '\n' )
			{
				linebuffer[i] = *p;
				i++;
				p++;
			}
			if ( *p == '\n' ) {
				p++;
			}
			linebuffer[i] = 0;

			strLower( linebuffer );

			// see if the line specifies an opaque map or blendmap
			if ( strstr( linebuffer, "opaquemap" ) == linebuffer ||
				 strstr( linebuffer, "blendmap" ) == linebuffer ) {
				int j;

				i = 0;

				mapa[0] = mapb[0] = 0;

				// skip past the keyword
				while ( linebuffer[i] != ' ' && linebuffer[i] != '\t' && linebuffer[i] )
					i++;
				// skip past spaces
				while ( ( linebuffer[i] == ' ' || linebuffer[i] == '\t' ) && linebuffer[i] )
					i++;

				// grab first map name
				j = 0;
				while ( linebuffer[i] != ' ' && linebuffer[i] != '\t' && linebuffer[i] )
				{
					mapa[j] = linebuffer[i];
					j++;
					i++;
				}
				mapa[j] = 0;

				// skip past spaces
				while ( ( linebuffer[i] == ' ' || linebuffer[i] == '\t' ) && linebuffer[i] )
					i++;

				// grab second map name
				j = 0;
				while ( linebuffer[i] != ' ' && linebuffer[i] != '\t' && linebuffer[i] )
				{
					mapb[j] = linebuffer[i];
					j++;
					i++;
				}
				mapb[j] = 0;

				// store map names
				if ( mapa[0] != 0 && mapa[0] != '-' ) {
					sprintf( s_shaderFiles.names[s_shaderFiles.num], "%s%s", gamedir, mapa );
					s_shaderFiles.num++;
				}
				if ( mapb[0] != 0 && mapb[0] != '-' && mapb[0] != '^' && mapb[0] != '*' ) {
					sprintf( s_shaderFiles.names[s_shaderFiles.num], "%s%s", gamedir, mapb );
					s_shaderFiles.num++;
				}
			}
		}
	}
	else
	{
		StripExtension( stripped );

		// look for diffuse maps
		for ( i = 0; i < 3; i++ )
		{
			strcpy( buffer, stripped );
			strcat( buffer, diffuseExtensions[i] );
			if ( FileExists( buffer ) ) {
				strcpy( s_shaderFiles.names[s_shaderFiles.num], buffer );
				s_shaderFiles.num++;
				break;
			}
		}
		for ( i = 0; i < 3; i++ )
		{
			strcpy( buffer, stripped );
			strcat( buffer, otherExtensions[i] );
			if ( FileExists( buffer ) ) {
				strcpy( s_shaderFiles.names[s_shaderFiles.num], buffer );
				s_shaderFiles.num++;
			}
		}
	}
}

/*
   ==============
   ReleaseShader

   Copies all needed files for a shader to the release directory
   ==============
 */
void ReleaseShader( char *filename ){
	char fullpath[1024];
	char dest[1024];
	char stripped[1024];
	int i;

	sprintf( fullpath, "%s%s", gamedir, filename );

	FindShaderFiles( fullpath );

	for ( i = 0; i < s_shaderFiles.num; i++ )
	{
		strcpy( stripped, s_shaderFiles.names[i] );
		if ( strstr( stripped, gamedir ) ) {
			memmove( stripped, stripped + strlen( gamedir ), strlen( stripped ) );
		}
		sprintf( dest, "%s/%s", g_releasedir, stripped );
		printf( "copying to %s\n", dest );
		QCopyFile( s_shaderFiles.names[i], dest );
	}
}

/*
   ===============
   Cmd_File

   This is only used to cause a file to be copied during a release
   build (default.cfg, maps, etc)
   ===============
 */
void Cmd_File( void ){
	GetToken( false );
	ReleaseFile( token );
}

/*
   ===============
   PackDirectory_r

   ===============
 */
#ifdef WIN32
#include "io.h"
void PackDirectory_r( char *dir ){
	struct _finddata_t fileinfo;
	int handle;
	char dirstring[1024];
	char filename[1024];

	sprintf( dirstring, "%s%s/*.*", gamedir, dir );

	handle = _findfirst( dirstring, &fileinfo );
	if ( handle == -1 ) {
		return;
	}

	do
	{
		sprintf( filename, "%s/%s", dir, fileinfo.name );
		if ( fileinfo.attrib & _A_SUBDIR ) { // directory
			if ( fileinfo.name[0] != '.' ) {  // don't pak . and ..
				PackDirectory_r( filename );
			}
			continue;
		}
		// copy or pack the file
		ReleaseFile( filename );
	} while ( _findnext( handle, &fileinfo ) != -1 );

	_findclose( handle );
}
#else

#include <sys/types.h>
#include <sys/dir.h>

void PackDirectory_r( char *dir ){
	struct dirent **namelist, *ent;
	int count;
	struct stat st;
	int i;
	char fullname[1024];
	char dirstring[1024];
	char        *name;

	sprintf( dirstring, "%s%s", gamedir, dir );
	count = scandir( dirstring, &namelist, NULL, NULL );

	for ( i = 0 ; i < count ; i++ )
	{
		ent = namelist[i];
		name = ent->d_name;

		if ( name[0] == '.' ) {
			continue;
		}

		sprintf( fullname, "%s/%s", dir, name );
		sprintf( dirstring, "%s%s/%s", gamedir, dir, name );

		if ( stat( dirstring, &st ) == -1 ) {
			Error( "fstating %s", pf->name );
		}
		if ( S_ISDIR( st.st_mode ) != 0 ) { // directory
			PackDirectory_r( fullname );
			continue;
		}

		// copy or pack the file
		ReleaseFile( fullname );
	}
}
#endif


/*
   ===============
   Cmd_Dir

   This is only used to cause a directory to be copied during a
   release build (sounds, etc)
   ===============
 */
void Cmd_Dir( void ){
	GetToken( false );
	PackDirectory_r( token );
}

//========================================================================

#define MAX_RTEX    16384
int numrtex;
char rtex[MAX_RTEX][64];

void ReleaseTexture( char *name ){
	int i;
	char path[1024];

	for ( i = 0 ; i < numrtex ; i++ )
		if ( striEqual( name, rtex[i] ) ) {
			return;
		}

	if ( numrtex == MAX_RTEX ) {
		Error( "numrtex == MAX_RTEX" );
	}

	strcpy( rtex[i], name );
	numrtex++;

	sprintf( path, "textures/%s.wal", name );
	ReleaseFile( path );
}

/*
   ===============
   Cmd_Maps

   Only relevent for release and pak files.
   Releases the .bsp files for the maps, and scans all of the files to
   build a list of all textures used, which are then released.
   ===============
 */
void Cmd_Maps( void ){
	char map[1024];

	while ( TokenAvailable() )
	{
		GetToken( false );
		sprintf( map, "maps/%s.bsp", token );
		ReleaseFile( map );

		if ( !g_release ) {
			continue;
		}

		// get all the texture references
		sprintf( map, "%smaps/%s.bsp", gamedir, token );
		LoadBSPFile( map );
	}
}


//==============================================================

/*
   ===============
   ParseScript
   ===============
 */
void ParseScript( void ){
	while ( 1 )
	{
		do
		{   // look for a line starting with a $ command
			GetToken( true );
			if ( endofscript ) {
				return;
			}
			if ( token[0] == '$' ) {
				break;
			}
			while ( TokenAvailable() )
				GetToken( false );
		} while ( 1 );

		//
		// model commands
		//
		if ( !strcmp( token, "$modelname" ) ) {
			Cmd_Modelname();
		}
		else if ( !strcmp( token, "$base" ) ) {
			Cmd_Base();
		}
		else if ( !strcmp( token, "$exit" ) ) {
			break;
		}
		else if ( !strcmp( token, "$3dsconvert" ) ) {
			Cmd_3DSConvert();
		}
		else if ( !strcmp( token, "$spritebase" ) ) {
			Cmd_SpriteBase();
		}
		else if ( !strcmp( token, "$cd" ) ) {
			Cmd_Cd();
		}
		else if ( !strcmp( token, "$origin" ) ) {
			Cmd_Origin();
		}
		else if ( !strcmp( token, "$scale" ) ) {
			Cmd_ScaleUp();
		}
		else if ( !strcmp( token, "$frame" ) ) {
			Cmd_Frame();
		}
		else if ( !strcmp( token, "$skin" ) ) {
			Cmd_Skin();
		}
		else if ( !strcmp( token, "$spriteshader" ) ) {
			Cmd_SpriteShader();
		}
		else if ( !strcmp( token, "$aseconvert" ) ) {
			Cmd_ASEConvert( false );
		}
		else if ( !strcmp( token, "$aseanimconvert" ) ) {
			Cmd_ASEConvert( true );
		}

		//
		// image commands
		//
		else if ( !strcmp( token, "$grab" ) ) {
			Cmd_Grab();
		}
		else if ( !strcmp( token, "$raw" ) ) {
			Cmd_Raw();
		}
		else if ( !strcmp( token, "$colormap" ) ) {
			Cmd_Colormap();
		}
		else if ( !strcmp( token, "$environment" ) ) {
			Cmd_Environment();
		}

		//
		// video
		//
		else if ( !strcmp( token, "$video" ) ) {
			Cmd_Video();
		}
		//
		// misc
		//
		else if ( !strcmp( token, "$file" ) ) {
			Cmd_File();
		}
		else if ( !strcmp( token, "$dir" ) ) {
			Cmd_Dir();
		}
		else if ( !strcmp( token, "$maps" ) ) {
			Cmd_Maps();
		}
		else{
			Error( "bad command %s\n", token );
		}
	}
}

//=======================================================

#include "version.h"

/*
   ==============
   main
   ==============
 */
int main( int argc, char **argv ){
	int i;           // VC4.2 compiler bug if auto...
	char path[1024];

	// using GtkRadiant's versioning next to Id's versioning
	printf( "Q3Data      - (c) 1999 Id Software Inc.\n" );
	printf( "NetRadiant  - v" RADIANT_VERSION " " __DATE__ "\n" );

	ExpandWildcards( &argc, &argv );

	for ( i = 1 ; i < argc ; i++ )
	{
		if ( !strcmp( argv[i], "-release" ) ) {
			g_release = true;
			strcpy( g_releasedir, argv[i + 1] );
			printf( "Copy output to: %s\n", g_releasedir );
			i++;
		}
		else if ( !strcmp( argv[i], "-nostrips" ) ) {
			g_stripify = false;
			printf( "Not optimizing for strips\n" );
		}
		else if ( !strcmp( argv[i], "-writedir" ) ) {
			strcpy( writedir, argv[i + 1] );
			printf( "Write output to: %s\n", writedir );
			i++;
		}
		else if ( !strcmp( argv[i], "-verbose" ) ) {
			g_verbose = true;
		}
		else if ( !strcmp( argv[i], "-dump" ) ) {
			printf( "Dumping contents of: '%s'\n", argv[i + 1] );
			if ( striEqual( path_get_filename_base_end( argv[i + 1] ), ".md3" ) ) {
				MD3_Dump( argv[i + 1] );
			}
			else
			{
				Error( "Do not know how to dump the contents of '%s'\n", argv[i + 1] );
			}
			i++;
		}
		else if ( !strcmp( argv[i], "-3dsconvert" ) ) {
			// NOTE TTimo this is broken, tried on a sample .3ds
			// what happens .. it calls the Convert3DStoMD3,
			// which calls the scriptlib function in non initialized state .. and crashes
			printf( "Converting %s.3DS to %s.MD3\n", argv[i + 1], argv[i + 1] );
			SetQdirFromPath( argv[i + 1] );
			vfsInitDirectory( gamedir );
			Convert3DStoMD3( argv[i + 1] );
			i++;
		}
		else if ( !strcmp( argv[i], "-only" ) ) {
			strcpy( g_only, argv[i + 1] );
			printf( "Only grabbing %s\n", g_only );
			i++;
		}
		else if ( !strcmp( argv[i], "-gamedir" ) ) {
			strcpy( gamedir, argv[i + 1] );
			i++;
		}
		else if ( argv[i][0] == '-' ) {
			Error( "Unknown option \"%s\"", argv[i] );
		}
		else{
			break;
		}
	}

	if ( i == argc ) {
		Error( "usage: q3data [-dump <file.md3>] [-release <directory>] [-only <model>] [-3dsconvert <file.3ds>] [-verbose] [file.qdt]" );
	}

	for ( ; i < argc ; i++ )
	{
		printf( "--------------- %s ---------------\n", argv[i] );
		// load the script
		strcpy( path, argv[i] );
		DefaultExtension( path, ".qdt" );
		if ( !gamedir[0] ) {
			SetQdirFromPath( path );
		}
		// NOTE TTimo
		// q3data went through a partial conversion to use the vfs
		// it was never actually tested before 1.1.1
		// the code is still mostly using direct file access calls
		vfsInitDirectory( gamedir );
		LoadScriptFile( ExpandArg( path ), -1 );

		//
		// parse it
		//
		ParseScript();

		// write out the last model
		FinishModel( TYPE_UNKNOWN );
	}

	return 0;
}

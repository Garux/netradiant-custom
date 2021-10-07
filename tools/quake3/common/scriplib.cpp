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

// scriplib.c

#include "cmdlib.h"
#include "inout.h"
#include "qstringops.h"
#include "qpathops.h"
#include "scriplib.h"
#include "vfs.h"
#include <list>

/*
   =============================================================================

                        PARSING STUFF

   =============================================================================
 */

struct script_t
{
	CopiedString filename;
	char    *buffer;
	const char *it, *end;
	int line;
	/* buffer is optional: != nullptr == own it  */
	script_t( const char *filename, char *buffer, char *start, int size ) :
		filename( filename ),
		buffer( buffer ),
		it( start ),
		end( start + size ),
		line( 1 )
	{}
	script_t() = delete;
	script_t( const script_t& ) = delete;
	script_t( script_t&& ) noexcept = delete;
	script_t& operator=( const script_t& ) = delete;
	script_t& operator=( script_t&& ) noexcept = delete;
	~script_t(){
		free( buffer );
	}
};

std::list<script_t> scriptstack;

int scriptline;
char token[MAXTOKEN];
bool tokenready;                     // only true if UnGetToken was just called

/*
   ==============
   AddScriptToStack
   ==============
 */
static void AddScriptToStack( const char *filename, int index, bool verbose ){
	void* buffer;
	const int size = vfsLoadFile( filename, &buffer, index );

	if ( size == -1 ) {
		Sys_FPrintf( SYS_WRN, "Script file %s was not found\n", filename );
	}
	else
	{
		if( verbose ){
			if ( index > 0 )
				Sys_Printf( "entering %s (%d)\n", filename, index + 1 );
			else
				Sys_Printf( "entering %s\n", filename );
		}

		scriptstack.emplace_back( filename, void_ptr( buffer ), void_ptr( buffer ), size );
	}
}


/*
   ==============
   LoadScriptFile
   ==============
 */
void LoadScriptFile( const char *filename, int index, bool verbose /* = true */ ){
	scriptstack.clear();
	AddScriptToStack( filename, index, verbose );
	tokenready = false;
}

/*
   ==============
   ParseFromMemory
   ==============
 */
void ParseFromMemory( char *buffer, int size ){
	scriptstack.clear();
	scriptstack.emplace_back( "memory buffer", nullptr, buffer, size );
	tokenready = false;
}


/*
   ==============
   UnGetToken

   Signals that the current token was not used, and should be reported
   for the next GetToken.  Note that

   GetToken (true);
   UnGetToken ();
   GetToken (false);

   could cross a line boundary.
   ==============
 */
void UnGetToken( void ){
	ENSURE( !tokenready && "Can't UnGetToken() twice in a row!" );
	tokenready = true;
}


static bool EndOfScript( bool crossline ){
	if ( !crossline ) {
		Error( "Line %i is incomplete\nFile location be: %s\n", scriptline, g_strLoadedFileLocation );
	}

	scriptstack.pop_back();

	if ( scriptstack.empty() ) {
		return false;
	}
	else{
		scriptline = scriptstack.back().line;
		Sys_Printf( "returning to %s\n", scriptstack.back().filename.c_str() );
		return GetToken( crossline );
	}
}

/*
   ==============
   GetToken
   ==============
 */
bool GetToken( bool crossline ){
	/* ydnar: dummy testing */
	if ( scriptstack.empty() ) {
		return false;
	}

	if ( tokenready ) {                       // is a token already waiting?
		tokenready = false;
		return true;
	}

	script_t& script = scriptstack.back();

	if ( script.it >= script.end ) {
		return EndOfScript( crossline );
	}

//
// skip space
//
skipspace:
	while ( script.it < script.end && *script.it <= 32 )
	{
		if ( *script.it++ == '\n' ) {
			if ( !crossline ) {
				Error( "Line %i is incomplete\nFile location be: %s\n", scriptline, g_strLoadedFileLocation );
			}
			script.line++;
			scriptline = script.line;
		}
	}

	if ( script.it >= script.end ) {
		return EndOfScript( crossline );
	}

	// ; # // comments
	if ( *script.it == ';' || *script.it == '#'
	     || ( script.it[0] == '/' && script.it[1] == '/' ) ) {
		if ( !crossline ) {
			Error( "Line %i is incomplete\nFile location be: %s\n", scriptline, g_strLoadedFileLocation );
		}
		while ( *script.it++ != '\n' )
			if ( script.it >= script.end ) {
				return EndOfScript( crossline );
			}
		script.line++;
		scriptline = script.line;
		goto skipspace;
	}

	// /* */ comments
	if ( script.it[0] == '/' && script.it[1] == '*' ) {
		script.it += 2;
		while ( script.it[0] != '*' || script.it[1] != '/' )
		{
			if ( *script.it == '\n' ) {
				if ( !crossline ) {
					Error( "Line %i is incomplete\nFile location be: %s\n", scriptline, g_strLoadedFileLocation );
				}
				script.line++;
				scriptline = script.line;
			}
			script.it++;
			if ( script.it >= script.end ) {
				return EndOfScript( crossline );
			}
		}
		script.it += 2;
		goto skipspace;
	}

//
// copy token
//
	char *token_p = token;

	if ( *script.it == '"' ) {
		// quoted token
		script.it++;
		while ( *script.it != '"' )
		{
			*token_p++ = *script.it++;
			if ( script.it == script.end ) {
				break;
			}
			if ( token_p == token + MAXTOKEN ) {
				Error( "Token too large on line %i\nFile location be: %s\n", scriptline, g_strLoadedFileLocation );
			}
		}
		script.it++;
	}
	else{   // regular token
		while ( *script.it > 32 && *script.it != ';' )
		{
			*token_p++ = *script.it++;
			if ( script.it == script.end ) {
				break;
			}
			if ( token_p == token + MAXTOKEN ) {
				Error( "Token too large on line %i\nFile location be: %s\n", scriptline, g_strLoadedFileLocation );
			}
		}
	}

	*token_p = 0;

	if ( strEqual( token, "$include" ) ) {
		GetToken( false );
		AddScriptToStack( token, 0, true );
		return GetToken( crossline );
	}

	return true;
}


/*
   ==============
   TokenAvailable

   Returns true if there is another token on the line
   ==============
 */
bool TokenAvailable( void ) {
	/* save */
	const int oldLine = scriptline;

	/* test */
	if ( !GetToken( true ) ) {
		return false;
	}
	UnGetToken();
	if ( oldLine == scriptline ) {
		return true;
	}

	/* restore */
	//%	scriptline = oldLine;
	//%	script->line = oldScriptLine;

	return false;
}


//=====================================================================


void MatchToken( const char *match ) {
	GetToken( true );

	if ( !strEqual( token, match ) ) {
		Error( "MatchToken( \"%s\" ) failed at line %i in file %s", match, scriptline, g_strLoadedFileLocation );
	}
}


template<typename T>
void Parse1DMatrix( int x, T *m ) {
	int i;

	MatchToken( "(" );

	for ( i = 0 ; i < x ; i++ ) {
		GetToken( false );
		m[i] = atof( token );
	}

	MatchToken( ")" );
}
template void Parse1DMatrix<float>( int x, float *m );
template void Parse1DMatrix<double>( int x, double *m );

void Parse2DMatrix( int y, int x, float *m ) {
	int i;

	MatchToken( "(" );

	for ( i = 0 ; i < y ; i++ ) {
		Parse1DMatrix( x, m + i * x );
	}

	MatchToken( ")" );
}

void Parse3DMatrix( int z, int y, int x, float *m ) {
	int i;

	MatchToken( "(" );

	for ( i = 0 ; i < z ; i++ ) {
		Parse2DMatrix( y, x, m + i * x * y );
	}

	MatchToken( ")" );
}


void Write1DMatrix( FILE *f, int x, float *m ) {
	int i;

	fprintf( f, "( " );
	for ( i = 0 ; i < x ; i++ ) {
		if ( m[i] == (int)m[i] ) {
			fprintf( f, "%i ", (int)m[i] );
		}
		else {
			fprintf( f, "%f ", m[i] );
		}
	}
	fprintf( f, ")" );
}

void Write2DMatrix( FILE *f, int y, int x, float *m ) {
	int i;

	fprintf( f, "( " );
	for ( i = 0 ; i < y ; i++ ) {
		Write1DMatrix( f, x, m + i * x );
		fprintf( f, " " );
	}
	fprintf( f, ")\n" );
}


void Write3DMatrix( FILE *f, int z, int y, int x, float *m ) {
	int i;

	fprintf( f, "(\n" );
	for ( i = 0 ; i < z ; i++ ) {
		Write2DMatrix( f, y, x, m + i * ( x * y ) );
	}
	fprintf( f, ")\n" );
}

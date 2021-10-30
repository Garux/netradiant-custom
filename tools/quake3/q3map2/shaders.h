/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
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


#pragma once


class ShaderTextCollector
{
	int oldScriptLine = -1;
	int tabDepth = 0;

	void indent() {
		for ( int i = 0; i < tabDepth; ++i )
			text << '\t';
	}
public:
	StringOutputStream text;
	void tokenAppend() {
		/* pre-tabstops */
		if ( strEqual( token, "}" ) ) {
			tabDepth--;
			if( tabDepth < 0 )
				Sys_Warning( "tabDepth < 0 in shader\n%s", text.c_str() );
		}

		/* new line or append? */
		if( strEqual( token, "}" ) || strEqual( token, "{" ) ){ // special case: always on separate line
			text << '\n';
			indent();
			oldScriptLine = -1; // trigger new line for the next token
		}
		else if ( oldScriptLine != scriptline ) {
			oldScriptLine = scriptline;
			text << '\n';
			indent();
		}
		else{
			text << ' ';
		}
		text << token;

		/* post-tabstops */
		if ( strEqual( token, "{" ) ) {
			tabDepth++;
			if( tabDepth > 2 )
				Sys_Warning( "tabDepth > 2 in shader\n%s", text.c_str() );
		}
	}
	bool GetToken( bool crossline ){
		const bool r = ::GetToken( crossline );
		if ( r && !strEmpty( token ) ) {
			tokenAppend();
		}
		return r;
	}
	void clear() {
		text.clear();
		oldScriptLine = -1; // 'scriptline' is not very reliable, thus some deduction; 1st line will be new line consistently
		tabDepth = 0;
	}
};

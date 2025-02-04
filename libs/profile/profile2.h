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

#pragma once

#include "stream/textfilestream.h"
#include "stream/textstream.h"
#include "stream/stringstream.h"
#include <optional>
#include <map>

class IniFile
{
	//       section                key           value
	std::map<CopiedString, std::map<CopiedString, CopiedString>> m_sections;
public:
	bool read( const char *filename ){
		TextFileInputStream inputFile( filename );
		if( inputFile.failed() ){
			return false;
		}
		SingleCharacterInputStream<TextFileInputStream> bufferedInput( inputFile );
		StringBuffer line, section, key;

		const auto isSpace = []( char c ) -> bool { return c <= 32; };
		const auto trimTrailing = [isSpace]( StringBuffer& buf ){ while( !buf.empty() && isSpace( buf.back() ) ) buf.pop_back(); };
		/* filters comments and leading spaces */
		const auto getLine = [&bufferedInput, &line, isSpace]() -> bool {
			line.clear();
			for ( char c; bufferedInput.readChar( c ); ){
				if( c == '\n' ){ // got line
					return true;
				}
				else if( c == ';' && ( line.empty() || isSpace( line.back() ) ) ){ // entire or trailing comment
					while( bufferedInput.readChar( c ) && c != '\n' ){} // skip it
					return true;
				}
				else if( line.empty() && isSpace( c ) ){ // skip leading spaces
					continue;
				}
				line.push_back( c );
			}
			return !line.empty(); // last line w/o '\n'
		};

		while( getLine() ){
			trimTrailing( line );
			if( line.front() == '[' ){ // section
				if( line.back() == ']' ){
					section.clear();
					section.push_range( line.c_str() + 1, &line.back() );
				}
			}
			else if( char *p = strchr( line.c_str(), '=' ) ){ // key
				key.clear();
				key.push_range( line.c_str(), p );
				trimTrailing( key );
				while( isSpace( *++p ) && !string_empty( p ) ){} // skip leading value spaces
				if( !key.empty() && !section.empty() )
					m_sections[ section.c_str() ][ key.c_str() ] = p;
			}
		}

		return true;
	}
	bool write( const char *filename ) const {
		TextFileOutputStream outFile( filename );
		if( outFile.failed() ){
			return false;
		}

		for( const auto& [ section, keyvalue ] : m_sections ){
			outFile << '[' << section << ']' << '\n';
			for( const auto& [ key, value ] : keyvalue ){
				outFile << key << '=' << value << '\n';
			}
		}

		return true;
	}
	void setValue( const char *section, const char *key, const char *value ){
		m_sections[ section ][ key ] = value;
	}
	std::optional<const char*> getValue( const char *section, const char *key ) const {
		if( auto sec = m_sections.find( section ); sec != m_sections.cend() )
			if( auto k = sec->second.find( key ); k != sec->second.cend() )
				return k->second.c_str();
		return {};
	}
};
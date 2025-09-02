/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#include "iscriplib.h"

class SimpleTokenWriter final : public TokenWriter
{
public:
	SimpleTokenWriter( TextOutputStream& ostream )
		: m_ostream( ostream ), m_separator( '\n' ){
	}
	~SimpleTokenWriter(){
		writeSeparator();
	}
	void release() override {
		delete this;
	}
	void nextLine() override {
		m_separator = '\n';
	}
	void writeToken( const char* token ) override {
		ASSERT_MESSAGE( strchr( token, ' ' ) == 0, "token contains whitespace: " );
		writeSeparator();
		m_ostream << token;
	}
	void writeString( const char* string ) override {
		writeSeparator();
		m_ostream << '"' << string << '"';
	}
	void writeInteger( int i ) override {
		writeSeparator();
		m_ostream << i;
	}
	void writeUnsigned( std::size_t i ) override {
		writeSeparator();
		m_ostream << i;
	}
	void writeFloat( double f ) override {
		writeSeparator();
		m_ostream << Decimal( f );
	}

private:
	void writeSeparator(){
		m_ostream << m_separator;
		m_separator = ' ';
	}
	TextOutputStream& m_ostream;
	char m_separator;
};

inline TokenWriter& NewSimpleTokenWriter( TextOutputStream& ostream ){
	return *( new SimpleTokenWriter( ostream ) );
}

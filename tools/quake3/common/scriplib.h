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

#pragma once

#include "cmdlib.h"

#define MAXTOKEN    1024

extern char token[MAXTOKEN];
extern int scriptline;

/// \param[in] index -1: \p filename is absolute path
/// \param[in] index >= 0: \p filename is relative path in VSF, Nth occurrence of file
/// \return true on success
bool LoadScriptFile( const char *filename, int index = 0, bool verbose = true );
void ParseFromMemory( const char *buffer, size_t size );

/// \param[in] crossline true: write next token to \c token or return false on EOF
/// \param[in] crossline false: find next token on the current line or emit \c Error
bool GetToken( bool crossline );

/// \brief Signals that the current token was not used, and should be reported for the next \c GetToken().
/// Only may be used once between the \c GetToken() calls.
void UnGetToken();

/// \brief
/// \return true, if there is another token on the line.
/// Warning: 2nd of two sequential calls can cross the line.
bool TokenAvailable();

/// \brief Parses next token and emits \c Error, if it's not equal to \p match.
/// Allowed to cross a line.
void MatchToken( const char *match );

template<typename T>
void Parse1DMatrix( int x, T *m );
void Parse2DMatrix( int y, int x, float *m );
void Parse3DMatrix( int z, int y, int x, float *m );

void Write1DMatrix( FILE *f, int x, float *m );
void Write2DMatrix( FILE *f, int y, int x, float *m );
void Write3DMatrix( FILE *f, int z, int y, int x, float *m );

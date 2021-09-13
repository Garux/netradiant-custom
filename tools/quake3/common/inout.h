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

void Broadcast_Setup( const char *dest );
void Broadcast_Shutdown();

#define SYS_VRBflag 8 // verbose support (on/off) //internal only, not for sending!
#define SYS_NOXMLflag 16 // don't send that down the XML stream //internal only, not for sending!

//#define SYS_VRB 0 // verbose support (on/off)
#define SYS_STD 1 // standard print level
#define SYS_WRN 2 // warnings
#define SYS_ERR 3 // error
//#define SYS_NOXML 4 // don't send that down the XML stream

#define SYS_VRB SYS_STD | SYS_VRBflag // verbose support (on/off) //a shortcut, not for sending!

extern bool verbose;
void Sys_Printf( const char *text, ... );
void Sys_FPrintf( int flag, const char *text, ... );
void Sys_Warning( const char *format, ... );
[[ noreturn ]] void Error( const char *error, ... );
#define ENSURE( condition ) \
	(void) \
	( (!!( condition )) || \
	(Error( "%s:%u:%s: Condition '%s' failed.", __FILE__, __LINE__, __func__, #condition ), 0) )

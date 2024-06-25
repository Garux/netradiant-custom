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

#include "bytebool.h"
#include "l_net.h"

bool  WINS_Init( void );
void WINS_Shutdown( void );
int  WINS_Listen( int socket );
int  WINS_Accept( int socket, struct sockaddr_s *addr );
int  WINS_OpenReliableSocket( int port );
int  WINS_CloseSocket( int socket );
int  WINS_Connect( int socket, struct sockaddr_s *addr );
int  WINS_Read( int socket, byte *buf, int len, struct sockaddr_s *addr );
bool WINS_Write( int socket, byte *buf, int len, struct sockaddr_s *addr );
char *WINS_AddrToString( struct sockaddr_s *addr );
int  WINS_StringToAddr( char *string, struct sockaddr_s *addr );
int  WINS_GetSocketAddr( int socket, struct sockaddr_s *addr );

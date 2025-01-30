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

// inout is the only stuff relying on xml, include the headers there
typedef struct _xmlNode xmlNode;
typedef xmlNode *xmlNodePtr;

#include "math/vectorfwd.h"

// some useful xml routines
xmlNodePtr xml_NodeForVec( const Vector3& v );
void xml_SendNode( xmlNodePtr node );
// print a message in q3map output and send the corresponding select information down the xml stream
// bError: do we end with an error on this one or do we go ahead?
void xml_Select( const char *msg, int entitynum, int brushnum, bool bError );
// end q3map with an error message and send a point information in the xml stream
// note: we might want to add a boolean to use this as a warning or an error thing..
void xml_Winding( const char *msg, const Vector3 p[], int numpoints, bool die );
void xml_Point( const char *msg, const Vector3& pt );

#ifdef _DEBUG
#define DBG_XML 1
#endif

#ifdef DBG_XML
void DumpXML();
#endif

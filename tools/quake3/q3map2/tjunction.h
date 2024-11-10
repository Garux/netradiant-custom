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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */


#pragma once

/* dependencies */
#include "q3map2.h"

inline void bspDrawVert_edge_index_write( bspDrawVert_t& dv, int index ){
	dv.lightmap[MAX_LIGHTMAPS - 1][0] = index;
}

inline int bspDrawVert_edge_index_read( const bspDrawVert_t& dv ){
	return dv.lightmap[MAX_LIGHTMAPS - 1][0];
}

inline void bspDrawVert_mark_tjunc( bspDrawVert_t& dv ){
	dv.lightmap[MAX_LIGHTMAPS - 1][1] = 1;
}

inline bool bspDrawVert_is_tjunc( const bspDrawVert_t& dv ){
	return dv.lightmap[MAX_LIGHTMAPS - 1][1] == 1;
}

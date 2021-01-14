/*
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

#include "qbsp.h"


/*

   Lightmap allocation has to be done after all flood filling and
   visible surface determination.

 */

int numSortShaders;
mapDrawSurface_t    **surfsOnShader;
int allocatedSurfsOnShader;


int allocated[ LIGHTMAP_WIDTH ];


void PrepareNewLightmap( void ) {
	memset( allocated, 0, sizeof( allocated ) );
}

/*
   ===============
   AllocLMBlock

   returns a texture number and the position inside it
   ===============
 */
bool AllocLMBlock( int w, int h, int *x, int *y ){
	int i, j;
	int best, best2;

	best = LIGHTMAP_HEIGHT;

	for ( i = 0 ; i <= LIGHTMAP_WIDTH - w ; i++ ) {
		best2 = 0;

		for ( j = 0 ; j < w ; j++ ) {
			if ( allocated[i + j] >= best ) {
				break;
			}
			if ( allocated[i + j] > best2 ) {
				best2 = allocated[i + j];
			}
		}
		if ( j == w ) {   // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if ( best + h > LIGHTMAP_HEIGHT ) {
		return false;
	}

	for ( i = 0 ; i < w ; i++ ) {
		allocated[*x + i] = best + h;
	}

	return true;
}


/*
   ===================
   AllocateLightmapForPatch
   ===================
 */
//#define LIGHTMAP_PATCHSHIFT



/*
   ===================
   AllocateLightmapForSurface
   ===================
 */

//#define	LIGHTMAP_BLOCK	16



/*
   ===================
   AllocateLightmaps
   ===================
 */

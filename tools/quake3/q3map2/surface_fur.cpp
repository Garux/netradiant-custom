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



/* dependencies */
#include "q3map2.h"




/* -------------------------------------------------------------------------------

   ydnar: fur module

   ------------------------------------------------------------------------------- */

/*
   Fur()
   runs the fur processing algorithm on a map drawsurface
 */

void Fur( mapDrawSurface_t& ds ){
	/* dummy check */
	if ( ds.fur || ds.shaderInfo->furNumLayers < 1 ) {
		return;
	}

	/* get basic info */
	const int numLayers = ds.shaderInfo->furNumLayers;
	const float offset = ds.shaderInfo->furOffset;
	const float fade = ds.shaderInfo->furFade * 255.0f;

	/* debug code */
	//%	Sys_FPrintf( SYS_VRB, "Fur():  layers: %d  offset: %f   fade: %f  %s\n",
	//%		numLayers, offset, fade, ds.shaderInfo->shader );

	/* initial offset */
	for ( bspDrawVert_t& dv : ds.verts )
	{
		/* offset is scaled by original vertex alpha */
		const float a = dv.color[ 0 ].alpha() / 255.0;

		/* offset it */
		dv.xyz += dv.normal * ( offset * a );
	}

	/* wash, rinse, repeat */
	for ( int i = 1; i < numLayers; ++i )
	{
		/* clone the surface */
		mapDrawSurface_t *fur = CloneSurface( ds, ds.shaderInfo );
		if ( fur == nullptr ) {
			return;
		}

		/* set it to fur */
		fur->fur = true;

		/* walk the verts */
		for ( size_t j = 0; j < fur->verts.size(); ++j )
		{
			/* offset is scaled by original vertex alpha */
			const float a = ds.verts[ j ].color[ 0 ].alpha() / 255.0;

			/* get fur vert */
			bspDrawVert_t& dv = fur->verts[ j ];

			/* offset it */
			dv.xyz += dv.normal * ( offset * a * i );

			/* fade alpha */
			for ( auto& color : dv.color )
			{
				color.alpha() = color_to_byte( color.alpha() - fade );
			}
		}
	}
}

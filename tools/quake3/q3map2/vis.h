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

/* vis structures */

using visPlane_t = Plane3f;


struct fixedWinding_t
{
	int numpoints;
	Vector3 points[ MAX_POINTS_ON_FIXED_WINDING ];                   /* variable sized */
};


struct passage_t
{
	struct passage_t    *next;
	byte cansee[ 1 ];                   /* all portals that can be seen through this passage */
};


enum class EVStatus
{
	None,
	Working,
	Done
};


struct vportal_t
{
	int num;
	bool hint;                          /* true if this portal was created from a hint splitter */
	bool sky;                           /* true if this portal belongs to a sky leaf */
	bool removed;
	visPlane_t plane;                   /* normal pointing into neighbor */
	int leaf;                           /* neighbor */

	Vector3 origin;                     /* for fast clip testing */
	float radius;

	fixedWinding_t      *winding;
	EVStatus status;
	byte                *portalfront;   /* [portals], preliminary */
	byte                *portalflood;   /* [portals], intermediate */
	byte                *portalvis;     /* [portals], final */

	int nummightsee;                    /* bit count on portalflood for sort */
	passage_t           *passages;      /* there are just as many passages as there */
	                                    /* are portals in the leaf this portal leads */
};


struct leaf_t
{
	int numportals;
	int merged;
	vportal_t           *portals[MAX_PORTALS_ON_LEAF];
};


struct pstack_t
{
	byte mightsee[ MAX_PORTALS / 8 ];
	pstack_t            *next;
	leaf_t              *leaf;
	vportal_t           *portal;        /* portal exiting */
	fixedWinding_t      *source;
	fixedWinding_t      *pass;

	fixedWinding_t windings[ 3 ];       /* source, pass, temp in any order */
	int freewindings[ 3 ];

	visPlane_t portalplane;
	int depth;
#ifdef SEPERATORCACHE
	visPlane_t seperators[ 2 ][ MAX_SEPERATORS ];
	int numseperators[ 2 ];
#endif
};


struct threaddata_t
{
	vportal_t           *base;
	int c_chains;
	pstack_t pstack_head;
};


/* commandline arguments */
inline bool fastvis;
inline bool noPassageVis;
inline bool passageVisOnly;
inline bool mergevis;
inline bool mergevisportals;
inline bool nosort;
inline bool saveprt;
inline bool hint;             /* ydnar */

inline float farPlaneDist;                /* rr2do2, rf, mre, ydnar all contributed to this one... */
inline int farPlaneDistMode;


/* global variables */
inline int numportals;
inline int portalclusters;

inline vportal_t          *portals;
inline leaf_t             *leafs;

inline vportal_t          *faces;
inline leaf_t             *faceleafs;

inline int numfaces;

inline int leafbytes;
inline int portalbytes, portallongs;

extern vportal_t          *sorted_portals[ MAX_MAP_PORTALS * 2 ];

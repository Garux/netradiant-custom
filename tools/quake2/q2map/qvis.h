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
/* Files:

   flow.c
   qvis3.c

 */

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "inout.h"

#ifdef _MSC_VER
	#ifdef NDEBUG                           // Don't show in a Release build
		#pragma warning(disable : 4305)     // truncate from double to float
		#pragma warning(disable : 4244)     // conversion from double to float
		#pragma warning(disable : 4018)     // signed/unsigned mismatch
	#endif
#endif

#define MAX_PORTALS 32768

#define PORTALFILE  "PRT1"

#define ON_EPSILON  0.1

typedef struct
{
	vec3_t normal;
	float dist;
} plane_t;

#define MAX_POINTS_ON_WINDING   64
#define MAX_POINTS_ON_FIXED_WINDING 12

typedef struct
{
	qboolean original;              // don't free, it's part of the portal
	int numpoints;
	vec3_t points[MAX_POINTS_ON_FIXED_WINDING];             // variable sized
} winding_t;

winding_t   *NewWinding( int points );
void        FreeWinding( winding_t *w );
winding_t   *CopyWinding( winding_t *w );


typedef enum {stat_none, stat_working, stat_done} vstatus_t;
typedef struct
{
	plane_t plane;      // normal pointing into neighbor
	int leaf;           // neighbor

	vec3_t origin;      // for fast clip testing
	float radius;

	winding_t   *winding;
	vstatus_t status;
	byte        *portalfront;   // [portals], preliminary
	byte        *portalflood;   // [portals], intermediate
	byte        *portalvis;     // [portals], final

	int nummightsee;            // bit count on portalflood for sort
} portal_t;

typedef struct seperating_plane_s
{
	struct seperating_plane_s *next;
	plane_t plane;          // from portal is on positive side
} sep_t;


typedef struct passage_s
{
	struct passage_s    *next;
	int from, to;               // leaf numbers
	sep_t               *planes;
} passage_t;

#define MAX_PORTALS_ON_LEAF     128
typedef struct leaf_s
{
	int numportals;
	passage_t   *passages;
	portal_t    *portals[MAX_PORTALS_ON_LEAF];
} leaf_t;


typedef struct pstack_s
{
	byte mightsee[MAX_PORTALS / 8];             // bit string
	struct pstack_s *next;
	leaf_t      *leaf;
	portal_t    *portal;    // portal exiting
	winding_t   *source;
	winding_t   *pass;

	winding_t windings[3];      // source, pass, temp in any order
	int freewindings[3];

	plane_t portalplane;
} pstack_t;

typedef struct
{
	portal_t    *base;
	int c_chains;
	pstack_t pstack_head;
} threaddata_t;



extern int numportals;
extern int portalclusters;

extern portal_t    *portals;
extern leaf_t      *leafs;

extern int c_portaltest, c_portalpass, c_portalcheck;
extern int c_portalskip, c_leafskip;
extern int c_vistest, c_mighttest;
extern int c_chains;

extern byte    *vismap, *vismap_p, *vismap_end;     // past visfile

extern int testlevel;

extern byte        *uncompressed;

extern int leafbytes, leaflongs;
extern int portalbytes, portallongs;


void LeafFlow( int leafnum );


void BasePortalVis( int portalnum );
void BetterPortalVis( int portalnum );
void PortalFlow( int portalnum );

extern portal_t    *sorted_portals[MAX_MAP_PORTALS * 2];

int CountBits( byte *bits, int numbits );

//=============================================================================

// externs

extern char *mapname;

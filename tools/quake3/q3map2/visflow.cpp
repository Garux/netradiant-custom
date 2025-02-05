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
#include "vis.h"




/*

   each portal will have a list of all possible to see from first portal

   if ( !thread->portalmightsee[portalnum] )

   portal mightsee

   for p2 = all other portals in leaf
    get sperating planes
    for all portals that might be seen by p2
        mark as unseen if not present in separating plane
    flood fill a new mightsee
    save as passagemightsee


   void CalcMightSee( leaf_t *leaf,
 */

int CountBits( const byte *bits, int numbits ){
	int c = 0;
	for ( int i = 0; i < numbits; ++i )
		if ( bit_is_enabled( bits, i ) ) {
			c++;
		}

	return c;
}


static void CheckStack( leaf_t *leaf, threaddata_t *thread ){
	for ( pstack_t *p = thread->pstack_head.next; p; p = p->next )
	{
//		Sys_Printf( "=" );
		if ( p->leaf == leaf ) {
			Error( "CheckStack: leaf recursion" );
		}
		for ( pstack_t *p2 = thread->pstack_head.next; p2 != p; p2 = p2->next )
			if ( p2->leaf == p->leaf ) {
				Error( "CheckStack: late leaf recursion" );
			}
	}
//	Sys_Printf( "\n" );
}


static fixedWinding_t *AllocStackWinding( pstack_t *stack ){
	for ( int i = 0; i < 3; ++i )
	{
		if ( stack->freewindings[i] ) {
			stack->freewindings[i] = 0;
			return &stack->windings[i];
		}
	}

	Error( "AllocStackWinding: failed" );

	return NULL;
}

static void FreeStackWinding( fixedWinding_t *w, pstack_t *stack ){
	const int i = w - stack->windings;

	if ( i < 0 || i > 2 ) {
		return;     // not from local

	}
	if ( stack->freewindings[i] ) {
		Error( "FreeStackWinding: already free" );
	}
	stack->freewindings[i] = 1;
}

/*
   ==============
   VisChopWinding

   ==============
 */
static fixedWinding_t  *VisChopWinding( fixedWinding_t *in, pstack_t *stack, const visPlane_t& split ){
	float dists[128];
	EPlaneSide sides[128];
	int counts[3];
	float dot;
	int i, j;
	Vector3 mid;
	fixedWinding_t  *neww;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0; i < in->numpoints; ++i )
	{
		dists[i] = plane3_distance_to_point( split, in->points[i] );
		if ( dists[i] > ON_EPSILON ) {
			sides[i] = eSideFront;
		}
		else if ( dists[i] < -ON_EPSILON ) {
			sides[i] = eSideBack;
		}
		else
		{
			sides[i] = eSideOn;
		}
		counts[sides[i]]++;
	}

	if ( !counts[1] ) {
		return in;      // completely on front side

	}
	if ( !counts[0] ) {
		FreeStackWinding( in, stack );
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	neww = AllocStackWinding( stack );

	neww->numpoints = 0;

	for ( i = 0; i < in->numpoints; ++i )
	{
		const Vector3& p1 = in->points[i];

		if ( neww->numpoints == MAX_POINTS_ON_FIXED_WINDING ) {
			FreeStackWinding( neww, stack );
			return in;      // can't chop -- fall back to original
		}

		if ( sides[i] == eSideOn ) {
			neww->points[neww->numpoints] = p1;
			neww->numpoints++;
			continue;
		}

		if ( sides[i] == eSideFront ) {
			neww->points[neww->numpoints] = p1;
			neww->numpoints++;
		}

		if ( sides[i + 1] == eSideOn || sides[i + 1] == sides[i] ) {
			continue;
		}

		if ( neww->numpoints == MAX_POINTS_ON_FIXED_WINDING ) {
			FreeStackWinding( neww, stack );
			return in;      // can't chop -- fall back to original
		}

		// generate a split point
		const Vector3& p2 = in->points[( i + 1 ) % in->numpoints];

		dot = dists[i] / ( dists[i] - dists[i + 1] );
		for ( j = 0; j < 3; ++j )
		{	// avoid round off error when possible
			if ( split.normal()[j] == 1 ) {
				mid[j] = split.dist();
			}
			else if ( split.normal()[j] == -1 ) {
				mid[j] = -split.dist();
			}
			else{
				mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
			}
		}

		neww->points[neww->numpoints] = mid;
		neww->numpoints++;
	}

	// free the original winding
	FreeStackWinding( in, stack );

	return neww;
}

/*
   ==============
   ClipToSeperators

   Source, pass, and target are an ordering of portals.

   Generates separating planes canidates by taking two points from source and one
   point from pass, and clips target by them.

   If target is totally clipped away, that portal can not be seen through.

   Normal clip keeps target on the same side as pass, which is correct if the
   order goes source, pass, target.  If the order goes pass, source, target then
   flipclip should be set.
   ==============
 */
static fixedWinding_t  *ClipToSeperators( fixedWinding_t *source, fixedWinding_t *pass, fixedWinding_t *target, bool flipclip, pstack_t *stack ){
	int i, j, k, l;
	float d;
	int counts[3];
	bool fliptest;

	// check all combinations
	for ( i = 0; i < source->numpoints; ++i )
	{
		l = ( i + 1 ) % source->numpoints;

		// find a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for ( j = 0; j < pass->numpoints; ++j )
		{
			visPlane_t plane;
			// if points don't make a valid plane, skip it
			if ( !PlaneFromPoints( plane, source->points[i], pass->points[j], source->points[l] ) ) {
				continue;
			}

			//
			// find out which side of the generated separating plane has the
			// source portal
			//
#if 1
			fliptest = false;
			for ( k = 0; k < source->numpoints; ++k )
			{
				if ( k == i || k == l ) {
					continue;
				}
				d = plane3_distance_to_point( plane, source->points[k] );
				if ( d < -ON_EPSILON ) { // source is on the negative side, so we want all
				                        // pass and target on the positive side
					fliptest = false;
					break;
				}
				else if ( d > ON_EPSILON ) { // source is on the positive side, so we want all
				                            // pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if ( k == source->numpoints ) {
				continue;       // planar with source portal
			}
#else
			fliptest = flipclip;
#endif
			//
			// flip the normal if the source portal is backwards
			//
			if ( fliptest ) {
				plane = plane3_flipped( plane );
			}
#if 1
			//
			// if all of the pass portal points are now on the positive side,
			// this is the separating plane
			//
			counts[0] = counts[1] = counts[2] = 0;
			for ( k = 0; k < pass->numpoints; ++k )
			{
				if ( k == j ) {
					continue;
				}
				d = plane3_distance_to_point( plane, pass->points[k] );
				if ( d < -ON_EPSILON ) {
					break;
				}
				else if ( d > ON_EPSILON ) {
					counts[0]++;
				}
				else{
					counts[2]++;
				}
			}
			if ( k != pass->numpoints ) {
				continue;   // points on negative side, not a separating plane

			}
			if ( !counts[0] ) {
				continue;   // planar with separating plane
			}
#else
			k = ( j + 1 ) % pass->numpoints;
			d = plane3_distance_to_point( plane, pass->points[k] );
			if ( d < -ON_EPSILON ) {
				continue;
			}
			k = ( j + pass->numpoints - 1 ) % pass->numpoints;
			d = plane3_distance_to_point( plane, pass->points[k] );
			if ( d < -ON_EPSILON ) {
				continue;
			}
#endif
			//
			// flip the normal if we want the back side
			//
			if ( flipclip ) {
				plane = plane3_flipped( plane );
			}

#ifdef SEPERATORCACHE
			stack->seperators[flipclip][stack->numseperators[flipclip]] = plane;
			if ( ++stack->numseperators[flipclip] >= MAX_SEPERATORS ) {
				Error( "MAX_SEPERATORS" );
			}
#endif
			//MrE: fast check first
			d = plane3_distance_to_point( plane, stack->portal->origin );
			//if completely at the back of the separator plane
			if ( d < -stack->portal->radius ) {
				return NULL;
			}
			//if completely on the front of the separator plane
			if ( d > stack->portal->radius ) {
				break;
			}

			//
			// clip target by the separating plane
			//
			target = VisChopWinding( target, stack, plane );
			if ( !target ) {
				return NULL;        // target is not visible

			}
			break;      // optimization by Antony Suter
		}
	}

	return target;
}

/*
   ==================
   RecursiveLeafFlow

   Flood fill through the leafs
   If src_portal is NULL, this is the originating leaf
   ==================
 */
static void RecursiveLeafFlow( int leafnum, threaddata_t *thread, pstack_t *prevstack ){
	pstack_t stack;
	visPlane_t backplane;
	leaf_t      *leaf;
	int j, n;
	long        *test, *might, *prevmight, *vis, more;

	thread->c_chains++;

	leaf = &leafs[leafnum];
//	CheckStack( leaf, thread );

	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;
	stack.depth = prevstack->depth + 1;

#ifdef SEPERATORCACHE
	stack.numseperators[0] = 0;
	stack.numseperators[1] = 0;
#endif

	might = (long *)stack.mightsee;
	vis = (long *)thread->base->portalvis;

	// check all portals for flowing into other leafs
	for ( vportal_t *p : Span( leaf->portals, leaf->numportals ) )
	{
		if ( p->removed ) {
			continue;
		}
		const int pnum = p - portals;

		/* MrE: portal trace debug code
		   {
		    int portaltrace[] = { 13, 16, 17, 37 };
		    pstack_t *s;

		    s = &thread->pstack_head;
		    for ( j = 0; s->next && j < sizeof( portaltrace ) / sizeof( int ) - 1; j++, s = s->next )
		    {
		        if ( s->portal->num != portaltrace[j] )
		            break;
		    }
		    if ( j >= sizeof( portaltrace ) / sizeof( int ) - 1 )
		    {
		        if ( p->num == portaltrace[j] )
		            n = 0; //traced through all the portals
		    }
		   }
		 */

		if ( !bit_is_enabled( prevstack->mightsee, pnum ) ) {
			continue;   // can't possibly see it
		}

		// if the portal can't see anything we haven't already seen, skip it
		if ( p->status == EVStatus::Done ) {
			test = (long *)p->portalvis;
		}
		else
		{
			test = (long *)p->portalflood;
		}

		more = 0;
		prevmight = (long *)prevstack->mightsee;
		for ( j = 0; j < portallongs; ++j )
		{
			might[j] = prevmight[j] & test[j];
			more |= ( might[j] & ~vis[j] );
		}

		if ( !more &&
		     bit_is_enabled( thread->base->portalvis, pnum ) ) { // can't see anything new
			continue;
		}

		// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		backplane = plane3_flipped( p->plane );

		stack.portal = p;
		stack.next = NULL;
		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;

#if 1
		{
			const float d = plane3_distance_to_point( thread->pstack_head.portalplane, p->origin );
			if ( d < -p->radius ) {
				continue;
			}
			else if ( d > p->radius ) {
				stack.pass = p->winding;
			}
			else
			{
				stack.pass = VisChopWinding( p->winding, &stack, thread->pstack_head.portalplane );
				if ( !stack.pass ) {
					continue;
				}
			}
		}
#else
		stack.pass = VisChopWinding( p->winding, &stack, &thread->pstack_head.portalplane );
		if ( !stack.pass ) {
			continue;
		}
#endif


#if 1
		{
			const float d = plane3_distance_to_point( p->plane, thread->base->origin );
			//MrE: vis-bug fix
			//if ( d > p->radius )
			if ( d > thread->base->radius ) {
				continue;
			}
			//MrE: vis-bug fix
			//if ( d < -p->radius )
			else if ( d < -thread->base->radius ) {
				stack.source = prevstack->source;
			}
			else
			{
				stack.source = VisChopWinding( prevstack->source, &stack, backplane );
				//FIXME: shouldn't we create a new source origin and radius for fast checks?
				if ( !stack.source ) {
					continue;
				}
			}
		}
#else
		stack.source = VisChopWinding( prevstack->source, &stack, backplane );
		if ( !stack.source ) {
			continue;
		}
#endif

		if ( !prevstack->pass ) { // the second leaf can only be blocked if coplanar

			// mark the portal as visible
			bit_enable( thread->base->portalvis, pnum );

			RecursiveLeafFlow( p->leaf, thread, &stack );
			continue;
		}

#ifdef SEPERATORCACHE
		if ( stack.numseperators[0] ) {
			for ( n = 0; n < stack.numseperators[0]; n++ )
			{
				stack.pass = VisChopWinding( stack.pass, &stack, stack.seperators[0][n] );
				if ( !stack.pass ) {
					break;      // target is not visible
				}
			}
			if ( n < stack.numseperators[0] ) {
				continue;
			}
		}
		else
		{
			stack.pass = ClipToSeperators( prevstack->source, prevstack->pass, stack.pass, false, &stack );
		}
#else
		stack.pass = ClipToSeperators( stack.source, prevstack->pass, stack.pass, false, &stack );
#endif
		if ( !stack.pass ) {
			continue;
		}

#ifdef SEPERATORCACHE
		if ( stack.numseperators[1] ) {
			for ( n = 0; n < stack.numseperators[1]; n++ )
			{
				stack.pass = VisChopWinding( stack.pass, &stack, stack.seperators[1][n] );
				if ( !stack.pass ) {
					break;      // target is not visible
				}
			}
		}
		else
		{
			stack.pass = ClipToSeperators( prevstack->pass, prevstack->source, stack.pass, true, &stack );
		}
#else
		stack.pass = ClipToSeperators( prevstack->pass, stack.source, stack.pass, true, &stack );
#endif
		if ( !stack.pass ) {
			continue;
		}

		// mark the portal as visible
		bit_enable( thread->base->portalvis, pnum );

		// flow through it for real
		RecursiveLeafFlow( p->leaf, thread, &stack );
		//
		stack.next = NULL;
	}
}

/*
   ===============
   PortalFlow

   generates the portalvis bit vector
   ===============
 */
void PortalFlow( int portalnum ){
	threaddata_t data;
	vportal_t       *p;
	int c_might, c_can;

#ifdef MREDEBUG
	Sys_Printf( "\r%6d", portalnum );
#endif

	p = sorted_portals[portalnum];

	if ( p->removed ) {
		p->status = EVStatus::Done;
		return;
	}

	p->status = EVStatus::Working;

	c_might = CountBits( p->portalflood, numportals * 2 );

	memset( &data, 0, sizeof( data ) );
	data.base = p;

	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.depth = 0;
	memcpy( data.pstack_head.mightsee, p->portalflood, portalbytes );

	RecursiveLeafFlow( p->leaf, &data, &data.pstack_head );

	p->status = EVStatus::Done;

	c_can = CountBits( p->portalvis, numportals * 2 );

	Sys_FPrintf( SYS_VRB, "portal:%4i  mightsee:%4i  cansee:%4i (%i chains)\n",
	             (int)( p - portals ), c_might, c_can, data.c_chains );
}

/*
   ==================
   RecursivePassageFlow
   ==================
 */
static void RecursivePassageFlow( vportal_t *portal, threaddata_t *thread, pstack_t *prevstack ){
	pstack_t stack;
	vportal_t   *p;
	leaf_t      *leaf;
	passage_t   *passage, *nextpassage;
	int i, j;
	long        *might, *vis, *prevmight, *cansee, *portalvis, more;

	leaf = &leafs[portal->leaf];

	prevstack->next = &stack;

	stack.next = NULL;
	stack.depth = prevstack->depth + 1;

	vis = (long *)thread->base->portalvis;

	passage = portal->passages;
	nextpassage = passage;
	// check all portals for flowing into other leafs
	for ( i = 0; i < leaf->numportals; i++, passage = nextpassage )
	{
		p = leaf->portals[i];
		if ( p->removed ) {
			continue;
		}
		nextpassage = passage->next;
		const int pnum = p - portals;

		if ( !bit_is_enabled( prevstack->mightsee, pnum ) ) {
			continue;   // can't possibly see it
		}

		// mark the portal as visible
		bit_enable( thread->base->portalvis, pnum );

		prevmight = (long *)prevstack->mightsee;
		cansee = (long *)passage->cansee;
		might = (long *)stack.mightsee;
		memcpy( might, prevmight, portalbytes );
		if ( p->status == EVStatus::Done ) {
			portalvis = (long *) p->portalvis;
		}
		else{
			portalvis = (long *) p->portalflood;
		}
		more = 0;
		for ( j = 0; j < portallongs; j++ )
		{
			if ( *might ) {
				*might &= *cansee & *portalvis;
				more |= ( *might & ~vis[j] );
			}
			cansee++;
			portalvis++;
			might++;
		}

		if ( !more ) {
			// can't see anything new
			continue;
		}

		// flow through it for real
		RecursivePassageFlow( p, thread, &stack );

		stack.next = NULL;
	}
}

/*
   ===============
   PassageFlow
   ===============
 */
void PassageFlow( int portalnum ){
	threaddata_t data;
	vportal_t       *p;
//	int             c_might, c_can;

#ifdef MREDEBUG
	Sys_Printf( "\r%6d", portalnum );
#endif

	p = sorted_portals[portalnum];

	if ( p->removed ) {
		p->status = EVStatus::Done;
		return;
	}

	p->status = EVStatus::Working;

//	c_might = CountBits( p->portalflood, numportals * 2 );

	memset( &data, 0, sizeof( data ) );
	data.base = p;

	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.depth = 0;
	memcpy( data.pstack_head.mightsee, p->portalflood, portalbytes );

	RecursivePassageFlow( p, &data, &data.pstack_head );

	p->status = EVStatus::Done;

	/*
	   c_can = CountBits( p->portalvis, numportals * 2 );

	   Sys_FPrintf( SYS_VRB, "portal:%4i  mightsee:%4i  cansee:%4i (%i chains)\n",
	    (int)( p - portals ), c_might, c_can, data.c_chains );
	 */
}

/*
   ==================
   RecursivePassagePortalFlow
   ==================
 */
static void RecursivePassagePortalFlow( vportal_t *portal, threaddata_t *thread, pstack_t *prevstack ){
	pstack_t stack;
	vportal_t   *p;
	leaf_t      *leaf;
	visPlane_t backplane;
	passage_t   *passage, *nextpassage;
	int i, j, n;
	long        *might, *vis, *prevmight, *cansee, *portalvis, more;

//	thread->c_chains++;

	leaf = &leafs[portal->leaf];
//	CheckStack( leaf, thread );

	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;
	stack.depth = prevstack->depth + 1;

#ifdef SEPERATORCACHE
	stack.numseperators[0] = 0;
	stack.numseperators[1] = 0;
#endif

	vis = (long *)thread->base->portalvis;

	passage = portal->passages;
	nextpassage = passage;
	// check all portals for flowing into other leafs
	for ( i = 0; i < leaf->numportals; i++, passage = nextpassage )
	{
		p = leaf->portals[i];
		if ( p->removed ) {
			continue;
		}
		nextpassage = passage->next;
		const int pnum = p - portals;

		if ( !bit_is_enabled( prevstack->mightsee, pnum ) ) {
			continue;   // can't possibly see it

		}
		prevmight = (long *)prevstack->mightsee;
		cansee = (long *)passage->cansee;
		might = (long *)stack.mightsee;
		memcpy( might, prevmight, portalbytes );
		if ( p->status == EVStatus::Done ) {
			portalvis = (long *) p->portalvis;
		}
		else{
			portalvis = (long *) p->portalflood;
		}
		more = 0;
		for ( j = 0; j < portallongs; j++ )
		{
			if ( *might ) {
				*might &= *cansee & *portalvis;
				more |= ( *might & ~vis[j] );
			}
			cansee++;
			portalvis++;
			might++;
		}

		if ( !more && bit_is_enabled( thread->base->portalvis, pnum ) ) { // can't see anything new
			continue;
		}

		// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		backplane = plane3_flipped( p->plane );

		stack.portal = p;
		stack.next = NULL;
		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;

#if 1
		{
			const float d = plane3_distance_to_point( thread->pstack_head.portalplane, p->origin );
			if ( d < -p->radius ) {
				continue;
			}
			else if ( d > p->radius ) {
				stack.pass = p->winding;
			}
			else
			{
				stack.pass = VisChopWinding( p->winding, &stack, thread->pstack_head.portalplane );
				if ( !stack.pass ) {
					continue;
				}
			}
		}
#else
		stack.pass = VisChopWinding( p->winding, &stack, thread->pstack_head.portalplane );
		if ( !stack.pass ) {
			continue;
		}
#endif


#if 1
		{
			const float d = plane3_distance_to_point( p->plane, thread->base->origin );
			//MrE: vis-bug fix
			//if ( d > p->radius )
			if ( d > thread->base->radius ) {
				continue;
			}
			//MrE: vis-bug fix
			//if ( d < -p->radius )
			else if ( d < -thread->base->radius ) {
				stack.source = prevstack->source;
			}
			else
			{
				stack.source = VisChopWinding( prevstack->source, &stack, backplane );
				//FIXME: shouldn't we create a new source origin and radius for fast checks?
				if ( !stack.source ) {
					continue;
				}
			}
		}
#else
		stack.source = VisChopWinding( prevstack->source, &stack, backplane );
		if ( !stack.source ) {
			continue;
		}
#endif

		if ( !prevstack->pass ) { // the second leaf can only be blocked if coplanar

			// mark the portal as visible
			bit_enable( thread->base->portalvis, pnum );

			RecursivePassagePortalFlow( p, thread, &stack );
			continue;
		}

#ifdef SEPERATORCACHE
		if ( stack.numseperators[0] ) {
			for ( n = 0; n < stack.numseperators[0]; n++ )
			{
				stack.pass = VisChopWinding( stack.pass, &stack, stack.seperators[0][n] );
				if ( !stack.pass ) {
					break;      // target is not visible
				}
			}
			if ( n < stack.numseperators[0] ) {
				continue;
			}
		}
		else
		{
			stack.pass = ClipToSeperators( prevstack->source, prevstack->pass, stack.pass, false, &stack );
		}
#else
		stack.pass = ClipToSeperators( stack.source, prevstack->pass, stack.pass, false, &stack );
#endif
		if ( !stack.pass ) {
			continue;
		}

#ifdef SEPERATORCACHE
		if ( stack.numseperators[1] ) {
			for ( n = 0; n < stack.numseperators[1]; n++ )
			{
				stack.pass = VisChopWinding( stack.pass, &stack, stack.seperators[1][n] );
				if ( !stack.pass ) {
					break;      // target is not visible
				}
			}
		}
		else
		{
			stack.pass = ClipToSeperators( prevstack->pass, prevstack->source, stack.pass, true, &stack );
		}
#else
		stack.pass = ClipToSeperators( prevstack->pass, stack.source, stack.pass, true, &stack );
#endif
		if ( !stack.pass ) {
			continue;
		}

		// mark the portal as visible
		bit_enable( thread->base->portalvis, pnum );

		// flow through it for real
		RecursivePassagePortalFlow( p, thread, &stack );
		//
		stack.next = NULL;
	}
}

/*
   ===============
   PassagePortalFlow
   ===============
 */
void PassagePortalFlow( int portalnum ){
	threaddata_t data;
	vportal_t       *p;
//	int				c_might, c_can;

#ifdef MREDEBUG
	Sys_Printf( "\r%6d", portalnum );
#endif

	p = sorted_portals[portalnum];

	if ( p->removed ) {
		p->status = EVStatus::Done;
		return;
	}

	p->status = EVStatus::Working;

//	c_might = CountBits( p->portalflood, numportals * 2 );

	memset( &data, 0, sizeof( data ) );
	data.base = p;

	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.depth = 0;
	memcpy( data.pstack_head.mightsee, p->portalflood, portalbytes );

	RecursivePassagePortalFlow( p, &data, &data.pstack_head );

	p->status = EVStatus::Done;

	/*
	   c_can = CountBits( p->portalvis, numportals * 2 );

	   Sys_FPrintf( SYS_VRB, "portal:%4i  mightsee:%4i  cansee:%4i (%i chains)\n",
	    (int)( p - portals ), c_might, c_can, data.c_chains );
	 */
}

static fixedWinding_t *PassageChopWinding( fixedWinding_t *in, fixedWinding_t *out, const visPlane_t& split ){
	float dists[128];
	EPlaneSide sides[128];
	int counts[3];
	float dot;
	int i, j;
	Vector3 mid;
	fixedWinding_t  *neww;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0; i < in->numpoints; ++i )
	{
		dists[i] = plane3_distance_to_point( split, in->points[i] );
		if ( dists[i] > ON_EPSILON ) {
			sides[i] = eSideFront;
		}
		else if ( dists[i] < -ON_EPSILON ) {
			sides[i] = eSideBack;
		}
		else
		{
			sides[i] = eSideOn;
		}
		counts[sides[i]]++;
	}

	if ( !counts[1] ) {
		return in;      // completely on front side

	}
	if ( !counts[0] ) {
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	neww = out;

	neww->numpoints = 0;

	for ( i = 0; i < in->numpoints; ++i )
	{
		const Vector3& p1 = in->points[i];

		if ( neww->numpoints == MAX_POINTS_ON_FIXED_WINDING ) {
			return in;      // can't chop -- fall back to original
		}

		if ( sides[i] == eSideOn ) {
			neww->points[neww->numpoints] = p1;
			neww->numpoints++;
			continue;
		}

		if ( sides[i] == eSideFront ) {
			neww->points[neww->numpoints] = p1;
			neww->numpoints++;
		}

		if ( sides[i + 1] == eSideOn || sides[i + 1] == sides[i] ) {
			continue;
		}

		if ( neww->numpoints == MAX_POINTS_ON_FIXED_WINDING ) {
			return in;      // can't chop -- fall back to original
		}

		// generate a split point
		const Vector3& p2 = in->points[( i + 1 ) % in->numpoints];

		dot = dists[i] / ( dists[i] - dists[i + 1] );
		for ( j = 0; j < 3; ++j )
		{	// avoid round off error when possible
			if ( split.normal()[j] == 1 ) {
				mid[j] = split.dist();
			}
			else if ( split.normal()[j] == -1 ) {
				mid[j] = -split.dist();
			}
			else{
				mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
			}
		}

		neww->points[neww->numpoints] = mid;
		neww->numpoints++;
	}

	return neww;
}

/*
   ===============
   AddSeperators
   ===============
 */
static int AddSeperators( const fixedWinding_t *source, const fixedWinding_t *pass, bool flipclip, visPlane_t *seperators, int maxseperators ){
	int i, j, k, l;
	int counts[3], numseperators;
	bool fliptest;

	numseperators = 0;
	// check all combinations
	for ( i = 0; i < source->numpoints; ++i )
	{
		l = ( i + 1 ) % source->numpoints;

		// find a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for ( j = 0; j < pass->numpoints; ++j )
		{
			visPlane_t plane;
			// if points don't make a valid plane, skip it
			if ( !PlaneFromPoints( plane, source->points[i], pass->points[j], source->points[l] ) ) {
				continue;
			}

			//
			// find out which side of the generated separating plane has the
			// source portal
			//
#if 1
			fliptest = false;
			for ( k = 0; k < source->numpoints; ++k )
			{
				if ( k == i || k == l ) {
					continue;
				}
				const double d = plane3_distance_to_point( plane, source->points[k] );
				if ( d < -ON_EPSILON ) { // source is on the negative side, so we want all
				                        // pass and target on the positive side
					fliptest = false;
					break;
				}
				else if ( d > ON_EPSILON ) { // source is on the positive side, so we want all
				                            // pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if ( k == source->numpoints ) {
				continue;       // planar with source portal
			}
#else
			fliptest = flipclip;
#endif
			//
			// flip the normal if the source portal is backwards
			//
			if ( fliptest ) {
				plane = plane3_flipped( plane );
			}
#if 1
			//
			// if all of the pass portal points are now on the positive side,
			// this is the separating plane
			//
			counts[0] = counts[1] = counts[2] = 0;
			for ( k = 0; k < pass->numpoints; ++k )
			{
				if ( k == j ) {
					continue;
				}
				const double d = plane3_distance_to_point( plane, pass->points[k] );
				if ( d < -ON_EPSILON ) {
					break;
				}
				else if ( d > ON_EPSILON ) {
					counts[0]++;
				}
				else{
					counts[2]++;
				}
			}
			if ( k != pass->numpoints ) {
				continue;   // points on negative side, not a separating plane

			}
			if ( !counts[0] ) {
				continue;   // planar with separating plane
			}
#else
			k = ( j + 1 ) % pass->numpoints;
			d = vector3_dot( pass->points[k], plane.normal ) - plane.dist;
			if ( d < -ON_EPSILON ) {
				continue;
			}
			k = ( j + pass->numpoints - 1 ) % pass->numpoints;
			d = vector3_dot( pass->points[k], plane.normal ) - plane.dist;
			if ( d < -ON_EPSILON ) {
				continue;
			}
#endif
			//
			// flip the normal if we want the back side
			//
			if ( flipclip ) {
				plane = plane3_flipped( plane );
			}

			if ( numseperators >= maxseperators ) {
				Error( "max seperators" );
			}
			seperators[numseperators] = plane;
			numseperators++;
			break;
		}
	}
	return numseperators;
}

/*
   ===============
   CreatePassages

   MrE: create passages from one portal to all the portals in the leaf the portal leads to
     every passage has a cansee bit string with all the portals that can be
     seen through the passage
   ===============
 */
void CreatePassages( int portalnum ){
	int j, k, n, numseperators, numsee;
	vportal_t       *portal, *p;
	passage_t       *passage, *lastpassage;
	visPlane_t seperators[MAX_SEPERATORS * 2];
	fixedWinding_t  *w;
	fixedWinding_t in, out, *res;


#ifdef MREDEBUG
	Sys_Printf( "\r%6d", portalnum );
#endif

	portal = sorted_portals[portalnum];

	if ( portal->removed ) {
		portal->status = EVStatus::Done;
		return;
	}

	lastpassage = NULL;
	for ( const vportal_t *target : Span( leafs[portal->leaf].portals, leafs[portal->leaf].numportals ) )
	{
		if ( target->removed ) {
			continue;
		}

		passage = safe_calloc( sizeof( passage_t ) + portalbytes );
		numseperators = AddSeperators( portal->winding, target->winding, false, seperators, MAX_SEPERATORS * 2 );
		numseperators += AddSeperators( target->winding, portal->winding, true, &seperators[numseperators], MAX_SEPERATORS * 2 - numseperators );

		passage->next = NULL;
		if ( lastpassage ) {
			lastpassage->next = passage;
		}
		else{
			portal->passages = passage;
		}
		lastpassage = passage;

		numsee = 0;
		//create the passage->cansee
		for ( j = 0; j < numportals * 2; j++ )
		{
			p = &portals[j];
			if ( p->removed ) {
				continue;
			}
			if ( !bit_is_enabled( target->portalflood, j ) ) {
				continue;
			}
			if ( !bit_is_enabled( portal->portalflood, j ) ) {
				continue;
			}
			for ( k = 0; k < numseperators; k++ )
			{
				//if completely at the back of the separator plane
				if ( plane3_distance_to_point( seperators[k], p->origin ) < -p->radius + ON_EPSILON ) {
					break;
				}
				w = p->winding;
				for ( n = 0; n < w->numpoints; n++ )
				{
					//if at the front of the separator
					if ( plane3_distance_to_point( seperators[k], w->points[n] ) > ON_EPSILON ) {
						break;
					}
				}
				//if no points are at the front of the separator
				if ( n >= w->numpoints ) {
					break;
				}
			}
			if ( k < numseperators ) {
				continue;
			}

			/* explitive deleted */


			/* ydnar: prefer correctness to stack overflow  */
			//% memcpy( &in, p->winding, (int)((fixedWinding_t *)0)->points[p->winding->numpoints] );
			if ( p->winding->numpoints <= MAX_POINTS_ON_FIXED_WINDING ) {
				memcpy( &in, p->winding, offsetof_array( fixedWinding_t, points, p->winding->numpoints ) );
			}
			else{
				memcpy( &in, p->winding, sizeof( fixedWinding_t ) );
			}


			for ( k = 0; k < numseperators; k++ )
			{
				/* ydnar: this is a shitty crutch */
				//% if ( in.numpoints > MAX_POINTS_ON_FIXED_WINDING ) Sys_Printf( "[%d]", p->winding->numpoints );
				value_minimize( in.numpoints, MAX_POINTS_ON_FIXED_WINDING );

				res = PassageChopWinding( &in, &out, seperators[ k ] );
				if ( res == &out ) {
					memcpy( &in, &out, sizeof( fixedWinding_t ) );
				}


				if ( res == NULL ) {
					break;
				}
			}
			if ( k < numseperators ) {
				continue;
			}
			bit_enable( passage->cansee, j );
			numsee++;
		}
	}
}

void PassageMemory(){
	int totalmem = 0, totalportals = 0;

	for ( const vportal_t *portal : Span( sorted_portals, numportals ) )
	{
		if ( portal->removed ) {
			continue;
		}
		for ( const vportal_t *target : Span( leafs[portal->leaf].portals, leafs[portal->leaf].numportals ) )
		{
			if ( target->removed ) {
				continue;
			}
			totalmem += sizeof( passage_t ) + portalbytes;
			totalportals++;
		}
	}
	Sys_Printf( "%7i average number of passages per leaf\n", totalportals / numportals );
	Sys_Printf( "%7i MB required passage memory\n", totalmem >> 10 >> 10 );
}

/*
   ===============================================================================

   This is a rough first-order aproximation that is used to trivially reject some
   of the final calculations.


   Calculates portalfront and portalflood bit vectors

   thinking about:

   typedef struct passage_s
   {
    struct passage_s	*next;
    struct portal_s		*to;
    stryct sep_s		*seperators;
    byte				*mightsee;
   } passage_t;

   typedef struct portal_s
   {
    struct passage_s	*passages;
    int					leaf;		// leaf portal faces into
   } portal_s;

   leaf = portal->leaf
   clear
   for all portals


   calc portal visibility
    clear bit vector
    for all passages
        passage visibility


   for a portal to be visible to a passage, it must be on the front of
   all separating planes, and both portals must be behind the new portal

   ===============================================================================
 */



/*
   ==================
   SimpleFlood

   ==================
 */
static void SimpleFlood( vportal_t *srcportal, int leafnum ){
	for ( const vportal_t *p : Span( leafs[leafnum].portals, leafs[leafnum].numportals ) )
	{
		if ( p->removed ) {
			continue;
		}
		const int pnum = p - portals;
		if ( !bit_is_enabled( srcportal->portalfront, pnum ) ) {
			continue;
		}

		if ( bit_is_enabled( srcportal->portalflood, pnum ) ) {
			continue;
		}

		bit_enable( srcportal->portalflood, pnum );

		SimpleFlood( srcportal, p->leaf );
	}
}

/*
   ==============
   BasePortalVis
   ==============
 */
void BasePortalVis( int portalnum ){
	int j, k;
	vportal_t   *tp, *p;
	fixedWinding_t  *w;


	p = portals + portalnum;

	if ( p->removed ) {
		return;
	}

	p->portalfront = safe_calloc( portalbytes );
	p->portalflood = safe_calloc( portalbytes );
	p->portalvis = safe_calloc( portalbytes );

	for ( j = 0, tp = portals; j < numportals * 2; ++j, ++tp )
	{
		if ( j == portalnum ) {
			continue;
		}
		if ( tp->removed ) {
			continue;
		}

		/* ydnar: this is old farplane vis code from mre */
		/*
		   if ( farplanedist >= 0 )
		   {
		    vec3_t dir;
		    VectorSubtract( p->origin, tp->origin, dir );
		    if ( VectorLength( dir ) > farplanedist - p->radius - tp->radius )
		        continue;
		   }
		 */

		if ( !p->sky && !tp->sky && farPlaneDist != 0.0f ) {
			if( farPlaneDistMode == 'o' ){
				if( vector3_length( p->origin - tp->origin ) > farPlaneDist )
					continue;
			}
			else if( farPlaneDistMode == 'e' ){
				if( vector3_length( p->origin - tp->origin ) + p->radius + tp->radius > 2.0f * farPlaneDist )
					continue;
			}
			else if( farPlaneDistMode == 'r' ){
				if( p->radius + tp->radius > farPlaneDist )
					continue;
			}
			else{ /* ydnar: this is known-to-be-working farplane code */
				if ( vector3_length( p->origin - tp->origin ) - p->radius - tp->radius > farPlaneDist )
					continue;
			}

		}


		w = tp->winding;
		for ( k = 0; k < w->numpoints; ++k )
		{
			if ( plane3_distance_to_point( p->plane, w->points[k] ) > ON_EPSILON ) {
				break;
			}
		}
		if ( k == w->numpoints ) {
			continue;   // no points on front

		}
		w = p->winding;
		for ( k = 0; k < w->numpoints; ++k )
		{
			if ( plane3_distance_to_point( tp->plane, w->points[k] ) < -ON_EPSILON ) {
				break;
			}
		}
		if ( k == w->numpoints ) {
			continue;   // no points on front

		}
		bit_enable( p->portalfront, j );
	}

	SimpleFlood( p, p->leaf );

	p->nummightsee = CountBits( p->portalflood, numportals * 2 );
//	Sys_Printf( "portal %i: %i mightsee\n", portalnum, p->nummightsee );
}





/*
   ===============================================================================

   This is a second order aproximation

   Calculates portalvis bit vector

   WAAAAAAY too slow.

   ===============================================================================
 */

/*
   ==================
   RecursiveLeafBitFlow

   ==================
 */
static void RecursiveLeafBitFlow( int leafnum, byte *mightsee, byte *cansee ){
	byte newmight[MAX_PORTALS / 8];


	// check all portals for flowing into other leafs
	for ( const vportal_t *p : Span( leafs[leafnum].portals, leafs[leafnum].numportals ) )
	{
		if ( p->removed ) {
			continue;
		}
		const int pnum = p - portals;

		// if some previous portal can't see it, skip
		if ( !bit_is_enabled( mightsee, pnum ) ) {
			continue;
		}

		// if this portal can see some portals we mightsee, recurse
		long more = 0;
		for ( int i = 0; i < portallongs; ++i )
		{
			( (long *)newmight )[i] = ( (long *)mightsee )[i]
			                          & ( (long *)p->portalflood )[i];
			more |= ( (long *)newmight )[i] & ~( (long *)cansee )[i];
		}

		if ( !more ) {
			continue;   // can't see anything new

		}
		bit_enable( cansee, pnum );

		RecursiveLeafBitFlow( p->leaf, newmight, cansee );
	}
}

/*
   ==============
   BetterPortalVis
   ==============
 */
void BetterPortalVis( int portalnum ){
	vportal_t   *p;

	p = portals + portalnum;

	if ( p->removed ) {
		return;
	}

	RecursiveLeafBitFlow( p->leaf, p->portalflood, p->portalvis );

	// build leaf vis information
	p->nummightsee = CountBits( p->portalvis, numportals * 2 );
}

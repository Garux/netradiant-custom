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

   functions

   ------------------------------------------------------------------------------- */

/*
   AllocSideRef() - ydnar
   allocates and assigns a brush side reference
 */

sideRef_t *AllocSideRef( const side_t *side, sideRef_t *next ){
	/* dummy check */
	if ( side == NULL ) {
		return next;
	}

	/* allocate and return */
	sideRef_t *sideRef = safe_malloc( sizeof( *sideRef ) );
	sideRef->side = side;
	sideRef->next = next;
	return sideRef;
}





/*
   BoundBrush()
   sets the mins/maxs based on the windings
   returns false if the brush doesn't enclose a valid volume
 */

static bool BoundBrush( brush_t& brush ){
	brush.minmax.clear();
	for ( const side_t& side : brush.sides )
	{
		WindingExtendBounds( side.winding, brush.minmax );
	}

	return brush.minmax.valid() && c_worldMinmax.surrounds( brush.minmax );
}




/*
   SnapWeldVector() - ydnar
   welds two Vector3's into a third, taking into account nearest-to-integer
   instead of averaging
 */

#define SNAP_EPSILON    0.01

Vector3 SnapWeldVector( const Vector3& a, const Vector3& b ){
	Vector3 out;

	/* do each element */
	for ( int i = 0; i < 3; i++ )
	{
		/* round to integer */
		const float ai = std::rint( a[ i ] );
		const float bi = std::rint( b[ i ] );

		/* prefer exact integer */
		if ( ai == a[ i ] ) {
			out[ i ] = a[ i ];
		}
		else if ( bi == b[ i ] ) {
			out[ i ] = b[ i ];
		}

		/* use nearest */
		else if ( fabs( ai - a[ i ] ) < fabs( bi - b[ i ] ) ) {
			out[ i ] = a[ i ];
		}
		else{
			out[ i ] = b[ i ];
		}

		/* snap */
		const float outi = std::rint( out[ i ] );
		if ( fabs( outi - out[ i ] ) <= SNAP_EPSILON ) {
			out[ i ] = outi;
		}
	}

	return out;
}

/*
   ==================
   SnapWeldVectorAccu

   Welds two vectors into a third, taking into account nearest-to-integer
   instead of averaging.
   ==================
 */
static DoubleVector3 SnapWeldVectorAccu( const DoubleVector3& a, const DoubleVector3& b ){
	// I'm just preserving what I think was the intended logic of the original
	// SnapWeldVector().  I'm not actually sure where this function should even
	// be used.  I'd like to know which kinds of problems this function addresses.

	// TODO: I thought we're snapping all coordinates to nearest 1/8 unit?
	// So what is natural about snapping to the nearest integer?  Maybe we should
	// be snapping to the nearest 1/8 unit instead?

	DoubleVector3 out;

	for ( int i = 0; i < 3; i++ )
	{
		const double ai = std::rint( a[i] );
		const double bi = std::rint( b[i] );
		const double ad = fabs( ai - a[i] );
		const double bd = fabs( bi - b[i] );

		if ( ad < bd ) {
			if ( ad < SNAP_EPSILON ) {
				out[i] = ai;
			}
			else{
				out[i] = a[i];
			}
		}
		else
		{
			if ( bd < SNAP_EPSILON ) {
				out[i] = bi;
			}
			else{
				out[i] = b[i];
			}
		}
	}

	return out;
}

/*
   FixWinding() - ydnar
   removes degenerate edges from a winding
   returns true if the winding is valid
 */

#define DEGENERATE_EPSILON  0.1

static bool FixWinding( winding_t& w ){
	bool valid = true;

	/* dummy check */
	if ( w.empty() ) {
		return false;
	}

	/* check all verts */
	for ( winding_t::iterator i = w.begin(); i != w.end(); )
	{
		winding_t::iterator j = winding_next( w, i );
		/* don't remove points if winding is a triangle */
		if ( w.size() == 3 ) {
			return valid;
		}

		/* degenerate edge? */
		if ( vector3_length( *i - *j ) < DEGENERATE_EPSILON ) {
			valid = false;
			//Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: Degenerate winding edge found, fixing...\n" );

			/* create an average point (ydnar 2002-01-26: using nearest-integer weld preference) */
			*j = SnapWeldVector( *i, *j );
			//VectorAdd( w[ i ], w[ j ], vec );
			//VectorScale( vec, 0.5, w[ i ] );

			/* move the remaining verts */
			i = w.erase( i );
		}
		else{
			++i;
		}
	}

	/* one last check and return */
	if ( w.size() < 3 ) {
		valid = false;
	}
	return valid;
}

/*
   ==================
   FixWindingAccu

   Removes degenerate edges (edges that are too short) from a winding.
   Returns true if the winding has been altered by this function.
   Returns false if the winding is untouched by this function.

   It's advised that you check the winding after this function exits to make
   sure it still has at least 3 points.  If that is not the case, the winding
   cannot be considered valid.  The winding may degenerate to one or two points
   if the some of the winding's points are close together.
   ==================
 */
static bool FixWindingAccu( winding_accu_t& w ){
	bool altered = false;

	while ( true )
	{
		if ( w.size() < 2 ) {
			break;                   // Don't remove the only remaining point.
		}
		bool done = true;
		for ( winding_accu_t::iterator i = w.end() - 1, j = w.begin(); j != w.end(); i = j, ++j )
		{
			if ( vector3_length( *i - *j ) < DEGENERATE_EPSILON ) {
				// TODO: I think the "snap weld vector" was written before
				// some of the math precision fixes, and its purpose was
				// probably to address math accuracy issues.  We can think
				// about changing the logic here.  Maybe once plane distance
				// gets 64 bits, we can look at it then.
				*i = SnapWeldVectorAccu( *i, *j );
				w.erase( j );
				altered = true;
				// The only way to finish off fixing the winding consistently and
				// accurately is by fixing the winding all over again.  For example,
				// the point at index i and the point at index i-1 could now be
				// less than the epsilon distance apart.  There are too many special
				// case problems we'd need to handle if we didn't start from the
				// beginning.
				done = false;
				break; // This will cause us to return to the "while" loop.
			}
		}
		if ( done ) {
			break;
		}
	}

	return altered;
}


/*
   CreateBrushWindings()
   makes basewindigs for sides and mins/maxs for the brush
   returns false if the brush doesn't enclose a valid volume
 */

bool CreateBrushWindings( brush_t& brush ){
	/* walk the list of brush sides */
	for ( size_t i = 0; i < brush.sides.size(); ++i )
	{
		/* get side and plane */
		side_t& side = brush.sides[ i ];
		const plane_t& plane = mapplanes[ side.planenum ];

		/* make huge winding */
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
		winding_accu_t w = BaseWindingForPlaneAccu( ( side.plane.normal() != g_vector3_identity )? side.plane : Plane3( plane.plane ) );
#else
		winding_t w = BaseWindingForPlane( plane.plane );
#endif

		/* walk the list of brush sides */
		for ( size_t j = 0; j < brush.sides.size() && !w.empty(); ++j )
		{
			const side_t& cside = brush.sides[ j ];
			const plane_t& cplane = mapplanes[ cside.planenum ^ 1 ];
			if ( i == j
			|| cside.planenum == ( side.planenum ^ 1 ) /* back side clipaway */
			|| cside.bevel ) {
				continue;
			}
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
			ChopWindingInPlaceAccu( w, ( cside.plane.normal() != g_vector3_identity )? plane3_flipped( cside.plane ) : Plane3( cplane.plane ), 0 );
#else
			ChopWindingInPlace( w, cplane.plane, 0 ); // CLIP_EPSILON );
#endif

			/* ydnar: fix broken windings that would generate trifans */
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
			// I think it's better to FixWindingAccu() once after we chop with all planes
			// so that error isn't multiplied.  There is nothing natural about welding
			// the points unless they are the final endpoints.  ChopWindingInPlaceAccu()
			// is able to handle all kinds of degenerate windings.
#else
			FixWinding( w );
#endif
		}

		/* set side winding */
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
		FixWindingAccu( w );
		if( w.size() >= 3 )
			side.winding = CopyWindingAccuToRegular( w );
		else
			side.winding.clear();
#else
		side.winding.swap( w );
#endif
	}

	/* find brush bounds */
	return BoundBrush( brush );
}




/*
   ==================
   BrushFromBounds

   Creates a new axial brush
   ==================
 */
static brush_t BrushFromBounds( const Vector3& mins, const Vector3& maxs ){
	brush_t b;
	b.sides.resize( 6 );
	for ( int i = 0; i < 3; ++i )
	{
		float dist = maxs[i];
		b.sides[i].planenum = FindFloatPlane( g_vector3_axes[i], dist, 1, &maxs );

		dist = -mins[i];
		b.sides[3 + i].planenum = FindFloatPlane( -g_vector3_axes[i], dist, 1, &mins );
	}

	CreateBrushWindings( b );

	return b;
}

/*
   ==================
   BrushVolume

   ==================
 */
static float BrushVolume( const brush_t& brush ){
	float volume = 0;

	for ( auto i = brush.sides.cbegin(); i != brush.sides.cend(); ++i ){
		// grab the first valid point as the corner
		if( !i->winding.empty() ){
			const Vector3 corner = i->winding[0];
			// make tetrahedrons to all other faces
			for ( ++i; i != brush.sides.cend(); ++i )
			{
				if ( !i->winding.empty() ) {
					volume += -plane3_distance_to_point( mapplanes[i->planenum].plane, corner ) * WindingArea( i->winding );
				}
			}
			break;
		}
	}

	return volume / 3;
}



/*
   WriteBSPBrushMap()
   writes a map with the split bsp brushes
 */

void WriteBSPBrushMap( const char *name, const brushlist_t& list ){
	/* note it */
	Sys_Printf( "Writing %s\n", name );

	/* open the map file */
	FILE *f = SafeOpenWrite( name );

	fprintf( f, "{\n\"classname\" \"worldspawn\"\n" );

	for ( const brush_t& brush : list )
	{
		fprintf( f, "{\n" );
		for ( const side_t& side : brush.sides )
		{
			// TODO: See if we can use a smaller winding to prevent resolution loss.
			// Is WriteBSPBrushMap() used only to decompile maps?
			const winding_t w = BaseWindingForPlane( mapplanes[side.planenum].plane );

			fprintf( f, "( %i %i %i ) ", (int)w[0][0], (int)w[0][1], (int)w[0][2] );
			fprintf( f, "( %i %i %i ) ", (int)w[1][0], (int)w[1][1], (int)w[1][2] );
			fprintf( f, "( %i %i %i ) ", (int)w[2][0], (int)w[2][1], (int)w[2][2] );

			fprintf( f, "notexture 0 0 0 1 1\n" );
		}
		fprintf( f, "}\n" );
	}
	fprintf( f, "}\n" );

	fclose( f );

}



/*
   FilterBrushIntoTree_r()
   adds brush reference to any intersecting bsp leafnode
 */
static std::pair<brush_t, brush_t> SplitBrush( const brush_t& brush, int planenum );

static int FilterBrushIntoTree_r( brush_t&& b, node_t *node ){
	/* dummy check */
	if ( b.sides.empty() ) {
		return 0;
	}

	/* add it to the leaf list */
	if ( node->planenum == PLANENUM_LEAF ) {
		/* something somewhere is hammering brushlist */
		node->brushlist.push_front( std::move( b ) );

		/* classify the leaf by the structural brush */
		if ( !b.detail ) {
			if ( b.opaque ) {
				node->opaque = true;
				node->areaportal = false;
			}
			else if ( b.compileFlags & C_AREAPORTAL ) {
				if ( !node->opaque ) {
					node->areaportal = true;
				}
			}
		}

		return 1;
	}

	/* split it by the node plane */
	auto [front, back] = SplitBrush( b, node->planenum );

	int c = 0;
	c += FilterBrushIntoTree_r( std::move( front ), node->children[ 0 ] );
	c += FilterBrushIntoTree_r( std::move( back ), node->children[ 1 ] );

	return c;
}



/*
   FilterDetailBrushesIntoTree
   fragment all the detail brushes into the structural leafs
 */

void FilterDetailBrushesIntoTree( const entity_t& e, tree_t& tree ){
	int c_unique = 0, c_clusters = 0;

	/* note it */
	Sys_FPrintf( SYS_VRB,  "--- FilterDetailBrushesIntoTree ---\n" );

	/* walk the list of brushes */
	for ( const brush_t& b : e.brushes )
	{
		if ( b.detail ) {
			c_unique++;
			c_clusters += FilterBrushIntoTree_r( brush_t( b ), tree.headnode );
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d detail brushes\n", c_unique );
	Sys_FPrintf( SYS_VRB, "%9d cluster references\n", c_clusters );
}

/*
   =====================
   FilterStructuralBrushesIntoTree

   Mark the leafs as opaque and areaportals
   =====================
 */
void FilterStructuralBrushesIntoTree( const entity_t& e, tree_t& tree ) {
	int c_unique = 0, c_clusters = 0;

	Sys_FPrintf( SYS_VRB, "--- FilterStructuralBrushesIntoTree ---\n" );

	for ( const brush_t& b : e.brushes ) {
		if ( !b.detail ) {
			c_unique++;
			c_clusters += FilterBrushIntoTree_r( brush_t( b ), tree.headnode );
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d structural brushes\n", c_unique );
	Sys_FPrintf( SYS_VRB, "%9d cluster references\n", c_clusters );
}


/*
   ================
   WindingIsTiny

   Returns true if the winding would be crunched out of
   existence by the vertex snapping.
   ================
 */
#define EDGE_LENGTH 0.2
bool WindingIsTiny( const winding_t& w ){
/*
	return WindingArea( w ) < 1;
*/
	int edges = 0;

	for ( size_t i = w.size() - 1, j = 0; j < w.size(); i = j, ++j )
	{
		if ( vector3_length( w[j] - w[i] ) > EDGE_LENGTH ) {
			if ( ++edges == 3 ) {
				return false;
			}
		}
	}
	return true;
}

/*
   ================
   WindingIsHuge

   Returns true if the winding still has one of the points
   from basewinding for plane
   ================
 */
static bool WindingIsHuge( const winding_t& w ){
	for ( const Vector3& p : w )
		if ( !c_worldMinmax.test( p ) )
			return true;
	return false;
}

//============================================================

/*
   ==================
   BrushMostlyOnSide

   ==================
 */
static EPlaneSide BrushMostlyOnSide( const brush_t& brush, const Plane3f& plane ){
	float max = 0;
	EPlaneSide side = eSideFront;
	for ( const side_t& s : brush.sides )
	{
		for ( const Vector3& p : s.winding )
		{
			const double d = plane3_distance_to_point( plane, p );
			if ( d > max ) {
				max = d;
				side = eSideFront;
			}
			if ( -d > max ) {
				max = -d;
				side = eSideBack;
			}
		}
	}
	return side;
}



/*
   SplitBrush()
   generates two new brushes, leaving the original unchanged
 */

static std::pair<brush_t, brush_t> SplitBrush( const brush_t& brush, int planenum ){
	const Plane3f& plane = mapplanes[planenum].plane;

	// check all points
	float d_front = 0, d_back = 0;
	for ( const side_t& side : brush.sides )
	{
		for ( const Vector3& p : side.winding )
		{
			const float d = plane3_distance_to_point( plane, p );
			if ( d > 0 ) {
				value_maximize( d_front, d );
			}
			if ( d < 0 ) {
				value_minimize( d_back, d );
			}
		}
	}

	if ( d_front < 0.1 ) { // PLANESIDE_EPSILON)
		// only on back
		return { {}, brush };
	}

	if ( d_back > -0.1 ) { // PLANESIDE_EPSILON)
		// only on front
		return { brush, {} };
	}

	// create a new winding from the split plane
	winding_t midwinding = BaseWindingForPlane( plane );
	for ( const side_t& side : brush.sides )
	{
		ChopWindingInPlace( midwinding, mapplanes[side.planenum ^ 1].plane, 0 ); // PLANESIDE_EPSILON);
		if( midwinding.empty() )
			break;
	}

	if ( midwinding.empty() || WindingIsTiny( midwinding ) ) { // the brush isn't really split
		if ( BrushMostlyOnSide( brush, plane ) == eSideBack ) {
			return { {}, brush };
		}
		else { // side == eSideFront
			return { brush, {} };
		}
	}

	if ( WindingIsHuge( midwinding ) ) {
		Sys_FPrintf( SYS_WRN | SYS_VRBflag, "WARNING: huge winding\n" );
	}

	// split it for real
	brush_t     b[2]{ brush, brush };
	for ( int i = 0; i < 2; i++ )
	{
		b[i].sides.clear();
	}

	// split all the current windings

	for ( const side_t& side : brush.sides )
	{
		if ( !side.winding.empty() ) {
			winding_t cw[2];
			std::tie( cw[0], cw[1] ) =
			ClipWindingEpsilonStrict( side.winding, plane, 0 /*PLANESIDE_EPSILON*/ ); /* strict, in parallel case we get the face back because it also is the midwinding */
			for ( int i = 0; i < 2; i++ )
			{
				if ( !cw[i].empty() ) {
					side_t& cs = b[i].sides.emplace_back( side );
					cs.winding.swap( cw[i] );
				}
			}
		}
	}


	// see if we have valid polygons on both sides
	for ( int i = 0; i < 2; i++ )
	{
		if ( b[i].sides.size() < 3 || !BoundBrush( b[i] ) ) {
			if ( b[i].sides.size() >= 3 ) {
				Sys_FPrintf( SYS_WRN | SYS_VRBflag, "bogus brush after clip\n" );
			}
			b[i].sides.clear();
		}
	}

	if ( b[0].sides.empty() || b[1].sides.empty() ) {
		if ( b[0].sides.empty() && b[1].sides.empty() ) {
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "split removed brush\n" );
		}
		else{
			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "split not on both sides\n" );
		}
		if ( !b[0].sides.empty() ) {
			return { brush, {} };
		}
		else if ( !b[1].sides.empty() ) {
			return { {}, brush };
		}
		return{};
	}

	// add the midwinding to both sides
	for ( int i = 0; i < 2; i++ )
	{
		side_t& cs = b[i].sides.emplace_back();

		cs.planenum = planenum ^ i ^ 1;
		cs.shaderInfo = NULL;
		if ( i == 0 ) {
			cs.winding = midwinding; // copy
		}
		else{
			cs.winding.swap( midwinding ); // move
		}
	}

	for ( int i = 0; i < 2; i++ )
	{
		if ( BrushVolume( b[i] ) < 1.0 ) {
			b[i].sides.clear();
			//			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "tiny volume after clip\n" );
		}
	}

	return{ std::move( b[0] ), std::move( b[1] ) };
}

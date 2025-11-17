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
	for ( int i = 0; i < 3; ++i )
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
		else if ( std::fabs( ai - a[ i ] ) < std::fabs( bi - b[ i ] ) ) {
			out[ i ] = a[ i ];
		}
		else{
			out[ i ] = b[ i ];
		}

		/* snap */
		const float outi = std::rint( out[ i ] );
		if ( std::fabs( outi - out[ i ] ) <= SNAP_EPSILON ) {
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

	for ( int i = 0; i < 3; ++i )
	{
		const double ai = std::rint( a[i] );
		const double bi = std::rint( b[i] );
		const double ad = std::fabs( ai - a[i] );
		const double bd = std::fabs( bi - b[i] );

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

	while ( w.size() > 1 )   // Don't remove the only remaining point.
	{
		bool done = true;
		for ( winding_accu_t::iterator i = w.end() - 1, j = w.begin(); j != w.end(); i = j++ )
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

// Solve: n1·x = d1, n2·x = d2, n3·x = d3
// Returns true if unique solution
static bool solve3Planes( const Plane3 &p1, const Plane3 &p2, const Plane3 &p3, DoubleVector3 &out ){
	// Build matrix: rows = normals
	const double m[3][3] = { { p1.normal().x(), p1.normal().y(), p1.normal().z() },
	                         { p2.normal().x(), p2.normal().y(), p2.normal().z() },
	                         { p3.normal().x(), p3.normal().y(), p3.normal().z() } };
	const double b[3] = { p1.dist(), p2.dist(), p3.dist() };

	// Cramer's rule
	const double det = m[0][0] * ( m[1][1] * m[2][2] - m[2][1] * m[1][2] ) -
	                   m[0][1] * ( m[1][0] * m[2][2] - m[2][0] * m[1][2] ) +
	                   m[0][2] * ( m[1][0] * m[2][1] - m[1][1] * m[2][0] );

	if( std::fabs( det ) < 1e-9 )
		return false; // parallel or degenerate

	// x = det(mx) / det
	const double mx[3][3] = { { b[0], m[0][1], m[0][2] },
	                          { b[1], m[1][1], m[1][2] },
	                          { b[2], m[2][1], m[2][2] } };
	const double det_x = mx[0][0] * ( mx[1][1] * mx[2][2] - mx[2][1] * mx[1][2] ) -
	                     mx[0][1] * ( mx[1][0] * mx[2][2] - mx[2][0] * mx[1][2] ) +
	                     mx[0][2] * ( mx[1][0] * mx[2][1] - mx[1][1] * mx[2][0] );
	// y
	const double my[3][3] = { { m[0][0], b[0], m[0][2] },
	                          { m[1][0], b[1], m[1][2] },
	                          { m[2][0], b[2], m[2][2] } };
	const double det_y = my[0][0] * ( my[1][1] * my[2][2] - my[2][1] * my[1][2] ) -
	                     my[0][1] * ( my[1][0] * my[2][2] - my[2][0] * my[1][2] ) +
	                     my[0][2] * ( my[1][0] * my[2][1] - my[1][1] * my[2][0] );
	// z
	const double mz[3][3] = { { m[0][0], m[0][1], b[0] },
	                          { m[1][0], m[1][1], b[1] },
	                          { m[2][0], m[2][1], b[2] } };
	const double det_z = mz[0][0] * ( mz[1][1] * mz[2][2] - mz[2][1] * mz[1][2] ) -
	                     mz[0][1] * ( mz[1][0] * mz[2][2] - mz[2][0] * mz[1][2] ) +
	                     mz[0][2] * ( mz[1][0] * mz[2][1] - mz[1][1] * mz[2][0] );
	out.x() = det_x / det;
	out.y() = det_y / det;
	out.z() = det_z / det;

	return true;
}

static DoubleMinMax brushMinMaxFromPlanes( const std::vector<Plane3>& planes ){
	DoubleMinMax minmax;

	for ( auto i = planes.cbegin(), end = planes.cend(); i != end; ++i )
		for ( auto j = i + 1; j != end; ++j )
			for ( auto k = j + 1; k != end; ++k )
				if( DoubleVector3 v; solve3Planes( *i, *j, *k, v ) )
					// Validate against ALL planes
					if( std::ranges::all_of( planes, [&]( const Plane3& plane ){ return plane3_distance_to_point( plane, v ) < ON_EPSILON; } ) )
						minmax.extend( v );

	return minmax;
}

#define Q3MAP2_EXPERIMENTAL_OFFSET_WINDING_CREATION 1
/*
   CreateBrushWindings()
   makes basewindigs for sides and mins/maxs for the brush
   returns false if the brush doesn't enclose a valid volume
 */

bool CreateBrushWindings( brush_t& brush ){
	std::vector<Plane3> planes;
	planes.reserve( brush.sides.size() );

	for( const side_t& side : brush.sides ){
		ENSURE( !side.bevel );
		planes.push_back( ( side.plane.normal() != g_vector3_identity )? side.plane : Plane3( mapplanes[ side.planenum ].plane ) );
	}
#if Q3MAP2_EXPERIMENTAL_OFFSET_WINDING_CREATION
	DoubleMinMax minmax = brushMinMaxFromPlanes( planes );

	if( !minmax.valid() )
		return false;

	const DoubleVector3 offset = minmax.origin();
	minmax.maxs -= offset;
	minmax.mins -= offset;
	for( Plane3& p : planes )
		p = plane3_translated( plane3_flipped( p ), -offset ); // flip for clipping
#else
	for( Plane3& p : planes )
		p = plane3_flipped( p );
#endif
	/* walk the list of brush sides */
	for ( size_t i = 0; i < brush.sides.size(); ++i )
	{
		/* get side and plane */
		side_t& side = brush.sides[ i ];

		/* make huge winding */
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
	#if Q3MAP2_EXPERIMENTAL_OFFSET_WINDING_CREATION
		winding_accu_t w = BaseWindingForPlaneAccu( plane3_flipped( planes[ i ] ), minmax );
	#else
		winding_accu_t w = BaseWindingForPlaneAccu( plane3_flipped( planes[ i ] ) );
	#endif
#else
		const plane_t& plane = mapplanes[ side.planenum ];
		winding_t w = BaseWindingForPlane( plane.plane );
#endif

		/* walk the list of brush sides */
		for ( size_t j = 0; j < brush.sides.size() && !w.empty(); ++j )
		{
			const side_t& cside = brush.sides[ j ];
			if ( i == j
			|| cside.planenum == ( side.planenum ^ 1 ) ) { /* back side clipaway */
				continue;
			}
#if Q3MAP2_EXPERIMENTAL_HIGH_PRECISION_MATH_FIXES
			ChopWindingInPlaceAccu( w, planes[ j ], 0 );
#else
			const plane_t& cplane = mapplanes[ cside.planenum ^ 1 ];
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
		if( w.size() >= 3 ){
	#if Q3MAP2_EXPERIMENTAL_OFFSET_WINDING_CREATION
			for( DoubleVector3& v : w )
				v += offset;
	#endif
			side.winding = CopyWindingAccuToRegular( w );
		}
		else{
			side.winding.clear();
		}
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
		b.sides[i    ].planenum = FindFloatPlane( Plane3f(  g_vector3_axes[i],  maxs[i] ), Span( &maxs, 1 ) );
		b.sides[i + 3].planenum = FindFloatPlane( Plane3f( -g_vector3_axes[i], -mins[i] ), Span( &mins, 1 ) );
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
		node->brushlist.push_front( std::move( b ) );

		/* classify the leaf by the structural brush */
		if ( !b.detail ) {
			if ( b.opaque ) {
				node->opaque = true;
			}
			else if ( b.compileFlags & C_AREAPORTAL ) { // find and flag C_AREAPORTAL portals, this is not always passed through node->compileFlags
				const auto side = std::ranges::find_if( b.original->sides, []( const side_t& side ){ return side.compileFlags & C_AREAPORTAL; } );

				for ( portal_t *p = node->portals; p; p = p->nextPortal( node ) )
				{
					if( p->onnode != nullptr && ( p->onnode->planenum | 1 ) == ( side->planenum | 1 ) )
						if( windings_intersect_coplanar( ( p->onnode->planenum == side->planenum )
						                                 ? p->winding : ReverseWinding( p->winding ), side->winding, side->plane ) )
							p->compileFlags |= C_AREAPORTAL;
				}
			}
		}

		return 1;
	}

	/* split it by the node plane */
	auto [front, back] = SplitBrush( b, node->planenum );

	int c = 0;
	c += FilterBrushIntoTree_r( std::move( front ), node->children[eFront] );
	c += FilterBrushIntoTree_r( std::move( back ), node->children[eBack] );

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

	for ( auto prev = w.cend() - 1, next = w.cbegin(); next != w.cend(); prev = next++ )
		if ( vector3_length( *next - *prev ) > EDGE_LENGTH && ++edges == 3 )
			return false;
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

	if ( d_front < 0.1f ) { // PLANESIDE_EPSILON)
		// only on back
		return { {}, brush };
	}

	if ( d_back > -0.1f ) { // PLANESIDE_EPSILON)
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
	for ( int i = 0; i < 2; ++i )
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
			for ( int i = 0; i < 2; ++i )
			{
				if ( !cw[i].empty() ) {
					side_t& cs = b[i].sides.emplace_back( side );
					cs.winding.swap( cw[i] );
				}
			}
		}
	}


	// see if we have valid polygons on both sides
	for ( int i = 0; i < 2; ++i )
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
	for ( int i = 0; i < 2; ++i )
	{
		side_t& cs = b[i].sides.emplace_back();

		cs.planenum = planenum ^ i ^ 1;
		cs.shaderInfo = nullptr;
		if ( i == 0 ) {
			cs.winding = midwinding; // copy
		}
		else{
			cs.winding.swap( midwinding ); // move
		}
	}

	for ( int i = 0; i < 2; ++i )
	{
		if ( BrushVolume( b[i] ) < 1 ) {
			b[i].sides.clear();
			//			Sys_FPrintf( SYS_WRN | SYS_VRBflag, "tiny volume after clip\n" );
		}
	}

	return{ std::move( b[0] ), std::move( b[1] ) };
}

/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#if !defined( INCLUDED_DRAGPLANES_H )
#define INCLUDED_DRAGPLANES_H

#include "selectable.h"
#include "selectionlib.h"
#include "math/aabb.h"
#include "math/line.h"

// local must be a pure rotation
inline Vector3 translation_to_local( const Vector3& translation, const Matrix4& local ){
	return matrix4_get_translation_vec3(
			   matrix4_multiplied_by_matrix4(
				   matrix4_translated_by_vec3( matrix4_transposed( local ), translation ),
				   local
				   )
			   );
}

// local must be a pure rotation
inline Vector3 translation_from_local( const Vector3& translation, const Matrix4& local ){
	return matrix4_get_translation_vec3(
			   matrix4_multiplied_by_matrix4(
				   matrix4_translated_by_vec3( local, translation ),
				   matrix4_transposed( local )
				   )
			   );
}
#if 0
namespace{
// https://www.gamedev.net/forums/topic/230443-initializing-array-member-objects-in-c/?page=2
class ObservedSelectables6x {
	ObservedSelectable m_selectable0, m_selectable1, m_selectable2, m_selectable3, m_selectable4, m_selectable5;

	// array of 6 pointers to ObservedSelectable members of ObservedSelectables6x
	static ObservedSelectable ObservedSelectables6x::* const array[6];

public:
	ObservedSelectables6x( const SelectionChangeCallback& onchanged ) :
		m_selectable0( onchanged ),
		m_selectable1( onchanged ),
		m_selectable2( onchanged ),
		m_selectable3( onchanged ),
		m_selectable4( onchanged ),
		m_selectable5( onchanged ) {
	}
	ObservedSelectable& operator[]( std::size_t idx ) {
		return this->*array[idx];
	}
	const ObservedSelectable& operator[]( std::size_t idx ) const {
		return this->*array[idx];
	}
};

// Parse this
ObservedSelectable ObservedSelectables6x::* const ObservedSelectables6x::array[6] = { &ObservedSelectables6x::m_selectable0,
																						&ObservedSelectables6x::m_selectable1,
																						&ObservedSelectables6x::m_selectable2,
																						&ObservedSelectables6x::m_selectable3,
																						&ObservedSelectables6x::m_selectable4,
																						&ObservedSelectables6x::m_selectable5 };
} //namespace
#endif

class DragPlanes
{
ObservedSelectable m_selectables[6];
public:
AABB m_bounds;
DragPlanes( const SelectionChangeCallback& onchanged ) : m_selectables{ ObservedSelectable( onchanged ),
																		ObservedSelectable( onchanged ),
																		ObservedSelectable( onchanged ),
																		ObservedSelectable( onchanged ),
																		ObservedSelectable( onchanged ),
																		ObservedSelectable( onchanged ) }{
}
bool isSelected() const {
	for ( std::size_t i = 0; i < 6; ++i )
		if( m_selectables[i].isSelected() )
			return true;
	return false;
}
void setSelected( bool selected ){
	for ( std::size_t i = 0; i < 6; ++i )
		m_selectables[i].setSelected( selected );
}
void selectPlanes( const AABB& aabb, Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback, const Matrix4& rotation = g_matrix4_identity ){
	Vector3 corners[8];
	aabb_corners_oriented( aabb, rotation, corners );

	Plane3 planes[6];
	aabb_planes_oriented( aabb, rotation, planes );

	const std::size_t indices[24] = {
		2, 1, 5, 6, //+x //right
		3, 7, 4, 0, //-x //left
		1, 0, 4, 5, //+y //front
		3, 2, 6, 7, //-y //back
		0, 1, 2, 3, //+z //top
		7, 6, 5, 4, //-z //bottom
	};

	const Vector3 viewdir( test.getVolume().getViewDir() );
	double bestDot = 1;
	ObservedSelectable* selectable = 0;
	ObservedSelectable* selectable2 = 0;

	for ( std::size_t i = 0; i < 6; ++i ){
		const std::size_t index = i * 4;
		const Vector3 centroid = vector3_mid( corners[indices[index]], corners[indices[index + 2]] );
		const Vector3 projected = vector4_projected( matrix4_transformed_vector4( test.getVolume().GetViewMatrix(), Vector4( centroid, 1 ) ) );
		const Vector3 closest_point = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, projected[2], 1 ) ) );
		if ( vector3_dot( planes[i].normal(), closest_point - corners[indices[index]] ) > 0
			&& vector3_dot( planes[i].normal(), closest_point - corners[indices[index + 1]] ) > 0
			&& vector3_dot( planes[i].normal(), closest_point - corners[indices[index + 2]] ) > 0
			&& vector3_dot( planes[i].normal(), closest_point - corners[indices[index + 3]] ) > 0 ) {
			const double dot = fabs( vector3_dot( planes[i].normal(), viewdir ) );
			const double diff = bestDot - dot;
			if( diff > 0.03 ){
				bestDot = dot;
				selectable = &m_selectables[i];
				selectable2 = 0;
			}
			else if( fabs( diff ) <= 0.03 ){
				selectable2 = &m_selectables[i];
			}
		}
	}
	if( test.getVolume().fill() ) // select only plane in camera
		selectable2 = 0;
	for ( std::size_t i = 0; i < 6; ++i )
		if( &m_selectables[i] == selectable || &m_selectables[i] == selectable2 ){
			Selector_add( selector, m_selectables[i] );
			selectedPlaneCallback( planes[i] );
		}
	m_bounds = aabb;
}
void selectReversedPlanes( const AABB& aabb, Selector& selector, const SelectedPlanes& selectedPlanes, const Matrix4& rotation = g_matrix4_identity ){
	Plane3 planes[6];
	aabb_planes_oriented( aabb, rotation, planes );
	for ( std::size_t i = 0; i < 6; ++i )
		if ( selectedPlanes.contains( plane3_flipped( planes[i] ) ) )
			Selector_add( selector, m_selectables[i] );
}

void bestPlaneDirect( const AABB& aabb, SelectionTest& test, Plane3& plane, SelectionIntersection& intersection, const Matrix4& rotation = g_matrix4_identity ){
	AABB aabb_ = aabb;
	for( std::size_t i = 0; i < 3; ++i ) /* make sides of flat patches more selectable */
		if( aabb_.extents[i] < 1 )
			aabb_.extents[i] = 4;

	Vector3 corners[8];
	aabb_corners_oriented( aabb_, rotation, corners );

	Plane3 planes[6];
	aabb_planes_oriented( aabb_, rotation, planes );

	const IndexPointer::index_type indices[24] = {
		2, 1, 5, 6, //+x //right
		3, 7, 4, 0, //-x //left
		1, 0, 4, 5, //+y //front
		3, 2, 6, 7, //-y //back
		0, 1, 2, 3, //+z //top
		7, 6, 5, 4, //-z //bottom
	};

	for ( std::size_t i = 0; i < 6; ++i ){
		const std::size_t index = i * 4;
		SelectionIntersection intersection_new;
		test.TestQuads( VertexPointer( reinterpret_cast<VertexPointer::pointer>( corners ), sizeof( Vector3 ) ), IndexPointer( &indices[index], 4 ), intersection_new );
		if( SelectionIntersection_closer( intersection_new, intersection ) ){
			intersection = intersection_new;
			plane = planes[i];
		}
	}
	m_bounds = aabb;
}
void bestPlaneIndirect( const AABB& aabb, SelectionTest& test, Plane3& plane, Vector3& intersection, float& dist, const Matrix4& rotation = g_matrix4_identity ){
	Vector3 corners[8];
	aabb_corners_oriented( aabb, rotation, corners );

	Plane3 planes[6];
	aabb_planes_oriented( aabb, rotation, planes );
/*
	const std::size_t indices[24] = {
		2, 1, 5, 6, //+x //right
		3, 7, 4, 0, //-x //left
		1, 0, 4, 5, //+y //front
		3, 2, 6, 7, //-y //back
		0, 1, 2, 3, //+z //top
		7, 6, 5, 4, //-z //bottom
	};
*/
/*

        0 ----- 1
        /|    /|
       / |   / |
      /  |  /  |
    3 ----- 2  |
     |  4|_|___|5
     |  /  |   /
     | /   |  /
     |/    | /
    7|_____|/6

 */

	const std::size_t edges[24] = {
		0, 1,
		1, 2,
		2, 3,
		3, 0,
		4, 5,
		5, 6,
		6, 7,
		7, 4,
		0, 4,
		1, 5,
		2, 6,
		3, 7,
	};

	const std::size_t adjacent_planes[24] = {
		4, 2,
		4, 0,
		4, 3,
		4, 1,
		5, 2,
		5, 0,
		5, 3,
		5, 1,
		1, 2,
		2, 0,
		0, 3,
		3, 1,
	};

	for ( std::size_t i = 0; i < 24; ++++i ){
		Line line( corners[edges[i]], corners[edges[i + 1]] );
		if( matrix4_clip_line_by_nearplane( test.getVolume().GetViewMatrix(), line ) == 2 ){
			const Vector3 intersection_new = line_closest_point( line, g_vector3_identity );
			const float dist_new = vector3_length_squared( intersection_new );
			if( dist_new < dist ){
				const Plane3& plane1 = planes[adjacent_planes[i]];
				const Plane3& plane2 = planes[adjacent_planes[i + 1]];
				if( plane3_distance_to_point( plane1, test.getVolume().getViewer() ) <= 0 ){
					if( aabb.extents[( ( adjacent_planes[i] >> 1 ) << 1 ) / 2] == 0 ) /* select the other, if zero bound */
						plane = plane2;
					else
						plane = plane1;
					intersection = intersection_new;
					dist = dist_new;
				}
				else{
					if( plane3_distance_to_point( plane2, test.getVolume().getViewer() ) <= 0 ){
						if( aabb.extents[( ( adjacent_planes[i + 1] >> 1 ) << 1 ) / 2] == 0 ) /* select the other, if zero bound */
							plane = plane1;
						else
							plane = plane2;
						intersection = intersection_new;
						dist = dist_new;
					}
				}
			}
		}
	}
	m_bounds = aabb;
}
void selectByPlane( const AABB& aabb, const Plane3& plane, const Matrix4& rotation = g_matrix4_identity ){
	Plane3 planes[6];
	aabb_planes_oriented( aabb, rotation, planes );

	for ( std::size_t i = 0; i < 6; ++i ){
		if( plane3_equal( plane, planes[i] ) || plane3_equal( plane, plane3_flipped( planes[i] ) ) ){
			m_selectables[i].setSelected( true );
		}
	}
}

AABB evaluateResize( const Vector3& translation ) const {
	Vector3 min = m_bounds.origin - m_bounds.extents;
	Vector3 max = m_bounds.origin + m_bounds.extents;
	for ( std::size_t i = 0; i < 3; ++i )
		if ( m_bounds.extents[i] != 0 ){
			if ( m_selectables[i * 2].isSelected() )
				max[i] += translation[i];
			if ( m_selectables[i * 2 + 1].isSelected() )
				min[i] += translation[i];
		}
	return AABB( vector3_mid( min, max ), vector3_scaled( vector3_subtracted( max, min ), 0.5 ) );
}
AABB evaluateResize( const Vector3& translation, const Matrix4& rotation ) const {
	AABB aabb( evaluateResize( translation_to_local( translation, rotation ) ) );
	aabb.origin = m_bounds.origin + translation_from_local( aabb.origin - m_bounds.origin, rotation );
	return aabb;
}
Matrix4 evaluateTransform( const Vector3& translation ) const {
	AABB aabb( evaluateResize( translation ) );
	Vector3 scale(
		m_bounds.extents[0] != 0 ? aabb.extents[0] / m_bounds.extents[0] : 1,
		m_bounds.extents[1] != 0 ? aabb.extents[1] / m_bounds.extents[1] : 1,
		m_bounds.extents[2] != 0 ? aabb.extents[2] / m_bounds.extents[2] : 1
		);

	Matrix4 matrix( matrix4_translation_for_vec3( aabb.origin - m_bounds.origin ) );
	matrix4_pivoted_scale_by_vec3( matrix, scale, m_bounds.origin );

	return matrix;
}
};


class ScaleRadius {
	ObservedSelectable m_selectable;
public:
	static Matrix4& m_model() {
		static Matrix4 model;
		return model;
	}

	ScaleRadius( const SelectionChangeCallback& onchanged ) :
		m_selectable( onchanged ) {
	}
	bool isSelected() const {
		return m_selectable.isSelected();
	}
	void setSelected( bool selected ) {
		m_selectable.setSelected( selected );
	}
	void selectPlanes( Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback ) {
		m_model() = test.getVolume().GetModelview();
		Selector_add( selector, m_selectable );
		selectedPlaneCallback( Plane3( 2, 0, 0, 0 ) );
	}
	float evaluateResize( const Vector3& translation ) const {
		const float len = vector3_length( translation );
		if( len == 0 )
			return 0;
		Vector3 tra = matrix4_transformed_direction( m_model(), translation );
		vector3_normalise( tra );
		return tra[0] * len;
	}
};

#endif

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

#include "selection.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtor_clip.h"
#include "selection_mtor_drag.h"
#include "selection_mtor_rotate.h"
#include "selection_mtor_scale.h"
#include "selection_mtor_skew.h"
#include "selection_mtor_translate.h"
#include "selection_mtor_uv.h"
#include "clippertool.h"
#include "selection_mtable_translate.h"

#include "debugging/debugging.h"

#include "windowobserver.h"
#include "iundo.h"
#include "ientity.h"
#include "cullable.h"
#include "renderable.h"
#include "selectable.h"
#include "editable.h"

#include "math/frustum.h"
#include "signal/signal.h"
#include "selectionlib.h"
#include "render.h"
#include "view.h"
#include "renderer.h"
#include "stream/stringstream.h"
#include "eclasslib.h"
#include "generic/bitfield.h"
#include "stringio.h"

#include "grid.h"
#include "brush.h"


#if defined( _DEBUG ) && !defined( _DEBUG_QUICKER )
class test_quat
{
public:
	test_quat( const Vector3& from, const Vector3& to ){
		Vector4 quaternion( quaternion_for_unit_vectors( from, to ) );
		Matrix4 matrix( matrix4_rotation_for_quaternion( quaternion_multiplied_by_quaternion( quaternion, c_quaternion_identity ) ) );
	}
private:
};

static test_quat bleh( g_vector3_axis_x, g_vector3_axis_y );
#endif


void Scene_Translate_Component_Selected( scene::Graph& graph, const Vector3& translation );
void Scene_Translate_Selected( scene::Graph& graph, const Vector3& translation );
void Scene_TestSelect_Primitive( Selector& selector, SelectionTest& test, const VolumeTest& volume );
void Scene_TestSelect_Component( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode );
void Scene_TestSelect_Component_Selected( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode );
void Scene_SelectAll_Component( bool select, SelectionSystem::EComponentMode componentMode );


class SelectionCounter
{
public:
	using func = void(const Selectable &);

	SelectionCounter( const SelectionChangeCallback& onchanged )
		: m_count( 0 ), m_onchanged( onchanged ){
	}
	void operator()( const Selectable& selectable ){
		if ( selectable.isSelected() ) {
			++m_count;
		}
		else
		{
			ASSERT_MESSAGE( m_count != 0, "selection counter underflow" );
			--m_count;
		}

		m_onchanged( selectable );
	}
	bool empty() const {
		return m_count == 0;
	}
	std::size_t size() const {
		return m_count;
	}
private:
	std::size_t m_count;
	SelectionChangeCallback m_onchanged;
};

class SelectedStuffCounter
{
public:
	std::size_t m_brushcount;
	std::size_t m_patchcount;
	std::size_t m_entitycount;
	SelectedStuffCounter() : m_brushcount( 0 ), m_patchcount( 0 ), m_entitycount( 0 ){
	}
	void increment( scene::Node& node ) {
		if( Node_isBrush( node ) )
			++m_brushcount;
		else if( Node_isPatch( node ) )
			++m_patchcount;
		else if( Node_isEntity( node ) )
			++m_entitycount;
	}
	void decrement( scene::Node& node ) {
		if( Node_isBrush( node ) )
			--m_brushcount;
		else if( Node_isPatch( node ) )
			--m_patchcount;
		else if( Node_isEntity( node ) )
			--m_entitycount;
	}
	void get( std::size_t& brushes, std::size_t& patches, std::size_t& entities ) const {
		brushes = m_brushcount;
		patches = m_patchcount;
		entities = m_entitycount;
	}
};

#if 0
Quaternion construct_local_rotation( const Quaternion& world, const Quaternion& localToWorld ){
	return quaternion_normalised( quaternion_multiplied_by_quaternion(
	                                  quaternion_normalised( quaternion_multiplied_by_quaternion(
	                                          quaternion_inverse( localToWorld ),
	                                          world
	                                          ) ),
	                                  localToWorld
	                              ) );
}
#endif
inline void matrix4_assign_rotation( Matrix4& matrix, const Matrix4& other ){
	matrix[0] = other[0];
	matrix[1] = other[1];
	matrix[2] = other[2];
	matrix[4] = other[4];
	matrix[5] = other[5];
	matrix[6] = other[6];
	matrix[8] = other[8];
	matrix[9] = other[9];
	matrix[10] = other[10];
}
#define SELECTIONSYSTEM_AXIAL_PIVOTS
void matrix4_assign_rotation_for_pivot( Matrix4& matrix, scene::Instance& instance ){
#ifndef SELECTIONSYSTEM_AXIAL_PIVOTS
	Editable* editable = Node_getEditable( instance.path().top() );
	if ( editable != 0 ) {
		matrix4_assign_rotation( matrix, matrix4_multiplied_by_matrix4( instance.localToWorld(), editable->getLocalPivot() ) );
	}
	else
	{
		matrix4_assign_rotation( matrix, instance.localToWorld() );
	}
#endif
}

class TranslateSelected : public SelectionSystem::Visitor
{
	const Vector3& m_translate;
public:
	TranslateSelected( const Vector3& translate )
		: m_translate( translate ){
	}
	void visit( scene::Instance& instance ) const override {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setRotation( c_rotation_identity );
			transform->setTranslation( m_translate );
		}
	}
};

void Scene_Translate_Selected( scene::Graph& graph, const Vector3& translation ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( TranslateSelected( translation ) );
	}
}

Vector3 get_local_pivot( const Vector3& world_pivot, const Matrix4& localToWorld ){
	return matrix4_transformed_point(
	           matrix4_full_inverse( localToWorld ),
	           world_pivot
	       );
}

void translation_for_pivoted_matrix_transform( Vector3& parent_translation, const Matrix4& local_transform, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	// we need a translation inside the parent system to move the origin of this object to the right place

	// mathematically, it must fulfill:
	//
	//   local_translation local_transform local_pivot = local_pivot
	//   local_translation = local_pivot - local_transform local_pivot
	//
	//   or maybe?
	//   local_transform local_translation local_pivot = local_pivot
	//                   local_translation local_pivot = local_transform^-1 local_pivot
	//                 local_translation + local_pivot = local_transform^-1 local_pivot
	//                   local_translation             = local_transform^-1 local_pivot - local_pivot

	Vector3 local_pivot( get_local_pivot( world_pivot, localToWorld ) );

	Vector3 local_translation(
	    vector3_subtracted(
	        local_pivot,
	        matrix4_transformed_point(
	            local_transform,
	            local_pivot
	        )
	        /*
	            matrix4_transformed_point(
	                matrix4_full_inverse( local_transform ),
	                local_pivot
	            ),
	            local_pivot
	         */
	    )
	);

	parent_translation = translation_local2object( local_translation, localToParent );

	/*
	   // verify it!
	   globalOutputStream() << "World pivot is at " << world_pivot << '\n';
	   globalOutputStream() << "Local pivot is at " << local_pivot << '\n';
	   globalOutputStream() << "Transformation " << local_transform << " moves it to: " << matrix4_transformed_point( local_transform, local_pivot ) << '\n';
	   globalOutputStream() << "Must move by " << local_translation << " in the local system" << '\n';
	   globalOutputStream() << "Must move by " << parent_translation << " in the parent system" << '\n';
	 */
}

void translation_for_pivoted_rotation( Vector3& parent_translation, const Quaternion& local_rotation, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	translation_for_pivoted_matrix_transform( parent_translation, matrix4_rotation_for_quaternion_quantised( local_rotation ), world_pivot, localToWorld, localToParent );
}

void translation_for_pivoted_scale( Vector3& parent_translation, const Vector3& world_scale, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	Matrix4 local_transform(
	    matrix4_multiplied_by_matrix4(
	        matrix4_full_inverse( localToWorld ),
	        matrix4_multiplied_by_matrix4(
	            matrix4_scale_for_vec3( world_scale ),
	            localToWorld
	        )
	    )
	);
	local_transform.tx() = local_transform.ty() = local_transform.tz() = 0; // cancel translation parts
	translation_for_pivoted_matrix_transform( parent_translation, local_transform, world_pivot, localToWorld, localToParent );
}

void translation_for_pivoted_skew( Vector3& parent_translation, const Skew& local_skew, const Vector3& world_pivot, const Matrix4& localToWorld, const Matrix4& localToParent ){
	Matrix4 local_transform( g_matrix4_identity );
	local_transform[local_skew.index] = local_skew.amount;
	translation_for_pivoted_matrix_transform( parent_translation, local_transform, world_pivot, localToWorld, localToParent );
}

class rotate_selected : public SelectionSystem::Visitor
{
	const Quaternion& m_rotate;
	const Vector3& m_world_pivot;
public:
	rotate_selected( const Quaternion& rotation, const Vector3& world_pivot )
		: m_rotate( rotation ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
		if ( transformNode != 0 ) {
			Transformable* transform = Instance_getTransformable( instance );
			if ( transform != 0 ) {
				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setScale( c_scale_identity );
				transform->setTranslation( c_translation_identity );

				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setRotation( m_rotate );

				{
					Editable* editable = Node_getEditable( instance.path().top() );
					const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

					Vector3 parent_translation;
					translation_for_pivoted_rotation(
					    parent_translation,
					    m_rotate,
					    m_world_pivot,
#ifdef SELECTIONSYSTEM_AXIAL_PIVOTS
					    matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( instance.localToWorld() ) ), localPivot ),
					    matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( transformNode->localToParent() ) ), localPivot )
#else
					    matrix4_multiplied_by_matrix4( instance.localToWorld(), localPivot ),
					    matrix4_multiplied_by_matrix4( transformNode->localToParent(), localPivot )
#endif
					);

					transform->setTranslation( parent_translation );
				}
			}
		}
	}
};

void Scene_Rotate_Selected( scene::Graph& graph, const Quaternion& rotation, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( rotate_selected( rotation, world_pivot ) );
	}
}

class scale_selected : public SelectionSystem::Visitor
{
	const Vector3& m_scale;
	const Vector3& m_world_pivot;
public:
	scale_selected( const Vector3& scaling, const Vector3& world_pivot )
		: m_scale( scaling ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
		if ( transformNode != 0 ) {
			Transformable* transform = Instance_getTransformable( instance );
			if ( transform != 0 ) {
				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setScale( c_scale_identity );
				transform->setTranslation( c_translation_identity );

				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setScale( m_scale );
				{
					Editable* editable = Node_getEditable( instance.path().top() );
					const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

					Vector3 parent_translation;
					translation_for_pivoted_scale(
					    parent_translation,
					    m_scale,
					    m_world_pivot,
					    matrix4_multiplied_by_matrix4( instance.localToWorld(), localPivot ),
					    matrix4_multiplied_by_matrix4( transformNode->localToParent(), localPivot )
					);

					transform->setTranslation( parent_translation );
				}
			}
		}
	}
};

void Scene_Scale_Selected( scene::Graph& graph, const Vector3& scaling, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( scale_selected( scaling, world_pivot ) );
	}
}

class skew_selected : public SelectionSystem::Visitor
{
	const Skew& m_skew;
	const Vector3& m_world_pivot;
public:
	skew_selected( const Skew& skew, const Vector3& world_pivot )
		: m_skew( skew ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
		if ( transformNode != 0 ) {
			Transformable* transform = Instance_getTransformable( instance );
			if ( transform != 0 ) {
				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setScale( c_scale_identity );
				transform->setTranslation( c_translation_identity );

				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setSkew( m_skew );
				{
					Editable* editable = Node_getEditable( instance.path().top() );
					const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

					Vector3 parent_translation;
					translation_for_pivoted_skew(
					    parent_translation,
					    m_skew,
					    m_world_pivot,
					    matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( instance.localToWorld() ) ), localPivot ),
					    matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( transformNode->localToParent() ) ), localPivot )
					);

					transform->setTranslation( parent_translation );
				}
			}
		}
	}
};

void Scene_Skew_Selected( scene::Graph& graph, const Skew& skew, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelected( skew_selected( skew, world_pivot ) );
	}
}

class transform_selected : public SelectionSystem::Visitor
{
	const Transforms& m_transforms;
	const Vector3& m_world_pivot;
public:
	transform_selected( const Transforms& transforms, const Vector3& world_pivot )
		: m_transforms( transforms ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		TransformNode* transformNode = Node_getTransformNode( instance.path().top() );
		if ( transformNode != 0 ) {
			Transformable* transform = Instance_getTransformable( instance );
			if ( transform != 0 ) {
				transform->setType( TRANSFORM_PRIMITIVE );
				transform->setRotation( m_transforms.getRotation() );
				transform->setScale( m_transforms.getScale() );
				transform->setSkew( m_transforms.getSkew() );
				transform->setTranslation( c_translation_identity );
				{
					Editable* editable = Node_getEditable( instance.path().top() );
					const Matrix4& localPivot = editable != 0 ? editable->getLocalPivot() : g_matrix4_identity;

					const Matrix4 local_transform = matrix4_transform_for_components( c_translation_identity, m_transforms.getRotation(), m_transforms.getScale(), m_transforms.getSkew() );
					Vector3 parent_translation;
					translation_for_pivoted_matrix_transform(
					    parent_translation,
					    local_transform,
					    m_world_pivot,
					    matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( instance.localToWorld() ) ), localPivot ),
					    matrix4_multiplied_by_matrix4( matrix4_translation_for_vec3( matrix4_get_translation_vec3( transformNode->localToParent() ) ), localPivot )
					);

					transform->setTranslation( parent_translation + m_transforms.getTranslation() );
				}
			}
		}
	}
};


class translate_component_selected : public SelectionSystem::Visitor
{
	const Vector3& m_translate;
public:
	translate_component_selected( const Vector3& translate )
		: m_translate( translate ){
	}
	void visit( scene::Instance& instance ) const override {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_COMPONENT );
			transform->setRotation( c_rotation_identity );
			transform->setTranslation( m_translate );
		}
	}
};

void Scene_Translate_Component_Selected( scene::Graph& graph, const Vector3& translation ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( translate_component_selected( translation ) );
	}
}

class rotate_component_selected : public SelectionSystem::Visitor
{
	const Quaternion& m_rotate;
	const Vector3& m_world_pivot;
public:
	rotate_component_selected( const Quaternion& rotation, const Vector3& world_pivot )
		: m_rotate( rotation ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			Vector3 parent_translation;
			translation_for_pivoted_rotation( parent_translation, m_rotate, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

			transform->setType( TRANSFORM_COMPONENT );
			transform->setRotation( m_rotate );
			transform->setTranslation( parent_translation );
		}
	}
};

void Scene_Rotate_Component_Selected( scene::Graph& graph, const Quaternion& rotation, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( rotate_component_selected( rotation, world_pivot ) );
	}
}

class scale_component_selected : public SelectionSystem::Visitor
{
	const Vector3& m_scale;
	const Vector3& m_world_pivot;
public:
	scale_component_selected( const Vector3& scaling, const Vector3& world_pivot )
		: m_scale( scaling ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			Vector3 parent_translation;
			translation_for_pivoted_scale( parent_translation, m_scale, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

			transform->setType( TRANSFORM_COMPONENT );
			transform->setScale( m_scale );
			transform->setTranslation( parent_translation );
		}
	}
};

void Scene_Scale_Component_Selected( scene::Graph& graph, const Vector3& scaling, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( scale_component_selected( scaling, world_pivot ) );
	}
}

class skew_component_selected : public SelectionSystem::Visitor
{
	const Skew& m_skew;
	const Vector3& m_world_pivot;
public:
	skew_component_selected( const Skew& skew, const Vector3& world_pivot )
		: m_skew( skew ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			Vector3 parent_translation;
			translation_for_pivoted_skew( parent_translation, m_skew, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

			transform->setType( TRANSFORM_COMPONENT );
			transform->setSkew( m_skew );
			transform->setTranslation( parent_translation );
		}
	}
};

void Scene_Skew_Component_Selected( scene::Graph& graph, const Skew& skew, const Vector3& world_pivot ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
		GlobalSelectionSystem().foreachSelectedComponent( skew_component_selected( skew, world_pivot ) );
	}
}


class transform_component_selected : public SelectionSystem::Visitor
{
	const Transforms& m_transforms;
	const Vector3& m_world_pivot;
public:
	transform_component_selected( const Transforms& transforms, const Vector3& world_pivot )
		: m_transforms( transforms ), m_world_pivot( world_pivot ){
	}
	void visit( scene::Instance& instance ) const override {
		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			const Matrix4 local_transform = matrix4_transform_for_components( c_translation_identity, m_transforms.getRotation(), m_transforms.getScale(), m_transforms.getSkew() );
			Vector3 parent_translation;
			translation_for_pivoted_matrix_transform( parent_translation, local_transform, m_world_pivot, instance.localToWorld(), Node_getTransformNode( instance.path().top() )->localToParent() );

			transform->setType( TRANSFORM_COMPONENT );
			transform->setRotation( m_transforms.getRotation() );
			transform->setScale( m_transforms.getScale() );
			transform->setSkew( m_transforms.getSkew() );
			transform->setTranslation( parent_translation + m_transforms.getTranslation() );
		}
	}
};



namespace detail
{
inline void testselect_scene_point__brush( BrushInstance* brush, ScenePointSelector& m_selector, SelectionTest& m_test ){
	m_test.BeginMesh( brush->localToWorld() );
	for( const auto& face : brush->getBrush() ) {
		if( !face->isFiltered() ) {
			SelectionIntersection intersection;
			face->testSelect( m_test, intersection );
			m_selector.addIntersection( intersection, face );
		}
	}
}
}

class testselect_scene_point : public scene::Graph::Walker {
	ScenePointSelector& m_selector;
	SelectionTest& m_test;
public:
	testselect_scene_point( ScenePointSelector& selector, SelectionTest& test ) : m_selector( selector ), m_test( test ) {
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if( BrushInstance* brush = Instance_getBrush( instance ) ) {
			detail::testselect_scene_point__brush( brush, m_selector, m_test );
		}
		else if( SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance ) ) {
			selectionTestable->testSelect( m_selector, m_test );
		}
		return true;
	}
};

class testselect_scene_point_unselected : public scene::Graph::Walker {
	ScenePointSelector& m_selector;
	SelectionTest& m_test;
public:
	testselect_scene_point_unselected( ScenePointSelector& selector, SelectionTest& test ) : m_selector( selector ), m_test( test ) {
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if( !Instance_isSelected( instance ) ){
			if( BrushInstance* brush = Instance_getBrush( instance ) ) {
				detail::testselect_scene_point__brush( brush, m_selector, m_test );
			}
			else if( SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance ) ) {
				selectionTestable->testSelect( m_selector, m_test );
			}
			return true;
		}
		return false; // avoids entities with node unselected (e.g. model)
	}
};

class testselect_scene_point_selected_brushes : public scene::Graph::Walker {
	ScenePointSelector& m_selector;
	SelectionTest& m_test;
public:
	testselect_scene_point_selected_brushes( ScenePointSelector& selector, SelectionTest& test ) : m_selector( selector ), m_test( test ) {
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if( Instance_isSelected( instance ) ){
			if( BrushInstance* brush = Instance_getBrush( instance ) ) {
				detail::testselect_scene_point__brush( brush, m_selector, m_test );
			}
		}
		return true;
	}
};

DoubleVector3 testSelected_scene_snapped_point( const SelectionVolume& test, ScenePointSelector& selector ){
	DoubleVector3 point = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, selector.best().depth(), 1 ) ) );
	if( selector.face() ){
		const Face& face = *selector.face();
		double bestDist = FLT_MAX;
		DoubleVector3 wannabePoint;
		for ( Winding::const_iterator prev = face.getWinding().end() - 1, curr = face.getWinding().begin(); curr != face.getWinding().end(); prev = curr, ++curr ){
			const DoubleVector3 v1( prev->vertex );
			const DoubleVector3 v2( curr->vertex );
			{	/* try vertices */
				const double dist = vector3_length_squared( v2 - point );
				if( dist < bestDist ){
					wannabePoint = v2;
					bestDist = dist;
				}
			}
			{	/* try edges */
				DoubleVector3 edgePoint = line_closest_point( DoubleLine( v1, v2 ), point );
				if( edgePoint != v1 && edgePoint != v2 ){
					const DoubleVector3 edgedir = vector3_normalised( v2 - v1 );
					const std::size_t maxi = vector3_max_abs_component_index( edgedir );
					// v1[maxi] + edgedir[maxi] * coef = float_snapped( point[maxi], GetSnapGridSize() )
					const double coef = ( float_snapped( point[maxi], GetSnapGridSize() ) - v1[maxi] ) / edgedir[maxi];
					edgePoint = v1 + edgedir * coef;
					const double dist = vector3_length_squared( edgePoint - point );
					if( dist < bestDist ){
						wannabePoint = edgePoint;
						bestDist = dist;
					}
				}
			}
		}
		if( selector.best().distance() == 0 ){ /* try plane, if pointing inside of polygon */
			const std::size_t maxi = vector3_max_abs_component_index( face.plane3().normal() );
			DoubleVector3 planePoint( vector3_snapped( point, GetSnapGridSize() ) );
			// face.plane3().normal().dot( point snapped ) = face.plane3().dist()
			planePoint[maxi] = ( face.plane3().dist()
			                     - face.plane3().normal()[( maxi + 1 ) % 3] * planePoint[( maxi + 1 ) % 3]
			                     - face.plane3().normal()[( maxi + 2 ) % 3] * planePoint[( maxi + 2 ) % 3] ) / face.plane3().normal()[maxi];
			const double dist = vector3_length_squared( planePoint - point );
			if( dist < bestDist ){
				wannabePoint = planePoint;
				bestDist = dist;
			}
		}
		point = wannabePoint;
	}
	else{
		vector3_snap( point, GetSnapGridSize() );
	}
	return point;
}

std::optional<testSelect_unselected_scene_point_return_t>
testSelect_unselected_scene_point( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon ){
	View scissored( view );
	ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

	SelectionVolume test( scissored );
	ScenePointSelector selector;
	Scene_forEachVisible( GlobalSceneGraph(), scissored, testselect_scene_point_unselected( selector, test ) );
	test.BeginMesh( g_matrix4_identity, true );
	if( selector.isSelected() ){
		return testSelect_unselected_scene_point_return_t{ testSelected_scene_snapped_point( test, selector ),
			selector.face() != nullptr? selector.face()->plane3() : std::optional<Plane3>() };
	}
	return {};
}


void Scene_forEachVisible_testselect_scene_point( const View& view, ScenePointSelector& selector, SelectionTest& test ){
	Scene_forEachVisible( GlobalSceneGraph(), view, testselect_scene_point( selector, test ) );
}

void Scene_forEachVisible_testselect_scene_point_selected_brushes( const View& view, ScenePointSelector& selector, SelectionTest& test ){
	Scene_forEachVisible( GlobalSceneGraph(), view, testselect_scene_point_selected_brushes( selector, test ) );
}



class BuildManipulator : public Manipulator, public Manipulatable
{
	bool m_isSelected;
	bool m_isInitialised;
	RenderablePoint m_point;
	RenderableLine m_line;
	RenderableLine m_midline;
public:
	static Shader* m_state_point;
	static Shader* m_state_line;

	BuildManipulator() : m_isSelected( false ), m_isInitialised( false ) {
		m_point.setColour( g_colour_selected );
		m_line.setColour( g_colour_selected );
		m_midline.setColour( g_colour_screen );
	}
	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		renderer.SetState( m_state_point, Renderer::eWireframeOnly );
		renderer.SetState( m_state_point, Renderer::eFullMaterials );
		renderer.addRenderable( m_point, g_matrix4_identity );
		renderer.SetState( m_state_line, Renderer::eWireframeOnly );
		renderer.SetState( m_state_line, Renderer::eFullMaterials );
		renderer.addRenderable( m_line, g_matrix4_identity );
		renderer.addRenderable( m_midline, g_matrix4_identity );
	}
	void initialise(){
	}
	void highlight( const View& view, const Matrix4& pivot2world ) override {
		SceneChangeNotify();
	}

	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		m_isSelected = true;
	}
	/* Manipulatable */
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		//do things with undo
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
	}

	Manipulatable* GetManipulatable() override {
		m_isSelected = false; //don't handle the manipulator move part void MoveSelected()
		return this;
	}

	void setSelected( bool select ) override {
		m_isSelected = select;
	}
	bool isSelected() const override {
		return m_isSelected;
	}
};
Shader* BuildManipulator::m_state_point;
Shader* BuildManipulator::m_state_line;



class TransformOriginTranslatable
{
public:
	virtual void transformOriginTranslate( const Vector3& translation, const bool set[3] ) = 0;
};

class TransformOriginTranslate : public Manipulatable
{
private:
	Vector3 m_start;
	TransformOriginTranslatable& m_transformOriginTranslatable;
public:
	TransformOriginTranslate( TransformOriginTranslatable& transformOriginTranslatable )
		: m_transformOriginTranslatable( transformOriginTranslatable ){
	}
	void Construct( const Matrix4& device2manip, const DeviceVector device_point, const AABB& bounds, const Vector3& transform_origin ) override {
		m_start = point_on_plane( device2manip, device_point );
	}
	void Transform( const Matrix4& manip2object, const Matrix4& device2manip, const DeviceVector device_point ) override {
		Vector3 current = point_on_plane( device2manip, device_point );
		current = vector3_subtracted( current, m_start );

		if( g_modifiers.shift() ){ // snap to axis
			for ( std::size_t i = 0; i < 3; ++i ){
				if( std::fabs( current[i] ) >= std::fabs( current[( i + 1 ) % 3] ) ){
					current[( i + 1 ) % 3] = 0;
				}
				else{
					current[i] = 0;
				}
			}
		}

		bool set[3] = { true, true, true };
		for ( std::size_t i = 0; i < 3; ++i ){
			if( std::fabs( current[i] ) < 1e-3f ){
				set[i] = false;
			}
		}

		current = translation_local2object( current, manip2object );

		m_transformOriginTranslatable.transformOriginTranslate( current, set );
	}
};

class TransformOriginManipulator : public Manipulator, public ManipulatorSelectionChangeable
{
	TransformOriginTranslate m_translate;
	const bool& m_pivotIsCustom;
	RenderablePoint m_point;
	SelectableBool m_selectable;
	Pivot2World m_pivot;
public:
	static Shader* m_state;

	TransformOriginManipulator( TransformOriginTranslatable& transformOriginTranslatable, const bool& pivotIsCustom ) :
		m_translate( transformOriginTranslatable ),
		m_pivotIsCustom( pivotIsCustom ){
	}

	void UpdateColours() {
		m_point.setColour(
			m_selectable.isSelected()?
				m_pivotIsCustom? Colour4b( 255, 232, 0, 255 )
				: g_colour_selected
			:	m_pivotIsCustom? Colour4b( 0, 125, 255, 255 )
				: g_colour_screen );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		m_pivot.update( pivot2world, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

		// temp hack
		UpdateColours();

		renderer.SetState( m_state, Renderer::eWireframeOnly );
		renderer.SetState( m_state, Renderer::eFullMaterials );

		renderer.addRenderable( m_point, m_pivot.m_worldSpace );
	}
	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		if( g_modifiers != c_modifierNone )
			return selectionChange( nullptr );

		m_pivot.update( pivot2world, view.GetModelview(), view.GetProjection(), view.GetViewport() );

		SelectionPool selector;
		{
			const Matrix4 local2view( matrix4_multiplied_by_matrix4( view.GetViewMatrix(), m_pivot.m_worldSpace ) );

#if defined( DEBUG_SELECTION )
			g_render_clipped.construct( view.GetViewMatrix() );
#endif
			{
				SelectionIntersection best;

				Point_BestPoint( local2view, m_point.m_point, best );
				selector.addSelectable( best, &m_selectable );
			}
		}

		selectionChange( selector );
	}

	Manipulatable* GetManipulatable() override {
		return &m_translate;
	}

	void setSelected( bool select ) override {
		m_selectable.setSelected( select );
	}
	bool isSelected() const override {
		return m_selectable.isSelected();
	}
};
Shader* TransformOriginManipulator::m_state;

class TransformsObserved : public Transforms
{
public:
	void setTranslation( const Translation& value ){
		Transforms::setTranslation( value );
		m_changedCallbacks[SelectionSystem::eTranslate]( StringStream<64>( m_translation == c_translation_identity? ' ' : 'x',
			" Translate ", m_translation.x(), ' ', m_translation.y(), ' ', m_translation.z() ) );
	}
	void setRotation( const Rotation& value ){
		Transforms::setRotation( value );
		m_changedCallbacks[SelectionSystem::eRotate]( StringStream<64>( m_rotation == c_rotation_identity? ' ' : 'x',
			" Rotate ", m_rotation.x(), ' ', m_rotation.y(), ' ', m_rotation.z() ) );
	}
	void setScale( const Scale& value ){
		Transforms::setScale( value );
		m_changedCallbacks[SelectionSystem::eScale]( StringStream<64>( m_scale == c_scale_identity? ' ' : 'x',
			" Scale ", m_scale.x(), ' ', m_scale.y(), ' ', m_scale.z() ) );
	}
	void setSkew( const Skew& value ){
		Transforms::setSkew( value );
		m_changedCallbacks[SelectionSystem::eSkew]( StringStream<64>( m_skew == c_skew_identity? ' ' : 'x',
			" Skew ", m_skew.index, ' ', m_skew.amount ) );
	}

	std::array<Callback<void(const char*)>, 4> m_changedCallbacks;
	static_assert( SelectionSystem::eTranslate == 0
	            && SelectionSystem::eRotate == 1
				&& SelectionSystem::eScale == 2
				&& SelectionSystem::eSkew == 3 );
};

class select_all : public scene::Graph::Walker
{
	bool m_select;
public:
	select_all( bool select )
		: m_select( select ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 ) {
			selectable->setSelected( m_select );
		}
		return true;
	}
};

class select_all_component : public scene::Graph::Walker
{
	bool m_select;
	SelectionSystem::EComponentMode m_mode;
public:
	select_all_component( bool select, SelectionSystem::EComponentMode mode )
		: m_select( select ), m_mode( mode ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
		if ( componentSelectionTestable ) {
			componentSelectionTestable->setSelectedComponents( m_select, m_mode );
		}
		return true;
	}
};

void Scene_SelectAll_Component( bool select, SelectionSystem::EComponentMode componentMode ){
	GlobalSceneGraph().traverse( select_all_component( select, componentMode ) );
}

void Scene_BoundsSelected( scene::Graph& graph, AABB& bounds );
class LazyBounds
{
	AABB m_bounds;
	bool m_valid;
public:
	LazyBounds() : m_valid( false ){
	}
	void setInvalid(){
		m_valid = false;
	}
	const AABB& getBounds(){
		if( !m_valid ){
			Scene_BoundsSelected( GlobalSceneGraph(), m_bounds );
			m_valid = true;
		}
		return m_bounds;
	}
};


// RadiantSelectionSystem
class RadiantSelectionSystem final :
	public SelectionSystem,
	public Translatable,
	public Rotatable,
	public Scalable,
	public Skewable,
	public AllTransformable,
	public TransformOriginTranslatable,
	public Renderable
{
	mutable Matrix4 m_pivot2world;
	mutable AABB m_bounds;
	mutable LazyBounds m_lazy_bounds;
	Matrix4 m_pivot2world_start;
	Matrix4 m_manip2pivot_start;
	Translation m_translation;
	Rotation m_rotation;
	Scale m_scale;
	Skew m_skew;
public:
	static Shader* m_state;
	bool m_bPreferPointEntsIn2D;
private:
	EManipulatorMode m_manipulator_mode;
	Manipulator* m_manipulator;

// state
	bool m_undo_begun;
	EMode m_mode;
	EComponentMode m_componentmode;

	SelectionCounter m_count_primitive;
	SelectionCounter m_count_component;
	SelectedStuffCounter m_count_stuff;

	std::unique_ptr<TranslateManipulator> m_translate_manipulator;
	std::unique_ptr<RotateManipulator> m_rotate_manipulator;
	std::unique_ptr<ScaleManipulator> m_scale_manipulator;
	std::unique_ptr<SkewManipulator> m_skew_manipulator;
	std::unique_ptr<DragManipulator> m_drag_manipulator;
	std::unique_ptr<ClipManipulator> m_clip_manipulator;
	BuildManipulator m_build_manipulator;
	std::unique_ptr<UVManipulator> m_uv_manipulator;
	mutable TransformOriginManipulator m_transformOrigin_manipulator;

	typedef UnsortedSet<scene::Instance*, false> selection_t;
	selection_t m_selection;
	selection_t m_component_selection;

	Signal1<const Selectable&> m_selectionChanged_callbacks;

	void ConstructPivot() const;
	void ConstructPivotRotation() const;
	void setCustomTransformOrigin( const Vector3& origin, const bool set[3] ) const override;
	AABB getSelectionAABB() const;
	mutable bool m_pivotChanged;
	bool m_pivot_moving;
	mutable bool m_pivotIsCustom;

	void Scene_TestSelect( Selector& selector, SelectionTest& test, const View& view, SelectionSystem::EMode mode, SelectionSystem::EComponentMode componentMode );

	bool somethingSelected() const {
		return ( Mode() == eComponent && !m_count_component.empty() )
		    || ( Mode() == ePrimitive && !m_count_primitive.empty() );
	}
public:
	enum EModifier
	{
		eManipulator,
		eReplace,
		eCycle,
		eSelect,
		eDeselect,
	};

	RadiantSelectionSystem() :
		m_bPreferPointEntsIn2D( true ),
		m_undo_begun( false ),
		m_mode( ePrimitive ),
		m_componentmode( eDefault ),
		m_count_primitive( SelectionChangedCaller( *this ) ),
		m_count_component( SelectionChangedCaller( *this ) ),
		m_translate_manipulator( New_TranslateManipulator( *this, 2, 64 ) ),
		m_rotate_manipulator( New_RotateManipulator( *this, 8, 64 ) ),
		m_scale_manipulator( New_ScaleManipulator( *this, 0, 64 ) ),
		m_skew_manipulator( New_SkewManipulator( *this, *this, *this, *this, *this, m_bounds, m_pivot2world, m_pivotIsCustom ) ),
		m_drag_manipulator( New_DragManipulator( *this, *this ) ),
		m_clip_manipulator( New_ClipManipulator( m_pivot2world, m_bounds ) ),
		m_uv_manipulator( New_UVManipulator() ),
		m_transformOrigin_manipulator( *this, m_pivotIsCustom ),
		m_pivotChanged( false ),
		m_pivot_moving( false ),
		m_pivotIsCustom( false ){
		SetManipulatorMode( eTranslate );
		pivotChanged();
		addSelectionChangeCallback( PivotChangedSelectionCaller( *this ) );
		AddGridChangeCallback( PivotChangedCaller( *this ) );
	}
	void pivotChanged() const override {
		m_pivotChanged = true;
		m_lazy_bounds.setInvalid();
		SceneChangeNotify();
	}
	typedef ConstMemberCaller<RadiantSelectionSystem, void(), &RadiantSelectionSystem::pivotChanged> PivotChangedCaller;
	void pivotChangedSelection( const Selectable& selectable ){
		pivotChanged();
	}
	typedef MemberCaller<RadiantSelectionSystem, void(const Selectable&), &RadiantSelectionSystem::pivotChangedSelection> PivotChangedSelectionCaller;

	const AABB& getBoundsSelected() const override {
		return m_lazy_bounds.getBounds();
	}

	void SetMode( EMode mode ) override {
		if ( m_mode != mode ) {
			m_mode = mode;
			pivotChanged();
		}
	}
	EMode Mode() const override {
		return m_mode;
	}
	void SetComponentMode( EComponentMode mode ) override {
		m_componentmode = mode;
	}
	EComponentMode ComponentMode() const override {
		return m_componentmode;
	}
	void SetManipulatorMode( EManipulatorMode mode ) override {
		if( ( mode == eClip ) || ( ManipulatorMode() == eClip ) ){
			m_clip_manipulator->reset( ( mode == eClip ) && ( ManipulatorMode() != eClip ) );
			if( ( mode == eClip ) != ( ManipulatorMode() == eClip ) )
				Clipper_modeChanged( mode == eClip );
		}

		m_pivotIsCustom = false;
		m_manipulator_mode = mode;
		switch ( m_manipulator_mode )
		{
		case eTranslate: m_manipulator = m_translate_manipulator.get(); break;
		case eRotate: m_manipulator = m_rotate_manipulator.get(); break;
		case eScale: m_manipulator = m_scale_manipulator.get(); break;
		case eSkew: m_manipulator = m_skew_manipulator.get(); break;
		case eDrag: m_manipulator = m_drag_manipulator.get(); break;
		case eClip: m_manipulator = m_clip_manipulator.get(); resetTransforms( eClip ); break;
		case eBuild:
			{
				m_build_manipulator.initialise();
				m_manipulator = &m_build_manipulator; break;
			}
		case eUV: m_manipulator = m_uv_manipulator.get(); break;
		}
		pivotChanged();
	}
	EManipulatorMode ManipulatorMode() const override {
		return m_manipulator_mode;
	}

	SelectionChangeCallback getObserver( EMode mode ) override {
		if ( mode == ePrimitive ) {
			return makeCallback( m_count_primitive );
		}
		else
		{
			return makeCallback( m_count_component );
		}
	}
	std::size_t countSelected() const override {
		return m_count_primitive.size();
	}
	std::size_t countSelectedComponents() const override {
		return m_count_component.size();
	}
	void countSelectedStuff( std::size_t& brushes, std::size_t& patches, std::size_t& entities ) const override {
		m_count_stuff.get( brushes, patches, entities );
	}
	void onSelectedChanged( scene::Instance& instance, const Selectable& selectable ) override {
		if ( selectable.isSelected() ) {
			m_selection.push_back( &instance );
			m_count_stuff.increment( instance.path().top() );
		}
		else
		{
			m_selection.erase( &instance );
			m_count_stuff.decrement( instance.path().top() );
		}

		ASSERT_MESSAGE( m_selection.size() == m_count_primitive.size(), "selection-tracking error" );
	}
	void onComponentSelection( scene::Instance& instance, const Selectable& selectable ) override {
		if ( selectable.isSelected() ) {
			m_component_selection.push_back( &instance );
		}
		else
		{
			m_component_selection.erase( &instance );
		}

		ASSERT_MESSAGE( m_component_selection.size() == m_count_component.size(), "selection-tracking error" );
	}
	scene::Instance& firstSelected() const override {
		ASSERT_MESSAGE( m_selection.size() > 0, "no instance selected" );
		return **m_selection.begin();
	}
	scene::Instance& ultimateSelected() const override {
		ASSERT_MESSAGE( m_selection.size() > 0, "no instance selected" );
		return *m_selection.back();
	}
	scene::Instance& penultimateSelected() const override {
		ASSERT_MESSAGE( m_selection.size() > 1, "only one instance selected" );
		return *( *( --( --m_selection.end() ) ) );
	}
	void setSelectedAll( bool selected ) override {
		GlobalSceneGraph().traverse( select_all( selected ) );

		m_manipulator->setSelected( selected );
	}
	void setSelectedAllComponents( bool selected ) override {
		Scene_SelectAll_Component( selected, SelectionSystem::eVertex );
		Scene_SelectAll_Component( selected, SelectionSystem::eEdge );
		Scene_SelectAll_Component( selected, SelectionSystem::eFace );

		m_manipulator->setSelected( selected );
	}

	void foreachSelected( const Visitor& visitor ) const override {
		selection_t::const_iterator i = m_selection.begin();
		while ( i != m_selection.end() )
		{
			visitor.visit( *( *( i++ ) ) );
		}
	}
	void foreachSelectedComponent( const Visitor& visitor ) const override {
		selection_t::const_iterator i = m_component_selection.begin();
		while ( i != m_component_selection.end() )
		{
			visitor.visit( *( *( i++ ) ) );
		}
	}

	void addSelectionChangeCallback( const SelectionChangeHandler& handler ) override {
		m_selectionChanged_callbacks.connectLast( handler );
	}
	void selectionChanged( const Selectable& selectable ){
		m_selectionChanged_callbacks( selectable );
	}
	typedef MemberCaller<RadiantSelectionSystem, void(const Selectable&), &RadiantSelectionSystem::selectionChanged> SelectionChangedCaller;


	void startMove(){
		m_pivot2world_start = GetPivot2World();
	}

	bool SelectManipulator( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon ){
		bool movingOrigin = false;

		if ( somethingSelected()
		|| ManipulatorMode() == eDrag
		|| ManipulatorMode() == eClip
		|| ManipulatorMode() == eBuild
		|| ManipulatorMode() == eUV ) {
#if defined ( DEBUG_SELECTION )
			g_render_clipped.destroy();
#endif
			Manipulatable::assign_static( view, device_point, device_epsilon ); //this b4 m_manipulator calls!

			m_transformOrigin_manipulator.setSelected( false );
			m_manipulator->setSelected( false );

			{
				View scissored( view );
				ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

				if( transformOrigin_isTranslatable() ){
					m_transformOrigin_manipulator.testSelect( scissored, GetPivot2World() );
					movingOrigin = m_transformOrigin_manipulator.isSelected();
				}

				if( !movingOrigin )
					m_manipulator->testSelect( scissored, GetPivot2World() );
			}

			startMove();

			m_pivot_moving = m_manipulator->isSelected();

			if ( m_pivot_moving || movingOrigin ) {
				Pivot2World pivot;
				pivot.update( GetPivot2World(), view.GetModelview(), view.GetProjection(), view.GetViewport() );

				m_manip2pivot_start = matrix4_multiplied_by_matrix4( matrix4_full_inverse( m_pivot2world_start ), pivot.m_worldSpace );

				Matrix4 device2manip;
				ConstructDevice2Manip( device2manip, m_pivot2world_start, view.GetModelview(), view.GetProjection(), view.GetViewport() );
				if( m_pivot_moving ){
					m_manipulator->GetManipulatable()->Construct( device2manip, device_point, m_bounds, GetPivot2World().t().vec3() );
					m_undo_begun = false;
				}
				else if( movingOrigin ){
					m_transformOrigin_manipulator.GetManipulatable()->Construct( device2manip, device_point, m_bounds, GetPivot2World().t().vec3() );
				}
			}

			SceneChangeNotify();
		}

		return m_pivot_moving || movingOrigin;
	}

	void HighlightManipulator( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon ){
		Manipulatable::assign_static( view, device_point, device_epsilon ); //this b4 m_manipulator calls!

		if ( ( somethingSelected() && transformOrigin_isTranslatable() )
		     || ManipulatorMode() == eDrag
		     || ManipulatorMode() == eClip
		     || ManipulatorMode() == eBuild
		     || ManipulatorMode() == eUV ) {
#if defined ( DEBUG_SELECTION )
			g_render_clipped.destroy();
#endif

			m_transformOrigin_manipulator.setSelected( false );
			m_manipulator->setSelected( false );

			View scissored( view );
			ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

			if( transformOrigin_isTranslatable() )
				m_transformOrigin_manipulator.highlight( scissored, GetPivot2World() );

			if( !m_transformOrigin_manipulator.isSelected() )
				m_manipulator->highlight( scissored, GetPivot2World() );
		}
	}

	void deselectAll(){
		if ( Mode() == eComponent ) {
			setSelectedAllComponents( false );
		}
		else
		{
			setSelectedAll( false );
		}
	}

	void deselectComponentsOrAll( bool components ){
		if ( components ) {
			setSelectedAllComponents( false );
		}
		else
		{
			deselectAll();
		}
	}
#define SELECT_MATCHING
#define SELECT_MATCHING_DEPTH 1e-6f
#define SELECT_MATCHING_DIST 1e-6f
#define SELECT_MATCHING_COMPONENTS_DIST .25f
	void SelectionPool_Select( SelectionPool& pool, bool select, float dist_epsilon ){
		SelectionPool::iterator best = pool.begin();
		if( best->second->isSelected() != select ){
			best->second->setSelected( select );
		}
#ifdef SELECT_MATCHING
		for ( SelectionPool::iterator i = std::next( best ); i != pool.end(); ++i )
		{
			if( i->first.equalEpsilon( best->first, dist_epsilon, SELECT_MATCHING_DEPTH ) ){
				//if( i->second->isSelected() != select ){
				i->second->setSelected( select );
				//}
			}
			else{
				break;
			}
		}
#endif // SELECT_MATCHING
	}

	void SelectPoint( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon, RadiantSelectionSystem::EModifier modifier, bool face ){
		//globalOutputStream() << device_point[0] << "   " << device_point[1] << '\n';
		ASSERT_MESSAGE( std::fabs( device_point[0] ) <= 1 && std::fabs( device_point[1] ) <= 1, "point-selection error" );

		if ( modifier == eReplace ) {
			deselectComponentsOrAll( face );
		}
		/*
		//somethingSelected() doesn't consider faces, selected in non-component mode, m
		if ( modifier == eCycle && !somethingSelected() ){
			modifier = eReplace;
		}
		*/
#if defined ( DEBUG_SELECTION )
		g_render_clipped.destroy();
#endif

		{
			View scissored( view );
			ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

			SelectionVolume volume( scissored );
			SelectionPool selector;
			const bool prefer_point_ents = m_bPreferPointEntsIn2D && Mode() == ePrimitive && !view.fill() && !face
			                               && ( modifier == EModifier::eReplace || modifier == EModifier::eSelect || modifier == EModifier::eDeselect );

			if( prefer_point_ents && ( Scene_TestSelect( selector, volume, scissored, eEntity, ComponentMode() ), !selector.failed() ) ){
				switch ( modifier )
				{
				// if cycle mode not enabled, enable it
				case EModifier::eReplace:
					{
						// select closest
						selector.begin()->second->setSelected( true );
					}
					break;
				case EModifier::eSelect:
					{
						SelectionPool_Select( selector, true, SELECT_MATCHING_DIST );
					}
					break;
				case EModifier::eDeselect:
					{
						SelectionPool_Select( selector, false, SELECT_MATCHING_DIST );
					}
					break;
				default:
					break;
				}
			}
			else{
				const EMode mode = g_modifiers == c_modifierAlt? ePrimitive : Mode();
				if ( face ){
					Scene_TestSelect_Component( selector, volume, scissored, eFace );
				}
				else{
					Scene_TestSelect( selector, volume, scissored, mode, ComponentMode() );
				}

				if ( !selector.failed() ) {
					switch ( modifier )
					{
					// if cycle mode not enabled, enable it
					case EModifier::eReplace:
						{
							// select closest
							selector.begin()->second->setSelected( true );
						}
						break;
					// select the next object in the list from the one already selected
					case EModifier::eCycle:
						{
							bool cycleSelectionOccured = false;
							for ( SelectionPool::iterator i = selector.begin(); i != selector.end(); ++i )
							{
								if ( i->second->isSelected() ) {
									deselectComponentsOrAll( face );
									++i;
									if ( i != selector.end() ) {
										i->second->setSelected( true );
									}
									else
									{
										selector.begin()->second->setSelected( true );
									}
									cycleSelectionOccured = true;
									break;
								}
							}
							if( !cycleSelectionOccured ){
								deselectComponentsOrAll( face );
								selector.begin()->second->setSelected( true );
							}
						}
						break;
					case EModifier::eSelect:
						{
							SelectionPool_Select( selector, true, mode == eComponent? SELECT_MATCHING_COMPONENTS_DIST : SELECT_MATCHING_DIST );
						}
						break;
					case EModifier::eDeselect:
						{
							if( !( mode == ePrimitive && Mode() == eComponent && countSelected() == 1 ) ) // don't deselect only primitive in component mode
								SelectionPool_Select( selector, false, mode == eComponent? SELECT_MATCHING_COMPONENTS_DIST : SELECT_MATCHING_DIST );
						}
						break;
					default:
						break;
					}
				}
				else if( modifier == eCycle ){
					deselectComponentsOrAll( face );
				}
			}
		}
	}

	RadiantSelectionSystem::EModifier
	SelectPoint_InitPaint( const View& view, const DeviceVector device_point, const DeviceVector device_epsilon, bool face ){
		ASSERT_MESSAGE( std::fabs( device_point[0] ) <= 1 && std::fabs( device_point[1] ) <= 1, "point-selection error" );
#if defined ( DEBUG_SELECTION )
		g_render_clipped.destroy();
#endif

		{
			View scissored( view );
			ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );

			SelectionVolume volume( scissored );
			SelectionPool selector;
			const bool prefer_point_ents = m_bPreferPointEntsIn2D && Mode() == ePrimitive && !view.fill() && !face;

			if( prefer_point_ents && ( Scene_TestSelect( selector, volume, scissored, eEntity, ComponentMode() ), !selector.failed() ) ){
				const bool wasSelected = selector.begin()->second->isSelected();
				SelectionPool_Select( selector, !wasSelected, SELECT_MATCHING_DIST );
				return wasSelected? eDeselect : eSelect;
			}
			else{//do primitives, if ents failed
				const EMode mode = g_modifiers == c_modifierAlt? ePrimitive : Mode();
				if ( face ){
					Scene_TestSelect_Component( selector, volume, scissored, eFace );
				}
				else{
					Scene_TestSelect( selector, volume, scissored, mode, ComponentMode() );
				}
				if ( !selector.failed() ){
					const bool wasSelected = selector.begin()->second->isSelected();
					if( !( mode == ePrimitive && Mode() == eComponent && countSelected() == 1 && wasSelected ) ) // don't deselect only primitive in component mode
						SelectionPool_Select( selector, !wasSelected, mode == eComponent? SELECT_MATCHING_COMPONENTS_DIST : SELECT_MATCHING_DIST );

#if 0
					SelectionPool::iterator best = selector.begin();
					SelectionPool::iterator i = best;
					globalOutputStream() << "\n\n\n===========\n";
					while ( i != selector.end() )
					{
						globalOutputStream() << "depth:" << ( *i ).first.m_depth << " dist:" << ( *i ).first.m_distance << " depth2:" << ( *i ).first.m_depth2 << '\n';
						globalOutputStream() << "depth - best depth:" << ( *i ).first.m_depth - ( *best ).first.m_depth << '\n';
						++i;
					}
#endif

					return wasSelected? eDeselect : eSelect;
				}
				else{
					return eSelect;
				}
			}
		}
	}

	void SelectArea( const View& view, const rect_t rect, bool face ){
#if defined ( DEBUG_SELECTION )
		g_render_clipped.destroy();
#endif
		View scissored( view );
		ConstructSelectionTest( scissored, rect );

		SelectionVolume volume( scissored );
		SelectionPool pool;
		if ( face ) {
			Scene_TestSelect_Component( pool, volume, scissored, eFace );
		}
		else
		{
			Scene_TestSelect( pool, volume, scissored, Mode(), ComponentMode() );
		}

		for ( auto& [ intersection, selectable ] : pool )
		{
			selectable->setSelected( rect.modifier == rect_t::eSelect? true
			                       : rect.modifier == rect_t::eDeselect? false
			                       : !selectable->isSelected() );
		}
	}


	void translate( const Vector3& translation ) override {
		if ( somethingSelected() ) {
			//ASSERT_MESSAGE( !m_pivotChanged, "pivot is invalid" );

			m_translation = translation;
			m_repeatableTransforms.setTranslation( translation );

			m_pivot2world = m_pivot2world_start;
			matrix4_translate_by_vec3( m_pivot2world, translation );

			if ( Mode() == eComponent ) {
				Scene_Translate_Component_Selected( GlobalSceneGraph(), m_translation );
			}
			else
			{
				Scene_Translate_Selected( GlobalSceneGraph(), m_translation );
			}

			SceneChangeNotify();
		}
	}
	void outputTranslation( TextOutputStream& ostream ){
		ostream << " -xyz " << m_translation.x() << ' ' << m_translation.y() << ' ' << m_translation.z();
	}
	void rotate( const Quaternion& rotation ) override {
		if ( somethingSelected() ) {
			//ASSERT_MESSAGE( !m_pivotChanged, "pivot is invalid" );

			m_rotation = rotation;
			m_repeatableTransforms.setRotation( rotation );

			if ( Mode() == eComponent ) {
				Scene_Rotate_Component_Selected( GlobalSceneGraph(), m_rotation, m_pivot2world.t().vec3() );

				matrix4_assign_rotation_for_pivot( m_pivot2world, *m_component_selection.back() );
			}
			else
			{
				Scene_Rotate_Selected( GlobalSceneGraph(), m_rotation, m_pivot2world.t().vec3() );

				matrix4_assign_rotation_for_pivot( m_pivot2world, *m_selection.back() );
			}
#ifdef SELECTIONSYSTEM_AXIAL_PIVOTS
			matrix4_assign_rotation( m_pivot2world, matrix4_rotation_for_quaternion_quantised( m_rotation ) );
#endif

			SceneChangeNotify();
		}
	}
	void outputRotation( TextOutputStream& ostream ){
		ostream << " -eulerXYZ " << m_rotation.x() << ' ' << m_rotation.y() << ' ' << m_rotation.z();
	}
	void scale( const Vector3& scaling ) override {
		if ( somethingSelected() ) {
			m_scale = scaling;
			m_repeatableTransforms.setScale( scaling );

			if ( Mode() == eComponent ) {
				Scene_Scale_Component_Selected( GlobalSceneGraph(), m_scale, m_pivot2world.t().vec3() );
			}
			else
			{
				Scene_Scale_Selected( GlobalSceneGraph(), m_scale, m_pivot2world.t().vec3() );
			}

			if( ManipulatorMode() == eSkew ){
				m_pivot2world[0] = scaling[0];
				m_pivot2world[5] = scaling[1];
				m_pivot2world[10] = scaling[2];
			}

			SceneChangeNotify();
		}
	}
	void outputScale( TextOutputStream& ostream ){
		ostream << " -scale " << m_scale.x() << ' ' << m_scale.y() << ' ' << m_scale.z();
	}

	void skew( const Skew& skew ) override {
		if ( somethingSelected() ) {
			m_skew = skew;
			m_repeatableTransforms.setSkew( skew );

			if ( Mode() == eComponent ) {
				Scene_Skew_Component_Selected( GlobalSceneGraph(), m_skew, m_pivot2world.t().vec3() );
			}
			else
			{
				Scene_Skew_Selected( GlobalSceneGraph(), m_skew, m_pivot2world.t().vec3() );
			}
			m_pivot2world[skew.index] = skew.amount;
			SceneChangeNotify();
		}
	}

	void alltransform( const Transforms& transforms, const Vector3& world_pivot ) override {
		if ( somethingSelected() ) {
			if ( Mode() == eComponent ) {
				GlobalSelectionSystem().foreachSelectedComponent( transform_component_selected( transforms, world_pivot ) );
			}
			else
			{
				GlobalSelectionSystem().foreachSelected( transform_selected( transforms, world_pivot ) );
			}
			SceneChangeNotify();
		}
	}

	void rotateSelected( const Quaternion& rotation, bool snapOrigin = false ) override {
		if( snapOrigin && !m_pivotIsCustom )
			vector3_snap( m_pivot2world.t().vec3(), GetSnapGridSize() );
		startMove();
		rotate( rotation );
		freezeTransforms();
	}
	void translateSelected( const Vector3& translation ) override {
		startMove();
		translate( translation );
		freezeTransforms();
	}
	void scaleSelected( const Vector3& scaling, bool snapOrigin = false ) override {
		if( snapOrigin && !m_pivotIsCustom )
			vector3_snap( m_pivot2world.t().vec3(), GetSnapGridSize() );
		startMove();
		scale( scaling );
		freezeTransforms();
	}

	TransformsObserved m_repeatableTransforms;

	void repeatTransforms() override {
		extern void Scene_Clone_Selected();
		if ( somethingSelected() && !m_repeatableTransforms.isIdentity() ) {
			startMove();
			UndoableCommand undo( "repeatTransforms" );
			if( Mode() == ePrimitive )
				Scene_Clone_Selected();
			alltransform( m_repeatableTransforms, m_pivot2world.t().vec3() );
			freezeTransforms();
		}
	}
	void resetTransforms( EManipulatorMode which ) override {
		const bool all = ( which != eTranslate && which != eRotate && which != eScale && which != eSkew );
		if( which == eTranslate || all )
			m_repeatableTransforms.setTranslation( c_translation_identity );
		if( which == eRotate || all )
			m_repeatableTransforms.setRotation( c_rotation_identity );
		if( which == eScale || all )
			m_repeatableTransforms.setScale( c_scale_identity );
		if( which == eSkew || all )
			m_repeatableTransforms.setSkew( c_skew_identity );
	}

	bool transformOrigin_isTranslatable() const {
		return ManipulatorMode() == eScale
		    || ManipulatorMode() == eSkew
		    || ManipulatorMode() == eRotate
		    || ManipulatorMode() == eTranslate;
	}

	void transformOriginTranslate( const Vector3& translation, const bool set[3] ) override {
		m_pivot2world = m_pivot2world_start;
		setCustomTransformOrigin( translation + m_pivot2world_start.t().vec3(), set );
		SceneChangeNotify();
	}

	void MoveSelected( const View& view, const DeviceVector device_point ){
		if ( m_manipulator->isSelected() ) {
			if ( !m_undo_begun ) {
				m_undo_begun = true;
				GlobalUndoSystem().start();
			}

			Matrix4 device2manip;
			ConstructDevice2Manip( device2manip, m_pivot2world_start, view.GetModelview(), view.GetProjection(), view.GetViewport() );
			m_manipulator->GetManipulatable()->Transform( m_manip2pivot_start, device2manip, device_point );
		}
		else if( m_transformOrigin_manipulator.isSelected() ){
			Matrix4 device2manip;
			ConstructDevice2Manip( device2manip, m_pivot2world_start, view.GetModelview(), view.GetProjection(), view.GetViewport() );
			m_transformOrigin_manipulator.GetManipulatable()->Transform( m_manip2pivot_start, device2manip, device_point );
		}
	}

	/// \todo Support view-dependent nudge.
	void NudgeManipulator( const Vector3& nudge, const Vector3& view ) override {
	//	if ( ManipulatorMode() == eTranslate || ManipulatorMode() == eDrag ) {
		translateSelected( nudge );
	//	}
	}

	bool endMove();
	void freezeTransforms();

	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const override;
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const override {
		renderSolid( renderer, volume );
	}

	const Matrix4& GetPivot2World() const {
		ConstructPivot();
		return m_pivot2world;
	}

	static void constructStatic(){
#if defined( DEBUG_SELECTION )
		g_state_clipped = GlobalShaderCache().capture( "$DEBUG_CLIPPED" );
#endif
		m_state = GlobalShaderCache().capture( "$POINT" );
		TranslateManipulator::m_state_wire =
		RotateManipulator::m_state_outer =
		SkewManipulator::m_state_wire =
		BuildManipulator::m_state_line = GlobalShaderCache().capture( "$WIRE_OVERLAY" );
		TranslateManipulator::m_state_fill =
		SkewManipulator::m_state_fill = GlobalShaderCache().capture( "$FLATSHADE_OVERLAY" );
		TransformOriginManipulator::m_state =
		ClipManipulator::m_state =
		SkewManipulator::m_state_point =
		BuildManipulator::m_state_point =
		UVManipulator::m_state_point = GlobalShaderCache().capture( "$BIGPOINT" );
		RenderablePivot::StaticShader::instance() = GlobalShaderCache().capture( "$PIVOT" );
		UVManipulator::m_state_line = GlobalShaderCache().capture( "$BLENDLINE" );
		DragManipulator::m_state_wire = GlobalShaderCache().capture( "$PLANE_WIRE_OVERLAY" );
	}

	static void destroyStatic(){
#if defined( DEBUG_SELECTION )
		GlobalShaderCache().release( "$DEBUG_CLIPPED" );
#endif
		GlobalShaderCache().release( "$PLANE_WIRE_OVERLAY" );
		GlobalShaderCache().release( "$BLENDLINE" );
		GlobalShaderCache().release( "$PIVOT" );
		GlobalShaderCache().release( "$BIGPOINT" );
		GlobalShaderCache().release( "$FLATSHADE_OVERLAY" );
		GlobalShaderCache().release( "$WIRE_OVERLAY" );
		GlobalShaderCache().release( "$POINT" );
	}
};

Shader* RadiantSelectionSystem::m_state = 0;


namespace
{
RadiantSelectionSystem* g_RadiantSelectionSystem;

inline RadiantSelectionSystem& getSelectionSystem(){
	return *g_RadiantSelectionSystem;
}
}

#include "map.h"

class testselect_entity_visible : public scene::Graph::Walker
{
	Selector& m_selector;
	SelectionTest& m_test;
public:
	testselect_entity_visible( Selector& selector, SelectionTest& test )
		: m_selector( selector ), m_test( test ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if( path.top().get_pointer() == Map_GetWorldspawn( g_map ) ||
		    node_is_group( path.top().get() ) ){
			return false;
		}
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0
		     && Node_isEntity( path.top() ) ) {
			m_selector.pushSelectable( *selectable );
		}

		SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance );
		if ( selectionTestable ) {
			selectionTestable->testSelect( m_selector, m_test );
		}

		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const override {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0
		     && Node_isEntity( path.top() ) ) {
			m_selector.popSelectable();
		}
	}
};

class testselect_primitive_visible : public scene::Graph::Walker
{
	Selector& m_selector;
	SelectionTest& m_test;
public:
	testselect_primitive_visible( Selector& selector, SelectionTest& test )
		: m_selector( selector ), m_test( test ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 ) {
			m_selector.pushSelectable( *selectable );
		}

		SelectionTestable* selectionTestable = Instance_getSelectionTestable( instance );
		if ( selectionTestable ) {
			selectionTestable->testSelect( m_selector, m_test );
		}

		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const override {
		Selectable* selectable = Instance_getSelectable( instance );
		if ( selectable != 0 ) {
			m_selector.popSelectable();
		}
	}
};

class testselect_component_visible : public scene::Graph::Walker
{
	Selector& m_selector;
	SelectionTest& m_test;
	SelectionSystem::EComponentMode m_mode;
public:
	testselect_component_visible( Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode )
		: m_selector( selector ), m_test( test ), m_mode( mode ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
		if ( componentSelectionTestable ) {
			componentSelectionTestable->testSelectComponents( m_selector, m_test, m_mode );
		}

		return true;
	}
};


class testselect_component_visible_selected : public scene::Graph::Walker
{
	Selector& m_selector;
	SelectionTest& m_test;
	SelectionSystem::EComponentMode m_mode;
public:
	testselect_component_visible_selected( Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode )
		: m_selector( selector ), m_test( test ), m_mode( mode ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if ( Instance_isSelected( instance ) ) {
			ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
			if ( componentSelectionTestable ) {
				componentSelectionTestable->testSelectComponents( m_selector, m_test, m_mode );
			}
		}

		return true;
	}
};

void Scene_TestSelect_Primitive( Selector& selector, SelectionTest& test, const VolumeTest& volume ){
	Scene_forEachVisible( GlobalSceneGraph(), volume, testselect_primitive_visible( selector, test ) );
}

void Scene_TestSelect_Component_Selected( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode ){
	Scene_forEachVisible( GlobalSceneGraph(), volume, testselect_component_visible_selected( selector, test, componentMode ) );
}

void Scene_TestSelect_Component( Selector& selector, SelectionTest& test, const VolumeTest& volume, SelectionSystem::EComponentMode componentMode ){
	Scene_forEachVisible( GlobalSceneGraph(), volume, testselect_component_visible( selector, test, componentMode ) );
}

void RadiantSelectionSystem::Scene_TestSelect( Selector& selector, SelectionTest& test, const View& view, SelectionSystem::EMode mode, SelectionSystem::EComponentMode componentMode ){
	switch ( mode )
	{
	case eEntity:
		Scene_forEachVisible( GlobalSceneGraph(), view, testselect_entity_visible( selector, test ) );
		break;
	case ePrimitive:
		Scene_TestSelect_Primitive( selector, test, view );
		break;
	case eComponent:
		Scene_TestSelect_Component_Selected( selector, test, view, componentMode );
		break;
	}
}


void Scene_Intersect( const View& view, const Vector2& device_point, const Vector2& device_epsilon, Vector3& intersection ){
	View scissored( view );
	ConstructSelectionTest( scissored, SelectionBoxForPoint( device_point, device_epsilon ) );
	SelectionVolume test( scissored );

	BestPointSelector bestPointSelector;
	Scene_TestSelect_Primitive( bestPointSelector, test, scissored );

	test.BeginMesh( g_matrix4_identity, true );
	if( bestPointSelector.isSelected() ){
		intersection = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, bestPointSelector.best().depth(), 1 ) ) );
	}
	else{
		const Vector3 pnear = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, -1, 1 ) ) );
		const Vector3 pfar = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, 1, 1 ) ) );
		intersection = vector3_normalised( pfar - pnear ) * 256.f + pnear;
	}
}

class FreezeTransforms : public scene::Graph::Walker
{
public:
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		TransformNode* transformNode = Node_getTransformNode( path.top() );
		if ( transformNode != 0 ) {
			Transformable* transform = Instance_getTransformable( instance );
			if ( transform != 0 ) {
				transform->freezeTransform();
			}
		}
		return true;
	}
};

void RadiantSelectionSystem::freezeTransforms(){
	GlobalSceneGraph().traverse( FreezeTransforms() );
}


bool RadiantSelectionSystem::endMove(){
	if( m_transformOrigin_manipulator.isSelected() ){
		if( m_pivot2world == m_pivot2world_start ){
			m_pivotIsCustom = !m_pivotIsCustom;
			pivotChanged();
		}
		return true;
	}

	if ( ManipulatorMode() == eUV )
		m_uv_manipulator->freezeTransform();
	else
		freezeTransforms();

//	if ( Mode() == ePrimitive && ManipulatorMode() == eDrag ) {
//		g_bTmpComponentMode = false;
//		Scene_SelectAll_Component( false, g_modifiers == c_modifierAlt? SelectionSystem::eVertex : SelectionSystem::eFace );
//	}
	if( g_bTmpComponentMode ){
		g_bTmpComponentMode = false;
		setSelectedAllComponents( false );
	}

	m_pivot_moving = false;
	pivotChanged();

	SceneChangeNotify();

	if ( m_undo_begun ) {
		StringOutputStream command( 64 );

		if ( ManipulatorMode() == eTranslate ) {
			command << "translateTool";
			outputTranslation( command );
		}
		else if ( ManipulatorMode() == eRotate ) {
			command << "rotateTool";
			outputRotation( command );
		}
		else if ( ManipulatorMode() == eScale ) {
			command << "scaleTool";
			outputScale( command );
		}
		else if ( ManipulatorMode() == eSkew ) {
			command << "transformTool";
//			outputScale( command );
		}
		else if ( ManipulatorMode() == eDrag ) {
			command << "dragTool";
		}
		else if ( ManipulatorMode() == eUV ) {
			command << "UVTool";
		}

		GlobalUndoSystem().finish( command );
	}
	return false;
}

class bounds_selected_withEntityBounds : public scene::Graph::Walker
{
	AABB& m_bounds;
public:
	bounds_selected_withEntityBounds( AABB& bounds )
		: m_bounds( bounds ){
		m_bounds = AABB();
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		const auto getBounds = [&]() -> AABB {
			if( Entity* entity = Node_getEntity( path.top() ) ){
				if ( const EntityClass& eclass = entity->getEntityClass(); eclass.fixedsize && !eclass.miscmodel_is ) {
					Editable* editable = Node_getEditable( path.top() );
					const Vector3 origin = editable != 0
						? matrix4_multiplied_by_matrix4( instance.localToWorld(), editable->getLocalPivot() ).t().vec3()
						: instance.localToWorld().t().vec3();
					return aabb_for_minmax( eclass.mins + origin, eclass.maxs + origin );
				}
			}
			return instance.worldAABB();
		};
		if ( Instance_isSelected( instance ) ) {
			aabb_extend_by_aabb_safe( m_bounds, getBounds() );
		}
		return true;
	}
};

inline AABB Instance_getPivotBounds( scene::Instance& instance ){
	Entity* entity = Node_getEntity( instance.path().top() );
	if ( entity != 0 && !entity->getEntityClass().miscmodel_is
	     && ( entity->getEntityClass().fixedsize
	          || !node_is_group( instance.path().top() ) ) ) {
		Editable* editable = Node_getEditable( instance.path().top() );
		if ( editable != 0 ) {
			return AABB( matrix4_multiplied_by_matrix4( instance.localToWorld(), editable->getLocalPivot() ).t().vec3(), Vector3( 0, 0, 0 ) );
		}
		else
		{
			return AABB( instance.localToWorld().t().vec3(), Vector3( 0, 0, 0 ) );
		}
	}

	return instance.worldAABB();
}

class bounds_selected : public scene::Graph::Walker
{
	AABB& m_bounds;
public:
	bounds_selected( AABB& bounds )
		: m_bounds( bounds ){
		m_bounds = AABB();
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if ( Instance_isSelected( instance ) ) {
			aabb_extend_by_aabb_safe( m_bounds, Instance_getPivotBounds( instance ) );
		}
		return true;
	}
};

class bounds_selected_component : public scene::Graph::Walker
{
	AABB& m_bounds;
public:
	bounds_selected_component( AABB& bounds )
		: m_bounds( bounds ){
		m_bounds = AABB();
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if ( Instance_isSelected( instance ) ) {
			ComponentEditable* componentEditable = Instance_getComponentEditable( instance );
			if ( componentEditable ) {
				aabb_extend_by_aabb_safe( m_bounds, aabb_for_oriented_aabb_safe( componentEditable->getSelectedComponentsBounds(), instance.localToWorld() ) );
			}
		}
		return true;
	}
};

void Scene_BoundsSelected_withEntityBounds( scene::Graph& graph, AABB& bounds ){
	graph.traverse( bounds_selected_withEntityBounds( bounds ) );
}

void Scene_BoundsSelected( scene::Graph& graph, AABB& bounds ){
	graph.traverse( bounds_selected( bounds ) );
}

void Scene_BoundsSelectedComponent( scene::Graph& graph, AABB& bounds ){
	graph.traverse( bounds_selected_component( bounds ) );
}

#if 0
inline void pivot_for_node( Matrix4& pivot, scene::Node& node, scene::Instance& instance ){
	ComponentEditable* componentEditable = Instance_getComponentEditable( instance );
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent
	     && componentEditable != 0 ) {
		pivot = matrix4_translation_for_vec3( componentEditable->getSelectedComponentsBounds().origin );
	}
	else
	{
		Bounded* bounded = Instance_getBounded( instance );
		if ( bounded != 0 ) {
			pivot = matrix4_translation_for_vec3( bounded->localAABB().origin );
		}
		else
		{
			pivot = g_matrix4_identity;
		}
	}
}
#endif

void RadiantSelectionSystem::ConstructPivotRotation() const {
	switch ( m_manipulator_mode )
	{
	case eTranslate:
		break;
	case eRotate:
		if ( Mode() == eComponent ) {
			matrix4_assign_rotation_for_pivot( m_pivot2world, *m_component_selection.back() );
		}
		else
		{
			matrix4_assign_rotation_for_pivot( m_pivot2world, *m_selection.back() );
		}
		break;
	case eScale:
		if ( Mode() == eComponent ) {
			matrix4_assign_rotation_for_pivot( m_pivot2world, *m_component_selection.back() );
		}
		else
		{
			matrix4_assign_rotation_for_pivot( m_pivot2world, *m_selection.back() );
		}
		break;
	default:
		break;
	}
}

void RadiantSelectionSystem::ConstructPivot() const {
	if ( !m_pivotChanged || m_pivot_moving ) {
		return;
	}
	m_pivotChanged = false;

	if ( somethingSelected() ) {
		m_bounds = getSelectionAABB();
		if( !m_pivotIsCustom ){
			Vector3 object_pivot = m_bounds.origin;

			//vector3_snap( object_pivot, GetSnapGridSize() );
			//globalOutputStream() << object_pivot << '\n';
			m_pivot2world = matrix4_translation_for_vec3( object_pivot );
		}
		else{
//			m_pivot2world = matrix4_translation_for_vec3( m_pivot2world.t().vec3() );
			matrix4_assign_rotation( m_pivot2world, g_matrix4_identity );
		}

		ConstructPivotRotation();
	}
}

void RadiantSelectionSystem::setCustomTransformOrigin( const Vector3& origin, const bool set[3] ) const {
	if ( somethingSelected() && transformOrigin_isTranslatable() ) {

		//globalOutputStream() << origin << '\n';
		for( std::size_t i = 0; i < 3; ++i ){
			float value = origin[i];
			if( set[i] ){
				float bestsnapDist = std::fabs( m_bounds.origin[i] - value );
				float bestsnapTo = m_bounds.origin[i];
				float othersnapDist = std::fabs( m_bounds.origin[i] + m_bounds.extents[i] - value );
				if( othersnapDist < bestsnapDist ){
					bestsnapDist = othersnapDist;
					bestsnapTo = m_bounds.origin[i] + m_bounds.extents[i];
				}
				othersnapDist = std::fabs( m_bounds.origin[i] - m_bounds.extents[i] - value );
				if( othersnapDist < bestsnapDist ){
					bestsnapDist = othersnapDist;
					bestsnapTo = m_bounds.origin[i] - m_bounds.extents[i];
				}
				othersnapDist = std::fabs( float_snapped( value, GetSnapGridSize() ) - value );
				if( othersnapDist < bestsnapDist ){
					bestsnapDist = othersnapDist;
					bestsnapTo = float_snapped( value, GetSnapGridSize() );
				}
				value = bestsnapTo;

				m_pivot2world[i + 12] = value; //m_pivot2world.tx() .ty() .tz()
			}
		}
		m_pivotIsCustom = true;

		ConstructPivotRotation();
	}
}

AABB RadiantSelectionSystem::getSelectionAABB() const {
	AABB bounds;
	if ( somethingSelected() ) {
		if ( Mode() == eComponent || g_bTmpComponentMode ) {
			Scene_BoundsSelectedComponent( GlobalSceneGraph(), bounds );
			if( !aabb_valid( bounds ) ) /* selecting PlaneSelectables sets g_bTmpComponentMode, but only brushes return correct componentEditable->getSelectedComponentsBounds() */
				bounds = getBoundsSelected();
		}
		else
		{
			bounds = getBoundsSelected();
		}
	}
	return bounds;
}

void RadiantSelectionSystem::renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
	//if( view->TestPoint( m_object_pivot ) )
	if ( somethingSelected()
	     || ManipulatorMode() == eClip
	     || ManipulatorMode() == eBuild
	     || ManipulatorMode() == eUV
	     || ManipulatorMode() == eDrag ) {
		renderer.Highlight( Renderer::ePrimitive, false );
		renderer.Highlight( Renderer::eFace, false );

		renderer.SetState( m_state, Renderer::eWireframeOnly );
		renderer.SetState( m_state, Renderer::eFullMaterials );

		if( transformOrigin_isTranslatable() )
			m_transformOrigin_manipulator.render( renderer, volume, GetPivot2World() );

		m_manipulator->render( renderer, volume, GetPivot2World() );
	}

#if defined( DEBUG_SELECTION )
	renderer.SetState( g_state_clipped, Renderer::eWireframeOnly );
	renderer.SetState( g_state_clipped, Renderer::eFullMaterials );
	renderer.addRenderable( g_render_clipped, g_render_clipped.m_world );
#endif
}

#include "preferencesystem.h"
#include "preferences.h"

void SelectionSystem_constructPreferences( PreferencesPage& page ){
	page.appendSpinner( "Selector size (pixels)", g_SELECT_EPSILON, 2, 64 );
	page.appendCheckBox( "", "Prefer point entities in 2D", getSelectionSystem().m_bPreferPointEntsIn2D );
	page.appendCheckBox( "", "Create brushes in 3D", g_3DCreateBrushes );
	{
		const char* styles[] = { "XY plane + Z with Alt", "View plane + Forward with Alt", };
		page.appendCombo(
		    "Move style in 3D",
		    StringArrayRange( styles ),
		    IntImportCaller( TranslateFreeXY_Z::m_viewdependent ),
		    IntExportCaller( TranslateFreeXY_Z::m_viewdependent )
		);
	}
}
void SelectionSystem_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Selection", "Selection System Settings" ) );
	SelectionSystem_constructPreferences( page );
}
void SelectionSystem_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF( SelectionSystem_constructPage ) );
}


void SelectionSystem_connectTransformsCallbacks( const std::array<Callback<void(const char*)>, 4>& callbacks ){
	getSelectionSystem().m_repeatableTransforms.m_changedCallbacks = callbacks;
}


void SelectionSystem_OnBoundsChanged(){
	getSelectionSystem().pivotChanged();
}

SignalHandlerId SelectionSystem_boundsChanged;

void SelectionSystem_Construct(){
	RadiantSelectionSystem::constructStatic();

	g_RadiantSelectionSystem = new RadiantSelectionSystem;

	SelectionSystem_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback( FreeCaller<void(), SelectionSystem_OnBoundsChanged>() );

	GlobalShaderCache().attachRenderable( getSelectionSystem() );

	GlobalPreferenceSystem().registerPreference( "SELECT_EPSILON", IntImportStringCaller( g_SELECT_EPSILON ), IntExportStringCaller( g_SELECT_EPSILON ) );
	GlobalPreferenceSystem().registerPreference( "PreferPointEntsIn2D", BoolImportStringCaller( getSelectionSystem().m_bPreferPointEntsIn2D ), BoolExportStringCaller( getSelectionSystem().m_bPreferPointEntsIn2D ) );
	GlobalPreferenceSystem().registerPreference( "3DCreateBrushes", BoolImportStringCaller( g_3DCreateBrushes ), BoolExportStringCaller( g_3DCreateBrushes ) );
	GlobalPreferenceSystem().registerPreference( "3DMoveStyle", IntImportStringCaller( TranslateFreeXY_Z::m_viewdependent ), IntExportStringCaller( TranslateFreeXY_Z::m_viewdependent ) );
	SelectionSystem_registerPreferencesPage();
}

void SelectionSystem_Destroy(){
	GlobalShaderCache().detachRenderable( getSelectionSystem() );

	GlobalSceneGraph().removeBoundsChangedCallback( SelectionSystem_boundsChanged );

	delete g_RadiantSelectionSystem;

	RadiantSelectionSystem::destroyStatic();
}




inline float screen_normalised( float pos, std::size_t size ){
	return ( ( 2.0f * pos ) / size ) - 1.0f;
}

inline DeviceVector window_to_normalised_device( WindowVector window, std::size_t width, std::size_t height ){
	return DeviceVector( screen_normalised( window.x(), width ), screen_normalised( height - 1 - window.y(), height ) );
}

inline float device_constrained( float pos ){
	return std::clamp( pos, -1.0f, 1.0f );
}

inline DeviceVector device_constrained( DeviceVector device ){
	return DeviceVector( device_constrained( device.x() ), device_constrained( device.y() ) );
}

inline float window_constrained( float pos, std::size_t origin, std::size_t size ){
	return std::clamp( pos, static_cast<float>( origin ), static_cast<float>( origin + size ) );
}

inline WindowVector window_constrained( WindowVector window, std::size_t x, std::size_t y, std::size_t width, std::size_t height ){
	return WindowVector( window_constrained( window.x(), x, width ), window_constrained( window.y(), y, height ) );
}

typedef Callback<void(DeviceVector)> MouseEventCallback;

Single<MouseEventCallback> g_mouseMovedCallback;
Single<MouseEventCallback> g_mouseUpCallback;

#if 1
const ButtonIdentifier c_button_select = c_buttonLeft;
const ButtonIdentifier c_button_select2 = c_buttonRight;
const ModifierFlags c_modifier_manipulator = c_modifierNone;
const ModifierFlags c_modifier_toggle = c_modifierShift;
const ModifierFlags c_modifier_replace = c_modifierShift | c_modifierAlt;
const ModifierFlags c_modifier_face = c_modifierControl;
#else
const ButtonIdentifier c_button_select = c_buttonLeft;
const ModifierFlags c_modifier_manipulator = c_modifierNone;
const ModifierFlags c_modifier_toggle = c_modifierControl;
const ModifierFlags c_modifier_replace = c_modifierNone;
const ModifierFlags c_modifier_face = c_modifierShift;
#endif
const ModifierFlags c_modifier_toggle_face = c_modifier_toggle | c_modifier_face;
const ModifierFlags c_modifier_replace_face = c_modifier_replace | c_modifier_face;

const ButtonIdentifier c_button_texture = c_buttonMiddle;
const ModifierFlags c_modifier_apply_texture1_project = c_modifierControl | c_modifierShift;
const ModifierFlags c_modifier_apply_texture2_seamless = c_modifierControl;
const ModifierFlags c_modifier_apply_texture3 =                     c_modifierShift;
const ModifierFlags c_modifier_copy_texture = c_modifierNone;



void Scene_copyClosestTexture( SelectionTest& test );
void Scene_applyClosestTexture( SelectionTest& test, bool shift, bool ctrl, bool alt, bool texturize_selected = false );
const char* Scene_applyClosestTexture_getUndoName( bool shift, bool ctrl, bool alt );

class TexManipulator_
{
	const DeviceVector& m_epsilon;
public:
	const View* m_view;
	bool m_undo_begun;

	TexManipulator_( const DeviceVector& epsilon ) :
		m_epsilon( epsilon ),
		m_undo_begun( false ){
	}

	void mouseDown( DeviceVector position ){
		View scissored( *m_view );
		ConstructSelectionTest( scissored, SelectionBoxForPoint( position, m_epsilon ) );
		SelectionVolume volume( scissored );

		if( g_modifiers == c_modifier_copy_texture ) {
			Scene_copyClosestTexture( volume );
		}
		else{
			m_undo_begun = true;
			GlobalUndoSystem().start();
			Scene_applyClosestTexture( volume, g_modifiers.shift(), g_modifiers.ctrl(), g_modifiers.alt(), true );
		}
	}

	void mouseMoved( DeviceVector position ){
		if( m_undo_begun ){
			View scissored( *m_view );
			ConstructSelectionTest( scissored, SelectionBoxForPoint( device_constrained( position ), m_epsilon ) );
			SelectionVolume volume( scissored );

			Scene_applyClosestTexture( volume, g_modifiers.shift(), g_modifiers.ctrl(), g_modifiers.alt() );
		}
	}
	typedef MemberCaller<TexManipulator_, void(DeviceVector), &TexManipulator_::mouseMoved> MouseMovedCaller;

	void mouseUp( DeviceVector position ){
		if( m_undo_begun ){
			GlobalUndoSystem().finish( Scene_applyClosestTexture_getUndoName( g_modifiers.shift(), g_modifiers.ctrl(), g_modifiers.alt() ) );
			m_undo_begun = false;
		}
	}
	typedef MemberCaller<TexManipulator_, void(DeviceVector), &TexManipulator_::mouseUp> MouseUpCaller;
};


class Selector_
{
	bool m1selecting() const {
		return !m_mouse2 && ( g_modifiers == c_modifier_toggle || g_modifiers == c_modifier_face
		|| ( g_modifiers == c_modifierAlt && getSelectionSystem().Mode() == SelectionSystem::eComponent ) ); // select primitives in component mode
	}
	bool m2selecting() const {
		return m_mouse2 && ( g_modifiers == c_modifier_toggle || g_modifiers == c_modifier_face );
	}

	RadiantSelectionSystem::EModifier modifier_for_mouseMoved() const {
		return m_mouseMoved
		       ? RadiantSelectionSystem::eReplace
	           : RadiantSelectionSystem::eCycle;
	}
	RadiantSelectionSystem::EModifier modifier_for_state() const {
		return m2selecting()
		       ? modifier_for_mouseMoved()
		       : RadiantSelectionSystem::eManipulator;
	}

	rect_t getDeviceArea() const {
		const DeviceVector delta( m_current - m_start );
		if ( m_mouseMovedWhilePressed && m2selecting() && delta.x() != 0 && delta.y() != 0 )
			return SelectionBoxForArea( m_start, delta );
		else
			return rect_t();
	}

	void draw_area(){
		m_window_update( getDeviceArea() );
	}

	void m2testSelect( DeviceVector position ){
		const RadiantSelectionSystem::EModifier modifier = modifier_for_state();
		if ( modifier != RadiantSelectionSystem::eManipulator ) {
			const DeviceVector delta( position - m_start );
			if ( m_mouseMovedWhilePressed ) {
				if( delta.x() != 0 && delta.y() != 0 )
					getSelectionSystem().SelectArea( *m_view, SelectionBoxForArea( m_start, delta ), g_modifiers == c_modifier_face );
			}
			else{
				getSelectionSystem().SelectPoint( *m_view, position, m_epsilon, modifier, g_modifiers == c_modifier_face );
			}
		}

		m_start = m_current = DeviceVector( 0, 0 );
		draw_area();
	}

	const DeviceVector& m_epsilon;
public:
	DeviceVector m_start;
	DeviceVector m_current;
	bool m_mouse2;
	bool m_mouseMoved;
	bool m_mouseMovedWhilePressed;
	RadiantSelectionSystem::EModifier m_paintMode;
	const View* m_view;
	RectangleCallback m_window_update;

	Selector_( const DeviceVector& epsilon ) :
		m_epsilon( epsilon ),
		m_start( 0, 0 ),
		m_current( 0, 0 ),
		m_mouse2( false ),
		m_mouseMoved( false ),
		m_mouseMovedWhilePressed( false ){
	}

	void testSelect_simpleM1( DeviceVector position ){
		getSelectionSystem().SelectPoint( *m_view, device_constrained( position ), m_epsilon, modifier_for_mouseMoved(), false );
	}

	void mouseDown( DeviceVector position ){
		m_start = m_current = device_constrained( position );
		m_paintMode = RadiantSelectionSystem::eSelect;
		if( m1selecting() ){
			m_paintMode = getSelectionSystem().SelectPoint_InitPaint( *m_view, position, m_epsilon, g_modifiers == c_modifier_face );
		}
	}

	void mouseMoved( DeviceVector position ){
		m_current = device_constrained( position );
		if( m_mouse2 ){
			draw_area();
		}
		else if( m1selecting() ){
			getSelectionSystem().SelectPoint( *m_view, m_current, m_epsilon, m_paintMode, g_modifiers == c_modifier_face );
		}
	}
	typedef MemberCaller<Selector_, void(DeviceVector), &Selector_::mouseMoved> MouseMovedCaller;

	void mouseUp( DeviceVector position ){
		if( m_mouse2 ){
			m2testSelect( device_constrained( position ) );
		}
		else{
			m_start = m_current = DeviceVector( 0, 0 );
		}
	}
	typedef MemberCaller<Selector_, void(DeviceVector), &Selector_::mouseUp> MouseUpCaller;
};


class Manipulator_
{
	DeviceVector getEpsilon() const {
		switch ( getSelectionSystem().ManipulatorMode() )
		{
		case SelectionSystem::eClip:
			return m_epsilon / g_SELECT_EPSILON * ( g_SELECT_EPSILON + 4 );
		case SelectionSystem::eDrag:
		case SelectionSystem::eUV:
			return m_epsilon;
		default: //getSelectionSystem().transformOrigin_isTranslatable()
			return m_epsilon / g_SELECT_EPSILON * 8;
		}
	}
	const DeviceVector& m_epsilon;

public:
	const View* m_view;

	bool m_moving_transformOrigin;
	bool m_mouseMovedWhilePressed;

	Manipulator_( const DeviceVector& epsilon ) :
		m_epsilon( epsilon ),
		m_moving_transformOrigin( false ),
		m_mouseMovedWhilePressed( false ) {
	}

	bool mouseDown( DeviceVector position ){
		if( getSelectionSystem().ManipulatorMode() == SelectionSystem::eClip )
			Clipper_tryDoubleclick(); // this b4 SelectManipulator() to track that latest click added no points (hence 2x click one point)
		return getSelectionSystem().SelectManipulator( *m_view, position, getEpsilon() );
	}

	void mouseMoved( DeviceVector position ){
		if( m_mouseMovedWhilePressed )
			getSelectionSystem().MoveSelected( *m_view, position );
	}
	typedef MemberCaller<Manipulator_, void(DeviceVector), &Manipulator_::mouseMoved> MouseMovedCaller;

	void mouseUp( DeviceVector position ){
		m_moving_transformOrigin = getSelectionSystem().endMove();
	}
	typedef MemberCaller<Manipulator_, void(DeviceVector), &Manipulator_::mouseUp> MouseUpCaller;

	void highlight( DeviceVector position ){
		getSelectionSystem().HighlightManipulator( *m_view, position, getEpsilon() );
	}
};




class RadiantWindowObserver final : public SelectionSystemWindowObserver
{
	DeviceVector m_epsilon;

	int m_width;
	int m_height;

	bool m_mouse_down;

	const float m_moveEpsilon;
	float m_move; /* released move after m_moveEnd, for tunnel selector decision: eReplace or eCycle */
	float m_movePressed; /* pressed move after m_moveStart, for decision: m1 tunnel selector or manipulate and if to do tunnel selector at all */
	DeviceVector m_moveStart;
	DeviceVector m_moveEnd;
	// track latest onMouseMotion() interaction to trigger update onModifierDown(), onModifierUp()
	inline static RadiantWindowObserver *m_latestObserver = nullptr;
	inline static WindowVector m_latestPosition;

	Selector_ m_selector;
	Manipulator_ m_manipulator;
	TexManipulator_ m_texmanipulator;
public:

	RadiantWindowObserver() :
		m_mouse_down( false ),
		m_moveEpsilon( .01f ),
		m_selector( m_epsilon ),
		m_manipulator( m_epsilon ),
		m_texmanipulator( m_epsilon ){
	}
	~RadiantWindowObserver(){
		m_latestObserver = nullptr;
	}
	void release() override {
		delete this;
	}
	void setView( const View& view ) override {
		m_selector.m_view = &view;
		m_manipulator.m_view = &view;
		m_texmanipulator.m_view = &view;
	}
	void setRectangleDrawCallback( const RectangleCallback& callback ) override {
		m_selector.m_window_update = callback;
	}
	void updateEpsilon(){
		m_epsilon = DeviceVector( g_SELECT_EPSILON / static_cast<float>( m_width ), g_SELECT_EPSILON / static_cast<float>( m_height ) );
	}
	void onSizeChanged( int width, int height ) override {
		m_width = width;
		m_height = height;
		updateEpsilon();
	}
	void onMouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ) override {
		updateEpsilon(); /* could have changed, as it is user setting */

		if( m_mouse_down ) return; /* prevent simultaneous mouse presses */

		const DeviceVector devicePosition( device( position ) );

		if ( button == c_button_select || ( button == c_button_select2 && modifiers != c_modifierNone ) ) {
			m_mouse_down = true;

			const bool clipper2d( button == c_button_select && ClipManipulator::quickCondition( modifiers, *m_manipulator.m_view ) );
			if( clipper2d && getSelectionSystem().ManipulatorMode() != SelectionSystem::eClip )
				ClipperModeQuick();

			if ( button == c_button_select && m_manipulator.mouseDown( devicePosition ) ) {
				g_mouseMovedCallback.insert( MouseEventCallback( Manipulator_::MouseMovedCaller( m_manipulator ) ) );
				g_mouseUpCallback.insert( MouseEventCallback( Manipulator_::MouseUpCaller( m_manipulator ) ) );
			}
			else
			{
				m_selector.m_mouse2 = ( button == c_button_select2 );
				m_selector.mouseDown( devicePosition );
				g_mouseMovedCallback.insert( MouseEventCallback( Selector_::MouseMovedCaller( m_selector ) ) );
				g_mouseUpCallback.insert( MouseEventCallback( Selector_::MouseUpCaller( m_selector ) ) );
			}
		}
		else if ( button == c_button_texture ) {
			m_mouse_down = true;
			m_texmanipulator.mouseDown( devicePosition );
			g_mouseMovedCallback.insert( MouseEventCallback( TexManipulator_::MouseMovedCaller( m_texmanipulator ) ) );
			g_mouseUpCallback.insert( MouseEventCallback( TexManipulator_::MouseUpCaller( m_texmanipulator ) ) );
		}

		m_moveStart = devicePosition;
		m_movePressed = 0;
	}
	void onMouseMotion( const WindowVector& position, ModifierFlags modifiers ) override {
		m_selector.m_mouseMoved = mouse_moved_epsilon( position, m_moveEnd, m_move );
		if ( m_mouse_down && !g_mouseMovedCallback.empty() ) {
			m_manipulator.m_mouseMovedWhilePressed = m_selector.m_mouseMovedWhilePressed = mouse_moved_epsilon( position, m_moveStart, m_movePressed );
			g_mouseMovedCallback.get() ( device( position ) );
		}
		else{
			m_manipulator.highlight( device( position ) );
		}
		m_latestObserver = this;
		m_latestPosition = position;
	}
	void onMouseUp( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ) override {
		if ( button != c_buttonInvalid && !g_mouseUpCallback.empty() ) {
			g_mouseUpCallback.get() ( device( position ) );
			g_mouseMovedCallback.clear();
			g_mouseUpCallback.clear();
		}
		if( button == c_button_select	/* L button w/o mouse moved = tunnel selection */
		 && modifiers == c_modifierNone
		 && !m_selector.m_mouseMovedWhilePressed
		 && !m_manipulator.m_moving_transformOrigin
		 && !( getSelectionSystem().Mode() == SelectionSystem::eComponent && getSelectionSystem().ManipulatorMode() == SelectionSystem::eDrag )
		 && getSelectionSystem().ManipulatorMode() != SelectionSystem::eClip
		 && getSelectionSystem().ManipulatorMode() != SelectionSystem::eBuild ){
			m_selector.testSelect_simpleM1( device( position ) );
		}
		if( getSelectionSystem().ManipulatorMode() == SelectionSystem::eClip
		&& button == c_button_select && ( modifiers == c_modifierNone || ClipManipulator::quickCondition( modifiers, *m_manipulator.m_view ) ) )
			Clipper_tryDoubleclickedCut();

		m_mouse_down = false; /* unconditionally drop the flag to surely not lock the onMouseDown() */
		m_manipulator.m_moving_transformOrigin = false;
		m_selector.m_mouseMoved = false;
		m_selector.m_mouseMovedWhilePressed = false;
		m_manipulator.m_mouseMovedWhilePressed = false;
		m_moveEnd = device( position );
		m_move = 0;
	}
	void onModifierDown( ModifierFlags type ) override {
		g_modifiers = bitfield_enable( g_modifiers, type );
		if( this == m_latestObserver )
			onMouseMotion( m_latestPosition, g_modifiers );
	}
	void onModifierUp( ModifierFlags type ) override {
		g_modifiers = bitfield_disable( g_modifiers, type );
		if( this == m_latestObserver )
			onMouseMotion( m_latestPosition, g_modifiers );
	}
	DeviceVector device( WindowVector window ) const {
		return window_to_normalised_device( window, m_width, m_height );
	}
	bool mouse_moved_epsilon( const WindowVector& position, const DeviceVector& moveStart, float& move ){
		if( move > m_moveEpsilon )
			return true;
		const DeviceVector devicePosition( device( position ) );
		const float currentMove = std::max( std::fabs( devicePosition.x() - moveStart.x() ), std::fabs( devicePosition.y() - moveStart.y() ) );
		move = std::max( move, currentMove );
	//	globalOutputStream() << move << " move\n";
		return move > m_moveEpsilon;
	}
	/* support mouse_moved_epsilon with frozen pointer (camera freelook) */
	void incMouseMove( const WindowVector& delta ) override {
		const WindowVector normalized_delta( delta.x() * 2.f / m_width, delta.y() * 2.f / m_height );
		m_moveEnd -= normalized_delta;
		if( m_mouse_down )
			m_moveStart -= normalized_delta;
	}
};



SelectionSystemWindowObserver* NewWindowObserver(){
	return new RadiantWindowObserver;
}



#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

class SelectionDependencies :
	public GlobalSceneGraphModuleRef,
	public GlobalShaderCacheModuleRef,
	public GlobalOpenGLModuleRef
{
};

class SelectionAPI : public TypeSystemRef
{
	SelectionSystem* m_selection;
public:
	typedef SelectionSystem Type;
	STRING_CONSTANT( Name, "*" );

	SelectionAPI(){
		SelectionSystem_Construct();

		m_selection = &getSelectionSystem();
	}
	~SelectionAPI(){
		SelectionSystem_Destroy();
	}
	SelectionSystem* getTable(){
		return m_selection;
	}
};

typedef SingletonModule<SelectionAPI, SelectionDependencies> SelectionModule;
typedef Static<SelectionModule> StaticSelectionModule;
StaticRegisterModule staticRegisterSelection( StaticSelectionModule::instance() );

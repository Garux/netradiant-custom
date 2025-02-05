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

#pragma once

#include "generic/constant.h"
#include "math/matrix.h"
#include "math/quaternion.h"


/// \brief A transform node.
class TransformNode
{
public:
	STRING_CONSTANT( Name, "TransformNode" );
	/// \brief Returns the transform which maps the node's local-space into the local-space of its parent node.
	virtual const Matrix4& localToParent() const  = 0;
};

/// \brief A transform node which has no effect.
class IdentityTransform : public TransformNode
{
public:
	/// \brief Returns the identity matrix.
	const Matrix4& localToParent() const {
		return g_matrix4_identity;
	}
};

/// \brief A transform node which stores a generic transformation matrix.
class MatrixTransform : public TransformNode
{
	Matrix4 m_localToParent;
public:
	MatrixTransform() : m_localToParent( g_matrix4_identity ){
	}

	Matrix4& localToParent(){
		return m_localToParent;
	}
	/// \brief Returns the stored local->parent transform.
	const Matrix4& localToParent() const {
		return m_localToParent;
	}
};


#include "generic/callback.h"

typedef Vector3 Translation;
typedef Quaternion Rotation;
typedef Vector3 Scale;

//simple one axis skew
/// [0]     [4]x(y)  [8]x(z) [12]
/// [1]y(x) [5]      [9]y(z) [13]
/// [2]z(x) [6]z(y)  [10]    [14]
/// [3]     [7]      [11]    [15]
struct Skew{
	std::size_t index;
	float amount;
	Skew(){
	}
	Skew( std::size_t index_, float amount_ ) : index( index_ ), amount( amount_ ){
	}
	bool operator!= ( const Skew& other ) const {
		return !( *this == other );
	}
	bool operator== ( const Skew& other ) const {
		return ( amount == 0 && other.amount == 0 ) || ( index == other.index && amount == other.amount );
	}
};


inline Matrix4 matrix4_transform_for_components( const Translation& translation, const Rotation& rotation, const Scale& scale, const Skew& skew ){
	Matrix4 result( matrix4_rotation_for_quaternion_quantised( rotation ) );
	result[skew.index] += skew.amount;
	result.x().vec3() *= scale.x();
	result.y().vec3() *= scale.y();
	result.z().vec3() *= scale.z();
	result.tx() = translation.x();
	result.ty() = translation.y();
	result.tz() = translation.z();
	return result;
}

typedef bool TransformModifierType;
const TransformModifierType TRANSFORM_PRIMITIVE = false;
const TransformModifierType TRANSFORM_COMPONENT = true;

/// \brief A transformable scene-graph instance.
///
/// A transformable instance may be translated, rotated, scaled or skewed.
/// The state of the instanced node's geometrical representation
/// will be the product of its geometry and the transforms of each
/// of its instances, applied in the order they appear in a graph
/// traversal.
/// Freezing the transform on an instance will cause its transform
/// to be permanently applied to the geometry of the node.
class Transformable
{
public:
	STRING_CONSTANT( Name, "Transformable" );

	virtual void setType( TransformModifierType type ) = 0;
	virtual void setTranslation( const Translation& value ) = 0;
	virtual void setRotation( const Rotation& value ) = 0;
	virtual void setScale( const Scale& value ) = 0;
	virtual void setSkew( const Skew& value ) = 0;
	virtual void freezeTransform() = 0;
};

const Translation c_translation_identity = Translation( 0, 0, 0 );
const Rotation c_rotation_identity = c_quaternion_identity;
const Scale c_scale_identity = Scale( 1, 1, 1 );
const Skew c_skew_identity = Skew( 4, 0 );


class Transforms
{
protected:
	Translation m_translation;
	Rotation m_rotation;
	Scale m_scale;
	Skew m_skew;
public:
	Transforms() :
		m_translation( c_translation_identity ),
		m_rotation( c_quaternion_identity ),
		m_scale( c_scale_identity ),
		m_skew( c_skew_identity ){
	}

	bool isIdentity() const {
		return m_translation == c_translation_identity
		       && m_rotation == c_rotation_identity
		       && m_scale == c_scale_identity
		       && m_skew == c_skew_identity;
	}
	void setIdentity(){
		*this = Transforms();
	}

	const Translation& getTranslation() const {
		return m_translation;
	}
	const Rotation& getRotation() const {
		return m_rotation;
	}
	const Scale& getScale() const {
		return m_scale;
	}
	const Skew& getSkew() const {
		return m_skew;
	}
	Matrix4 calculateTransform() const {
		return matrix4_transform_for_components( getTranslation(), getRotation(), getScale(), getSkew() );
	}

	void setTranslation( const Translation& value ){
		m_translation = value;
	}
	void setRotation( const Rotation& value ){
		m_rotation = value;
	}
	void setScale( const Scale& value ){
		m_scale = value;
	}
	void setSkew( const Skew& value ){
		m_skew = value;
	}
};


class TransformModifier : public Transforms, public Transformable
{
protected:
	Callback<void()> m_changed;
	Callback<void()> m_apply;
	TransformModifierType m_type;
public:
	TransformModifier( const Callback<void()>& changed, const Callback<void()>& apply ) :
		m_changed( changed ),
		m_apply( apply ),
		m_type( TRANSFORM_PRIMITIVE ){
	}
	void setType( TransformModifierType type ) override {
		m_type = type;
	}
	TransformModifierType getType() const {
		return m_type;
	}
	void setTranslation( const Translation& value ) override {
		m_translation = value;
		m_changed();
	}
	void setRotation( const Rotation& value ) override {
		m_rotation = value;
		m_changed();
	}
	void setScale( const Scale& value ) override {
		m_scale = value;
		m_changed();
	}
	void setSkew( const Skew& value ) override {
		m_skew = value;
		m_changed();
	}
	void freezeTransform() override {
		if ( !isIdentity() ) {
			m_apply();
			setIdentity();
			m_changed();
		}
	}
};

// modification intended for more controllable freezeTransform() implementation via direct isIdentity(), setIdentity() accesses
class BrushTransformModifier : public TransformModifier
{
	using TransformModifier::TransformModifier;
public:
	bool m_transformFrozen = true;

	void freezeTransform() override {
		m_apply();
		m_transformFrozen = true;
	}
};

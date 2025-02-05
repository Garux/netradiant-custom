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

#include "ireference.h"
#include "debugging/debugging.h"

#include "ientity.h"
#include "irender.h"
#include "igl.h"
#include "selectable.h"

#include "generic/callback.h"
#include "math/vector.h"
#include "math/aabb.h"
#include "undolib.h"
#include "string/pooledstring.h"
#include "generic/referencecounted.h"
#include "scenelib.h"
#include "container/container.h"
#include "eclasslib.h"

#include <list>
#include <set>
#include <optional>

inline void arrow_draw( const Vector3& origin, const Vector3& direction_forward, const Vector3& direction_left, const Vector3& direction_up ){
	Vector3 endpoint( vector3_added( origin, vector3_scaled( direction_forward, 32.0 ) ) );

	Vector3 tip1( vector3_added( vector3_added( endpoint, vector3_scaled( direction_forward, -8.0 ) ), vector3_scaled( direction_up, -4.0 ) ) );
	Vector3 tip2( vector3_added( tip1, vector3_scaled( direction_up, 8.0 ) ) );
	Vector3 tip3( vector3_added( vector3_added( endpoint, vector3_scaled( direction_forward, -8.0 ) ), vector3_scaled( direction_left, -4.0 ) ) );
	Vector3 tip4( vector3_added( tip3, vector3_scaled( direction_left, 8.0 ) ) );

	gl().glBegin( GL_LINES );

	gl().glVertex3fv( vector3_to_array( origin ) );
	gl().glVertex3fv( vector3_to_array( endpoint ) );

	gl().glVertex3fv( vector3_to_array( endpoint ) );
	gl().glVertex3fv( vector3_to_array( tip1 ) );

	gl().glVertex3fv( vector3_to_array( endpoint ) );
	gl().glVertex3fv( vector3_to_array( tip2 ) );

	gl().glVertex3fv( vector3_to_array( endpoint ) );
	gl().glVertex3fv( vector3_to_array( tip3 ) );

	gl().glVertex3fv( vector3_to_array( endpoint ) );
	gl().glVertex3fv( vector3_to_array( tip4 ) );

	gl().glVertex3fv( vector3_to_array( tip1 ) );
	gl().glVertex3fv( vector3_to_array( tip3 ) );

	gl().glVertex3fv( vector3_to_array( tip3 ) );
	gl().glVertex3fv( vector3_to_array( tip2 ) );

	gl().glVertex3fv( vector3_to_array( tip2 ) );
	gl().glVertex3fv( vector3_to_array( tip4 ) );

	gl().glVertex3fv( vector3_to_array( tip4 ) );
	gl().glVertex3fv( vector3_to_array( tip1 ) );

	gl().glEnd();
}

class RenderableArrow : public OpenGLRenderable
{
	const Vector3& m_origin;
	const Vector3& m_angles;

public:
	RenderableArrow( const Vector3& origin, const Vector3& angles )
		: m_origin( origin ), m_angles( angles ){
	}

	void render( RenderStateFlags state ) const {
		Matrix4 mat = matrix4_rotation_for_euler_xyz_degrees( m_angles );
		arrow_draw( m_origin, matrix4_transformed_direction( mat, Vector3( 1, 0, 0 ) ), matrix4_transformed_direction( mat, Vector3( 0, 1, 0 ) ), matrix4_transformed_direction( mat, Vector3( 0, 0, 1 ) ) );
	}
};


class SelectionIntersection;

inline void aabb_testselect( const AABB& aabb, SelectionTest& test, SelectionIntersection& best ){
	const IndexPointer::index_type indices[24] = {
		2, 1, 5, 6,
		1, 0, 4, 5,
		0, 1, 2, 3,
		3, 7, 4, 0,
		3, 2, 6, 7,
		7, 6, 5, 4,
	};

	const std::array<Vector3, 8> points = aabb_corners( aabb );
	test.TestQuads( VertexPointer( points[0].data(), sizeof( Vector3 ) ), IndexPointer( indices, 24 ), best );
}

inline void aabb_draw_wire( const std::array<Vector3, 8>& points ){
	unsigned int indices[26] = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7,
		// 0, 6, 1, 7, 2, 4, 3, 5 // X cross
		1, 7 // diagonal line (connect mins to maxs corner)
	};
#if 1
	gl().glVertexPointer( 3, GL_FLOAT, 0, points[0].data() );
	gl().glDrawElements( GL_LINES, sizeof( indices ) / sizeof( indices[0] ), GL_UNSIGNED_INT, indices );
#else
	gl().glBegin( GL_LINES );
	for ( std::size_t i = 0; i < sizeof( indices ) / sizeof( indices[0] ); ++i )
	{
		gl().glVertex3fv( vector3_to_array( points[indices[i]] ) );
	}
	gl().glEnd();
#endif
}

inline void aabb_draw_flatshade( const std::array<Vector3, 8>& points ){
	gl().glBegin( GL_QUADS );

	gl().glNormal3fv( vector3_to_array( aabb_normals[0] ) );
	gl().glVertex3fv( vector3_to_array( points[2] ) );
	gl().glVertex3fv( vector3_to_array( points[1] ) );
	gl().glVertex3fv( vector3_to_array( points[5] ) );
	gl().glVertex3fv( vector3_to_array( points[6] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[1] ) );
	gl().glVertex3fv( vector3_to_array( points[1] ) );
	gl().glVertex3fv( vector3_to_array( points[0] ) );
	gl().glVertex3fv( vector3_to_array( points[4] ) );
	gl().glVertex3fv( vector3_to_array( points[5] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[2] ) );
	gl().glVertex3fv( vector3_to_array( points[0] ) );
	gl().glVertex3fv( vector3_to_array( points[1] ) );
	gl().glVertex3fv( vector3_to_array( points[2] ) );
	gl().glVertex3fv( vector3_to_array( points[3] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[3] ) );
	gl().glVertex3fv( vector3_to_array( points[0] ) );
	gl().glVertex3fv( vector3_to_array( points[3] ) );
	gl().glVertex3fv( vector3_to_array( points[7] ) );
	gl().glVertex3fv( vector3_to_array( points[4] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[4] ) );
	gl().glVertex3fv( vector3_to_array( points[3] ) );
	gl().glVertex3fv( vector3_to_array( points[2] ) );
	gl().glVertex3fv( vector3_to_array( points[6] ) );
	gl().glVertex3fv( vector3_to_array( points[7] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[5] ) );
	gl().glVertex3fv( vector3_to_array( points[7] ) );
	gl().glVertex3fv( vector3_to_array( points[6] ) );
	gl().glVertex3fv( vector3_to_array( points[5] ) );
	gl().glVertex3fv( vector3_to_array( points[4] ) );

	gl().glEnd();
}

inline void aabb_draw_wire( const AABB& aabb ){
	aabb_draw_wire( aabb_corners( aabb ) );
}

inline void aabb_draw_flatshade( const AABB& aabb ){
	aabb_draw_flatshade( aabb_corners( aabb ) );
}

inline void aabb_draw_textured( const AABB& aabb ){
	const std::array<Vector3, 8> points = aabb_corners( aabb );

	gl().glBegin( GL_QUADS );

	gl().glNormal3fv( vector3_to_array( aabb_normals[0] ) );
	gl().glTexCoord2fv( aabb_texcoord_topleft );
	gl().glVertex3fv( vector3_to_array( points[2] ) );
	gl().glTexCoord2fv( aabb_texcoord_topright );
	gl().glVertex3fv( vector3_to_array( points[1] ) );
	gl().glTexCoord2fv( aabb_texcoord_botright );
	gl().glVertex3fv( vector3_to_array( points[5] ) );
	gl().glTexCoord2fv( aabb_texcoord_botleft );
	gl().glVertex3fv( vector3_to_array( points[6] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[1] ) );
	gl().glTexCoord2fv( aabb_texcoord_topleft );
	gl().glVertex3fv( vector3_to_array( points[1] ) );
	gl().glTexCoord2fv( aabb_texcoord_topright );
	gl().glVertex3fv( vector3_to_array( points[0] ) );
	gl().glTexCoord2fv( aabb_texcoord_botright );
	gl().glVertex3fv( vector3_to_array( points[4] ) );
	gl().glTexCoord2fv( aabb_texcoord_botleft );
	gl().glVertex3fv( vector3_to_array( points[5] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[2] ) );
	gl().glTexCoord2fv( aabb_texcoord_topleft );
	gl().glVertex3fv( vector3_to_array( points[0] ) );
	gl().glTexCoord2fv( aabb_texcoord_topright );
	gl().glVertex3fv( vector3_to_array( points[1] ) );
	gl().glTexCoord2fv( aabb_texcoord_botright );
	gl().glVertex3fv( vector3_to_array( points[2] ) );
	gl().glTexCoord2fv( aabb_texcoord_botleft );
	gl().glVertex3fv( vector3_to_array( points[3] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[3] ) );
	gl().glTexCoord2fv( aabb_texcoord_topleft );
	gl().glVertex3fv( vector3_to_array( points[0] ) );
	gl().glTexCoord2fv( aabb_texcoord_topright );
	gl().glVertex3fv( vector3_to_array( points[3] ) );
	gl().glTexCoord2fv( aabb_texcoord_botright );
	gl().glVertex3fv( vector3_to_array( points[7] ) );
	gl().glTexCoord2fv( aabb_texcoord_botleft );
	gl().glVertex3fv( vector3_to_array( points[4] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[4] ) );
	gl().glTexCoord2fv( aabb_texcoord_topleft );
	gl().glVertex3fv( vector3_to_array( points[3] ) );
	gl().glTexCoord2fv( aabb_texcoord_topright );
	gl().glVertex3fv( vector3_to_array( points[2] ) );
	gl().glTexCoord2fv( aabb_texcoord_botright );
	gl().glVertex3fv( vector3_to_array( points[6] ) );
	gl().glTexCoord2fv( aabb_texcoord_botleft );
	gl().glVertex3fv( vector3_to_array( points[7] ) );

	gl().glNormal3fv( vector3_to_array( aabb_normals[5] ) );
	gl().glTexCoord2fv( aabb_texcoord_topleft );
	gl().glVertex3fv( vector3_to_array( points[7] ) );
	gl().glTexCoord2fv( aabb_texcoord_topright );
	gl().glVertex3fv( vector3_to_array( points[6] ) );
	gl().glTexCoord2fv( aabb_texcoord_botright );
	gl().glVertex3fv( vector3_to_array( points[5] ) );
	gl().glTexCoord2fv( aabb_texcoord_botleft );
	gl().glVertex3fv( vector3_to_array( points[4] ) );

	gl().glEnd();
}

inline void aabb_draw_solid( const AABB& aabb, RenderStateFlags state ){
	if ( state & RENDER_TEXTURE ) {
		aabb_draw_textured( aabb );
	}
	else
	{
		aabb_draw_flatshade( aabb );
	}
}

inline void aabb_draw( const AABB& aabb, RenderStateFlags state ){
	if ( state & RENDER_FILL ) {
		aabb_draw_solid( aabb, state );
	}
	else
	{
		aabb_draw_wire( aabb );
	}
}

class RenderableSolidAABB : public OpenGLRenderable
{
	const AABB& m_aabb;
public:
	RenderableSolidAABB( const AABB& aabb ) : m_aabb( aabb ){
	}
	void render( RenderStateFlags state ) const {
		aabb_draw_solid( m_aabb, state );
	}
};

class RenderableWireframeAABB : public OpenGLRenderable
{
	const AABB& m_aabb;
public:
	RenderableWireframeAABB( const AABB& aabb ) : m_aabb( aabb ){
	}
	void render( RenderStateFlags state ) const {
		aabb_draw_wire( m_aabb );
	}
};


/// \brief A key/value pair of strings.
///
/// - Notifies observers when value changes - value changes to "" on destruction.
/// - Provides undo support through the global undo system.
class KeyValue final : public EntityKeyValue
{
	typedef UnsortedSet<KeyObserver> KeyObservers;

	std::size_t m_refcount;
	KeyObservers m_observers;
	CopiedString m_string;
	const char* m_empty;
	ObservedUndoableObject<CopiedString> m_undo;
	static EntityCreator::KeyValueChangedFunc m_entityKeyValueChanged;
public:

	KeyValue( const char* string, const char* empty )
		: m_refcount( 0 ), m_string( string ), m_empty( empty ), m_undo( m_string, UndoImportCaller( *this ) ){
		notify();
	}
	~KeyValue(){
		ASSERT_MESSAGE( m_observers.empty(), "KeyValue::~KeyValue: observers still attached" );
	}

	static void setKeyValueChangedFunc( EntityCreator::KeyValueChangedFunc func ){
		m_entityKeyValueChanged = func;
	}

	void IncRef(){
		++m_refcount;
	}
	void DecRef(){
		if ( --m_refcount == 0 ) {
			delete this;
		}
	}

	void instanceAttach( MapFile* map ){
		m_undo.instanceAttach( map );
	}
	void instanceDetach( MapFile* map ){
		m_undo.instanceDetach( map );
	}

	void attach( const KeyObserver& observer ) override {
		( *m_observers.insert ( observer ) )( c_str() );
	}
	void detach( const KeyObserver& observer ) override {
		observer( m_empty );
		m_observers.erase( observer );
	}
	const char* c_str() const override {
		if ( m_string.empty() ) {
			return m_empty;
		}
		return m_string.c_str();
	}
	void assign( const char* other ) override {
		if ( !string_equal( m_string.c_str(), other ) ) {
			m_undo.save();
			m_string = other;
			notify();
		}
	}

	void notify(){
		m_entityKeyValueChanged();
		KeyObservers::reverse_iterator i = m_observers.rbegin();
		while ( i != m_observers.rend() )
		{
			( *i++ )( c_str() );
		}
	}

	void importState( const CopiedString& string ){
		m_string = string;

		notify();
	}
	typedef MemberCaller<KeyValue, void(const CopiedString&), &KeyValue::importState> UndoImportCaller;
};

/// \brief An unsorted list of key/value pairs.
///
/// - Notifies observers when a pair is inserted or removed.
/// - Provides undo support through the global undo system.
/// - New keys are appended to the end of the list.
class EntityKeyValues : public Entity
{
public:
	typedef KeyValue Value;

	static StringPool& getPool(){
		return Static<StringPool, KeyContext>::instance();
	}
private:
	static EntityCreator::KeyValueChangedFunc m_entityKeyValueChanged;
	static Counter* m_counter;

	EntityClass* m_eclass;

	class KeyContext {};
	typedef Static<StringPool, KeyContext> KeyPool;
	typedef PooledString<KeyPool> Key;
	typedef SmartPointer<KeyValue> KeyValuePtr;
	typedef UnsortedMap<Key, KeyValuePtr> KeyValues;
	KeyValues m_keyValues;

	typedef UnsortedSet<Observer*> Observers;
	Observers m_observers;

	ObservedUndoableObject<KeyValues> m_undo;
	bool m_instanced;

	bool m_observerMutex;

	void notifyInsert( const char* key, Value& value ){
		m_observerMutex = true;
		for ( Observer *o : m_observers )
		{
			o->insert( key, value );
		}
		m_observerMutex = false;
	}
	void notifyErase( const char* key, Value& value ){
		m_observerMutex = true;
		for ( Observer *o : m_observers )
		{
			o->erase( key, value );
		}
		m_observerMutex = false;
	}
	void forEachKeyValue_notifyInsert(){
		for ( const auto& [ key, value ] : m_keyValues )
		{
			notifyInsert( key.c_str(), *value );
		}
	}
	void forEachKeyValue_notifyErase(){
		for ( const auto& [ key, value ] : m_keyValues )
		{
			notifyErase( key.c_str(), *value );
		}
	}

	std::optional<KeyValues::const_iterator> find( const char* key ) const {
		/* present in the pool -> actual search makes sense */
		if( const StringPool::iterator it = getPool().find( const_cast<char *>( key ) ); it != getPool().end() )
			for( KeyValues::const_iterator i = m_keyValues.begin(); i != m_keyValues.end(); ++i )
				if( i->first == it )
					return i;
		return {};
	}
	std::optional<KeyValues::iterator> find( const char* key ){
		/* present in the pool -> actual search makes sense */
		if( const StringPool::iterator it = getPool().find( const_cast<char *>( key ) ); it != getPool().end() )
			for( KeyValues::iterator i = m_keyValues.begin(); i != m_keyValues.end(); ++i )
				if( i->first == it )
					return i;
		return {};
	}

	void insert( const char* key, const KeyValuePtr& keyValue ){
		KeyValues::iterator i = m_keyValues.insert( KeyValues::value_type( key, keyValue ) );
		notifyInsert( key, *( *i ).second );

		if ( m_instanced ) {
			( *i ).second->instanceAttach( m_undo.map() );
		}
	}

	void insert( const char* key, const char* value ){
		if ( auto i = find( key ) ) {
			( *i )->second->assign( value );
		}
		else
		{
			m_undo.save();
			insert( key, KeyValuePtr( new KeyValue( value, EntityClass_valueForKey( *m_eclass, key ) ) ) );
		}
	}

	void erase( KeyValues::iterator i ){
		if ( m_instanced ) {
			( *i ).second->instanceDetach( m_undo.map() );
		}

		Key key( ( *i ).first );
		KeyValuePtr value( ( *i ).second );
		m_keyValues.erase( i );
		notifyErase( key.c_str(), *value );
	}

	void erase( const char* key ){
		if ( auto i = find( key ) ) {
			m_undo.save();
			erase( *i );
		}
	}

public:
	bool m_isContainer;

	EntityKeyValues( EntityClass* eclass ) :
		m_eclass( eclass ),
		m_undo( m_keyValues, UndoImportCaller( *this ) ),
		m_instanced( false ),
		m_observerMutex( false ),
		m_isContainer( !eclass->fixedsize ){
	}
	EntityKeyValues( const EntityKeyValues& other ) :
		Entity( other ),
		m_eclass( &other.getEntityClass() ),
		m_undo( m_keyValues, UndoImportCaller( *this ) ),
		m_instanced( false ),
		m_observerMutex( false ),
		m_isContainer( other.m_isContainer ){
		for ( const auto& [ key, value ] : other.m_keyValues )
		{
			insert( key.c_str(), value->c_str() );
		}
	}
	~EntityKeyValues(){
		for ( Observers::iterator i = m_observers.begin(); i != m_observers.end(); )
		{
			// post-increment to allow current element to be removed safely
			( *i++ )->clear();
		}
		ASSERT_MESSAGE( m_observers.empty(), "EntityKeyValues::~EntityKeyValues: observers still attached" );
	}

	static void setKeyValueChangedFunc( EntityCreator::KeyValueChangedFunc func ){
		m_entityKeyValueChanged = func;
		KeyValue::setKeyValueChangedFunc( func );
	}
	static void setCounter( Counter* counter ){
		m_counter = counter;
	}

	void importState( const KeyValues& keyValues ){
		for ( KeyValues::iterator i = m_keyValues.begin(); i != m_keyValues.end(); )
		{
			erase( i++ ); // post-increment to allow current element to be removed safely
		}

		for ( const auto& [ key, value ] : keyValues )
		{
			insert( key.c_str(), value );
		}

		m_entityKeyValueChanged();
	}
	typedef MemberCaller<EntityKeyValues, void(const KeyValues&), &EntityKeyValues::importState> UndoImportCaller;

	void attach( Observer& observer ) override {
		ASSERT_MESSAGE( !m_observerMutex, "observer cannot be attached during iteration" );
		m_observers.insert( &observer );
		for ( const auto& [ key, value ] : m_keyValues )
		{
			observer.insert( key.c_str(), *value );
		}
	}
	void detach( Observer& observer ) override {
		ASSERT_MESSAGE( !m_observerMutex, "observer cannot be detached during iteration" );
		m_observers.erase( &observer );
		for ( const auto& [ key, value ] : m_keyValues )
		{
			observer.erase( key.c_str(), *value );
		}
	}

	void forEachKeyValue_instanceAttach( MapFile* map ){
		for ( const auto& [ key, value ] : m_keyValues )
		{
			value->instanceAttach( map );
		}
	}
	void forEachKeyValue_instanceDetach( MapFile* map ){
		for ( const auto& [ key, value ] : m_keyValues )
		{
			value->instanceDetach( map );
		}
	}

	void instanceAttach( MapFile* map ){
		if ( m_counter != 0 ) {
			m_counter->increment();
		}

		m_instanced = true;
		forEachKeyValue_instanceAttach( map );
		m_undo.instanceAttach( map );
	}
	void instanceDetach( MapFile* map ){
		if ( m_counter != 0 ) {
			m_counter->decrement();
		}

		m_undo.instanceDetach( map );
		forEachKeyValue_instanceDetach( map );
		m_instanced = false;
	}

	// entity
	EntityClass& getEntityClass() const override {
		return *m_eclass;
	}
	const char* getClassName() const override {
		return m_eclass->name();
	}
	void forEachKeyValue( Visitor& visitor ) const override {
		for ( const auto& [ key, value ] : m_keyValues )
		{
			visitor.visit( key.c_str(), value->c_str() );
		}
	}
	void setKeyValue( const char* key, const char* value ) override {
		if ( string_empty( value )
		     /*|| string_equal( EntityClass_valueForKey( *m_eclass, key ), value )*/ ) { // don't delete values equal to default
			erase( key );
		}
		else
		{
			insert( key, value );
		}
		m_entityKeyValueChanged();
	}
	const char* getKeyValue( const char* key ) const override {
		if ( auto i = find( key ) ) {
			return ( *i )->second->c_str();
		}

		// return EntityClass_valueForKey( *m_eclass, key );
		return "";
	}
	bool hasKeyValue( const char* key ) const override {
		return find( key ).has_value();
	}

	bool isContainer() const override {
		return m_isContainer;
	}
};

/// \brief A Resource reference with a controlled lifetime.
/// \brief The resource is released when the ResourceReference is destroyed.
class ResourceReference
{
	CopiedString m_name;
	Resource* m_resource;
public:
	ResourceReference( const char* name )
		: m_name( name ){
		capture();
	}
	ResourceReference( const ResourceReference& other )
		: m_name( other.m_name ){
		capture();
	}
	ResourceReference& operator=( const ResourceReference& other ){
		ResourceReference tmp( other );
		tmp.swap( *this );
		return *this;
	}
	~ResourceReference(){
		release();
	}

	void capture(){
		m_resource = GlobalReferenceCache().capture( m_name.c_str() );
	}
	void release(){
		GlobalReferenceCache().release( m_name.c_str() );
	}

	const char* getName() const {
		return m_name.c_str();
	}
	void setName( const char* name ){
		ResourceReference tmp( name );
		tmp.swap( *this );
	}

	void swap( ResourceReference& other ){
		std::swap( m_resource, other.m_resource );
		std::swap( m_name, other.m_name );
	}

	void attach( ModuleObserver& observer ){
		m_resource->attach( observer );
	}
	void detach( ModuleObserver& observer ){
		m_resource->detach( observer );
	}

	Resource* get(){
		return m_resource;
	}
};

namespace std
{
/// \brief Swaps the values of \p self and \p other.
/// Overloads std::swap.
inline void swap( ResourceReference& self, ResourceReference& other ){
	self.swap( other );
}
}

/// this is only correct for radiant 2d views matrices
inline bool aabb_fits_view( const AABB& aabb, const Matrix4& modelview, const Matrix4& viewport, int ratio ){
	const AABB transformed_bounds = aabb_for_oriented_aabb(
	                                    AABB( aabb.origin, Vector3( std::max( aabb.extents[0], 8.f ), std::max( aabb.extents[1], 8.f ), std::max( aabb.extents[2], 8.f ) ) ),
	                                    modelview
	                                );

	//return ( aabb.extents[0] / viewport[0] ) > 0.25f || ( aabb.extents[1] / viewport[5] ) > 0.25f;
	return ( viewport[0] + viewport[5] ) / ( transformed_bounds.extents[0] + transformed_bounds.extents[1] ) < ratio;
}

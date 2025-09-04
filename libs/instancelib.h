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

#include "debugging/debugging.h"

#include "iscenegraph.h"

#include "scenelib.h"
#include "generic/reference.h"
#include "generic/callback.h"
#include <map>

class InstanceSubgraphWalker : public scene::Traversable::Walker
{
	scene::Instantiable::Observer* m_observer;
	mutable scene::Path m_path;
	mutable Stack<scene::Instance*> m_parent;
public:
	InstanceSubgraphWalker( scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* parent )
		: m_observer( observer ), m_path( path ), m_parent( parent ){
	}
	bool pre( scene::Node& node ) const override {
		m_path.push( makeReference( node ) );
		scene::Instance* instance = Node_getInstantiable( node )->create( m_path, m_parent.top() );
		m_observer->insert( instance );
		Node_getInstantiable( node )->insert( m_observer, m_path, instance );
		m_parent.push( instance );
		return true;
	}
	void post( scene::Node& node ) const override {
		m_path.pop();
		m_parent.pop();
	}
};

class UninstanceSubgraphWalker : public scene::Traversable::Walker
{
	scene::Instantiable::Observer* m_observer;
	mutable scene::Path m_path;
public:
	UninstanceSubgraphWalker( scene::Instantiable::Observer* observer, const scene::Path& parent )
		: m_observer( observer ), m_path( parent ){
	}
	bool pre( scene::Node& node ) const override {
		m_path.push( makeReference( node ) );
		return true;
	}
	void post( scene::Node& node ) const override {
		scene::Instance* instance = Node_getInstantiable( node )->erase( m_observer, m_path );
		m_observer->erase( instance );
		delete instance;
		m_path.pop();
	}
};

class InstanceSet : public scene::Traversable::Observer
{
	typedef std::pair<scene::Instantiable::Observer*, PathConstReference> CachePath;

	typedef CachePath key_type;

	typedef std::map<key_type, scene::Instance*> InstanceMap;
	InstanceMap m_instances;
public:

	typedef InstanceMap::iterator iterator;

	iterator begin(){
		return m_instances.begin();
	}
	iterator end(){
		return m_instances.end();
	}

// traverse observer
	void insert( scene::Node& child ) override {
		for ( auto& [ key, instance ] : m_instances )
		{
			Node_traverseSubgraph( child, InstanceSubgraphWalker( key.first, key.second, instance ) );
			instance->boundsChanged();
		}
	}
	void erase( scene::Node& child ) override {
		for ( auto& [ key, instance ] : m_instances )
		{
			Node_traverseSubgraph( child, UninstanceSubgraphWalker( key.first, key.second ) );
			instance->boundsChanged();
		}
	}

// instance
	void forEachInstance( const scene::Instantiable::Visitor& visitor ){
		for ( auto& i : m_instances )
		{
			visitor.visit( *i.second );
		}
	}

	void insert( scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance ){
		const bool inserted = m_instances.insert( InstanceMap::value_type( key_type( observer, PathConstReference( instance->path() ) ), instance ) ).second;
		ASSERT_MESSAGE( inserted, "InstanceSet::insert - element already exists" );
	}
	scene::Instance* erase( scene::Instantiable::Observer* observer, const scene::Path& path ){
		InstanceMap::iterator i = m_instances.find( key_type( observer, PathConstReference( path ) ) );
		ASSERT_MESSAGE( i != m_instances.end(), "InstanceSet::erase - failed to find element" );
		scene::Instance* instance = i->second;
		m_instances.erase( i );
		return instance;
	}

	void transformChanged(){
		for ( auto& i : m_instances )
		{
			i.second->transformChanged();
		}
	}
	typedef MemberCaller<InstanceSet, void(), &InstanceSet::transformChanged> TransformChangedCaller;
	void boundsChanged(){
		for ( auto& i : m_instances )
		{
			i.second->boundsChanged();
		}
	}
	typedef MemberCaller<InstanceSet, void(), &InstanceSet::boundsChanged> BoundsChangedCaller;
};

template<typename Functor>
inline void InstanceSet_forEach( InstanceSet& instances, const Functor& functor ){
	for ( InstanceSet::iterator i = instances.begin(), end = instances.end(); i != end; ++i )
	{
		functor( *( *i ).second );
	}
}

template<typename Type>
class InstanceSetEvaluateTransform
{
public:
	static void apply( InstanceSet& instances ){
		InstanceSet_forEach( instances, [&]( scene::Instance& instance ){
			InstanceTypeCast<Type>::cast( instance )->evaluateTransform();
		});
	}
	typedef ReferenceCaller<InstanceSet, void(), &InstanceSetEvaluateTransform<Type>::apply> Caller;
};

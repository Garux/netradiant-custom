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

#include "scenelib.h"
#include "string/string.h"
#include <concepts>


#define LAYERIDX0 0
#define LAYERIDXPARENT -1

template <typename F>
concept LayerFunctor = std::invocable<F, Layer&> && std::is_void_v<std::invoke_result_t<F, Layer&>>;

class Layer
{
public:
	CopiedString m_name;
	std::list<Layer> m_children;
	Layer *m_parent; // must be valid besides 0 in the root 'Layers' container
	int m_color[ 3 ]{}; // 0 0 0 = default gui color
	int m_parentIndex = LAYERIDXPARENT; // parent index during map loading // -1 parent = root
	int m_ownIndex = LAYERIDXPARENT; // own index for map writing and operator== set via update_ownIndices()
	Layer( const char *name, Layer *parent ) : m_name( name ), m_parent( parent ){
	}
	Layer( Layer&& ) noexcept = default; // no copy
	bool operator==( const Layer& other ) const {
		return m_name == other.m_name
		    && m_parent->m_ownIndex == const_cast<Layer*>( other.m_parent )->m_ownIndex
		    && !memcmp( m_color, other.m_color, sizeof( m_color ) )
		    && m_children == other.m_children;
	}
private:
	void forEach( std::list<Layer>& layers, LayerFunctor auto&& functor ){
		for( Layer& layer : layers ){
			functor( layer );
			forEach( layer.m_children, functor );
		}
	}
public:
	void forEach( LayerFunctor auto&& functor ){
		forEach( m_children, functor );
	}

	using iterator = decltype( m_children )::iterator;
};

class Layers : public Layer
{
public:
	STRING_CONSTANT( Name, "Layers" );
	Layer *m_currentLayer;
	Layers() : Layer( "", nullptr ){
		m_children.emplace_back( "0", this ); // add default layer
		m_currentLayer = &m_children.front();
	}
	Layers( Layers&& ) noexcept = default; // no copy

	void update_ownIndices(){
		forEach( [i = 0]( Layer& layer ) mutable { layer.m_ownIndex = i++; } );
	}
};

inline Layers* Node_getLayers( scene::Node& node ){
	return NodeTypeCast<Layers>::cast( node );
}

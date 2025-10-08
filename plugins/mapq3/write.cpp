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

#include "write.h"

#include "ientity.h"
#include "iscriplib.h"
#include "scenelib.h"
#include "layers.h"

void Layers_Write( Layers& layers, TokenWriter& writer ){
	layers.forEach( [&]( Layer& layer ){
		writer.writeToken( "//@$&" );
		writer.writeToken( "layerdef" );
		writer.writeString( layer.m_name.c_str() );
		writer.writeInteger( layer.m_parent->m_ownIndex );
		writer.writeInteger( layer.m_color[ 0 ] );
		writer.writeInteger( layer.m_color[ 1 ] );
		writer.writeInteger( layer.m_color[ 2 ] );
		writer.nextLine();
	} );
}

void Layer_Write( const Layer *layer, int& currentLayer, TokenWriter& writer ){
	if( layer != nullptr && currentLayer != layer->m_ownIndex ){
		currentLayer = layer->m_ownIndex;
		writer.writeToken( "//@$&" );
		writer.writeToken( "layer" );
		writer.writeInteger( currentLayer );
		writer.nextLine();
	}
}


inline MapExporter* Node_getMapExporter( scene::Node& node ){
	return NodeTypeCast<MapExporter>::cast( node );
}


static std::size_t g_count_entities;
static std::size_t g_count_brushes;


void Entity_ExportTokens( const Entity& entity, TokenWriter& writer ){
	g_count_brushes = 0;

	class WriteKeyValue : public Entity::Visitor
	{
		TokenWriter& m_writer;
	public:
		WriteKeyValue( TokenWriter& writer )
			: m_writer( writer ){
		}

		void visit( const char* key, const char* value ) override {
			m_writer.writeString( key );
			m_writer.writeString( value );
			m_writer.nextLine();
		}
	} visitor( writer );

	entity.forEachKeyValue( visitor );
}

class WriteTokensWalker : public scene::Traversable::Walker
{
	mutable Stack<bool> m_stack;
	TokenWriter& m_writer;
	bool m_ignorePatches;
	mutable int m_currentLayer = LAYERIDX0;
public:
	WriteTokensWalker( TokenWriter& writer, bool ignorePatches, Layers& layers )
		: m_writer( writer ), m_ignorePatches( ignorePatches )
	{
		layers.update_ownIndices();
		Layers_Write( layers, m_writer );
	}
	bool pre( scene::Node& node ) const override {
		m_stack.push( false );

		Entity* entity = Node_getEntity( node );
		if ( entity != 0 ) {
			if( entity->isContainer() && Node_getTraversable( node )->empty() && !string_equal( entity->getClassName(), "worldspawn" )
			 && !entity->hasKeyValue( "origin" ) ){
				globalErrorStream() << "discarding empty group entity: # = " << g_count_entities << "; classname = " << entity->getClassName() << '\n';
				return false;
			}
			m_writer.writeToken( "//" );
			m_writer.writeToken( "entity" );
			m_writer.writeUnsigned( g_count_entities++ );
			m_writer.nextLine();

			Layer_Write( node.m_layer, m_currentLayer, m_writer );

			m_writer.writeToken( "{" );
			m_writer.nextLine();
			m_stack.top() = true;

			Entity_ExportTokens( *entity, m_writer );
		}
		else
		{
			MapExporter* exporter = Node_getMapExporter( node );
			if ( exporter != 0
			  && !( m_ignorePatches && Node_isPatch( node ) ) ) {
				m_writer.writeToken( "//" );
				m_writer.writeToken( "brush" );
				m_writer.writeUnsigned( g_count_brushes++ );
				m_writer.nextLine();

				Layer_Write( node.m_layer, m_currentLayer, m_writer );

				exporter->exportTokens( m_writer );
			}
		}

		return true;
	}
	void post( scene::Node& node ) const override {
		if ( m_stack.top() ) {
			m_writer.writeToken( "}" );
			m_writer.nextLine();
		}
		m_stack.pop();
	}
};

void Map_Write( scene::Node& root, GraphTraversalFunc traverse, TokenWriter& writer, bool ignorePatches ){
	g_count_entities = 0;
	traverse( root, WriteTokensWalker( writer, ignorePatches, *Node_getLayers( root ) ) );
}

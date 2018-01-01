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

#include "treemodel.h"

#include "debugging/debugging.h"

#include <map>

#include <QAbstractItemModel>
#include <QBrush>
#include <QApplication>
#include <QPalette>

#include "iscenegraph.h"
#include "nameable.h"

#include "generic/callback.h"
#include "scenelib.h"
#include "string/string.h"
#include "generic/reference.h"


inline Nameable* Node_getNameable( scene::Node& node ){
	return NodeTypeCast<Nameable>::cast( node );
}

const char* node_get_name( scene::Node& node ){
	Nameable* nameable = Node_getNameable( node );
	return ( nameable != 0 )
	       ? nameable->name()
	       : "node";
}

const char* node_get_name_safe( scene::Node& node ){
	// https://github.com/TTimo/GtkRadiant/issues/289
	// Reference cannot be bound to dereferenced null pointer in well-defined
	// C++ code, and Clang will assume that comparison below always evaluates
	// to true, resulting in a segmentation fault.  Use a dirty hack to force
	// Clang to check those "bad" references for null nonetheless.
	// At least here check is vital , 0 is g_null_node
	volatile intptr_t n = (intptr_t)&node;  // see the comment on line 54
	if ( n == 0 ) {
		return "";
	}
	return node_get_name( node );
}

void node_attach_name_changed_callback( scene::Node& node, const NameCallback& callback ){
	volatile intptr_t n = (intptr_t)&node;  // see the comment on line 54
	if ( n != 0 ) {
		Nameable* nameable = Node_getNameable( node );
		if ( nameable != 0 ) {
			nameable->attach( callback );
		}
	}
}
void node_detach_name_changed_callback( scene::Node& node, const NameCallback& callback ){
	volatile intptr_t n = (intptr_t)&node;  // see the comment on line 54
	if ( n != 0 ) {
		Nameable* nameable = Node_getNameable( node );
		if ( nameable != 0 ) {
			nameable->detach( callback );
		}
	}
}

void graph_tree_model_set_name( const scene::Instance& instance, const char* name );


#include "iselection.h"

namespace
{
scene::Node* g_null_node = 0;
}

class NullInstance : public scene::Instance
{
public:
	NullInstance() : scene::Instance( scene::Path( makeReference( *g_null_node ) ), 0, 0, Static<InstanceTypeCastTable>::instance() ){
	}
};

namespace
{
NullInstance g_null_instance;
}


class GraphTreeNode
{
private:
	struct Compare{
		bool operator()( const GraphTreeNode *one, const GraphTreeNode *other ) const {
			const int n = string_compare( one->m_name.c_str(), other->m_name.c_str() );
			return n != 0? n < 0 : one->m_node < other->m_node;
		}
	};
	typedef std::vector<GraphTreeNode*> ChildNodes;
	ChildNodes m_childnodes;
public:
	Reference<scene::Instance> m_instance;
	GraphTreeNode* m_parent;
	CopiedString m_name;
	scene::Node* m_node;

	typedef ChildNodes::iterator iterator;

	GraphTreeNode( scene::Instance& instance, const char *name, scene::Node* node ) :
		m_instance( instance ), m_parent( 0 ), m_name( name ), m_node( node ){
	}
	~GraphTreeNode(){
		ASSERT_MESSAGE( empty(), "GraphTreeNode::~GraphTreeNode: memory leak" );
	}
	GraphTreeNode( GraphTreeNode&& ) noexcept = delete;

	iterator begin(){
		return m_childnodes.begin();
	}
	iterator end(){
		return m_childnodes.end();
	}

	std::size_t size() const {
		return m_childnodes.size();
	}
	bool empty() const {
		return m_childnodes.empty();
	}
	// may not be called on the root node!
	int getIndex() const {
		return getIndex( m_parent->find( m_name.c_str(), m_node ) );
	}
	// iterator in parent's children list!
	int getIndex( iterator it ) const {
		return std::distance( m_parent->begin(), it );
	}
	GraphTreeNode *child( int row ) const {
		if( row < 0 || size_t( row ) >= m_childnodes.size() )
			return nullptr;
		return m_childnodes[row];
	}

	iterator insert( GraphTreeNode* value ){
		value->m_parent = this;
		return m_childnodes.insert( std::lower_bound( begin(), end(), value, Compare() ), value );
//.		ASSERT_MESSAGE( inserted, "GraphTreeNode::insert: already added" );
	}
	void erase( iterator i ){
		m_childnodes.erase( i );
	}
	iterator find( const char *name, const scene::Node* node ){
		return std::lower_bound( begin(), end(), name, [node]( const GraphTreeNode *other, const char *name ){
			const int n = string_compare( other->m_name.c_str(), name );
			return n != 0? n < 0 : other->m_node < node;
		} );
	}
	// find index of future insertion
	int lower_bound( const char *name, const scene::Node* node ){
		return std::distance( begin(), find( name, node ) );
	}
};


class GraphTreeModel : public QAbstractItemModel
{
public:
	GraphTreeModel(){
		rootItem = new GraphTreeNode( g_null_instance, "", nullptr );
	}
	~GraphTreeModel(){
		delete rootItem;
	}
	QVariant data( const QModelIndex &index, int role ) const override {
		if ( index.isValid() ){
			GraphTreeNode *item = getNode( index );
			if ( role == Qt::ItemDataRole::DisplayRole )
				return item->m_name.c_str();
			else if( role == c_ItemDataRole_Instance )
				return QVariant::fromValue( static_cast<void*>( item->m_instance.get_pointer() ) );
			else if( role == c_ItemDataRole_Node )
				return QVariant::fromValue( static_cast<void*>( item->m_node ) );
			else if( role == Qt::ItemDataRole::BackgroundRole ){
				const QColor color = item->m_instance.get().isSelected()
				                     ? qApp->palette().color( QPalette::ColorRole::Highlight ).darker( 200 )
				                     : item->m_instance.get().childSelected()
				                     ? qApp->palette().color( QPalette::ColorRole::Highlight ).darker( 300 )
				                     : qApp->palette().color( QPalette::ColorRole::Base );
				return QBrush( color );
			}
		}
		return QVariant();

	}
	Qt::ItemFlags flags( const QModelIndex &index ) const override {
		if ( !index.isValid() )
			return Qt::NoItemFlags;
		return QAbstractItemModel::flags( index );
	}
	QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override {
		return QVariant();
	}
	GraphTreeNode* getNode( const QModelIndex &index ) const {
		return static_cast<GraphTreeNode*>( index.internalPointer() );
	}
	std::pair<GraphTreeNode*, QModelIndex> findParents( const scene::Path& path ) const {
		GraphTreeNode* parent = rootItem;
		QModelIndex index;
		for ( scene::Path::const_iterator i = path.begin(); i != path.end() - 1; ++i )
		{
			GraphTreeNode::iterator child = parent->find( node_get_name_safe( ( *i ).get() ), ( *i ).get_pointer() );
			ASSERT_MESSAGE( child != parent->end(), "ERROR" );
			parent = *child;
			index = this->index( ( *child )->getIndex( child ), 0, index );
			ASSERT_MESSAGE( index.isValid(), "index.isValid()" );
		}
		return { parent, index };
	}
	QModelIndex index( int row, int column, const QModelIndex &parent ) const override {
		if ( !hasIndex( row, column, parent ) )
			return QModelIndex();

		GraphTreeNode *parentItem = parent.isValid()? getNode( parent ) : rootItem;
		GraphTreeNode *childItem = parentItem->child( row );
		if ( childItem )
			return createIndex( row, column, childItem );
		return QModelIndex();
	}
	QModelIndex parent( const QModelIndex &index ) const override {
		if ( !index.isValid() )
			return QModelIndex();

		GraphTreeNode *childItem = getNode( index );
		GraphTreeNode *parentItem = childItem->m_parent;

		if ( parentItem == rootItem )
			return QModelIndex();

		return createIndex( parentItem->getIndex(), 0, parentItem );
	}
	int rowCount( const QModelIndex &parent ) const override {
		if ( parent.column() > 0 )
			return 0;

		GraphTreeNode *parentItem = parent.isValid()? getNode( parent ) : rootItem;
		return parentItem->size();
	}
	int columnCount( const QModelIndex &parent = QModelIndex() ) const override {
		return 1;
	}
	void insert( const scene::Instance& instance ) {
		auto [parent, index] = findParents( instance.path() );

		const int n = parent->lower_bound( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() );

		beginInsertRows( index, n, n );
		parent->insert( new GraphTreeNode( const_cast<scene::Instance&>( instance ), node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() ) );
		endInsertRows();

		node_attach_name_changed_callback( instance.path().top(), ConstReferenceCaller<scene::Instance, void(const char*), graph_tree_model_set_name>( instance ) );
	}
	void remove( const scene::Instance& instance ) {
		node_detach_name_changed_callback( instance.path().top(), ConstReferenceCaller<scene::Instance, void(const char*), graph_tree_model_set_name>( instance ) );

		auto [parent, index] = findParents( instance.path() );

		GraphTreeNode::iterator i = parent->find( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() );

		const int n = ( *i )->getIndex( i );

		beginRemoveRows( index, n, n );
		GraphTreeNode* node( *i );
		parent->erase( i );
		delete node;
		endRemoveRows();
	}
	void rename( const scene::Instance& instance, const char* name ){
		auto [parent, index] = findParents( instance.path() );

		GraphTreeNode::iterator i = parent->find( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() );

		const int n = ( *i )->getIndex( i );
		const int n2 = parent->lower_bound( name, instance.path().top().get_pointer() );

		if( ( n2 - n ) == 0 || ( n2 - n ) == 1 ){ //rename
			( *i )->m_name = name;
			dataChanged( this->index( n, 0, index ), this->index( n, 0, index ), { Qt::ItemDataRole::DisplayRole } );
		}
		else{ // move
			beginMoveRows( index, n, n, index, n2 );
			GraphTreeNode* node( *i );
			parent->erase( i );
			node->m_name = name;
			parent->insert( node );
			endMoveRows();
		}
	}
private:
	GraphTreeNode *rootItem;
};

GraphTreeModel* graph_tree_model_new(){
	return new GraphTreeModel;
}

void graph_tree_model_delete( GraphTreeModel* model ){
	delete model;
}

void graph_tree_model_insert( GraphTreeModel* model, const scene::Instance& instance ){
	model->insert( instance );
}

void graph_tree_model_erase( GraphTreeModel* model, const scene::Instance& instance ){
	model->remove( instance );
}


GraphTreeModel* scene_graph_get_tree_model(); // temp hack
void graph_tree_model_set_name( const scene::Instance& instance, const char* name ){
	scene_graph_get_tree_model()->rename( instance, name );
}


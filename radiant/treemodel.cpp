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
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreednd.h>
#include <gtk/gtkmain.h>

#include "iscenegraph.h"
#include "nameable.h"

#include "generic/callback.h"
#include "scenelib.h"
#include "string/string.h"
#include "generic/reference.h"

inline Nameable* Node_getNameable( scene::Node& node ){
	return NodeTypeCast<Nameable>::cast( node );
}

class GraphTreeNode;
void graph_tree_model_row_changed( GraphTreeNode& node );

class GraphTreeNode
{
typedef std::map<std::pair<CopiedString, scene::Node*>, GraphTreeNode*> ChildNodes;
ChildNodes m_childnodes;
public:
Reference<scene::Instance> m_instance;
GraphTreeNode* m_parent;

typedef ChildNodes::iterator iterator;
typedef ChildNodes::key_type key_type;
typedef ChildNodes::value_type value_type;
typedef ChildNodes::size_type size_type;

GraphTreeNode( scene::Instance& instance ) : m_instance( instance ), m_parent( 0 ){
	m_instance.get().setChildSelectedChangedCallback( RowChangedCaller( *this ) );
}
~GraphTreeNode(){
	m_instance.get().setChildSelectedChangedCallback( Callback() );
	ASSERT_MESSAGE( empty(), "GraphTreeNode::~GraphTreeNode: memory leak" );
}

iterator begin(){
	return m_childnodes.begin();
}
iterator end(){
	return m_childnodes.end();
}

size_type size() const {
	return m_childnodes.size();
}
bool empty() const {
	return m_childnodes.empty();
}

iterator insert( const value_type& value ){
	iterator i = m_childnodes.insert( value ).first;
	( *i ).second->m_parent = this;
	return i;
}
void erase( iterator i ){
	m_childnodes.erase( i );
}
iterator find( const key_type& key ){
	return m_childnodes.find( key );
}

void swap( GraphTreeNode& other ){
	std::swap( m_parent, other.m_parent );
	std::swap( m_childnodes, other.m_childnodes );
	std::swap( m_instance, other.m_instance );
}

void rowChanged(){
	graph_tree_model_row_changed( *this );
}
typedef MemberCaller<GraphTreeNode, &GraphTreeNode::rowChanged> RowChangedCaller;
};

struct GraphTreeModel
{
	GObject parent;

	GraphTreeNode* m_graph;
};

struct GraphTreeModelClass
{
	GObjectClass parent_class;
};

#define GRAPH_TREE_MODEL( p ) ( reinterpret_cast<GraphTreeModel*>( p ) )

static GtkTreeModelFlags graph_tree_model_get_flags( GtkTreeModel* tree_model ){
	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint graph_tree_model_get_n_columns( GtkTreeModel* tree_model ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	//GraphTreeModel* graph_tree_model = (GraphTreeModel*) tree_model;

	return 2;
}

static const gint c_stamp = 0xabcdef;

inline GraphTreeNode::iterator graph_iterator_read_tree_iter( GtkTreeIter* iter ){
	ASSERT_MESSAGE( iter != 0,  "tree model error" );
	ASSERT_MESSAGE( iter->user_data != 0,  "tree model error" );
	ASSERT_MESSAGE( iter->stamp == c_stamp,  "tree model error" );
	return *reinterpret_cast<GraphTreeNode::iterator*>( &iter->user_data );
}

inline void graph_iterator_write_tree_iter( GraphTreeNode::iterator i, GtkTreeIter* iter ){
	ASSERT_MESSAGE( iter != 0,  "tree model error" );
	iter->stamp = c_stamp;
	*reinterpret_cast<GraphTreeNode::iterator*>( &iter->user_data ) = i;
	ASSERT_MESSAGE( iter->user_data != 0,  "tree model error" );
}

static GType graph_tree_model_get_column_type( GtkTreeModel *tree_model, gint index ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	//GraphTreeModel *graph_tree_model = (GraphTreeModel *) tree_model;

	return G_TYPE_POINTER;
}

static gboolean graph_tree_model_get_iter( GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	gint* indices = gtk_tree_path_get_indices( path );
	gint depth = gtk_tree_path_get_depth( path );

	g_return_val_if_fail( depth > 0, FALSE );

	GraphTreeNode* graph = GRAPH_TREE_MODEL( tree_model )->m_graph;

	if ( graph->empty() ) {
		return FALSE;
	}

	GtkTreeIter tmp;
	GtkTreeIter* parent = 0;

	for ( gint i = 0; i < depth; i++ )
	{
		if ( !gtk_tree_model_iter_nth_child( tree_model, iter, parent, indices[i] ) ) {
			return FALSE;
		}
		tmp = *iter;
		parent = &tmp;
	}

	return TRUE;
}

static GtkTreePath* graph_tree_model_get_path( GtkTreeModel* tree_model, GtkTreeIter* iter ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode* graph = GRAPH_TREE_MODEL( tree_model )->m_graph;

	GtkTreePath* path = gtk_tree_path_new();

	for ( GraphTreeNode* node = ( *graph_iterator_read_tree_iter( iter ) ).second; node != graph; node = node->m_parent )
	{
		std::size_t index = 0;
		for ( GraphTreeNode::iterator i = node->m_parent->begin(); i != node->m_parent->end(); ++i, ++index )
		{
			if ( ( *i ).second == node ) {
				gtk_tree_path_prepend_index( path, gint( index ) );
				break;
			}
		}
		ASSERT_MESSAGE( index != node->m_parent->size(), "error resolving tree path" );
	}

	return path;
}


static void graph_tree_model_get_value( GtkTreeModel *tree_model, GtkTreeIter  *iter, gint column, GValue *value ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	ASSERT_MESSAGE( column == 0 || column == 1, "tree model error" );

	GraphTreeNode::iterator i = graph_iterator_read_tree_iter( iter );

	g_value_init( value, G_TYPE_POINTER );

	if ( column == 0 ) {
		g_value_set_pointer( value, reinterpret_cast<gpointer>( ( *i ).first.second ) );
	}
	else
	{
		g_value_set_pointer( value, reinterpret_cast<gpointer>( &( *i ).second->m_instance.get() ) );
	}
}

static gboolean graph_tree_model_iter_next( GtkTreeModel  *tree_model, GtkTreeIter   *iter ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode::iterator i = graph_iterator_read_tree_iter( iter );
	GraphTreeNode& parent = *( *i ).second->m_parent;

	ASSERT_MESSAGE( i != parent.end(), "RUNTIME ERROR" );

	if ( ++i == parent.end() ) {
		return FALSE;
	}

	graph_iterator_write_tree_iter( i, iter );

	return TRUE;
}

static gboolean graph_tree_model_iter_children( GtkTreeModel *tree_model, GtkTreeIter  *iter, GtkTreeIter  *parent ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode& node = ( parent == 0 ) ? *GRAPH_TREE_MODEL( tree_model )->m_graph : *( *graph_iterator_read_tree_iter( parent ) ).second;
	if ( !node.empty() ) {
		graph_iterator_write_tree_iter( node.begin(), iter );
		return TRUE;
	}

	return FALSE;
}

static gboolean graph_tree_model_iter_has_child( GtkTreeModel *tree_model, GtkTreeIter  *iter ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode& node = *( *graph_iterator_read_tree_iter( iter ) ).second;
	return !node.empty();
}

static gint graph_tree_model_iter_n_children( GtkTreeModel *tree_model, GtkTreeIter *parent ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode& node = ( parent == 0 ) ? *GRAPH_TREE_MODEL( tree_model )->m_graph : *( *graph_iterator_read_tree_iter( parent ) ).second;
	return static_cast<gint>( node.size() );
}

static gboolean graph_tree_model_iter_nth_child( GtkTreeModel *tree_model, GtkTreeIter  *iter, GtkTreeIter  *parent, gint n ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode& node = ( parent == 0 ) ? *GRAPH_TREE_MODEL( tree_model )->m_graph : *( *graph_iterator_read_tree_iter( parent ) ).second;
	if ( static_cast<std::size_t>( n ) < node.size() ) {
		GraphTreeNode::iterator i = node.begin();
		std::advance( i, n );
		graph_iterator_write_tree_iter( i, iter );
		return TRUE;
	}

	return FALSE;
}

static gboolean graph_tree_model_iter_parent( GtkTreeModel *tree_model, GtkTreeIter  *iter, GtkTreeIter  *child ){
	ASSERT_MESSAGE( tree_model != 0, "RUNTIME ERROR" );
	GraphTreeNode& node = *( *graph_iterator_read_tree_iter( child ) ).second;
	if ( node.m_parent != GRAPH_TREE_MODEL( tree_model )->m_graph ) {
		GraphTreeNode& parentParent = *node.m_parent->m_parent;
		for ( GraphTreeNode::iterator i = parentParent.begin(); i != parentParent.end(); ++i )
		{
			if ( ( *i ).second == node.m_parent ) {
				graph_iterator_write_tree_iter( i, iter );
				return TRUE;
			}
		}
	}
	return FALSE;
}

static GObjectClass *g_parent_class = 0;

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

static void graph_tree_model_init( GraphTreeModel *graph_tree_model ){
	graph_tree_model->m_graph = new GraphTreeNode( g_null_instance );
}

static void graph_tree_model_finalize( GObject* object ){
	GraphTreeModel* graph_tree_model = GRAPH_TREE_MODEL( object );

	delete graph_tree_model->m_graph;

	/* must chain up */
	( *g_parent_class->finalize )( object );
}

static void graph_tree_model_class_init( GraphTreeModelClass *class_ ){
	GObjectClass *object_class;

	g_parent_class = (GObjectClass*)g_type_class_peek_parent( class_ );
	object_class = (GObjectClass *) class_;

	object_class->finalize = graph_tree_model_finalize;
}

static void graph_tree_model_tree_model_init( GtkTreeModelIface *iface ){
	iface->get_flags = graph_tree_model_get_flags;
	iface->get_n_columns = graph_tree_model_get_n_columns;
	iface->get_column_type = graph_tree_model_get_column_type;
	iface->get_iter = graph_tree_model_get_iter;
	iface->get_path = graph_tree_model_get_path;
	iface->get_value = graph_tree_model_get_value;
	iface->iter_next = graph_tree_model_iter_next;
	iface->iter_children = graph_tree_model_iter_children;
	iface->iter_has_child = graph_tree_model_iter_has_child;
	iface->iter_n_children = graph_tree_model_iter_n_children;
	iface->iter_nth_child = graph_tree_model_iter_nth_child;
	iface->iter_parent = graph_tree_model_iter_parent;
}

GType graph_tree_model_get_type( void ){
	static GType graph_tree_model_type = 0;

	if ( !graph_tree_model_type ) {
		static const GTypeInfo graph_tree_model_info =
		{
			sizeof( GraphTreeModelClass ),
			0, /* base_init */
			0, /* base_finalize */
			(GClassInitFunc) graph_tree_model_class_init,
			0, /* class_finalize */
			0, /* class_data */
			sizeof( GraphTreeModel ),
			0,        /* n_preallocs */
			(GInstanceInitFunc) graph_tree_model_init,
			0
		};

		static const GInterfaceInfo tree_model_info =
		{
			(GInterfaceInitFunc) graph_tree_model_tree_model_init,
			0,
			0
		};

		graph_tree_model_type = g_type_register_static( G_TYPE_OBJECT, "GraphTreeModel",
														&graph_tree_model_info, (GTypeFlags)0 );

		g_type_add_interface_static( graph_tree_model_type,
									 GTK_TYPE_TREE_MODEL,
									 &tree_model_info );
	}

	return graph_tree_model_type;
}

GraphTreeModel* graph_tree_model_new(){
	GraphTreeModel* graph_tree_model = GRAPH_TREE_MODEL( g_object_new( graph_tree_model_get_type(), 0 ) );

	return graph_tree_model;
}

void graph_tree_model_delete( GraphTreeModel* model ){
	g_object_unref( G_OBJECT( model ) );
}

void graph_tree_model_row_changed( GraphTreeModel* model, GraphTreeNode::iterator i ){
	GtkTreeIter iter;
	graph_iterator_write_tree_iter( i, &iter );

	GtkTreePath* tree_path = graph_tree_model_get_path( GTK_TREE_MODEL( model ), &iter );

	gtk_tree_model_row_changed( GTK_TREE_MODEL( model ), tree_path, &iter );

	gtk_tree_path_free( tree_path );
}

void graph_tree_model_row_inserted( GraphTreeModel* model, GraphTreeNode::iterator i ){
	GtkTreeIter iter;
	graph_iterator_write_tree_iter( i, &iter );

	GtkTreePath* tree_path = graph_tree_model_get_path( GTK_TREE_MODEL( model ), &iter );

	gtk_tree_model_row_inserted( GTK_TREE_MODEL( model ), tree_path, &iter );

	gtk_tree_path_free( tree_path );
}

void graph_tree_model_row_deleted( GraphTreeModel* model, GraphTreeNode::iterator i ){
	GtkTreeIter iter;
	graph_iterator_write_tree_iter( i, &iter );

	GtkTreePath* tree_path = graph_tree_model_get_path( GTK_TREE_MODEL( model ), &iter );

	gtk_tree_model_row_deleted( GTK_TREE_MODEL( model ), tree_path );

	gtk_tree_path_free( tree_path );
}

void graph_tree_model_row_inserted( GraphTreeModel& model, GraphTreeNode::iterator i ){
	graph_tree_model_row_inserted( &model, i );
}

void graph_tree_model_row_deleted( GraphTreeModel& model, GraphTreeNode::iterator i ){
	graph_tree_model_row_deleted( &model, i );
}

const char* node_get_name( scene::Node& node );

const char* node_get_name_safe( scene::Node& node ){
	volatile intptr_t n = (intptr_t)&node;  // see the comment on line 650
	if ( n == 0 ) {
		return "";
	}
	return node_get_name( node );
}

GraphTreeNode* graph_tree_model_find_parent( GraphTreeModel* model, const scene::Path& path ){
	GraphTreeNode* parent = model->m_graph;
	for ( scene::Path::const_iterator i = path.begin(); i != path.end() - 1; ++i )
	{
		GraphTreeNode::iterator child = parent->find( GraphTreeNode::key_type( node_get_name_safe( ( *i ).get() ), ( *i ).get_pointer() ) );
		ASSERT_MESSAGE( child != parent->end(), "ERROR" );
		parent = ( *child ).second;
	}
	return parent;
}

void node_attach_name_changed_callback( scene::Node& node, const NameCallback& callback ){
	volatile intptr_t n = (intptr_t)&node;  // see the comment on line 650
	if ( n != 0 ) {
		Nameable* nameable = Node_getNameable( node );
		if ( nameable != 0 ) {
			nameable->attach( callback );
		}
	}
}
void node_detach_name_changed_callback( scene::Node& node, const NameCallback& callback ){
	volatile intptr_t n = (intptr_t)&node;  // see the comment on line 650
	if ( n != 0 ) {
		Nameable* nameable = Node_getNameable( node );
		if ( nameable != 0 ) {
			nameable->detach( callback );
		}
	}
}

GraphTreeModel* scene_graph_get_tree_model(); // temp hack

void graph_tree_node_foreach_pre( GraphTreeNode::iterator root, const Callback1<GraphTreeNode::iterator>& callback ){
	callback( root );
	for ( GraphTreeNode::iterator i = ( *root ).second->begin(); i != ( *root ).second->end(); ++i )
	{
		graph_tree_node_foreach_pre( i, callback );
	}
}

void graph_tree_node_foreach_post( GraphTreeNode::iterator root, const Callback1<GraphTreeNode::iterator>& callback ){
	for ( GraphTreeNode::iterator i = ( *root ).second->begin(); i != ( *root ).second->end(); ++i )
	{
		graph_tree_node_foreach_post( i, callback );
	}
	callback( root );
}

void graph_tree_model_row_changed( GraphTreeNode& node ){
	GraphTreeModel* model = scene_graph_get_tree_model();
	const scene::Instance& instance = node.m_instance.get();

	GraphTreeNode::iterator i = node.m_parent->find( GraphTreeNode::key_type( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() ) );

	graph_tree_model_row_changed( model, i );
}

void graph_tree_model_set_name( const scene::Instance& instance, const char* name ){
	GraphTreeModel* model = scene_graph_get_tree_model();
	GraphTreeNode* parent = graph_tree_model_find_parent( model, instance.path() );

	GraphTreeNode::iterator oldNode = parent->find( GraphTreeNode::key_type( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() ) );
	graph_tree_node_foreach_post( oldNode, ReferenceCaller1<GraphTreeModel, GraphTreeNode::iterator, graph_tree_model_row_deleted>( *model ) );
	GraphTreeNode* node( ( *oldNode ).second );
	parent->erase( oldNode );

	GraphTreeNode::iterator newNode = parent->insert( GraphTreeNode::value_type( GraphTreeNode::key_type( name, &instance.path().top().get() ), node ) );
	graph_tree_node_foreach_pre( newNode, ReferenceCaller1<GraphTreeModel, GraphTreeNode::iterator, graph_tree_model_row_inserted>( *model ) );
}

void graph_tree_model_insert( GraphTreeModel* model, const scene::Instance& instance ){
	GraphTreeNode* parent = graph_tree_model_find_parent( model, instance.path() );

	GraphTreeNode::iterator i = parent->insert( GraphTreeNode::value_type( GraphTreeNode::key_type( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() ), new GraphTreeNode( const_cast<scene::Instance&>( instance ) ) ) );

	graph_tree_model_row_inserted( model, i );

	node_attach_name_changed_callback( instance.path().top(), ConstReferenceCaller1<scene::Instance, const char*, graph_tree_model_set_name>( instance ) );
}

void graph_tree_model_erase( GraphTreeModel* model, const scene::Instance& instance ){
	node_detach_name_changed_callback( instance.path().top(), ConstReferenceCaller1<scene::Instance, const char*, graph_tree_model_set_name>( instance ) );

	GraphTreeNode* parent = graph_tree_model_find_parent( model, instance.path() );

	GraphTreeNode::iterator i = parent->find( GraphTreeNode::key_type( node_get_name_safe( instance.path().top().get() ), instance.path().top().get_pointer() ) );

	graph_tree_model_row_deleted( model, i );

	GraphTreeNode* node( ( *i ).second );
	parent->erase( i );
	delete node;
}

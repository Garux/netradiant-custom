/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
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
 */

#include "layerswindow.h"

#include "layers.h"
#include <QApplication>
#include <QTreeWidget>
#include <QHeaderView>
#include <QBoxLayout>
#include <QToolButton>
#include <QDropEvent>
#include <QMimeData>
#include <QIcon>
#include <QMenu>
#include <QInputDialog>
#include <QTimer>
#include "map.h"
#include "ientity.h"
#include "generic/callback.h"
#include "signal/signal.h"
#include "gtkutil/image.h"
#include "gtkmisc.h"
#include "windowobservers.h"

/*
	?todo operator[] range check // checked during loading
	layer color
	restrict " in the name, empty name
	'move to layer' context menu on keybind
? tree 'move to layer' menu
	def line format extensible
? extensibility with new root keywords
	handle paste, clone, map import
	new layer = make current
	hide recursively
	always nullptr layer for group ents node
	hide/show group entity nodes automatically
	don't select hidden nodes
	non locked 0 layer //may be confusing for user when falling back to default layer in operations, it's implicitly 1st in the list//?write 0 layer for future
? feedback which layers are selected //highlight tree items
? feedback items count in a layer
? undo
	button to add a layer
	buttons to hide/show all layers
? tree item highlight is torn with 'default' theme in windows
*/

class LayerAssignVisitor : public SelectionSystem::Visitor
{
	Layer *m_layer;
public:
	LayerAssignVisitor( Layer *layer ) : m_layer( layer ){
	}
	void visit( scene::Instance& instance ) const override {
		scene::Node& node = instance.path().top().get();
		if( Entity *entity = Node_getEntity( node ); entity == nullptr || !entity->isContainer() )
			node.m_layer = m_layer;
	}
};

class LayerCollectVisitor : public SelectionSystem::Visitor
{
	std::vector<Layer*>& m_layers;
public:
	LayerCollectVisitor( std::vector<Layer*>& layers ) : m_layers( layers ){
	}
	void visit( scene::Instance& instance ) const override {
		Layer *layer = instance.path().top().get().m_layer;
		if( layer != nullptr && std::ranges::find( m_layers, layer ) == m_layers.cend() )
			m_layers.push_back( layer );
	}
};

class LayerHideWalker : public scene::Graph::Walker
{
	const std::vector<Layer*>& m_layers;
public:
	LayerHideWalker( const std::vector<Layer*>& layers ) : m_layers( layers ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		scene::Node& node = path.top().get();
		if( std::ranges::find( m_layers, node.m_layer ) != m_layers.cend() ){
			node.enable( scene::Node::eLayerHidden );
			if( Selectable* selectable = Instance_getSelectable( instance ) )
				selectable->setSelected( false );
		}
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const override {
		scene::Node& node = path.top().get();
		if( Node_isEntity( node ) ) // hide group entity labels, when their content is entirely hidden
			if( scene::Traversable* traversable = Node_getTraversable( node ) )
				if( Traversable_all_of_children( traversable, []( const scene::Node& node ){ return node.excluded( scene::Node::eLayerHidden ); } ) )
					node.enable( scene::Node::eLayerHidden );
	}
};
class LayerShowWalker : public scene::Graph::Walker
{
	const std::vector<Layer*>& m_layers;
public:
	LayerShowWalker( const std::vector<Layer*>& layers ) : m_layers( layers ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		scene::Node& node = path.top().get();
		if( std::ranges::find( m_layers, node.m_layer ) != m_layers.cend() ){
			node.disable( scene::Node::eLayerHidden );
		}
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const override {
		scene::Node& node = path.top().get();
		if( Node_isEntity( node ) ) // unhide group entity labels, when their content is partially visible
			if( scene::Traversable* traversable = Node_getTraversable( node ) )
				if( Traversable_any_of_children( traversable, []( const scene::Node& node ){
				return !node.excluded( scene::Node::eLayerHidden ) && !node.isRoot(); } ) ) // also not misc_model content node
					node.disable( scene::Node::eLayerHidden );
	}
};

class LayerSelectWalker : public scene::Graph::Walker
{
	const std::vector<Layer*>& m_layers;
	const bool m_select;
public:
	LayerSelectWalker( const std::vector<Layer*>& layers, bool select ) : m_layers( layers ), m_select( select ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		scene::Node& node = path.top().get();
		if( node.visible() && std::ranges::find( m_layers, node.m_layer ) != m_layers.cend() ){
			Instance_setSelected( instance, m_select );
		}
		return true;
	}
};

class LayerDeletetWalker : public scene::Graph::Walker
{
	const std::vector<Layer*>& m_layers;
	Layer *m_currentLayer;
public:
	LayerDeletetWalker( const std::vector<Layer*>& layers, Layer *currentLayer ) : m_layers( layers ), m_currentLayer( currentLayer ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		scene::Node& node = path.top().get();
		if( std::ranges::find( m_layers, node.m_layer ) != m_layers.cend() ){
			node.m_layer = m_currentLayer;
		}
		return true;
	}
};



class LayersBrowser
{
public:
	QTreeWidget *m_tree = nullptr;
	QMenu *m_movetoMenu = nullptr;
	QIcon m_iconSelect;
	QIcon m_iconSelectAdd;
	QIcon m_iconSelectDeselect;
	QIcon m_iconSelectMove;
	QIcon m_iconVisibleOn;
	QIcon m_iconVisibleOff;
};

LayersBrowser g_lbro;


namespace Column
{
enum
{
	name,
	current,
	select,
	visible,
};
};

template<class Functor>
	requires( std::invocable<Functor, QTreeWidgetItem*> && std::is_void_v<std::invoke_result_t<Functor, QTreeWidgetItem*>> )
void items_iterate_recursively( QTreeWidgetItem *item, Functor&& functor ){
	functor( item );
	for( int i = 0; i < item->childCount(); ++i )
		items_iterate_recursively( item->child( i ), std::forward<Functor>( functor ) );
}

Layer::iterator item_getLayerIterator( QTreeWidgetItem *item ){
	static_assert( sizeof( Layer::iterator ) == sizeof( size_t ) );
	size_t layer = item->data( Column::name, Qt::ItemDataRole::UserRole ).value<size_t>();
	return *reinterpret_cast<Layer::iterator*>( &layer );
}
Layer* item_getLayer( QTreeWidgetItem *item ){
	return item_getLayerIterator( item ).operator->();
}
std::vector<Layer*> item_getLayers( QTreeWidgetItem *item ){
	std::vector<Layer*> layers;
	items_iterate_recursively( item, [&]( QTreeWidgetItem *item ){ layers.push_back( item_getLayer( item ) ); } );
	return layers;
}
std::vector<Layer::iterator> item_getLayersIterators( QTreeWidgetItem *item ){
	std::vector<Layer::iterator> layers;
	items_iterate_recursively( item, [&]( QTreeWidgetItem *item ){ layers.push_back( item_getLayerIterator( item ) ); } );
	return layers;
}

void item_setCurrent( QTreeWidgetItem *item ){
	for( QTreeWidgetItemIterator it( item->treeWidget() ); *it; ++it )
		( *it )->setCheckState( Column::current, Qt::CheckState::Unchecked );

	item->setCheckState( Column::current, Qt::CheckState::Checked );
	Node_getLayers( GlobalSceneGraph().root() )->m_currentLayer = item_getLayer( item );
}

void item_setIcons( QTreeWidgetItem *item ){
	item->setIcon( Column::select, g_lbro.m_iconSelect );
	item->setIcon( Column::visible, item->data( Column::visible, Qt::ItemDataRole::UserRole ).toBool()? g_lbro.m_iconVisibleOn : g_lbro.m_iconVisibleOff );
}

void item_setColor( QTreeWidgetItem *item, const int ( &color )[ 3 ] ){
	if( color[ 0 ] == 0 && color[ 1 ] == 0 && color[ 2 ] == 0 )
		item->setBackground( Column::current, {} );
	else
		item->setBackground( Column::current, QColor( color[ 0 ], color[ 1 ], color[ 2 ] ) );
}

void item_construct( QTreeWidgetItem *item, Layer::iterator layer ){
	item->setText( Column::name, layer->m_name.c_str() );
	item->setData( Column::name, Qt::ItemDataRole::UserRole, QVariant::fromValue( *reinterpret_cast<size_t*>( &layer ) ) ); // Q_DECLARE_METATYPE( Layer* ) = crash during drag and drop
	item->setData( Column::visible, Qt::ItemDataRole::UserRole, true ); // save toggle state
	item->setToolTip( Column::current, "Set current layer" );
	item->setToolTip( Column::select, "CLICK = Select\nCTRL = multiple\nALT = deselect\nSHIFT = Move to layer" );
	item->setToolTip( Column::visible, "Toggle visibility" );
	item_setIcons( item );
	item_setColor( item, layer->m_color );
	item->setCheckState( Column::current, Qt::CheckState::Unchecked ); // make checkable
}

void insert_children( QTreeWidgetItem *parentItem, std::list<Layer>& children ){
	for( auto layer = children.begin(); layer != children.end(); ++layer )
	{
		auto *item = new QTreeWidgetItem( parentItem );
		item_construct( item, layer );
		insert_children( item, layer->m_children );
	}
}

void LayersBrowser_constructTree( QTreeWidget *tree ){
	if( Map_Valid( g_map ) ){
		Layers& layers = *Node_getLayers( GlobalSceneGraph().root() );
		tree->clear();
		insert_children( tree->invisibleRootItem(), layers.m_children );
		tree->topLevelItem( LAYERIDX0 )->setCheckState( Column::current, Qt::CheckState::Checked );
		tree->expandAll();
	}
}


class QTreeWidget_layers : public QTreeWidget, public WindowObserver
{
protected:
	bool m_drop = false;
	void rowsInserted( const QModelIndex& parent, int start, int end ) override {
		if( std::exchange( m_drop, false ) ){ // insertion source is drop
			QTreeWidgetItem *parentItem = parent.isValid()? this->itemFromIndex( parent ) : this->invisibleRootItem();
			QTreeWidgetItem *item = parentItem->child( start );
			Layer *parentLayer = parent.isValid()? item_getLayer( parentItem ) : Node_getLayers( GlobalSceneGraph().root() );
			Layer::iterator layer = item_getLayerIterator( item );
			auto& srcList = layer->m_parent->m_children;
			// move to the end of list 1st, so that resulting 'start' index can be used correctly, when moving inside a list farther
			srcList.splice( srcList.cend(), srcList, layer );
			parentLayer->m_children.splice( std::next( parentLayer->m_children.cbegin(), start ), srcList, layer );
			layer->m_parent = parentLayer;
		}
		QTreeWidget::rowsInserted( parent, start, end );
		expandAll();
	}
	// bool positionAboveItem0( const QPoint& pos ) const {
	// 	const QModelIndex index = indexAt( pos );
	// 	return index.row() == LAYERIDX0
	// 		&& !index.parent().isValid() // in the root
	// 		&& pos.y() < visualRect( index ).center().y();
	// }
	void dragMoveEvent( QDragMoveEvent* event ) override {
		// if( positionAboveItem0( event->pos() ) ){
		// 	event->ignore();
		// 	return;
		// }

		QTreeWidget::dragMoveEvent( event );
	}
	void dropEvent( QDropEvent* event ) override {
		// if( positionAboveItem0( event->pos() ) ){
		// 	event->ignore();
		// 	return;
		// }
		ASSERT_MESSAGE( !m_drop, "dropEvent() without rowsInserted()" );
		m_drop = true;
		QTreeWidget::dropEvent( event );
	}
private:
	ModifierFlags m_modifiers;
	void set_select_icons(){
		if( this->isVisible() ){
			QIcon& icon = m_modifiers == c_modifierControl? g_lbro.m_iconSelectAdd
			            : m_modifiers == c_modifierAlt?     g_lbro.m_iconSelectDeselect
			            : m_modifiers == c_modifierShift?   g_lbro.m_iconSelectMove
			            :                                   g_lbro.m_iconSelect;
			for( QTreeWidgetItemIterator it( this ); *it; ++it )
				( *it )->setIcon( Column::select, icon );
		}
	}
public:
	void release() override {};
	void onSizeChanged( int width, int height ) override {};
	void onMouseDown( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ) override {};
	void onMouseUp( const WindowVector& position, ButtonIdentifier button, ModifierFlags modifiers ) override {};
	void onMouseMotion( const WindowVector& position, ModifierFlags modifiers ) override {};
	void onModifierDown( ModifierFlags modifier ) override {
		m_modifiers = bitfield_enable( m_modifiers, modifier );
		set_select_icons();
	}
	void onModifierUp( ModifierFlags modifier ) override {
		m_modifiers = bitfield_disable( m_modifiers, modifier );
		set_select_icons();
	}
};


void layer_new( QTreeWidgetItem *parentItem, Layer *parentLayer, Layers& layers, QTreeWidget *tree ){
	bool ok;
	QString text = QInputDialog::getText( tree, "Layer Name", "Enter layer name:", QLineEdit::EchoMode::Normal,
	                                      "UnnamedpLayer", &ok );
	text.remove( '"' ); // remove token bounds symbol
	if( ok && !text.isEmpty() ){
		parentItem = parentItem? parentItem : tree->invisibleRootItem();
		parentLayer = parentLayer? parentLayer : &layers;

		parentLayer->m_children.emplace_back( text.toLatin1().constData(), parentLayer );
		auto *newItem = new QTreeWidgetItem( parentItem );
		item_construct( newItem, --parentLayer->m_children.end() );
		item_setCurrent( newItem );
	}
}

void context_menu( const QPoint& pos ){
	QTreeWidget *tree = g_lbro.m_tree;
	auto *menu = new QMenu( tree );
	menu->setAttribute( Qt::WA_DeleteOnClose );

	QTreeWidgetItem *item = tree->itemAt( pos );
	Layer *layer = item? item_getLayer( item ) : nullptr;
	Layers& layers = *Node_getLayers( GlobalSceneGraph().root() );

	menu->addAction( "Move Selection to This Layer", [&](){
		GlobalSelectionSystem().foreachSelected( LayerAssignVisitor( layer ) );
	} )->setDisabled( item == nullptr );

	menu->addAction( g_lbro.m_iconSelectAdd, "New Layer", [&](){
		layer_new( item, layer, layers, tree );
	} );

	menu->addAction( new_local_icon( "delete.png" ), "Delete", [&](){
		auto dels = item_getLayers( item );
		auto it = item_getLayerIterator( item );

		delete item;
		// handle possible deletion of layers.m_currentLayer 1st, as it will be used as a destination later
		if( std::ranges::find( dels, layers.m_currentLayer ) != dels.cend() )
			item_setCurrent( tree->topLevelItem( LAYERIDX0 ) );

		GlobalSceneGraph().traverse( LayerDeletetWalker( dels, layers.m_currentLayer ) );
		it->m_parent->m_children.erase( it );
	} )->setDisabled( item == nullptr
	             || ( item->parent() == nullptr && tree->topLevelItemCount() == 1 ) ); // trying to delete the only toplevel item

	menu->addAction( "Rename", [&](){
		bool ok;
		QString text = QInputDialog::getText( tree, "Layer Name", "Enter layer name:", QLineEdit::EchoMode::Normal,
		                                      item->text( Column::name ), &ok );
		text.remove( '"' ); // remove token bounds symbol
		if( ok && !text.isEmpty() ){
			item->setText( Column::name, text );
			layer->m_name = text.toLatin1().constData();
		}
	} )->setDisabled( item == nullptr );

	menu->addAction( "Color", [&](){
		Vector3 color( layer->m_color[ 0 ] / 255.f, layer->m_color[ 1 ] / 255.f, layer->m_color[ 2 ] / 255.f );
		if( color_dialog( tree, color ) ){
			layer->m_color[ 0 ] = color[ 0 ] * 255;
			layer->m_color[ 1 ] = color[ 1 ] * 255;
			layer->m_color[ 2 ] = color[ 2 ] * 255;
			item_setColor( item, layer->m_color );
		}
	} )->setDisabled( item == nullptr );

	menu->exec( tree->mapToGlobal( pos ) );
}

void context_moveto_menu(){
	if( !g_lbro.m_movetoMenu->isVisible() ){
		g_lbro.m_movetoMenu->clear();

		Node_getLayers( GlobalSceneGraph().root() )->forEach( []( Layer& layer ){
			g_lbro.m_movetoMenu->addAction( QString( "-> " ) + layer.m_name.c_str(), [layer = &layer](){
				GlobalSelectionSystem().foreachSelected( LayerAssignVisitor( layer ) );
			} )->setDisabled( GlobalSelectionSystem().countSelected() == 0 );
		} );

		g_lbro.m_movetoMenu->exec( QCursor::pos() );
	}
}

class LayersSetVisible
{
	const bool m_setVisible;
	std::vector<Layer *> m_layers;
public:
	LayersSetVisible( bool setVisible ) : m_setVisible( setVisible ){
	}
	void operator()( QTreeWidgetItem *item ){
		m_layers.push_back( item_getLayer( item ) );
		item->setData( Column::visible, Qt::ItemDataRole::UserRole, m_setVisible );
		item_setIcons( item );
	}
	void setVisible() const {
		if( m_setVisible ){
			GlobalSceneGraph().traverse( LayerShowWalker( m_layers ) );
		}
		else{
			GlobalSceneGraph().traverse( LayerHideWalker( m_layers ) );
			/* not hiding worldspawn node so that newly created brushes are visible */
			if( scene::Node* w = Map_FindWorldspawn( g_map ) )
				w->disable( scene::Node::eLayerHidden );
			if( GlobalSelectionSystem().countSelectedComponents() != 0 )
				GlobalSelectionSystem().setSelectedAllComponents( false );
		}
		SceneChangeNotify();
	}
};

void layers_setVisible_all( bool setVisible ){
	LayersSetVisible layersSetVisible( setVisible );
	for( QTreeWidgetItemIterator it( g_lbro.m_tree ); *it; ++it )
		layersSetVisible( *it );
	layersSetVisible.setVisible();
}

void itemClicked( QTreeWidgetItem *item, int column ){
	if( item != nullptr ){
		if( column == Column::select ){
			const Qt::KeyboardModifiers kb = QApplication::keyboardModifiers();
			if( kb == Qt::KeyboardModifier::NoModifier || kb == Qt::KeyboardModifier::ControlModifier || kb == Qt::KeyboardModifier::AltModifier ){
				if( kb != Qt::KeyboardModifier::ControlModifier && kb != Qt::KeyboardModifier::AltModifier)
					GlobalSelectionSystem().setSelectedAll( false );
				GlobalSceneGraph().traverse( LayerSelectWalker( item_getLayers( item ), kb != Qt::KeyboardModifier::AltModifier ) );
			}
			else if( kb == Qt::KeyboardModifier::ShiftModifier ){
				GlobalSelectionSystem().foreachSelected( LayerAssignVisitor( item_getLayer( item ) ) );
			}
		}
		else if( column == Column::visible ){
			LayersSetVisible layersSetVisible( !item->data( Column::visible, Qt::ItemDataRole::UserRole ).toBool() );
			items_iterate_recursively( item, layersSetVisible );
			layersSetVisible.setVisible();
		}
		else if( column == Column::current ){
			item_setCurrent( item );
		}
	}
}

class QWidget* LayersBrowser_constructWindow( QWidget* toplevel ){
	auto *containerWidget = new QWidget;
	auto *vbox = new QVBoxLayout( containerWidget );
	vbox->setContentsMargins( 0, 0, 0, 0 );

	auto *tree = new QTreeWidget_layers;
	g_lbro.m_tree = tree;
	vbox->addWidget( tree );

	tree->setColumnCount( 4 );
	tree->setUniformRowHeights( true ); // optimization
	tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
	tree->header()->setStretchLastSection( false ); // non greedy column sizing //makes it greedy for name section here
	tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );
	tree->header()->setSectionResizeMode( Column::name, QHeaderView::ResizeMode::Stretch );
	tree->setHeaderHidden( true );
	tree->setAutoScroll( true );
	tree->setAlternatingRowColors( true );
	const int iconSize = tree->style()->pixelMetric( QStyle::PixelMetric::PM_SmallIconSize ) * 1.3;
	tree->setIconSize( QSize( iconSize, iconSize) );
	// tree->setStyleSheet( "QTreeWidget::item { border-bottom: 1px solid gray; }" );
	tree->setDragDropMode( QAbstractItemView::DragDropMode::InternalMove );

	tree->setContextMenuPolicy( Qt::ContextMenuPolicy::CustomContextMenu );
	QObject::connect( tree, &QTreeWidget::customContextMenuRequested, context_menu );

	tree->header()->swapSections( Column::current, Column::name ); // display current indicator 1st, tree second

	QObject::connect( tree, &QTreeWidget::itemClicked, itemClicked );
	QObject::connect( tree, &QTreeWidget::itemDoubleClicked, itemClicked ); // workaround fast clicks being not registered

	// Connect to itemCollapsed signal to prevent collapsing
    QObject::connect( tree, &QTreeWidget::itemCollapsed, []( QTreeWidgetItem *item ){ item->setExpanded( true ); });

	g_lbro.m_movetoMenu = new QMenu( tree );

	g_lbro.m_iconSelect = new_local_icon( "check_mark.png" );
	g_lbro.m_iconSelectAdd = new_local_icon( "plus.png" );
	g_lbro.m_iconSelectDeselect = new_local_icon( "minus.png" );
	g_lbro.m_iconSelectMove = new_local_icon( "arrow_right.png" );
	g_lbro.m_iconVisibleOn = new_local_icon( "eye_open.png" );
	g_lbro.m_iconVisibleOff = new_local_icon( "eye_closed.png" );

	{
		auto *hbox = new QHBoxLayout;
		vbox->addLayout( hbox );
		hbox->setAlignment( Qt::AlignmentFlag::AlignRight );
		auto newButton = [&]( const QIcon& icon, const char *tooltip ){
			auto *butt = new QToolButton;
			butt->setIcon( icon );
			butt->setToolTip( tooltip );
			hbox->addWidget( butt );
			return butt;
		};
		QObject::connect( newButton( g_lbro.m_iconSelectAdd, "New Layer" ), &QToolButton::clicked, [](){
			layer_new( nullptr, nullptr, *Node_getLayers( GlobalSceneGraph().root() ), g_lbro.m_tree );
		} );
		QObject::connect( newButton( g_lbro.m_iconVisibleOn, "Show All Layers" ), &QToolButton::clicked, [](){ layers_setVisible_all( true ); } );
		QObject::connect( newButton( g_lbro.m_iconVisibleOff, "Hide All Layers" ), &QToolButton::clicked, [](){ layers_setVisible_all( false ); } );
	}

	GlobalWindowObservers_add( tree ); // track modifiers for Column::select icons toggle
	Map_addValidCallback( g_map, PointerCaller<QTreeWidget, void(), LayersBrowser_constructTree>( g_lbro.m_tree ) );

	return containerWidget;
}

void LayersBrowser_destroyWindow(){
	g_lbro.m_tree = nullptr;
}


void Scene_ExpandSelectionToLayers(){
	std::vector<Layer*> layers;
	GlobalSelectionSystem().foreachSelected( LayerCollectVisitor( layers ) );
	for( size_t i = 0, end = layers.size(); i < end; ++i ){ // vector may be changed during loop
		layers[ i ]->forEach( [&]( Layer& layer ){
			if( std::ranges::find( layers, &layer ) == layers.cend() )
				layers.push_back( &layer );
		} );
	}
	GlobalSceneGraph().traverse( LayerSelectWalker( layers, true ) );
}

#include "commands.h"
void Layers_registerCommands(){
	GlobalCommands_insert( "ExpandSelectionToLayers", makeCallbackF( Scene_ExpandSelectionToLayers ), QKeySequence( "Ctrl+Q" ) );
	GlobalCommands_insert( "LayersMenu", makeCallbackF( context_moveto_menu ), QKeySequence( "Shift+Q" ) );
}

void Layers_registerShortcuts(){
	command_connect_accelerator( "LayersMenu" );
}


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

#include "entitylist.h"

#include "iselection.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QCheckBox>
#include <QLineEdit>
#include <QHeaderView>
#include <QApplication>
#include <QAction>
#include <QKeyEvent>

#include "string/string.h"
#include "scenelib.h"
#include "nameable.h"
#include "signal/isignal.h"
#include "generic/object.h"

#include "gtkutil/widget.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/guisettings.h"

#include "treemodel.h"

#include "mainframe.h"

void RedrawEntityList();
typedef FreeCaller<void(), RedrawEntityList> RedrawEntityListCaller;

class EntityList
{
public:
	enum EDirty
	{
		eDefault,
		eSelection,
		eInsertRemove,
	};

	EDirty m_dirty;

	IdleDraw m_idleDraw;

	QWidget* m_window;
	QCheckBox* m_check;
	QTreeView* m_tree_view;
	QAbstractItemModel* m_tree_model;
	bool m_selection_disabled;

	bool m_search_from_start;

	EntityList() :
		m_dirty( EntityList::eDefault ),
		m_idleDraw( RedrawEntityListCaller() ),
		m_window( 0 ),
		m_selection_disabled( false ),
		m_search_from_start( false ){
	}

	bool visible() const {
		return m_window->isVisible();
	}
};

namespace
{
EntityList* g_EntityList;

inline EntityList& getEntityList(){
	ASSERT_NOTNULL( g_EntityList );
	return *g_EntityList;
}
}

void entitylist_focusSelected( bool checked ){
	if( checked )
		FocusAllViews();
}

template<typename Functor>
void item_model_foreach( Functor f, QAbstractItemModel* model, QModelIndex parent = QModelIndex() )
{
	if ( !parent.isValid() )
		parent = model->index( 0, 0, QModelIndex() );

	const int numRows = model->rowCount( parent );

	for ( int i = 0; i < numRows; ++i )
		item_model_foreach( f, model, model->index( i, 0, parent ) );

	f( parent );
}

void EntityList_UpdateSelection( QAbstractItemModel* model, QTreeView* view ){
	item_model_foreach( [view]( QModelIndex &index ){
		scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
		if ( Selectable* selectable = Instance_getSelectable( *instance ) ) {
			view->selectionModel()->select( index, selectable->isSelected()
			                                     ? QItemSelectionModel::SelectionFlag::Select
			                                     : QItemSelectionModel::SelectionFlag::Deselect );
		}
	}, model );
}


void RedrawEntityList(){
	switch ( getEntityList().m_dirty )
	{
	case EntityList::eInsertRemove:
	case EntityList::eSelection:
		EntityList_UpdateSelection( getEntityList().m_tree_model, getEntityList().m_tree_view );
	default:
		break;
	}
	getEntityList().m_dirty = EntityList::eDefault;
}

void entitylist_queue_draw(){
	getEntityList().m_idleDraw.queueDraw();
}

void EntityList_SelectionUpdate(){
	if( getEntityList().visible() ){
		// deselect tree items on deseletion in the scene for some degree of consistency
		if( getEntityList().m_tree_view->selectionModel()->hasSelection() ){
			QTimer::singleShot( 0, [](){
				for( const auto& index : getEntityList().m_tree_view->selectionModel()->selectedRows( 0 ) ){
					scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
					if ( Selectable* selectable = Instance_getSelectable( *instance ) )
						if( !selectable->isSelected() )
							getEntityList().m_tree_view->selectionModel()->select( index, QItemSelectionModel::SelectionFlag::Deselect );
				}
			} );
		}

		getEntityList().m_tree_view->viewport()->update(); // reads in Qt::ItemDataRole::BackgroundRole of visible items
	}
	return; //. making actual selection is ULTRA SLOW :F thus using Qt::ItemDataRole::BackgroundRole instead

	if ( getEntityList().m_selection_disabled ) {
		return;
	}

	if ( getEntityList().m_dirty < EntityList::eSelection ) {
		getEntityList().m_dirty = EntityList::eSelection;
	}
	entitylist_queue_draw();
}

void EntityList_SelectionChanged( const Selectable& selectable ){
	EntityList_SelectionUpdate();
}


void EntityList_SetShown( bool shown ){
	getEntityList().m_window->setVisible( shown );
	if( shown ){ /* expand map's root node for convenience */
		auto index = getEntityList().m_tree_model->index( 0, 0 );
		if( index.isValid() && !getEntityList().m_tree_view->isExpanded( index ) )
			getEntityList().m_tree_view->expand( index );
	}
}

void EntityList_toggleShown(){
	EntityList_SetShown( !getEntityList().visible() );
}

class Filter_QLineEdit : public QLineEdit
{
protected:
	void enterEvent( QEvent *event ) override {
		setFocus();
	}
	void leaveEvent( QEvent *event ) override {
		clearFocus();
	}
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent*>( event );
			if( keyEvent->key() == Qt::Key_Escape ){
				clear();
				event->accept();
			}
		}
		else if( event->type() == QEvent::Hide ){
			if( !text().isEmpty() )
				clear(); // workaround: reset filtering, as modification of tree after setRowHidden() is ULTRA SLOW
		}
		return QLineEdit::event( event );
	}
};

void searchEntrySetModeIcon( QAction *action, bool search_from_start ){
	action->setIcon( QApplication::style()->standardIcon(
		search_from_start
		? QStyle::StandardPixmap::SP_CommandLink
		: QStyle::StandardPixmap::SP_FileDialogContentsView ) );
}

/* search */
void tree_view_filter( const char *string, bool from_start ){
	const auto traverse = [=]( const auto& self, QAbstractItemModel* model, QTreeView *tree, QModelIndex parent = QModelIndex() ) -> bool {
		if ( !parent.isValid() )
			parent = model->index( 0, 0, QModelIndex() );

		const int numRows = model->rowCount( parent );

		bool childVisible = false;

		for ( int i = 0; i < numRows; ++i )
			childVisible |= self( self, model, tree, model->index( i, 0, parent ) );

		const bool visible = childVisible || string_empty( string )
			|| ( from_start && string_equal_prefix_nocase( parent.data( Qt::ItemDataRole::DisplayRole ).toString().toLatin1().constData(), string ) )
			|| ( !from_start && string_in_string_nocase( parent.data( Qt::ItemDataRole::DisplayRole ).toString().toLatin1().constData(), string ) );
		tree->setRowHidden( parent.row(), parent.parent(), !visible );

		return childVisible || visible;
	};

	traverse( traverse, getEntityList().m_tree_model, getEntityList().m_tree_view );
}



extern QAbstractItemModel* scene_graph_get_tree_model();
void AttachEntityTreeModel(){
	getEntityList().m_tree_model = scene_graph_get_tree_model();
	getEntityList().m_tree_view->setModel( getEntityList().m_tree_model );

	QObject::connect( getEntityList().m_tree_view->selectionModel(), &QItemSelectionModel::selectionChanged,
		[]( const QItemSelection &selected, const QItemSelection &deselected ){
			for( const auto& index : deselected.indexes() ){
				scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
				if( Selectable* selectable = Instance_getSelectable( *instance ) )
					selectable->setSelected( false );
			}
			for( const auto& index : selected.indexes() ){
				scene::Instance* instance = static_cast<scene::Instance*>( index.data( c_ItemDataRole_Instance ).value<void*>() );
				if( Selectable* selectable = Instance_getSelectable( *instance ) )
					selectable->setSelected( true );
			}
			if( !selected.empty() && getEntityList().m_check->isChecked() )
				FocusAllViews();
		} );
}

void DetachEntityTreeModel(){
	getEntityList().m_tree_model = 0;
	getEntityList().m_tree_view->setModel( nullptr );
}

void EntityList_constructWindow( QWidget* main_window ){
	ASSERT_MESSAGE( getEntityList().m_window == 0, "error" );

	auto window = getEntityList().m_window = new QWidget( main_window, Qt::Dialog | Qt::WindowCloseButtonHint );
	window->setWindowTitle( "Entity List" );

	g_guiSettings.addWindow( window, "EntityList/geometry", 350, 500 );
	{
		auto *vbox = new QVBoxLayout( window );
		vbox->setContentsMargins( 4, 0, 4, 4 );
		{
			auto *tree = getEntityList().m_tree_view = new QTreeView;
			tree->setHeaderHidden( true );
			tree->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
			tree->setUniformRowHeights( true ); // optimization
			tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
			tree->header()->setStretchLastSection( false ); // non greedy column sizing; + QHeaderView::ResizeMode::ResizeToContents = no text elision 🤷‍♀️
			tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );
			tree->setSelectionMode( QAbstractItemView::SelectionMode::ExtendedSelection );
			vbox->addWidget( tree );
		}
		{
			auto *hbox = new QHBoxLayout;
			vbox->addLayout( hbox );
			{
				auto *check = getEntityList().m_check = new QCheckBox( "AutoFocus on Selection" );
				hbox->addWidget( check );
				QObject::connect( check, &QAbstractButton::clicked, entitylist_focusSelected );
			}
			{	//search entry
				QLineEdit *entry = new Filter_QLineEdit;
				hbox->addWidget( entry );
				entry->setClearButtonEnabled( true );
				entry->setFocusPolicy( Qt::FocusPolicy::ClickFocus );

				QAction *action = entry->addAction( QApplication::style()->standardIcon( QStyle::StandardPixmap::SP_CommandLink ), QLineEdit::LeadingPosition );
				searchEntrySetModeIcon( action, getEntityList().m_search_from_start );
				action->setToolTip( "toggle match mode ( start / any position )" );

				QObject::connect( entry, &QLineEdit::textChanged, []( const QString& text ){
					tree_view_filter( text.toLatin1().constData(), getEntityList().m_search_from_start );
				} );
				QObject::connect( action, &QAction::triggered, [action, entry](){
					getEntityList().m_search_from_start ^= 1;
					searchEntrySetModeIcon( action, getEntityList().m_search_from_start );
					entry->textChanged( entry->text() ); // trigger filtering update
				} );
			}
		}
	}

	AttachEntityTreeModel();
}

void EntityList_destroyWindow(){
	DetachEntityTreeModel();
	delete getEntityList().m_window;
}

#include "preferencesystem.h"

#include "stringio.h"

void EntityList_Construct(){
	g_EntityList = new EntityList;

	GlobalPreferenceSystem().registerPreference( "EntListSearchFromStart", BoolImportStringCaller( getEntityList().m_search_from_start ), BoolExportStringCaller( getEntityList().m_search_from_start ) );

	typedef FreeCaller<void(const Selectable&), EntityList_SelectionChanged> EntityListSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( EntityListSelectionChangedCaller() );
}
void EntityList_Destroy(){
	delete g_EntityList;
}

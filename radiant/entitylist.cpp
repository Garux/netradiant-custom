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

#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>

#include "string/string.h"
#include "scenelib.h"
#include "nameable.h"
#include "signal/isignal.h"
#include "generic/object.h"

#include "gtkutil/widget.h"
#include "gtkutil/window.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/closure.h"

#include "treemodel.h"

#include "mainframe.h"

void RedrawEntityList();
typedef FreeCaller<RedrawEntityList> RedrawEntityListCaller;

typedef struct _GtkTreeView GtkTreeView;

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
WindowPositionTracker m_positionTracker;

GtkWindow* m_window;
GtkWidget* m_check;
GtkTreeView* m_tree_view;
GraphTreeModel* m_tree_model;
bool m_selection_disabled;

bool m_search_from_start;
scene::Node* m_search_focus_node;

EntityList() :
	m_dirty( EntityList::eDefault ),
	m_idleDraw( RedrawEntityListCaller() ),
	m_window( 0 ),
	m_selection_disabled( false ),
	m_search_from_start( false ),
	m_search_focus_node( 0 ){
		m_positionTracker.setPosition( WindowPosition( -1, -1, 350, 500 ) );
}

bool visible() const {
	return GTK_WIDGET_VISIBLE( GTK_WIDGET( m_window ) );
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


inline Nameable* Node_getNameable( scene::Node& node ){
	return NodeTypeCast<Nameable>::cast( node );
}

const char* node_get_name( scene::Node& node ){
	Nameable* nameable = Node_getNameable( node );
	return ( nameable != 0 )
		   ? nameable->name()
		   : "node";
}

template<typename value_type>
inline void gtk_tree_model_get_pointer( GtkTreeModel* model, GtkTreeIter* iter, gint column, value_type** pointer ){
	GValue value = GValue_default();
	gtk_tree_model_get_value( model, iter, column, &value );
	*pointer = (value_type*)g_value_get_pointer( &value );
}



void entitylist_treeviewcolumn_celldatafunc( GtkTreeViewColumn* column, GtkCellRenderer* renderer, GtkTreeModel* model, GtkTreeIter* iter, gpointer data ){
	scene::Node* node;
	gtk_tree_model_get_pointer( model, iter, 0, &node );
	scene::Instance* instance;
	gtk_tree_model_get_pointer( model, iter, 1, &instance );
	if ( node != 0 ) {
		gtk_cell_renderer_set_fixed_size( renderer, -1, -1 );
		char* name = const_cast<char*>( node_get_name( *node ) );
		g_object_set( G_OBJECT( renderer ), "text", name, "visible", TRUE, NULL );

		//globalOutputStream() << "rendering cell " << makeQuoted(name) << "\n";
		GtkStyle* style = gtk_widget_get_style( GTK_WIDGET( getEntityList().m_tree_view ) );
		if ( instance->childSelected() ) {
			g_object_set( G_OBJECT( renderer ), "cell-background-gdk", &style->base[GTK_STATE_ACTIVE], NULL );
		}
		else
		{
			g_object_set( G_OBJECT( renderer ), "cell-background-gdk", &style->base[GTK_STATE_NORMAL], NULL );
		}
	}
	else
	{
		gtk_cell_renderer_set_fixed_size( renderer, -1, 0 );
		g_object_set( G_OBJECT( renderer ), "text", "", "visible", FALSE, NULL );
	}
}

void entitylist_focusSelected( GtkButton *button, gpointer user_data ){
	FocusAllViews();
}

static gboolean entitylist_tree_select( GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data ){
	GtkTreeIter iter;
	gtk_tree_model_get_iter( model, &iter, path );
	scene::Node* node;
	gtk_tree_model_get_pointer( model, &iter, 0, &node );
	scene::Instance* instance;
	gtk_tree_model_get_pointer( model, &iter, 1, &instance );
	Selectable* selectable = Instance_getSelectable( *instance );

	if ( node == 0 ) {
		if ( path_currently_selected != FALSE ) {
			getEntityList().m_selection_disabled = true;
			GlobalSelectionSystem().setSelectedAll( false );
			getEntityList().m_selection_disabled = false;
		}
	}
	else if ( selectable != 0 ) {
		getEntityList().m_selection_disabled = true;
		selectable->setSelected( path_currently_selected == FALSE );
		getEntityList().m_selection_disabled = false;
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( getEntityList().m_check ) ) ){
			FocusAllViews();
		}
		return TRUE;
	}

	return FALSE;
}

static gboolean entitylist_tree_select_null( GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data ){
	return TRUE;
}

void EntityList_ConnectSignals( GtkTreeView* view ){
	GtkTreeSelection* select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_select_function( select, entitylist_tree_select, NULL, 0 );
}

void EntityList_DisconnectSignals( GtkTreeView* view ){
	GtkTreeSelection* select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_select_function( select, entitylist_tree_select_null, 0, 0 );
}



gboolean treemodel_update_selection( GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data ){
	GtkTreeView* view = reinterpret_cast<GtkTreeView*>( data );

	scene::Instance* instance;
	gtk_tree_model_get_pointer( model, iter, 1, &instance );
	Selectable* selectable = Instance_getSelectable( *instance );

	if ( selectable != 0 ) {
		GtkTreeSelection* selection = gtk_tree_view_get_selection( view );
		if ( selectable->isSelected() ) {
			gtk_tree_selection_select_path( selection, path );
		}
		else
		{
			gtk_tree_selection_unselect_path( selection, path );
		}
	}

	return FALSE;
}

void EntityList_UpdateSelection( GtkTreeModel* model, GtkTreeView* view ){
	EntityList_DisconnectSignals( view );
	gtk_tree_model_foreach( model, treemodel_update_selection, view );
	EntityList_ConnectSignals( view );
}


void RedrawEntityList(){
	switch ( getEntityList().m_dirty )
	{
	case EntityList::eInsertRemove:
	case EntityList::eSelection:
		EntityList_UpdateSelection( GTK_TREE_MODEL( getEntityList().m_tree_model ), getEntityList().m_tree_view );
	default:
		break;
	}
	getEntityList().m_dirty = EntityList::eDefault;
}

void entitylist_queue_draw(){
	getEntityList().m_idleDraw.queueDraw();
}

void EntityList_SelectionUpdate(){
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

void entitylist_treeview_rowcollapsed( GtkTreeView* view, GtkTreeIter* iter, GtkTreePath* path, gpointer user_data ){
}

void entitylist_treeview_row_expanded( GtkTreeView* view, GtkTreeIter* iter, GtkTreePath* path, gpointer user_data ){
	EntityList_SelectionUpdate();
}


void EntityList_SetShown( bool shown ){
	widget_set_visible( GTK_WIDGET( getEntityList().m_window ), shown );
	if( shown ){ /* expand map's root node for convenience */
		GtkTreePath* path = gtk_tree_path_new_from_string( "1" );
		if( gtk_tree_view_row_expanded( getEntityList().m_tree_view, path ) == FALSE )
			gtk_tree_view_expand_row( getEntityList().m_tree_view, path, FALSE );
		gtk_tree_path_free( path );
	}
}

void EntityList_toggleShown(){
	EntityList_SetShown( !getEntityList().visible() );
}

gint graph_tree_model_compare_name( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data ){
	scene::Node* first;
	gtk_tree_model_get( model, a, 0, (gpointer*)&first, -1 );
	scene::Node* second;
	gtk_tree_model_get( model, b, 0, (gpointer*)&second, -1 );
	int result = 0;
	if ( first != 0 && second != 0 ) {
		result = string_compare( node_get_name( *first ), node_get_name( *second ) );
	}
	if ( result == 0 ) {
		return ( first < second ) ? -1 : ( second < first ) ? 1 : 0;
	}
	return result;
}

/* search */
#include <gtk/gtkstock.h>
static gboolean tree_view_search_equal_func( GtkTreeModel* model, gint column, const gchar* key, GtkTreeIter* iter, gpointer search_from_start ) {
	scene::Node* node;
	gtk_tree_model_get( model, iter, column, (gpointer*)&node, -1 );
	/* return FALSE means match */
	return ( node && !node->isRoot() )? *(bool*)search_from_start? !string_equal_prefix_nocase( node_get_name( *node ), key ) : !string_in_string_nocase( node_get_name( *node ), key ) : TRUE;
}

void searchEntrySetModeIcon( GtkEntry* entry, bool search_from_start ){
	gtk_entry_set_icon_from_stock( entry, GTK_ENTRY_ICON_PRIMARY, search_from_start? GTK_STOCK_MEDIA_PLAY : GTK_STOCK_ABOUT );
	g_signal_emit_by_name( G_OBJECT( entry ), "changed" );
}

void searchEntryIconPress( GtkEntry* entry, gint position, GdkEventButton* event, gpointer user_data ) {
	if( position == GTK_ENTRY_ICON_PRIMARY ){
		getEntityList().m_search_from_start = !getEntityList().m_search_from_start;
		searchEntrySetModeIcon( entry, getEntityList().m_search_from_start );
	}
}

gboolean searchEntryFocus( GtkWidget *widget, GdkEvent *event, gpointer user_data ){
	gtk_widget_grab_focus( widget );
	return FALSE;
}

gboolean searchEntryUnfocus( GtkWidget *widget, GdkEvent *event, gpointer user_data ){
	gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( widget ) ), NULL );
	return FALSE;
}

gboolean searchEntryScroll( GtkWidget* widget, GdkEventScroll* event, gpointer user_data ){
//	globalOutputStream() << "scroll\n";
	if( string_empty( gtk_entry_get_text( GTK_ENTRY( widget ) ) ) ){ /* scroll through selected/child selected entities */
		GtkTreeModel* model = gtk_tree_view_get_model( getEntityList().m_tree_view );
		GtkTreePath* path0 = gtk_tree_path_new_from_string( "1" );
		GtkTreeIter iter0, iter, iter_first, iter_prev, iter_found, iter_next;
		iter_first.stamp = iter_prev.stamp = iter_found.stamp = iter_next.stamp = 0;
		if( gtk_tree_model_get_iter( model, &iter0, path0 ) ){
			if( gtk_tree_model_iter_children( model, &iter, &iter0 ) ) {
				do{
					scene::Node* node;
					gtk_tree_model_get_pointer( model, &iter, 0, &node );
					if( node ){
						scene::Instance* instance;
						gtk_tree_model_get_pointer( model, &iter, 1, &instance );
						if( Instance_isSelected( *instance ) || instance->childSelected() ){
							if( iter_first.stamp == 0 ){
								iter_first = iter;
							}
							if( iter_found.stamp != 0 ){
								iter_next = iter;
								break;
							}
							if( node == getEntityList().m_search_focus_node ){
								iter_found = iter;
							}
							else{
								iter_prev = iter;
							}
						}
					}
				} while( gtk_tree_model_iter_next( model, &iter ) );
			}
		}
		iter.stamp = 0;
		if( iter_found.stamp != 0 ){
			iter = iter_found;
			if( event->direction == GDK_SCROLL_DOWN ){
				if( iter_next.stamp != 0 ){
					iter = iter_next;
				}
			}
			else{
				if( iter_prev.stamp != 0 ){
					iter = iter_prev;
				}
			}
		}
		else if( iter_first.stamp != 0 ){
			iter = iter_first;
		}
		if( iter.stamp != 0 ){
			gtk_tree_model_get_pointer( model, &iter, 0, &getEntityList().m_search_focus_node );
			GtkTreePath* path = gtk_tree_model_get_path( model, &iter );
			gtk_tree_view_scroll_to_cell( getEntityList().m_tree_view, path, 0, TRUE, .5f, 0.f );
			gtk_tree_path_free( path );
		}
		gtk_tree_path_free( path0 );
		return FALSE;
	}
	/* hijack internal gtk keypress function for handling scroll via synthesized event */
	GdkEvent* eventmp = gdk_event_new( GDK_KEY_PRESS );
	if ( event->direction == GDK_SCROLL_UP ) {
		eventmp->key.keyval = GDK_Up;
	}
	else if ( event->direction == GDK_SCROLL_DOWN ) {
		eventmp->key.keyval = GDK_Down;
	}
	if( eventmp->key.keyval ){
		eventmp->key.window = gtk_widget_get_window( widget );
		if( eventmp->key.window )
			g_object_ref( eventmp->key.window );
		gtk_widget_event( widget, eventmp );
//		gtk_window_propagate_key_event( GTK_WINDOW( gtk_widget_get_toplevel( widget ) ), (GdkEventKey*)eventmp );
//		gtk_main_do_event( eventmp );
//		gdk_event_put( eventmp );
	}
	gdk_event_free( eventmp );
	return FALSE;
}

static gboolean searchEntryKeypress( GtkEntry* widget, GdkEventKey* event, gpointer user_data ){
	if ( event->keyval == GDK_Escape ) {
		gtk_entry_set_text( GTK_ENTRY( widget ), "" );
		return TRUE;
	}
	return FALSE;
}


extern GraphTreeModel* scene_graph_get_tree_model();
void AttachEntityTreeModel(){
	getEntityList().m_tree_model = scene_graph_get_tree_model();

	gtk_tree_view_set_model( getEntityList().m_tree_view, GTK_TREE_MODEL( getEntityList().m_tree_model ) );

	gtk_tree_view_set_search_column( getEntityList().m_tree_view, 0 );
}

void DetachEntityTreeModel(){
	getEntityList().m_tree_model = 0;

	gtk_tree_view_set_model( getEntityList().m_tree_view, 0 );
}

void EntityList_constructWindow( GtkWindow* main_window ){
	ASSERT_MESSAGE( getEntityList().m_window == 0, "error" );

	GtkWindow* window = create_persistent_floating_window( "Entity List", main_window );

	//gtk_window_add_accel_group( window, global_accel );
	global_accel_connect_window( window );

	getEntityList().m_positionTracker.connect( window );


	getEntityList().m_window = window;

	{
		GtkVBox* vbox = GTK_VBOX( gtk_vbox_new( FALSE, 0 ) );
		gtk_container_set_border_width( GTK_CONTAINER( vbox ), 0 );
		gtk_widget_show( GTK_WIDGET( vbox ) );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( vbox ) );

		GtkScrolledWindow* scr = create_scrolled_window( GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
		//gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( scr ) );
		gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( scr ), TRUE, TRUE, 0 );

		{
			GtkWidget* view = gtk_tree_view_new();
			gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( view ), FALSE );

			GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
			GtkTreeViewColumn* column = gtk_tree_view_column_new();
			gtk_tree_view_column_pack_start( column, renderer, TRUE );
			gtk_tree_view_column_set_cell_data_func( column, renderer, entitylist_treeviewcolumn_celldatafunc, 0, 0 );

			GtkTreeSelection* select = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );
			gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );

			g_signal_connect( G_OBJECT( view ), "row_expanded", G_CALLBACK( entitylist_treeview_row_expanded ), 0 );
			g_signal_connect( G_OBJECT( view ), "row_collapsed", G_CALLBACK( entitylist_treeview_rowcollapsed ), 0 );

			gtk_tree_view_append_column( GTK_TREE_VIEW( view ), column );

			gtk_widget_show( view );
			gtk_container_add( GTK_CONTAINER( scr ), view );
			getEntityList().m_tree_view = GTK_TREE_VIEW( view );
		}
		{
			GtkHBox* hbox = GTK_HBOX( gtk_hbox_new( FALSE, 0 ) );
			gtk_container_set_border_width( GTK_CONTAINER( hbox ), 0 );
			gtk_widget_show( GTK_WIDGET( hbox ) );
			gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( hbox ), FALSE, FALSE, 0 );
			{
				GtkWidget* check = gtk_check_button_new_with_label( "AutoFocus on Selection" );
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), FALSE );
				gtk_widget_show( check );
				gtk_box_pack_start( GTK_BOX( hbox ), check, FALSE, FALSE, 0 );
				getEntityList().m_check = check;
				g_signal_connect( G_OBJECT( check ), "clicked", G_CALLBACK( entitylist_focusSelected ), 0 );
			}
			{//search entry
				GtkWidget* entry = gtk_entry_new();
				gtk_box_pack_start( GTK_BOX( hbox ), entry, TRUE, TRUE, 8 );
				searchEntrySetModeIcon( GTK_ENTRY( entry ), getEntityList().m_search_from_start );
				gtk_entry_set_icon_tooltip_text( GTK_ENTRY( entry ), GTK_ENTRY_ICON_PRIMARY, "toggle search mode ( start / any position )" );
				gtk_widget_show( entry );
				g_signal_connect( G_OBJECT( entry ), "icon-press", G_CALLBACK( searchEntryIconPress ), 0 );
				g_signal_connect( G_OBJECT( entry ), "enter_notify_event", G_CALLBACK( searchEntryFocus ), 0 );
				g_signal_connect( G_OBJECT( entry ), "leave_notify_event", G_CALLBACK( searchEntryUnfocus ), 0 );
				g_signal_connect( G_OBJECT( entry ), "scroll_event", G_CALLBACK( searchEntryScroll ), 0 );
				g_signal_connect( G_OBJECT( entry ), "key_press_event", G_CALLBACK( searchEntryKeypress ), 0 );
				gtk_tree_view_set_search_entry( getEntityList().m_tree_view, GTK_ENTRY( entry ) );
				gtk_tree_view_set_search_equal_func( getEntityList().m_tree_view, (GtkTreeViewSearchEqualFunc)tree_view_search_equal_func, &getEntityList().m_search_from_start, 0 );
			}
		}
	}

	EntityList_ConnectSignals( getEntityList().m_tree_view );
	AttachEntityTreeModel();
}

void EntityList_destroyWindow(){
	DetachEntityTreeModel();
	EntityList_DisconnectSignals( getEntityList().m_tree_view );
	destroy_floating_window( getEntityList().m_window );
}

#include "preferencesystem.h"

#include "iselection.h"

namespace
{
scene::Node* nullNode = 0;
}

class NullSelectedInstance : public scene::Instance, public Selectable
{
class TypeCasts
{
InstanceTypeCastTable m_casts;
public:
TypeCasts(){
	InstanceStaticCast<NullSelectedInstance, Selectable>::install( m_casts );
}
InstanceTypeCastTable& get(){
	return m_casts;
}
};

public:
typedef LazyStatic<TypeCasts> StaticTypeCasts;

NullSelectedInstance() : Instance( scene::Path( makeReference( *nullNode ) ), 0, this, StaticTypeCasts::instance().get() ){
}

void setSelected( bool select ){
	ERROR_MESSAGE( "error" );
}
bool isSelected() const {
	return true;
}
};

typedef LazyStatic<NullSelectedInstance> StaticNullSelectedInstance;

#include "stringio.h"

void EntityList_Construct(){
	graph_tree_model_insert( scene_graph_get_tree_model(), StaticNullSelectedInstance::instance() );

	g_EntityList = new EntityList;

	GlobalPreferenceSystem().registerPreference( "EntityInfoDlg", WindowPositionTrackerImportStringCaller( getEntityList().m_positionTracker ), WindowPositionTrackerExportStringCaller( getEntityList().m_positionTracker ) );
	GlobalPreferenceSystem().registerPreference( "EntListSearchFromStart", BoolImportStringCaller( getEntityList().m_search_from_start ), BoolExportStringCaller( getEntityList().m_search_from_start ) );

	typedef FreeCaller1<const Selectable&, EntityList_SelectionChanged> EntityListSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( EntityListSelectionChangedCaller() );
}
void EntityList_Destroy(){
	delete g_EntityList;

	graph_tree_model_erase( scene_graph_get_tree_model(), StaticNullSelectedInstance::instance() );
}

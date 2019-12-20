#include <glib.h>
#include <gtk/gtk.h>
#include <set>

#include "qerplugin.h"
#include "debugging/debugging.h"
#include "support.h"
#include "export.h"

// stuff from interface.cpp
void DestroyWindow();

//! TODO add tooltip for ignore: shader name after last slash, case sensitive // or make insensitive
//! TODO make togglebuttons inactive on !exportmat
//! TODO add ignore mat on ENTER, del on del
//! TODO add entry with path to save to (to resave faster)
//! TODO ignore case in mat name comparison		materials_comparator
namespace callbacks {

void OnDestroy( GtkWidget* w, gpointer data ){
	DestroyWindow();
}

void OnExportClicked( GtkButton* button, gpointer user_data ){
	GtkWidget* window = lookup_widget( GTK_WIDGET( button ), "w_plugplug2" );
	ASSERT_NOTNULL( window );
	const char* cpath = GlobalRadiant().m_pfnFileDialog( window, false, "Save as Obj", 0, 0, false, false, true );
	if ( !cpath ) {
		return;
	}

	std::string path( cpath );

	// get ignore list from ui
	std::set<std::string> ignore;

	GtkTreeView* view = GTK_TREE_VIEW( lookup_widget( GTK_WIDGET( button ), "t_materialist" ) );
	GtkListStore* list = GTK_LIST_STORE( gtk_tree_view_get_model( view ) );

	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first( GTK_TREE_MODEL( list ), &iter );
	while ( valid )
	{
		gchar* data;
		gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter, 0, &data, -1 );
#ifdef _DEBUG
		globalOutputStream() << data << "\n";
#endif
		ignore.insert( std::string( data ) );
		g_free( data );
		valid = gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &iter );
	}
#ifdef _DEBUG
	for ( const std::string& str : ignore )
		globalOutputStream() << str.c_str() << "\n";
#endif
	// collapse mode
	collapsemode mode = COLLAPSE_NONE;

	GtkWidget* radio = lookup_widget( GTK_WIDGET( button ), "r_collapse" );
	ASSERT_NOTNULL( radio );

	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radio ) ) ) {
		mode = COLLAPSE_ALL;
	}
	else
	{
		radio = lookup_widget( GTK_WIDGET( button ), "r_collapsebymaterial" );
		ASSERT_NOTNULL( radio );
		if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radio ) ) ) {
			mode = COLLAPSE_BY_MATERIAL;
		}
		else
		{
			radio = lookup_widget( GTK_WIDGET( button ), "r_nocollapse" );
			ASSERT_NOTNULL( radio );
			ASSERT_NOTNULL( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radio ) ) );
			mode = COLLAPSE_NONE;
		}
	}

	// export materials?
	GtkWidget* toggle = lookup_widget( GTK_WIDGET( button ), "t_exportmaterials" );
	ASSERT_NOTNULL( toggle );

	const bool exportmat = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) );

	// limit material names?
	toggle = lookup_widget( GTK_WIDGET( button ), "t_limitmatnames" );
	ASSERT_NOTNULL( toggle );

	const bool limitMatNames = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) ) && exportmat;

	// create objects instead of groups?
	toggle = lookup_widget( GTK_WIDGET( button ), "t_objects" );
	ASSERT_NOTNULL( toggle );

	const bool objects = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) ) && exportmat;

	toggle = lookup_widget( GTK_WIDGET( button ), "t_weld" );
	ASSERT_NOTNULL( toggle );

	const bool weld = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) );

	// export
	ExportSelection( ignore, mode, exportmat, path, limitMatNames, objects, weld );
}

void OnAddMaterial( GtkButton* button, gpointer user_data ){
	GtkEntry* edit = GTK_ENTRY( lookup_widget( GTK_WIDGET( button ), "ed_materialname" ) );
	ASSERT_NOTNULL( edit );

	const gchar* name = gtk_entry_get_text( edit );
	if ( g_utf8_strlen( name, -1 ) > 0 ) {
		GtkListStore* list = GTK_LIST_STORE( gtk_tree_view_get_model( GTK_TREE_VIEW( lookup_widget( GTK_WIDGET( button ), "t_materialist" ) ) ) );
		GtkTreeIter iter;
		gtk_list_store_append( list, &iter );
		gtk_list_store_set( list, &iter, 0, name, -1 );
		gtk_entry_set_text( edit, "" );
	}
}

void OnRemoveMaterial( GtkButton* button, gpointer user_data ){
	GtkTreeView* view = GTK_TREE_VIEW( lookup_widget( GTK_WIDGET( button ), "t_materialist" ) );
	GtkListStore* list = GTK_LIST_STORE( gtk_tree_view_get_model( view ) );
	GtkTreeSelection* sel = gtk_tree_view_get_selection( view );

	GtkTreeIter iter;
	if ( gtk_tree_selection_get_selected( sel, 0, &iter ) ) {
		gtk_list_store_remove( list, &iter );
	}
}

void OnExportMatClicked( GtkButton* button, gpointer user_data ){
	GtkWidget* toggleLimit = lookup_widget( GTK_WIDGET( button ), "t_limitmatnames" );
	GtkWidget* toggleObject = lookup_widget( GTK_WIDGET( button ), "t_objects" );

	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button ) ) ) {
		gtk_widget_set_sensitive( GTK_WIDGET( toggleLimit ), TRUE );
		gtk_widget_set_sensitive( GTK_WIDGET( toggleObject ), TRUE );
	}
	else {
		gtk_widget_set_sensitive( GTK_WIDGET( toggleLimit ), FALSE );
		gtk_widget_set_sensitive( GTK_WIDGET( toggleObject ), FALSE );
	}
}

} // callbacks

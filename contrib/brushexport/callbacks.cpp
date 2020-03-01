#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <set>

#include "qerplugin.h"
#include "debugging/debugging.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "support.h"
#include "export.h"


namespace callbacks {

static std::string s_export_path;

void OnExportClicked( GtkButton* button, gpointer choose_path ){
	GtkWidget* window = lookup_widget( GTK_WIDGET( button ), "w_plugplug2" );
	ASSERT_NOTNULL( window );
	if( choose_path ){
		StringOutputStream buffer( 1024 );

		if( !s_export_path.empty() ){
			buffer << s_export_path.c_str();
		}
		if( buffer.empty() ){
			buffer << GlobalRadiant().getEnginePath() << GlobalRadiant().getGameName() << "/models/";

			if ( !file_readable( buffer.c_str() ) ) {
				// just go to fsmain
				buffer.clear();
				buffer << GlobalRadiant().getEnginePath() << GlobalRadiant().getGameName();
			}
		}

		const char* cpath = GlobalRadiant().m_pfnFileDialog( window, false, "Save as Obj", buffer.c_str(), 0, false, false, true );
		if ( !cpath ) {
			return;
		}
		s_export_path = cpath;
		if( !string_equal_suffix_nocase( s_export_path.c_str(), ".obj" ) )
			s_export_path += ".obj";
		// enable button to reexport with the selected name
		GtkWidget* b_export = lookup_widget( GTK_WIDGET( button ), "b_export" );
		ASSERT_NOTNULL( b_export );
		gtk_widget_set_sensitive( b_export, TRUE );
		// add tooltip
		gtk_widget_set_tooltip_text( b_export, ( std::string( "ReExport to " ) + s_export_path ).c_str() );
	}
	else if( s_export_path.empty() ){
		return;
	}

	// get ignore list from ui
	StringSetWithLambda ignore
		( []( const std::string& lhs, const std::string& rhs )->bool{
			return string_less_nocase( lhs.c_str(), rhs.c_str() );
		} );

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

	GtkWidget* toggle;
	// export materials?
	ASSERT_NOTNULL( ( toggle = lookup_widget( GTK_WIDGET( button ), "t_exportmaterials" ) ) );
	const bool exportmat = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) );

	// limit material names?
	ASSERT_NOTNULL( ( toggle = lookup_widget( GTK_WIDGET( button ), "t_limitmatnames" ) ) );
	const bool limitMatNames = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) );

	// create objects instead of groups?
	ASSERT_NOTNULL( ( toggle = lookup_widget( GTK_WIDGET( button ), "t_objects" ) ) );
	const bool objects = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) );

	ASSERT_NOTNULL( ( toggle = lookup_widget( GTK_WIDGET( button ), "t_weld" ) ) );
	const bool weld = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ) );

	// export
	ExportSelection( ignore, mode, exportmat, s_export_path, limitMatNames, objects, weld );
}

void OnAddMaterial( GtkButton* button, gpointer user_data ){
	GtkEntry* edit = GTK_ENTRY( lookup_widget( GTK_WIDGET( button ), "ed_materialname" ) );
	ASSERT_NOTNULL( edit );

	const gchar* name = path_get_filename_start( gtk_entry_get_text( edit ) );
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

gboolean OnRemoveMaterialKb( GtkWidget* widget, GdkEventKey* event, gpointer user_data ){
	if( event->keyval == GDK_Delete )
		OnRemoveMaterial( reinterpret_cast<GtkButton*>( widget ), NULL );
	return FALSE;
}

} // callbacks

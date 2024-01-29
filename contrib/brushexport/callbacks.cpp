

#include "callbacks.h"

#include <set>

#include "qerplugin.h"
#include "debugging/debugging.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "export.h"




namespace callbacks {

static std::string s_export_path;

void OnExportClicked( bool choose_path ){
	if( choose_path ){
		StringOutputStream buffer( 256 );

		if( !s_export_path.empty() ){
			buffer << s_export_path.c_str();
		}
		if( buffer.empty() ){
			buffer << GlobalRadiant().getEnginePath() << GlobalRadiant().getGameName() << "/models/";

			if ( !file_readable( buffer ) ) {
				// just go to fsmain
				buffer( GlobalRadiant().getEnginePath(), GlobalRadiant().getGameName() );
			}
		}

		const char* cpath = GlobalRadiant().m_pfnFileDialog( g_dialog.window, false, "Save as Obj", buffer, 0, false, false, true );
		if ( !cpath ) {
			return;
		}
		s_export_path = cpath;
		if( !string_equal_suffix_nocase( s_export_path.c_str(), ".obj" ) )
			s_export_path += ".obj";
		// enable button to reexport with the selected name
		g_dialog.b_export->setEnabled( true );
		// add tooltip
		g_dialog.b_export->setToolTip( ( std::string( "ReExport to " ) + s_export_path ).c_str() );
	}
	else if( s_export_path.empty() ){
		return;
	}

	// get ignore list from ui
	StringSetWithLambda ignore
	( []( const std::string& lhs, const std::string& rhs )->bool{
		return string_less_nocase( lhs.c_str(), rhs.c_str() );
	} );

	for( int i = 0; i < g_dialog.t_materialist->count(); ++i )
	{
		ignore.insert( g_dialog.t_materialist->item( i )->text().toStdString() );
	}
#ifdef _DEBUG
	for ( const std::string& str : ignore )
		globalOutputStream() << str.c_str() << '\n';
#endif
	// collapse mode
	collapsemode mode = COLLAPSE_NONE;

	if ( g_dialog.r_collapse->isChecked() ) {
		mode = COLLAPSE_ALL;
	}
	else if ( g_dialog.r_collapsebymaterial->isChecked() ) {
		mode = COLLAPSE_BY_MATERIAL;
	}
	else{
		ASSERT_NOTNULL( g_dialog.r_nocollapse->isChecked() );
		mode = COLLAPSE_NONE;
	}

	// export materials?
	const bool exportmat = g_dialog.t_exportmaterials->isChecked();

	// limit material names?
	const bool limitMatNames = g_dialog.t_limitmatnames->isChecked();

	// create objects instead of groups?
	const bool objects = g_dialog.t_objects->isChecked();

	const bool weld = g_dialog.t_weld->isChecked();

	// export
	ExportSelection( ignore, mode, exportmat, s_export_path, limitMatNames, objects, weld );
}

void OnAddMaterial(){
	const auto text = g_dialog.ed_materialname->text().toLatin1();

	const char* name = path_get_filename_start( text.constData() );
	if ( strlen( name ) > 0 ) {
		auto item = new QListWidgetItem( name );
		item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren );
		g_dialog.t_materialist->addItem( item );
	}
}

void OnRemoveMaterial(){
	qDeleteAll( g_dialog.t_materialist->selectedItems() );
}


} // callbacks

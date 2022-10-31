/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Some small dialogs that don't need much
//
// Leonardo Zide (leo@lokigames.com)
//

#include "gtkdlgs.h"

#include "debugging/debugging.h"
#include "version.h"
#include "aboutmsg.h"

#include "igl.h"
#include "iscenegraph.h"
#include "iselection.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include "gtkutil/spinbox.h"
#include "gtkutil/guisettings.h"
#include <QPlainTextEdit>
#include <QComboBox>
#include <QTextEdit>

#include "os/path.h"
#include "math/aabb.h"
#include "container/array.h"
#include "generic/static.h"
#include "stream/stringstream.h"
#include "convert.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/image.h"

#include "gtkmisc.h"
#include "brushmanip.h"
#include "build.h"
#include "qe3.h"
#include "texwindow.h"
#include "xywindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "url.h"
#include "commandlib.h"

#include "qerplugin.h"
#include "os/file.h"



// =============================================================================
// Project settings dialog

class GameComboConfiguration
{
public:
	const char* basegame_dir;
	const char* basegame;
	const char* known_dir;
	const char* known;
	const char* custom;

	GameComboConfiguration() :
		basegame_dir( g_pGameDescription->getRequiredKeyValue( "basegame" ) ),
		basegame( g_pGameDescription->getRequiredKeyValue( "basegamename" ) ),
		known_dir( g_pGameDescription->getKeyValue( "knowngame" ) ),
		known( g_pGameDescription->getKeyValue( "knowngamename" ) ),
		custom( g_pGameDescription->getRequiredKeyValue( "unknowngamename" ) ){
	}
};

typedef LazyStatic<GameComboConfiguration> LazyStaticGameComboConfiguration;

inline GameComboConfiguration& globalGameComboConfiguration(){
	return LazyStaticGameComboConfiguration::instance();
}


struct gamecombo_t
{
	gamecombo_t( int _game, const char* _fs_game, bool _sensitive )
		: game( _game ), fs_game( _fs_game ), sensitive( _sensitive )
	{}
	int game;
	const char* fs_game;
	bool sensitive;
};

gamecombo_t gamecombo_for_dir( const char* dir ){
	if ( path_equal( dir, globalGameComboConfiguration().basegame_dir ) ) {
		return gamecombo_t( 0, dir, false );
	}
	else if ( path_equal( dir, globalGameComboConfiguration().known_dir ) ) {
		return gamecombo_t( 1, dir, false );
	}
	else
	{
		return gamecombo_t( string_empty( globalGameComboConfiguration().known_dir ) ? 1 : 2, dir, true );
	}
}

gamecombo_t gamecombo_for_gamename( const char* gamename ){
	if ( string_empty( gamename ) || string_equal( gamename, globalGameComboConfiguration().basegame ) ) {
		return gamecombo_t( 0, globalGameComboConfiguration().basegame_dir, false );
	}
	else if ( string_equal( gamename, globalGameComboConfiguration().known ) ) {
		return gamecombo_t( 1, globalGameComboConfiguration().known_dir, false );
	}
	else
	{
		return gamecombo_t( string_empty( globalGameComboConfiguration().known_dir ) ? 1 : 2, "", true );
	}
}


class MappingMode
{
public:
	bool do_mapping_mode;
	const char* sp_mapping_mode;
	const char* mp_mapping_mode;

	MappingMode() :
		do_mapping_mode( !string_empty( g_pGameDescription->getKeyValue( "show_gamemode" ) ) ),
		sp_mapping_mode( "Single Player mapping mode" ),
		mp_mapping_mode( "Multiplayer mapping mode" ){
	}
};

typedef LazyStatic<MappingMode> LazyStaticMappingMode;

inline MappingMode& globalMappingMode(){
	return LazyStaticMappingMode::instance();
}


struct GameCombo
{
	QComboBox* game_select{};
	QComboBox* fsgame_entry{};
};
static GameCombo s_gameCombo;


void GameModeImport( int value ){
	gamemode_set( value == 0? "sp" : "mp" );
}
typedef FreeCaller1<int, GameModeImport> GameModeImportCaller;

void GameModeExport( const IntImportCallback& importer ){
	const char *gamemode = gamemode_get();
	importer( ( string_empty( gamemode ) || string_equal( gamemode, "sp" ) )? 0 : 1 );
}
typedef FreeCaller1<const IntImportCallback&, GameModeExport> GameModeExportCaller;


void FSGameImport( int value ){
}
typedef FreeCaller1<int, FSGameImport> FSGameImportCaller;

void FSGameExport( const IntImportCallback& importer ){
}
typedef FreeCaller1<const IntImportCallback&, FSGameExport> FSGameExportCaller;


void GameImport( int value ){
	const auto dir = s_gameCombo.fsgame_entry->currentText().toLatin1();

	const char* new_gamename = dir.isEmpty()
	                           ? globalGameComboConfiguration().basegame_dir
	                           : dir.constData();

	if ( !path_equal( new_gamename, gamename_get() ) ) {
		if ( ConfirmModified( "Edit Project Settings" ) ) {
			ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing Game Name" );

			EnginePath_Unrealise();

			gamename_set( new_gamename );

			EnginePath_Realise();
		}
	}
}
typedef FreeCaller1<int, GameImport> GameImportCaller;

void GameExport( const IntImportCallback& importer ){
	const gamecombo_t gamecombo = gamecombo_for_dir( gamename_get() );

	s_gameCombo.game_select->setCurrentIndex( gamecombo.game );
	s_gameCombo.fsgame_entry->setEditText( gamecombo.fs_game );
	s_gameCombo.fsgame_entry->setEnabled( gamecombo.sensitive );
}
typedef FreeCaller1<const IntImportCallback&, GameExport> GameExportCaller;


void Game_constructPreferences( PreferencesPage& page ){
	{
		s_gameCombo.game_select = page.appendCombo(
			"Select mod",
			StringArrayRange(),
			IntImportCallback( GameImportCaller() ),
			IntExportCallback( GameExportCaller() )
		);
		s_gameCombo.game_select->addItem( globalGameComboConfiguration().basegame );
		if ( !string_empty( globalGameComboConfiguration().known ) )
			s_gameCombo.game_select->addItem( globalGameComboConfiguration().known );
		s_gameCombo.game_select->addItem( globalGameComboConfiguration().custom );
	}
	{
		s_gameCombo.fsgame_entry = page.appendCombo(
			"fs_game",
			StringArrayRange(),
			IntImportCallback( FSGameImportCaller() ),
			IntExportCallback( FSGameExportCaller() )
		);
		s_gameCombo.fsgame_entry->setEditable( true );
		std::error_code err; // use func version with error handling, since other throws error on non-existing directory
		for( const auto& entry : std::filesystem::directory_iterator( EnginePath_get(), std::filesystem::directory_options::skip_permission_denied, err ) )
			if( entry.is_directory() )
				s_gameCombo.fsgame_entry->addItem( entry.path().filename().string().c_str() );
	}
	QObject::connect( s_gameCombo.game_select, &QComboBox::currentTextChanged, []( const QString& text ){
		const gamecombo_t gamecombo = gamecombo_for_gamename( text.toLatin1().constData() );
		s_gameCombo.fsgame_entry->setEditText( gamecombo.fs_game );
		s_gameCombo.fsgame_entry->setEnabled( gamecombo.sensitive );
	} );

	if( globalMappingMode().do_mapping_mode ){
		page.appendCombo(
			"Mapping mode",
			(const char*[]){ globalMappingMode().sp_mapping_mode, globalMappingMode().mp_mapping_mode },
			IntImportCallback( GameModeImportCaller() ),
			IntExportCallback( GameModeExportCaller() )
		);
	}
}

// =============================================================================
// Arbitrary Sides dialog

void DoSides( EBrushPrefab type ){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Arbitrary sides" );

	auto spin = new SpinBox;
	auto check = new QCheckBox( "Truncate" );
	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );

		QLabel* label = new SpinBoxLabel( "Sides:", spin );
		form->addRow( label, spin );
		form->addWidget( check );
		check->hide();
		{
			switch ( type )
			{
			case EBrushPrefab::Prism :
			case EBrushPrefab::Cone :
				spin->setValue( 8 );
				spin->setRange( 3, 1022 );
				break;
			case EBrushPrefab::Sphere :
				spin->setValue( 8 );
				spin->setRange( 3, 31 );
				break;
			case EBrushPrefab::Rock :
				spin->setValue( 32 );
				spin->setRange( 10, 1000 );
				break;
			case EBrushPrefab::Icosahedron :
				spin->setValue( 1 );
				spin->setRange( 0, 2 ); //possible with 3, but buggy
				check->show();
				label->setText( "Subdivisions:" );
				break;
			default:
				break;
			}
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if ( dialog.exec() ) {
		const int sides = spin->value();
		const bool option = check->isChecked();
		Scene_BrushConstructPrefab( GlobalSceneGraph(), type, sides, option, TextureBrowser_GetSelectedShader() );
	}
}

// =============================================================================
// About dialog (no program is complete without one)

void DoAbout(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "About NetRadiant" );

	{
		auto vbox = new QVBoxLayout( &dialog );
		{
			auto hbox = new QHBoxLayout;
			vbox->addLayout( hbox );
			{
				auto label = new QLabel;
				label->setPixmap( new_local_image( "logo.png" ) );
				hbox->addWidget( label );
			}

			{
				auto label = new QLabel( "NetRadiant " RADIANT_VERSION "\n"
				                         __DATE__ "\n\n"
				                         RADIANT_ABOUTMSG "\n\n"
				                         "By alientrap.org\n\n"
				                         "This program is free software\n"
				                         "licensed under the GNU GPL.\n"
				                       );
				hbox->addWidget( label );
			}

			{
				auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok, Qt::Orientation::Vertical );
				QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
				hbox->addWidget( buttons );
				{
					auto button = buttons->addButton( "Credits", QDialogButtonBox::ButtonRole::NoRole );
					QObject::connect( button, &QPushButton::clicked, [](){ OpenURL( StringOutputStream( 256 )( AppPath_get(), "credits.html" ) ); } );
					button->setEnabled( false );
				}
				{
					auto button = buttons->addButton( "Changelog", QDialogButtonBox::ButtonRole::NoRole );
					QObject::connect( button, &QPushButton::clicked, [](){ OpenURL( StringOutputStream( 256 )( AppPath_get(), "changelog.txt" ) ); } );
					button->setEnabled( false );
				}
			}
		}
		{
			{
				auto frame = new QGroupBox( "OpenGL Properties" );
				vbox->addWidget( frame );
				{
					auto form = new QFormLayout( frame );
					form->addRow( "Vendor:", new QLabel( reinterpret_cast<const char*>( gl().glGetString( GL_VENDOR ) ) ) );
					form->addRow( "Version:", new QLabel( reinterpret_cast<const char*>( gl().glGetString( GL_VERSION ) ) ) );
					form->addRow( "Renderer:", new QLabel( reinterpret_cast<const char*>( gl().glGetString( GL_RENDERER ) ) ) );
				}
			}
			{
				auto frame = new QGroupBox( "OpenGL Extensions" );
				vbox->addWidget( frame );
				{
					auto textView = new QPlainTextEdit( reinterpret_cast<const char*>( gl().glGetString( GL_EXTENSIONS ) ) );
					textView->setReadOnly( true );
					auto box = new QVBoxLayout( frame );
					box->addWidget( textView );
				}
			}
		}
	}
	dialog.exec();
}

// =============================================================================

class TextEditor
{
	QWidget *m_window = 0;
	QTextEdit *m_textView; // slave, text widget from the gtk editor
	QPushButton *m_button; // save button
	CopiedString m_filename;

	void construct(){
		m_window = new QWidget( MainFrame_getWindow(), Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
		g_guiSettings.addWindow( m_window, "ShaderEditor/geometry" );

		auto *vbox = new QVBoxLayout( m_window );
		vbox->setContentsMargins( 0, 0, 0, 0 );

		m_textView = new QTextEdit;
		m_textView->setAcceptRichText( false );
		m_textView->setLineWrapMode( QTextEdit::LineWrapMode::NoWrap );
		vbox->addWidget( m_textView );

		m_button = new QPushButton( "Save" );
		m_button->setSizePolicy( QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed );
		vbox->addWidget( m_button, Qt::AlignmentFlag::AlignRight );

		QObject::connect( m_textView->document(), &QTextDocument::modificationChanged, [this]( bool modified ){
			m_button->setEnabled( modified );

			StringOutputStream str( 256 );
			str << ( modified? "*" : "" ) << m_filename;
			m_window->setWindowTitle( str.c_str() );
		} );

		QObject::connect( m_button, &QAbstractButton::clicked, [this](){ editor_save(); } );
	}
	void editor_save(){
		FILE *f = fopen( m_filename.c_str(), "wb" ); //write in binary mode to preserve line feeds

		if ( f == nullptr ) {
			globalErrorStream() << "Error saving file" << makeQuoted( m_filename ) << '\n';
			return;
		}

		const auto str = m_textView->toPlainText().toLatin1();
		fwrite( str.constData(), 1, str.length(), f );
		fclose( f );

		m_textView->document()->setModified( false );
	}
public:
	void DoGtkTextEditor( const char* text, const char* shaderName, const char* filename, const bool editable ){
		if ( !m_window ) {
			construct(); // build it the first time we need it
		}

		m_filename = filename;
		m_textView->setReadOnly( !editable );
		m_textView->setPlainText( text );

		m_window->show();
		m_window->raise();
		m_window->activateWindow();

		{ // scroll to shader
			const QRegularExpression::PatternOptions rxFlags = QRegularExpression::PatternOption::MultilineOption |
			                                                   QRegularExpression::PatternOption::CaseInsensitiveOption;
			const QRegularExpression rx( "^\\s*" + QRegularExpression::escape( shaderName ) + "(|:q3map)$", rxFlags );
			auto *doc = m_textView->document();

			for( QTextCursor cursor( doc ); cursor = doc->find( rx ), !cursor.isNull(); )
				if( !doc->find( QRegularExpression( "^\\s*\\{", rxFlags ), cursor ).isNull() ){
					QTextCursor cur( cursor );
					cur.movePosition( QTextCursor::MoveOperation::NextBlock, QTextCursor::MoveMode::MoveAnchor, 99 );
					m_textView->setTextCursor( cur );
					m_textView->setTextCursor( cursor );
					break;
				}
		}
	}
};

static TextEditor g_textEditor;


// =============================================================================
// Light Intensity dialog

static bool g_dontDoLightIntensityDlg = false;

bool DoLightIntensityDlg( int *intensity ){
	if( g_dontDoLightIntensityDlg )
		return true;

	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Light intensity" );

	auto spin = new SpinBox( -99999, 99999, *intensity );

	auto check = new QCheckBox( "Don't Show" );
	QObject::connect( check, &QCheckBox::toggled, []( bool checked ){ g_dontDoLightIntensityDlg = checked; } );

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		form->addRow( new QLabel( "ESC for default, ENTER to validate" ) );
		form->addRow( new SpinBoxLabel( "Intensity:", spin ), spin );
		form->addWidget( check );

		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if ( dialog.exec() ) {
		*intensity = spin->value();
		return true;
	}
	else
		return false;
}

void DoShaderInfoDlg( const char* name, const char* filename, const char* title ){
	StringOutputStream text( 256 );
	text << "&nbsp;&nbsp;The selected shader<br>";
	text << "<b>" << name << "</b><br>";
	text << "&nbsp;&nbsp;is located in file<br>";
	text << "<b>" << filename << "</b>";

	qt_MessageBox( MainFrame_getWindow(), text.c_str(), title );
}



CopiedString g_TextEditor_editorCommand;

#include "ifilesystem.h"
#include "iarchive.h"
#include "idatastream.h"

void DoShaderView( const char *shaderFileName, const char *shaderName, bool external_editor ){
	const char* pathRoot = GlobalFileSystem().findFile( shaderFileName );
	const bool pathEmpty = string_empty( pathRoot );
	const bool pathIsDir = !pathEmpty && file_is_directory( pathRoot );

	StringOutputStream pathFull( 256 );
	pathFull << pathRoot << ( pathIsDir? "" : "::" ) << shaderFileName;

	if( pathEmpty ){
		globalErrorStream() << "Failed to load shader file " << shaderFileName << "\n";
	}
	else if( external_editor && pathIsDir ){
		if( g_TextEditor_editorCommand.empty() ){
#ifdef WIN32
			ShellExecute( (HWND)MainFrame_getWindow()->effectiveWinId(), 0, pathFull.c_str(), 0, 0, SW_SHOWNORMAL );
#else
			globalWarningStream() << "Failed to open '" << pathFull.c_str() << "'\nSet Shader Editor Command in preferences\n";
#endif
		}
		else{
			StringOutputStream command( 256 );
			command << g_TextEditor_editorCommand << " \"" << pathFull.c_str() << "\"";
			globalOutputStream() << "Launching: " << command.c_str() << "\n";
			// note: linux does not return false if the command failed so it will assume success
			if ( !Q_Exec( 0, const_cast<char*>( command.c_str() ), 0, true, false ) )
				globalErrorStream() << "Failed to execute " << command.c_str() << "\n";
		}
	}
	else if( ArchiveFile* file = GlobalFileSystem().openFile( shaderFileName ) ){
		const std::size_t size = file->size();
		char* text = ( char* )malloc( size + 1 );
		file->getInputStream().read( ( InputStream::byte_type* )text, size );
		text[size] = 0;
		file->release();

		g_textEditor.DoGtkTextEditor( text, shaderName, pathFull.c_str(), pathIsDir );
		free( text );
	}
}

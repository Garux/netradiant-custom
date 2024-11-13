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
#include <QApplication>
#include "gtkutil/spinbox.h"
#include "gtkutil/guisettings.h"
#include <QPlainTextEdit>
#include <QComboBox>

#include "os/path.h"
#include "math/aabb.h"
#include "container/array.h"
#include "generic/static.h"
#include "stream/stringstream.h"
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
		spin->selectAll();
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
					QObject::connect( button, &QPushButton::clicked, [](){ OpenURL( StringStream( AppPath_get(), "credits.html" ) ); } );
					button->setEnabled( false );
				}
				{
					auto button = buttons->addButton( "Changelog", QDialogButtonBox::ButtonRole::NoRole );
					QObject::connect( button, &QPushButton::clicked, [](){ OpenURL( StringStream( AppPath_get(), "docs/changelog-custom.txt" ) ); } );
				}
				{
					auto button = buttons->addButton( "About Qt", QDialogButtonBox::ButtonRole::NoRole );
					QObject::connect( button, &QPushButton::clicked, &QApplication::aboutQt );
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
	const auto text = StringStream(
		"&nbsp;&nbsp;The selected shader<br>"
		"<b>", name, "</b><br>"
		"&nbsp;&nbsp;is located in file<br>"
		"<b>", filename, "</b>"
	);
	qt_MessageBox( MainFrame_getWindow(), text, title );
}

// =============================================================================
// Install dev files dialog
#include <QListWidget>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextStream>

void DoInstallDevFilesDlg( const char *enginePath ){
	std::vector<std::filesystem::path> files; // relative source files paths
	const auto sourceBase = std::filesystem::path( g_pGameDescription->mGameToolsPath.c_str() ) / "install/";
	const auto targetBase = std::filesystem::path( enginePath ) / basegame_get();
	QString description;
	{
		std::error_code err;
		std::filesystem::recursive_directory_iterator dirIter( sourceBase, err );
		if( err ){
			globalErrorStream() << err.message().c_str() << ' ' << sourceBase.string().c_str() << '\n';
			return;
		}
		for( const auto& dirEntry : dirIter ) {
			if( err ){
				globalErrorStream() << err.message().c_str() << '\n';
				break;
			}
			if( dirEntry.is_regular_file( err ) && !err ){
				if( dirIter.depth() == 0 && dirEntry.path().filename() == ".description" ){
					if( QFile f( QString::fromStdString( dirEntry.path().string() ) ); f.open( QIODevice::ReadOnly | QIODevice::Text ) )
						description = QTextStream( &f ).readAll();
				}
				else{
					files.push_back( std::filesystem::relative( dirEntry.path(), sourceBase, err ) );
				}
			}
		}
	}
	if( !files.empty() ){
		QDialog dialog( nullptr, Qt::Window );
		dialog.setWindowTitle( "Install Map Developer's Files" );
		{
			auto *box = new QVBoxLayout( &dialog );
			{
				auto *label = new QLabel( "Would you like to install following files recommended for fluent map development\nto " + QString::fromStdString( targetBase.string() ) + "?" );
				label->setAlignment( Qt::AlignmentFlag::AlignHCenter );
				box->addWidget( label );
			}
			QListWidget *listWidget;
			{
				listWidget = new QListWidget;
				listWidget->setSelectionMode( QAbstractItemView::SelectionMode::NoSelection );
				box->addWidget( listWidget, 0 );
				for( const auto& file : files ){
					listWidget->addItem( QString::fromStdString( file.string() ) );
				}
			}
			if( !description.isEmpty() ){
				box->addWidget( new QLabel( ".description" ) );
				auto *text = new QPlainTextEdit( description );
				text->setSizePolicy( QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::MinimumExpanding );
				text->setLineWrapMode( QPlainTextEdit::LineWrapMode::NoWrap );
				text->setReadOnly( true );
				// set minimal size to fit text to avoid the need to resize window/scroll
				const auto rect = text->fontMetrics().boundingRect( QRect(), 0, description );
				text->setMinimumSize( rect.width() + text->contentsMargins().left() + text->contentsMargins().right()
				                      + text->document()->documentMargin() * 2 + text->verticalScrollBar()->sizeHint().width(),
				                      rect.height() + text->contentsMargins().top() + text->contentsMargins().bottom()
				                      + text->document()->documentMargin() * 2 + text->horizontalScrollBar()->sizeHint().height() );

				box->addWidget( text, 0 );
			}
			const auto doCopy = [&](){
				QMessageBox::StandardButton overwrite = QMessageBox::StandardButton::Yes;
				size_t copiedN = 0;
				for( size_t i = 0; i < files.size(); ++i ){
					const auto source = sourceBase / files[i];
					const auto target = targetBase / files[i];
					std::error_code err;
					if( ( std::filesystem::exists( target, err ) || err ) && overwrite != QMessageBox::StandardButton::YesToAll ){
						if( overwrite == QMessageBox::StandardButton::NoToAll ) continue;
						overwrite = (QMessageBox::StandardButton)QMessageBox( QMessageBox::Icon::Question, "File exists",
							QString( "File \"" ) + QString::fromStdString( target.string() ) + "\" exists.\nOverwrite it?",
							QMessageBox::StandardButton::Yes |
							QMessageBox::StandardButton::YesToAll |
							QMessageBox::StandardButton::No |
							QMessageBox::StandardButton::NoToAll |
							QMessageBox::StandardButton::Abort, &dialog ).exec();
						if( overwrite == QMessageBox::StandardButton::Abort ) break;
						if( overwrite == QMessageBox::StandardButton::NoToAll || overwrite == QMessageBox::StandardButton::No ) continue;
					}

					const auto copy_file = [&](){
						if( std::filesystem::exists( target, err ) ){
							if( !std::filesystem::remove( target, err ) ){
								return false;
							}
						}
						else if( err ){
							return false;
						}
						std::filesystem::create_directories( target.parent_path(), err );
						if( err )
							return false;
						// std::filesystem::copy_options::overwrite_existing is broken in libstdc++ on windows, thus using std::filesystem::remove
						return std::filesystem::copy_file( source, target, std::filesystem::copy_options::none, err );
					};
retry:
					if( !copy_file() ){
						const auto ret = (QMessageBox::StandardButton)QMessageBox( QMessageBox::Icon::Question, "Fail",
							"Failed to write \"" + QString::fromStdString( target.string() ) + "\"\n" + err.message().c_str(),
							QMessageBox::StandardButton::Retry |
							QMessageBox::StandardButton::Ignore |
							QMessageBox::StandardButton::Abort, &dialog ).exec();
						if( ret == QMessageBox::StandardButton::Retry ) goto retry;
						if( ret == QMessageBox::StandardButton::Ignore ) continue;
						if( ret == QMessageBox::StandardButton::Abort ) break;
					}
					auto *item = listWidget->item( i );
					item->setCheckState( Qt::CheckState::Checked );
					listWidget->scrollToItem( item );
					QCoreApplication::processEvents( QEventLoop::ProcessEventsFlag::ExcludeUserInputEvents );
					++copiedN;
				}

				if( copiedN == files.size() )
					qt_MessageBox( &dialog, "All files have been copied.", "Great Success!" );
				else if( copiedN != 0 )
					qt_MessageBox( &dialog, StringStream<64>( copiedN, '/', files.size(), " files have been copied." ), "Moderate Success!" );
				else
					qt_MessageBox( &dialog, "No files have been copied.", "Boo!" );

				dialog.accept();
			};
			{
				auto *buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
				box->addWidget( buttons );
				QObject::connect( buttons, &QDialogButtonBox::accepted, doCopy );
				QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
			}
		}
		dialog.exec();
	}
}


// =============================================================================
// Shader Editor

/*
	force dark background, bright foreground
?save font size
	ctrl+d duplicate line, selection
	find and replace
f3, shift+f3, ctrl+f
ctrl+s
	move selected text block with alt+arrows
	move line up/dn too
	ctrl+x to cut whole line
	ctrl+c to copy whole line
?paste these on new line: put cursor to start 1st
	url to manual
	hl bug: when \n} is deleted, then undone //was paste=state -1->hl -1 = unchanged = hl break
	complete tex paths from radiant's VFS
separate shader path completion
?complete shader name from tex paths?
	shader templates in completion; on {
	color3f display
	suggest common prefix, e.g. q3map_ for q3 input
?ctrl+bs del to _
	sort fix \d completion
	animmap fix completion
	map $lightmap etc
	skyparms nearbox '-' completion is wanted
	skyparms farbox '-' completion is wanted
sensible default num values on completion
num values description on completion //?in comment
?complete continuous num sequences at once
no next token completion in the middle, if line is complete
no completion on undo, paste? //atm on adding undo, not on removing
QStringLiteral optimization
QCompleter inactive entry in list // because is wrapAround()
	check %p %t lengths in hl
	display line numbers, exremely useful for error messages handling
*/

#include <set>
#include <QSyntaxHighlighter>
#include <QLineEdit>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QPainter>
#include "stringio.h"
#include "plugin.h"
#include "ifilesystem.h"

static const struct{ const char *name; const char *text; } c_shaderTemplates[] = {
	{
		"map",
R"(
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}
)"
	},
	{
		"map-vertex",
R"(
	surfaceparm nolightmap
	{
		map textures/
		rgbGen exactVertex
	}
}
)"
	},
	{
		"mask",
R"(
	cull none
	{
		map textures/
		alphaFunc GE128
		depthWrite
	}
	{
		map $lightmap
		rgbGen identity
		depthFunc equal
	}
	{
		// same texture once more
		map textures/
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
		depthFunc equal
	}
}
)"
	},
	{
		"mask-vertex",
R"(
	surfaceparm nolightmap
	cull none
	{
		map textures/
		alphaFunc GE128
		depthWrite
		rgbGen exactVertex
	}
}
)"
	},
	{
		"blend",
R"(
	cull none
	{
		map textures/
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}
)"
	},
	{
		"remap",
R"(
	// compile time parameter
	surfaceparm slick
	qer_editorimage textures/
	// remap back to original shader
	q3map_remapShader textures/
}
)"
	},
};


static const char c_pageGen[] = "general-directives.html#";
static const char c_pageGlob[] = "q3map-global-directives.html#";
static const char c_pageSurf[] = "q3map-surface-parameter-directives.html$";
static const char c_pageQER[] = "quake-editor-radiant-directives.html#";
static const char c_pageStage[] = "stage-directives.html#";

static const QColor c_colorForeground( Qt::white );
static const QColor c_colorBackground( 46, 52, 54 );

static const QColor c_colorComment( Qt::darkGray );
static const QColor c_colorShaderName( 249, 174, 88 );
static const QColor c_colorBrace( 248, 228, 0 );
static const QColor c_colorBraceLv1( 248, 228, 0 );
static const QColor c_colorBraceLv2( 236, 44, 215 );
static const QColor c_colorNumbers( 172, 214, 167 );
static const QColor c_colorColor3f( -1, -1, -1 ); // invalid color to pass info that it's color3f
static const QColor c_colorKeyLv1( 95, 141, 187 );  // build time keys
static const QColor c_colorKeyLv1E( 85, 111, 214 ); // engine runtime keys
static const QColor c_colorKeyLv2( 77, 172, 179 );  // stages
static const QColor c_colorValue( 196, 146, 188 );
static const QColor c_colorPath( 178, 168, 96 );

const char c_float_regex[] = "[+-]?(?:[0-9]*[.])?[0-9]+"; // (?:) - non capturing group
const char c_int_regex[] = "[+-]?\\d+";

struct ShaderFormat{
	const char * const key;
	const char * const page;
	const QColor color{};
	std::vector<const char *> values{};
	const QColor valuesColor = c_colorValue;
};
/* Legend:
%s = one of $values, generic string, if $values is empty //latter possibly not supported in completer
%t = texture path
%p = generic path
%f = float
%с = float of color3f
%i = int
*/
static const std::vector<ShaderFormat> g_shaderGeneralFormats{
	{
		"surfaceparm %s", c_pageSurf, c_colorKeyLv1, {
			"alphashadow",
			"antiportal",
			"areaportal",
			"botclip",
			"clusterportal",
			"detail",
			"donotenter",
			"dust",
			"flesh",
			"fog",
			"hint",
			"ladder",
			"lava",
			"lightfilter",
			"lightgrid",
			"metalsteps",
			"monsterclip",
			"nodamage",
			"nodlight",
			"nodraw",
			"nodrop",
			"noimpact",
			"nomarks",
			"nolightmap",
			"nosteps",
			"nonsolid",
			"origin",
			"playerclip",
			"pointlight",
			"skip",
			"sky",
			"slick",
			"slime",
			"structural",
			"trans",
			"trigger",
			"water",
		}
	},
	{
		"cull %s", c_pageGen, c_colorKeyLv1E, {
			"none",
			"disable",
			"twosided",
			"backsided",
			"backside",
			"back", //this last, so it doesn't take precendence, while matching longer version
		}
	},
	{
		"noPicMip", c_pageGen, c_colorKeyLv1E
	},
	{
		"noMipMaps", c_pageGen, c_colorKeyLv1E
	},
	{
		"polygonOffset", c_pageGen, c_colorKeyLv1E
	},
	{
		"portal", c_pageGen, c_colorKeyLv1E
	},
	{
		"skyParms %t %i -", c_pageGen, c_colorKeyLv1E
	},
	{
		"skyParms %t - -", c_pageGen, c_colorKeyLv1E
	},
	{
		"skyParms - %i -", c_pageGen, c_colorKeyLv1E
	},
	{
		"skyParms - - -", c_pageGen, c_colorKeyLv1E
	},
	{
		"fogParms ( %c %c %c ) %i", c_pageGen, c_colorKeyLv1E
	},
	{
		"sort %s", c_pageGen, c_colorKeyLv1E, {
			"portal",
			"Sky",
			"Opaque",
			"Decal",
			"SeeThrough",
			"Banner",
			"Underwater",
			"Additive",
			"Nearest",
		}
	},
	{
		"sort %i", c_pageGen, c_colorKeyLv1E
	},
	{
		"deformVertexes wave %f %s %f %f %f %f", c_pageGen, c_colorKeyLv1E, {
			"sin",
			"triangle",
			"square",
			"sawtooth",
			"inversesawtooth",
		}
	},
	{
		"deformVertexes move %f %f %f %s %f %f %f %f", c_pageGen, c_colorKeyLv1E, {
			"sin",
			"triangle",
			"square",
			"sawtooth",
			"inversesawtooth",
		}
	},
	{
		"deformVertexes %s %f %f", c_pageGen, c_colorKeyLv1E, {
			"normal",
		}
	},
	{
		"deformVertexes %s %f %f %f", c_pageGen, c_colorKeyLv1E, {
			"bulge",
		}
	},
	{
		"deformVertexes %s", c_pageGen, c_colorKeyLv1E, {
			"autosprite2",
			"autosprite", //this last, so it doesn't take precendence, while matching longer version
		}
	},
	{
		"qer_editorImage %t", "quake-editor-radiant-directives.html#editorImage", c_colorKeyLv1
	},
	{
		"qer_trans %f", "quake-editor-radiant-directives.html#trans", c_colorKeyLv1
	},
	{
		"qer_alphaFunc %s %f", "quake-editor-radiant-directives.html#alphaFunc", c_colorKeyLv1, {
			"equal",
			"greater",
			"less",
			"gequal",
			"lequal",
		}
	},
	{
		"light %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_alphaGen const %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_colorGen const ( %c %c %c )", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_alphaMod %s ( %f %f %f )", c_pageGlob, c_colorKeyLv1, {
			"dotproduct",
			"dotproduct2",
		}
	},
	{
		"q3map_alphaMod %s ( %f %f %f %f %f )", c_pageGlob, c_colorKeyLv1, {
			"dotproductScale",
			"dotproduct2Scale",
		}
	},
	{
		"q3map_alphaMod %s %f", c_pageGlob, c_colorKeyLv1, {
			"scale",
			"set",
		}
	},
	{
		"q3map_alphaMod %s", c_pageGlob, c_colorKeyLv1, {
			"volume",
		}
	},
	{
		"q3map_colorMod %s ( %f %f %f )", c_pageGlob, c_colorKeyLv1, {
			"dotproduct",
			"dotproduct2",
		}
	},
	{
		"q3map_colorMod %s ( %f %f %f %f %f )", c_pageGlob, c_colorKeyLv1, {
			"dotproductScale",
			"dotproduct2Scale",
		}
	},
	{
		"q3map_colorMod %s ( %c %c %c )", c_pageGlob, c_colorKeyLv1, {
			"scale",
			"set",
		}
	},
	{
		"q3map_colorMod %s", c_pageGlob, c_colorKeyLv1, {
			"volume",
		}
	},
	{
		"q3map_backShader %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_backSplash %f %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_baseShader %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_bounceScale %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_clipModel", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_cloneShader %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_deprecateShader %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_flare %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_flareShader %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_floodLight %c %c %c %f %f %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_fogDir ( %f %f %f )", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_foliage %p %f %f %f %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_forceMeta", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_fur %i %f %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_globalTexture", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_indexed", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_invert", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightImage %t", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightmapAxis %s", c_pageGlob, c_colorKeyLv1, {
			"x",
			"y",
			"z",
		}
	},
	{
		"q3map_lightmapBrightness %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightmapFilterRadius %f %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightmapMergable", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightmapSampleOffset %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightmapSampleSize %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightmapSize %i %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightRGB %c %c %c", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightStyle %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_lightSubdivide %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_noClip", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_noDirty", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_noFast", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_noFog", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_nonPlanar", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_normalImage %t", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_noTJunc", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_noVertexLight", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_offset %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_remapShader %p", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_shadeAngle %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_skylight %f %i %f %f %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_skylight %f %i", c_pageGlob, c_colorKeyLv1 //this last, so it doesn't take precendence, while matching longer version
	},
	{
		"q3map_splotchFix", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_styleMarker2", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_styleMarker", c_pageGlob, c_colorKeyLv1 //this last, so it doesn't take precendence, while matching longer version
	},
	{
		"q3map_sun %c %c %c %f %f %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_sunExt %c %c %c %f %f %f %f %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_surfaceLight %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_surfaceModel %p %f %f %f %f %f %f %i", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_tcGen %s ( %f %f %f ) ( %f %f %f )", c_pageGlob, c_colorKeyLv1, {
			"vector",
			"ivector",
		}
	},
	{
		"q3map_tcMod %s %f", c_pageGlob, c_colorKeyLv1, {
			"rotate",
		}
	},
	{
		"q3map_tcMod %s %f %f", c_pageGlob, c_colorKeyLv1, {
			"scale",
			"translate",
			"shift",
			"offset",
		}
	},
	{
		"q3map_terrain", c_pageGlob, c_colorKeyLv1
	},
	{
		"q3map_tessSize %f", c_pageGlob, c_colorKeyLv1
	},
	{
		"tessSize %f", "q3map-global-directives.html#q3map_tessSize", c_colorKeyLv1
	},
	{
		"q3map_vertexScale %f", c_pageGlob, c_colorKeyLv1
	},
};

static const std::vector<ShaderFormat> g_shaderStageFormats{
	{
		"map %s", c_pageStage, c_colorKeyLv2, {
			"$lightmap", // these do not work for highlighting, $ is special
			"$whiteimage", // next rule highlights and this works for completion - acceptable
		}
	},
	{
		"map %t", c_pageStage, c_colorKeyLv2
	},
	{
		"clampMap %t", c_pageStage, c_colorKeyLv2
	},
	{
		"videoMap %p", c_pageStage, c_colorKeyLv2
	},
	{
		"animMap %f %t %t %t %t %t %t %t %t", c_pageStage, c_colorKeyLv2
	},
	{
		"blendFunc %s", c_pageStage, c_colorKeyLv2, {
			"add",
			"filter",
			"blend",
		}
	},
	{
		"blendFunc %s %s", c_pageStage, c_colorKeyLv2, {
			"GL_DST_COLOR", //fixme this only in src blend
			"GL_SRC_COLOR", //fixme this only in dst blend
			"GL_ONE_MINUS_DST_COLOR", //fixme this only in src blend
			"GL_ONE_MINUS_SRC_COLOR", //fixme this only in dst blend
			"GL_SRC_ALPHA",
			"GL_ONE_MINUS_SRC_ALPHA",
			"GL_ONE", //this last, so it doesn't take precendence, while matching longer version
			"GL_ZERO", //this last, so it doesn't take precendence, while matching longer version
		}
	},
	{
		"rgbGen %s", c_pageStage, c_colorKeyLv2, {
			"identityLighting",
			"identity", //this last, so it doesn't take precendence, while matching longer version
			"vertex",
			"oneMinusVertex",
			"exactVertex",
			"entity",
			"oneMinusEntity",
			"lightingDiffuse",
		}
	},
	{
		"rgbGen wave %s %f %f %f %f", c_pageStage, c_colorKeyLv2, {
			"sin",
			"triangle",
			"square",
			"sawtooth",
			"inversesawtooth",
			"noise",
		}
	},
	{
		"rgbGen const ( %c %c %c )", c_pageStage, c_colorKeyLv2
	},
	{
		"alphaGen %s", c_pageStage, c_colorKeyLv2, {
			"lightingSpecular",
			"entity",
			"oneMinusEntity",
			"vertex",
			"oneMinusVertex",
			"portal",
		}
	},
	{
		"alphaGen wave %s %f %f %f %f", c_pageStage, c_colorKeyLv2, {
			"sin",
			"triangle",
			"square",
			"sawtooth",
			"inversesawtooth",
			"noise",
		}
	},
	{
		"alphaGen const %f", c_pageStage, c_colorKeyLv2
	},
	{
		"tcGen %s", c_pageStage, c_colorKeyLv2, {
			"base",
			"lightmap",
			"environment",
		}
	},
	{
		"tcGen vector ( %f %f %f ) ( %f %f %f )", c_pageStage, c_colorKeyLv2
	},
	{
		"tcMod rotate %f", c_pageStage, c_colorKeyLv2
	},
	{
		"tcMod %s %f %f", c_pageStage, c_colorKeyLv2, {
			"scale",
			"scroll",
		}
	},
	{
		"tcMod stretch %s %f %f %f %f", c_pageStage, c_colorKeyLv2, {
			"sin",
			"triangle",
			"square",
			"sawtooth",
			"inversesawtooth",
			"noise",
		}
	},
	{
		"tcMod transform %f %f %f %f %f %f", c_pageStage, c_colorKeyLv2
	},
	{
		"tcMod turb %f %f %f %f", c_pageStage, c_colorKeyLv2
	},
	{
		"depthFunc %s", c_pageStage, c_colorKeyLv2, {
			"equal",
			"lequal",
		}
	},
	{
		"depthWrite", c_pageStage, c_colorKeyLv2
	},
	{
		"detail", c_pageStage, c_colorKeyLv2
	},
	{
		"alphaFunc %s", c_pageStage, c_colorKeyLv2, {
			"GT0",
			"LT128",
			"GE128",
		}
	},
};

struct BlockData : public QTextBlockUserData
{
	const ShaderFormat *shaderFormat;
	BlockData( const ShaderFormat *shaderFormat ) : shaderFormat( shaderFormat ){}
};

enum EShaderDepth
{
	eShaderDepth0 = 512, //shader names
	eShaderDepth1 = 513, //general directives
	eShaderDepth2 = 514, //stages
};


class ShaderHighlighter : public QSyntaxHighlighter
{
public:
	ShaderHighlighter( QTextDocument *parent = 0 );
protected:
	void highlightBlock( const QString &text ) override;
private:
	void depthSet( const std::int16_t depth ){
		std::int32_t state = currentBlockState();
		memcpy( &state, &depth, 2 );
		setCurrentBlockState( state );
	}
public:
	static std::int16_t depth( const int state ){
		return state;
	}
private:
	bool stateIsComment( const int state ) const {
		return !( state & c_comment_flag );
	}
	void stateSetComment( const bool enabled ){
		setCurrentBlockState( enabled
		                      ? ( currentBlockState() & ~c_comment_flag )
		                      : ( currentBlockState() | c_comment_flag ) );
	}

	struct Rule{
		QRegularExpression pattern;
		std::vector<QColor> colors;
		const ShaderFormat& shaderFormat;
		Rule( const ShaderFormat& shaderFormat ) : shaderFormat( shaderFormat ){}
	};
	std::vector<Rule> m_rulesGeneral; // general directives
	std::vector<Rule> m_rulesStage; // stage directives

	const int c_comment_flag = ( 1 << 16 );

	QRegularExpression commentStartExpression{ QStringLiteral( "/\\*" ) };
	QRegularExpression commentEndExpression{ QStringLiteral( "\\*/" ) };
	QRegularExpression commentInlineExpression{ QStringLiteral( "//" ) };
};

ShaderHighlighter::ShaderHighlighter( QTextDocument *parent )
	: QSyntaxHighlighter( parent )
{
	//? may be alt style with \b match in the end
	const auto construc_rules = []( std::vector<Rule>& rules, const std::vector<ShaderFormat>& formats ){
		for( const auto& format : formats ){
			Rule& rule = rules.emplace_back( format );
			QString pattern( "(\\s*" );
			rule.colors.push_back( format.color );
			for( const char *c = format.key; *c; ++c ){
				if( *c == ' ' ){
					pattern += "\\s+";
				}
				else if( string_equal_prefix( c, "%s" ) ){ // string
					++c;
					pattern += ")((?:"; // extra inner non capturing group, as space may be added
					for( const auto value : format.values ){
						pattern += value;
						pattern += '|';
					}
					if( format.values.empty() ){  // no predefined list = generic string
						pattern += "\\S+|";
					}
					pattern.back() = ')'; // replace trailing | by non capturing group end
					rule.colors.push_back( format.valuesColor );
				}
				else if( string_equal_prefix( c, "%t" ) ){ // texture path
					++c;
					pattern += ")(";
					pattern += "\\S{1,63}";
					rule.colors.push_back( c_colorPath );
					if( string_equal_prefix_nocase( format.key, "animMap" ) ){ // special case... variable num of paths
						pattern += "(?:\\s+\\S{1,63})+";
						break;
					}
				}
				else if( string_equal_prefix( c, "%p" ) ){ // generic path
					++c;
					pattern += ")(";
					pattern += "\\S{1,63}";
					rule.colors.push_back( c_colorPath );
				}
				else if( string_equal_prefix( c, "%f" ) ){ // float
					++c;
					pattern += ")(";
					pattern += c_float_regex;
					rule.colors.push_back( c_colorNumbers );
				}
				else if( string_equal_prefix( c, "%c %c %c" ) ){ // color3f
					c += strlen( "%c %c %c" ) - 1;
					pattern += ")(";
					pattern.append( c_float_regex ).append( "\\s+" ).append( c_float_regex ).append( "\\s+" ).append( c_float_regex );
					rule.colors.push_back( c_colorColor3f );
				}
				else if( string_equal_prefix( c, "%i" ) ){ // int
					++c;
					pattern += ")(";
					pattern += c_int_regex;
					rule.colors.push_back( c_colorNumbers );
				}
				else if( *c == '(' ){
					pattern += ")(";
					pattern += "\\(";
					rule.colors.push_back( c_colorBrace );
				}
				else if( *c == ')' ){
					pattern += ")(";
					pattern += "\\)";
					rule.colors.push_back( c_colorBrace );
				}
				else{
					pattern += *c;
				}
			}
			pattern += ')';
			rule.pattern = QRegularExpression( pattern, QRegularExpression::PatternOption::CaseInsensitiveOption );
		}
	};

	construc_rules( m_rulesGeneral, g_shaderGeneralFormats );
	construc_rules( m_rulesStage, g_shaderStageFormats );
}

void ShaderHighlighter::highlightBlock( const QString &text )
{
	int start = 0;
	stateSetComment( false );
	depthSet( depth( previousBlockState() ) == -1? eShaderDepth0 : depth( previousBlockState() ) );

	if( auto *data = currentBlockUserData() ){
		static_cast<BlockData*>( data )->shaderFormat = nullptr;
	}

	const auto highlight_normal = [&]( const std::vector<Rule>& rules, const QStringView str ){
		for( const auto& rule : rules ){
			const auto match = rule.pattern.match( str, start, QRegularExpression::MatchType::NormalMatch,
				QRegularExpression::MatchOption::AnchoredMatchOption );
			if( match.hasMatch() ){
				for( int i = 1; i <= match.lastCapturedIndex(); ++i ){
					if( !rule.colors[i - 1].isValid() ){ // c_colorColor3f
						Vector3 clr( 0 );
						string_parse_vector3( match.captured( i ).toLatin1().constData(), clr );
						if( const auto max = vector3_max_component( clr ); max > 0 ) // normalise color
							clr /= max;
						QTextCharFormat format;
						format.setBackground( QColor::fromRgbF( clr[0], clr[1], clr[2] ) );
						format.setForeground( QColor::fromRgbF( 1 - clr[0], 1 - clr[1], 1 - clr[2] ) );
						setFormat( match.capturedStart( i ), match.capturedLength( i ), format );
					}
					else
						setFormat( match.capturedStart( i ), match.capturedLength( i ), rule.colors[i - 1] );
				}

				if( auto *data = currentBlockUserData() )
					static_cast<BlockData*>( data )->shaderFormat = &rule.shaderFormat;
				else
					setCurrentBlockUserData( new BlockData( &rule.shaderFormat ) );

				break;
			}
		}
	};

	const auto parse_normal = [&]( const QStringView str ){
		if( start < str.length() ){
			const auto d = depth( currentBlockState() );
			switch ( d )
			{
			case eShaderDepth0:
				setFormat( start, str.length(), c_colorShaderName );
				break;
			case eShaderDepth1:
				highlight_normal( m_rulesGeneral, str );
				break;
			case eShaderDepth2:
				highlight_normal( m_rulesStage, str );
				break;
			default:
				break;
			}
		}
	};

	const auto parse_blocks = [&]( const QStringView str ){
		while( start < str.length() ){
			const int matchOpen = str.indexOf( '{', start );
			const int matchClose = str.indexOf( '}', start );
			if( matchOpen >= 0 && ( matchClose < 0 || matchOpen < matchClose ) ){
				parse_normal( QStringView( str.cbegin(), matchOpen ) );
				const auto d = depth( currentBlockState() ) + 1;
				depthSet( d );
				if( d == eShaderDepth1 )
					setFormat( matchOpen, 1, c_colorBraceLv1 );
				else if( d == eShaderDepth2 )
					setFormat( matchOpen, 1, c_colorBraceLv2 );
				start = matchOpen + 1;
			}
			else if( matchClose >= 0 && ( matchOpen < 0 || matchClose < matchOpen ) ){
				parse_normal( QStringView( str.cbegin(), matchClose ) );
				const auto d = depth( currentBlockState() ) - 1;
				depthSet( d );
				if( d == eShaderDepth0 )
					setFormat( matchClose, 1, c_colorBraceLv1 );
				else if( d == eShaderDepth1 )
					setFormat( matchClose, 1, c_colorBraceLv2 );
				start = matchClose + 1;
			}
			else{
				parse_normal( QStringView( str.cbegin(), str.length() ) );
				start = str.length();
			}
		}
	};

	const auto parse_block_comment = [&](){
		QRegularExpressionMatch match = commentEndExpression.match( text, start );
		if( !match.hasMatch() ){ // unclosed comment
			stateSetComment( true );
			setFormat( start, text.length() - start, c_colorComment );
			start = text.length();
		}
		else{ // closed
			stateSetComment( false );
			setFormat( start, match.capturedEnd() - start, c_colorComment );
			start = match.capturedEnd();
		}
	};

	if( stateIsComment( previousBlockState() ) ){ // prev block is unclosed multiline comment
		parse_block_comment();
	}

	while( start < text.length() ){
		const int matchBlock = commentStartExpression.match( text, start ).capturedStart();
		const int matchInline = commentInlineExpression.match( text, start ).capturedStart();
		if( matchBlock >= 0 && ( matchInline < 0 || matchBlock < matchInline ) ){
			parse_blocks( QStringView( text.constData(), matchBlock ) );
			start = matchBlock;
			parse_block_comment();
		}
		else if( matchInline >= 0 && ( matchBlock < 0 || matchInline < matchBlock ) ){
			parse_blocks( QStringView( text.constData(), matchInline ) );
			setFormat( matchInline, text.length() - matchInline, c_colorComment );
			start = text.length();
		}
		else{
			parse_blocks( QStringView( text ) );
			start = text.length();
		}
	}
}

class QLineEdit_search : public QLineEdit
{
	QPlainTextEdit& m_textEdit;
public:
	QLineEdit_search( QPlainTextEdit& textEdit ) : m_textEdit( textEdit ){
		setPlaceholderText( QString::fromUtf8( u8"🔍" ) );
		QObject::connect( this, &QLineEdit::textEdited, [this]( const QString &text ){
			// when typing, we do not want jumping to next occurence on each letter input, set cursor to selection start
			if( auto cursor = m_textEdit.textCursor(); cursor.hasSelection() ){
				cursor.setPosition( cursor.selectionStart() );
				m_textEdit.setTextCursor( cursor );
			}
			search( text );
		} );
	}
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			// fix leaking keys
			if( keyEvent->key() == Qt::Key_Enter
			 || keyEvent->key() == Qt::Key_Return
			 || keyEvent->key() == Qt::Key_Up
			 || keyEvent->key() == Qt::Key_Down
			 || keyEvent->key() == Qt::Key_Tab )
				event->accept();
		}
		return QLineEdit::event( event );
	}
	void keyPressEvent( QKeyEvent *event ) override {
		if( !this->text().isEmpty() ){
			if( ( ( event->key() == Qt::Key_Return || event->key() == Qt::Key_Down ) && event->modifiers() == Qt::KeyboardModifier::NoModifier )
			|| ( event->key() == Qt::Key_Enter && event->modifiers() == Qt::KeyboardModifier::KeypadModifier ) )
				search( this->text() );
			else if( ( ( event->key() == Qt::Key_Return || ( event->key() == Qt::Key_Enter && ( event->modifiers() & Qt::KeyboardModifier::KeypadModifier ) ) )
			&& ( event->modifiers() & Qt::KeyboardModifier::ControlModifier || event->modifiers() & Qt::KeyboardModifier::ShiftModifier ) )
			|| ( event->key() == Qt::Key_Up && event->modifiers() == Qt::KeyboardModifier::NoModifier ) )
				search( this->text(), true );
		}
		QLineEdit::keyPressEvent( event );
	}
private:
	void search( const QString &text, bool reverse = false, bool words = false, bool casesens = false ){
		QTextDocument::FindFlags flag;
		if( reverse ) flag |= QTextDocument::FindBackward;
		if( casesens ) flag |= QTextDocument::FindCaseSensitively;
		if( words ) flag |= QTextDocument::FindWholeWords;

		QTextCursor cursor = m_textEdit.textCursor();
		QTextCursor cursorSaved = cursor; // save the cursor position

		if ( !m_textEdit.find( text, flag ) ){
			cursor.movePosition( reverse? QTextCursor::End : QTextCursor::Start ); //nothing is found: jump to start/end
			m_textEdit.setTextCursor( cursor );
			if ( !m_textEdit.find( text, flag ) ){
				m_textEdit.setTextCursor( cursorSaved ); // word not found : we set the cursor back to its initial position
			}
		}
	}
};


class TexTree
{
public:
	struct Prefix{ const char *prefix; };
	struct Compare{
		using is_transparent = void;

		bool operator()( const TexTree& texTree, const TexTree& texTree2 ) const {
			return string_less_nocase( texTree.m_name.c_str(), texTree2.m_name.c_str() );
		}
		bool operator()( const TexTree& texTree, const char *name ) const {
			return string_less_nocase( texTree.m_name.c_str(), name );
		}
		bool operator()( const char *name, const TexTree& texTree ) const {
			return string_less_nocase( name, texTree.m_name.c_str() );
		}
		bool operator()( const TexTree& texTree, const Prefix prefix ) const {
			return string_compare_nocase_n( texTree.m_name.c_str(), prefix.prefix, strlen( prefix.prefix ) ) < 0;
		}
		bool operator()( const Prefix prefix, const TexTree& texTree ) const {
			return string_compare_nocase_n( texTree.m_name.c_str(), prefix.prefix, strlen( prefix.prefix ) ) > 0;
		}
		bool operator()( const TexTree& texTree, const StringRange range ) const {
			return string_compare_nocase_n( texTree.m_name.c_str(), range.begin(), range.size() ) < 0;
		}
		bool operator()( const StringRange range, const TexTree& texTree ) const {
			return string_compare_nocase_n( texTree.m_name.c_str(), range.begin(), range.size() ) > 0;
		}
	};

	const CopiedString m_name;
	TexTree() = default;
	TexTree( const StringRange range ) : m_name( range ){
	}
	TexTree( const char *name ) : m_name( name ){
	}
	mutable std::set<TexTree, Compare> m_children;
	void insert( const char* filepath ) const {
		if( const char* slash = strchr( filepath, '/' ) ){
			m_children.emplace( StringRange( filepath, slash ) ).first->insert( slash + 1 );
		}
		else{
			m_children.emplace( filepath );
		}
	}

	std::pair<decltype( m_children )::const_iterator, decltype( m_children )::const_iterator>
	find( const char *filepath ) const {
		if( const char* slash = strchr( filepath, '/' ) ){
			if( const auto it = m_children.find( StringRange( filepath, slash ) ); it != m_children.cend() ){
				return it->find( ++slash );
			}
			else{
				return { m_children.cend(), m_children.cend() };
			}
		}
		else{
			return m_children.equal_range( Prefix{ filepath } );
		}
	}

	bool isLeaf() const {
		return m_children.empty();
	}
};


class LineNumberArea : public QWidget
{
	QPlainTextEdit *m_textEdit;
	const Callback1<QPaintEvent*> m_paintCallback;
public:
	LineNumberArea( QPlainTextEdit *textEdit, const decltype( m_paintCallback )& paintCallback ) :
		QWidget( textEdit ), m_textEdit( textEdit ), m_paintCallback( paintCallback ){}

	QSize sizeHint() const override	{
		return QSize( lineNumberAreaWidth(), 0 );
	}
	int lineNumberAreaWidth() const {
		const int digits = 1 + std::log10( std::max( 1, m_textEdit->blockCount() ) );
		return 3 + 10 + m_textEdit->fontMetrics().horizontalAdvance( QLatin1Char('9') ) * digits;
	}
	void updateLineNumberArea( const QRect &rect, int dy ){
		if( dy )
			scroll( 0, dy );
		else
			update( 0, rect.y(), width(), rect.height() );
	}
protected:
	void paintEvent( QPaintEvent *event ) override {
		m_paintCallback( event );
	}
};

class QPlainTextEdit_Shader : public QPlainTextEdit
{
	QCompleter *m_completer;
	TexTree m_texTree;
	LineNumberArea *m_lineNumberArea;
public:
	QPlainTextEdit_Shader(){
		m_completer = new QCompleter( this );
		m_completer->setWidget( this );
		m_completer->setCompletionMode( QCompleter::CompletionMode::UnfilteredPopupCompletion );
		QObject::connect( this, &QPlainTextEdit::textChanged, [this](){ autoComplete(); } );
		QObject::connect( m_completer, QOverload<const QString &>::of( &QCompleter::activated ), [this]( const QString& str ){ autoCompleteInsert( str ); } );

		setLineWrapMode( QPlainTextEdit::LineWrapMode::NoWrap );
		QFont font( "nonexistent" ); // dummy name is required
		font.setStyleHint( QFont::Monospace );
		setFont( font );
		updateTabStopDistance();
		new ShaderHighlighter( document() );

		m_lineNumberArea = new LineNumberArea( this, MemberCaller1<QPlainTextEdit_Shader, QPaintEvent *, &QPlainTextEdit_Shader::lineNumberAreaPaintEvent>( *this ) );
		QObject::connect( this, &QPlainTextEdit::blockCountChanged, [this]( int newBlockCount ){ updateLineNumberAreaWidth(); } );
		QObject::connect( this, &QPlainTextEdit::updateRequest, [this]( const QRect &rect, int dy ){
			m_lineNumberArea->updateLineNumberArea( rect, dy );
			if( rect.contains( viewport()->rect() ) )
				updateLineNumberAreaWidth();
		} );
		updateLineNumberAreaWidth();

		// force back/foreground colors to not be ruined by global theme
		QPalette pal = palette();
		pal.setColor( QPalette::Base, c_colorBackground );
		pal.setColor( QPalette::Text, c_colorForeground );
		setPalette( pal );
	}
	void lineNumberAreaPaintEvent( QPaintEvent *event ){
		QPainter painter( m_lineNumberArea );
		painter.setFont( font() );
		painter.setPen( Qt::darkGray );
		painter.fillRect( event->rect(), c_colorBackground.lighter( 128 ) );

		QTextBlock block = firstVisibleBlock();
		int blockNumber = block.blockNumber();
		int top = qRound( blockBoundingGeometry( block ).translated( contentOffset() ).top() );
		int bottom = top + qRound( blockBoundingRect( block ).height() );

		while( block.isValid() && top <= event->rect().bottom() ){
			if( block.isVisible() && bottom >= event->rect().top() ){
				painter.drawText( 0, top, m_lineNumberArea->width() - 10, fontMetrics().height(), Qt::AlignRight, QString::number( blockNumber + 1 ) );
			}

			block = block.next();
			top = bottom;
			bottom = top + qRound( blockBoundingRect( block ).height() );
			++blockNumber;
		}
	}
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			// fix leaking shortcuts
			if( keyEvent->key() == Qt::Key_PageUp
			 || keyEvent->key() == Qt::Key_PageDown
			 || keyEvent->key() == Qt::Key_Up
			 || keyEvent->key() == Qt::Key_Down
			 || keyEvent->key() == Qt::Key_Escape // esc for completer
			 || keyEvent == QKeySequence::StandardKey::DeleteEndOfWord
			 || keyEvent == QKeySequence::StandardKey::DeleteStartOfWord )
				event->accept();
			// cut current line w/o selection
			if( keyEvent == QKeySequence::StandardKey::Cut && !textCursor().hasSelection() ){
				auto cursor = textCursor();
				cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock );
				if( !cursor.movePosition( QTextCursor::MoveOperation::NextBlock, QTextCursor::MoveMode::KeepAnchor ) ){ //no next line
					cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock );
					if( cursor.movePosition( QTextCursor::MoveOperation::PreviousBlock, QTextCursor::MoveMode::KeepAnchor ) ) //yes prev line
						cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock, QTextCursor::MoveMode::KeepAnchor );
					else
						cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock, QTextCursor::MoveMode::KeepAnchor ); //single line left
				}
				// while key is held, tight stream of clipboard copies causes crash (windows)
				// thus let's only copy on single press, furthermore it's not too reasonable to do so otherwise
				if( !keyEvent->isAutoRepeat() ){
					setTextCursor( cursor );
					this->cut();
				}
				else{
					cursor.removeSelectedText();
				}
				event->accept();
				return true;
			}
			// copy current line w/o selection
			if( keyEvent == QKeySequence::StandardKey::Copy && !textCursor().hasSelection() && !keyEvent->isAutoRepeat() ){
				const auto cursorOriginal = textCursor();
				auto cursor( cursorOriginal );
				cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock );
				cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock, QTextCursor::MoveMode::KeepAnchor ); // helps when no next block
				cursor.movePosition( QTextCursor::MoveOperation::NextBlock, QTextCursor::MoveMode::KeepAnchor );
				setTextCursor( cursor );
				this->copy();
				setTextCursor( cursorOriginal );
				event->accept();
				return true;
			}
			// move line down
			if( keyEvent->modifiers() == Qt::KeyboardModifier::AltModifier && keyEvent->key() == Qt::Key_Down ){
				auto cursor = textCursor();
				if( !cursor.hasSelection() ){
					cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock );
					cursor.movePosition( QTextCursor::MoveOperation::NextBlock, QTextCursor::MoveMode::KeepAnchor );
				}

				if( cursor.hasSelection() ){
					const int start = cursor.selectionStart();
					const int end = cursor.selectionEnd();
					cursor.setPosition( end );
					if( cursor.atBlockStart() || cursor.movePosition( QTextCursor::MoveOperation::NextBlock ) ){ // ensure there is next line
						cursor.setPosition( start, QTextCursor::MoveMode::KeepAnchor );
						cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock, QTextCursor::MoveMode::KeepAnchor );
						QString txt = cursor.selectedText();
						cursor.beginEditBlock();
						cursor.removeSelectedText();
						if( cursor.movePosition( QTextCursor::MoveOperation::NextBlock ) ){
							const int newStart = cursor.position();
							cursor.insertText( txt );
							cursor.setPosition( newStart );
							cursor.setPosition( newStart + txt.length(), QTextCursor::MoveMode::KeepAnchor );
						}
						else{
							cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock );
							const int newStart = cursor.position();
							txt.prepend( '\n' );
							txt.chop( 1 );
							cursor.insertText( txt );
							cursor.setPosition( newStart + 1 );
							cursor.setPosition( newStart + txt.length(), QTextCursor::MoveMode::KeepAnchor );
						}
						cursor.endEditBlock();
						setTextCursor( cursor );
					}
				}

				event->accept();
				return true;
			}
			// move line up
			if( keyEvent->modifiers() == Qt::KeyboardModifier::AltModifier && keyEvent->key() == Qt::Key_Up ){
				auto cursor = textCursor();
				{
					const int start = cursor.selectionStart();
					const int end = cursor.selectionEnd();
					cursor.setPosition( start );
					if( cursor.movePosition( QTextCursor::MoveOperation::PreviousBlock ) ){ // ensure there is prev line
						cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock ); // returns false for empty line...
						cursor.setPosition( end, QTextCursor::MoveMode::KeepAnchor );
						if( !cursor.atBlockStart() || !textCursor().hasSelection() ) // select line to the end
							cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock, QTextCursor::MoveMode::KeepAnchor );
						else if( cursor.anchor() != end - 1 ) // remove trailing \n selection, unless it's the only \n
							cursor.setPosition( end - 1, QTextCursor::MoveMode::KeepAnchor );

						QString txt = cursor.selectedText();
						cursor.beginEditBlock();
						cursor.removeSelectedText();
						{
							cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock );
							const int newStart = cursor.position();
							txt.append( '\n' );
							txt.remove( 0, 1 );
							cursor.insertText( txt );
							cursor.setPosition( newStart );
							cursor.setPosition( newStart + txt.length(), QTextCursor::MoveMode::KeepAnchor );
						}
						cursor.endEditBlock();
						setTextCursor( cursor );
					}
				}

				event->accept();
				return true;
			}
			// duplicate
			if( keyEvent->modifiers() == Qt::KeyboardModifier::ControlModifier && keyEvent->key() == Qt::Key_D ){
				auto cursor = textCursor();
				if( cursor.hasSelection() ){
					cursor.setPosition( cursor.selectionStart() );
					cursor.insertText( textCursor().selectedText() );
				}
				else{
					cursor.movePosition( QTextCursor::MoveOperation::StartOfBlock );
					cursor.movePosition( QTextCursor::MoveOperation::EndOfBlock, QTextCursor::MoveMode::KeepAnchor );
					const QString txt = cursor.selectedText() + '\n';
					cursor.setPosition( cursor.selectionStart() );
					cursor.insertText( txt );
				}

				event->accept();
				return true;
			}
		}
		return QPlainTextEdit::event( event );
	}
	void keyPressEvent( QKeyEvent *e ) override {
		if( m_completer->popup()->isVisible() ){ // The following keys are forwarded by the completer to the widget
			if( e->key() == Qt::Key_Enter
			 || e->key() == Qt::Key_Return
			 || e->key() == Qt::Key_Escape
			 || e->key() == Qt::Key_Tab
			 || e->key() == Qt::Key_Backtab ){
				e->ignore();
				return; // let the completer do default behavior
			 }
		}
		QPlainTextEdit::keyPressEvent( e );
	}
	void wheelEvent( QWheelEvent *e ) override {
		// this is only allowed for read only state for some reason, we want for editable too
		if( e->modifiers() & Qt::ControlModifier ){
			const float delta = e->angleDelta().y() / 120.f;
			zoomInF( delta );
			updateTabStopDistance();
			return;
		}
		QPlainTextEdit::wheelEvent(e);
	}
	void paintEvent( QPaintEvent* pEvent ) override {
		static QRect rect;
		static int block;
		int newblock = textCursor().blockNumber();
		QRect newrect = cursorRect();
		newrect.setLeft( -1 );
		newrect.setRight( width() - 1 );
		if( rect != newrect || block != newblock ){
			QRegion region( newrect + QMargins( 0, 0, 0, 1 ) ); // expand invalidated area, bottom line appears drawn 1px lower
			// this may differ from static rect, if e.g. scrolled, thus reevaluate
			QRect oldr = cursorRect( QTextCursor( document()->findBlockByNumber( block ) ) );
			oldr.setLeft( -1 );
			oldr.setRight( width() - 1 );
			region += oldr + QMargins( 0, 0, 0, 1 );

			rect = newrect;
			block = newblock;
			viewport()->update( region );
		}
		else{ // highlight current line
			QPainter painter( viewport() );
			painter.setPen( c_colorBackground.lighter( 150 ) );
			painter.drawRect( rect );
		}
		QPlainTextEdit::paintEvent( pEvent );
	}
	void resizeEvent( QResizeEvent *e ) override {
		QPlainTextEdit::resizeEvent( e );

		const QRect cr = contentsRect();
		m_lineNumberArea->setGeometry( QRect( cr.left(), cr.top(), m_lineNumberArea->lineNumberAreaWidth(), cr.height() ) );
	}
	void updateLineNumberAreaWidth(){
		setViewportMargins( m_lineNumberArea->lineNumberAreaWidth(), 0, 0, 0 );
	}
private:
	void updateTabStopDistance(){
		setTabStopDistance( fontMetrics().horizontalAdvance( "MMMM" ) );
	}
	void texTree_construct(){
		class LoadTexturesByTypeVisitor : public ImageModules::Visitor
		{
			const char* m_dirstring;
			TexTree& m_texTree;
			mutable StringOutputStream m_stringStream;
		public:
			void insert( const char *name ) const {
				m_texTree.insert( m_stringStream( m_dirstring, PathExtensionless( name ) ) );
			}
			typedef ConstMemberCaller1<LoadTexturesByTypeVisitor, const char*, &LoadTexturesByTypeVisitor::insert> InsertCaller;
			LoadTexturesByTypeVisitor( const char* dirstring, TexTree& texTree ) :
				m_dirstring( dirstring ), m_texTree( texTree ), m_stringStream( 64 )
			{}
			void visit( const char* minor, const _QERPlugImageTable& table ) const {
				GlobalFileSystem().forEachFile( m_dirstring, minor, InsertCaller( *this ), 99 );
			}
		};

		Radiant_getImageModules().foreachModule( LoadTexturesByTypeVisitor( "textures/", m_texTree ) );
		Radiant_getImageModules().foreachModule( LoadTexturesByTypeVisitor( "models/", m_texTree ) );
		Radiant_getImageModules().foreachModule( LoadTexturesByTypeVisitor( "env/", m_texTree ) );
	}
	auto texTree_find_completion( const char *path ){
		if( m_texTree.m_children.empty() )
			texTree_construct();

		return m_texTree.find( path );
	}
	void autoComplete(){
		QTextCursor cursor = textCursor();
		cursor.movePosition( QTextCursor::MoveOperation::StartOfLine, QTextCursor::MoveMode::KeepAnchor );
		const QString selectedText = cursor.selectedText();
		const auto line = selectedText.split( QRegularExpression( "\\s+" ), Qt::SplitBehaviorFlags::SkipEmptyParts );

		const int depth = ShaderHighlighter::depth( cursor.block().userState() );

		QStringList list;
		const auto list_push = [&list]( const QString& string ){
			if( !list.contains( string, Qt::CaseSensitivity::CaseInsensitive ) )
				list.push_back( string );
		};

		if( !line.isEmpty() && depth == eShaderDepth1 && line.back() == '{' ){
			for( const auto& shader : c_shaderTemplates )
				list.push_back( shader.name );
		}
		else if( !line.isEmpty() && ( depth == eShaderDepth1 || depth == eShaderDepth2 ) ){
			const auto& shaderFormats = ( depth == eShaderDepth1 )? g_shaderGeneralFormats : g_shaderStageFormats;
			for( const auto& format : shaderFormats ){
				const auto tokens = QString( format.key ).split( ' ', Qt::SplitBehaviorFlags::SkipEmptyParts );
				if( line.size() > tokens.size() ) // line too long, nothing to match
					continue;
				for( int i = 0; i < line.size(); ++i ){
					const auto& word = line[i];
					const auto& token = tokens[i];

					const auto complete_tex_path = [&]( const char *path ){
						const auto range = texTree_find_completion( path );
						for( auto it = range.first; it != range.second; ++it ){
							QString str( it->m_name.c_str() );
							if( it->isLeaf() ){
								str += ".tga";
								if( i + 1 < tokens.size() ) // there is next token, add space
									str += ' ';
							}
							else{
								str += '/';
							}
							list_push( str );
						}
					};

					const auto push_next_token = [&](){
						++i; // advance to the next token
						const auto push_token = [&]( QString token ){
							if( i + 1 < tokens.size() ) // there is next token, add space
								token.append( ' ' );
							if( !selectedText.endsWith( ' ' ) ) // no space after matched word, add one
								token.prepend( ' ' );
							list_push( token );
						};
						if( i < tokens.size() ){ // token is available
							if( tokens[i] == "%s" ){
								for( const auto value : format.values ){
									push_token( value );
								}
							}
							else if( tokens[i] == "%f" || tokens[i] == "%c" ){
								push_token( ".0" );
							}
							else if( tokens[i] == "%i" ){
								push_token( "1" );
							}
							else if( tokens[i] == "%t" ){
								complete_tex_path( "" );
							}
							else if( tokens[i] == "%p" ){
								push_token( "textures/" ); // isn't textures/ every time, but mostly
							}
							else{
								push_token( tokens[i] );
							}
						}
					};

					const auto values_contain = []( const std::vector<const char*> values, const QString& string ){
						for( const auto value : values )
							if( string.compare( value, Qt::CaseSensitivity::CaseInsensitive ) == 0 )
								return true;
						return false;
					};

					if( i == line.size() - 1 ){ // last word, partial match is okay
						if( token == "%s" ){
							if( values_contain( format.values, word ) ){ // exact match, grab next token
								push_next_token();
							}
							else{ // partial match
								if( !selectedText.back().isSpace() ){
									for( const auto v : format.values ){
										QString value( v );
										if( value.startsWith( word, Qt::CaseSensitivity::CaseInsensitive ) ){
											if( i + 1 < tokens.size() ) // there is next token, add space
												value.append( ' ' );
											list_push( value );
										}
									}
								}
							}
						}
						else if( token == "%f" || token == "%c" ){
							if( QRegularExpression( QRegularExpression::anchoredPattern( c_float_regex ) ).match( word ).hasMatch() )
								push_next_token();
						}
						else if( token == "%i" ){
							if( QRegularExpression( QRegularExpression::anchoredPattern( c_int_regex ) ).match( word ).hasMatch() )
								push_next_token();
						}
						else if( token == "%t" ){ //any string is fine
							if( selectedText.back().isSpace() )
								push_next_token();
							else
								complete_tex_path( word.toLatin1().constData() );
						}
						else if( token == "%p" ){ //any string is fine
							push_next_token();
						}
						else if( token.compare( word, Qt::CaseSensitivity::CaseInsensitive ) == 0 ){ // exact match, grab next token
							push_next_token();
						}
						else if( token.startsWith( word, Qt::CaseSensitivity::CaseInsensitive ) ){ // partial match
							if( !selectedText.back().isSpace() ){
								if( i + 1 < tokens.size() ) // there is next token, add space
									list_push( token + ' ' );
								else
									list_push( token );
							}
						}
					}
					else{ // midway, want exact match
						if( token == "%s" ){
							if( values_contain( format.values, word ) )
								continue;
						}
						else if( token == "%f" || token == "%c" ){
							if( QRegularExpression( QRegularExpression::anchoredPattern( c_float_regex ) ).match( word ).hasMatch() )
								continue;
						}
						else if( token == "%i" ){
							if( QRegularExpression( QRegularExpression::anchoredPattern( c_int_regex ) ).match( word ).hasMatch() )
								continue;
						}
						else if( token == "%t" || token == "%p" ){
							continue; //any string is fine
						}
						else if( token.compare( word, Qt::CaseSensitivity::CaseInsensitive ) == 0 ){
							continue;
						}
						break; // no match
					}
				}
			}
		}
		if( !list.isEmpty() ){
			if( list.size() > 3 ){ // try to find long enough common prefix to reduce typing
				int len = list[0].length();
				for( int i = 0; i < list.size() && len > 0; ++i ){
					len = std::min( len, list[i].length() );
					for( int j = 0; j < len; ++j ){
						if( list[0][j].toLower() != list[i][j].toLower() ){
							len = j;
							break;
						}
					}
				}
				const int postSlashId = line.last().lastIndexOf( '/' ) + 1; // -1 + 1 when not found
				if( len >= line.last().length() - postSlashId + 2 ){ // two or more chars may be completed, cool
					QString prefix( list[0].left( len ) );
					list.clear();
					list.push_back( prefix );
				}
			}
			auto *model = new QStringListModel( list, m_completer );
			m_completer->setModel( model );
			m_completer->popup()->setCurrentIndex( m_completer->completionModel()->index( 0, 0 ) );
			QRect cr = cursorRect();
			cr.setWidth( m_completer->popup()->sizeHintForColumn( 0 ) + m_completer->popup()->verticalScrollBar()->sizeHint().width() );
			m_completer->complete( cr );
		}
		else{
			m_completer->popup()->hide();
		}
	}
	void autoCompleteInsert( const QString& str ){
		QTextCursor cursor = textCursor();
		QTextCursor cu = cursor;
		cu.movePosition( QTextCursor::MoveOperation::PreviousCharacter, QTextCursor::MoveMode::KeepAnchor );
		if( !str.startsWith( ' ' ) && !cu.selectedText().back().isSpace() ) // completing current token: overwrite it
			cursor.movePosition( QTextCursor::MoveOperation::StartOfWord, QTextCursor::MoveMode::KeepAnchor );

		for( const auto& shader : c_shaderTemplates ){
			if( shader.name == str ){
				cursor.insertText( shader.text );
				return;
			}
		}
		cursor.insertText( str );
	}
};

class TextEditor : public QObject
{
	QWidget *m_window = 0;
	QPlainTextEdit *m_textView; // slave, text widget from the gtk editor
	QPushButton *m_button; // save button
	CopiedString m_filename;

	void construct(){
		m_window = new QWidget( MainFrame_getWindow(), Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
		g_guiSettings.addWindow( m_window, "ShaderEditor/geometry" );
		m_window->installEventFilter( this );

		auto *vbox = new QVBoxLayout( m_window );
		vbox->setContentsMargins( 0, 0, 0, 0 );

		m_textView = new QPlainTextEdit_Shader;
		vbox->addWidget( m_textView );

		auto *hbox = new QHBoxLayout;
		vbox->addLayout( hbox );
		hbox->setContentsMargins( 4, 0, 4, 4 );

		m_button = new QPushButton( "Save" );
		m_button->setSizePolicy( QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed );
		hbox->addWidget( m_button, Qt::AlignmentFlag::AlignRight );

		QObject::connect( m_textView->document(), &QTextDocument::modificationChanged, [this]( bool modified ){
			m_button->setEnabled( modified );

			m_window->setWindowTitle( StringStream( ( modified? "*" : "" ), m_filename ).c_str() );
		} );

		QObject::connect( m_button, &QAbstractButton::clicked, [this](){ editor_save(); } );

		{
			QLabel *label = new QLabel;
			// label->setOpenExternalLinks( true );
			hbox->addWidget( label );
			QObject::connect( label, &QLabel::linkActivated, []( const QString& link ){
#ifdef WIN32
				// win prohibits opening html with #fragment param for security reasons, so workaround
				const QString filename = QString( SettingsPath_get() ) + "urlopener.html";
				if( QFile file( filename ); file.open( QIODevice::WriteOnly | QIODevice::Text ) ){
					QTextStream out( &file );
					out << "<html>"
							"<meta http-equiv=Refresh content=\"0; url=" << link << "\"><body></body>"
						"</html>";
					file.close();
					QDesktopServices::openUrl( QUrl::fromUserInput( filename ) );
				}
#else
				QDesktopServices::openUrl( QUrl::fromUserInput( link ) );
#endif
			} );

			const auto cb = [this, label](){
				if( const auto *data = m_textView->textCursor().block().userData() ){
					if( const auto *shaderFormat = static_cast<const BlockData*>( data )->shaderFormat ){
						QString page( shaderFormat->page );
						if( page.back() == '#' ){ // no explicit id, id = 1st word
							const QRegularExpression regex( "\\w+" );
							const QString id = regex.match( shaderFormat->key ).captured();
							page += id;
						}
						else if( page.back() == '$' ){ // no explicit id, id = one of values
							page.back() = '#';
							const QString txt = m_textView->textCursor().block().text();
							for( const auto value : shaderFormat->values ){
								if( txt.contains( QRegularExpression( QString( "\\b" ) + value + "\\b" ) ) ){
									page += value;
									break;
								}
							}
						}
						label->setText( QString( "<a href='file:///" ) + AppPath_get() + "docs/shaderManual/"
							+ page + "'>"
							+ page + "</a>" );
						return;
					}
				}
				label->clear();
			};
			QObject::connect( m_textView, &QPlainTextEdit::cursorPositionChanged, cb );
			QObject::connect( m_textView, &QPlainTextEdit::textChanged, cb );
		}

		auto *search = new QLineEdit_search( *m_textView );
		hbox->addWidget( search );
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
	// returns true, if document modifications got saved or user decided to discard them
	bool ensure_saved(){
		if( m_textView->document()->isModified() ) {
			const auto ret = qt_MessageBox( m_window, "Document has been modified.\nSave it?", "Save", EMessageBoxType::Question,
				EMessageBoxReturn::eIDYES | EMessageBoxReturn::eIDNO | EMessageBoxReturn::eIDCANCEL );
			if( ret == EMessageBoxReturn::eIDYES ){
				editor_save();
			}
			if( ret == EMessageBoxReturn::eIDNO ){ // discard changes
				m_textView->clear(); // unset isModified flag this way to avoid messagebox on next opening
			}
			else if( ret == EMessageBoxReturn::eIDCANCEL ){
				return false;
			}
		}
		return true;
	}
public:
	void DoGtkTextEditor( const char* text, const char* shaderName, const char* filename, const bool editable ){
		if ( !m_window ) {
			construct(); // build it the first time we need it
		}

		if( !ensure_saved() )
			return;

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
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::Close ) {
			if( !ensure_saved() ){ // keep editor opened
				event->ignore();
				return true;
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};

static TextEditor g_textEditor;

CopiedString g_TextEditor_editorCommand;

#include "ifilesystem.h"
#include "iarchive.h"
#include "idatastream.h"

void DoShaderView( const char *shaderFileName, const char *shaderName, bool external_editor ){
	const char* pathRoot = GlobalFileSystem().findFile( shaderFileName );
	const bool pathEmpty = string_empty( pathRoot );
	const bool pathIsDir = !pathEmpty && file_is_directory( pathRoot );

	const auto pathFull = StringStream( pathRoot, ( pathIsDir? "" : "::" ), shaderFileName );

	if( pathEmpty ){
		globalErrorStream() << "Failed to load shader file " << shaderFileName << '\n';
	}
	else if( external_editor && pathIsDir ){
		if( g_TextEditor_editorCommand.empty() ){
#ifdef WIN32
			ShellExecute( (HWND)MainFrame_getWindow()->effectiveWinId(), 0, pathFull.c_str(), 0, 0, SW_SHOWNORMAL );
#else
			globalWarningStream() << "Failed to open '" << pathFull << "'\nSet Shader Editor Command in preferences\n";
#endif
		}
		else{
			auto command = StringStream( g_TextEditor_editorCommand, ' ', makeQuoted( pathFull ) );
			globalOutputStream() << "Launching: " << command << '\n';
			// note: linux does not return false if the command failed so it will assume success
			if ( !Q_Exec( 0, command.c_str(), 0, true, false ) )
				globalErrorStream() << "Failed to execute " << command << '\n';
		}
	}
	else if( ArchiveFile* file = GlobalFileSystem().openFile( shaderFileName ) ){
		const std::size_t size = file->size();
		char* text = ( char* )malloc( size + 1 );
		file->getInputStream().read( ( InputStream::byte_type* )text, size );
		text[size] = 0;
		file->release();

		g_textEditor.DoGtkTextEditor( text, shaderName, pathFull, pathIsDir );
		free( text );
	}
}

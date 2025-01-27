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

//
// User preferences
//
// Leonardo Zide (leo@lokigames.com)
//

#include "preferences.h"
#include "environment.h"

#include "debugging/debugging.h"

#include "generic/callback.h"
#include "math/vector.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "os/file.h"
#include "os/path.h"
#include "os/dir.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "commandlib.h"

#include "error.h"
#include "console.h"
#include "xywindow.h"
#include "mainframe.h"
#include "qe3.h"
#include "gtkdlgs.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QGroupBox>
#include <QCheckBox>
#include <QHeaderView>


void Global_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "Console", "Enable Logging", g_Console_enableLogging );
}

void Interface_constructPreferences( PreferencesPage& page ){
	page.appendPathEntry( "Shader Editor Command", g_TextEditor_editorCommand, false );
}

/*!
   =========================================================
   Games selection dialog
   =========================================================
 */

inline const char* xmlAttr_getName( xmlAttrPtr attr ){
	return reinterpret_cast<const char*>( attr->name );
}

inline const char* xmlAttr_getValue( xmlAttrPtr attr ){
	return reinterpret_cast<const char*>( attr->children->content );
}

CGameDescription::CGameDescription( xmlDocPtr pDoc, const CopiedString& gameFile ){
	// read the user-friendly game name
	xmlNodePtr pNode = pDoc->children;

	while ( pNode != 0 && strcmp( (const char*)pNode->name, "game" ) )
	{
		pNode = pNode->next;
	}
	if ( !pNode ) {
		Error( "Didn't find 'game' node in the game description file '%s'\n", pDoc->URL );
	}

	for ( xmlAttrPtr attr = pNode->properties; attr != 0; attr = attr->next )
	{
		m_gameDescription.insert( GameDescription::value_type( xmlAttr_getName( attr ), xmlAttr_getValue( attr ) ) );
	}

	mGameToolsPath = StringStream( AppPath_get(), "gamepacks/", gameFile, '/' );

	ASSERT_MESSAGE( file_exists( mGameToolsPath.c_str() ), "game directory not found: " << makeQuoted( mGameToolsPath ) );

	mGameFile = gameFile;

	{
		GameDescription::iterator i = m_gameDescription.find( "type" );
		if ( i == m_gameDescription.end() ) {
			globalWarningStream() << "Warning, 'type' attribute not found in '" << reinterpret_cast<const char*>( pDoc->URL ) << "'\n";
			// default
			mGameType = "q3";
		}
		else
		{
			mGameType = ( *i ).second.c_str();
		}
	}
}

void CGameDescription::Dump(){
	globalOutputStream() << "game description file: " << makeQuoted( mGameFile ) << '\n';
	for ( GameDescription::iterator i = m_gameDescription.begin(); i != m_gameDescription.end(); ++i )
	{
		globalOutputStream() << ( *i ).first << " = " << makeQuoted( ( *i ).second ) << '\n';
	}
}

CGameDescription *g_pGameDescription;


#include "stream/textfilestream.h"
#include "container/array.h"
#include "xml/ixml.h"
#include "xml/xmlparser.h"
#include "xml/xmlwriter.h"

#include "preferencedictionary.h"
#include "stringio.h"

const char* const PREFERENCES_VERSION = "1.0";

bool Preferences_Load( PreferenceDictionary& preferences, const char* filename, const char *cmdline_prefix ){
	bool ret = false;
	TextFileInputStream file( filename );
	if ( !file.failed() ) {
		XMLStreamParser parser( file );
		XMLPreferenceDictionaryImporter importer( preferences, PREFERENCES_VERSION );
		parser.exportXML( importer );
		ret = true;
	}

	int l = strlen( cmdline_prefix );
	for ( int i = 1; i < g_argc - 1; ++i )
	{
		if ( g_argv[i][0] == '-' ) {
			if ( !strncmp( g_argv[i] + 1, cmdline_prefix, l ) ) {
				if ( g_argv[i][l + 1] == '-' ) {
					preferences.importPref( g_argv[i] + l + 2, g_argv[i + 1] );
				}
			}
			++i;
		}
	}

	return ret;
}

bool Preferences_Save( PreferenceDictionary& preferences, const char* filename ){
	TextFileOutputStream file( filename );
	if ( !file.failed() ) {
		XMLStreamWriter writer( file );
		XMLPreferenceDictionaryExporter exporter( preferences, PREFERENCES_VERSION );
		exporter.exportXML( writer );
		return true;
	}
	return false;
}

bool Preferences_Save_Safe( PreferenceDictionary& preferences, const char* filename ){
	const auto tmpName = StringStream( filename, "TMP" );
	return Preferences_Save( preferences, tmpName ) && file_move( tmpName, filename );
}


void LogConsole_importString( const char* string ){
	g_Console_enableLogging = string_equal( string, "true" );
	Sys_LogFile( g_Console_enableLogging );
}
typedef FreeCaller<void(const char*), LogConsole_importString> LogConsoleImportStringCaller;


void RegisterGlobalPreferences( PreferenceSystem& preferences ){
	preferences.registerPreference( "gamefile", makeCopiedStringStringImportCallback( LatchedAssignCaller( g_GamesDialog.m_sGameFile ) ), CopiedStringExportStringCaller( g_GamesDialog.m_sGameFile.m_latched ) );
	preferences.registerPreference( "gamePrompt", BoolImportStringCaller( g_GamesDialog.m_bGamePrompt ), BoolExportStringCaller( g_GamesDialog.m_bGamePrompt ) );
	preferences.registerPreference( "log console", LogConsoleImportStringCaller(), BoolExportStringCaller( g_Console_enableLogging ) );
}


PreferenceDictionary g_global_preferences;

void GlobalPreferences_Init(){
	RegisterGlobalPreferences( g_global_preferences );
}

void CGameDialog::LoadPrefs(){
	// load global .pref file
	const auto strGlobalPref = StringStream( g_Preferences.m_global_rc_path, "global.pref" );

	globalOutputStream() << "loading global preferences from " << makeQuoted( strGlobalPref ) << '\n';

	if ( !Preferences_Load( g_global_preferences, strGlobalPref, "global" ) ) {
		globalOutputStream() << "failed to load global preferences from " << strGlobalPref << '\n';
	}
}

void CGameDialog::SavePrefs(){
	const auto strGlobalPref = StringStream( g_Preferences.m_global_rc_path, "global.pref" );

	globalOutputStream() << "saving global preferences to " << strGlobalPref << '\n';

	if ( !Preferences_Save_Safe( g_global_preferences, strGlobalPref ) ) {
		globalOutputStream() << "failed to save global preferences to " << strGlobalPref << '\n';
	}
}

void CGameDialog::DoGameDialog(){
	// show the UI
	DoModal();

	// we save the prefs file
	SavePrefs();
}

CGameDescription* CGameDialog::GameDescriptionForComboItem(){
	return ( m_nComboSelect >= 0 && m_nComboSelect < static_cast<int>( mGames.size() ) )?
	       *std::next( mGames.begin(), m_nComboSelect )
	       : 0; // not found
}

void CGameDialog::GameFileAssign( int value ){
	m_nComboSelect = value;
	// use value to set m_sGameFile
	if( CGameDescription* iGame = GameDescriptionForComboItem() )
		m_sGameFile.assign( iGame->mGameFile );
}

void CGameDialog::GameFileImport( int value ){
	m_nComboSelect = value;
	// use value to set m_sGameFile
	if( CGameDescription* iGame = GameDescriptionForComboItem() )
		m_sGameFile.import( iGame->mGameFile );
}

void CGameDialog::GameFileExport( const IntImportCallback& importCallback ) const {
	// use m_sGameFile to set value
	std::list<CGameDescription *>::const_iterator iGame;
	int i = 0;
	for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame )
	{
		if ( ( *iGame )->mGameFile == m_sGameFile.m_latched ) {
			m_nComboSelect = i;
			break;
		}
		i++;
	}
	importCallback( m_nComboSelect );
}

void CGameDialog::CreateGlobalFrame( PreferencesPage& page, bool global ){
	std::vector<const char*> games;
	games.reserve( mGames.size() );
	for ( std::list<CGameDescription *>::iterator i = mGames.begin(); i != mGames.end(); ++i )
	{
		games.push_back( ( *i )->getRequiredKeyValue( "name" ) );
	}
	page.appendCombo(
	    "Select the game",
	    StringArrayRange( games ),
	    global?
	    IntImportCallback( MemberCaller<CGameDialog, void(int), &CGameDialog::GameFileAssign>( *this ) ):
	    IntImportCallback( MemberCaller<CGameDialog, void(int), &CGameDialog::GameFileImport>( *this ) ),
	    ConstMemberCaller<CGameDialog, void(const IntImportCallback&), &CGameDialog::GameFileExport>( *this )
	);
	page.appendCheckBox( "Startup", "Show Global Preferences", m_bGamePrompt );
}

void CGameDialog::BuildDialog(){
	GetWidget()->setWindowTitle( "Global Preferences" );

	auto vbox = new QVBoxLayout( GetWidget() );
	vbox->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
	{
		auto frame = new QGroupBox( "Game settings" );
		vbox->addWidget( frame );

		auto grid = new QGridLayout( frame );
		grid->setAlignment( Qt::AlignmentFlag::AlignTop );
		grid->setColumnStretch( 0, 111 );
		grid->setColumnStretch( 1, 333 );
		{
			PreferencesPage preferencesPage( *this, grid );
			Global_constructPreferences( preferencesPage );
			CreateGlobalFrame( preferencesPage, true );
		}
	}
	{
		auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok );
		vbox->addWidget( buttons );
		QObject::connect( buttons, &QDialogButtonBox::accepted, GetWidget(), &QDialog::accept );
	}
}

void CGameDialog::ScanForGames(){
	const auto path = StringStream( AppPath_get(), "gamepacks/games/" );

	globalOutputStream() << "Scanning for game description files: " << path << '\n';

	/*!
	   \todo FIXME LINUX:
	   do we put game description files below AppPath, or in ~/.radiant
	   i.e. read only or read/write?
	   my guess .. readonly cause it's an install
	   we will probably want to add ~/.radiant/<version>/games/ scanning on top of that for developers
	   (if that's really needed)
	 */

	Directory_forEach( path, matchFileExtension( "game", [&]( const char *name ){
		const auto strPath = StringStream( path, name );
		globalOutputStream() << strPath << '\n';

		xmlDocPtr pDoc = xmlParseFile( strPath );
		if ( pDoc ) {
			mGames.push_back( new CGameDescription( pDoc, name ) );
			xmlFreeDoc( pDoc );
		}
		else
		{
			globalErrorStream() << "XML parser failed on '" << strPath << "'\n";
		}
	}));
}

void CGameDialog::InitGlobalPrefPath(){
	g_Preferences.m_global_rc_path = SettingsPath_get();
}

void CGameDialog::Reset(){
	if ( g_Preferences.m_global_rc_path.empty() ) {
		InitGlobalPrefPath();
	}

	file_remove( StringStream( g_Preferences.m_global_rc_path, "global.pref" ) );
}

void CGameDialog::Init(){
	InitGlobalPrefPath();
	LoadPrefs();
	ScanForGames();
	if ( mGames.empty() ) {
		Error( "Didn't find any valid game file descriptions, aborting\n" );
	}
	else
	{
		mGames.sort( []( const CGameDescription* one, const CGameDescription* another ){
			return one->mGameFile < another->mGameFile;
		} );
	}

	CGameDescription* currentGameDescription = 0;

	if ( !m_bGamePrompt ) {
		// search by .game name
		std::list<CGameDescription *>::iterator iGame;
		for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame )
		{
			if ( ( *iGame )->mGameFile == m_sGameFile.m_value ) {
				currentGameDescription = ( *iGame );
				break;
			}
		}
	}
	if ( !currentGameDescription ) {
		Create( nullptr );
		DoGameDialog();
		// use m_nComboSelect to identify the game to run as and set the globals
		currentGameDescription = GameDescriptionForComboItem();
		ASSERT_NOTNULL( currentGameDescription );
	}
	g_pGameDescription = currentGameDescription;

	g_pGameDescription->Dump();
}

CGameDialog::~CGameDialog(){
	// free all the game descriptions
	std::list<CGameDescription *>::iterator iGame;
	for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame )
	{
		delete ( *iGame );
		*iGame = 0;
	}
	if ( GetWidget() != 0 ) {
		Destroy();
	}
}

inline const char* GameDescription_getIdentifier( const CGameDescription& gameDescription ){
	const char* identifier = gameDescription.getKeyValue( "index" );
	if ( string_empty( identifier ) ) {
		identifier = "1";
	}
	return identifier;
}

void CGameDialog::AddPacksURL( StringOutputStream &URL ){
	// add the URLs for the list of game packs installed
	// FIXME: this is kinda hardcoded for now..
	for ( const CGameDescription *iGame : mGames )
	{
		URL << "&Games_dlup%5B%5D=" << GameDescription_getIdentifier( *iGame );
	}
}

CGameDialog g_GamesDialog;


// =============================================================================
// Widget callbacks for PrefsDlg

static void OnButtonClean( PrefsDlg *dlg ){
	// make sure this is what the user wants
	if ( qt_MessageBox( g_Preferences.GetWidget(),
	                     "This will close Radiant and clean the corresponding registry entries.\n"
	                     "Next time you start Radiant it will be good as new. Do you wish to continue?",
	                     "Reset Registry", EMessageBoxType::Warning, eIDYES | eIDNO ) == eIDYES ) {
		dlg->EndModal( QDialog::DialogCode::Rejected );

		g_preferences_globals.disable_ini = true;
		Preferences_Reset();
		QCoreApplication::quit();
	}
}

// =============================================================================
// PrefsDlg class

/*
   ========

   very first prefs init deals with selecting the game and the game tools path
   then we can load .ini stuff

   using prefs / ini settings:
   those are per-game

   look in ~/.radiant/<version>/gamename
   ========
 */

#define PREFS_LOCAL_FILENAME "local.pref"

void PrefsDlg::Init(){
	// m_global_rc_path has been set above
	// m_rc_path is for game specific preferences
	// takes the form: global-pref-path/gamename/prefs-file

	// this is common to win32 and Linux init now
	// game sub-dir
	m_rc_path = StringStream( m_global_rc_path, g_pGameDescription->mGameFile.c_str(), '/' );
	Q_mkdir( m_rc_path.c_str() );

	// then the ini file
	m_inipath = StringStream( m_rc_path, PREFS_LOCAL_FILENAME );
}


typedef std::list<PreferenceGroupCallback> PreferenceGroupCallbacks;

inline void PreferenceGroupCallbacks_constructGroup( const PreferenceGroupCallbacks& callbacks, PreferenceGroup& group ){
	for ( PreferenceGroupCallbacks::const_iterator i = callbacks.begin(); i != callbacks.end(); ++i )
	{
		( *i )( group );
	}
}


inline void PreferenceGroupCallbacks_pushBack( PreferenceGroupCallbacks& callbacks, const PreferenceGroupCallback& callback ){
	callbacks.push_back( callback );
}

typedef std::list<PreferencesPageCallback> PreferencesPageCallbacks;

inline void PreferencesPageCallbacks_constructPage( const PreferencesPageCallbacks& callbacks, PreferencesPage& page ){
	for ( PreferencesPageCallbacks::const_iterator i = callbacks.begin(); i != callbacks.end(); ++i )
	{
		( *i )( page );
	}
}

inline void PreferencesPageCallbacks_pushBack( PreferencesPageCallbacks& callbacks, const PreferencesPageCallback& callback ){
	callbacks.push_back( callback );
}

PreferencesPageCallbacks g_gamePreferences;
void PreferencesDialog_addGamePreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_gamePreferences, callback );
}
PreferenceGroupCallbacks g_gameCallbacks;
void PreferencesDialog_addGamePage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_gameCallbacks, callback );
}

PreferencesPageCallbacks g_interfacePreferences;
void PreferencesDialog_addInterfacePreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_interfacePreferences, callback );
}
PreferenceGroupCallbacks g_interfaceCallbacks;
void PreferencesDialog_addInterfacePage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_interfaceCallbacks, callback );
}

PreferencesPageCallbacks g_displayPreferences;
void PreferencesDialog_addDisplayPreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_displayPreferences, callback );
}
PreferenceGroupCallbacks g_displayCallbacks;
void PreferencesDialog_addDisplayPage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_displayCallbacks, callback );
}

PreferencesPageCallbacks g_settingsPreferences;
void PreferencesDialog_addSettingsPreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_settingsPreferences, callback );
}
PreferenceGroupCallbacks g_settingsCallbacks;
void PreferencesDialog_addSettingsPage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_settingsCallbacks, callback );
}

void Widget_connectToggleDependency( QWidget* self, QCheckBox* toggleButton ){
	class EnabledTracker : public QObject
	{
		QCheckBox *const m_checkbox;
		QWidget *const m_dependent;
	public:
		EnabledTracker( QCheckBox *checkbox, QWidget *dependent ) : QObject( checkbox ), m_checkbox( checkbox ), m_dependent( dependent ){
			m_checkbox->installEventFilter( this );
		}
	protected:
		bool eventFilter( QObject *obj, QEvent *event ) override {
			if( event->type() == QEvent::EnabledChange ) {
				m_dependent->setEnabled( m_checkbox->checkState() && m_checkbox->isEnabled() );
			}
			return QObject::eventFilter( obj, event ); // standard event processing
		}
	};
	new EnabledTracker( toggleButton, self ); // track graying out for chained dependencies
	QObject::connect( toggleButton, &QCheckBox::stateChanged, [self, toggleButton]( int state ){ // track being checked
		self->setEnabled( state && toggleButton->isEnabled() );
	} );
	self->setEnabled( toggleButton->checkState() && toggleButton->isEnabled() ); // apply dependency effect right away
}
void Widget_connectToggleDependency( QCheckBox* self, QCheckBox* toggleButton ){
	Widget_connectToggleDependency( static_cast<QWidget*>( self ), toggleButton );
}


QStandardItem* PreferenceTree_appendPage( QStandardItemModel* model, QStandardItem* parent, const char* name, int pageIndex ){
	auto item = new QStandardItem( name );
	item->setData( pageIndex, Qt::ItemDataRole::UserRole );
	parent->appendRow( item );
	return item;
}

auto PreferencePages_addPage( QStackedWidget* notebook, const char* name ){
	auto frame = new QGroupBox( name );
	auto grid = new QGridLayout( frame );
	grid->setAlignment( Qt::AlignmentFlag::AlignTop );
	grid->setColumnStretch( 0, 111 );
	grid->setColumnStretch( 1, 333 );
	return std::pair( notebook->addWidget( frame ), grid );
}

class PreferenceTreeGroup : public PreferenceGroup
{
	Dialog& m_dialog;
	QStackedWidget* m_notebook;
	QStandardItemModel* m_model;
	QStandardItem *m_group;
public:
	PreferenceTreeGroup( Dialog& dialog, QStackedWidget* notebook, QStandardItemModel* model, QStandardItem *group ) :
		m_dialog( dialog ),
		m_notebook( notebook ),
		m_model( model ),
		m_group( group ){
	}
	PreferencesPage createPage( const char* treeName, const char* frameName ) override {
		const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, frameName );
		PreferenceTree_appendPage( m_model, m_group, treeName, pageIndex );
		return PreferencesPage( m_dialog, layout );
	}
};

void PrefsDlg::BuildDialog(){
	PreferencesDialog_addInterfacePreferences( makeCallbackF( Interface_constructPreferences ) );

	GetWidget()->setWindowTitle( "NetRadiant Preferences" );

	{
		auto grid = new QGridLayout( GetWidget() );
		grid->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			grid->addWidget( buttons, 1, 1 );
			QObject::connect( buttons, &QDialogButtonBox::accepted, GetWidget(), &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, GetWidget(), &QDialog::reject );
			QObject::connect( buttons->addButton( "Clean", QDialogButtonBox::ButtonRole::ResetRole ), &QPushButton::clicked, [this](){ OnButtonClean( this ); } );
		}

		{
			{
				// prefs pages notebook
				m_notebook = new QStackedWidget;
				grid->addWidget( m_notebook, 0, 1 );
				{
					m_treeview = new QTreeView;
					m_treeview->setHeaderHidden( true );
					m_treeview->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
					m_treeview->setUniformRowHeights( true ); // optimization
					m_treeview->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
					m_treeview->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
					m_treeview->setSizePolicy( QSizePolicy::Policy::Fixed, m_treeview->sizePolicy().verticalPolicy() );
					m_treeview->header()->setStretchLastSection( false ); // non greedy column sizing; + QHeaderView::ResizeMode::ResizeToContents = no text elision 🤷‍♀️
					m_treeview->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );
					grid->addWidget( m_treeview, 0, 0, 2, 1 );

					// store display name in column #0 and page index in data( Qt::ItemDataRole::UserRole )
					auto model = new QStandardItemModel( m_treeview );
					m_treeview->setModel( model );

					QObject::connect( m_treeview->selectionModel(), &QItemSelectionModel::currentChanged, [this]( const QModelIndex& current ){
						m_notebook->setCurrentIndex( current.data( Qt::ItemDataRole::UserRole ).toInt() );
					} );

					{
						/********************************************************************/
						/* Add preference tree options                                      */
						/********************************************************************/
						{
							const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, "Global Preferences" );
							{
								PreferencesPage preferencesPage( *this, layout );
								Global_constructPreferences( preferencesPage );
							}
							QStandardItem *group = PreferenceTree_appendPage( model, model->invisibleRootItem(), "Global", pageIndex );
							{
								const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, "Game" );
								PreferencesPage preferencesPage( *this, layout );
								g_GamesDialog.CreateGlobalFrame( preferencesPage, false );

								PreferenceTree_appendPage( model, group, "Game", pageIndex );
							}
						}

						{
							const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, "Game Settings" );
							{
								PreferencesPage preferencesPage( *this, layout );
								Game_constructPreferences( preferencesPage );
								PreferencesPageCallbacks_constructPage( g_gamePreferences, preferencesPage );
							}

							QStandardItem *group = PreferenceTree_appendPage( model, model->invisibleRootItem(), "Game", pageIndex );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, model, group );

							PreferenceGroupCallbacks_constructGroup( g_gameCallbacks, preferenceGroup );
						}

						{
							const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, "Interface Preferences" );
							{
								PreferencesPage preferencesPage( *this, layout );
								PreferencesPageCallbacks_constructPage( g_interfacePreferences, preferencesPage );
							}

							QStandardItem *group = PreferenceTree_appendPage( model, model->invisibleRootItem(), "Interface", pageIndex );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, model, group );

							PreferenceGroupCallbacks_constructGroup( g_interfaceCallbacks, preferenceGroup );
						}

						{
							const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, "Display Preferences" );
							{
								PreferencesPage preferencesPage( *this, layout );
								PreferencesPageCallbacks_constructPage( g_displayPreferences, preferencesPage );
							}
							QStandardItem *group = PreferenceTree_appendPage( model, model->invisibleRootItem(), "Display", pageIndex );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, model, group );

							PreferenceGroupCallbacks_constructGroup( g_displayCallbacks, preferenceGroup );
						}

						{
							const auto [ pageIndex, layout ] = PreferencePages_addPage( m_notebook, "General Settings" );
							{
								PreferencesPage preferencesPage( *this, layout );
								PreferencesPageCallbacks_constructPage( g_settingsPreferences, preferencesPage );
							}

							QStandardItem *group = PreferenceTree_appendPage( model, model->invisibleRootItem(), "Settings", pageIndex );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, model, group );

							PreferenceGroupCallbacks_constructGroup( g_settingsCallbacks, preferenceGroup );
						}
					}
					// convenience calls
					m_treeview->expandAll();
					m_treeview->setCurrentIndex( m_treeview->model()->index( 0, 0 ) );
				}
			}
		}
	}
}

preferences_globals_t g_preferences_globals;

PrefsDlg g_Preferences;               // global prefs instance


void PreferencesDialog_constructWindow( QWidget* main_window ){
	g_Preferences.Create( main_window );
}
void PreferencesDialog_destroyWindow(){
	g_Preferences.Destroy();
}


PreferenceDictionary g_preferences;

PreferenceSystem& GetPreferenceSystem(){
	return g_preferences;
}

class PreferenceSystemAPI
{
	PreferenceSystem* m_preferencesystem;
public:
	typedef PreferenceSystem Type;
	STRING_CONSTANT( Name, "*" );

	PreferenceSystemAPI(){
		m_preferencesystem = &GetPreferenceSystem();
	}
	PreferenceSystem* getTable(){
		return m_preferencesystem;
	}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<PreferenceSystemAPI> PreferenceSystemModule;
typedef Static<PreferenceSystemModule> StaticPreferenceSystemModule;
StaticRegisterModule staticRegisterPreferenceSystem( StaticPreferenceSystemModule::instance() );

void Preferences_Load(){
	g_GamesDialog.LoadPrefs();

	globalOutputStream() << "loading local preferences from " << g_Preferences.m_inipath << '\n';

	if ( !Preferences_Load( g_preferences, g_Preferences.m_inipath.c_str(), g_GamesDialog.m_sGameFile.m_value.c_str() ) ) {
		globalWarningStream() << "failed to load local preferences from " << g_Preferences.m_inipath << '\n';
	}
}

void Preferences_Save(){
	if ( g_preferences_globals.disable_ini ) {
		return;
	}

	g_GamesDialog.SavePrefs();

	globalOutputStream() << "saving local preferences to " << g_Preferences.m_inipath << '\n';

	if ( !Preferences_Save_Safe( g_preferences, g_Preferences.m_inipath.c_str() ) ) {
		globalWarningStream() << "failed to save local preferences to " << g_Preferences.m_inipath << '\n';
	}
}

void Preferences_Reset(){
	file_remove( g_Preferences.m_inipath.c_str() );
}


void PrefsDlg::PostModal( QDialog::DialogCode code ){
	if ( code == QDialog::DialogCode::Accepted ) {
		Preferences_Save();
		UpdateAllWindows();
	}
}

std::vector<const char*> g_restart_required;

void PreferencesDialog_restartRequired( const char* staticName ){
	g_restart_required.push_back( staticName );
}

void PreferencesDialog_showDialog(){
	//if ( ConfirmModified( "Edit Preferences" ) && g_Preferences.DoModal() == eIDOK ) {
	g_Preferences.m_treeview->setFocus(); // focus tree to have it immediately available for text search
	if ( g_Preferences.DoModal() == QDialog::DialogCode::Accepted ) {
		if ( !g_restart_required.empty() ) {
			auto message = StringStream( "Preference changes require a restart:\n\n" );
			for ( const auto i : g_restart_required )
				message << i << '\n';
			g_restart_required.clear();
			message << "\nRestart now?";

			if( qt_MessageBox( MainFrame_getWindow(), message, "Restart is required", EMessageBoxType::Question ) == eIDYES )
				Radiant_Restart();
		}
	}
}





void GameName_importString( const char* value ){
	gamename_set( value );
}
typedef FreeCaller<void(const char*), GameName_importString> GameNameImportStringCaller;
void GameName_exportString( const StringImportCallback& importer ){
	importer( gamename_get() );
}
typedef FreeCaller<void(const StringImportCallback&), GameName_exportString> GameNameExportStringCaller;

void GameMode_importString( const char* value ){
	gamemode_set( value );
}
typedef FreeCaller<void(const char*), GameMode_importString> GameModeImportStringCaller;
void GameMode_exportString( const StringImportCallback& importer ){
	importer( gamemode_get() );
}
typedef FreeCaller<void(const StringImportCallback&), GameMode_exportString> GameModeExportStringCaller;


void RegisterPreferences( PreferenceSystem& preferences ){
	preferences.registerPreference( "CustomShaderEditorCommand", CopiedStringImportStringCaller( g_TextEditor_editorCommand ), CopiedStringExportStringCaller( g_TextEditor_editorCommand ) );

	preferences.registerPreference( "GameName", GameNameImportStringCaller(), GameNameExportStringCaller() );
	preferences.registerPreference( "GameMode", GameModeImportStringCaller(), GameModeExportStringCaller() );
}

void Preferences_Init(){
	RegisterPreferences( GetPreferenceSystem() );
}

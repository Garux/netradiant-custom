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

/*
   The following source code is licensed by Id Software and subject to the terms of
   its LIMITED USE SOFTWARE LICENSE AGREEMENT, a copy of which is included with
   GtkRadiant. If you did not receive a LIMITED USE SOFTWARE LICENSE AGREEMENT,
   please contact Id Software immediately at info@idsoftware.com.
 */

#pragma once

#include <libxml/parser.h>
#include "stream/textstream.h"
#include "debugging/debugging.h"
#include "dialog.h"
#include <list>
#include <map>

void Widget_connectToggleDependency( QWidget* self, QCheckBox* toggleButton );
void Widget_connectToggleDependency( QCheckBox* self, QCheckBox* toggleButton );

class PreferencesPage
{
	Dialog& m_dialog;
	class QGridLayout* m_grid;
public:
	PreferencesPage( Dialog& dialog, QGridLayout* grid ) : m_dialog( dialog ), m_grid( grid ){
	}
	QCheckBox* appendCheckBox( const char* name, const char* flag, bool& data ){
		return m_dialog.addCheckBox( m_grid, name, flag, data );
	}
	QCheckBox* appendCheckBox( const char* name, const char* flag, const BoolImportCallback& importCallback, const BoolExportCallback& exportCallback ){
		return m_dialog.addCheckBox( m_grid, name, flag, importCallback, exportCallback );
	}
	QComboBox* appendCombo( const char* name, StringArrayRange values, const IntImportCallback& importCallback, const IntExportCallback& exportCallback ){
		return m_dialog.addCombo( m_grid, name, values, importCallback, exportCallback );
	}
	QComboBox* appendCombo( const char* name, int& data, StringArrayRange values ){
		return m_dialog.addCombo( m_grid, name, data, values );
	}
	void appendSlider( const char* name, int& data, int lower, int upper, int step_increment, int page_increment ){
		m_dialog.addSlider( m_grid, name, data, lower, upper, step_increment, page_increment );
	}
	void appendSlider( const char* name, float& data, double lower, double upper, double step_increment, double page_increment ){
		m_dialog.addSlider( m_grid, name, data, lower, upper, step_increment, page_increment );
	}
	void appendRadio( const char* name, StringArrayRange names, const IntImportCallback& importCallback, const IntExportCallback& exportCallback ){
		m_dialog.addRadio( m_grid, name, names, importCallback, exportCallback );
	}
	void appendRadio( const char* name, int& data, StringArrayRange names ){
		m_dialog.addRadio( m_grid, name, data, names );
	}
	void appendRadioIcons( const char* name, StringArrayRange icons, const IntImportCallback& importCallback, const IntExportCallback& exportCallback ){
		m_dialog.addRadioIcons( m_grid, name, icons, importCallback, exportCallback );
	}
	void appendRadioIcons( const char* name, int& data, StringArrayRange icons ){
		m_dialog.addRadioIcons( m_grid, name, data, icons );
	}
	QWidget* appendEntry( const char* name, const StringImportCallback& importCallback, const StringExportCallback& exportCallback ){
		return m_dialog.addTextEntry( m_grid, name, importCallback, exportCallback );
	}
	QWidget* appendEntry( const char* name, CopiedString& data ){
		return m_dialog.addEntry( m_grid, name, data );
	}
	void appendPathEntry( const char* name, bool browse_directory, const StringImportCallback& importCallback, const StringExportCallback& exportCallback ){
		m_dialog.addPathEntry( m_grid, name, browse_directory, importCallback, exportCallback );
	}
	void appendPathEntry( const char* name, CopiedString& data, bool directory ){
		m_dialog.addPathEntry( m_grid, name, data, directory );
	}
	QWidget* appendSpinner( const char* name, int& data, int lower, int upper ){
		return m_dialog.addSpinner( m_grid, name, data, lower, upper );
	}
	QWidget* appendSpinner( const char* name, int lower, int upper, const IntImportCallback& importCallback, const IntExportCallback& exportCallback ){
		return m_dialog.addSpinner( m_grid, name, lower, upper, importCallback, exportCallback );
	}
	QWidget* appendSpinner( const char* name, double lower, double upper, const FloatImportCallback& importCallback, const FloatExportCallback& exportCallback, int decimals = 2 ){
		return m_dialog.addSpinner( m_grid, name, lower, upper, importCallback, exportCallback, decimals );
	}
	QWidget* appendSpinner( const char* name, float& data, double lower, double upper, int decimals = 2 ){
		return m_dialog.addSpinner( m_grid, name, data, lower, upper, decimals );
	}
};

typedef Callback<void(PreferencesPage&)> PreferencesPageCallback;

class PreferenceGroup
{
public:
	virtual PreferencesPage createPage( const char* treeName, const char* frameName ) = 0;
};

typedef Callback<void(PreferenceGroup&)> PreferenceGroupCallback;

void PreferencesDialog_addGamePreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addGamePage( const PreferenceGroupCallback& callback );
void PreferencesDialog_addInterfacePreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addInterfacePage( const PreferenceGroupCallback& callback );
void PreferencesDialog_addDisplayPreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addDisplayPage( const PreferenceGroupCallback& callback );
void PreferencesDialog_addSettingsPreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addSettingsPage( const PreferenceGroupCallback& callback );

void PreferencesDialog_restartRequired( const char* staticName );

template<typename Value>
class LatchedValue
{
public:
	Value m_value;
	Value m_latched;
	const char* m_description;

	LatchedValue( Value value, const char* description ) : m_value( value ), m_latched( value ), m_description( description ){
	}
	void assign( Value value ){ // assign during initialization
		m_value = m_latched = value;
	}
	void import( Value value ){ // import during runtime
		m_latched = value;
		if ( m_latched != m_value ) {
			PreferencesDialog_restartRequired( m_description );
		}
	}
};

typedef LatchedValue<bool> LatchedBool;
typedef LatchedValue<int> LatchedInt;

template<typename T, typename R = MemberCaller<LatchedValue<T>, void(T), &LatchedValue<T>::assign>>
inline R LatchedAssignCaller( LatchedValue<T>& latchedValue ){
	return R( latchedValue );
}

template<typename T, typename R = MemberCaller<LatchedValue<T>, void(T), &LatchedValue<T>::import>>
inline R LatchedImportCaller( LatchedValue<T>& latchedValue ){
	return R( latchedValue );
}

/*!
   holds information for a given game
   I'm a bit unclear on that still
   it holds game specific configuration stuff
   such as base names, engine names, some game specific features to activate in the various modules
   it is not strictly a prefs thing since the user is not supposed to edit that (unless he is hacking
   support for a new game)

   what we do now is fully generate the information for this during the setup. We might want to
   generate a piece that just says "the game pack is there", but put the rest of the config somewhere
   else (i.e. not generated, copied over during setup .. for instance in the game tools directory)
 */
class CGameDescription
{
	typedef std::map<CopiedString, CopiedString> GameDescription;

public:
	CopiedString mGameFile;   ///< the .game file that describes this game
	GameDescription m_gameDescription;

	CopiedString mGameToolsPath;   ///< the explicit path to the game-dependent modules
	CopiedString mGameType;   ///< the type of the engine

	const char* getKeyValue( const char* key ) const {
		GameDescription::const_iterator i = m_gameDescription.find( key );
		if ( i != m_gameDescription.end() ) {
			return ( *i ).second.c_str();
		}
		return "";
	}
	const char* getRequiredKeyValue( const char* key ) const {
		GameDescription::const_iterator i = m_gameDescription.find( key );
		if ( i != m_gameDescription.end() ) {
			return ( *i ).second.c_str();
		}
		ERROR_MESSAGE( "game attribute " << makeQuoted( key ) << " not found in " << makeQuoted( mGameFile ) );
		return "";
	}

	CGameDescription( xmlDocPtr pDoc, const CopiedString &GameFile );

	void Dump();
};

extern CGameDescription *g_pGameDescription;

class StringOutputStream;

/*!
   standalone dialog for games selection, and more generally global settings
 */
class CGameDialog : public Dialog
{
protected:

	mutable int m_nComboSelect;   ///< intermediate int value for combo in dialog box

public:

	/*!
	   those settings are saved in the global prefs file
	   I'm too lazy to wrap behind protected access, not sure this needs to be public
	   NOTE: those are preference settings. if you change them it is likely that you would
	   have to restart the editor for them to take effect
	 */
	/*@{*/
	/*!
	   what game has been selected
	   this is the name of the .game file
	 */
	LatchedValue<CopiedString> m_sGameFile;
	/*!
	   prompt which game to load on startup
	 */
	bool m_bGamePrompt;
	/*!
	   log console to radiant.log
	   m_bForceLogConsole is an obscure forced latching situation
	 */
	bool m_bForceLogConsole;
	/*@}*/

	/*!
	   the list of game descriptions we scanned from the game/ dir
	 */
	std::list<CGameDescription*> mGames;

	CGameDialog() :
		m_sGameFile( "", "Selected Game" ),
		m_bGamePrompt( false ),
		m_bForceLogConsole( false ){
	}
	virtual ~CGameDialog();

	void AddPacksURL( StringOutputStream &s );

	/*!
	   initialize the game dialog, called at CPrefsDlg::Init
	   will scan for games, load prefs, and do game selection dialog if needed
	 */
	void Init();

	/*!
	   reset the global settings by removing the file
	 */
	void Reset();

	/*!
	   run the dialog UI for the list of games
	 */
	void DoGameDialog();

	/*!
	   Dialog API
	   this is only called when the dialog is built at startup for main engine select
	 */
	void BuildDialog() override;

	void GameFileAssign( int value );
	void GameFileImport( int value );
	void GameFileExport( const IntImportCallback& importCallback ) const;

	/*!
	   construction of the dialog frame
	   this is the part to be re-used in prefs dialog
	   for the standalone dialog, we include this in a modal box
	   for prefs, we hook the frame in the main notebook
	   build the frame on-demand (only once)
	 */
	/// \brief \p global controls if to use LatchedValue assignment callback (global settings dialog) or import (preferences dialog).
	void CreateGlobalFrame( PreferencesPage& page, bool global );

	/*!
	   global preferences subsystem
	   XML-based this time, hopefully this will generalize to other prefs
	   LoadPrefs has hardcoded defaults
	   NOTE: it may not be strictly 'CGameDialog' to put the global prefs here
	   could have named the class differently I guess
	 */
	/*@{*/
	void LoadPrefs();   ///< load from file into variables
	void SavePrefs();   ///< save pref variables to file
	/*@}*/

private:
	/*!
	   scan for .game files, load them
	 */
	void ScanForGames();

	/*!
	   inits g_Preferences.m_global_rc_path
	 */
	void InitGlobalPrefPath();

	/*!
	   uses m_nComboItem to find the right mGames
	 */
	CGameDescription *GameDescriptionForComboItem();
};

/*!
   this holds global level preferences
 */
extern CGameDialog g_GamesDialog;


class PrefsDlg : public Dialog
{
public:

	class QStackedWidget *m_notebook;
	class QTreeView *m_treeview;

	virtual ~PrefsDlg(){
	}

	/*!
	   path for global settings
	   win32: AppPath
	   linux: ~/.radiant/[version]/
	 */
	CopiedString m_global_rc_path;

	/*!
	   path to per-game settings
	   used for various game dependant storage
	   win32: GameToolsPath
	   linux: ~/.radiant/[version]/[gamename]/
	 */
	CopiedString m_rc_path;

	/*!
	   holds per-game settings
	   m_rc_path+"local.pref"
	   \todo FIXME at some point this should become XML property bag code too
	 */
	CopiedString m_inipath;

// initialize the above paths
	void Init();

protected:

	/*! Dialog API */
	void BuildDialog() override;
	void PostModal( QDialog::DialogCode code ) override;
};

extern PrefsDlg g_Preferences;

struct preferences_globals_t
{
	// disabled all INI / registry read write .. used when shutting down after registry cleanup
	bool disable_ini = false;
};
extern preferences_globals_t g_preferences_globals;

void PreferencesDialog_constructWindow( QWidget* main_window );
void PreferencesDialog_destroyWindow();

void PreferencesDialog_showDialog();

void GlobalPreferences_Init();
void Preferences_Init();

void Preferences_Load();
void Preferences_Save();

void Preferences_Reset();

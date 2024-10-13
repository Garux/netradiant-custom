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

#include "commands.h"

#include "debugging/debugging.h"

#include <map>
#include "string/string.h"
#include "versionlib.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/messagebox.h"
#include "gtkmisc.h"

struct ShortcutValue{
	QKeySequence accelerator;
	const QKeySequence accelerator_default;
	int type; // 0 = !isRegistered, 1 = command, 2 = toggle
	ShortcutValue( const QKeySequence& a ) : accelerator( a ), accelerator_default( a ), type( 0 ){
	}
};
typedef std::map<CopiedString, ShortcutValue> Shortcuts;

Shortcuts g_shortcuts;

const QKeySequence& GlobalShortcuts_insert( const char* name, const QKeySequence& accelerator ){
	return ( *g_shortcuts.insert( Shortcuts::value_type( name, ShortcutValue( accelerator ) ) ).first ).second.accelerator;
}

template<typename Functor>
void GlobalShortcuts_foreach( Functor& functor ){
	for ( auto& [name, shortcut] : g_shortcuts )
		functor( name.c_str(), shortcut.accelerator );
}

void GlobalShortcuts_register( const char* name, int type ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		( *i ).second.type = type;
	}
}

void GlobalShortcuts_reportUnregistered(){
	for ( const auto& [name, shortcut] : g_shortcuts )
		if ( !shortcut.accelerator.isEmpty() && shortcut.type == 0 )
			globalWarningStream() << "shortcut not registered: " << name << '\n';
}

typedef std::map<CopiedString, Command> Commands;

Commands g_commands;

void GlobalCommands_insert( const char* name, const Callback& callback, const QKeySequence& accelerator ){
	bool added = g_commands.insert( Commands::value_type( name, Command( callback, GlobalShortcuts_insert( name, accelerator ) ) ) ).second;
	ASSERT_MESSAGE( added, "command already registered: " << makeQuoted( name ) );
}

const Command& GlobalCommands_find( const char* command ){
	Commands::iterator i = g_commands.find( command );
	ASSERT_MESSAGE( i != g_commands.end(), "failed to lookup command " << makeQuoted( command ) );
	return ( *i ).second;
}

typedef std::map<CopiedString, Toggle> Toggles;


Toggles g_toggles;

void GlobalToggles_insert( const char* name, const Callback& callback, const BoolExportCallback& exportCallback, const QKeySequence& accelerator ){
	bool added = g_toggles.insert( Toggles::value_type( name, Toggle( callback, GlobalShortcuts_insert( name, accelerator ), exportCallback ) ) ).second;
	ASSERT_MESSAGE( added, "toggle already registered: " << makeQuoted( name ) );
}
const Toggle& GlobalToggles_find( const char* name ){
	Toggles::iterator i = g_toggles.find( name );
	ASSERT_MESSAGE( i != g_toggles.end(), "failed to lookup toggle " << makeQuoted( name ) );
	return ( *i ).second;
}

typedef std::map<CopiedString, KeyEvent> KeyEvents;


KeyEvents g_keyEvents;

void GlobalKeyEvents_insert( const char* name, const Callback& keyDown, const Callback& keyUp, const QKeySequence& accelerator ){
	bool added = g_keyEvents.insert( KeyEvents::value_type( name, KeyEvent( GlobalShortcuts_insert( name, accelerator ), keyDown, keyUp ) ) ).second;
	ASSERT_MESSAGE( added, "command already registered: " << makeQuoted( name ) );
}
const KeyEvent& GlobalKeyEvents_find( const char* name ){
	KeyEvents::iterator i = g_keyEvents.find( name );
	ASSERT_MESSAGE( i != g_keyEvents.end(), "failed to lookup keyEvent " << makeQuoted( name ) );
	return ( *i ).second;
}




#include "mainframe.h"

#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include <QDialog>
#include <QTreeWidget>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QKeySequenceEdit>
#include <QKeyEvent>
#include <QApplication>


void disconnect_accelerator( const char *name ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		switch ( ( *i ).second.type )
		{
		case 1:
			// command
			command_disconnect_accelerator( name );
			break;
		case 2:
			// toggle
			toggle_remove_accelerator( name );
			break;
		}
	}
}

void connect_accelerator( const char *name ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		switch ( ( *i ).second.type )
		{
		case 1:
			// command
			command_connect_accelerator( name );
			break;
		case 2:
			// toggle
			toggle_add_accelerator( name );
			break;
		}
	}
}


inline void accelerator_item_set_icon( QTreeWidgetItem *item, const ShortcutValue& value ){
	value.accelerator != value.accelerator_default
	? item->setIcon( 1, QApplication::style()->standardIcon( QStyle::StandardPixmap::SP_DialogNoButton ) )
	: item->setIcon( 1, {} );
}


void accelerator_clear_button_clicked( QTreeWidgetItem *item ){
	const auto commandName = item->text( 0 ).toLatin1();

	// clear the ACTUAL accelerator too!
	disconnect_accelerator( commandName );

	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find( commandName.constData() );
	if ( thisShortcutIterator != g_shortcuts.end() ) {
		thisShortcutIterator->second.accelerator = {};
		item->setText( 1, {} );
		accelerator_item_set_icon( item, thisShortcutIterator->second );
	}
}


// note: ideally this should also consider some shortcuts being KeyEvent and thus enabled by occasion
// so technically they do not definitely clash with Command/Toggle with the same shortcut
class VerifyAcceleratorNotTaken
{
	const char *commandName;
	const QKeySequence newAccel;
	QTreeWidget *tree;
public:
	bool allow;
	VerifyAcceleratorNotTaken( const char *name, const QKeySequence accelerator, QTreeWidget *tree ) :
		commandName( name ), newAccel( accelerator ), tree( tree ), allow( true ){
	}
	void operator()( const char* name, QKeySequence& accelerator ){
		if ( !allow
		  || !QKeySequence_valid( accelerator )
		  || !strcmp( name, commandName ) ) {
			return;
		}
		if ( accelerator == newAccel ) {
			const auto msg = StringStream( "The command <b>", name, "</b> is already assigned to the key <b>", accelerator, "</b>.<br><br>",
			                               "Do you want to unassign <b>", name, "</b> first?" );
			const EMessageBoxReturn r = qt_MessageBox( tree->window(), msg, "Key already used", EMessageBoxType::Question, eIDYES | eIDNO | eIDCANCEL );
			if ( r == eIDYES ) {
				// clear the ACTUAL accelerator too!
				disconnect_accelerator( name );
				// delete the modifier
				accelerator = {};
				// empty the cell of the key binds dialog
				for( QTreeWidgetItemIterator it( tree ); *it; ++it )
				{
					if( ( *it )->text( 0 ) == name ){
						( *it )->setText( 1, {} );
						Shortcuts::const_iterator thisShortcutIterator = g_shortcuts.find( name );
						if ( thisShortcutIterator != g_shortcuts.end() ) {
							accelerator_item_set_icon( ( *it ), thisShortcutIterator->second );
						}
						break;
					}
				}
			}
			else if ( r == eIDCANCEL ) {
				// aborted
				allow = false;
			}
			// eIDNO : keep duplicate key
		}
	}
};
// multipurpose function: invalid accelerator = reset to default
static void accelerator_alter( QTreeWidgetItem *item, const QKeySequence accelerator ){
	// 7. find the name of the accelerator
	auto commandName = item->text( 0 ).toLatin1();

	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find( commandName.constData() );
	if ( thisShortcutIterator == g_shortcuts.end() ) {
		globalErrorStream() << "commandName " << makeQuoted( commandName.constData() ) << " not found in g_shortcuts.\n";
		return;
	}

	// 8. build an Accelerator
	const QKeySequence newAccel( QKeySequence_valid( accelerator )? accelerator : thisShortcutIterator->second.accelerator_default );
	// note: can skip the rest, if newAccel == current accel
	// 8. verify the key is still free, show a dialog to ask what to do if not
	VerifyAcceleratorNotTaken verify_visitor( commandName, newAccel, item->treeWidget() );
	GlobalShortcuts_foreach( verify_visitor );
	if ( verify_visitor.allow ) {
		// clear the ACTUAL accelerator first
		disconnect_accelerator( commandName );

		thisShortcutIterator->second.accelerator = newAccel;

		// write into the cell
		item->setText( 1, newAccel.toString() );
		accelerator_item_set_icon( item, thisShortcutIterator->second );

		// set the ACTUAL accelerator too!
		connect_accelerator( commandName );
	}
}

void accelerator_reset_all_button_clicked( QTreeWidget *tree ){
	for ( const auto&[name, value] : g_shortcuts ){ // at first disconnect all to avoid conflicts during connecting
		if( value.accelerator != value.accelerator_default ){ // can just do this for all, but it breaks menu accelerator labels :b
			// clear the ACTUAL accelerator
			disconnect_accelerator( name.c_str() );
		}
	}
	for ( auto&[name, value] : g_shortcuts ){
		if( value.accelerator != value.accelerator_default ){
			value.accelerator = value.accelerator_default;
			// set the ACTUAL accelerator
			connect_accelerator( name.c_str() );
		}
	}
	// update tree view
	for( QTreeWidgetItemIterator it( tree ); *it; ++it )
	{
		Shortcuts::const_iterator thisShortcutIterator = g_shortcuts.find( ( *it )->text( 0 ).toLatin1().constData() );
		if ( thisShortcutIterator != g_shortcuts.end() ) {
			// write into the cell
			( *it )->setText( 1, thisShortcutIterator->second.accelerator.toString() );
			accelerator_item_set_icon( ( *it ), thisShortcutIterator->second );
		}
	}
}


class Single_QKeySequenceEdit : public QKeySequenceEdit
{
protected:
	void keyPressEvent( QKeyEvent *e ) override {
		QKeySequenceEdit::keyPressEvent( e );
		if( e->modifiers() & Qt::KeypadModifier ) //. workaround Qt issue: Qt::KeypadModifier is ignored
			setKeySequence( QKeySequence( keySequence()[0] | Qt::KeypadModifier ) );
		if( QKeySequence_valid( keySequence() ) )
			clearFocus(); // trigger editingFinished(); via losing focus 🙉
			              // because this can still receive focus loss b4 getting deleted (practically because modal msgbox)
			              // and two editingFinished(); b no good
	}
	void focusOutEvent( QFocusEvent *event ) override {
		editingFinished();
	}
	bool event( QEvent *event ) override { // comsume ALL key presses including Tab
		if( event->type() == QEvent::KeyPress ){
			keyPressEvent( static_cast<QKeyEvent*>( event ) );
			return true;
		}
		return QKeySequenceEdit::event( event );
	}
};

void accelerator_edit( QTreeWidgetItem *item ){
		auto edit = new Single_QKeySequenceEdit;
		QObject::connect( edit, &QKeySequenceEdit::editingFinished, [item, edit](){
			const QKeySequence accelerator = edit->keySequence();
			item->treeWidget()->setItemWidget( item, 1, nullptr );
			if( QKeySequence_valid( accelerator ) )
				accelerator_alter( item, accelerator );
		} );
		item->treeWidget()->setItemWidget( item, 1, edit );
		edit->setFocus(); // track sanity gently via edit being focused property
}

void DoCommandListDlg(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Mapped Commands" );

	auto grid = new QGridLayout( &dialog );

	auto tree = new QTreeWidget;
	grid->addWidget( tree, 1, 0, 1, 2 );
	tree->setColumnCount( 2 );
	tree->setSortingEnabled( true );
	tree->sortByColumn( 0, Qt::SortOrder::AscendingOrder );
	tree->setUniformRowHeights( true ); // optimization
	tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
	tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
	tree->header()->setStretchLastSection( false ); // non greedy column sizing
	tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
	tree->setRootIsDecorated( false );
	tree->setHeaderLabels( { "Command", "Key" } );

	QObject::connect( tree, &QTreeWidget::itemActivated, []( QTreeWidgetItem *item, int column ){
		if( item != nullptr )
			accelerator_edit( item );
	} );

	{
		// Initialize dialog
		const auto path = StringStream( SettingsPath_get(), "commandlist.txt" );
		globalOutputStream() << "Writing the command list to " << path << '\n';

		TextFileOutputStream commandList( path );

		for( const auto&[ name, value ] : g_shortcuts )
		{
			auto item = new QTreeWidgetItem( tree, { name.c_str(), value.accelerator.toString() } );
			accelerator_item_set_icon( item, value );

			if ( !commandList.failed() ) {
				int l = strlen( name.c_str() );
				commandList << name.c_str();
				while ( l++ < 32 )
					commandList << ' ';
				commandList << value.accelerator << '\n';
			}
		}
	}

	{
		auto commandLine = new QLineEdit;
		grid->addWidget( commandLine, 0, 0 );
		commandLine->setClearButtonEnabled( true );
		commandLine->setPlaceholderText( QString::fromUtf8( u8"🔍 by command name" ) );

		auto keyLine = new QLineEdit;
		grid->addWidget( keyLine, 0, 1 );
		keyLine->setClearButtonEnabled( true );
		keyLine->setPlaceholderText( QString::fromUtf8( u8"🔍 by keys" ) );

		const auto filter = [tree]( const int column, const QString& text ){
			for( QTreeWidgetItemIterator it( tree ); *it; ++it )
			{
				( *it )->setHidden( !( *it )->text( column ).contains( text, Qt::CaseSensitivity::CaseInsensitive ) );
			}
		};
		QObject::connect( commandLine, &QLineEdit::textChanged, [filter]( const QString& text ){ filter( 0, text ); } );
		QObject::connect( keyLine, &QLineEdit::textChanged, [filter]( const QString& text ){ filter( 1, text ); } );
	}

	{
		auto buttons = new QDialogButtonBox( Qt::Orientation::Vertical );
		grid->addWidget( buttons, 1, 2, 1, 1 );

		QPushButton *editbutton = buttons->addButton( "Edit", QDialogButtonBox::ButtonRole::ActionRole );
		QObject::connect( editbutton, &QPushButton::clicked, [tree](){
			if( const auto items = tree->selectedItems(); !items.isEmpty() )
				accelerator_edit( items.first() );
		} );

		QPushButton *clearbutton = buttons->addButton( "Clear", QDialogButtonBox::ButtonRole::ActionRole );
		QObject::connect( clearbutton, &QPushButton::clicked, [tree](){
			if( const auto items = tree->selectedItems(); !items.isEmpty() )
				accelerator_clear_button_clicked( items.first() );
		} );

		QPushButton *resetbutton = buttons->addButton( "Reset", QDialogButtonBox::ButtonRole::ResetRole );
		QObject::connect( resetbutton, &QPushButton::clicked, [tree](){
			if( const auto items = tree->selectedItems(); !items.isEmpty() )
				accelerator_alter( items.first(), {} );
		} );

		QPushButton *resetallbutton = buttons->addButton( "Reset All", QDialogButtonBox::ButtonRole::ResetRole );
		QObject::connect( resetallbutton, &QPushButton::clicked, [tree](){
			if( eIDYES == qt_MessageBox( tree, "Surely reset all shortcuts now?", "Boo!", EMessageBoxType::Question ) )
				accelerator_reset_all_button_clicked( tree );
		} );
	}

	dialog.exec();
}



#include "profile/profile.h"

const char* const COMMANDS_VERSION = "1.0-gtk-accelnames";

void SaveCommandMap( const char* path ){
	const auto strINI = StringStream( path, "shortcuts.ini" );

	TextFileOutputStream file( strINI );
	if ( !file.failed() ) {
		file << "[Version]\n";
		file << "number=" << COMMANDS_VERSION << '\n';
		file << '\n';
		file << "[Commands]\n";

		auto writeCommandMap = [&file]( const char* name, const QKeySequence& accelerator ){
			file << name << '=';
			file << accelerator;
			file << '\n';
		};
		GlobalShortcuts_foreach( writeCommandMap );
	}
}

class ReadCommandMap
{
	const char* m_filename;
	std::size_t m_count;
public:
	ReadCommandMap( const char* filename ) : m_filename( filename ), m_count( 0 ){
	}
	void operator()( const char* name, QKeySequence& accelerator ){
		char value[1024];
		if ( read_var( m_filename, "Commands", name, value ) ) {
			if ( string_empty( value ) ) {
				accelerator = {};
			}
			else{
				accelerator = QKeySequence( value );
				if ( QKeySequence_valid( accelerator ) ) {
					++m_count;
				}
				else
				{
					globalWarningStream() << "WARNING: failed to parse user command " << makeQuoted( name ) << ": unknown key " << makeQuoted( value ) << '\n';
				}
			}
		}
	}
	std::size_t count() const {
		return m_count;
	}
};

void LoadCommandMap( const char* path ){
	const auto strINI = StringStream( path, "shortcuts.ini" );

	FILE* f = fopen( strINI, "r" );
	if ( f != 0 ) {
		fclose( f );
		globalOutputStream() << "loading custom shortcuts list from " << makeQuoted( strINI ) << '\n';

		Version version = version_parse( COMMANDS_VERSION );
		Version dataVersion = { 0, 0 };

		{
			char value[1024];
			if ( read_var( strINI, "Version", "number", value ) ) {
				dataVersion = version_parse( value );
			}
		}

		if ( version_compatible( version, dataVersion ) ) {
			globalOutputStream() << "commands import: data version " << dataVersion << " is compatible with code version " << version << '\n';
			ReadCommandMap visitor( strINI );
			GlobalShortcuts_foreach( visitor );
			globalOutputStream() << "parsed " << visitor.count() << " custom shortcuts\n";
		}
		else
		{
			globalWarningStream() << "commands import: data version " << dataVersion << " is not compatible with code version " << version << '\n';
		}
	}
	else
	{
		globalWarningStream() << "failed to load custom shortcuts from " << makeQuoted( strINI ) << '\n';
	}
}

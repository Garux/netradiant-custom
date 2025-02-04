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
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Small functions to help with GTK
//

#include "gtkmisc.h"

#include "math/vector.h"
#include "os/path.h"

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/menu.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/image.h"
#include "commands.h"

#include <QCoreApplication>
#include <QLineEdit>
#include <QFontDialog>


void process_gui(){
	QCoreApplication::processEvents();
}

// =============================================================================
// Misc stuff

static QWidget *g_shortcuts_widget = nullptr;

void GlobalShortcuts_setWidget( QWidget *widget ){
	g_shortcuts_widget = widget;
}

inline QAction* command_connect_accelerator( const Command& command ){
	QAction *&action = command.getAction();
	if( action == nullptr ){
		action = new QAction( g_shortcuts_widget );
		g_shortcuts_widget->addAction( action );
		action->setShortcutContext( Qt::ShortcutContext::ApplicationShortcut );
		QObject::connect( action, &QAction::triggered, command.m_callback );
	}
	action->setShortcut( command.m_accelerator );
	return action;
}


inline QAction* command_connect_accelerator_( const char* name ){
	GlobalShortcuts_register( name, 1 );
	return command_connect_accelerator( GlobalCommands_find( name ) );
}

void command_connect_accelerator( const char* name ){
	command_connect_accelerator_( name );
}

void command_disconnect_accelerator( const char* name ){
	const Command& command = GlobalCommands_find( name );
	if( command.getAction() != nullptr )
		command.getAction()->setShortcut( {} );
}


static void action_set_checked_callback( QAction& action, bool enabled ){
	action.setChecked( enabled );
}
typedef ReferenceCaller<QAction, void(bool), action_set_checked_callback> ActionSetCheckedCaller;

inline QAction* toggle_add_accelerator_( const char* name ){
	GlobalShortcuts_register( name, 2 );
	const Toggle& toggle = GlobalToggles_find( name );
	auto action = command_connect_accelerator( toggle.m_command );
	action->setCheckable( true );
	toggle.m_exportCallback( ActionSetCheckedCaller( *action ) );
	return action;
}

void toggle_add_accelerator( const char* name ){
	toggle_add_accelerator_( name );
}

void toggle_remove_accelerator( const char* name ){
	const Toggle& toggle = GlobalToggles_find( name );
	if( toggle.m_command.getAction() != nullptr )
		toggle.m_command.getAction()->setShortcut( {} );
}


QAction* create_check_menu_item_with_mnemonic( QMenu* menu, const char* mnemonic, const char* commandName ){
	auto action = toggle_add_accelerator_( commandName );
	action->setText( mnemonic );
	menu->addAction( action );
	return action;
}

QAction* create_menu_item_with_mnemonic( QMenu *menu, const char *mnemonic, const char* commandName ){
	auto action = command_connect_accelerator_( commandName );
	action->setText( mnemonic );
	menu->addAction( action );
	return action;
}


// can update this on QAction::changed() signal, but it's called too often and even on setChecked(); let's only have this on construction
static void toolbar_action_set_tooltip( QAction *action, const char *description ){
	if( QKeySequence_valid( action->shortcut() ) ){
		QString out;
		const char *p = description;
		for( ; *p && *p != '\n'; ++p ) // append 1st line
			out += *p;
		out += " (";
		out += action->shortcut().toString();  // append shortcut
		out += ")";
		for( ; *p; ++p )  // append the rest
			out += *p;
		action->setToolTip( out );
	}
	else{
		action->setToolTip( description );
	}
}

QAction* toolbar_append_button( QToolBar* toolbar, const char* description, const char* icon, const char* commandName ){
	auto action = command_connect_accelerator_( commandName );
	action->setIcon( new_local_icon( icon ) );
	toolbar_action_set_tooltip( action, description );
	toolbar->addAction( action );
	return action;
}

QAction* toolbar_append_toggle_button( QToolBar* toolbar, const char* description, const char* icon, const char* commandName ){
	auto action = toggle_add_accelerator_( commandName );
	action->setIcon( new_local_icon( icon ) );
	toolbar_action_set_tooltip( action, description );
	toolbar->addAction( action );
	return action;
}

#include <QColorDialog>
bool color_dialog( QWidget *parent, Vector3& color, const char* title ){
	const QColor clr = QColorDialog::getColor( QColor::fromRgbF( color[0], color[1], color[2] ), parent, title );

	if( clr.isValid() )
		color = Vector3( clr.redF(), clr.greenF(), clr.blueF() );
	return clr.isValid(); // invalid color if the user cancels the dialog
}

bool OpenGLFont_dialog( QWidget *parent, const char* font, const int size, CopiedString &newfont, int &newsize ){
	bool ok;
	QFont f = QFontDialog::getFont( &ok, QFont( font, size ), parent );
	if( ok ){
		newfont = f.family().toLatin1().constData();
		newsize = f.pointSize();
	}
	return ok;
}

void button_clicked_entry_browse_file( QLineEdit* entry ){
	const char *filename = file_dialog( entry, true, "Choose File", entry->text().toLatin1().constData() );

	if ( filename != 0 ) {
		entry->setText( filename );
	}
}

void button_clicked_entry_browse_directory( QLineEdit* entry ){
	const QString dir = dir_dialog( entry,
		path_is_absolute( entry->text().toLatin1().constData() )
		? entry->text()
		: QString() );

	if ( !dir.isEmpty() )
		entry->setText( dir );
}

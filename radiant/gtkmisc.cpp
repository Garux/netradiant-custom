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

#include "generic/vector.h"
#include "os/path.h"

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/image.h"
#include "commands.h"
#include "stream/stringstream.h"
#include "stream/textstream.h"

#include <QCoreApplication>
#include <QLineEdit>
#include <QFontDialog>
#include <QMouseEvent>


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
	auto *action = command_connect_accelerator( toggle.m_command );
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
	auto *action = toggle_add_accelerator_( commandName );
	action->setText( mnemonic );
	menu->addAction( action );
	return action;
}

QAction* create_menu_item_with_mnemonic( QMenu *menu, const char *mnemonic, const char* commandName ){
	auto *action = command_connect_accelerator_( commandName );
	action->setText( mnemonic );
	menu->addAction( action );
	return action;
}


struct ToolbarItem
{
	ToolbarItem( QToolBar *toolbar, QAction *action, const char *commandName, bool separator )
		: m_toolbar( toolbar ), m_action( action ), m_commandName( commandName ), m_separator( separator ) {
	}
	QToolBar *m_toolbar;
	QAction *m_action;
	CopiedString m_commandName; // empty for separator
	const bool m_separator;
	bool m_enabled = true;
};

CopiedString g_toolbarHiddenButtons =
	"%OpenMap%"
	"%SaveMap%"
	"%Undo%"
	"%Redo%"
	"%SelectTouching%"
	"%SelectInside%"
	"%NextView%"
	"%ToggleCubicClip%"
	"%CapCurrentCurve%"
	"%ToggleEntityInspector%"
	"%ToggleConsole%"
	"%ToggleTextures%"
	"%RefreshReferences%"
	"%bobToolz::PolygonBuilder%"
	"%bobToolz::TreePlanter%"
	"%bobToolz::PlotSplines%"
	"%bobToolz::DropEntity%"
	"%bobToolz::FlipTerrain%";

class ToolbarItems
{
	std::vector<ToolbarItem> m_toolbarItems;
	using iterator = decltype( m_toolbarItems )::iterator;
	using reverse_iterator = decltype( m_toolbarItems )::reverse_iterator;
	iterator begin(){
		return m_toolbarItems.begin();
	}
	iterator end(){
		return m_toolbarItems.end();
	}

	void item_enable( iterator item, bool enable ){
		if( ( item->m_enabled = enable ) ){
			for( auto it = item + 1; it != end() && item->m_toolbar == it->m_toolbar; ++it )
				if( it->m_enabled )
					return item->m_toolbar->insertAction( it->m_action, item->m_action ); // insert before next visible toolbar item
			item->m_toolbar->addAction( item->m_action ); // or end
		}
		else
			item->m_toolbar->removeAction( item->m_action );
	}
	void update_separators_visibility(){
		for( auto item = begin(); item != end(); ++item )
		{
			if( item->m_separator ){
				const bool enable =
				[&](){ // check that there are visible items up to the next separator or toolbar end
					for( auto it = item + 1; item != end() && item->m_toolbar == it->m_toolbar && !it->m_separator; ++it )
						if( it->m_enabled )
							return true;
					return false;
				}() &&
				[&](){ // check if any item before is visible // &*rev == &*(it - 1)
					for( auto it = reverse_iterator( item ); it != m_toolbarItems.rend() && item->m_toolbar == it->m_toolbar; ++it )
						if( it->m_enabled )
							return true;
					return false;
				}();

				if( item->m_enabled != enable )
					item_enable( item, enable );
			}
		}
	}
	 // modify particular entires, so that layout dependent entries will not be wiped, when toggling buttons visibility in different layout
	void exportState( const char *commandName, bool enable ){
		auto& entries = g_toolbarHiddenButtons;
		const auto entry = StringStream<64>( '%', commandName, '%' );
		const char *found = strstr( entries.c_str(), entry );
		if( enable && found != nullptr ) // enable = wipe
			entries = StringStream( StringRange( entries.c_str(), found ), found + string_length( entry ) );
		if( !enable && found == nullptr ) // disable = add
			entries = StringStream( entries, entry );
	}
public:
	void importState( const char *commandNames ){
		StringOutputStream str( 64 );
		for( auto& item : *this )
		{
			if( !item.m_separator && strstr( commandNames, str( '%', item.m_commandName, '%' ) ) != nullptr ){
				item.m_toolbar->removeAction( item.m_action );
				item.m_enabled = false;
			}
		}
		update_separators_visibility();
	}
	void construct_control_menu( QMenu *menu ){
		for( auto item = begin(); item != end(); ++item )
		{
			if( !item->m_separator ){
				auto *action = menu->addAction( item->m_action->icon(), item->m_commandName.c_str(), [item, this]( bool checked ){
					item_enable( item, checked );
					update_separators_visibility();
					exportState( item->m_commandName.c_str(), checked );
				} );
				action->setCheckable( true );
				action->setChecked( item->m_enabled );
				// separate different toolbars
				if( auto next = item + 1; next != end() && item->m_toolbar != next->m_toolbar )
					menu->addSeparator();
			}
		}
		// prevent closing the menu on clicks (note: click to detach menu closes it)
		class Filter : public QObject
		{
			using QObject::QObject;
		protected:
			bool eventFilter( QObject *obj, QEvent *event ) override {
				if( event->type() == QEvent::MouseButtonRelease ) {
					auto *mouseEvent = static_cast<QMouseEvent *>( event );
					if( mouseEvent->button() == Qt::MouseButton::LeftButton ){
						auto *menu = static_cast<QMenu *>( obj );
						if( QAction *action = menu->actionAt( mouseEvent->pos() ) ){
							action->trigger();
						}
						event->accept();
						return true;
					}
				}
				return QObject::eventFilter( obj, event ); // standard event processing
			}
		};
		menu->installEventFilter( new Filter( menu ) );
	}
	void push_back( ToolbarItem&& item ){
		m_toolbarItems.push_back( std::move( item ) );
	}
};
static ToolbarItems s_toolbarItems;

void toolbar_importState( const char *commandNames ){
	s_toolbarItems.importState( commandNames );
}
void toolbar_construct_control_menu( QMenu *menu ){
	s_toolbarItems.construct_control_menu( menu );
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

QAction* toolbar_append_button( QToolBar* toolbar, const char* description, const QIcon& icon, const char* commandName ){
	auto *action = command_connect_accelerator_( commandName );
	action->setIcon( icon );
	toolbar_action_set_tooltip( action, description );
	toolbar->addAction( action );
	s_toolbarItems.push_back( { toolbar, action, commandName, false } );
	return action;
}
QAction* toolbar_append_button( QToolBar* toolbar, const char* description, const char* icon, const char* commandName ){
	return toolbar_append_button( toolbar, description, new_local_icon( icon ), commandName );
}

QAction* toolbar_append_toggle_button( QToolBar* toolbar, const char* description, const char* icon, const char* commandName ){
	auto *action = toggle_add_accelerator_( commandName );
	action->setIcon( new_local_icon( icon ) );
	toolbar_action_set_tooltip( action, description );
	toolbar->addAction( action );
	s_toolbarItems.push_back( { toolbar, action, commandName, false } );
	return action;
}

void toolbar_append_separator( QToolBar* toolbar ){
	s_toolbarItems.push_back( { toolbar, toolbar->addSeparator(), "", true } );
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

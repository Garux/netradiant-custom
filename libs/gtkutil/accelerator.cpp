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

#include "accelerator.h"

#include "debugging/debugging.h"

#include <map>
#include <set>

#include "generic/callback.h"
#include "generic/bitfield.h"
#include "string/string.h"

#include "accelerator_translate.h"

#include <QWidget>
#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QApplication>



void accelerator_write( const QKeySequence& accelerator, TextOutputStream& ostream ){
	ostream << accelerator.toString().toLatin1().constData();
}

typedef std::map<QKeySequence, Callback<void()>> AcceleratorMap;

bool accelerator_map_insert( AcceleratorMap& acceleratorMap, QKeySequence accelerator, const Callback<void()>& callback ){
	if ( QKeySequence_valid( accelerator ) ) {
		return acceleratorMap.insert( AcceleratorMap::value_type( accelerator, callback ) ).second;
	}
	return true;
}

bool accelerator_map_erase( AcceleratorMap& acceleratorMap, QKeySequence accelerator ){
	if ( QKeySequence_valid( accelerator ) ) {
		AcceleratorMap::iterator i = acceleratorMap.find( accelerator );
		if ( i == acceleratorMap.end() ) {
			return false;
		}
		acceleratorMap.erase( i );
	}
	return true;
}

QKeySequence accelerator_for_event_key( int keyval, Qt::KeyboardModifiers state ){
	return QKeySequence( keyval | state );
}

QKeySequence accelerator_for_event_key( const QKeyEvent* event ){
	return accelerator_for_event_key( event->key(), event->modifiers() );
}

bool AcceleratorMap_activate( const AcceleratorMap& acceleratorMap, const QKeySequence& accelerator ){
	AcceleratorMap::const_iterator i = acceleratorMap.find( accelerator );
	if ( i != acceleratorMap.end() ) {
		( *i ).second();
		return true;
	}

	return false;
}


#include <set>

struct PressedKeys
{
	typedef std::set<int> Keys;
	Keys keys;
};

AcceleratorMap g_keydown_accelerators;
AcceleratorMap g_keyup_accelerators;

bool Keys_press( PressedKeys::Keys& keys, int keyval ){
	if ( keys.insert( keyval ).second ) {
		return AcceleratorMap_activate( g_keydown_accelerators, accelerator_for_event_key( keyval, {} ) );
	}
	return g_keydown_accelerators.contains( accelerator_for_event_key( keyval, {} ) );
}

bool Keys_release( PressedKeys::Keys& keys, int keyval ){
	if ( keys.erase( keyval ) != 0 ) {
		return AcceleratorMap_activate( g_keyup_accelerators, accelerator_for_event_key( keyval, {} ) );
	}
	return g_keyup_accelerators.contains( accelerator_for_event_key( keyval, {} ) );
}

void Keys_releaseAll( PressedKeys::Keys& keys, Qt::KeyboardModifiers state ){
	for ( auto key : keys )
	{
		AcceleratorMap_activate( g_keyup_accelerators, accelerator_for_event_key( key, state ) );
	}
	keys.clear();
}

bool PressedKeys_key_press( const QKeyEvent* event, PressedKeys& pressedKeys ){
	//globalOutputStream() << "pressed: " << event->key() << '\n';
	return event->modifiers() == 0 && Keys_press( pressedKeys.keys, qt_keyvalue_is_known( event->key() )? event->key() : event->nativeVirtualKey() );
}

bool PressedKeys_key_release( const QKeyEvent* event, PressedKeys& pressedKeys ){
	//globalOutputStream() << "released: " << event->key() << '\n';
	return Keys_release( pressedKeys.keys, qt_keyvalue_is_known( event->key() )? event->key() : event->nativeVirtualKey() );
}

PressedKeys g_pressedKeys;

void GlobalPressedKeys_releaseAll(){
	Keys_releaseAll( g_pressedKeys.keys, {} );
}

class PressedKeysHandler : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( PressedKeys_key_press( keyEvent, g_pressedKeys ) ){ // note autorepeat fires this too
				event->accept();
				return true;
			}
		}
		else if( event->type() == QEvent::KeyPress ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( PressedKeys_key_press( keyEvent, g_pressedKeys ) ){ // note autorepeat fires this too
				event->accept();
				return true;
			}
		}
		else if( event->type() == QEvent::KeyRelease ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( !keyEvent->isAutoRepeat() && PressedKeys_key_release( keyEvent, g_pressedKeys ) ){
				event->accept();
				return true;
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
g_pressedKeysHandler;

void GlobalPressedKeys_connect( QWidget* window ){
	window->installEventFilter( &g_pressedKeysHandler );
//	qApp->installEventFilter( &g_pressedKeysHandler );
	QObject::connect( qApp, &QApplication::focusChanged, GlobalPressedKeys_releaseAll );
}




void keydown_accelerators_add( QKeySequence accelerator, const Callback<void()>& callback ){
	//globalOutputStream() << "keydown_accelerators_add: " << makeQuoted( accelerator ) << '\n';
	if ( !accelerator_map_insert( g_keydown_accelerators, accelerator, callback ) ) {
		globalErrorStream() << "keydown_accelerators_add: already exists: " << makeQuoted( accelerator ) << '\n';
	}
}
void keydown_accelerators_remove( QKeySequence accelerator ){
	//globalOutputStream() << "keydown_accelerators_remove: " << makeQuoted( accelerator ) << '\n';
	if ( !accelerator_map_erase( g_keydown_accelerators, accelerator ) ) {
		globalErrorStream() << "keydown_accelerators_remove: not found: " << makeQuoted( accelerator ) << '\n';
	}
}

void keyup_accelerators_add( QKeySequence accelerator, const Callback<void()>& callback ){
	//globalOutputStream() << "keyup_accelerators_add: " << makeQuoted( accelerator ) << '\n';
	if ( !accelerator_map_insert( g_keyup_accelerators, accelerator, callback ) ) {
		globalErrorStream() << "keyup_accelerators_add: already exists: " << makeQuoted( accelerator ) << '\n';
	}
}
void keyup_accelerators_remove( QKeySequence accelerator ){
	//globalOutputStream() << "keyup_accelerators_remove: " << makeQuoted( accelerator ) << '\n';
	if ( !accelerator_map_erase( g_keyup_accelerators, accelerator ) ) {
		globalErrorStream() << "keyup_accelerators_remove: not found: " << makeQuoted( accelerator ) << '\n';
	}
}


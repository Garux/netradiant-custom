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

#include "windowobservers.h"

#include <vector>
#include "generic/bitfield.h"

#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>

namespace
{
ModifierFlags g_modifier_state = c_modifierNone;
}

typedef std::vector<WindowObserver*> WindowObservers;
WindowObservers g_window_observers;

inline void WindowObservers_OnModifierDown( WindowObservers& observers, ModifierFlags type ){
	g_modifier_state = bitfield_enable( g_modifier_state, type );
	for ( auto *observer : observers )
	{
		observer->onModifierDown( type );
	}
}

inline void WindowObservers_OnModifierUp( WindowObservers& observers, ModifierFlags type ){
	g_modifier_state = bitfield_disable( g_modifier_state, type );
	for ( auto *observer : observers )
	{
		observer->onModifierUp( type );
	}
}


class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::KeyPress ) {
			auto *keyEvent = static_cast<QKeyEvent *>( event );
			switch ( keyEvent->key() )
			{
			case Qt::Key_Alt:
				//globalOutputStream() << "Alt PRESSED\n";
				WindowObservers_OnModifierDown( g_window_observers, c_modifierAlt );
				break;
			case Qt::Key_Shift:
				//globalOutputStream() << "Shift PRESSED\n";
				WindowObservers_OnModifierDown( g_window_observers, c_modifierShift );
				break;
			case Qt::Key_Control:
				//globalOutputStream() << "Control PRESSED\n";
				WindowObservers_OnModifierDown( g_window_observers, c_modifierControl );
				break;
			}
		}
		else if( event->type() == QEvent::KeyRelease ) {
			auto *keyEvent = static_cast<QKeyEvent *>( event );
			switch ( keyEvent->key() )
			{
			case Qt::Key_Alt:
				//globalOutputStream() << "Alt RELEASED\n";
				WindowObservers_OnModifierUp( g_window_observers, c_modifierAlt );
				break;
			case Qt::Key_Shift:
				//globalOutputStream() << "Shift RELEASED\n";
				WindowObservers_OnModifierUp( g_window_observers, c_modifierShift );
				break;
			case Qt::Key_Control:
				//globalOutputStream() << "Control RELEASED\n";
				WindowObservers_OnModifierUp( g_window_observers, c_modifierControl );
				break;
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
g_keyboard_event_filter;

void WindowObservers_UpdateModifier( WindowObservers& observers, ModifierFlags modifiers, ModifierFlags modifier ){
	if ( !bitfield_enabled( g_modifier_state, modifier ) && bitfield_enabled( modifiers, modifier ) ) {
		WindowObservers_OnModifierDown( observers, modifier );
	}
	if ( bitfield_enabled( g_modifier_state, modifier ) && !bitfield_enabled( modifiers, modifier ) ) {
		WindowObservers_OnModifierUp( observers, modifier );
	}
}

void WindowObservers_UpdateModifiers( WindowObservers& observers, ModifierFlags modifiers ){
	WindowObservers_UpdateModifier( observers, modifiers, c_modifierAlt );
	WindowObservers_UpdateModifier( observers, modifiers, c_modifierShift );
	WindowObservers_UpdateModifier( observers, modifiers, c_modifierControl );
}


class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::MouseButtonPress
		 || event->type() == QEvent::MouseMove
		 || event->type() == QEvent::MouseButtonRelease ) {
			auto *mouseEvent = static_cast<QMouseEvent *>( event );
			WindowObservers_UpdateModifiers( g_window_observers, modifiers_for_state( mouseEvent->modifiers() ) );
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
g_mouse_event_filter;

void GlobalWindowObservers_add( WindowObserver* observer ){
	g_window_observers.push_back( observer );
}

void GlobalWindowObservers_connectTopLevel( QWidget* window ){
	window->installEventFilter( &g_keyboard_event_filter );
}

void GlobalWindowObservers_connectWidget( QWidget* widget ){
	widget->installEventFilter( &g_mouse_event_filter );
}

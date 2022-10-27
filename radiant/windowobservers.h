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

#pragma once

#include "windowobserver.h"

#include <Qt>

void GlobalWindowObservers_add( class WindowObserver* observer );
void GlobalWindowObservers_connectWidget( class QWidget* widget );
void GlobalWindowObservers_connectTopLevel( class QWidget* window );

inline ButtonIdentifier button_for_button( Qt::MouseButton button ){
	switch ( button )
	{
	case Qt::MouseButton::LeftButton:
		return c_buttonLeft;
	case Qt::MouseButton::MiddleButton:
		return c_buttonMiddle;
	case Qt::MouseButton::RightButton:
		return c_buttonRight;
	default:
		return c_buttonInvalid;
	}
}

inline ModifierFlags modifiers_for_state( Qt::KeyboardModifiers state ){
	ModifierFlags modifiers = c_modifierNone;
	if ( state & Qt::KeyboardModifier::ShiftModifier ) {
		modifiers |= c_modifierShift;
	}
	if ( state & Qt::KeyboardModifier::ControlModifier ) {
		modifiers |= c_modifierControl;
	}
	if ( state & Qt::KeyboardModifier::AltModifier ) {
		modifiers |= c_modifierAlt;
	}
	return modifiers;
}

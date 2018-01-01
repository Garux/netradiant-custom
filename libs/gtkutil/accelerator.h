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

#include <QKeySequence>

#include "generic/callback.h"

// technically this should also check if sequence has keys, not just modifiers
// but this works with current shortcuts providers ( QKeySequence( string ) & QKeySequenceEdit )
inline bool QKeySequence_valid( const QKeySequence& accelerator ){
	return !accelerator.isEmpty() && accelerator[0] != Qt::Key_unknown;
}

QKeySequence accelerator_for_event_key( const class QKeyEvent* event );


class TextOutputStream;
void accelerator_write( const QKeySequence& accelerator, TextOutputStream& ostream );

template<typename TextOutputStreamType>
TextOutputStreamType& ostream_write( TextOutputStreamType& ostream, const QKeySequence& accelerator ){
	accelerator_write( accelerator, ostream );
	return ostream;
}

void keydown_accelerators_add( QKeySequence accelerator, const Callback<void()>& callback );
void keydown_accelerators_remove( QKeySequence accelerator );
void keyup_accelerators_add( QKeySequence accelerator, const Callback<void()>& callback );
void keyup_accelerators_remove( QKeySequence accelerator );

void GlobalPressedKeys_releaseAll();
void GlobalPressedKeys_connect( class QWidget* window );


class QAction;

class Command
{
	mutable QAction *m_action{};
public:
	QAction*& getAction() const {
		return m_action;
	}
	Callback<void()> m_callback;
	const QKeySequence& m_accelerator;
	Command( const Callback<void()>& callback, const QKeySequence& accelerator ) : m_callback( callback ), m_accelerator( accelerator ){
	}
};

class Toggle
{
public:
	Command m_command;
	BoolExportCallback m_exportCallback;
	Toggle( const Callback<void()>& callback, const QKeySequence& accelerator, const BoolExportCallback& exportCallback ) : m_command( callback, accelerator ), m_exportCallback( exportCallback ){
	}
};

class KeyEvent
{
public:
	const QKeySequence& m_accelerator;
	Callback<void()> m_keyDown;
	Callback<void()> m_keyUp;
	KeyEvent( const QKeySequence& accelerator, const Callback<void()>& keyDown, const Callback<void()>& keyUp ) : m_accelerator( accelerator ), m_keyDown( keyDown ), m_keyUp( keyUp ){
	}
};

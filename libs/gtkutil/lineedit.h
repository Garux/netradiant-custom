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

#include <QLineEdit>
#include <QKeyEvent>
#include <QCompleter>
#include <QAbstractItemView>

/// @brief Subclassed QLineEdit not comsuming undo/redo shortcuts
/// it's more useful to have working undo of editor
/// Qt should better not eat these shortcuts when QLineEdit's undo is disabled, but it does
class LineEdit : public QLineEdit
{
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent == QKeySequence::StandardKey::Undo
			 || keyEvent == QKeySequence::StandardKey::Redo )
				return false;
			// fix QCompleter leaking shortcuts
			if( completer() != nullptr && completer()->popup() != nullptr && completer()->popup()->isVisible() )
				if( keyEvent->key() == Qt::Key_Return
				 || keyEvent->key() == Qt::Key_Enter
				 || keyEvent->key() == Qt::Key_Escape
				 || keyEvent->key() == Qt::Key_Up
				 || keyEvent->key() == Qt::Key_Down
				 || keyEvent->key() == Qt::Key_PageUp
				 || keyEvent->key() == Qt::Key_PageDown )
					event->accept();
		}
		return QLineEdit::event( event );
	}
};

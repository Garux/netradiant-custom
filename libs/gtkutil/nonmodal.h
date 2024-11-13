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

#include "lineedit.h"
#include "spinbox.h"
#include <QTimer>

#include "generic/callback.h"


class NonModalEntry : public LineEdit
{
	bool m_editing{};
	Callback m_apply;
	Callback m_cancel;
public:
	NonModalEntry( const Callback& apply, const Callback& cancel ) : LineEdit(), m_apply( apply ), m_cancel( cancel ){
		QObject::connect( this, &QLineEdit::textEdited, [this](){ m_editing = true; } );
		// triggered on enter & focus out; need to track editing state, as nonedited triggers this too
		QObject::connect( this, &QLineEdit::editingFinished, [this](){
			if( m_editing ){
				m_editing = false;
				m_apply();
			}
			clearFocus();
		} );
	}
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() == Qt::Key_Escape ){
				m_editing = false;
				m_cancel();
				event->accept();
				// defer clearFocus(); as immediately done after certain actions = cursor visible + not handling key input
				QTimer::singleShot( 0, [this](){ clearFocus(); } );
			}
			else if( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter ){
				m_editing = true; // on Enter m_apply() is always wanted, since not every selected instance necessarily has shown property applied
			}
		}
		return LineEdit::event( event );
	}
	void focusInEvent( QFocusEvent *event ) override {
		if( event->reason() == Qt::FocusReason::MouseFocusReason )
			QTimer::singleShot( 0, [this](){ selectAll(); } );
		QLineEdit::focusInEvent( event );
	}
};


class NonModalSpinner : public DoubleSpinBox
{
	using DoubleSpinBox::DoubleSpinBox;
	bool m_editing{};
	Callback m_apply;
	Callback m_cancel;
public:
	void setCallbacks( const Callback& apply, const Callback& cancel ){
		m_apply = apply;
		m_cancel = cancel;
		QObject::connect( lineEdit(), &QLineEdit::textEdited, [this](){ m_editing = true; } );
		// triggered on enter & focus out; need to track editing state, as nonedited triggers this too
		QObject::connect( this, &QAbstractSpinBox::editingFinished, [this](){
			if( m_editing ){
				m_editing = false;
				m_apply();
			}
			clearFocus();
		} );
	}
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() == Qt::Key_Escape ){
				m_editing = false;
				m_cancel();
				clearFocus();
				event->accept();
			}
			else if( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter ){
				m_editing = true; // on Enter m_apply() is always wanted, since not every selected instance necessarily has shown property applied
			}
		}
		return DoubleSpinBox::event( event );
	}
	void stepBy( int steps ) override {
		DoubleSpinBox::stepBy( steps );
		m_editing = false;
		m_apply();
	}
};


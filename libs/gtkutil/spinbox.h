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

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>
#include <QLabel>
#include <QMouseEvent>

template<class SpinT>
class QSpinBox_mod : public SpinT
{
	using ValueT = typename std::conditional<std::is_same_v<SpinT, QSpinBox>, int, double>::type;
public:
	QSpinBox_mod( ValueT min = 0, ValueT max = 99, ValueT value = 0, int decimals = 2, ValueT step = 1, bool wrap = false ) : SpinT() {
		if constexpr ( std::is_same_v<SpinT, QDoubleSpinBox> ){
			SpinT::setLocale( QLocale::Language::C ); // force period separator
			SpinT::setDecimals( decimals );
		}
		SpinT::setRange( min, max );
		SpinT::setValue( value );
		SpinT::setSingleStep( step );
		SpinT::setWrapping( wrap );
		SpinT::setAccelerated( true );
	}
protected:
	bool event( QEvent *event ) override {
		/* QAbstractSpinBox has no well defined ShortcutOverride routine, unlike underlying QLineEdit
		   thus box' portion of key events ends up firing application's shortcuts (e.g. up, down, pgUp, pgDown)
		   let's lock entire input on the box except a few keys, handled by parent window */
		if( event->type() == QEvent::ShortcutOverride ){
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() != Qt::Key_Return
			 && keyEvent->key() != Qt::Key_Enter
			 && keyEvent->key() != Qt::Key_Escape )
				event->accept();
		}
		return SpinT::event( event );
	}
	void focusInEvent( QFocusEvent *event ) override {
		if( event->reason() == Qt::FocusReason::MouseFocusReason )
			QTimer::singleShot( 0, [this](){ SpinT::selectAll(); } );
		SpinT::focusInEvent( event );
	}
};

using DoubleSpinBox = QSpinBox_mod<QDoubleSpinBox>;
using SpinBox = QSpinBox_mod<QSpinBox>;


/// \brief Label for a QSpinBox or QDoubleSpinBox
/// Changes their value by left mouse drag
/// Ctrl adds 10x multiplier
template <typename SpinBoxT>
class SpinBoxLabel : public QLabel
{
protected:
	SpinBoxT *m_spin;
	bool m_isInDrag{};
	QPoint m_dragStart;
	int m_dragOccured;
	int m_dragAccum;
public:
    SpinBoxLabel( const QString& labelText, SpinBoxT* spin ) : QLabel( labelText ), m_spin( spin )
    {
		setCursor( Qt::CursorShape::SizeHorCursor );
    }
protected:
    void mousePressEvent( QMouseEvent* event ) override {
		if( event->button() == Qt::MouseButton::LeftButton ){
			m_spin->setFocus();
			m_spin->selectAll();
			m_isInDrag = true;
			m_dragStart = event->globalPos();
			m_dragOccured = false;
			m_dragAccum = 0;
			setCursor( Qt::CursorShape::BlankCursor );
		}
    }
    void mouseMoveEvent( QMouseEvent* event ) override {
		if( m_isInDrag && event->buttons() == Qt::MouseButton::LeftButton ){
			m_dragAccum += event->globalPos().x() - m_dragStart.x();
			const int delta = m_dragAccum / 20;
			if( delta != 0 ){
				m_dragOccured = true;
				m_dragAccum %= 20;
				m_spin->stepBy( event->modifiers().testFlag( Qt::KeyboardModifier::ControlModifier )? delta * 10 : delta );
				QCursor::setPos( m_dragStart );
			}
		}
    }
    void mouseReleaseEvent( QMouseEvent* event ) override {
		if( m_isInDrag ){
			m_isInDrag = false;
			setCursor( Qt::CursorShape::SizeHorCursor );
		}
    }
};
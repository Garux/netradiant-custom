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

#include "timer.h"
#include <QMouseEvent>

/// \brief manages mouse button presses in a way:
/// 1st pressed button determines the release result
/// release has result, when last button is released
/// press has result only for 1st button press
class MousePresses
{
public:
	enum Result
	{
		None = Qt::MouseButton::NoButton,
		Left = Qt::MouseButton::LeftButton,
		Right = Qt::MouseButton::RightButton,
		Middle = Qt::MouseButton::MiddleButton,
		Left2x,
		Right2x,
		Middle2x,
	};
private:
	Result result_to_2x( Result button ) const {
		switch ( button )
		{
		case Left: return Left2x;
		case Right: return Right2x;
		case Middle: return Middle2x;
		default: return None;
		}
	}
	Result result_for_button( Qt::MouseButton button ) const {
		switch ( button )
		{
		case Qt::MouseButton::LeftButton: return Left;
		case Qt::MouseButton::RightButton: return Right;
		case Qt::MouseButton::MiddleButton: return Middle;
		default: return None;
		}
	}
	DoubleClickTimer m_doubleClick;
	Result m_button{};
	Result m_buttonPrev{};
public:
#if 1
	Result press( const QMouseEvent *event ){
		const Result newbutton = result_for_button( event->button() );
		if( newbutton != None && event->button() == event->buttons() ){ // brand new press definitely; process even if release event was lost, for responsiveness
			m_button = newbutton;
			m_doubleClick.click();
			return ( m_doubleClick.fired() && m_button == m_buttonPrev )? result_to_2x( m_button ) : m_button;
		}
		return None;
	}
	Result release( const QMouseEvent *event ){
		if( m_button != None && event->buttons() == 0 ){ // last button is released
			const auto result = ( m_doubleClick.fired() && m_button == m_buttonPrev )? result_to_2x( m_button ) : m_button;
			m_buttonPrev = std::exchange( m_button, None );
			return result;
		}
		return None;
	}
#else
	Result press( const QMouseEvent *event ){
		const Result newbutton = result_for_button( event->button() );
		if( newbutton != None && m_button == None ){
			m_button = newbutton;
			m_doubleClick.click();
			return ( m_doubleClick.fired() && m_button == m_buttonPrev )? result_to_2x( m_button ) : m_button;
		}
		return None;
	}
	Result release( const QMouseEvent *event ){ // release event may be lost... ignore new press and release saved button for consistency
		if( m_button != None && event->buttons() == 0 ){ // last button is released
			const auto result = ( m_doubleClick.fired() && m_button == m_buttonPrev )? result_to_2x( m_button ) : m_button;
			m_buttonPrev = std::exchange( m_button, None );
			return result;
		}
		return None;
	}
#endif
};

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

#include "generic/callback.h"

#include <QTimer>

class IdleDraw
{
	Callback<void()> m_draw;
	QTimer m_timer;
public:
	IdleDraw( const Callback<void()>& draw ) : m_draw( draw ){
		m_timer.setSingleShot( true );
		m_timer.callOnTimeout( m_draw );
	}
	void queueDraw(){
		if( !m_timer.isActive() )
			m_timer.start();
	}
	typedef MemberCaller<IdleDraw, void(), &IdleDraw::queueDraw> QueueDrawCaller;

	void flush(){
		if ( m_timer.isActive() ) {
			m_timer.stop();
			m_draw();
		}
	}
};

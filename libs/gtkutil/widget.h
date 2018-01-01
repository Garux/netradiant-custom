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

#include <list>
#include "generic/callback.h"
#include "debugging/debugging.h"

#include <QWidget>
#include <QEvent>

class ToggleItem
{
	BoolExportCallback m_exportCallback;
	typedef std::list<BoolImportCallback> ImportCallbacks;
	ImportCallbacks m_importCallbacks;
public:
	ToggleItem( const BoolExportCallback& exportCallback ) : m_exportCallback( exportCallback ){
	}

	void update(){
		for ( ImportCallbacks::iterator i = m_importCallbacks.begin(); i != m_importCallbacks.end(); ++i )
		{
			m_exportCallback( *i );
		}
	}

	void addCallback( const BoolImportCallback& callback ){
		m_importCallbacks.push_back( callback );
		m_exportCallback( callback );
	}
	typedef MemberCaller<ToggleItem, void(const BoolImportCallback&), &ToggleItem::addCallback> AddCallbackCaller;
};

class ToggleShown : public QObject
{
	bool m_shownDeferred;
	QWidget* m_widget;
public:
	ToggleItem m_item;

	ToggleShown( const ToggleShown& other ) = delete; // NOT COPYABLE
	ToggleShown& operator=( const ToggleShown& other ) = delete; // NOT ASSIGNABLE

	ToggleShown( bool shown )
		: m_shownDeferred( shown ), m_widget( 0 ), m_item( ActiveCaller( *this ) ){
	}
	void update(){
		m_item.update();
	}
	bool active() const {
		return m_widget == nullptr
		     ? m_shownDeferred
		     : m_widget->isVisible();
	}
	void exportActive( const BoolImportCallback& importCallback ){
		importCallback( active() );
	}
	typedef MemberCaller<ToggleShown, void(const BoolImportCallback&), &ToggleShown::exportActive> ActiveCaller;
	void set( bool shown ){
		m_shownDeferred = shown;
		if ( m_widget != nullptr )
			m_widget->setVisible( shown );
	}
	void toggle(){
		m_widget->setVisible( m_shownDeferred = !m_widget->isVisible() );
	}
	typedef MemberCaller<ToggleShown, void(), &ToggleShown::toggle> ToggleCaller;
	void connect( QWidget* widget ){
		m_widget = widget;
		m_widget->setVisible( m_shownDeferred );
		m_widget->installEventFilter( this );
		QObject::connect( m_widget, &QObject::destroyed, [this](){ m_widget = nullptr; } );
		update();
	}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::Close ) {
			m_shownDeferred = false;
			/* HACK */ //.  because widget isVisible() at this point
			const auto tmp = std::exchange( m_widget, nullptr );
			update();
			m_widget = tmp;
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};

namespace{

void ToggleShown_importBool( ToggleShown& self, bool value ){
	self.set( value );
}
typedef ReferenceCaller<ToggleShown, void(bool), ToggleShown_importBool> ToggleShownImportBoolCaller;
void ToggleShown_exportBool( const ToggleShown& self, const BoolImportCallback& importer ){
	importer( self.active() );
}
typedef ConstReferenceCaller<ToggleShown, void(const BoolImportCallback&), ToggleShown_exportBool> ToggleShownExportBoolCaller;

}


inline void widget_queue_draw( QWidget& widget ){
	widget.update();
}
typedef ReferenceCaller<QWidget, void(), widget_queue_draw> WidgetQueueDrawCaller;


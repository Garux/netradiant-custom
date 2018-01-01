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


#include <QCompleter>
#include <QStringListModel>
#include <QObject>
#include <QLineEdit>
#include <QEvent>

#include "generic/static.h"
#include "signal/isignal.h"
#include "shaderlib.h"

#include "texwindow.h"

template<typename StringList>
class EntryCompletion : public QObject
{
	QCompleter* m_completer{};
	QStringListModel* m_model{};
	bool m_invalid = true;
public:
	~EntryCompletion(){
		delete m_completer;
	}

	void connect( QLineEdit* entry ){
		if ( m_completer == nullptr ) {
			m_completer = new QCompleter;
			m_model = new QStringListModel( m_completer );
			m_completer->setModel( m_model );
			m_completer->setCaseSensitivity( Qt::CaseSensitivity::CaseInsensitive );

			StringList().connect( InvalidateCaller( *this ) );
		}

		entry->setCompleter( m_completer );
		entry->installEventFilter( this );
	}

	void append( const char* string ){
		if( m_model->insertRow( m_model->rowCount() ) ){
			m_model->setData( m_model->index( m_model->rowCount() - 1 ), string );
		}
	}
	typedef MemberCaller<EntryCompletion, void(const char*), &EntryCompletion::append> AppendCaller;

	void fill(){
		StringList().forEach( AppendCaller( *this ) );
		m_invalid = false;
	}

	void clear(){
		m_model->removeRows( 0, m_model->rowCount() );
	}

	void update(){
		if( m_invalid ){
			clear();
			fill();
		}
	}

	void invalidate(){
		m_invalid = true;
	}
	typedef MemberCaller<EntryCompletion, void(), &EntryCompletion::invalidate> InvalidateCaller;
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::FocusIn ) {
			update();
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};

/* loaded ( shaders + textures ) */
class TextureNameList
{
public:
	void forEach( const ShaderNameCallback& callback ) const {
		for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
		{
			const IShader *shader = QERApp_ActiveShaders_IteratorCurrent();

			if ( shader_equal_prefix( shader->getName(), "textures/" ) ) {
				callback( shader->getName() + 9 );
			}
		}
	}
	void connect( const SignalHandler& update ) const {
		TextureBrowser_addActiveShadersChangedCallback( update );
	}
};

typedef Static< EntryCompletion<TextureNameList> > GlobalTextureEntryCompletion;

/* shaders + loaded textures */
class AllShadersNameList
{
public:
	void forEach( const ShaderNameCallback& callback ) const {
		GlobalShaderSystem().foreachShaderName( callback );
	}
	void connect( const SignalHandler& update ) const {
		TextureBrowser_addActiveShadersChangedCallback( update );
	}
};

typedef Static< EntryCompletion<AllShadersNameList> > GlobalAllShadersEntryCompletion;

/* shaders + may also include plain textures, loaded before first use */
class ShaderList
{
public:
	void forEach( const ShaderNameCallback& callback ) const {
		GlobalShaderSystem().foreachShaderName( callback );
	}
	void connect( const SignalHandler& update ) const {
		TextureBrowser_addShadersRealiseCallback( update );
	}
};

typedef Static< EntryCompletion<ShaderList> > GlobalShaderEntryCompletion;

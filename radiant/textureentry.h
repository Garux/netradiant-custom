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

#if !defined( INCLUDED_TEXTUREENTRY_H )
#define INCLUDED_TEXTUREENTRY_H


#include <gtk/gtk.h>

#include "gtkutil/idledraw.h"

#include "generic/static.h"
#include "signal/isignal.h"
#include "shaderlib.h"

#include "texwindow.h"

template<typename StringList>
class EntryCompletion
{
GtkListStore* m_store;
bool m_invalid;
public:
EntryCompletion() : m_store( 0 ), m_invalid( true ){
}

static gboolean focus_in( GtkEntry* entry, GdkEventFocus *event, EntryCompletion* self ){
	self->update();
	return FALSE;
}

void connect( GtkEntry* entry ){
	if ( m_store == 0 ) {
		m_store = gtk_list_store_new( 1, G_TYPE_STRING );

		fill();

		StringList().connect( InvalidateCaller( *this ) );
	}

	GtkEntryCompletion* completion = gtk_entry_completion_new();
	gtk_entry_set_completion( entry, completion );
	gtk_entry_completion_set_model( completion, GTK_TREE_MODEL( m_store ) );
	gtk_entry_completion_set_text_column( completion, 0 );
	g_signal_connect( G_OBJECT( entry ), "focus_in_event", G_CALLBACK( focus_in ), this );
}

void append( const char* string ){
	GtkTreeIter iter;
	gtk_list_store_append( m_store, &iter );
	gtk_list_store_set( m_store, &iter, 0, string, -1 );
}
typedef MemberCaller1<EntryCompletion, const char*, &EntryCompletion::append> AppendCaller;

void fill(){
	StringList().forEach( AppendCaller( *this ) );
	m_invalid = false;
}

void clear(){
	gtk_list_store_clear( m_store );
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
typedef MemberCaller<EntryCompletion, &EntryCompletion::invalidate> InvalidateCaller;
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


#endif

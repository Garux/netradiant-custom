/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

//
// Find/Replace textures dialogs
//
// Leonardo Zide (leo@lokigames.com)
//

#include "findtexturedialog.h"

#include "debugging/debugging.h"

#include "ishaders.h"

#include <QHBoxLayout>
#include <QFormLayout>
#include "gtkutil/lineedit.h"
#include <QLabel>
#include <QCheckBox>
#include <QEvent>
#include <QDialogButtonBox>
#include <QPushButton>

#include "gtkutil/guisettings.h"
#include "stream/stringstream.h"
#include "os/path.h"

#include "commands.h"
#include "dialog.h"
#include "select.h"
#include "textureentry.h"



class FindTextureDialog : public Dialog
{
public:
	static void setReplaceStr( const char* name );
	static void setFindStr( const char* name );
	static bool isOpen();
	static void show();
	typedef FreeCaller<void(), &FindTextureDialog::show> ShowCaller;
	static void updateTextures( const char* name );

	FindTextureDialog();
	virtual ~FindTextureDialog();
	void BuildDialog() override;

	void constructWindow( QWidget* parent ){
		Create( parent );
	}
	void destroyWindow(){
		Destroy();
	}


	bool m_bSelectedOnly;
	CopiedString m_strFind;
	CopiedString m_strReplace;
};

FindTextureDialog g_FindTextureDialog;
static bool g_bFindActive = true;

namespace
{
void FindTextureDialog_apply(){
	const auto find = StringStream<64>( "textures/", g_FindTextureDialog.m_strFind );
	const auto replace = StringStream<64>( "textures/", PathCleaned( g_FindTextureDialog.m_strReplace.c_str() ) );
	FindReplaceTextures( find, replace, g_FindTextureDialog.m_bSelectedOnly );
}

class FindActiveTracker : public QObject
{
	const bool m_findActive;
public:
	FindActiveTracker( bool findActive ) : m_findActive( findActive ){}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::FocusIn ) {
			g_bFindActive = m_findActive;
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};
FindActiveTracker s_find_focus_in( true );
FindActiveTracker s_replace_focus_in( false );

class : public QObject
{
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>( event );
			if( keyEvent->key() == Qt::Key_Tab ){
				event->accept();
			}
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
}
s_pressedKeysFilter;

}


// =============================================================================
// FindTextureDialog class

FindTextureDialog::FindTextureDialog() : m_bSelectedOnly( false ){
}

FindTextureDialog::~FindTextureDialog(){
}

void FindTextureDialog::BuildDialog(){
	GetWidget()->setWindowTitle( "Find / Replace Texture(s)" );

	GetWidget()->installEventFilter( &s_pressedKeysFilter );

	g_guiSettings.addWindow( GetWidget(), "TextureBrowser/FindReplace" );

	auto hbox = new QHBoxLayout( GetWidget() );
	auto form = new QFormLayout;
	hbox->addLayout( form );

	{
		auto entry = new LineEdit;
		form->addRow( "Find:", entry );
		AddDialogData( *entry, m_strFind );
		entry->installEventFilter( &s_find_focus_in );
		GlobalTextureEntryCompletion::instance().connect( entry );
	}
	{
		auto entry = new LineEdit;
		auto label = new QLabel( "Replace:" );
		form->addRow( label, entry );
		entry->setPlaceholderText( "Empty = search mode" );
		AddDialogData( *entry, m_strReplace );
		entry->installEventFilter( &s_replace_focus_in );
		GlobalTextureEntryCompletion::instance().connect( entry );
	}
	{
		auto check = new QCheckBox( "Within selected brushes only" );
		form->addWidget( check );
		AddDialogData( *check, m_bSelectedOnly );
	}

	{
		auto buttons = new QDialogButtonBox( Qt::Orientation::Vertical );
		hbox->addWidget( buttons );
		QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Apply ), &QPushButton::clicked, [](){
			g_FindTextureDialog.exportData();
			FindTextureDialog_apply();
		} );
		QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Close ), &QPushButton::clicked, [](){
			g_FindTextureDialog.HideDlg();
		} );
	}
}

void FindTextureDialog::updateTextures( const char* name ){
	if ( isOpen() ) {
		if ( g_bFindActive ) {
			setFindStr( name + strlen( "textures/" ) );
		}
		else
		{
			setReplaceStr( name + strlen( "textures/" ) );
		}
	}
}

bool FindTextureDialog::isOpen(){
	return g_FindTextureDialog.GetWidget()->isVisible();
}

void FindTextureDialog::setFindStr( const char* name ){
	g_FindTextureDialog.exportData();
	g_FindTextureDialog.m_strFind = name;
	g_FindTextureDialog.importData();
}

void FindTextureDialog::setReplaceStr( const char* name ){
	g_FindTextureDialog.exportData();
	g_FindTextureDialog.m_strReplace = name;
	g_FindTextureDialog.importData();
}

void FindTextureDialog::show(){
	g_FindTextureDialog.ShowDlg();
}


void FindTextureDialog_constructWindow( QWidget* main_window ){
	g_FindTextureDialog.constructWindow( main_window );
}

void FindTextureDialog_destroyWindow(){
	g_FindTextureDialog.destroyWindow();
}

bool FindTextureDialog_isOpen(){
	return g_FindTextureDialog.isOpen();
}

void FindTextureDialog_selectTexture( const char* name ){
	g_FindTextureDialog.updateTextures( name );
}

#include "preferencesystem.h"

void FindTextureDialog_Construct(){
	GlobalCommands_insert( "FindReplaceTextures", FindTextureDialog::ShowCaller() );
}

void FindTextureDialog_Destroy(){
}

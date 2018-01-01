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
// Floating dialog that contains a notebook with at least Entities and Group tabs
// I merged the 2 MS Windows dialogs in a single class
//
// Leonardo Zide (leo@lokigames.com)
//

#include "groupdialog.h"

#include "debugging/debugging.h"

#include <vector>

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "gtkutil/guisettings.h"
#include "gtkutil/widget.h"
#include "gtkutil/accelerator.h"
#include "entityinspector.h"
#include "gtkmisc.h"
#include "console.h"
#include "commands.h"


class GroupDlg
{
public:
	QTabWidget* m_pNotebook;
	QWidget* m_window;

	GroupDlg();
	void Create( QWidget* parent );

	void Show(){
		m_window->show();
		m_window->raise();
		m_window->activateWindow();
	}
	void Hide(){
		m_window->hide();
	}
};

namespace
{
GroupDlg g_GroupDlg;

std::vector<StringExportCallback> g_pages;
}

void GroupDialog_updatePageTitle( QWidget* window, int pageIndex ){
	if ( pageIndex >= 0 && pageIndex < static_cast<int>( g_pages.size() ) ) {
		const auto la = [window]( const char *title ){ window->setWindowTitle( title ); };
		g_pages[pageIndex]( ConstMemberCaller<decltype( la ), void(const char*), &decltype( la )::operator()>( la ) );
	}
}

GroupDlg::GroupDlg() : m_window( 0 ){
}

void GroupDlg::Create( QWidget* parent ){
	ASSERT_MESSAGE( m_window == 0, "dialog already created" );

	m_window = new QWidget( parent, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
	m_window->setWindowTitle( "Entities" );

//.	window_connect_focus_in_clear_focus_widget( m_window );

	g_guiSettings.addWindow( m_window, "GroupDlg/geometry", 444, 777 );

	{
		auto box = new QVBoxLayout( m_window );
		box->setContentsMargins( 0, 0, 0, 0 );
		m_pNotebook = new QTabWidget;
		m_pNotebook->setTabPosition( QTabWidget::TabPosition::South );
		m_pNotebook->setFocusPolicy( Qt::FocusPolicy::NoFocus );
		box->addWidget( m_pNotebook );

		QObject::connect( m_pNotebook, &QTabWidget::currentChanged, [window = m_window]( int index ){
			GroupDialog_updatePageTitle( window, index );
		} );
	}
}


QWidget* GroupDialog_addPage( const char* tabLabel, QWidget* widget, const StringExportCallback& title ){
	g_GroupDlg.m_pNotebook->addTab( widget, tabLabel );
	g_pages.push_back( title );
	return widget;
}


bool GroupDialog_isShown(){
	return g_GroupDlg.m_window->isVisible();
}
void GroupDialog_setShown( bool shown ){
	shown ? g_GroupDlg.Show() : g_GroupDlg.Hide();
}
void GroupDialog_ToggleShow(){
	GroupDialog_setShown( !GroupDialog_isShown() );
}

void GroupDialog_constructWindow( QWidget* main_window ){
	g_GroupDlg.Create( main_window );
}
void GroupDialog_destroyWindow(){
	ASSERT_NOTNULL( g_GroupDlg.m_window );
	delete g_GroupDlg.m_window;
	g_GroupDlg.m_window = 0;
}


QWidget* GroupDialog_getWindow(){
	return g_GroupDlg.m_window;
}
void GroupDialog_show(){
	g_GroupDlg.Show();
}

QWidget* GroupDialog_getPage(){
	return g_GroupDlg.m_pNotebook->currentWidget();
}

void GroupDialog_showPage( QWidget* page ){
	if ( GroupDialog_getPage() == page ) {
		GroupDialog_ToggleShow();
	}
	else
	{
		g_GroupDlg.m_pNotebook->setCurrentWidget( page );
		GroupDialog_show();
	}
}

void GroupDialog_updatePageTitle( QWidget* page ){
	if ( GroupDialog_getPage() == page ) {
		GroupDialog_updatePageTitle( g_GroupDlg.m_window, g_GroupDlg.m_pNotebook->currentIndex() );
	}
}


#include "preferencesystem.h"

void GroupDialog_Construct(){
}
void GroupDialog_Destroy(){
}

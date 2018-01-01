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

#include <QStyle>
#include <QApplication>
#include <QMenu>
#include <QActionGroup>

#include "preferences.h"
#include "mainframe.h"
#include "preferencesystem.h"
#include "stringio.h"


enum class ETheme{
	Default = 0,
	Dark,
	Darker
};

static QActionGroup *s_theme_group;
static ETheme s_theme = ETheme::Dark;

void theme_set( ETheme theme ){
	s_theme = theme;
#ifdef WIN32
//	QSettings settings( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat );
//	if( settings.value( "AppsUseLightTheme" ) == 0 )
#endif
	static struct
	{
		bool is1stThemeApplication = true; // guard to not apply possibly wrong defaults while app is started with Default theme
		const QPalette palette = qApp->palette();
		const QString style = qApp->style()->objectName();
	}
	defaults;

	const char* sheet = R"(
	QToolTip {
		color: #ffffff;
		background-color: #4D4F4B;
		border: 1px solid white;
	}

	QScrollBar:vertical {
		background: rgb( 73, 74, 71 );
		border: 0px solid grey;
		width: 7px;
		margin: 0px 0px 0px 0px;
	}
	QScrollBar::handle:vertical {
		border: 1px solid gray;
		background: rgb( 111, 105, 100 );
		min-height: 20px;
	}
	QScrollBar::add-line:vertical {
		border: 0px solid grey;
		background: #32CC99;
		height: 0px;
		subcontrol-position: bottom;
		subcontrol-origin: margin;
	}
	QScrollBar::sub-line:vertical {
		border: 0px solid grey;
		background: #32CC99;
		height: 0px;
		subcontrol-position: top;
		subcontrol-origin: margin;
	}

	QScrollBar:horizontal {
		background: rgb( 73, 74, 71 );
		border: 0px solid grey;
		height: 7px;
		margin: 0px 0px 0px 0px;
	}
	QScrollBar::handle:horizontal {
		border: 1px solid gray;
		background: rgb( 111, 105, 100 );
		min-width: 20px;
	}
	QScrollBar::add-line:horizontal {
		border: 0px solid grey;
		background: #32CC99;
		width: 0px;
		subcontrol-position: right;
		subcontrol-origin: margin;
	}
	QScrollBar::sub-line:horizontal {
		border: 0px solid grey;
		background: #32CC99;
		width: 0px;
		subcontrol-position: left;
		subcontrol-origin: margin;
	}

	QScrollBar::handle:hover {
		background: rgb( 250, 203, 129 );
	}

	QToolBar::separator:horizontal {
		width: 1px;
		margin: 3px 1px;
		background-color: #aaaaaa;
	}
	QToolBar::separator:vertical {
		height: 1px;
		margin: 1px 3px;
		background-color: #aaaaaa;
	}
	QToolButton {
		padding: 0;
		margin: 0;
	}

	QMenu::separator {
		background: rgb( 93, 94, 91 );
		height: 1px;
		margin-top: 3px;
		margin-bottom: 3px;
		margin-left: 5px;
		margin-right: 7px;
	}
	)";

	if( theme == ETheme::Default ){
		if( !defaults.is1stThemeApplication ){
			qApp->setPalette( defaults.palette );
			qApp->setStyleSheet( QString() );
			qApp->setStyle( defaults.style );
		}
	}
	else if( theme == ETheme::Dark ){
		qApp->setStyle( "Fusion" );
		QPalette darkPalette;
		const QColor darkColor = QColor( 83, 84, 81 );
		const QColor disabledColor = QColor( 127, 127, 127 );
		const QColor baseColor( 46, 52, 54 );
		darkPalette.setColor( QPalette::Window, darkColor );
		darkPalette.setColor( QPalette::WindowText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::WindowText, disabledColor );
		darkPalette.setColor( QPalette::Base, baseColor );
		darkPalette.setColor( QPalette::AlternateBase, baseColor.darker( 130 ) );
		darkPalette.setColor( QPalette::ToolTipBase, Qt::white );
		darkPalette.setColor( QPalette::ToolTipText, Qt::white );
		darkPalette.setColor( QPalette::Text, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::Text, disabledColor );
		darkPalette.setColor( QPalette::Button, darkColor.lighter( 130 ) );
		darkPalette.setColor( QPalette::ButtonText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledColor.lighter( 130 ) );
		darkPalette.setColor( QPalette::BrightText, Qt::red );
		darkPalette.setColor( QPalette::Link, QColor( 42, 130, 218 ) );

		darkPalette.setColor( QPalette::Highlight, QColor( 250, 203, 129 ) );
		darkPalette.setColor( QPalette::Inactive, QPalette::Highlight, disabledColor );
		darkPalette.setColor( QPalette::HighlightedText, Qt::black );
		darkPalette.setColor( QPalette::Disabled, QPalette::HighlightedText, disabledColor );

		qApp->setPalette( darkPalette );

		qApp->setStyleSheet( sheet );
	}
	else if( theme == ETheme::Darker ){
		qApp->setStyle( "Fusion" );
		QPalette darkPalette;
		const QColor darkColor = QColor( 45, 45, 45 );
		const QColor disabledColor = QColor( 127, 127, 127 );
		const QColor baseColor( 18, 18, 18 );
		darkPalette.setColor( QPalette::Window, darkColor );
		darkPalette.setColor( QPalette::WindowText, Qt::white );
		darkPalette.setColor( QPalette::Base, baseColor );
		darkPalette.setColor( QPalette::AlternateBase, baseColor.darker( 130 ) );
		darkPalette.setColor( QPalette::ToolTipBase, Qt::white );
		darkPalette.setColor( QPalette::ToolTipText, Qt::white );
		darkPalette.setColor( QPalette::Text, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::Text, disabledColor );
		darkPalette.setColor( QPalette::Button, darkColor );
		darkPalette.setColor( QPalette::ButtonText, Qt::white );
		darkPalette.setColor( QPalette::Disabled, QPalette::ButtonText, disabledColor );
		darkPalette.setColor( QPalette::BrightText, Qt::red );
		darkPalette.setColor( QPalette::Link, QColor( 42, 130, 218 ) );

		darkPalette.setColor( QPalette::Highlight, QColor( 42, 130, 218 ) );
		darkPalette.setColor( QPalette::HighlightedText, Qt::black );
		darkPalette.setColor( QPalette::Disabled, QPalette::HighlightedText, disabledColor );

		qApp->setPalette( darkPalette );

		qApp->setStyleSheet( sheet );
	}

	defaults.is1stThemeApplication = false;
}

void theme_contruct_menu( class QMenu *menu ){
	auto *m = menu->addMenu( "GUI Theme" );
	m->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
	auto *group = s_theme_group = new QActionGroup( m );
	{
		auto *a = m->addAction( "Default" );
		a->setCheckable( true );
		group->addAction( a );
	}
	{
		auto *a = m->addAction( "Dark" );
		a->setCheckable( true );
		group->addAction( a );
	}
	{
		auto *a = m->addAction( "Darker" );
		a->setCheckable( true );
		group->addAction( a );
	}

	QObject::connect( s_theme_group, &QActionGroup::triggered, []( QAction *action ){
		theme_set( static_cast<ETheme>( s_theme_group->actions().indexOf( action ) ) );
	} );
}

void ThemeImport( int value ){
	s_theme = static_cast<ETheme>( value );
	if( s_theme_group != nullptr && 0 <= value && value < s_theme_group->actions().size() ){
		s_theme_group->actions().at( value )->setChecked( true );
	}
}
typedef FreeCaller<void(int), ThemeImport> ThemeImportCaller;

void ThemeExport( const IntImportCallback& importer ){
	importer( static_cast<int>( s_theme ) );
}
typedef FreeCaller<void(const IntImportCallback&), ThemeExport> ThemeExportCaller;


void theme_contruct(){
	GlobalPreferenceSystem().registerPreference( "GUITheme", makeIntStringImportCallback( ThemeImportCaller() ), makeIntStringExportCallback( ThemeExportCaller() ) );
	theme_set( s_theme ); // set theme here, not in importer, so it's set on the very 1st start too (when there is no preference to load)
}

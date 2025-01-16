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

#include "guisettings.h"

#include <optional>
#include <QSettings>
#include <QCoreApplication>
#include <QWidget>
#include <QMainWindow>
#include <QSplitter>

class GuiSetting
{
protected:
	const char * const m_path;
private:
	inline static std::optional<QSettings> m_qsettings{};
public:
	inline static QSettings& qsettings(){
		return m_qsettings.has_value()
			 ? m_qsettings.value()
		     : m_qsettings.emplace( QCoreApplication::organizationName(), QCoreApplication::applicationName() );
	}
	GuiSetting( const char* path ) : m_path( path ){}
	virtual ~GuiSetting() = default;
	virtual void save() const = 0;
};

class WindowSetting : public GuiSetting, QObject
{
	QWidget * const m_window;
	bool m_wantSave{};
public:
	WindowSetting( QWidget *window, const char *path, int w, int h, int x, int y ) : GuiSetting( path ), m_window( window ){
		if( const QByteArray geometry = qsettings().value( m_path, QByteArray() ).toByteArray(); !geometry.isEmpty() ){
			m_window->restoreGeometry( geometry );
			m_wantSave = true;
		}
		else{
			if( w > 0 && h > 0 )
				m_window->resize( w, h );
			if( x > 0 && y > 0 ){
				m_window->move( x, y );
				m_wantSave = true;
			}
			else{
				m_window->installEventFilter( this );
				if( m_window->isVisible() )
					m_wantSave = true;
			}
		}
	}
	void save() const override {
		// w/o restoreGeometry() or move(): only save, if window has been shown, so default pos has been applied
		if( m_wantSave )
			qsettings().setValue( m_path, m_window->saveGeometry() );
	}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		// deal with the case w/o restoreGeometry() or move():
		// when window is closed via VM means (gui cross, alt+f4), pos is defaulted on next show() (np with hide() )
		// upd: problem with hide() too, defaulted size too
		if( event->type() == QEvent::Show || event->type() == QEvent::Close ) {
			m_window->setAttribute( Qt::WidgetAttribute::WA_Moved );
			m_window->setAttribute( Qt::WidgetAttribute::WA_Resized );
			m_window->removeEventFilter( this );
			m_wantSave = true; // also track 'was shown' property
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
};

class MainWindowSetting : public GuiSetting
{
	QMainWindow * const m_window;
public:
	MainWindowSetting( QMainWindow *window, const char *path ) : GuiSetting( path ), m_window( window ){
		if( const QByteArray state = qsettings().value( m_path, QByteArray() ).toByteArray(); !state.isEmpty() )
			m_window->restoreState( state );
		else
			m_window->showMaximized();
	}
	void save() const override {
		qsettings().setValue( m_path, m_window->saveState() );
	}
};

class SplitterSetting : public GuiSetting
{
	QSplitter * const m_splitter;
public:
	SplitterSetting( QSplitter *splitter, const char *path, const QList<int> &sizes ) : GuiSetting( path ), m_splitter( splitter ){
		if( const QByteArray state = qsettings().value( m_path, QByteArray() ).toByteArray(); !state.isEmpty() )
			m_splitter->restoreState( state );
		else
			m_splitter->setSizes( sizes );
	}
	void save() const override {
		qsettings().setValue( m_path, m_splitter->saveState() );
	}
};


GuiSettings::~GuiSettings(){
	for( auto setting : m_settings )
		delete setting;
}
void GuiSettings::save(){
	for( const auto setting : m_settings )
		setting->save();
}
void GuiSettings::addWindow( QWidget *window, const char *path, int w /* = 0 */, int h /* = 0 */, int x /* = 0 */, int y /* = 0 */ ){
	m_settings.push_back( new WindowSetting( window, path, w, h, x, y ) );
}
void GuiSettings::addMainWindow( QMainWindow *window, const char *path ){
	m_settings.push_back( new MainWindowSetting( window, path ) );
}
void GuiSettings::addSplitter( QSplitter *splitter, const char *path, const QList<int> &sizes ){
	m_settings.push_back( new SplitterSetting( splitter, path, sizes ) );
}

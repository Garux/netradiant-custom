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

#include "console.h"

#include <ctime>

#include "gtkutil/accelerator.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "stream/stringstream.h"

#include "version.h"
#include "aboutmsg.h"
#include "gtkmisc.h"
#include "mainframe.h"

#include <QPlainTextEdit>
#include <QContextMenuEvent>

#ifndef WIN32
#include <unistd.h> // write()
#endif

// handle to the console log file
namespace
{
FILE* g_hLogFile;
}

bool g_Console_enableLogging = true;

// called whenever we need to open/close/check the console log file
void Sys_LogFile( bool enable ){
	if ( enable && !g_hLogFile ) {
		// settings say we should be logging and we don't have a log file .. so create it
		if ( !SettingsPath_get()[0] ) {
			return; // cannot open a log file yet
		}
		// open a file to log the console (if user prefs say so)
		// the file handle is g_hLogFile
		// the log file is erased
		const auto name = StringStream( SettingsPath_get(), "radiant.log" );
		g_hLogFile = fopen( name, "w" );
		if ( g_hLogFile != 0 ) {
			globalOutputStream() << "Started logging to " << name << '\n';
			time_t localtime;
			time( &localtime );
			globalOutputStream() << "Today is: " << ctime( &localtime )
			                     << "This is NetRadiant '" RADIANT_VERSION "' compiled " __DATE__ "\n" RADIANT_ABOUTMSG "\n";
		}
		else{
			qt_MessageBox( 0, "Failed to create log file, check write permissions in Radiant directory.\n",
			                "Console logging", EMessageBoxType::Error );
		}
	}
	else if ( !enable && g_hLogFile != 0 ) {
		// settings say we should not be logging but still we have an active logfile .. close it
		time_t localtime;
		time( &localtime );
		globalOutputStream() << "Closing log file at " << ctime( &localtime ) << '\n';
		fclose( g_hLogFile );
		g_hLogFile = 0;
	}
}

QPlainTextEdit* g_console = 0;

class QPlainTextEdit_console : public QPlainTextEdit
{
protected:
	void contextMenuEvent( QContextMenuEvent *event ) override {
		QMenu *menu = createStandardContextMenu();
		QAction *action = menu->addAction( "Clear" );
		connect( action, &QAction::triggered, this, &QPlainTextEdit::clear );
		menu->exec( event->globalPos() );
		delete menu;
	}
};


QWidget* Console_constructWindow(){
	QPlainTextEdit *text = new QPlainTextEdit_console();
	text->setReadOnly( true );
	text->setUndoRedoEnabled( false );
	text->setMinimumHeight( 10 );
	text->setFocusPolicy( Qt::FocusPolicy::NoFocus );

	{
		g_console = text;

		//globalExtendedASCIICharacterSet().print();

		text->connect( text, &QObject::destroyed, [](){ g_console = nullptr; } );
	}

	return text;
}

//#pragma GCC push_options
//#pragma GCC optimize ("O0")

class GtkTextBufferOutputStream : public TextOutputStream
{
	QPlainTextEdit* textBuffer;
public:
	GtkTextBufferOutputStream( QPlainTextEdit* textBuffer ) : textBuffer( textBuffer ) {
	}
	std::size_t
#ifdef __GNUC__
//__attribute__((optimize("O0")))
#endif
	write( const char* buffer, std::size_t length ){
		textBuffer->insertPlainText( QString::fromLatin1( buffer, length ) );
		return length;
	}
};

//#pragma GCC pop_options

std::size_t Sys_Print( int level, const char* buf, std::size_t length ){
	const bool contains_newline = std::find( buf, buf + length, '\n' ) != buf + length;

	if ( level == SYS_ERR ) {
		Sys_LogFile( true );
	}

	if ( g_hLogFile != 0 ) {
		fwrite( buf, 1, length, g_hLogFile );
		if ( contains_newline ) {
			fflush( g_hLogFile );
		}
	}

	if ( level != SYS_NOCON ) {
#ifndef WIN32
		{  // on linux/macos log also to terminal
			switch ( level )
			{
			case SYS_WRN:
			case SYS_ERR:
				write( 2, buf, length );
				break;
			case SYS_STD:
			case SYS_VRB:
			default:
				write( 1, buf, length );
				break;
			}
		}
#endif

		if ( g_console != 0 ) {
			g_console->moveCursor( QTextCursor::End ); // must go before setCurrentCharFormat() & insertPlainText()

			{
				QTextCharFormat format = g_console->currentCharFormat();
				switch ( level )
				{
				case SYS_WRN:
					format.setForeground( QColor( 255, 127, 0 ) );
					break;
				case SYS_ERR:
					format.setForeground( QColor( 255, 0, 0 ) );
					break;
				case SYS_STD:
				case SYS_VRB:
				default:
					format.clearForeground();
					break;
				}
				g_console->setCurrentCharFormat( format );
			}

			{
				GtkTextBufferOutputStream textBuffer( g_console );
				textBuffer << StringRange( buf, length );
			}

 			if ( contains_newline ) {
				g_console->ensureCursorVisible();

				// update console widget immediately if we're doing something time-consuming
				if ( !ScreenUpdates_Enabled() && g_console->isVisible() ) {
					ScreenUpdates_process();
				}
			}
 		}
	}
	return length;
}


template<int level>
class SysPrintStream : public TextOutputStream
{
public:
	std::size_t write( const char* buffer, std::size_t length ){
		return Sys_Print( level, buffer, length );
	}
};

TextOutputStream& getSysPrintOutputStream(){
	static SysPrintStream<SYS_STD> stream;
	return stream;
}

TextOutputStream& getSysPrintWarningStream(){
	static SysPrintStream<SYS_WRN> stream;
	return stream;
}

TextOutputStream& getSysPrintErrorStream(){
	static SysPrintStream<SYS_ERR> stream;
	return stream;
}

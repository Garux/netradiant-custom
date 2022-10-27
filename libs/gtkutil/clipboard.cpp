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

#include "clipboard.h"

#include "stream/memstream.h"
#include "stream/textstream.h"


/// \file
/// \brief Platform-independent clipboard support.

#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>

constexpr char c_clipboard_format[] = "RadiantClippings";

void clipboard_copy( ClipboardCopyFunc copy ){
	BufferOutputStream ostream;
	copy( ostream );

	const std::size_t length = ostream.size();
	QByteArray array( sizeof( std::size_t ) + length, Qt::Initialization::Uninitialized );
	*reinterpret_cast<std::size_t*>( array.data() ) = length;
	memcpy( array.data() + sizeof( std::size_t ), ostream.data(), length );

	auto mimedata = new QMimeData;
	mimedata->setData( c_clipboard_format, array );
	QGuiApplication::clipboard()->setMimeData( mimedata );
}

void clipboard_paste( ClipboardPasteFunc paste ){
	if( const auto mimedata = QGuiApplication::clipboard()->mimeData() ){
		if( const auto array = mimedata->data( c_clipboard_format ); !array.isEmpty() ){
			/* 32 & 64 bit radiants use the same clipboard signature 👀
			   handle varying sizeof( std::size_t ), also try to be safe
			   note: GtkR1.4 uses xml map format in clipboard */
			if( const std::size_t length = *reinterpret_cast<const std::size_t*>( array.data() ); size_t( array.size() ) == length + sizeof( std::size_t ) ){
				BufferInputStream istream( array.data() + sizeof( std::size_t ), length );
				paste( istream );
			} else
			if( const std::size_t length = *reinterpret_cast<const std::uint32_t*>( array.data() ); size_t( array.size() ) == length + sizeof( std::uint32_t ) ){
				BufferInputStream istream( array.data() + sizeof( std::uint32_t ), length );
				paste( istream );
			} else
				globalWarningStream() << "Unrecognized clipboard contents\n";
		}
	}
}

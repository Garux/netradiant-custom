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

#include "image.h"

#include "os/file.h"
#include "os/path.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include <QDir>


namespace
{
CopiedString g_bitmapsPath;
}

void BitmapsPath_set( const char* path ){
	g_bitmapsPath = path;
}

/* generate in settings path, app path may have no write permission */
void Bitmaps_generateLight( const char *appPath, const char *settingsPath ){
	const char *fromto[][2] = { { "bitmaps/", "bitmaps_light/" }, { "plugins/bitmaps/", "plugins/bitmaps/" } };
	for( const auto [ f, t ] : fromto )
	{
		QDir from( QString( appPath ) + f );
		QDir to( QString( settingsPath ) + t );
		for( auto *d : { &from, &to } ){
			d->setNameFilters( QStringList() << "*.svg" << "*.png" << "*.ico" << "*.theme" );
			d->setFilter( QDir::Filter::Files );
		}

		if( to.count() < from.count() ){
			to.mkpath( to.absolutePath() );
			for( const QFileInfo& fileinfo : from.entryInfoList() )
			{
				QFile file( fileinfo.absoluteFilePath() );
				if( file.open( QIODevice::OpenModeFlag::ReadOnly ) ){
					QByteArray data( file.readAll() );
					if( fileinfo.suffix() == "svg" )
						data.replace( "#C0C0C0", "#575757" );
					QFile outfile( to.absolutePath() + '/' + fileinfo.fileName() );
					if( outfile.open( QIODevice::OpenModeFlag::WriteOnly ) )
						outfile.write( data );
				}
			}
		}
	}
}

QPixmap new_local_image( const char* filename ){
	StringOutputStream fullpath( 256 );

	for( const auto *ext : { ".svg", ".png" } )
		if( file_exists( fullpath( g_bitmapsPath, PathExtensionless( filename ), ext ) ) )
			return QPixmap( fullpath.c_str() );

	return {};
}

QIcon new_local_icon( const char* filename ){
	if( QString name( CopiedString( PathExtensionless( filename ) ).c_str() ); QIcon::hasThemeIcon( name ) )
		return QIcon::fromTheme( name );

	StringOutputStream fullpath( 256 );

	for( const auto *ext : { ".svg", ".png", ".ico" } )
		if( file_exists( fullpath( g_bitmapsPath, PathExtensionless( filename ), ext ) ) )
			return QIcon( fullpath.c_str() );

	return {};
}

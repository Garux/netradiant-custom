/*
   BobToolz plugin for GtkRadiant
   Copyright (C) 2001 Gordon Biggans

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "lists.h"

#include "misc.h"

bool LoadExclusionList( const char* filename, std::vector<CopiedString>& exclusionList ){
	FILE* eFile = fopen( filename, "r" );
	if ( eFile ) {
		char buffer[256];
		while ( !feof( eFile ) )
		{
			memset( buffer, 0, sizeof( buffer ) );
			fscanf( eFile, "%s\n", buffer );

			if ( !string_empty( buffer ) ) {
				exclusionList.push_back( buffer );
			}
		}

		fclose( eFile );

		return true;
	}

	globalErrorStream() << "Failed To Load Exclusion List: " << filename << '\n';
	return false;
}

QStringList LoadListStore( char* filename ){
	QStringList list;
	FILE* eFile = fopen( filename, "r" );
	if ( eFile ) {
		char buffer[256];
		while ( !feof( eFile ) )
		{
			memset( buffer, 0, 256 );
			fscanf( eFile, "%s\n", buffer );

			if ( strlen( buffer ) > 0 ) {
				list.append( buffer );
			}
		}

		fclose( eFile );

		return list;
	}

	globalErrorStream() << "Failed To Load GList: " << filename << '\n';
	return list;
}

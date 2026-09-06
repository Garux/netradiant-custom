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

/// \file
/// \brief OS directory-listing object.

#include <vector>

class QDir;

class Directory {

	private:
		QDir* dir;
		size_t entry_idx = 0;
		std::vector<const char*> entries;

	public:
		Directory(const char* name);
		bool good();
		void close();
		const char* read_and_increment();
};

inline bool directory_good(Directory* directory) {
	if(directory) {
		return directory->good();
	}
	return false;
}

inline Directory* directory_open(const char* name){
	return new Directory(name);
}

inline void directory_close(Directory* directory){
	if(directory) {
		directory->close();
	}
}

inline const char* directory_read_and_increment(Directory* directory) {
	if(directory) {
		return directory->read_and_increment();
	}
	return nullptr;
}

template<typename Functor>
void Directory_forEach( const char* path, const Functor& functor ){
	Directory* dir = directory_open( path );

	if ( directory_good( dir ) ) {
		for (;; )
		{
			const char* name = directory_read_and_increment( dir );
			if ( name == 0 ) {
				break;
			}

			functor( name );
		}

		directory_close( dir );
	}
}

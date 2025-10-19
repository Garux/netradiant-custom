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

#include "filechooser.h"

#include "ifiletypes.h"

#include <list>
#include <vector>
#include <QFileDialog>

#include "string/string.h"
#include "stream/stringstream.h"
#include "os/path.h"
#include "messagebox.h"


struct filetype_pair_t
{
	filetype_pair_t()
		: m_moduleName( "" ){
	}
	filetype_pair_t( const char* moduleName, filetype_t type )
		: m_moduleName( moduleName ), m_type( type ){
	}
	const char* m_moduleName;
	filetype_t m_type;
};

class FileTypeList : public IFileTypeList
{
	struct filetype_copy_t
	{
		filetype_copy_t( const filetype_pair_t& other )
			: m_moduleName( other.m_moduleName ), m_name( other.m_type.name ), m_pattern( other.m_type.pattern ){
		}
		CopiedString m_moduleName;
		CopiedString m_name;
		CopiedString m_pattern;
	};

	std::list<filetype_copy_t> m_types;
public:
	auto begin() const {
		return m_types.cbegin();
	}
	auto end() const {
		return m_types.cend();
	}
	std::size_t size() const {
		return m_types.size();
	}

	void addType( const char* moduleName, filetype_t type ) override {
		m_types.push_back( filetype_pair_t( moduleName, type ) );
	}
};


class GTKMasks
{
public:
	std::vector<CopiedString> m_filters;
	std::vector<CopiedString> m_masks;

	GTKMasks( const FileTypeList& types ){
		m_masks.reserve( types.size() );
		m_filters.reserve( types.size() );
		for ( const auto& type : types )
		{
			m_masks.push_back( StringStream<64>( type.m_name, " (", type.m_pattern, ')' ).c_str() );
			m_filters.push_back( type.m_pattern );
		}
	}
	GTKMasks( const char *patterns ){
		while( ( patterns = strstr( patterns, "*." ) ) )
		{
			const char *end = patterns + 1;
			while( std::isalnum( *++end ) ){}
			m_filters.push_back( StringRange( patterns, end ) );
			patterns = end;
		}
	}
};

static QByteArray g_file_dialog_file;

const char* file_dialog( QWidget* parent, bool open, const char* title, const char* path, const char* pattern, bool want_load, bool want_import, bool want_save ){
	if ( pattern == 0 ) {
		pattern = "*";
	}

	FileTypeList typelist;
	GlobalFiletypes().getTypeList( pattern, &typelist, want_load, want_import, want_save );

	const auto masks = typelist.size()? GTKMasks( typelist ) : GTKMasks( pattern ); // pattern is moduleType or explicit patterns

	if ( path != 0 && !string_empty( path ) ) {
		ASSERT_MESSAGE( path_is_absolute( path ), "file_dialog_show: path not absolute: " << Quoted( path ) );
	}

	// we should add all important paths as shortcut folder...
	// gtk_file_chooser_add_shortcut_folder( GTK_FILE_CHOOSER( dialog ), "/tmp/", nullptr );

	QString filter;

	if( typelist.size() ){ // pattern is moduleType
		if ( open && masks.m_filters.size() > 1 ){ // e.g.: All supported formats ( *.map *.reg)
			filter += "All supported formats (";
			for ( const auto& f : masks.m_filters )
			{
				filter += ' ';
				filter += f.c_str();
			}
			filter += ')';
		}

		for ( const auto& mask : masks.m_masks ) // e.g.: quake3 maps (*.map);;quake3 region (*.reg)
		{
			if( !filter.isEmpty() )
				filter += ";;";
			filter += mask.c_str();
		}
	}
	else{ // explicit patterns
		filter = pattern;
	}
	// this handles backslashes as input and returns forwardly slashed path
	// input path may be either folder or file
	// only existing file path may be chosen for open; overwriting is prompted on save
	QString selectedFilter;
	g_file_dialog_file = open
		? QFileDialog::getOpenFileName( parent, title, path, filter, &selectedFilter ).toLatin1()
		: QFileDialog::getSaveFileName( parent, title, path, filter, &selectedFilter ).toLatin1();

	/* validate extension: it is possible to pick existing file, not respecting the filter...
	   some dialog implementations may return file name w/o autoappended extension too */
	if( !g_file_dialog_file.isEmpty() && !string_equal( pattern, "*" ) ){
		const char* extension = path_get_extension( g_file_dialog_file.constData() );
		if( !string_empty( extension ) ){ // validate it
			const auto check = [extension]( const CopiedString& filter ){ return extension_equal( extension, path_get_extension( filter.c_str() ) ); };
			if( !std::ranges::any_of( masks.m_filters, check ) ) {
				qt_MessageBox( parent, StringStream<64>( Quoted( extension ), " is unsupported file type for requested operation\n" ), extension, EMessageBoxType::Error );
				g_file_dialog_file.clear();
			}
		}
		else{ // add extension
			selectedFilter = selectedFilter.right( selectedFilter.size() - ( selectedFilter.indexOf( "*." ) + 1 ) );
			selectedFilter = selectedFilter.left( selectedFilter.indexOf( ')' ) );
			selectedFilter = selectedFilter.left( selectedFilter.indexOf( ' ' ) ); // left() is preferred over truncate(), since it returns entire string on negative input
			g_file_dialog_file.append( selectedFilter.toLatin1() );
		}
	}

	// don't return an empty filename
	return g_file_dialog_file.isEmpty()
		? nullptr
		: g_file_dialog_file.constData();
}

QString dir_dialog( QWidget *parent, const QString& path ){
	return QFileDialog::getExistingDirectory( parent, {}, path );
}

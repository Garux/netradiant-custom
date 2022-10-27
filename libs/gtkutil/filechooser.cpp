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

	typedef std::list<filetype_copy_t> Types;
	Types m_types;
public:

	typedef Types::const_iterator const_iterator;
	const_iterator begin() const {
		return m_types.begin();
	}
	const_iterator end() const {
		return m_types.end();
	}

	std::size_t size() const {
		return m_types.size();
	}

	void addType( const char* moduleName, filetype_t type ){
		m_types.push_back( filetype_pair_t( moduleName, type ) );
	}
};


class GTKMasks
{
	const FileTypeList& m_types;
public:
	std::vector<CopiedString> m_filters;
	std::vector<CopiedString> m_masks;

	GTKMasks( const FileTypeList& types ) : m_types( types ){
		m_masks.reserve( m_types.size() );
		for ( const auto& type : types )
		{
			std::size_t len = strlen( type.m_name.c_str() ) + strlen( type.m_pattern.c_str() ) + 3;
			StringOutputStream buffer( len + 1 ); // length + null char

			buffer << type.m_name << " (" << type.m_pattern << ")";

			m_masks.push_back( buffer.c_str() );
		}

		m_filters.reserve( m_types.size() );
		for ( const auto& type : types )
		{
			m_filters.push_back( type.m_pattern );
		}
	}

	filetype_pair_t GetTypeForGTKMask( const char *mask ) const {
		std::vector<CopiedString>::const_iterator j = m_masks.begin();
		for ( FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i, ++j )
		{
			if ( string_equal( ( *j ).c_str(), mask ) ) {
				return filetype_pair_t( ( *i ).m_moduleName.c_str(), filetype_t( ( *i ).m_name.c_str(), ( *i ).m_pattern.c_str() ) );
			}
		}
		return filetype_pair_t();
	}

};

static QByteArray g_file_dialog_file;

const char* file_dialog( QWidget* parent, bool open, const char* title, const char* path, const char* pattern, bool want_load, bool want_import, bool want_save ){
	if ( pattern == 0 ) {
		pattern = "*";
	}

	FileTypeList typelist;
	GlobalFiletypes().getTypeList( pattern, &typelist, want_load, want_import, want_save );

	const GTKMasks masks( typelist );

	if ( path != 0 && !string_empty( path ) ) {
		ASSERT_MESSAGE( path_is_absolute( path ), "file_dialog_show: path not absolute: " << makeQuoted( path ) );
	}

	// we should add all important paths as shortcut folder...
	// gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog), "/tmp/", NULL);

	QString filter;

	if ( open && masks.m_filters.size() > 1 ){
		filter += "All supported formats (";
		for ( const auto& f : masks.m_filters )
		{
			filter += ' ';
			filter += f.c_str();
		}
		filter += ")";
	}

	for ( const auto& mask : masks.m_masks )
	{
		if( !filter.isEmpty() )
			filter += ";;";
		filter += mask.c_str();
	}
	// this handles backslashes as input and returns forwardly slashed path
	// input path may be either folder or file
	// only existing file path may be chosen for open; overwriting is prompted on save
	g_file_dialog_file = open
		? QFileDialog::getOpenFileName( parent, title, path, filter ).toLatin1()
		: QFileDialog::getSaveFileName( parent, title, path, filter ).toLatin1();

	/* validate extension: it is possible pick existing file, not respecting the filter... */
	if( !g_file_dialog_file.isEmpty() && !string_equal( pattern, "*" ) ){
		const char* extension = path_get_extension( g_file_dialog_file.constData() );
		if( !string_empty( extension ) )
			for( const auto& f : masks.m_filters )
				if( extension_equal( extension, path_get_extension( f.c_str() ) ) )
					goto extension_validated;
		qt_MessageBox( parent, StringOutputStream( 256 )( makeQuoted( extension ), " is unsupported file type for requested operation\n" ), extension, EMessageBoxType::Error );
		g_file_dialog_file.clear();
	}
extension_validated:

	// don't return an empty filename
	return g_file_dialog_file.isEmpty()
		? nullptr
		: g_file_dialog_file.constData();
}

QString dir_dialog( QWidget *parent, const QString& path ){
	return QFileDialog::getExistingDirectory( parent, {}, path );
}

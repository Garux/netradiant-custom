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

#include "dialog.h"

#include <QGridLayout>
#include <QWidget>
#include <QLabel>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QLineEdit>
#include "gtkutil/image.h"


RadioHBox RadioHBox_new( StringArrayRange names ){
	auto *hbox = new QHBoxLayout;
	auto *group = new QButtonGroup( hbox );

	for ( size_t i = 0; i < names.size(); ++i )
	{
		auto *button = new QRadioButton( names[i] );
		group->addButton( button, i ); // set ids 0+, default ones are negative
		hbox->addWidget( button );
	}

	return RadioHBox( hbox, group );
}


PathEntry PathEntry_new(){
	auto *entry = new QLineEdit;
	auto *button = entry->addAction( new_local_icon( "ellipsis.png" ), QLineEdit::ActionPosition::TrailingPosition );
	return PathEntry( entry, button );
}


void DialogGrid_packRow( QGridLayout* grid, QWidget* row, QLabel *label ){
	const int rowCount = grid->rowCount();
	grid->addWidget( row, rowCount, 1 );
	grid->addWidget( label, rowCount, 0, Qt::AlignmentFlag::AlignRight );
}

void DialogGrid_packRow( QGridLayout* grid, QWidget* row, const char* name ){
	DialogGrid_packRow( grid, row, new QLabel( name ) );
}

void DialogGrid_packRow( QGridLayout* grid, QLayout* row, QLabel *label ){
	const int rowCount = grid->rowCount();
	grid->addLayout( row, rowCount, 1 );
	grid->addWidget( label, rowCount, 0, Qt::AlignmentFlag::AlignRight );
}

void DialogGrid_packRow( QGridLayout* grid, QLayout* row, const char* name ){
	DialogGrid_packRow( grid, row, new QLabel( name ) );
}

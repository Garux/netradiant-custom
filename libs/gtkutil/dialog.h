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

#include "generic/arrayrange.h"

class QWidget;
class QGridLayout;
class QButtonGroup;
class QHBoxLayout;
class QAction;
class QLineEdit;
class QLayout;
class QLabel;


class RadioHBox
{
public:
	QHBoxLayout* m_hbox;
	QButtonGroup* m_radio;
	RadioHBox( QHBoxLayout* hbox, QButtonGroup* radio ) :
		m_hbox( hbox ),
		m_radio( radio ){
	}
};

RadioHBox RadioHBox_new( StringArrayRange names );


class PathEntry
{
public:
	QLineEdit* m_entry;
	QAction* m_button;
	PathEntry( QLineEdit* entry, QAction* button ) :
		m_entry( entry ),
		m_button( button ){
	}
};

PathEntry PathEntry_new();

void DialogGrid_packRow( QGridLayout* grid, QWidget* row, QLabel *label );
void DialogGrid_packRow( QGridLayout* grid, QWidget* row, const char* name );
void DialogGrid_packRow( QGridLayout* grid, QLayout* row, QLabel *label );
void DialogGrid_packRow( QGridLayout* grid, QLayout* row, const char* name );
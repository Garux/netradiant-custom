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

#include "messagebox.h"
#include <QMessageBox>


inline QMessageBox::StandardButtons qt_buttons_for_mask( int buttons ){
	QMessageBox::StandardButtons out;
	if( buttons & eIDOK )
		out |= QMessageBox::StandardButton::Ok;
	if( buttons & eIDCANCEL )
		out |= QMessageBox::StandardButton::Cancel;
	if( buttons & eIDYES )
		out |= QMessageBox::StandardButton::Yes;
	if( buttons & eIDNO )
		out |= QMessageBox::StandardButton::No;
	return out;
}

EMessageBoxReturn qt_MessageBox( QWidget *parent, const char* text, const char* title /* = "NetRadiant" */, EMessageBoxType type /* = EMessageBoxType::Info */, int buttons /* = 0 */ ){
	QMessageBox::StandardButton ret{};
	switch ( type )
	{
	case EMessageBoxType::Info:
		ret = QMessageBox::information( parent, title, text, buttons? qt_buttons_for_mask( buttons ) : QMessageBox::StandardButton::Ok );
		break;
	case EMessageBoxType::Question:
		ret = QMessageBox::question( parent, title, text, buttons? qt_buttons_for_mask( buttons ) : QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No );
		break;
	case EMessageBoxType::Warning:
		ret = QMessageBox::warning( parent, title, text, buttons? qt_buttons_for_mask( buttons ) : QMessageBox::StandardButton::Ok );
		break;
	case EMessageBoxType::Error:
		ret = QMessageBox::critical( parent, title, text, buttons? qt_buttons_for_mask( buttons ) : QMessageBox::StandardButton::Ok );
		break;
	}

	switch ( ret )
	{
	case QMessageBox::StandardButton::Ok:
		return eIDOK;
	case QMessageBox::StandardButton::Cancel:
		return eIDCANCEL;
	case QMessageBox::StandardButton::Yes:
		return eIDYES;
	case QMessageBox::StandardButton::No:
		return eIDNO;
	default:
		ASSERT_MESSAGE( false, "unexpected EMessageBoxReturn" );
		return eIDOK;
	}
}

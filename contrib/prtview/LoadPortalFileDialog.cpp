/*
   PrtView plugin for GtkRadiant
   Copyright (C) 2001 Geoffrey Dewan, Loki software and qeradiant.com

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

// LoadPortalFileDialog.cpp : implementation file
//

#include "LoadPortalFileDialog.h"

#include "stream/stringstream.h"
#include "os/path.h"

#include "qerplugin.h"

#include "prtview.h"
#include "portals.h"

#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QApplication>
#include <QStyle>
#include <QAction>
#include <QCheckBox>


bool DoLoadPortalFileDialog(){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Load .prt" );

	QLineEdit *line;
	QCheckBox *check3d, *check2d;

	{
		auto vbox = new QVBoxLayout( &dialog );
		{
			vbox->addWidget( line = new QLineEdit );
			auto button = line->addAction( QApplication::style()->standardIcon( QStyle::SP_DialogOpenButton ), QLineEdit::ActionPosition::TrailingPosition );
			QObject::connect( button, &QAction::triggered, [line](){
				if ( const char* filename = GlobalRadiant().m_pfnFileDialog( g_pRadiantWnd, true, "Locate portal (.prt) file", line->text().toLatin1().constData(), 0, true, false, false ) )
					line->setText( filename );
			} );
		}
		{
			vbox->addWidget( check3d = new QCheckBox( "Show 3D" ) );
			vbox->addWidget( check2d = new QCheckBox( "Show 2D" ) );
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			vbox->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}


	portals.fn = StringStream( PathExtensionless( GlobalRadiant().getMapName() ), ".prt" );

	line->setText( portals.fn.c_str() );
	check3d->setChecked( portals.show_3d );
	check2d->setChecked( portals.show_2d );

	if ( dialog.exec() ) {
		portals.fn = line->text().toLatin1().constData();

		portals.Purge();

		portals.show_3d = check3d->isChecked();
		portals.show_2d = check2d->isChecked();
		return true;
	}

	return false;
}

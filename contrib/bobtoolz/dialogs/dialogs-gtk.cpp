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

#include "dialogs-gtk.h"
#include "../funchandlers.h"

#include "../lists.h"
#include "../misc.h"

#include "../bobToolz-GTK.h"

#include <QDialog>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QFrame>
#include "gtkutil/spinbox.h"
#include "gtkutil/combobox.h"


/*--------------------------------
        Modal Dialog Boxes
   ---------------------------------*/

/*

   Major clean up of variable names etc required, excluding Mars's ones,
   which are nicely done :)

 */

EMessageBoxReturn DoMessageBox( const char* lpText, const char* lpCaption, EMessageBoxType type ){
	return GlobalRadiant().m_pfnMessageBox( g_pRadiantWnd, lpText, lpCaption, type, 0 );
}

bool DoIntersectBox( IntersectRS* rs ){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Intersect" );

	QRadioButton *radio1, *radio2;
	QCheckBox *check1, *check2;

	{
		auto vbox = new QVBoxLayout( &dialog );
		vbox->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			vbox->addWidget( radio1 = new QRadioButton( "Use Whole Map" ) );
			vbox->addWidget( radio2 = new QRadioButton( "Use Selected Brushes" ) );
			radio1->setChecked( true );
		}
		{
				auto line = new QFrame;
				line->setFrameShape( QFrame::Shape::HLine );
				line->setFrameShadow( QFrame::Shadow::Raised );
				vbox->addWidget( line );
		}
		{
			vbox->addWidget( check1 = new QCheckBox( "Include Detail Brushes" ) );
			vbox->addWidget( check2 = new QCheckBox( "Select Duplicate Brushes Only" ) );
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			vbox->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if( dialog.exec() ){
		rs->nBrushOptions = radio1->isChecked()
		                    ? BRUSH_OPT_WHOLE_MAP
							: BRUSH_OPT_SELECTED;
		rs->bUseDetail = check1->isChecked();
		rs->bDuplicateOnly = check2->isChecked();
		return true;
	}
	return false;
}

bool DoPolygonBox( PolygonRS* rs ){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Polygon Builder" );

	QSpinBox *spin_sides, *spin_border;
	QCheckBox *check_border, *check_inverse, *check_align;

	{
		auto hbox = new QHBoxLayout( &dialog );
		hbox->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			auto form = new QFormLayout;
			hbox->addLayout( form );
			{
				auto spin = spin_sides = new SpinBox( 3, 128, 6 );
				form->addRow( new SpinBoxLabel( "Number Of Sides", spin ), spin );
			}
			{
				auto spin = spin_border = new SpinBox( 8, 256, 16 );
				form->addRow( new SpinBoxLabel( "Border Width", spin ), spin );
			}
			{
				auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
				form->addWidget( buttons );
				QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
				QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
			}
		}
		{
			auto vbox = new QVBoxLayout;
			hbox->addLayout( vbox );
			{
				vbox->addWidget( check_border = new QCheckBox( "Use Border" ) );
				vbox->addWidget( check_inverse = new QCheckBox( "Inverse Polygon" ) );
				vbox->addWidget( check_align = new QCheckBox( "Align Top Edge" ) );
				QObject::connect( check_border, &QCheckBox::stateChanged, spin_border, &QWidget::setEnabled );
				spin_border->setEnabled( false );
				QObject::connect( check_inverse, &QCheckBox::stateChanged, [check_border]( int on ){
					if( on ) // either of border or inverse may be used
						check_border->setChecked( false );
				} );
				QObject::connect( check_border, &QCheckBox::stateChanged, [check_inverse]( int on ){
					if( on ) // either of border or inverse may be used
						check_inverse->setChecked( false );
				} );
			}
		}
	}

	if( dialog.exec() ){
		rs->nSides = spin_sides->value();
		rs->nBorderWidth = spin_border->value();

		rs->bUseBorder = check_border->isChecked();
		rs->bInverse = check_inverse->isChecked();
		rs->bAlignTop = check_align->isChecked();

		return true;
	}
	return false;
}

// mars
// for stair builder stuck as close as i could to the MFC version
// obviously feel free to change it at will :)
bool DoBuildStairsBox( BuildStairsRS* rs ){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Stair Builder" );

	QSpinBox *spin_stairHeight;
	QButtonGroup *group_direction, *group_style;
	QCheckBox *check_detail;
	QLineEdit *edit_mainTex, *edit_riserTex;

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			auto spin = spin_stairHeight = new SpinBox( 1, 1024, 8 );
			form->addRow( new SpinBoxLabel( "Stair Height", spin ), spin );
		}
		{
			auto hbox = new QHBoxLayout;
			form->addRow( "Direction:", hbox );
			group_direction = new QButtonGroup( form );
			group_direction->addButton( new QRadioButton( "North" ), MOVE_NORTH );
			group_direction->addButton( new QRadioButton( "South" ), MOVE_SOUTH );
			group_direction->addButton( new QRadioButton( "East" ), MOVE_EAST );
			group_direction->addButton( new QRadioButton( "West" ), MOVE_WEST );
			for( auto b : group_direction->buttons() )
				hbox->addWidget( b );
			group_direction->button( MOVE_NORTH )->setChecked( true );
		}
		{
			auto hbox = new QHBoxLayout;
			form->addRow( "Style:", hbox );
			group_style = new QButtonGroup( form );
			group_style->addButton( new QRadioButton( "Original" ), STYLE_ORIGINAL );
			group_style->addButton( new QRadioButton( "Bob's Style" ), STYLE_BOB );
			group_style->addButton( new QRadioButton( "Corner Style" ), STYLE_CORNER );
			for( auto b : group_style->buttons() )
				hbox->addWidget( b );
			group_style->button( STYLE_ORIGINAL )->setChecked( true );
		}
		{
			form->addWidget( check_detail = new QCheckBox( "Use Detail Brushes" ) );
			QObject::connect( group_style, &QButtonGroup::idClicked, [check_detail]( int id ){
				check_detail->setEnabled( id == STYLE_BOB );
			} );
			check_detail->setEnabled( false );
		}
		{
			form->addRow( "Main Texture", edit_mainTex = new QLineEdit( rs->mainTexture ) );
			edit_mainTex->setMaxLength( std::size( rs->mainTexture ) - 1 );
		}
		{
			form->addRow( "Riser Texture", edit_riserTex = new QLineEdit );
			edit_riserTex->setMaxLength( std::size( rs->riserTexture ) - 1 );
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if( dialog.exec() )
	{
		rs->stairHeight = spin_stairHeight->value();
		rs->direction = group_direction->checkedId();
		rs->style = group_style->checkedId();
		rs->bUseDetail = check_detail->isChecked();

		strcpy( rs->mainTexture, edit_mainTex->text().toLatin1().constData() );
		strcpy( rs->riserTexture, edit_riserTex->text().toLatin1().constData() );

		return true;
	}
	return false;
}

bool DoDoorsBox( DoorRS* rs ){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Door Builder" );

	QCheckBox   *checkScaleMainH, *checkScaleMainV, *checkScaleTrimH, *checkScaleTrimV;
	QComboBox   *comboMain, *comboTrim;
	QRadioButton   *radioNS, *radioEW;

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			form->addRow( "Door Front/Back Texture", comboMain = new ComboBox );
			char buffer[256];
			comboMain->addItems( LoadListStore( GetFilename( buffer, "bt/door-tex.txt" ) ) );
			comboMain->setEditable( true );
			comboMain->lineEdit()->setMaxLength( std::size( rs->mainTexture ) - 1 );
			comboMain->setCurrentText( rs->mainTexture );
		}
		{
			form->addWidget( checkScaleMainH = new QCheckBox( "Scale Main Texture Horizontally" ) );
			form->addWidget( checkScaleMainV = new QCheckBox( "Scale Main Texture Vertically" ) );
			checkScaleMainH->setChecked( true );
			checkScaleMainV->setChecked( true );
		}
		{
			form->addRow( "Door Trim Texture", comboTrim = new ComboBox );
			char buffer[256];
			comboTrim->addItems( LoadListStore( GetFilename( buffer, "bt/door-tex-trim.txt" ) ) );
			comboTrim->setEditable( true );
			comboTrim->lineEdit()->setMaxLength( std::size( rs->trimTexture ) - 1 );
			comboTrim->setCurrentText( QString() );
		}
		{
			form->addWidget( checkScaleTrimH = new QCheckBox( "Scale Trim Texture Horizontally" ) );
			form->addWidget( checkScaleTrimV = new QCheckBox( "Scale Trim Texture Vertically" ) );
			checkScaleTrimH->setChecked( true );
		}
		{
			form->addRow( "Orientation", radioNS = new QRadioButton( "North - South" ) );
			form->addRow( "", radioEW = new QRadioButton( "North - South" ) );
			radioNS->setChecked( true );
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if( dialog.exec() ){
		strcpy( rs->mainTexture, comboMain->currentText().toLatin1().constData() );
		strcpy( rs->trimTexture, comboTrim->currentText().toLatin1().constData() );

		rs->bScaleMainH = checkScaleMainH->isChecked();
		rs->bScaleMainV = checkScaleMainV->isChecked();
		rs->bScaleTrimH = checkScaleTrimH->isChecked();
		rs->bScaleTrimV = checkScaleTrimV->isChecked();

		rs->nOrientation = radioNS->isChecked()
		                 ? DIRECTION_NS
						 : DIRECTION_EW;
		return true;
	}
	return false;
}

EMessageBoxReturn DoPathPlotterBox( PathPlotterRS* rs ){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Path Plotter" );

	QSpinBox *spin_pts;
	QDoubleSpinBox *spin_mult, *spin_grav;
	QCheckBox *check1, *check2;

	EMessageBoxReturn ret = eIDCANCEL;

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			auto spin = spin_pts = new SpinBox( 1, 200, 50 );
			form->addRow( new SpinBoxLabel( "Number Of Points", spin ), spin );
		}
		{
			auto spin = spin_mult = new DoubleSpinBox( 1, 10, 3 );
			form->addRow( new SpinBoxLabel( "Distance Multipler", spin ), spin );
			spin->setToolTip( "Path Distance = dist(start -> apex) * multiplier" );
		}
		{
			auto spin = spin_grav = new DoubleSpinBox( -10000, -1, -800 );
			form->addRow( new SpinBoxLabel( "Gravity", spin ), spin );
		}
		{
			form->addWidget( check1 = new QCheckBox( "No Dynamic Update" ) );
			form->addWidget( check2 = new QCheckBox( "Show Bounding Lines" ) );
		}
		{
			auto buttons = new QDialogButtonBox;
			form->addWidget( buttons );
			// rejection via dialog means will return DialogCode::Rejected (0), eID* > 0
			QObject::connect( buttons->addButton( "Enable", QDialogButtonBox::ButtonRole::AcceptRole ),
			                  &QAbstractButton::clicked, [&dialog](){ dialog.done( eIDYES ); } );
			QObject::connect( buttons->addButton( "Disable", QDialogButtonBox::ButtonRole::NoRole ),
			                  &QAbstractButton::clicked, [&dialog](){ dialog.done( eIDNO ); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ),
			                  &QAbstractButton::clicked, &dialog, &QDialog::reject );
		}
	}

	if( const int r = dialog.exec() ){
		ret = static_cast<EMessageBoxReturn>( r );

		if ( ret == eIDYES ) {
			rs->nPoints = spin_pts->value();
			rs->fMultiplier = spin_mult->value();
			rs->fGravity = spin_grav->value();

			rs->bNoUpdate = check1->isChecked();
			rs->bShowExtra = check2->isChecked();
		}
	}

	return ret;
}

EMessageBoxReturn DoCTFColourChangeBox(){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "CTF Colour Changer" );

	EMessageBoxReturn ret = eIDCANCEL;

	{
		auto buttons = new QDialogButtonBox;
		// rejection via dialog means will return DialogCode::Rejected (0), eID* > 0
		QObject::connect( buttons->addButton( "Red->Blue", QDialogButtonBox::ButtonRole::AcceptRole ),
							&QAbstractButton::clicked, [&dialog](){ dialog.done( eIDOK ); } );
		QObject::connect( buttons->addButton( "Blue->Red", QDialogButtonBox::ButtonRole::NoRole ),
							&QAbstractButton::clicked, [&dialog](){ dialog.done( eIDYES ); } );
		QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ),
							&QAbstractButton::clicked, &dialog, &QDialog::reject );
		( new QHBoxLayout( &dialog ) )->addWidget( buttons );
	}

	if( const int r = dialog.exec() ){
		ret = static_cast<EMessageBoxReturn>( r );
	}

	return ret;
}


EMessageBoxReturn DoResetTextureBox( ResetTextureRS* rs ){
	EMessageBoxReturn ret = eIDCANCEL;

	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Texture Reset" );

	QLineEdit *editTexOld, *editTexNew;
	QDoubleSpinBox *spin_ScaleHor, *spin_ScaleVert;
	QDoubleSpinBox *spin_ShiftHor, *spin_ShiftVert;
	QDoubleSpinBox *spin_Rotation;

	{
		auto vbox = new QVBoxLayout( &dialog );
		{
			vbox->addWidget( new QLabel( QString( "Currently Selected Texture:   " ) + GetCurrentTexture() ) );
		}
		{
			auto group = new QGroupBox( "Reset Texture Names" );
			group->setCheckable( true );
			group->setChecked( false );
			vbox->addWidget( group );
			{
				auto form = new QFormLayout( group );
				{
					form->addRow( "Old Name: ", editTexOld = new QLineEdit( rs->textureName ) );
					editTexOld->setMaxLength( std::size( rs->textureName ) - 1 );
				}
				{
					form->addRow( "New Name: ", editTexNew = new QLineEdit( rs->newTextureName ) );
					editTexNew->setMaxLength( std::size( rs->newTextureName ) - 1 );
				}
			}
		}
		{
			auto group = new QGroupBox( "New Horizontal Scale" );
			group->setCheckable( true );
			group->setChecked( false );
			vbox->addWidget( group );
			{
				auto spin = spin_ScaleHor = new DoubleSpinBox( -1024, 1024, .5, 3, .25 );
				( new QFormLayout( group ) )->addWidget( spin );
			}
		}
		{
			auto group = new QGroupBox( "New Vertical Scale" );
			group->setCheckable( true );
			group->setChecked( false );
			vbox->addWidget( group );
			{
				auto spin = spin_ScaleVert = new DoubleSpinBox( -1024, 1024, .5, 3, .25 );
				( new QFormLayout( group ) )->addWidget( spin );
			}
		}
		{
			auto group = new QGroupBox( "New Horizontal Shift" );
			group->setCheckable( true );
			group->setChecked( false );
			vbox->addWidget( group );
			{
				auto spin = spin_ShiftHor = new DoubleSpinBox( -999999, 999999 );
				( new QFormLayout( group ) )->addWidget( spin );
			}
		}
		{
			auto group = new QGroupBox( "New Vertical Shift" );
			group->setCheckable( true );
			group->setChecked( false );
			vbox->addWidget( group );
			{
				auto spin = spin_ShiftVert = new DoubleSpinBox( -999999, 999999 );
				( new QFormLayout( group ) )->addWidget( spin );
			}
		}
		{
			auto group = new QGroupBox( "New Rotation Value" );
			group->setCheckable( true );
			group->setChecked( false );
			vbox->addWidget( group );
			{
				auto spin = spin_Rotation = new DoubleSpinBox( -360, 360, 0, 2, 1, true );
				( new QFormLayout( group ) )->addWidget( spin );
			}
		}

		{
			auto buttons = new QDialogButtonBox;
			vbox->addWidget( buttons );
			// rejection via dialog means will return DialogCode::Rejected (0), eID* > 0
			QObject::connect( buttons->addButton( "Use Selected Brushes", QDialogButtonBox::ButtonRole::AcceptRole ),
			                  &QAbstractButton::clicked, [&dialog](){ dialog.done( eIDOK ); } );
			QObject::connect( buttons->addButton( "Use All Brushes", QDialogButtonBox::ButtonRole::NoRole ),
			                  &QAbstractButton::clicked, [&dialog](){ dialog.done( eIDYES ); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ),
			                  &QAbstractButton::clicked, &dialog, &QDialog::reject );
		}
	}


	if( const int r = dialog.exec() ){
		ret = static_cast<EMessageBoxReturn>( r );
		if ( ( rs->bResetTextureName = editTexOld->isEnabled() ) ) {
			strcpy( rs->textureName,     editTexOld->text().toLatin1().constData() );
			strcpy( rs->newTextureName,  editTexNew->text().toLatin1().constData() );
		}
		if ( ( rs->bResetScale[0] = spin_ScaleHor->isEnabled() ) )
			rs->fScale[0] = spin_ScaleHor->value();

		if ( ( rs->bResetScale[1] = spin_ScaleVert->isEnabled() ) )
			rs->fScale[1] = spin_ScaleVert->value();

		if ( ( rs->bResetShift[0] = spin_ShiftHor->isEnabled() ) )
			rs->fShift[0] = spin_ShiftHor->value();

		if ( ( rs->bResetShift[1] = spin_ShiftVert->isEnabled() ) )
			rs->fShift[1] = spin_ShiftVert->value();

		if ( ( rs->bResetRotation = spin_Rotation->isEnabled() ) )
			rs->rotation = spin_Rotation->value();
	}

	return ret;
}


// ailmanki
// add a simple input for the MakeChain thing..
bool DoMakeChainBox( MakeChainRS* rs ){
	QDialog dialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Make Chain" );

	QSpinBox *spin_linkNum;
	QLineEdit *edit_linkName;

	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			auto spin = spin_linkNum = new SpinBox( 1, 1000 );
			form->addRow( new SpinBoxLabel( "Number of elements in chain", spin ), spin );
		}
		{
			form->addRow( "Basename for chain's targetnames", edit_linkName = new QLineEdit );
			edit_linkName->setMaxLength( std::size( rs->linkName ) - 1 );
		}
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	if( dialog.exec() ){
		rs->linkNum = spin_linkNum->value();
		strcpy( rs->linkName, edit_linkName->text().toLatin1().constData() );
		return true;
	}
	return false;
}

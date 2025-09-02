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

#include "ConfigDialog.h"

#include "iscenegraph.h"
#include "qerplugin.h"

#include "prtview.h"
#include "portals.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include "gtkutil/combobox.h"


static void OnColor( PackedColour& clr ){
	Vector3 color( RGB_UNPACK_R( clr ) / 255.f, RGB_UNPACK_G( clr ) / 255.f, RGB_UNPACK_B( clr ) / 255.f );
	if ( GlobalRadiant().m_pfnColorDialog( g_pRadiantWnd, color, "Choose Color" ) )
	{
		clr = RGB_PACK( color[0] * 255, color[1] * 255, color[2] * 255 );
		Portals_shadersChanged();
		SceneChangeNotify();
	}
}

static void form_add_color( QFormLayout *form, PackedColour& color ){
	auto *button = new QPushButton( "Color" );
	QObject::connect( button, &QAbstractButton::clicked, [&color](){ OnColor( color ); } );

	auto *hbox = new QHBoxLayout;
	form->addRow( hbox );
	hbox->addStretch();
	hbox->addWidget( button );
}

static void form_add_slider( QFormLayout *form, int& param, int min, int max, const char *prefix, const char *suffix, bool changeShaders ){
	auto *slider = new QSlider( Qt::Orientation::Horizontal );
	slider->setRange( min, max );

	auto *label = new QLabel;
	label->setMinimumWidth( label->fontMetrics().horizontalAdvance( QString( prefix ) + QString::number( max ) + suffix ) );
	const auto label_set_text = [label, prefix, suffix]( int value ){
		label->setText( QString( prefix ) + QString::number( value ) + suffix );
	};
	QObject::connect( slider, &QSlider::valueChanged, label_set_text );
	slider->setValue( param ); // sets label text too

	QObject::connect( slider, &QAbstractSlider::valueChanged, [&param, changeShaders]( int value ){
		param = value;
		if( changeShaders )
			Portals_shadersChanged();
		SceneChangeNotify();
	} );

	form->addRow( label, slider );
}

static QGroupBox* vbox_add_group( QVBoxLayout *vbox, const char *name, bool& param, bool changeShaders ){
	auto *group = new QGroupBox( name );
	vbox->addWidget( group );
	group->setCheckable( true );
	group->setChecked( param );
	QObject::connect( group, &QGroupBox::toggled, [&param, changeShaders]( bool on ){
		param = on;
		if( changeShaders )
			Portals_shadersChanged();
		SceneChangeNotify();
	} );

	return group;
}

static QCheckBox* new_checkbox( const char *name, bool& param ){
	auto *check = new QCheckBox( name );
	check->setChecked( param );
	QObject::connect( check, &QAbstractButton::toggled, [&param]( bool checked ){
		param = checked;
		SceneChangeNotify();
	} );
	return check;
}

void DoConfigDialog(){
	auto *dialog = new QDialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog->setWindowTitle( "Portal Viewer Configuration" );
	dialog->setAttribute( Qt::WidgetAttribute::WA_DeleteOnClose );

	{
		auto *dialog_vbox = new QVBoxLayout( dialog );
		dialog_vbox->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		{
			auto *vbox = new QVBoxLayout( vbox_add_group( dialog_vbox, "3D View", portals.show_3d, false ) );
			{
				auto *form = new QFormLayout( vbox_add_group( vbox, "Lines", portals.lines, false ) );
				form_add_slider( form, portals.width_3d, 1, 10, "Width = ", "", true );
				form_add_color( form, portals.color_3d );
			}
			{
				auto *form = new QFormLayout( vbox_add_group( vbox, "Polygons", portals.polygons, false ) );
				form_add_slider( form, portals.opacity_3d, 0, 100, "Opacity = ", "%", false );
			}
			{
				auto *form = new QFormLayout( vbox_add_group( vbox, "Fog", portals.fog, true ) );
				form_add_color( form, portals.color_fog );
			}
			{
				auto *form = new QFormLayout( vbox_add_group( vbox, "Cubic clipper", portals.clip, false ) );
				form_add_slider( form, portals.clip_range, 64, 8192, "Clip range = ", "", false );
			}
			{
				auto *combo = new ComboBox;
				vbox->addWidget( combo );
				combo->addItem( "Z-Buffer Test and Write (recommended for solid or no polygons)" );
				combo->addItem( "Z-Buffer Test Only (recommended for transparent polygons)" );
				combo->addItem( "Z-Buffer Off" );
				combo->setCurrentIndex( portals.zbuffer );
				QObject::connect( combo, QOverload<int>::of( &QComboBox::currentIndexChanged ), [&zbuffer = portals.zbuffer]( int index ){
					zbuffer = index;
					Portals_shadersChanged();
					SceneChangeNotify();
				} );
			}
		}
		{
			dialog_vbox->addWidget( new_checkbox( "Draw Hint Portals", portals.draw_hints ), 0, Qt::AlignmentFlag::AlignHCenter );
			dialog_vbox->addWidget( new_checkbox( "Draw Regular Portals", portals.draw_nonhints ), 0, Qt::AlignmentFlag::AlignHCenter );
		}
		{
			auto *form = new QFormLayout( vbox_add_group( dialog_vbox, "2D View Lines", portals.show_2d, false ) );
			form_add_slider( form, portals.width_2d, 1, 10, "Width = ", "", true );
			form_add_color( form, portals.color_2d );
		}
	}

	dialog->show();
}

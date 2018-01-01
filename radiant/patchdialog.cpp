/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

//
// Leonardo Zide (leo@lokigames.com)
//

#include "patchdialog.h"

#include "itexdef.h"

#include "debugging/debugging.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include "gtkutil/spinbox.h"
#include "gtkutil/nonmodal.h"
#include "dialog.h"
#include "gtkdlgs.h"
#include "mainframe.h"
#include "patchmanip.h"
#include "patch.h"


class PatchFixedSubdivisions
{
public:
	PatchFixedSubdivisions() : m_enabled( false ), m_x( 0 ), m_y( 0 ){
	}
	PatchFixedSubdivisions( bool enabled, std::size_t x, std::size_t y ) : m_enabled( enabled ), m_x( x ), m_y( y ){
	}
	bool m_enabled;
	std::size_t m_x;
	std::size_t m_y;
};

void Patch_getFixedSubdivisions( const Patch& patch, PatchFixedSubdivisions& subdivisions ){
	subdivisions.m_enabled = patch.m_patchDef3;
	subdivisions.m_x = patch.m_subdivisions_x;
	subdivisions.m_y = patch.m_subdivisions_y;
}

const std::size_t MAX_PATCH_SUBDIVISIONS = 32;

void Patch_setFixedSubdivisions( Patch& patch, const PatchFixedSubdivisions& subdivisions ){
	patch.undoSave();

	patch.m_patchDef3 = subdivisions.m_enabled;
	patch.m_subdivisions_x = subdivisions.m_x;
	patch.m_subdivisions_y = subdivisions.m_y;

	if ( patch.m_subdivisions_x == 0 ) {
		patch.m_subdivisions_x = 4;
	}
	else if ( patch.m_subdivisions_x > MAX_PATCH_SUBDIVISIONS ) {
		patch.m_subdivisions_x = MAX_PATCH_SUBDIVISIONS;
	}
	if ( patch.m_subdivisions_y == 0 ) {
		patch.m_subdivisions_y = 4;
	}
	else if ( patch.m_subdivisions_y > MAX_PATCH_SUBDIVISIONS ) {
		patch.m_subdivisions_y = MAX_PATCH_SUBDIVISIONS;
	}

	SceneChangeNotify();
	Patch_textureChanged();
	patch.controlPointsChanged();
}

class PatchGetFixedSubdivisions
{
	PatchFixedSubdivisions& m_subdivisions;
public:
	PatchGetFixedSubdivisions( PatchFixedSubdivisions& subdivisions ) : m_subdivisions( subdivisions ){
	}
	void operator()( Patch& patch ){
		Patch_getFixedSubdivisions( patch, m_subdivisions );
		SceneChangeNotify();
	}
};

void Scene_PatchGetFixedSubdivisions( PatchFixedSubdivisions& subdivisions ){
#if 1
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		Patch* patch = Node_getPatch( GlobalSelectionSystem().ultimateSelected().path().top() );
		if ( patch != 0 ) {
			Patch_getFixedSubdivisions( *patch, subdivisions );
		}
	}
#else
	Scene_forEachVisibleSelectedPatch( PatchGetFixedSubdivisions( subdivisions ) );
#endif
}

void Scene_PatchSetFixedSubdivisions( const PatchFixedSubdivisions& subdivisions ){
	UndoableCommand command( "patchSetFixedSubdivisions" );
	Scene_forEachVisibleSelectedPatch( [subdivisions]( Patch& patch ){ Patch_setFixedSubdivisions( patch, subdivisions ); } );
}

class Subdivisions
{
public:
	QGroupBox* m_enabled;
	NonModalSpinner* m_horizontal;
	NonModalSpinner* m_vertical;
	Subdivisions() : m_enabled( 0 ), m_horizontal( 0 ), m_vertical( 0 ){
	}
	void update(){
		PatchFixedSubdivisions subdivisions;
		Scene_PatchGetFixedSubdivisions( subdivisions );

		m_enabled->setChecked( subdivisions.m_enabled );

		m_horizontal->setValue( subdivisions.m_x );
		m_vertical->setValue( subdivisions.m_y );
	}
	void cancel(){
		update();
	}
	typedef MemberCaller<Subdivisions, void(), &Subdivisions::cancel> CancelCaller;
	void apply(){
		Scene_PatchSetFixedSubdivisions(
		    PatchFixedSubdivisions(
		        m_enabled->isChecked(),
		        static_cast<std::size_t>( m_horizontal->value() ),
		        static_cast<std::size_t>( m_vertical->value() )
		    )
		);
	}
	typedef MemberCaller<Subdivisions, void(), &Subdivisions::apply> ApplyCaller;
};



static Subdivisions *g_subdivisions;

QGroupBox* patch_tesselation_create(){
	g_subdivisions = new Subdivisions;

	auto *frame = g_subdivisions->m_enabled = new QGroupBox( "Fixed Tesselation" );

	auto *hbox = new QHBoxLayout( frame );
	hbox->setContentsMargins( 0, 0, 0, 0 );

	auto *hspin = g_subdivisions->m_horizontal = new NonModalSpinner( 1, MAX_PATCH_SUBDIVISIONS, 4, 0, 1 );
	hspin->setCallbacks( Subdivisions::ApplyCaller( *g_subdivisions ), Subdivisions::CancelCaller( *g_subdivisions ) );
	auto *wlabel = new SpinBoxLabel( "H", hspin );
	hbox->addWidget( wlabel, 0, Qt::AlignmentFlag::AlignRight );
	hbox->addWidget( hspin, 0, Qt::AlignmentFlag::AlignLeft );

	auto *vspin = g_subdivisions->m_vertical = new NonModalSpinner( 1, MAX_PATCH_SUBDIVISIONS, 4, 0, 1 );
	vspin->setCallbacks( Subdivisions::ApplyCaller( *g_subdivisions ), Subdivisions::CancelCaller( *g_subdivisions ) );
	auto *hlabel = new SpinBoxLabel( "V", vspin );
	hbox->addWidget( hlabel, 0, Qt::AlignmentFlag::AlignRight );
	hbox->addWidget( vspin, 0, Qt::AlignmentFlag::AlignLeft );

	QObject::connect( frame, &QGroupBox::toggled, [hspin, wlabel, vspin, hlabel]( bool on ){
		hspin->setVisible( on );
		wlabel->setVisible( on );
		vspin->setVisible( on );
		hlabel->setVisible( on );
	} );
	frame->setCheckable( true );
	frame->setChecked( false );
	QObject::connect( frame, &QGroupBox::clicked, Subdivisions::ApplyCaller( *g_subdivisions ) );

	return frame;
}

void patch_tesselation_update(){
	if( g_subdivisions != nullptr ){
		g_subdivisions->update();
	}
}


void Scene_PatchRotateTexture_Selected( scene::Graph& graph, float angle ){
	Scene_forEachVisibleSelectedPatch( [angle]( Patch& patch ){ patch.RotateTexture( angle ); } );
}

inline float Patch_convertScale( float scale ){
	if ( scale > 0 ) {
		return scale;
	}
	if ( scale < 0 ) {
		return -1 / scale;
	}
	return 1;
}

void Scene_PatchScaleTexture_Selected( scene::Graph& graph, float s, float t ){
	Scene_forEachVisibleSelectedPatch( [s = Patch_convertScale( s ), t = Patch_convertScale( t )]( Patch& patch ){ patch.ScaleTexture( s, t ); } );
}

void Scene_PatchTranslateTexture_Selected( scene::Graph& graph, float s, float t ){
	Scene_forEachVisibleSelectedPatch( [s, t]( Patch& patch ){ patch.TranslateTexture( s, t ); } );
}

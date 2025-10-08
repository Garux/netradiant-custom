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

// BobView.cpp: implementation of the DVisDrawer class.
//
//////////////////////////////////////////////////////////////////////

#include "DVisDrawer.h"

#include "iglrender.h"
#include "math/matrix.h"

#include "DPoint.h"

#include "misc.h"
#include "funchandlers.h"
#include "bsploader.h"

#include "bobToolz-GTK.h"
#include <QDialog>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QCheckBox>
#include "visfind.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DVisDrawer::DVisDrawer(){
	m_list = nullptr;
	m_colorPerSurf = false;

	ui_create();

	constructShaders();
	GlobalShaderCache().attachRenderable( *this );
}

DVisDrawer::~DVisDrawer(){
	GlobalShaderCache().detachRenderable( *this );
	destroyShaders();

	delete m_list;
	delete m_dialog;
	FreeBSPData();

	SceneChangeNotify();
}

//////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////
const char* g_state_solid = "$bobtoolz/visdrawer/solid";
const char* g_state_wireframe = "$bobtoolz/visdrawer/wireframe";

void DVisDrawer::constructShaders(){
	OpenGLState state;
	GlobalOpenGLStateLibrary().getDefaultState( state );
	state.m_sort = OpenGLState::eSortOverlayFirst;
	state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_COLOURCHANGE;
	state.m_linewidth = 1;

	GlobalOpenGLStateLibrary().insert( g_state_wireframe, state );

	GlobalOpenGLStateLibrary().getDefaultState( state );
	state.m_depthfunc = GL_LEQUAL;
	state.m_state = RENDER_FILL | RENDER_BLEND | RENDER_COLOURWRITE | RENDER_COLOURCHANGE | RENDER_DEPTHTEST;

	GlobalOpenGLStateLibrary().insert( g_state_solid, state );

	m_shader_solid = GlobalShaderCache().capture( g_state_solid );
	m_shader_wireframe = GlobalShaderCache().capture( g_state_wireframe );
}

void DVisDrawer::destroyShaders(){
	GlobalShaderCache().release( g_state_solid );
	GlobalShaderCache().release( g_state_wireframe );
	GlobalOpenGLStateLibrary().erase( g_state_solid );
	GlobalOpenGLStateLibrary().erase( g_state_wireframe );
}

void DVisDrawer::render( RenderStateFlags state ) const {
	gl().glEnable( GL_POLYGON_OFFSET_FILL );
	for( const DMetaSurf& surf : *m_list ){
		gl().glColor4f( surf.colour[0], surf.colour[1], surf.colour[2], 0.5f );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( vec3_t ), surf.verts );
		gl().glDrawElements( GL_TRIANGLES, GLsizei( surf.indicesN ), GL_UNSIGNED_INT, surf.indices );
	}
	for( const DWinding& w : m_windings ){
		gl().glColor4f( 1, 1, 1, 0.5f );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( *w.p ), w.p );
		gl().glDrawArrays( GL_POLYGON, 0, GLsizei( w.numpoints ) );
	}
	gl().glDisable( GL_POLYGON_OFFSET_FILL );
	gl().glColor4f( 1, 1, 1, 1 );
}

void DVisDrawer::renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
	if ( !m_list ) {
		return;
	}

	renderer.SetState( m_shader_wireframe, Renderer::eWireframeOnly );

	renderer.addRenderable( *this, g_matrix4_identity );
}

void DVisDrawer::renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
	if ( !m_list ) {
		return;
	}

	renderer.SetState( m_shader_solid, Renderer::eWireframeOnly );
	renderer.SetState( m_shader_solid, Renderer::eFullMaterials );

	renderer.addRenderable( *this, g_matrix4_identity );
}

void DVisDrawer::SetList( DMetaSurfaces* pointList ){
	delete std::exchange( m_list, pointList );
	SceneChangeNotify();
}

void DVisDrawer::ClearPoints(){
	SetList( nullptr );
	m_windings.clear();
	m_table->setCurrentCell( -1, -1 ); // unset current item, so QTableWidget::currentCellChanged will be triggered for the same valid index later
}


class VisDialog : public QDialog
{
protected:
	void closeEvent( QCloseEvent *event ) override {
		event->ignore();
		g_VisView.reset();
	}
	using QDialog::QDialog;
	// excess, intent is to have arrows working in the table
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			event->accept();
			return true;
		}
		return QDialog::event( event );
	}
};

void DVisDrawer::ui_create(){
	m_dialog = new VisDialog( g_pRadiantWnd, Qt::Dialog | Qt::WindowCloseButtonHint );
	m_dialog->setWindowTitle( "BSP leafs" );

	{
		auto *vbox = new QVBoxLayout( m_dialog );
		{
			m_table = new QTableWidget( 0, 4 );
			vbox->addWidget( m_table );
			m_table->setHorizontalHeaderLabels( { "leaf #", "leafs visible", "surfs visible", "shaders visible" } );
			m_table->horizontalHeader()->setSortIndicator( 2, Qt::SortOrder::DescendingOrder ); // sort by nsurfs
			m_table->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContentsOnFirstShow );
			m_table->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
			QObject::connect( m_table, &QTableWidget::currentCellChanged, [this]( int currentRow, int currentColumn, int previousRow, int previousColumn ){
				// globalOutputStream() << previousRow << "->" << currentRow << ";" << previousColumn << "->" << currentColumn << "\n";
				if( currentRow != previousRow && currentRow >= 0 ){
					const int leafnum = m_table->item( currentRow, 0 )->data( Qt::ItemDataRole::DisplayRole ).toInt();
					SetList( BuildTrace( leafnum, m_colorPerSurf ) );
					m_windings = BuildLeafWindings( leafnum );
				}
			} );
			m_table->setSelectionBehavior( QAbstractItemView::SelectionBehavior::SelectRows );
			m_table->setSelectionMode( QAbstractItemView::SelectionMode::SingleSelection );
		}
		{
			auto *check = new QCheckBox( "Color per leaf" );
			vbox->addWidget( check );
			QObject::connect( check, &QCheckBox::stateChanged, [check, this]( int on ){
				if( on )
					check->setText( "Color per surface" );
				else
					check->setText( "Color per leaf" );
				m_colorPerSurf = on;
				const int row = m_table->currentRow();
				ClearPoints();
				m_table->setCurrentCell( row, 0 );
			} );
		}
		{
			auto *butt = new QPushButton( "From Cam or Selection" );
			vbox->addWidget( butt );
			QObject::connect( butt, &QPushButton::clicked, []( bool ){ DoVisAnalyse(); } );
		}
	}
}

void DVisDrawer::ui_leaf_add( int leafnum, int nleafs, int nsurfs, int nshaders ){
	const int row = m_table->rowCount();
	m_table->insertRow( row );
	auto setColData = [&]( int col, int num ){
		auto *item = new QTableWidgetItem();
		item->setFlags( Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable );
		item->setData( Qt::ItemDataRole::DisplayRole, num );
		m_table->setItem( row, col, item );
	};
	setColData( 0, leafnum );
	setColData( 1, nleafs );
	setColData( 2, nsurfs );
	setColData( 3, nshaders );
}

void DVisDrawer::ui_show(){
	m_table->setSortingEnabled( true ); // enable once after construction for performance
	m_dialog->show();
}

void DVisDrawer::ui_leaf_show( int leafnum ){
	for( int i = 0; i < m_table->rowCount(); ++i )
		if( m_table->item( i, 0 )->data( Qt::ItemDataRole::DisplayRole ).toInt() == leafnum )
			return m_table->setCurrentCell( i, 0 );
}

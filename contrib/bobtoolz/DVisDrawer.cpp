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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DVisDrawer::DVisDrawer(){
	m_list = NULL;

	constructShaders();
	GlobalShaderCache().attachRenderable( *this );
}

DVisDrawer::~DVisDrawer(){
	GlobalShaderCache().detachRenderable( *this );
	destroyShaders();

	ClearPoints();
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
	for( const auto surf : *m_list ){
		const DMetaSurf& s = *surf;
		gl().glColor4f( s.colour[0], s.colour[1], s.colour[2], 0.5f );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( vec3_t ), s.verts );
		gl().glDrawElements( GL_TRIANGLES, GLsizei( s.indicesN ), GL_UNSIGNED_INT, s.indices );
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
	ClearPoints();
	m_list = pointList;
}

void DVisDrawer::ClearPoints(){
	if ( m_list ) {
		for ( auto deadPoint : *m_list )
			delete deadPoint;
		m_list->clear();
		delete m_list;
		m_list = 0;
	}
}

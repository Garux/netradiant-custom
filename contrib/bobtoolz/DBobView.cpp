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

// BobView.cpp: implementation of the DBobView class.
//
//////////////////////////////////////////////////////////////////////

#include "DBobView.h"
//#include "misc.h"
#include "funchandlers.h"

#include "iglrender.h"
#include "qerplugin.h"
#include "math/matrix.h"

#include "DEntity.h"
#include "DEPair.h"
#include "misc.h"
#include "dialogs/dialogs-gtk.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DBobView::DBobView(){
	nPathCount = 0;

	constructShaders();
	GlobalShaderCache().attachRenderable( *this );
}

DBobView::~DBobView(){
	GlobalShaderCache().detachRenderable( *this );
	destroyShaders();
	clear();
}

//////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////

void DBobView::render( RenderStateFlags state ) const {
	gl().glBegin( GL_LINE_STRIP );

	for ( int i = 0; i < nPathCount; i++ )
		gl().glVertex3fv( path[i] );

	gl().glEnd();
}

const char* DBobView_state_line = "$bobtoolz/bobview/line";
const char* DBobView_state_box = "$bobtoolz/bobview/box";

void DBobView::constructShaders(){
	OpenGLState state;
	GlobalOpenGLStateLibrary().getDefaultState( state );
	state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE | RENDER_DEPTHTEST;
	state.m_sort = OpenGLState::eSortFullbright;
	state.m_linewidth = 2;
	state.m_colour[0] = 1;
	state.m_colour[1] = 0;
	state.m_colour[2] = 0;
	state.m_colour[3] = 1;
	GlobalOpenGLStateLibrary().insert( DBobView_state_line, state );

	state.m_linewidth = 1;
	state.m_colour[0] = 0.25f;
	state.m_colour[1] = 0.75f;
	state.m_colour[2] = 0.75f;
	state.m_colour[3] = 1;
	GlobalOpenGLStateLibrary().insert( DBobView_state_box, state );

	m_shader_line = GlobalShaderCache().capture( DBobView_state_line );
	m_shader_box = GlobalShaderCache().capture( DBobView_state_box );
}

void DBobView::destroyShaders(){
	GlobalOpenGLStateLibrary().erase( DBobView_state_line );
	GlobalOpenGLStateLibrary().erase( DBobView_state_box );
	GlobalShaderCache().release( DBobView_state_line );
	GlobalShaderCache().release( DBobView_state_box );
}

Matrix4 g_transform_box1 = matrix4_translation_for_vec3( Vector3( 16.0f, 16.0f, 28.0f ) );
Matrix4 g_transform_box2 = matrix4_translation_for_vec3( Vector3( -16.0f, 16.0f, 28.0f ) );
Matrix4 g_transform_box3 = matrix4_translation_for_vec3( Vector3( 16.0f, -16.0f, -28.0f ) );
Matrix4 g_transform_box4 = matrix4_translation_for_vec3( Vector3( -16.0f, -16.0f, -28.0f ) );

void DBobView::renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
	if ( !path ) {
		return;
	}

	renderer.SetState( m_shader_line, Renderer::eWireframeOnly );
	renderer.SetState( m_shader_line, Renderer::eFullMaterials );
	renderer.addRenderable( *this, g_matrix4_identity );

	if ( m_bShowExtra ) {
		renderer.SetState( m_shader_box, Renderer::eWireframeOnly );
		renderer.SetState( m_shader_box, Renderer::eFullMaterials );
		renderer.addRenderable( *this, g_transform_box1 );
		renderer.addRenderable( *this, g_transform_box2 );
		renderer.addRenderable( *this, g_transform_box3 );
		renderer.addRenderable( *this, g_transform_box4 );
	}
}
void DBobView::renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
	renderSolid( renderer, volume );
}

#define LOCAL_GRAVITY -800.0f

bool DBobView::CalculateTrajectory( vec3_t start, vec3_t apex, float multiplier, int points, float varGravity ){
	if ( apex[2] <= start[2] ) {
		path.reset();
		return false;
	}
	// ----think q3a actually would allow these
	//scrub that, coz the plugin wont :]

	vec3_t dist, speed;
	VectorSubtract( apex, start, dist );

	vec_t speed_z = (float)sqrt( -2 * LOCAL_GRAVITY * dist[2] );
	float flight_time = -speed_z / LOCAL_GRAVITY;


	VectorScale( dist, 1 / flight_time, speed );
	speed[2] = speed_z;

//	Sys_Printf( "Speed: (%.4f %.4f %.4f)\n", speed[0], speed[1], speed[2] );

	path.reset( new vec3_t[points] );

	float interval = multiplier * flight_time / points;
	for ( int i = 0; i < points; i++ )
	{
		float ltime = interval * i;

		VectorScale( speed, ltime, path[i] );
		VectorAdd( path[i], start, path[i] );

		// could do this all with vectors
		// vGrav = { 0, 0, -800.0f }
		// VectorScale( vGrav, 0.5f*ltime*ltime, vAdd );
		// VectorScale( speed, ltime, pPath[i] );
		// _VectorAdd( pPath[i], start, pPath[i] )
		// _VectorAdd( pPath[i], vAdd, pPath[i] )

		path[i][2] = start[2] + ( speed_z * ltime ) + ( varGravity * 0.5f * ltime * ltime );
	}

	return true;
}

void DBobView::Begin( const char *targetName, float multiplier, int points, float varGravity, bool bShowExtra ){
	strcpy( this->targetName, targetName );

	fMultiplier = multiplier;
	fVarGravity = varGravity;
	nPathCount = points;
	m_bShowExtra = bShowExtra;

	if ( !UpdatePath() ) {
		globalErrorStream() << "Initialization Failure in DBobView::Begin\n";
		g_PathView.reset();
	}
	globalOutputStream() << "Initialization of Path Plotter succeeded.\n";
}

bool DBobView::UpdatePath(){
	vec3_t start, apex;

	if ( GetEntityCentre( targetName, true, start )
	  && GetEntityCentre( targetName, false, apex ) ) {
		CalculateTrajectory( start, apex, fMultiplier, nPathCount, fVarGravity );
		return true;
	}
	return false;
}

void DBobView_setEntity( Entity& entity, float multiplier, int points, float varGravity, bool bNoUpdate, bool bShowExtra ){
	DEntity trigger;
	trigger.LoadEPairList( &entity );

	if ( trigger.m_Classname == "trigger_push" ) {
		if ( DEPair* trigger_ep = trigger.FindEPairByKey( "target" ) ) {
			const scene::Path* entTarget = FindEntityFromTargetname( trigger_ep->value.c_str() );
			if ( entTarget ) {
				g_PathView.reset(); // delete old at first
				g_PathView.reset( new DBobView );

				Entity* target = Node_getEntity( entTarget->top() );
				if ( target != 0 ) {
					if ( !bNoUpdate ) {
						g_PathView->target = target;
						target->attach( *g_PathView );
					}
					g_PathView->Begin( trigger_ep->value.c_str(), multiplier, points, varGravity, bShowExtra );
				}
				else{
					globalErrorStream() << "bobToolz PathPlotter: trigger_push ARGH\n";
				}
			}
			else{
				globalErrorStream() << "bobToolz PathPlotter: trigger_push target could not be found..\n";
			}
		}
		else{
			globalErrorStream() << "bobToolz PathPlotter: Entity must have a target.\n";
		}
	}
	else{
		globalErrorStream() << "bobToolz PathPlotter: You must select a 'trigger_push' entity..\n";
	}
}
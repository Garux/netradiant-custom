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

#include "portals.h"
#include <cstring>
#include <cstdlib>
#ifndef __APPLE__
#include <search.h>
#endif
#include <cstdio>

#include "iglrender.h"
#include "cullable.h"

#include "prtview.h"

#define LINE_BUF 4096

CPortals portals;
CPortalsRender render;


CBspPortal::CBspPortal(){
}

CBspPortal::~CBspPortal(){
}

bool CBspPortal::Build( char *def ){
	char *c = def;
	unsigned int point_count;
	int dummy1, dummy2;
	int res_cnt = 0;

	if ( portals.hint_flags ) {
		res_cnt = sscanf( def, "%u %d %d %d", &point_count, &dummy1, &dummy2, (int *)&hint );
	}
	else
	{
		sscanf( def, "%u", &point_count );
		hint = false;
	}

	if ( point_count < 3 || ( portals.hint_flags && res_cnt < 4 ) ) {
		return false;
	}

	point.resize( point_count );
	inner_point.reserve( point_count );

	for ( auto& p : point )
	{
		for (; *c != 0 && *c != '('; c++ ){};

		if ( *c == 0 ) {
			return false;
		}

		c++;

		sscanf( c, "%f %f %f", &p.x(), &p.y(), &p.z() );

		center += p;

		if ( &p == &point.front() ) {
			min = p;
			max = p;
		}
		else
		{
			for ( size_t i = 0; i < 3; ++i )
			{
				min[i] = std::min( min[i], p[i] );
				max[i] = std::max( max[i], p[i] );
			}
		}
	}

	center /= point.size();

	for ( const auto& p : point )
	{
		inner_point.push_back( ( center * 0.01f ) + ( p * 0.99f ) );
	}

	fp_color_random[0] = ( rand() & 0xff ) / 255.0f;
	fp_color_random[1] = ( rand() & 0xff ) / 255.0f;
	fp_color_random[2] = ( rand() & 0xff ) / 255.0f;
	fp_color_random[3] = 1.0f;

	return true;
}

CPortals::CPortals(){
}

CPortals::~CPortals(){
}

void CPortals::Purge(){
	portal.clear();

	/*
	   delete[] node;
	   node = NULL;
	   node_count = 0;
	 */
}

void CPortals::Load(){
	char buf[LINE_BUF + 1];
	unsigned int portal_count, node_count;

	memset( buf, 0, LINE_BUF + 1 );

	Purge();

	globalOutputStream() << MSG_PREFIX "Loading portal file " << fn << ".\n";

	FILE *in;

	in = fopen( fn.c_str(), "rt" );

	if ( in == NULL ) {
		globalErrorStream() << "  ERROR - could not open file.\n";

		return;
	}

	#define GETLINE \
	if ( !fgets( buf, LINE_BUF, in ) ) { \
		fclose( in ); \
		globalErrorStream() << "  ERROR - File ended prematurely.\n"; \
		return; \
	}

	GETLINE;

	if ( strncmp( "PRT1-AM", buf, 7 ) == 0 ) {
		format = PRT1AM;
	}
	else if ( strncmp( "PRT1", buf, 4 ) == 0 ) {
		format = PRT1;
	}
	else if ( strncmp( "PRT2", buf, 4 ) == 0 ) {
		format = PRT2;
	}
	else {
		fclose( in );

		globalErrorStream() << "  ERROR - File header indicates wrong file type (should be \"PRT1\" or \"PRT2\" or \"PRT1-AM\").\n";

		return;
	}

	switch ( format )
	{
	case PRT1:
		{
			GETLINE; //leafs count https://github.com/kduske/TrenchBroom/issues/1157 //clusters in q3
			sscanf( buf, "%u", &node_count );
			GETLINE; //portals count
			sscanf( buf, "%u", &portal_count );
		}
		break;
	case PRT2:
		{
			GETLINE; //leafs count
			sscanf( buf, "%u", &node_count );
			GETLINE; //clusters count
			GETLINE; //portals count
			sscanf( buf, "%u", &portal_count );

		}
		break;
	case PRT1AM:
		{
			GETLINE; //clusters count
			GETLINE; //portals count
			sscanf( buf, "%u", &portal_count );
			GETLINE; //leafs count
			sscanf( buf, "%u", &node_count );
		}
		break;
	}

/*
	if(node_count > 0xFFFF)
	{
		fclose(in);

		Purge();

		globalErrorStream() << "  ERROR - Extreme number of nodes, aborting.\n";

		return;
	}
 */

	if ( portal_count > 0xFFFF ) {
		fclose( in );

		globalErrorStream() << "  ERROR - Extreme number of portals, aborting.\n";

		return;
	}

	if ( portal_count == 0 ) {
		fclose( in );

		globalErrorStream() << "  ERROR - number of portals equals 0, aborting.\n";

		return;
	}

//	node = new CBspNode[node_count];
	portal.resize( portal_count );

	unsigned test_vals_1, test_vals_2;

	hint_flags = false;

	for ( unsigned int n = 0; n < portal_count; )
	{
		if ( !fgets( buf, LINE_BUF, in ) ) {
			fclose( in );

			Purge();

			globalErrorStream() << "  ERROR - Could not find information for portal number " << n + 1 << " of " << portal_count << ".\n";

			return;
		}

		if ( !portal[n].Build( buf ) ) {
			if ( 0 == n && sscanf( buf, "%d %d", &test_vals_1, &test_vals_2 ) == 1 ) { // skip additional counts of later data, not needed
				// We can count on hint flags being in the file
				hint_flags = true;
				continue;
			}

			fclose( in );

			globalErrorStream() << "  ERROR - Information for portal number " << n + 1 << " of " << portal_count << " is not formatted correctly.\n";

			Purge();

			return;
		}

		++n;
	}

	fclose( in );

	globalOutputStream() << "  " << portal_count << " portals read in.\n";
}

#include "math/matrix.h"

const char* g_state_solid = "$plugins/prtview/solid";
const char* g_state_solid_outline = "$plugins/prtview/solid_outline";
const char* g_state_wireframe = "$plugins/prtview/wireframe";
Shader* g_shader_solid = 0;
Shader* g_shader_solid_outline = 0;
Shader* g_shader_wireframe = 0;

void Portals_constructShaders(){
	OpenGLState state;
	GlobalOpenGLStateLibrary().getDefaultState( state );
	state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
	state.m_sort = OpenGLState::eSortOverlayFirst;
	state.m_linewidth = portals.width_2d;
	state.m_colour[0] = portals.fp_color_2d[0];
	state.m_colour[1] = portals.fp_color_2d[1];
	state.m_colour[2] = portals.fp_color_2d[2];
	state.m_colour[3] = portals.fp_color_2d[3];

	GlobalOpenGLStateLibrary().insert( g_state_wireframe, state );

	GlobalOpenGLStateLibrary().getDefaultState( state );
	state.m_state = RENDER_FILL | RENDER_BLEND | RENDER_COLOURWRITE | RENDER_COLOURCHANGE | RENDER_SMOOTH;

	switch ( portals.zbuffer )
	{
	case 1:
		state.m_state |= RENDER_DEPTHTEST;
		break;
	case 2:
		break;
	default:
		state.m_state |= RENDER_DEPTHTEST;
		state.m_state |= RENDER_DEPTHWRITE;
	}

	if ( portals.fog ) {
		state.m_state |= RENDER_FOG;

		state.m_fog.mode = GL_EXP;
		state.m_fog.density = 0.001f;
		state.m_fog.start = 10.0f;
		state.m_fog.end = 10000.0f;
		state.m_fog.index = 0;
		state.m_fog.colour[0] = portals.fp_color_fog[0];
		state.m_fog.colour[1] = portals.fp_color_fog[1];
		state.m_fog.colour[2] = portals.fp_color_fog[2];
		state.m_fog.colour[3] = portals.fp_color_fog[3];
	}

	GlobalOpenGLStateLibrary().insert( g_state_solid, state );

	GlobalOpenGLStateLibrary().getDefaultState( state );
	state.m_state = RENDER_COLOURWRITE | RENDER_DEPTHWRITE;
	state.m_sort = OpenGLState::eSortOverlayFirst;
	state.m_linewidth = portals.width_3d;
	state.m_colour[0] = portals.fp_color_3d[0];
	state.m_colour[1] = portals.fp_color_3d[1];
	state.m_colour[2] = portals.fp_color_3d[2];
	state.m_colour[3] = portals.fp_color_3d[3];

	switch ( portals.zbuffer )
	{
	case 1:
		state.m_state |= RENDER_DEPTHTEST;
		break;
	case 2:
		break;
	default:
		state.m_state |= RENDER_DEPTHTEST;
		state.m_state |= RENDER_DEPTHWRITE;
	}

	if ( portals.fog ) {
		state.m_state |= RENDER_FOG;

		state.m_fog.mode = GL_EXP;
		state.m_fog.density = 0.001f;
		state.m_fog.start = 10.0f;
		state.m_fog.end = 10000.0f;
		state.m_fog.index = 0;
		state.m_fog.colour[0] = portals.fp_color_fog[0];
		state.m_fog.colour[1] = portals.fp_color_fog[1];
		state.m_fog.colour[2] = portals.fp_color_fog[2];
		state.m_fog.colour[3] = portals.fp_color_fog[3];
	}

	GlobalOpenGLStateLibrary().insert( g_state_solid_outline, state );

	g_shader_solid = GlobalShaderCache().capture( g_state_solid );
	g_shader_solid_outline = GlobalShaderCache().capture( g_state_solid_outline );
	g_shader_wireframe = GlobalShaderCache().capture( g_state_wireframe );
}

void Portals_destroyShaders(){
	GlobalShaderCache().release( g_state_solid );
	GlobalShaderCache().release( g_state_solid_outline );
	GlobalShaderCache().release( g_state_wireframe );
	GlobalOpenGLStateLibrary().erase( g_state_solid );
	GlobalOpenGLStateLibrary().erase( g_state_solid_outline );
	GlobalOpenGLStateLibrary().erase( g_state_wireframe );
}

void Portals_shadersChanged(){
	Portals_destroyShaders();
	portals.FixColors();
	Portals_constructShaders();
}

void CPortals::FixColors(){
	fp_color_2d[0] = RGB_UNPACK_R( color_2d ) / 255.0f;
	fp_color_2d[1] = RGB_UNPACK_G( color_2d ) / 255.0f;
	fp_color_2d[2] = RGB_UNPACK_B( color_2d ) / 255.0f;
	fp_color_2d[3] = 1.0f;

	fp_color_3d[0] = RGB_UNPACK_R( color_3d ) / 255.0f;
	fp_color_3d[1] = RGB_UNPACK_G( color_3d ) / 255.0f;
	fp_color_3d[2] = RGB_UNPACK_B( color_3d ) / 255.0f;
	fp_color_3d[3] = 1.0f;

	fp_color_fog[0] = RGB_UNPACK_R( color_fog ) / 255.0f;
	fp_color_fog[1] = RGB_UNPACK_G( color_fog ) / 255.0f;
	fp_color_fog[2] = RGB_UNPACK_B( color_fog ) / 255.0f;
	fp_color_fog[3] = 1.0f;
}

void CPortalsRender::renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
	if ( !portals.show_2d || portals.portal.empty() ) {
		return;
	}

	renderer.SetState( g_shader_wireframe, Renderer::eWireframeOnly );

	renderer.addRenderable( m_drawWireframe, g_matrix4_identity );
}

void CPortalsDrawWireframe::render( RenderStateFlags state ) const {
	for ( const auto& prt : portals.portal )
	{
		gl().glBegin( GL_LINE_LOOP );

		for ( const auto& p : prt.point )
			gl().glVertex3fv( p.data() );

		gl().glEnd();
	}
}

CubicClipVolume calculateCubicClipVolume( const Matrix4& viewproj ){
	CubicClipVolume clip;
	clip.cam = vector4_projected(
	               matrix4_transformed_vector4(
	                   matrix4_full_inverse( viewproj ),
	                   Vector4( 0, 0, -1, 1 )
	               )
	           );
	clip.min = clip.cam + Vector3( portals.clip_range );
	clip.max = clip.cam - Vector3( portals.clip_range );
	return clip;
}

void CPortalsRender::renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
	if ( !portals.show_3d || portals.portal.empty() ) {
		return;
	}

	CubicClipVolume clip = calculateCubicClipVolume( matrix4_multiplied_by_matrix4( volume.GetProjection(), volume.GetModelview() ) );

	if ( portals.polygons ) {
		renderer.SetState( g_shader_solid, Renderer::eWireframeOnly );
		renderer.SetState( g_shader_solid, Renderer::eFullMaterials );

		m_drawSolid.clip = clip;
		renderer.addRenderable( m_drawSolid, g_matrix4_identity );
	}

	if ( portals.lines ) {
		renderer.SetState( g_shader_solid_outline, Renderer::eWireframeOnly );
		renderer.SetState( g_shader_solid_outline, Renderer::eFullMaterials );

		m_drawSolidOutline.clip = clip;
		renderer.addRenderable( m_drawSolidOutline, g_matrix4_identity );
	}
}

void CPortalsDrawSolid::render( RenderStateFlags state ) const {
	const float opacity = portals.opacity_3d / 100.0f;

	if ( portals.zbuffer != 0 ) {
		portals.portal_sort.clear();
		portals.portal_sort.reserve( portals.portal.size() );
		for ( auto& prt : portals.portal )
		{
			prt.dist = vector3_length_squared( clip.cam - prt.center );

			portals.portal_sort.push_back( &prt );
		}

		std::sort( portals.portal_sort.begin(), portals.portal_sort.end(), []( const CBspPortal *a, const CBspPortal *b ){
			return a->dist < b->dist;
		} );

		for ( const auto prt : portals.portal_sort )
		{
			if( ( !prt->hint && portals.draw_nonhints )
			  || ( prt->hint && portals.draw_hints ) )
			{
				if ( portals.clip ) {
					if ( clip.min[0] < prt->min[0]
					  || clip.min[1] < prt->min[1]
					  || clip.min[2] < prt->min[2]
					  || clip.max[0] > prt->max[0]
					  || clip.max[1] > prt->max[1]
					  || clip.max[2] > prt->max[2]
					) continue;
				}

				gl().glColor4f( prt->fp_color_random[0], prt->fp_color_random[1], prt->fp_color_random[2], opacity );

				gl().glBegin( GL_POLYGON );

				for ( const auto& p : prt->point )
					gl().glVertex3fv( p.data() );

				gl().glEnd();
			}
		}
	}
	else
	{
		for ( const auto& prt : portals.portal )
		{
			if( ( !prt.hint && portals.draw_nonhints )
			  || ( prt.hint && portals.draw_hints ) )
			{
				if ( portals.clip ) {
					if ( clip.min[0] < prt.min[0]
					  || clip.min[1] < prt.min[1]
					  || clip.min[2] < prt.min[2]
					  || clip.max[0] > prt.max[0]
					  || clip.max[1] > prt.max[1]
					  || clip.max[2] > prt.max[2]
					) continue;
				}

				gl().glColor4f( prt.fp_color_random[0], prt.fp_color_random[1], prt.fp_color_random[2], opacity );

				gl().glBegin( GL_POLYGON );

				for ( const auto& p : prt.point )
					gl().glVertex3fv( p.data() );

				gl().glEnd();
			}
		}
	}
}

void CPortalsDrawSolidOutline::render( RenderStateFlags state ) const {
	for ( const auto& prt : portals.portal )
	{
		if( ( !prt.hint && portals.draw_nonhints )
			|| ( prt.hint && portals.draw_hints ) )
		{
			if ( portals.clip ) {
				if ( clip.min[0] < prt.min[0]
				  || clip.min[1] < prt.min[1]
				  || clip.min[2] < prt.min[2]
				  || clip.max[0] > prt.max[0]
				  || clip.max[1] > prt.max[1]
				  || clip.max[2] > prt.max[2]
				) continue;
			}

			gl().glBegin( GL_LINE_LOOP );

			for ( const auto& p : prt.inner_point )
				gl().glVertex3fv( p.data() );

			gl().glEnd();
		}
	}
}

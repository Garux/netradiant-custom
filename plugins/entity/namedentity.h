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

#if !defined( INCLUDED_NAMEDENTITY_H )
#define INCLUDED_NAMEDENTITY_H

#include "entitylib.h"
#include "eclasslib.h"
#include "generic/callback.h"
#include "nameable.h"
#include "entity.h" //g_showTargetNames

#include <set>

class NameCallbackSet
{
typedef std::set<NameCallback> NameCallbacks;
NameCallbacks m_callbacks;
public:
void insert( const NameCallback& callback ){
	m_callbacks.insert( callback );
}
void erase( const NameCallback& callback ){
	m_callbacks.erase( callback );
}
void changed( const char* name ) const {
	for ( NameCallbacks::const_iterator i = m_callbacks.begin(); i != m_callbacks.end(); ++i )
	{
		( *i )( name );
	}
}
};

class NamedEntity : public Nameable
{
EntityKeyValues& m_entity;
NameCallbackSet m_changed;
CopiedString m_name;
public:
NamedEntity( EntityKeyValues& entity ) : m_entity( entity ){
}
const char* name() const {
	if ( string_empty( m_name.c_str() ) ) {
		return m_entity.getEntityClass().name();
	}
	return m_name.c_str();
}
const char* classname() const {
	return m_entity.getEntityClass().name();
}
const Colour3& color() const {
	return m_entity.getEntityClass().color;
}
void attach( const NameCallback& callback ){
	m_changed.insert( callback );
}
void detach( const NameCallback& callback ){
	m_changed.erase( callback );
}

void identifierChanged( const char* value ){
	if ( string_empty( value ) ) {
		m_changed.changed( m_entity.getEntityClass().name() );
	}
	else
	{
		m_changed.changed( value );
	}
	m_name = value;
}
typedef MemberCaller1<NamedEntity, const char*, &NamedEntity::identifierChanged> IdentifierChangedCaller;
};


#include "renderable.h"
#include "pivot.h"
#include "math/frustum.h"

class RenderableNamedEntity : public OpenGLRenderable {
	enum ENameMode{
		eNameNormal = 0,
		eNameSelected = 1,
		eNameChildSelected = 2,
	};
	mutable ENameMode m_nameMode;

	NamedEntity& m_named;
	const Vector3& m_position;
	GLuint m_tex;
	int m_width;
	int m_height;
	mutable float m_screenPos[2];
public:
	typedef Static<Shader*, RenderableNamedEntity> StaticShader;
	static Shader* getShader() {
		return StaticShader::instance();
	}
	RenderableNamedEntity( NamedEntity& named, const Vector3& position )
		: m_named( named ), m_position( position ), m_tex( 0 ) {
		construct_textures( g_showTargetNames ? m_named.name() : m_named.classname() );
		m_named.attach( IdentifierChangedCaller( *this ) );
	}
private:
	void construct_textures( const char* name ){
		glGenTextures( 1, &m_tex );
		if( m_tex > 0 ) {
			unsigned int colour[3];
			colour[0] = static_cast<unsigned int>( m_named.color()[0] * 255.f );
			colour[1] = static_cast<unsigned int>( m_named.color()[1] * 255.f );
			colour[2] = static_cast<unsigned int>( m_named.color()[2] * 255.f );
			GlobalOpenGL().m_font->renderString( name, m_tex, colour, m_width, m_height );
		}
	}
	void delete_textures(){
		glDeleteTextures( 1, &m_tex );
		m_tex = 0;
	}
	void setMode( bool selected, bool childSelected ) const{
		if( selected ){
			m_nameMode = eNameSelected;
		}
		else if( childSelected ){
			m_nameMode = eNameChildSelected;
		}
		else{
			m_nameMode = eNameNormal;
		}
	}
public:
	void render( RenderStateFlags state ) const {
		if( m_tex > 0 ){
			glBindTexture( GL_TEXTURE_2D, m_tex );

			//Here we draw the texturemaped quads.
			//The bitmap that we got from FreeType was not
			//oriented quite like we would like it to be,
			//so we need to link the texture to the quad
			//so that the result will be properly aligned.
			glBegin( GL_QUADS );
			float xoffset0 = m_nameMode / 3.f;
			float xoffset1 = ( m_nameMode + 1 ) / 3.f;
			glTexCoord2f( xoffset0, 1 );
			glVertex2f( m_screenPos[0], m_screenPos[1] );
			glTexCoord2f( xoffset0, 0 );
			glVertex2f( m_screenPos[0], m_screenPos[1] + m_height + .01f );
			glTexCoord2f( xoffset1, 0 );
			glVertex2f( m_screenPos[0] + m_width + .01f, m_screenPos[1] + m_height + .01f );
			glTexCoord2f( xoffset1, 1 );
			glVertex2f( m_screenPos[0] + m_width + .01f, m_screenPos[1] );
			glEnd();

			glBindTexture( GL_TEXTURE_2D, 0 );
		}
	}
	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected, bool childSelected = false ) const{
		setMode( selected, childSelected );

		if( m_nameMode == eNameNormal && volume.fill() ){
//			globalOutputStream() << localToWorld << " localToWorld\n";
//			globalOutputStream() << volume.GetModelview() << " modelview\n";
//			globalOutputStream() << volume.GetProjection() << " Projection\n";
//			globalOutputStream() << volume.GetViewport() << " Viewport\n";
			Matrix4 viewproj = matrix4_multiplied_by_matrix4( volume.GetProjection(), volume.GetModelview() );
			Vector3 viewer = vector4_to_vector3( viewer_from_viewproj( viewproj ) );
			Vector3 pos_in_world = matrix4_transformed_point( localToWorld, m_position );
			if( vector3_length_squared( pos_in_world - viewer ) > g_showNamesDist * g_showNamesDist ){
				return;
			}
			//globalOutputStream() << viewer[0] << " " << viewer[1] << " " << viewer[2] << " Viewer\n";
			//globalOutputStream() << pos_in_world[0] << " " << pos_in_world[1] << " " << pos_in_world[2] << " position\n";
			//globalOutputStream() << m_position[0] << " " << m_position[1] << " " << m_position[2] << " position\n";
		}


		Vector4 position;
		position[0] = m_position[0];
		position[1] = m_position[1];
		position[2] = m_position[2];
		position[3] = 1.f;

#if 0
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " position\n";
		matrix4_transform_vector4( localToWorld, position );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " localToWorld\n";
		matrix4_transform_vector4( volume.GetModelview(), position );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Modelview\n";
		matrix4_transform_vector4( volume.GetProjection(), position );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Projection\n";
		position[0] /= position[3];
		position[1] /= position[3];
		position[2] /= position[3];
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Projection division\n";
		matrix4_transform_vector4( volume.GetViewport(), position );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Viewport\n";

#else
		Matrix4 object2screen = volume.GetProjection();
		matrix4_multiply_by_matrix4( object2screen, volume.GetModelview() );
		matrix4_multiply_by_matrix4( object2screen, localToWorld );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " position\n";
		matrix4_transform_vector4( object2screen, position );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Projection\n";
		position[0] /= position[3];
		position[1] /= position[3];
		position[2] /= position[3];
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Projection division\n";
		matrix4_transform_vector4( volume.GetViewport(), position );
//			globalOutputStream() << position[0] << " " << position[1] << " " << position[2] << " " << position[3] << " Viewport\n";
#endif

			//globalOutputStream() << volume.GetViewport()[0] << " " << volume.GetViewport()[5] << " Viewport size\n";

		m_screenPos[0] = position[0];
		m_screenPos[1] = position[1];
			//globalOutputStream() << m_screenPos[0] << " " << m_screenPos[1] << "\n";

		renderer.PushState();

//		Pivot2World_viewplaneSpace( m_localToWorld, localToWorld, volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );

		renderer.Highlight( Renderer::ePrimitive, false );
		renderer.Highlight( Renderer::eFace, false );
		renderer.SetState( getShader(), Renderer::eWireframeOnly );
		renderer.SetState( getShader(), Renderer::eFullMaterials );

//		m_localToWorld = volume.GetViewport();
//		matrix4_full_invert( m_localToWorld );

		renderer.addRenderable( *this, g_matrix4_identity );

		renderer.PopState();
	}
	~RenderableNamedEntity(){
		m_named.detach( IdentifierChangedCaller( *this ) );
		delete_textures();
	}
	void identifierChanged( const char* value ){
		delete_textures();
		construct_textures( g_showTargetNames ? value : m_named.classname() );
	}
	typedef MemberCaller1<RenderableNamedEntity, const char*, &RenderableNamedEntity::identifierChanged> IdentifierChangedCaller;
};



/*
class RenderableNamedEntity : public OpenGLRenderable
{
const NamedEntity& m_named;
const Vector3& m_position;
public:
RenderableNamedEntity( const NamedEntity& named, const Vector3& position )
	: m_named( named ), m_position( position ){
}
void render( RenderStateFlags state ) const {
	glRasterPos3fv( vector3_to_array( m_position ) );
	GlobalOpenGL().drawString( g_showTargetNames ? m_named.name() : m_named.classname() );
}
};
*/


#endif

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
#include "entity.h"

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
#include "cullable.h"
#include "render.h"

class RenderableNamedEntity
{
	enum ENameMode{
		eNameNormal = 0,
		eNameSelected = 1,
		eNameChildSelected = 2,
	};
	NamedEntity& m_named;
	const Vector3& m_position;
	mutable RenderTextLabel m_label;
	const char* const m_exclude;
public:
	typedef Static<Shader*, RenderableNamedEntity> StaticShader;
	static Shader* getShader() {
		return StaticShader::instance();
	}
	RenderableNamedEntity( NamedEntity& named, const Vector3& position, const char* exclude = 0 )
		: m_named( named ), m_position( position ), m_exclude( exclude ) {
		identifierChanged( m_named.name() );
		m_named.attach( IdentifierChangedCaller( *this ) );
	}
	bool excluded_not() const {
		return m_label.tex > 0;
	}
public:
	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected, bool childSelected = false ) const {
		m_label.subTex = selected? eNameSelected : childSelected? eNameChildSelected : eNameNormal;

		if( volume.fill() ){
			const Matrix4& viewproj = volume.GetViewMatrix();
			const Vector3 pos_in_world = matrix4_transformed_point( localToWorld, m_position );
			if( viewproj[3] * pos_in_world[0] + viewproj[7] * pos_in_world[1] + viewproj[11] * pos_in_world[2] + viewproj[15] < 0.005f ) //w < 0: behind nearplane
				return;
			if( m_label.subTex == eNameNormal && vector3_length_squared( pos_in_world - volume.getViewer() ) > static_cast<float>( g_showNamesDist ) * static_cast<float>( g_showNamesDist ) )
				return;
		}

		Vector4 position( m_position, 1.f );
		Matrix4 object2screen( volume.GetViewMatrix() );
		matrix4_multiply_by_matrix4( object2screen, localToWorld );
		matrix4_transform_vector4( object2screen, position );
//			globalOutputStream() << position << " Projection\n";
		position.x() /= position.w();
		position.y() /= position.w();
//		position.z() /= position.w();
//			globalOutputStream() << position << " Projection division\n";
		matrix4_transform_vector4( volume.GetViewport(), position );
//			globalOutputStream() << position << " Viewport\n";
//			globalOutputStream() << volume.GetViewport()[0] << " " << volume.GetViewport()[5] << " Viewport size\n";
		m_label.screenPos.x() = position.x();
		m_label.screenPos.y() = position.y();
//			globalOutputStream() << m_label.screenPos << "\n";

		renderer.PushState();

		renderer.Highlight( Renderer::ePrimitive, false );
		renderer.Highlight( Renderer::eFace, false );
		renderer.SetState( getShader(), Renderer::eWireframeOnly );
		renderer.SetState( getShader(), Renderer::eFullMaterials );

		renderer.addRenderable( m_label, g_matrix4_identity );

		renderer.PopState();
	}
	~RenderableNamedEntity(){
		m_named.detach( IdentifierChangedCaller( *this ) );
	}
	void identifierChanged( const char* value ){
		m_label.texFree();
		if( m_exclude && string_equal( m_exclude, value ) )
			return;
		m_label.texAlloc( value, m_named.color() );
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

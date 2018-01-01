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

// DBobView.h: interface for the DBobView class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "ientity.h"
#include "irender.h"
#include "renderable.h"
#include "mathlib.h"

class Shader;

class DBobView : public Renderable, public OpenGLRenderable, public Entity::Observer
{
	Shader* m_shader_line;
	Shader* m_shader_box;
public:
	DBobView();
	virtual ~DBobView();

protected:
	std::unique_ptr<vec3_t[]> path;
public:
	bool m_bShowExtra;
	float fVarGravity;
	float fMultiplier;
	int nPathCount;

	Entity* target{};

	bool UpdatePath();
	char targetName[256];
	void Begin( const char*, float, int, float, bool );
	bool CalculateTrajectory( vec3_t, vec3_t, float, int, float );

	void render( RenderStateFlags state ) const override;
	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const override;
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const override;

	void constructShaders();
	void destroyShaders();

	void valueChanged( const char* value ){
		UpdatePath();
	}
	typedef MemberCaller<DBobView, void(const char*), &DBobView::valueChanged> ValueChangedCaller;
	void insert( const char* key, EntityKeyValue& value ) override {
		value.attach( ValueChangedCaller( *this ) );
	}
	void erase( const char* key, EntityKeyValue& value ) override {
		value.detach( ValueChangedCaller( *this ) );
	}
	void clear() override {
		if ( target != 0 ) {
			target->detach( *this );
			target = 0;
		}
	}
};

void DBobView_setEntity( Entity& entity, float multiplier, int points, float varGravity, bool bNoUpdate, bool bShowExtra );

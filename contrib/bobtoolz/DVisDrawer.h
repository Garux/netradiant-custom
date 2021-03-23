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

#if !defined( AFX_VISDRAWER_H__6E36062A_EF0B_11D4_ACF7_004095A18133__INCLUDED_ )
#define AFX_VISDRAWER_H__6E36062A_EF0B_11D4_ACF7_004095A18133__INCLUDED_

#if _MSC_VER > 1000

#pragma once
#endif // _MSC_VER > 1000

#include <list>
#include "renderable.h"
#include "irender.h"
#include "mathlib.h"

class DMetaSurf
{
public:
	DMetaSurf() = delete;
	DMetaSurf( int numverts, int numindices ){
		verts = new vec3_t[numverts];
		indices = new unsigned int[numindices];
		indicesN = numindices;
	}
	~DMetaSurf(){
		delete[] verts;
		delete[] indices;
	}
	vec3_t* verts;
	unsigned int* indices;
	int indicesN;
	vec3_t colour;
};

typedef std::list<DMetaSurf*> DMetaSurfaces;

class DVisDrawer : public Renderable, public OpenGLRenderable
{
	Shader* m_shader_solid;
	Shader* m_shader_wireframe;
public:
	DVisDrawer();
	virtual ~DVisDrawer();

protected:
	DMetaSurfaces* m_list;
	int refCount;
public:
	void ClearPoints();
	void SetList( DMetaSurfaces* pointList );

	void render( RenderStateFlags state ) const;
	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const;
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const;

	void constructShaders();
	void destroyShaders();

};

#endif // !defined(AFX_VISDRAWER_H__6E36062A_EF0B_11D4_ACF7_004095A18133__INCLUDED_)

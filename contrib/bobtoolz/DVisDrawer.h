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

#include <vector>
#include <utility>
#include "renderable.h"
#include "irender.h"
#include "mathlib.h"

class DMetaSurf
{
public:
	DMetaSurf( int numverts, int numindices )
	:	verts( new vec3_t[numverts] ),
		indices( new unsigned int[numindices] ),
		indicesN( numindices )
	{}
	DMetaSurf( DMetaSurf&& other ) noexcept
	:	verts( std::exchange( other.verts, nullptr ) ),
		indices( std::exchange( other.indices, nullptr ) ),
		indicesN( other.indicesN ),
		colour{ other.colour[0], other.colour[1], other.colour[2] }
	{}
	~DMetaSurf(){
		delete[] verts;
		delete[] indices;
	}
	vec3_t* verts;
	unsigned int* indices;
	int indicesN;
	vec3_t colour;
};

using DMetaSurfaces = std::vector<DMetaSurf>;

class DVisDrawer : public Renderable, public OpenGLRenderable
{
	Shader* m_shader_solid;
	Shader* m_shader_wireframe;
	class QDialog *m_dialog;
	class QTableWidget *m_table;
	DMetaSurfaces* m_list;
	std::vector<class DWinding> m_windings;
	bool m_colorPerSurf;
public:
	DVisDrawer();
	virtual ~DVisDrawer();

	void ClearPoints();
private:
	void SetList( DMetaSurfaces* pointList );
	void ui_create();
public:
	void ui_leaf_add( int leafnum, int nleafs, int nsurfs, int nshaders );
	void ui_show();
	void ui_leaf_show( int leafnum );

	void render( RenderStateFlags state ) const override;
	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const override;
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const override;
private:
	void constructShaders();
	void destroyShaders();
};

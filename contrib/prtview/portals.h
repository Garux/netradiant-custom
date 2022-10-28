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

#pragma once

#include "irender.h"
#include "renderable.h"
#include "math/vector.h"
#include "string/string.h"
#include <vector>


class CBspPortal {
public:
	CBspPortal();
	~CBspPortal();

protected:

public:
	Vector3 center{ 0 };
	std::vector<Vector3> point;
	std::vector<Vector3> inner_point;
	float fp_color_random[4];
	Vector3 min;
	Vector3 max;
	float dist;
	bool hint;

	bool Build( char *def );
};

using PackedColour = std::uint32_t;
#define RGB_PACK( r, g, b ) ( (std::uint32_t)( ( (std::uint8_t)( r ) | ( (std::uint16_t)( (std::uint8_t)( g ) ) << 8 ) ) | ( ( (std::uint32_t)(std::uint8_t)( b ) ) << 16 ) ) )
#define RGB_UNPACK_R( rgb )      ( (std::uint8_t)( rgb ) )
#define RGB_UNPACK_G( rgb )      ( (std::uint8_t)( ( (std::uint16_t)( rgb ) ) >> 8 ) )
#define RGB_UNPACK_B( rgb )      ( (std::uint8_t)( ( rgb ) >> 16 ) )


class CPortals {
	enum ePrtFormat {
		PRT1,
		PRT2,
		PRT1AM
	} format;

public:

	CPortals();
	~CPortals();

	void Load();     // use filename in fn
	void Purge();

	void FixColors();

	CopiedString fn;

	int zbuffer;
	bool polygons;
	bool lines;
	bool show_3d;
	bool fog;
	PackedColour color_3d;
	int width_3d;
	float fp_color_3d[4];
	PackedColour color_fog;
	float fp_color_fog[4];
	int opacity_3d;
	int clip_range;
	bool clip;

	bool draw_hints;
	bool draw_nonhints;

	bool show_2d;
	PackedColour color_2d;
	int width_2d;
	float fp_color_2d[4];

	std::vector<CBspPortal> portal;
	std::vector<const CBspPortal*> portal_sort;
	bool hint_flags;
//	CBspNode *node;
};

class CubicClipVolume
{
public:
	Vector3 cam, min, max;
};

class CPortalsDrawSolid : public OpenGLRenderable
{
public:
	mutable CubicClipVolume clip;
	void render( RenderStateFlags state ) const;
};

class CPortalsDrawSolidOutline : public OpenGLRenderable
{
public:
	mutable CubicClipVolume clip;
	void render( RenderStateFlags state ) const;
};

class CPortalsDrawWireframe : public OpenGLRenderable
{
public:
	void render( RenderStateFlags state ) const;
};

class CPortalsRender : public Renderable
{
public:
	CPortalsDrawSolid m_drawSolid;
	CPortalsDrawSolidOutline m_drawSolidOutline;
	CPortalsDrawWireframe m_drawWireframe;

	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const;
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const;
};

extern CPortals portals;
extern CPortalsRender render;

void Portals_constructShaders();
void Portals_destroyShaders();

void Portals_shadersChanged();

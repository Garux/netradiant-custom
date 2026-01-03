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

#pragma once

#include "render.h"
#include "renderable.h"

const Colour4b g_colour_sphere( 0, 0, 0, 255 );
const Colour4b g_colour_screen( 0, 255, 255, 255 );
const Colour4b g_colour_selected( 255, 255, 0, 255 );

inline const Colour4b& colourSelected( const Colour4b& colour, bool selected ){
	return ( selected ) ? g_colour_selected : colour;
}


struct RenderablePoint : public OpenGLRenderable
{
	PointVertex m_point;
	RenderablePoint():
		m_point( vertex3f_identity ) {
	}
	void render( RenderStateFlags state ) const override {
		gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_point.colour );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_point.vertex );
		gl().glDrawArrays( GL_POINTS, 0, 1 );
	}
	void setColour( const Colour4b & colour ) {
		m_point.colour = colour;
	}
};

struct RenderableLine : public OpenGLRenderable
{
	PointVertex m_line[2];

	RenderableLine() {
	}
	void render( RenderStateFlags state ) const override {
		gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_line[0].colour );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_line[0].vertex );
		gl().glDrawArrays( GL_LINES, 0, 2 );
	}
	void setColour( const Colour4b& colour ) {
		m_line[0].colour = colour;
		m_line[1].colour = colour;
	}
};

struct RenderableQuad : public OpenGLRenderable
{
	PointVertex m_quad[4];
	void render( RenderStateFlags state ) const override {
		gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_quad[0].colour );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_quad[0].vertex );
		gl().glDrawArrays( GL_LINE_LOOP, 0, 4 );
	}
	void setColour( const Colour4b& colour ){
		m_quad[0].colour = colour;
		m_quad[1].colour = colour;
		m_quad[2].colour = colour;
		m_quad[3].colour = colour;
	}
};

template<GLenum MODE>
struct RenderableCircle___ : public OpenGLRenderable
{
	Array<PointVertex> m_vertices;

	RenderableCircle___( std::size_t size ) : m_vertices( size ){
	}
	void render( RenderStateFlags state ) const override {
		gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_vertices.data()->colour );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_vertices.data()->vertex );
		gl().glDrawArrays( MODE, 0, GLsizei( m_vertices.size() ) );
	}
	void setColour( const Colour4b& colour ){
		for ( auto& v : m_vertices )
			v.colour = colour;
	}
};
using RenderableCircle = RenderableCircle___<GL_LINE_LOOP>;
using RenderableSemiCircle = RenderableCircle___<GL_LINE_STRIP>;


struct FlatShadedVertex
{
	Vertex3f vertex;
	Colour4b colour;
	Normal3f normal;
};

struct RenderableArrowHead : public OpenGLRenderable
{
	Array<FlatShadedVertex> m_vertices;

	RenderableArrowHead( std::size_t size )
		: m_vertices( size ) {
	}
	void render( RenderStateFlags state ) const override {
		gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( FlatShadedVertex ), &m_vertices.data()->colour );
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( FlatShadedVertex ), &m_vertices.data()->vertex );
		gl().glNormalPointer( GL_FLOAT, sizeof( FlatShadedVertex ), &m_vertices.data()->normal );
		gl().glDrawArrays( GL_TRIANGLES, 0, GLsizei( m_vertices.size() ) );
	}
	void setColour( const Colour4b& colour ) {
		for ( auto& v : m_vertices )
		{
			v.colour = colour;
		}
	}
};

const float arrowhead_length = 16;
const float arrowhead_radius = 4;

inline void draw_arrowline( const float length, PointVertex* line, const std::size_t axis ){
	( *line++ ).vertex = vertex3f_identity;
	( *line ).vertex = vertex3f_identity;
	vertex3f_to_array( ( *line ).vertex )[axis] = length - arrowhead_length;
}

template<typename VertexRemap, typename NormalRemap>
void draw_arrowhead( const std::size_t segments, const float length, FlatShadedVertex* vertices, VertexRemap, NormalRemap ){
	std::size_t head_tris = ( segments << 3 );
	const double head_segment = c_2pi / head_tris;
	for ( std::size_t i = 0; i < head_tris; ++i )
	{
		{
			FlatShadedVertex& point = vertices[i * 6 + 0];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * cos( i * head_segment );
			VertexRemap::z( point.vertex ) = arrowhead_radius * sin( i * head_segment );
			NormalRemap::x( point.normal ) = arrowhead_radius / arrowhead_length;
			NormalRemap::y( point.normal ) = cos( i * head_segment );
			NormalRemap::z( point.normal ) = sin( i * head_segment );
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 1];
			VertexRemap::x( point.vertex ) = length;
			VertexRemap::y( point.vertex ) = 0;
			VertexRemap::z( point.vertex ) = 0;
			NormalRemap::x( point.normal ) = arrowhead_radius / arrowhead_length;
			NormalRemap::y( point.normal ) = cos( ( i + 0.5 ) * head_segment );
			NormalRemap::z( point.normal ) = sin( ( i + 0.5 ) * head_segment );
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 2];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * cos( ( i + 1 ) * head_segment );
			VertexRemap::z( point.vertex ) = arrowhead_radius * sin( ( i + 1 ) * head_segment );
			NormalRemap::x( point.normal ) = arrowhead_radius / arrowhead_length;
			NormalRemap::y( point.normal ) = cos( ( i + 1 ) * head_segment );
			NormalRemap::z( point.normal ) = sin( ( i + 1 ) * head_segment );
		}

		{
			FlatShadedVertex& point = vertices[i * 6 + 3];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = 0;
			VertexRemap::z( point.vertex ) = 0;
			NormalRemap::x( point.normal ) = -1;
			NormalRemap::y( point.normal ) = 0;
			NormalRemap::z( point.normal ) = 0;
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 4];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * cos( i * head_segment );
			VertexRemap::z( point.vertex ) = arrowhead_radius * sin( i * head_segment );
			NormalRemap::x( point.normal ) = -1;
			NormalRemap::y( point.normal ) = 0;
			NormalRemap::z( point.normal ) = 0;
		}
		{
			FlatShadedVertex& point = vertices[i * 6 + 5];
			VertexRemap::x( point.vertex ) = length - arrowhead_length;
			VertexRemap::y( point.vertex ) = arrowhead_radius * cos( ( i + 1 ) * head_segment );
			VertexRemap::z( point.vertex ) = arrowhead_radius * sin( ( i + 1 ) * head_segment );
			NormalRemap::x( point.normal ) = -1;
			NormalRemap::y( point.normal ) = 0;
			NormalRemap::z( point.normal ) = 0;
		}
	}
}

template<typename Triple>
class TripleRemapXYZ
{
public:
	static float& x( Triple& triple ){
		return triple.x();
	}
	static float& y( Triple& triple ){
		return triple.y();
	}
	static float& z( Triple& triple ){
		return triple.z();
	}
};

template<typename Triple>
class TripleRemapYZX
{
public:
	static float& x( Triple& triple ){
		return triple.y();
	}
	static float& y( Triple& triple ){
		return triple.z();
	}
	static float& z( Triple& triple ){
		return triple.x();
	}
};

template<typename Triple>
class TripleRemapZXY
{
public:
	static float& x( Triple& triple ){
		return triple.z();
	}
	static float& y( Triple& triple ){
		return triple.x();
	}
	static float& z( Triple& triple ){
		return triple.y();
	}
};

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
#include "math/matrix.h"

#if defined( _DEBUG ) && !defined( _DEBUG_QUICKER )
#define DEBUG_SELECTION
#endif

#if defined( DEBUG_SELECTION )

class RenderableClippedPrimitive : public OpenGLRenderable
{
	struct primitive_t
	{
		PointVertex m_points[9];
		std::size_t m_count;
	};
	Matrix4 m_inverse;
	std::vector<primitive_t> m_primitives;
public:
	Matrix4 m_world;

	void render( RenderStateFlags state ) const override {
		for ( std::size_t i = 0; i < m_primitives.size(); ++i )
		{
			gl().glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( PointVertex ), &m_primitives[i].m_points[0].colour );
			gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_primitives[i].m_points[0].vertex );
			switch ( m_primitives[i].m_count )
			{
			case 1: break;
			case 2: gl().glDrawArrays( GL_LINES, 0, GLsizei( m_primitives[i].m_count ) ); break;
			default: gl().glDrawArrays( GL_POLYGON, 0, GLsizei( m_primitives[i].m_count ) ); break;
			}
		}
	}

	void construct( const Matrix4& world2device ){
		m_inverse = matrix4_full_inverse( world2device );
		m_world = g_matrix4_identity;
	}

	void insert( const Vector4 clipped[9], std::size_t count ){
		add_one();

		m_primitives.back().m_count = count;
		for ( std::size_t i = 0; i < count; ++i )
		{
			Vector3 world_point( vector4_projected( matrix4_transformed_vector4( m_inverse, clipped[i] ) ) );
			m_primitives.back().m_points[i].vertex = vertex3f_for_vector3( world_point );
		}
	}

	void destroy(){
		m_primitives.clear();
	}
private:
	void add_one(){
		m_primitives.push_back( primitive_t() );

		const Colour4b colour_clipped( 255, 127, 0, 255 );

		for ( std::size_t i = 0; i < 9; ++i )
			m_primitives.back().m_points[i].colour = colour_clipped;
	}
};

inline Shader* g_state_clipped;
inline RenderableClippedPrimitive g_render_clipped;

#endif

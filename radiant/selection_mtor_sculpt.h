/*
   This file is part of NetRadiant.

   NetRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   NetRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with NetRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "selection_.h"

class SculptManipulator : public Manipulator
{
public:
	virtual ~SculptManipulator() = default;
	inline static Shader* m_state_line;
	inline static Shader* m_state_point;
};

SculptManipulator* New_SculptManipulator();

void Sculpt_modeChanged( bool isSculpt );

void Sculpt_setRadius( float radius );
bool Sculpt_wheelEvent( const class QWheelEvent& event );

inline float g_sculpt_radius = 128;

/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

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

struct surfaceExtra_t
{
	const mapDrawSurface_t        *mds = nullptr;
	const shaderInfo_t            *si = nullptr;
	int parentSurfaceNum = -1;
	int entityNum = 0;
	int castShadows = WORLDSPAWN_CAST_SHADOWS;
	int recvShadows = WORLDSPAWN_RECV_SHADOWS;
	int sampleSize = 0;
	Vector3 ambientColor{ 0, 0, 0 };
	float longestCurve = 0;
	Vector3 lightmapAxis{ 0, 0, 0 };
};

const surfaceExtra_t& GetSurfaceExtra( int num );
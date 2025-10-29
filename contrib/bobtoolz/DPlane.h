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

// DPlane.h: interface for the DPlane class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <list>
#include "ibrush.h"
#include "string/string.h"
#include "mathlib.h"
#include "DWinding.h"

#define FACE_DETAIL 0x8000000

class DPlane
{
public:
	DPlane( const vec3_t va, const vec3_t vb, const vec3_t vc, const char* textureName, bool bDetail );
	void ScaleTexture();
	DWinding BaseWindingForPlane() const ;

	void Rebuild();

	bool AddToBrush( scene::Node& brush );
	bool operator !=( DPlane& other );
	bool operator ==( const DPlane& other ) const;

	bool IsRedundant( std::list<class DPoint*>& pointList );
	bool PlaneIntersection( DPlane* pl1, DPlane* pl2, vec3_t out );;

	vec_t DistanceToPoint( const vec3_t pnt ) const;

	DPlane( const vec3_t va, const vec3_t vb, const vec3_t vc, const _QERFaceData* texData );
	DPlane(){
	}

	bool m_bChkOk;
	_QERFaceData texInfo;
	CopiedString m_shader;
	vec3_t points[3];           // djbob:do we really need these any more?
	vec3_t normal;
	float _d;
};

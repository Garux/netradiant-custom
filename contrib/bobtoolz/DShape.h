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

// DShape.h: interface for the DShape class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdlib>
#include "mathlib.h"
#include "DMap.h"
class DBrush;
class DEntity;

// defines for polygon stuff
#define MAX_POLYGON_FACES   128

extern bool bFacesAll[];

class DShape
{
public:
	bool BuildPit( vec3_t min, vec3_t max );
	void BuildBorderedPrism( vec3_t min, vec3_t max, int nSides, int nBorder, bool bAlignTop );
	void BuildInversePrism( vec3_t min, vec3_t max, int nSides, bool bAlignTop );
	void BuildRegularPrism( vec3_t min, vec3_t max, int nSides, bool bAlignTop );

	int m_nNextBrush;
	static DBrush* GetBoundingCube_Ext( vec3_t min, vec3_t max, const char* textureName, bool* bUseFaces = bFacesAll, bool detail = false );

	void Commit();
private:
	DBrush* GetBoundingCube( vec3_t min, vec3_t max, const char* textureName, DEntity* ent = nullptr, bool* bUseFaces = bFacesAll );

	DMap m_Container;
};

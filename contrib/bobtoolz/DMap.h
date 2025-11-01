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

// DMap.h: interface for the DMap class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <list>

class DEntity;

struct LoadOptions
{
	bool loadPatches = false;
	bool loadSelectedOnly = false;
	bool loadVisibleOnly = false;
	bool loadDetail = true;
};

class DMap
{
public:
	static void RebuildEntity( DEntity* ent );

	void ResetTextures( const char* textureName, const float fScale[2], const float fShift[2], int rotation, const char* newTextureName, bool bResetTextureName, const bool bResetScale[2], const bool bResetShift[2], bool bResetRotation );
	void LoadAll( const LoadOptions options = {} );
	void BuildInRadiant( bool bAllowDestruction );
	int m_nNextEntity;
	DEntity* GetWorldSpawn();
	void ClearEntities();

	DEntity* GetEntityForID( int ID );
	DEntity* AddEntity( const char* classname = "worldspawn", int ID = -1 );

	std::list<DEntity*> entityList;

	DMap();
	~DMap();

	int FixBrushes();
};

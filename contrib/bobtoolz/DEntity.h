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

// DEntity.h: interface for the DEntity class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "string/string.h"
#include "mathlib.h"
#include "DMap.h"
#include "DEPair.h"

class DBrush;
class DPlane;
class DPatch;
class Entity;

namespace scene
{
class Node;
}
class _QERFaceData;

class DEntity
{
public:
	void RemoveFromRadiant();
	scene::Node* QER_Entity;
	int m_nID;

//	Constrcution/Destruction
	DEntity( const char* classname = "worldspawn", int ID = -1 );   // sets classname
	virtual ~DEntity();
//	---------------------------------------------

//	epair functions........
	void LoadEPairList( Entity* epl );
	void AddEPair( const char* key, const char* value );
	void ClearEPairs();
	DEPair* FindEPairByKey( const char* keyname );
//	---------------------------------------------

//	random functions........
	bool ResetTextures( const char* textureName, float fScale[2], float fShift[2], int rotation, const char* newTextureName, bool bResetTextureName, bool bResetScale[2], bool bResetShift[2], bool bResetRotation, bool rebuild );
	void SaveToFile( FILE* pFile );
	void SetClassname( const char* classname );

	void BuildInRadiant( bool allowDestruction );
	void ResetChecks( const std::vector<CopiedString>& exclusionList );
	void RemoveNonCheckBrushes( const std::vector<CopiedString>& exclusionList );

	int GetBrushCount( void );
	DBrush* FindBrushByPointer( scene::Node& brush );
//	---------------------------------------------


//	bool list functions
	void SelectBrushes( bool* selectList );
	bool* BuildDuplicateList();
	bool* BuildIntersectList();
//	---------------------------------------------


//	brush operations
	void ClearBrushes();        // clears brush list and frees memory for brushes

	DBrush* NewBrush();
//	---------------------------------------------

//	patch operations
	void ClearPatches();

	DPatch* NewPatch();
//	---------------------------------------------

//	vars
	std::vector<DEPair> epairList;
	std::vector<DBrush*> brushList;
// new patches, wahey!!!
	std::vector<DPatch*> patchList;
	CopiedString m_Classname;
//	---------------------------------------------


	int FixBrushes();

	bool LoadFromEntity( scene::Node& ent, const LoadOptions options = {} );
	/* these two load any selected primitives to single entity, hence e.g. QER_Entity wont be correct
	   basically use with high care */
	void LoadSelectedBrushes();
	void LoadSelectedPatches();

	bool LoadFromPrt( char* filename );
//	---------------------------------------------
	void SpawnString( const char* key, const char* defaultstring, const char** out );
	void SpawnInt( const char* key, const char* defaultstring, int* out );
	void SpawnFloat( const char* key, const char* defaultstring, float* out );
	void SpawnVector( const char* key, const char* defaultstring, vec_t* out );
};

void select_primitive( scene::Node *primitive, scene::Node *entity );

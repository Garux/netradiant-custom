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

// DMap.cpp: implementation of the DMap class.
//
//////////////////////////////////////////////////////////////////////

#include "DMap.h"

#include "DPoint.h"
#include "DPlane.h"
#include "DBrush.h"
#include "DEPair.h"
#include "DPatch.h"
#include "DEntity.h"

#include "iundo.h"

#include "generic/referencecounted.h"

#include <algorithm>

#include "scenelib.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DMap::DMap(){
	m_nNextEntity = 1;
	AddEntity( "worldspawn", 0 );
}

DMap::~DMap(){
	ClearEntities();
}

DEntity* DMap::AddEntity( const char *classname, int ID ){
	DEntity* newEntity;
	if ( ID == -1 ) {
		newEntity = new DEntity( classname, m_nNextEntity++ );
	}
	else{
		newEntity = new DEntity( classname, ID );
	}

	entityList.push_back( newEntity );

	return newEntity;
}

void DMap::ClearEntities(){
	m_nNextEntity = 1;

	for ( DEntity *entity : entityList )
		delete entity;

	entityList.clear();
}

DEntity* DMap::GetEntityForID( int ID ){
	DEntity* findEntity = NULL;

	for ( DEntity *entity : entityList )
	{
		if ( entity->m_nID == ID ) {
			findEntity = entity;
			break;
		}
	}

	if ( !findEntity ) {
		findEntity = AddEntity( "worldspawn", ID );
	}

	return findEntity;
}


DEntity* DMap::GetWorldSpawn(){
	return GetEntityForID( 0 );
}

void DMap::BuildInRadiant( bool bAllowDestruction ){
	for ( DEntity *entity : entityList )
		entity->BuildInRadiant( bAllowDestruction );
}

void DMap::LoadAll( const LoadOptions options ){
	ClearEntities();

	GlobalSelectionSystem().setSelectedAll( false );

	class load_entities_t : public scene::Traversable::Walker
	{
		DMap* m_map;
		const LoadOptions m_options;
	public:
		load_entities_t( DMap* map, const LoadOptions options )
			: m_map( map ), m_options( options ){
		}
		bool pre( scene::Node& node ) const {
			if ( Node_isEntity( node ) && !( m_options.loadVisibleOnly && !node.visible() ) ) {
				m_map->AddEntity( "", 0 )->LoadFromEntity( node, m_options );
			}
			return false;
		}
	} load_entities( this, options );

	Node_getTraversable( GlobalSceneGraph().root() )->traverse( load_entities );
}

int DMap::FixBrushes(){
	int count = 0;
	for ( DEntity *entity : entityList )
	{
		count += entity->FixBrushes();
	}

	return count;
}

void DMap::ResetTextures( const char* textureName, float fScale[2],      float fShift[2],      int rotation, const char* newTextureName,
                          bool bResetTextureName,  bool bResetScale[2],  bool bResetShift[2],  bool bResetRotation ){
	for ( DEntity *entity : entityList )
	{
		if ( string_equal_nocase( "worldspawn", entity->m_Classname.c_str() ) ) {
			entity->ResetTextures( textureName,        fScale,       fShift,       rotation, newTextureName,
			                       bResetTextureName,  bResetScale,  bResetShift,  bResetRotation, true );
		}
		else
		{
			if ( entity->ResetTextures( textureName,        fScale,       fShift,       rotation, newTextureName,
			                            bResetTextureName,  bResetScale,  bResetShift,  bResetRotation, false ) ) {
				RebuildEntity( entity );
			}
		}
	}
}

void DMap::RebuildEntity( DEntity *ent ){
	ent->RemoveFromRadiant();
	ent->BuildInRadiant( false );
}

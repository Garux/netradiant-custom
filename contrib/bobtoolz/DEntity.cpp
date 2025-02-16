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

// DEntity.cpp: implementation of the DEntity class.
//
//////////////////////////////////////////////////////////////////////

#include "DEntity.h"

#include <utility>

#include "DPoint.h"
#include "DPlane.h"
#include "DBrush.h"
#include "DEPair.h"
#include "DPatch.h"

#include "dialogs/dialogs-gtk.h"
#include "misc.h"
#include "CPortals.h"

#include "iundo.h"
#include "ientity.h"
#include "ieclass.h"

#include "generic/referencecounted.h"

#include <algorithm>

#include "scenelib.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DEntity::DEntity( const char *classname, int ID ){
	SetClassname( classname );
	m_nID = ID;
	QER_Entity = NULL;
}

DEntity::~DEntity(){
	ClearPatches();
	ClearBrushes();
	ClearEPairs();
}

//////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////

void DEntity::ClearBrushes(){
	for ( DBrush *brush : brushList )
	{
		delete brush;
	}
	brushList.clear();
}

void DEntity::ClearPatches(){
	for ( DPatch *patch : patchList )
	{
		delete patch;
	}
	patchList.clear();
}

DPatch* DEntity::NewPatch(){
	return patchList.emplace_back( new DPatch );
}

DBrush* DEntity::NewBrush(){
	return brushList.emplace_back( new DBrush );
}

char* getNextBracket( char* s ){
	char* p = s;
	while ( *p )
	{
		p++;
		if ( *p == '(' ) {
			break;
		}
	}

	return p;
}

bool DEntity::LoadFromPrt( char *filename ){
	CPortals portals;
	strcpy( portals.fn, filename );
	portals.Load();

	if ( portals.node_count == 0 ) {
		return false;
	}

	ClearBrushes();
	ClearEPairs();

	bool build = false;
	for ( unsigned int i = 0; i < portals.node_count; i++ )
	{
		build = false;
		DBrush* brush = NewBrush();

		for ( unsigned int j = 0; j < portals.node[i].portal_count; j++ )
		{
			for ( unsigned int k = 0; k < portals.node[i].portal[j].point_count - 2; k++ )
			{
				vec3_t v1, v2, normal, n;
				VectorSubtract( portals.node[i].portal[j].point[k + 2].p, portals.node[i].portal[j].point[k + 1].p, v1 );
				VectorSubtract( portals.node[i].portal[j].point[k].p, portals.node[i].portal[j].point[k + 1].p, v2 );
				CrossProduct( v1, v2, n );
				VectorNormalize( n, v2 );

				if ( k == 0 ) {
					VectorCopy( v2, normal );
				}
				else
				{
					VectorSubtract( v2, normal, v1 );
					if ( VectorLength( v1 ) > 0.01 ) {
						build = true;
						break;
					}
				}
			}

			if ( !build ) {
				brush->AddFace( portals.node[i].portal[j].point[2].p, portals.node[i].portal[j].point[1].p, portals.node[i].portal[j].point[0].p, "textures/common/caulk", false );
			}
			else{
				brush->AddFace( portals.node[i].portal[j].point[0].p, portals.node[i].portal[j].point[1].p, portals.node[i].portal[j].point[2].p, "textures/common/caulk", false );
			}
		}
		if ( build ) {
			brush->BuildInRadiant( false, NULL );
		}
	}

	return true;
}

template<typename Functor>
class BrushSelectedVisitor : public SelectionSystem::Visitor
{
	const Functor& m_functor;
public:
	BrushSelectedVisitor( const Functor& functor ) : m_functor( functor ){
	}
	void visit( scene::Instance& instance ) const {
		if ( Node_isBrush( instance.path().top() ) ) {
			m_functor( instance );
		}
	}
};

template<typename Functor>
inline const Functor& Scene_forEachSelectedBrush( const Functor& functor ){
	GlobalSelectionSystem().foreachSelected( BrushSelectedVisitor<Functor>( functor ) );
	return functor;
}

void DEntity_loadBrush( DEntity& entity, scene::Instance& brush ){
	entity.NewBrush()->LoadFromBrush( brush, true );
}
typedef ReferenceCaller<DEntity, void(scene::Instance&), DEntity_loadBrush> DEntityLoadBrushCaller;

void DEntity::LoadSelectedBrushes(){
	ClearBrushes();
	ClearEPairs();

	Scene_forEachSelectedBrush( DEntityLoadBrushCaller( *this ) );
}

template<typename Functor>
class PatchSelectedVisitor : public SelectionSystem::Visitor
{
	const Functor& m_functor;
public:
	PatchSelectedVisitor( const Functor& functor ) : m_functor( functor ){
	}
	void visit( scene::Instance& instance ) const {
		if ( Node_isPatch( instance.path().top() ) ) {
			m_functor( instance );
		}
	}
};

template<typename Functor>
inline const Functor& Scene_forEachSelectedPatch( const Functor& functor ){
	GlobalSelectionSystem().foreachSelected( PatchSelectedVisitor<Functor>( functor ) );
	return functor;
}

void DEntity_loadPatch( DEntity& entity, scene::Instance& patch ){
	DPatch* loadPatch = entity.NewPatch();
	loadPatch->LoadFromPatch( patch );
}
typedef ReferenceCaller<DEntity, void(scene::Instance&), DEntity_loadPatch> DEntityLoadPatchCaller;

void DEntity::LoadSelectedPatches(){
	ClearPatches();
	ClearEPairs();

	Scene_forEachSelectedPatch( DEntityLoadPatchCaller( *this ) );
}

bool* DEntity::BuildIntersectList(){
	if ( brushList.empty() ) {
		return nullptr;
	}

	bool* pbIntList = new bool[brushList.size()] (); // () zero initialize

	for ( size_t i = 0; i < brushList.size(); ++i )
	{
		for ( size_t j = i + 1; j < brushList.size(); ++j )
		{
			if ( brushList[i]->IntersectsWith( brushList[j] ) ) {
				pbIntList[i] = true;
				pbIntList[j] = true;
			}
		}
	}

	return pbIntList;
}

bool* DEntity::BuildDuplicateList(){
	if ( brushList.empty() ) {
		return nullptr;
	}

	bool* pbDupList = new bool[brushList.size()] (); // () zero initialize

	for ( size_t i = 0; i < brushList.size(); ++i )
	{
		for ( size_t j = i + 1; j < brushList.size(); ++j )
		{
			if ( brushList[i]->operator==( brushList[j] ) ) {
				pbDupList[i] = true;
				pbDupList[j] = true;
			}
		}
	}

	return pbDupList;
}

void DEntity::SelectBrushes( bool *selectList ){
	if ( selectList == NULL ) {
		return;
	}

	GlobalSelectionSystem().setSelectedAll( false );

	for ( size_t i = 0; i < brushList.size(); ++i )
	{
		if ( selectList[i] ) {
			brushList[i]->selectInRadiant();
		}
	}
}

void select_primitive( scene::Node *primitive, scene::Node *entity ){
	scene::Path path( NodeReference( GlobalSceneGraph().root() ) );
	path.push( NodeReference( *entity ) );
	path.push( NodeReference( *primitive ) );
	Instance_getSelectable( *GlobalSceneGraph().find( path ) )->setSelected( true );
}

bool DEntity::LoadFromEntity( scene::Node& ent, const LoadOptions options ) {
	ClearPatches();
	ClearBrushes();
	ClearEPairs();

	QER_Entity = &ent;

	LoadEPairList( Node_getEntity( ent ) );

	if ( !node_is_group( ent ) ) {
		return false;
	}

	if ( Node_getTraversable( ent ) ) {
		class load_brushes_t : public scene::Traversable::Walker
		{
			DEntity* m_entity;
			const LoadOptions m_options;
		public:
			load_brushes_t( DEntity* entity, const LoadOptions options )
				: m_entity( entity ), m_options( options ){
			}
			bool pre( scene::Node& node ) const {
				if( !( m_options.loadVisibleOnly && !node.visible() ) ){
					scene::Path path( NodeReference( GlobalSceneGraph().root() ) );
					path.push( NodeReference( *m_entity->QER_Entity ) );
					path.push( NodeReference( node ) );
					scene::Instance* instance = GlobalSceneGraph().find( path );
					ASSERT_MESSAGE( instance != 0, "" );

					if( !( m_options.loadSelectedOnly && Instance_isSelected( *instance ) ) ){
						if ( Node_isPatch( node ) ) {
							if( m_options.loadPatches )
								m_entity->NewPatch()->LoadFromPatch( *instance );
						}
						else if ( Node_isBrush( node ) ) {
							m_entity->NewBrush()->LoadFromBrush( *instance, true );
							if( !m_options.loadDetail && m_entity->brushList.back()->IsDetail() ){
								delete m_entity->brushList.back();
								m_entity->brushList.pop_back();
							}
						}
					}
				}

				return false;
			}
		} load_brushes( this, options );

		Node_getTraversable( ent )->traverse( load_brushes );
	}

	return true;
}

void DEntity::RemoveNonCheckBrushes( const std::vector<CopiedString>& exclusionList ){
	std::erase_if( brushList, [&]( DBrush *brush ){
		if ( std::any_of( exclusionList.cbegin(), exclusionList.cend(), [brush]( const CopiedString& tex ){ return brush->HasTexture( tex.c_str() ); } ) ) {
			delete brush;
			return true;
		}
		return false;
	} );
}

void DEntity::ResetChecks( const std::vector<CopiedString>& exclusionList ){
	for ( DBrush *brush : brushList )
	{
		brush->ResetChecks( exclusionList );
	}
}

int DEntity::FixBrushes(){
	int count = 0;

	for ( DBrush *brush : brushList )
	{
		count += brush->RemoveRedundantPlanes();
	}

	return count;
}

void DEntity::BuildInRadiant( bool allowDestruction ){
	const bool makeEntity = m_Classname != "worldspawn";

	if ( makeEntity ) {
		NodeSmartReference node( GlobalEntityCreator().createEntity( GlobalEntityClassManager().findOrInsert( m_Classname.c_str(), !brushList.empty() || !patchList.empty() ) ) );

		for ( const DEPair& epair : epairList )
		{
			Node_getEntity( node )->setKeyValue( epair.key.c_str(), epair.value.c_str() );
		}

		Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

		for ( DBrush *brush : brushList )
			brush->BuildInRadiant( allowDestruction, NULL, node.get_pointer() );

		for ( DPatch *patch : patchList )
			patch->BuildInRadiant( node.get_pointer() );

		QER_Entity = node.get_pointer();
	}
	else
	{
		for ( DBrush *brush : brushList )
			brush->BuildInRadiant( allowDestruction, NULL );

		for ( DPatch *patch : patchList )
			patch->BuildInRadiant();
	}
}



void DEntity::SetClassname( const char *classname ) {
	m_Classname = classname;
}

void DEntity::SaveToFile( FILE *pFile ){
	fprintf( pFile, "{\n" );

	fprintf( pFile, "\"classname\" \"%s\"\n", m_Classname.c_str() );

	for ( const DEPair& ep : epairList )
	{
		fprintf( pFile, "\"%s\" \"%s\"\n", ep.key.c_str(), ep.value.c_str() );
	}

	for ( DBrush *brush : brushList )
	{
		brush->SaveToFile( pFile );
	}

	fprintf( pFile, "}\n" );
}

void DEntity::ClearEPairs(){
	epairList.clear();
}

void DEntity::AddEPair( const char *key, const char *value ) {
	if ( DEPair* pair = FindEPairByKey( key ) ) {
		*pair = DEPair( key, value );
	}
	else {
		epairList.push_back( DEPair( key, value ) );
	}
}

void DEntity::LoadEPairList( Entity *epl ){
	class load_epairs_t : public Entity::Visitor
	{
		DEntity* m_entity;
	public:
		load_epairs_t( DEntity* entity )
			: m_entity( entity ){
		}
		void visit( const char* key, const char* value ){
			if ( strcmp( key, "classname" ) == 0 ) {
				m_entity->SetClassname( value );
			}
			else{
				m_entity->AddEPair( key, value );
			}
		}

	} load_epairs( this );

	epl->forEachKeyValue( load_epairs );
}

bool DEntity::ResetTextures( const char* textureName, float fScale[2],     float fShift[2],     int rotation, const char* newTextureName,
                             bool bResetTextureName,  bool bResetScale[2], bool bResetShift[2], bool bResetRotation, bool rebuild ){
	bool reset = false;

	for ( DBrush *brush : brushList )
	{
		const bool tmp = brush->ResetTextures( textureName,        fScale,       fShift,       rotation, newTextureName,
		                                       bResetTextureName,  bResetScale,  bResetShift,  bResetRotation );
		if ( tmp ) {
			reset = true;
			if ( rebuild ) {
				Node_getTraversable( *brush->QER_entity )->erase( *brush->QER_brush );
				brush->BuildInRadiant( false, NULL, brush->QER_entity );
			}
		}
	}

	if ( bResetTextureName ) {
		for ( DPatch *patch : patchList )
		{
			if ( patch->ResetTextures( textureName, newTextureName ) ) {
				reset = true;
				if ( rebuild ) {
					Node_getTraversable( *patch->QER_entity )->erase( *patch->QER_brush );
					patch->BuildInRadiant( patch->QER_entity );
				}
			}
		}
	}

	return reset;
}

DEPair* DEntity::FindEPairByKey( const char* keyname ){
	for ( DEPair& ep : epairList )
	{
		if ( ep.key == keyname ) {
			return &ep;
		}
	}
	return nullptr;
}

void DEntity::RemoveFromRadiant(){
	Node_getTraversable( GlobalSceneGraph().root() )->erase( *QER_Entity );

	QER_Entity = NULL;
}

void DEntity::SpawnString( const char* key, const char* defaultstring, const char** out ){
	DEPair* pEP = FindEPairByKey( key );
	if ( pEP ) {
		*out = pEP->value.c_str();
	}
	else {
		*out = defaultstring;
	}
}

void DEntity::SpawnInt( const char* key, const char* defaultstring, int* out ){
	DEPair* pEP = FindEPairByKey( key );
	if ( pEP ) {
		*out = atoi( pEP->value.c_str() );
	}
	else {
		*out = atoi( defaultstring );
	}
}

void DEntity::SpawnFloat( const char* key, const char* defaultstring, float* out ){
	DEPair* pEP = FindEPairByKey( key );
	if ( pEP ) {
		*out = static_cast<float>( atof( pEP->value.c_str() ) );
	}
	else {
		*out = static_cast<float>( atof( defaultstring ) );
	}
}

void DEntity::SpawnVector( const char* key, const char* defaultstring, vec_t* out ){
	DEPair* pEP = FindEPairByKey( key );
	if ( pEP ) {
		sscanf( pEP->value.c_str(), "%f %f %f", &out[0], &out[1], &out[2] );
	}
	else {
		sscanf( defaultstring, "%f %f %f", &out[0], &out[1], &out[2] );
	}
}

int DEntity::GetBrushCount( void ) {
	return static_cast<int>( brushList.size() );
}

DBrush* DEntity::FindBrushByPointer( scene::Node& brush ) {
	for ( DBrush* pBrush : brushList ) {
		if ( pBrush->QER_brush == &brush ) {
			return pBrush;
		}
	}
	return NULL;
}

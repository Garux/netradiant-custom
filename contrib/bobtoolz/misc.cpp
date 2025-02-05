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

#include "misc.h"

#include "DPoint.h"
#include "DPlane.h"
#include "DBrush.h"
#include "DEPair.h"
#include "DPatch.h"
#include "DEntity.h"

#include "funchandlers.h"

#if defined ( POSIX )
#include <sys/types.h>
#include <unistd.h>
#endif

#include "iundo.h"
#include "ientity.h"
#include "iscenegraph.h"
#include "qerplugin.h"

#include <vector>
#include <algorithm>

#include "scenelib.h"

/*==========================
        Global Vars
   ==========================*/

//HANDLE bsp_process;
char g_CurrentTexture[256] = "";

//=============================================================
//=============================================================

void ReadCurrentTexture(){
	const char* textureName = GlobalRadiant().TextureBrowser_getSelectedShader();
	strcpy( g_CurrentTexture, textureName );
}

const char*  GetCurrentTexture(){
	ReadCurrentTexture();
	return g_CurrentTexture;
}

void MoveBlock( int dir, vec3_t min, vec3_t max, float dist ){
	switch ( dir )
	{
	case MOVE_EAST:
	{
		min[0] += dist;
		max[0] += dist;
		break;
	}
	case MOVE_WEST:
	{
		min[0] -= dist;
		max[0] -= dist;
		break;
	}
	case MOVE_NORTH:
	{
		min[1] += dist;
		max[1] += dist;
		break;
	}
	case MOVE_SOUTH:
	{
		min[1] -= dist;
		max[1] -= dist;
		break;
	}
	}
}

void SetInitialStairPos( int dir, vec3_t min, vec3_t max, float width ){
	switch ( dir )
	{
	case MOVE_EAST:
	{
		max[0] = min[0] + width;
		break;
	}
	case MOVE_WEST:
	{
		min[0] = max[0] - width;
		break;
	}
	case MOVE_NORTH:
	{
		max[1] = min[1] + width;
		break;
	}
	case MOVE_SOUTH:
	{
		min[1] = max[1] - width;
		break;
	}
	}
}

char* TranslateString( char *buf ){
	static char buf2[32768];

	std::size_t l = strlen( buf );
	char* out = buf2;
	for ( std::size_t i = 0; i < l; i++ )
	{
		if ( buf[i] == '\n' ) {
			*out++ = '\r';
			*out++ = '\n';
		}
		else{
			*out++ = buf[i];
		}
	}
	*out++ = 0;

	return buf2;
}


char* UnixToDosPath( char* path ){
#ifndef WIN32
	return path;
#else
	for ( char* p = path; *p; p++ )
	{
		if ( *p == '/' ) {
			*p = '\\';
		}
	}
	return path;
#endif
}

const char* ExtractFilename( const char* path ){
	const char* p = strrchr( path, '/' );
	if ( !p ) {
		p = strrchr( path, '\\' );

		if ( !p ) {
			return path;
		}
	}
	return ++p;
}

extern char* PLUGIN_NAME;
/*char* GetGameFilename( char* buffer, const char* filename )
   {
    strcpy( buffer, g_FuncTable.m_pfnGetGamePath() );
    char* p = strrchr( buffer, '/' );
   *++p = '\0';
    strcat( buffer, filename );
    buffer = UnixToDosPath( buffer );
    return buffer;
   }*/

#include "commandlib.h"

void StartBSP(){
	char exename[256];
	GetFilename( exename, "q3map" );
	UnixToDosPath( exename ); // do we want this done in linux version?

	char mapname[256];
	const char *pn = GlobalRadiant().getMapsPath();

	strcpy( mapname, pn );
	strcat( mapname, "/ac_prt.map" );
	UnixToDosPath( mapname );

	char command[1024];
	sprintf( command, "%s -nowater -fulldetail %s", exename, mapname );

	Q_Exec( NULL, command, NULL, false, true );
}

class EntityWriteMiniPrt
{
	mutable DEntity world;
	FILE* pFile;
	const std::vector<CopiedString>& exclusionList;
public:
	EntityWriteMiniPrt( FILE* pFile, const std::vector<CopiedString>& exclusionList )
		: pFile( pFile ), exclusionList( exclusionList ){
	}
	void operator()( scene::Instance& instance ) const {
		const char* classname = Node_getEntity( instance.path().top() )->getClassName();

		if ( string_equal( classname, "worldspawn" ) ) {
			world.LoadFromEntity( instance.path().top() );
			world.RemoveNonCheckBrushes( exclusionList );
			world.SaveToFile( pFile );
		}
		else if ( strstr( classname, "info_" ) ) {
			world.ClearBrushes();
			world.ClearEPairs();
			world.LoadEPairList( Node_getEntity( instance.path().top() ) );
			world.SaveToFile( pFile );
		}
	}
};

void BuildMiniPrt( const std::vector<CopiedString>& exclusionList ){
	// yes, we could just use -fulldetail option, but, as SPOG said
	// it'd be faster without all the hint, donotenter etc textures and
	// doors, etc



	char buffer[128];
	const char *pn = GlobalRadiant().getMapsPath();

	strcpy( buffer, pn );
	strcat( buffer, "/ac_prt.map" );
	FILE* pFile = fopen( buffer, "w" );

	// ahem, thx rr2
	if ( !pFile ) {
		return;
	}

	Scene_forEachEntity( EntityWriteMiniPrt( pFile, exclusionList ) );

	fclose( pFile );

	StartBSP();
}

class EntityFindByKeyValue
{
	const char* m_key;
	const char* m_value;
public:
	mutable const scene::Path* result;
	EntityFindByKeyValue( const char* key, const char* value )
		: m_key( key ), m_value( value ), result( 0 ){
	}
	void operator()( scene::Instance& instance ) const {
		if ( result == 0 ) {
			const char* value = Node_getEntity( instance.path().top() )->getKeyValue( m_key );

			if ( !strcmp( value, m_value ) ) {
				result = &instance.path();
			}
		}
	}
};

const scene::Path* FindEntityFromTarget( const char* target ){
	return Scene_forEachEntity( EntityFindByKeyValue( "target", target ) ).result;
}

const scene::Path* FindEntityFromTargetname( const char* targetname ){
	return Scene_forEachEntity( EntityFindByKeyValue( "targetname", targetname ) ).result;
}

void FillDefaultTexture( _QERFaceData* faceData, vec3_t va, vec3_t vb, vec3_t vc, const char* texture ){
	faceData->m_texdef.rotate = 0;
	faceData->m_texdef.scale[0] = 0.5;
	faceData->m_texdef.scale[1] = 0.5;
	faceData->m_texdef.shift[0] = 0;
	faceData->m_texdef.shift[1] = 0;
	faceData->contents = 0;
	faceData->flags = 0;
	faceData->value = 0;
	if ( *texture ) {
		faceData->m_shader = texture;
	}
	else{
		faceData->m_shader = "textures/common/caulk";
	}
	VectorCopy( va, faceData->m_p0 );
	VectorCopy( vb, faceData->m_p1 );
	VectorCopy( vc, faceData->m_p2 );
}

float Determinant3x3( float a1, float a2, float a3,
                      float b1, float b2, float b3,
                      float c1, float c2, float c3 ){
	return a1 * ( b2 * c3 - b3 * c2 ) - a2 * ( b1 * c3 - b3 * c1 ) + a3 * ( b1 * c2 - b2 * c1 );
}

bool GetEntityCentre( const char* entityKey, bool keyIsTarget, vec3_t centre ){
	const scene::Path* ent = keyIsTarget
	                       ? FindEntityFromTarget( entityKey )
	                       : FindEntityFromTargetname( entityKey );
	if ( !ent ) {
		return false;
	}

	scene::Instance& instance = *GlobalSceneGraph().find( *ent );
	VectorCopy( instance.worldAABB().origin, centre );

	return true;
}

vec_t Min( vec_t a, vec_t b ){
	if ( a < b ) {
		return a;
	}
	return b;
}

void MakeNormal( const vec_t* va, const vec_t* vb, const vec_t* vc, vec_t* out ) {
	vec3_t v1, v2;
	VectorSubtract( va, vb, v1 );
	VectorSubtract( vc, vb, v2 );
	CrossProduct( v1, v2, out );
}

char* GetFilename( char* buffer, const char* filename ) {
	strcpy( buffer, GlobalRadiant().getAppPath() );
	strcat( buffer, "plugins/" );
	strcat( buffer, filename );
	return buffer;
}

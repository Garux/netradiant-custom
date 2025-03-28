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

// DBrush.cpp: implementation of the DBrush class.
//
//////////////////////////////////////////////////////////////////////

#include "DBrush.h"

#include <list>

#include "DPoint.h"
#include "DPlane.h"
#include "DEPair.h"
#include "DPatch.h"
#include "DEntity.h"
#include "DWinding.h"

#include "dialogs/dialogs-gtk.h"

#include "misc.h"

#include "iundo.h"

#include "generic/referencecounted.h"

#include "scenelib.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DBrush::DBrush(){
	bBoundsBuilt = false;
	QER_entity = NULL;
	QER_brush = NULL;
}

DBrush::~DBrush(){
	ClearFaces();
	ClearPoints();
}

//////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////

DPlane* DBrush::AddFace( const vec3_t va, const vec3_t vb, const vec3_t vc, const _QERFaceData* texData ){
#ifdef _DEBUG
//	Sys_Printf( "(%f %f %f) (%f %f %f) (%f %f %f)\n", va[0], va[1], va[2], vb[0], vb[1], vb[2], vc[0], vc[1], vc[2] );
#endif
	bBoundsBuilt = false;
	DPlane* newFace = new DPlane( va, vb, vc, texData );
	faceList.push_back( newFace );

	return newFace;
}

int DBrush::BuildPoints(){
	ClearPoints();

	if ( faceList.size() <= 3 ) {  // if less than 3 faces, there can be no points
		return 0;                   // with only 3 faces u can't have a bounded soild

	}
	for ( std::list<DPlane *>::const_iterator p1 = faceList.begin(); p1 != faceList.end(); p1++ )
	{
		std::list<DPlane *>::const_iterator p2 = p1;
		for ( p2++; p2 != faceList.end(); p2++ )
		{
			std::list<DPlane *>::const_iterator p3 = p2;
			for ( p3++; p3 != faceList.end(); p3++ )
			{
				vec3_t pnt;
				if ( ( *p1 )->PlaneIntersection( *p2, *p3, pnt ) ) {
					int pos = PointPosition( pnt );

					if ( pos == POINT_IN_BRUSH ) { // ???? shouldn't happen here
						globalErrorStream() << "ERROR:: Build Brush Points: Point IN brush!!!\n";
					}
					else if ( pos == POINT_ON_BRUSH ) { // normal point
						if ( !HasPoint( pnt ) ) {
							AddPoint( pnt );
						}
/*						else
                            Sys_Printf( "Duplicate Point Found, pyramids ahoy!!!!!\n" );*/
						// point lies on more that 3 planes
					}

					// otherwise point is removed due to another plane..

					// Sys_Printf( "(%f, %f, %f)\n", pnt[0], pnt[1], pnt[2] );
				}
			}
		}
	}

#ifdef _DEBUG
//	Sys_Printf( "%i points on brush\n", pointList.size() );
#endif

	return static_cast<int>( pointList.size() );
}

void DBrush_addFace( DBrush& brush, const _QERFaceData& faceData ){
	brush.AddFace( Vector3( faceData.m_p0 ).data(), Vector3( faceData.m_p1 ).data(), Vector3( faceData.m_p2 ).data(), 0 );
}
typedef ReferenceCaller<DBrush, void(const _QERFaceData&), DBrush_addFace> DBrushAddFaceCaller;

void DBrush_addFaceTextured( DBrush& brush, const _QERFaceData& faceData ){
	brush.AddFace( Vector3( faceData.m_p0 ).data(), Vector3( faceData.m_p1 ).data(), Vector3( faceData.m_p2 ).data(), &faceData );
}
typedef ReferenceCaller<DBrush, void(const _QERFaceData&), DBrush_addFaceTextured> DBrushAddFaceTexturedCaller;

void DBrush::LoadFromBrush( scene::Instance& brush, bool textured ){
	ClearFaces();
	ClearPoints();

	GlobalBrushCreator().Brush_forEachFace( brush.path().top(), textured ? BrushFaceDataCallback( DBrushAddFaceTexturedCaller( *this ) ) : BrushFaceDataCallback( DBrushAddFaceCaller( *this ) ) );

	QER_entity = brush.path().parent().get_pointer();
	QER_brush = brush.path().top().get_pointer();
}

int DBrush::PointPosition( vec3_t pnt ){
	int state = POINT_IN_BRUSH; // if nothing happens point is inside brush

	for ( DPlane *plane : faceList )
	{
		float dist = plane->DistanceToPoint( pnt );

		if ( dist > MAX_ROUND_ERROR ) {
			return POINT_OUT_BRUSH;     // if point is in front of plane, it CANT be in the brush
		}
		else if ( fabs( dist ) < MAX_ROUND_ERROR ) {
			state = POINT_ON_BRUSH;     // if point is ON plane point is either ON the brush
		}
		// or outside it, it can no longer be in it
	}

	return state;
}

void DBrush::ClearPoints(){
	for ( DPoint *point : pointList ) {
		delete point;
	}
	pointList.clear();
}

void DBrush::ClearFaces(){
	bBoundsBuilt = false;
	for ( DPlane *plane : faceList )
	{
		delete plane;
	}
	faceList.clear();
}

void DBrush::AddPoint( vec3_t pnt ){
	DPoint* newPoint = new DPoint;
	VectorCopy( pnt, newPoint->_pnt );
	pointList.push_back( newPoint );
}

bool DBrush::HasPoint( vec3_t pnt ){
	for ( DPoint *chkPoint : pointList )
	{
		if ( *chkPoint == pnt ) {
			return true;
		}
	}

	return false;
}

int DBrush::RemoveRedundantPlanes(){
	int cnt = 0;
	std::list<DPlane *>::iterator chkPlane;

	// find duplicate planes
	std::list<DPlane *>::iterator p1 = faceList.begin();

	while ( p1 != faceList.end() )
	{
		std::list<DPlane *>::iterator p2 = p1;

		for ( p2++; p2 != faceList.end(); p2++ )
		{
			if ( **p1 == **p2 ) {
				if ( !strcmp( ( *p1 )->m_shader.c_str(), "textures/common/caulk" ) ) {
					delete *p1;
					p1 = faceList.erase( p1 );    // duplicate plane
				}
				else
				{
					delete *p2;
					p2 = faceList.erase( p2 );    // duplicate plane
				}

				cnt++;
				break;
			}
		}

		if ( p2 == faceList.end() ) {
			p1++;
		}
	}

	//+djbob kill planes with bad normal, they are more of a nuisance than losing a brush
	chkPlane = faceList.begin();
	while ( chkPlane != faceList.end() )
	{
		if ( VectorLength( ( *chkPlane )->normal ) == 0 ) { // plane has bad normal
			delete *chkPlane;
			chkPlane = faceList.erase( chkPlane );
			cnt++;
		}
		else {
			chkPlane++;
		}
	}
	//-djbob

	if ( pointList.size() == 0 ) { // if points may not have been built, build them
/*		if( BuildPoints() == 0 )	// just let the planes die if they are all bad
			return cnt;*/
		BuildPoints();
	}

	chkPlane = faceList.begin();
	while ( chkPlane != faceList.end() )
	{
		if ( ( *chkPlane )->IsRedundant( pointList ) ) { // checks that plane "0wnz" :), 3 or more points
			delete *chkPlane;
			chkPlane = faceList.erase( chkPlane );
			cnt++;
		}
		else{
			chkPlane++;
		}
	}

	return cnt;
}

bool DBrush::GetBounds( vec3_t min, vec3_t max ){
	BuildBounds();

	if ( !bBoundsBuilt ) {
		return false;
	}

	VectorCopy( bbox_min, min );
	VectorCopy( bbox_max, max );

	return true;
}

bool DBrush::BBoxCollision( DBrush* chkBrush ){
	vec3_t min1, min2;
	vec3_t max1, max2;

	if( !GetBounds( min1, max1 ) ){
		return false;
	}
	if( !chkBrush->GetBounds( min2, max2 ) ){
		return false;
	}

	if ( min1[0] >= max2[0] ) {
		return false;
	}
	if ( min1[1] >= max2[1] ) {
		return false;
	}
	if ( min1[2] >= max2[2] ) {
		return false;
	}

	if ( max1[0] <= min2[0] ) {
		return false;
	}
	if ( max1[1] <= min2[1] ) {
		return false;
	}
	if ( max1[2] <= min2[2] ) {
		return false;
	}

	return true;
}

DPlane* DBrush::HasPlane( DPlane* chkPlane ) const {
	for ( DPlane *plane : faceList )
	{
		if ( *plane == *chkPlane ) {
			return plane;
		}
	}
	return NULL;
}

bool DBrush::IsCutByPlane( DPlane *cuttingPlane ){
	bool isInFront;

	if ( pointList.size() == 0 ) {
		if ( BuildPoints() == 0 ) {
			return false;
		}
	}

	std::list<DPoint *>::const_iterator chkPnt = pointList.begin();

	if ( chkPnt == pointList.end() ) {
		return false;
	}

	float dist = cuttingPlane->DistanceToPoint( ( *chkPnt )->_pnt );

	if ( dist > MAX_ROUND_ERROR ) {
		isInFront = false;
	}
	else if ( dist < MAX_ROUND_ERROR ) {
		isInFront = true;
	}
	else{
		return true;
	}

	for ( chkPnt++ = pointList.begin(); chkPnt != pointList.end(); chkPnt++ )
	{
		dist = cuttingPlane->DistanceToPoint( ( *chkPnt )->_pnt );

		if ( dist > MAX_ROUND_ERROR ) {
			if ( isInFront ) {
				return true;
			}
		}
		else if ( dist < MAX_ROUND_ERROR ) {
			if ( !isInFront ) {
				return true;
			}
		}
		else{
			return true;
		}
	}

	return false;
}


scene::Node* DBrush::BuildInRadiant( bool allowDestruction, int* changeCnt, scene::Node* entity ){
	if ( allowDestruction ) {
		bool kill = true;

		for ( DPlane *plane : faceList )
		{
			if ( plane->m_bChkOk ) {
				kill = false;
				break;
			}
		}
		if ( kill ) {
			return NULL;
		}
	}

	//+djbob: fixed bug when brush had no faces "phantom brush" in radiant.
	if ( faceList.size() < 4 ) {
		globalErrorStream() << "Possible Phantom Brush Found, will not rebuild\n";
		return NULL;
	}
	//-djbob

	NodeSmartReference node( GlobalBrushCreator().createBrush() );

	for ( DPlane *plane : faceList ) {
		if ( plane->AddToBrush( node ) && changeCnt ) {
			( *changeCnt )++;
		}
	}

	if ( entity ) {
		Node_getTraversable( *entity )->insert( node );
	}
	else {
		Node_getTraversable( GlobalRadiant().getMapWorldEntity() )->insert( node );
	}

	QER_entity = entity;
	QER_brush = node.get_pointer();

	return node.get_pointer();
}

void DBrush::selectInRadiant() const {
	ASSERT_MESSAGE( QER_entity != nullptr, "QER_entity == nullptr" );
	ASSERT_MESSAGE( QER_brush != nullptr, "QER_brush == nullptr" );
	select_primitive( QER_brush, QER_entity );
}

void DBrush::CutByPlane( DPlane *cutPlane, DBrush **newBrush1, DBrush **newBrush2 ){
	if ( !IsCutByPlane( cutPlane ) ) {
		*newBrush1 = NULL;
		*newBrush2 = NULL;
		return;
	}

	DBrush* b1 = new DBrush;
	DBrush* b2 = new DBrush;

	for ( DPlane *plane : faceList )
	{
		b1->AddFace( plane->points[0], plane->points[1], plane->points[2], NULL );
		b2->AddFace( plane->points[0], plane->points[1], plane->points[2], NULL );
	}

	b1->AddFace( cutPlane->points[0], cutPlane->points[1], cutPlane->points[2], NULL );
	b2->AddFace( cutPlane->points[2], cutPlane->points[1], cutPlane->points[0], NULL );

	b1->RemoveRedundantPlanes();
	b2->RemoveRedundantPlanes();

	*newBrush1 = b1;
	*newBrush2 = b2;
}

bool DBrush::IntersectsWith( DBrush *chkBrush ){
	if ( pointList.size() == 0 ) {
		if ( BuildPoints() == 0 ) {
			return false;   // invalid brush!!!!

		}
	}
	if ( chkBrush->pointList.size() == 0 ) {
		if ( chkBrush->BuildPoints() == 0 ) {
			return false;   // invalid brush!!!!

		}
	}
	if ( !BBoxCollision( chkBrush ) ) {
		return false;
	}

	for ( DPlane *plane : faceList )
	{

		bool allInFront = true;
		for ( DPoint *point : chkBrush->pointList )
		{
			if ( plane->DistanceToPoint( point->_pnt ) < -MAX_ROUND_ERROR ) {
				allInFront = false;
				break;
			}
		}
		if ( allInFront ) {
			return false;
		}
	}

	for ( DPlane *plane : chkBrush->faceList )
	{
		bool allInFront = true;
		for ( DPoint *point : pointList )
		{
			if ( plane->DistanceToPoint( point->_pnt ) < -MAX_ROUND_ERROR ) {
				allInFront = false;
				break;
			}
		}
		if ( allInFront ) {
			return false;
		}
	}

	return true;
}

bool DBrush::IntersectsWith( DPlane* p1, DPlane* p2, vec3_t v ) {
	vec3_t vDown = { 0, 0, -1 };

	for ( DPlane *plane : faceList )
	{
		vec_t d = DotProduct( plane->normal, vDown );
		if ( d >= 0 ) {
			continue;
		}
		if ( plane->PlaneIntersection( p1, p2, v ) ) {
			if ( PointPosition( v ) != POINT_OUT_BRUSH ) {
				return true;
			}
		}
	}

	return false;
}

void DBrush::BuildBounds(){
	if ( !bBoundsBuilt ) {
		if ( pointList.size() == 0 ) { // if points may not have been built, build them
			if ( BuildPoints() == 0 ) {
				return;
			}
		}

		std::list<DPoint *>::const_iterator first = pointList.begin();
		VectorCopy( ( *first )->_pnt, bbox_min );
		VectorCopy( ( *first )->_pnt, bbox_max );

		std::list<DPoint *>::const_iterator point = pointList.begin();
		for ( point++; point != pointList.end(); point++ )
		{
			if ( ( *point )->_pnt[0] > bbox_max[0] ) {
				bbox_max[0] = ( *point )->_pnt[0];
			}
			if ( ( *point )->_pnt[1] > bbox_max[1] ) {
				bbox_max[1] = ( *point )->_pnt[1];
			}
			if ( ( *point )->_pnt[2] > bbox_max[2] ) {
				bbox_max[2] = ( *point )->_pnt[2];
			}

			if ( ( *point )->_pnt[0] < bbox_min[0] ) {
				bbox_min[0] = ( *point )->_pnt[0];
			}
			if ( ( *point )->_pnt[1] < bbox_min[1] ) {
				bbox_min[1] = ( *point )->_pnt[1];
			}
			if ( ( *point )->_pnt[2] < bbox_min[2] ) {
				bbox_min[2] = ( *point )->_pnt[2];
			}
		}

		bBoundsBuilt = true;
	}
}

bool DBrush::BBoxTouch( DBrush *chkBrush ){
	vec3_t min1, min2;
	vec3_t max1, max2;

	if( !GetBounds( min1, max1 ) ){
		return false;
	}
	if( !chkBrush->GetBounds( min2, max2 ) ){
		return false;
	}

	if ( ( min1[0] - max2[0] ) > MAX_ROUND_ERROR ) {
		return false;
	}
	if ( ( min1[1] - max2[1] ) > MAX_ROUND_ERROR ) {
		return false;
	}
	if ( ( min1[2] - max2[2] ) > MAX_ROUND_ERROR ) {
		return false;
	}

	if ( ( min2[0] - max1[0] ) > MAX_ROUND_ERROR ) {
		return false;
	}
	if ( ( min2[1] - max1[1] ) > MAX_ROUND_ERROR ) {
		return false;
	}
	if ( ( min2[2] - max1[2] ) > MAX_ROUND_ERROR ) {
		return false;
	}

	int cnt = 0;

	if ( ( min2[0] - max1[0] ) == 0 ) {
		cnt++;
	}

	if ( ( min2[1] - max1[1] ) == 0 ) {
		cnt++;
	}

	if ( ( min2[2] - max1[2] ) == 0 ) {
		cnt++;
	}

	if ( ( min1[0] - max2[0] ) == 0 ) {
		cnt++;
	}

	if ( ( min1[1] - max2[1] ) == 0 ) {
		cnt++;
	}

	if ( ( min1[2] - max2[2] ) == 0 ) {
		cnt++;
	}

	if ( cnt > 1 ) {
		return false;
	}

	return true;
}

void DBrush::ResetChecks( const std::vector<CopiedString>& exclusionList ){
	for ( DPlane *plane : faceList )
	{
		plane->m_bChkOk = std::any_of( exclusionList.cbegin(), exclusionList.cend(),
			[plane]( const CopiedString& texture ){
				return strstr( plane->m_shader.c_str(), texture.c_str() ) != nullptr;
			} );
	}
}

DPlane* DBrush::HasPlaneInverted( DPlane *chkPlane ){
	for ( DPlane *plane : faceList )
	{
		if ( *plane != *chkPlane ) {
			if ( fabs( plane->_d + chkPlane->_d ) < 0.1 ) {
				return plane;
			}
		}
	}
	return NULL;
}

bool DBrush::HasTexture( const char *textureName ){
	for ( const DPlane *plane : faceList )
	{
		if ( strstr( plane->m_shader.c_str(), textureName ) ) {
			return true;
		}

	}
	return false;
}

bool DBrush::IsDetail(){
	for ( const DPlane *plane : faceList )
	{
		if ( plane->texInfo.contents & FACE_DETAIL ) {
			return true;
		}

	}
	return false;
}

void DBrush::BuildFromWinding( DWinding *w ){
	if ( w->numpoints < 3 ) {
		globalErrorStream() << "Winding has invalid number of points";
		return;
	}

	DPlane* wPlane = w->WindingPlane();

	DWinding* w2;
	w2 = w->CopyWinding();
	int i;
	for ( i = 0; i < w2->numpoints; i++ )
		VectorAdd( w2->p[i], wPlane->normal, w2->p[i] );

	AddFace( w2->p[0], w2->p[1], w2->p[2], NULL );
	AddFace( w->p[2], w->p[1], w->p[0], NULL );

	for ( i = 0; i < w->numpoints - 1; i++ )
		AddFace( w2->p[i], w->p[i], w->p[i + 1], NULL );
	AddFace( w2->p[w->numpoints - 1], w->p[w->numpoints - 1], w->p[0], NULL );

	delete wPlane;
	delete w2;
}

void DBrush::SaveToFile( FILE *pFile ){
	fprintf( pFile, "{\n" );

	for ( const DPlane *pp : faceList )
	{
		char buffer[512];

		sprintf( buffer, "( %.0f %.0f %.0f ) ( %.0f %.0f %.0f ) ( %.0f %.0f %.0f ) %s %.0f %.0f %f %f %.0f 0 0 0\n",
		         pp->points[0][0], pp->points[0][1], pp->points[0][2],
		         pp->points[1][0], pp->points[1][1], pp->points[1][2],
		         pp->points[2][0], pp->points[2][1], pp->points[2][2],
		         pp->m_shader.c_str(),
		         pp->texInfo.m_texdef.shift[0], pp->texInfo.m_texdef.shift[1],
		         pp->texInfo.m_texdef.scale[0], pp->texInfo.m_texdef.scale[0],
		         pp->texInfo.m_texdef.rotate );

		fprintf( pFile, "%s", buffer );
	}

	fprintf( pFile, "}\n" );
}

void DBrush::Rotate( vec3_t vOrigin, vec3_t vRotation ){
	for ( DPlane *plane : faceList )
	{
		for ( int i = 0; i < 3; i++ )
			VectorRotate( plane->points[i], vRotation, vOrigin );

		plane->Rebuild();
	}
}

void DBrush::RotateAboutCentre( vec3_t vRotation ){
	vec3_t min, max, centre;
	if( !GetBounds( min, max ) ){
		return;
	}
	VectorAdd( min, max, centre );
	VectorScale( centre, 0.5f, centre );

	Rotate( centre, vRotation );
}

bool DBrush::ResetTextures( const char* textureName, float fScale[2],     float fShift[2],     int rotation, const char* newTextureName,
                            bool bResetTextureName,  bool bResetScale[2], bool bResetShift[2], bool bResetRotation ){
	if ( textureName ) {
		bool changed = false;
		for ( DPlane *plane : faceList )
		{
			if ( !strcmp( plane->m_shader.c_str(), textureName ) ) {
				if ( bResetTextureName ) {
					plane->m_shader = newTextureName;
				}

				if ( bResetScale[0] ) {
					plane->texInfo.m_texdef.scale[0] = fScale[0];
				}
				if ( bResetScale[1] ) {
					plane->texInfo.m_texdef.scale[1] = fScale[1];
				}

				if ( bResetShift[0] ) {
					plane->texInfo.m_texdef.shift[0] = fShift[0];
				}
				if ( bResetShift[1] ) {
					plane->texInfo.m_texdef.shift[1] = fShift[1];
				}

				if ( bResetRotation ) {
					plane->texInfo.m_texdef.rotate = (float)rotation;
				}

				changed = true;
			}
		}
		return changed; // no point rebuilding unless we need to, only slows things down
	}
	else
	{
		for ( DPlane *plane : faceList )
		{
			if ( bResetTextureName ) {
				plane->m_shader = newTextureName;
			}

			if ( bResetScale[0] ) {
				plane->texInfo.m_texdef.scale[0] = fScale[0];
			}
			if ( bResetScale[1] ) {
				plane->texInfo.m_texdef.scale[1] = fScale[1];
			}

			if ( bResetShift[0] ) {
				plane->texInfo.m_texdef.shift[0] = fShift[0];
			}
			if ( bResetShift[1] ) {
				plane->texInfo.m_texdef.shift[1] = fShift[1];
			}

			if ( bResetRotation ) {
				plane->texInfo.m_texdef.rotate = (float)rotation;
			}
		}
		return true;
	}
}

bool DBrush::operator ==( const DBrush* other ) const {
	for ( DPlane *plane : faceList )
	{
		if ( !other->HasPlane( plane ) ) {
			return false;
		}
	}

	for ( DPlane *plane : other->faceList )
	{
		if ( !HasPlane( plane ) ) {
			return false;
		}
	}

	return true;
}

DPlane* DBrush::AddFace( const vec3_t va, const vec3_t vb, const vec3_t vc, const char *textureName, bool bDetail ){
	bBoundsBuilt = false;
	DPlane* newFace = new DPlane( va, vb, vc, textureName, bDetail );
	faceList.push_back( newFace );

	return newFace;
}

DPlane* DBrush::FindPlaneWithClosestNormal( vec_t* normal ) {
	vec_t bestDot = -2;
	DPlane* bestDotPlane = NULL;
	for ( DPlane *plane : faceList ) {
		vec_t dot = DotProduct( plane->normal, normal );
		if ( dot > bestDot ) {
			bestDot = dot;
			bestDotPlane = plane;
		}
	}

	return bestDotPlane;
}

int DBrush::FindPointsForPlane( DPlane* plane, DPoint** pnts, int maxpnts ) {
	int numpnts = 0;

	if ( !maxpnts ) {
		return 0;
	}

	BuildPoints();

	for ( DPoint *point : pointList )
	{
		if ( fabs( plane->DistanceToPoint( point->_pnt ) ) < MAX_ROUND_ERROR ) {
			pnts[numpnts] = point;
			numpnts++;

			if ( numpnts >= maxpnts ) {
				return numpnts;
			}

		}
	}

	return numpnts;
}

void DBrush::RemovePlane( DPlane* plane ) {
	bBoundsBuilt = false;
	for ( DPlane *deadPlane : faceList ) {
		if ( deadPlane == plane ) {
			delete deadPlane;
			faceList.remove( plane );
		}
	}
}

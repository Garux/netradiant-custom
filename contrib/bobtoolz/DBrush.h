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

// DBrush.h: interface for the DBrush class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdio>
#include <list>
#include <vector>
#include "mathlib.h"
#include "string/string.h"

class DPlane;
class DWinding;
class DPoint;
class _QERFaceData;

namespace scene
{
class Node;
class Instance;
}

#define POINT_IN_BRUSH  0
#define POINT_ON_BRUSH  1
#define POINT_OUT_BRUSH 2

class DBrush
{
public:
	DPlane* AddFace( const vec3_t va, const vec3_t vb, const vec3_t vc, const char* textureName, bool bDetail );
	void SaveToFile( FILE* pFile );

	void Rotate( vec3_t vOrigin, vec3_t vRotation );
	void RotateAboutCentre( vec3_t vRotation );

	DPlane* HasPlaneInverted( DPlane* chkPlane );
	DPlane* HasPlane( DPlane* chkPlane ) const;
	DPlane* AddFace( const vec3_t va, const vec3_t vb, const vec3_t vc, const _QERFaceData* texData );

	bool ResetTextures( const char* textureName, float fScale[2], float fShift[2], int rotation, const char* newTextureName, bool bResetTextureName, bool bResetScale[2], bool bResetShift[2], bool bResetRotation );
	bool IsDetail();
	bool HasTexture( const char* textureName );
	bool IntersectsWith( DBrush *chkBrush );
	bool IntersectsWith( DPlane* p1, DPlane* p2, vec3_t v );
	bool IsCutByPlane( DPlane* cuttingPlane );
	bool GetBounds( vec3_t min, vec3_t max );
	bool HasPoint( vec3_t pnt );
	bool BBoxCollision( DBrush* chkBrush );
	bool BBoxTouch( DBrush* chkBrush );

	int BuildPoints();
	void BuildBounds();
	void BuildFromWinding( DWinding* w );
	scene::Node* BuildInRadiant( bool allowDestruction, int* changeCnt, scene::Node* entity = NULL );
	void selectInRadiant() const;

	void ResetChecks( const std::vector<CopiedString>& exclusionList );

	void ClearFaces();
	void ClearPoints();

	int RemoveRedundantPlanes( void );
	void RemovePlane( DPlane* plane );
	int PointPosition( vec3_t pnt );


	void CutByPlane( DPlane* cutPlane, DBrush** newBrush1, DBrush** newBrush2 );

	void LoadFromBrush( scene::Instance& brush, bool textured );
	void AddPoint( vec3_t pnt );

	DPlane* FindPlaneWithClosestNormal( vec_t* normal );
	int FindPointsForPlane( DPlane* plane, DPoint** pnts, int maxpnts );

	DBrush();
	DBrush( DBrush&& ) noexcept = delete;
	virtual ~DBrush();

	bool operator==( const DBrush* other ) const;

//	members
	scene::Node* QER_entity;
	scene::Node* QER_brush;
	std::list<DPlane*> faceList;
	std::list<DPoint*> pointList;
	vec3_t bbox_min, bbox_max;
	bool bBoundsBuilt;
};

//typedef CList<DBrush*, DBrush*> DBrushList;

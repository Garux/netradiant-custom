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


#include "shapes.h"

#include "DPoint.h"
#include "DPlane.h"

#include "misc.h"
#include "funchandlers.h"

#include "iundo.h"
#include "ishaders.h"
#include "ientity.h"
#include "ieclass.h"
#include "ipatch.h"
#include "qerplugin.h"

#include <algorithm>
#include <ctime>

#include "scenelib.h"
#include "texturelib.h"

/************************
    Cube Diagram
************************/

/*

        7 ----- 5
        /|    /|
       / |   / |
      /  |  /  |
    4 ----- 6  |
     |  2|_|___|8
     |  /  |   /
     | /   |  /       ----> WEST, definitely
    ||/    | /
    1|_____|/3

 */

/************************
    Global Variables
************************/

vec3_t g_Origin = { 0.0f, 0.0f, 0.0f };

extern bool bFacesAll[];

/************************
    Helper Functions
************************/

float Deg2Rad( float angle ){
	return angle * Q_PI / 180;
}
// points in CCW order
void AddFaceWithTexture( scene::Node& brush, const vec3_accu_t va, const vec3_accu_t vb, const vec3_accu_t vc, const char* texture, bool detail ){
	_QERFaceData faceData;
	FillDefaultTexture( &faceData, va, vb, vc, texture );
	if ( detail ) {
		faceData.contents |= FACE_DETAIL;
	}
	GlobalBrushCreator().Brush_addFace( brush, faceData );
}
void AddFaceWithTexture( scene::Node& brush, const vec3_t va, const vec3_t vb, const vec3_t vc, const char* texture, bool detail ){
	AddFaceWithTexture( brush, vec3_accu_t{ va[0], va[1], va[2] }, vec3_accu_t{ vb[0], vb[1], vb[2] }, vec3_accu_t{ vc[0], vc[1], vc[2] }, texture, detail );
}

void AddFaceWithTextureScaled( scene::Node& brush, vec3_t va, vec3_t vb, vec3_t vc,
                               const char* texture, bool bVertScale, bool bHorScale,
                               float minX, float minY, float maxX, float maxY ){
	qtexture_t* pqtTexInfo;

	// TTimo: there used to be a call to pfnHasShader here
	//   this was not necessary. In Radiant everything is shader.
	//   If a texture doesn't have a shader script, a default shader object is used.
	// The IShader object was leaking also
	// collect texture info: sizes, etc
	IShader* i = GlobalShaderSystem().getShaderForName( texture );
	pqtTexInfo = i->getTexture(); // shader width/height doesn't come out properly

	if ( pqtTexInfo ) {
		float scale[2] = {0.5f, 0.5f};
		float shift[2] = {0, 0};

		if ( bHorScale ) {
			float width = maxX - minX;

			scale[0] = width / pqtTexInfo->width;
			shift[0] = -( (int)maxX % (int)width ) / scale[0];
		}

		if ( bVertScale ) {
			float height = maxY - minY;

			scale[1] = height / pqtTexInfo->height;
			shift[1] = ( (int)minY % (int)height ) / scale[1];
		}

		_QERFaceData addFace;
		FillDefaultTexture( &addFace, va, vb, vc, texture );
		addFace.m_texdef.scale[0] = scale[0];
		addFace.m_texdef.scale[1] = scale[1];
		addFace.m_texdef.shift[0] = shift[0];
		addFace.m_texdef.shift[1] = shift[1];

		GlobalBrushCreator().Brush_addFace( brush, addFace );
	}
	else
	{
		// shouldn't even get here, as default missing texture should be returned if
		// texture doesn't exist, but just in case
		AddFaceWithTexture( brush, va, vb, vc, texture, false );
		globalErrorStream() << "BobToolz::Invalid Texture Name-> " << texture;
	}
	// the IShader is not kept referenced, DecRef it
	i->DecRef();
}

/************************
    --Main Functions--
************************/

void Build_Wedge( int dir, vec3_t min, vec3_t max, bool bUp ){
	NodeSmartReference newBrush( GlobalBrushCreator().createBrush() );

	vec3_t v1, v2, v3, v5, v6, v7, v8;
	VectorCopy( min, v1 );
	VectorCopy( min, v2 );
	VectorCopy( min, v3 );
	VectorCopy( max, v5 );
	VectorCopy( max, v6 );
	VectorCopy( max, v7 );
	VectorCopy( max, v8 );

	v2[0] = max[0];
	v3[1] = max[1];

	v6[0] = min[0];
	v7[1] = min[1];
	v8[2] = min[2];

	if ( bUp ) {

		if ( dir != MOVE_EAST ) {
			AddFaceWithTexture( newBrush, v1, v3, v6, "textures/common/caulk", false );
		}

		if ( dir != MOVE_WEST ) {
			AddFaceWithTexture( newBrush, v7, v5, v8, "textures/common/caulk", false );
		}

		if ( dir != MOVE_NORTH ) {
			AddFaceWithTexture( newBrush, v1, v7, v2, "textures/common/caulk", false );
		}

		if ( dir != MOVE_SOUTH ) {
			AddFaceWithTexture( newBrush, v3, v8, v6, "textures/common/caulk", false );
		}

		AddFaceWithTexture( newBrush, v1, v2, v3, "textures/common/caulk", false );

		if ( dir == MOVE_EAST ) {
			AddFaceWithTexture( newBrush, v1, v3, v5, "textures/common/caulk", false );
		}

		if ( dir == MOVE_WEST ) {
			AddFaceWithTexture( newBrush, v2, v6, v8, "textures/common/caulk", false );
		}

		if ( dir == MOVE_NORTH ) {
			AddFaceWithTexture( newBrush, v1, v6, v5, "textures/common/caulk", false );
		}

		if ( dir == MOVE_SOUTH ) {
			AddFaceWithTexture( newBrush, v7, v3, v8, "textures/common/caulk", false );
		}
	}
	else
	{
		if ( dir != MOVE_WEST ) {
			AddFaceWithTexture( newBrush, v7, v5, v8, "textures/common/caulk", false );
		}

		if ( dir != MOVE_EAST ) {
			AddFaceWithTexture( newBrush, v1, v3, v6, "textures/common/caulk", false );
		}

		if ( dir != MOVE_NORTH ) {
			AddFaceWithTexture( newBrush, v3, v8, v6, "textures/common/caulk", false );
		}

		if ( dir != MOVE_SOUTH ) {
			AddFaceWithTexture( newBrush, v1, v7, v2, "textures/common/caulk", false );
		}


		AddFaceWithTexture( newBrush, v6, v5, v7, "textures/common/caulk", false );

		if ( dir == MOVE_WEST ) {
			AddFaceWithTexture( newBrush, v1, v5, v3, "textures/common/caulk", false );
		}

		if ( dir == MOVE_EAST ) {
			AddFaceWithTexture( newBrush, v2, v8, v6, "textures/common/caulk", false );
		}

		if ( dir == MOVE_NORTH ) {
			AddFaceWithTexture( newBrush, v1, v5, v6, "textures/common/caulk", false );
		}

		if ( dir == MOVE_SOUTH ) {
			AddFaceWithTexture( newBrush, v7, v8, v3, "textures/common/caulk", false );
		}
	}

	Node_getTraversable( GlobalRadiant().getMapWorldEntity() )->insert( newBrush );
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

void Build_StairStep_Wedge( int dir, vec3_t min, vec3_t max, const char* mainTexture, const char* riserTexture, bool detail ){
	NodeSmartReference newBrush( GlobalBrushCreator().createBrush() );

	//----- Build Outer Bounds ---------

	vec3_t v1, v2, v3, v5, v6, v7, v8;
	VectorCopy( min, v1 );
	VectorCopy( min, v2 );
	VectorCopy( min, v3 );
	VectorCopy( max, v5 );
	VectorCopy( max, v6 );
	VectorCopy( max, v7 );
	VectorCopy( max, v8 );

	v2[0] = max[0];
	v3[1] = max[1];

	v6[0] = min[0];
	v7[1] = min[1];

	v8[2] = min[2];
	//v8 needed this time, becoz of sloping faces (2-4-6-8)

	//----------------------------------

	AddFaceWithTexture( newBrush, v6, v5, v7, mainTexture, detail );

	if ( dir != MOVE_EAST ) {
		if ( dir == MOVE_WEST ) {
			AddFaceWithTexture( newBrush, v5, v2, v7, riserTexture, detail );
		}
		else{
			AddFaceWithTexture( newBrush, v5, v2, v7, "textures/common/caulk", detail );
		}
	}

	if ( dir != MOVE_WEST ) {
		if ( dir == MOVE_EAST ) {
			AddFaceWithTexture( newBrush, v1, v3, v6, riserTexture, detail );
		}
		else{
			AddFaceWithTexture( newBrush, v1, v3, v6, "textures/common/caulk", detail );
		}
	}

	if ( dir != MOVE_NORTH ) {
		if ( dir == MOVE_SOUTH ) {
			AddFaceWithTexture( newBrush, v3, v5, v6, riserTexture, detail );
		}
		else{
			AddFaceWithTexture( newBrush, v3, v5, v6, "textures/common/caulk", detail );
		}
	}

	if ( dir != MOVE_SOUTH ) {
		if ( dir == MOVE_NORTH ) {
			AddFaceWithTexture( newBrush, v1, v7, v2, riserTexture, detail );
		}
		else{
			AddFaceWithTexture( newBrush, v1, v7, v2, "textures/common/caulk", detail );
		}
	}


	if ( dir == MOVE_EAST ) {
		AddFaceWithTexture( newBrush, v1, v5, v3, "textures/common/caulk", detail );
	}

	if ( dir == MOVE_WEST ) {
		AddFaceWithTexture( newBrush, v2, v8, v6, "textures/common/caulk", detail );
	}

	if ( dir == MOVE_NORTH ) {
		AddFaceWithTexture( newBrush, v1, v5, v6, "textures/common/caulk", detail );
	}

	if ( dir == MOVE_SOUTH ) {
		AddFaceWithTexture( newBrush, v7, v8, v3, "textures/common/caulk", detail );
	}

	Node_getTraversable( GlobalRadiant().getMapWorldEntity() )->insert( newBrush );
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

// internal use only, to get a box without finishing construction
scene::Node& Build_Get_BoundingCube_Selective( vec3_t min, vec3_t max, char* texture, bool* useFaces ){
	NodeSmartReference newBrush( GlobalBrushCreator().createBrush() );

	//----- Build Outer Bounds ---------

	vec3_t v1, v2, v3, v5, v6, v7;
	VectorCopy( min, v1 );
	VectorCopy( min, v2 );
	VectorCopy( min, v3 );
	VectorCopy( max, v5 );
	VectorCopy( max, v6 );
	VectorCopy( max, v7 );

	v2[0] = max[0];
	v3[1] = max[1];

	v6[0] = min[0];
	v7[1] = min[1];

	//----------------------------------

	//----- Add Six Cube Faces ---------

	if ( useFaces[0] ) {
		AddFaceWithTexture( newBrush, v1, v2, v3, texture, false );
	}
	if ( useFaces[1] ) {
		AddFaceWithTexture( newBrush, v1, v3, v6, texture, false );
	}
	if ( useFaces[2] ) {
		AddFaceWithTexture( newBrush, v1, v7, v2, texture, false );
	}

	if ( useFaces[3] ) {
		AddFaceWithTexture( newBrush, v5, v6, v3, texture, false );
	}
	if ( useFaces[4] ) {
		AddFaceWithTexture( newBrush, v5, v2, v7, texture, false );
	}
	if ( useFaces[5] ) {
		AddFaceWithTexture( newBrush, v5, v7, v6, texture, false );
	}

	//----------------------------------

	return newBrush;
}

scene::Node& Build_Get_BoundingCube( vec3_t min, vec3_t max, char* texture ){
	return Build_Get_BoundingCube_Selective( min, max, texture, bFacesAll );
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

void Build_StairStep( vec3_t min, vec3_t max, const char* mainTexture, const char* riserTexture, int direction ){
	NodeSmartReference newBrush( GlobalBrushCreator().createBrush() );

	//----- Build Outer Bounds ---------

	vec3_t v1, v2, v3, v5, v6, v7;
	VectorCopy( min, v1 );
	VectorCopy( min, v2 );
	VectorCopy( min, v3 );
	VectorCopy( max, v5 );
	VectorCopy( max, v6 );
	VectorCopy( max, v7 );

	v2[0] = max[0];
	v3[1] = max[1];

	v6[0] = min[0];
	v7[1] = min[1];

	//----------------------------------

	AddFaceWithTexture( newBrush, v6, v5, v7, mainTexture, false );
	// top gets current texture


	if ( direction == MOVE_EAST ) {
		AddFaceWithTexture( newBrush, v1, v3, v6, riserTexture, false );
	}
	else{
		AddFaceWithTexture( newBrush, v1, v3, v6, "textures/common/caulk", false );
	}
	// west facing side, etc...


	if ( direction == MOVE_NORTH ) {
		AddFaceWithTexture( newBrush, v1, v7, v2, riserTexture, false );
	}
	else{
		AddFaceWithTexture( newBrush, v1, v7, v2, "textures/common/caulk", false );
	}

	if ( direction == MOVE_SOUTH ) {
		AddFaceWithTexture( newBrush, v3, v5, v6, riserTexture, false );
	}
	else{
		AddFaceWithTexture( newBrush, v3, v5, v6, "textures/common/caulk", false );
	}

	if ( direction == MOVE_WEST ) {
		AddFaceWithTexture( newBrush, v7, v5, v2, riserTexture, false );
	}
	else{
		AddFaceWithTexture( newBrush, v7, v5, v2, "textures/common/caulk", false );
	}


	AddFaceWithTexture( newBrush, v1, v2, v3, "textures/common/caulk", false );
	// base is caulked

	Node_getTraversable( GlobalRadiant().getMapWorldEntity() )->insert( newBrush );
	// finish brush
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

void BuildDoorsX2( vec3_t min, vec3_t max,
                   bool bSclMainHor, bool bSclMainVert,
                   bool bSclTrimHor, bool bSclTrimVert,
                   const char* mainTexture, const char* trimTexture,
                   int direction ){
	int xy;
	if ( direction == 0 ) {
		xy = 0;
	}
	else{
		xy = 1;
	}

	//----- Build Outer Bounds ---------

	vec3_t v1, v2, v3, v5, v6, v7, ve_1, ve_2, ve_3;
	VectorCopy( min, v1 );
	VectorCopy( min, v2 );
	VectorCopy( min, v3 );
	VectorCopy( max, v5 );
	VectorCopy( max, v6 );
	VectorCopy( max, v7 );

	v2[0] = max[0];
	v3[1] = max[1];

	v6[0] = min[0];
	v7[1] = min[1];

	float width = ( max[xy] - min[xy] ) / 2;

	if ( direction == 0 ) {
		VectorCopy( v1, ve_1 );
		VectorCopy( v3, ve_2 );
		VectorCopy( v6, ve_3 );
	}
	else
	{
		VectorCopy( v7, ve_1 );
		VectorCopy( v1, ve_2 );
		VectorCopy( v2, ve_3 );
	}

	ve_1[xy] += width;
	ve_2[xy] += width;
	ve_3[xy] += width;

	//----------------------------------

	NodeSmartReference newBrush1( GlobalBrushCreator().createBrush() );
	NodeSmartReference newBrush2( GlobalBrushCreator().createBrush() );

	AddFaceWithTexture( newBrush1, v1, v2, v3, "textures/common/caulk", false );
	AddFaceWithTexture( newBrush1, v5, v7, v6, "textures/common/caulk", false );

	AddFaceWithTexture( newBrush2, v1, v2, v3, "textures/common/caulk", false );
	AddFaceWithTexture( newBrush2, v5, v7, v6, "textures/common/caulk", false );

	if ( direction == 0 ) {
		AddFaceWithTexture( newBrush1, v1, v3, v6, "textures/common/caulk", false );
		AddFaceWithTexture( newBrush2, v5, v2, v7, "textures/common/caulk", false );
	}
	else
	{
		AddFaceWithTexture( newBrush1, v1, v7, v2, "textures/common/caulk", false );
		AddFaceWithTexture( newBrush2, v5, v6, v3, "textures/common/caulk", false );
	}

	if ( direction == 0 ) {
		AddFaceWithTextureScaled( newBrush1, v1, v7, v2, mainTexture, bSclMainVert, bSclMainHor,
		                          min[0], min[2], max[0], max[2] );
		AddFaceWithTextureScaled( newBrush1, v5, v6, v3, mainTexture, bSclMainVert, bSclMainHor,
		                          max[0], min[2], min[0], max[2] );


		AddFaceWithTextureScaled( newBrush2, v1, v7, v2, mainTexture, bSclMainVert, bSclMainHor,
		                          min[0], min[2], max[0], max[2] );
		AddFaceWithTextureScaled( newBrush2, v5, v6, v3, mainTexture, bSclMainVert, bSclMainHor,
		                          max[0], min[2], min[0], max[2] ); // flip max/min to reverse tex dir



		AddFaceWithTextureScaled( newBrush1, ve_3, ve_2, ve_1, trimTexture, bSclTrimVert, bSclTrimHor,
		                          min[1], min[2], max[1], max[2] );

		AddFaceWithTextureScaled( newBrush2, ve_1, ve_2, ve_3, trimTexture, bSclTrimVert, bSclTrimHor,
		                          max[1], min[2], min[1], max[2] );
	}
	else
	{
		AddFaceWithTextureScaled( newBrush1, v1, v3, v6, mainTexture, bSclMainVert, bSclMainHor,
		                          min[1], min[2], max[1], max[2] );
		AddFaceWithTextureScaled( newBrush1, v5, v2, v7, mainTexture, bSclMainVert, bSclMainHor,
		                          max[1], min[2], min[1], max[2] );


		AddFaceWithTextureScaled( newBrush2, v1, v3, v6, mainTexture, bSclMainVert, bSclMainHor,
		                          min[1], min[2], max[1], max[2] );
		AddFaceWithTextureScaled( newBrush2, v5, v2, v7, mainTexture, bSclMainVert, bSclMainHor,
		                          max[1], min[2], min[1], max[2] ); // flip max/min to reverse tex dir


		AddFaceWithTextureScaled( newBrush1, ve_1, ve_2, ve_3, trimTexture, bSclTrimVert, bSclTrimHor,
		                          min[0], min[2], max[0], max[2] );

		AddFaceWithTextureScaled( newBrush2, ve_3, ve_2, ve_1, trimTexture, bSclTrimVert, bSclTrimHor,
		                          max[0], min[2], min[0], max[2] );
	}

	//----------------------------------


	EntityClass* doorClass = GlobalEntityClassManager().findOrInsert( "func_door", true );
	NodeSmartReference pEDoor1( GlobalEntityCreator().createEntity( doorClass ) );
	NodeSmartReference pEDoor2( GlobalEntityCreator().createEntity( doorClass ) );

	if ( direction == 0 ) {
		Node_getEntity( pEDoor1 )->setKeyValue( "angle", "180" );
		Node_getEntity( pEDoor2 )->setKeyValue( "angle", "360" );
	}
	else
	{
		Node_getEntity( pEDoor1 )->setKeyValue( "angle", "270" );
		Node_getEntity( pEDoor2 )->setKeyValue( "angle", "90" );
	}

	srand( (unsigned)time( nullptr ) );

	char teamname[256];
	sprintf( teamname, "t%i", rand() );
	Node_getEntity( pEDoor1 )->setKeyValue( "team", teamname );
	Node_getEntity( pEDoor2 )->setKeyValue( "team", teamname );

	Node_getTraversable( pEDoor1 )->insert( newBrush1 );
	Node_getTraversable( pEDoor2 )->insert( newBrush2 );

	Node_getTraversable( GlobalSceneGraph().root() )->insert( pEDoor1 );
	Node_getTraversable( GlobalSceneGraph().root() )->insert( pEDoor2 );

//	ResetCurrentTexture();
}

#include "DBrush.h"
#include "dialogs/dialogs-gtk.h"

/*
?snap tmp verts for aabb to .125 for lip accuracy (helps) (not relevant when finding out via radiant means)
?option to actually snap verts //up to user now in from-winding gen method, except center
	arbitrary winding
	add sound cull brushes
	move dist control
	time setting
?oval shape when generating
	prefer +face when similar area
?explicit ccw setting or better
inner layer filling orientation depending on move angle
?round expansion in non round shape
?way to adjust lip, speed of existing door
?reset options on dialog 'cancel'
*/

void BuildApertureDoors( scene::Instance& brushinstance, const class ApertureDoorRS& rs ){
	auto float2string = [string = std::array<char, 64>()]( float value ) mutable -> const char* {
		sprintf( string.data(), "%g", value );
		return string.data();
	};

	DBrush dbrush;
	dbrush.LoadFromBrush( brushinstance, false );

	const DPlane *bestplane = nullptr;
	DWinding bestwinding;
	{
		vec_t bestarea = 0;
		for( const DPlane *plane : dbrush.faceList )
		{
			DWinding w = plane->BaseWindingForPlane();
			for( const DPlane *cplane : dbrush.faceList )
			{
				DPlane negcplane( *cplane ); //negate plane to keep proper side
				VectorNegate( negcplane.normal, negcplane.normal );
				negcplane._d = -negcplane._d;
				if( plane != cplane )
					w.ChopWindingInPlace( &negcplane, .1f );
				if( w.numpoints < 3 )
					break;
			}
			if( const vec_t area = w.WindingArea(); area > bestarea + 1 //definitely better
			// in doubts prefer positive direction for selection consistency
			|| ( area > bestarea - 1 && VectorMax( plane->normal ) > VectorMax( bestplane->normal ) ) ){
				bestarea = area;
				bestplane = plane;
				bestwinding = std::move( w );
			}
		}
	}
	if( bestplane != nullptr ){
		UndoableCommand undo( "bobToolz.buildApertureDoor" );

		vec3_t center;
		bestwinding.WindingCentroid( center );

		double thickness = 32;
		{
			DPlane *negplane = nullptr;
			vec_t bestdot = 1;
			for( DPlane *plane : dbrush.faceList )
			{
				if( vec_t dot = DotProduct( bestplane->normal, plane->normal ); dot < bestdot ){
					bestdot = dot;
					negplane = plane;
				}
			}
			if( negplane != nullptr )
				thickness = -negplane->DistanceToPoint( center );
		}
		vec_t radius = std::numeric_limits<vec_t>::max();
		for( int i = bestwinding.numpoints - 1, j = 0; j < bestwinding.numpoints; i = j++ )
		{
			vec3_t r;
			VectorMid( bestwinding.p[i], bestwinding.p[j], r );
			VectorSubtract( r, center, r );
			radius = std::min( radius, VectorLength( r ) );
		}

		{
			const DoubleVector3 normal( vector3_from_array( bestplane->normal ) );
			const DoubleVector3 rot = vector3_normalised( plane3_project_point( Plane3( normal, 0 ), // 0 distance to project vector
				g_vector3_axes[ vector3_min_abs_component_index( normal ) ],
				g_vector3_axes[ vector3_max_abs_component_index( normal ) ] ) ) * radius + vector3_from_array( center );

			srand( (unsigned)time( nullptr ) );
			char teamname[256];
			sprintf( teamname, "t%i", rand() );
			EntityClass* doorClass = GlobalEntityClassManager().findOrInsert( "func_door", true );

			const DoubleVector3 cent = vector3_from_array( center );
			std::vector<DoubleVector3> winding;
			if( rs.fromWinding ){
				for( const auto& p : std::span( bestwinding.p, bestwinding.numpoints ) )
					winding.push_back( vector3_from_array( p ) );
			}
			else{
				for( int i = 0; i < rs.segments; ++i )
				{
					Matrix4 mat( g_matrix4_identity );
					// clockwise rotation; clockwise points order
					matrix4_pivoted_rotate_by_axisangle( mat, normal, -c_2pi / rs.segments * ( i + ( rs.offsetStartAngle? 0.5 : 0 ) ), cent );
					winding.push_back( matrix4_transformed_point( mat, rot ) );
				}
			}

			const double edgespeed0 = rs.speedUse ? rs.speed : ( rs.distanceSet? rs.distance : radius ) / rs.time;
			double edgespeed = edgespeed0;

			for( size_t i = 0; i < winding.size(); ++i )
			{
				const DoubleVector3 rot1 = winding[ i ];
				const DoubleVector3 rot2 = winding[( i + 1 ) % winding.size()];
				const DoubleVector3 cent_ = cent - normal * thickness;
				const DoubleVector3 rot1_ = rot1 - normal * thickness;
				const DoubleVector3 rot2_ = rot2 - normal * thickness;


				NodeSmartReference door( GlobalEntityCreator().createEntity( doorClass ) );

				NodeSmartReference brush( GlobalBrushCreator().createBrush() );
				AABB bounds;

				if( !rs.slopedSegments ){
					AddFaceWithTexture( brush, cent.data(), rot1.data(), rot2.data(), rs.textureMain.get(), false );
					AddFaceWithTexture( brush, cent_.data(), rot2_.data(), rot1_.data(), rs.textureMain.get(), false );
					AddFaceWithTexture( brush, cent.data(), cent_.data(), rot1_.data(), rs.textureTrim.get(), false );
					AddFaceWithTexture( brush, cent_.data(), cent.data(), rot2.data(), rs.textureTrim.get(), false );
					AddFaceWithTexture( brush, rot2.data(), rot1.data(), rot1_.data(), rs.textureTrim.get(), false );

					for( const auto& p : { cent, cent_, rot1, rot1_, rot2, rot2_ } )
						aabb_extend_by_point_safe( bounds, p );

					if( rs.innerType == ApertureDoorRS::Inner::segmented ){
						const double depth1 = std::clamp( rs.innerDepth1, 0.0, thickness / 2 - 1 );
						const double depth2 = std::clamp( rs.innerDepth2, 0.0, thickness / 2 - 1 );

						size_t jend = 1;
						for( ; jend < winding.size(); ++jend ) //find out jend beforehand for depth step calc
						{
							const DoubleVector3 r1 = winding[( i + jend + 0 ) % winding.size()];
							if( plane3_distance_to_point( plane3_for_points( cent, cent_, rot1_ ), r1 ) > -1 )
								break;
						}

						for( size_t j = 1; j < jend; ++j )
						{
							const DoubleVector3 extradepth = normal * ( depth1 + ( depth2 - depth1 ) / std::max( size_t( 1 ), jend - 2 ) * ( j - 1 ) );
							const DoubleVector3 c = cent - extradepth;
							const DoubleVector3 r1 = winding[( i + j + 0 ) % winding.size()] - extradepth;
							const DoubleVector3 r2 = winding[( i + j + 1 ) % winding.size()] - extradepth;

							const DoubleVector3 c_ = cent_ + extradepth;
							const DoubleVector3 r1_ = r1 - normal * thickness + extradepth * 2;
							const DoubleVector3 r2_ = r2 - normal * thickness + extradepth * 2;

							for( const auto& p : { c, c_, r1, r1_, r2, r2_ } )
								aabb_extend_by_point_safe( bounds, p );

							NodeSmartReference b( GlobalBrushCreator().createBrush() );

							AddFaceWithTexture( b, c.data(), r1.data(), r2.data(), rs.innerTextureMain.get(), false );
							AddFaceWithTexture( b, c_.data(), r2_.data(), r1_.data(), rs.innerTextureMain.get(), false );
							AddFaceWithTexture( b, c.data(), c_.data(), r1_.data(), rs.innerTextureTrim.get(), false );
							AddFaceWithTexture( b, c_.data(), c.data(), r2.data(), rs.innerTextureTrim.get(), false );
							AddFaceWithTexture( b, r2.data(), r1.data(), r1_.data(), rs.innerTextureTrim.get(), false );

							Node_getTraversable( door )->insert( b );
						}
					}
					else if( rs.innerType == ApertureDoorRS::Inner::sloped ){
						NodeSmartReference b( GlobalBrushCreator().createBrush() );
						AddFaceWithTexture( b, cent.data(), cent_.data(), rot2.data(), rs.innerTextureTrim.get(), false );
						AddFaceWithTexture( b, cent.data(), cent_.data(), rot1_.data(), rs.innerTextureTrim.get(), false );
						double mindot = 2;
						DoubleVector3 r( 0 ); // suppress -Wmaybe-uninitialized
						for( size_t j = 1; j < winding.size(); ++j )
						{
							const DoubleVector3 r1 = winding[( i + j + 0 ) % winding.size()];
							const DoubleVector3 r2 = winding[( i + j + 1 ) % winding.size()];
							const DoubleVector3 r2_ = r2 + normal * 16;

							if( plane3_distance_to_point( plane3_for_points( cent, cent_, rot1_ ), r1 ) > -1 )
								break;

							AddFaceWithTexture( b, r1.data(), r2.data(), r2_.data(), rs.innerTextureTrim.get(), false );

							if( double dot = std::fabs( vector3_dot( vector3_normalised( rot2 - cent ), vector3_normalised( r2 - cent ) ) ); dot < mindot ){
								mindot = dot;
								r = r2;
							}
						}
						const double depth1 = std::clamp( rs.innerDepth1, 0.0, thickness / 2 - 1 );
						const double depth2 = std::clamp( rs.innerDepth2, 0.0, thickness / 2 - 1 );
						AddFaceWithTexture( b, ( cent - normal * depth1 ).data(),
						                       ( rot2 - normal * depth1 ).data(),
						                       ( r - normal * depth2 ).data(), rs.innerTextureMain.get(), false );
						AddFaceWithTexture( b, ( rot2_ + normal * depth1 ).data(),
						                       ( cent_ + normal * depth1 ).data(),
						                       ( r - normal * ( thickness - depth2 ) ).data(), rs.innerTextureMain.get(), false );
						Node_getTraversable( door )->insert( b );
						bounds = AABB(); //invalidate to find out via radiant means
					}
				}
				else if( rs.slopedSegments ){ // bounds via radiant means
					const DoubleVector3 r1 = rot1 + normal * rs.slopedDepth1;
					const DoubleVector3 r2 = rot1 + vector3_cross( normal, cent - rot1 ) + normal * rs.slopedDepth2;
					const DoubleVector3 r4 = cent + ( cent - rot1 );
					const DoubleVector3 r3 = r4 + vector3_cross( normal, cent - rot1 );
					const DoubleVector3 r1_ = rot1_ - normal * rs.slopedDepth1;
					const DoubleVector3 r2_ = rot1_ + vector3_cross( normal, cent - rot1 ) - normal * rs.slopedDepth2;
					const DoubleVector3 r4_ = cent_ + ( cent - rot1 );
					const DoubleVector3 r3_ = r4_ + vector3_cross( normal, cent - rot1 );

					AddFaceWithTexture( brush, r1.data(), r2.data(), r3.data(), rs.textureMain.get(), false );
					AddFaceWithTexture( brush, r2_.data(), r1_.data(), r3_.data(), rs.textureMain.get(), false );
					AddFaceWithTexture( brush, r1.data(), r4.data(), r4_.data(), rs.textureTrim.get(), false );

					if( !rs.slopedSegmentsRoundize ){
						AddFaceWithTexture( brush, r2.data(), r1.data(), r1_.data(), rs.textureTrim.get(), false );
						AddFaceWithTexture( brush, r3.data(), r2.data(), r2_.data(), rs.textureTrim.get(), false );
						AddFaceWithTexture( brush, r4.data(), r3.data(), r3_.data(), rs.textureTrim.get(), false );
					}
					else{ // optional round outline
						for( size_t j = 0; j < winding.size(); ++j )
						{
							const DoubleVector3 r1 = winding[( i + j + 0 ) % winding.size()];
							const DoubleVector3 r2 = winding[( i + j + 1 ) % winding.size()];
							const DoubleVector3 r2_ = r2 + normal * 16;

							if( j != 0 && plane3_distance_to_point( plane3_for_points( cent, cent_, rot1_ ), r1 ) > -1 )
								break;

							AddFaceWithTexture( brush, r1.data(), r2.data(), r2_.data(), rs.textureTrim.get(), false );
						}
					}
				}

				if( rs.silenceType == ApertureDoorRS::Silence::brush ){
					const DoubleVector3 a = cent + rs.silenceBrushesOffset;
					const DoubleVector3 b = a + DoubleVector3( 0, 64, 0 );
					const DoubleVector3 c = a + DoubleVector3( 64, 64, 0 );
					const DoubleVector3 d = a + DoubleVector3( 64, 0, 0 );
					const DoubleVector3 a_ = a - DoubleVector3( 0, 0, 64 );
					const DoubleVector3 b_ = b - DoubleVector3( 0, 0, 64 );
					const DoubleVector3 c_ = c - DoubleVector3( 0, 0, 64 );
					const DoubleVector3 d_ = d - DoubleVector3( 0, 0, 64 );

					NodeSmartReference brush( GlobalBrushCreator().createBrush() );
					AddFaceWithTexture( brush, a.data(), b.data(), c.data(), "textures/common/caulk", false );
					AddFaceWithTexture( brush, c_.data(), b_.data(), a_.data(), "textures/common/caulk", false );
					AddFaceWithTexture( brush, b.data(), a.data(), a_.data(), "textures/common/caulk", false );
					AddFaceWithTexture( brush, c.data(), b.data(), b_.data(), "textures/common/caulk", false );
					AddFaceWithTexture( brush, d.data(), c.data(), c_.data(), "textures/common/caulk", false );
					AddFaceWithTexture( brush, a.data(), d.data(), d_.data(), "textures/common/caulk", false );

					Node_getTraversable( door )->insert( brush );

					if( aabb_valid( bounds ) ) // add to bounds, if we construct bounds here
						for( const auto& p : { a, a_, b, b_, c, c_, d, d_ } )
							aabb_extend_by_point_safe( bounds, p );
				}

				DoubleVector3 dir;
				{
					Matrix4 mat( g_matrix4_identity );
					matrix4_rotate_by_axisangle( mat, normal, degrees_to_radians( rs.openAngle ) );
					// "equal orthogonal speed of edges"
					// dir = vector3_normalised( matrix4_transformed_direction( mat, vector3_normalised( rot2 - cent ) - vector3_normalised( rot1 - cent ) ) );
					// winding shape dependent speeds
					dir = vector3_normalised( matrix4_transformed_direction( mat, rot2 - rot1 ) );

					DoubleVector3 angles( -radians_to_degrees( asin( dir[2] ) ), //PITCH
					                       radians_to_degrees( atan2( dir[1], dir[0] ) ), //YAW
					                       0 );
					vector3_snap_to_zero( angles, 0.0001 ); // snap to avoid scientific notation

					char value[64];
					sprintf( value, "%g %g %g", angles[0], angles[1], angles[2] );
					Node_getEntity( door )->setKeyValue( "angles", value );
				}
				Node_getEntity( door )->setKeyValue( "team", teamname );
				if( rs.health )
					Node_getEntity( door )->setKeyValue( "health", "1" );
				Node_getEntity( door )->setKeyValue( "wait", float2string( rs.wait ) );

				Node_getTraversable( door )->insert( brush );
				Node_getTraversable( GlobalSceneGraph().root() )->insert( door );

				if( !aabb_valid( bounds ) ){
					scene::Path path( makeReference( GlobalSceneGraph().root() ) );
					path.push( makeReference( door.get() ) );
					bounds = GlobalSceneGraph().find( path )->worldAABB();
				}
				{
					// calculate speeds to work with arbitrary windings as input, shape dependent expansion
					// is it possible to calculate directly w/o global 'edgespeed' value?
					// speed.len = edgespeed.len / speed.dot(edgespeed)
					// edgespeed2.len = speed.len * speed.dot(edgespeed2)
					const double
					speed = edgespeed / vector3_dot( vector3_normalised( rot2 - rot1 ), vector3_cross( vector3_normalised( rot1 - cent ), normal ) );
					edgespeed = speed * vector3_dot( vector3_normalised( rot2 - rot1 ), vector3_cross( vector3_normalised( rot2 - cent ), normal ) );
					// two versions for "equal orthogonal speed of edges" (also arbitrary windings)
					// const double speed = rs.speed / ( vector3_length( vector3_normalised( rot2 - cent ) + vector3_normalised( rot1 - cent ) ) / 2 );
					// const double speed = rs.speed / sin( ( c_pi - acos( vector3_dot( vector3_normalised( rot2 - cent ), vector3_normalised( rot1 - cent ) ) ) ) / 2 );
					Node_getEntity( door )->setKeyValue( "speed", float2string( speed ) );

					const DoubleVector3 abs_dir( std::fabs( dir[0] ), std::fabs( dir[1] ), std::fabs( dir[2] ) );
					const auto fullDistance = vector3_dot( abs_dir, DoubleVector3( 2 ) + bounds.extents * 2 ); //there is +2 to bounds in engine somewhere 🧩
//					distance = DotProduct( abs_movedir, size ) - lip;   // game code
//					VectorMA( ent->pos1, distance, ent->movedir, ent->pos2 );
					// note: radius is too much for 3/4/5 segments, might want to limit value
					const float lip = fullDistance - ( rs.distanceSet? rs.distance : radius ) * ( edgespeed0 != 0 ? speed / edgespeed0 : 0 ); // 0 speed and distance inputs are allowed, handle
					Node_getEntity( door )->setKeyValue( "lip", float2string( lip ) );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

void MakeBevel( vec3_t vMin, vec3_t vMax ){
	NodeSmartReference patch( GlobalPatchCreator().createPatch() );
	GlobalPatchCreator().Patch_resize( patch, 3, 3 );
	GlobalPatchCreator().Patch_setShader( patch, "textures/common/caulk" );
	PatchControlMatrix matrix = GlobalPatchCreator().Patch_getControlPoints( patch );
	vec3_t x_3, y_3, z_3;
	x_3[0] = vMin[0];   x_3[1] = vMin[0];                   x_3[2] = vMax[0];
	y_3[0] = vMin[1];   y_3[1] = vMax[1];                   y_3[2] = vMax[1];
	z_3[0] = vMin[2];   z_3[1] = ( vMax[2] + vMin[2] ) / 2; z_3[2] = vMax[2];
	/*
	   x_3[0] = 0;		x_3[1] = 0;		x_3[2] = 64;
	   y_3[0] = 0;		y_3[1] = 64;	y_3[2] = 64;
	   z_3[0] = 0;		z_3[1] = 32;	z_3[2] = 64;*/
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			PatchControl& p = matrix( i, j );
			p.m_vertex[0] = x_3[i];
			p.m_vertex[1] = y_3[i];
			p.m_vertex[2] = z_3[j];
		}
	}
	//does invert the matrix, else the patch face is on wrong side.
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 1; ++j )
		{
			PatchControl& p = matrix( i, 2 - j );
			PatchControl& q = matrix( i, j );
			std::swap( p.m_vertex, q.m_vertex );
			//std::swap( p.m_texcoord, q.m_texcoord );
		}
	}
	GlobalPatchCreator().Patch_controlPointsChanged( patch );
	//TODO - the patch has textures weird, patchmanip.h has all function it needs.. lots of duplicate code..
	//NaturalTexture( patch );
	Node_getTraversable( GlobalRadiant().getMapWorldEntity() )->insert( patch );
}

void BuildCornerStairs( vec3_t vMin, vec3_t vMax, int nSteps, const char* mainTexture, const char* riserTex ){
	auto *topPoints = new vec3_t[nSteps + 1];
	auto *botPoints = new vec3_t[nSteps + 1];

	//bool bFacesUse[6] = {true, true, false, true, false, false};

	vec3_t centre;
	VectorCopy( vMin, centre );
	centre[0] = vMax[0];

	int height = (int)( vMax[2] - vMin[2] ) / nSteps;

	vec3_t vTop, vBot;
	VectorCopy( vMax, vTop );
	VectorCopy( vMin, vBot );
	vTop[2] = vMin[2] + height;

	int i;
	for ( i = 0; i <= nSteps; ++i )
	{
		VectorCopy( centre, topPoints[i] );
		VectorCopy( centre, botPoints[i] );

		topPoints[i][2] = vMax[2];
		botPoints[i][2] = vMin[2];

		topPoints[i][0] -= 10 * sinf( Q_PI * i / ( 2 * nSteps ) );
		topPoints[i][1] += 10 * cosf( Q_PI * i / ( 2 * nSteps ) );

		botPoints[i][0] = topPoints[i][0];
		botPoints[i][1] = topPoints[i][1];
	}

	vec3_t tp[3];
	for ( int j = 0; j < 3; ++j )
		VectorCopy( topPoints[j], tp[j] );

	for ( i = 0; i < nSteps; ++i )
	{
		NodeSmartReference brush( GlobalBrushCreator().createBrush() );
		vec3_t v1, v2, v3, v5, v6, v7;
		VectorCopy( vBot, v1 );
		VectorCopy( vBot, v2 );
		VectorCopy( vBot, v3 );
		VectorCopy( vTop, v5 );
		VectorCopy( vTop, v6 );
		VectorCopy( vTop, v7 );

		v2[0] = vTop[0];
		v3[1] = vTop[1];

		v6[0] = vBot[0];
		v7[1] = vBot[1];

		AddFaceWithTexture( brush, v1, v2, v3, "textures/common/caulk", false );
		AddFaceWithTexture( brush, v1, v3, v6, "textures/common/caulk", false );
		AddFaceWithTexture( brush, v5, v6, v3, "textures/common/caulk", false );

		for ( int j = 0; j < 3; ++j )
			tp[j][2] = vTop[2];

		AddFaceWithTexture( brush, tp[2], tp[1], tp[0], mainTexture, false );

		AddFaceWithTexture( brush, centre, botPoints[i + 1], topPoints[i + 1], "textures/common/caulk", false );
		AddFaceWithTexture( brush, centre, topPoints[i], botPoints[i], riserTex, false );

		Node_getTraversable( GlobalRadiant().getMapWorldEntity() )->insert( brush );

		vTop[2] += height;
		vBot[2] += height;
	}

	delete[] topPoints;
	delete[] botPoints;

	vMin[2] += height;
	vMax[2] += height;
	MakeBevel( vMin, vMax );
}

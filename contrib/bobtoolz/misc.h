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

#pragma once

#include "mathlib.h"
#include <vector>
#include "string/string.h"
#include "iscenegraph.h"

#define MAX_ROUND_ERROR 0.05

vec_t Min( vec_t a, vec_t b );

// reads current texture into global, returns pointer to it
const char* GetCurrentTexture();

class _QERFaceData;
void FillDefaultTexture( _QERFaceData* faceData, const vec3_accu_t va, const vec3_accu_t vb, const vec3_accu_t vc, const char* texture );
inline void FillDefaultTexture( _QERFaceData* faceData, const vec3_t va, const vec3_t vb, const vec3_t vc, const char* texture ){
	FillDefaultTexture( faceData, vec3_accu_t{ va[0], va[1], va[2] }, vec3_accu_t{ vb[0], vb[1], vb[2] }, vec3_accu_t{ vc[0], vc[1], vc[2] }, texture );
}

void BuildMiniPrt( const std::vector<CopiedString>& exclusionList );

void MoveBlock( int dir, vec3_t min, vec3_t max, float dist );
void SetInitialStairPos( int dir, vec3_t min, vec3_t max, float width );

const scene::Path* FindEntityFromTarget( const char* target );
const scene::Path* FindEntityFromTargetname( const char* targetname );

char* UnixToDosPath( char* path );

char* GetFilename( char* buffer, const char* filename );
char* GetGameFilename( char* buffer, const char* filename );

float Determinant3x3( float a1, float a2, float a3,
                      float b1, float b2, float b3,
                      float c1, float c2, float c3 );

bool GetEntityCentre( const char* entityKey, bool keyIsTarget, vec3_t centre );
void MakeNormal( const vec_t* va, const vec_t* vb, const vec_t* vc, vec_t* out );

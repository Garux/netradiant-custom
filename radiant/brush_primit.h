/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined( INCLUDED_BRUSH_PRIMIT_H )
#define INCLUDED_BRUSH_PRIMIT_H

#include "math/vector.h"
#include "itexdef.h"
#include "debugging/debugging.h"
// Timo
// new brush primitive texdef
struct brushprimit_texdef_t
{
	brushprimit_texdef_t(){
		coords[0][0] = 2.0f;
		coords[0][1] = 0.f;
		coords[0][2] = 0.f;
		coords[1][0] = 0.f;
		coords[1][1] = 2.0f;
		coords[1][2] = 0.f;
	}
	void removeScale( std::size_t width, std::size_t height ){ /* values in texture size scale for certain operations */
#if 1
		coords[0][0] *= width;
		coords[0][1] *= width;
		coords[0][2] *= width;
		coords[1][0] *= height;
		coords[1][1] *= height;
		coords[1][2] *= height;
#endif
	}
	void addScale( std::size_t width, std::size_t height ){ /* addScaled in .map; offsets in range -1..1; texture size irrelevant */
#if 1
		ASSERT_MESSAGE( width > 0, "shader-width is 0" );
		ASSERT_MESSAGE( height > 0, "shader-height is 0" );
		coords[0][0] /= width;
		coords[0][1] /= width;
		coords[0][2] /= width;
		coords[1][0] /= height;
		coords[1][1] /= height;
		coords[1][2] /= height;
#endif
	}
	float coords[2][3];
};

class TextureProjection
{
public:
texdef_t m_texdef;
brushprimit_texdef_t m_brushprimit_texdef;
Vector3 m_basis_s;
Vector3 m_basis_t;

TextureProjection(){
}
TextureProjection(
	const texdef_t& texdef,
	const brushprimit_texdef_t& brushprimit_texdef,
	const Vector3& basis_s,
	const Vector3& basis_t
	) :
	m_texdef( texdef ),
	m_brushprimit_texdef( brushprimit_texdef ),
	m_basis_s( basis_s ),
	m_basis_t( basis_t ){
}
};

float Texdef_getDefaultTextureScale();

class texdef_t;
struct Winding;
class Matrix4;
class Plane3;

void Normal_GetTransform( const Vector3& normal, Matrix4& transform );

void TexDef_Construct_Default( TextureProjection& projection );

void Texdef_Assign( TextureProjection& projection, const TextureProjection& other, bool setBasis = true );
void Texdef_Assign( TextureProjection& projection, const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation );
void Texdef_Shift( TextureProjection& projection, float s, float t );
void Texdef_Scale( TextureProjection& projection, float s, float t );
void Texdef_Rotate( TextureProjection& projection, float angle );
void Texdef_ProjectTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const texdef_t& texdef, const Vector3* direction );
void Texdef_ProjectTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, TextureProjection other_proj, const Vector3& other_normal );
void Texdef_FitTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Vector3& normal, const Winding& w, float s_repeat, float t_repeat, bool only_dimension );
void Texdef_Construct_local2tex( const TextureProjection& projection, std::size_t width, std::size_t height, const Vector3& normal, Matrix4& local2tex );
void Texdef_Construct_local2tex4projection( const texdef_t& texdef, std::size_t width, std::size_t height, const Vector3& normal, const Vector3* direction, Matrix4& local2tex );
void Texdef_Construct_local2tex_from_ST( const DoubleVector3 points[3], const DoubleVector3 st[3], Matrix4& local2tex );
void Texdef_EmitTextureCoordinates( const TextureProjection& projection, std::size_t width, std::size_t height, Winding& w, const Vector3& normal, const Matrix4& localToWorld );

void ShiftScaleRotate_fromFace( texdef_t& shiftScaleRotate, const TextureProjection& projection );
void ShiftScaleRotate_toFace( const texdef_t& shiftScaleRotate, TextureProjection& projection );

void Texdef_transformLocked( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& transform, const Vector3& invariant = g_vector3_identity );
void Texdef_transform( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& transform, const Vector3& invariant = g_vector3_identity );
void Texdef_normalise( TextureProjection& projection, float width, float height );

enum TexdefTypeId
{
	TEXDEFTYPEID_QUAKE,
	TEXDEFTYPEID_BRUSHPRIMITIVES,
	TEXDEFTYPEID_VALVE,
};

struct bp_globals_t
{
	TexdefTypeId m_texdefTypeId;
};

extern bp_globals_t g_bp_globals;
extern float g_texdef_default_scale;

void Texdef_Convert( TexdefTypeId in, TexdefTypeId out, const Plane3& plane, TextureProjection& projection, std::size_t width, std::size_t height );
void Texdef_from_ST( TextureProjection& projection, const DoubleVector3 points[3], const DoubleVector3 st[3], std::size_t width, std::size_t height );

//++timo replace everywhere texX by texS etc. ( ----> and in q3map !)
// NOTE : ComputeAxisBase here and in q3map code must always BE THE SAME !
// WARNING : special case behaviour of atan2(y,x) <-> atan(y/x) might not be the same everywhere when x == 0
// rotation by (0,RotY,RotZ) assigns X to normal
template <typename Element, typename OtherElement>
inline void ComputeAxisBase( const BasicVector3<Element>& normal, BasicVector3<OtherElement>& texS, BasicVector3<OtherElement>& texT ){
#if 1
	const BasicVector3<Element> up( 0, 0, 1 );
	const BasicVector3<Element> down( 0, 0, -1 );

	if ( vector3_equal_epsilon( normal, up, Element(1e-6) ) ) {
		texS = BasicVector3<OtherElement>( 0, 1, 0 );
		texT = BasicVector3<OtherElement>( 1, 0, 0 );
	}
	else if ( vector3_equal_epsilon( normal, down, Element(1e-6) ) ) {
		texS = BasicVector3<OtherElement>( 0, 1, 0 );
		texT = BasicVector3<OtherElement>( -1, 0, 0 );
	}
	else
	{
		texS = vector3_normalised( vector3_cross( normal, up ) );
		texT = vector3_normalised( vector3_cross( normal, texS ) );
		vector3_negate( texS );
	}

#else
	float RotY,RotZ;
	// do some cleaning
	/*
	   if (fabs(normal[0])<1e-6)
	      normal[0]=0.0f;
	   if (fabs(normal[1])<1e-6)
	      normal[1]=0.0f;
	   if (fabs(normal[2])<1e-6)
	      normal[2]=0.0f;
	 */
	RotY = -atan2( normal[2],sqrt( normal[1] * normal[1] + normal[0] * normal[0] ) );
	RotZ = atan2( normal[1],normal[0] );
	// rotate (0,1,0) and (0,0,1) to compute texS and texT
	texS[0] = -sin( RotZ );
	texS[1] = cos( RotZ );
	texS[2] = 0;
	// the texT vector is along -Z ( T texture coorinates axis )
	texT[0] = -sin( RotY ) * cos( RotZ );
	texT[1] = -sin( RotY ) * sin( RotZ );
	texT[2] = -cos( RotY );
#endif
}

#endif

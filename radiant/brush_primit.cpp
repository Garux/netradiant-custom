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

#include "brush_primit.h"

#include "debugging/debugging.h"

#include "itexdef.h"
#include "itextures.h"

#include <algorithm>

#include "stringio.h"
#include "texturelib.h"
#include "math/matrix.h"
#include "math/plane.h"
#include "math/aabb.h"

#include "winding.h"
#include "preferences.h"


bp_globals_t g_bp_globals;
float g_texdef_default_scale;

/*!
   \brief Construct a transform from XYZ space to ST space (3d to 2d).
   This will be one of three axis-aligned spaces, depending on the surface normal.
   NOTE: could also be done by swapping values.
 */
void Normal_GetTransform( const Vector3& normal, Matrix4& transform ){
	switch ( projectionaxis_for_normal( normal ) )
	{
	case eProjectionAxisZ:
		transform[0]  =  1;
		transform[1]  =  0;
		transform[2]  =  0;

		transform[4]  =  0;
		transform[5]  =  1;
		transform[6]  =  0;

		transform[8]  =  0;
		transform[9]  =  0;
		transform[10] =  1;
		break;
	case eProjectionAxisY:
		transform[0]  =  1;
		transform[1]  =  0;
		transform[2]  =  0;

		transform[4]  =  0;
		transform[5]  =  0;
		transform[6]  = -1;

		transform[8]  =  0;
		transform[9]  =  1;
		transform[10] =  0;
		break;
	case eProjectionAxisX:
		transform[0]  =  0;
		transform[1]  =  0;
		transform[2]  =  1;

		transform[4]  =  1;
		transform[5]  =  0;
		transform[6]  =  0;

		transform[8]  =  0;
		transform[9]  =  1;
		transform[10] =  0;
		break;
	}
	transform[3] = transform[7] = transform[11] = transform[12] = transform[13] = transform[14] = 0;
	transform[15] = 1;
}

/*!
   \brief Construct a transform in ST space from the texdef.
   Transforms constructed from quake's texdef format are (-shift)*(1/scale)*(-rotate) with x translation sign flipped.
   This would really make more sense if it was inverseof(shift*rotate*scale).. oh well.
 */
inline void Texdef_toTransform( const texdef_t& texdef, float width, float height, Matrix4& transform ){
	double inverse_scale[2];

	// transform to texdef shift/scale/rotate
	inverse_scale[0] = 1 / ( texdef.scale[0] * width );
	inverse_scale[1] = 1 / ( texdef.scale[1] * -height );
	transform[12] = texdef.shift[0] / width;
	transform[13] = -texdef.shift[1] / -height;
	double c = cos( degrees_to_radians( -texdef.rotate ) );
	double s = sin( degrees_to_radians( -texdef.rotate ) );
	transform[0] = static_cast<float>( c * inverse_scale[0] );
	transform[1] = static_cast<float>( s * inverse_scale[1] );
	transform[4] = static_cast<float>( -s * inverse_scale[0] );
	transform[5] = static_cast<float>( c * inverse_scale[1] );
	transform[2] = transform[3] = transform[6] = transform[7] = transform[8] = transform[9] = transform[11] = transform[14] = 0;
	transform[10] = transform[15] = 1;
}

inline void Valve220Texdef_toTransform( const texdef_t& texdef, float width, float height, Matrix4& transform ){
	transform = g_matrix4_identity;
	transform[12] = texdef.shift[0] / width;
	transform[13] = -texdef.shift[1] / -height;
	transform[0] = 1 / ( texdef.scale[0] * width );
	transform[5] = 1 / ( texdef.scale[1] * -height );
}

inline void BPTexdef_toTransform( const brushprimit_texdef_t& bp_texdef, Matrix4& transform ){
	transform = g_matrix4_identity;
	transform.xx() = bp_texdef.coords[0][0];
	transform.yx() = bp_texdef.coords[0][1];
	transform.tx() = bp_texdef.coords[0][2];
	transform.xy() = bp_texdef.coords[1][0];
	transform.yy() = bp_texdef.coords[1][1];
	transform.ty() = bp_texdef.coords[1][2];
}

inline void Texdef_toTransform( const TextureProjection& projection, float width, float height, Matrix4& transform ){
	switch ( g_bp_globals.m_texdefTypeId )
	{
	case TEXDEFTYPEID_BRUSHPRIMITIVES:
		BPTexdef_toTransform( projection.m_brushprimit_texdef, transform );
		break;
	case TEXDEFTYPEID_VALVE:
		Valve220Texdef_toTransform( projection.m_texdef, width, height, transform );
		break;
	default: //case TEXDEFTYPEID_QUAKE:
		Texdef_toTransform( projection.m_texdef, width, height, transform );
		break;
	}
}

// handles degenerate cases, just in case library atan2 doesn't
inline double arctangent_yx( double y, double x ){
	if ( fabs( x ) > 1.0E-6 ) {
		return atan2( y, x );
	}
	else if ( y > 0 ) {
		return c_half_pi;
	}
	else
	{
		return -c_half_pi;
	}
}

inline void Texdef_fromTransform( texdef_t& texdef, float width, float height, const Matrix4& transform ){
	texdef.scale[0] = static_cast<float>( ( 1.0 / vector2_length( Vector2( transform[0], transform[4] ) ) ) / width );
	texdef.scale[1] = static_cast<float>( ( 1.0 / vector2_length( Vector2( transform[1], transform[5] ) ) ) / height );

	texdef.rotate = static_cast<float>( -radians_to_degrees( arctangent_yx( -transform[4], transform[0] ) ) );

	if ( texdef.rotate == -180.0f ) {
		texdef.rotate = 180.0f;
	}

	texdef.shift[0] = transform[12] * width;
	texdef.shift[1] = transform[13] * height;

	// If the 2d cross-product of the x and y axes is positive, one of the axes has a negative scale.
	if ( vector2_cross( Vector2( transform[0], transform[4] ), Vector2( transform[1], transform[5] ) ) > 0 ) {
		if ( texdef.rotate >= 180.0f ) {
			texdef.rotate -= 180.0f;
			texdef.scale[0] = -texdef.scale[0];
		}
		else
		{
			texdef.scale[1] = -texdef.scale[1];
		}
	}
	//globalOutputStream() << "fromTransform: " << texdef.shift[0] << " " << texdef.shift[1] << " " << texdef.scale[0] << " " << texdef.scale[1] << " " << texdef.rotate << "\n";
}

inline void BPTexdef_fromTransform( brushprimit_texdef_t& bp_texdef, const Matrix4& transform ){
	bp_texdef.coords[0][0] = transform.xx();
	bp_texdef.coords[0][1] = transform.yx();
	bp_texdef.coords[0][2] = transform.tx();
	bp_texdef.coords[1][0] = transform.xy();
	bp_texdef.coords[1][1] = transform.yy();
	bp_texdef.coords[1][2] = transform.ty();
	//globalOutputStream() << bp_texdef.coords[0][0] << " " << bp_texdef.coords[0][1] << " " << bp_texdef.coords[0][2] << "\n";
}

inline void Texdef_fromTransform( TextureProjection& projection, float width, float height, const Matrix4& transform ){
	ASSERT_MESSAGE( ( transform[0] != 0 || transform[4] != 0 )
					&& ( transform[1] != 0 || transform[5] != 0 ), "invalid texture matrix" );

	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_fromTransform( projection.m_brushprimit_texdef, transform );
	}
	else
	{
		Texdef_fromTransform( projection.m_texdef, width, height, transform );
	}
}

inline void Texdef_normalise( texdef_t& texdef, float width, float height ){
	// it may be useful to also normalise the rotation here, if this function is used elsewhere.
	texdef.shift[0] = float_mod( texdef.shift[0], width );
	texdef.shift[1] = float_mod( texdef.shift[1], height );
	//globalOutputStream() << "normalise: " << texdef.shift[0] << " " << texdef.shift[1] << " " << texdef.scale[0] << " " << texdef.scale[1] << " " << texdef.rotate << "\n";
}
/// this is supposed to work with brushprimit_texdef_t.removeScale()'d
inline void BPTexdef_normalise( brushprimit_texdef_t& bp_texdef, float width, float height ){
	bp_texdef.coords[0][2] = float_mod( bp_texdef.coords[0][2], width );
	bp_texdef.coords[1][2] = float_mod( bp_texdef.coords[1][2], height );
}

/// \brief Normalise \p projection for a given texture \p width and \p height.
///
/// All texture-projection translation (shift) values are congruent modulo the dimensions of the texture.
/// This function normalises shift values to the smallest positive congruent values.
void Texdef_normalise( TextureProjection& projection, float width, float height ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_normalise( projection.m_brushprimit_texdef, width, height );
	}
	else
	{
		Texdef_normalise( projection.m_texdef, width, height );
	}
}

//++timo replace everywhere texX by texS etc. ( ----> and in q3map !)
// NOTE : ComputeAxisBase here and in q3map code must always BE THE SAME !
// WARNING : special case behaviour of atan2(y,x) <-> atan(y/x) might not be the same everywhere when x == 0
// rotation by (0,RotY,RotZ) assigns X to normal
template <typename Element, typename OtherElement>
void ComputeAxisBase( const BasicVector3<Element>& normal, BasicVector3<OtherElement>& texS, BasicVector3<OtherElement>& texT ){
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

inline void DebugAxisBase( const Vector3& normal ){
	Vector3 x, y;
	ComputeAxisBase( normal, x, y );
	globalOutputStream() << "BP debug: " << x << y << normal << "\n";
}

void Texdef_basisForNormal( const TextureProjection& projection, const Vector3& normal, Matrix4& basis ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		basis = g_matrix4_identity;
		ComputeAxisBase( normal, vector4_to_vector3( basis.x() ), vector4_to_vector3( basis.y() ) );
		vector4_to_vector3( basis.z() ) = normal;
		matrix4_transpose( basis );
		//DebugAxisBase( normal );
	}
	else if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
		basis = g_matrix4_identity;
		vector4_to_vector3( basis.x() ) = projection.m_basis_s;
		vector4_to_vector3( basis.y() ) = vector3_negated( projection.m_basis_t );
		vector4_to_vector3( basis.z() ) = vector3_normalised( vector3_cross( vector4_to_vector3( basis.x() ), vector4_to_vector3( basis.y() ) ) );
//		matrix4_multiply_by_matrix4( basis, matrix4_rotation_for_z_degrees( -projection.m_texdef.rotate ) );
		//globalOutputStream() << "debug: " << projection.m_basis_s << projection.m_basis_t << normal << "\n";
		matrix4_transpose( basis );
	}
	else
	{
		Normal_GetTransform( normal, basis );
	}
}

void Texdef_Construct_local2tex( const TextureProjection& projection, std::size_t width, std::size_t height, const Vector3& normal, Matrix4& local2tex ){
	Texdef_toTransform( projection, (float)width, (float)height, local2tex );
	{
		Matrix4 xyz2st;
		Texdef_basisForNormal( projection, normal, xyz2st );
		matrix4_multiply_by_matrix4( local2tex, xyz2st );
	}
}

void Texdef_EmitTextureCoordinates( const TextureProjection& projection, std::size_t width, std::size_t height, Winding& w, const Vector3& normal, const Matrix4& localToWorld ){
	if ( w.numpoints < 3 ) {
		return;
	}
	//globalOutputStream() << "normal: " << normal << "\n";

	Matrix4 local2tex;
	Texdef_toTransform( projection, (float)width, (float)height, local2tex );
	//globalOutputStream() << "texdef: " << static_cast<const Vector3&>(local2tex.x()) << static_cast<const Vector3&>(local2tex.y()) << "\n";

#if 0
	{
		TextureProjection tmp;
		Texdef_fromTransform( tmp, (float)width, (float)height, local2tex );
		Matrix4 tmpTransform;
		Texdef_toTransform( tmp, (float)width, (float)height, tmpTransform );
		ASSERT_MESSAGE( matrix4_equal_epsilon( local2tex, tmpTransform, 0.0001f ), "bleh" );
	}
#endif

	{
		Matrix4 xyz2st;
		// we don't care if it's not normalised...
		Texdef_basisForNormal( projection, matrix4_transformed_direction( localToWorld, normal ), xyz2st );
		//globalOutputStream() << "basis: " << static_cast<const Vector3&>(xyz2st.x()) << static_cast<const Vector3&>(xyz2st.y()) << static_cast<const Vector3&>(xyz2st.z()) << "\n";
		matrix4_multiply_by_matrix4( local2tex, xyz2st );
	}

	Vector3 tangent( vector3_normalised( vector4_to_vector3( matrix4_transposed( local2tex ).x() ) ) );
	Vector3 bitangent( vector3_normalised( vector4_to_vector3( matrix4_transposed( local2tex ).y() ) ) );

	matrix4_multiply_by_matrix4( local2tex, localToWorld );

	for ( Winding::iterator i = w.begin(); i != w.end(); ++i )
	{
		Vector3 texcoord = matrix4_transformed_point( local2tex, ( *i ).vertex );
		( *i ).texcoord[0] = texcoord[0];
		( *i ).texcoord[1] = texcoord[1];

		( *i ).tangent = tangent;
		( *i ).bitangent = bitangent;
	}
}

/*!
   \brief Provides the axis-base of the texture ST space for this normal,
   as they had been transformed to world XYZ space.
 */
void TextureAxisFromNormal( const Vector3& normal, Vector3& s, Vector3& t ){
	switch ( projectionaxis_for_normal( normal ) )
	{
	case eProjectionAxisZ:
		s[0]  =  1;
		s[1]  =  0;
		s[2]  =  0;

		t[0]  =  0;
		t[1]  = -1;
		t[2]  =  0;

		break;
	case eProjectionAxisY:
		s[0]  =  1;
		s[1]  =  0;
		s[2]  =  0;

		t[0]  =  0;
		t[1]  =  0;
		t[2]  = -1;

		break;
	case eProjectionAxisX:
		s[0]  =  0;
		s[1]  =  1;
		s[2]  =  0;

		t[0]  =  0;
		t[1]  =  0;
		t[2]  = -1;

		break;
	}
}

void Texdef_Assign( texdef_t& td, const texdef_t& other ){
	td = other;
}


void Texdef_Assign( texdef_t& td, const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	if( hShift ){
		td.shift[0] = *hShift;
	}
	if( vShift ){
		td.shift[1] = *vShift;
	}
	if( hScale ){
		if( fabs( *hScale ) > 1e-5 ){
			td.scale[0] = *hScale;
		}
		else{
			td.scale[0] = -td.scale[0];
		}
	}
	if( vScale ){
		if( fabs( *vScale ) > 1e-5 ){
			td.scale[1] = *vScale;
		}
		else{
			td.scale[1] = -td.scale[1];
		}
	}
	if( rotation ){
		td.rotate = *rotation;
		//td.rotate = static_cast<float>( float_to_integer( td.rotate * 100.f ) % 36000 ) / 100.f;
		td.rotate = fmod( td.rotate, 360.f );
	}
}

void Texdef_Shift( texdef_t& td, float s, float t ){
	td.shift[0] += s;
	td.shift[1] += t;
}

void Texdef_Scale( texdef_t& td, float s, float t ){
	if( fabs( td.scale[0] + s ) > 1e-5 ){
		td.scale[0] += s;
	}
	else{
		td.scale[0] = -td.scale[0];
	}
	if( fabs( td.scale[1] + t ) > 1e-5 ){
		td.scale[1] += t;
	}
	else{
		td.scale[1] = -td.scale[1];
	}
}

void Texdef_Rotate( texdef_t& td, float angle ){
	td.rotate += angle;
	td.rotate = fmod( td.rotate, 360.f );
}
#if 0
// NOTE: added these from Ritual's Q3Radiant
void ClearBounds( Vector3& mins, Vector3& maxs ){
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

void AddPointToBounds( const Vector3& v, Vector3& mins, Vector3& maxs ){
	int i;
	float val;

	for ( i = 0 ; i < 3 ; i++ )
	{
		val = v[i];
		if ( val < mins[i] ) {
			mins[i] = val;
		}
		if ( val > maxs[i] ) {
			maxs[i] = val;
		}
	}
}
#endif

#if 0
template<typename Element>
inline BasicVector3<Element> vector3_inverse( const BasicVector3<Element>& self ){
	return BasicVector3<Element>(
			   Element( 1.0 / self.x() ),
			   Element( 1.0 / self.y() ),
			   Element( 1.0 / self.z() )
			   );
}
#endif

#if 0
// compute a determinant using Sarrus rule
//++timo "inline" this with a macro
// NOTE : the three vectors are understood as columns of the matrix
inline float SarrusDet( const Vector3& a, const Vector3& b, const Vector3& c ){
	return a[0] * b[1] * c[2] + b[0] * c[1] * a[2] + c[0] * a[1] * b[2]
		   - c[0] * b[1] * a[2] - a[1] * b[0] * c[2] - a[0] * b[2] * c[1];
}

// in many case we know three points A,B,C in two axis base B1 and B2
// and we want the matrix M so that A(B1) = T * A(B2)
// NOTE: 2D homogeneous space stuff
// NOTE: we don't do any check to see if there's a solution or we have a particular case .. need to make sure before calling
// NOTE: the third coord of the A,B,C point is ignored
// NOTE: see the commented out section to fill M and D
//++timo TODO: update the other members to use this when possible
void MatrixForPoints( Vector3 M[3], Vector3 D[2], brushprimit_texdef_t *T ){
//	Vector3 M[3]; // columns of the matrix .. easier that way (the indexing is not standard! it's column-line .. later computations are easier that way)
	float det;
//	Vector3 D[2];
	M[2][0] = 1.0f; M[2][1] = 1.0f; M[2][2] = 1.0f;
#if 0
	// fill the data vectors
	M[0][0] = A2[0]; M[0][1] = B2[0]; M[0][2] = C2[0];
	M[1][0] = A2[1]; M[1][1] = B2[1]; M[1][2] = C2[1];
	M[2][0] = 1.0f; M[2][1] = 1.0f; M[2][2] = 1.0f;
	D[0][0] = A1[0];
	D[0][1] = B1[0];
	D[0][2] = C1[0];
	D[1][0] = A1[1];
	D[1][1] = B1[1];
	D[1][2] = C1[1];
#endif
	// solve
	det = SarrusDet( M[0], M[1], M[2] );
	T->coords[0][0] = SarrusDet( D[0], M[1], M[2] ) / det;
	T->coords[0][1] = SarrusDet( M[0], D[0], M[2] ) / det;
	T->coords[0][2] = SarrusDet( M[0], M[1], D[0] ) / det;
	T->coords[1][0] = SarrusDet( D[1], M[1], M[2] ) / det;
	T->coords[1][1] = SarrusDet( M[0], D[1], M[2] ) / det;
	T->coords[1][2] = SarrusDet( M[0], M[1], D[1] ) / det;
}
#endif

#if 0

#ifdef _DEBUG
//#define DBG_BP
#endif

// texdef conversion
void FaceToBrushPrimitFace( face_t *f ){
	Vector3 texX,texY;
	Vector3 proj;
	// ST of (0,0) (1,0) (0,1)
	float ST[3][5]; // [ point index ] [ xyz ST ]
	//++timo not used as long as brushprimit_texdef and texdef are static
/*	f->brushprimit_texdef.contents=f->texdef.contents;
    f->brushprimit_texdef.flags=f->texdef.flags;
    f->brushprimit_texdef.value=f->texdef.value;
    strcpy(f->brushprimit_texdef.name,f->texdef.name); */
#ifdef DBG_BP
	if ( f->plane.normal[0] == 0.0f && f->plane.normal[1] == 0.0f && f->plane.normal[2] == 0.0f ) {
		globalWarningStream() << "Warning : f->plane.normal is (0,0,0) in FaceToBrushPrimitFace\n";
	}
	// check d_texture
	if ( !f->d_texture ) {
		globalWarningStream() << "Warning : f.d_texture is 0 in FaceToBrushPrimitFace\n";
		return;
	}
#endif
	// compute axis base
	ComputeAxisBase( f->plane.normal,texX,texY );
	// compute projection vector
	VectorCopy( f->plane.normal,proj );
	VectorScale( proj,f->plane.dist,proj );
	// (0,0) in plane axis base is (0,0,0) in world coordinates + projection on the affine plane
	// (1,0) in plane axis base is texX in world coordinates + projection on the affine plane
	// (0,1) in plane axis base is texY in world coordinates + projection on the affine plane
	// use old texture code to compute the ST coords of these points
	VectorCopy( proj,ST[0] );
	EmitTextureCoordinates( ST[0], f->pShader->getTexture(), f );
	VectorCopy( texX,ST[1] );
	VectorAdd( ST[1],proj,ST[1] );
	EmitTextureCoordinates( ST[1], f->pShader->getTexture(), f );
	VectorCopy( texY,ST[2] );
	VectorAdd( ST[2],proj,ST[2] );
	EmitTextureCoordinates( ST[2], f->pShader->getTexture(), f );
	// compute texture matrix
	f->brushprimit_texdef.coords[0][2] = ST[0][3];
	f->brushprimit_texdef.coords[1][2] = ST[0][4];
	f->brushprimit_texdef.coords[0][0] = ST[1][3] - f->brushprimit_texdef.coords[0][2];
	f->brushprimit_texdef.coords[1][0] = ST[1][4] - f->brushprimit_texdef.coords[1][2];
	f->brushprimit_texdef.coords[0][1] = ST[2][3] - f->brushprimit_texdef.coords[0][2];
	f->brushprimit_texdef.coords[1][1] = ST[2][4] - f->brushprimit_texdef.coords[1][2];
}

// compute texture coordinates for the winding points
void EmitBrushPrimitTextureCoordinates( face_t * f, Winding * w ){
	Vector3 texX,texY;
	float x,y;
	// compute axis base
	ComputeAxisBase( f->plane.normal,texX,texY );
	// in case the texcoords matrix is empty, build a default one
	// same behaviour as if scale[0]==0 && scale[1]==0 in old code
	if ( f->brushprimit_texdef.coords[0][0] == 0 && f->brushprimit_texdef.coords[1][0] == 0 && f->brushprimit_texdef.coords[0][1] == 0 && f->brushprimit_texdef.coords[1][1] == 0 ) {
		f->brushprimit_texdef.coords[0][0] = 1.0f;
		f->brushprimit_texdef.coords[1][1] = 1.0f;
		ConvertTexMatWithQTexture( &f->brushprimit_texdef, 0, &f->brushprimit_texdef, f->pShader->getTexture() );
	}
	int i;
	for ( i = 0 ; i < w.numpoints ; i++ )
	{
		x = vector3_dot( w.point_at( i ),texX );
		y = vector3_dot( w.point_at( i ),texY );
#ifdef DBG_BP
		if ( g_bp_globals.bNeedConvert ) {
			// check we compute the same ST as the traditional texture computation used before
			float S = f->brushprimit_texdef.coords[0][0] * x + f->brushprimit_texdef.coords[0][1] * y + f->brushprimit_texdef.coords[0][2];
			float T = f->brushprimit_texdef.coords[1][0] * x + f->brushprimit_texdef.coords[1][1] * y + f->brushprimit_texdef.coords[1][2];
			if ( fabs( S - w.point_at( i )[3] ) > 1e-2 || fabs( T - w.point_at( i )[4] ) > 1e-2 ) {
				if ( fabs( S - w.point_at( i )[3] ) > 1e-4 || fabs( T - w.point_at( i )[4] ) > 1e-4 ) {
					globalWarningStream() << "Warning : precision loss in brush -> brush primitive texture computation\n";
				}
				else{
					globalWarningStream() << "Warning : brush -> brush primitive texture computation bug detected\n";
				}
			}
		}
#endif
		w.point_at( i )[3] = f->brushprimit_texdef.coords[0][0] * x + f->brushprimit_texdef.coords[0][1] * y + f->brushprimit_texdef.coords[0][2];
		w.point_at( i )[4] = f->brushprimit_texdef.coords[1][0] * x + f->brushprimit_texdef.coords[1][1] * y + f->brushprimit_texdef.coords[1][2];
	}
}
#endif

#if 0
typedef float texmat_t[2][3];

void TexMat_Scale( texmat_t texmat, float s, float t ){
	texmat[0][0] *= s;
	texmat[0][1] *= s;
	texmat[0][2] *= s;
	texmat[1][0] *= t;
	texmat[1][1] *= t;
	texmat[1][2] *= t;
}

void TexMat_Assign( texmat_t texmat, const texmat_t other ){
	texmat[0][0] = other[0][0];
	texmat[0][1] = other[0][1];
	texmat[0][2] = other[0][2];
	texmat[1][0] = other[1][0];
	texmat[1][1] = other[1][1];
	texmat[1][2] = other[1][2];
}

void ConvertTexMatWithDimensions( const texmat_t texmat1, std::size_t w1, std::size_t h1,
								  texmat_t texmat2, std::size_t w2, std::size_t h2 ){
	TexMat_Assign( texmat2, texmat1 );
	TexMat_Scale( texmat2, static_cast<float>( w1 ) / static_cast<float>( w2 ), static_cast<float>( h1 ) / static_cast<float>( h2 ) );
}

// convert a texture matrix between two qtexture_t
// if 0 for qtexture_t, basic 2x2 texture is assumed ( straight mapping between s/t coordinates and geometric coordinates )
void ConvertTexMatWithQTexture( const float texMat1[2][3], const qtexture_t *qtex1, float texMat2[2][3], const qtexture_t *qtex2 ){
	ConvertTexMatWithDimensions( texMat1, ( qtex1 ) ? qtex1->width : 2, ( qtex1 ) ? qtex1->height : 2,
								 texMat2, ( qtex2 ) ? qtex2->width : 2, ( qtex2 ) ? qtex2->height : 2 );
}

void ConvertTexMatWithQTexture( const brushprimit_texdef_t *texMat1, const qtexture_t *qtex1, brushprimit_texdef_t *texMat2, const qtexture_t *qtex2 ){
	ConvertTexMatWithQTexture( texMat1->coords, qtex1, texMat2->coords, qtex2 );
}
#endif

// compute a fake shift scale rot representation from the texture matrix
// these shift scale rot values are to be understood in the local axis base
// Note: this code looks similar to Texdef_fromTransform, but the algorithm is slightly different.

void TexMatToFakeTexCoords( const brushprimit_texdef_t& bp_texdef, texdef_t& texdef ){
#if 0
	texdef.scale[0] = static_cast<float>( 1.0 / vector2_length( Vector2( bp_texdef.coords[0][0], bp_texdef.coords[1][0] ) ) );
	texdef.scale[1] = static_cast<float>( 1.0 / vector2_length( Vector2( bp_texdef.coords[0][1], bp_texdef.coords[1][1] ) ) );

	texdef.rotate = -static_cast<float>( radians_to_degrees( arctangent_yx( bp_texdef.coords[1][0], bp_texdef.coords[0][0] ) ) );

	texdef.shift[0] = -bp_texdef.coords[0][2];
	texdef.shift[1] = bp_texdef.coords[1][2];

	// determine whether or not an axis is flipped using a 2d cross-product
	double cross = vector2_cross( Vector2( bp_texdef.coords[0][0], bp_texdef.coords[0][1] ), Vector2( bp_texdef.coords[1][0], bp_texdef.coords[1][1] ) );
	if ( cross < 0 ) {
		// This is a bit of a compromise when using BPs--since we don't know *which* axis was flipped,
		// we pick one (rather arbitrarily) using the following convention: If the X-axis is between
		// 0 and 180, we assume it's the Y-axis that flipped, otherwise we assume it's the X-axis and
		// subtract out 180 degrees to compensate.
		if ( texdef.rotate >= 180.0f ) {
			texdef.rotate -= 180.0f;
			texdef.scale[0] = -texdef.scale[0];
		}
		else
		{
			texdef.scale[1] = -texdef.scale[1];
		}
	}
#else
	texdef.scale[0] = static_cast<float>( 1.0 / vector2_length( Vector2( bp_texdef.coords[0][0], bp_texdef.coords[0][1] ) ) );
	texdef.scale[1] = static_cast<float>( 1.0 / vector2_length( Vector2( bp_texdef.coords[1][0], bp_texdef.coords[1][1] ) ) );
	if( bp_texdef.coords[0][0] < 0 ){
		texdef.scale[0] = -texdef.scale[0];
	}
	if( bp_texdef.coords[1][1] < 0 ){
		texdef.scale[1] = -texdef.scale[1];
	}
#if 1
	texdef.rotate = static_cast<float>( radians_to_degrees( acos( vector2_normalised( Vector2( bp_texdef.coords[0][0], bp_texdef.coords[0][1] ) )[0] ) ) );
	if( bp_texdef.coords[0][1] > 0 ){
		texdef.rotate = -texdef.rotate;
	}
#else
	texdef.rotate = static_cast<float>( radians_to_degrees( arctangent_yx( bp_texdef.coords[0][1], bp_texdef.coords[0][0] ) ) );
#endif
	texdef.shift[0] = -bp_texdef.coords[0][2];
	texdef.shift[1] = bp_texdef.coords[1][2];
#endif
}

// compute back the texture matrix from fake shift scale rot
void FakeTexCoordsToTexMat( const texdef_t& texdef, brushprimit_texdef_t& bp_texdef ){
	double r = degrees_to_radians( -texdef.rotate );
	double c = cos( r );
	double s = sin( r );
	double x = 1.0f / texdef.scale[0];
	double y = 1.0f / texdef.scale[1];
	bp_texdef.coords[0][0] = static_cast<float>( x * c );
	bp_texdef.coords[1][0] = static_cast<float>( x * s );
	bp_texdef.coords[0][1] = static_cast<float>( y * -s );
	bp_texdef.coords[1][1] = static_cast<float>( y * c );
	bp_texdef.coords[0][2] = -texdef.shift[0];
	bp_texdef.coords[1][2] = texdef.shift[1];
//	globalOutputStream() << "[ " << bp_texdef.coords[0][0] << " " << bp_texdef.coords[0][1] << " ][ " << bp_texdef.coords[1][0] << " " << bp_texdef.coords[1][1] << " ]\n";
}

#if 0 // texture locking (brush primit)
// used for texture locking
// will move the texture according to a geometric vector
void ShiftTextureGeometric_BrushPrimit( face_t *f, Vector3& delta ){
	Vector3 texS,texT;
	float tx,ty;
	Vector3 M[3]; // columns of the matrix .. easier that way
	float det;
	Vector3 D[2];
	// compute plane axis base ( doesn't change with translation )
	ComputeAxisBase( f->plane.normal, texS, texT );
	// compute translation vector in plane axis base
	tx = vector3_dot( delta, texS );
	ty = vector3_dot( delta, texT );
	// fill the data vectors
	M[0][0] = tx; M[0][1] = 1.0f + tx; M[0][2] = tx;
	M[1][0] = ty; M[1][1] = ty; M[1][2] = 1.0f + ty;
	M[2][0] = 1.0f; M[2][1] = 1.0f; M[2][2] = 1.0f;
	D[0][0] = f->brushprimit_texdef.coords[0][2];
	D[0][1] = f->brushprimit_texdef.coords[0][0] + f->brushprimit_texdef.coords[0][2];
	D[0][2] = f->brushprimit_texdef.coords[0][1] + f->brushprimit_texdef.coords[0][2];
	D[1][0] = f->brushprimit_texdef.coords[1][2];
	D[1][1] = f->brushprimit_texdef.coords[1][0] + f->brushprimit_texdef.coords[1][2];
	D[1][2] = f->brushprimit_texdef.coords[1][1] + f->brushprimit_texdef.coords[1][2];
	// solve
	det = SarrusDet( M[0], M[1], M[2] );
	f->brushprimit_texdef.coords[0][0] = SarrusDet( D[0], M[1], M[2] ) / det;
	f->brushprimit_texdef.coords[0][1] = SarrusDet( M[0], D[0], M[2] ) / det;
	f->brushprimit_texdef.coords[0][2] = SarrusDet( M[0], M[1], D[0] ) / det;
	f->brushprimit_texdef.coords[1][0] = SarrusDet( D[1], M[1], M[2] ) / det;
	f->brushprimit_texdef.coords[1][1] = SarrusDet( M[0], D[1], M[2] ) / det;
	f->brushprimit_texdef.coords[1][2] = SarrusDet( M[0], M[1], D[1] ) / det;
}

// shift a texture (texture adjustments) along it's current texture axes
// x and y are geometric values, which we must compute as ST increments
// this depends on the texture size and the pixel/texel ratio
void ShiftTextureRelative_BrushPrimit( face_t *f, float x, float y ){
	float s,t;
	// as a ratio against texture size
	// the scale of the texture is not relevant here (we work directly on a transformation from the base vectors)
	s = ( x * 2.0 ) / (float)f->pShader->getTexture().width;
	t = ( y * 2.0 ) / (float)f->pShader->getTexture().height;
	f->brushprimit_texdef.coords[0][2] -= s;
	f->brushprimit_texdef.coords[1][2] -= t;
}
#endif
#if 0
// TTimo: FIXME: I don't like that, it feels broken
//   (and it's likely that it's not used anymore)
// best fitted 2D vector is x.X+y.Y
void ComputeBest2DVector( Vector3& v, Vector3& X, Vector3& Y, int &x, int &y ){
	double sx,sy;
	sx = vector3_dot( v, X );
	sy = vector3_dot( v, Y );
	if ( fabs( sy ) > fabs( sx ) ) {
		x = 0;
		if ( sy > 0.0 ) {
			y =  1;
		}
		else{
			y = -1;
		}
	}
	else
	{
		y = 0;
		if ( sx > 0.0 ) {
			x =  1;
		}
		else{
			x = -1;
		}
	}
}
#endif

#if 0 // texdef conversion
void BrushPrimitFaceToFace( face_t *face ){
	// we have parsed brush primitives and need conversion back to standard format
	// NOTE: converting back is a quick hack, there's some information lost and we can't do anything about it
	// FIXME: if we normalize the texture matrix to a standard 2x2 size, we end up with wrong scaling
	// I tried various tweaks, no luck .. seems shifting is lost
	brushprimit_texdef_t aux;
	ConvertTexMatWithQTexture( &face->brushprimit_texdef, face->pShader->getTexture(), &aux, 0 );
	TexMatToFakeTexCoords( aux.coords, face->texdef.shift, &face->texdef.rotate, face->texdef.scale );
	face->texdef.scale[0] /= 2.0;
	face->texdef.scale[1] /= 2.0;
}
#endif


#if 0 // texture locking (brush primit)
// TEXTURE LOCKING -----------------------------------------------------------------------------------------------------
// (Relevant to the editor only?)

// internally used for texture locking on rotation and flipping
// the general algorithm is the same for both lockings, it's only the geometric transformation part that changes
// so I wanted to keep it in a single function
// if there are more linear transformations that need the locking, going to a C++ or code pointer solution would be best
// (but right now I want to keep brush_primit.cpp striclty C)

bool txlock_bRotation;

// rotation locking params
int txl_nAxis;
float txl_fDeg;
Vector3 txl_vOrigin;

// flip locking params
Vector3 txl_matrix[3];
Vector3 txl_origin;

void TextureLockTransformation_BrushPrimit( face_t *f ){
	Vector3 Orig,texS,texT;      // axis base of initial plane
	// used by transformation algo
	Vector3 temp; int j;
	Vector3 vRotate;                        // rotation vector

	Vector3 rOrig,rvecS,rvecT;   // geometric transformation of (0,0) (1,0) (0,1) { initial plane axis base }
	Vector3 rNormal,rtexS,rtexT; // axis base for the transformed plane
	Vector3 lOrig,lvecS,lvecT;  // [2] are not used ( but usefull for debugging )
	Vector3 M[3];
	float det;
	Vector3 D[2];

	// compute plane axis base
	ComputeAxisBase( f->plane.normal, texS, texT );
	VectorSet( Orig, 0.0f, 0.0f, 0.0f );

	// compute coordinates of (0,0) (1,0) (0,1) ( expressed in initial plane axis base ) after transformation
	// (0,0) (1,0) (0,1) ( expressed in initial plane axis base ) <-> (0,0,0) texS texT ( expressed world axis base )
	// input: Orig, texS, texT (and the global locking params)
	// ouput: rOrig, rvecS, rvecT, rNormal
	if ( txlock_bRotation ) {
		// rotation vector
		VectorSet( vRotate, 0.0f, 0.0f, 0.0f );
		vRotate[txl_nAxis] = txl_fDeg;
		VectorRotateOrigin( Orig, vRotate, txl_vOrigin, rOrig );
		VectorRotateOrigin( texS, vRotate, txl_vOrigin, rvecS );
		VectorRotateOrigin( texT, vRotate, txl_vOrigin, rvecT );
		// compute normal of plane after rotation
		VectorRotate( f->plane.normal, vRotate, rNormal );
	}
	else
	{
		for ( j = 0 ; j < 3 ; j++ )
			rOrig[j] = vector3_dot( vector3_subtracted( Orig, txl_origin ), txl_matrix[j] ) + txl_origin[j];
		for ( j = 0 ; j < 3 ; j++ )
			rvecS[j] = vector3_dot( vector3_subtracted( texS, txl_origin ), txl_matrix[j] ) + txl_origin[j];
		for ( j = 0 ; j < 3 ; j++ )
			rvecT[j] = vector3_dot( vector3_subtracted( texT, txl_origin ), txl_matrix[j] ) + txl_origin[j];
		// we also need the axis base of the target plane, apply the transformation matrix to the normal too..
		for ( j = 0 ; j < 3 ; j++ )
			rNormal[j] = vector3_dot( f->plane.normal, txl_matrix[j] );
	}

	// compute rotated plane axis base
	ComputeAxisBase( rNormal, rtexS, rtexT );
	// compute S/T coordinates of the three points in rotated axis base ( in M matrix )
	lOrig[0] = vector3_dot( rOrig, rtexS );
	lOrig[1] = vector3_dot( rOrig, rtexT );
	lvecS[0] = vector3_dot( rvecS, rtexS );
	lvecS[1] = vector3_dot( rvecS, rtexT );
	lvecT[0] = vector3_dot( rvecT, rtexS );
	lvecT[1] = vector3_dot( rvecT, rtexT );
	M[0][0] = lOrig[0]; M[1][0] = lOrig[1]; M[2][0] = 1.0f;
	M[0][1] = lvecS[0]; M[1][1] = lvecS[1]; M[2][1] = 1.0f;
	M[0][2] = lvecT[0]; M[1][2] = lvecT[1]; M[2][2] = 1.0f;
	// fill data vector
	D[0][0] = f->brushprimit_texdef.coords[0][2];
	D[0][1] = f->brushprimit_texdef.coords[0][0] + f->brushprimit_texdef.coords[0][2];
	D[0][2] = f->brushprimit_texdef.coords[0][1] + f->brushprimit_texdef.coords[0][2];
	D[1][0] = f->brushprimit_texdef.coords[1][2];
	D[1][1] = f->brushprimit_texdef.coords[1][0] + f->brushprimit_texdef.coords[1][2];
	D[1][2] = f->brushprimit_texdef.coords[1][1] + f->brushprimit_texdef.coords[1][2];
	// solve
	det = SarrusDet( M[0], M[1], M[2] );
	f->brushprimit_texdef.coords[0][0] = SarrusDet( D[0], M[1], M[2] ) / det;
	f->brushprimit_texdef.coords[0][1] = SarrusDet( M[0], D[0], M[2] ) / det;
	f->brushprimit_texdef.coords[0][2] = SarrusDet( M[0], M[1], D[0] ) / det;
	f->brushprimit_texdef.coords[1][0] = SarrusDet( D[1], M[1], M[2] ) / det;
	f->brushprimit_texdef.coords[1][1] = SarrusDet( M[0], D[1], M[2] ) / det;
	f->brushprimit_texdef.coords[1][2] = SarrusDet( M[0], M[1], D[1] ) / det;
}

// texture locking
// called before the points on the face are actually rotated
void RotateFaceTexture_BrushPrimit( face_t *f, int nAxis, float fDeg, Vector3& vOrigin ){
	// this is a placeholder to call the general texture locking algorithm
	txlock_bRotation = true;
	txl_nAxis = nAxis;
	txl_fDeg = fDeg;
	VectorCopy( vOrigin, txl_vOrigin );
	TextureLockTransformation_BrushPrimit( f );
}

// compute the new brush primit texture matrix for a transformation matrix and a flip order flag (change plane orientation)
// this matches the select_matrix algo used in select.cpp
// this needs to be called on the face BEFORE any geometric transformation
// it will compute the texture matrix that will represent the same texture on the face after the geometric transformation is done
void ApplyMatrix_BrushPrimit( face_t *f, Vector3 matrix[3], Vector3& origin ){
	// this is a placeholder to call the general texture locking algorithm
	txlock_bRotation = false;
	VectorCopy( matrix[0], txl_matrix[0] );
	VectorCopy( matrix[1], txl_matrix[1] );
	VectorCopy( matrix[2], txl_matrix[2] );
	VectorCopy( origin, txl_origin );
	TextureLockTransformation_BrushPrimit( f );
}
#endif
#if 0
// low level functions .. put in mathlib?
#define BPMatCopy( a,b ) {b[0][0] = a[0][0]; b[0][1] = a[0][1]; b[0][2] = a[0][2]; b[1][0] = a[1][0]; b[1][1] = a[1][1]; b[1][2] = a[1][2]; }
// apply a scale transformation to the BP matrix
#define BPMatScale( m,sS,sT ) {m[0][0] *= sS; m[1][0] *= sS; m[0][1] *= sT; m[1][1] *= sT; }
// apply a translation transformation to a BP matrix
#define BPMatTranslate( m,s,t ) {m[0][2] += m[0][0] * s + m[0][1] * t; m[1][2] += m[1][0] * s + m[1][1] * t; }
// 2D homogeneous matrix product C = A*B
void BPMatMul( float A[2][3], float B[2][3], float C[2][3] );
// apply a rotation (degrees)
void BPMatRotate( float A[2][3], float theta );

// don't do C==A!
void BPMatMul( float A[2][3], float B[2][3], float C[2][3] ){
	C[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0];
	C[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0];
	C[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1];
	C[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1];
	C[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2];
	C[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2];
}

void BPMatDump( float A[2][3] ){
	globalOutputStream() << "" << A[0][0]
						 << " " << A[0][1]
						 << " " << A[0][2]
						 << "\n" << A[1][0]
						 << " " << A[1][2]
						 << " " << A[1][2]
						 << "\n0 0 1\n";
}

void BPMatRotate( float A[2][3], float theta ){
	float m[2][3];
	float aux[2][3];
	memset( &m, 0, sizeof( float ) * 6 );
	m[0][0] = static_cast<float>( cos( degrees_to_radians( theta ) ) );
	m[0][1] = static_cast<float>( -sin( degrees_to_radians( theta ) ) );
	m[1][0] = -m[0][1];
	m[1][1] = m[0][0];
	BPMatMul( A, m, aux );
	BPMatCopy( aux,A );
}
#endif
#if 0 // camera-relative texture shift
// get the relative axes of the current texturing
void BrushPrimit_GetRelativeAxes( face_t *f, Vector3& vecS, Vector3& vecT ){
	float vS[2],vT[2];
	// first we compute them as expressed in plane axis base
	// BP matrix has coordinates of plane axis base expressed in geometric axis base
	// so we use the line vectors
	vS[0] = f->brushprimit_texdef.coords[0][0];
	vS[1] = f->brushprimit_texdef.coords[0][1];
	vT[0] = f->brushprimit_texdef.coords[1][0];
	vT[1] = f->brushprimit_texdef.coords[1][1];
	// now compute those vectors in geometric space
	Vector3 texS, texT; // axis base of the plane (geometric)
	ComputeAxisBase( f->plane.normal, texS, texT );
	// vecS[] = vS[0].texS[] + vS[1].texT[]
	// vecT[] = vT[0].texS[] + vT[1].texT[]
	vecS[0] = vS[0] * texS[0] + vS[1] * texT[0];
	vecS[1] = vS[0] * texS[1] + vS[1] * texT[1];
	vecS[2] = vS[0] * texS[2] + vS[1] * texT[2];
	vecT[0] = vT[0] * texS[0] + vT[1] * texT[0];
	vecT[1] = vT[0] * texS[1] + vT[1] * texT[1];
	vecT[2] = vT[0] * texS[2] + vT[1] * texT[2];
}

// brush primitive texture adjustments, use the camera view to map adjustments
// ShiftTextureRelative_BrushPrimit ( s , t ) will shift relative to the texture
void ShiftTextureRelative_Camera( face_t *f, int x, int y ){
	Vector3 vecS, vecT;
	float XY[2]; // the values we are going to send for translation
	float sgn[2]; // +1 or -1
	int axis[2];
	CamWnd* pCam;

	// get the two relative texture axes for the current texturing
	BrushPrimit_GetRelativeAxes( f, vecS, vecT );

	// center point of the face, project it on the camera space
	Vector3 C;
	VectorClear( C );
	int i;
	for ( i = 0; i < f->face_winding->numpoints; i++ )
	{
		VectorAdd( C,f->face_winding->point_at( i ),C );
	}
	VectorScale( C,1.0 / f->face_winding->numpoints,C );

	pCam = g_pParentWnd->GetCamWnd();
	pCam->MatchViewAxes( C, vecS, axis[0], sgn[0] );
	pCam->MatchViewAxes( C, vecT, axis[1], sgn[1] );

	// this happens when the two directions can't be mapped on two different directions on the screen
	// then the move will occur against a single axis
	// (i.e. the user is not positioned well enough to send understandable shift commands)
	// NOTE: in most cases this warning is not very relevant because the user would use one of the two axes
	// for which the solution is easy (the other one being unknown)
	// so this warning could be removed
	if ( axis[0] == axis[1] ) {
		globalWarningStream() << "Warning: degenerate in ShiftTextureRelative_Camera\n";
	}

	// compute the X Y geometric increments
	// those geometric increments will be applied along the texture axes (the ones we computed above)
	XY[0] = 0;
	XY[1] = 0;
	if ( x != 0 ) {
		// moving right/left
		XY[axis[0]] += sgn[0] * x;
	}
	if ( y != 0 ) {
		XY[axis[1]] += sgn[1] * y;
	}
	// we worked out a move along vecS vecT, and we now it's geometric amplitude
	// apply it
	ShiftTextureRelative_BrushPrimit( f, XY[0], XY[1] );
}
#endif

#include "math/quaternion.h"

void Valve220_rotate( TextureProjection& projection, float angle ){
//	globalOutputStream() << angle << " angle\n";
//	globalOutputStream() << projection.m_texdef.rotate << " projection.m_texdef.rotate\n";
	const Matrix4 rotmat = matrix4_rotation_for_axisangle( vector3_cross( projection.m_basis_s, projection.m_basis_t ), degrees_to_radians( -angle ) );
	matrix4_transform_direction( rotmat, projection.m_basis_s );
	matrix4_transform_direction( rotmat, projection.m_basis_t );
	vector3_normalise( projection.m_basis_s );
	vector3_normalise( projection.m_basis_t );
//	globalOutputStream() << projection.m_basis_s << " projection.m_basis_s\n";
//	globalOutputStream() << projection.m_basis_t << " projection.m_basis_t\n";
}

void BPTexdef_Assign( brushprimit_texdef_t& bp_td, const brushprimit_texdef_t& bp_other ){
	bp_td = bp_other;
}

void BPTexdef_Shift( brushprimit_texdef_t& bp_td, float s, float t ){
	// shift a texture (texture adjustments) along it's current texture axes
	// x and y are geometric values, which we must compute as ST increments
	// this depends on the texture size and the pixel/texel ratio
	// as a ratio against texture size
	// the scale of the texture is not relevant here (we work directly on a transformation from the base vectors)
	bp_td.coords[0][2] -= s;
	bp_td.coords[1][2] += t;
}

void BPTexdef_Scale( brushprimit_texdef_t& bp_td, float s, float t ){
#if 0
	// apply same scale as the spinner button of the surface inspector
	texdef_t texdef;
	// compute fake shift scale rot
	TexMatToFakeTexCoords( bp_td, texdef );
	// update
	texdef.scale[0] += s;
	texdef.scale[1] += t;
	// compute new normalized texture matrix
	FakeTexCoordsToTexMat( texdef, bp_td );
#else
	texdef_t texdef;
	TexMatToFakeTexCoords( bp_td, texdef );

	float scaleS = -1.f;
	float scaleT = -1.f;
	if( fabs( texdef.scale[0] + s ) > 1e-5 ){
		scaleS = texdef.scale[0] / ( texdef.scale[0] + s );
	}
	if( fabs( texdef.scale[1] + t ) > 1e-5 ){
		scaleT = texdef.scale[1] / ( texdef.scale[1] + t );
	}
	bp_td.coords[0][0] *= scaleS;
	bp_td.coords[0][1] *= scaleS;
	bp_td.coords[1][0] *= scaleT;
	bp_td.coords[1][1] *= scaleT;
#endif
}

void BPTexdef_Rotate( brushprimit_texdef_t& bp_td, float angle ){
#if 0
	// apply same scale as the spinner button of the surface inspector
	texdef_t texdef;
	// compute fake shift scale rot
	TexMatToFakeTexCoords( bp_td, texdef );
	// update
	texdef.rotate += angle;
	// compute new normalized texture matrix
	FakeTexCoordsToTexMat( texdef, bp_td );
#else
	const float x = bp_td.coords[0][0];
	const float y = bp_td.coords[0][1];
	const float x1 = bp_td.coords[1][0];
	const float y1 = bp_td.coords[1][1];
	const float s = sin( degrees_to_radians( -angle ) );
	const float c = cos( degrees_to_radians( -angle ) );
	bp_td.coords[0][0] = x * c - y * s ;
	bp_td.coords[0][1] = x * s + y * c;
	bp_td.coords[1][0] = x1 * c - y1 * s;
	bp_td.coords[1][1] = x1 * s + y1 * c;
#endif
}

void BPTexdef_Assign( brushprimit_texdef_t& bp_td, const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	texdef_t texdef;
	TexMatToFakeTexCoords( bp_td, texdef );

	if( hShift ){
		bp_td.coords[0][2] = -*hShift;
	}
	if( vShift ){
		bp_td.coords[1][2] = *vShift;
	}
	if( hScale ){
		float scaleS = -1.f;
		if( fabs( *hScale ) > 1e-5 ){
			scaleS = texdef.scale[0] / *hScale;
		}
		bp_td.coords[0][0] *= scaleS;
		bp_td.coords[0][1] *= scaleS;
	}
	if( vScale ){
		float scaleT = -1.f;
		if( fabs( *vScale ) > 1e-5 ){
			scaleT = texdef.scale[1] / *vScale;
		}
		bp_td.coords[1][0] *= scaleT;
		bp_td.coords[1][1] *= scaleT;
	}
	if( rotation ){
		BPTexdef_Rotate( bp_td, *rotation - texdef.rotate );
	}
}
#if 0
void BPTexdef_Construct( brushprimit_texdef_t& bp_td, std::size_t width, std::size_t height ){
	bp_td.coords[0][0] = 1.0f;
	bp_td.coords[1][1] = 1.0f;
	ConvertTexMatWithDimensions( bp_td.coords, 2, 2, bp_td.coords, width, height );
}
#endif
void Texdef_Assign( TextureProjection& projection, const TextureProjection& other, bool setBasis /*= true*/ ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_Assign( projection.m_brushprimit_texdef, other.m_brushprimit_texdef );
	}
	else
	{
		Texdef_Assign( projection.m_texdef, other.m_texdef );
		if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE && setBasis ) {
			projection.m_basis_s = other.m_basis_s;
			projection.m_basis_t = other.m_basis_t;
		}
	}
}

void Texdef_Assign( TextureProjection& projection, const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_Assign( projection.m_brushprimit_texdef, hShift, vShift, hScale, vScale, rotation );
	}
	else
	{
		if ( rotation && g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
			Valve220_rotate( projection, *rotation - projection.m_texdef.rotate );
		}
		Texdef_Assign( projection.m_texdef, hShift, vShift, hScale, vScale, rotation );
	}
}

void Texdef_Shift( TextureProjection& projection, float s, float t ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_Shift( projection.m_brushprimit_texdef, s, t );
	}
	else
	{
		Texdef_Shift( projection.m_texdef, s, t );
	}
}

void Texdef_Scale( TextureProjection& projection, float s, float t ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_Scale( projection.m_brushprimit_texdef, s, t );
	}
	else
	{
		Texdef_Scale( projection.m_texdef, s, t );
	}
}

void Texdef_Rotate( TextureProjection& projection, float angle ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_Rotate( projection.m_brushprimit_texdef, angle );
	}
	else
	{
		if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
			Valve220_rotate( projection, angle );
		}
		Texdef_Rotate( projection.m_texdef, angle );
	}
}

void Texdef_FitTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Vector3& normal, const Winding& w, float s_repeat, float t_repeat, bool only_dimension ){
	if ( w.numpoints < 3 ) {
		return;
	}

	Matrix4 st2tex;
	Texdef_toTransform( projection, (float)width, (float)height, st2tex );

	// the current texture transform
	Matrix4 local2tex = st2tex;
	{
		Matrix4 xyz2st;
		Texdef_basisForNormal( projection, normal, xyz2st );
		matrix4_multiply_by_matrix4( local2tex, xyz2st );
	}

	// the bounds of the current texture transform
	AABB bounds;
	for ( Winding::const_iterator i = w.begin(); i != w.end(); ++i )
	{
		Vector3 texcoord = matrix4_transformed_point( local2tex, ( *i ).vertex );
		aabb_extend_by_point_safe( bounds, texcoord );
	}
	bounds.origin.z() = 0;
	bounds.extents.z() = 1;

	// the bounds of a perfectly fitted texture transform
	AABB perfect;
	if( t_repeat == 0 && s_repeat == 0 ){
		//bad user's input
		t_repeat = s_repeat = 1;
		perfect.origin = Vector3( s_repeat * 0.5, t_repeat * 0.5, 0 );
		perfect.extents = Vector3( s_repeat * 0.5, t_repeat * 0.5, 1 );
	}
	if( t_repeat == 0 ){
		if( only_dimension ){ //fit width, keep height
			perfect.origin = Vector3( s_repeat * 0.5, bounds.origin.y(), 0 );
			perfect.extents = Vector3( s_repeat * 0.5, bounds.extents.y(), 1 );
		}
		else{ //fit width
			perfect.origin = Vector3( s_repeat * 0.5, s_repeat * 0.5 * bounds.extents.y() / bounds.extents.x(), 0 );
			perfect.extents = Vector3( s_repeat * 0.5, s_repeat * 0.5 * bounds.extents.y() / bounds.extents.x(), 1 );
		}
	}
	else if( s_repeat == 0 ){
		if( only_dimension ){ //fit height, keep width
			perfect.origin = Vector3( bounds.origin.x(), t_repeat * 0.5, 0 );
			perfect.extents = Vector3( bounds.extents.x(), t_repeat * 0.5, 1 );
		}
		else{ //fit height
			perfect.origin = Vector3( t_repeat * 0.5 * bounds.extents.x() / bounds.extents.y(), t_repeat * 0.5, 0 );
			perfect.extents = Vector3( t_repeat * 0.5 * bounds.extents.x() / bounds.extents.y(), t_repeat * 0.5, 1 );
		}
	}
	else{
		perfect.origin = Vector3( s_repeat * 0.5, t_repeat * 0.5, 0 );
		perfect.extents = Vector3( s_repeat * 0.5, t_repeat * 0.5, 1 );
	}

	// the difference between the current texture transform and the perfectly fitted transform
	Matrix4 matrix( matrix4_translation_for_vec3( bounds.origin - perfect.origin ) );
	matrix4_pivoted_scale_by_vec3( matrix, bounds.extents / perfect.extents, perfect.origin );
	matrix4_affine_invert( matrix );

	// apply the difference to the current texture transform
	matrix4_premultiply_by_matrix4( st2tex, matrix );

	Texdef_fromTransform( projection, (float)width, (float)height, st2tex );
	//Texdef_normalise( projection, (float)width, (float)height );
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES )
		BPTexdef_normalise( projection.m_brushprimit_texdef, 1.f, 1.f ); /* scaleApplied is! */
	else
		Texdef_normalise( projection.m_texdef, (float)width, (float)height );
}

float Texdef_getDefaultTextureScale(){
	return g_texdef_default_scale;
}

void TexDef_Construct_Default( TextureProjection& projection ){
	projection.m_texdef.scale[0] = Texdef_getDefaultTextureScale();
	projection.m_texdef.scale[1] = Texdef_getDefaultTextureScale();

	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		FakeTexCoordsToTexMat( projection.m_texdef, projection.m_brushprimit_texdef );
	}
}



void ShiftScaleRotate_fromFace( texdef_t& shiftScaleRotate, const TextureProjection& projection ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		TexMatToFakeTexCoords( projection.m_brushprimit_texdef, shiftScaleRotate );
	}
	else
	{
		shiftScaleRotate = projection.m_texdef;
	}
}

void ShiftScaleRotate_toFace( const texdef_t& shiftScaleRotate, TextureProjection& projection ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		// compute texture matrix
		// the matrix returned must be understood as a qtexture_t with width=2 height=2
		FakeTexCoordsToTexMat( shiftScaleRotate, projection.m_brushprimit_texdef );
	}
	else
	{
		projection.m_texdef = shiftScaleRotate;
	}
}

inline void printAP( const TextureProjection& projection ){
	globalOutputStream() << "AP: scale( " << projection.m_texdef.scale[0] << " " << projection.m_texdef.scale[1] << " ) shift( " << projection.m_texdef.shift[0] << " " << projection.m_texdef.shift[1] << " ) rotate: " << projection.m_texdef.rotate << "\n";
}
inline void printBP( const TextureProjection& projection ){
	globalOutputStream() << "BP: ( " << projection.m_brushprimit_texdef.coords[0][0] << " " << projection.m_brushprimit_texdef.coords[0][1] << " " << projection.m_brushprimit_texdef.coords[0][2] << " ) ( " << projection.m_brushprimit_texdef.coords[1][0] << " " << projection.m_brushprimit_texdef.coords[1][1] << " " << projection.m_brushprimit_texdef.coords[1][2] << " )\n";
}
inline void print220( const TextureProjection& projection ){
	globalOutputStream() << "220: projection.m_basis_s: " << projection.m_basis_s << " projection.m_basis_t: " << projection.m_basis_t << "\n";
	printAP( projection );
}

#if 0
inline void print_vector3( const Vector3& v ){
	globalOutputStream() << "( " << v.x() << " " << v.y() << " " << v.z() << " )\n";
}

inline void print_3x3( const Matrix4& m ){
	globalOutputStream() << "( " << m.xx() << " " << m.xy() << " " << m.xz() << " ) "
						 << "( " << m.yx() << " " << m.yy() << " " << m.yz() << " ) "
						 << "( " << m.zx() << " " << m.zy() << " " << m.zz() << " )\n";
}


inline Matrix4 matrix4_rotation_for_vector3( const Vector3& x, const Vector3& y, const Vector3& z ){
	return Matrix4(
			   x.x(), x.y(), x.z(), 0,
			   y.x(), y.y(), y.z(), 0,
			   z.x(), z.y(), z.z(), 0,
			   0, 0, 0, 1
			   );
}

inline Matrix4 matrix4_swap_axes( const Vector3& from, const Vector3& to ){
	if ( from.x() != 0 && to.y() != 0 ) {
		return matrix4_rotation_for_vector3( to, from, g_vector3_axis_z );
	}

	if ( from.x() != 0 && to.z() != 0 ) {
		return matrix4_rotation_for_vector3( to, g_vector3_axis_y, from );
	}

	if ( from.y() != 0 && to.z() != 0 ) {
		return matrix4_rotation_for_vector3( g_vector3_axis_x, to, from );
	}

	if ( from.y() != 0 && to.x() != 0 ) {
		return matrix4_rotation_for_vector3( from, to, g_vector3_axis_z );
	}

	if ( from.z() != 0 && to.x() != 0 ) {
		return matrix4_rotation_for_vector3( from, g_vector3_axis_y, to );
	}

	if ( from.z() != 0 && to.y() != 0 ) {
		return matrix4_rotation_for_vector3( g_vector3_axis_x, from, to );
	}

	ERROR_MESSAGE( "unhandled axis swap case" );

	return g_matrix4_identity;
}

inline Matrix4 matrix4_reflection_for_plane( const Plane3& plane ){
	return Matrix4(
			   static_cast<float>( 1 - ( 2 * plane.a * plane.a ) ),
			   static_cast<float>( -2 * plane.a * plane.b ),
			   static_cast<float>( -2 * plane.a * plane.c ),
			   0,
			   static_cast<float>( -2 * plane.b * plane.a ),
			   static_cast<float>( 1 - ( 2 * plane.b * plane.b ) ),
			   static_cast<float>( -2 * plane.b * plane.c ),
			   0,
			   static_cast<float>( -2 * plane.c * plane.a ),
			   static_cast<float>( -2 * plane.c * plane.b ),
			   static_cast<float>( 1 - ( 2 * plane.c * plane.c ) ),
			   0,
			   static_cast<float>( -2 * plane.d * plane.a ),
			   static_cast<float>( -2 * plane.d * plane.b ),
			   static_cast<float>( -2 * plane.d * plane.c ),
			   1
			   );
}

inline Matrix4 matrix4_reflection_for_plane45( const Plane3& plane, const Vector3& from, const Vector3& to ){
	Vector3 first = from;
	Vector3 second = to;

	if ( ( vector3_dot( from, plane.normal() ) > 0 ) == ( vector3_dot( to, plane.normal() ) > 0 ) ) {
		first = vector3_negated( first );
		second = vector3_negated( second );
	}

#if 0
	globalOutputStream() << "normal: ";
	print_vector3( plane.normal() );

	globalOutputStream() << "from: ";
	print_vector3( first );

	globalOutputStream() << "to: ";
	print_vector3( second );
#endif

	Matrix4 swap = matrix4_swap_axes( first, second );

	//Matrix4 tmp = matrix4_reflection_for_plane( plane );

	swap.tx() = -static_cast<float>( -2 * plane.a * plane.d );
	swap.ty() = -static_cast<float>( -2 * plane.b * plane.d );
	swap.tz() = -static_cast<float>( -2 * plane.c * plane.d );

	return swap;
}

void Texdef_transformLocked_original( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& identity2transformed, const Vector3 centroid ){
//			globalOutputStream() << "\t\t----------------------\n";
//			printAP( projection );
//			printBP( projection );
//			globalOutputStream() << "width:" << width << " height" << height << "\n";

		//globalOutputStream() << "identity2transformed: " << identity2transformed << "\n";

		//globalOutputStream() << "plane.normal(): " << plane.normal() << "\n";
		#if 0
		const Vector3 normalTransformed( matrix4_transformed_direction( identity2transformed, plane.normal() ) );
		#else //preserves scale in BP while scaling, but not shift //fixes QNAN
		const Matrix4 maa( matrix4_for_normal_transform( identity2transformed ) );
		const Vector3 normalTransformed( vector3_normalised( matrix4_transformed_direction( maa, plane.normal() ) ) );
		#endif

		//globalOutputStream() << "normalTransformed: " << normalTransformed << "\n";

		// identity: identity space
		// transformed: transformation
		// stIdentity: base st projection space before transformation
		// stTransformed: base st projection space after transformation
		// stOriginal: original texdef space

		// stTransformed2stOriginal = stTransformed -> transformed -> identity -> stIdentity -> stOriginal

		Matrix4 identity2stIdentity;
		Texdef_basisForNormal( projection, plane.normal(), identity2stIdentity );
		//globalOutputStream() << "identity2stIdentity: " << identity2stIdentity << "\n";

		if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
			matrix4_transform_direction( maa, projection.m_basis_s );
			matrix4_transform_direction( maa, projection.m_basis_t );
		}

		Matrix4 transformed2stTransformed;
		Texdef_basisForNormal( projection, normalTransformed, transformed2stTransformed );
//			globalOutputStream() << "transformed2stTransformed: " << transformed2stTransformed << "\n";
		Matrix4 stTransformed2identity( matrix4_affine_inverse( matrix4_multiplied_by_matrix4( transformed2stTransformed, identity2transformed ) ) );
//			globalOutputStream() << "stTransformed2identity: " << stTransformed2identity << "\n";
		Vector3 originalProjectionAxis( vector4_to_vector3( matrix4_affine_inverse( identity2stIdentity ).z() ) );

		Vector3 transformedProjectionAxis( vector4_to_vector3( stTransformed2identity.z() ) );

		Matrix4 stIdentity2stOriginal;
		Texdef_toTransform( projection, (float)width, (float)height, stIdentity2stOriginal );
//			globalOutputStream() << "stIdentity2stOriginal: " << stIdentity2stOriginal << "\n";
		Matrix4 identity2stOriginal( matrix4_multiplied_by_matrix4( stIdentity2stOriginal, identity2stIdentity ) );
//			globalOutputStream() << "identity2stOriginal: " << identity2stOriginal << "\n";
		//globalOutputStream() << "originalProj: " << originalProjectionAxis << "\n";
		//globalOutputStream() << "transformedProj: " << transformedProjectionAxis << "\n";
		double dot = vector3_dot( originalProjectionAxis, transformedProjectionAxis );
		//globalOutputStream() << "dot: " << dot << "\n";
		if ( dot == 0 ) {
			// The projection axis chosen for the transformed normal is at 90 degrees
			// to the transformed projection axis chosen for the original normal.
			// This happens when the projection axis is ambiguous - e.g. for the plane
			// 'X == Y' the projection axis could be either X or Y.
			//globalOutputStream() << "flipped\n";
	#if 0
			globalOutputStream() << "projection off by 90\n";
			globalOutputStream() << "normal: ";
			print_vector3( plane.normal() );
			globalOutputStream() << "original projection: ";
			print_vector3( originalProjectionAxis );
			globalOutputStream() << "transformed projection: ";
			print_vector3( transformedProjectionAxis );
	#endif

			Matrix4 identityCorrected = matrix4_reflection_for_plane45( plane, originalProjectionAxis, transformedProjectionAxis );

			identity2stOriginal = matrix4_multiplied_by_matrix4( identity2stOriginal, identityCorrected );
		}
		else if( dot != dot ){ //catch QNAN: happens on scaling cuboid on Z and sometimes on rotating (in bp mode) //and in making seamless to self or parallel
			return;
		}

		Matrix4 stTransformed2stOriginal = matrix4_multiplied_by_matrix4( identity2stOriginal, stTransformed2identity );
//			globalOutputStream() << "stTransformed2stOriginal: " << stTransformed2stOriginal << "\n";
		Texdef_fromTransform( projection, (float)width, (float)height, stTransformed2stOriginal );
//			printAP( projection );
//			printBP( projection );
		Texdef_normalise( projection, (float)width, (float)height );
//			globalOutputStream() << "norm ";	printAP( projection );
//			globalOutputStream() << "norm ";	printBP( projection );
}
#endif

double Det3x3( double a00, double a01, double a02,
			  double a10, double a11, double a12,
			  double a20, double a21, double a22 ){
	return
		a00 * ( a11 * a22 - a12 * a21 )
		-   a01 * ( a10 * a22 - a12 * a20 )
		+   a02 * ( a10 * a21 - a11 * a20 );
}

void BP_from_ST( brushprimit_texdef_t& bp, const DoubleVector3 points[3], const DoubleVector3 st[3], const DoubleVector3& normal ){
	double xyI[2], xyJ[2], xyK[2];
	double stI[2], stJ[2], stK[2];
	double D, D0, D1, D2;
	DoubleVector3 texX, texY;
	ComputeAxisBase( normal, texX, texY );

	xyI[0] = vector3_dot( points[0], texX );
	xyI[1] = vector3_dot( points[0], texY );
	xyJ[0] = vector3_dot( points[1], texX );
	xyJ[1] = vector3_dot( points[1], texY );
	xyK[0] = vector3_dot( points[2], texX );
	xyK[1] = vector3_dot( points[2], texY );
	stI[0] = st[0][0]; stI[1] = st[0][1];
	stJ[0] = st[1][0]; stJ[1] = st[1][1];
	stK[0] = st[2][0]; stK[1] = st[2][1];

	//   - solve linear equations:
	//     - (x, y) := xyz . (texX, texY)
	//     - st[i] = texMat[i][0]*x + texMat[i][1]*y + texMat[i][2]
	//       (for three vertices)
	D = Det3x3(
		xyI[0], xyI[1], 1,
		xyJ[0], xyJ[1], 1,
		xyK[0], xyK[1], 1
		);
	if ( D != 0 ) {
		for ( std::size_t i = 0; i < 2; ++i )
		{
			D0 = Det3x3(
				stI[i], xyI[1], 1,
				stJ[i], xyJ[1], 1,
				stK[i], xyK[1], 1
				);
			D1 = Det3x3(
				xyI[0], stI[i], 1,
				xyJ[0], stJ[i], 1,
				xyK[0], stK[i], 1
				);
			D2 = Det3x3(
				xyI[0], xyI[1], stI[i],
				xyJ[0], xyJ[1], stJ[i],
				xyK[0], xyK[1], stK[i]
				);
			bp.coords[i][0] = D0 / D;
			bp.coords[i][1] = D1 / D;
			bp.coords[i][2] = fmod( D2 / D, 1.0 );
		}
//			globalOutputStream() << "BP out: ( " << bp.coords[0][0] << " " << bp.coords[0][1] << " " << bp.coords[0][2] << " ) ( " << bp.coords[1][0] << " " << bp.coords[1][1] << " " << bp.coords[1][2] << " )\n";
	}
}

const Vector3 BaseAxes[] = {
            Vector3( 0.0,  0.0,  1.0), Vector3( 1.0,  0.0,  0.0), Vector3( 0.0, -1.0,  0.0),
            Vector3( 0.0,  0.0, -1.0), Vector3( 1.0,  0.0,  0.0), Vector3( 0.0, -1.0,  0.0),
            Vector3( 1.0,  0.0,  0.0), Vector3( 0.0,  1.0,  0.0), Vector3( 0.0,  0.0, -1.0),
            Vector3(-1.0,  0.0,  0.0), Vector3( 0.0,  1.0,  0.0), Vector3( 0.0,  0.0, -1.0),
            Vector3( 0.0,  1.0,  0.0), Vector3( 1.0,  0.0,  0.0), Vector3( 0.0,  0.0, -1.0),
            Vector3( 0.0, -1.0,  0.0), Vector3( 1.0,  0.0,  0.0), Vector3( 0.0,  0.0, -1.0),
        };

std::size_t planeNormalIndex( const Vector3& normal ) {
#if 0
	std::size_t bestIndex = 0;
	float bestDot = 0.f;
	for( std::size_t i = 0; i < 6; ++i ) {
		const float dot = vector3_dot( normal, BaseAxes[i * 3] );
		if( dot > bestDot ) { // no need to use -altaxis for qbsp, but -oldaxis is necessary
			bestDot = dot;
			bestIndex = i;
		}
	}
	return bestIndex;
#else
	switch ( projectionaxis_for_normal( normal ) )
	{
	case eProjectionAxisZ:
		return normal.z() > 0 ? 0 : 1;
		break;
	case eProjectionAxisX:
		return normal.x() > 0 ? 2 : 3;
		break;
	default: //case eProjectionAxisY:
		return normal.y() > 0 ? 4 : 5;
		break;
	}
#endif
}

void AP_from_axes( const Vector3& axisX, const Vector3& axisY, const DoubleVector3& normal, std::size_t width, std::size_t height, const Vector3& invariant, const Vector2& invariantTexCoords, texdef_t& texdef ){
	// obtain the texture plane norm and the base texture axes
	const std::size_t index = planeNormalIndex( normal );
	Vector3 xAxis = BaseAxes[index * 3 + 1];
	Vector3 yAxis = BaseAxes[index * 3 + 2];
	Vector3 zAxis = BaseAxes[( index / 2 ) * 6];

	const Plane3 texturePlane( zAxis, 0 );

	// project the transformed texture axes onto the new texture projection plane
	const Vector3 projectedXAxis = plane3_project_point( texturePlane, axisX );
	const Vector3 projectedYAxis = plane3_project_point( texturePlane, axisY );

	const Vector3 normalizedXAxis = vector3_normalised( projectedXAxis );
	const Vector3 normalizedYAxis = vector3_normalised( projectedYAxis );

	// determine the rotation angle from the dot product of the new base axes and the transformed, projected and normalized texture axes
	float cosX = vector3_dot( xAxis, normalizedXAxis );
	float cosY = vector3_dot( yAxis, normalizedYAxis );

	float radX = std::acos( cosX );
	if( vector3_dot( vector3_cross( xAxis, normalizedXAxis ), zAxis ) < 0.0 )
		radX *= -1.0f;

	float radY = std::acos( cosY );
	if( vector3_dot( vector3_cross( yAxis, normalizedYAxis ), zAxis ) < 0.0 )
		radY *= -1.0f;

	// choosing between the X and Y axis rotations
	float rad = width >= height ? radX : radY;

	// for some reason, when the texture plane normal is the Y axis, we must rotation clockwise
	if( ( index / 2 ) * 6 == 12 )
		rad *= -1.0f;

	//	doSetRotation( newNormal, newRotation, newRotation );
	const Matrix4 rotmat = matrix4_rotation_for_axisangle( vector3_cross( yAxis, xAxis ), rad );
	matrix4_transform_direction( rotmat, xAxis );
	matrix4_transform_direction( rotmat, yAxis );

	// finally compute the scaling factors
	Vector2 scale( vector3_length( projectedXAxis ),
						vector3_length( projectedYAxis ) );

	// the sign of the scaling factors depends on the angle between the new texture axis and the projected transformed axis
	if( vector3_dot( xAxis, normalizedXAxis ) < 0 )
		scale[0] *= -1.0f;
	if( vector3_dot( yAxis, normalizedYAxis ) < 0 )
		scale[1] *= -1.0f;

	// determine the new texture coordinates of the transformed center of the face, sans offsets
	const Vector2 newInvariantTexCoords( vector3_dot( xAxis / scale[0], invariant ),
										vector3_dot( yAxis / scale[1], invariant ) );
//		globalOutputStream() << "newInvariantTexCoords: " << newInvariantTexCoords[0] << " " << newInvariantTexCoords[1] << "\n";
	// since the center should be invariant, the offsets are determined by the difference of the current and
	// the original texture coordinates of the center
	texdef.shift[0] = invariantTexCoords[0] - newInvariantTexCoords[0];
	texdef.shift[1] = invariantTexCoords[1] - newInvariantTexCoords[1];
	texdef.scale[0] = scale[0];
	texdef.scale[1] = scale[1];
	texdef.rotate = radians_to_degrees( rad );
	Texdef_normalise( texdef, (float)width, (float)height );
}


void Texdef_transformLocked( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& identity2transformed, const Vector3& invariant ){
	if( identity2transformed == g_matrix4_identity ){
		//globalOutputStream() << "identity2transformed == g_matrix4_identity\n";
		return; //TODO FIXME !!! this (and whole pipeline?) is called with g_matrix4_identity after every transform //now only on freezeTransform, it seems
	}
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
//			globalOutputStream() << "identity2transformed: " << identity2transformed << "\n";
//			globalOutputStream() << "in ";	printBP( projection );
		DoubleVector3 texX, texY;
		ComputeAxisBase( plane.normal(), texX, texY );

		const DoubleVector3 anchor = plane.normal() * plane.dist();
		DoubleVector3 points[3] = { anchor, anchor + texX, anchor + texY };
		DoubleVector3 st[3];

		Matrix4 local2tex;
		Texdef_Construct_local2tex( projection, width, height, plane.normal(), local2tex );
		for ( std::size_t i = 0; i < 3; ++i ){
			st[i] = matrix4_transformed_point( local2tex, points[i] );
			matrix4_transform_point( identity2transformed, points[i] );
		}

#if 0
		const DoubleVector3 normalTransformed( matrix4_transformed_normal( identity2transformed, plane.normal() ) );
#else
        /* this is also handling scale = 0 case */
		DoubleVector3 normalTransformed( plane3_for_points( points ).normal() );
		if( matrix4_handedness( identity2transformed ) == MATRIX4_LEFTHANDED )
			vector3_negate( normalTransformed );
#endif
		BP_from_ST( projection.m_brushprimit_texdef, points, st, normalTransformed );
	}
	else if( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_QUAKE ) {
//			globalOutputStream() << "\t\t***: " << invariant << "\n";
//			globalOutputStream() << "identity2transformed: " << identity2transformed << "\n";
//			printAP( projection );
		if( projection.m_texdef.scale[0] == 0.0f || projection.m_texdef.scale[1] == 0.0f ) {
			return;
		}

		#if 0//not ok, if scaling
		const Vector3 offset = matrix4_transformed_point( identity2transformed, Vector3( 0, 0, 0 ) );
		Vector3 newNormal  = matrix4_transformed_point( identity2transformed, plane.normal() ) - offset;
		#elif 0
		const Vector3 newNormal( matrix4_transformed_normal( identity2transformed, plane.normal() ) );
		#elif 1
        /* this is also handling scale = 0 case */
		DoubleVector3 texX, texY;
		ComputeAxisBase( plane.normal(), texX, texY );
		const DoubleVector3 anchor = plane.normal() * plane.dist();
		DoubleVector3 points[3] = { anchor, anchor + texX, anchor + texY };
		for ( std::size_t i = 0; i < 3; ++i )
			matrix4_transform_point( identity2transformed, points[i] );
		Vector3 newNormal( plane3_for_points( points ).normal() );
		if( matrix4_handedness( identity2transformed ) == MATRIX4_LEFTHANDED )
			vector3_negate( newNormal );
		#endif
#if 0
		// fix some rounding errors - if the old and new texture axes are almost the same, use the old axis
		if( vector3_equal_epsilon( newNormal, plane.normal(), 0.01f ) ){
			newNormal = plane.normal();
		}
#endif
		// calculate the current texture coordinates of the origin
		const std::size_t index = planeNormalIndex( plane.normal() );
		Vector3 xAxis = BaseAxes[index * 3 + 1];
		Vector3 yAxis = BaseAxes[index * 3 + 2];
		Vector3 zAxis = BaseAxes[( index / 2 ) * 6];
//			globalOutputStream() << xAxis << " " << yAxis << " " << zAxis << "\n";
        Matrix4 rotmat = matrix4_rotation_for_axisangle( vector3_cross( yAxis, xAxis ), degrees_to_radians( projection.m_texdef.rotate ) );
        matrix4_transform_direction( rotmat, xAxis );
        matrix4_transform_direction( rotmat, yAxis );

		const Vector2 invariantTexCoords( vector3_dot( xAxis / projection.m_texdef.scale[0], invariant ) + projection.m_texdef.shift[0],
											vector3_dot( yAxis / projection.m_texdef.scale[1], invariant ) + projection.m_texdef.shift[1] );
//			globalOutputStream() << "invariantTexCoords: " << invariantTexCoords[0] << " " << invariantTexCoords[1] << "\n";
		// project the texture axes onto the boundary plane along the texture Z axis
		const Vector3 boundaryOffset  = plane3_project_point( plane, Vector3( 0, 0, 0 ), zAxis );
		const Vector3 xAxisOnBoundary = plane3_project_point( plane, xAxis * projection.m_texdef.scale[0], zAxis ) - boundaryOffset;
		const Vector3 yAxisOnBoundary = plane3_project_point( plane, yAxis * projection.m_texdef.scale[1], zAxis ) - boundaryOffset;

		// transform the projected texture axes and compensate the translational component
		const Vector3 transformedXAxis = matrix4_transformed_direction( identity2transformed, xAxisOnBoundary );
		const Vector3 transformedYAxis = matrix4_transformed_direction( identity2transformed, yAxisOnBoundary );

		AP_from_axes( transformedXAxis, transformedYAxis, newNormal, width, height, matrix4_transformed_point( identity2transformed, invariant ), invariantTexCoords, projection.m_texdef );
//			globalOutputStream() << "new "; printAP( projection );
	}
	else{ //TEXDEFTYPEID_VALVE
//			print220( projection );
//			globalOutputStream() << "identity2transformed: " << identity2transformed << "\n";
		/* hack: is often broken with niggative scale */
		if( projection.m_texdef.scale[0] < 0 ){
			projection.m_texdef.scale[0] *= -1.f;
			projection.m_basis_s *= -1.f;
		}
		if( projection.m_texdef.scale[1] < 0 ){
			projection.m_texdef.scale[1] *= -1.f;
			projection.m_basis_t *= -1.f;
		}

		//globalOutputStream() << "plane.normal(): " << plane.normal() << "\n";
		const Matrix4 maa( matrix4_for_normal_transform( identity2transformed ) );
		const Vector3 normalTransformed( vector3_normalised( matrix4_transformed_direction( maa, plane.normal() ) ) );
		//globalOutputStream() << "normalTransformed: " << normalTransformed << "\n";

		// identity: identity space
		// transformed: transformation
		// stIdentity: base st projection space before transformation
		// stTransformed: base st projection space after transformation
		// stOriginal: original texdef space

		// stTransformed2stOriginal = stTransformed -> transformed -> identity -> stIdentity -> stOriginal

		Matrix4 identity2stIdentity;
		Texdef_basisForNormal( projection, plane.normal(), identity2stIdentity );

		matrix4_transform_direction( maa, projection.m_basis_s );
		matrix4_transform_direction( maa, projection.m_basis_t );

		Matrix4 transformed2stTransformed;
		Texdef_basisForNormal( projection, normalTransformed, transformed2stTransformed );
		Matrix4 stTransformed2identity( matrix4_affine_inverse( matrix4_multiplied_by_matrix4( transformed2stTransformed, identity2transformed ) ) ); //QNAN here, if some scale = 0

		Matrix4 stIdentity2stOriginal;
		Texdef_toTransform( projection, (float)width, (float)height, stIdentity2stOriginal );
		Matrix4 identity2stOriginal( matrix4_multiplied_by_matrix4( stIdentity2stOriginal, identity2stIdentity ) );

		Matrix4 stTransformed2stOriginal = matrix4_multiplied_by_matrix4( identity2stOriginal, stTransformed2identity );
		if( stTransformed2stOriginal[0] == stTransformed2stOriginal[0] ){ /* catch QNAN: happens when projecting along plane */
			Texdef_fromTransform( projection, (float)width, (float)height, stTransformed2stOriginal );
			Texdef_normalise( projection, (float)width, (float)height );

			projection.m_texdef.scale[0] /= vector3_length( projection.m_basis_s );
			projection.m_texdef.scale[1] /= vector3_length( projection.m_basis_t );
		}
		vector3_normalise( projection.m_basis_s );
		vector3_normalise( projection.m_basis_t );
	}
}

void Texdef_transform( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& identity2transformed, const Vector3& invariant ){
	if( identity2transformed == g_matrix4_identity ){
		//globalOutputStream() << "identity2transformed == g_matrix4_identity\n";
		return; //TODO FIXME !!! this (and whole pipeline?) is called with g_matrix4_identity after every transform //now only on freezeTransform, it seems
	}
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ||
		 g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
		Texdef_transformLocked( projection, width, height, plane, identity2transformed, invariant );
	}
	else if( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_QUAKE ) {
//			globalOutputStream() << "\t\t***: " << invariant << "\n";
//			globalOutputStream() << "identity2transformed: " << identity2transformed << "\n";
//			printAP( projection );
		if( projection.m_texdef.scale[0] == 0.0f || projection.m_texdef.scale[1] == 0.0f ) {
			return;
		}

		// calculate the current texture coordinates of the origin
		const std::size_t index = planeNormalIndex( plane.normal() );
		Vector3 xAxis = BaseAxes[index * 3 + 1];
		Vector3 yAxis = BaseAxes[index * 3 + 2];
		Vector3 zAxis = BaseAxes[( index / 2 ) * 6];
//			globalOutputStream() << xAxis << " " << yAxis << " " << zAxis << "\n";
        Matrix4 rotmat = matrix4_rotation_for_axisangle( vector3_cross( yAxis, xAxis ), degrees_to_radians( projection.m_texdef.rotate ) );
        matrix4_transform_direction( rotmat, xAxis );
        matrix4_transform_direction( rotmat, yAxis );

		const Vector2 invariantTexCoords( vector3_dot( xAxis / projection.m_texdef.scale[0], invariant ) + projection.m_texdef.shift[0],
											vector3_dot( yAxis / projection.m_texdef.scale[1], invariant ) + projection.m_texdef.shift[1] );
//			globalOutputStream() << "invariantTexCoords: " << invariantTexCoords[0] << " " << invariantTexCoords[1] << "\n";
		// project the texture axes onto the boundary plane along the texture Z axis
		const Vector3 boundaryOffset  = plane3_project_point( plane, Vector3( 0, 0, 0 ), zAxis );
		const Vector3 xAxisOnBoundary = plane3_project_point( plane, xAxis * projection.m_texdef.scale[0], zAxis ) - boundaryOffset;
		const Vector3 yAxisOnBoundary = plane3_project_point( plane, yAxis * projection.m_texdef.scale[1], zAxis ) - boundaryOffset;

		// transform the projected texture axes and compensate the translational component
		const Vector3 transformedXAxis = matrix4_transformed_direction( identity2transformed, xAxisOnBoundary );
		const Vector3 transformedYAxis = matrix4_transformed_direction( identity2transformed, yAxisOnBoundary );

		AP_from_axes( transformedXAxis, transformedYAxis, plane.normal(), width, height, matrix4_transformed_point( identity2transformed, invariant ), invariantTexCoords, projection.m_texdef );
	}
}

#if 0
void Q3_to_matrix( const texdef_t& texdef, float width, float height, const Vector3& normal, Matrix4& matrix ){
	Normal_GetTransform( normal, matrix );

	Matrix4 transform;

	Texdef_toTransform( texdef, width, height, transform );

	matrix4_multiply_by_matrix4( matrix, transform );
}

void BP_from_matrix( brushprimit_texdef_t& bp_texdef, const Vector3& normal, const Matrix4& transform ){
	Matrix4 basis;
	basis = g_matrix4_identity;
	ComputeAxisBase( normal, vector4_to_vector3( basis.x() ), vector4_to_vector3( basis.y() ) );
	vector4_to_vector3( basis.z() ) = normal;
	matrix4_transpose( basis );
	matrix4_affine_invert( basis );

	Matrix4 basis2texture = matrix4_multiplied_by_matrix4( basis, transform );

	BPTexdef_fromTransform( bp_texdef, basis2texture );
}

void Q3_to_BP( const texdef_t& texdef, float width, float height, const Vector3& normal, brushprimit_texdef_t& bp_texdef ){
	Matrix4 matrix;
	Q3_to_matrix( texdef, width, height, normal, matrix );
	BP_from_matrix( bp_texdef, normal, matrix );
}
#endif




/// for arbitrary texture projections
void Texdef_Construct_local2tex4projection( const texdef_t& texdef, std::size_t width, std::size_t height, const Vector3& normal, const Vector3* direction, Matrix4& local2tex ){
	Texdef_toTransform( texdef, (float)width, (float)height, local2tex );
	{
		if( direction ){ //arbitrary
			Matrix4 basis = g_matrix4_identity;
			ComputeAxisBase( *direction, vector4_to_vector3( basis.x() ), vector4_to_vector3( basis.y() ) );
			vector4_to_vector3( basis.z() ) = *direction;
			matrix4_transpose( basis );

			matrix4_multiply_by_matrix4( local2tex, basis );
		}
		else{ //AP
			Matrix4 xyz2st;
			Normal_GetTransform( normal, xyz2st ); //Texdef_basisForNormal for AP
			matrix4_multiply_by_matrix4( local2tex, xyz2st );
		}
	}
}

inline void BPTexdef_fromST011( TextureProjection& projection, const Plane3& plane, const Matrix4& local2tex ){
	DoubleVector3 texX, texY;
	ComputeAxisBase( plane.normal(), texX, texY );

	// (0,0) in plane axis base is (0,0,0) in world coordinates + projection on the affine plane
	// (1,0) in plane axis base is texX in world coordinates + projection on the affine plane
	// (0,1) in plane axis base is texY in world coordinates + projection on the affine plane
	// use old texture code to compute the ST coords of these points
	// ST of (0,0) (1,0) (0,1)
	const DoubleVector3 anchor = plane.normal() * plane.dist();
	const DoubleVector3 points[3] = { anchor, anchor + texX, anchor + texY };
	DoubleVector3 st[3];

	for ( std::size_t i = 0; i < 3; ++i ){
		st[i] = matrix4_transformed_point( local2tex, points[i] );
		//globalOutputStream() << st[i] << "\n";
	}
	// compute texture matrix
	projection.m_brushprimit_texdef.coords[0][2] = float_mod( st[0][0], 1.0 );
	projection.m_brushprimit_texdef.coords[1][2] = float_mod( st[0][1], 1.0 );
	projection.m_brushprimit_texdef.coords[0][0] = st[1][0] - st[0][0];
	projection.m_brushprimit_texdef.coords[1][0] = st[1][1] - st[0][1];
	projection.m_brushprimit_texdef.coords[0][1] = st[2][0] - st[0][0];
	projection.m_brushprimit_texdef.coords[1][1] = st[2][1] - st[0][1];
}

void Texdef_ProjectTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const texdef_t& texdef, const Vector3* direction ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		//texdef_t texdef;
		//Q3_to_BP( texdef, (float)width, (float)height, normal, projection.m_brushprimit_texdef );

		Matrix4 local2tex;
		Texdef_Construct_local2tex4projection( texdef, width, height, plane.normal(), direction, local2tex );
		BPTexdef_fromST011( projection, plane, local2tex );
	}
	else if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
		Texdef_Assign( projection.m_texdef, texdef );
		if( direction ){ //arbitrary
			ComputeAxisBase( *direction, projection.m_basis_s, projection.m_basis_t );
		}
		else{ //AP
			Matrix4 basis;
			Normal_GetTransform( plane.normal(), basis );
			projection.m_basis_s = Vector3( basis.xx(), basis.yx(), basis.zx() );
			projection.m_basis_t = Vector3( -basis.xy(), -basis.yy(), -basis.zy() );
		}
		Valve220_rotate( projection, texdef.rotate );
	}
}

void Texdef_ProjectTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, TextureProjection other_proj, const Vector3& other_normal ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		other_proj.m_brushprimit_texdef.addScale( width, height );

		Matrix4 local2tex;
		Texdef_Construct_local2tex( other_proj, width, height, other_normal, local2tex );
		BPTexdef_fromST011( projection, plane, local2tex );
	}
	else
	{
		Texdef_Assign( projection.m_texdef, other_proj.m_texdef );
		if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ) {
			projection.m_basis_s = other_proj.m_basis_s;
			projection.m_basis_t = other_proj.m_basis_t;
		}
	}
}

inline bool BP_degenerate( const brushprimit_texdef_t& bp ){
	return vector2_cross( Vector2( bp.coords[0][0], bp.coords[0][1] ),
							Vector2( bp.coords[1][0], bp.coords[1][1] ) ) == 0
							|| bp.coords[0][0] != bp.coords[0][0];
}

/// g_bp_globals.m_texdefTypeId must be == TEXDEFTYPEID_BRUSHPRIMITIVES during this
void AP_from_BP( TextureProjection& projection, const Plane3& plane, std::size_t width, std::size_t height ) {
	/* catch degenerate BP basis, go default if so */
	if( BP_degenerate( projection.m_brushprimit_texdef ) ){
		projection.m_texdef.scale[0] = Texdef_getDefaultTextureScale();
		projection.m_texdef.scale[1] = Texdef_getDefaultTextureScale();
		return;
	}

	const Vector3 invariant( static_cast<Vector3>( plane.normal() * plane.dist() ) );

	Matrix4 local2tex;
	Texdef_Construct_local2tex( projection, width, height, plane.normal(), local2tex );
	const Vector3 st = matrix4_transformed_point( local2tex, invariant );
	const Vector2 invariantTexCoords( st[0] * width, st[1] * height );
//		globalOutputStream() << "local2tex: " << local2tex << "\n";
//		globalOutputStream() << "invariantTexCoords: " << invariantTexCoords[0] << " " << invariantTexCoords[1] << "\n";

	const Matrix4 tex2local = matrix4_affine_inverse( local2tex );
	AP_from_axes( vector4_to_vector3( tex2local.x() ) / width, vector4_to_vector3( tex2local.y() ) / height, plane.normal(), width, height, invariant, invariantTexCoords, projection.m_texdef );
}

void Valve220_from_BP( TextureProjection& projection, const Plane3& plane, std::size_t width, std::size_t height ) {
//		printBP( projection );
#if 0
	projection.m_texdef.scale[0] = 1.0 / ( vector2_length( Vector2( projection.m_brushprimit_texdef.coords[0][0], projection.m_brushprimit_texdef.coords[0][1] ) ) * (double)width );
	projection.m_texdef.scale[1] = 1.0 / ( vector2_length( Vector2( projection.m_brushprimit_texdef.coords[1][0], projection.m_brushprimit_texdef.coords[1][1] ) ) * (double)height );
	projection.m_texdef.shift[0] = projection.m_brushprimit_texdef.coords[0][2] * (float)width;
	projection.m_texdef.shift[1] = projection.m_brushprimit_texdef.coords[1][2] * (float)height;
	projection.m_texdef.rotate = static_cast<float>( -radians_to_degrees( arctangent_yx( projection.m_brushprimit_texdef.coords[0][1], projection.m_brushprimit_texdef.coords[0][0] ) ) );
	if( projection.m_brushprimit_texdef.coords[0][0] * projection.m_brushprimit_texdef.coords[1][1] < 0 )
		projection.m_texdef.rotate = -projection.m_texdef.rotate;

	DoubleVector3 texX, texY;
	ComputeAxisBase( plane.normal(), texX, texY );
	projection.m_basis_s = vector3_normalised( texX * static_cast<double>( projection.m_brushprimit_texdef.coords[0][0] ) + texY * static_cast<double>( projection.m_brushprimit_texdef.coords[0][1] ) );
	projection.m_basis_t = vector3_normalised( texX * static_cast<double>( projection.m_brushprimit_texdef.coords[1][0] ) + texY * static_cast<double>( projection.m_brushprimit_texdef.coords[1][1] ) );
#else
	/* more reliable values this way */
	DoubleVector3 texX, texY;
	ComputeAxisBase( plane.normal(), texX, texY );
	/* catch degenerate BP basis, go default if so */
	if( BP_degenerate( projection.m_brushprimit_texdef ) ){
		projection.m_basis_s = texX;
		projection.m_basis_t = texY;
		projection.m_texdef.scale[0] = Texdef_getDefaultTextureScale();
		projection.m_texdef.scale[1] = Texdef_getDefaultTextureScale();
		return;
	}
	projection.m_basis_s = vector3_normalised( texX * static_cast<double>( projection.m_brushprimit_texdef.coords[0][0] ) + texY * static_cast<double>( projection.m_brushprimit_texdef.coords[0][1] ) );
	projection.m_basis_t = vector3_normalised( texX * static_cast<double>( projection.m_brushprimit_texdef.coords[1][0] ) + texY * static_cast<double>( projection.m_brushprimit_texdef.coords[1][1] ) );
	projection.m_brushprimit_texdef.removeScale( width, height );
	TexMatToFakeTexCoords( projection.m_brushprimit_texdef, projection.m_texdef );
	projection.m_texdef.shift[0] *= -1.f;
	if( projection.m_brushprimit_texdef.coords[0][0] < 0 )
		projection.m_basis_s *= -1.f;
	if( projection.m_brushprimit_texdef.coords[1][1] < 0 )
		projection.m_basis_t *= -1.f;
	projection.m_brushprimit_texdef.addScale( width, height );
#endif
//		print220( projection );
}

/// g_bp_globals.m_texdefTypeId == 'in' during this
void Texdef_Convert( TexdefTypeId in, TexdefTypeId out, const Plane3& plane, TextureProjection& projection, std::size_t width, std::size_t height ) {
	switch( out ) {
	case TEXDEFTYPEID_QUAKE: {
		if( in == TEXDEFTYPEID_VALVE ){
			Matrix4 local2tex;
			Texdef_Construct_local2tex( projection, width, height, plane.normal(), local2tex );
			BPTexdef_fromST011( projection, plane, local2tex );

			const TexdefTypeId tmp = g_bp_globals.m_texdefTypeId;
			g_bp_globals.m_texdefTypeId = TEXDEFTYPEID_BRUSHPRIMITIVES;
			AP_from_BP( projection, plane, width, height );
			g_bp_globals.m_texdefTypeId = tmp;
		}
		else if( in == TEXDEFTYPEID_BRUSHPRIMITIVES ){
			AP_from_BP( projection, plane, width, height );
		}
	}
	break;
	case TEXDEFTYPEID_BRUSHPRIMITIVES:
	{
		Matrix4 local2tex;
		Texdef_Construct_local2tex( projection, width, height, plane.normal(), local2tex );
		BPTexdef_fromST011( projection, plane, local2tex );
	}
	break;
	case TEXDEFTYPEID_VALVE:
		if( in == TEXDEFTYPEID_QUAKE ) {
			Matrix4 basis;
			Normal_GetTransform( plane.normal(), basis );
			projection.m_basis_s = Vector3( basis.xx(), basis.yx(), basis.zx() );
			projection.m_basis_t = Vector3( -basis.xy(), -basis.yy(), -basis.zy() );
			Valve220_rotate( projection, projection.m_texdef.rotate );
		}
		else if( in == TEXDEFTYPEID_BRUSHPRIMITIVES ){
			Valve220_from_BP( projection, plane, width, height );
		}
		break;
	default:
		break;
	}
}

void Texdef_from_ST( TextureProjection& projection, const DoubleVector3 points[3], const DoubleVector3 st[3], std::size_t width, std::size_t height ){
	const Plane3 plane( plane3_for_points( points ) );
	brushprimit_texdef_t bp;
	BP_from_ST( bp, points, st, plane.normal() );
	if( BP_degenerate( bp ) )
		return;
	else
		projection.m_brushprimit_texdef = bp;
	if( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_QUAKE ){
		const TexdefTypeId tmp = g_bp_globals.m_texdefTypeId;
		g_bp_globals.m_texdefTypeId = TEXDEFTYPEID_BRUSHPRIMITIVES;
		AP_from_BP( projection, plane, width, height );
		g_bp_globals.m_texdefTypeId = tmp;
	}
	else if( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_VALVE ){
		Valve220_from_BP( projection, plane, width, height );
	}
}

#if 0
void Texdef_getTexAxes( const TextureProjection& projection, const Plane3& plane, std::size_t width, std::size_t height, Matrix4& local2tex, Matrix4& tex2local, Matrix4& basis ){
	Texdef_Construct_local2tex( projection, width, height, plane.normal(), local2tex );
	basis = matrix4_affine_inverse( local2tex ); //natural texture basis in world space

	TextureProjection proj( projection );
	if( g_bp_globals.m_texdefTypeId != TEXDEFTYPEID_BRUSHPRIMITIVES ){
		BPTexdef_fromST011( proj, plane, local2tex );
	}

	// rest is equal to inverse( BP local2tex ), but hopefully has more precision
	BPTexdef_toTransform( proj.m_brushprimit_texdef, local2tex );
	tex2local = matrix4_affine_inverse( local2tex );

	//Texdef_basisForNormal( proj, plane.normal(), xyz2st ); minus inverse of orthogonal basis via transpose
	Matrix4 xyz2st = g_matrix4_identity;
	ComputeAxisBase( plane.normal(), vector4_to_vector3( xyz2st.x() ), vector4_to_vector3( xyz2st.y() ) );
	vector4_to_vector3( xyz2st.z() ) = plane.normal();

	// natural texture basis, aligned to the plane
	matrix4_premultiply_by_matrix4( tex2local, xyz2st ); // ( A B )-1 = B-1 A-1

	// return BP local2tex to have STs range according to tex2local
	matrix4_multiply_by_matrix4( local2tex, matrix4_transposed( xyz2st ) );
}
#endif

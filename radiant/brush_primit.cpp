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
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_toTransform( projection.m_brushprimit_texdef, transform );
	}
	else
	{
		Texdef_toTransform( projection.m_texdef, width, height, transform );
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
}

inline void BPTexdef_fromTransform( brushprimit_texdef_t& bp_texdef, const Matrix4& transform ){
	bp_texdef.coords[0][0] = transform.xx();
	bp_texdef.coords[0][1] = transform.yx();
	bp_texdef.coords[0][2] = transform.tx();
	bp_texdef.coords[1][0] = transform.xy();
	bp_texdef.coords[1][1] = transform.yy();
	bp_texdef.coords[1][2] = transform.ty();
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
}

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

void ComputeAxisBase( const Vector3& normal, Vector3& texS, Vector3& texT );

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
	}
	else if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_HALFLIFE ) {
		basis = g_matrix4_identity;
		vector4_to_vector3( basis.x() ) = projection.m_basis_s;
		vector4_to_vector3( basis.y() ) = vector3_negated( projection.m_basis_t );
		vector4_to_vector3( basis.z() ) = vector3_normalised( vector3_cross( vector4_to_vector3( basis.x() ), vector4_to_vector3( basis.y() ) ) );
		matrix4_multiply_by_matrix4( basis, matrix4_rotation_for_z_degrees( -projection.m_texdef.rotate ) );
		matrix4_transpose( basis );
	}
	else
	{
		Normal_GetTransform( normal, basis );
	}
}

void Texdef_EmitTextureCoordinates( const TextureProjection& projection, std::size_t width, std::size_t height, Winding& w, const Vector3& normal, const Matrix4& localToWorld ){
	if ( w.numpoints < 3 ) {
		return;
	}

	Matrix4 local2tex;
	Texdef_toTransform( projection, (float)width, (float)height, local2tex );

	{
		Matrix4 xyz2st;
		// we don't care if it's not normalised...
		Texdef_basisForNormal( projection, matrix4_transformed_direction( localToWorld, normal ), xyz2st );
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

void Texdef_Shift( texdef_t& td, float s, float t ){
	td.shift[0] += s;
	td.shift[1] += t;
}

void Texdef_Scale( texdef_t& td, float s, float t ){
	td.scale[0] += s;
	td.scale[1] += t;
}

void Texdef_Rotate( texdef_t& td, float angle ){
	td.rotate += angle;
	td.rotate = static_cast<float>( float_to_integer( td.rotate ) % 360 );
}

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

template<typename Element>
inline BasicVector3<Element> vector3_inverse( const BasicVector3<Element>& self ){
	return BasicVector3<Element>(
			   Element( 1.0 / self.x() ),
			   Element( 1.0 / self.y() ),
			   Element( 1.0 / self.z() )
			   );
}

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



bp_globals_t g_bp_globals;
float g_texdef_default_scale;

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
	float det;
	M[2][0] = 1.0f; M[2][1] = 1.0f; M[2][2] = 1.0f;

	// solve
	det = SarrusDet( M[0], M[1], M[2] );
	T->coords[0][0] = SarrusDet( D[0], M[1], M[2] ) / det;
	T->coords[0][1] = SarrusDet( M[0], D[0], M[2] ) / det;
	T->coords[0][2] = SarrusDet( M[0], M[1], D[0] ) / det;
	T->coords[1][0] = SarrusDet( D[1], M[1], M[2] ) / det;
	T->coords[1][1] = SarrusDet( M[0], D[1], M[2] ) / det;
	T->coords[1][2] = SarrusDet( M[0], M[1], D[1] ) / det;
}

//++timo replace everywhere texX by texS etc. ( ----> and in q3map !)
// NOTE : ComputeAxisBase here and in q3map code must always BE THE SAME !
// WARNING : special case behaviour of atan2(y,x) <-> atan(y/x) might not be the same everywhere when x == 0
// rotation by (0,RotY,RotZ) assigns X to normal
void ComputeAxisBase( const Vector3& normal, Vector3& texS, Vector3& texT ){
	const Vector3 up( 0, 0, 1 );
	const Vector3 down( 0, 0, -1 );

	if ( vector3_equal_epsilon( normal, up, float(1e-6) ) ) {
		texS = Vector3( 0, 1, 0 );
		texT = Vector3( 1, 0, 0 );
	}
	else if ( vector3_equal_epsilon( normal, down, float(1e-6) ) ) {
		texS = Vector3( 0, 1, 0 );
		texT = Vector3( -1, 0, 0 );
	}
	else
	{
		texS = vector3_normalised( vector3_cross( normal, up ) );
		texT = vector3_normalised( vector3_cross( normal, texS ) );
		vector3_negate( texS );
	}
}

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

// compute a fake shift scale rot representation from the texture matrix
// these shift scale rot values are to be understood in the local axis base
// Note: this code looks similar to Texdef_fromTransform, but the algorithm is slightly different.

void TexMatToFakeTexCoords( const brushprimit_texdef_t& bp_texdef, texdef_t& texdef ){
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
}

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

// don't do C==A!
void BPMatMul( float A[2][3], float B[2][3], float C[2][3] ){
	C[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0];
	C[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0];
	C[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1];
	C[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1];
	C[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2];
	C[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2];
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
	// apply same scale as the spinner button of the surface inspector
	texdef_t texdef;
	// compute fake shift scale rot
	TexMatToFakeTexCoords( bp_td, texdef );
	// update
	texdef.scale[0] += s;
	texdef.scale[1] += t;
	// compute new normalized texture matrix
	FakeTexCoordsToTexMat( texdef, bp_td );
}

void BPTexdef_Rotate( brushprimit_texdef_t& bp_td, float angle ){
	// apply same scale as the spinner button of the surface inspector
	texdef_t texdef;
	// compute fake shift scale rot
	TexMatToFakeTexCoords( bp_td, texdef );
	// update
	texdef.rotate += angle;
	// compute new normalized texture matrix
	FakeTexCoordsToTexMat( texdef, bp_td );
}

void BPTexdef_Construct( brushprimit_texdef_t& bp_td, std::size_t width, std::size_t height ){
	bp_td.coords[0][0] = 1.0f;
	bp_td.coords[1][1] = 1.0f;
	ConvertTexMatWithDimensions( bp_td.coords, 2, 2, bp_td.coords, width, height );
}

void Texdef_Assign( TextureProjection& projection, const TextureProjection& other ){
	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_BRUSHPRIMITIVES ) {
		BPTexdef_Assign( projection.m_brushprimit_texdef, other.m_brushprimit_texdef );
	}
	else
	{
		Texdef_Assign( projection.m_texdef, other.m_texdef );
		if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_HALFLIFE ) {
			projection.m_basis_s = other.m_basis_s;
			projection.m_basis_t = other.m_basis_t;
		}
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
		Texdef_Rotate( projection.m_texdef, angle );
	}
}

void Texdef_FitTexture( TextureProjection& projection, std::size_t width, std::size_t height, const Vector3& normal, const Winding& w, float s_repeat, float t_repeat ){
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
	AABB perfect( Vector3( s_repeat * 0.5, t_repeat * 0.5, 0 ), Vector3( s_repeat * 0.5, t_repeat * 0.5, 1 ) );

	// the difference between the current texture transform and the perfectly fitted transform
	Matrix4 matrix( matrix4_translation_for_vec3( bounds.origin - perfect.origin ) );
	matrix4_pivoted_scale_by_vec3( matrix, bounds.extents / perfect.extents, perfect.origin );
	matrix4_affine_invert( matrix );

	// apply the difference to the current texture transform
	matrix4_premultiply_by_matrix4( st2tex, matrix );

	Texdef_fromTransform( projection, (float)width, (float)height, st2tex );
	Texdef_normalise( projection, (float)width, (float)height );
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

	if ( vector3_dot( from, plane.normal() ) > 0 == vector3_dot( to, plane.normal() ) > 0 ) {
		first = vector3_negated( first );
		second = vector3_negated( second );
	}


	Matrix4 swap = matrix4_swap_axes( first, second );

	Matrix4 tmp = matrix4_reflection_for_plane( plane );

	swap.tx() = -static_cast<float>( -2 * plane.a * plane.d );
	swap.ty() = -static_cast<float>( -2 * plane.b * plane.d );
	swap.tz() = -static_cast<float>( -2 * plane.c * plane.d );

	return swap;
}

void Texdef_transformLocked( TextureProjection& projection, std::size_t width, std::size_t height, const Plane3& plane, const Matrix4& identity2transformed ){


	Vector3 normalTransformed( matrix4_transformed_direction( identity2transformed, plane.normal() ) );


	// identity: identity space
	// transformed: transformation
	// stIdentity: base st projection space before transformation
	// stTransformed: base st projection space after transformation
	// stOriginal: original texdef space

	// stTransformed2stOriginal = stTransformed -> transformed -> identity -> stIdentity -> stOriginal

	Matrix4 identity2stIdentity;
	Texdef_basisForNormal( projection, plane.normal(), identity2stIdentity );

	if ( g_bp_globals.m_texdefTypeId == TEXDEFTYPEID_HALFLIFE ) {
		matrix4_transform_direction( identity2transformed, projection.m_basis_s );
		matrix4_transform_direction( identity2transformed, projection.m_basis_t );
	}

	Matrix4 transformed2stTransformed;
	Texdef_basisForNormal( projection, normalTransformed, transformed2stTransformed );

	Matrix4 stTransformed2identity( matrix4_affine_inverse( matrix4_multiplied_by_matrix4( transformed2stTransformed, identity2transformed ) ) );

	Vector3 originalProjectionAxis( vector4_to_vector3( matrix4_affine_inverse( identity2stIdentity ).z() ) );

	Vector3 transformedProjectionAxis( vector4_to_vector3( stTransformed2identity.z() ) );

	Matrix4 stIdentity2stOriginal;
	Texdef_toTransform( projection, (float)width, (float)height, stIdentity2stOriginal );
	Matrix4 identity2stOriginal( matrix4_multiplied_by_matrix4( stIdentity2stOriginal, identity2stIdentity ) );

	double dot = vector3_dot( originalProjectionAxis, transformedProjectionAxis );
	if ( dot == 0 ) {
		// The projection axis chosen for the transformed normal is at 90 degrees
		// to the transformed projection axis chosen for the original normal.
		// This happens when the projection axis is ambiguous - e.g. for the plane
		// 'X == Y' the projection axis could be either X or Y.

		Matrix4 identityCorrected = matrix4_reflection_for_plane45( plane, originalProjectionAxis, transformedProjectionAxis );

		identity2stOriginal = matrix4_multiplied_by_matrix4( identity2stOriginal, identityCorrected );
	}

	Matrix4 stTransformed2stOriginal = matrix4_multiplied_by_matrix4( identity2stOriginal, stTransformed2identity );

	Texdef_fromTransform( projection, (float)width, (float)height, stTransformed2stOriginal );
	Texdef_normalise( projection, (float)width, (float)height );
}

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

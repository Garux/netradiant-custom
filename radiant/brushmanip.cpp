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

#include "brushmanip.h"


#include "gtkutil/widget.h"
#include "gtkutil/menu.h"
#include "gtkmisc.h"
#include "brushnode.h"
#include "map.h"
#include "texwindow.h"
#include "gtkdlgs.h"
#include "commands.h"
#include "mainframe.h"
#include "dialog.h"
#include "xywindow.h"
#include "preferences.h"

#include <list>

void Brush_ConstructCuboid( Brush& brush, const AABB& bounds, const char* shader, const TextureProjection& projection ){
	const unsigned char box[3][2] = { { 0, 1 }, { 2, 0 }, { 1, 2 } };
	Vector3 mins( vector3_subtracted( bounds.origin, bounds.extents ) );
	Vector3 maxs( vector3_added( bounds.origin, bounds.extents ) );

	for( std::size_t i = 0; i < 3; ++i ){
		if( mins[i] < g_MinWorldCoord )
			mins[i] = g_MinWorldCoord;
		if( maxs[i] > g_MaxWorldCoord )
			maxs[i] = g_MaxWorldCoord;
	}

	brush.clear();
	brush.reserve( 6 );

	{
		for ( int i = 0; i < 3; ++i )
		{
			Vector3 planepts1( maxs );
			Vector3 planepts2( maxs );
			planepts2[box[i][0]] = mins[box[i][0]];
			planepts1[box[i][1]] = mins[box[i][1]];

			brush.addPlane( maxs, planepts1, planepts2, shader, projection );
		}
	}
	{
		for ( int i = 0; i < 3; ++i )
		{
			Vector3 planepts1( mins );
			Vector3 planepts2( mins );
			planepts1[box[i][0]] = maxs[box[i][0]];
			planepts2[box[i][1]] = maxs[box[i][1]];

			brush.addPlane( mins, planepts1, planepts2, shader, projection );
		}
	}
}

inline float max_extent( const Vector3& extents ){
	return std::max( std::max( extents[0], extents[1] ), extents[2] );
}

inline float max_extent_2d( const Vector3& extents, size_t axis ){
	switch ( axis )
	{
	case 0:
		return std::max( extents[1], extents[2] );
	case 1:
		return std::max( extents[0], extents[2] );
	default:
		return std::max( extents[0], extents[1] );
	}
}

const std::size_t c_brushPrism_minSides = 3;
const std::size_t c_brushPrism_maxSides = c_brush_maxFaces - 2;
const char* const c_brushPrism_name = "brushPrism";

void Brush_ConstructPrism( Brush& brush, const AABB& bounds, std::size_t sides, size_t axis, const char* shader, const TextureProjection& projection ){
	if ( sides < c_brushPrism_minSides ) {
		globalErrorStream() << c_brushPrism_name << ": sides " << sides << ": too few sides, minimum is " << c_brushPrism_minSides << '\n';
		return;
	}
	if ( sides > c_brushPrism_maxSides ) {
		globalErrorStream() << c_brushPrism_name << ": sides " << sides << ": too many sides, maximum is " << c_brushPrism_maxSides << '\n';
		return;
	}

	brush.clear();
	brush.reserve( sides + 2 );

	const Vector3 mins( bounds.origin - bounds.extents );
	const Vector3 maxs( bounds.origin + bounds.extents );

	const float radius = max_extent_2d( bounds.extents, axis );
	const Vector3& mid = bounds.origin;
	const size_t x = ( axis + 1 ) % 3, y = ( axis + 2 ) % 3, z = axis;
	Vector3 planepts[3];

	planepts[2][x] = mins[x];
	planepts[2][y] = mins[y];
	planepts[2][z] = maxs[z];
	planepts[1][x] = maxs[x];
	planepts[1][y] = mins[y];
	planepts[1][z] = maxs[z];
	planepts[0][x] = maxs[x];
	planepts[0][y] = maxs[y];
	planepts[0][z] = maxs[z];

	brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );

	planepts[0][x] = mins[x];
	planepts[0][y] = mins[y];
	planepts[0][z] = mins[z];
	planepts[1][x] = maxs[x];
	planepts[1][y] = mins[y];
	planepts[1][z] = mins[z];
	planepts[2][x] = maxs[x];
	planepts[2][y] = maxs[y];
	planepts[2][z] = mins[z];

	brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );

	for ( std::size_t i = 0; i < sides; ++i )
	{
		const double sv = sin( i * c_2pi / sides );
		const double cv = cos( i * c_2pi / sides );

		planepts[0][x] = mid[x] + radius * cv;
		planepts[0][y] = mid[y] + radius * sv;
		planepts[0][z] = mins[z];

		planepts[1][x] = planepts[0][x];
		planepts[1][y] = planepts[0][y];
		planepts[1][z] = maxs[z];

		planepts[2][x] = planepts[0][x] - radius * sv;
		planepts[2][y] = planepts[0][y] + radius * cv;
		planepts[2][z] = maxs[z];

		brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );
	}
}

const std::size_t c_brushCone_minSides = 3;
const std::size_t c_brushCone_maxSides = c_brush_maxFaces - 1;
const char* const c_brushCone_name = "brushCone";

void Brush_ConstructCone( Brush& brush, const AABB& bounds, std::size_t sides, size_t axis, const char* shader, const TextureProjection& projection ){
	if ( sides < c_brushCone_minSides ) {
		globalErrorStream() << c_brushCone_name << ": sides " << sides << ": too few sides, minimum is " << c_brushCone_minSides << '\n';
		return;
	}
	if ( sides > c_brushCone_maxSides ) {
		globalErrorStream() << c_brushCone_name << ": sides " << sides << ": too many sides, maximum is " << c_brushCone_maxSides << '\n';
		return;
	}

	brush.clear();
	brush.reserve( sides + 1 );

	const Vector3 mins( bounds.origin - bounds.extents );
	const Vector3 maxs( bounds.origin + bounds.extents );

	const float radius = max_extent_2d( bounds.extents, axis );
	const Vector3& mid = bounds.origin;
	const size_t x = ( axis + 1 ) % 3, y = ( axis + 2 ) % 3, z = axis;
	Vector3 planepts[3];

	planepts[0][x] = mins[x]; planepts[0][y] = mins[y]; planepts[0][z] = mins[z];
	planepts[1][x] = maxs[x]; planepts[1][y] = mins[y]; planepts[1][z] = mins[z];
	planepts[2][x] = maxs[x]; planepts[2][y] = maxs[y]; planepts[2][z] = mins[z];

	brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );

	for ( std::size_t i = 0; i < sides; ++i )
	{
		const double sv = sin( i * c_2pi / sides );
		const double cv = cos( i * c_2pi / sides );

		planepts[0][x] = mid[x] + radius * cv;
		planepts[0][y] = mid[y] + radius * sv;
		planepts[0][z] = mins[z];

		planepts[1][x] = mid[x];
		planepts[1][y] = mid[y];
		planepts[1][z] = maxs[z];

		planepts[2][x] = planepts[0][x] - radius * sv;
		planepts[2][y] = planepts[0][y] + radius * cv;
		planepts[2][z] = mins[z];

		brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );
	}
}

const std::size_t c_brushSphere_minSides = 3;
const std::size_t c_brushSphere_maxSides = 31;
const char* const c_brushSphere_name = "brushSphere";

void Brush_ConstructSphere( Brush& brush, const AABB& bounds, std::size_t sides, const char* shader, const TextureProjection& projection ){
	if ( sides < c_brushSphere_minSides ) {
		globalErrorStream() << c_brushSphere_name << ": sides " << sides << ": too few sides, minimum is " << c_brushSphere_minSides << '\n';
		return;
	}
	if ( sides > c_brushSphere_maxSides ) {
		globalErrorStream() << c_brushSphere_name << ": sides " << sides << ": too many sides, maximum is " << c_brushSphere_maxSides << '\n';
		return;
	}

	brush.clear();
	brush.reserve( sides * sides );

	float radius = max_extent( bounds.extents );
	const Vector3& mid = bounds.origin;
	Vector3 planepts[3];

	double dt = 2 * c_pi / sides;
	double dp = c_pi / sides;
	for ( std::size_t i = 0; i < sides; i++ )
	{
		for ( std::size_t j = 0; j < sides - 1; j++ )
		{
			double t = i * dt;
			double p = float( j * dp - c_pi / 2 );

			planepts[0] = vector3_added( mid, vector3_scaled( vector3_for_spherical( t, p ), radius ) );
			planepts[1] = vector3_added( mid, vector3_scaled( vector3_for_spherical( t, p + dp ), radius ) );
			planepts[2] = vector3_added( mid, vector3_scaled( vector3_for_spherical( t + dt, p + dp ), radius ) );

			brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );
		}
	}

	{
		double p = ( sides - 1 ) * dp - c_pi / 2;
		for ( std::size_t i = 0; i < sides; i++ )
		{
			double t = i * dt;

			planepts[0] = vector3_added( mid, vector3_scaled( vector3_for_spherical( t, p ), radius ) );
			planepts[1] = vector3_added( mid, vector3_scaled( vector3_for_spherical( t + dt, p + dp ), radius ) );
			planepts[2] = vector3_added( mid, vector3_scaled( vector3_for_spherical( t + dt, p ), radius ) );

			brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );
		}
	}
}

const std::size_t c_brushRock_minSides = 10;
const std::size_t c_brushRock_maxSides = 1000;
const char* const c_brushRock_name = "brushRock";

void Brush_ConstructRock( Brush& brush, const AABB& bounds, std::size_t sides, const char* shader, const TextureProjection& projection ){
	if ( sides < c_brushRock_minSides ) {
		globalErrorStream() << c_brushRock_name << ": sides " << sides << ": too few sides, minimum is " << c_brushRock_minSides << '\n';
		return;
	}
	if ( sides > c_brushRock_maxSides ) {
		globalErrorStream() << c_brushRock_name << ": sides " << sides << ": too many sides, maximum is " << c_brushRock_maxSides << '\n';
		return;
	}

	brush.clear();
	brush.reserve( sides * sides );

	float radius = max_extent( bounds.extents );
	const Vector3& mid = bounds.origin;
	Vector3 planepts[3];

	for ( std::size_t j = 0; j < sides; j++ )
	{
		planepts[0][0] = rand() - ( RAND_MAX / 2 );
		planepts[0][1] = rand() - ( RAND_MAX / 2 );
		planepts[0][2] = rand() - ( RAND_MAX / 2 );
		vector3_normalise( planepts[0] );

		// find two vectors that are perpendicular to planepts[0]
		ComputeAxisBase( planepts[0], planepts[1], planepts[2] );

		planepts[0] = vector3_added( mid, vector3_scaled( planepts[0], radius ) );
		planepts[1] = vector3_added( planepts[0], vector3_scaled( planepts[1], radius ) );
		planepts[2] = vector3_added( planepts[0], vector3_scaled( planepts[2], radius ) );

#if 0
		// make sure the orientation is right
		if ( vector3_dot( vector3_subtracted( planepts[0], mid ), vector3_cross( vector3_subtracted( planepts[1], mid ), vector3_subtracted( planepts[2], mid ) ) ) > 0 ) {
			Vector3 h;
			h = planepts[1];
			planepts[1] = planepts[2];
			planepts[2] = h;
			globalOutputStream() << "flip\n";
		}
		else{
			globalOutputStream() << "no flip\n";
		}
#endif

		brush.addPlane( planepts[0], planepts[1], planepts[2], shader, projection );
	}
}

#include "quickhull/QuickHull.hpp"
namespace icosahedron{

constexpr double X = .525731112119133606;
constexpr double Z = .850650808352039932;
// 12 vertices
static const DoubleVector3 vdata[12] = {
	{ -X, 0.0, Z}, {X, 0.0, Z}, { -X, 0.0, -Z}, {X, 0.0, -Z},
	{0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},
	{Z, X, 0.0}, { -Z, X, 0.0}, {Z, -X, 0.0}, { -Z, -X, 0.0}
};
// 20 faces
static constexpr unsigned int tindices[20][3] = {
	{0, 4, 1}, {0, 9, 4}, {9, 5, 4}, {4, 5, 8}, {4, 8, 1},
	{8, 10, 1}, {8, 3, 10}, {5, 3, 8}, {5, 2, 3}, {2, 7, 3},
	{7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
	{6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5}, {7, 2, 11}
};

void drawtri( const DoubleVector3& a, const DoubleVector3& b, const DoubleVector3& c, std::size_t subdivisions, bool truncate, std::vector<quickhull::Vector3<double>>& pointCloud ) {
	if( subdivisions == 0 ) {
		const auto push = [&pointCloud]( const DoubleVector3& p ){
			pointCloud.push_back( quickhull::Vector3<double>( p.x(), p.y(), p.z() ) );
		};
		if( truncate ){ // this is not quite correct after subdivision performed :thinking:
			push( ( a * ( 2.0 / 3.0 ) ) + ( b * ( 1.0 / 3.0 ) ) );
			push( ( a * ( 1.0 / 3.0 ) ) + ( b * ( 2.0 / 3.0 ) ) );
			push( ( a * ( 2.0 / 3.0 ) ) + ( c * ( 1.0 / 3.0 ) ) );
			push( ( a * ( 1.0 / 3.0 ) ) + ( c * ( 2.0 / 3.0 ) ) );
			push( ( b * ( 2.0 / 3.0 ) ) + ( c * ( 1.0 / 3.0 ) ) );
			push( ( b * ( 1.0 / 3.0 ) ) + ( c * ( 2.0 / 3.0 ) ) );
		}
		else{
			push( a );
			push( b );
			push( c );
		}
	}
	else{
		const DoubleVector3 ab = vector3_normalised( a + b );
		const DoubleVector3 ac = vector3_normalised( a + c );
		const DoubleVector3 bc = vector3_normalised( b + c );

		drawtri( a, ab, ac, subdivisions - 1, truncate, pointCloud );
		drawtri( b, bc, ab, subdivisions - 1, truncate, pointCloud );
		drawtri( c, ac, bc, subdivisions - 1, truncate, pointCloud );
		drawtri( ab, bc, ac, subdivisions - 1, truncate, pointCloud ); //<--Comment this line and sphere looks really cool!
	}
}


void Brush_ConstructIcosahedron( Brush& brush, const AABB& bounds, std::size_t subdivisions, bool truncate, const char* shader, const TextureProjection& projection ){
	brush.clear();

	const float radius = max_extent( bounds.extents );
	const Vector3& mid = bounds.origin;

	std::vector<quickhull::Vector3<double>> pointCloud;

	for( int i = 0; i < 20; i++ ){
		drawtri( vdata[tindices[i][0]], vdata[tindices[i][1]], vdata[tindices[i][2]], subdivisions, truncate, pointCloud );
	}

	quickhull::QuickHull<double> quickhull;
	auto hull = quickhull.getConvexHull( pointCloud, true, true );
	const auto& indexBuffer = hull.getIndexBuffer();
	const size_t triangleCount = indexBuffer.size() / 3;
	std::vector<Plane3> planes;
	for( size_t i = 0; i < triangleCount; ++i ) {
		DoubleVector3 p[3];
		for( size_t j = 0; j < 3; ++j ){
			p[j] = DoubleVector3( pointCloud[indexBuffer[i * 3 + j]].x,
			                      pointCloud[indexBuffer[i * 3 + j]].y,
			                      pointCloud[indexBuffer[i * 3 + j]].z );
		}
		const Plane3 plane = plane3_for_points( p );
		if( plane3_valid( plane ) ){
			if( std::none_of( planes.begin(), planes.end(), [&plane]( const Plane3& pla ){ return plane3_equal( plane, pla ); } ) ){
				planes.push_back( plane );
				brush.addPlane( p[0] * radius + mid, p[1] * radius + mid, p[2] * radius + mid, shader, projection );
			}
		}
	}
}

} //namespace icosahedron

void Brush_ConstructPrefab( Brush& brush, EBrushPrefab type, const AABB& bounds, std::size_t sides, bool option, const char* shader, const TextureProjection& projection ){
	switch ( type )
	{
	case EBrushPrefab::Cuboid:
		{
			UndoableCommand undo( "brushCuboid" );

			Brush_ConstructCuboid( brush, bounds, shader, projection );
		}
		break;
	case EBrushPrefab::Prism:
		{
			const size_t axis = GlobalXYWnd_getCurrentViewType();
			const auto command = StringStream<64>( c_brushPrism_name, " -sides ", sides, " -axis ", axis );
			UndoableCommand undo( command );

			Brush_ConstructPrism( brush, bounds, sides, axis, shader, projection );
		}
		break;
	case EBrushPrefab::Cone:
		{
			const size_t axis = GlobalXYWnd_getCurrentViewType();
			const auto command = StringStream<64>( c_brushCone_name, " -sides ", sides, " -axis ", axis );
			UndoableCommand undo( command );

			Brush_ConstructCone( brush, bounds, sides, axis, shader, projection );
		}
		break;
	case EBrushPrefab::Sphere:
		{
			const auto command = StringStream<64>( c_brushSphere_name, " -sides ", sides );
			UndoableCommand undo( command );

			Brush_ConstructSphere( brush, bounds, sides, shader, projection );
		}
		break;
	case EBrushPrefab::Rock:
		{
			const auto command = StringStream<64>( c_brushRock_name, " -sides ", sides );
			UndoableCommand undo( command );

			Brush_ConstructRock( brush, bounds, sides, shader, projection );
		}
		break;
	case EBrushPrefab::Icosahedron:
		{
			const auto command = StringStream<64>( "brushIcosahedron", " -subdivisions ", sides );
			UndoableCommand undo( command );

			icosahedron::Brush_ConstructIcosahedron( brush, bounds, sides, option, shader, projection );
		}
		break;
	}
}


CopiedString g_regionBoxShader;

void ConstructRegionBrushes( scene::Node* brushes[6], const Vector3& region_mins, const Vector3& region_maxs ){
	const char *shader = g_regionBoxShader.empty()
	                     ? texdef_name_default()
	                     : texdef_name_valid( g_regionBoxShader.c_str() )
	                     ? g_regionBoxShader.c_str()
	                     : ( globalWarningStream() << "g_regionBoxShader " << makeQuoted( g_regionBoxShader ) << " !texdef_name_valid()\n"
	                     , texdef_name_default() );

	{
		// set mins
		const Vector3 mins( region_mins - Vector3( 32 ) );

		// vary maxs
		for ( std::size_t i = 0; i < 3; i++ )
		{
			Vector3 maxs( region_maxs + Vector3( 32 ) );
			maxs[i] = region_mins[i];
			Brush_ConstructCuboid( *Node_getBrush( *brushes[i] ), aabb_for_minmax( mins, maxs ), shader, TextureProjection() );
		}
	}

	{
		// set maxs
		const Vector3 maxs( region_maxs + Vector3( 32 ) );

		// vary mins
		for ( std::size_t i = 0; i < 3; i++ )
		{
			Vector3 mins( region_mins - Vector3( 32 ) );
			mins[i] = region_maxs[i];
			Brush_ConstructCuboid( *Node_getBrush( *brushes[i + 3] ), aabb_for_minmax( mins, maxs ), shader, TextureProjection() );
		}
	}
}


class FaceSetTexdef
{
	const TextureProjection& m_projection;
	const bool m_setBasis;
	const bool m_resetBasis;
public:
	FaceSetTexdef( const TextureProjection& projection, bool setBasis, bool resetBasis ) : m_projection( projection ), m_setBasis( setBasis ), m_resetBasis( resetBasis ){
	}
	void operator()( Face& face ) const {
		face.SetTexdef( m_projection, m_setBasis, m_resetBasis );
	}
};

void Scene_BrushSetTexdef_Selected( scene::Graph& graph, const TextureProjection& projection, bool setBasis, bool resetBasis ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceSetTexdef( projection, setBasis, resetBasis ) );
	SceneChangeNotify();
}

void Scene_BrushSetTexdef_Component_Selected( scene::Graph& graph, const TextureProjection& projection, bool setBasis, bool resetBasis ){
	Scene_ForEachSelectedBrushFace( graph, FaceSetTexdef( projection, setBasis, resetBasis ) );
	SceneChangeNotify();
}

class FaceSetTexdef_
{
	const float* m_hShift;
	const float* m_vShift;
	const float* m_hScale;
	const float* m_vScale;
	const float* m_rotation;
public:
	FaceSetTexdef_( const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ) :
		m_hShift( hShift ), m_vShift( vShift ), m_hScale( hScale ), m_vScale( vScale ), m_rotation( rotation ) {
	}
	void operator()( Face& face ) const {
		face.SetTexdef( m_hShift, m_vShift, m_hScale, m_vScale, m_rotation );
	}
};

void Scene_BrushSetTexdef_Selected( scene::Graph& graph, const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceSetTexdef_( hShift, vShift, hScale, vScale, rotation ) );
	SceneChangeNotify();
}

void Scene_BrushSetTexdef_Component_Selected( scene::Graph& graph, const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	Scene_ForEachSelectedBrushFace( graph, FaceSetTexdef_( hShift, vShift, hScale, vScale, rotation ) );
	SceneChangeNotify();
}


class FaceSetFlags
{
	const ContentsFlagsValue& m_flags;
public:
	FaceSetFlags( const ContentsFlagsValue& flags ) : m_flags( flags ){
	}
	void operator()( Face& face ) const {
		face.SetFlags( m_flags );
	}
};

void Scene_BrushSetFlags_Selected( scene::Graph& graph, const ContentsFlagsValue& flags ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceSetFlags( flags ) );
	SceneChangeNotify();
}

void Scene_BrushSetFlags_Component_Selected( scene::Graph& graph, const ContentsFlagsValue& flags ){
	Scene_ForEachSelectedBrushFace( graph, FaceSetFlags( flags ) );
	SceneChangeNotify();
}

class FaceShiftTexdef
{
	float m_s, m_t;
public:
	FaceShiftTexdef( float s, float t ) : m_s( s ), m_t( t ){
	}
	void operator()( Face& face ) const {
		face.ShiftTexdef( m_s, m_t );
	}
};

void Scene_BrushShiftTexdef_Selected( scene::Graph& graph, float s, float t ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceShiftTexdef( s, t ) );
	SceneChangeNotify();
}

void Scene_BrushShiftTexdef_Component_Selected( scene::Graph& graph, float s, float t ){
	Scene_ForEachSelectedBrushFace( graph, FaceShiftTexdef( s, t ) );
	SceneChangeNotify();
}

class FaceScaleTexdef
{
	float m_s, m_t;
public:
	FaceScaleTexdef( float s, float t ) : m_s( s ), m_t( t ){
	}
	void operator()( Face& face ) const {
		face.ScaleTexdef( m_s, m_t );
	}
};

void Scene_BrushScaleTexdef_Selected( scene::Graph& graph, float s, float t ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceScaleTexdef( s, t ) );
	SceneChangeNotify();
}

void Scene_BrushScaleTexdef_Component_Selected( scene::Graph& graph, float s, float t ){
	Scene_ForEachSelectedBrushFace( graph, FaceScaleTexdef( s, t ) );
	SceneChangeNotify();
}

class FaceRotateTexdef
{
	float m_angle;
public:
	FaceRotateTexdef( float angle ) : m_angle( angle ){
	}
	void operator()( Face& face ) const {
		face.RotateTexdef( m_angle );
	}
};

void Scene_BrushRotateTexdef_Selected( scene::Graph& graph, float angle ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceRotateTexdef( angle ) );
	SceneChangeNotify();
}

void Scene_BrushRotateTexdef_Component_Selected( scene::Graph& graph, float angle ){
	Scene_ForEachSelectedBrushFace( graph, FaceRotateTexdef( angle ) );
	SceneChangeNotify();
}


class FaceSetShader
{
	const char* m_name;
public:
	FaceSetShader( const char* name ) : m_name( name ) {}
	void operator()( Face& face ) const {
		face.SetShader( m_name );
	}
};

void Scene_BrushSetShader_Selected( scene::Graph& graph, const char* name ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceSetShader( name ) );
	SceneChangeNotify();
}

void Scene_BrushSetShader_Component_Selected( scene::Graph& graph, const char* name ){
	Scene_ForEachSelectedBrushFace( graph, FaceSetShader( name ) );
	SceneChangeNotify();
}

void Scene_BrushSetDetail_Selected( scene::Graph& graph, bool detail ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, [detail]( Face& face ){ face.setDetail( detail ); } );
	SceneChangeNotify();
}

bool Face_FindReplaceShader( Face& face, const char* find, const char* replace ){
	if ( shader_equal( face.GetShader(), find ) ) {
		face.SetShader( replace );
		return true;
	}
	return false;
}

class FaceFindReplaceShader
{
	const char* m_find;
	const char* m_replace;
public:
	FaceFindReplaceShader( const char* find, const char* replace ) : m_find( find ), m_replace( replace ){
	}
	void operator()( Face& face ) const {
		Face_FindReplaceShader( face, m_find, m_replace );
	}
};

class FaceSelectByShader
{
	const char* m_name;
public:
	FaceSelectByShader( const char* name )
		: m_name( name ){
	}
	void operator()( FaceInstance& face ) const {
		if ( shader_equal( face.getFace().GetShader(), m_name ) ) {
			face.setSelected( SelectionSystem::eFace, true );
		}
	}
};

void Scene_BrushFacesSelectByShader( scene::Graph& graph, const char* name ){
	Scene_ForEachBrush_ForEachFaceInstance( graph, FaceSelectByShader( name ) );
}

void Scene_BrushFindReplaceShader( scene::Graph& graph, const char* find, const char* replace ){
	if ( !replace ) {
		Scene_ForEachBrush_ForEachFaceInstance( graph, FaceSelectByShader( find ) );
	}
	else
	{
		Scene_ForEachBrush_ForEachFace( graph, FaceFindReplaceShader( find, replace ) );
	}
}

void Scene_BrushFindReplaceShader_Selected( scene::Graph& graph, const char* find, const char* replace ){
	if ( !replace ) {
		Scene_ForEachSelectedBrush_ForEachFaceInstance( graph, FaceSelectByShader( find ) );
	}
	else
	{
		Scene_ForEachSelectedBrush_ForEachFace( graph, FaceFindReplaceShader( find, replace ) );
	}
}

// TODO: find for components
// d1223m: dont even know what they are...
void Scene_BrushFindReplaceShader_Component_Selected( scene::Graph& graph, const char* find, const char* replace ){
	if ( !replace ) {
		Scene_ForEachSelectedBrush_ForEachFaceInstance( graph, FaceSelectByShader( find ) );
	}
	else
	{
		Scene_ForEachSelectedBrushFace( graph, FaceFindReplaceShader( find, replace ) );
	}
}


class FaceProjectTexture
{
	const texdef_t& m_texdef;
	const Vector3* m_direction;
public:
	FaceProjectTexture( const texdef_t& texdef, const Vector3* direction ) : m_texdef( texdef ), m_direction( direction ) {
	}
	void operator()( Face& face ) const {
		face.ProjectTexture( m_texdef, m_direction );
	}
};

void Scene_BrushProjectTexture_Selected( scene::Graph& graph, const texdef_t& texdef, const Vector3* direction ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceProjectTexture( texdef, direction ) );
	SceneChangeNotify();
}

void Scene_BrushProjectTexture_Component_Selected( scene::Graph& graph, const texdef_t& texdef, const Vector3* direction ){
	Scene_ForEachSelectedBrushFace( graph, FaceProjectTexture( texdef, direction ) );
	SceneChangeNotify();
}

class FaceProjectTexture_fromFace
{
	const TextureProjection& m_projection;
	const Vector3& m_normal;
public:
	FaceProjectTexture_fromFace( const TextureProjection& projection, const Vector3& normal ) : m_projection( projection ), m_normal( normal ) {
	}
	void operator()( Face& face ) const {
		face.ProjectTexture( m_projection, m_normal );
	}
};

void Scene_BrushProjectTexture_Selected( scene::Graph& graph, const TextureProjection& projection, const Vector3& normal ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceProjectTexture_fromFace( projection, normal ) );
	SceneChangeNotify();
}

void Scene_BrushProjectTexture_Component_Selected( scene::Graph& graph, const TextureProjection& projection, const Vector3& normal ){
	Scene_ForEachSelectedBrushFace( graph, FaceProjectTexture_fromFace( projection, normal ) );
	SceneChangeNotify();
}


class FaceFitTexture
{
	const float m_s_repeat, m_t_repeat;
	const bool m_only_dimension;
public:
	FaceFitTexture( float s_repeat, float t_repeat, bool only_dimension ) : m_s_repeat( s_repeat ), m_t_repeat( t_repeat ), m_only_dimension( only_dimension ) {
	}
	void operator()( Face& face ) const {
		face.FitTexture( m_s_repeat, m_t_repeat, m_only_dimension );
	}
};

void Scene_BrushFitTexture_Selected( scene::Graph& graph, float s_repeat, float t_repeat, bool only_dimension ){
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceFitTexture( s_repeat, t_repeat, only_dimension ) );
	SceneChangeNotify();
}

void Scene_BrushFitTexture_Component_Selected( scene::Graph& graph, float s_repeat, float t_repeat, bool only_dimension ){
	Scene_ForEachSelectedBrushFace( graph, FaceFitTexture( s_repeat, t_repeat, only_dimension ) );
	SceneChangeNotify();
}

TextureProjection g_defaultTextureProjection;
const TextureProjection& TextureTransform_getDefault(){
	TexDef_Construct_Default( g_defaultTextureProjection );
	return g_defaultTextureProjection;
}

void Scene_BrushConstructPrefab( scene::Graph& graph, EBrushPrefab type, std::size_t sides, bool option, const char* shader ){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		Brush* brush = Node_getBrush( path.top() );
		if ( brush != 0 ) {
			const AABB bounds = brush->localAABB(); // copy bounds because the brush will be modified
			Brush_ConstructPrefab( *brush, type, bounds, sides, option, shader, TextureTransform_getDefault() );
			SceneChangeNotify();
		}
	}
}

#include "filterbar.h"
extern bool g_brush_always_caulk;

void Scene_BrushResize_Cuboid( scene::Node*& node, const AABB& bounds ){
	if ( node == 0 ) {
		NodeSmartReference node_( GlobalBrushCreator().createBrush() );
		Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->insert( node_ );

		scene::Path brushpath( makeReference( GlobalSceneGraph().root() ) );
		brushpath.push( makeReference( *Map_GetWorldspawn( g_map ) ) );
		brushpath.push( makeReference( node_.get() ) );
		selectPath( brushpath, true );

		node = node_.get_pointer();
	}

	Brush* brush = Node_getBrush( *node );
	if ( brush != 0 ) {
		const char* shader = g_brush_always_caulk?
		                     GetCaulkShader()
		                     : TextureBrowser_GetSelectedShader();
		Brush_ConstructCuboid( *brush, bounds, shader, TextureTransform_getDefault() );
		SceneChangeNotify();
	}
}

void Brush_ConstructPlacehoderCuboid( scene::Node& node, const AABB& bounds ){
	scene::Node* brush = &GlobalBrushCreator().createBrush();
	Node_getTraversable( node )->insert( NodeSmartReference( *brush ) );
	Brush_ConstructCuboid( *Node_getBrush( *brush ), bounds, texdef_name_default(), TextureTransform_getDefault() );
}

bool Brush_hasShader( const Brush& brush, const char* name ){
	for ( Brush::const_iterator i = brush.begin(); i != brush.end(); ++i )
	{
		if ( shader_equal( ( *i )->GetShader(), name ) ) {
			return true;
		}
	}
	return false;
}

class BrushSelectByShaderWalker : public scene::Graph::Walker
{
	const char* m_name;
public:
	BrushSelectByShaderWalker( const char* name )
		: m_name( name ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.top().get().visible() ) {
			Brush* brush = Node_getBrush( path.top() );
			if ( brush != 0 && Brush_hasShader( *brush, m_name ) ) {
				Instance_getSelectable( instance )->setSelected( true );
			}
		}
		else{
			return false;
		}
		return true;
	}
};

void Scene_BrushSelectByShader( scene::Graph& graph, const char* name ){
	graph.traverse( BrushSelectByShaderWalker( name ) );
}

void Scene_BrushSelectByShader_Component( scene::Graph& graph, const char* name ){
	Scene_ForEachSelectedBrush_ForEachFaceInstance( graph, FaceSelectByShader( name ) );
}


void Scene_BrushGetTexdef_Selected( scene::Graph& graph, TextureProjection& projection ){
	bool done = false;
	Scene_ForEachSelectedBrush_ForEachFace( graph, [&]( Face& face ){
		if ( !done ) {
			done = true;
			face.GetTexdef( projection );
		}
	});
}

bool Scene_BrushGetShaderTexdef_Selected( scene::Graph& graph, CopiedString& shader, TextureProjection& projection ){
	BrushInstance *brush = nullptr;
	if ( GlobalSelectionSystem().countSelected() == 0
	|| !( brush = Instance_getBrush( GlobalSelectionSystem().ultimateSelected() ) ) ) {
		Scene_forEachSelectedBrush( [&brush]( BrushInstance& b ){ if( !brush ) brush = &b; } );
	}
	if( brush && !brush->getBrush().empty() ){
		Face *face = brush->getBrush().begin()->get();
		shader = face->GetShader();
		face->GetTexdef( projection );
		return true;
	}
	return false;
}

void Scene_BrushGetTexdef_Component_Selected( scene::Graph& graph, TextureProjection& projection ){
#if 1
	if ( !g_SelectedFaceInstances.empty() ) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetTexdef( projection );
	}
#else
	FaceGetTexdef visitor( projection );
	Scene_ForEachSelectedBrushFace( graph, visitor );
#endif
}


void Scene_BrushGetFlags_Selected( scene::Graph& graph, ContentsFlagsValue& flags ){
#if 1
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		if ( BrushInstance* brush = Instance_getBrush( GlobalSelectionSystem().ultimateSelected() ) ) {
			bool done = false;
			Brush_forEachFace( *brush, [&]( Face& face ){
				if ( !done ) {
					done = true;
					face.GetFlags( flags );
				}
			});
		}
	}
#else
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceGetFlags( flags ) );
#endif
}

void Scene_BrushGetFlags_Component_Selected( scene::Graph& graph, ContentsFlagsValue& flags ){
#if 1
	if ( !g_SelectedFaceInstances.empty() ) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetFlags( flags );
	}
#else
	Scene_ForEachSelectedBrushFace( graph, FaceGetFlags( flags ) );
#endif
}


void Scene_BrushGetShader_Selected( scene::Graph& graph, CopiedString& shader ){
#if 1
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		if ( BrushInstance* brush = Instance_getBrush( GlobalSelectionSystem().ultimateSelected() ) ) {
			bool done = false;
			Brush_forEachFace( *brush, [&]( Face& face ){
				if ( !done ) {
					done = true;
					shader = face.GetShader();
				}
			});
		}
	}
#else
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceGetShader( shader ) );
#endif
}

void Scene_BrushGetShader_Component_Selected( scene::Graph& graph, CopiedString& shader ){
#if 1
	if ( !g_SelectedFaceInstances.empty() ) {
		FaceInstance& faceInstance = g_SelectedFaceInstances.last();
		shader = faceInstance.getFace().GetShader();
	}
#else
	FaceGetShader visitor( shader );
	Scene_ForEachSelectedBrushFace( graph, visitor );
#endif
}


class filter_face_shader : public FaceFilter
{
	const char* m_shader;
public:
	filter_face_shader( const char* shader ) : m_shader( shader ){
	}
	bool filter( const Face& face ) const {
		return shader_equal( face.GetShader(), m_shader );
	}
};

class filter_face_shader_prefix : public FaceFilter
{
	const char* m_prefix;
public:
	filter_face_shader_prefix( const char* prefix ) : m_prefix( prefix ){
	}
	bool filter( const Face& face ) const {
		return shader_equal_n( face.GetShader(), m_prefix, strlen( m_prefix ) );
	}
};

class filter_face_flags : public FaceFilter
{
	int m_flags;
public:
	filter_face_flags( int flags ) : m_flags( flags ){
	}
	bool filter( const Face& face ) const {
		return ( face.getShader().shaderFlags() & m_flags ) != 0;
	}
};

class filter_face_contents : public FaceFilter
{
	int m_contents;
public:
	filter_face_contents( int contents ) : m_contents( contents ){
	}
	bool filter( const Face& face ) const {
		return ( face.getShader().m_flags.m_contentFlags & m_contents ) != 0;
	}
};



class filter_brush_any_face : public BrushFilter
{
	FaceFilter* m_filter;
public:
	filter_brush_any_face( FaceFilter* filter ) : m_filter( filter ){
	}
	bool filter( const Brush& brush ) const {
		bool filtered = false;
		Brush_forEachFace( brush, [&]( Face& face ){
			if ( m_filter->filter( face ) ) {
				filtered = true;
			}
		});
		return filtered;
	}
};

class filter_brush_all_faces : public BrushFilter
{
	FaceFilter* m_filter;
public:
	filter_brush_all_faces( FaceFilter* filter ) : m_filter( filter ){
	}
	bool filter( const Brush& brush ) const {
		bool filtered = true;
		Brush_forEachFace( brush, [&]( Face& face ){
			if ( !m_filter->filter( face ) ) {
				filtered = false;
			}
		});
		return filtered;
	}
};


filter_face_flags g_filter_face_clip( QER_CLIP );
filter_brush_all_faces g_filter_brush_clip( &g_filter_face_clip );

filter_face_shader g_filter_face_clip_q2( "textures/clip" );
filter_brush_all_faces g_filter_brush_clip_q2( &g_filter_face_clip_q2 );

filter_face_shader g_filter_face_weapclip( "textures/common/weapclip" );
filter_brush_all_faces g_filter_brush_weapclip( &g_filter_face_weapclip );

filter_face_shader g_filter_face_commonclip( "textures/common/clip" );
filter_brush_all_faces g_filter_brush_commonclip( &g_filter_face_commonclip );

filter_face_shader g_filter_face_fullclip( "textures/common/fullclip" );
filter_brush_all_faces g_filter_brush_fullclip( &g_filter_face_fullclip );

filter_face_shader g_filter_face_botclip( "textures/common/botclip" );
filter_brush_all_faces g_filter_brush_botclip( &g_filter_face_botclip );

filter_face_shader g_filter_face_donotenter( "textures/common/donotenter" );
filter_brush_all_faces g_filter_brush_donotenter( &g_filter_face_donotenter );

filter_face_shader_prefix g_filter_face_caulk( "textures/common/caulk" );
filter_brush_all_faces g_filter_brush_caulk( &g_filter_face_caulk );

filter_face_shader_prefix g_filter_face_caulk_ja( "textures/system/caulk" );
filter_brush_all_faces g_filter_brush_caulk_ja( &g_filter_face_caulk_ja );

filter_face_shader g_filter_face_caulk_q1( "textures/skip" );
filter_brush_all_faces g_filter_brush_caulk_q1( &g_filter_face_caulk_q1 );

filter_face_flags g_filter_face_liquids( QER_LIQUID );
filter_brush_any_face g_filter_brush_liquids( &g_filter_face_liquids );

filter_face_shader_prefix g_filter_face_liquidsdir( "textures/liquids/" );
filter_brush_any_face g_filter_brush_liquidsdir( &g_filter_face_liquidsdir );

filter_face_shader_prefix g_filter_face_liquids_q1( "textures/*" ); // textures/*04water1
filter_brush_any_face g_filter_brush_liquids_q1( &g_filter_face_liquids_q1 );

filter_face_shader g_filter_face_hint( "textures/common/hint" );
filter_brush_any_face g_filter_brush_hint( &g_filter_face_hint );

filter_face_shader g_filter_face_hintlocal( "textures/common/hintlocal" );
filter_brush_any_face g_filter_brush_hintlocal( &g_filter_face_hintlocal );

filter_face_shader g_filter_face_hint_q2( "textures/hint" );
filter_brush_any_face g_filter_brush_hint_q2( &g_filter_face_hint_q2 );

filter_face_shader g_filter_face_hint_ja( "textures/system/hint" );
filter_brush_any_face g_filter_brush_hint_ja( &g_filter_face_hint_ja );

filter_face_shader g_filter_face_areaportal( "textures/common/areaportal" );
filter_brush_any_face g_filter_brush_areaportal( &g_filter_face_areaportal );

filter_face_shader g_filter_face_visportal( "textures/editor/visportal" );
filter_brush_any_face g_filter_brush_visportal( &g_filter_face_visportal );

filter_face_shader g_filter_face_clusterportal( "textures/common/clusterportal" );
filter_brush_all_faces g_filter_brush_clusterportal( &g_filter_face_clusterportal );

filter_face_shader g_filter_face_lightgrid( "textures/common/lightgrid" );
filter_brush_all_faces g_filter_brush_lightgrid( &g_filter_face_lightgrid );

filter_face_flags g_filter_face_translucent( QER_TRANS | QER_ALPHATEST );
filter_brush_any_face g_filter_brush_translucent( &g_filter_face_translucent );

filter_face_contents g_filter_face_detail( BRUSH_DETAIL_MASK );
filter_brush_all_faces g_filter_brush_detail( &g_filter_face_detail );

filter_face_shader_prefix g_filter_face_decals( "textures/decals/" );
filter_brush_any_face g_filter_brush_decals( &g_filter_face_decals );

filter_face_flags g_filter_face_sky( QER_SKY );
filter_brush_any_face g_filter_brush_sky( &g_filter_face_sky );


void BrushFilters_construct(){
	add_brush_filter( g_filter_brush_clip, EXCLUDE_CLIP );
	add_brush_filter( g_filter_brush_clip_q2, EXCLUDE_CLIP );
	add_brush_filter( g_filter_brush_weapclip, EXCLUDE_CLIP );
	add_brush_filter( g_filter_brush_fullclip, EXCLUDE_CLIP );
	add_brush_filter( g_filter_brush_commonclip, EXCLUDE_CLIP );
	add_brush_filter( g_filter_brush_botclip, EXCLUDE_BOTCLIP );
	add_brush_filter( g_filter_brush_donotenter, EXCLUDE_BOTCLIP );
	add_brush_filter( g_filter_brush_caulk, EXCLUDE_CAULK );
	add_brush_filter( g_filter_brush_caulk_ja, EXCLUDE_CAULK );
	add_face_filter( g_filter_face_caulk, EXCLUDE_CAULK );
	add_face_filter( g_filter_face_caulk_ja, EXCLUDE_CAULK );
	add_brush_filter( g_filter_brush_liquids, EXCLUDE_LIQUIDS );
	add_brush_filter( g_filter_brush_liquidsdir, EXCLUDE_LIQUIDS );
	add_brush_filter( g_filter_brush_hint, EXCLUDE_HINTSSKIPS );
	add_brush_filter( g_filter_brush_hintlocal, EXCLUDE_HINTSSKIPS );
	add_brush_filter( g_filter_brush_hint_q2, EXCLUDE_HINTSSKIPS );
	add_brush_filter( g_filter_brush_hint_ja, EXCLUDE_HINTSSKIPS );
	add_brush_filter( g_filter_brush_clusterportal, EXCLUDE_CLUSTERPORTALS );
	add_brush_filter( g_filter_brush_visportal, EXCLUDE_VISPORTALS );
	add_brush_filter( g_filter_brush_areaportal, EXCLUDE_AREAPORTALS );
	add_brush_filter( g_filter_brush_translucent, EXCLUDE_TRANSLUCENT );
	if( !string_equal( GlobalRadiant().getRequiredGameDescriptionKeyValue( "brushtypes" ), "quake" ) ){ /* conditional for entity based structural/detail filters; see entity plugin */
		add_brush_filter( g_filter_brush_detail, EXCLUDE_DETAILS );
		add_brush_filter( g_filter_brush_detail, EXCLUDE_STRUCTURAL, true );
	}
	if( string_equal( GlobalRadiant().getRequiredGameDescriptionKeyValue( "entities" ), "quake" ) ){
		add_brush_filter( g_filter_brush_liquids_q1, EXCLUDE_LIQUIDS );
		add_brush_filter( g_filter_brush_caulk_q1, EXCLUDE_CAULK );
		add_face_filter( g_filter_face_caulk_q1, EXCLUDE_CAULK );
	}
	add_brush_filter( g_filter_brush_lightgrid, EXCLUDE_LIGHTGRID );
	add_brush_filter( g_filter_brush_decals, EXCLUDE_DECALS );
	add_brush_filter( g_filter_brush_sky, EXCLUDE_SKY );
}

#if 0

void normalquantisation_draw(){
	gl().glPointSize( 1 );
	gl().glBegin( GL_POINTS );
	for ( std::size_t i = 0; i <= c_quantise_normal; ++i )
	{
		for ( std::size_t j = 0; j <= c_quantise_normal; ++j )
		{
			Normal3f vertex( normal3f_normalised( Normal3f(
			        static_cast<float>( c_quantise_normal - j - i ),
			        static_cast<float>( i ),
			        static_cast<float>( j )
			                                      ) ) );
			VectorScale( normal3f_to_array( vertex ), 64.f, normal3f_to_array( vertex ) );
			gl().glVertex3fv( normal3f_to_array( vertex ) );
			vertex.x = -vertex.x;
			gl().glVertex3fv( normal3f_to_array( vertex ) );
		}
	}
	gl().glEnd();
}

class RenderableNormalQuantisation : public OpenGLRenderable
{
public:
	void render( RenderStateFlags state ) const {
		normalquantisation_draw();
	}
};

const float g_test_quantise_normal = 1.f / static_cast<float>( 1 << 3 );

class TestNormalQuantisation
{
	void check_normal( const Normal3f& normal, const Normal3f& other ){
		spherical_t spherical = spherical_from_normal3f( normal );
		double longditude = RAD2DEG( spherical.longditude );
		double latitude = RAD2DEG( spherical.latitude );
		double x = cos( spherical.longditude ) * sin( spherical.latitude );
		double y = sin( spherical.longditude ) * sin( spherical.latitude );
		double z = cos( spherical.latitude );

		ASSERT_MESSAGE( normal3f_dot( normal, other ) > 0.99, "bleh" );
	}

	void test_normal( const Normal3f& normal ){
		Normal3f test = normal3f_from_spherical( spherical_from_normal3f( normal ) );
		check_normal( normal, test );

		EOctant octant = normal3f_classify_octant( normal );
		Normal3f folded = normal3f_fold_octant( normal, octant );
		ESextant sextant = normal3f_classify_sextant( folded );
		folded = normal3f_fold_sextant( folded, sextant );

		double scale = static_cast<float>( c_quantise_normal ) / ( folded.x + folded.y + folded.z );

		double zbits = folded.z * scale;
		double ybits = folded.y * scale;

		std::size_t zbits_q = static_cast<std::size_t>( zbits );
		std::size_t ybits_q = static_cast<std::size_t>( ybits );

		ASSERT_MESSAGE( zbits_q <= ( c_quantise_normal / 8 ) * 3, "bleh" );
		ASSERT_MESSAGE( ybits_q <= ( c_quantise_normal / 2 ), "bleh" );
		ASSERT_MESSAGE( zbits_q + ( ( c_quantise_normal / 2 ) - ybits_q ) <= ( c_quantise_normal / 2 ), "bleh" );

		std::size_t y_t = ( zbits_q < ( c_quantise_normal / 4 ) ) ? ybits_q : ( c_quantise_normal / 2 ) - ybits_q;
		std::size_t z_t = ( zbits_q < ( c_quantise_normal / 4 ) ) ? zbits_q : ( c_quantise_normal / 2 ) - zbits_q;
		std::size_t index = ( c_quantise_normal / 4 ) * y_t + z_t;
		ASSERT_MESSAGE( index <= ( c_quantise_normal / 4 ) * ( c_quantise_normal / 2 ), "bleh" );

		Normal3f tmp( c_quantise_normal - zbits_q - ybits_q, ybits_q, zbits_q );
		tmp = normal3f_normalised( tmp );

		Normal3f unfolded = normal3f_unfold_octant( normal3f_unfold_sextant( tmp, sextant ), octant );

		check_normal( normal, unfolded );

		double dot = normal3f_dot( normal, unfolded );
		float length = VectorLength( normal3f_to_array( unfolded ) );
		float inv_length = 1 / length;

		Normal3f quantised = normal3f_quantised( normal );
		check_normal( normal, quantised );
	}
	void test2( const Normal3f& normal, const Normal3f& other ){
		if ( normal3f_quantised( normal ) != normal3f_quantised( other ) ) {
			int bleh = 0;
		}
	}

	static Normal3f normalise( float x, float y, float z ){
		return normal3f_normalised( Normal3f( x, y, z ) );
	}

	float vec_rand(){
		return static_cast<float>( rand() - ( RAND_MAX / 2 ) );
	}

	Normal3f normal3f_rand(){
		return normalise( vec_rand(), vec_rand(), vec_rand() );
	}

public:
	TestNormalQuantisation(){
		for ( int i = 4096; i > 0; --i )
			test_normal( normal3f_rand() );

		test_normal( normalise( 1, 0, 0 ) );
		test_normal( normalise( 0, 1, 0 ) );
		test_normal( normalise( 0, 0, 1 ) );
		test_normal( normalise( 1, 1, 0 ) );
		test_normal( normalise( 1, 0, 1 ) );
		test_normal( normalise( 0, 1, 1 ) );

		test_normal( normalise( 10000, 10000, 10000 ) );
		test_normal( normalise( 10000, 10000, 10001 ) );
		test_normal( normalise( 10000, 10000, 10002 ) );
		test_normal( normalise( 10000, 10000, 10010 ) );
		test_normal( normalise( 10000, 10000, 10020 ) );
		test_normal( normalise( 10000, 10000, 10030 ) );
		test_normal( normalise( 10000, 10000, 10100 ) );
		test_normal( normalise( 10000, 10000, 10101 ) );
		test_normal( normalise( 10000, 10000, 10102 ) );
		test_normal( normalise( 10000, 10000, 10200 ) );
		test_normal( normalise( 10000, 10000, 10201 ) );
		test_normal( normalise( 10000, 10000, 10202 ) );
		test_normal( normalise( 10000, 10000, 10203 ) );
		test_normal( normalise( 10000, 10000, 10300 ) );


		test2( normalise( 10000, 10000, 10000 ), normalise( 10000, 10000, 10001 ) );
		test2( normalise( 10000, 10000, 10001 ), normalise( 10000, 10001, 10000 ) );
	}
};

TestNormalQuantisation g_testNormalQuantisation;


#endif

#if 0
class TestSelectableObserver : public observer_template<const Selectable&>
{
public:
	void notify( const Selectable& arguments ){
		bool bleh = arguments.isSelected();
	}
};

inline void test_bleh(){
	TestSelectableObserver test;
	ObservableSelectableInstance< SingleObservable< SelectionChangeCallback > > bleh;
	bleh.attach( test );
	bleh.setSelected( true );
	bleh.detach( test );
}

class TestBleh
{
public:
	TestBleh(){
		test_bleh();
	}
};

const TestBleh testbleh;
#endif


#if 0
class TestRefcountedString
{
public:
	TestRefcountedString(){
		{
			// copy construct
			SmartString string1( "string1" );
			SmartString string2( string1 );
			SmartString string3( string2 );
		}
		{
			// refcounted assignment
			SmartString string1( "string1" );
			SmartString string2( "string2" );
			string1 = string2;
		}
		{
			// copy assignment
			SmartString string1( "string1" );
			SmartString string2( "string2" );
			string1 = string2.c_str();
		}
		{
			// self-assignment
			SmartString string1( "string1" );
			string1 = string1;
		}
		{
			// self-assignment via another reference
			SmartString string1( "string1" );
			SmartString string2( string1 );
			string1 = string2;
		}
	}
};

const TestRefcountedString g_testRefcountedString;

#endif

void Select_MakeDetail(){
	UndoableCommand undo( "brushSetDetail" );
	Scene_BrushSetDetail_Selected( GlobalSceneGraph(), true );
}

void Select_MakeStructural(){
	UndoableCommand undo( "brushClearDetail" );
	Scene_BrushSetDetail_Selected( GlobalSceneGraph(), false );
}

class BrushMakeSided
{
	std::size_t m_count;
public:
	BrushMakeSided( std::size_t count )
		: m_count( count ){
	}
	void set(){
		Scene_BrushConstructPrefab( GlobalSceneGraph(), EBrushPrefab::Prism, m_count, false, TextureBrowser_GetSelectedShader() );
	}
	typedef MemberCaller<BrushMakeSided, void(), &BrushMakeSided::set> SetCaller;
};


BrushMakeSided g_brushmakesided3( 3 );
BrushMakeSided g_brushmakesided4( 4 );
BrushMakeSided g_brushmakesided5( 5 );
BrushMakeSided g_brushmakesided6( 6 );
BrushMakeSided g_brushmakesided7( 7 );
BrushMakeSided g_brushmakesided8( 8 );
BrushMakeSided g_brushmakesided9( 9 );


class BrushPrefab
{
	EBrushPrefab m_type;
public:
	BrushPrefab( EBrushPrefab type )
		: m_type( type ){
	}
	void set(){
		DoSides( m_type );
	}
	typedef MemberCaller<BrushPrefab, void(), &BrushPrefab::set> SetCaller;
};

BrushPrefab g_brushprism( EBrushPrefab::Prism );
BrushPrefab g_brushcone( EBrushPrefab::Cone );
BrushPrefab g_brushsphere( EBrushPrefab::Sphere );
BrushPrefab g_brushrock( EBrushPrefab::Rock );
BrushPrefab g_brushicosahedron( EBrushPrefab::Icosahedron );




Callback<void()> g_texture_lock_status_changed;
ToggleItem g_texdef_movelock_item{ BoolExportCaller( g_brush_texturelock_enabled ) };

void Texdef_ToggleMoveLock(){
	g_brush_texturelock_enabled = !g_brush_texturelock_enabled;
	g_texdef_movelock_item.update();
	g_texture_lock_status_changed();
}

ToggleItem g_texdef_moveVlock_item{ BoolExportCaller( g_brush_textureVertexlock_enabled ) };
void Texdef_ToggleMoveVLock(){
	g_brush_textureVertexlock_enabled = !g_brush_textureVertexlock_enabled;
	g_texdef_moveVlock_item.update();
}




void Brush_registerCommands(){
	GlobalToggles_insert( "TogTexLock", makeCallbackF( Texdef_ToggleMoveLock ), ToggleItem::AddCallbackCaller( g_texdef_movelock_item ), QKeySequence( "Shift+T" ) );
	GlobalToggles_insert( "TogTexVertexLock", makeCallbackF( Texdef_ToggleMoveVLock ), ToggleItem::AddCallbackCaller( g_texdef_moveVlock_item ) );

	GlobalCommands_insert( "BrushPrism", BrushPrefab::SetCaller( g_brushprism ) );
	GlobalCommands_insert( "BrushCone", BrushPrefab::SetCaller( g_brushcone ) );
	GlobalCommands_insert( "BrushSphere", BrushPrefab::SetCaller( g_brushsphere ) );
	GlobalCommands_insert( "BrushRock", BrushPrefab::SetCaller( g_brushrock ) );
	GlobalCommands_insert( "BrushIcosahedron", BrushPrefab::SetCaller( g_brushicosahedron ) );

	GlobalCommands_insert( "Brush3Sided", BrushMakeSided::SetCaller( g_brushmakesided3 ), QKeySequence( "Ctrl+3" ) );
	GlobalCommands_insert( "Brush4Sided", BrushMakeSided::SetCaller( g_brushmakesided4 ), QKeySequence( "Ctrl+4" ) );
	GlobalCommands_insert( "Brush5Sided", BrushMakeSided::SetCaller( g_brushmakesided5 ), QKeySequence( "Ctrl+5" ) );
	GlobalCommands_insert( "Brush6Sided", BrushMakeSided::SetCaller( g_brushmakesided6 ), QKeySequence( "Ctrl+6" ) );
	GlobalCommands_insert( "Brush7Sided", BrushMakeSided::SetCaller( g_brushmakesided7 ), QKeySequence( "Ctrl+7" ) );
	GlobalCommands_insert( "Brush8Sided", BrushMakeSided::SetCaller( g_brushmakesided8 ), QKeySequence( "Ctrl+8" ) );
	GlobalCommands_insert( "Brush9Sided", BrushMakeSided::SetCaller( g_brushmakesided9 ), QKeySequence( "Ctrl+9" ) );

	GlobalCommands_insert( "MakeDetail", makeCallbackF( Select_MakeDetail ), QKeySequence( "Alt+D" ) );
	GlobalCommands_insert( "MakeStructural", makeCallbackF( Select_MakeStructural ), QKeySequence( "Alt+S" ) );
}

void Brush_constructMenu( QMenu* menu ){
	create_menu_item_with_mnemonic( menu, "Prism...", "BrushPrism" );
	create_menu_item_with_mnemonic( menu, "Cone...", "BrushCone" );
	create_menu_item_with_mnemonic( menu, "Sphere...", "BrushSphere" );
	create_menu_item_with_mnemonic( menu, "Rock...", "BrushRock" );
	create_menu_item_with_mnemonic( menu, "Icosahedron...", "BrushIcosahedron" );
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "CSG" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "CSG &Subtract", "CSGSubtract" );
		create_menu_item_with_mnemonic( submenu, "CSG &Merge", "CSGMerge" );
		create_menu_item_with_mnemonic( submenu, "CSG &Wrap Merge", "CSGWrapMerge" );
		create_menu_item_with_mnemonic( submenu, "CSG &Intersect", "CSGIntersect" );
		create_menu_item_with_mnemonic( submenu, "Make &Room", "CSGroom" );
		create_menu_item_with_mnemonic( submenu, "CSG &Tool", "CSGTool" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Clipper" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Clip selection", "ClipperClip" );
		create_menu_item_with_mnemonic( submenu, "Split selection", "ClipperSplit" );
		create_menu_item_with_mnemonic( submenu, "Flip Clip orientation", "ClipperFlip" );
	}
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Make detail", "MakeDetail" );
	create_menu_item_with_mnemonic( menu, "Make structural", "MakeStructural" );

	menu->addSeparator();
	create_check_menu_item_with_mnemonic( menu, "Texture Lock", "TogTexLock" );
	create_check_menu_item_with_mnemonic( menu, "Texture Vertex Lock", "TogTexVertexLock" );
	create_menu_item_with_mnemonic( menu, "Reset Texture", "TextureReset/Cap" );
	create_menu_item_with_mnemonic( menu, "AutoCaulk Selected", "AutoCaulkSelected" );

	command_connect_accelerator( "Brush3Sided" );
	command_connect_accelerator( "Brush4Sided" );
	command_connect_accelerator( "Brush5Sided" );
	command_connect_accelerator( "Brush6Sided" );
	command_connect_accelerator( "Brush7Sided" );
	command_connect_accelerator( "Brush8Sided" );
	command_connect_accelerator( "Brush9Sided" );
}

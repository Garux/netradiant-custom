/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"

#include "model.h"

#include "assimp/Importer.hpp"
#include "assimp/importerdesc.h"
#include "assimp/Logger.hpp"
#include "assimp/DefaultLogger.hpp"
#include "assimp/IOSystem.hpp"
#include "assimp/MemoryIOWrapper.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/mesh.h"

#include <map>


class AssLogger : public Assimp::Logger
{
public:
	void OnDebug( const char* message ) override {
#ifdef _DEBUG
		Sys_Printf( "%s\n", message );
#endif
	}
	void OnVerboseDebug( const char *message ) override {
#ifdef _DEBUG
		Sys_FPrintf( SYS_VRB, "%s\n", message );
#endif
	}
	void OnInfo( const char* message ) override {
#ifdef _DEBUG
		Sys_Printf( "%s\n", message );
#endif
	}
	void OnWarn( const char* message ) override {
		Sys_Warning( "%s\n", message );
	}
	void OnError( const char* message ) override {
		Sys_FPrintf( SYS_WRN, "ERROR: %s\n", message ); /* let it be a warning, since radiant stops monitoring on error message flag */
	}

	bool attachStream( Assimp::LogStream *pStream, unsigned int severity ) override {
		return false;
	}
	bool detachStream( Assimp::LogStream *pStream, unsigned int severity ) override {
		return false;
	}
};


class AssIOSystem : public Assimp::IOSystem
{
public:
	// -------------------------------------------------------------------
	/** @brief Tests for the existence of a file at the given path.
	 *
	 * @param pFile Path to the file
	 * @return true if there is a file with this path, else false.
	 */
	bool Exists( const char* pFile ) const override {
		return vfsGetFileCount( pFile ) != 0;
	}

	// -------------------------------------------------------------------
	/** @brief Returns the system specific directory separator
	 *  @return System specific directory separator
	 */
	char getOsSeparator() const override {
		return '/';
	}

	// -------------------------------------------------------------------
	/** @brief Open a new file with a given path.
	 *
	 *  When the access to the file is finished, call Close() to release
	 *  all associated resources (or the virtual dtor of the IOStream).
	 *
	 *  @param pFile Path to the file
	 *  @param pMode Desired file I/O mode. Required are: "wb", "w", "wt",
	 *         "rb", "r", "rt".
	 *
	 *  @return New IOStream interface allowing the lib to access
	 *         the underlying file.
	 *  @note When implementing this class to provide custom IO handling,
	 *  you probably have to supply an own implementation of IOStream as well.
	 */
	Assimp::IOStream* Open( const char* pFile, const char* pMode = "rb" ) override {
		if ( MemBuffer boo = vfsLoadFile( pFile ) ) {
			return new Assimp::MemoryIOStream( boo.release(), boo.size(), true );
		}
		return nullptr;
	}

	// -------------------------------------------------------------------
	/** @brief Closes the given file and releases all resources
	 *    associated with it.
	 *  @param pFile The file instance previously created by Open().
	 */
	void Close( Assimp::IOStream* pFile ) override {
		delete pFile;
	}

	// -------------------------------------------------------------------
	/** @brief CReates an new directory at the given path.
	 *  @param  path    [in] The path to create.
	 *  @return True, when a directory was created. False if the directory
	 *           cannot be created.
	 */
	bool CreateDirectory( const std::string &path ) override {
		Error( "AssIOSystem::CreateDirectory" );
		return false;
	}

	// -------------------------------------------------------------------
	/** @brief Will change the current directory to the given path.
	 *  @param path     [in] The path to change to.
	 *  @return True, when the directory has changed successfully.
	 */
	bool ChangeDirectory( const std::string &path ) override {
		Error( "AssIOSystem::ChangeDirectory" );
		return false;
	}

	bool DeleteFile( const std::string &file ) override {
		Error( "AssIOSystem::DeleteFile" );
		return false;
	}

private:
};

static Assimp::Importer *s_assImporter = nullptr;

void assimp_init(){
	s_assImporter = new Assimp::Importer();

	s_assImporter->SetPropertyBool( AI_CONFIG_PP_PTV_ADD_ROOT_TRANSFORMATION, true );
	s_assImporter->SetPropertyInteger( AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE );
	s_assImporter->SetPropertyString( AI_CONFIG_IMPORT_MDL_COLORMAP, "gfx/palette.lmp" ); // Q1 palette, default is fine too
	s_assImporter->SetPropertyBool( AI_CONFIG_IMPORT_MD3_LOAD_SHADERS, false );
	s_assImporter->SetPropertyString( AI_CONFIG_IMPORT_MD3_SHADER_SRC, "scripts/" );
	s_assImporter->SetPropertyBool( AI_CONFIG_IMPORT_MD3_HANDLE_MULTIPART, false );
	s_assImporter->SetPropertyInteger( AI_CONFIG_PP_RVC_FLAGS, aiComponent_TANGENTS_AND_BITANGENTS ); // varying tangents prevent aiProcess_JoinIdenticalVertices

	Assimp::DefaultLogger::set( new AssLogger );

	s_assImporter->SetIOHandler( new AssIOSystem );
}

struct ModelNameFrame
{
	CopiedString m_name;
	int m_frame;
	bool operator<( const ModelNameFrame& other ) const {
		const int cmp = string_compare_nocase( m_name.c_str(), other.m_name.c_str() );
		return cmp != 0? cmp < 0 : m_frame < other.m_frame;
	}
};
struct AssModel
{
	struct AssModelMesh final : public AssMeshWalker
	{
		const aiMesh *m_mesh;
		CopiedString m_shader;

		AssModelMesh( const aiScene *scene, const aiMesh *mesh, const char *rootPath ) : m_mesh( mesh ){
			aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

			aiString matname = material->GetName();
#ifdef _DEBUG
						Sys_Printf( "matname: %s\n", matname.C_Str() );
#endif
			if( aiString texname;
			    aiReturn_SUCCESS == material->Get( AI_MATKEY_TEXTURE_DIFFUSE( 0 ), texname )
			 && texname.length != 0
			 && !string_equal_prefix_nocase( matname.C_Str(), "textures/" ) /* matname looks intentionally named as ingame shader */
			 && !string_equal_prefix_nocase( matname.C_Str(), "textures\\" )
			 && !string_equal_prefix_nocase( matname.C_Str(), "models/" )
			 && !string_equal_prefix_nocase( matname.C_Str(), "models\\" ) ){
#ifdef _DEBUG
							Sys_Printf( "texname: %s\n", texname.C_Str() );
#endif
				m_shader = StringStream<64>( PathCleaned( PathExtensionless( texname.C_Str() ) ) );
			}
			else{
				m_shader = StringStream<64>( PathCleaned( PathExtensionless( matname.C_Str() ) ) );
			}

			const CopiedString oldShader( m_shader );
			if( strchr( m_shader.c_str(), '/' ) == nullptr ){ /* texture is likely in the folder, where model is */
				m_shader = StringStream<64>( rootPath, m_shader );
			}
			else{
				const char *name = m_shader.c_str();
				if( name[0] == '/' || ( name[0] != '\0' && name[1] == ':' ) || strstr( name, ".." ) ){ /* absolute path or with .. */
					const char* p;
					if( ( p = string_in_string_nocase( name, "/models/" ) )
					 || ( p = string_in_string_nocase( name, "/textures/" ) ) ){
						m_shader = p + 1;
					}
					else{
						m_shader = StringStream<64>( rootPath, path_get_filename_start( name ) );
					}
				}
			}

			if( oldShader != m_shader )
				Sys_FPrintf( SYS_VRB, "substituting: %s -> %s\n", oldShader.c_str(), m_shader.c_str() );
		}

		void forEachFace( std::function<void( const Vector3 ( &xyz )[3], const Vector2 ( &st )[3])> visitor ) const override {
			for ( const aiFace& face : Span( m_mesh->mFaces, m_mesh->mNumFaces ) ){
				// if( face.mNumIndices == 3 )
				Vector3 xyz[3];
				Vector2 st[3];
				for( size_t n = 0; n < 3; ++n ){
					const auto i = face.mIndices[n];
					xyz[n] = { m_mesh->mVertices[i].x, m_mesh->mVertices[i].y, m_mesh->mVertices[i].z };
					if( m_mesh->HasTextureCoords( 0 ) )
						st[n] = { m_mesh->mTextureCoords[0][i].x, m_mesh->mTextureCoords[0][i].y };
					else
						st[n] = { 0, 0 };
				}
				visitor( xyz, st );
			}
		}
		const char *getShaderName() const override {
			return m_shader.c_str();
		}
	};

	aiScene *m_scene;
	std::vector<AssModelMesh> m_meshes;

	AssModel( aiScene *scene, const char *modelname ) : m_scene( scene ){
		m_meshes.reserve( scene->mNumMeshes );
		const auto rootPath = StringStream<64>( PathCleaned( PathFilenameless( modelname ) ) );
		const auto traverse = [&]( const auto& self, const aiNode* node ) -> void {
			for( size_t n = 0; n < node->mNumMeshes; ++n ){
				const aiMesh *mesh = scene->mMeshes[node->mMeshes[n]];
				if( mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE ){
					m_meshes.emplace_back( scene, mesh, rootPath );
				}
			}

			// traverse all children
			for ( size_t n = 0; n < node->mNumChildren; ++n ){
				self( self, node->mChildren[n] );
			}
		};

		traverse( traverse, scene->mRootNode );
	}
};

static std::map<ModelNameFrame, AssModel> s_assModels;



/*
   LoadModel() - ydnar
   loads a picoModel and returns a pointer to the picoModel_t struct or NULL if not found
 */

static AssModel *LoadModel( const char *name, int frame ){
	/* dummy check */
	if ( strEmptyOrNull( name ) ) {
		return nullptr;
	}

	/* try to find existing picoModel */
	auto it = s_assModels.find( ModelNameFrame{ name, frame } );
	if( it != s_assModels.end() ){
		return &it->second;
	}

	unsigned flags = //aiProcessPreset_TargetRealtime_Fast
	            //    | aiProcess_FixInfacingNormals
	                 aiProcess_GenNormals
	               | aiProcess_JoinIdenticalVertices
	               | aiProcess_Triangulate
	               | aiProcess_GenUVCoords
	               | aiProcess_SortByPType
	               | aiProcess_FindDegenerates
	               | aiProcess_FindInvalidData
	               | aiProcess_ValidateDataStructure
	               | aiProcess_FlipUVs
	               | aiProcess_FlipWindingOrder
	               | aiProcess_PreTransformVertices
	               | aiProcess_RemoveComponent
	               | aiProcess_SplitLargeMeshes;
	// rotate the whole scene 90 degrees around the x axis to convert assimp's Y = UP to Quakes's Z = UP
	s_assImporter->SetPropertyMatrix( AI_CONFIG_PP_PTV_ROOT_TRANSFORMATION, aiMatrix4x4( 1, 0, 0, 0,
	                                                                                     0, 0, -1, 0,
	                                                                                     0, 1, 0, 0,
	                                                                                     0, 0, 0, 1 ) ); // aiMatrix4x4::RotationX( c_half_pi )

	s_assImporter->SetPropertyInteger( AI_CONFIG_PP_SLM_VERTEX_LIMIT, maxSurfaceVerts ); // TODO this optimal and with respect to lightmapped/not
	s_assImporter->SetPropertyInteger( AI_CONFIG_IMPORT_GLOBAL_KEYFRAME, frame );

	const aiScene *scene = s_assImporter->ReadFile( name, flags );

	if( scene != nullptr ){
		if( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE )
			Sys_Warning( "AI_SCENE_FLAGS_INCOMPLETE\n" );
		return &s_assModels.emplace( ModelNameFrame{ name, frame }, AssModel( s_assImporter->GetOrphanedScene(), name ) ).first->second;
	}
	else{
		return nullptr; // TODO /* if loading failed, make a bogus model to silence the rest of the warnings */
	}
}

std::vector<const AssMeshWalker*> LoadModelWalker( const char *name, int frame ){
	AssModel *model = LoadModel( name, frame );
	std::vector<const AssMeshWalker*> vector;
	if( model != nullptr )
		for( const auto& val : model->m_meshes )
			vector.push_back( &val );
	return vector;
}



enum EModelFlags{
	eRMG_BSP = 1 << 0,
	eClipModel = 1 << 1,
	eForceMeta = 1 << 2,
	eExtrudeFaceNormals = 1 << 3,
	eExtrudeTerrain = 1 << 4,
	eColorToAlpha = 1 << 5,
	eNoSmooth = 1 << 6,
	eExtrudeVertexNormals = 1 << 7,
	ePyramidalClip = 1 << 8,
	eExtrudeDownwards = 1 << 9,
	eExtrudeUpwards = 1 << 10,
	eMaxExtrude = 1 << 11,
	eAxialBackplane = 1 << 12,
	eClipFlags = eClipModel | eExtrudeFaceNormals | eExtrudeTerrain | eExtrudeVertexNormals | ePyramidalClip | eExtrudeDownwards | eExtrudeUpwards | eMaxExtrude | eAxialBackplane,
};

template<class T>
size_t normal_make_axial( BasicVector3<T>& normal ){
	const size_t i = vector3_max_abs_component_index( normal );
	normal = normal[i] >= 0? g_vector3_axes[i] : -g_vector3_axes[i];
	return i;
}

struct ClipSides
{
	Plane3f fplane; // front plane
	winding_accu_t fw; // front winding
	Plane3f bplane{ 0, 0, 0, 0 }; // back plane, present if != 0
	winding_accu_t bw; // back winding
	std::vector<Plane3> splanes; // side planes, using fw[i], fw[i + 1] points, size = fw.size

	shaderInfo_t &si;
	entity_t& entity;
	const double clipDepth;
	ClipSides( shaderInfo_t& si, entity_t& entity, float clipDepth ) : si( si ), entity( entity ), clipDepth( clipDepth ){
	}

	/* construct front and allocate sides, requires fw */
	bool construct(){
		/* prepare a brush */
		buildBrush.sides.reserve( MAX_BUILD_SIDES );
		buildBrush.entityNum = entity.mapEntityNum;
		buildBrush.contentShader = &si;
		buildBrush.compileFlags = si.compileFlags;
		buildBrush.contentFlags = si.contentFlags;
		buildBrush.detail = true;

		// choose decent triangle to create plane
		using Witer = decltype( fw )::const_iterator;
		Witer a = fw.cbegin(), b = a + 1, c = b + 1;
		const auto perimeter = []( Witer a, Witer b, Witer c ){
			return vector3_length_squared( *b - *a ) +
			       vector3_length_squared( *a - *c ) +
			       vector3_length_squared( *c - *b );
		};
		while( c + 1 != fw.cend() && perimeter( a, b, c + 1 ) > perimeter( a, b, c ) )
			++c;
		while( b + 1 != c && perimeter( a, b + 1, c ) > perimeter( a, b, c ) )
			++b;

		if( !PlaneFromPoints( fplane, *a, *b, *c ) )
			return false;

		// snap points before using them for further calculations
		// precision suffers a lot, when two of normal values are under .00025 (often no collision, knocking up effect in ioq3)
		// also broken drawsurfs in case of normal brushes
		// ? worth to snap nearly axial edges (or on nearly axial plane) beforehand or SnapPlaneImproved is nuff good for sides
		// latter seems good nuff, no noticeable difference
		if( SnapPlaneImproved( fplane, Span( std::as_const( fw ) ) ) ){
			for( DoubleVector3& v : fw ){
				v = plane3_project_point( fplane, v );
			}
		}

		splanes.resize( fw.size() );

		/* sanity check */
		if ( triangle_min_angle_squared_sin( *a, *b, *c ) < 1e-8 ) // degenerate triangle
			return false;

		return true;
	}

	void add_back_plane( const Vector3& bestNormal ){
		bplane = plane3_flipped( fplane );
		bplane.dist() += vector3_dot( bestNormal, fplane.normal() ) * clipDepth;
		bw = fw;
		for( DoubleVector3& v : bw )
			v -= bestNormal * clipDepth;
	}

	bool create_brush() const {
		const bool doBack = bplane.normal() != g_vector3_identity;
		/* set up brush sides */
		buildBrush.sides.clear(); // clear, so resize() will value-initialize elements
		buildBrush.sides.resize( splanes.size() + 1 + doBack );

		if( debugClip ){
			buildBrush.sides[0].shaderInfo = &ShaderInfoForShader( "debugclip2" );
			for ( size_t i = 1; i < buildBrush.sides.size(); ++i )
				buildBrush.sides[i].shaderInfo = &ShaderInfoForShader( "debugclip" );
		}
		else{
			buildBrush.sides[0].shaderInfo = &si;
			buildBrush.sides[0].surfaceFlags = si.surfaceFlags;
			for ( size_t i = 1; i < buildBrush.sides.size(); ++i )
				buildBrush.sides[i].shaderInfo = nullptr;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
		}

		buildBrush.sides[0].planenum = FindFloatPlane( fplane, fw );
		// buildBrush.sides[0].plane = Plane3( fplane );
		for( size_t i = 0; i < splanes.size(); ++i ){
			buildBrush.sides[i + 1].planenum = FindFloatPlane( Plane3f( splanes[i] ), std::array{ fw[i], winding_next_point( fw, i ) } );
			// buildBrush.sides[i + 1].plane = splanes[i]; // this only improves debug windings quality, but it's better to respect actual bsp planes
		}
		if( doBack ){
			buildBrush.sides.back().planenum = FindFloatPlane( bplane, bw );
			// buildBrush.sides.back().plane = Plane3( bplane );
		}

		/* add to entity */
		if ( CreateBrushWindings( buildBrush ) ) {
			AddBrushBevels();
			brush_t& newBrush = entity.brushes.emplace_front( buildBrush );
			newBrush.original = &newBrush;
			return true;
		}
		return false;
	}
};


static void clipModel_default( ClipSides& cs ){
	// axial normal
	DoubleVector3 bestNormal = cs.fplane.normal();
	normal_make_axial( bestNormal );

	/* make side planes */
	for ( size_t i = 0; i < cs.fw.size(); ++i )
	{
		cs.splanes[i].normal() = VectorNormalized( vector3_cross( bestNormal, winding_next_point( cs.fw, i ) - cs.fw[i] ) );
		cs.splanes[i].dist() = vector3_dot( cs.fw[i], cs.splanes[i].normal() );
	}

	/* make back plane */
	cs.add_back_plane( bestNormal );
}

static void clipModel_pyramidal( ClipSides& cs ){
	/* calculate center */
	DoubleVector3 cnt = WindingCentroid( cs.fw );
	/* make back pyramid point */
	cnt -= cs.fplane.normal() * cs.clipDepth;

	/* make side planes */
	for ( size_t i = 0; i < cs.fw.size(); ++i )
	{
		PlaneFromPoints( cs.splanes[i], winding_next_point( cs.fw, i ), cs.fw[i], cnt );
#if 0 // no definite profit
		const auto susNormal = []( float a, float b ){ return ( a != 0 || b != 0 ) && std::fabs( a ) < .00025f && std::fabs( b ) < .00025f; };
		if( susNormal( cs.splanes[i].a, cs.splanes[i].b )
		 || susNormal( cs.splanes[i].a, cs.splanes[i].c )
		 || susNormal( cs.splanes[i].b, cs.splanes[i].c ) ){
			cnt -= cs.fplane.normal() * .125; // shift, if produces sus sides, since extreme angle with front
			i = -1; // restart loop
		 }
#endif
	}
}

static void clipModel_faceNormals( ClipSides& cs ){
	/* make side planes */
	for ( size_t i = 0; i < cs.fw.size(); ++i )
	{
		cs.splanes[i].normal() = VectorNormalized( vector3_cross( DoubleVector3( cs.fplane.normal() ), winding_next_point( cs.fw, i ) - cs.fw[i] ) );
		cs.splanes[i].dist() = vector3_dot( cs.fw[i], cs.splanes[i].normal() );
	}

	/* make back plane */
	cs.add_back_plane( cs.fplane.normal() );
}

static void clipModel_vertexNormals( ClipSides& cs, const std::array<Vector3, 3>& Vnorm, bool noOutwardsCheck ){
	std::array<Vector3, 3> Enorm;

	//avg normals for side planes
	for ( int i = 0; i < 3; ++i )
	{
		Enorm[i] = VectorNormalized( Vnorm[i] + Vnorm[( i + 1 ) % 3] );
		//check fuer bad ones
		const Vector3 nrm = VectorNormalized( vector3_cross( cs.fplane.normal(), cs.fw[( i + 1 ) % 3] - cs.fw[i] ) );
		//check for negative or outside direction
		if ( vector3_dot( Enorm[i], cs.fplane.normal() ) > 0.1 ){
			if ( ( vector3_dot( Enorm[i], nrm ) > -0.2 ) || noOutwardsCheck ){
				//ok++;
				continue;
			}
		}
		//notok++;
		//Sys_Printf( "faulty Enormal %i/%i\n", notok, ok );
		//use 45 normal
		Enorm[i] = VectorNormalized( cs.fplane.normal() + nrm );
	}

	/* make side planes */
	for ( int i = 0; i < 3; ++i )
	{
		cs.splanes[i].normal() = VectorNormalized( vector3_cross( DoubleVector3( Enorm[i] ), cs.fw[( i + 1 ) % 3] - cs.fw[i] ) );
		cs.splanes[i].dist() = vector3_dot( cs.fw[i], cs.splanes[i].normal() );
	}

	/* make back plane */
	cs.add_back_plane( cs.fplane.normal() );
}

static void clipModel_45( ClipSides& cs ){
	/* 45 degrees normals for side planes */
	for ( size_t i = 0; i < cs.fw.size(); ++i )
	{
		const DoubleVector3 enrm = VectorNormalized( vector3_cross( DoubleVector3( cs.fplane.normal() ), winding_next_point( cs.fw, i ) - cs.fw[i] ) );
		/* make side planes */
		cs.splanes[i].normal() = VectorNormalized( enrm - cs.fplane.normal() );
		cs.splanes[i].dist() = vector3_dot( cs.fw[i], cs.splanes[i].normal() );
	}

	/* make back plane */
	cs.add_back_plane( cs.fplane.normal() );
}

static void clipModel_terrain( ClipSides& cs, const int spf, size_t axis, const Vector3& avgDirection, const MinMax& minmax, const float limDepth ){
	Vector3 bestNormal;

	if ( spf & eExtrudeTerrain ){ //autodirection
		bestNormal = avgDirection;
	}
	else{
		axis = 2;
		if ( ( spf & eExtrudeDownwards ) && ( spf & eExtrudeUpwards ) ){
			bestNormal = cs.fplane.normal().z() >= 0? g_vector3_axis_z : -g_vector3_axis_z;
		}
		else if ( spf & eExtrudeDownwards ){
			bestNormal = g_vector3_axis_z;
		}
		else if ( spf & eExtrudeUpwards ){
			bestNormal = -g_vector3_axis_z;
		}
		else{ // best axial normal
			bestNormal = cs.fplane.normal();
			axis = normal_make_axial( bestNormal );
		}
	}

	if ( vector3_dot( cs.fplane.normal(), bestNormal ) < 0.05 ){
		return clipModel_default( cs );
	}


	/* make side planes */
	for ( size_t i = 0; i < cs.fw.size(); ++i )
	{
		cs.splanes[i].normal() = VectorNormalized( vector3_cross( DoubleVector3( bestNormal ), winding_next_point( cs.fw, i ) - cs.fw[i] ) );
		cs.splanes[i].dist() = vector3_dot( cs.fw[i], cs.splanes[i].normal() );
	}

	/* make back plane */
	if ( spf & eMaxExtrude ){ //max extrude
		cs.bplane.normal() = -bestNormal;
		if ( bestNormal[axis] > 0 )
			cs.bplane.dist() = -minmax.mins[axis] + cs.clipDepth;
		else
			cs.bplane.dist() = minmax.maxs[axis] + cs.clipDepth;
	}
	else if ( spf & eAxialBackplane ){ //axial backplane
		cs.bplane.normal() = -bestNormal;
		cs.bplane.dist() = cs.fw[0][axis];
		if ( bestNormal[axis] > 0 ){
			for ( size_t i = 1; i < cs.fw.size(); ++i ){
				value_minimize( cs.bplane.dist(), float( cs.fw[i][axis] ) );
			}
			cs.bplane.dist() = -cs.bplane.dist() + cs.clipDepth;
		}
		else{
			for ( size_t i = 1; i < cs.fw.size(); ++i ){
				value_maximize( cs.bplane.dist(), float( cs.fw[i][axis] ) );
			}
			cs.bplane.dist() += cs.clipDepth;
		}
		if ( limDepth != 0 ){
			Vector3 cnt = cs.fw[0];
			if ( bestNormal[axis] > 0 ){
				for ( size_t i = 1; i < cs.fw.size(); ++i )
					if ( cs.fw[i][axis] > cnt[axis] )
						cnt = cs.fw[i];
			}
			else {
				for ( size_t i = 1; i < cs.fw.size(); ++i )
					if ( cs.fw[i][axis] < cnt[axis] )
						cnt = cs.fw[i];
			}
			cnt = plane3_project_point( cs.bplane, cnt );
			if ( -plane3_distance_to_point( cs.fplane, cnt ) > limDepth ){	//normal backplane
				cs.add_back_plane( bestNormal );
			}
		}
	}
	else{	//normal backplane
		cs.add_back_plane( bestNormal );
	}
}

static void clipModel_axialPyramid( ClipSides& cs, const float limDepth ){
	for ( int i = 0; i < 3; ++i )
		if ( std::fabs( cs.fplane.normal()[i] ) < 0.05f && std::fabs( cs.fplane.normal()[( i + 1 ) % 3] ) < 0.05f ) //no way, close to lay on two axes
			return clipModel_default( cs );

	// best axial normal
	Vector3 bestNormal = cs.fplane.normal();
	const size_t axis = normal_make_axial( bestNormal );

	float mindist = 999999;

	for ( size_t i = 0; i < cs.fw.size(); ++i ){ // planes
		float bestdist = 999999, bestangle = 1;

		for ( size_t j = 0; j < 3; ++j ){ // axes
			Plane3f pln;
			Vector3 nrm = winding_next_point( cs.fw, i ) - cs.fw[i];
			if ( j == axis ){
				pln.normal() = VectorNormalized( vector3_cross( bestNormal, nrm ) );
			}
			else{
				Vector3 vnrm( 0 );
				if ( ( j + 1 ) % 3 == axis ){
					if ( nrm[( j + 2 ) % 3] == 0 )
						continue;
					vnrm[( j + 2 ) % 3] = nrm[( j + 2 ) % 3];
				}
				else{
					if ( nrm[( j + 1 ) % 3] == 0 )
						continue;
					vnrm[( j + 1 ) % 3] = nrm[( j + 1 ) % 3];
				}
				const Vector3 enrm = vector3_cross( bestNormal, vnrm );
				pln.normal() = VectorNormalized( vector3_cross( enrm, nrm ) );
			}
			pln.dist() = vector3_dot( cs.fw[i], pln.normal() );
			//check facing, thickness
			const float currdist = -plane3_distance_to_point( pln, cs.fw[( i + 2 ) % cs.fw.size()] );
			const float currangle = vector3_dot( pln.normal(), cs.fplane.normal() );
			if ( ( ( currdist > 0.1 ) && ( currdist < bestdist ) && ( currangle < 0 ) ) ||
			     ( ( currangle >= 0 ) && ( currangle <= bestangle ) ) ){
				bestangle = currangle;
				if ( currangle < 0 )
					bestdist = currdist;
				cs.splanes[i] = Plane3( pln );
			}
		}
		if ( bestdist == 999999 && bestangle == 1 ){
			// Sys_Printf( "default_CLIPMODEL\n" );
			return clipModel_default( cs );
		}
		value_minimize( mindist, bestdist );
	}
	if ( ( limDepth != 0 ) && ( mindist > limDepth ) )
		return clipModel_default( cs );
}


struct ClipTriangle
{
	Plane3 plane;
	std::array<DoubleVector3, 3> points;
	mapDrawSurface_t *ds;

	bool operator<( const ClipTriangle& other ) const {
		return plane.dist() > other.plane.dist(); // decreasing order (to iterate from the end)
	}
};


struct ClipTriangles
{
	// separate by surfaceFlags, contentFlags, compileFlags, sort by triangle plane distance
	std::map<std::tuple<int, int, int>, std::vector<ClipTriangle>> triangleSets;
	std::vector<mapDrawSurface_t*> modelSurfs;
};


inline bool clipflags_doClip( const shaderInfo_t& si, const int spawnFlags ){
	const int spf = ( spawnFlags & ( eClipFlags & ~eClipModel ) ); // w/e eClipModel flag, if others are set

	const bool fineFlags =
		( si.clipModel && spf == 0 ) // default CLIPMODEL
		|| ( spawnFlags & eClipFlags ) == eClipModel // default CLIPMODEL
		|| spf == eExtrudeFaceNormals
		|| spf == eExtrudeTerrain
		|| spf == eExtrudeVertexNormals
		|| spf == ePyramidalClip
		|| spf == eExtrudeDownwards
		|| spf == eExtrudeUpwards
		|| spf == eAxialBackplane // default sides + axial backplane
		|| spf == ( eExtrudeFaceNormals | ePyramidalClip ) // extrude 45
		|| spf == ( eExtrudeTerrain | eMaxExtrude )
		|| spf == ( eExtrudeTerrain | eAxialBackplane )
		|| spf == ( eExtrudeVertexNormals | ePyramidalClip ) // vertex normals + don't check for sides, sticking outwards
		|| spf == ( ePyramidalClip | eAxialBackplane ) // pyramid with 3 of 4 sides axial (->small bsp)
		|| spf == ( eExtrudeDownwards | eExtrudeUpwards )
		|| spf == ( eExtrudeDownwards | eMaxExtrude )
		|| spf == ( eExtrudeDownwards | eAxialBackplane )
		|| spf == ( eExtrudeDownwards | eExtrudeUpwards | eMaxExtrude )
		|| spf == ( eExtrudeDownwards | eExtrudeUpwards | eAxialBackplane )
		|| spf == ( eExtrudeUpwards | eMaxExtrude )
		|| spf == ( eExtrudeUpwards | eAxialBackplane );

	if( ( spawnFlags & eClipFlags ) && !fineFlags )
		Sys_Warning( "nonexistent clipping mode selected\n" );

	return ( ( si.compileFlags & C_SOLID ) || si.clipModel ) /* skip nonsolid */ && fineFlags;
}


/* ydnar: giant hack land: generate clipping brushes for model triangles */
static void ClipModel( const int spawnFlags, float clipDepth, ClipTriangles& clipTriangles, const char *modelName, entity_t& entity ){
	const int spf = ( spawnFlags & ( eClipFlags & ~eClipModel ) ); // w/e eClipModel flag, if others are set

	float limDepth = 0;
	if ( clipDepth < 0 ){
		limDepth = -clipDepth;
		clipDepth = 2.f;
	}

	// mergable triangles support
	if( ( /* si.clipModel && */ spf == 0 ) // default CLIPMODEL
		|| ( spawnFlags & eClipFlags ) == eClipModel //default CLIPMODEL
		|| spf == eExtrudeFaceNormals
		|| spf == ePyramidalClip
		|| spf == ( eExtrudeFaceNormals | ePyramidalClip ) // extrude 45
	){
		//? consider MAX_BUILD_SIDES MAX_POINTS_ON_WINDING
		//? try to merge windings too
		for( auto& [ _, triSet ] : clipTriangles.triangleSets )
		{
			std::sort( triSet.begin(), triSet.end() );
			while( !triSet.empty() )
			{
				ClipTriangle tri1st = triSet.back();
				triSet.pop_back();
				ClipSides cs( *tri1st.ds->shaderInfo, entity, clipDepth );
				winding_accu_t& w = cs.fw;
				w.assign( tri1st.points.cbegin(), tri1st.points.cend() );
				// try to merge some triangles
				for( auto tri = triSet.crbegin(); tri != triSet.crend(); ++tri ){
					// sorted by distance; break on distance range overflow
					if( tri->plane.dist() > tri1st.plane.dist() + 1 ) // big epsilon, points still can be there
						break;
					// points off plane
					// const double epsilon = distanceEpsilon * 2;
					const double epsilon = ON_EPSILON / 2;
					if( std::fabs( plane3_distance_to_point( tri1st.plane, tri->points[0] ) ) > epsilon
					 || std::fabs( plane3_distance_to_point( tri1st.plane, tri->points[1] ) ) > epsilon
					 || std::fabs( plane3_distance_to_point( tri1st.plane, tri->points[2] ) ) > epsilon )
						continue;
					// rough normal check; only catches inverted planes with dist 0?
					if( !vector3_equal_epsilon( tri1st.plane.normal(), tri->plane.normal(), .1 ) )
						continue;
					// find matching points
					for( auto prev = w.end() - 1, next = w.begin(); next != w.end(); prev = next++ )
					{
						for( auto pre = tri->points.cend() - 1, nex = tri->points.cbegin(); nex != tri->points.cend(); pre = nex++ )
						{
							if( VectorCompare( *prev, *nex ) && VectorCompare( *next, *pre ) ){ // source points are typically perfectly equal hence small epsilon
							// if( vector3_equal_epsilon( *prev, *nex, ON_EPSILON ) && vector3_equal_epsilon( *next, *pre, ON_EPSILON ) ){
								auto newPoint = ( nex + 1 != tri->points.cend() )? nex + 1 : tri->points.cbegin();
								auto nnext = winding_next( w, next );
								auto pprev = winding_prev( w, prev );
								// check if new point preserves convexity
								Plane3 pplane( VectorNormalized( vector3_cross( tri1st.plane.normal(), *newPoint - *pprev ) ), 0 );
								pplane.dist() = vector3_dot( pplane.normal(), *pprev );
								Plane3 nplane( VectorNormalized( vector3_cross( tri1st.plane.normal(), *nnext - *newPoint ) ), 0 );
								nplane.dist() = vector3_dot( nplane.normal(), *nnext );
								double pd = plane3_distance_to_point( pplane, *prev );
								double nd = plane3_distance_to_point( nplane, *next );
								// insert
								if( pd > -ON_EPSILON && nd > -ON_EPSILON ){
									auto inserted = w.insert( next, *newPoint );
									triSet.erase( ( ++tri ).base() );
									// inserted, restart the search
									tri = triSet.crbegin() - 1;
									// remove possible colinear points
									auto inext = winding_next( w, inserted );
									auto iprev = winding_prev( w, inserted );
									// remove higher iterator 1st to keep lower one valid
									if( iprev > inext ){
										std::swap( iprev, inext );
										std::swap( pd, nd );
									}
									if( std::fabs( nd ) < ON_EPSILON )
										w.erase( inext );
									if( std::fabs( pd ) < ON_EPSILON && w.size() > 3 )
										w.erase( iprev );
								}
								goto doNextTriangle;
							}
						}
					}
					doNextTriangle:		continue;
				}

				//% CheckWinding( CopyWindingAccuToRegular( cs.fw ) );

				/* make plane for triangle */
				if ( cs.construct() ) {
					if ( ( /* si.clipModel && */ spf == 0 ) || ( spawnFlags & eClipFlags ) == eClipModel ){	// default CLIPMODEL
						clipModel_default( cs );
					}
					else if ( spf == eExtrudeFaceNormals ){
						clipModel_faceNormals( cs );
					}
					else if ( spf == ePyramidalClip ){
						clipModel_pyramidal( cs );
					}
					else if ( spf == ( eExtrudeFaceNormals | ePyramidalClip ) ){ // extrude 45
						clipModel_45( cs );
					}

					if ( cs.create_brush() ) {
						continue; // success
					}
				}
				Sys_Warning( "triangle (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) of %s was not autoclipped\n",
				             cs.fw[0][0], cs.fw[0][1], cs.fw[0][2],
				             cs.fw[1][0], cs.fw[1][1], cs.fw[1][2],
				             cs.fw[2][0], cs.fw[2][1], cs.fw[2][2], modelName );
			}
		}
	}
	else{ // no mergable triangles support
		for( mapDrawSurface_t *ds : clipTriangles.modelSurfs )
		{
			MinMax minmax;
			Vector3 avgDirection( 0 );
			size_t axis;

			//wont snap these in normal way, or will explode
			// const double normalEpsilon_save = normalEpsilon;
			//normalEpsilon = 0.000001;

			if ( ( spf & eMaxExtrude ) || ( spf & eExtrudeTerrain ) ){

				for ( auto idx = ds->indexes.cbegin(); idx != ds->indexes.cend(); idx += 3 ){
					const Vector3 points[3]{ ds->verts[*( idx + 0 )].xyz,
					                         ds->verts[*( idx + 1 )].xyz,
					                         ds->verts[*( idx + 2 )].xyz };
					if ( Plane3f plane; PlaneFromPoints( plane, points ) ){
						if ( spf & eExtrudeTerrain )
							avgDirection += plane.normal();	// calculate average mesh facing direction

						for ( const auto& p : points ) // get min/max
							minmax.extend( p );
					}
				}
				//unify avg direction
				if ( spf & eExtrudeTerrain ){
					if ( avgDirection == g_vector3_identity )
						avgDirection = g_vector3_axis_z;
					axis = normal_make_axial( avgDirection );
				}
			}

			/* walk triangle list */
			for ( auto idx = ds->indexes.cbegin(); idx != ds->indexes.cend(); idx += 3 )
			{
				ClipSides cs( *ds->shaderInfo, entity, clipDepth );
				/* make points */
				cs.fw.assign( { ds->verts[*( idx + 0 )].xyz,
				                ds->verts[*( idx + 1 )].xyz,
				                ds->verts[*( idx + 2 )].xyz } );

				/* make plane for triangle */
				if ( cs.construct() ) {
					if ( spf == ( ePyramidalClip | eAxialBackplane ) ){ // pyramid with 3 of 4 sides axial (->small bsp)
						clipModel_axialPyramid( cs, limDepth );
					}
					else if ( spf == eExtrudeTerrain
					       || spf == eExtrudeDownwards
					       || spf == eExtrudeUpwards
					       || spf == eAxialBackplane
					       || spf == ( eExtrudeTerrain | eMaxExtrude )
					       || spf == ( eExtrudeTerrain | eAxialBackplane )
					       || spf == ( eExtrudeDownwards | eExtrudeUpwards )
					       || spf == ( eExtrudeDownwards | eMaxExtrude )
					       || spf == ( eExtrudeDownwards | eAxialBackplane )
					       || spf == ( eExtrudeDownwards | eExtrudeUpwards | eMaxExtrude )
					       || spf == ( eExtrudeDownwards | eExtrudeUpwards | eAxialBackplane )
					       || spf == ( eExtrudeUpwards | eMaxExtrude )
					       || spf == ( eExtrudeUpwards | eAxialBackplane ) ){
						clipModel_terrain( cs, spf, axis, avgDirection, minmax, limDepth );
					}
					else if ( spf == eExtrudeVertexNormals
					       || spf == ( eExtrudeVertexNormals | ePyramidalClip ) ){ // vertex normals + don't check for sides, sticking outwards
						clipModel_vertexNormals( cs, { ds->verts[*( idx + 0 )].normal,
						                               ds->verts[*( idx + 1 )].normal,
						                               ds->verts[*( idx + 2 )].normal }, spf & ePyramidalClip );
					}

					if ( cs.create_brush() ) {
						continue; // success
					}
				}
				Sys_Warning( "triangle (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) of %s was not autoclipped\n",
				             cs.fw[0][0], cs.fw[0][1], cs.fw[0][2],
				             cs.fw[1][0], cs.fw[1][1], cs.fw[1][2],
				             cs.fw[2][0], cs.fw[2][1], cs.fw[2][2], modelName );
			}
			// normalEpsilon = normalEpsilon_save;
		}
	}
}

/*
   InsertModel() - ydnar
   adds a picomodel into the bsp
 */

void InsertModel( const char *name, const char *skin, int frame, const Matrix4& transform, const std::list<remap_t> *remaps,
                  entity_t& entity, int spawnFlags, float clipDepth, const EntityCompileParams& params ){

	const Matrix4 nTransform( matrix4_for_normal_transform( transform ) );
	const bool transform_lefthanded = MATRIX4_LEFTHANDED == matrix4_handedness( transform );
	AssModel            *model;
	const char          *picoShaderName;


	/* get model */
	model = LoadModel( name, frame );
	if ( model == nullptr ) {
		return;
	}

	/* load skin file */
	std::list<remap_t> skins;
	if( !strEmptyOrNull( skin ) ){
		const bool isnumber = std::all_of( skin, skin + strlen( skin ), ::isdigit );

		StringOutputStream skinfilename( 99 );
		if( isnumber )
			skinfilename( name, '_', skin, ".skin" ); // DarkPlaces naming: models/relics/relic.md3_14.skin for models/relics/relic.md3
		else
			skinfilename( PathExtensionless( name ), '_', skin, ".skin" ); // Q3 naming: models/players/sarge/head_roderic.skin for models/players/sarge/head.md3

		if ( MemBuffer skinfile = vfsLoadFile( skinfilename ) ) {
			Sys_Printf( "Using skin %s of %s\n", skin, name );
			for ( char *skinfilenextptr, *skinfileptr = skinfile.data(); !strEmpty( skinfileptr ); skinfileptr = skinfilenextptr )
			{
				// for sscanf
				char format[64];

				skinfilenextptr = strchr( skinfileptr, '\r' );
				if ( skinfilenextptr != nullptr ) {
					strClear( skinfilenextptr++ );
					if( *skinfilenextptr == '\n' ) // handle \r\n
						++skinfilenextptr;
				}
				else
				{
					skinfilenextptr = strchr( skinfileptr, '\n' );
					if ( skinfilenextptr != nullptr ) {
						strClear( skinfilenextptr++ );
					}
					else{
						skinfilenextptr = skinfileptr + strlen( skinfileptr );
					}
				}

				/* create new item */
				remap_t skin;

				sprintf( format, "replace %%%ds %%%ds", (int)sizeof( skin.from ) - 1, (int)sizeof( skin.to ) - 1 );
				if ( sscanf( skinfileptr, format, skin.from, skin.to ) == 2 ) {
					skins.push_back( skin );
					continue;
				}
				sprintf( format, " %%%d[^,  ] ,%%%ds", (int)sizeof( skin.from ) - 1, (int)sizeof( skin.to ) - 1 );
				if ( sscanf( skinfileptr, format, skin.from, skin.to ) == 2 ) {
					skins.push_back( skin );
					continue;
				}

				/* invalid input line -> discard skin struct */
				Sys_Printf( "Discarding skin directive in %s: %s\n", skinfilename.c_str(), skinfileptr );
			}
		}
	}

	ClipTriangles clipTriangles;
	/* each surface on the model will become a new map drawsurface */
	//%	Sys_FPrintf( SYS_VRB, "Model %s has %d surfaces\n", name, numSurfaces );
	for ( const auto& surface : model->m_meshes )
	{
		const aiMesh *mesh = surface.m_mesh;
		/* only handle triangle surfaces initially (fixme: support patches) */

		/* get shader name */
		picoShaderName = surface.m_shader.c_str();

		/* handle .skin file */
		if ( !skins.empty() ) {
			picoShaderName = nullptr;
			for( const auto& skin : skins )
			{
				if ( striEqual( surface.m_shader.c_str(), skin.from ) ) {
					Sys_FPrintf( SYS_VRB, "Skin file: mapping %s to %s\n", surface.m_shader.c_str(), skin.to );
					picoShaderName = skin.to;
					break;
				}
			}
			if ( picoShaderName == nullptr ) {
				Sys_FPrintf( SYS_VRB, "Skin file: not mapping %s\n", surface.m_shader.c_str() );
				continue;
			}
		}

		/* handle shader remapping */
		if( remaps != nullptr ){
			const char* to = nullptr;
			size_t fromlen = 0;
			for( const auto& rm : *remaps )
			{
				if ( strEqual( rm.from, "*" ) && fromlen == 0 ) { // only globbing, if no respective match
					to = rm.to;
				}
				else if( striEqualSuffix( picoShaderName, rm.from ) && strlen( rm.from ) > fromlen ){ // longer match has priority
					to = rm.to;
					fromlen = strlen( rm.from );
				}
			}
			if( to != nullptr ){
				Sys_FPrintf( SYS_VRB, ( fromlen == 0? "Globbing '%s' to '%s'\n" : "Remapping '%s' to '%s'\n" ), picoShaderName, to );
				picoShaderName = to;
			}
		}

		/* shader renaming for sof2 */
		shaderInfo_t& si = renameModelShaders
			? ShaderInfoForShader( String64( PathExtensionless( picoShaderName ), ( spawnFlags & eRMG_BSP )? "_RMG_BSP" : "_BSP" ) )
			: ShaderInfoForShader( picoShaderName );

		/* allocate a surface (ydnar: gs mods) */
		mapDrawSurface_t& ds = AllocDrawSurface( ESurfaceType::Triangles );
		ds.entityNum = entity.mapEntityNum;
		ds.castShadows  = params.castShadows;
		ds.recvShadows  = params.recvShadows;
		ds.celShader    = params.celShader;
		ds.ambientColor = params.ambientColor;

		/* set shader */
		ds.shaderInfo = &si;

		/* force to meta? */
		if ( si.forceMeta || ( spawnFlags & eForceMeta ) ) { /* 3rd bit */
			ds.type = ESurfaceType::ForcedMeta;
		}

		/* fix the surface's normals (jal: conditioned by shader info) */
		if ( !( spawnFlags & eNoSmooth ) && ( params.shadeAngle == 0 || ds.type != ESurfaceType::ForcedMeta ) ) {
			// PicoFixSurfaceNormals( surface );
		}

		/* set sample size */
		if ( params.lightmapSampleSize > 0 ) {
			ds.sampleSize = params.lightmapSampleSize;
		}

		/* set lightmap scale */
		if ( params.lightmapScale > 0 ) {
			ds.lightmapScale = params.lightmapScale;
		}

		/* set shading angle */
		if ( params.shadeAngle > 0 ) {
			ds.shadeAngleDegrees = params.shadeAngle;
		}

		/* set particulars */
		ds.verts.resize( mesh->mNumVertices, c_bspDrawVert_t0 );
		ds.indexes.resize( mesh->mNumFaces * 3 );
// Sys_Printf( "verts %zu idx %zu\n", ds.verts.size(), ds.indexes.size() );
		/* copy vertexes */
		for ( size_t i = 0; i < ds.verts.size(); ++i )
		{
			/* get vertex */
			bspDrawVert_t& dv = ds.verts[ i ];

			/* xyz and normal */
			dv.xyz = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
			matrix4_transform_point( transform, dv.xyz );

			if( mesh->HasNormals() ){
				dv.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
				matrix4_transform_direction( nTransform, dv.normal );
				VectorNormalize( dv.normal );
			}

			/* ydnar: tek-fu celshading support for flat shaded shit */
			if ( flat ) {
				dv.st = si.stFlat;
			}

			/* ydnar: gs mods: added support for explicit shader texcoord generation */
			else if ( si.tcGen ) {
				/* project the texture */
				dv.st[ 0 ] = vector3_dot( si.vecs[ 0 ], dv.xyz );
				dv.st[ 1 ] = vector3_dot( si.vecs[ 1 ], dv.xyz );
			}

			/* normal texture coordinates */
			else
			{
				if( mesh->HasTextureCoords( 0 ) )
					dv.st = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			}

			/* set lightmap/color bits */
			{
				const aiColor4D color = mesh->HasVertexColors( 0 )? mesh->mColors[0][i] : aiColor4D( 1 );
				if ( spawnFlags & eColorToAlpha ) { // spawnflag 32: model color -> alpha hack
					dv.color[ 0 ] = { 255, 255, 255, color_to_byte( RGBTOGRAY( color ) * 255 ) };
				}
				else
				{
					dv.color[ 0 ] = { color_to_byte( color[0] * 255 ),
					                  color_to_byte( color[1] * 255 ),
					                  color_to_byte( color[2] * 255 ),
					                  color_to_byte( color[3] * 255 ) };
				}
				dv.color[ 1 ] = dv.color[ 2 ] = dv.color[ 3 ] = dv.color[ 0 ];
			}
		}

		/* copy indexes */
		for ( size_t idCopied = 0; const aiFace& face : Span( mesh->mFaces, mesh->mNumFaces ) ){
			// if( face.mNumIndices == 3 )
			for ( size_t i = 0; i < 3; ++i ){
				ds.indexes[idCopied++] = face.mIndices[i];
			}
			if( transform_lefthanded ){
				std::swap( ds.indexes[idCopied - 1], ds.indexes[idCopied - 2] );
			}
		}

		if( clipflags_doClip( si, spawnFlags) ){
			clipTriangles.modelSurfs.push_back( &ds );

			const auto key = std::tuple{ ds.shaderInfo->surfaceFlags,
			                             ds.shaderInfo->contentFlags,
			                             ds.shaderInfo->compileFlags };
			auto& triangles = clipTriangles.triangleSets[ key ];
			for ( const aiFace& face : Span( mesh->mFaces, mesh->mNumFaces ) )
			{
				std::array<DoubleVector3, 3> points;
				for( size_t i = 0; i < 3; ++i ){
					auto& v = mesh->mVertices[face.mIndices[i]];
					points[i] = matrix4_transformed_point( transform, DoubleVector3( v.x, v.y, v.z ) );
				}
				if( transform_lefthanded ){
					std::swap( points[1], points[2] );
				}
				if ( Plane3 plane; PlaneFromPoints( plane, points.data() ) ){
					triangles.emplace_back( ClipTriangle{ .plane = plane, .points = points, .ds = &ds } );
				}
			}
		}
	}

	ClipModel( spawnFlags, clipDepth, clipTriangles, name, entity );
}


Matrix4 ModelGetTransform( const entity_t& e, const Vector3& parent_origin /* = g_vector3_identity */ ){
	/* get origin */
	const Vector3 origin = e.vectorForKey( "origin" ) - parent_origin;    /* offset by parent, it will be added ingame */

	/* get scale */
	Vector3 scale( 1 );
	if( !e.read_keyvalue( scale, "modelscale_vec" ) )
		if( e.read_keyvalue( scale[0], "modelscale" ) )
			scale[1] = scale[2] = scale[0];

	/* get "angle" (yaw) or "angles" (pitch yaw roll), store as (roll pitch yaw) */
	Vector3 angles( 0 );
	if ( e.read_keyvalue( angles, "angles" ) || e.read_keyvalue( angles.y(), "angle" ) )
		angles = angles_pyr2rpy( angles );

	/* set transform matrix (thanks spog) */
	Matrix4 transform( g_matrix4_identity );
	matrix4_transform_by_euler_xyz_degrees( transform, origin, angles, scale );
	return transform;
}


/*
   AddTriangleModels()
   adds misc_model surfaces to the bsp
 */

void AddTriangleModels( entity_t& eparent ){
	/* note it */
	Sys_FPrintf( SYS_VRB, "--- AddTriangleModels ---\n" );

	/* get current brush entity targetname */
	const char *targetName;
	if ( &eparent == &entities[0] ) {
		targetName = "";
	}
	else{  /* misc_model entities target non-worldspawn brush model entities */
		if ( !eparent.read_keyvalue( targetName, "_targetname", "targetname" ) ) {
			return;
		}
	}

	/* walk the entity list */
	for ( std::size_t i = 1; i < entities.size(); ++i )
	{
		/* get entity */
		const entity_t& e = entities[ i ];

		/* convert misc_models into raw geometry */
		if ( !e.classname_is( "misc_model" ) ) {
			continue;
		}

		/* ydnar: added support for md3 models on non-worldspawn models */
		if ( const char *target = ""; e.read_keyvalue( target, "_target", "target" ), !strEqual( target, targetName ) ) {
			continue;
		}

		/* get model name */
		const char *model;
		if ( !e.read_keyvalue( model, "model" ) ) {
			Sys_Warning( "entity#%d misc_model without a model key\n", e.mapEntityNum );
			continue;
		}

		/* get model frame */
		const int frame = e.intForKey( "_frame", "frame" );

		/* get spawnflags */
		const int spawnFlags = e.intForKey( "spawnflags" );

		/* get shader remappings */
		std::list<remap_t> remaps;
		for ( const auto& ep : e.epairs )
		{
			/* look for keys prefixed with "_remap" */
			if ( striEqualPrefix( ep.key.c_str(), "_remap" ) ) {
				/* create new remapping */
				remap_t remap;
				strcpy( remap.from, ep.value.c_str() );

				/* split the string */
				char *split = strchr( remap.from, ';' );
				if ( split == nullptr ) {
					Sys_Warning( "Shader _remap key found in misc_model without a ; character: '%s'\n", remap.from );
					continue;
				}
				else if( split == remap.from ){
					Sys_Warning( "_remap FROM is empty in '%s'\n", remap.from );
					continue;
				}
				else if( strEmpty( split + 1 ) ){
					Sys_Warning( "_remap TO is empty in '%s'\n", remap.from );
					continue;
				}
				else if( strlen( split + 1 ) >= sizeof( remap.to ) ){
					Sys_Warning( "_remap TO is too long in '%s'\n", remap.from );
					continue;
				}

				/* store the split */
				strClear( split );
				strcpy( remap.to, ( split + 1 ) );
				remaps.push_back( remap );

				/* note it */
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", remap->from, remap->to );
			}
		}

		const char *skin = nullptr;
		e.read_keyvalue( skin, "_skin", "skin" );

		float clipDepth = clipDepthGlobal;
		if ( e.read_keyvalue( clipDepth, "_clipdepth" ) )
			Sys_Printf( "misc_model %s has autoclip depth of %.3f\n", model, clipDepth );

		const EntityCompileParams params = ParseEntityCompileParams( e, &eparent, &eparent == &entities[ 0 ] );

		/* insert the model */
		InsertModel( model, skin, frame, ModelGetTransform( e, eparent.origin ), &remaps, eparent, spawnFlags, clipDepth, params );
	}
}

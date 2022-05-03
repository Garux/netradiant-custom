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
	struct AssModelMesh : public AssMeshWalker
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
			    aiReturn_SUCCESS == material->Get( AI_MATKEY_TEXTURE_DIFFUSE(0), texname )
			 && texname.length != 0
			 && !string_equal_prefix_nocase( matname.C_Str(), "textures/" ) /* matname looks intentionally named as ingame shader */
			 && !string_equal_prefix_nocase( matname.C_Str(), "textures\\" )
			 && !string_equal_prefix_nocase( matname.C_Str(), "models/" )
			 && !string_equal_prefix_nocase( matname.C_Str(), "models\\" ) ){
#ifdef _DEBUG
							Sys_Printf( "texname: %s\n", texname.C_Str() );
#endif
				m_shader = StringOutputStream()( PathCleaned( PathExtensionless( texname.C_Str() ) ) );

			}
			else{
				m_shader = StringOutputStream()( PathCleaned( PathExtensionless( matname.C_Str() ) ) );
			}

			const CopiedString oldShader( m_shader );
			if( strchr( m_shader.c_str(), '/' ) == nullptr ){ /* texture is likely in the folder, where model is */
				m_shader = StringOutputStream()( rootPath, m_shader );
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
						m_shader = StringOutputStream()( rootPath, path_get_filename_start( name ) );
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
		const auto rootPath = StringOutputStream()( PathCleaned( PathFilenameless( modelname ) ) );
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
		std::for_each( model->m_meshes.begin(), model->m_meshes.end(), [&vector]( const auto& val ){
			vector.push_back( &val );
		} );
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
	ePyramidaClip = 1 << 8,
	eExtrudeDownwards = 1 << 9,
	eExtrudeUpwards = 1 << 10,
	eMaxExtrude = 1 << 11,
	eAxialBackplane = 1 << 12,
	eClipFlags = eClipModel | eExtrudeFaceNormals | eExtrudeTerrain | eExtrudeVertexNormals | ePyramidaClip | eExtrudeDownwards | eExtrudeUpwards | eMaxExtrude | eAxialBackplane,
};

inline size_t normal_make_axial( Vector3& normal ){
	const size_t i = vector3_max_abs_component_index( normal );
	normal = normal[i] >= 0? g_vector3_axes[i] : -g_vector3_axes[i];
	return i;
}

struct ClipSides
{
	struct ClipSide
	{
		winding_t points;
		Plane3f plane;
	};

	winding_t w;
	ClipSide front;
	std::vector<ClipSide> sides;

	/* construct front and sides, requires w */
	bool construct(){
		if( PlaneFromPoints( front.plane, w.data() ) ){
			const Plane3f pln = front.plane;
			SnapPlaneImproved( front.plane, w.size(), w.data() );
			if( pln.normal() != front.plane.normal() || pln.dist() != front.plane.dist() ){
				for( Vector3& v : w ){
					v = plane3_project_point( front.plane, v );
				}
			}

			front.points = w;
			sides.resize( w.size() );

			/* sanity check */
			{
				const Vector3 d1 = w[1] - w[0];
				const Vector3 d2 = w[2] - w[0];
				const Vector3 normal = vector3_cross( d2, d1 );
				/* https://en.wikipedia.org/wiki/Cross_product#Geometric_meaning
					cross( a, b ).length = a.length b.length sin( angle ) */
				const double lengthsSquared = vector3_length_squared( d1 ) * vector3_length_squared( d2 );
				if ( lengthsSquared == 0 || ( vector3_length_squared( normal ) / lengthsSquared ) < 1e-8 ) {
					Sys_Warning( "points on line\n" );
				}
				else{
					return true;
				}
			}
		}
		return false;
	}
#if 0
	void push_side( const Vector3& point ){
		ClipSide& side = sides.emplace_back();
		side.w.push_back( point );
		PlaneFromPoints( side.plane, front.w[sides.size()], front.w[sides.size() - 1], side.w[0] );
		const Plane3f pln = side.plane;
		SnapPlaneImproved( side.plane, 2, front.w.data() + sides.size() - 1 );
		if( pln.normal() != side.plane.normal() || pln.dist() != side.plane.dist() ){
			const Vector3 n = VectorNormalized( vector3_cross( front.plane.normal(), vector3_cross( front.plane.normal(), side.plane.normal() ) ) );
			for( Vector3& v : Span( &front.w[sides.size() - 1], 2 ) ){
				v = plane3_project_point( Plane3( side.plane ), v, n );
			}
		}
	}
#endif
	void add_back_plane( float clipDepth, const Vector3& bestNormal ){
		ClipSide& back = sides.emplace_back( front );
		back.plane = plane3_flipped( back.plane );
		back.plane.dist() += vector3_dot( bestNormal, front.plane.normal() ) * clipDepth;
		for( Vector3& v : back.points )
			v -= bestNormal * clipDepth;
	}

	void make_brush_sides( shaderInfo_t *si ) const {
		/* set up brush sides */
		buildBrush.sides.clear(); // clear, so resize() will value-initialize elements
		buildBrush.sides.resize( sides.size() + 1 );

		if( debugClip ){
			buildBrush.sides[0].shaderInfo = ShaderInfoForShader( "debugclip2" );
			for ( size_t i = 0; i < sides.size(); ++i )
				buildBrush.sides[i + 1].shaderInfo = ShaderInfoForShader( "debugclip" );
		}
		else{
			buildBrush.sides[0].shaderInfo = si;
			buildBrush.sides[0].surfaceFlags = si->surfaceFlags;
			for ( size_t i = 0; i < sides.size(); ++i )
				buildBrush.sides[i + 1].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
		}

		buildBrush.sides[0].planenum = FindFloatPlane( front.plane, front.points.size(), front.points.data() );
		for( size_t i = 0; i < sides.size(); ++i ){
			buildBrush.sides[i + 1].planenum = FindFloatPlane( sides[i].plane, sides[i].points.size(), sides[i].points.data() );
		}
	}
};


static void ClipModel( int spawnFlags, float clipDepth, shaderInfo_t *si, const mapDrawSurface_t *ds, const char *modelName, entity_t& entity ){
	const int spf = ( spawnFlags & ( eClipFlags & ~eClipModel ) );

	/* ydnar: giant hack land: generate clipping brushes for model triangles */
	if ( ( si->clipModel && spf == 0 ) // default CLIPMODEL
	  || ( spawnFlags & eClipFlags ) == eClipModel //default CLIPMODEL
	  || spf == eExtrudeFaceNormals
	  || spf == eExtrudeTerrain
	  || spf == eExtrudeVertexNormals
	  || spf == ePyramidaClip
	  || spf == eExtrudeDownwards
	  || spf == eExtrudeUpwards
	  || spf == eAxialBackplane // default sides + axial backplane
	  || spf == ( eExtrudeFaceNormals | ePyramidaClip ) // extrude 45
	  || spf == ( eExtrudeTerrain | eMaxExtrude )
	  || spf == ( eExtrudeTerrain | eAxialBackplane )
	  || spf == ( eExtrudeVertexNormals | ePyramidaClip ) // vertex normals + don't check for sides, sticking outwards
	  || spf == ( ePyramidaClip | eAxialBackplane ) // pyramid with 3 of 4 sides axial (->small bsp)
	  || spf == ( eExtrudeDownwards | eExtrudeUpwards )
	  || spf == ( eExtrudeDownwards | eMaxExtrude )
	  || spf == ( eExtrudeDownwards | eAxialBackplane )
	  || spf == ( eExtrudeDownwards | eExtrudeUpwards | eMaxExtrude )
	  || spf == ( eExtrudeDownwards | eExtrudeUpwards | eAxialBackplane )
	  || spf == ( eExtrudeUpwards | eMaxExtrude )
	  || spf == ( eExtrudeUpwards | eAxialBackplane ) ){
		float limDepth = 0;
		if ( clipDepth < 0 ){
			limDepth = -clipDepth;
			clipDepth = 2.0;
		}
		MinMax minmax;
		Vector3 avgDirection( 0 );
		size_t axis;

		/* temp hack */
		if ( !si->clipModel && !( si->compileFlags & C_SOLID ) ) {
			return;
		}

		//wont snap these in normal way, or will explode
		// const double normalEpsilon_save = normalEpsilon;
		//normalEpsilon = 0.000001;


		if ( ( spf & eMaxExtrude ) || ( spf & eExtrudeTerrain ) ){

			for ( int idx = 0; idx < ds->numIndexes; idx += 3 ){
				Vector3 points[3];
				Plane3f plane;
				for ( int i = 0; i < 3; ++i ){
					points[i] = ds->verts[ds->indexes[idx + i]].xyz;
				}
				if ( PlaneFromPoints( plane, points ) ){
					if ( spf & eExtrudeTerrain )
						avgDirection += plane.normal();	//calculate average mesh facing direction

					//get min/max
					for ( int i = 0; i < 3; ++i ){
						minmax.extend( points[i] );
					}
				}
			}
			//unify avg direction
			if ( spf & eExtrudeTerrain ){
				if ( vector3_length( avgDirection ) == 0 )
					avgDirection = g_vector3_axis_z;
				axis = normal_make_axial( avgDirection );
			}
		}

		/* prepare a brush */
		buildBrush.sides.reserve( MAX_BUILD_SIDES );
		buildBrush.entityNum = entity.mapEntityNum;
		buildBrush.contentShader = si;
		buildBrush.compileFlags = si->compileFlags;
		buildBrush.contentFlags = si->contentFlags;
		buildBrush.detail = true;

		/* walk triangle list */
		for ( int idx = 0; idx < ds->numIndexes; idx += 3 ){
			ClipSides cs;
			/* make points */
			for ( int i = 0; i < 3; ++i ){
				/* copy xyz */
				cs.w.push_back( ds->verts[ds->indexes[idx + i]].xyz );
			}

			/* make plane for triangle */
			if ( cs.construct() ) {
				//snap points before using them for further calculations
				//precision suffers a lot, when two of normal values are under .00025 (often no collision, knocking up effect in ioq3)
				//also broken drawsurfs in case of normal brushes

				if ( spf == ( ePyramidaClip | eAxialBackplane ) ){ // pyramid with 3 of 4 sides axial (->small bsp)

					for ( int i = 0; i < 3; ++i )
						if ( fabs( cs.front.plane.normal()[i] ) < 0.05 && fabs( cs.front.plane.normal()[( i + 1 ) % 3] ) < 0.05 ) //no way, close to lay on two axes
							goto default_CLIPMODEL;

					// best axial normal
					Vector3 bestNormal = cs.front.plane.normal();
					axis = normal_make_axial( bestNormal );

					float mindist = 999999;

					for ( size_t i = 0; i < cs.w.size(); ++i ){ // planes
						float bestdist = 999999, bestangle = 1;

						for ( size_t j = 0; j < 3; ++j ){ // axes
							Plane3f pln;
							Vector3 nrm = cs.w[winding_next( cs.w, i )] - cs.w[i];
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
							pln.dist() = vector3_dot( cs.w[i], pln.normal() );
							//check facing, thickness
							const float currdist = -plane3_distance_to_point( pln, cs.w[( i + 2 ) % cs.w.size()] );
							const float currangle = vector3_dot( pln.normal(), cs.front.plane.normal() );
							if ( ( ( currdist > 0.1 ) && ( currdist < bestdist ) && ( currangle < 0 ) ) ||
							     ( ( currangle >= 0 ) && ( currangle <= bestangle ) ) ){
								bestangle = currangle;
								if ( currangle < 0 )
									bestdist = currdist;
								cs.sides[i].plane = pln;
							}
						}
						if ( bestdist == 999999 && bestangle == 1 ){
							// Sys_Printf( "default_CLIPMODEL\n" );
							goto default_CLIPMODEL;
						}
						value_minimize( mindist, bestdist );
					}
					if ( (limDepth != 0.0) && (mindist > limDepth) )
						goto default_CLIPMODEL;

					cs.make_brush_sides( si );
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

					Vector3 bestNormal;

					if ( spf & eExtrudeTerrain ){ //autodirection
						bestNormal = avgDirection;
					}
					else{
						axis = 2;
						if ( ( spf & eExtrudeDownwards ) && ( spf & eExtrudeUpwards ) ){
							bestNormal = cs.front.plane.normal().z() >= 0? g_vector3_axis_z : -g_vector3_axis_z;
						}
						else if ( spf & eExtrudeDownwards ){
							bestNormal = g_vector3_axis_z;
						}
						else if ( spf & eExtrudeUpwards ){
							bestNormal = -g_vector3_axis_z;
						}
						else{ // best axial normal
							bestNormal = cs.front.plane.normal();
							axis = normal_make_axial( bestNormal );
						}
					}

					if ( vector3_dot( cs.front.plane.normal(), bestNormal ) < 0.05 ){
						goto default_CLIPMODEL;
					}


					/* make side planes */
					for ( size_t i = 0; i < cs.w.size(); ++i )
					{
						cs.sides[i].plane.normal() = VectorNormalized( vector3_cross( bestNormal, cs.w[winding_next( cs.w, i )] - cs.w[i] ) );
						cs.sides[i].plane.dist() = vector3_dot( cs.w[i], cs.sides[i].plane.normal() );
					}

					/* make back plane */
					if ( spf & eMaxExtrude ){ //max extrude
						Plane3f& pln = cs.sides.emplace_back().plane;
						pln.normal() = -bestNormal;
						if ( bestNormal[axis] > 0 ){
							pln.dist() = -minmax.mins[axis] + clipDepth;
						}
						else{
							pln.dist() = minmax.maxs[axis] + clipDepth;
						}
					}
					else if ( spf & eAxialBackplane ){ //axial backplane
						Plane3f& pln = cs.sides.emplace_back().plane;
						pln.normal() = -bestNormal;
						pln.dist() = cs.w[0][axis];
						if ( bestNormal[axis] > 0 ){
							for ( size_t i = 1; i < cs.w.size(); ++i ){
								value_minimize( pln.dist(), cs.w[i][axis] );
							}
							pln.dist() = -pln.dist() + clipDepth;
						}
						else{
							for ( size_t i = 1; i < cs.w.size(); ++i ){
								value_maximize( pln.dist(), cs.w[i][axis] );
							}
							pln.dist() += clipDepth;
						}
						if ( limDepth != 0.0 ){
							Vector3 cnt = cs.w[0];
							if ( bestNormal[axis] > 0 ){
								for ( size_t i = 1; i < cs.w.size(); ++i ){
									if ( cs.w[i][axis] > cnt[axis] ){
										cnt = cs.w[i];
									}
								}
							}
							else {
								for ( size_t i = 1; i < cs.w.size(); ++i ){
									if ( cs.w[i][axis] < cnt[axis] ){
										cnt = cs.w[i];
									}
								}
							}
							cnt = plane3_project_point( pln, cnt );
							if ( -plane3_distance_to_point( cs.front.plane, cnt ) > limDepth ){
								cs.sides.pop_back();
								cs.add_back_plane( clipDepth, cs.front.plane.normal() );
							}
						}
					}
					else{	//normal backplane
						cs.add_back_plane( clipDepth, cs.front.plane.normal() );
					}

					cs.make_brush_sides( si );
				}

				else if ( spf == ( eExtrudeFaceNormals | ePyramidaClip ) ){	// extrude 45
					//45 degrees normals for side planes
					for ( size_t i = 0; i < cs.w.size(); ++i )
					{
						const Vector3 enrm = VectorNormalized( vector3_cross( cs.front.plane.normal(), cs.w[winding_next( cs.w, i )] - cs.w[i] ) );
						/* make side planes */
						cs.sides[i].plane.normal() = VectorNormalized( enrm - cs.front.plane.normal() );
						cs.sides[i].plane.dist() = vector3_dot( cs.w[i], cs.sides[i].plane.normal() );
					}

					/* make back plane */
					cs.add_back_plane( clipDepth, cs.front.plane.normal() );

					cs.make_brush_sides( si );
				}

				else if ( spf == eExtrudeVertexNormals
				       || spf == ( eExtrudeVertexNormals | ePyramidaClip ) ){ // vertex normals + don't check for sides, sticking outwards
					Vector3 Vnorm[3], Enorm[3];
					/* get vertex normals */
					for ( int i = 0; i < 3; ++i ){
						/* copy normal */
						Vnorm[i] = ds->verts[ds->indexes[idx + i]].normal;
					}

					//avg normals for side planes
					for ( int i = 0; i < 3; ++i )
					{
						Enorm[i] = VectorNormalized( Vnorm[i] + Vnorm[( i + 1 ) % 3] );
						//check fuer bad ones
						const Vector3 nrm = VectorNormalized( vector3_cross( cs.front.plane.normal(), cs.w[( i + 1 ) % 3] - cs.w[i] ) );
						//check for negative or outside direction
						if ( vector3_dot( Enorm[i], cs.front.plane.normal() ) > 0.1 ){
							if ( ( vector3_dot( Enorm[i], nrm ) > -0.2 ) || ( spf & ePyramidaClip ) ){
								//ok++;
								continue;
							}
						}
						//notok++;
						//Sys_Printf( "faulty Enormal %i/%i\n", notok, ok );
						//use 45 normal
						Enorm[i] = VectorNormalized( cs.front.plane.normal() + nrm );
					}

					/* make side planes */
					for ( int i = 0; i < 3; ++i )
					{
						cs.sides[i].plane.normal() = VectorNormalized( vector3_cross( Enorm[i], cs.w[( i + 1 ) % 3] - cs.w[i] ) );
						cs.sides[i].plane.dist() = vector3_dot( cs.w[i], cs.sides[i].plane.normal() );
					}

					/* make back plane */
					cs.add_back_plane( clipDepth, cs.front.plane.normal() );

					cs.make_brush_sides( si );
				}

				else if ( spf == eExtrudeFaceNormals ){
					/* make side planes */
					for ( size_t i = 0; i < cs.w.size(); ++i )
					{
						cs.sides[i].plane.normal() = VectorNormalized( vector3_cross( cs.front.plane.normal(), cs.w[winding_next( cs.w, i )] - cs.w[i] ) );
						cs.sides[i].plane.dist() = vector3_dot( cs.w[i], cs.sides[i].plane.normal() );
					}

					/* make back plane */
					cs.add_back_plane( clipDepth, cs.front.plane.normal() );

					cs.make_brush_sides( si );
				}

				else if ( spf == ePyramidaClip ){
					/* calculate center */
					Vector3 cnt = ( cs.w[0] + cs.w[1] + cs.w[2] ) / 3.0;
					/* make back pyramid point */
					cnt -= cs.front.plane.normal() * clipDepth;

					/* make 3 more planes */
					for ( size_t i = 0; i < cs.w.size(); ++i )
					{
						PlaneFromPoints( cs.sides[i].plane, cs.w[winding_next( cs.w, i )], cs.w[i], cnt );
					}

					cs.make_brush_sides( si );
				}

				else if ( ( si->clipModel && spf == 0 ) || ( spawnFlags & eClipFlags ) == eClipModel ){	//default CLIPMODEL
default_CLIPMODEL:
					// axial normal
					Vector3 bestNormal = cs.front.plane.normal();
					normal_make_axial( bestNormal );

					/* make side planes */
					for ( size_t i = 0; i < cs.w.size(); ++i )
					{
						cs.sides[i].plane.normal() = VectorNormalized( vector3_cross( bestNormal, cs.w[winding_next( cs.w, i )] - cs.w[i] ) );
						cs.sides[i].plane.dist() = vector3_dot( cs.w[i], cs.sides[i].plane.normal() );
					}

					/* make back plane */
					cs.add_back_plane( clipDepth, bestNormal );

					cs.make_brush_sides( si );
				}


				/* add to entity */
				if ( CreateBrushWindings( buildBrush ) ) {
					AddBrushBevels();
					//%	EmitBrushes( buildBrush, NULL, NULL );
					brush_t& newBrush = entity.brushes.emplace_front( buildBrush );
					newBrush.original = &newBrush;
					entity.numBrushes++;
					continue;
				}
			}
			Sys_Warning( "triangle (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) of %s was not autoclipped\n",
			             cs.w[0][0], cs.w[0][1], cs.w[0][2],
			             cs.w[1][0], cs.w[1][1], cs.w[1][2],
			             cs.w[2][0], cs.w[2][1], cs.w[2][2], modelName );
		}
		// normalEpsilon = normalEpsilon_save;
	}
	else if ( spawnFlags & eClipFlags ){
		Sys_Warning( "nonexistent clipping mode selected\n" );
	}
}

/*
   InsertModel() - ydnar
   adds a picomodel into the bsp
 */

void InsertModel( const char *name, const char *skin, int frame, const Matrix4& transform, const std::list<remap_t> *remaps, shaderInfo_t *celShader, entity_t& entity, int castShadows, int recvShadows, int spawnFlags, float lightmapScale, int lightmapSampleSize, float shadeAngle, float clipDepth ){
	int i, j;
	const Matrix4 nTransform( matrix4_for_normal_transform( transform ) );
	const bool transform_lefthanded = MATRIX4_LEFTHANDED == matrix4_handedness( transform );
	AssModel            *model;
	shaderInfo_t        *si;
	mapDrawSurface_t    *ds;
	const char          *picoShaderName;


	/* get model */
	model = LoadModel( name, frame );
	if ( model == NULL ) {
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
				if ( skinfilenextptr != NULL ) {
					strClear( skinfilenextptr++ );
					if( *skinfilenextptr == '\n' ) // handle \r\n
						++skinfilenextptr;
				}
				else
				{
					skinfilenextptr = strchr( skinfileptr, '\n' );
					if ( skinfilenextptr != NULL ) {
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

	/* hack: Stable-1_2 and trunk have differing row/column major matrix order
	   this transpose is necessary with Stable-1_2
	   uncomment the following line with old m4x4_t (non 1.3/spog_branch) code */
	//%	m4x4_transpose( transform );

	/* fix bogus lightmap scale */
	if ( lightmapScale <= 0.0f ) {
		lightmapScale = 1.0f;
	}

	/* fix bogus shade angle */
	if ( shadeAngle <= 0.0f ) {
		shadeAngle = 0.0f;
	}

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
			picoShaderName = NULL;
			for( const auto& skin : skins )
			{
				if ( striEqual( surface.m_shader.c_str(), skin.from ) ) {
					Sys_FPrintf( SYS_VRB, "Skin file: mapping %s to %s\n", surface.m_shader.c_str(), skin.to );
					picoShaderName = skin.to;
					break;
				}
			}
			if ( picoShaderName == NULL ) {
				Sys_FPrintf( SYS_VRB, "Skin file: not mapping %s\n", surface.m_shader.c_str() );
				continue;
			}
		}

		/* handle shader remapping */
		if( remaps != NULL ){
			const char* to = NULL;
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
			if( to != NULL ){
				Sys_FPrintf( SYS_VRB, ( fromlen == 0? "Globbing '%s' to '%s'\n" : "Remapping '%s' to '%s'\n" ), picoShaderName, to );
				picoShaderName = to;
			}
		}

		/* shader renaming for sof2 */
		if ( renameModelShaders ) {
			auto shaderName = String64()( PathExtensionless( picoShaderName ) );
			if ( spawnFlags & eRMG_BSP ) {
				shaderName << "_RMG_BSP";
			}
			else{
				shaderName << "_BSP";
			}
			si = ShaderInfoForShader( shaderName );
		}
		else{
			si = ShaderInfoForShader( picoShaderName );
		}

		/* allocate a surface (ydnar: gs mods) */
		ds = AllocDrawSurface( ESurfaceType::Triangles );
		ds->entityNum = entity.mapEntityNum;
		ds->castShadows = castShadows;
		ds->recvShadows = recvShadows;

		/* set shader */
		ds->shaderInfo = si;

		/* force to meta? */
		if ( ( si != NULL && si->forceMeta ) || ( spawnFlags & eForceMeta ) ) { /* 3rd bit */
			ds->type = ESurfaceType::ForcedMeta;
		}

		/* fix the surface's normals (jal: conditioned by shader info) */
		if ( !( spawnFlags & eNoSmooth ) && ( shadeAngle == 0.0f || ds->type != ESurfaceType::ForcedMeta ) ) {
			// PicoFixSurfaceNormals( surface );
		}

		/* set sample size */
		if ( lightmapSampleSize > 0.0f ) {
			ds->sampleSize = lightmapSampleSize;
		}

		/* set lightmap scale */
		if ( lightmapScale > 0.0f ) {
			ds->lightmapScale = lightmapScale;
		}

		/* set shading angle */
		if ( shadeAngle > 0.0f ) {
			ds->shadeAngleDegrees = shadeAngle;
		}

		/* set particulars */
		ds->numVerts = mesh->mNumVertices;
		ds->verts = safe_calloc( ds->numVerts * sizeof( ds->verts[ 0 ] ) );

		ds->numIndexes = mesh->mNumFaces * 3;
		ds->indexes = safe_calloc( ds->numIndexes * sizeof( ds->indexes[ 0 ] ) );
// Sys_Printf( "verts %i idx %i\n", ds->numVerts, ds->numIndexes );
		/* copy vertexes */
		for ( i = 0; i < ds->numVerts; i++ )
		{
			/* get vertex */
			bspDrawVert_t& dv = ds->verts[ i ];

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
				dv.st = si->stFlat;
			}

			/* ydnar: gs mods: added support for explicit shader texcoord generation */
			else if ( si->tcGen ) {
				/* project the texture */
				dv.st[ 0 ] = vector3_dot( si->vecs[ 0 ], dv.xyz );
				dv.st[ 1 ] = vector3_dot( si->vecs[ 1 ], dv.xyz );
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
				for ( j = 0; j < MAX_LIGHTMAPS; j++ )
				{
					dv.lightmap[ j ] = { 0, 0 };
					if ( spawnFlags & eColorToAlpha ) { // spawnflag 32: model color -> alpha hack
						dv.color[ j ] = { 255, 255, 255, color_to_byte( RGBTOGRAY( color ) * 255 ) };
					}
					else
					{
						dv.color[ j ] = { color_to_byte( color[0] * 255 ),
						                  color_to_byte( color[1] * 255 ),
						                  color_to_byte( color[2] * 255 ),
						                  color_to_byte( color[3] * 255 ) };
					}
				}
			}
		}

		/* copy indexes */
		{
			size_t idCopied = 0;
			for ( const aiFace& face : Span( mesh->mFaces, mesh->mNumFaces ) ){
				// if( face.mNumIndices == 3 )
				for ( size_t i = 0; i < 3; i++ ){
					ds->indexes[idCopied++] = face.mIndices[i];
				}
				if( transform_lefthanded ){
					std::swap( ds->indexes[idCopied - 1], ds->indexes[idCopied - 2] );
				}
			}
		}

		/* set cel shader */
		ds->celShader = celShader;

		ClipModel( spawnFlags, clipDepth, si, ds, name, entity );
	}
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
		if ( !eparent.read_keyvalue( targetName, "targetname" ) ) {
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
		if ( !strEqual( e.valueForKey( "target" ), targetName ) ) {
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

		int castShadows, recvShadows;
		if ( &eparent == &entities[0] ) {    /* worldspawn (and func_groups) default to cast/recv shadows in worldspawn group */
			castShadows = WORLDSPAWN_CAST_SHADOWS;
			recvShadows = WORLDSPAWN_RECV_SHADOWS;
		}
		else{                   /* other entities don't cast any shadows, but recv worldspawn shadows */
			castShadows = ENTITY_CAST_SHADOWS;
			recvShadows = ENTITY_RECV_SHADOWS;
		}

		/* get explicit shadow flags */
		GetEntityShadowFlags( &e, &eparent, &castShadows, &recvShadows );

		/* get spawnflags */
		const int spawnFlags = e.intForKey( "spawnflags" );

		/* get origin */
		const Vector3 origin = e.vectorForKey( "origin" ) - eparent.origin;    /* offset by parent */

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
				if ( split == NULL ) {
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

		/* ydnar: cel shader support */
		shaderInfo_t *celShader;
		if( const char *value; e.read_keyvalue( value, "_celshader" ) ||
		    entities[ 0 ].read_keyvalue( value, "_celshader" ) ){
			celShader = ShaderInfoForShader( String64()( "textures/", value ) );
		}
		else{
			celShader = globalCelShader.empty() ? NULL : ShaderInfoForShader( globalCelShader );
		}

		/* jal : entity based _samplesize */
		const int lightmapSampleSize = std::max( 0, e.intForKey( "_lightmapsamplesize", "_samplesize", "_ss" ) );
		if ( lightmapSampleSize != 0 )
			Sys_Printf( "misc_model has lightmap sample size of %.d\n", lightmapSampleSize );

		/* get lightmap scale */
		const float lightmapScale = std::max( 0.f, e.floatForKey( "lightmapscale", "_lightmapscale", "_ls" ) );
		if ( lightmapScale != 0 )
			Sys_Printf( "misc_model has lightmap scale of %.4f\n", lightmapScale );

		/* jal : entity based _shadeangle */
		const float shadeAngle = std::max( 0.f, e.floatForKey( "_shadeangle",
		                                        "_smoothnormals", "_sn", "_sa", "_smooth" ) ); /* vortex' aliases */
		if ( shadeAngle != 0 )
			Sys_Printf( "misc_model has shading angle of %.4f\n", shadeAngle );

		const char *skin = nullptr;
		e.read_keyvalue( skin, "_skin", "skin" );

		float clipDepth = clipDepthGlobal;
		if ( e.read_keyvalue( clipDepth, "_clipdepth" ) )
			Sys_Printf( "misc_model %s has autoclip depth of %.3f\n", model, clipDepth );


		/* insert the model */
		InsertModel( model, skin, frame, transform, &remaps, celShader, eparent, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapSampleSize, shadeAngle, clipDepth );
	}
}

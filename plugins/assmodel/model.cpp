/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#include "model.h"

#include "iarchive.h"
#include "idatastream.h"
#include "imodel.h"
#include "modelskin.h"

#include "cullable.h"
#include "renderable.h"
#include "selectable.h"

#include "math/frustum.h"
#include "string/string.h"
#include "generic/static.h"
#include "shaderlib.h"
#include "scenelib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "render.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/mesh.h"
#include "os/path.h"
#include "stream/stringstream.h"

class VectorLightList final : public LightList
{
	typedef std::vector<const RendererLight*> Lights;
	Lights m_lights;
public:
	void addLight( const RendererLight& light ){
		m_lights.push_back( &light );
	}
	void clear(){
		m_lights.clear();
	}
	void evaluateLights() const override {
	}
	void lightsChanged() const override {
	}
	void forEachLight( const RendererLightCallback& callback ) const override {
		for ( Lights::const_iterator i = m_lights.begin(); i != m_lights.end(); ++i )
		{
			callback( *( *i ) );
		}
	}
};

struct AssScene
{
	const aiScene* m_scene;
	const char* m_rootPath;
	const char* m_matName; // forced global mat name, if not null
};

class PicoSurface final :
	public OpenGLRenderable
{
	AABB m_aabb_local;
	CopiedString m_shader;
	Shader* m_state;

	Array<ArbitraryMeshVertex> m_vertices;
	Array<RenderIndex> m_indices;

public:

	PicoSurface(){
		constructNull();
		CaptureShader();
	}
	PicoSurface( const AssScene scene, const aiMesh* mesh ){
		CopyPicoSurface( scene, mesh );
		CaptureShader();
	}
	~PicoSurface(){
		ReleaseShader();
	}

	void render( RenderStateFlags state ) const {
		if ( ( state & RENDER_BUMP ) != 0 ) {
			gl().glNormalPointer( GL_FLOAT, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->normal );
			gl().glVertexAttribPointer( c_attr_TexCoord0, 2, GL_FLOAT, 0, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->texcoord );
			gl().glVertexAttribPointer( c_attr_Tangent, 3, GL_FLOAT, 0, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->tangent );
			gl().glVertexAttribPointer( c_attr_Binormal, 3, GL_FLOAT, 0, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->bitangent );
		}
		else
		{
			gl().glNormalPointer( GL_FLOAT, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->normal );
			gl().glTexCoordPointer( 2, GL_FLOAT, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->texcoord );
		}
		gl().glVertexPointer( 3, GL_FLOAT, sizeof( ArbitraryMeshVertex ), &m_vertices.data()->vertex );
		gl().glDrawElements( GL_TRIANGLES, GLsizei( m_indices.size() ), RenderIndexTypeID, m_indices.data() );

#if defined( _DEBUG ) && !defined( _DEBUG_QUICKER )
		GLfloat modelview[16];
		gl().glGetFloatv( GL_MODELVIEW_MATRIX, modelview ); // I know this is slow as hell, but hey - we're in _DEBUG
		Matrix4 modelview_inv(
		    modelview[0], modelview[1], modelview[2], modelview[3],
		    modelview[4], modelview[5], modelview[6], modelview[7],
		    modelview[8], modelview[9], modelview[10], modelview[11],
		    modelview[12], modelview[13], modelview[14], modelview[15] );
		matrix4_full_invert( modelview_inv );
		Matrix4 modelview_inv_transposed = matrix4_transposed( modelview_inv );

		gl().glBegin( GL_LINES );

		for ( Array<ArbitraryMeshVertex>::const_iterator i = m_vertices.begin(); i != m_vertices.end(); ++i )
		{
			Vector3 normal = normal3f_to_vector3( ( *i ).normal );
			normal = matrix4_transformed_direction( modelview_inv, vector3_normalised( matrix4_transformed_direction( modelview_inv_transposed, normal ) ) ); // do some magic
			Vector3 normalTransformed = vector3_added( vertex3f_to_vector3( ( *i ).vertex ), vector3_scaled( normal, 8 ) );
			gl().glVertex3fv( vertex3f_to_array( ( *i ).vertex ) );
			gl().glVertex3fv( vector3_to_array( normalTransformed ) );
		}
		gl().glEnd();
#endif
	}

	VolumeIntersectionValue intersectVolume( const VolumeTest& test, const Matrix4& localToWorld ) const {
		return test.TestAABB( m_aabb_local, localToWorld );
	}

	const AABB& localAABB() const {
		return m_aabb_local;
	}

	void render( Renderer& renderer, const Matrix4& localToWorld, Shader* state ) const {
		renderer.SetState( state, Renderer::eFullMaterials );
		renderer.addRenderable( *this, localToWorld );
	}

	void render( Renderer& renderer, const Matrix4& localToWorld ) const {
		render( renderer, localToWorld, m_state );
	}

	void testSelect( Selector& selector, SelectionTest& test, const Matrix4& localToWorld ){
		test.BeginMesh( localToWorld, true );

		SelectionIntersection best;
		testSelect( test, best );
		if ( best.valid() ) {
			selector.addIntersection( best );
		}
	}

	const char* getShader() const {
		return m_shader.c_str();
	}
	Shader* getState() const {
		return m_state;
	}

private:

	void CaptureShader(){
		m_state = GlobalShaderCache().capture( m_shader.c_str() );
	}
	void ReleaseShader(){
		GlobalShaderCache().release( m_shader.c_str() );
	}

	void UpdateAABB(){
		m_aabb_local = AABB();
		for ( std::size_t i = 0; i < m_vertices.size(); ++i )
			aabb_extend_by_point_safe( m_aabb_local, reinterpret_cast<const Vector3&>( m_vertices[i].vertex ) );


		for ( Array<RenderIndex>::iterator i = m_indices.begin(); i != m_indices.end(); i += 3 )
		{
			ArbitraryMeshVertex& a = m_vertices[*( i + 0 )];
			ArbitraryMeshVertex& b = m_vertices[*( i + 1 )];
			ArbitraryMeshVertex& c = m_vertices[*( i + 2 )];

			ArbitraryMeshTriangle_sumTangents( a, b, c );
		}

		for ( Array<ArbitraryMeshVertex>::iterator i = m_vertices.begin(); i != m_vertices.end(); ++i )
		{
			vector3_normalise( reinterpret_cast<Vector3&>( ( *i ).tangent ) );
			vector3_normalise( reinterpret_cast<Vector3&>( ( *i ).bitangent ) );
		}
	}

	void testSelect( SelectionTest& test, SelectionIntersection& best ){
		test.TestTriangles(
		    VertexPointer( VertexPointer::pointer( &m_vertices.data()->vertex ), sizeof( ArbitraryMeshVertex ) ),
		    IndexPointer( m_indices.data(), IndexPointer::index_type( m_indices.size() ) ),
		    best
		);
	}

	void CopyPicoSurface( const AssScene scene, const aiMesh* mesh ){
		if( scene.m_matName != nullptr ){
			m_shader = scene.m_matName;
		}
		else{
			aiMaterial *material = scene.m_scene->mMaterials[mesh->mMaterialIndex];

			aiString matname = material->GetName();
#ifdef _DEBUG
						globalOutputStream() << "matname: " << matname.C_Str() << '\n';
#endif
			if( aiString texname;
			    aiReturn_SUCCESS == material->Get( AI_MATKEY_TEXTURE_DIFFUSE( 0 ), texname )
			 && texname.length != 0
			 && !string_equal_prefix_nocase( matname.C_Str(), "textures/" ) /* matname looks intentionally named as ingame shader */
			 && !string_equal_prefix_nocase( matname.C_Str(), "textures\\" )
			 && !string_equal_prefix_nocase( matname.C_Str(), "models/" )
			 && !string_equal_prefix_nocase( matname.C_Str(), "models\\" ) ){
#ifdef _DEBUG
							globalOutputStream() << "texname: " << texname.C_Str() << '\n';
#endif
				m_shader = StringStream<64>( PathCleaned( PathExtensionless( texname.C_Str() ) ) );

			}
			else{
				m_shader = StringStream<64>( PathCleaned( PathExtensionless( matname.C_Str() ) ) );
			}

			const CopiedString oldShader( m_shader );
			if( strchr( m_shader.c_str(), '/' ) == nullptr ){ /* texture is likely in the folder, where model is */
				m_shader = StringStream<64>( scene.m_rootPath, m_shader );
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
						m_shader = StringStream<64>( scene.m_rootPath, path_get_filename_start( name ) );
					}
				}
			}

			if( oldShader != m_shader )
				globalOutputStream() << "substituting: " << oldShader << " -> " << m_shader << '\n';
		}

		m_vertices.resize( mesh->mNumVertices );
		m_indices.resize( mesh->mNumFaces * 3 );

		for ( std::size_t i = 0; i < m_vertices.size(); ++i )
		{
			m_vertices[i].vertex = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

			if( mesh->HasNormals() )
				m_vertices[i].normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

			if( mesh->HasTextureCoords( 0 ) )
				m_vertices[i].texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
#if 0
			if( mesh->HasTangentsAndBitangents() ){
				m_vertices[i].tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
				m_vertices[i].bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
			}
#endif
#if 0
			picoVec_t* color = PicoGetSurfaceColor( surface, 0, int( i ) );
			m_vertices[i].colour = Colour4b( color[0], color[1], color[2], color[3] );
#endif
		}

		size_t idCopied = 0;
		for ( size_t t = 0; t < mesh->mNumFaces; ++t ){
			const aiFace& face = mesh->mFaces[t];
			// if( face.mNumIndices == 3 )
			for ( size_t i = 0; i < 3; i++ ){
				m_indices[idCopied++] = face.mIndices[i];
			}
		}

		UpdateAABB();
	}

	void constructQuad( std::size_t index, const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, const Vector3& normal ){
		m_vertices[index * 4 + 0] = ArbitraryMeshVertex(
		                                vertex3f_for_vector3( a ),
		                                normal3f_for_vector3( normal ),
		                                texcoord2f_from_array( aabb_texcoord_topleft )
		                            );
		m_vertices[index * 4 + 1] = ArbitraryMeshVertex(
		                                vertex3f_for_vector3( b ),
		                                normal3f_for_vector3( normal ),
		                                texcoord2f_from_array( aabb_texcoord_topright )
		                            );
		m_vertices[index * 4 + 2] = ArbitraryMeshVertex(
		                                vertex3f_for_vector3( c ),
		                                normal3f_for_vector3( normal ),
		                                texcoord2f_from_array( aabb_texcoord_botright )
		                            );
		m_vertices[index * 4 + 3] = ArbitraryMeshVertex(
		                                vertex3f_for_vector3( d ),
		                                normal3f_for_vector3( normal ),
		                                texcoord2f_from_array( aabb_texcoord_botleft )
		                            );
	}

	void constructNull(){
		AABB aabb( Vector3( 0, 0, 0 ), Vector3( 8, 8, 8 ) );

		const std::array<Vector3, 8> points = aabb_corners( aabb );

		m_vertices.resize( 24 );

		constructQuad( 0, points[2], points[1], points[5], points[6], aabb_normals[0] );
		constructQuad( 1, points[1], points[0], points[4], points[5], aabb_normals[1] );
		constructQuad( 2, points[0], points[1], points[2], points[3], aabb_normals[2] );
		constructQuad( 3, points[0], points[3], points[7], points[4], aabb_normals[3] );
		constructQuad( 4, points[3], points[2], points[6], points[7], aabb_normals[4] );
		constructQuad( 5, points[7], points[6], points[5], points[4], aabb_normals[5] );

		m_indices.resize( 36 );

		RenderIndex indices[36] = {
			 0,  1,  2,  0,  2,  3,
			 4,  5,  6,  4,  6,  7,
			 8,  9, 10,  8, 10, 11,
			12, 13, 14, 12, 14, 15,
			16, 17, 18, 16, 18, 19,
			20, 21, 22, 20, 22, 23,
		};


		Array<RenderIndex>::iterator j = m_indices.begin();
		for ( RenderIndex* i = indices; i != indices + ( sizeof( indices ) / sizeof( RenderIndex ) ); ++i )
		{
			*j++ = *i;
		}

		m_shader = "";

		UpdateAABB();
	}
};



class PicoModel :
	public Cullable,
	public Bounded
{
	typedef std::vector<PicoSurface*> surfaces_t;
	surfaces_t m_surfaces;

	AABB m_aabb_local;
public:
	Callback<void()> m_lightsChanged;

	PicoModel(){
		constructNull();
	}
	PicoModel( const AssScene scene ){
		m_aabb_local = AABB();
		CopyPicoModel( scene, scene.m_scene->mRootNode );
	}
	~PicoModel(){
		for ( surfaces_t::iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i )
			delete *i;
	}

	typedef surfaces_t::const_iterator const_iterator;

	const_iterator begin() const {
		return m_surfaces.begin();
	}
	const_iterator end() const {
		return m_surfaces.end();
	}
	std::size_t size() const {
		return m_surfaces.size();
	}

	VolumeIntersectionValue intersectVolume( const VolumeTest& test, const Matrix4& localToWorld ) const {
		return test.TestAABB( m_aabb_local, localToWorld );
	}

	virtual const AABB& localAABB() const {
		return m_aabb_local;
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, std::vector<Shader*> states ) const {
		for ( surfaces_t::const_iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i )
		{
			if ( ( *i )->intersectVolume( volume, localToWorld ) != c_volumeOutside ) {
				( *i )->render( renderer, localToWorld, states[i - m_surfaces.begin()] );
			}
		}
	}

	void testSelect( Selector& selector, SelectionTest& test, const Matrix4& localToWorld ){
		for ( surfaces_t::iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i )
		{
			if ( ( *i )->intersectVolume( test.getVolume(), localToWorld ) != c_volumeOutside ) {
				( *i )->testSelect( selector, test, localToWorld );
			}
		}
	}

private:
	void CopyPicoModel( const AssScene scene, const aiNode* node ){
		for( size_t n = 0; n < node->mNumMeshes; ++n ){
			const aiMesh *mesh = scene.m_scene->mMeshes[node->mMeshes[n]];
			if( mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE ){
				PicoSurface* picosurface = new PicoSurface( scene, mesh );
				aabb_extend_by_aabb_safe( m_aabb_local, picosurface->localAABB() );
				m_surfaces.push_back( picosurface );
			}
		}

		// traverse all children
		for ( size_t n = 0; n < node->mNumChildren; ++n ){
			CopyPicoModel( scene, node->mChildren[n] );
		}
	}
	void constructNull(){
		PicoSurface* picosurface = new PicoSurface();
		m_aabb_local = picosurface->localAABB();
		m_surfaces.push_back( picosurface );
	}
};

inline void Surface_addLight( PicoSurface& surface, VectorLightList& lights, const Matrix4& localToWorld, const RendererLight& light ){
	if ( light.testAABB( aabb_for_oriented_aabb( surface.localAABB(), localToWorld ) ) ) {
		lights.addLight( light );
	}
}

class PicoModelInstance :
	public scene::Instance,
	public Renderable,
	public SelectionTestable,
	public LightCullable,
	public SkinnedModel
{
	class TypeCasts
	{
		InstanceTypeCastTable m_casts;
	public:
		TypeCasts(){
			InstanceContainedCast<PicoModelInstance, Bounded>::install( m_casts );
			InstanceContainedCast<PicoModelInstance, Cullable>::install( m_casts );
			InstanceStaticCast<PicoModelInstance, Renderable>::install( m_casts );
			InstanceStaticCast<PicoModelInstance, SelectionTestable>::install( m_casts );
			InstanceStaticCast<PicoModelInstance, SkinnedModel>::install( m_casts );
		}
		InstanceTypeCastTable& get(){
			return m_casts;
		}
	};

	PicoModel& m_picomodel;

	const LightList* m_lightList;
	typedef Array<VectorLightList> SurfaceLightLists;
	SurfaceLightLists m_surfaceLightLists;

	class Remap
	{
	public:
		CopiedString first;
		Shader* second;
		Remap() : second( 0 ){
		}
	};
	typedef Array<Remap> SurfaceRemaps;
	SurfaceRemaps m_skins;
public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	void* m_test;

	Bounded& get( NullType<Bounded>){
		return m_picomodel;
	}
	Cullable& get( NullType<Cullable>){
		return m_picomodel;
	}

	void lightsChanged(){
		m_lightList->lightsChanged();
	}
	typedef MemberCaller<PicoModelInstance, void(), &PicoModelInstance::lightsChanged> LightsChangedCaller;

	void constructRemaps(){
		ASSERT_MESSAGE( m_skins.size() == m_picomodel.size(), "ERROR" );
		ModelSkin* skin = NodeTypeCast<ModelSkin>::cast( path().parent() );
		if ( skin != 0 && skin->realised() ) {
			SurfaceRemaps::iterator j = m_skins.begin();
			for ( PicoModel::const_iterator i = m_picomodel.begin(); i != m_picomodel.end(); ++i, ++j )
			{
				const char* remap = skin->getRemap( ( *i )->getShader() );
				if ( !string_empty( remap ) ) {
					( *j ).first = remap;
					( *j ).second = GlobalShaderCache().capture( remap );
				}
				else
				{
					( *j ).second = 0;
				}
			}
			SceneChangeNotify();
		}
	}
	void destroyRemaps(){
		ASSERT_MESSAGE( m_skins.size() == m_picomodel.size(), "ERROR" );
		for ( SurfaceRemaps::iterator i = m_skins.begin(); i != m_skins.end(); ++i )
		{
			if ( ( *i ).second != 0 ) {
				GlobalShaderCache().release( ( *i ).first.c_str() );
				( *i ).second = 0;
			}
		}
	}
	void skinChanged(){
		destroyRemaps();
		constructRemaps();
	}

	PicoModelInstance( const PicoModelInstance& ) = delete; // not copyable
	PicoModelInstance operator=( const PicoModelInstance& ) = delete; // not assignable

	PicoModelInstance( const scene::Path& path, scene::Instance* parent, PicoModel& picomodel ) :
		Instance( path, parent, this, StaticTypeCasts::instance().get() ),
		m_picomodel( picomodel ),
		m_surfaceLightLists( m_picomodel.size() ),
		m_skins( m_picomodel.size() ){
		m_lightList = &GlobalShaderCache().attach( *this );
		m_picomodel.m_lightsChanged = LightsChangedCaller( *this );

		Instance::setTransformChangedCallback( LightsChangedCaller( *this ) );

		constructRemaps();
	}
	~PicoModelInstance(){
		destroyRemaps();

		Instance::setTransformChangedCallback( Callback<void()>() );

		m_picomodel.m_lightsChanged = Callback<void()>();
		GlobalShaderCache().detach( *this );
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld ) const {
		SurfaceLightLists::const_iterator j = m_surfaceLightLists.begin();
		SurfaceRemaps::const_iterator k = m_skins.begin();
		for ( PicoModel::const_iterator i = m_picomodel.begin(); i != m_picomodel.end(); ++i, ++j, ++k )
		{
			if ( ( *i )->intersectVolume( volume, localToWorld ) != c_volumeOutside ) {
				renderer.setLights( *j );
				( *i )->render( renderer, localToWorld, ( *k ).second != 0 ? ( *k ).second : ( *i )->getState() );
			}
		}
	}

	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
		m_lightList->evaluateLights();

		render( renderer, volume, Instance::localToWorld() );
	}
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
		renderSolid( renderer, volume );
	}

	void testSelect( Selector& selector, SelectionTest& test ){
		m_picomodel.testSelect( selector, test, Instance::localToWorld() );
	}

	bool testLight( const RendererLight& light ) const {
		return light.testAABB( worldAABB() );
	}
	void insertLight( const RendererLight& light ){
		const Matrix4& localToWorld = Instance::localToWorld();
		SurfaceLightLists::iterator j = m_surfaceLightLists.begin();
		for ( PicoModel::const_iterator i = m_picomodel.begin(); i != m_picomodel.end(); ++i )
		{
			Surface_addLight( *( *i ), *j++, localToWorld, light );
		}
	}
	void clearLights(){
		for ( SurfaceLightLists::iterator i = m_surfaceLightLists.begin(); i != m_surfaceLightLists.end(); ++i )
		{
			( *i ).clear();
		}
	}
};

class PicoModelNode : public scene::Node::Symbiot, public scene::Instantiable
{
	class TypeCasts
	{
		NodeTypeCastTable m_casts;
	public:
		TypeCasts(){
			NodeStaticCast<PicoModelNode, scene::Instantiable>::install( m_casts );
		}
		NodeTypeCastTable& get(){
			return m_casts;
		}
	};


	scene::Node m_node;
	InstanceSet m_instances;
	PicoModel m_picomodel;

public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	PicoModelNode() : m_node( this, this, StaticTypeCasts::instance().get() ){
	}
	PicoModelNode( const AssScene scene ) : m_node( this, this, StaticTypeCasts::instance().get() ), m_picomodel( scene ){
	}

	void release(){
		delete this;
	}
	scene::Node& node(){
		return m_node;
	}

	scene::Instance* create( const scene::Path& path, scene::Instance* parent ){
		return new PicoModelInstance( path, parent, m_picomodel );
	}
	void forEachInstance( const scene::Instantiable::Visitor& visitor ){
		m_instances.forEachInstance( visitor );
	}
	void insert( scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance ){
		m_instances.insert( observer, path, instance );
	}
	scene::Instance* erase( scene::Instantiable::Observer* observer, const scene::Path& path ){
		return m_instances.erase( observer, path );
	}
};



scene::Node& loadPicoModel( Assimp::Importer& importer, ArchiveFile& file ){
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
	               | aiProcess_PreTransformVertices;
	// rotate the whole scene 90 degrees around the x axis to convert assimp's Y = UP to Quakes's Z = UP
	importer.SetPropertyMatrix( AI_CONFIG_PP_PTV_ROOT_TRANSFORMATION, aiMatrix4x4( 1, 0, 0, 0,
	                                                                               0, 0, -1, 0,
	                                                                               0, 1, 0, 0,
	                                                                               0, 0, 0, 1 ) ); // aiMatrix4x4::RotationX( c_half_pi )

	const aiScene *scene = importer.ReadFile( file.getName(), flags );

	if( scene != nullptr ){
		if( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE )
			globalWarningStream() << "AI_SCENE_FLAGS_INCOMPLETE\n";
		const auto rootPath = StringStream<64>( PathFilenameless( file.getName() ) );
		const auto matName = StringStream<64>( PathExtensionless( file.getName() ) );
		return ( new PicoModelNode( AssScene{ scene, rootPath, path_extension_is( file.getName(), "mdl" )? matName.c_str() : nullptr } ) )->node();
	}
	else{
		return ( new PicoModelNode() )->node();
	}
}

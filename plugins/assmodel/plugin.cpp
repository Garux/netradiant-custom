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

#include "plugin.h"

#include <cstdio>
typedef unsigned char byte;
#include <cstdlib>
#include <algorithm>
#include <list>

#include "iscenegraph.h"
#include "irender.h"
#include "iselection.h"
#include "iimage.h"
#include "imodel.h"
#include "igl.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "ifiletypes.h"

#include "modulesystem/singletonmodule.h"
#include "stream/textstream.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "typesystem.h"

#include "model.h"

#include "assimp/Importer.hpp"
#include "assimp/importerdesc.h"
#include "assimp/Logger.hpp"
#include "assimp/DefaultLogger.hpp"
#include "assimp/IOSystem.hpp"
#include "assimp/MemoryIOWrapper.h"
#include "assimp/mesh.h"
#include "iarchive.h"
#include "idatastream.h"
#include "mdlimage.h"


class AssLogger : public Assimp::Logger
{
public:
	void OnDebug( const char* message ) override {
#ifdef _DEBUG
		globalOutputStream() << message << '\n';
#endif
	}
	void OnVerboseDebug( const char *message ) override {
#ifdef _DEBUG
		globalOutputStream() << message << '\n';
#endif
	}
	void OnInfo( const char* message ) override {
#ifdef _DEBUG
		globalOutputStream() << message << '\n';
#endif
	}
	void OnWarn( const char* message ) override {
		globalWarningStream() << message << '\n';
	}
	void OnError( const char* message ) override {
		globalErrorStream() << message << '\n';
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
		if( strchr( pFile, '\\' ) != nullptr ){
			globalWarningStream() << "AssIOSystem::Exists " << pFile << '\n';
			return false;
		}

		ArchiveFile *file = GlobalFileSystem().openFile( pFile );
		if ( file != nullptr ) {
			file->release();
			return true;
		}
		return false;
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
		if( strchr( pFile, '\\' ) != nullptr ){
			globalWarningStream() << "AssIOSystem::Open " << pFile << '\n';
			return nullptr;
		}

		ArchiveFile *file = GlobalFileSystem().openFile( pFile );
		if ( file != nullptr ) {
			const size_t size = file->size();
			byte *buffer = new byte[ size ];
			file->getInputStream().read( buffer, size );
			file->release();
			return new Assimp::MemoryIOStream( buffer, size, true );
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
		ASSERT_MESSAGE( false, "AssIOSystem::CreateDirectory" );
		return false;
	}

	// -------------------------------------------------------------------
	/** @brief Will change the current directory to the given path.
	 *  @param path     [in] The path to change to.
	 *  @return True, when the directory has changed successfully.
	 */
	bool ChangeDirectory( const std::string &path ) override {
		ASSERT_MESSAGE( false, "AssIOSystem::ChangeDirectory" );
		return false;
	}

	bool DeleteFile( const std::string &file ) override {
		ASSERT_MESSAGE( false, "AssIOSystem::DeleteFile" );
		return false;
	}

private:
};

static Assimp::Importer *s_assImporter = nullptr;

void pico_initialise(){
	s_assImporter = new Assimp::Importer();

	s_assImporter->SetPropertyBool( AI_CONFIG_PP_PTV_ADD_ROOT_TRANSFORMATION, true );
	s_assImporter->SetPropertyInteger( AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE );
	s_assImporter->SetPropertyString( AI_CONFIG_IMPORT_MDL_COLORMAP, "gfx/palette.lmp" ); // Q1 palette, default is fine too
	s_assImporter->SetPropertyBool( AI_CONFIG_IMPORT_MD3_LOAD_SHADERS, false );
	s_assImporter->SetPropertyString( AI_CONFIG_IMPORT_MD3_SHADER_SRC, "scripts/" );
	s_assImporter->SetPropertyBool( AI_CONFIG_IMPORT_MD3_HANDLE_MULTIPART, false );

	Assimp::DefaultLogger::set( new AssLogger );

	s_assImporter->SetIOHandler( new AssIOSystem );
}


class PicoModelLoader : public ModelLoader
{
public:
	PicoModelLoader(){
	}
	scene::Node& loadModel( ArchiveFile& file ){
		return loadPicoModel( *s_assImporter, file );
	}
};

class ModelPicoDependencies :
	public GlobalFileSystemModuleRef,
	public GlobalOpenGLModuleRef,
	public GlobalUndoModuleRef,
	public GlobalSceneGraphModuleRef,
	public GlobalShaderCacheModuleRef,
	public GlobalSelectionModuleRef,
	public GlobalFiletypesModuleRef
{
};

class ModelPicoAPI : public TypeSystemRef
{
	PicoModelLoader m_modelLoader;
public:
	typedef ModelLoader Type;

	ModelPicoAPI( const char* extension ){
		GlobalFiletypesModule::getTable().addType( Type::Name, extension, filetype_t( StringStream<32>( extension, " model" ), StringStream<16>( "*.", extension ) ) );
	}
	ModelLoader* getTable(){
		return &m_modelLoader;
	}
};

class PicoModelAPIConstructor
{
	CopiedString m_extension;
public:
	PicoModelAPIConstructor( const char* extension ) :
		m_extension( extension ) {
	}
	const char* getName(){
		return m_extension.c_str();
	}
	ModelPicoAPI* constructAPI( ModelPicoDependencies& dependencies ){
		return new ModelPicoAPI( m_extension.c_str() );
	}
	void destroyAPI( ModelPicoAPI* api ){
		delete api;
	}
};


typedef SingletonModule<ModelPicoAPI, ModelPicoDependencies, PicoModelAPIConstructor> PicoModelModule;
typedef std::list<PicoModelModule> PicoModelModules;
PicoModelModules g_PicoModelModules;



class ImageMDLAPI
{
	_QERPlugImageTable m_imagemdl;
public:
	typedef _QERPlugImageTable Type;
	STRING_CONSTANT( Name, "mdl" );

	ImageMDLAPI(){
		m_imagemdl.loadImage = &LoadMDLImage_;
	}
	_QERPlugImageTable* getTable(){
		return &m_imagemdl;
	}
	static Image* LoadMDLImage_( ArchiveFile& file ){
		return LoadMDLImage( *s_assImporter, file ); //!!! NOTE this lazily relies on model being loaded right before its images
	}
};

typedef SingletonModule<ImageMDLAPI, GlobalFileSystemModuleRef> ImageMDLModule;

ImageMDLModule g_ImageMDLModule;



extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );

	pico_initialise();

	for( size_t i = 0; i < s_assImporter->GetImporterCount(); ++i ){
		globalOutputStream() << s_assImporter->GetImporterInfo( i )->mName << " (" << s_assImporter->GetImporterInfo( i )->mFileExtensions << ")\n";
	}

	aiString extensions;
	s_assImporter->GetExtensionList( extensions ); // "*.3ds;*.obj;*.dae"
	const char *c = extensions.C_Str();
	while( !string_empty( c ) ){
		StringOutputStream ext( 16 );
		do{
			if( *c == '*' && *( c + 1 ) == '.' ){
				c += 2;
				continue;
			}
			else if( *c == ';' ){
				++c;
				break;
			}
			else{
				ext << *c;
				++c;
			}
		} while( !string_empty( c ) );

		g_PicoModelModules.push_back( PicoModelModule( PicoModelAPIConstructor( ext ) ) );
		g_PicoModelModules.back().selfRegister();

//		globalOutputStream() << ext << '\n';
	}

	g_ImageMDLModule.selfRegister();
}

/* TODO
	write tangents
	.obj generated weird normals direction		ArnoldSchwarzeneggerBust.OBJ	t_objFlip-plug.obj
	.ase 0		hub1.ase	assimp test utf le be	dfwc2019tv // non closed *MATERIAL_LIST brace
	?aiProcess_RemoveRedundantMaterials		e.g. in md2 //no, since mat name is ignored and diffuse may be empty
AI_CONFIG_IMPORT_GLOBAL_KEYFRAME // vertex anim frame to load
;;;wzmap: btw my fix to most of this stuff is modelconverterX	https://www.scenerydesign.org/modelconverterx/
	don't substitute q1 mdl mat paths in q1 mode
	q1 light_flame_small_yellow etc error
fbx orientation fix		https://github.com/assimp/assimp/issues/849
? aiProcess_GenSmoothNormals //if it does not join disconnected verts
	"ase ask" asc? // 'c' check in the code
crashes: !regr01.obj !openGEX
	et mdc 0 + weapons2/thompson crash
	hl mdl flipped faces + texture
hl mdl multiple texs, number of fails in models/
hl spr 'models'?
	mdl# flipped normals
	q4 lwo, *.md5mesh: mat name should be preferred // done so due to models/ textures/ prefix
*.3d;*.3ds;*.3mf;*.ac;*.ac3d;*.acc;*.amf;*.ase;*.ask;*.assbin;*.b3d;*.blend;*.bvh;*.cob;*.csm;*.dae;*.dxf;*.enff;*.fbx;*.glb;*.gltf;*.hmp;*.ifc;*.ifczip;*.iqm;*.irr;*.irrmesh;*.lwo;*.lws;*.lxo;*.m3d;*.md2;*.md3;*.md5anim;*.md5camera;*.md5mesh;*.mdc;*.mdl;*.mesh;*.mesh.xml;*.mot;*.ms3d;*.ndo;*.nff;*.obj;*.off;*.ogex;*.pk3;*.ply;*.pmx;*.prj;*.q3o;*.q3s;*.raw;*.scn;*.sib;*.smd;*.stl;*.stp;*.ter;*.uc;*.vta;*.x;*.x3d;*.x3db;*.xgl;*.xml;*.zae;*.zgl
Enabled importer formats: AMF 3DS AC ASE ASSBIN B3D BVH COLLADA DXF CSM HMP IQM IRRMESH IRR LWO LWS M3D MD2 MD3 MD5 MDC MDL NFF NDO OFF OBJ OGRE OPENGEX PLY MS3D COB BLEND IFC XGL FBX Q3D Q3BSP RAW SIB SMD STL TERRAGEN 3D X X3D GLTF 3MF MMD
_minus: bsp pk3 md5anim md5camera ogex
old list: md2 md3 ase lwo obj 3ds picoterrain mdl md5mesh ms3d fm
	-DNDEBUG
disable crashy loaders
aiProcess_JoinIdenticalVertices uses fixed const static float epsilon = 1e-5f; while spatial sort considers mesh aabb

___q3map2
	non black default colours
?support non vertex anim frames
	test failed model loading
?is PicoFixSurfaceNormals needed?
shaderlab_terrain.ase great error spam, table2.ase
	split to smaller meshes
aiProcess_SplitLargeMeshes is inefficient
	handle nodes transformations; or aiProcess_PreTransformVertices -> applies them (but also removes animations)

*/
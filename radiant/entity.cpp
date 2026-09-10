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

#include "entity.h"

#include <cstdlib>
#include <vector>
#include <map>
#include <filesystem>
#include <system_error>
#include <cstdio>
#include <cctype>
#include <cerrno>

#include "ientity.h"
#include "iselection.h"
#include "imodel.h"
#include "ifilesystem.h"
#include "imap.h"
#include "iundo.h"
#include "ireference.h"

#include "eclasslib.h"
#include "scenelib.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "stringio.h"
#include "math/quaternion.h"
#include "math/pi.h"
#include "brush.h"

#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/widget.h"
#include "gtkmisc.h"
#include "select.h"
#include "map.h"
#include "preferences.h"
#include "gtkdlgs.h"
#include "mainframe.h"
#include "camwindow.h"
#include "qe3.h"
#include "commands.h"
#include "environment.h"
#include "commandlib.h"
#include "scenegraph.h"
#include "referencecache.h"

#include "brushmanip.h"
#include "patchmanip.h"
#include "filterbar.h"

struct entity_globals_t
{
	Vector3 color_entity = Vector3( 1 );
};

entity_globals_t g_entity_globals;

class PrefabTextureLockScope
{
	bool m_previousValue;
public:
	explicit PrefabTextureLockScope( bool enabled )
		: m_previousValue( g_brush_texturelock_enabled ){
		g_brush_texturelock_enabled = enabled;
	}
	~PrefabTextureLockScope(){
		g_brush_texturelock_enabled = m_previousValue;
	}
};

bool entity_path_is_misc_prefab( const scene::Path& selectedPath ){
	const scene::Path& path = ( !Node_isEntity( selectedPath.top() ) && selectedPath.size() > 1 )
		? selectedPath.parent()
		: selectedPath;
	Entity* entity = Node_getEntity( path.top() );
	return entity != nullptr && string_equal( entity->getClassName(), "misc_prefab" );
}

Vector3 prefab_read_angles( const Entity& entity ){
	Vector3 angles( 0, 0, 0 );
	if( entity.hasKeyValue( "angles" ) ){
		Vector3 parsed;
		if( string_parse_vector3( entity.getKeyValue( "angles" ), parsed ) ){
			return Vector3( parsed[2], parsed[0], parsed[1] );
		}
	}

	float yaw = 0.f;
	if( string_parse_float( entity.getKeyValue( "angle" ), yaw ) ){
		angles[2] = yaw;
	}
	return angles;
}

Vector3 prefab_read_scale( const Entity& entity ){
	if( entity.hasKeyValue( "modelscale_vec" ) ){
		Vector3 scale;
		if( string_parse_vector3( entity.getKeyValue( "modelscale_vec" ), scale )
		    && scale[0] != 0.f
		    && scale[1] != 0.f
		    && scale[2] != 0.f ){
			return scale;
		}
	}

	float uniformScale = 1.f;
	if( string_parse_float( entity.getKeyValue( "modelscale" ), uniformScale ) && uniformScale != 0.f ){
		return Vector3( uniformScale, uniformScale, uniformScale );
	}

	return Vector3( 1, 1, 1 );
}

Matrix4 prefab_instance_local_to_world( const Vector3& origin, const Vector3& angles, const Vector3& scale ){
	Matrix4 transform = g_matrix4_identity;
	matrix4_transform_by_euler_xyz_degrees( transform, origin, angles, scale );
	return transform;
}

Vector3 camera_angles_from_direction( const Vector3& direction, const Vector3& fallback ){
	if( vector3_length_squared( direction ) <= 1e-10f ){
		return fallback;
	}
	const Vector3 dir = vector3_normalised( direction );
	Vector3 angles = fallback;
	angles[CAMERA_YAW] = radians_to_degrees( atan2( dir[1], dir[0] ) );
	angles[CAMERA_PITCH] = radians_to_degrees( asin( dir[2] ) );
	return angles;
}

Entity* selected_misc_prefab_entity( scene::Path* outPath = nullptr ){
	if( GlobalSelectionSystem().countSelected() == 0 )
		return nullptr;

	scene::Path path = GlobalSelectionSystem().ultimateSelected().path();
	if( !Node_isEntity( path.top() ) && path.size() > 1 )
		path.pop();
	Entity* entity = Node_getEntity( path.top() );
	if( entity == nullptr || !string_equal( entity->getClassName(), "misc_prefab" ) )
		return nullptr;

	if( outPath != nullptr )
		*outPath = path;
	return entity;
}

std::string resolve_prefab_path( const char* prefab ){
	if( prefab == nullptr || string_empty( prefab ) )
		return {};

	const std::string cleaned = StringStream( PathCleaned( prefab ) ).c_str();
	const bool hasMapsPrefix = cleaned.size() > 5
		&& ( cleaned[0] == 'm' || cleaned[0] == 'M' )
		&& ( cleaned[1] == 'a' || cleaned[1] == 'A' )
		&& ( cleaned[2] == 'p' || cleaned[2] == 'P' )
		&& ( cleaned[3] == 's' || cleaned[3] == 'S' )
		&& cleaned[4] == '/';
	const std::string trimmedMaps = hasMapsPrefix ? cleaned.substr( 5 ) : cleaned;
	const std::string mapsRelative = StringStream( "maps/", PathCleaned( trimmedMaps.c_str() ) ).c_str();
	const std::string fallbackInMaps = StringStream( DirectoryCleaned( getMapsPath() ), PathCleaned( trimmedMaps.c_str() ) ).c_str();
	if( path_is_absolute( cleaned.c_str() ) && file_exists( cleaned.c_str() ) )
		return cleaned;

	const auto inMaps = fallbackInMaps;
	if( file_exists( inMaps.c_str() ) )
		return inMaps.c_str();

	const auto inUserGameMaps = StringStream( DirectoryCleaned( g_qeglobals.m_userGamePath.c_str() ), mapsRelative );
	if( file_exists( inUserGameMaps.c_str() ) )
		return inUserGameMaps.c_str();

	const auto inEngineMaps = StringStream( DirectoryCleaned( EnginePath_get() ), mapsRelative );
	if( file_exists( inEngineMaps.c_str() ) )
		return inEngineMaps.c_str();

	/* Ensure map importer always gets an absolute path for relative prefab keys. */
	return path_is_absolute( cleaned.c_str() ) ? cleaned : fallbackInMaps;
}

std::map<std::string, std::filesystem::file_time_type> g_prefabFileWriteTime;

void Prefab_refreshReferenceByName( const std::string& name ){
	if( name.empty() ){
		return;
	}
	Resource* resource = GlobalReferenceCache().capture( name.c_str() );
	if( resource == nullptr ){
		return;
	}
	resource->refresh();
	GlobalReferenceCache().release( name.c_str() );
}

void Prefab_refreshReferenceIfChanged( const char* prefabModelPath ){
	if( prefabModelPath == nullptr || string_empty( prefabModelPath ) ){
		return;
	}

	const std::string modelPath = StringStream( PathCleaned( prefabModelPath ) ).c_str();
	const std::string absolutePath = resolve_prefab_path( prefabModelPath );
	std::string trackedPath = absolutePath.empty() ? modelPath : StringStream( PathCleaned( absolutePath.c_str() ) ).c_str();

	bool shouldRefresh = true;
	if( !trackedPath.empty() ){
		std::error_code ec;
		const std::filesystem::path filePath( trackedPath );
		if( std::filesystem::exists( filePath, ec ) ){
			ec.clear();
			const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time( filePath, ec );
			if( !ec ){
				if( const auto i = g_prefabFileWriteTime.find( trackedPath ); i != g_prefabFileWriteTime.end() && i->second == writeTime ){
					shouldRefresh = false;
				}
				else{
					g_prefabFileWriteTime[trackedPath] = writeTime;
				}
			}
		}
	}

	if( !shouldRefresh ){
		return;
	}

	Prefab_refreshReferenceByName( modelPath );
	if( !absolutePath.empty() && !path_equal( absolutePath.c_str(), modelPath.c_str() ) ){
		Prefab_refreshReferenceByName( absolutePath );
	}
}

struct PrefabEditContext
{
	scene::Path previousTraversalRoot;
	bool hadPreviousTraversalRoot;
	bool mapWasModifiedBeforeEnter = false;
	std::vector<scene::Path> previousSelection;
	scene::Path enteredPrefabParentPath;
	scene::Path enteredPrefabPath;
	std::string prefabFile;
	std::string prefabResourceName;
	NodeSmartReference enteredPrefabOriginalNode = NodeSmartReference( NewNullNode() );
	NodeSmartReference enteredPrefabWorkingNode = NodeSmartReference( NewNullNode() );
	bool hasPrefabCenter = false;
	Vector3 prefabCenter = g_vector3_identity;
	bool hasSavedCamera = false;
	Vector3 savedCameraOrigin = g_vector3_identity;
	Vector3 savedCameraAngles = g_vector3_identity;
	Vector3 prefabInstanceOrigin = g_vector3_identity;
};

std::vector<PrefabEditContext> g_prefabEditStack;

bool prefab_parse_vec3_at( const char* p, Vector3& out, const char** outNext ){
	const char* cur = p;
	while( *cur != '\0' && std::isspace( static_cast<unsigned char>( *cur ) ) ){
		++cur;
	}
	if( *cur != '(' ){
		return false;
	}
	++cur;

	char* end = nullptr;
	errno = 0;
	const double x = std::strtod( cur, &end );
	if( end == cur || errno != 0 ){
		return false;
	}
	cur = end;

	errno = 0;
	const double y = std::strtod( cur, &end );
	if( end == cur || errno != 0 ){
		return false;
	}
	cur = end;

	errno = 0;
	const double z = std::strtod( cur, &end );
	if( end == cur || errno != 0 ){
		return false;
	}
	cur = end;

	while( *cur != '\0' && std::isspace( static_cast<unsigned char>( *cur ) ) ){
		++cur;
	}
	if( *cur != ')' ){
		return false;
	}
	++cur;

	out = Vector3( static_cast<float>( x ), static_cast<float>( y ), static_cast<float>( z ) );
	*outNext = cur;
	return true;
}

bool prefab_read_bounds_center( const char* filePath, Vector3& outCenter ){
	if( filePath == nullptr || string_empty( filePath ) ){
		return false;
	}

	std::FILE* f = std::fopen( filePath, "rb" );
	if( f == nullptr ){
		return false;
	}
	if( std::fseek( f, 0, SEEK_END ) != 0 ){
		std::fclose( f );
		return false;
	}
	const long size = std::ftell( f );
	if( size < 0 ){
		std::fclose( f );
		return false;
	}
	if( std::fseek( f, 0, SEEK_SET ) != 0 ){
		std::fclose( f );
		return false;
	}

	std::string text;
	text.resize( static_cast<std::size_t>( size ) );
	if( size > 0 ){
		const std::size_t read = std::fread( &text[0], 1, static_cast<std::size_t>( size ), f );
		if( read != static_cast<std::size_t>( size ) ){
			std::fclose( f );
			return false;
		}
	}
	std::fclose( f );

	bool havePoint = false;
	Vector3 mins( g_vector3_identity );
	Vector3 maxs( g_vector3_identity );
	const char* cur = text.c_str();
	while( *cur != '\0' ){
		if( *cur != '(' ){
			++cur;
			continue;
		}
		Vector3 point;
		const char* next = cur;
		if( prefab_parse_vec3_at( next, point, &next ) ){
			if( !havePoint ){
				mins = maxs = point;
				havePoint = true;
			}
			else{
				mins[0] = std::min( mins[0], point[0] );
				mins[1] = std::min( mins[1], point[1] );
				mins[2] = std::min( mins[2], point[2] );
				maxs[0] = std::max( maxs[0], point[0] );
				maxs[1] = std::max( maxs[1], point[1] );
				maxs[2] = std::max( maxs[2], point[2] );
			}
			cur = next;
		}
		else{
			++cur;
		}
	}

	if( !havePoint ){
		return false;
	}

	outCenter = vector3_mid( mins, maxs );
	return true;
}

bool prefab_compute_save_center_compensation( const PrefabEditContext& context, Vector3& outShift ){
	outShift = g_vector3_identity;
	if( !context.hasPrefabCenter ){
		return false;
	}
	Vector3 mins, maxs;
	Select_GetBounds( mins, maxs );
	const Vector3 currentCenter = vector3_mid( mins, maxs );
	outShift = vector3_subtracted( context.prefabCenter, currentCenter );
	return vector3_length_squared( outShift ) > 0.0001;
}

bool ensure_worldspawn_entity_in_map_file( const char* filename );

void prefab_select_all_selectable_descendants( const scene::Path& rootPath ){
	class SelectDescendantsWalker : public scene::Graph::Walker
	{
		mutable std::size_t m_depth{ 0 };
	public:
		bool pre( const scene::Path& path, scene::Instance& instance ) const override {
			if( m_depth > 0 ){
				if( Selectable* selectable = Instance_getSelectable( instance ); selectable != nullptr ){
					selectable->setSelected( true );
				}
			}
			++m_depth;
			return true;
		}
		void post( const scene::Path& path, scene::Instance& instance ) const override {
			(void)path;
			(void)instance;
			--m_depth;
		}
	};

	GlobalSceneGraph().traverse_subgraph( SelectDescendantsWalker(), rootPath );
}

void prefab_ensure_any_selection_for_save( const scene::Path& preferredPath, const scene::Path& fallbackPath ){
	if( GlobalSelectionSystem().countSelected() != 0 ){
		return;
	}
	if( scene::Instance* preferred = GlobalSceneGraph().find( preferredPath ); preferred != nullptr ){
		Instance_setSelected( *preferred, true );
		return;
	}
	if( scene::Instance* fallback = GlobalSceneGraph().find( fallbackPath ); fallback != nullptr ){
		Instance_setSelected( *fallback, true );
	}
}

bool prefab_save_resource_if_dirty( const std::string& resourceName ){
	if( resourceName.empty() ){
		return true;
	}

	Resource* resource = GlobalReferenceCache().capture( resourceName.c_str() );
	if( resource == nullptr ){
		return false;
	}

	bool ok = true;
	if( scene::Node* node = resource->getNode(); node != nullptr ){
		if( MapFile* map = Node_getMapFile( *node ); map != nullptr ){
			if( !map->saved() ){
				ok = resource->save();
			}
		}
	}
	GlobalReferenceCache().release( resourceName.c_str() );
	return ok;
}

bool prefab_save_current_resource( const PrefabEditContext& context ){
	const std::string byName = StringStream( PathCleaned( context.prefabResourceName.c_str() ) ).c_str();
	const std::string byFile = StringStream( PathCleaned( context.prefabFile.c_str() ) ).c_str();

	bool ok = true;
	ok = ok && prefab_save_resource_if_dirty( byName );
	if( !path_equal( byName.c_str(), byFile.c_str() ) ){
		ok = ok && prefab_save_resource_if_dirty( byFile );
	}
	return ok && ensure_worldspawn_entity_in_map_file( context.prefabFile.c_str() );
}

bool Entity_isPrefabEditMode(){
	return !g_prefabEditStack.empty();
}

void PrefabEdit_UpdateLeaveCommandEnabled(){
	if( QAction* action = GlobalCommands_find( "PrefabLeave" ).getAction(); action != nullptr ){
		action->setEnabled( Entity_isPrefabEditMode() );
	}
}

void PrefabEdit_DiscardReferenceChanges( const std::string& prefabResourceName, const std::string& prefabFile ){
	const auto discardByName = []( const std::string& name ){
		if( name.empty() ){
			return;
		}
		Resource* resource = GlobalReferenceCache().capture( name.c_str() );
		if( resource == nullptr ){
			return;
		}

		/* Prefab contents in editor are backed by the shared reference-cache resource.
		   On "Leave Prefab -> No", force a reload from disk to drop in-memory edits. */
		resource->flush();
		resource->unrealise();
		resource->realise();
		GlobalReferenceCache().release( name.c_str() );
	};

	discardByName( prefabResourceName );
	if( prefabFile != prefabResourceName ){
		discardByName( prefabFile );
	}
}

bool PrefabEdit_HasUnsavedReferenceChanges( const std::string& prefabResourceName, const std::string& prefabFile ){
	const auto hasUnsavedByName = []( const std::string& name ){
		if( name.empty() ){
			return false;
		}
		Resource* resource = GlobalReferenceCache().capture( name.c_str() );
		if( resource == nullptr ){
			return false;
		}
		bool unsaved = false;
		if( scene::Node* node = resource->getNode(); node != nullptr ){
			if( MapFile* map = Node_getMapFile( *node ); map != nullptr ){
				unsaved = !map->saved();
			}
		}
		GlobalReferenceCache().release( name.c_str() );
		return unsaved;
	};

	if( hasUnsavedByName( prefabResourceName ) ){
		return true;
	}
	if( prefabFile != prefabResourceName && hasUnsavedByName( prefabFile ) ){
		return true;
	}
	return false;
}

bool Entity_getPrefabEditWorldspawn( scene::Path& outPath, scene::Traversable*& outTraversable ){
	outTraversable = nullptr;
	if( !Entity_isPrefabEditMode() || !SceneGraph_HasTraversalRoot() ){
		return false;
	}
	const scene::Path* traversalRoot = SceneGraph_GetTraversalRoot();
	if( traversalRoot == nullptr ){
		return false;
	}
	scene::Traversable* rootTraversable = Node_getTraversable( traversalRoot->top() );
	if( rootTraversable == nullptr ){
		return false;
	}

	class FindDirectWorldspawnWalker : public scene::Traversable::Walker {
	public:
		mutable scene::Node* m_found{ nullptr };
		mutable scene::Node* m_fallback{ nullptr };
		bool pre( scene::Node& child ) const override {
			if( m_found != nullptr ){
				return false;
			}
			if( Node_getTraversable( child ) != nullptr ){
				if( m_fallback == nullptr ){
					m_fallback = &child;
				}
				if( Entity* entity = Node_getEntity( child ); entity != nullptr
				 && string_equal( entity->getClassName(), "worldspawn" ) ){
					m_found = &child;
				}
			}
			/* Only direct children of traversal root are relevant. */
			return false;
		}
	} walker;

	rootTraversable->traverse( walker );
	scene::Node* target = walker.m_found != nullptr ? walker.m_found : walker.m_fallback;
	if( target == nullptr ){
		return false;
	}

	scene::Traversable* worldTraversable = Node_getTraversable( *target );
	if( worldTraversable == nullptr ){
		return false;
	}

	outPath = *traversalRoot;
	outPath.push( makeReference( *target ) );
	outTraversable = worldTraversable;
	return true;
}

std::vector<scene::Path> prefab_capture_selected_paths(){
	class CaptureSelectedPathsVisitor : public SelectionSystem::Visitor
	{
	public:
		mutable std::vector<scene::Path> m_paths;
		void visit( scene::Instance& instance ) const override {
			m_paths.push_back( instance.path() );
		}
	};

	CaptureSelectedPathsVisitor visitor;
	GlobalSelectionSystem().foreachSelected( visitor );
	return visitor.m_paths;
}

scene::Node& Entity_createPoint( const char* classname, const Vector3& origin ){
	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( classname, !string_equal( classname, "misc_prefab" ) );
	NodeSmartReference node( GlobalEntityCreator().createEntity( entityClass ) );
	Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

	scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
	entitypath.push( makeReference( node.get() ) );
	scene::Instance& instance = findInstance( entitypath );
	if( Transformable* transform = Instance_getTransformable( instance ); transform != nullptr ){
		transform->setType( TRANSFORM_PRIMITIVE );
		transform->setTranslation( origin );
		transform->freezeTransform();
	}

	GlobalSelectionSystem().setSelectedAll( false );
	Instance_setSelected( instance, true );
	return node.get();
}

std::string read_text_file( const char* filename ){
	TextFileInputStream file( filename );
	if( file.failed() )
		return {};

	std::string content;
	char buffer[4096];
	for( ;; ){
		const std::size_t bytes = file.read( buffer, sizeof( buffer ) );
		if( bytes == 0 )
			break;
		content.append( buffer, bytes );
	}
	return content;
}

bool ensure_worldspawn_entity_in_map_file( const char* filename ){
	const std::string content = read_text_file( filename );
	if( content.empty() )
		return false;

	if( content.find( "\"classname\" \"worldspawn\"" ) != std::string::npos )
		return true;

	TextFileOutputStream out( filename );
	if( out.failed() )
		return false;

	out << "{\n"
	    << "\"classname\" \"worldspawn\"\n"
	    << "}\n\n"
	    << content;
	return true;
}

class EntitySetKeyValueSelected : public scene::Graph::Walker
{
	const char* m_key;
	const char* m_value;
public:
	EntitySetKeyValueSelected( const char* key, const char* value )
		: m_key( key ), m_value( value ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const override {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0
		     && ( instance.childSelected() || Instance_isSelected( instance ) ) ) {
			entity->setKeyValue( m_key, m_value );
		}
	}
};

class EntitySetClassnameSelected : public scene::Graph::Walker
{
	const char* m_classname;
	scene::Node* m_world;
	const bool m_2world;
public:
	EntitySetClassnameSelected( const char* classname )
		: m_classname( classname ), m_world( Map_FindWorldspawn( g_map ) ), m_2world( m_world && string_equal( m_classname, "worldspawn" ) ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const override {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0 && ( instance.childSelected() || Instance_isSelected( instance ) ) ) {
			if( path.top().get_pointer() == m_world ){ /* do not want to convert whole worldspawn entity */
				if( instance.childSelected() && !m_2world ){ /* create an entity from world brushes instead */
					EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( m_classname, !string_equal( m_classname, "misc_prefab" ) );
					if( entityClass->fixedsize )
						return;

					//is important to have retexturing here; if doing in the end, undo doesn't succeed; //don't do this extra now, as it requires retexturing, working for subgraph
//					if ( string_compare_nocase_n( m_classname, "trigger_", 8 ) == 0 ){
//						Scene_PatchSetShader_Selected( GlobalSceneGraph(), GetCommonShader( "trigger" ).c_str() );
//						Scene_BrushSetShader_Selected( GlobalSceneGraph(), GetCommonShader( "trigger" ).c_str() );
//					}

					NodeSmartReference node( GlobalEntityCreator().createEntity( entityClass ) );
					Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

					scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
					entitypath.push( makeReference( node.get() ) );
					scene::Instance& entityInstance = findInstance( entitypath );

					if ( g_pGameDescription->mGameType == "doom3" ) {
						Node_getEntity( node )->setKeyValue( "model", Node_getEntity( node )->getKeyValue( "name" ) );
					}

					//Scene_parentSelectedBrushesToEntity( GlobalSceneGraph(), node );
					Scene_parentSubgraphSelectedBrushesToEntity( GlobalSceneGraph(), node, path );
					Scene_forEachChildSelectable( SelectableSetSelected( true ), entityInstance.path() );
				}
				return;
			}
			else if( m_2world ){ /* ungroupSelectedEntities */ //condition is skipped with world = 0, so code next to this may create multiple worldspawns; todo handle this very special case?
				if( node_is_group( path.top() ) ){
					parentBrushes( path.top(), *m_world );
					Path_deleteTop( path );
				}
				return;
			}

			EntityClass* eclass = GlobalEntityClassManager().findOrInsert( m_classname, node_is_group( path.top() ) && !string_equal( m_classname, "misc_prefab" ) );
			NodeSmartReference node( GlobalEntityCreator().createEntity( eclass ) );

			if( entity->isContainer() && eclass->fixedsize ){ /* group entity to point one */
				char value[64];
				sprintf( value, "%g %g %g", instance.worldAABB().origin[0], instance.worldAABB().origin[1], instance.worldAABB().origin[2] );
				entity->setKeyValue( "origin", value );
			}

			EntityCopyingVisitor visitor( *Node_getEntity( node ) );
//		entity->forEachKeyValue( visitor );

			NodeSmartReference child( path.top().get() );
			NodeSmartReference parent( path.parent().get() );
//		Node_getTraversable( parent )->erase( child );
			if ( Node_getTraversable( child ) != 0 && node_is_group( node ) ) { /* group entity to group one */
				parentBrushes( child, node );
			}
			Node_getTraversable( parent )->insert( node );

			entity->forEachKeyValue( visitor ); /* must do this after inserting node, otherwise problem: targeted + having model + not loaded b4 new entities aren't selectable normally + rendered only while 0 0 0 is rendered */

			if( !entity->isContainer() && !eclass->fixedsize ){ /* point entity to group one */
				AABB bounds( g_vector3_identity, Vector3( 16, 16, 16 ) );
				if ( !string_parse_vector3( entity->getKeyValue( "origin" ), bounds.origin ) ) {
					bounds.origin = g_vector3_identity;
				}
				Brush_ConstructPlacehoderCuboid( node.get(), bounds );
				Node_getEntity( node )->setKeyValue( "origin", "" );
			}

			Node_getTraversable( parent )->erase( child );
		}
	}
};

void Scene_EntitySetKeyValue_Selected( const char* key, const char* value ){
	GlobalSceneGraph().traverse( EntitySetKeyValueSelected( key, value ) );
}

void Scene_EntitySetClassname_Selected( const char* classname ){
	if ( GlobalSelectionSystem().countSelected() > 0 ) {
		StringOutputStream command( 64 );
		if( string_equal( classname, "worldspawn" ) )
			command << "ungroupSelectedEntities";
		else
			command << "entitySetClass -class " << classname;
		UndoableCommand undo( command );
		GlobalSceneGraph().traverse( EntitySetClassnameSelected( classname ) );
	}
}

void Entity_ungroup(){
	Scene_EntitySetClassname_Selected( "worldspawn" );
}

#if 0
void Entity_ungroupSelected(){
	if ( GlobalSelectionSystem().countSelected() < 1 ) {
		return;
	}

	UndoableCommand undo( "ungroupSelectedEntities" );

	scene::Instance &instance = GlobalSelectionSystem().ultimateSelected();
	scene::Path path = instance.path();

	scene::Node& world = Map_FindOrInsertWorldspawn( g_map );

	if ( !Node_isEntity( path.top() ) && path.size() > 1 ) {
		path.pop();
	}

	if ( Node_isEntity( path.top() )
	  && node_is_group( path.top() ) ) {
		if ( &world != path.top().get_pointer() ) {
			parentBrushes( path.top(), world );
			Path_deleteTop( path );
		}
	}
}
#endif

#if 0
class EntityFindSelected : public scene::Graph::Walker
{
public:
	mutable const scene::Path *groupPath;
	mutable scene::Instance *groupInstance;
	EntityFindSelected() : groupPath( 0 ), groupInstance( 0 ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0
		  && Instance_isSelected( instance )
		  && node_is_group( path.top() )
		  && !groupPath ) {
			groupPath = &path;
			groupInstance = &instance;
		}
	}
};

class EntityGroupSelected : public scene::Graph::Walker
{
	NodeSmartReference group, worldspawn;
//typedef std::pair<NodeSmartReference, NodeSmartReference> DeletionPair;
//Stack<DeletionPair> deleteme;
public:
	EntityGroupSelected( const scene::Path &p ) : group( p.top().get() ), worldspawn( Map_FindOrInsertWorldspawn( g_map ) ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		if ( Instance_isSelected( instance ) ) {
			Entity* entity = Node_getEntity( path.top() );
			if ( entity == 0 && Node_isPrimitive( path.top() ) ) {
				NodeSmartReference child( path.top().get() );
				NodeSmartReference parent( path.parent().get() );

				if ( path.size() >= 3 && parent != worldspawn ) {
					NodeSmartReference parentparent( path[path.size() - 3].get() );

					Node_getTraversable( parent )->erase( child );
					Node_getTraversable( group )->insert( child );

					if ( Node_getTraversable( parent )->empty() ) {
						//deleteme.push( DeletionPair( parentparent, parent ) );
						Node_getTraversable( parentparent )->erase( parent );
					}
				}
				else
				{
					Node_getTraversable( parent )->erase( child );
					Node_getTraversable( group )->insert( child );
				}
			}
		}
	}
};
/// moves selected primitives to entity, whose entityNode is selected or to worldspawn, if none
void Entity_moveSelectedPrimitives(){
	if ( GlobalSelectionSystem().countSelected() < 1 ) {
		return;
	}

	UndoableCommand undo( "reGroupSelectedEntities" );

	scene::Path world_path( makeReference( GlobalSceneGraph().root() ) );
	world_path.push( makeReference( Map_FindOrInsertWorldspawn( g_map ) ) );

	EntityFindSelected fse;
	GlobalSceneGraph().traverse( fse );
	if ( fse.groupPath ) {
		GlobalSceneGraph().traverse( EntityGroupSelected( *fse.groupPath ) );
	}
	else
	{
		GlobalSceneGraph().traverse( EntityGroupSelected( world_path ) );
	}
}
#else
/// moves selected primitives to entity, which is or its primitive is ultimateSelected() or firstSelected()
void Entity_moveSelectedPrimitives( bool toLast ){
	if ( GlobalSelectionSystem().countSelected() < 2 ) {
		globalErrorStream() << "Source and target entity primitives should be selected!\n";
		return;
	}

	const scene::Path& path = toLast? GlobalSelectionSystem().ultimateSelected().path() : GlobalSelectionSystem().firstSelected().path();
	scene::Node& node = ( !Node_isEntity( path.top() ) && path.size() > 1 )? path.parent() : path.top();

	if ( Node_isEntity( node ) && node_is_group( node ) ) {
		const auto command = StringStream<64>( "movePrimitivesToEntity ", Quoted( Node_getEntity( node )->getClassName() ) );
		UndoableCommand undo( command );
		Scene_parentSelectedBrushesToEntity( GlobalSceneGraph(), node );
	}
}
void Entity_moveSelectedPrimitivesToLast(){
	Entity_moveSelectedPrimitives( true );
}
void Entity_moveSelectedPrimitivesToFirst(){
	Entity_moveSelectedPrimitives( false );
}
#endif



void Entity_connectSelected(){
	if ( GlobalSelectionSystem().countSelected() == 2 ) {
		GlobalEntityCreator().connectEntities(
		    GlobalSelectionSystem().penultimateSelected().path(),
		    GlobalSelectionSystem().ultimateSelected().path(),
		    0
		);
	}
	else
	{
		globalErrorStream() << "entityConnectSelected: exactly two instances must be selected\n";
	}
}

void Entity_killconnectSelected(){
	if ( GlobalSelectionSystem().countSelected() == 2 ) {
		GlobalEntityCreator().connectEntities(
		    GlobalSelectionSystem().penultimateSelected().path(),
		    GlobalSelectionSystem().ultimateSelected().path(),
		    1
		);
	}
	else
	{
		globalErrorStream() << "entityKillConnectSelected: exactly two instances must be selected\n";
	}
}

AABB Doom3Light_getBounds( const AABB& workzone ){
	AABB aabb( workzone );

	Vector3 defaultRadius( 300, 300, 300 );
	if ( !string_parse_vector3( EntityClass_valueForKey( *GlobalEntityClassManager().findOrInsert( "light", false ), "light_radius" ), defaultRadius ) ) {
		globalErrorStream() << "Doom3Light_getBounds: failed to parse default light radius\n";
	}

	if ( aabb.extents[0] == 0 ) {
		aabb.extents[0] = defaultRadius[0];
	}
	if ( aabb.extents[1] == 0 ) {
		aabb.extents[1] = defaultRadius[1];
	}
	if ( aabb.extents[2] == 0 ) {
		aabb.extents[2] = defaultRadius[2];
	}

	if ( aabb_valid( aabb ) ) {
		return aabb;
	}
	return AABB( Vector3( 0, 0, 0 ), Vector3( 64, 64, 64 ) );
}


int g_iLastLightIntensity = 300;

void Entity_createFromSelection( const char* name, const Vector3& origin, const char* model ){
#if 0
	if ( string_equal_nocase( name, "worldspawn" ) ) {
		qt_MessageBox( MainFrame_getWindow(), "Can't create an entity with worldspawn.", "info" );
		return;
	}
#else
	if ( string_equal( name, "worldspawn" ) ) {
		scene::Path prefabWorldPath;
		scene::Traversable* prefabWorldTraversable = nullptr;
		if( Entity_getPrefabEditWorldspawn( prefabWorldPath, prefabWorldTraversable ) ){
			UndoableCommand undo( "ungroupSelectedPrimitives" );
			Scene_parentSubgraphSelectedBrushesToEntity( GlobalSceneGraph(), prefabWorldPath.top(), prefabWorldPath );
			return;
		}
		// only process if worldspawn is present
		// Map_FindOrInsertWorldspawn( g_map ) ) would be no action (since worldspawn insertion deselects everything)
		if( scene::Node* world_node = Map_FindWorldspawn( g_map ) ){
			UndoableCommand undo( "ungroupSelectedPrimitives" );
			Scene_parentSelectedBrushesToEntity( GlobalSceneGraph(), *world_node );
			return;
		}
	}
#endif

	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( name, !string_equal( name, "misc_prefab" ) );

	const bool isModel = entityClass->miscmodel_is
	                     || string_equal( name, "misc_prefab" )
	                     || ( GlobalSelectionSystem().countSelected() == 0 && classname_equal( name, "func_static" ) && g_pGameDescription->mGameType == "doom3" );
	const bool needsModelDialog = isModel
		&& string_empty( EntityClass_valueForKey( *entityClass, entityClass->miscmodel_key() ) )
		&& ( model == nullptr || string_empty( model ) );
	const char* chosenModel = model;
	if( needsModelDialog ){
		chosenModel = classname_equal( name, "misc_prefab" )
			? misc_prefab_dialog( MainFrame_getWindow() )
			: misc_model_dialog( MainFrame_getWindow() );
		if( chosenModel == nullptr || string_empty( chosenModel ) ){
			return;
		}
	}

	const auto command = StringStream<64>( "entityCreate -class ", name );
	UndoableCommand undo( command );

	const bool brushesSelected = Scene_countSelectedBrushes( GlobalSceneGraph() ) != 0;

	//is important to have retexturing here; if doing in the end, undo doesn't succeed;
	if ( string_compare_nocase_n( name, "trigger_", 8 ) == 0 && brushesSelected && !entityClass->fixedsize ){
		//const char* shader = GetCommonShader( "trigger" ).c_str();
		Scene_PatchSetShader_Selected( GlobalSceneGraph(), GetCommonShader( "trigger" ).c_str() );
		Scene_BrushSetShader_Selected( GlobalSceneGraph(), GetCommonShader( "trigger" ).c_str() );
	}

	if ( !( entityClass->fixedsize || isModel ) && !brushesSelected ) {
		globalErrorStream() << "failed to create a group entity - no brushes are selected\n";
		return;
	}

	AABB workzone( aabb_for_minmax( Select_getWorkZone().d_work_min, Select_getWorkZone().d_work_max ) );


	NodeSmartReference node( GlobalEntityCreator().createEntity( entityClass ) );
	scene::Path parentPath( makeReference( GlobalSceneGraph().root() ) );
	scene::Traversable* parentTraversable = Node_getTraversable( GlobalSceneGraph().root() );
	scene::Path prefabWorldPath;
	scene::Traversable* prefabWorldTraversable = nullptr;
	if( Entity_getPrefabEditWorldspawn( prefabWorldPath, prefabWorldTraversable ) ){
		parentPath = prefabWorldPath;
		parentTraversable = prefabWorldTraversable;
	}
	parentTraversable->insert( node );

	scene::Path entitypath( parentPath );
	entitypath.push( makeReference( node.get() ) );
	scene::Instance& instance = findInstance( entitypath );

	Entity* entity = Node_getEntity( node );

	if ( entityClass->fixedsize || ( isModel && !brushesSelected ) ) {
		//Select_Delete();

		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			Vector3 localOrigin = origin;
			if( scene::Instance* parentInstance = GlobalSceneGraph().find( parentPath ); parentInstance != nullptr ){
				localOrigin = matrix4_transformed_point( matrix4_affine_inverse( parentInstance->localToWorld() ), origin );
			}
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setTranslation( localOrigin );
			transform->freezeTransform();
		}

		GlobalSelectionSystem().setSelectedAll( false );

		Instance_setSelected( instance, true );
	}
	else
	{
		if ( g_pGameDescription->mGameType == "doom3" ) {
			entity->setKeyValue( "model", entity->getKeyValue( "name" ) );
		}

		Scene_parentSelectedBrushesToEntity( GlobalSceneGraph(), node );
		Scene_forEachChildSelectable( SelectableSetSelected( true ), instance.path() );
	}

	// tweaking: when right click dropping a light entity, ask for light value in a custom dialog box
	// see SF bug 105383

	if ( g_pGameDescription->mGameType == "hl" ) {
		// FIXME - Hydra: really we need a combined light AND color dialog for halflife.
		if ( string_equal_nocase( name, "light" )
		  || string_equal_nocase( name, "light_environment" )
		  || string_equal_nocase( name, "light_spot" ) ) {
			int intensity = g_iLastLightIntensity;

			if ( DoLightIntensityDlg( &intensity ) ) {
				g_iLastLightIntensity = intensity;
				char buf[30];
				sprintf( buf, "255 255 255 %d", intensity );
				entity->setKeyValue( "_light", buf );
			}
		}
	}
	else if ( string_equal_nocase( name, "light" ) ) {
		if ( g_pGameDescription->mGameType != "doom3" ) {
			int intensity = g_iLastLightIntensity;

			if ( DoLightIntensityDlg( &intensity ) ) {
				g_iLastLightIntensity = intensity;
				char buf[10];
				sprintf( buf, "%d", intensity );
				entity->setKeyValue( "light", buf );
			}
		}
		else if ( brushesSelected ) { // use workzone to set light position/size for doom3 lights, if there are brushes selected
			const AABB bounds( Doom3Light_getBounds( workzone ) );
			entity->setKeyValue( "origin", StringStream<64>( bounds.origin[0], ' ', bounds.origin[1], ' ', bounds.origin[2] ) );
			entity->setKeyValue( "light_radius", StringStream<64>( bounds.extents[0], ' ', bounds.extents[1], ' ', bounds.extents[2] ) );
		}
	}

	if( chosenModel != nullptr && !string_empty( chosenModel ) ){
		if( string_equal( name, "misc_prefab" ) ){
			/* Insertion from prefab browser must use current file contents, not stale cache. */
			Prefab_refreshReferenceIfChanged( chosenModel );
		}
		entity->setKeyValue( entityClass->miscmodel_key(), chosenModel );
	}

	if( string_equal( name, "misc_prefab" ) ){
		Instance_setSelected( instance, false );
	}
}

void Entity_ungroupSelectedPrimitives(){
	Entity_createFromSelection( "worldspawn", g_vector3_identity );
}

bool Entity_isSelectedMiscPrefab(){
	return GlobalSelectionSystem().countSelected() != 0
		&& entity_path_is_misc_prefab( GlobalSelectionSystem().ultimateSelected().path() );
}

void Entity_prefabCreateFromSelection(){
	if( GlobalSelectionSystem().countSelected() == 0 ){
		globalErrorStream() << "Create Prefab: select at least one object.\n";
		return;
	}

	if( GlobalSelectionSystem().countSelectedComponents() != 0 ){
		globalErrorStream() << "Create Prefab: switch to object selection mode.\n";
		return;
	}

	std::string defaultPath;
	if( !Map_Unnamed( g_map ) && path_is_absolute( Map_Name( g_map ) ) ){
		defaultPath = StringStream( PathExtensionless( Map_Name( g_map ) ), "_prefab.map" ).c_str();
	}
	else{
		defaultPath = StringStream( DirectoryCleaned( getMapsPath() ), "new_prefab.map" ).c_str();
	}

	const char* chosen = file_dialog( MainFrame_getWindow(), false, "Save Prefab As", defaultPath.c_str(), MapFormat::Name, false, false, true );
	if( chosen == nullptr || string_empty( chosen ) )
		return;

	std::string prefabFile = StringStream( PathCleaned( chosen ) ).c_str();
	if( *path_get_extension( prefabFile.c_str() ) == '\0' ){
		prefabFile += ".map";
	}

	Vector3 mins, maxs;
	Select_GetBounds( mins, maxs );
	const Vector3 origin( vector3_mid( mins, maxs ) );
	const Vector3 localShift( vector3_negated( origin ) );

	ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Creating Prefab" );
	const PrefabTextureLockScope textureLockScope( true );
	GlobalSelectionSystem().translateSelected( localShift );
	const bool saved = Map_SaveSelected( prefabFile.c_str() );
	GlobalSelectionSystem().translateSelected( origin );
	if( !saved ){
		globalErrorStream() << "Create Prefab: failed to save '" << prefabFile.c_str() << "'.\n";
		return;
	}
	if( !ensure_worldspawn_entity_in_map_file( prefabFile.c_str() ) ){
		globalErrorStream() << "Create Prefab: failed to finalize prefab map '" << prefabFile.c_str() << "'.\n";
		return;
	}

	const auto command = StringStream( "prefabCreateFromSelection -file ", Quoted( prefabFile.c_str() ) );
	UndoableCommand undo( command );

	Select_Delete();
	scene::Node& prefabNode = Entity_createPoint( "misc_prefab", origin );
	Entity* prefabEntity = Node_getEntity( prefabNode );
	const char* prefabRelative = path_make_relative( prefabFile.c_str(), getMapsPath() );
	if( prefabRelative != prefabFile.c_str() ){
		prefabEntity->setKeyValue( "model", StringStream( "maps/", PathCleaned( prefabRelative ) ) );
	}
	else{
		prefabEntity->setKeyValue( "model", prefabFile.c_str() );
	}
	SceneChangeNotify();
}

void Entity_StampPrefabSelected(){
	scene::Path prefabPath;
	Entity* entity = selected_misc_prefab_entity( &prefabPath );
	if( entity == nullptr ){
		globalErrorStream() << "Prefab Explode: select one misc_prefab entity.\n";
		return;
	}

	const std::string prefabFile = resolve_prefab_path( entity->getKeyValue( "model" ) );
	if( prefabFile.empty() || !file_exists( prefabFile.c_str() ) ){
		globalErrorStream() << "Prefab Explode: missing prefab file '" << entity->getKeyValue( "model" ) << "'.\n";
		return;
	}

	Vector3 origin( g_vector3_identity );
	string_parse_vector3( entity->getKeyValue( "origin" ), origin );
	const Vector3 angles = prefab_read_angles( *entity );
	const Vector3 scale = prefab_read_scale( *entity );

	UndoableCommand undo( "StampPrefabSelected" );
	GlobalSelectionSystem().setSelectedAll( false );
	if( !Map_ImportFile( prefabFile.c_str() ) ){
		globalErrorStream() << "Prefab Explode: failed to import '" << prefabFile.c_str() << "'.\n";
		return;
	}

	const PrefabTextureLockScope textureLockScope( true );
	const bool set[3] = { true, true, true };
	const bool unset[3] = { false, false, false };
	GlobalSelectionSystem().setCustomTransformOrigin( origin, set );
	if( scale != Vector3( 1, 1, 1 ) ){
		GlobalSelectionSystem().scaleSelected( scale, false );
	}
	if( angles != g_vector3_identity ){
		GlobalSelectionSystem().rotateSelected( quaternion_for_matrix4_rotation( matrix4_rotation_for_euler_xyz_degrees( angles ) ), true );
	}
	GlobalSelectionSystem().setCustomTransformOrigin( g_vector3_identity, unset );
	if( origin != g_vector3_identity ){
		GlobalSelectionSystem().translateSelected( origin );
	}
	Path_deleteTop( prefabPath );
	SceneChangeNotify();
}

void Entity_prefabEditSelected(){
	scene::Path prefabPath;
	Entity* entity = selected_misc_prefab_entity( &prefabPath );
	if( entity == nullptr ){
		globalErrorStream() << "Prefab Enter: select one misc_prefab entity.\n";
		return;
	}

	const std::string prefabFile = resolve_prefab_path( entity->getKeyValue( "model" ) );
	if( prefabFile.empty() || !file_exists( prefabFile.c_str() ) ){
		globalErrorStream() << "Prefab Enter: missing prefab file '" << entity->getKeyValue( "model" ) << "'.\n";
		return;
	}

	PrefabEditContext context;
	context.hadPreviousTraversalRoot = SceneGraph_HasTraversalRoot();
	if( context.hadPreviousTraversalRoot ){
		context.previousTraversalRoot = *SceneGraph_GetTraversalRoot();
	}
	context.previousSelection = prefab_capture_selected_paths();
	context.mapWasModifiedBeforeEnter = Map_Modified( g_map );
	context.prefabFile = prefabFile;
	context.hasPrefabCenter = prefab_read_bounds_center( prefabFile.c_str(), context.prefabCenter );
	string_parse_vector3( entity->getKeyValue( "origin" ), context.prefabInstanceOrigin );
	const Vector3 prefabInstanceAngles = prefab_read_angles( *entity );
	const Vector3 prefabInstanceScale = prefab_read_scale( *entity );
	context.prefabResourceName = StringStream( PathCleaned( entity->getKeyValue( "model" ) ) ).c_str();
	if( context.prefabResourceName.empty() ){
		context.prefabResourceName = prefabFile;
	}
	context.enteredPrefabParentPath = prefabPath.parent();
	context.enteredPrefabOriginalNode = NodeSmartReference( prefabPath.top().get() );
	context.enteredPrefabWorkingNode = NodeSmartReference( Node_Clone( prefabPath.top() ) );

	scene::Traversable* parentTraversable = Node_getTraversable( context.enteredPrefabParentPath.top() );
	if( parentTraversable == nullptr ){
		globalErrorStream() << "Prefab Enter: invalid parent traversable.\n";
		return;
	}
	parentTraversable->erase( context.enteredPrefabOriginalNode );
	parentTraversable->insert( context.enteredPrefabWorkingNode );
	if( Entity* workingEntity = Node_getEntity( context.enteredPrefabWorkingNode.get() ); workingEntity != nullptr ){
		/* Prefab edit works in prefab-file local coordinates.
		   Keep wrapper transform neutral while isolated; original node is restored on leave. */
		workingEntity->setKeyValue( "origin", "0 0 0" );
		workingEntity->setKeyValue( "angle", "0" );
		workingEntity->setKeyValue( "angles", "0 0 0" );
		workingEntity->setKeyValue( "modelscale", "1" );
		workingEntity->setKeyValue( "modelscale_vec", "1 1 1" );
	}

	context.enteredPrefabPath = context.enteredPrefabParentPath;
	context.enteredPrefabPath.push( makeReference( context.enteredPrefabWorkingNode.get() ) );
	g_prefabEditStack.push_back( context );
	PrefabEdit_UpdateLeaveCommandEnabled();

	SceneGraph_SetTraversalRoot( context.enteredPrefabPath );
	if( g_pParentWnd != nullptr && g_pParentWnd->GetCamWnd() != nullptr ){
		CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
		context.hasSavedCamera = true;
		context.savedCameraOrigin = Camera_getOrigin( camwnd );
		context.savedCameraAngles = Camera_getAngles( camwnd );

		const Matrix4 worldToLocal = matrix4_affine_inverse(
			prefab_instance_local_to_world( context.prefabInstanceOrigin, prefabInstanceAngles, prefabInstanceScale )
		);

		const Vector3 lookDirWorld = vector3_negated( Camera_getViewVector( camwnd ) );
		const Vector3 cameraLocal = matrix4_transformed_point( worldToLocal, context.savedCameraOrigin );
		const Vector3 lookDirLocal = matrix4_transformed_direction( worldToLocal, lookDirWorld );
		const Vector3 anglesLocal = camera_angles_from_direction( lookDirLocal, context.savedCameraAngles );

		Camera_setOrigin( camwnd, cameraLocal );
		Camera_setAngles( camwnd, anglesLocal );
	}
	g_prefabEditStack.back() = context;
	GlobalSelectionSystem().setSelectedAll( false );
	GlobalSelectionSystem().setSelectedAllComponents( false );
	Map_SetTitleAddon( StringStream( "Prefab: ", prefabFile.c_str() ).c_str() );
	globalOutputStream() << "Prefab Enter: " << prefabFile.c_str() << " (level " << g_prefabEditStack.size() << ", isolated)\n";
}

void Entity_prefabLeaveSelected(){
	if( g_prefabEditStack.empty() ){
		/* Command is blocked outside Prefab Edit mode. */
		return;
	}

	const PrefabEditContext context = g_prefabEditStack.back();

	EMessageBoxReturn saveChoice = eIDNO;
	const bool hasUnsavedChanges = PrefabEdit_HasUnsavedReferenceChanges( context.prefabResourceName, context.prefabFile );
	if( hasUnsavedChanges ){
		// Save currently edited prefab subgraph back to its map file.
		saveChoice = qt_MessageBox(
			MainFrame_getWindow(),
			StringStream( "Save changes to prefab?\n", context.prefabFile.c_str() ),
			"Leave Prefab",
			EMessageBoxType::Question,
			eIDYES | eIDNO | eIDCANCEL
		);
		if( saveChoice == eIDCANCEL ){
			return;
		}
	}
	const bool savePrefab = hasUnsavedChanges && saveChoice == eIDYES;
	if( savePrefab ){
		const bool saved = prefab_save_current_resource( context );
		if( !saved ){
			globalErrorStream() << "Prefab Leave: failed to save '" << context.prefabFile.c_str() << "'.\n";
			return;
		}
	}
	else if( hasUnsavedChanges && saveChoice != eIDNO ){
		return;
	}
	else if( hasUnsavedChanges ){
		PrefabEdit_DiscardReferenceChanges( context.prefabResourceName, context.prefabFile );
	}

	scene::Traversable* parentTraversable = Node_getTraversable( context.enteredPrefabParentPath.top() );
	if( parentTraversable == nullptr ){
		globalErrorStream() << "Prefab Leave: invalid parent traversable.\n";
		return;
	}
	parentTraversable->erase( context.enteredPrefabWorkingNode );
	parentTraversable->insert( context.enteredPrefabOriginalNode );

	g_prefabEditStack.pop_back();
	PrefabEdit_UpdateLeaveCommandEnabled();

	if( context.hadPreviousTraversalRoot ){
		SceneGraph_SetTraversalRoot( context.previousTraversalRoot );
	}
	else{
		SceneGraph_ClearTraversalRoot();
	}
	if( context.hasSavedCamera && g_pParentWnd != nullptr && g_pParentWnd->GetCamWnd() != nullptr ){
		CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
		Camera_setOrigin( camwnd, context.savedCameraOrigin );
		Camera_setAngles( camwnd, context.savedCameraAngles );
	}

	GlobalSelectionSystem().setSelectedAll( false );
	for( const scene::Path& path : context.previousSelection ){
		if( path.top().get_pointer() == context.enteredPrefabOriginalNode.get_pointer() ){
			continue; /* on leave, clear selection of the prefab entity itself */
		}
		if( scene::Instance* instance = GlobalSceneGraph().find( path ); instance != nullptr ){
			Instance_setSelected( *instance, true );
		}
	}

	if( !g_prefabEditStack.empty() ){
		Map_SetTitleAddon( StringStream( "Prefab: ", g_prefabEditStack.back().prefabFile.c_str() ).c_str() );
	}
	else{
		Map_ClearTitleAddon();
	}
	Map_SetModified( g_map, context.mapWasModifiedBeforeEnter );
	RefreshReferences();
	/* Force redraw/update after leaving isolated traversal root. */
	SceneChangeNotify();

	globalOutputStream() << "Prefab Leave (level " << g_prefabEditStack.size() << ", isolated)\n";
}

bool Entity_prefabSaveCurrent(){
	if( g_prefabEditStack.empty() ){
		return false;
	}

	const PrefabEditContext& context = g_prefabEditStack.back();
	const bool saved = prefab_save_current_resource( context );
	if( !saved ){
		globalErrorStream() << "Save Prefab: failed to save '" << context.prefabFile.c_str() << "'.\n";
		return false;
	}

	globalOutputStream() << "Save Prefab: " << context.prefabFile.c_str() << '\n';
	return true;
}


/* scale color so that at least one component is at 1.0F */
void NormalizeColor( Vector3& color ){
	const auto max = vector3_max_component( color );
	if ( max == 0 )
		color = Vector3( 1, 1, 1 );
	else
		color /= max;
}

void Entity_normalizeColor(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();
		Entity* entity = Node_getEntity( path.top() );

		if( entity == 0 && path.size() == 3 ){
			entity = Node_getEntity( path.parent() );
		}

		if ( entity != 0 ) {
			const char* strColor = entity->getKeyValue( "_color" );
			if ( !string_empty( strColor ) ) {
				Vector3 rgb;
				if ( string_parse_vector3( strColor, rgb ) ) {
					g_entity_globals.color_entity = rgb;
					NormalizeColor( g_entity_globals.color_entity );

					char buffer[128];
					sprintf( buffer, "%g %g %g", g_entity_globals.color_entity[0],
					                             g_entity_globals.color_entity[1],
					                             g_entity_globals.color_entity[2] );

					const auto command = StringStream<64>( "entityNormalizeColour ", buffer );
					UndoableCommand undo( command );
					Scene_EntitySetKeyValue_Selected( "_color", buffer );
				}
			}
		}
	}
}

void Entity_setColour(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();
		Entity* entity = Node_getEntity( path.top() );

		if( entity == 0 && path.size() == 3 ){
			entity = Node_getEntity( path.parent() );
		}

		if ( entity != 0 ) {
			const char* strColor = entity->getKeyValue( "_color" );
			if ( !string_empty( strColor ) ) {
				Vector3 rgb;
				if ( string_parse_vector3( strColor, rgb ) ) {
					g_entity_globals.color_entity = rgb;
				}
			}
			if ( color_dialog( MainFrame_getWindow(), g_entity_globals.color_entity ) ) {
				char buffer[128];
				sprintf( buffer, "%g %g %g", g_entity_globals.color_entity[0],
				                             g_entity_globals.color_entity[1],
				                             g_entity_globals.color_entity[2] );

				const auto command = StringStream<64>( "entitySetColour ", buffer );
				UndoableCommand undo( command );
				Scene_EntitySetKeyValue_Selected( "_color", buffer );
			}
		}
	}
}

const char* misc_model_dialog( QWidget* parent, const char* filepath ){
	StringOutputStream buffer( 256 );

	if( !string_empty( filepath ) ){
		const char* root = GlobalFileSystem().findFile( filepath );
		if( !string_empty( root ) && file_is_directory( root ) )
			buffer << root << filepath;
	}
	if( buffer.empty() ){
		buffer << g_qeglobals.m_userGamePath << "models/";

		if ( !file_readable( buffer ) ) {
			// just go to fsmain
			buffer( g_qeglobals.m_userGamePath );
		}
	}

	const char *filename = file_dialog( parent, true, "Choose Model", buffer, ModelLoader::Name );
	if ( filename != 0 ) {
		// use VFS to get the correct relative path
		const char* relative = path_make_relative( filename, GlobalFileSystem().findRoot( filename ) );
		if ( relative == filename ) {
			globalWarningStream() << "WARNING: could not extract the relative path, using full path instead\n";
		}
		return relative;
	}
	return 0;
}

const char* misc_prefab_dialog( QWidget* parent, const char* filepath ){
	static std::string s_prefabPath;
	StringOutputStream buffer( 256 );

	if( !string_empty( filepath ) ){
		const char* root = GlobalFileSystem().findFile( filepath );
		if( !string_empty( root ) && file_is_directory( root ) )
			buffer << root << filepath;
	}
	if( buffer.empty() ){
		if( !Map_Unnamed( g_map ) && path_is_absolute( Map_Name( g_map ) ) ){
			buffer << PathFilenameless( Map_Name( g_map ) );
		}
		else{
			buffer << getMapsPath();
		}
	}

	const char* filename = file_dialog( parent, true, "Choose Prefab Map", buffer, MapFormat::Name, true, false, false );
	if( filename != 0 ) {
		const char* relative = path_make_relative( filename, getMapsPath() );
		if( relative != filename ){
			s_prefabPath = StringStream( "maps/", PathCleaned( relative ) ).c_str();
		}
		else{
			s_prefabPath = StringStream( PathCleaned( filename ) ).c_str();
		}
		return s_prefabPath.c_str();
	}
	return 0;
}

void Entity_reloadDefinitions(){
	if( ConfirmModified( "Reload Entity Definitions" ) ){
		GlobalEntityClassManager().unrealise();
		GlobalEntityClassManager().realise();
	}
}

/*
void LightRadiiImport( EntityCreator& self, bool value ){
	self.setLightRadii( value );
}
typedef ReferenceCaller<EntityCreator, void(bool), LightRadiiImport> LightRadiiImportCaller;

void LightRadiiExport( EntityCreator& self, const BoolImportCallback& importer ){
	importer( self.getLightRadii() );
}
typedef ReferenceCaller<EntityCreator, void(const BoolImportCallback&), LightRadiiExport> LightRadiiExportCaller;

void Entity_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox(
		"Show", "Light Radii",
		LightRadiiImportCaller( GlobalEntityCreator() ),
		LightRadiiExportCaller( GlobalEntityCreator() )
		);
}
*/
void ShowNamesDistImport( EntityCreator& self, int value ){
	self.setShowNamesDist( value );
	UpdateAllWindows();
}
typedef ReferenceCaller<EntityCreator, void(int), ShowNamesDistImport> ShowNamesDistImportCaller;

void ShowNamesDistExport( EntityCreator& self, const IntImportCallback& importer ){
	importer( self.getShowNamesDist() );
}
typedef ReferenceCaller<EntityCreator, void(const IntImportCallback&), ShowNamesDistExport> ShowNamesDistExportCaller;


void ShowNamesRatioImport( EntityCreator& self, int value ){
	self.setShowNamesRatio( value );
	UpdateAllWindows();
}
typedef ReferenceCaller<EntityCreator, void(int), ShowNamesRatioImport> ShowNamesRatioImportCaller;

void ShowNamesRatioExport( EntityCreator& self, const IntImportCallback& importer ){
	importer( self.getShowNamesRatio() );
}
typedef ReferenceCaller<EntityCreator, void(const IntImportCallback&), ShowNamesRatioExport> ShowNamesRatioExportCaller;


void ShowTargetNamesImport( EntityCreator& self, bool value ){
	if( self.getShowTargetNames() != value )
		PreferencesDialog_restartRequired( "Entity Names = Targetnames" ); // technically map reloading or entities recreation do update too, as it's not LatchedValue
	self.setShowTargetNames( value );
}
typedef ReferenceCaller<EntityCreator, void(bool), ShowTargetNamesImport> ShowTargetNamesImportCaller;

void ShowTargetNamesExport( EntityCreator& self, const BoolImportCallback& importer ){
	importer( self.getShowTargetNames() );
}
typedef ReferenceCaller<EntityCreator, void(const BoolImportCallback&), ShowTargetNamesExport> ShowTargetNamesExportCaller;


void Entity_constructPreferences( PreferencesPage& page ){
	page.appendSpinner(	"Names Display Distance (3D)", 0, 200500,
	                    IntImportCallback( ShowNamesDistImportCaller( GlobalEntityCreator() ) ),
	                    IntExportCallback( ShowNamesDistExportCaller( GlobalEntityCreator() ) )
	                  );
	page.appendSpinner(	"Names Display Ratio (2D)", 0, 100500,
	                    IntImportCallback( ShowNamesRatioImportCaller( GlobalEntityCreator() ) ),
	                    IntExportCallback( ShowNamesRatioExportCaller( GlobalEntityCreator() ) )
	                  );
	page.appendCheckBox( "Entity Names", "= Targetnames",
	                     BoolImportCallback( ShowTargetNamesImportCaller( GlobalEntityCreator() ) ),
	                     BoolExportCallback( ShowTargetNamesExportCaller( GlobalEntityCreator() ) ) );
}
void Entity_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Entities", "Entity Display Preferences" ) );
	Entity_constructPreferences( page );
}
void Entity_registerPreferencesPage(){
	PreferencesDialog_addDisplayPage( makeCallbackF( Entity_constructPage ) );
}


void ShowLightRadiiExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getLightRadii() );
}
typedef FreeCaller<void(const BoolImportCallback&), ShowLightRadiiExport> ShowLightRadiiExportCaller;
ShowLightRadiiExportCaller g_show_lightradii_caller;
ToggleItem g_show_lightradii_item( g_show_lightradii_caller );
void ToggleShowLightRadii(){
	GlobalEntityCreator().setLightRadii( !GlobalEntityCreator().getLightRadii() );
	g_show_lightradii_item.update();
	UpdateAllWindows();
}

inline bool game_has_killConnect(){
	return g_pGameDescription->mGameType == "nexuiz"
	    || g_pGameDescription->mGameType == "xonotic"
	    || g_pGameDescription->mGameType == "q1";
}

void Entity_constructMenu( QMenu* menu ){
	create_menu_item_with_mnemonic( menu, "Create Prefab from Selection", "PrefabCreateFromSelection" );
	create_menu_item_with_mnemonic( menu, "&Connect Entities", "EntitiesConnect" );
	if ( game_has_killConnect() ) {
		create_menu_item_with_mnemonic( menu, "&KillConnect Entities", "EntitiesKillConnect" );
	}
	create_menu_item_with_mnemonic( menu, "&Move Primitives to Entity", "EntityMovePrimitivesToLast" );
	create_menu_item_with_mnemonic( menu, "&Select Color...", "EntityColorSet" );
	create_menu_item_with_mnemonic( menu, "&Normalize Color", "EntityColorNormalize" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Reload Entity Definitions", "EntityReloadDefinitions" );
}

void Entity_registerShortcuts(){
	command_connect_accelerator( "EntityMovePrimitivesToFirst" );
	command_connect_accelerator( "EntityUngroup" );
	command_connect_accelerator( "EntityUngroupPrimitives" );
}



#include "preferencesystem.h"
#include "stringio.h"

void Entity_Construct(){
	GlobalCommands_insert( "EntityColorSet", makeCallbackF( Entity_setColour ), QKeySequence( "K" ) );
	GlobalCommands_insert( "EntityColorNormalize", makeCallbackF( Entity_normalizeColor ) );
	GlobalCommands_insert( "EntitiesConnect", makeCallbackF( Entity_connectSelected ), QKeySequence( "Ctrl+K" ) );
	if ( game_has_killConnect() )
		GlobalCommands_insert( "EntitiesKillConnect", makeCallbackF( Entity_killconnectSelected ), QKeySequence( "Shift+K" ) );
	GlobalCommands_insert( "EntityMovePrimitivesToLast", makeCallbackF( Entity_moveSelectedPrimitivesToLast ), QKeySequence( "Ctrl+M" ) );
	GlobalCommands_insert( "EntityMovePrimitivesToFirst", makeCallbackF( Entity_moveSelectedPrimitivesToFirst ) );
	GlobalCommands_insert( "EntityUngroup", makeCallbackF( Entity_ungroup ) );
	GlobalCommands_insert( "EntityUngroupPrimitives", makeCallbackF( Entity_ungroupSelectedPrimitives ) );
	GlobalCommands_insert( "PrefabCreateFromSelection", makeCallbackF( Entity_prefabCreateFromSelection ) );
	GlobalCommands_insert( "PrefabEnter", makeCallbackF( Entity_prefabEditSelected ) );
	GlobalCommands_insert( "PrefabEdit", makeCallbackF( Entity_prefabEditSelected ) );
	GlobalCommands_insert( "PrefabLeave", makeCallbackF( Entity_prefabLeaveSelected ) );
	GlobalCommands_insert( "StampPrefab", makeCallbackF( Entity_StampPrefabSelected ) );
	PrefabEdit_UpdateLeaveCommandEnabled();
	GlobalCommands_insert( "EntityReloadDefinitions", makeCallbackF( Entity_reloadDefinitions ) );

	GlobalToggles_insert( "ShowLightRadiuses", makeCallbackF( ToggleShowLightRadii ), ToggleItem::AddCallbackCaller( g_show_lightradii_item ) );

	GlobalPreferenceSystem().registerPreference( "EntitySelectedColor", Vector3ImportStringCaller( g_entity_globals.color_entity ), Vector3ExportStringCaller( g_entity_globals.color_entity ) );
	GlobalPreferenceSystem().registerPreference( "LastLightIntensity", IntImportStringCaller( g_iLastLightIntensity ), IntExportStringCaller( g_iLastLightIntensity ) );

	Entity_registerPreferencesPage();
}

void Entity_prefabAbortAllEdits(){
	/* Shutdown safety: if app exits while prefab edit mode is active,
	   release all stacked prefab edit references and restore traversal root. */
	bool restoreCamera = false;
	Vector3 restoreCameraOrigin = g_vector3_identity;
	Vector3 restoreCameraAngles = g_vector3_identity;
	while( !g_prefabEditStack.empty() ){
		const PrefabEditContext context = g_prefabEditStack.back();
		g_prefabEditStack.pop_back();
		if( context.hasSavedCamera ){
			restoreCamera = true;
			restoreCameraOrigin = context.savedCameraOrigin;
			restoreCameraAngles = context.savedCameraAngles;
		}

		// On shutdown, drop unsaved in-memory prefab resource edits.
		PrefabEdit_DiscardReferenceChanges( context.prefabResourceName, context.prefabFile );

		if( scene::Traversable* parentTraversable = Node_getTraversable( context.enteredPrefabParentPath.top() ); parentTraversable != nullptr ){
			parentTraversable->erase( context.enteredPrefabWorkingNode );
			parentTraversable->insert( context.enteredPrefabOriginalNode );
		}
	}

	if( SceneGraph_HasTraversalRoot() ){
		SceneGraph_ClearTraversalRoot();
	}
	if( restoreCamera && g_pParentWnd != nullptr && g_pParentWnd->GetCamWnd() != nullptr ){
		CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
		Camera_setOrigin( camwnd, restoreCameraOrigin );
		Camera_setAngles( camwnd, restoreCameraAngles );
	}
	GlobalSelectionSystem().setSelectedAll( false );
	Map_ClearTitleAddon();
}

void Entity_Destroy(){
	Entity_prefabAbortAllEdits();
}

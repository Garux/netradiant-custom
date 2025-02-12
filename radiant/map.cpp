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

#include "map.h"

#include "debugging/debugging.h"

#include "imap.h"
#include "iselection.h"
#include "iundo.h"
#include "ibrush.h"
#include "ifilter.h"
#include "ireference.h"
#include "ifiletypes.h"
#include "ieclass.h"
#include "irender.h"
#include "ientity.h"
#include "editable.h"
#include "ifilesystem.h"
#include "namespace.h"
#include "moduleobserver.h"

#include <set>

#include "scenelib.h"
#include "transformlib.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "traverselib.h"
#include "maplib.h"
#include "eclasslib.h"
#include "commandlib.h"
#include "stream/textfilestream.h"
#include "os/path.h"
#include "uniquenames.h"
#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"
#include "stream/stringstream.h"
#include "signal/signal.h"

#include "gtkutil/filechooser.h"
#include "timer.h"
#include "select.h"
#include "plugin.h"
#include "filetypes.h"
#include "gtkdlgs.h"
#include "entityinspector.h"
#include "points.h"
#include "qe3.h"
#include "camwindow.h"
#include "xywindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "referencecache.h"
#include "mru.h"
#include "commands.h"
#include "autosave.h"
#include "brushmodule.h"
#include "brush.h"
#include "patch.h"
#include "grid.h"

class NameObserver
{
	UniqueNames& m_names;
	CopiedString m_name;

	void construct(){
		if ( !empty() ) {
			//globalOutputStream() << "construct " << makeQuoted( c_str() ) << '\n';
			m_names.insert( name_read( c_str() ) );
		}
	}
	void destroy(){
		if ( !empty() ) {
			//globalOutputStream() << "destroy " << makeQuoted( c_str() ) << '\n';
			m_names.erase( name_read( c_str() ) );
		}
	}
public:
	NameObserver( UniqueNames& names ) : m_names( names ){
		construct();
	}
	NameObserver( const NameObserver& other ) : m_names( other.m_names ), m_name( other.m_name ){
		construct();
	}
	NameObserver& operator=( const NameObserver& other ) = delete; // not assignable
	~NameObserver(){
		destroy();
	}
	bool empty() const {
		return string_empty( c_str() );
	}
	const char* c_str() const {
		return m_name.c_str();
	}
	void nameChanged( const char* name ){
		destroy();
		m_name = name;
		construct();
	}
	typedef MemberCaller<NameObserver, void(const char*), &NameObserver::nameChanged> NameChangedCaller;
};

class BasicNamespace : public Namespace
{
	typedef std::map<NameCallback, NameObserver> Names;
	Names m_names;
	UniqueNames m_uniqueNames;
public:
	~BasicNamespace(){
		ASSERT_MESSAGE( m_names.empty(), "namespace: names still registered at shutdown" );
	}
	void attach( const NameCallback& setName, const NameCallbackCallback& attachObserver ){
		std::pair<Names::iterator, bool> result = m_names.insert( Names::value_type( setName, m_uniqueNames ) );
		ASSERT_MESSAGE( result.second, "cannot attach name" );
		attachObserver( NameObserver::NameChangedCaller( ( *result.first ).second ) );
		//globalOutputStream() << "attach: " << reinterpret_cast<const unsigned int&>( setName ) << '\n';
	}
	void detach( const NameCallback& setName, const NameCallbackCallback& detachObserver ){
		Names::iterator i = m_names.find( setName );
		ASSERT_MESSAGE( i != m_names.end(), "cannot detach name" );
		//globalOutputStream() << "detach: " << reinterpret_cast<const unsigned int&>( setName ) << '\n';
		detachObserver( NameObserver::NameChangedCaller( ( *i ).second ) );
		m_names.erase( i );
	}

	void makeUnique( const char* name, const NameCallback& setName ) const {
		char buffer[1024];
		name_write( buffer, m_uniqueNames.make_unique( name_read( name ) ) );
		setName( buffer );
	}

	void mergeNames( const BasicNamespace& other ) const {
		typedef std::list<NameCallback> SetNameCallbacks;
		typedef std::map<CopiedString, SetNameCallbacks> NameGroups;
		NameGroups groups;

		UniqueNames uniqueNames( other.m_uniqueNames );

		for ( const auto& [callback, observer] : m_names )
		{
			groups[observer.c_str()].push_back( callback );
		}

		for ( const auto& [name, setNameCallbacks] : groups )
		{
			name_t uniqueName( uniqueNames.make_unique( name_read( name.c_str() ) ) );
			uniqueNames.insert( uniqueName );

			char buffer[1024];
			name_write( buffer, uniqueName );

			//globalOutputStream() << "renaming " << makeQuoted( name.c_str() ) << " to " << makeQuoted( buffer ) << '\n';

			for ( const NameCallback& nameCallback : setNameCallbacks )
			{
				nameCallback( buffer );
			}
		}
	}
};

BasicNamespace g_defaultNamespace;
BasicNamespace g_cloneNamespace;

class NamespaceAPI
{
	Namespace* m_namespace;
public:
	typedef Namespace Type;
	STRING_CONSTANT( Name, "*" );

	NamespaceAPI(){
		m_namespace = &g_defaultNamespace;
	}
	Namespace* getTable(){
		return m_namespace;
	}
};

typedef SingletonModule<NamespaceAPI> NamespaceModule;
typedef Static<NamespaceModule> StaticNamespaceModule;
StaticRegisterModule staticRegisterDefaultNamespace( StaticNamespaceModule::instance() );


std::vector<Namespaced*> g_cloned;

inline Namespaced* Node_getNamespaced( scene::Node& node ){
	return NodeTypeCast<Namespaced>::cast( node );
}

void Node_gatherNamespaced( scene::Node& node ){
	Namespaced* namespaced = Node_getNamespaced( node );
	if ( namespaced != 0 ) {
		g_cloned.push_back( namespaced );
	}
}

class GatherNamespaced : public scene::Traversable::Walker
{
public:
	bool pre( scene::Node& node ) const {
		Node_gatherNamespaced( node );
		return true;
	}
};

void Map_gatherNamespaced( scene::Node& root ){
	Node_traverseSubgraph( root, GatherNamespaced() );
}

void Map_mergeClonedNames( bool makeUnique /*= true*/ ){
	if( makeUnique ){
		for ( Namespaced *namespaced : g_cloned )
		{
			namespaced->setNamespace( g_cloneNamespace );
		}
		g_cloneNamespace.mergeNames( g_defaultNamespace );
	}
	for ( Namespaced *namespaced : g_cloned )
	{
		namespaced->setNamespace( g_defaultNamespace );
	}

	g_cloned.clear();
}

class WorldNode
{
	scene::Node* m_node;
public:
	WorldNode()
		: m_node( 0 ){
	}
	void set( scene::Node* node ){
		if ( m_node != 0 ) {
			m_node->DecRef();
		}
		m_node = node;
		if ( m_node != 0 ) {
			m_node->IncRef();
		}
	}
	scene::Node* get() const {
		return m_node;
	}
};

class Map;
void Map_SetValid( Map& map, bool valid );
void Map_UpdateTitle( const Map& map );
void Map_SetWorldspawn( Map& map, scene::Node* node );


class Map : public ModuleObserver
{
public:
	CopiedString m_name;
	Resource* m_resource;
	bool m_valid;

	bool m_modified;
	void ( *m_modified_changed )( const Map& );

	Signal0 m_mapValidCallbacks;

	WorldNode m_world_node;   // "classname" "worldspawn" !

	Map() : m_resource( 0 ), m_valid( false ), m_modified_changed( Map_UpdateTitle ){
	}

	void realise(){
		if ( m_resource != 0 ) {
			if ( Map_Unnamed( *this ) ) {
				g_map.m_resource->setNode( NewMapRoot( "" ).get_pointer() );
				MapFile* map = Node_getMapFile( *g_map.m_resource->getNode() );
				if ( map != 0 ) {
					map->save();
				}
			}
			else
			{
				m_resource->load();
			}

			GlobalSceneGraph().insert_root( *m_resource->getNode() );

			AutoSave_clear();

			Map_SetValid( g_map, true );
		}
	}
	void unrealise(){
		if ( m_resource != 0 ) {
			Map_SetValid( g_map, false );
			Map_SetWorldspawn( g_map, 0 );


			GlobalUndoSystem().clear();

			GlobalSceneGraph().erase_root();
		}
	}
};

Map g_map;
Map* g_currentMap = 0;

void Map_addValidCallback( Map& map, const SignalHandler& handler ){
	map.m_mapValidCallbacks.connectLast( handler );
}

bool Map_Valid( const Map& map ){
	return map.m_valid;
}

void Map_SetValid( Map& map, bool valid ){
	map.m_valid = valid;
	map.m_mapValidCallbacks();
}


const char* Map_Name( const Map& map ){
	return map.m_name.c_str();
}

bool Map_Unnamed( const Map& map ){
	return string_equal( Map_Name( map ), "unnamed.map" );
}

inline const MapFormat& MapFormat_forFile( const char* filename ){
	const char* moduleName = findModuleName( GetFileTypeRegistry(), MapFormat::Name, path_get_extension( filename ) );
	MapFormat* format = Radiant_getMapModules().findModule( moduleName );
	ASSERT_MESSAGE( format != 0, "map format not found for file " << makeQuoted( filename ) );
	return *format;
}

const MapFormat& Map_getFormat( const Map& map ){
	return MapFormat_forFile( Map_Name( map ) );
}


bool Map_Modified( const Map& map ){
	return map.m_modified;
}

void Map_SetModified( Map& map, bool modified ){
	if ( map.m_modified ^ modified ) {
		map.m_modified = modified;

		map.m_modified_changed( map );
	}
}

void Map_UpdateTitle( const Map& map ){
	Sys_SetTitle( map.m_name.c_str(), Map_Modified( map ) );
}



scene::Node* Map_GetWorldspawn( const Map& map ){
	return map.m_world_node.get();
}

void Map_SetWorldspawn( Map& map, scene::Node* node ){
	map.m_world_node.set( node );
}


/*
   ================
   Map_Free
   free all map elements, reinitialize the structures that depend on them
   ================
 */
#include "modelwindow.h"
void Map_Free(){
	Map_RegionOff();
	Select_ShowAllHidden();

	Pointfile_Clear();

	g_map.m_resource->detach( g_map );
	GlobalReferenceCache().release( g_map.m_name.c_str() );
	g_map.m_resource = 0;

	ModelBrowser_flushReferences();

	FlushReferences();

	g_currentMap = 0;
	Brush_unlatchPreferences();
}

class EntityFindByClassname : public scene::Graph::Walker
{
	const char* m_name;
	Entity*& m_entity;
public:
	EntityFindByClassname( const char* name, Entity*& entity ) : m_name( name ), m_entity( entity ){
		m_entity = 0;
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( m_entity == 0 ) {
			Entity* entity = Node_getEntity( path.top() );
			if ( entity != 0
			  && string_equal( m_name, entity->getClassName() ) ) {
				m_entity = entity;
			}
		}
		return true;
	}
};

Entity* Scene_FindEntityByClass( const char* name ){
	Entity* entity;
	GlobalSceneGraph().traverse( EntityFindByClassname( name, entity ) );
	return entity;
}

Entity *Scene_FindPlayerStart(){
	typedef const char* StaticString;
	StaticString strings[] = {
		"info_player_start",
		"info_player_deathmatch",
		"team_CTF_redplayer",
		"team_CTF_blueplayer",
		"team_CTF_redspawn",
		"team_CTF_bluespawn",
	};
	typedef const StaticString* StaticStringIterator;
	for ( StaticStringIterator i = strings, end = strings + ( sizeof( strings ) / sizeof( StaticString ) ); i != end; ++i )
	{
		Entity* entity = Scene_FindEntityByClass( *i );
		if ( entity != 0 ) {
			return entity;
		}
	}
	return 0;
}

//
// move the view to a start position
//


void FocusViews( const Vector3& point, float angle ){
	CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
	Camera_setOrigin( camwnd, point );
	Vector3 angles( Camera_getAngles( camwnd ) );
	angles[CAMERA_PITCH] = 0;
	angles[CAMERA_YAW] = angle;
	Camera_setAngles( camwnd, angles );

	g_pParentWnd->forEachXYWnd( [&point]( XYWnd* xywnd ){
		xywnd->SetOrigin( point );
	} );
}

#include "stringio.h"

void Map_StartPosition(){
	Entity* entity = Scene_FindPlayerStart();

	Vector3 origin;
	if ( entity != nullptr && string_parse_vector3( entity->getKeyValue( "origin" ), origin ) ) {
		FocusViews( origin, string_read_float( entity->getKeyValue( "angle" ) ) );
	}
	else
	{
		FocusViews( g_vector3_identity, 0 );
	}
}


inline bool node_is_worldspawn( scene::Node& node ){
	Entity* entity = Node_getEntity( node );
	return entity != 0 && string_equal( entity->getClassName(), "worldspawn" );
}


// use first worldspawn
class entity_updateworldspawn : public scene::Traversable::Walker
{
public:
	bool pre( scene::Node& node ) const {
		if ( node_is_worldspawn( node ) ) {
			if ( Map_GetWorldspawn( g_map ) == 0 ) {
				Map_SetWorldspawn( g_map, &node );
			}
		}
		return false;
	}
};

scene::Node* Map_FindWorldspawn( Map& map ){
	Map_SetWorldspawn( map, 0 );

	Node_getTraversable( GlobalSceneGraph().root() )->traverse( entity_updateworldspawn() );

	return Map_GetWorldspawn( map );
}


class CollectAllWalker : public scene::Traversable::Walker
{
	scene::Node& m_root;
	UnsortedNodeSet& m_nodes;
public:
	CollectAllWalker( scene::Node& root, UnsortedNodeSet& nodes ) : m_root( root ), m_nodes( nodes ){
	}
	bool pre( scene::Node& node ) const {
		m_nodes.insert( NodeSmartReference( node ) );
		Node_getTraversable( m_root )->erase( node );
		return false;
	}
};

void Node_insertChildFirst( scene::Node& parent, scene::Node& child ){
	UnsortedNodeSet nodes;
	Node_getTraversable( parent )->traverse( CollectAllWalker( parent, nodes ) );
	Node_getTraversable( parent )->insert( child );

	for ( UnsortedNodeSet::iterator i = nodes.begin(); i != nodes.end(); ++i )
	{
		Node_getTraversable( parent )->insert( ( *i ) );
	}
}

scene::Node& createWorldspawn(){
	NodeSmartReference worldspawn( GlobalEntityCreator().createEntity( GlobalEntityClassManager().findOrInsert( "worldspawn", true ) ) );
	Node_insertChildFirst( GlobalSceneGraph().root(), worldspawn );
	return worldspawn;
}

void Map_UpdateWorldspawn( Map& map ){
	if ( Map_FindWorldspawn( map ) == 0 ) {
		Map_SetWorldspawn( map, &createWorldspawn() );
	}
}

scene::Node& Map_FindOrInsertWorldspawn( Map& map ){
	Map_UpdateWorldspawn( map );
	return *Map_GetWorldspawn( map );
}


class MapMergeAll : public scene::Traversable::Walker
{
	mutable scene::Path m_path;
public:
	MapMergeAll( const scene::Path& root )
		: m_path( root ){
	}
	bool pre( scene::Node& node ) const {
		Node_getTraversable( m_path.top() )->insert( node );
		m_path.push( makeReference( node ) );
		selectPath( m_path, true );
		return false;
	}
	void post( scene::Node& node ) const {
		m_path.pop();
	}
};

class MapMergeEntities : public scene::Traversable::Walker
{
	mutable scene::Path m_path;
public:
	MapMergeEntities( const scene::Path& root )
		: m_path( root ){
	}
	bool pre( scene::Node& node ) const {
		if ( node_is_worldspawn( node ) ) {
			scene::Node* world_node = Map_FindWorldspawn( g_map );
			if ( world_node == 0 ) {
				Map_SetWorldspawn( g_map, &node );
				Node_getTraversable( m_path.top().get() )->insert( node );
				m_path.push( makeReference( node ) );
				Node_getTraversable( node )->traverse( SelectChildren( m_path ) );
			}
			else
			{
				m_path.push( makeReference( *world_node ) );
				Node_getTraversable( node )->traverse( MapMergeAll( m_path ) );
			}
		}
		else
		{
			Node_getTraversable( m_path.top() )->insert( node );
			m_path.push( makeReference( node ) );
			if ( node_is_group( node ) ) {
				Node_getTraversable( node )->traverse( SelectChildren( m_path ) );
			}
			else
			{
				selectPath( m_path, true );
			}
		}
		return false;
	}
	void post( scene::Node& node ) const {
		m_path.pop();
	}
};

class BasicContainer : public scene::Node::Symbiot
{
	class TypeCasts
	{
		NodeTypeCastTable m_casts;
	public:
		TypeCasts(){
			NodeContainedCast<BasicContainer, scene::Traversable>::install( m_casts );
		}
		NodeTypeCastTable& get(){
			return m_casts;
		}
	};

	scene::Node m_node;
	TraversableNodeSet m_traverse;
public:

	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	scene::Traversable& get( NullType<scene::Traversable>){
		return m_traverse;
	}

	BasicContainer() : m_node( this, this, StaticTypeCasts::instance().get() ){
	}
	void release(){
		delete this;
	}
	scene::Node& node(){
		return m_node;
	}
};

/// Merges the map graph rooted at \p node into the global scene-graph.
void MergeMap( scene::Node& node ){
	Node_getTraversable( node )->traverse( MapMergeEntities( scene::Path( makeReference( GlobalSceneGraph().root() ) ) ) );
}


class Convert_Faces {
	const TexdefTypeId _in, _out;
public:
	Convert_Faces( TexdefTypeId in, TexdefTypeId out )
		: _in( in ), _out( out ) {
	}
	void operator()( Face& face ) const {
		face.Convert( _in, _out );
	}
};
#include "brushnode.h"
class Convert_Brushes : public scene::Traversable::Walker {
	const Convert_Faces _convert_faces;
public:
	Convert_Brushes( TexdefTypeId in, TexdefTypeId out )
		: _convert_faces( in, out ) {
	}
	bool pre( scene::Node& node ) const {
		if( node.isRoot() ) {
			return false;
		}
		Brush* brush = Node_getBrush( node );
		if( brush ) {
			Brush_forEachFace( *brush, _convert_faces );
		}
		return true;
	}
	void post( scene::Node& node ) const {
	}
};


void Map_ImportSelected( TextInputStream& in, const MapFormat& format ){
	NodeSmartReference node( ( new BasicContainer )->node() );
	const EBrushType brush_type = GlobalBrushCreator().getFormat();
	format.readGraph( node, in, GlobalEntityCreator() );
	if ( brush_type != GlobalBrushCreator().getFormat() ) {
		Node_getTraversable( node )->traverse( Convert_Brushes( BrushType_getTexdefType( GlobalBrushCreator().getFormat() ), BrushType_getTexdefType( brush_type ) ) );
		GlobalBrushCreator().toggleFormat( brush_type );
	}
	Map_gatherNamespaced( node );
	Map_mergeClonedNames();
	MergeMap( node );
}

inline scene::Cloneable* Node_getCloneable( scene::Node& node ){
	return NodeTypeCast<scene::Cloneable>::cast( node );
}

inline scene::Node& node_clone( scene::Node& node ){
	scene::Cloneable* cloneable = Node_getCloneable( node );
	if ( cloneable != 0 ) {
		return cloneable->clone();
	}

	return ( new scene::NullNode )->node();
}

class CloneAll : public scene::Traversable::Walker
{
	mutable scene::Path m_path;
public:
	CloneAll( scene::Node& root )
		: m_path( makeReference( root ) ){
	}
	bool pre( scene::Node& node ) const {
		if ( node.isRoot() ) {
			return false;
		}

		m_path.push( makeReference( node_clone( node ) ) );
		m_path.top().get().IncRef();

		return true;
	}
	void post( scene::Node& node ) const {
		if ( node.isRoot() ) {
			return;
		}

		Node_getTraversable( m_path.parent() )->insert( m_path.top() );

		m_path.top().get().DecRef();
		m_path.pop();
	}
};

scene::Node& Node_Clone( scene::Node& node ){
	scene::Node& clone = node_clone( node );
	scene::Traversable* traversable = Node_getTraversable( node );
	if ( traversable != 0 ) {
		traversable->traverse( CloneAll( clone ) );
	}
	return clone;
}

bool Node_instanceSelected( scene::Node& node );

class CloneAllSelected : public scene::Traversable::Walker
{
	mutable scene::Path m_path;
public:
	CloneAllSelected( scene::Node& root )
		: m_path( makeReference( root ) ){
	}
	bool pre( scene::Node& node ) const {
		if ( node.isRoot() ) {
			return false;
		}

		if( Node_instanceSelected( node ) ){
			m_path.push( makeReference( node_clone( node ) ) );
			m_path.top().get().IncRef();
		}

		return true;
	}
	void post( scene::Node& node ) const {
		if ( node.isRoot() ) {
			return;
		}

		if( Node_instanceSelected( node ) ){
			Node_getTraversable( m_path.parent() )->insert( m_path.top() );

			m_path.top().get().DecRef();
			m_path.pop();
		}
	}
};

scene::Node& Node_Clone_Selected( scene::Node& node ){
	scene::Node& clone = node_clone( node );
	scene::Traversable* traversable = Node_getTraversable( node );
	if ( traversable != 0 ) {
		traversable->traverse( CloneAllSelected( clone ) );
	}
	return clone;
}


typedef std::map<CopiedString, std::size_t> EntityBreakdown;

class EntityBreakdownWalker : public scene::Graph::Walker
{
	EntityBreakdown& m_entitymap;
public:
	EntityBreakdownWalker( EntityBreakdown& entitymap )
		: m_entitymap( entitymap ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0 ) {
			++m_entitymap[entity->getClassName()];
		}
		return true;
	}
};

void Scene_EntityBreakdown( EntityBreakdown& entitymap ){
	GlobalSceneGraph().traverse( EntityBreakdownWalker( entitymap ) );
}

class CountStuffWalker : public scene::Graph::Walker
{
	int& m_ents_ingame;
	int& m_groupents;
	int& m_groupents_ingame;
public:
	CountStuffWalker( int& ents_ingame, int& groupents, int& groupents_ingame )
		: m_ents_ingame( ents_ingame ), m_groupents( groupents ), m_groupents_ingame( groupents_ingame ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0 ){
			const char* classname = entity->getClassName();
			if( entity->isContainer() ){
				++m_groupents;
				if( !string_equal_nocase( "func_group", classname ) &&
				    !string_equal_nocase( "_decal", classname ) &&
				    !string_equal_nocase_n( "func_detail", classname, 11 ) ){
					++m_groupents_ingame;
					++m_ents_ingame;
				}
				return true;
			}
			if( !string_equal_nocase_n( "light", classname, 5 ) &&
			    !string_equal_nocase( "misc_model", classname ) ){
				++m_ents_ingame;
			}
		}
		return true;
	}
};

void Scene_CountStuff( int& ents_ingame, int& groupents, int& groupents_ingame ){
	GlobalSceneGraph().traverse( CountStuffWalker( ents_ingame, groupents, groupents_ingame ) );
}

#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>

void DoMapInfo(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Map Info" );

	auto w_brushes = new QLabel;
	auto w_patches = new QLabel;
	auto w_ents = new QLabel;
	auto w_ents_ingame = new QLabel;
	auto w_groupents = new QLabel;
	auto w_groupents_ingame = new QLabel;

	auto tree = new QTreeWidget;
	tree->setColumnCount( 2 );
	tree->setSortingEnabled( true );
	tree->sortByColumn( 0, Qt::SortOrder::AscendingOrder );
	tree->setUniformRowHeights( true ); // optimization
	tree->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
	tree->setSizeAdjustPolicy( QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents ); // scroll area will inherit column size
	tree->header()->setStretchLastSection( false ); // non greedy column sizing
	tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents ); // no text elision
	tree->setRootIsDecorated( false );
	tree->setHeaderLabels( { "Entity", "Count" } );

	{
		auto grid = new QGridLayout( &dialog );

		grid->addWidget( new QLabel( "Total Brushes:" ), 0, 0 );
		grid->addWidget( w_brushes, 0, 1 );
		grid->addWidget( new QLabel( "Total Patches:" ), 1, 0 );
		grid->addWidget( w_patches, 1, 1 );
		grid->addWidget( new QLabel( "Total Entities:" ), 2, 0 );
		grid->addWidget( w_ents, 2, 1 );
		grid->addWidget( new QLabel( "Ingame Entities:" ), 0, 2 );
		grid->addWidget( w_ents_ingame, 0, 3 );
		grid->addWidget( new QLabel( "Group Entities:" ), 1, 2 );
		grid->addWidget( w_groupents, 1, 3 );
		grid->addWidget( new QLabel( "Ingame Group Entities:" ), 2, 2 );
		grid->addWidget( w_groupents_ingame, 2, 3 );

		grid->addWidget( new QLabel( "*** Entity breakdown ***" ), 3, 0, 1, 4, Qt::AlignmentFlag::AlignCenter );

		grid->addWidget( tree, 4, 0, 1, 4 );
	}

	// Initialize fields

	{
		EntityBreakdown entitymap;
		Scene_EntityBreakdown( entitymap );

		for ( const auto&[name, count] : entitymap )
		{
			auto item = new QTreeWidgetItem( tree );
			item->setData( 0, Qt::ItemDataRole::DisplayRole, name.c_str() );
			item->setData( 1, Qt::ItemDataRole::DisplayRole, int( count ) );
		}
	}

	int n_ents_ingame = 0;
	int n_groupents = 0;
	int n_groupents_ingame = 0;
	Scene_CountStuff( n_ents_ingame, n_groupents, n_groupents_ingame );

	StringOutputStream str( 32 );
	w_brushes->setText( str( "<b><i>", g_brushCount.get(), "</b></i>" ).c_str() );
	w_patches->setText( str( "<b><i>", g_patchCount.get(), "</b></i>" ).c_str() );
	w_ents->setText( str( "<b><i>", g_entityCount.get(), "</b></i>" ).c_str() );
	w_ents_ingame->setText( str( "<b><i>", n_ents_ingame, "</b></i>" ).c_str() );
	w_groupents->setText( str( "<b><i>", n_groupents, "</b></i>" ).c_str() );
	w_groupents_ingame->setText( str( "<b><i>", n_groupents_ingame, "</b></i>" ).c_str() );

	dialog.exec();
}



class ScopeTimer
{
	Timer m_timer;
	const char* m_message;
public:
	ScopeTimer( const char* message )
		: m_message( message ){
	}
	~ScopeTimer(){
		globalOutputStream() << m_message << " timer: " << FloatFormat( m_timer.elapsed_sec(), 5, 2 ) << " second(s) elapsed\n";
	}
};

/*
   ================
   Map_LoadFile
   ================
 */

void Map_LoadFile( const char *filename ){
	globalOutputStream() << "Loading map from " << filename << '\n';
	ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Map" );

	{
		ScopeTimer timer( "map load" );
		g_map.m_name = filename;
		Map_UpdateTitle( g_map );
		g_map.m_resource = GlobalReferenceCache().capture( g_map.m_name.c_str() );
		g_map.m_resource->attach( g_map );
		Node_getTraversable( GlobalSceneGraph().root() )->traverse( entity_updateworldspawn() );
	}

	globalOutputStream() << "--- LoadMapFile ---\n";
	globalOutputStream() << g_map.m_name << '\n';

	globalOutputStream() << g_brushCount.get() + g_patchCount.get() << " primitives\n";
	globalOutputStream() << g_entityCount.get() << " entities\n";

	//GlobalEntityCreator().printStatistics();

	/* move the view to a start position */
	Map_StartPosition();

	g_currentMap = &g_map;

	GridStatus_changed();
}

class Excluder
{
public:
	virtual bool excluded( scene::Node& node ) const = 0;
};

class ExcludeWalker : public scene::Traversable::Walker
{
	const scene::Traversable::Walker& m_walker;
	const Excluder* m_exclude;
	mutable bool m_skip;
public:
	ExcludeWalker( const scene::Traversable::Walker& walker, const Excluder& exclude )
		: m_walker( walker ), m_exclude( &exclude ), m_skip( false ){
	}
	bool pre( scene::Node& node ) const {
		if ( m_exclude->excluded( node ) || node.isRoot() ) {
			m_skip = true;
			return false;
		}
		else
		{
			m_walker.pre( node );
		}
		return true;
	}
	void post( scene::Node& node ) const {
		if ( m_skip ) {
			m_skip = false;
		}
		else
		{
			m_walker.post( node );
		}
	}
};

class AnyInstanceSelected : public scene::Instantiable::Visitor
{
	bool& m_selected;
public:
	AnyInstanceSelected( bool& selected ) : m_selected( selected ){
		m_selected = false;
	}
	void visit( scene::Instance& instance ) const {
		if ( Instance_isSelected( instance ) ) {
			m_selected = true;
		}
	}
};

bool Node_instanceSelected( scene::Node& node ){
	scene::Instantiable* instantiable = Node_getInstantiable( node );
	ASSERT_NOTNULL( instantiable );
	bool selected;
	instantiable->forEachInstance( AnyInstanceSelected( selected ) );
	return selected;
}

class SelectedDescendantWalker : public scene::Traversable::Walker
{
	bool& m_selected;
public:
	SelectedDescendantWalker( bool& selected ) : m_selected( selected ){
		m_selected = false;
	}

	bool pre( scene::Node& node ) const {
		if ( node.isRoot() ) {
			return false;
		}

		if ( Node_instanceSelected( node ) ) {
			m_selected = true;
		}

		return true;
	}
};

bool Node_selectedDescendant( scene::Node& node ){
	bool selected;
	Node_traverseSubgraph( node, SelectedDescendantWalker( selected ) );
	return selected;
}

class SelectionExcluder : public Excluder
{
public:
	bool excluded( scene::Node& node ) const {
		return !Node_selectedDescendant( node );
	}
};

class IncludeSelectedWalker : public scene::Traversable::Walker
{
	const scene::Traversable::Walker& m_walker;
	mutable std::size_t m_selected;
	mutable bool m_skip;

	bool selectedParent() const {
		return m_selected != 0;
	}
public:
	IncludeSelectedWalker( const scene::Traversable::Walker& walker )
		: m_walker( walker ), m_selected( 0 ), m_skip( false ){
	}
	bool pre( scene::Node& node ) const {
		// include node if:
		// node is not a 'root' AND ( node is selected OR any child of node is selected OR any parent of node is selected )
		if ( !node.isRoot() && ( Node_selectedDescendant( node ) || selectedParent() ) ) {
			if ( Node_instanceSelected( node ) ) {
				++m_selected;
			}
			m_walker.pre( node );
			return true;
		}
		else
		{
			m_skip = true;
			return false;
		}
	}
	void post( scene::Node& node ) const {
		if ( m_skip ) {
			m_skip = false;
		}
		else
		{
			if ( Node_instanceSelected( node ) ) {
				--m_selected;
			}
			m_walker.post( node );
		}
	}
};

void Map_Traverse_Selected( scene::Node& root, const scene::Traversable::Walker& walker ){
	scene::Traversable* traversable = Node_getTraversable( root );
	if ( traversable != 0 ) {
#if 0
		traversable->traverse( ExcludeWalker( walker, SelectionExcluder() ) );
#else
		traversable->traverse( IncludeSelectedWalker( walker ) );
#endif
	}
}

void Map_ExportSelected( TextOutputStream& out, const MapFormat& format ){
	format.writeGraph( GlobalSceneGraph().root(), Map_Traverse_Selected, out );
}

void Map_Traverse( scene::Node& root, const scene::Traversable::Walker& walker ){
	scene::Traversable* traversable = Node_getTraversable( root );
	if ( traversable != 0 ) {
		traversable->traverse( walker );
	}
}

class RegionExcluder : public Excluder
{
public:
	bool excluded( scene::Node& node ) const {
		return node.excluded();
	}
};

void Map_Traverse_Region( scene::Node& root, const scene::Traversable::Walker& walker ){
	scene::Traversable* traversable = Node_getTraversable( root );
	if ( traversable != 0 ) {
		traversable->traverse( ExcludeWalker( walker, RegionExcluder() ) );
	}
}


void Map_RenameAbsolute( const char* absolute ){
	Resource* resource = GlobalReferenceCache().capture( absolute );
	NodeSmartReference clone( NewMapRoot( path_make_relative( absolute, GlobalFileSystem().findRoot( absolute ) ) ) );
	resource->setNode( clone.get_pointer() );

	{
		//ScopeTimer timer( "clone subgraph" );
		Node_getTraversable( GlobalSceneGraph().root() )->traverse( CloneAll( clone ) );
	}

	g_map.m_resource->detach( g_map );
	g_map.m_resource->flush(); /* wipe map from cache to not spoil namespace */
	GlobalReferenceCache().release( g_map.m_name.c_str() );

	Map_gatherNamespaced( clone );
	Map_mergeClonedNames( false ); // set default namespace

	g_map.m_resource = resource;

	g_map.m_name = absolute;
	Map_UpdateTitle( g_map );

	g_map.m_resource->attach( g_map );
}

void Map_Rename( const char* filename ){
	if ( !string_equal( g_map.m_name.c_str(), filename ) ) {
		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Saving Map" );

		Map_RenameAbsolute( filename );

		SceneChangeNotify();
	}
	else
	{
		SaveReferences();
	}
}

bool Map_Save(){
	Pointfile_Clear();

	ScopeTimer timer( "map save" );
	SaveReferences();
	return true; // assume success..
}

/*
   ===========
   Map_New

   ===========
 */
void Map_New(){
	//globalOutputStream() << "Map_New\n";

	g_map.m_name = "unnamed.map";
	Map_UpdateTitle( g_map );

	{
		g_map.m_resource = GlobalReferenceCache().capture( g_map.m_name.c_str() );
//		ASSERT_MESSAGE( g_map.m_resource->getNode() == 0, "bleh" );
		g_map.m_resource->attach( g_map );

		SceneChangeNotify();
	}

	FocusViews( g_vector3_identity, 0 );

	g_currentMap = &g_map;

	GridStatus_changed();
}

/*
   ===========================================================

   REGION

   ===========================================================
 */
bool g_region_active = false;

ToggleItem g_region_item{ BoolExportCaller( g_region_active ) };

Vector3 g_region_mins;
Vector3 g_region_maxs;
void Region_defaultMinMax(){
	if( !g_region_active ){ // don't invalidate region bounds, while in region mode
		g_region_maxs = Vector3( GetMaxGridCoord() );
		g_region_mins = -g_region_maxs;
	}
}

/*
   ===========
   AddRegionBrushes
   a regioned map will have temp walls put up at the region boundary
   \todo TODO TTimo old implementation of region brushes
   we still add them straight in the worldspawn and take them out after the map is saved
   with the new implementation we should be able to append them in a temporary manner to the data we pass to the map module
   ===========
 */
extern void ConstructRegionBrushes( scene::Node * brushes[6], const Vector3 &region_mins, const Vector3 &region_maxs );

class ScopeRegionBrushes
{
	scene::Node* m_brushes[6];
	scene::Node* m_startpoint;

	void ConstructRegionStartpoint( const Vector3& vOrig ){
		// write the info_playerstart
		char sTmp[1024];
		sprintf( sTmp, "%d %d %d", (int)vOrig[0], (int)vOrig[1], (int)vOrig[2] );
		Node_getEntity( *m_startpoint )->setKeyValue( "origin", sTmp );
		sprintf( sTmp, "%d", (int)Camera_getAngles( *g_pParentWnd->GetCamWnd() )[CAMERA_YAW] );
		Node_getEntity( *m_startpoint )->setKeyValue( "angle", sTmp );
	}
public:
	ScopeRegionBrushes(){
		for ( auto&& brush : m_brushes )
		{
			brush = &GlobalBrushCreator().createBrush();
			Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->insert( NodeSmartReference( *brush ) );
		}

		m_startpoint = &GlobalEntityCreator().createEntity( GlobalEntityClassManager().findOrInsert( "info_player_start", false ) );

		/* adjust temp box: space may be too small, also help with lights and flat primitives */
		const Vector3 min( g_region_mins - Vector3( 256, 256, 8 ) ), max( g_region_maxs + Vector3( 256, 256, 512 ) );
		Vector3 spawn( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ) );
		/* pull spawn point to the box, if needed */
		for( size_t i = 0; i < 3; ++i )
		{
			spawn[i] = std::max( spawn[i], min[i] + 64 );
			spawn[i] = std::min( spawn[i], max[i] - 64 );
		}

		ConstructRegionBrushes( m_brushes, min, max );
		ConstructRegionStartpoint( spawn );

		Node_getTraversable( GlobalSceneGraph().root() )->insert( NodeSmartReference( *m_startpoint ) );
	}
	~ScopeRegionBrushes(){
		for ( auto&& brush : m_brushes )
		{
			Node_getTraversable( *Map_GetWorldspawn( g_map ) )->erase( *brush );
		}
		Node_getTraversable( GlobalSceneGraph().root() )->erase( *m_startpoint );
	}
	ScopeRegionBrushes( ScopeRegionBrushes&& ) noexcept = delete;
};

bool Map_SaveRegion( const char *filename ){
	ScopeRegionBrushes tmp;
	return MapResource_saveFile( MapFormat_forFile( filename ), GlobalSceneGraph().root(), Map_Traverse_Region, filename );
}


inline void exclude_node( scene::Node& node, bool exclude ){
	exclude
	? node.enable( scene::Node::eExcluded )
	: node.disable( scene::Node::eExcluded );
}

class ExcludeAllWalker : public scene::Graph::Walker
{
	bool m_exclude;
public:
	ExcludeAllWalker( bool exclude )
		: m_exclude( exclude ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		exclude_node( path.top(), m_exclude );

		return true;
	}
};

void Scene_Exclude_All( bool exclude ){
	GlobalSceneGraph().traverse( ExcludeAllWalker( exclude ) );
}

class ExcludeSelectedWalker : public scene::Graph::Walker
{
	bool m_exclude;
public:
	ExcludeSelectedWalker( bool exclude )
		: m_exclude( exclude ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if( !path.top().get().isRoot() ) /* don't touch model node: disabling one will disable all instances! */
			exclude_node( path.top(), ( instance.isSelected() || instance.childSelected() || instance.parentSelected() ) == m_exclude );
		return true;
	}
};

void Scene_Exclude_Selected( bool exclude ){
	GlobalSceneGraph().traverse( ExcludeSelectedWalker( exclude ) );
}

class ExcludeRegionedWalker : public scene::Graph::Walker
{
	const bool m_exclude;
	const AABB m_region = aabb_for_minmax( g_region_mins, g_region_maxs );
public:
	ExcludeRegionedWalker( bool exclude )
		: m_exclude( exclude ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if( !path.top().get().isRoot() ){ /* don't touch model node: disabling one will disable all its instances! */
			const bool exclude = m_exclude == aabb_intersects_aabb( instance.worldAABB(), m_region );
			exclude_node( path.top(), exclude );
			if( exclude )
				Instance_setSelected( instance, false );
		}
		return true;
	}
};

void Scene_Exclude_Region( bool exclude ){
	GlobalSceneGraph().traverse( ExcludeRegionedWalker( exclude ) );
}

/*
   ===========
   Map_RegionOff

   Other filtering options may still be on
   ===========
 */
void Map_RegionOff(){
	g_region_active = false;
	g_region_item.update();

	Region_defaultMinMax();

	Scene_Exclude_All( false );
}

void Map_ApplyRegion(){
	if( GlobalSelectionSystem().countSelectedComponents() != 0 )
		GlobalSelectionSystem().setSelectedAllComponents( false );

	g_region_active = true;
	g_region_item.update();

	Scene_Exclude_Region( false );
	/* newly created brushes have to be visible! */
	if( scene::Node* w = Map_FindWorldspawn( g_map ) )
		exclude_node( *w, false );
}


/*
   ========================
   Map_RegionSelectedBrushes
   ========================
 */
void Map_RegionSelectedBrushes(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		if( GlobalSelectionSystem().countSelectedComponents() != 0 )
			GlobalSelectionSystem().setSelectedAllComponents( false );

		g_region_active = true;
		g_region_item.update();
		Select_GetBounds( g_region_mins, g_region_maxs );

		Scene_Exclude_Selected( false );
		/* newly created brushes have to be visible! */
		if( scene::Node* w = Map_FindWorldspawn( g_map ) )
			exclude_node( *w, false );

		GlobalSelectionSystem().setSelectedAll( false );
	}
	else{
		Map_RegionOff();
	}
}


/*
   ===========
   Map_RegionXY
   ===========
 */
void Map_RegionXY( const Vector3& min, const Vector3& max ){
	for( std::size_t i = 0; i < 3; ++i ){
		g_region_mins[i] = std::max( g_region_mins[i], min[i] );
		g_region_maxs[i] = std::min( g_region_maxs[i], max[i] );
	}
	Map_ApplyRegion();
}

void Map_RegionBounds( const AABB& bounds ){
	Map_RegionOff();

	g_region_mins = vector3_subtracted( bounds.origin, bounds.extents );
	g_region_maxs = vector3_added( bounds.origin, bounds.extents );

	Map_ApplyRegion();
}

/*
   ===========
   Map_RegionBrush
   ===========
 */
void Map_RegionBrush(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		scene::Instance& instance = GlobalSelectionSystem().ultimateSelected();
		Map_RegionBounds( instance.worldAABB() );

		if( GlobalSelectionSystem().countSelected() != 1 ){
			GlobalSelectionSystem().setSelectedAll( false );
			Instance_setSelected( instance, true );
		}
		deleteSelection();
	}
	else{
		globalErrorStream() << "Nothing is selected!\n";
	}
}

//
//================
//Map_ImportFile
//================
//
bool Map_ImportFile( const char* filename ){
	ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Map" );

	bool success = false;

	if ( path_extension_is( filename, "bsp" ) ) {
		goto tryDecompile;
	}

	{
		const EBrushType brush_type = GlobalBrushCreator().getFormat();

		Resource* resource = GlobalReferenceCache().capture( filename );
		resource->refresh(); // avoid loading old version if map has changed on disk since last import
		if ( !resource->load() ) {
			GlobalReferenceCache().release( filename );
			if ( brush_type != GlobalBrushCreator().getFormat() ) {
				GlobalBrushCreator().toggleFormat( brush_type );
			}
			goto tryDecompile;
		}
		if ( brush_type != GlobalBrushCreator().getFormat() ) {
			Node_getTraversable( *resource->getNode() )->traverse( Convert_Brushes( BrushType_getTexdefType( GlobalBrushCreator().getFormat() ), BrushType_getTexdefType( brush_type ) ) );
			GlobalBrushCreator().toggleFormat( brush_type );
		}
		NodeSmartReference clone( NewMapRoot( "" ) );
		Node_getTraversable( *resource->getNode() )->traverse( CloneAll( clone ) );
		resource->flush(); /* wipe map from cache to not spoil namespace */
		GlobalReferenceCache().release( filename );
		Map_gatherNamespaced( clone );
		Map_mergeClonedNames();
		MergeMap( clone );
		success = true;
	}

	SceneChangeNotify();

	return success;

tryDecompile:

	const char *type = GlobalRadiant().getGameDescriptionKeyValue( "q3map2_type" );
	if ( path_extension_is( filename, "bsp" ) || path_extension_is( filename, "map" ) ) {
		StringOutputStream str( 256 );
		str << AppPath_get() << "q3map2." << RADIANT_EXECUTABLE
		    << " -v -game " << ( ( type && *type ) ? type : "quake3" )
		    << " -fs_basepath " << makeQuoted( EnginePath_get() )
		    << " -fs_homepath " << makeQuoted( g_qeglobals.m_userEnginePath )
		    << " -fs_game " << gamename_get()
		    << " -convert -format " << ( BrushType_getTexdefType( GlobalBrushCreator().getFormat() ) == TEXDEFTYPEID_QUAKE ? "map" : "map_bp" );
		if ( path_extension_is( filename, "map" ) ) {
			str << " -readmap ";
		}
		str << ' ' << makeQuoted( filename );

		// run
		Q_Exec( NULL, str.c_str(), NULL, false, true );

		// rebuild filename as "filenamewithoutext_converted.map"
		str( PathExtensionless( filename ), "_converted.map" );
		filename = str.c_str();

		const EBrushType brush_type = GlobalBrushCreator().getFormat();
		// open
		Resource* resource = GlobalReferenceCache().capture( filename );
		resource->refresh(); // avoid loading old version if map has changed on disk since last import
		if ( !resource->load() ) {
			GlobalReferenceCache().release( filename );
			if ( brush_type != GlobalBrushCreator().getFormat() ) {
				GlobalBrushCreator().toggleFormat( brush_type );
			}
			return success;
		}
		if ( brush_type != GlobalBrushCreator().getFormat() ) {
			Node_getTraversable( *resource->getNode() )->traverse( Convert_Brushes( BrushType_getTexdefType( GlobalBrushCreator().getFormat() ), BrushType_getTexdefType( brush_type ) ) );
			GlobalBrushCreator().toggleFormat( brush_type );
		}
		NodeSmartReference clone( NewMapRoot( "" ) );
		Node_getTraversable( *resource->getNode() )->traverse( CloneAll( clone ) );
		resource->flush(); /* wipe map from cache to not spoil namespace */
		GlobalReferenceCache().release( filename );
		Map_gatherNamespaced( clone );
		Map_mergeClonedNames();
		MergeMap( clone );
		success = true;
	}

	SceneChangeNotify();
	return success;
}

/*
   ===========
   Map_SaveFile
   ===========
 */
bool Map_SaveFile( const char* filename ){
	ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Saving Map" );
	return MapResource_saveFile( MapFormat_forFile( filename ), GlobalSceneGraph().root(), Map_Traverse, filename );
}

//
//===========
//Map_SaveSelected
//===========
//
// Saves selected world brushes and whole entities with partial/full selections
//
bool Map_SaveSelected( const char* filename ){
	return MapResource_saveFile( MapFormat_forFile( filename ), GlobalSceneGraph().root(), Map_Traverse_Selected, filename );
}

class ParentSelectedBrushesToEntityWalker : public scene::Graph::Walker
{
	scene::Node& m_parent;
	scene::Node* m_world = Map_FindWorldspawn( g_map );
	mutable bool m_emptyOldParent = false;
public:
	ParentSelectedBrushesToEntityWalker( scene::Node& parent ) : m_parent( parent ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		return path.top().get_pointer() != &m_parent; /* skip traverse of target node */
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		if ( Node_isPrimitive( path.top() ) ){
			if ( Instance_isSelected( instance ) ){
				NodeSmartReference node( path.top().get() );
				scene::Traversable* parent_traversable = Node_getTraversable( path.parent() );
				parent_traversable->erase( node );
				Node_getTraversable( m_parent )->insert( node );
				m_emptyOldParent = parent_traversable->empty();
			}
		}
		else if ( m_emptyOldParent ){
			m_emptyOldParent = false;
			if ( path.top().get_pointer() != m_world ) /* delete empty entity left */
				Path_deleteTop( path );
		}
	}
};

void Scene_parentSelectedBrushesToEntity( scene::Graph& graph, scene::Node& parent ){
	graph.traverse( ParentSelectedBrushesToEntityWalker( parent ) );
}

void Scene_parentSubgraphSelectedBrushesToEntity( scene::Graph& graph, scene::Node& parent, const scene::Path& start ){
	graph.traverse_subgraph( ParentSelectedBrushesToEntityWalker( parent ), start );
}

class CountSelectedBrushes : public scene::Graph::Walker
{
	std::size_t& m_count;
	mutable std::size_t m_depth;
public:
	CountSelectedBrushes( std::size_t& count ) : m_count( count ), m_depth( 0 ){
		m_count = 0;
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( ++m_depth != 1 && path.top().get().isRoot() ) {
			return false;
		}
		if ( Instance_isSelected( instance )
		  && Node_isPrimitive( path.top() ) ) {
			++m_count;
		}
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		--m_depth;
	}
};

std::size_t Scene_countSelectedBrushes( scene::Graph& graph ){
	std::size_t count;
	graph.traverse( CountSelectedBrushes( count ) );
	return count;
}
#if 0
enum ENodeType
{
	eNodeUnknown,
	eNodeMap,
	eNodeEntity,
	eNodePrimitive,
};

const char* nodetype_get_name( ENodeType type ){
	if ( type == eNodeMap ) {
		return "map";
	}
	if ( type == eNodeEntity ) {
		return "entity";
	}
	if ( type == eNodePrimitive ) {
		return "primitive";
	}
	return "unknown";
}

ENodeType node_get_nodetype( scene::Node& node ){
	if ( Node_isEntity( node ) ) {
		return eNodeEntity;
	}
	if ( Node_isPrimitive( node ) ) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

bool contains_entity( scene::Node& node ){
	return Node_getTraversable( node ) != 0 && !Node_isBrush( node ) && !Node_isPatch( node ) && !Node_isEntity( node );
}

bool contains_primitive( scene::Node& node ){
	return Node_isEntity( node ) && Node_getTraversable( node ) != 0 && Node_getEntity( node )->isContainer();
}

ENodeType node_get_contains( scene::Node& node ){
	if ( contains_entity( node ) ) {
		return eNodeEntity;
	}
	if ( contains_primitive( node ) ) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

void Path_parent( const scene::Path& parent, const scene::Path& child ){
	ENodeType contains = node_get_contains( parent.top() );
	ENodeType type = node_get_nodetype( child.top() );

	if ( contains != eNodeUnknown && contains == type ) {
		NodeSmartReference node( child.top().get() );
		Path_deleteTop( child );
		Node_getTraversable( parent.top() )->insert( node );
		SceneChangeNotify();
	}
	else
	{
		globalErrorStream() << "failed - " << nodetype_get_name( type ) << " cannot be parented to " << nodetype_get_name( contains ) << " container.\n";
	}
}

void Scene_parentSelected(){
	UndoableCommand undo( "parentSelected" );

	if ( GlobalSelectionSystem().countSelected() > 1 ) {
		class ParentSelectedBrushesToEntityWalker : public SelectionSystem::Visitor
		{
			const scene::Path& m_parent;
		public:
			ParentSelectedBrushesToEntityWalker( const scene::Path& parent ) : m_parent( parent ){
			}
			void visit( scene::Instance& instance ) const {
				if ( &m_parent != &instance.path() ) {
					Path_parent( m_parent, instance.path() );
				}
			}
		};

		ParentSelectedBrushesToEntityWalker visitor( GlobalSelectionSystem().ultimateSelected().path() );
		GlobalSelectionSystem().foreachSelected( visitor );
	}
	else
	{
		globalWarningStream() << "failed - did not find two selected nodes.\n";
	}
}
#endif


void NewMap(){
	if ( ConfirmModified( "New Map" ) ) {
		Map_Free();
		Map_New();
	}
}

CopiedString g_mapsPath;

const char* getMapsPath(){
	return g_mapsPath.c_str();
}

const char* map_open( const char* title ){
	const char* path = Map_Unnamed( g_map )? getMapsPath() : g_map.m_name.c_str();
	return file_dialog( MainFrame_getWindow(), true, title, path, MapFormat::Name, true, false, false );
}

const char* map_import( const char* title ){
	const char* path = Map_Unnamed( g_map )? getMapsPath() : g_map.m_name.c_str();
	return file_dialog( MainFrame_getWindow(), true, title, path, MapFormat::Name, false, true, false );
}

const char* map_save( const char* title ){
	const char* path = Map_Unnamed( g_map )? getMapsPath() : g_map.m_name.c_str();
	return file_dialog( MainFrame_getWindow(), false, title, path, MapFormat::Name, false, false, true );
}

void OpenMap(){
	if ( !ConfirmModified( "Open Map" ) ) {
		return;
	}

	const char* filename = map_open( "Open Map" );

	if ( filename != 0 ) {
		MRU_AddFile( filename );
		Map_Free();
		Map_LoadFile( filename );
	}
}

void ImportMap(){
	const char* filename = map_import( "Import Map" );

	if ( filename != 0 ) {
		UndoableCommand undo( "mapImport" );
		Map_ImportFile( filename );
	}
}

bool Map_SaveAs(){
	const char* filename = map_save( "Save Map" );

	if ( filename != 0 ) {
		MRU_AddFile( filename );
		Map_Rename( filename );
		return Map_Save();
	}
	return false;
}

void SaveMapAs(){
	Map_SaveAs();
}

void SaveMap(){
	if ( Map_Unnamed( g_map ) ) {
		SaveMapAs();
	}
	else if ( Map_Modified( g_map ) ) {
		Map_Save();
		MRU_AddFile( g_map.m_name.c_str() );	//add on saving, but not opening via cmd line: spoils the list
	}
}

void ExportMap(){
	const char* filename = map_save( "Export Selection" );

	if ( filename != 0 ) {
		Map_SaveSelected( filename );
	}
}

void SaveRegion(){
	const char* filename = map_save( "Export Region" );

	if ( filename != 0 ) {
		Map_SaveRegion( filename );
	}
}


void RegionOff(){
	Map_RegionOff();
	SceneChangeNotify();
}

void RegionXY(){
	const int nDim = GlobalXYWnd_getCurrentViewType();
	NDIM1NDIM2( nDim );
	const XYWnd& wnd = *( g_pParentWnd->ActiveXY() );
	Vector3 min, max;
	min[nDim1] = wnd.GetOrigin()[nDim1] - 0.5f * wnd.Width() / wnd.Scale();
	min[nDim2] = wnd.GetOrigin()[nDim2] - 0.5f * wnd.Height() / wnd.Scale();
	min[nDim] = g_MinWorldCoord + 64;
	max[nDim1] = wnd.GetOrigin()[nDim1] + 0.5f * wnd.Width() / wnd.Scale();
	max[nDim2] = wnd.GetOrigin()[nDim2] + 0.5f * wnd.Height() / wnd.Scale();
	max[nDim] = g_MaxWorldCoord - 64;

	Map_RegionXY( min, max );
	SceneChangeNotify();
}

void RegionBrush(){
	Map_RegionBrush();
	SceneChangeNotify();
}

void RegionSelected(){
	Map_RegionSelectedBrushes();
	SceneChangeNotify();
}





class BrushFindByIndexWalker : public scene::Traversable::Walker
{
	mutable std::size_t m_index;
	scene::Path& m_path;
public:
	BrushFindByIndexWalker( std::size_t index, scene::Path& path )
		: m_index( index ), m_path( path ){
	}
	bool pre( scene::Node& node ) const {
		if ( Node_isPrimitive( node ) && m_index-- == 0 ) {
			m_path.push( makeReference( node ) );
		}
		return false;
	}
};

class EntityFindByIndexWalker : public scene::Traversable::Walker
{
	mutable std::size_t m_index;
	scene::Path& m_path;
public:
	EntityFindByIndexWalker( std::size_t index, scene::Path& path )
		: m_index( index ), m_path( path ){
	}
	bool pre( scene::Node& node ) const {
		if ( Node_isEntity( node ) && m_index-- == 0 ) {
			m_path.push( makeReference( node ) );
		}
		return false;
	}
};

void Scene_FindEntityBrush( std::size_t entity, std::size_t brush, scene::Path& path ){
	path.push( makeReference( GlobalSceneGraph().root() ) );

	Node_getTraversable( path.top() )->traverse( EntityFindByIndexWalker( entity, path ) );

	if ( path.size() == 2 ) {
		scene::Traversable* traversable = Node_getTraversable( path.top() );
		if ( traversable != 0 ) {
			traversable->traverse( BrushFindByIndexWalker( brush, path ) );
		}
	}
}

inline bool Node_hasChildren( scene::Node& node ){
	scene::Traversable* traversable = Node_getTraversable( node );
	return traversable != 0 && !traversable->empty();
}

void SelectBrush( int entitynum, int brushnum ){
	scene::Path path;
	Scene_FindEntityBrush( entitynum, brushnum, path );
	if ( path.size() == 3 || ( path.size() == 2 && !Node_hasChildren( path.top() ) ) ) {
		scene::Instance* instance = GlobalSceneGraph().find( path );
		ASSERT_MESSAGE( instance != 0, "SelectBrush: path not found in scenegraph" );
		Selectable* selectable = Instance_getSelectable( *instance );
		ASSERT_MESSAGE( selectable != 0, "SelectBrush: path not selectable" );
		selectable->setSelected( true );
		g_pParentWnd->forEachXYWnd( [instance]( XYWnd* xywnd ){
			xywnd->SetOrigin( instance->worldAABB().origin );
		} );
	}
}


class BrushFindIndexWalker : public scene::Traversable::Walker
{
	mutable const scene::Node* m_node;
	std::size_t& m_count;
public:
	BrushFindIndexWalker( const scene::Node& node, std::size_t& count )
		: m_node( &node ), m_count( count ){
	}
	bool pre( scene::Node& node ) const {
		if ( Node_isPrimitive( node ) ) {
			if ( m_node == &node ) {
				m_node = 0;
			}
			if ( m_node ) {
				++m_count;
			}
		}
		return true;
	}
};

class EntityFindIndexWalker : public scene::Traversable::Walker
{
	mutable const scene::Node* m_node;
	std::size_t& m_count;
public:
	EntityFindIndexWalker( const scene::Node& node, std::size_t& count )
		: m_node( &node ), m_count( count ){
	}
	bool pre( scene::Node& node ) const {
		if ( Node_isEntity( node ) ) {
			if ( m_node == &node ) {
				m_node = 0;
			}
			if ( m_node ) {
				++m_count;
			}
		}
		return true;
	}
};

static void GetSelectionIndex( int *ent, int *brush ){
	std::size_t count_brush = 0;
	std::size_t count_entity = 0;
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();

		{
			scene::Traversable* traversable = Node_getTraversable( path.parent() );
			if ( traversable != 0 && path.size() == 3 ) {
				traversable->traverse( BrushFindIndexWalker( path.top(), count_brush ) );
			}
		}

		{
			scene::Traversable* traversable = Node_getTraversable( GlobalSceneGraph().root() );
			if ( traversable != 0 ) {
				if( path.size() == 3 ){
					traversable->traverse( EntityFindIndexWalker( path.parent(), count_entity ) );
				}
				else if ( path.size() == 2 ){
					traversable->traverse( EntityFindIndexWalker( path.top(), count_entity ) );
				}
			}
		}
	}
	*brush = int(count_brush);
	*ent = int(count_entity);
}

#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include "gtkutil/spinbox.h"

void DoFind(){
	QDialog dialog( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
	dialog.setWindowTitle( "Find Brush" );

	auto entity = new SpinBox( 0, 999999 );
	entity->setButtonSymbols( QAbstractSpinBox::ButtonSymbols::NoButtons );
	auto brush = new SpinBox( 0, 999999 );
	brush->setButtonSymbols( QAbstractSpinBox::ButtonSymbols::NoButtons );
	{
		auto form = new QFormLayout( &dialog );
		form->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );

		form->addRow( new SpinBoxLabel( "Entity number", entity ), entity );
		form->addRow( new SpinBoxLabel( "Brush number", brush ), brush );
		{
			auto buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Close );
			buttons->addButton( "Find", QDialogButtonBox::ButtonRole::AcceptRole );
			form->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
			QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
		}
	}

	// Initialize dialog
	int ent, br;

	GetSelectionIndex( &ent, &br );
	entity->setValue( ent );
	brush->setValue( br );

	if ( dialog.exec() ) {
		SelectBrush( entity->value(), brush->value() );
	}
}


#include "filterbar.h"
////
void map_autocaulk_selected(){
	if ( Map_Unnamed( g_map ) ) {
		if( !Map_SaveAs() )
			return;
	}

	{
		class DeselectTriggers : public scene::Graph::Walker
		{
			mutable const scene::Instance* m_trigger = 0;
		public:
			bool pre( const scene::Path& path, scene::Instance& instance ) const {
				if( path.size() == 2 ){
					Entity* entity = Node_getEntity( path.top() );
					if( entity != 0 && entity->isContainer() && string_equal_nocase_n( entity->getClassName(), "trigger_", 8 )
					 && ( instance.childSelected() || instance.isSelected() ) )
						m_trigger = &instance;
					else
						return false;
				}
				return true;
			}
			void post( const scene::Path& path, scene::Instance& instance ) const {
				if( m_trigger )
					Instance_setSelected( instance, false );
				if( m_trigger == &instance )
					m_trigger = 0;
			}
		};
		GlobalSceneGraph().traverse( DeselectTriggers() );
	}

	if( GlobalSelectionSystem().countSelected() == 0 ){
		globalErrorStream() << "map_autocaulk_selected(): nothing is selected\n";
		return;
	}

	ScopeDisableScreenUpdates disableScreenUpdates( "processing", "autocaulk" );

	auto filename = StringStream( PathExtensionless( g_map.m_name.c_str() ), "_ac.map" );

	{	// write .map
		const Vector3 spawn( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ) );
		Vector3 mins, maxs;
		Select_GetBounds( mins, maxs );
		mins -= Vector3( 1024 );
		maxs += Vector3( 1024 );

		if( !aabb_intersects_point( aabb_for_minmax( mins, maxs ), spawn ) ){
			globalErrorStream() << "map_autocaulk_selected(): camera must be near selection!\n";
			return;
		}

		TextFileOutputStream file( filename );
		if ( file.failed() ) {
			globalErrorStream() << "writing " << filename << " failure\n";
			return;
		}

		// all brushes to the worldspawn
		file << "{\n"
		        "\"classname\" \"worldspawn\"";
		TokenWriter& writer = GlobalScripLibModule::getTable().m_pfnNewSimpleTokenWriter( file );
		class WriteBrushesWalker : public scene::Traversable::Walker
		{
			TokenWriter& m_writer;
		public:
			WriteBrushesWalker( TokenWriter& writer )
				: m_writer( writer ){
			}
			bool pre( scene::Node& node ) const {
				if( Node_getBrush( node ) ){
					NodeTypeCast<MapExporter>::cast( node )->exportTokens( m_writer );
				}
				return true;
			}
		};
		Map_Traverse_Selected( GlobalSceneGraph().root(), WriteBrushesWalker( writer ) );
		// plus box
		scene::Node* box[6];
		for ( std::size_t i = 0; i < 6; ++i ){
			box[i] = &GlobalBrushCreator().createBrush();
			box[i]->IncRef();
		}
		ConstructRegionBrushes( box, mins, maxs );
		for ( std::size_t i = 0; i < 6; ++i ){
			NodeTypeCast<MapExporter>::cast( *box[i] )->exportTokens( writer );
			box[i]->DecRef();
		}
		// close world
		file << "\n}\n";
		// spawn
		file << "{\n"
		        "\"classname\" \"info_player_start\"\n"
		        "\"origin\" \"" << spawn[0] << ' ' << spawn[1] << ' ' << spawn[2] << "\"\n"
		        "}\n";
		// point entities
		const MapFormat& format = MapFormat_forFile( filename );
		auto traverse_selected_point_entities = []( scene::Node& root, const scene::Traversable::Walker& walker ){
			scene::Traversable* traversable = Node_getTraversable( root );
			if ( traversable != 0 ) {
				class selected_point_entities_walker : public scene::Traversable::Walker
				{
					const scene::Traversable::Walker& m_walker;
					mutable bool m_skip;
				public:
					selected_point_entities_walker( const scene::Traversable::Walker& walker )
						: m_walker( walker ), m_skip( false ){
					}
					bool pre( scene::Node& node ) const {
						Entity* entity = Node_getEntity( node );
						if( !node.isRoot() && entity != 0 && !entity->isContainer() && Node_instanceSelected( node ) ) {
							m_walker.pre( node );
						}
						else{
							m_skip = true;
						}
						return false;
					}
					void post( scene::Node& node ) const {
						if( m_skip )
							m_skip = false;
						else
							m_walker.post( node );
					}
				};
				traversable->traverse( selected_point_entities_walker( walker ) );
			}
		};
		format.writeGraph( GlobalSceneGraph().root(), traverse_selected_point_entities, file );

		writer.release();
	}

	{	// compile
		StringOutputStream str( 256 );
		str << AppPath_get() << "q3map2." << RADIANT_EXECUTABLE
		    << " -game quake3"
		    << " -fs_basepath " << makeQuoted( EnginePath_get() )
		    << " -fs_homepath " << makeQuoted( g_qeglobals.m_userEnginePath )
		    << " -fs_game " << gamename_get()
		    << " -autocaulk -fulldetail "
		    << makeQuoted( filename );
		// run
		Q_Exec( NULL, str.c_str(), NULL, false, true );
	}

	typedef std::map<std::size_t, CopiedString> CaulkMap;
	CaulkMap map;
	{	// load
		filename( PathExtensionless( g_map.m_name.c_str() ), "_ac.caulk" );

		TextFileInputStream file( filename );
		if( file.failed() ){
			globalErrorStream() << "reading " << filename << " failure\n";
			return;
		}

		Tokeniser& tokeniser = GlobalScripLibModule::getTable().m_pfnNewSimpleTokeniser( file );
		while( 1 ){
			const char* num = tokeniser.getToken();
			if( !num )
				break;
			std::size_t n;
			string_parse_size( num, n );

			const char* faces = tokeniser.getToken();
			if( !faces )
				break;
			tokeniser.nextLine();

			map.emplace( n, faces );
		}
		tokeniser.release();
	}

	{	// apply
		class CaulkBrushesWalker : public scene::Traversable::Walker
		{
			const CaulkMap& m_map;
			mutable std::size_t m_brushIndex = 0;
			const CopiedString m_caulk = GetCommonShader( "caulk" );
			const CopiedString m_watercaulk = GetCommonShader( "watercaulk" );
			const CopiedString m_lavacaulk = GetCommonShader( "lavacaulk" );
			const CopiedString m_slimecaulk = GetCommonShader( "slimecaulk" );
			const CopiedString m_nodraw = GetCommonShader( "nodraw" );
			const CopiedString m_nodrawnonsolid = GetCommonShader( "nodrawnonsolid" );
		public:
			mutable std::size_t m_caulkedCount = 0;
			CaulkBrushesWalker( CaulkMap& map )
				: m_map( map ){
			}
			bool pre( scene::Node& node ) const {
				Brush* brush = Node_getBrush( node );
				if( brush ){
					CaulkMap::const_iterator iter = m_map.find( m_brushIndex );
					if( iter != m_map.end() ){
						const char* faces = ( *iter ).second.c_str();
						for ( Brush::const_iterator f = brush->begin(); f != brush->end() && *faces; ++f ){
							Face& face = *( *f );
							if ( face.contributes() ){
								++m_caulkedCount;
								switch ( *faces )
								{
								case 'c':
									face.SetShader( m_caulk.c_str() );
									break;
								case 'w':
									face.SetShader( m_watercaulk.c_str() );
									break;
								case 'l':
									face.SetShader( m_lavacaulk.c_str() );
									break;
								case 's':
									face.SetShader( m_slimecaulk.c_str() );
									break;
								case 'N':
									face.SetShader( m_nodraw.c_str() );
									break;
								case 'n':
									face.SetShader( m_nodrawnonsolid.c_str() );
									break;

								default:
									--m_caulkedCount;
									break;
								}

								++faces;
							}
						}
					}
					++m_brushIndex;
				}
				return true;
			}
		};
		CaulkBrushesWalker caulkBrushesWalker( map );
		GlobalUndoSystem().start();
		Map_Traverse_Selected( GlobalSceneGraph().root(), caulkBrushesWalker );
		const auto str = StringStream<32>( "AutoCaulk ", caulkBrushesWalker.m_caulkedCount, " faces" );
		GlobalUndoSystem().finish( str );
	}
}




void Map_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Load last map at startup", g_bLoadLastMap );
}


class MapEntityClasses : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	MapEntityClasses() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			if ( g_map.m_resource != 0 ) {
				ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Map" );
				g_map.m_resource->realise();
			}
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			if ( g_map.m_resource != 0 ) {
				g_map.m_resource->flush();
				g_map.m_resource->unrealise();
			}
		}
	}
};

MapEntityClasses g_MapEntityClasses;


class MapModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	MapModuleObserver() : m_unrealised( 1 ){
	}
	void realise(){
		if ( --m_unrealised == 0 ) {
			ASSERT_MESSAGE( !g_qeglobals.m_userGamePath.empty(), "maps_directory: user-game-path is empty" );
			g_mapsPath = StringStream( g_qeglobals.m_userGamePath, "maps/" );
			Q_mkdir( g_mapsPath.c_str() );
		}
	}
	void unrealise(){
		if ( ++m_unrealised == 1 ) {
			g_mapsPath = "";
		}
	}
};

MapModuleObserver g_MapModuleObserver;

#include "preferencesystem.h"

CopiedString g_strLastMap;
bool g_bLoadLastMap = true;

void Map_Construct(){
	GlobalCommands_insert( "NewMap", makeCallbackF( NewMap ) );
	GlobalCommands_insert( "OpenMap", makeCallbackF( OpenMap ), QKeySequence( "Ctrl+O" ) );
	GlobalCommands_insert( "ImportMap", makeCallbackF( ImportMap ) );
	GlobalCommands_insert( "SaveMap", makeCallbackF( SaveMap ), QKeySequence( "Ctrl+S" ) );
	GlobalCommands_insert( "SaveMapAs", makeCallbackF( SaveMapAs ) );
	GlobalCommands_insert( "SaveSelected", makeCallbackF( ExportMap ) );
	GlobalCommands_insert( "SaveRegion", makeCallbackF( SaveRegion ) );

	GlobalCommands_insert( "RegionOff", makeCallbackF( RegionOff ) );
	GlobalCommands_insert( "RegionSetXY", makeCallbackF( RegionXY ) );
	GlobalCommands_insert( "RegionSetBrush", makeCallbackF( RegionBrush ) );
	//GlobalCommands_insert( "RegionSetSelection", makeCallbackF( RegionSelected ), QKeySequence( "Ctrl+Shift+R" ) );
	GlobalToggles_insert( "RegionSetSelection", makeCallbackF( RegionSelected ), ToggleItem::AddCallbackCaller( g_region_item ), QKeySequence( "Ctrl+Shift+R" ) );
	GlobalCommands_insert( "AutoCaulkSelected", makeCallbackF( map_autocaulk_selected ), QKeySequence( "F4" ) );

	GlobalCommands_insert( "FindBrush", makeCallbackF( DoFind ) );
	GlobalCommands_insert( "MapInfo", makeCallbackF( DoMapInfo ), QKeySequence( "M" ) );

	GlobalPreferenceSystem().registerPreference( "LastMap", CopiedStringImportStringCaller( g_strLastMap ), CopiedStringExportStringCaller( g_strLastMap ) );
	GlobalPreferenceSystem().registerPreference( "LoadLastMap", BoolImportStringCaller( g_bLoadLastMap ), BoolExportStringCaller( g_bLoadLastMap ) );

	PreferencesDialog_addSettingsPreferences( makeCallbackF( Map_constructPreferences ) );

	GlobalEntityClassManager().attach( g_MapEntityClasses );
	Radiant_attachHomePathsObserver( g_MapModuleObserver );
}

void Map_Destroy(){
	Radiant_detachHomePathsObserver( g_MapModuleObserver );
	GlobalEntityClassManager().detach( g_MapEntityClasses );
}

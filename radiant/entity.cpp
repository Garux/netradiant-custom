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

#include "ientity.h"
#include "iselection.h"
#include "imodel.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "editable.h"

#include "eclasslib.h"
#include "scenelib.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "stringio.h"

#include "gtkutil/filechooser.h"
#include "gtkmisc.h"
#include "select.h"
#include "map.h"
#include "preferences.h"
#include "gtkdlgs.h"
#include "mainframe.h"
#include "qe3.h"
#include "commands.h"

#include "brushmanip.h"
#include "patchmanip.h"
#include "filterbar.h"


struct entity_globals_t
{
	Vector3 color_entity;

	entity_globals_t() :
		color_entity( 0.0f, 0.0f, 0.0f ){
	}
};

entity_globals_t g_entity_globals;

class EntitySetKeyValueSelected : public scene::Graph::Walker
{
const char* m_key;
const char* m_value;
public:
EntitySetKeyValueSelected( const char* key, const char* value )
	: m_key( key ), m_value( value ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
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
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	Entity* entity = Node_getEntity( path.top() );
	if ( entity != 0 && ( instance.childSelected() || Instance_isSelected( instance ) ) ) {
		if( path.top().get_pointer() == m_world ){ /* do not want to convert whole worldspawn entity */
			if( instance.childSelected() && !m_2world ){ /* create an entity from world brushes instead */
				EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( m_classname, true );
				if( entityClass->fixedsize )
					return;

				//is important to have retexturing here; if doing in the end, undo doesn't succeed; //don't do this extra now, as it requires retexturing, working for subgraph
//				if ( string_compare_nocase_n( m_classname, "trigger_", 8 ) == 0 ){
//					Scene_PatchSetShader_Selected( GlobalSceneGraph(), GetCommonShader( "trigger" ).c_str() );
//					Scene_BrushSetShader_Selected( GlobalSceneGraph(), GetCommonShader( "trigger" ).c_str() );
//				}

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

		EntityClass* eclass = GlobalEntityClassManager().findOrInsert( m_classname, node_is_group( path.top() ) );
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
		StringOutputStream command;
		if( string_equal( classname, "worldspawn" ) )
			command << "ungroupSelectedEntities";
		else
			command << "entitySetClass -class " << classname;
		UndoableCommand undo( command.c_str() );
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
					//deleteme.push(DeletionPair(parentparent, parent));
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
		StringOutputStream command;
		command << "movePrimitivesToEntity " << makeQuoted( Node_getEntity( node )->getEntityClass().name() );
		UndoableCommand undo( command.c_str() );
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


int g_iLastLightIntensity;

void Entity_createFromSelection( const char* name, const Vector3& origin ){
#if 0
	if ( string_equal_nocase( name, "worldspawn" ) ) {
		gtk_MessageBox( GTK_WIDGET( MainFrame_getWindow() ), "Can't create an entity with worldspawn.", "info" );
		return;
	}
#else
	const scene::Node* world_node = Map_FindWorldspawn( g_map );
	if ( world_node && string_equal( name, "worldspawn" ) ) {
//		GlobalRadiant().m_pfnMessageBox( GTK_WIDGET( MainFrame_getWindow() ), "There's already a worldspawn in your map!", "Info", eMB_OK, eMB_ICONDEFAULT );
		UndoableCommand undo( "ungroupSelectedPrimitives" );
		Scene_parentSelectedBrushesToEntity( GlobalSceneGraph(), Map_FindOrInsertWorldspawn( g_map ) ); //=no action, if no worldspawn (but one inserted) (since insertion deselects everything)
		//Scene_parentSelectedBrushesToEntity( GlobalSceneGraph(), *Map_FindWorldspawn( g_map ) ); = crash, if no worldspawn
		return;
	}
#endif

	StringOutputStream command;
	command << "entityCreate -class " << name;
	UndoableCommand undo( command.c_str() );

	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( name, true );

	const bool isModel = entityClass->miscmodel_is
				   || ( GlobalSelectionSystem().countSelected() == 0 && classname_equal( name, "func_static" ) && g_pGameDescription->mGameType == "doom3" );

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

	Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

	scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
	entitypath.push( makeReference( node.get() ) );
	scene::Instance& instance = findInstance( entitypath );

	if ( entityClass->fixedsize || ( isModel && !brushesSelected ) ) {
		//Select_Delete();

		Transformable* transform = Instance_getTransformable( instance );
		if ( transform != 0 ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setTranslation( origin );
			transform->freezeTransform();
		}

		GlobalSelectionSystem().setSelectedAll( false );

		Instance_setSelected( instance, true );
	}
	else
	{
		if ( g_pGameDescription->mGameType == "doom3" ) {
			Node_getEntity( node )->setKeyValue( "model", Node_getEntity( node )->getKeyValue( "name" ) );
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

			if ( DoLightIntensityDlg( &intensity ) == eIDOK ) {
				g_iLastLightIntensity = intensity;
				char buf[30];
				sprintf( buf, "255 255 255 %d", intensity );
				Node_getEntity( node )->setKeyValue( "_light", buf );
			}
		}
	}
	else if ( string_equal_nocase( name, "light" ) ) {
		if ( g_pGameDescription->mGameType != "doom3" ) {
			int intensity = g_iLastLightIntensity;

			if ( DoLightIntensityDlg( &intensity ) == eIDOK ) {
				g_iLastLightIntensity = intensity;
				char buf[10];
				sprintf( buf, "%d", intensity );
				Node_getEntity( node )->setKeyValue( "light", buf );
			}
		}
		else if ( brushesSelected ) { // use workzone to set light position/size for doom3 lights, if there are brushes selected
			AABB bounds( Doom3Light_getBounds( workzone ) );
			StringOutputStream key( 64 );
			key << bounds.origin[0] << " " << bounds.origin[1] << " " << bounds.origin[2];
			Node_getEntity( node )->setKeyValue( "origin", key.c_str() );
			key.clear();
			key << bounds.extents[0] << " " << bounds.extents[1] << " " << bounds.extents[2];
			Node_getEntity( node )->setKeyValue( "light_radius", key.c_str() );
		}
	}

	if ( isModel ) {
		const char* model = misc_model_dialog( GTK_WIDGET( MainFrame_getWindow() ) );
		if ( model != 0 ) {
			Node_getEntity( node )->setKeyValue( entityClass->miscmodel_key() , model );
		}
	}
}

void Entity_ungroupSelectedPrimitives(){
	Entity_createFromSelection( "worldspawn", g_vector3_identity );
}


/* scale color so that at least one component is at 1.0F */
void NormalizeColor( Vector3& color ){
	const std::size_t maxi = vector3_max_abs_component_index( color );
	if ( color[maxi] == 0.f )
		color = Vector3( 1, 1, 1 );
	else{
		const float max = color[maxi];
		color /= max;
	}
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

					StringOutputStream command( 256 );
					command << "entityNormalizeColour " << buffer;
					UndoableCommand undo( command.c_str() );
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
			if ( color_dialog( GTK_WIDGET( MainFrame_getWindow() ), g_entity_globals.color_entity ) ) {
				char buffer[128];
				sprintf( buffer, "%g %g %g", g_entity_globals.color_entity[0],
						 g_entity_globals.color_entity[1],
						 g_entity_globals.color_entity[2] );

				StringOutputStream command( 256 );
				command << "entitySetColour " << buffer;
				UndoableCommand undo( command.c_str() );
				Scene_EntitySetKeyValue_Selected( "_color", buffer );
			}
		}
	}
}

const char* misc_model_dialog( GtkWidget* parent, const char* filepath ){
	StringOutputStream buffer( 1024 );

	if( !string_empty( filepath ) ){
		const char* root = GlobalFileSystem().findFile( filepath );
		if( !string_empty( root ) && file_is_directory( root ) )
			buffer << root << filepath;
	}
	if( buffer.empty() ){
		buffer << g_qeglobals.m_userGamePath.c_str() << "models/";

		if ( !file_readable( buffer.c_str() ) ) {
			// just go to fsmain
			buffer.clear();
			buffer << g_qeglobals.m_userGamePath.c_str();
		}
	}

	const char *filename = file_dialog( parent, true, "Choose Model", buffer.c_str(), ModelLoader::Name() );
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
/*
void LightRadiiImport( EntityCreator& self, bool value ){
	self.setLightRadii( value );
}
typedef ReferenceCaller1<EntityCreator, bool, LightRadiiImport> LightRadiiImportCaller;

void LightRadiiExport( EntityCreator& self, const BoolImportCallback& importer ){
	importer( self.getLightRadii() );
}
typedef ReferenceCaller1<EntityCreator, const BoolImportCallback&, LightRadiiExport> LightRadiiExportCaller;

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
typedef ReferenceCaller1<EntityCreator, int, ShowNamesDistImport> ShowNamesDistImportCaller;

void ShowNamesDistExport( EntityCreator& self, const IntImportCallback& importer ){
	importer( self.getShowNamesDist() );
}
typedef ReferenceCaller1<EntityCreator, const IntImportCallback&, ShowNamesDistExport> ShowNamesDistExportCaller;


void ShowNamesRatioImport( EntityCreator& self, int value ){
	self.setShowNamesRatio( value );
	UpdateAllWindows();
}
typedef ReferenceCaller1<EntityCreator, int, ShowNamesRatioImport> ShowNamesRatioImportCaller;

void ShowNamesRatioExport( EntityCreator& self, const IntImportCallback& importer ){
	importer( self.getShowNamesRatio() );
}
typedef ReferenceCaller1<EntityCreator, const IntImportCallback&, ShowNamesRatioExport> ShowNamesRatioExportCaller;


void ShowTargetNamesImport( EntityCreator& self, bool value ){
	if( self.getShowTargetNames() != value )
		PreferencesDialog_restartRequired( "Entity Names = Targetnames" ); // technically map reloading or entities recreation do update too, as it's not LatchedValue
	self.setShowTargetNames( value );
}
typedef ReferenceCaller1<EntityCreator, bool, ShowTargetNamesImport> ShowTargetNamesImportCaller;

void ShowTargetNamesExport( EntityCreator& self, const BoolImportCallback& importer ){
	importer( self.getShowTargetNames() );
}
typedef ReferenceCaller1<EntityCreator, const BoolImportCallback&, ShowTargetNamesExport> ShowTargetNamesExportCaller;


void Entity_constructPreferences( PreferencesPage& page ){
	page.appendSpinner(	"Names Display Distance (3D)", 512.0, 0.0, 200500.0,
		IntImportCallback( ShowNamesDistImportCaller( GlobalEntityCreator() ) ),
		IntExportCallback( ShowNamesDistExportCaller( GlobalEntityCreator() ) )
		);
	page.appendSpinner(	"Names Display Ratio (2D)", 64.0, 0.0, 100500.0,
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
	PreferencesDialog_addDisplayPage( FreeCaller1<PreferenceGroup&, Entity_constructPage>() );
}


void ShowLightRadiiExport( const BoolImportCallback& importer ){
	importer( GlobalEntityCreator().getLightRadii() );
}
typedef FreeCaller1<const BoolImportCallback&, ShowLightRadiiExport> ShowLightRadiiExportCaller;
ShowLightRadiiExportCaller g_show_lightradii_caller;
ToggleItem g_show_lightradii_item( g_show_lightradii_caller );
void ToggleShowLightRadii(){
	GlobalEntityCreator().setLightRadii( !GlobalEntityCreator().getLightRadii() );
	g_show_lightradii_item.update();
	UpdateAllWindows();
}

void Entity_constructMenu( GtkMenu* menu ){
	create_menu_item_with_mnemonic( menu, "_Connect Entities", "EntitiesConnect" );
	if ( g_pGameDescription->mGameType == "nexuiz" || g_pGameDescription->mGameType == "q1" ) {
		create_menu_item_with_mnemonic( menu, "_KillConnect Entities", "EntitiesKillConnect" );
	}
	create_menu_item_with_mnemonic( menu, "_Move Primitives to Entity", "EntityMovePrimitivesToLast" );
	create_menu_item_with_mnemonic( menu, "_Select Color...", "EntityColorSet" );
	create_menu_item_with_mnemonic( menu, "_Normalize Color", "EntityColorNormalize" );
}

void Entity_registerShortcuts(){
	command_connect_accelerator( "EntityMovePrimitivesToFirst" );
	command_connect_accelerator( "EntityUngroup" );
	command_connect_accelerator( "EntityUngroupPrimitives" );
}



#include "preferencesystem.h"
#include "stringio.h"

void Entity_Construct(){
	GlobalCommands_insert( "EntityColorSet", FreeCaller<Entity_setColour>(), Accelerator( 'K' ) );
	GlobalCommands_insert( "EntityColorNormalize", FreeCaller<Entity_normalizeColor>() );
	GlobalCommands_insert( "EntitiesConnect", FreeCaller<Entity_connectSelected>(), Accelerator( 'K', (GdkModifierType)GDK_CONTROL_MASK ) );
	if ( g_pGameDescription->mGameType == "nexuiz" || g_pGameDescription->mGameType == "q1" )
		GlobalCommands_insert( "EntitiesKillConnect", FreeCaller<Entity_killconnectSelected>(), Accelerator( 'K', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "EntityMovePrimitivesToLast", FreeCaller<Entity_moveSelectedPrimitivesToLast>(), Accelerator( 'M', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "EntityMovePrimitivesToFirst", FreeCaller<Entity_moveSelectedPrimitivesToFirst>() );
	GlobalCommands_insert( "EntityUngroup", FreeCaller<Entity_ungroup>() );
	GlobalCommands_insert( "EntityUngroupPrimitives", FreeCaller<Entity_ungroupSelectedPrimitives>() );

	GlobalToggles_insert( "ShowLightRadiuses", FreeCaller<ToggleShowLightRadii>(), ToggleItem::AddCallbackCaller( g_show_lightradii_item ) );

	GlobalPreferenceSystem().registerPreference( "SI_Colors5", Vector3ImportStringCaller( g_entity_globals.color_entity ), Vector3ExportStringCaller( g_entity_globals.color_entity ) );
	GlobalPreferenceSystem().registerPreference( "LastLightIntensity", IntImportStringCaller( g_iLastLightIntensity ), IntExportStringCaller( g_iLastLightIntensity ) );

	Entity_registerPreferencesPage();
}

void Entity_Destroy(){
}

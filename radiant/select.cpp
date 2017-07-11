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

#include "select.h"

#include "debugging/debugging.h"

#include "ientity.h"
#include "iselection.h"
#include "iundo.h"

#include <vector>

#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "shaderlib.h"
#include "scenelib.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include "brushmanip.h"
#include "brush.h"
#include "patch.h"
#include "patchmanip.h"
#include "patchdialog.h"
#include "texwindow.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "grid.h"
#include "map.h"
#include "entityinspector.h"



select_workzone_t g_select_workzone;


/**
   Loops over all selected brushes and stores their
   world AABBs in the specified array.
 */
class CollectSelectedBrushesBounds : public SelectionSystem::Visitor
{
AABB* m_bounds;     // array of AABBs
Unsigned m_max;     // max AABB-elements in array
Unsigned& m_count;  // count of valid AABBs stored in array

public:
CollectSelectedBrushesBounds( AABB* bounds, Unsigned max, Unsigned& count )
	: m_bounds( bounds ),
	m_max( max ),
	m_count( count ){
	m_count = 0;
}

void visit( scene::Instance& instance ) const {
	ASSERT_MESSAGE( m_count <= m_max, "Invalid m_count in CollectSelectedBrushesBounds" );

	// stop if the array is already full
	if ( m_count == m_max ) {
		return;
	}

	if ( Instance_isSelected( instance ) ) {
		// brushes only
		if ( Instance_getBrush( instance ) != 0 ) {
			m_bounds[m_count] = instance.worldAABB();
			++m_count;
		}
	}
}
};

/**
   Selects all objects that intersect one of the bounding AABBs.
   The exact intersection-method is specified through TSelectionPolicy
 */
template<class TSelectionPolicy>
class SelectByBounds : public scene::Graph::Walker
{
AABB* m_aabbs;             // selection aabbs
Unsigned m_count;          // number of aabbs in m_aabbs
TSelectionPolicy policy;   // type that contains a custom intersection method aabb<->aabb

public:
SelectByBounds( AABB* aabbs, Unsigned count )
	: m_aabbs( aabbs ),
	m_count( count ){
}

bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if( path.top().get().visible() ){
		Selectable* selectable = Instance_getSelectable( instance );

		// ignore worldspawn
		Entity* entity = Node_getEntity( path.top() );
		if ( entity ) {
			if ( string_equal( entity->getKeyValue( "classname" ), "worldspawn" ) ) {
				return true;
			}
		}

		if ( ( path.size() > 1 ) &&
			( !path.top().get().isRoot() ) &&
			( selectable != 0 ) &&
			( !node_is_group( path.top() ) )
			) {
			for ( Unsigned i = 0; i < m_count; ++i )
			{
				if ( policy.Evaluate( m_aabbs[i], instance ) ) {
					selectable->setSelected( true );
				}
			}
		}
	}
	else{
		return false;
	}

	return true;
}

/**
   Performs selection operation on the global scenegraph.
   If delete_bounds_src is true, then the objects which were
   used as source for the selection aabbs will be deleted.
 */
static void DoSelection( bool delete_bounds_src = true ){
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ) {
		// we may not need all AABBs since not all selected objects have to be brushes
		const Unsigned max = (Unsigned)GlobalSelectionSystem().countSelected();
		AABB* aabbs = new AABB[max];

		Unsigned count;
		CollectSelectedBrushesBounds collector( aabbs, max, count );
		GlobalSelectionSystem().foreachSelected( collector );

		// nothing usable in selection
		if ( !count ) {
			delete[] aabbs;
			return;
		}

		// delete selected objects
		if ( delete_bounds_src ) { // see deleteSelection
			UndoableCommand undo( "deleteSelected" );
			Select_Delete();
		}

		// select objects with bounds
		GlobalSceneGraph().traverse( SelectByBounds<TSelectionPolicy>( aabbs, count ) );

		SceneChangeNotify();
		delete[] aabbs;
	}
}
};

/**
   SelectionPolicy for SelectByBounds
   Returns true if box and the AABB of instance intersect
 */
class SelectionPolicy_Touching
{
public:
bool Evaluate( const AABB& box, scene::Instance& instance ) const {
	const AABB& other( instance.worldAABB() );
	for ( Unsigned i = 0; i < 3; ++i )
	{
		if ( fabsf( box.origin[i] - other.origin[i] ) > ( box.extents[i] + other.extents[i] ) ) {
			return false;
		}
	}
	return true;
}
};

/**
   SelectionPolicy for SelectByBounds
   Returns true if the AABB of instance is inside box
 */
class SelectionPolicy_Inside
{
public:
bool Evaluate( const AABB& box, scene::Instance& instance ) const {
	const AABB& other( instance.worldAABB() );
	for ( Unsigned i = 0; i < 3; ++i )
	{
		if ( fabsf( box.origin[i] - other.origin[i] ) > ( box.extents[i] - other.extents[i] ) ) {
			return false;
		}
	}
	return true;
}
};

class DeleteSelected : public scene::Graph::Walker
{
mutable bool m_remove;
mutable bool m_removedChild;
public:
DeleteSelected()
	: m_remove( false ), m_removedChild( false ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	m_removedChild = false;

	if ( Instance_isSelected( instance )
		 && path.size() > 1
		 && !path.top().get().isRoot() ) {
		m_remove = true;

		return false; // dont traverse into child elements
	}
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {

	if ( m_removedChild ) {
		m_removedChild = false;

		// delete empty entities
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0
			 && path.top().get_pointer() != Map_FindWorldspawn( g_map )
			 && Node_getTraversable( path.top() )->empty() ) {
			Path_deleteTop( path );
		}
	}

	// node should be removed
	if ( m_remove ) {
		if ( Node_isEntity( path.parent() ) ) {
			m_removedChild = true;
		}

		m_remove = false;
		Path_deleteTop( path );
	}
}
};

void Scene_DeleteSelected( scene::Graph& graph ){
	graph.traverse( DeleteSelected() );
	SceneChangeNotify();
}

void Select_Delete( void ){
	Scene_DeleteSelected( GlobalSceneGraph() );
}

class InvertSelectionWalker : public scene::Graph::Walker
{
SelectionSystem::EMode m_mode;
SelectionSystem::EComponentMode m_compmode;
mutable Selectable* m_selectable;
public:
InvertSelectionWalker( SelectionSystem::EMode mode, SelectionSystem::EComponentMode compmode )
	: m_mode( mode ), m_compmode( compmode ), m_selectable( 0 ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if( !path.top().get().visible() ){
		m_selectable = 0;
		return false;
	}
	Selectable* selectable = Instance_getSelectable( instance );
	if ( selectable ) {
		switch ( m_mode )
		{
		case SelectionSystem::eEntity:
			if ( Node_isEntity( path.top() ) != 0 ) {
				m_selectable = path.top().get().visible() ? selectable : 0;
			}
			break;
		case SelectionSystem::ePrimitive:
			m_selectable = path.top().get().visible() ? selectable : 0;
			break;
		case SelectionSystem::eComponent:
			BrushInstance* brushinstance = Instance_getBrush( instance );
			if( brushinstance != 0 ){
				if( brushinstance->isSelected() )
					brushinstance->invertComponentSelection( m_compmode );
			}
			else{
				PatchInstance* patchinstance = Instance_getPatch( instance );
				if( patchinstance != 0 && m_compmode == SelectionSystem::eVertex ){
					if( patchinstance->isSelected() )
						patchinstance->invertComponentSelection();
				}
			}
			break;
		}
	}
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	if ( m_selectable != 0 ) {
		m_selectable->setSelected( !m_selectable->isSelected() );
		m_selectable = 0;
	}
}
};

void Scene_Invert_Selection( scene::Graph& graph ){
	graph.traverse( InvertSelectionWalker( GlobalSelectionSystem().Mode(), GlobalSelectionSystem().ComponentMode() ) );
}

void Select_Invert(){
	Scene_Invert_Selection( GlobalSceneGraph() );
}

#if 0
//interesting printings
class ExpandSelectionToEntitiesWalker_dbg : public scene::Graph::Walker
{
mutable std::size_t m_depth;
const scene::Node* m_world;
public:
ExpandSelectionToEntitiesWalker_dbg() : m_depth( 0 ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	++m_depth;
	globalOutputStream() << "pre depth_" << m_depth;
	globalOutputStream() << " path.size()_" << path.size();
	if ( path.top().get_pointer() == m_world )
		globalOutputStream() << " worldspawn";
	if( path.top().get().isRoot() )
		globalOutputStream() << " path.top().get().isRoot()";
	Entity* entity = Node_getEntity( path.top() );
	if ( entity != 0 ){
		globalOutputStream() << " entity!=0";
		if( entity->isContainer() ){
			globalOutputStream() << " entity->isContainer()";
		}
		globalOutputStream() << " classname_" << entity->getKeyValue( "classname" );
	}
	globalOutputStream() << "\n";
//	globalOutputStream() << "" <<  ;
//	globalOutputStream() << "" <<  ;
//	globalOutputStream() << "" <<  ;
//	globalOutputStream() << "" <<  ;
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	globalOutputStream() << "post depth_" << m_depth;
	globalOutputStream() << " path.size()_" << path.size();
	if ( path.top().get_pointer() == m_world )
		globalOutputStream() << " worldspawn";
	if( path.top().get().isRoot() )
		globalOutputStream() << " path.top().get().isRoot()";
	Entity* entity = Node_getEntity( path.top() );
	if ( entity != 0 ){
		globalOutputStream() << " entity!=0";
		if( entity->isContainer() ){
			globalOutputStream() << " entity->isContainer()";
		}
		globalOutputStream() << " classname_" << entity->getKeyValue( "classname" );
	}
	globalOutputStream() << "\n";
	--m_depth;
}
};
#endif

class ExpandSelectionToPrimitivesWalker : public scene::Graph::Walker
{
mutable std::size_t m_depth;
const scene::Node* m_world;
public:
ExpandSelectionToPrimitivesWalker() : m_depth( 0 ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	++m_depth;

	if( !path.top().get().visible() )
		return false;

//	if ( path.top().get_pointer() == m_world ) // ignore worldspawn
//		return false;

	if ( m_depth == 2 ) { // entity depth
		// traverse and select children if any one is selected
		bool beselected = false;
		const bool isContainer = Node_getEntity( path.top() )->isContainer();
		if ( instance.childSelected() || instance.isSelected() ) {
			beselected = true;
			Instance_setSelected( instance, !isContainer );
		}
		return isContainer && beselected;
	}
	else if ( m_depth == 3 ) { // primitive depth
		Instance_setSelected( instance, true );
		return false;
	}
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	--m_depth;
}
};

void Scene_ExpandSelectionToPrimitives(){
	GlobalSceneGraph().traverse( ExpandSelectionToPrimitivesWalker() );
}

class ExpandSelectionToEntitiesWalker : public scene::Graph::Walker
{
mutable std::size_t m_depth;
const scene::Node* m_world;
public:
ExpandSelectionToEntitiesWalker() : m_depth( 0 ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	++m_depth;

	if( !path.top().get().visible() )
		return false;

//	if ( path.top().get_pointer() == m_world ) // ignore worldspawn
//		return false;

	if ( m_depth == 2 ) { // entity depth
		// traverse and select children if any one is selected
		bool beselected = false;
		if ( instance.childSelected() || instance.isSelected() ) {
			beselected = true;
			if( path.top().get_pointer() != m_world ){ //avoid selecting world node
				Instance_setSelected( instance, true );
			}
		}
		return Node_getEntity( path.top() )->isContainer() && beselected;
	}
	else if ( m_depth == 3 ) { // primitive depth
		Instance_setSelected( instance, true );
		return false;
	}
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	--m_depth;
}
};

void Scene_ExpandSelectionToEntities(){
	GlobalSceneGraph().traverse( ExpandSelectionToEntitiesWalker() );
}


namespace
{
void Selection_UpdateWorkzone(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		Select_GetBounds( g_select_workzone.d_work_min, g_select_workzone.d_work_max );
	}
}
typedef FreeCaller<Selection_UpdateWorkzone> SelectionUpdateWorkzoneCaller;

IdleDraw g_idleWorkzone = IdleDraw( SelectionUpdateWorkzoneCaller() );
}

const select_workzone_t& Select_getWorkZone(){
	g_idleWorkzone.flush();
	return g_select_workzone;
}

void UpdateWorkzone_ForSelection(){
	g_idleWorkzone.queueDraw();
}

// update the workzone to the current selection
void UpdateWorkzone_ForSelectionChanged( const Selectable& selectable ){
	//if ( selectable.isSelected() ) {
		UpdateWorkzone_ForSelection();
	//}
}

void Select_SetShader( const char* shader ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushSetShader_Selected( GlobalSceneGraph(), shader );
		Scene_PatchSetShader_Selected( GlobalSceneGraph(), shader );
	}
	Scene_BrushSetShader_Component_Selected( GlobalSceneGraph(), shader );
}

void Select_SetShader_Undo( const char* shader ){
	if ( GlobalSelectionSystem().countSelectedComponents() != 0 || GlobalSelectionSystem().countSelected() != 0 ) {
		UndoableCommand undo( "textureNameSetSelected" );
		Select_SetShader( shader );
	}
}

void Select_SetTexdef( const TextureProjection& projection, bool setBasis /*= true*/, bool resetBasis /*= false*/ ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushSetTexdef_Selected( GlobalSceneGraph(), projection, setBasis, resetBasis );
	}
	Scene_BrushSetTexdef_Component_Selected( GlobalSceneGraph(), projection, setBasis, resetBasis );
}

void Select_SetTexdef( const float* hShift, const float* vShift, const float* hScale, const float* vScale, const float* rotation ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushSetTexdef_Selected( GlobalSceneGraph(), hShift, vShift, hScale, vScale, rotation );
	}
	Scene_BrushSetTexdef_Component_Selected( GlobalSceneGraph(), hShift, vShift, hScale, vScale, rotation );
}

void Select_SetFlags( const ContentsFlagsValue& flags ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushSetFlags_Selected( GlobalSceneGraph(), flags );
	}
	Scene_BrushSetFlags_Component_Selected( GlobalSceneGraph(), flags );
}

void Select_GetBounds( Vector3& mins, Vector3& maxs ){
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	maxs = vector3_added( bounds.origin, bounds.extents );
	mins = vector3_subtracted( bounds.origin, bounds.extents );
}


void Select_FlipAxis( int axis ){
	Vector3 flip( 1, 1, 1 );
	flip[axis] = -1;
	GlobalSelectionSystem().scaleSelected( flip, true );
}


void Select_Scale( float x, float y, float z ){
	GlobalSelectionSystem().scaleSelected( Vector3( x, y, z ) );
}

enum axis_t
{
	eAxisX = 0,
	eAxisY = 1,
	eAxisZ = 2,
};

enum sign_t
{
	eSignPositive = 1,
	eSignNegative = -1,
};

inline Matrix4 matrix4_rotation_for_axis90( axis_t axis, sign_t sign ){
	switch ( axis )
	{
	case eAxisX:
		if ( sign == eSignPositive ) {
			return matrix4_rotation_for_sincos_x( 1, 0 );
		}
		else
		{
			return matrix4_rotation_for_sincos_x( -1, 0 );
		}
	case eAxisY:
		if ( sign == eSignPositive ) {
			return matrix4_rotation_for_sincos_y( 1, 0 );
		}
		else
		{
			return matrix4_rotation_for_sincos_y( -1, 0 );
		}
	default: //case eAxisZ:
		if ( sign == eSignPositive ) {
			return matrix4_rotation_for_sincos_z( 1, 0 );
		}
		else
		{
			return matrix4_rotation_for_sincos_z( -1, 0 );
		}
	}
}

inline void matrix4_rotate_by_axis90( Matrix4& matrix, axis_t axis, sign_t sign ){
	matrix4_multiply_by_matrix4( matrix, matrix4_rotation_for_axis90( axis, sign ) );
}

inline void matrix4_pivoted_rotate_by_axis90( Matrix4& matrix, axis_t axis, sign_t sign, const Vector3& pivotpoint ){
	matrix4_translate_by_vec3( matrix, pivotpoint );
	matrix4_rotate_by_axis90( matrix, axis, sign );
	matrix4_translate_by_vec3( matrix, vector3_negated( pivotpoint ) );
}

inline Quaternion quaternion_for_axis90( axis_t axis, sign_t sign ){
#if 1
	switch ( axis )
	{
	case eAxisX:
		if ( sign == eSignPositive ) {
			return Quaternion( c_half_sqrt2f, 0, 0, c_half_sqrt2f );
		}
		else
		{
			return Quaternion( -c_half_sqrt2f, 0, 0, -c_half_sqrt2f );
		}
	case eAxisY:
		if ( sign == eSignPositive ) {
			return Quaternion( 0, c_half_sqrt2f, 0, c_half_sqrt2f );
		}
		else
		{
			return Quaternion( 0, -c_half_sqrt2f, 0, -c_half_sqrt2f );
		}
	default: //case eAxisZ:
		if ( sign == eSignPositive ) {
			return Quaternion( 0, 0, c_half_sqrt2f, c_half_sqrt2f );
		}
		else
		{
			return Quaternion( 0, 0, -c_half_sqrt2f, -c_half_sqrt2f );
		}
	}
#else
	quaternion_for_matrix4_rotation( matrix4_rotation_for_axis90( (axis_t)axis, ( deg > 0 ) ? eSignPositive : eSignNegative ) );
#endif
}

void Select_RotateAxis( int axis, float deg ){
	if ( fabs( deg ) == 90.f ) {
		GlobalSelectionSystem().rotateSelected( quaternion_for_axis90( (axis_t)axis, ( deg > 0 ) ? eSignPositive : eSignNegative ), true );
	}
	else
	{
		switch ( axis )
		{
		case 0:
			GlobalSelectionSystem().rotateSelected( quaternion_for_matrix4_rotation( matrix4_rotation_for_x_degrees( deg ) ) );
			break;
		case 1:
			GlobalSelectionSystem().rotateSelected( quaternion_for_matrix4_rotation( matrix4_rotation_for_y_degrees( deg ) ) );
			break;
		case 2:
			GlobalSelectionSystem().rotateSelected( quaternion_for_matrix4_rotation( matrix4_rotation_for_z_degrees( deg ) ) );
			break;
		}
	}
}


void Select_ShiftTexture( float x, float y ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushShiftTexdef_Selected( GlobalSceneGraph(), x, y );
		Scene_PatchTranslateTexture_Selected( GlobalSceneGraph(), x, y );
	}
	//globalOutputStream() << "shift selected face textures: s=" << x << " t=" << y << '\n';
	Scene_BrushShiftTexdef_Component_Selected( GlobalSceneGraph(), x, y );
}

void Select_ScaleTexture( float x, float y ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushScaleTexdef_Selected( GlobalSceneGraph(), x, y );
		Scene_PatchScaleTexture_Selected( GlobalSceneGraph(), x, y );
	}
	Scene_BrushScaleTexdef_Component_Selected( GlobalSceneGraph(), x, y );
}

void Select_RotateTexture( float amt ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushRotateTexdef_Selected( GlobalSceneGraph(), amt );
		Scene_PatchRotateTexture_Selected( GlobalSceneGraph(), amt );
	}
	Scene_BrushRotateTexdef_Component_Selected( GlobalSceneGraph(), amt );
}

// TTimo modified to handle shader architecture:
// expects shader names at input, comparison relies on shader names .. texture names no longer relevant
void FindReplaceTextures( const char* pFind, const char* pReplace, bool bSelected ){
	if ( !texdef_name_valid( pFind ) ) {
		globalErrorStream() << "FindReplaceTextures: invalid texture name: '" << pFind << "', aborted\n";
		return;
	}
	if ( !texdef_name_valid( pReplace ) ) {
		globalErrorStream() << "FindReplaceTextures: invalid texture name: '" << pReplace << "', aborted\n";
		return;
	}

	StringOutputStream command;
	command << "textureFindReplace -find " << pFind << " -replace " << pReplace;
	UndoableCommand undo( command.c_str() );

	if( shader_equal( pReplace, "textures/" ) )
		pReplace = 0; //do search

	if ( bSelected ) {
		if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
			Scene_BrushFindReplaceShader_Selected( GlobalSceneGraph(), pFind, pReplace );
			Scene_PatchFindReplaceShader_Selected( GlobalSceneGraph(), pFind, pReplace );
		}
		Scene_BrushFindReplaceShader_Component_Selected( GlobalSceneGraph(), pFind, pReplace );
	}
	else
	{
		Scene_BrushFindReplaceShader( GlobalSceneGraph(), pFind, pReplace );
		Scene_PatchFindReplaceShader( GlobalSceneGraph(), pFind, pReplace );
	}
}

typedef std::vector<const char*> PropertyValues;

bool propertyvalues_contain( const PropertyValues& propertyvalues, const char *str ){
	for ( PropertyValues::const_iterator i = propertyvalues.begin(); i != propertyvalues.end(); ++i )
	{
		if ( string_equal( str, *i ) ) {
			return true;
		}
	}
	return false;
}

class EntityFindByPropertyValueWalker : public scene::Graph::Walker
{
const PropertyValues& m_propertyvalues;
const char *m_prop;
const scene::Node* m_world;
public:
EntityFindByPropertyValueWalker( const char *prop, const PropertyValues& propertyvalues )
	: m_propertyvalues( propertyvalues ), m_prop( prop ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if( !path.top().get().visible() ){
		return false;
	}
	// ignore worldspawn
	if ( path.top().get_pointer() == m_world ) {
		return false;
	}

	Entity* entity = Node_getEntity( path.top() );
	if ( entity != 0 ){
		if( propertyvalues_contain( m_propertyvalues, entity->getKeyValue( m_prop ) ) ) {
			Instance_getSelectable( instance )->setSelected( true );
			return true;
		}
		return false;
	}
	else if( path.size() > 2 && !path.top().get().isRoot() ){
		Selectable* selectable = Instance_getSelectable( instance );
		if( selectable != 0 )
			selectable->setSelected( true );
	}
	return true;
}
};

void Scene_EntitySelectByPropertyValues( scene::Graph& graph, const char *prop, const PropertyValues& propertyvalues ){
	graph.traverse( EntityFindByPropertyValueWalker( prop, propertyvalues ) );
}

class EntityGetSelectedPropertyValuesWalker : public scene::Graph::Walker
{
PropertyValues& m_propertyvalues;
const char *m_prop;
const scene::Node* m_world;
public:
EntityGetSelectedPropertyValuesWalker( const char *prop, PropertyValues& propertyvalues )
	: m_propertyvalues( propertyvalues ), m_prop( prop ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Entity* entity = Node_getEntity( path.top() );
	if ( entity != 0 ){
		if( path.top().get_pointer() != m_world ){
			if ( Instance_isSelected( instance ) || instance.childSelected() ) {
				if ( !propertyvalues_contain( m_propertyvalues, entity->getKeyValue( m_prop ) ) ) {
					m_propertyvalues.push_back( entity->getKeyValue( m_prop ) );
				}
			}
		}
		return false;
	}
	return true;
}
};
/*
class EntityGetSelectedPropertyValuesWalker : public scene::Graph::Walker
{
PropertyValues& m_propertyvalues;
const char *m_prop;
mutable bool m_selected_children;
const scene::Node* m_world;
public:
EntityGetSelectedPropertyValuesWalker( const char *prop, PropertyValues& propertyvalues )
	: m_propertyvalues( propertyvalues ), m_prop( prop ), m_selected_children( false ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( Instance_isSelected( instance ) ) {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity != 0 ) {
			if ( !propertyvalues_contain( m_propertyvalues, entity->getKeyValue( m_prop ) ) ) {
				m_propertyvalues.push_back( entity->getKeyValue( m_prop ) );
			}
			return false;
		}
		else{
			m_selected_children = true;
		}
	}
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	Entity* entity = Node_getEntity( path.top() );
	if( entity != 0 && m_selected_children ){
		m_selected_children = false;
		if( path.top().get_pointer() == m_world )
			return;
		if ( !propertyvalues_contain( m_propertyvalues, entity->getKeyValue( m_prop ) ) ) {
			m_propertyvalues.push_back( entity->getKeyValue( m_prop ) );
		}
	}
}
};
*/
void Scene_EntityGetPropertyValues( scene::Graph& graph, const char *prop, PropertyValues& propertyvalues ){
	graph.traverse( EntityGetSelectedPropertyValuesWalker( prop, propertyvalues ) );
}

void Select_AllOfType(){
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent ) {
		if ( GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace ) {
			GlobalSelectionSystem().setSelectedAllComponents( false );
			Scene_BrushSelectByShader_Component( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
		}
	}
	else
	{
		PropertyValues propertyvalues;
		const char *prop = EntityInspector_getCurrentKey();
		if ( !prop || !*prop ) {
			prop = "classname";
		}
		Scene_EntityGetPropertyValues( GlobalSceneGraph(), prop, propertyvalues );
		GlobalSelectionSystem().setSelectedAll( false );
		if ( !propertyvalues.empty() ) {
			Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), prop, propertyvalues );
		}
		else
		{
			Scene_BrushSelectByShader( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
			Scene_PatchSelectByShader( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
		}
	}
}

void Select_FacesAndPatchesByShader(){
	Scene_BrushFacesSelectByShader( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
	Scene_PatchSelectByShader( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
}

void Select_Inside( void ){
	SelectByBounds<SelectionPolicy_Inside>::DoSelection();
}

void Select_Touching( void ){
	SelectByBounds<SelectionPolicy_Touching>::DoSelection( false );
}

void Select_ProjectTexture( const texdef_t& texdef, const Vector3* direction ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushProjectTexture_Selected( GlobalSceneGraph(), texdef, direction );
		Scene_PatchProjectTexture_Selected( GlobalSceneGraph(), texdef, direction );
	}
	Scene_BrushProjectTexture_Component_Selected( GlobalSceneGraph(), texdef, direction );

	SceneChangeNotify();
}

void Select_ProjectTexture( const TextureProjection& projection, const Vector3& normal ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushProjectTexture_Selected( GlobalSceneGraph(), projection, normal );
		Scene_PatchProjectTexture_Selected( GlobalSceneGraph(), projection, normal );
	}
	Scene_BrushProjectTexture_Component_Selected( GlobalSceneGraph(), projection, normal );

	SceneChangeNotify();
}

void Select_FitTexture( float horizontal, float vertical, bool only_dimension ){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::eComponent ) {
		Scene_BrushFitTexture_Selected( GlobalSceneGraph(), horizontal, vertical, only_dimension );
	}
	Scene_BrushFitTexture_Component_Selected( GlobalSceneGraph(), horizontal, vertical, only_dimension );

	SceneChangeNotify();
}


#include "commands.h"
#include "dialog.h"

inline void hide_node( scene::Node& node, bool hide ){
	hide
	? node.enable( scene::Node::eHidden )
	: node.disable( scene::Node::eHidden );
}

bool g_nodes_be_hidden = false;

BoolExportCaller g_hidden_caller( g_nodes_be_hidden );
ToggleItem g_hidden_item( g_hidden_caller );

class HideSelectedWalker : public scene::Graph::Walker
{
bool m_hide;
public:
HideSelectedWalker( bool hide )
	: m_hide( hide ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	if ( Instance_isSelected( instance ) ) {
		g_nodes_be_hidden = m_hide;
		hide_node( path.top(), m_hide );
	}
	return true;
}
};

void Scene_Hide_Selected( bool hide ){
	GlobalSceneGraph().traverse( HideSelectedWalker( hide ) );
}

void Select_Hide(){
	Scene_Hide_Selected( true );
	SceneChangeNotify();
}

void HideSelected(){
	Select_Hide();
	if( GlobalSelectionSystem().countSelectedComponents() != 0 )
		GlobalSelectionSystem().setSelectedAllComponents( false );
	GlobalSelectionSystem().setSelectedAll( false );
	g_hidden_item.update();
}


class HideAllWalker : public scene::Graph::Walker
{
bool m_hide;
public:
HideAllWalker( bool hide )
	: m_hide( hide ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	hide_node( path.top(), m_hide );
	return true;
}
};

void Scene_Hide_All( bool hide ){
	GlobalSceneGraph().traverse( HideAllWalker( hide ) );
}

void Select_ShowAllHidden(){
	Scene_Hide_All( false );
	SceneChangeNotify();
	g_nodes_be_hidden = false;
	g_hidden_item.update();
}


void Selection_Flipx(){
	UndoableCommand undo( "mirrorSelected -axis x" );
	Select_FlipAxis( 0 );
}

void Selection_Flipy(){
	UndoableCommand undo( "mirrorSelected -axis y" );
	Select_FlipAxis( 1 );
}

void Selection_Flipz(){
	UndoableCommand undo( "mirrorSelected -axis z" );
	Select_FlipAxis( 2 );
}

void Selection_Rotatex(){
	UndoableCommand undo( "rotateSelected -axis x -angle -90" );
	Select_RotateAxis( 0,-90 );
}

void Selection_Rotatey(){
	UndoableCommand undo( "rotateSelected -axis y -angle 90" );
	Select_RotateAxis( 1, 90 );
}

void Selection_Rotatez(){
	UndoableCommand undo( "rotateSelected -axis z -angle -90" );
	Select_RotateAxis( 2,-90 );
}
#include "xywindow.h"
void Selection_FlipHorizontally(){
	VIEWTYPE viewtype = GlobalXYWnd_getCurrentViewType();
	switch ( viewtype )
	{
	case XY:
	case XZ:
		Selection_Flipx();
		break;
	default:
		Selection_Flipy();
		break;
	}
}

void Selection_FlipVertically(){
	VIEWTYPE viewtype = GlobalXYWnd_getCurrentViewType();
	switch ( viewtype )
	{
	case XZ:
	case YZ:
		Selection_Flipz();
		break;
	default:
		Selection_Flipy();
		break;
	}
}

void Selection_RotateClockwise(){
	UndoableCommand undo( "rotateSelected Clockwise 90" );
	VIEWTYPE viewtype = GlobalXYWnd_getCurrentViewType();
	switch ( viewtype )
	{
	case XY:
		Select_RotateAxis( 2, -90 );
		break;
	case XZ:
		Select_RotateAxis( 1, 90 );
		break;
	default:
		Select_RotateAxis( 0, -90 );
		break;
	}
}

void Selection_RotateAnticlockwise(){
	UndoableCommand undo( "rotateSelected Anticlockwise 90" );
	VIEWTYPE viewtype = GlobalXYWnd_getCurrentViewType();
	switch ( viewtype )
	{
	case XY:
		Select_RotateAxis( 2, 90 );
		break;
	case XZ:
		Select_RotateAxis( 1, -90 );
		break;
	default:
		Select_RotateAxis( 0, 90 );
		break;
	}

}



void Select_registerCommands(){
	GlobalCommands_insert( "ShowHidden", FreeCaller<Select_ShowAllHidden>(), Accelerator( 'H', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalToggles_insert( "HideSelected", FreeCaller<HideSelected>(), ToggleItem::AddCallbackCaller( g_hidden_item ), Accelerator( 'H' ) );

	GlobalCommands_insert( "MirrorSelectionX", FreeCaller<Selection_Flipx>() );
	GlobalCommands_insert( "RotateSelectionX", FreeCaller<Selection_Rotatex>() );
	GlobalCommands_insert( "MirrorSelectionY", FreeCaller<Selection_Flipy>() );
	GlobalCommands_insert( "RotateSelectionY", FreeCaller<Selection_Rotatey>() );
	GlobalCommands_insert( "MirrorSelectionZ", FreeCaller<Selection_Flipz>() );
	GlobalCommands_insert( "RotateSelectionZ", FreeCaller<Selection_Rotatez>() );

	GlobalCommands_insert( "MirrorSelectionHorizontally", FreeCaller<Selection_FlipHorizontally>() );
	GlobalCommands_insert( "MirrorSelectionVertically", FreeCaller<Selection_FlipVertically>() );

	GlobalCommands_insert( "RotateSelectionClockwise", FreeCaller<Selection_RotateClockwise>() );
	GlobalCommands_insert( "RotateSelectionAnticlockwise", FreeCaller<Selection_RotateAnticlockwise>() );

	GlobalCommands_insert( "SelectTextured", FreeCaller<Select_FacesAndPatchesByShader>(), Accelerator( 'A', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
}


void Nudge( int nDim, float fNudge ){
	Vector3 translate( 0, 0, 0 );
	translate[nDim] = fNudge;

	GlobalSelectionSystem().translateSelected( translate );
}

void Selection_NudgeZ( float amount ){
	StringOutputStream command;
	command << "nudgeSelected -axis z -amount " << amount;
	UndoableCommand undo( command.c_str() );

	Nudge( 2, amount );
}

void Selection_MoveDown(){
	Selection_NudgeZ( -GetGridSize() );
}

void Selection_MoveUp(){
	Selection_NudgeZ( GetGridSize() );
}

void SceneSelectionChange( const Selectable& selectable ){
	SceneChangeNotify();
}

SignalHandlerId Selection_boundsChanged;

void Selection_construct(){
	typedef FreeCaller1<const Selectable&, SceneSelectionChange> SceneSelectionChangeCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( SceneSelectionChangeCaller() );
	typedef FreeCaller1<const Selectable&, UpdateWorkzone_ForSelectionChanged> UpdateWorkzoneForSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( UpdateWorkzoneForSelectionChangedCaller() );
	typedef FreeCaller<UpdateWorkzone_ForSelection> UpdateWorkzoneForSelectionCaller;
	Selection_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback( UpdateWorkzoneForSelectionCaller() );
}

void Selection_destroy(){
	GlobalSceneGraph().removeBoundsChangedCallback( Selection_boundsChanged );
}


#include "gtkdlgs.h"
#include <gtk/gtkbox.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gdk/gdkkeysyms.h>


inline Quaternion quaternion_for_euler_xyz_degrees( const Vector3& eulerXYZ ){
#if 0
	return quaternion_for_matrix4_rotation( matrix4_rotation_for_euler_xyz_degrees( eulerXYZ ) );
#elif 0
	return quaternion_multiplied_by_quaternion(
			   quaternion_multiplied_by_quaternion(
				   quaternion_for_z( degrees_to_radians( eulerXYZ[2] ) ),
				   quaternion_for_y( degrees_to_radians( eulerXYZ[1] ) )
				   ),
			   quaternion_for_x( degrees_to_radians( eulerXYZ[0] ) )
			   );
#elif 1
	double cx = cos( degrees_to_radians( eulerXYZ[0] * 0.5 ) );
	double sx = sin( degrees_to_radians( eulerXYZ[0] * 0.5 ) );
	double cy = cos( degrees_to_radians( eulerXYZ[1] * 0.5 ) );
	double sy = sin( degrees_to_radians( eulerXYZ[1] * 0.5 ) );
	double cz = cos( degrees_to_radians( eulerXYZ[2] * 0.5 ) );
	double sz = sin( degrees_to_radians( eulerXYZ[2] * 0.5 ) );

	return Quaternion(
			   cz * cy * sx - sz * sy * cx,
			   cz * sy * cx + sz * cy * sx,
			   sz * cy * cx - cz * sy * sx,
			   cz * cy * cx + sz * sy * sx
			   );
#endif
}

struct RotateDialog
{
	GtkSpinButton* x;
	GtkSpinButton* y;
	GtkSpinButton* z;
	GtkWindow *window;
};

static gboolean rotatedlg_apply( GtkWidget *widget, RotateDialog* rotateDialog ){
	Vector3 eulerXYZ;

	/* only update in scenario of enter pressed to also allow extra precision of values after execution by buttons */
	if( gtk_widget_has_focus( GTK_WIDGET( rotateDialog->x ) ) )
		gtk_spin_button_update( rotateDialog->x );
	else if( gtk_widget_has_focus( GTK_WIDGET( rotateDialog->y ) ) )
		gtk_spin_button_update( rotateDialog->y );
	else if( gtk_widget_has_focus( GTK_WIDGET( rotateDialog->z ) ) )
		gtk_spin_button_update( rotateDialog->z );

	eulerXYZ[0] = static_cast<float>( gtk_spin_button_get_value( rotateDialog->x ) );
	eulerXYZ[1] = static_cast<float>( gtk_spin_button_get_value( rotateDialog->y ) );
	eulerXYZ[2] = static_cast<float>( gtk_spin_button_get_value( rotateDialog->z ) );

	StringOutputStream command;
	command << "rotateSelectedEulerXYZ -x " << eulerXYZ[0] << " -y " << eulerXYZ[1] << " -z " << eulerXYZ[2];
	UndoableCommand undo( command.c_str() );

	GlobalSelectionSystem().rotateSelected( quaternion_for_euler_xyz_degrees( eulerXYZ ) );
	return TRUE;
}

static gboolean rotatedlg_cancel( GtkWidget *widget, RotateDialog* rotateDialog ){
	gtk_widget_hide( GTK_WIDGET( rotateDialog->window ) );

	gtk_spin_button_set_value( rotateDialog->x, 0.0 ); // reset to 0 on close
	gtk_spin_button_set_value( rotateDialog->y, 0.0 );
	gtk_spin_button_set_value( rotateDialog->z, 0.0 );

	return TRUE;
}

static gboolean rotatedlg_ok( GtkWidget *widget, RotateDialog* rotateDialog ){
	rotatedlg_apply( widget, rotateDialog );
//	rotatedlg_cancel( widget, rotateDialog );
	gtk_widget_hide( GTK_WIDGET( rotateDialog->window ) );
	return TRUE;
}

static gboolean rotatedlg_delete( GtkWidget *widget, GdkEventAny *event, RotateDialog* rotateDialog ){
	rotatedlg_cancel( widget, rotateDialog );
	return TRUE;
}

RotateDialog g_rotate_dialog;
void DoRotateDlg(){
	if ( g_rotate_dialog.window == NULL ) {
		g_rotate_dialog.window = create_dialog_window( MainFrame_getWindow(), "Arbitrary rotation", G_CALLBACK( rotatedlg_delete ), &g_rotate_dialog );

		GtkAccelGroup* accel = gtk_accel_group_new();
		gtk_window_add_accel_group( g_rotate_dialog.window, accel );

		{
			GtkHBox* hbox = create_dialog_hbox( 4, 4 );
			gtk_container_add( GTK_CONTAINER( g_rotate_dialog.window ), GTK_WIDGET( hbox ) );
			{
				GtkTable* table = create_dialog_table( 3, 2, 4, 4 );
				gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
				{
					GtkWidget* label = gtk_label_new( "  X  " );
					gtk_widget_show( label );
					gtk_table_attach( table, label, 0, 1, 0, 1,
									  (GtkAttachOptions) ( 0 ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkWidget* label = gtk_label_new( "  Y  " );
					gtk_widget_show( label );
					gtk_table_attach( table, label, 0, 1, 1, 2,
									  (GtkAttachOptions) ( 0 ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkWidget* label = gtk_label_new( "  Z  " );
					gtk_widget_show( label );
					gtk_table_attach( table, label, 0, 1, 2, 3,
									  (GtkAttachOptions) ( 0 ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkAdjustment* adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0, -359, 359, 1, 10, 0 ) );
					GtkSpinButton* spin = GTK_SPIN_BUTTON( gtk_spin_button_new( adj, 1, 2 ) );
					gtk_widget_show( GTK_WIDGET( spin ) );
					gtk_table_attach( table, GTK_WIDGET( spin ), 1, 2, 0, 1,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_widget_set_size_request( GTK_WIDGET( spin ), 64, -1 );
					gtk_spin_button_set_wrap( spin, TRUE );

					gtk_widget_grab_focus( GTK_WIDGET( spin ) );

					g_rotate_dialog.x = spin;
				}
				{
					GtkAdjustment* adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0, -359, 359, 1, 10, 0 ) );
					GtkSpinButton* spin = GTK_SPIN_BUTTON( gtk_spin_button_new( adj, 1, 2 ) );
					gtk_widget_show( GTK_WIDGET( spin ) );
					gtk_table_attach( table, GTK_WIDGET( spin ), 1, 2, 1, 2,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_widget_set_size_request( GTK_WIDGET( spin ), 64, -1 );
					gtk_spin_button_set_wrap( spin, TRUE );

					g_rotate_dialog.y = spin;
				}
				{
					GtkAdjustment* adj = GTK_ADJUSTMENT( gtk_adjustment_new( 0, -359, 359, 1, 10, 0 ) );
					GtkSpinButton* spin = GTK_SPIN_BUTTON( gtk_spin_button_new( adj, 1, 2 ) );
					gtk_widget_show( GTK_WIDGET( spin ) );
					gtk_table_attach( table, GTK_WIDGET( spin ), 1, 2, 2, 3,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
					gtk_widget_set_size_request( GTK_WIDGET( spin ), 64, -1 );
					gtk_spin_button_set_wrap( spin, TRUE );

					g_rotate_dialog.z = spin;
				}
			}
			{
				GtkVBox* vbox = create_dialog_vbox( 4 );
				gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE, 0 );
				{
					GtkButton* button = create_dialog_button( "OK", G_CALLBACK( rotatedlg_ok ), &g_rotate_dialog );
					gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
					widget_make_default( GTK_WIDGET( button ) );
					gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
				}
				{
					GtkButton* button = create_dialog_button( "Cancel", G_CALLBACK( rotatedlg_cancel ), &g_rotate_dialog );
					gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
					gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
				}
				{
					GtkButton* button = create_dialog_button( "Apply", G_CALLBACK( rotatedlg_apply ), &g_rotate_dialog );
					gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				}
			}
		}
	}

	gtk_widget_show( GTK_WIDGET( g_rotate_dialog.window ) );
}









struct ScaleDialog
{
	GtkWidget* x;
	GtkWidget* y;
	GtkWidget* z;
	GtkWindow *window;
};

static gboolean scaledlg_apply( GtkWidget *widget, ScaleDialog* scaleDialog ){
	float sx, sy, sz;

	sx = static_cast<float>( atof( gtk_entry_get_text( GTK_ENTRY( scaleDialog->x ) ) ) );
	sy = static_cast<float>( atof( gtk_entry_get_text( GTK_ENTRY( scaleDialog->y ) ) ) );
	sz = static_cast<float>( atof( gtk_entry_get_text( GTK_ENTRY( scaleDialog->z ) ) ) );

	StringOutputStream command;
	command << "scaleSelected -x " << sx << " -y " << sy << " -z " << sz;
	UndoableCommand undo( command.c_str() );

	Select_Scale( sx, sy, sz );

	return TRUE;
}

static gboolean scaledlg_cancel( GtkWidget *widget, ScaleDialog* scaleDialog ){
	gtk_widget_hide( GTK_WIDGET( scaleDialog->window ) );

	gtk_entry_set_text( GTK_ENTRY( scaleDialog->x ), "1.0" );
	gtk_entry_set_text( GTK_ENTRY( scaleDialog->y ), "1.0" );
	gtk_entry_set_text( GTK_ENTRY( scaleDialog->z ), "1.0" );

	return TRUE;
}

static gboolean scaledlg_ok( GtkWidget *widget, ScaleDialog* scaleDialog ){
	scaledlg_apply( widget, scaleDialog );
	//scaledlg_cancel( widget, scaleDialog );
	gtk_widget_hide( GTK_WIDGET( scaleDialog->window ) );
	return TRUE;
}

static gboolean scaledlg_delete( GtkWidget *widget, GdkEventAny *event, ScaleDialog* scaleDialog ){
	scaledlg_cancel( widget, scaleDialog );
	return TRUE;
}

ScaleDialog g_scale_dialog;

void DoScaleDlg(){
	if ( g_scale_dialog.window == NULL ) {
		g_scale_dialog.window = create_dialog_window( MainFrame_getWindow(), "Arbitrary scale", G_CALLBACK( scaledlg_delete ), &g_scale_dialog );

		GtkAccelGroup* accel = gtk_accel_group_new();
		gtk_window_add_accel_group( g_scale_dialog.window, accel );

		{
			GtkHBox* hbox = create_dialog_hbox( 4, 4 );
			gtk_container_add( GTK_CONTAINER( g_scale_dialog.window ), GTK_WIDGET( hbox ) );
			{
				GtkTable* table = create_dialog_table( 3, 2, 4, 4 );
				gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
				{
					GtkWidget* label = gtk_label_new( "  X  " );
					gtk_widget_show( label );
					gtk_table_attach( table, label, 0, 1, 0, 1,
									  (GtkAttachOptions) ( 0 ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkWidget* label = gtk_label_new( "  Y  " );
					gtk_widget_show( label );
					gtk_table_attach( table, label, 0, 1, 1, 2,
									  (GtkAttachOptions) ( 0 ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkWidget* label = gtk_label_new( "  Z  " );
					gtk_widget_show( label );
					gtk_table_attach( table, label, 0, 1, 2, 3,
									  (GtkAttachOptions) ( 0 ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );
				}
				{
					GtkWidget* entry = gtk_entry_new();
					gtk_entry_set_text( GTK_ENTRY( entry ), "1.0" );
					gtk_widget_show( entry );
					gtk_table_attach( table, entry, 1, 2, 0, 1,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );

					g_scale_dialog.x = entry;
				}
				{
					GtkWidget* entry = gtk_entry_new();
					gtk_entry_set_text( GTK_ENTRY( entry ), "1.0" );
					gtk_widget_show( entry );
					gtk_table_attach( table, entry, 1, 2, 1, 2,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );

					g_scale_dialog.y = entry;
				}
				{
					GtkWidget* entry = gtk_entry_new();
					gtk_entry_set_text( GTK_ENTRY( entry ), "1.0" );
					gtk_widget_show( entry );
					gtk_table_attach( table, entry, 1, 2, 2, 3,
									  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
									  (GtkAttachOptions) ( 0 ), 0, 0 );

					g_scale_dialog.z = entry;
				}
			}
			{
				GtkVBox* vbox = create_dialog_vbox( 4 );
				gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE, 0 );
				{
					GtkButton* button = create_dialog_button( "OK", G_CALLBACK( scaledlg_ok ), &g_scale_dialog );
					gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
					widget_make_default( GTK_WIDGET( button ) );
					gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
				}
				{
					GtkButton* button = create_dialog_button( "Cancel", G_CALLBACK( scaledlg_cancel ), &g_scale_dialog );
					gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
					gtk_widget_add_accelerator( GTK_WIDGET( button ), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
				}
				{
					GtkButton* button = create_dialog_button( "Apply", G_CALLBACK( scaledlg_apply ), &g_scale_dialog );
					gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( button ), FALSE, FALSE, 0 );
				}
			}
		}
	}

	gtk_widget_show( GTK_WIDGET( g_scale_dialog.window ) );
}


class EntityGetSelectedPropertyValuesWalker_nonEmpty : public scene::Graph::Walker
{
PropertyValues& m_propertyvalues;
const char *m_prop;
const scene::Node* m_world;
public:
EntityGetSelectedPropertyValuesWalker_nonEmpty( const char *prop, PropertyValues& propertyvalues )
	: m_propertyvalues( propertyvalues ), m_prop( prop ), m_world( Map_FindWorldspawn( g_map ) ){
}
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	Entity* entity = Node_getEntity( path.top() );
	if ( entity != 0 ){
		if( path.top().get_pointer() != m_world ){
			if ( Instance_isSelected( instance ) || instance.childSelected() ) {
				const char* keyvalue = entity->getKeyValue( m_prop );
				if ( !string_empty( keyvalue ) && !propertyvalues_contain( m_propertyvalues, keyvalue ) ) {
					m_propertyvalues.push_back( keyvalue );
				}
			}
		}
		return false;
	}
	return true;
}
};

void Scene_EntityGetPropertyValues_nonEmpty( scene::Graph& graph, const char *prop, PropertyValues& propertyvalues ){
	graph.traverse( EntityGetSelectedPropertyValuesWalker_nonEmpty( prop, propertyvalues ) );
}

#include "preferences.h"

void Select_ConnectedEntities( bool targeting, bool targets, bool focus ){
	PropertyValues target_propertyvalues;
	PropertyValues targetname_propertyvalues;
	const char *target_prop = "target";
	const char *targetname_prop;
	if ( g_pGameDescription->mGameType == "doom3" ) {
		targetname_prop = "name";
	}
	else{
		targetname_prop = "targetname";
	}

	if( targeting ){
		Scene_EntityGetPropertyValues_nonEmpty( GlobalSceneGraph(), targetname_prop, targetname_propertyvalues );
	}
	if( targets ){
		Scene_EntityGetPropertyValues_nonEmpty( GlobalSceneGraph(), target_prop, target_propertyvalues );
	}

	if( target_propertyvalues.empty() && targetname_propertyvalues.empty() ){
		globalErrorStream() << "SelectConnectedEntities: nothing found\n";
		return;
	}

	if( !targeting || !targets ){
		GlobalSelectionSystem().setSelectedAll( false );
	}
	if ( targeting && !targetname_propertyvalues.empty() ) {
		Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), target_prop, targetname_propertyvalues );
	}
	if ( targets && !target_propertyvalues.empty() ) {
		Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), targetname_prop, target_propertyvalues );
	}
	if( focus ){
		FocusAllViews();
	}
}

void SelectConnectedEntities(){
	Select_ConnectedEntities( true, true, false );
}

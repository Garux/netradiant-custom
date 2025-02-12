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
#include "signal/isignal.h"
#include "shaderlib.h"
#include "scenelib.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include "gtkutil/clipboard.h"
#include "brushmanip.h"
#include "brush.h"
#include "patch.h"
#include "patchmanip.h"
#include "patchdialog.h"
#include "surfacedialog.h"
#include "texwindow.h"
#include "mainframe.h"
#include "camwindow.h"
#include "tools.h"
#include "grid.h"
#include "map.h"
#include "entityinspector.h"
#include "csg.h"



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
	CollectSelectedBrushesBounds( AABB* bounds, Unsigned max, Unsigned& count ) :
		m_bounds( bounds ),
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
	SelectByBounds( AABB* aabbs, Unsigned count ) :
		m_aabbs( aabbs ),
		m_count( count ){
	}

	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if( path.top().get().visible() ){
			Selectable* selectable = Instance_getSelectable( instance );

			// ignore worldspawn
			Entity* entity = Node_getEntity( path.top() );
			if ( entity != nullptr && string_equal( entity->getClassName(), "worldspawn" ) ) {
				return true;
			}

			if ( path.size() > 1
			  && !path.top().get().isRoot()
			  && selectable != 0
			  && !node_is_group( path.top() ) ) {
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
			if ( Node_isEntity( path.top() )
			     && path.top().get_pointer() != Map_FindWorldspawn( g_map ) // direct worldspawn deletion is permitted, so do find it each time
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

void Select_Delete(){
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
	mutable std::size_t m_depth = 0;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
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
		globalOutputStream() << '\n';
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
		globalOutputStream() << '\n';
		--m_depth;
	}
};
#endif

class ExpandSelectionToPrimitivesWalker : public scene::Graph::Walker
{
	mutable std::size_t m_depth = 0;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		++m_depth;

		if( !path.top().get().visible() )
			return false;

//		if ( path.top().get_pointer() == m_world ) // ignore worldspawn
//			return false;

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
	mutable std::size_t m_depth = 0;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		++m_depth;

		if( !path.top().get().visible() )
			return false;

//		if ( path.top().get_pointer() == m_world ) // ignore worldspawn
//			return false;

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
typedef FreeCaller<void(), Selection_UpdateWorkzone> SelectionUpdateWorkzoneCaller;

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

	const auto command = StringStream<64>( "textureFindReplace -find ", pFind, " -replace ", pReplace );
	UndoableCommand undo( command );

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

template<typename EntityMatcher>
class EntityFindByPropertyValueWalker : public scene::Graph::Walker
{
	const EntityMatcher& m_entityMatcher;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	EntityFindByPropertyValueWalker( const EntityMatcher& entityMatcher ) : m_entityMatcher( entityMatcher ){
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
			if( m_entityMatcher( entity ) ) {
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

template<typename EntityMatcher>
void Scene_EntitySelectByPropertyValues( scene::Graph& graph, const EntityMatcher& entityMatcher ){
	graph.traverse( EntityFindByPropertyValueWalker<EntityMatcher>( entityMatcher ) );
}

void Scene_EntitySelectByPropertyValues( scene::Graph& graph, const char *prop, const PropertyValues& propertyvalues ){
	Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), [prop, &propertyvalues]( const Entity* entity )->bool{
		return propertyvalues_contain( propertyvalues, entity->getKeyValue( prop ) );
	} );
}

class EntityGetSelectedPropertyValuesWalker : public scene::Graph::Walker
{
	PropertyValues& m_propertyvalues;
	const char *m_prop;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	EntityGetSelectedPropertyValuesWalker( const char *prop, PropertyValues& propertyvalues )
		: m_propertyvalues( propertyvalues ), m_prop( prop ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( Entity* entity = Node_getEntity( path.top() ) ){
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
		const char *prop = "classname";
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

void Select_EntitiesByKeyValue( const char* key, const char* value ){
	GlobalSelectionSystem().setSelectedAll( false );
	if( key != nullptr && value != nullptr ){
		if( !string_empty( key ) && !string_empty( value ) ){
			Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), [key, value]( const Entity* entity )->bool{
				return string_equal_nocase( entity->getKeyValue( key ), value );
			} );
		}
	}
	else if( key != nullptr ){
		if( !string_empty( key ) ){
			Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), [key]( const Entity* entity )->bool{
				return entity->hasKeyValue( key );
			} );
		}
	}
	else if( value != nullptr ){
		if( !string_empty( value ) ){
			Scene_EntitySelectByPropertyValues( GlobalSceneGraph(), [value]( const Entity* entity )->bool{
				class Visitor : public Entity::Visitor
				{
					const char* const m_value;
				public:
					bool m_found = false;
					Visitor( const char* value ) : m_value( value ){
					}
					void visit( const char* key, const char* value ){
						if ( string_equal_nocase( m_value, value ) ) {
							m_found = true;
						}
					}
				} visitor( value );
				entity->forEachKeyValue( visitor );
				return visitor.m_found;
			} );
		}
	}
}

void Select_FacesAndPatchesByShader(){
	Scene_BrushFacesSelectByShader( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
	Scene_PatchSelectByShader( GlobalSceneGraph(), TextureBrowser_GetSelectedShader() );
}

void Select_Inside(){
	SelectByBounds<SelectionPolicy_Inside>::DoSelection();
}

void Select_Touching(){
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
		Scene_PatchTileTexture_Selected( GlobalSceneGraph(), horizontal, vertical );
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

ToggleItem g_hidden_item{ BoolExportCaller( g_nodes_be_hidden ) };

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
	Select_RotateAxis( 0, -90 );
}

void Selection_Rotatey(){
	UndoableCommand undo( "rotateSelected -axis y -angle 90" );
	Select_RotateAxis( 1, 90 );
}

void Selection_Rotatez(){
	UndoableCommand undo( "rotateSelected -axis z -angle -90" );
	Select_RotateAxis( 2, -90 );
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


void Nudge( int nDim, float fNudge ){
	Vector3 translate( 0, 0, 0 );
	translate[nDim] = fNudge;

	GlobalSelectionSystem().translateSelected( translate );
}

void Selection_NudgeZ( float amount ){
	const auto command = StringStream<64>( "nudgeSelected -axis z -amount ", amount );
	UndoableCommand undo( command );

	Nudge( 2, amount );
}

void Selection_MoveDown(){
	Selection_NudgeZ( -GetGridSize() );
}

void Selection_MoveUp(){
	Selection_NudgeZ( GetGridSize() );
}


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


void Undo(){
	GlobalUndoSystem().undo();
	SceneChangeNotify();
}

void Redo(){
	GlobalUndoSystem().redo();
	SceneChangeNotify();
}

void deleteSelection(){
	if( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().countSelectedComponents() != 0 ){
		UndoableCommand undo( "deleteSelectedComponents" );
		CSG_DeleteComponents();
	}
	else{
		UndoableCommand undo( "deleteSelected" );
		Select_Delete();
	}
}

void Map_ExportSelected( TextOutputStream& ostream ){
	Map_ExportSelected( ostream, Map_getFormat( g_map ) );
}

void Map_ImportSelected( TextInputStream& istream ){
	Map_ImportSelected( istream, Map_getFormat( g_map ) );
}

void Selection_Copy(){
	clipboard_copy( Map_ExportSelected );
}

void Selection_Paste(){
	clipboard_paste( Map_ImportSelected );
}

void Copy(){
	Selection_Copy();
}

void Paste(){
	UndoableCommand undo( "paste" );

	GlobalSelectionSystem().setSelectedAll( false );
	Selection_Paste();
}

void TranslateToCamera(){
	CamWnd& camwnd = *g_pParentWnd->GetCamWnd();
	GlobalSelectionSystem().translateSelected( vector3_snapped( Camera_getOrigin( camwnd ) - GlobalSelectionSystem().getBoundsSelected().origin, GetSnapGridSize() ) );
}

void PasteToCamera(){
	GlobalSelectionSystem().setSelectedAll( false );
	UndoableCommand undo( "pasteToCamera" );
	Selection_Paste();
	TranslateToCamera();
}

void MoveToCamera(){
	UndoableCommand undo( "moveToCamera" );
	TranslateToCamera();
}



class CloneSelected : public scene::Graph::Walker
{
	const bool m_makeUnique;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	mutable std::vector<scene::Node*> m_cloned;
	CloneSelected( bool makeUnique ) : m_makeUnique( makeUnique ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.size() == 1 ) {
			return true;
		}

		if ( path.top().get_pointer() == m_world ) { // ignore worldspawn, but keep checking children
			return true;
		}

		if ( !path.top().get().isRoot() ) {
			if ( Instance_isSelected( instance ) ) {
				return false;
			}
			if( m_makeUnique && instance.childSelected() ){ /* clone selected group entity primitives to new group entity */
				return false;
			}
		}

		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.size() == 1 ) {
			return;
		}

		if ( path.top().get_pointer() == m_world ) { // ignore worldspawn
			return;
		}

		if ( !path.top().get().isRoot() ) {
			if ( Instance_isSelected( instance ) ) {
				NodeSmartReference clone( Node_Clone( path.top() ) );
				Map_gatherNamespaced( clone );
				Node_getTraversable( path.parent().get() )->insert( clone );
				m_cloned.push_back( clone.get_pointer() );
			}
			else if( m_makeUnique && instance.childSelected() ){ /* clone selected group entity primitives to new group entity */
				NodeSmartReference clone( Node_Clone_Selected( path.top() ) );
				Map_gatherNamespaced( clone );
				Node_getTraversable( path.parent().get() )->insert( clone );
				m_cloned.push_back( clone.get_pointer() );
			}
		}
	}
};

void Scene_Clone_Selected( scene::Graph& graph, bool makeUnique ){
	CloneSelected cloneSelected( makeUnique );
	graph.traverse( cloneSelected );

	Map_mergeClonedNames( makeUnique );

	/* deselect originals */
	GlobalSelectionSystem().setSelectedAll( false );
	/* select cloned */
	for( scene::Node *node : cloneSelected.m_cloned )
	{
		class walker : public scene::Traversable::Walker
		{
		public:
			bool pre( scene::Node& node ) const override {
				if( scene::Instantiable *instantiable = Node_getInstantiable( node ) ){
					class visitor : public scene::Instantiable::Visitor
					{
					public:
						void visit( scene::Instance& instance ) const override {
							Instance_setSelected( instance, true );
						}
					};

					instantiable->forEachInstance( visitor() );
				}
				return true;
			}
		};
		Node_traverseSubgraph( *node, walker() );
	}
}

enum ENudgeDirection
{
	eNudgeUp = 1,
	eNudgeDown = 3,
	eNudgeLeft = 0,
	eNudgeRight = 2,
};

struct AxisBase
{
	Vector3 x;
	Vector3 y;
	Vector3 z;
	AxisBase( const Vector3& x_, const Vector3& y_, const Vector3& z_ )
		: x( x_ ), y( y_ ), z( z_ ){
	}
};

AxisBase AxisBase_forViewType( VIEWTYPE viewtype ){
	switch ( viewtype )
	{
	case XY:
		return AxisBase( g_vector3_axis_x, g_vector3_axis_y, g_vector3_axis_z );
	case XZ:
		return AxisBase( g_vector3_axis_x, g_vector3_axis_z, g_vector3_axis_y );
	case YZ:
		return AxisBase( g_vector3_axis_y, g_vector3_axis_z, g_vector3_axis_x );
	}

	ERROR_MESSAGE( "invalid viewtype" );
	return AxisBase( Vector3( 0, 0, 0 ), Vector3( 0, 0, 0 ), Vector3( 0, 0, 0 ) );
}

Vector3 AxisBase_axisForDirection( const AxisBase& axes, ENudgeDirection direction ){
	switch ( direction )
	{
	case eNudgeLeft:
		return vector3_negated( axes.x );
	case eNudgeUp:
		return axes.y;
	case eNudgeRight:
		return axes.x;
	case eNudgeDown:
		return vector3_negated( axes.y );
	}

	ERROR_MESSAGE( "invalid direction" );
	return Vector3( 0, 0, 0 );
}

bool g_bNudgeAfterClone = false;

void NudgeSelection( ENudgeDirection direction, float fAmount, VIEWTYPE viewtype ){
	AxisBase axes( AxisBase_forViewType( viewtype ) );
	Vector3 view_direction( vector3_negated( axes.z ) );
	Vector3 nudge( vector3_scaled( AxisBase_axisForDirection( axes, direction ), fAmount ) );
	GlobalSelectionSystem().NudgeManipulator( nudge, view_direction );
}

void Selection_Clone(){
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ) {
		UndoableCommand undo( "cloneSelected" );

		Scene_Clone_Selected( GlobalSceneGraph(), false );

		if( g_bNudgeAfterClone ){
			NudgeSelection( eNudgeRight, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
			NudgeSelection( eNudgeDown, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
		}
	}
}

void Selection_Clone_MakeUnique(){
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ) {
		UndoableCommand undo( "cloneSelectedMakeUnique" );

		Scene_Clone_Selected( GlobalSceneGraph(), true );

		if( g_bNudgeAfterClone ){
			NudgeSelection( eNudgeRight, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
			NudgeSelection( eNudgeDown, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
		}
	}
}

// called when the escape key is used (either on the main window or on an inspector)
void Selection_Deselect(){
	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent ) {
		if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
			GlobalSelectionSystem().setSelectedAllComponents( false );
		}
		else
		{
			SelectionSystem_DefaultMode();
			ComponentModeChanged();
		}
	}
	else
	{
		if ( GlobalSelectionSystem().countSelectedComponents() != 0 ) {
			GlobalSelectionSystem().setSelectedAllComponents( false );
		}
		else
		{
			GlobalSelectionSystem().setSelectedAll( false );
		}
	}
}

void Scene_Clone_Selected(){
	Scene_Clone_Selected( GlobalSceneGraph(), false );
}


void Selection_NudgeUp(){
	UndoableCommand undo( "nudgeSelectedUp" );
	NudgeSelection( eNudgeUp, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
}

void Selection_NudgeDown(){
	UndoableCommand undo( "nudgeSelectedDown" );
	NudgeSelection( eNudgeDown, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
}

void Selection_NudgeLeft(){
	UndoableCommand undo( "nudgeSelectedLeft" );
	NudgeSelection( eNudgeLeft, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
}

void Selection_NudgeRight(){
	UndoableCommand undo( "nudgeSelectedRight" );
	NudgeSelection( eNudgeRight, GetGridSize(), GlobalXYWnd_getCurrentViewType() );
}



void Texdef_Rotate( float angle ){
	const auto command = StringStream<64>( "brushRotateTexture -angle ", angle );
	UndoableCommand undo( command );
	Select_RotateTexture( angle );
}
// these are actually {Anti,}Clockwise in BP mode only (AP/220 - 50/50)
// TODO is possible to make really {Anti,}Clockwise
void Texdef_RotateClockwise(){
	Texdef_Rotate( static_cast<float>( -fabs( g_si_globals.rotate ) ) );
}

void Texdef_RotateAntiClockwise(){
	Texdef_Rotate( static_cast<float>( fabs( g_si_globals.rotate ) ) );
}

void Texdef_Scale( float x, float y ){
	const auto command = StringStream<64>( "brushScaleTexture -x ", x, " -y ", y );
	UndoableCommand undo( command );
	Select_ScaleTexture( x, y );
}

void Texdef_ScaleUp(){
	Texdef_Scale( 0, g_si_globals.scale[1] );
}

void Texdef_ScaleDown(){
	Texdef_Scale( 0, -g_si_globals.scale[1] );
}

void Texdef_ScaleLeft(){
	Texdef_Scale( -g_si_globals.scale[0], 0 );
}

void Texdef_ScaleRight(){
	Texdef_Scale( g_si_globals.scale[0], 0 );
}

void Texdef_Shift( float x, float y ){
	const auto command = StringStream<64>( "brushShiftTexture -x ", x, " -y ", y );
	UndoableCommand undo( command );
	Select_ShiftTexture( x, y );
}

void Texdef_ShiftLeft(){
	Texdef_Shift( -g_si_globals.shift[0], 0 );
}

void Texdef_ShiftRight(){
	Texdef_Shift( g_si_globals.shift[0], 0 );
}

void Texdef_ShiftUp(){
	Texdef_Shift( 0, g_si_globals.shift[1] );
}

void Texdef_ShiftDown(){
	Texdef_Shift( 0, -g_si_globals.shift[1] );
}



class SnappableSnapToGridSelected : public scene::Graph::Walker
{
	float m_snap;
public:
	SnappableSnapToGridSelected( float snap )
		: m_snap( snap ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.top().get().visible() ) {
			Snappable* snappable = Node_getSnappable( path.top() );
			if ( snappable != 0
			  && Instance_isSelected( instance ) ) {
				snappable->snapto( m_snap );
			}
		}
		return true;
	}
};

void Scene_SnapToGrid_Selected( scene::Graph& graph, float snap ){
	graph.traverse( SnappableSnapToGridSelected( snap ) );
}

class ComponentSnappableSnapToGridSelected : public scene::Graph::Walker
{
	float m_snap;
public:
	ComponentSnappableSnapToGridSelected( float snap )
		: m_snap( snap ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.top().get().visible() ) {
			ComponentSnappable* componentSnappable = Instance_getComponentSnappable( instance );
			if ( componentSnappable != 0
			  && Instance_isSelected( instance ) ) {
				componentSnappable->snapComponents( m_snap );
			}
		}
		return true;
	}
};

void Scene_SnapToGrid_Component_Selected( scene::Graph& graph, float snap ){
	graph.traverse( ComponentSnappableSnapToGridSelected( snap ) );
}

void Selection_SnapToGrid(){
	const auto command = StringStream<64>( "snapSelected -grid ", GetGridSize() );
	UndoableCommand undo( command );

	if ( GlobalSelectionSystem().Mode() == SelectionSystem::eComponent && GlobalSelectionSystem().countSelectedComponents() ) {
		Scene_SnapToGrid_Component_Selected( GlobalSceneGraph(), GetGridSize() );
	}
	else
	{
		Scene_SnapToGrid_Selected( GlobalSceneGraph(), GetGridSize() );
	}
}



#include <QWidget>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include "gtkutil/spinbox.h"


class RotateDialog : public QObject
{
	QWidget *m_window{};
	QDoubleSpinBox *m_x;
	QDoubleSpinBox *m_y;
	QDoubleSpinBox *m_z;
	void construct(){
		m_window = new QWidget( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
		m_window->setWindowTitle( "Arbitrary rotation" );
		m_window->installEventFilter( this );

		auto grid = new QGridLayout( m_window );
		grid->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );

		{
			grid->addWidget( m_x = new DoubleSpinBox( -360, 360, 0, 6, 1, true ), 0, 1 );
			grid->addWidget( m_y = new DoubleSpinBox( -360, 360, 0, 6, 1, true ), 1, 1 );
			grid->addWidget( m_z = new DoubleSpinBox( -360, 360, 0, 6, 1, true ), 2, 1 );
		}
		{
			grid->addWidget( new SpinBoxLabel( "  X  ", m_x ), 0, 0 );
			grid->addWidget( new SpinBoxLabel( "  Y  ", m_y ), 1, 0 );
			grid->addWidget( new SpinBoxLabel( "  Z  ", m_z ), 2, 0 );
		}
		{
			auto buttons = new QDialogButtonBox( Qt::Orientation::Vertical );
			grid->addWidget( buttons, 0, 2, 3, 1 );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Ok ), &QPushButton::clicked, [this](){ ok(); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ), &QPushButton::clicked, [this](){ cancel(); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Apply ), &QPushButton::clicked, [this](){ apply(); } );
		}
	}
	void apply(){
		const Vector3 eulerXYZ( m_x->value(), m_y->value(), m_z->value() );

		const auto command = StringStream<64>( "rotateSelectedEulerXYZ -x ", eulerXYZ[0], " -y ", eulerXYZ[1], " -z ", eulerXYZ[2] );
		UndoableCommand undo( command );

		GlobalSelectionSystem().rotateSelected( quaternion_for_euler_xyz_degrees( eulerXYZ ) );
	}
	void cancel(){
		m_window->hide();

		m_x->setValue( 0 ); // reset to 0 on close
		m_y->setValue( 0 );
		m_z->setValue( 0 );
	}
	void ok(){
		apply();
	//	cancel();
		m_window->hide();
	}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent*>( event );
			if( keyEvent->key() == Qt::Key_Escape ){
				cancel();
				event->accept();
			}
			else if( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter ){
				ok();
				event->accept();
			}
			else if( keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Space ){
				event->accept();
			}
		}
		else if( event->type() == QEvent::Close ) {
			event->ignore();
			cancel();
			return true;
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
public:
	void show(){
		if( m_window == nullptr )
			construct();
		m_window->show();
		m_window->raise();
		m_window->activateWindow();
	}
}
g_rotate_dialog;

void DoRotateDlg(){
	g_rotate_dialog.show();
}



class ScaleDialog : public QObject
{
	QWidget *m_window{};
	QDoubleSpinBox *m_x;
	QDoubleSpinBox *m_y;
	QDoubleSpinBox *m_z;
	void construct(){
		m_window = new QWidget( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
		m_window->setWindowTitle( "Arbitrary scale" );
		m_window->installEventFilter( this );

		auto grid = new QGridLayout( m_window );
		grid->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );

		{
			grid->addWidget( m_x = new DoubleSpinBox( -32768, 32768, 1, 6, 1, false ), 0, 1 );
			grid->addWidget( m_y = new DoubleSpinBox( -32768, 32768, 1, 6, 1, false ), 1, 1 );
			grid->addWidget( m_z = new DoubleSpinBox( -32768, 32768, 1, 6, 1, false ), 2, 1 );
		}
		{
			grid->addWidget( new SpinBoxLabel( "  X  ", m_x ), 0, 0 );
			grid->addWidget( new SpinBoxLabel( "  Y  ", m_y ), 1, 0 );
			grid->addWidget( new SpinBoxLabel( "  Z  ", m_z ), 2, 0 );
		}
		{
			auto buttons = new QDialogButtonBox( Qt::Orientation::Vertical );
			grid->addWidget( buttons, 0, 2, 3, 1 );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Ok ), &QPushButton::clicked, [this](){ ok(); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Cancel ), &QPushButton::clicked, [this](){ cancel(); } );
			QObject::connect( buttons->addButton( QDialogButtonBox::StandardButton::Apply ), &QPushButton::clicked, [this](){ apply(); } );
		}
	}
	void apply(){
		const float sx = m_x->value(), sy = m_y->value(), sz = m_z->value();

		const auto command = StringStream<64>( "scaleSelected -x ", sx, " -y ", sy, " -z ", sz );
		UndoableCommand undo( command );

		Select_Scale( sx, sy, sz );
	}
	void cancel(){
		m_window->hide();

		m_x->setValue( 1 ); // reset to 1 on close
		m_y->setValue( 1 );
		m_z->setValue( 1 );
	}
	void ok(){
		apply();
	//	cancel();
		m_window->hide();
	}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent*>( event );
			if( keyEvent->key() == Qt::Key_Escape ){
				cancel();
				event->accept();
			}
			else if( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter ){
				ok();
				event->accept();
			}
			else if( keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Space ){
				event->accept();
			}
		}
		else if( event->type() == QEvent::Close ) {
			event->ignore();
			cancel();
			return true;
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
public:
	void show(){
		if( m_window == nullptr )
			construct();
		m_window->show();
		m_window->raise();
		m_window->activateWindow();
	}
}
g_scale_dialog;

void DoScaleDlg(){
	g_scale_dialog.show();
}


class EntityGetSelectedPropertyValuesWalker_nonEmpty : public scene::Graph::Walker
{
	PropertyValues& m_propertyvalues;
	const char *m_prop;
	const scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	EntityGetSelectedPropertyValuesWalker_nonEmpty( const char *prop, PropertyValues& propertyvalues )
		: m_propertyvalues( propertyvalues ), m_prop( prop ){
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



void Select_registerCommands(){
	GlobalCommands_insert( "ShowHidden", makeCallbackF( Select_ShowAllHidden ), QKeySequence( "Shift+H" ) );
	GlobalToggles_insert( "HideSelected", makeCallbackF( HideSelected ), ToggleItem::AddCallbackCaller( g_hidden_item ), QKeySequence( "H" ) );

	GlobalCommands_insert( "MirrorSelectionX", makeCallbackF( Selection_Flipx ) );
	GlobalCommands_insert( "RotateSelectionX", makeCallbackF( Selection_Rotatex ) );
	GlobalCommands_insert( "MirrorSelectionY", makeCallbackF( Selection_Flipy ) );
	GlobalCommands_insert( "RotateSelectionY", makeCallbackF( Selection_Rotatey ) );
	GlobalCommands_insert( "MirrorSelectionZ", makeCallbackF( Selection_Flipz ) );
	GlobalCommands_insert( "RotateSelectionZ", makeCallbackF( Selection_Rotatez ) );

	GlobalCommands_insert( "MirrorSelectionHorizontally", makeCallbackF( Selection_FlipHorizontally ) );
	GlobalCommands_insert( "MirrorSelectionVertically", makeCallbackF( Selection_FlipVertically ) );

	GlobalCommands_insert( "RotateSelectionClockwise", makeCallbackF( Selection_RotateClockwise ) );
	GlobalCommands_insert( "RotateSelectionAnticlockwise", makeCallbackF( Selection_RotateAnticlockwise ) );

	GlobalCommands_insert( "SelectTextured", makeCallbackF( Select_FacesAndPatchesByShader ), QKeySequence( "Ctrl+Shift+A" ) );

	GlobalCommands_insert( "Undo", makeCallbackF( Undo ), QKeySequence( "Ctrl+Z" ) );
	GlobalCommands_insert( "Redo", makeCallbackF( Redo ), QKeySequence( "Ctrl+Shift+Z" ) );
	GlobalCommands_insert( "Redo2", makeCallbackF( Redo ), QKeySequence( "Ctrl+Y" ) );
	GlobalCommands_insert( "Copy", makeCallbackF( Copy ), QKeySequence( "Ctrl+C" ) );
	GlobalCommands_insert( "Paste", makeCallbackF( Paste ), QKeySequence( "Ctrl+V" ) );
	GlobalCommands_insert( "PasteToCamera", makeCallbackF( PasteToCamera ), QKeySequence( "Shift+V" ) );
	GlobalCommands_insert( "MoveToCamera", makeCallbackF( MoveToCamera ), QKeySequence( "Ctrl+Shift+V" ) );
	GlobalCommands_insert( "CloneSelection", makeCallbackF( Selection_Clone ), QKeySequence( "Space" ) );
	GlobalCommands_insert( "CloneSelectionAndMakeUnique", makeCallbackF( Selection_Clone_MakeUnique ), QKeySequence( "Shift+Space" ) );
	GlobalCommands_insert( "DeleteSelection2", makeCallbackF( deleteSelection ), QKeySequence( "Backspace" ) );
	GlobalCommands_insert( "DeleteSelection", makeCallbackF( deleteSelection ), QKeySequence( "Z" ) );
	GlobalCommands_insert( "RepeatTransforms", makeCallbackF( +[](){ GlobalSelectionSystem().repeatTransforms(); } ), QKeySequence( "Ctrl+R" ) );
	GlobalCommands_insert( "ResetTransforms", makeCallbackF( +[](){ GlobalSelectionSystem().resetTransforms(); } ), QKeySequence( "Alt+R" ) );
//	GlobalCommands_insert( "ParentSelection", makeCallbackF( Scene_parentSelected ) );
	GlobalCommands_insert( "UnSelectSelection2", makeCallbackF( Selection_Deselect ), QKeySequence( "Escape" ) );
	GlobalCommands_insert( "UnSelectSelection", makeCallbackF( Selection_Deselect ), QKeySequence( "C" ) );
	GlobalCommands_insert( "InvertSelection", makeCallbackF( Select_Invert ), QKeySequence( "I" ) );
	GlobalCommands_insert( "SelectInside", makeCallbackF( Select_Inside ) );
	GlobalCommands_insert( "SelectTouching", makeCallbackF( Select_Touching ) );
	GlobalCommands_insert( "ExpandSelectionToPrimitives", makeCallbackF( Scene_ExpandSelectionToPrimitives ), QKeySequence( "Ctrl+E" ) );
	GlobalCommands_insert( "ExpandSelectionToEntities", makeCallbackF( Scene_ExpandSelectionToEntities ), QKeySequence( "Shift+E" ) );
	GlobalCommands_insert( "SelectConnectedEntities", makeCallbackF( SelectConnectedEntities ), QKeySequence( "Ctrl+Shift+E" ) );

	GlobalCommands_insert( "ArbitraryRotation", makeCallbackF( DoRotateDlg ), QKeySequence( "Shift+R" ) );
	GlobalCommands_insert( "ArbitraryScale", makeCallbackF( DoScaleDlg ), QKeySequence( "Ctrl+Shift+S" ) );

	GlobalCommands_insert( "SnapToGrid", makeCallbackF( Selection_SnapToGrid ), QKeySequence( "Ctrl+G" ) );

	GlobalCommands_insert( "SelectAllOfType", makeCallbackF( Select_AllOfType ), QKeySequence( "Shift+A" ) );

	GlobalCommands_insert( "TexRotateClock", makeCallbackF( Texdef_RotateClockwise ), QKeySequence( "Shift+PgDown" ) );
	GlobalCommands_insert( "TexRotateCounter", makeCallbackF( Texdef_RotateAntiClockwise ), QKeySequence( "Shift+PgUp" ) );
	GlobalCommands_insert( "TexScaleUp", makeCallbackF( Texdef_ScaleUp ), QKeySequence( "Ctrl+Up" ) );
	GlobalCommands_insert( "TexScaleDown", makeCallbackF( Texdef_ScaleDown ), QKeySequence( "Ctrl+Down" ) );
	GlobalCommands_insert( "TexScaleLeft", makeCallbackF( Texdef_ScaleLeft ), QKeySequence( "Ctrl+Left" ) );
	GlobalCommands_insert( "TexScaleRight", makeCallbackF( Texdef_ScaleRight ), QKeySequence( "Ctrl+Right" ) );
	GlobalCommands_insert( "TexShiftUp", makeCallbackF( Texdef_ShiftUp ), QKeySequence( "Shift+Up" ) );
	GlobalCommands_insert( "TexShiftDown", makeCallbackF( Texdef_ShiftDown ), QKeySequence( "Shift+Down" ) );
	GlobalCommands_insert( "TexShiftLeft", makeCallbackF( Texdef_ShiftLeft ), QKeySequence( "Shift+Left" ) );
	GlobalCommands_insert( "TexShiftRight", makeCallbackF( Texdef_ShiftRight ), QKeySequence( "Shift+Right" ) );

	GlobalCommands_insert( "MoveSelectionDOWN", makeCallbackF( Selection_MoveDown ), QKeySequence( Qt::Key_Minus + Qt::KeypadModifier ) );
	GlobalCommands_insert( "MoveSelectionUP", makeCallbackF( Selection_MoveUp ), QKeySequence( Qt::Key_Plus + Qt::KeypadModifier ) );

	GlobalCommands_insert( "SelectNudgeLeft", makeCallbackF( Selection_NudgeLeft ), QKeySequence( "Alt+Left" ) );
	GlobalCommands_insert( "SelectNudgeRight", makeCallbackF( Selection_NudgeRight ), QKeySequence( "Alt+Right" ) );
	GlobalCommands_insert( "SelectNudgeUp", makeCallbackF( Selection_NudgeUp ), QKeySequence( "Alt+Up" ) );
	GlobalCommands_insert( "SelectNudgeDown", makeCallbackF( Selection_NudgeDown ), QKeySequence( "Alt+Down" ) );
}



void SceneSelectionChange( const Selectable& selectable ){
	SceneChangeNotify();
}

SignalHandlerId Selection_boundsChanged;

#include "preferencesystem.h"

void Nudge_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Nudge selected after duplication", g_bNudgeAfterClone );
}

void Selection_construct(){
	GlobalPreferenceSystem().registerPreference( "NudgeAfterClone", BoolImportStringCaller( g_bNudgeAfterClone ), BoolExportStringCaller( g_bNudgeAfterClone ) );

	PreferencesDialog_addSettingsPreferences( makeCallbackF( Nudge_constructPreferences ) );

	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), SceneSelectionChange>() );
	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), UpdateWorkzone_ForSelectionChanged>() );
	Selection_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback( FreeCaller<void(), UpdateWorkzone_ForSelection>() );
}

void Selection_destroy(){
	GlobalSceneGraph().removeBoundsChangedCallback( Selection_boundsChanged );
}

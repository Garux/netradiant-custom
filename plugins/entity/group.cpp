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

///\file
///\brief Represents any entity which does not have a fixed size specified in its entity-definition (except misc_model).
///
/// This entity behaves as a group, i.e. it contains brushes.

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"

#include "targetable.h"
#include "origin.h"
#include "angles.h"
#include "scale.h"
#include "filters.h"
#include "namedentity.h"
#include "keyobservers.h"
#include "namekeys.h"

#include "entity.h"

/// The "origin" key directly controls the entity's local-to-parent transform.

const char EXCLUDE_NAME[] = "worldspawn";

class Group
{
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;
	TraversableNodeSet m_traverse;

	ClassnameFilter m_filter;
	NamedEntity m_named;
	NameKeys m_nameKeys;

	OriginKey m_originKey;
	Vector3 m_origin;

	mutable Vector3 m_name_origin;
	RenderableNamedEntity m_renderName;

	AnglesKey m_anglesKey;
	RenderableArrow m_arrow;
	bool m_anglesDraw;
	void updateAnglesDraw(){
		m_anglesDraw = m_entity.getEntityClass().has_angles || m_entity.hasKeyValue( "angle" ) || m_entity.hasKeyValue( "angles" );
		SceneChangeNotify();
	}
	typedef MemberCaller<Group, void(), &Group::updateAnglesDraw> UpdateAnglesDrawCaller;

	Callback<void()> m_transformChanged;
	Callback<void()> m_evaluateTransform;

	void construct(){
		m_keyObservers.insert( "classname", ClassnameFilter::ClassnameChangedCaller( m_filter ) );
		m_keyObservers.insert( Static<KeyIsName>::instance().m_nameKey, NamedEntity::IdentifierChangedCaller( m_named ) );
		m_keyObservers.insert( "origin", OriginKey::OriginChangedCaller( m_originKey ) );
		m_keyObservers.insert( "angle", m_anglesKey.getGroupAngleChangedCallback() );
		m_keyObservers.insert( "angles", m_anglesKey.getAnglesChangedCallback() );
		updateAnglesDraw();
	}

public:
	Group( EntityClass* eclass, scene::Node& node, const Callback<void()>& transformChanged, const Callback<void()>& evaluateTransform ) :
		m_entity( eclass ),
		m_filter( m_entity, node ),
		m_named( m_entity ),
		m_nameKeys( m_entity ),
		m_originKey( OriginChangedCaller( *this ) ),
		m_origin( ORIGINKEY_IDENTITY ),
		m_name_origin( g_vector3_identity ),
		m_renderName( m_named, m_name_origin, EXCLUDE_NAME ),
		m_anglesKey( UpdateAnglesDrawCaller( *this ), m_entity ),
		m_arrow( m_name_origin, m_anglesKey.m_angles ),
		m_transformChanged( transformChanged ),
		m_evaluateTransform( evaluateTransform ){
		construct();
	}
	Group( const Group& other, scene::Node& node, const Callback<void()>& transformChanged, const Callback<void()>& evaluateTransform ) :
		m_entity( other.m_entity ),
		m_filter( m_entity, node ),
		m_named( m_entity ),
		m_nameKeys( m_entity ),
		m_originKey( OriginChangedCaller( *this ) ),
		m_origin( ORIGINKEY_IDENTITY ),
		m_name_origin( g_vector3_identity ),
		m_renderName( m_named, m_name_origin, EXCLUDE_NAME ),
		m_anglesKey( UpdateAnglesDrawCaller( *this ), m_entity ),
		m_arrow( m_name_origin, m_anglesKey.m_angles ),
		m_transformChanged( transformChanged ),
		m_evaluateTransform( evaluateTransform ){
		construct();
	}

	InstanceCounter m_instanceCounter;
	void instanceAttach( const scene::Path& path ){
		if ( ++m_instanceCounter.m_count == 1 ) {
			m_filter.instanceAttach();
			m_entity.instanceAttach( path_find_mapfile( path.begin(), path.end() ) );
			m_traverse.instanceAttach( path_find_mapfile( path.begin(), path.end() ) );
			m_entity.attach( m_keyObservers );
		}
	}
	void instanceDetach( const scene::Path& path ){
		if ( --m_instanceCounter.m_count == 0 ) {
			m_entity.detach( m_keyObservers );
			m_traverse.instanceDetach( path_find_mapfile( path.begin(), path.end() ) );
			m_entity.instanceDetach( path_find_mapfile( path.begin(), path.end() ) );
			m_filter.instanceDetach();
		}
	}

	EntityKeyValues& getEntity(){
		return m_entity;
	}
	const EntityKeyValues& getEntity() const {
		return m_entity;
	}

	scene::Traversable& getTraversable(){
		return m_traverse;
	}
	Namespaced& getNamespaced(){
		return m_nameKeys;
	}
	Nameable& getNameable(){
		return m_named;
	}
	TransformNode& getTransformNode(){
		return m_transform;
	}

	void attach( scene::Traversable::Observer* observer ){
		m_traverse.attach( observer );
	}
	void detach( scene::Traversable::Observer* observer ){
		m_traverse.detach( observer );
	}

	void renderSolid( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected, bool childSelected, const AABB& childBounds ) const {
		if ( m_renderName.excluded_not() ) {
			// place name in the middle of the "children cloud"
			m_name_origin = extents_valid( childBounds.extents.x() )? childBounds.origin : localToWorld.t().vec3();

			if ( selected || childSelected || g_showNames )
				m_renderName.render( renderer, volume, g_matrix4_identity, selected, childSelected );
			if ( m_anglesDraw && g_showAngles ) {
				if( selected || childSelected ){
					renderer.PushState();
					renderer.Highlight( Renderer::ePrimitive );
					renderer.SetState( m_entity.getEntityClass().m_state_fill, Renderer::eFullMaterials );
					renderer.addRenderable( m_arrow, g_matrix4_identity );
					renderer.PopState();
				}
				else{
					renderer.SetState( m_entity.getEntityClass().m_state_fill, Renderer::eFullMaterials );
					renderer.addRenderable( m_arrow, g_matrix4_identity );
				}
			}
		}
	}

	void renderWireframe( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected, bool childSelected, const AABB& childBounds ) const {
		renderer.SetState( m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly );
		// don't draw the name for worldspawn
//		if ( !strcmp( m_entity.getEntityClass().name(), "worldspawn" ) ) {
//			return;
//		}
		if ( m_renderName.excluded_not() ) {
			// place name in the middle of the "children cloud"
			m_name_origin = extents_valid( childBounds.extents.x() )? childBounds.origin : localToWorld.t().vec3();

			if ( selected || childSelected || ( g_showNames && aabb_fits_view( childBounds, volume.GetModelview(), volume.GetViewport(), g_showNamesRatio ) ) )
				m_renderName.render( renderer, volume, g_matrix4_identity, selected, childSelected );
			if ( m_anglesDraw && g_showAngles ) {
				if( selected || childSelected ){
					renderer.PushState();
					renderer.Highlight( Renderer::ePrimitive );
					renderer.addRenderable( m_arrow, g_matrix4_identity );
					renderer.PopState();
				}
				else{
					renderer.addRenderable( m_arrow, g_matrix4_identity );
				}
			}
		}
	}

	void updateTransform(){
		m_transform.localToParent() = g_matrix4_identity;
		matrix4_translate_by_vec3( m_transform.localToParent(), m_origin );
		m_transformChanged();
	}
	typedef MemberCaller<Group, void(), &Group::updateTransform> UpdateTransformCaller;
	void originChanged(){
		m_origin = m_originKey.m_origin;
		updateTransform();
	}
	typedef MemberCaller<Group, void(), &Group::originChanged> OriginChangedCaller;

	void translate( const Vector3& translation ){
		m_origin = origin_translated( m_origin, translation );
	}

	void revertTransform(){
		m_origin = m_originKey.m_origin;
	}
	void freezeTransform(){
		m_originKey.m_origin = m_origin;
		m_originKey.write( &m_entity );
	}
	void transformChanged(){
		revertTransform();
		m_evaluateTransform();
		updateTransform();
	}
	typedef MemberCaller<Group, void(), &Group::transformChanged> TransformChangedCaller;
};

#if 0
class TransformableSetTranslation
{
	Translation m_value;
public:
	TransformableSetTranslation( const Translation& value ) : m_value( value ){
	}
	void operator()( Transformable& transformable ) const {
		transformable.setTranslation( m_value );
	}
};

class TransformableSetRotation
{
	Rotation m_value;
public:
	TransformableSetRotation( const Rotation& value ) : m_value( value ){
	}
	void operator()( Transformable& transformable ) const {
		transformable.setRotation( m_value );
	}
};

class TransformableSetScale
{
	Scale m_value;
public:
	TransformableSetScale( const Scale& value ) : m_value( value ){
	}
	void operator()( Transformable& transformable ) const {
		transformable.setScale( m_value );
	}
};

class TransformableSetType
{
	TransformModifierType m_value;
public:
	TransformableSetType( const TransformModifierType& value ) : m_value( value ){
	}
	void operator()( Transformable& transformable ) const {
		transformable.setType( m_value );
	}
};

class TransformableFreezeTransform
{
	TransformModifierType m_value;
public:
	void operator()( Transformable& transformable ) const {
		transformable.freezeTransform();
	}
};

template<typename Functor>
inline void Scene_forEachChildTransformable( const Functor& functor, const scene::Path& path ){
	GlobalSceneGraph().traverse_subgraph( ChildInstanceWalker< InstanceApply<Transformable, Functor> >( functor ), path );
}
#endif

class GroupInstance :
	public TargetableInstance,
	public TransformModifier,
#if 0
	public Transformable,
#endif
	public Renderable
{
	class TypeCasts
	{
		InstanceTypeCastTable m_casts;
	public:
		TypeCasts(){
			m_casts = TargetableInstance::StaticTypeCasts::instance().get();
			InstanceStaticCast<GroupInstance, Renderable>::install( m_casts );
#if 0
			InstanceStaticCast<GroupInstance, Transformable>::install( m_casts );
#endif
		}
		InstanceTypeCastTable& get(){
			return m_casts;
		}
	};

	Group& m_contained;
public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	GroupInstance( const scene::Path& path, scene::Instance* parent, Group& group ) :
		TargetableInstance( path, parent, this, StaticTypeCasts::instance().get(), group.getEntity(), *this ),
		TransformModifier( Group::TransformChangedCaller( group ), ApplyTransformCaller( *this ) ),
		m_contained( group ){
		m_contained.instanceAttach( Instance::path() );
		StaticRenderableConnectionLines::instance().attach( *this );
	}
	~GroupInstance(){
		StaticRenderableConnectionLines::instance().detach( *this );
		m_contained.instanceDetach( Instance::path() );
	}
	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const {
		m_contained.renderSolid( renderer, volume, Instance::localToWorld(), getSelectable().isSelected(), Instance::childSelected(), Instance::childBounds() );
	}
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const {
		m_contained.renderWireframe( renderer, volume, Instance::localToWorld(), getSelectable().isSelected(), Instance::childSelected(), Instance::childBounds() );
	}

	STRING_CONSTANT( Name, "GroupInstance" );

#if 0
	void setType( TransformModifierType type ){
		Scene_forEachChildTransformable( TransformableSetType( type ), Instance::path() );
	}
	void setTranslation( const Translation& value ){
		Scene_forEachChildTransformable( TransformableSetTranslation( value ), Instance::path() );
	}
	void setRotation( const Rotation& value ){
		Scene_forEachChildTransformable( TransformableSetRotation( value ), Instance::path() );
	}
	void setScale( const Scale& value ){
		Scene_forEachChildTransformable( TransformableSetScale( value ), Instance::path() );
	}
	void freezeTransform(){
		Scene_forEachChildTransformable( TransformableFreezeTransform(), Instance::path() );
	}

	void evaluateTransform(){
	}
#endif

	void evaluateTransform(){
		if ( getType() == TRANSFORM_PRIMITIVE ) {
			m_contained.translate( getTranslation() );
		}
	}
	void applyTransform(){
		m_contained.revertTransform();
		evaluateTransform();
		m_contained.freezeTransform();
	}
	typedef MemberCaller<GroupInstance, void(), &GroupInstance::applyTransform> ApplyTransformCaller;
};

class GroupNode :
	public scene::Node::Symbiot,
	public scene::Instantiable,
	public scene::Cloneable,
	public scene::Traversable::Observer
{
	class TypeCasts
	{
		NodeTypeCastTable m_casts;
	public:
		TypeCasts(){
			NodeStaticCast<GroupNode, scene::Instantiable>::install( m_casts );
			NodeStaticCast<GroupNode, scene::Cloneable>::install( m_casts );
			NodeContainedCast<GroupNode, scene::Traversable>::install( m_casts );
			NodeContainedCast<GroupNode, TransformNode>::install( m_casts );
			NodeContainedCast<GroupNode, Entity>::install( m_casts );
			NodeContainedCast<GroupNode, Nameable>::install( m_casts );
			NodeContainedCast<GroupNode, Namespaced>::install( m_casts );
		}
		NodeTypeCastTable& get(){
			return m_casts;
		}
	};


	scene::Node m_node;
	InstanceSet m_instances;
	Group m_contained;

	void construct(){
		m_contained.attach( this );
	}
	void destroy(){
		m_contained.detach( this );
	}

public:

	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	scene::Traversable& get( NullType<scene::Traversable>){
		return m_contained.getTraversable();
	}
	TransformNode& get( NullType<TransformNode>){
		return m_contained.getTransformNode();
	}
	Entity& get( NullType<Entity>){
		return m_contained.getEntity();
	}
	Nameable& get( NullType<Nameable>){
		return m_contained.getNameable();
	}
	Namespaced& get( NullType<Namespaced>){
		return m_contained.getNamespaced();
	}

	GroupNode( EntityClass* eclass ) :
		m_node( this, this, StaticTypeCasts::instance().get() ),
		m_contained( eclass, m_node, InstanceSet::TransformChangedCaller( m_instances ), InstanceSetEvaluateTransform<GroupInstance>::Caller( m_instances ) ){
		construct();
	}
	GroupNode( const GroupNode& other ) :
		scene::Node::Symbiot( other ),
		scene::Instantiable( other ),
		scene::Cloneable( other ),
		scene::Traversable::Observer( other ),
		m_node( this, this, StaticTypeCasts::instance().get() ),
		m_contained( other.m_contained, m_node, InstanceSet::TransformChangedCaller( m_instances ), InstanceSetEvaluateTransform<GroupInstance>::Caller( m_instances ) ){
		construct();
	}
	~GroupNode(){
		destroy();
	}

	void release(){
		delete this;
	}
	scene::Node& node(){
		return m_node;
	}

	scene::Node& clone() const {
		return ( new GroupNode( *this ) )->node();
	}

	void insert( scene::Node& child ){
		m_instances.insert( child );
	}
	void erase( scene::Node& child ){
		m_instances.erase( child );
	}

	scene::Instance* create( const scene::Path& path, scene::Instance* parent ){
		return new GroupInstance( path, parent, m_contained );
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

scene::Node& New_Group( EntityClass* eclass ){
	return ( new GroupNode( eclass ) )->node();
}

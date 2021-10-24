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

#include "modelwindow.h"

#include <set>
#include <deque>
#include "ifiletypes.h"
#include "ifilesystem.h"
#include "iarchive.h"
#include "imodel.h"
#include "igl.h"
#include "irender.h"
#include "renderable.h"
#include "render.h"
#include "renderer.h"
#include "view.h"
#include "os/path.h"
#include "string/string.h"
#include "stringio.h"
#include "stream/stringstream.h"
#include "generic/callback.h"

#include <gtk/gtk.h>
#include "gtkutil/glwidget.h"
#include "gtkutil/button.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/cursor.h"
#include "gtkmisc.h"

#include "mainframe.h"
#include "camwindow.h"
#include "grid.h"
#include "instancelib.h"
#include "traverselib.h"
#include "selectionlib.h"


/* specialized copy of class CompiledGraph */
class ModelGraph final : public scene::Graph, public scene::Instantiable::Observer
{
	typedef std::map<PathConstReference, scene::Instance*> InstanceMap;

	InstanceMap m_instances;
	scene::Path m_rootpath;

	scene::Instantiable::Observer& m_observer;

public:

	ModelGraph( scene::Instantiable::Observer& observer ) : m_observer( observer ){
	}

	void addSceneChangedCallback( const SignalHandler& handler ){
		ASSERT_MESSAGE( 0, "Reached unreachable: addSceneChangedCallback()" );
	}
	void sceneChanged(){
		ASSERT_MESSAGE( 0, "Reached unreachable: sceneChanged()" );
	}

	scene::Node& root(){
		ASSERT_MESSAGE( !m_rootpath.empty(), "scenegraph root does not exist" );
		return m_rootpath.top();
	}
	void insert_root( scene::Node& root ){
		//globalOutputStream() << "insert_root\n";

		ASSERT_MESSAGE( m_rootpath.empty(), "scenegraph root already exists" );

		root.IncRef();

		Node_traverseSubgraph( root, InstanceSubgraphWalker( this, scene::Path(), 0 ) );

		m_rootpath.push( makeReference( root ) );
	}
	void erase_root(){
		//globalOutputStream() << "erase_root\n";

		ASSERT_MESSAGE( !m_rootpath.empty(), "scenegraph root does not exist" );

		scene::Node& root = m_rootpath.top();

		m_rootpath.pop();

		Node_traverseSubgraph( root, UninstanceSubgraphWalker( this, scene::Path() ) );

		root.DecRef();
	}
	void boundsChanged(){
		ASSERT_MESSAGE( 0, "Reached unreachable: boundsChanged()" );
	}

	void traverse( const Walker& walker ){
		ASSERT_MESSAGE( 0, "Reached unreachable: traverse()" );
	}

	void traverse_subgraph( const Walker& walker, const scene::Path& start ){
		ASSERT_MESSAGE( 0, "Reached unreachable: traverse_subgraph()" );
	}

	scene::Instance* find( const scene::Path& path ){
		ASSERT_MESSAGE( 0, "Reached unreachable: find()" );
		return nullptr;
	}

	void insert( scene::Instance* instance ){
		m_instances.insert( InstanceMap::value_type( PathConstReference( instance->path() ), instance ) );
		m_observer.insert( instance );
	}
	void erase( scene::Instance* instance ){
		m_instances.erase( PathConstReference( instance->path() ) );
		m_observer.erase( instance );
	}

	SignalHandlerId addBoundsChangedCallback( const SignalHandler& boundsChanged ){
		ASSERT_MESSAGE( 0, "Reached unreachable: addBoundsChangedCallback()" );
		return Handle<Opaque<SignalHandler>>( nullptr );
	}
	void removeBoundsChangedCallback( SignalHandlerId id ){
		ASSERT_MESSAGE( 0, "Reached unreachable: removeBoundsChangedCallback()" );
	}

	TypeId getNodeTypeId( const char* name ){
		ASSERT_MESSAGE( 0, "Reached unreachable: getNodeTypeId()" );
		return 0;
	}

	TypeId getInstanceTypeId( const char* name ){
		ASSERT_MESSAGE( 0, "Reached unreachable: getInstanceTypeId()" );
		return 0;
	}

	void clear(){
		DeleteSubgraph( root() );
	}

};

/* specialized copy of class TraversableNodeSet */
/// \brief A sequence of node references which notifies an observer of inserts and deletions, and uses the global undo system to provide undo for modifications.
class TraversableModelNodeSet : public scene::Traversable
{
	UnsortedNodeSet m_children;
	Observer* m_observer;

	void copy( const TraversableModelNodeSet& other ){
		m_children = other.m_children;
	}
	void notifyInsertAll(){
		if ( m_observer ) {
			for ( UnsortedNodeSet::iterator i = m_children.begin(); i != m_children.end(); ++i )
			{
				m_observer->insert( *i );
			}
		}
	}
	void notifyEraseAll(){
		if ( m_observer ) {
			for ( UnsortedNodeSet::iterator i = m_children.begin(); i != m_children.end(); ++i )
			{
				m_observer->erase( *i );
			}
		}
	}
public:
	TraversableModelNodeSet()
		: m_observer( 0 ){
	}
	TraversableModelNodeSet( const TraversableModelNodeSet& other )
		: scene::Traversable( other ), m_observer( 0 ){
		copy( other );
		notifyInsertAll();
	}
	~TraversableModelNodeSet(){
		notifyEraseAll();
	}
	TraversableModelNodeSet& operator=( const TraversableModelNodeSet& other ){
#if 1 // optimised change-tracking using diff algorithm
		if ( m_observer ) {
			nodeset_diff( m_children, other.m_children, m_observer );
		}
		copy( other );
#else
		TraversableModelNodeSet tmp( other );
		tmp.swap( *this );
#endif
		return *this;
	}
	void swap( TraversableModelNodeSet& other ){
		std::swap( m_children, other.m_children );
		std::swap( m_observer, other.m_observer );
	}

	void attach( Observer* observer ){
		ASSERT_MESSAGE( m_observer == 0, "TraversableModelNodeSet::attach: observer cannot be attached" );
		m_observer = observer;
		notifyInsertAll();
	}
	void detach( Observer* observer ){
		ASSERT_MESSAGE( m_observer == observer, "TraversableModelNodeSet::detach: observer cannot be detached" );
		notifyEraseAll();
		m_observer = 0;
	}
/// \brief \copydoc scene::Traversable::insert()
	void insert( scene::Node& node ){
		ASSERT_MESSAGE( (volatile intptr_t)&node != 0, "TraversableModelNodeSet::insert: sanity check failed" );

		ASSERT_MESSAGE( m_children.find( NodeSmartReference( node ) ) == m_children.end(), "TraversableModelNodeSet::insert - element already exists" );

		m_children.insert( NodeSmartReference( node ) );

		if ( m_observer ) {
			m_observer->insert( node );
		}
	}
/// \brief \copydoc scene::Traversable::erase()
	void erase( scene::Node& node ){
		ASSERT_MESSAGE( (volatile intptr_t)&node != 0, "TraversableModelNodeSet::erase: sanity check failed" );

		ASSERT_MESSAGE( m_children.find( NodeSmartReference( node ) ) != m_children.end(), "TraversableModelNodeSet::erase - failed to find element" );

		if ( m_observer ) {
			m_observer->erase( node );
		}

		m_children.erase( NodeSmartReference( node ) );
	}
/// \brief \copydoc scene::Traversable::traverse()
	void traverse( const Walker& walker ){
		UnsortedNodeSet::iterator i = m_children.begin();
		while ( i != m_children.end() )
		{
			// post-increment the iterator
			Node_traverseSubgraph( *i++, walker );
			// the Walker can safely remove the current node from
			// this container without invalidating the iterator
		}
	}
/// \brief \copydoc scene::Traversable::empty()
	bool empty() const {
		return m_children.empty();
	}
};

/* specialized copy of class MapRoot */
class ModelGraphRoot : public scene::Node::Symbiot, public scene::Instantiable, public scene::Traversable::Observer
{
	class TypeCasts
	{
		NodeTypeCastTable m_casts;
	public:
		TypeCasts(){
			NodeStaticCast<ModelGraphRoot, scene::Instantiable>::install( m_casts );
			NodeContainedCast<ModelGraphRoot, scene::Traversable>::install( m_casts );
			NodeContainedCast<ModelGraphRoot, TransformNode>::install( m_casts );
		}
		NodeTypeCastTable& get(){
			return m_casts;
		}
	};

	scene::Node m_node;
	IdentityTransform m_transform;
	TraversableModelNodeSet m_traverse;
	InstanceSet m_instances;
public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	scene::Traversable& get( NullType<scene::Traversable>){
		return m_traverse;
	}
	TransformNode& get( NullType<TransformNode>){
		return m_transform;
	}

	ModelGraphRoot() : m_node( this, this, StaticTypeCasts::instance().get() ){
		m_node.m_isRoot = true;

		m_traverse.attach( this );
	}
	~ModelGraphRoot(){
	}
	void release(){
		m_traverse.detach( this );
		delete this;
	}
	scene::Node& node(){
		return m_node;
	}

	void insert( scene::Node& child ){
		m_instances.insert( child );
	}
	void erase( scene::Node& child ){
		m_instances.erase( child );
	}

	scene::Node& clone() const {
		return ( new ModelGraphRoot( *this ) )->node();
	}

	scene::Instance* create( const scene::Path& path, scene::Instance* parent ){
		return new SelectableInstance( path, parent );
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





#include "../plugins/entity/model.h"

class ModelNode :
	public scene::Node::Symbiot,
	public scene::Instantiable,
	public scene::Traversable::Observer
{
	class TypeCasts
	{
		NodeTypeCastTable m_casts;
	public:
		TypeCasts(){
			NodeStaticCast<ModelNode, scene::Instantiable>::install( m_casts );
			NodeContainedCast<ModelNode, scene::Traversable>::install( m_casts );
			NodeContainedCast<ModelNode, TransformNode>::install( m_casts );
		}
		NodeTypeCastTable& get(){
			return m_casts;
		}
	};


	scene::Node m_node;
	InstanceSet m_instances;
	SingletonModel m_model;
	MatrixTransform m_transform;

	void construct(){
		m_model.attach( this );
	}
	void destroy(){
		m_model.detach( this );
	}

public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	scene::Traversable& get( NullType<scene::Traversable>){
		return m_model.getTraversable();
	}
	TransformNode& get( NullType<TransformNode>){
		return m_transform;
	}

	ModelNode() :
		m_node( this, this, StaticTypeCasts::instance().get() ){
		construct();
	}
	ModelNode( const ModelNode& other ) :
		scene::Node::Symbiot( other ),
		scene::Instantiable( other ),
		scene::Traversable::Observer( other ),
		m_node( this, this, StaticTypeCasts::instance().get() ){
		construct();
	}
	~ModelNode(){
		destroy();
	}

	void release(){
		delete this;
	}
	scene::Node& node(){
		return m_node;
	}

	void insert( scene::Node& child ){
		m_instances.insert( child );
	}
	void erase( scene::Node& child ){
		m_instances.erase( child );
	}

	scene::Instance* create( const scene::Path& path, scene::Instance* parent ){
		return new SelectableInstance( path, parent );
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

	void setModel( const char* modelname ){
		m_model.modelChanged( modelname );
	}
};


ModelGraph* g_modelGraph = nullptr;





void ModelGraph_clear(){
	g_modelGraph->clear();
}




class ModelFS
{
public:
	const CopiedString m_folderName;
	ModelFS() = default;
	ModelFS( const StringRange range ) : m_folderName( range ){
	}
	bool operator<( const ModelFS& other ) const {
		return string_less( m_folderName.c_str(), other.m_folderName.c_str() );
	}
	mutable std::set<ModelFS> m_folders;
	mutable std::set<CopiedString> m_files;
	void insert( const char* filepath ) const {
		const char* slash = strchr( filepath, '/' );
		if( slash == nullptr ){
			m_files.emplace( filepath );
		}
		else{
			m_folders.emplace( StringRange( filepath, slash ) ).first->insert( slash + 1 );
		}
	}
};

class CellPos
{
	const int m_cellSize; //half size of model square (radius)
	const int m_fontHeight;
	const int m_fontDescent;
	const int m_plusWidth; //pre offset on the left
	const int m_plusHeight; //above text
	const int m_cellsInRow;

	int m_index = 0;
public:
	CellPos( int width, int cellSize, int fontHeight ) :
		m_cellSize( cellSize ), m_fontHeight( fontHeight ),
		m_fontDescent( GlobalOpenGL().m_font->getPixelDescent() ),
		m_plusWidth( 8 ),
		m_plusHeight( 0 ),
		m_cellsInRow( std::max( 1, ( width - m_plusWidth ) / ( m_cellSize * 2 + m_plusWidth ) ) ){
	}
	void operator++(){
		++m_index;
	}
	Vector3 getOrigin( int index ) const { // origin of model square
		const int x = ( index % m_cellsInRow ) * m_cellSize * 2 + m_cellSize + ( index % m_cellsInRow + 1 ) * m_plusWidth;
		const int z = ( index / m_cellsInRow ) * m_cellSize * 2 + m_cellSize + ( index / m_cellsInRow + 1 ) * ( m_fontHeight + m_plusHeight );
		return Vector3( x, 0, -z );
	}
	Vector3 getOrigin() const { // origin of model square
		return getOrigin( m_index );
	}
	Vector3 getTextPos( int index ) const {
		const int x = ( index % m_cellsInRow ) * m_cellSize * 2 + ( index % m_cellsInRow + 1 ) * m_plusWidth;
		const int z = ( index / m_cellsInRow ) * m_cellSize * 2 + ( index / m_cellsInRow + 1 ) * ( m_fontHeight + m_plusHeight ) - 1 + m_fontDescent;
		return Vector3( x, 0, -z );
	}
	Vector3 getTextPos() const {
		return getTextPos( m_index );
	}
	int getCellSize() const {
		return m_cellSize;
	}
	int totalHeight( int height, int cellCount ) const {
		return std::max( height, ( ( cellCount - 1 ) / m_cellsInRow + 1 ) * ( m_cellSize * 2 + m_fontHeight + m_plusHeight ) + m_fontHeight );
	}
	int testSelect( int x, int z ) const { // index of cell at ( x, z )
		return std::min( m_cellsInRow - 1, ( x / ( m_cellSize * 2 + m_plusWidth ) ) ) + ( m_cellsInRow * ( z / ( m_cellSize * 2 + m_fontHeight + m_plusHeight ) ) );
	}
};

void ModelBrowser_scrollChanged( void* data, gdouble value );

class ModelBrowser : public scene::Instantiable::Observer
{
	// track instances in the order of insertion
	std::vector<scene::Instance*> m_modelInstances;
public:
	ModelFS m_modelFS;
	CopiedString m_prefFoldersToLoad = "*models/99*";
	ModelBrowser() : m_scrollAdjustment( ModelBrowser_scrollChanged, this ){
	}
	~ModelBrowser(){
	}

	FBO* m_fbo = nullptr;
	FBO* fbo_get(){
		return m_fbo = m_fbo? m_fbo : GlobalOpenGL().support_ARB_framebuffer_object? new FBO : new FBO_fallback;
	}
	const int m_MSAA = 8;

	GtkWindow* m_parent = nullptr;
	GtkWidget* m_gl_widget = nullptr;
	GtkWidget* m_gl_scroll = nullptr;
	GtkWidget* m_treeViewTree = nullptr;

	guint m_sizeHandler;
	guint m_exposeHandler;
	int m_width;
	int m_height;

	int m_originZ = 0; // <= 0
	DeferredAdjustment m_scrollAdjustment;

	int m_cellSize = 80;

	CopiedString m_currentFolderPath;
	const ModelFS* m_currentFolder = nullptr;
	int m_currentModelId = -1; // selected model index in m_modelInstances, m_currentFolder->m_files; these must be in sync!

	CellPos constructCellPos() const {
		return CellPos( m_width, m_cellSize, GlobalOpenGL().m_font->getPixelHeight() );
	}
	void testSelect( int x, int z ){
		m_currentModelId = constructCellPos().testSelect( x, z - m_originZ );
		if( m_currentModelId >= static_cast<int>( m_modelInstances.size() ) )
			m_currentModelId = -1;
	}
private:
	int totalHeight() const {
		return constructCellPos().totalHeight( m_height, m_modelInstances.size() );
	}
	void updateScroll() const {
		GtkAdjustment *vadjustment = gtk_range_get_adjustment( GTK_RANGE( m_gl_scroll ) );

		gtk_adjustment_set_value( vadjustment, -m_originZ );
		gtk_adjustment_set_page_size( vadjustment, m_height );
		gtk_adjustment_set_page_increment( vadjustment, m_height / 2 );
		gtk_adjustment_set_step_increment( vadjustment, 20 );
		gtk_adjustment_set_lower( vadjustment, 0 );
		gtk_adjustment_set_upper( vadjustment, totalHeight() );

		g_signal_emit_by_name( G_OBJECT( vadjustment ), "changed" );
	}
public:
	void setOriginZ( int origin ){
		m_originZ = origin;
		m_originInvalid = true;
		queueDraw();
	}
	void queueDraw() const {
		if ( m_gl_widget != nullptr )
			gtk_widget_queue_draw( m_gl_widget );
	}
	bool m_originInvalid = true;
	void validate(){
		if( m_originInvalid ){
			m_originInvalid = false;
			const int lowest = std::min( m_height - totalHeight(), 0 );
			m_originZ = std::max( lowest, std::min( m_originZ, 0 ) );
			updateScroll();
		}
	}

private:
	FreezePointer m_freezePointer;
	bool m_move_started = false;
public:
	int m_move_amount;
	static void trackingDelta( int x, int y, unsigned int state, void* data ){
		ModelBrowser& modelBrowser = *reinterpret_cast<ModelBrowser*>( data );
		modelBrowser.m_move_amount += std::abs( x ) + std::abs( y );
		if ( ( state & GDK_BUTTON3_MASK ) && y != 0 ) { // scroll view
			const int scale = ( state & GDK_SHIFT_MASK )? 4 : 1;
			modelBrowser.setOriginZ( modelBrowser.m_originZ + y * scale );
		}
		else if ( ( state & GDK_BUTTON1_MASK ) && ( x != 0 || y != 0 ) && modelBrowser.m_currentModelId >= 0 ) { // rotate selected model
			ASSERT_MESSAGE( modelBrowser.m_currentModelId < static_cast<int>( modelBrowser.m_modelInstances.size() ), "modelBrowser.m_currentModelId out of range" );
			scene::Instance *instance = modelBrowser.m_modelInstances[modelBrowser.m_currentModelId];
			if( TransformNode *transformNode = Node_getTransformNode( instance->path().parent() ) ){
				Matrix4 rot( g_matrix4_identity );
				matrix4_pivoted_rotate_by_euler_xyz_degrees( rot, Vector3( y, 0, x ) * ( 45.f / modelBrowser.m_cellSize ), modelBrowser.constructCellPos().getOrigin( modelBrowser.m_currentModelId ) );
				matrix4_premultiply_by_matrix4( const_cast<Matrix4&>( transformNode->localToParent() ), rot );
				instance->parent()->transformChangedLocal();
				instance->transformChangedLocal();
				modelBrowser.queueDraw();
			}
		}
	}
	void tracking_MouseUp(){
		if( m_move_started ){
			m_move_started = false;
			m_freezePointer.unfreeze_pointer( false );
		}
	}
	void tracking_MouseDown(){
		tracking_MouseUp();
		m_move_started = true;
		m_move_amount = 0;
		m_freezePointer.freeze_pointer( m_parent, m_gl_widget, trackingDelta, this );
	}

	void insert( scene::Instance* instance ) override {
		if( instance->path().size() == 3 ){
			m_modelInstances.push_back( instance );
			m_originZ = 0;
			m_originInvalid = true;
		}
	}
	void erase( scene::Instance* instance ) override { // just invalidate everything (also happens on resource flush and refresh) //FIXME: redraw on resource refresh
		m_modelInstances.clear();
		m_currentFolder = nullptr;
		m_originZ = 0;
		m_originInvalid = true;
	}
	template<typename Functor>
	void forEachModelInstance( const Functor& functor ) const {
		for( scene::Instance* instance : m_modelInstances )
			functor( instance );
	}
};

ModelBrowser g_ModelBrowser;





class models_set_transforms
{
	mutable CellPos m_cellPos = g_ModelBrowser.constructCellPos();
public:
	void operator()( scene::Instance* instance ) const {
		if( TransformNode *transformNode = Node_getTransformNode( instance->path().parent() ) ){
			if( Bounded *bounded = Instance_getBounded( *instance ) ){
				AABB aabb = bounded->localAABB();
				const float scale = m_cellPos.getCellSize() / aabb.extents[ vector3_max_abs_component_index( aabb.extents ) ];
				aabb.extents.z() *= 2; // prioritize Z for orientation
				const Matrix4 rotation = matrix4_rotation_for_euler_xyz_degrees(
				                             vector3_min_abs_component_index( aabb.extents ) == 0? Vector3( 0, 0, -90 )
				                             : vector3_min_abs_component_index( aabb.extents ) == 2? Vector3( 90, 0, 0 )
				                             : g_vector3_identity );
				const_cast<Matrix4&>( transformNode->localToParent() ) =
				        matrix4_multiplied_by_matrix4(
				            matrix4_translation_for_vec3( m_cellPos.getOrigin() ),
				            matrix4_multiplied_by_matrix4(
				                rotation,
				                matrix4_multiplied_by_matrix4(
				                    matrix4_scale_for_vec3( Vector3( scale, scale, scale ) ),
				                    matrix4_translation_for_vec3( -aabb.origin )
				                )
				            )
				        );
				instance->parent()->transformChangedLocal();
				instance->transformChangedLocal();
//%		globalOutputStream() << transformNode->localToParent() << " transformNode->localToParent()\n";
				++m_cellPos;
			}
		}
	}
};


class ModelRenderer : public Renderer
{
	struct state_type
	{
		state_type() :
			m_state( 0 ){
		}
		Shader* m_state;
	};
public:
	ModelRenderer( RenderStateFlags globalstate ) :
		m_globalstate( globalstate ){
		m_state_stack.push_back( state_type() );
	}

	void SetState( Shader* state, EStyle style ){
		ASSERT_NOTNULL( state );
		if ( style == eFullMaterials ) {
			m_state_stack.back().m_state = state;
		}
	}
	EStyle getStyle() const {
		return eFullMaterials;
	}
	void PushState(){
		m_state_stack.push_back( m_state_stack.back() );
	}
	void PopState(){
		ASSERT_MESSAGE( !m_state_stack.empty(), "popping empty stack" );
		m_state_stack.pop_back();
	}
	void Highlight( EHighlightMode mode, bool bEnable = true ){
	}
	void addRenderable( const OpenGLRenderable& renderable, const Matrix4& localToWorld ){
		m_state_stack.back().m_state->addRenderable( renderable, localToWorld );
	}

	void render( const Matrix4& modelview, const Matrix4& projection ){
		GlobalShaderCache().render( m_globalstate, modelview, projection );
	}
private:
	std::vector<state_type> m_state_stack;
	RenderStateFlags m_globalstate;
};

/*
x=0, z<=0
origin -----> +x
      |  --
      | |  |
      |  --
      \/ -z
*/
void ModelBrowser_render(){
	g_ModelBrowser.validate();

	g_ModelBrowser.fbo_get()->start();

	const int W = g_ModelBrowser.m_width;
	const int H = g_ModelBrowser.m_height;
	glViewport( 0, 0, W, H );

	// enable depth buffer writes
	glDepthMask( GL_TRUE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	glClearColor( .25f, .25f, .25f, 0 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	const unsigned int globalstate = RENDER_DEPTHTEST
	                               | RENDER_COLOURWRITE
	                               | RENDER_DEPTHWRITE
	                               | RENDER_ALPHATEST
	                               | RENDER_BLEND
	                               | RENDER_CULLFACE
	                               | RENDER_COLOURARRAY
	                               | RENDER_POLYGONSMOOTH
	                               | RENDER_LINESMOOTH
	                               | RENDER_FOG
	                               | RENDER_COLOURCHANGE
	                               | RENDER_FILL
	                               | RENDER_LIGHTING
	                               | RENDER_TEXTURE
	                               | RENDER_SMOOTH
	                               | RENDER_SCALED;


	Matrix4 m_projection;

	m_projection[0] = 1.0f / static_cast<float>( W / 2.f );
	m_projection[5] = 1.0f / static_cast<float>( H / 2.f );
	m_projection[10] = 1.0f / ( 9999 );

	m_projection[12] = 0.0f;
	m_projection[13] = 0.0f;
	m_projection[14] = -1.0f;

	m_projection[1] = m_projection[2] = m_projection[3] =
	m_projection[4] = m_projection[6] = m_projection[7] =
	m_projection[8] = m_projection[9] = m_projection[11] = 0.0f;

	m_projection[15] = 1.0f;


	Matrix4 m_modelview;
	// translation
	m_modelview[12] = -W / 2.f;
	m_modelview[13] = H / 2.f - g_ModelBrowser.m_originZ;
	m_modelview[14] = 9999;

	// axis base
	//XZ:
	m_modelview[0]  =  1;
	m_modelview[1]  =  0;
	m_modelview[2]  =  0;

	m_modelview[4]  =  0;
	m_modelview[5]  =  0;
	m_modelview[6]  =  1;

	m_modelview[8]  =  0;
	m_modelview[9]  =  1;
	m_modelview[10] =  0;


	m_modelview[3] = m_modelview[7] = m_modelview[11] = 0;
	m_modelview[15] = 1;



	View m_view( true );
	m_view.Construct( m_projection, m_modelview, W, H );


	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );


	if( g_ModelBrowser.m_currentFolder != nullptr ){
		{	// prepare for 2d stuff
			glDisable( GL_BLEND );

			if ( GlobalOpenGL().GL_1_3() ) {
				glClientActiveTexture( GL_TEXTURE0 );
				glActiveTexture( GL_TEXTURE0 );
			}

			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_COLOR_ARRAY );

			glDisable( GL_TEXTURE_2D );
			glDisable( GL_LIGHTING );
			glDisable( GL_COLOR_MATERIAL );
			glDisable( GL_DEPTH_TEST );
		}

		{	// brighter background squares
			glColor4f( 0.3f, 0.3f, 0.3f, 1.f );
			glDepthMask( GL_FALSE );
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glDisable( GL_CULL_FACE );

			CellPos cellPos = g_ModelBrowser.constructCellPos();
			glBegin( GL_QUADS );
			for( std::size_t i = g_ModelBrowser.m_currentFolder->m_files.size(); i != 0; --i ){
				const Vector3 origin = cellPos.getOrigin();
				const float minx = origin.x() - cellPos.getCellSize();
				const float maxx = origin.x() + cellPos.getCellSize();
				const float minz = origin.z() - cellPos.getCellSize();
				const float maxz = origin.z() + cellPos.getCellSize();
				glVertex3f( minx, 0, maxz );
				glVertex3f( minx, 0, minz );
				glVertex3f( maxx, 0, minz );
				glVertex3f( maxx, 0, maxz );
				++cellPos;
			}
			glEnd();
		}

		// one directional light source directly behind the viewer
		{
			GLfloat inverse_cam_dir[4], ambient[4], diffuse[4];

			ambient[0] = ambient[1] = ambient[2] = 0.4f;
			ambient[3] = 1.0f;
			diffuse[0] = diffuse[1] = diffuse[2] = 0.4f;
			diffuse[3] = 1.0f;

			inverse_cam_dir[0] = -m_view.getViewDir()[0];
			inverse_cam_dir[1] = -m_view.getViewDir()[1];
			inverse_cam_dir[2] = -m_view.getViewDir()[2];
			inverse_cam_dir[3] = 0;

			glLightfv( GL_LIGHT0, GL_POSITION, inverse_cam_dir );

			glLightfv( GL_LIGHT0, GL_AMBIENT, ambient );
			glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuse );

			glEnable( GL_LIGHT0 );
		}

		{
			ModelRenderer renderer( globalstate );

			g_ModelBrowser.forEachModelInstance( [&renderer, &m_view]( scene::Instance* instance ){
				if( Renderable *renderable = Instance_getRenderable( *instance ) )
					renderable->renderSolid( renderer, m_view );
			} );

			renderer.render( m_modelview, m_projection );
		}

		{	// prepare for 2d stuff
			glColor4f( 1, 1, 1, 1 );
			glDisable( GL_BLEND );

			if ( GlobalOpenGL().GL_1_3() ) {
				glClientActiveTexture( GL_TEXTURE0 );
				glActiveTexture( GL_TEXTURE0 );
			}

			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_COLOR_ARRAY );

			glDisable( GL_TEXTURE_2D );
			glDisable( GL_LIGHTING );
			glDisable( GL_COLOR_MATERIAL );
			glDisable( GL_DEPTH_TEST );
			glLineWidth( 1 );
		}
		{	// render model file names
			CellPos cellPos = g_ModelBrowser.constructCellPos();
			for( const CopiedString& string : g_ModelBrowser.m_currentFolder->m_files ){
				const Vector3 pos = cellPos.getTextPos();
				if( m_view.TestPoint( pos ) ){
					glRasterPos3f( pos.x(), pos.y(), pos.z() );
					GlobalOpenGL().drawString( string.c_str() );
				}
				++cellPos;
			}
		}
	}

	// bind back to the default texture so that we don't have problems
	// elsewhere using/modifying texture maps between contexts
	glBindTexture( GL_TEXTURE_2D, 0 );

	g_ModelBrowser.fbo_get()->save();
}


gboolean ModelBrowser_size_allocate( GtkWidget* widget, GtkAllocation* allocation, ModelBrowser* modelBrowser ){
	modelBrowser->fbo_get()->reset( allocation->width, allocation->height, modelBrowser->m_MSAA, true );
	modelBrowser->m_width = allocation->width;
	modelBrowser->m_height = allocation->height;
	modelBrowser->m_originInvalid = true;
	modelBrowser->forEachModelInstance( models_set_transforms() );
	modelBrowser->queueDraw();
	return FALSE;
}

gboolean ModelBrowser_expose( GtkWidget* widget, GdkEventExpose* event, ModelBrowser* modelBrowser ){
	if ( glwidget_make_current( modelBrowser->m_gl_widget ) ) {
		GlobalOpenGL_debugAssertNoErrors();
		ModelBrowser_render();
		GlobalOpenGL_debugAssertNoErrors();
		glwidget_swap_buffers( modelBrowser->m_gl_widget );
	}
	return FALSE;
}




gboolean ModelBrowser_mouseScroll( GtkWidget* widget, GdkEventScroll* event, ModelBrowser* modelBrowser ){
	gtk_widget_grab_focus( widget );
	if( !gtk_window_is_active( modelBrowser->m_parent ) )
		gtk_window_present( modelBrowser->m_parent );

	if ( event->direction == GDK_SCROLL_UP ) {
		modelBrowser->setOriginZ( modelBrowser->m_originZ + 64 );
	}
	else if ( event->direction == GDK_SCROLL_DOWN ) {
		modelBrowser->setOriginZ( modelBrowser->m_originZ - 64 );
	}
	return FALSE;
}

void ModelBrowser_scrollChanged( void* data, gdouble value ){
	//globalOutputStream() << "vertical scroll\n";
	reinterpret_cast<ModelBrowser*>( data )->setOriginZ( -(int)value );
}

static void ModelBrowser_scrollbarScroll( GtkAdjustment *adjustment, ModelBrowser* modelBrowser ){
	modelBrowser->m_scrollAdjustment.value_changed( gtk_adjustment_get_value( adjustment ) );
}


gboolean ModelBrowser_button_press( GtkWidget* widget, GdkEventButton* event, ModelBrowser* modelBrowser ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		gtk_widget_grab_focus( widget );
		if ( event->button == 1 || event->button == 3 ) {
			modelBrowser->tracking_MouseDown();
		}
		if ( event->button == 1 ) {
			modelBrowser->testSelect( static_cast<int>( event->x ), static_cast<int>( event->y ) );
		}
	}
	/* create misc_model */
	else if ( event->type == GDK_2BUTTON_PRESS && event->button == 1 && modelBrowser->m_currentFolder != nullptr && modelBrowser->m_currentModelId >= 0 ) {
		UndoableCommand undo( "insertModel" );
		// todo
		// GlobalEntityClassManager() search for "misc_model"
		// otherwise search for entityClass->miscmodel_is
		// otherwise go with GlobalEntityClassManager().findOrInsert( "misc_model", false );
		EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( "misc_model", false );
		NodeSmartReference node( GlobalEntityCreator().createEntity( entityClass ) );

		Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

		scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
		entitypath.push( makeReference( node.get() ) );
		scene::Instance& instance = findInstance( entitypath );

		if ( Transformable* transform = Instance_getTransformable( instance ) ) { // might be cool to consider model aabb here
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setTranslation( vector3_snapped( Camera_getOrigin( *g_pParentWnd->GetCamWnd() ) - Camera_getViewVector( *g_pParentWnd->GetCamWnd() ) * 128.f, GetSnapGridSize() ) );
			transform->freezeTransform();
		}

		GlobalSelectionSystem().setSelectedAll( false );
		Instance_setSelected( instance, true );

		StringOutputStream sstream( 128 );
		sstream << modelBrowser->m_currentFolderPath << std::next( modelBrowser->m_currentFolder->m_files.begin(), modelBrowser->m_currentModelId )->c_str();
		Node_getEntity( node )->setKeyValue( entityClass->miscmodel_key(), sstream.c_str() );
	}
	return FALSE;
}

gboolean ModelBrowser_button_release( GtkWidget* widget, GdkEventButton* event, ModelBrowser* modelBrowser ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		if ( event->button == 1 || event->button == 3 ) {
			modelBrowser->tracking_MouseUp();
		}
		if ( event->button == 1 && modelBrowser->m_move_amount < 16 && modelBrowser->m_currentFolder != nullptr && modelBrowser->m_currentModelId >= 0 ) { // assign model to selected entity nodes
			StringOutputStream sstream( 128 );
			sstream << modelBrowser->m_currentFolderPath << std::next( modelBrowser->m_currentFolder->m_files.begin(), modelBrowser->m_currentModelId )->c_str();
			class EntityVisitor : public SelectionSystem::Visitor
			{
				const char* m_filePath;
			public:
				EntityVisitor( const char* filePath ) : m_filePath( filePath ){
				}
				void visit( scene::Instance& instance ) const override {
					if( Entity* entity = Node_getEntity( instance.path().top() ) ){
						entity->setKeyValue( entity->getEntityClass().miscmodel_key(), m_filePath );
					}
				}
			} visitor( sstream.c_str() );
			UndoableCommand undo( "entityAssignModel" );
			GlobalSelectionSystem().foreachSelected( visitor );
		}
	}
	return FALSE;
}



static void TreeView_onRowActivated( GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* col, gpointer userdata ){
	GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ) );
	GtkTreeIter iter;
	if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
		std::deque<GtkTreeIter> iters;
		iters.push_front( iter );
		GtkTreeIter parent;
		while( gtk_tree_model_iter_parent( model, &parent, &iters.front() ) )
			iters.push_front( parent );
		StringOutputStream sstream( 64 );
		const ModelFS *modelFS = &g_ModelBrowser.m_modelFS;
		for( GtkTreeIter& i : iters ){
			gchar* buffer;
			gtk_tree_model_get( model, &i, 0, &buffer, -1 );
			const auto found = modelFS->m_folders.find( ModelFS( StringRange( buffer, strlen( buffer ) ) ) );
			if( found != modelFS->m_folders.end() ){ // ok to not find, while loading root
				modelFS = &( *found );
				sstream << buffer << "/";
			}
			g_free( buffer );
		}

//%						globalOutputStream() << sstream.c_str() << " sstream.c_str()\n";

		ModelGraph_clear(); // this goes 1st: resets m_currentFolder

		g_ModelBrowser.m_currentFolder = modelFS;
		g_ModelBrowser.m_currentFolderPath = sstream.c_str();

		ScopeDisableScreenUpdates disableScreenUpdates( g_ModelBrowser.m_currentFolderPath.c_str(), "Loading Models" );
		{
			for( const CopiedString& filename : g_ModelBrowser.m_currentFolder->m_files ){
				sstream.clear();
				sstream << g_ModelBrowser.m_currentFolderPath << filename;
				ModelNode *modelNode = new ModelNode;
				modelNode->setModel( sstream.c_str() );
				NodeSmartReference node( modelNode->node() );
				Node_getTraversable( g_modelGraph->root() )->insert( node );
			}
			g_ModelBrowser.forEachModelInstance( models_set_transforms() );
		}

		g_ModelBrowser.queueDraw();

		//deactivate, so SPACE and RETURN wont be broken for 2d
		gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( treeview ) ) ), NULL );
	}
}



#if 0
void modelFS_traverse( const ModelFS& modelFS ){
	static int depth = -1;
	++depth;
	for( int i = 0; i < depth; ++i ){
		globalOutputStream() << "\t";
	}
	globalOutputStream() << modelFS.m_folderName.c_str() << "\n";
	for( const ModelFS& m : modelFS.m_folders )
		modelFS_traverse( m );

	--depth;
}
#endif
void ModelBrowser_constructTreeModel( const ModelFS& modelFS, GtkTreeStore* store, GtkTreeIter* parent ){
	GtkTreeIter iter;
	gtk_tree_store_append( store, &iter, parent );
	gtk_tree_store_set( store, &iter, 0, modelFS.m_folderName.c_str(), -1 );
	for( const ModelFS& m : modelFS.m_folders )
		ModelBrowser_constructTreeModel( m, store, &iter ); //recursion
}

typedef std::map<CopiedString, std::size_t> ModelFoldersMap;

class ModelFolders
{
public:
	ModelFoldersMap m_modelFoldersMap;
	// parse string of format *pathToLoad/depth*path2ToLoad/depth*
	// */depth* for root path
	ModelFolders( const char* pathsString ){
		const auto str = StringOutputStream( 128 )( PathCleaned( pathsString ) );

		const char* start = str.c_str();
		while( 1 ){
			while( *start == '*' )
				++start;
			const char* end = start;
			while( *end && *end != '*' )
				++end;
			if( start == end )
				break;
			const char* slash = nullptr;
			for( const char* s = start; s != end; ++s )
				if( *s == '/' )
					slash = s;
			if( slash != nullptr && end - slash > 1 ){
				std::size_t depth;
				Size_importString( depth, CopiedString( StringRange( slash + 1, end ) ).c_str() );
				StringRange folder( start, ( start == slash )? slash : slash + 1 );
				m_modelFoldersMap.emplace( folder, depth );
			}
			start = end;
		}

		if( m_modelFoldersMap.empty() )
			m_modelFoldersMap.emplace( "models/", 99 );
	}
};


typedef std::set<CopiedString, bool(*)( const CopiedString&, const CopiedString& )> StringSetWithLambda;

class ModelPaths_ArchiveVisitor : public Archive::Visitor
{
	const StringSetWithLambda& m_modelExtensions;
	ModelFS& m_modelFS;
public:
	const ModelFoldersMap& m_modelFoldersMap;
	bool m_avoid_pk3dir;
	ModelPaths_ArchiveVisitor( const StringSetWithLambda& modelExtensions, ModelFS& modelFS, const ModelFoldersMap& modelFoldersMap )
		: m_modelExtensions( modelExtensions ),	m_modelFS( modelFS ), m_modelFoldersMap( modelFoldersMap ){
	}
	void visit( const char* name ) override {
		if( m_modelExtensions.count( path_get_extension( name ) ) && ( !m_avoid_pk3dir || !string_in_string_nocase( name, ".pk3dir/" ) ) ){
			m_modelFS.insert( name );
//%			globalOutputStream() << name << " name\n";
		}
	}
};

void ModelPaths_addFromArchive( ModelPaths_ArchiveVisitor& visitor, const char *archiveName ){
//%	globalOutputStream() << "\t\t" << archiveName << " archiveName\n";
	Archive *archive = GlobalFileSystem().getArchive( archiveName, false );
	if ( archive != nullptr ) {
		for( const auto& folder : visitor.m_modelFoldersMap ){
			/* should better avoid .pk3dir traversal right in archive implementation for normal folders */
			visitor.m_avoid_pk3dir = string_empty( folder.first.c_str() ) // root
			                      && folder.second > 1 // deep nuff
			                      && string_equal_suffix( archiveName, "/" ) // normal folder, not archive
			                      && !string_equal_suffix_nocase( archiveName, ".pk3dir/" ); // not .pk3dir
			archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eFiles, folder.second ), folder.first.c_str() );
		}
	}
}
typedef ReferenceCaller1<ModelPaths_ArchiveVisitor, const char*, ModelPaths_addFromArchive> ModelPaths_addFromArchiveCaller;

void ModelBrowser_constructTree(){
	g_ModelBrowser.m_modelFS.m_folders.clear();
	g_ModelBrowser.m_modelFS.m_files.clear();
	ModelGraph_clear();
	g_ModelBrowser.queueDraw();

	class : public IFileTypeList
	{
	public:
		StringSetWithLambda m_modelExtensions{ []( const CopiedString& lhs, const CopiedString& rhs )->bool{
			return string_less_nocase( lhs.c_str(), rhs.c_str() );
		} };
		void addType( const char* moduleName, filetype_t type ) override {
			m_modelExtensions.emplace( moduleName );
		}
	} typelist;
	GlobalFiletypes().getTypeList( ModelLoader::Name, &typelist, true, false, false );

	ModelFolders modelFolders( g_ModelBrowser.m_prefFoldersToLoad.c_str() );

	ModelPaths_ArchiveVisitor visitor( typelist.m_modelExtensions, g_ModelBrowser.m_modelFS, modelFolders.m_modelFoldersMap );
	GlobalFileSystem().forEachArchive( ModelPaths_addFromArchiveCaller( visitor ), false, false );

//%	modelFS_traverse( g_ModelBrowser.m_modelFS );


	GtkTreeStore* store = gtk_tree_store_new( 1, G_TYPE_STRING );

	{
		if( !g_ModelBrowser.m_modelFS.m_files.empty() ){ // models in the root
			GtkTreeIter iter;
			gtk_tree_store_append( store, &iter, nullptr );
			gtk_tree_store_set( store, &iter, 0, "", -1 );
		}

		for( const ModelFS& m : g_ModelBrowser.m_modelFS.m_folders )
			ModelBrowser_constructTreeModel( m, store, nullptr );
	}

	gtk_tree_view_set_model( GTK_TREE_VIEW( g_ModelBrowser.m_treeViewTree ), GTK_TREE_MODEL( store ) );

	g_object_unref( G_OBJECT( store ) );
}

GtkWidget* ModelBrowser_constructWindow( GtkWindow* toplevel ){
	g_ModelBrowser.m_parent = toplevel;

	GtkWidget* table = gtk_table_new( 1, 3, FALSE );
	GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );
	gtk_table_attach( GTK_TABLE( table ), vbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0 );
	gtk_widget_show( vbox );

	{	// menu bar
		GtkToolbar* toolbar = toolbar_new();
		gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( toolbar ), FALSE, FALSE, 0 );

		GtkToolButton* button = toolbar_append_button( toolbar, "Reload Model Folders Tree View", "texbro_refresh.png", FreeCaller<ModelBrowser_constructTree>() );
//		gtk_widget_set_size_request( GTK_WIDGET( button ), 22, 22 );
	}
	{	// TreeView
		GtkWidget* scr = gtk_scrolled_window_new( NULL, NULL );
		gtk_container_set_border_width( GTK_CONTAINER( scr ), 0 );
		// vertical only scrolling for treeview
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
		gtk_widget_show( scr );

		g_ModelBrowser.m_treeViewTree = gtk_tree_view_new();
		GtkTreeView* treeview = GTK_TREE_VIEW( g_ModelBrowser.m_treeViewTree );
		//gtk_tree_view_set_enable_search( treeview, FALSE );

		gtk_tree_view_set_headers_visible( treeview, FALSE );
		g_signal_connect( treeview, "row-activated", (GCallback) TreeView_onRowActivated, NULL );

		GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
		//g_object_set( G_OBJECT( renderer ), "ellipsize", PANGO_ELLIPSIZE_START, NULL );
		gtk_tree_view_insert_column_with_attributes( treeview, -1, "", renderer, "text", 0, NULL );


		ModelBrowser_constructTree();


		//gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( scr ), g_ModelBrowser.m_treeViewTree );
		gtk_container_add( GTK_CONTAINER( scr ), g_ModelBrowser.m_treeViewTree ); //GtkTreeView has native scrolling support; should not be used with the GtkViewport proxy.
		gtk_widget_show( g_ModelBrowser.m_treeViewTree );

		gtk_box_pack_start( GTK_BOX( vbox ), scr, TRUE, TRUE, 0 );
	}
	{	// gl_widget scrollbar
		GtkWidget* w = g_ModelBrowser.m_gl_scroll = gtk_vscrollbar_new( GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 0, 1, 1, 0 ) ) );
		gtk_table_attach( GTK_TABLE( table ), w, 2, 3, 0, 1, GTK_SHRINK, GTK_FILL, 0, 0 );
		gtk_widget_show( w );

		GtkAdjustment *vadjustment = gtk_range_get_adjustment( GTK_RANGE( w ) );
		g_signal_connect( G_OBJECT( vadjustment ), "value_changed", G_CALLBACK( ModelBrowser_scrollbarScroll ), &g_ModelBrowser );
	}
	{	// gl_widget
		GtkWidget* w = g_ModelBrowser.m_gl_widget = glwidget_new( TRUE );
		g_object_ref( G_OBJECT( w ) );

		gtk_widget_set_events( w, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK );
		gtk_widget_set_can_focus( w, TRUE );

		gtk_table_attach_defaults( GTK_TABLE( table ), w, 1, 2, 0, 1 );
		gtk_widget_show( w );

		g_ModelBrowser.m_sizeHandler = g_signal_connect( G_OBJECT( w ), "size_allocate", G_CALLBACK( ModelBrowser_size_allocate ), &g_ModelBrowser );
		g_ModelBrowser.m_exposeHandler = g_signal_connect( G_OBJECT( w ), "expose_event", G_CALLBACK( ModelBrowser_expose ), &g_ModelBrowser );

		g_signal_connect( G_OBJECT( w ), "button_press_event", G_CALLBACK( ModelBrowser_button_press ), &g_ModelBrowser );
		g_signal_connect( G_OBJECT( w ), "button_release_event", G_CALLBACK( ModelBrowser_button_release ), &g_ModelBrowser );
		g_signal_connect( G_OBJECT( w ), "scroll_event", G_CALLBACK( ModelBrowser_mouseScroll ), &g_ModelBrowser );
	}

	//prevent focusing on filter entry or tex dirs treeview after click on tab of floating group dialog (np, if called via hotkey)
	gtk_container_set_focus_chain( GTK_CONTAINER( table ), NULL );

	return table;
}

void ModelBrowser_destroyWindow(){
	g_signal_handler_disconnect( G_OBJECT( g_ModelBrowser.m_gl_widget ), g_ModelBrowser.m_sizeHandler );
	g_signal_handler_disconnect( G_OBJECT( g_ModelBrowser.m_gl_widget ), g_ModelBrowser.m_exposeHandler );

	g_object_unref( G_OBJECT( g_ModelBrowser.m_gl_widget ) );
	g_ModelBrowser.m_gl_widget = nullptr;

	delete g_ModelBrowser.m_fbo;
}


#include "preferencesystem.h"
#include "preferences.h"
#include "stringio.h"

void CellSizeImport( int& oldvalue, int value ){
	if( oldvalue != value ){
		oldvalue = value;
		g_ModelBrowser.forEachModelInstance( models_set_transforms() );
		g_ModelBrowser.m_originInvalid = true;
		g_ModelBrowser.queueDraw();
	}
}
typedef ReferenceCaller1<int, int, CellSizeImport> CellSizeImportCaller;

void FoldersToLoadImport( CopiedString& self, const char* value ){
	if( self != value ){
		self = value;
		ModelBrowser_constructTree();
	}
}
typedef ReferenceCaller1<CopiedString, const char*, FoldersToLoadImport> FoldersToLoadImportCaller;

void ModelBrowser_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Model Browser", "Model Browser Preferences" ) );

	page.appendSpinner( "Model View Size", 80.0, 16.0, 8192.0,
	                    IntImportCallback( CellSizeImportCaller( g_ModelBrowser.m_cellSize ) ),
	                    IntExportCallback( IntExportCaller( g_ModelBrowser.m_cellSize ) ) );
	page.appendEntry( "List of *folderToLoad/depth*",
	                  StringImportCallback( FoldersToLoadImportCaller( g_ModelBrowser.m_prefFoldersToLoad ) ),
	                  StringExportCallback( StringExportCaller( g_ModelBrowser.m_prefFoldersToLoad ) ) );
}
void ModelBrowser_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( FreeCaller1<PreferenceGroup&, ModelBrowser_constructPage>() );
}

void ModelBrowser_Construct(){
	GlobalPreferenceSystem().registerPreference( "ModelBrowserFolders", CopiedStringImportStringCaller( g_ModelBrowser.m_prefFoldersToLoad ), CopiedStringExportStringCaller( g_ModelBrowser.m_prefFoldersToLoad ) );
	GlobalPreferenceSystem().registerPreference( "ModelBrowserCellSize", IntImportStringCaller( g_ModelBrowser.m_cellSize ), IntExportStringCaller( g_ModelBrowser.m_cellSize ) );

	ModelBrowser_registerPreferencesPage();

	g_modelGraph = new ModelGraph( g_ModelBrowser );
	g_modelGraph->insert_root( ( new ModelGraphRoot )->node() );
}

void ModelBrowser_Destroy(){
	g_modelGraph->erase_root();
	delete g_modelGraph;
}

GtkWidget* ModelBrowser_getGLWidget(){
	return g_ModelBrowser.m_gl_widget;
}

void ModelBrowser_flushReferences(){
	ModelGraph_clear();
	g_ModelBrowser.queueDraw();
}

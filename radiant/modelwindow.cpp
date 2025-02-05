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

#include "gtkutil/glwidget.h"
#include "gtkutil/toolbar.h"
#include "gtkutil/cursor.h"
#include "gtkmisc.h"
#include "gtkutil/fbo.h"
#include "gtkutil/mousepresses.h"
#include "gtkutil/guisettings.h"

#include <QWidget>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QTreeView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QScrollBar>
#include <QOpenGLWidget>

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

class ModelBrowser : public scene::Instantiable::Observer
{
	// track instances in the order of insertion
	std::vector<scene::Instance*> m_modelInstances;
public:
	ModelFS m_modelFS;
	CopiedString m_prefFoldersToLoad = "*models/99*";
	ModelBrowser() : m_scrollAdjustment( [this]( int value ){
		//globalOutputStream() << "vertical scroll\n";
		setOriginZ( -value );
	} )
	{
	}
	~ModelBrowser(){
	}

	const int m_MSAA = 8;
	Vector3 m_background_color = Vector3( .25f );

	QWidget* m_parent = nullptr;
	QOpenGLWidget* m_gl_widget = nullptr;
	QScrollBar* m_gl_scroll = nullptr;
	QTreeView* m_treeView = nullptr;

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
		m_gl_scroll->setMinimum( 0 );
		m_gl_scroll->setMaximum( totalHeight() - m_height );
		m_gl_scroll->setValue( -m_originZ );
		m_gl_scroll->setPageStep( m_height );
		m_gl_scroll->setSingleStep( 20 );
	}
public:
	void setOriginZ( int origin ){
		m_originZ = origin;
		m_originInvalid = true;
		validate(); // do updateScroll() immediately here; calling it in render() may call setOriginZ() again with old value
		queueDraw();
	}
	void queueDraw() const {
		if ( m_gl_widget != nullptr )
			widget_queue_draw( *m_gl_widget );
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
	void trackingDelta( int x, int y, const QMouseEvent *event ){
		m_move_amount += std::abs( x ) + std::abs( y );
		if ( event->buttons() & Qt::MouseButton::RightButton && y != 0 ) { // scroll view
			const int scale = event->modifiers().testFlag( Qt::KeyboardModifier::ShiftModifier )? 4 : 1;
			setOriginZ( m_originZ + y * scale );
		}
		else if ( event->buttons() & Qt::MouseButton::LeftButton && ( x != 0 || y != 0 ) && m_currentModelId >= 0 ) { // rotate selected model
			ASSERT_MESSAGE( m_currentModelId < static_cast<int>( m_modelInstances.size() ), "modelBrowser.m_currentModelId out of range" );
			scene::Instance *instance = m_modelInstances[m_currentModelId];
			if( TransformNode *transformNode = Node_getTransformNode( instance->path().parent() ) ){
				Matrix4 rot( g_matrix4_identity );
				matrix4_pivoted_rotate_by_euler_xyz_degrees( rot, Vector3( y, 0, x ) * ( 45.f / m_cellSize ), constructCellPos().getOrigin( m_currentModelId ) );
				matrix4_premultiply_by_matrix4( const_cast<Matrix4&>( transformNode->localToParent() ), rot );
				instance->parent()->transformChangedLocal();
				instance->transformChangedLocal();
				queueDraw();
			}
		}
	}
	FreezePointer m_freezePointer;
	bool m_move_started = false;
public:
	int m_move_amount;
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
		m_freezePointer.freeze_pointer( m_gl_widget,
			[this]( int x, int y, const QMouseEvent *event ){
				trackingDelta( x, y, event );
			},
			[this](){
				tracking_MouseUp();
			} );
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

	const int W = g_ModelBrowser.m_width;
	const int H = g_ModelBrowser.m_height;
	gl().glViewport( 0, 0, W, H );

	// enable depth buffer writes
	gl().glDepthMask( GL_TRUE );
	gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	gl().glClearColor( g_ModelBrowser.m_background_color[0],
	                   g_ModelBrowser.m_background_color[1],
	                   g_ModelBrowser.m_background_color[2], 0 );
	gl().glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	const unsigned int globalstate = RENDER_DEPTHTEST
	                               | RENDER_COLOURWRITE
	                               | RENDER_DEPTHWRITE
	                               | RENDER_ALPHATEST
	                               | RENDER_BLEND
	                               | RENDER_CULLFACE
	                               | RENDER_COLOURARRAY
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


	gl().glMatrixMode( GL_PROJECTION );
	gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_projection ) );

	gl().glMatrixMode( GL_MODELVIEW );
	gl().glLoadMatrixf( reinterpret_cast<const float*>( &m_modelview ) );


	if( g_ModelBrowser.m_currentFolder != nullptr ){
		{	// prepare for 2d stuff
			gl().glDisable( GL_BLEND );

			gl().glClientActiveTexture( GL_TEXTURE0 );
			gl().glActiveTexture( GL_TEXTURE0 );

			gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			gl().glDisableClientState( GL_NORMAL_ARRAY );
			gl().glDisableClientState( GL_COLOR_ARRAY );

			gl().glDisable( GL_TEXTURE_2D );
			gl().glDisable( GL_LIGHTING );
			gl().glDisable( GL_COLOR_MATERIAL );
			gl().glDisable( GL_DEPTH_TEST );
		}

		{	// brighter background squares
			gl().glColor4f( g_ModelBrowser.m_background_color[0] + .05f,
			                g_ModelBrowser.m_background_color[1] + .05f,
			                g_ModelBrowser.m_background_color[2] + .05f, 1.f );
			gl().glDepthMask( GL_FALSE );
			gl().glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			gl().glDisable( GL_CULL_FACE );

			CellPos cellPos = g_ModelBrowser.constructCellPos();
			gl().glBegin( GL_QUADS );
			for( std::size_t i = g_ModelBrowser.m_currentFolder->m_files.size(); i != 0; --i ){
				const Vector3 origin = cellPos.getOrigin();
				const float minx = origin.x() - cellPos.getCellSize();
				const float maxx = origin.x() + cellPos.getCellSize();
				const float minz = origin.z() - cellPos.getCellSize();
				const float maxz = origin.z() + cellPos.getCellSize();
				gl().glVertex3f( minx, 0, maxz );
				gl().glVertex3f( minx, 0, minz );
				gl().glVertex3f( maxx, 0, minz );
				gl().glVertex3f( maxx, 0, maxz );
				++cellPos;
			}
			gl().glEnd();
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

			gl().glLightfv( GL_LIGHT0, GL_POSITION, inverse_cam_dir );

			gl().glLightfv( GL_LIGHT0, GL_AMBIENT, ambient );
			gl().glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuse );

			gl().glEnable( GL_LIGHT0 );
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
			gl().glColor4f( 1, 1, 1, 1 );
			gl().glDisable( GL_BLEND );

			gl().glClientActiveTexture( GL_TEXTURE0 );
			gl().glActiveTexture( GL_TEXTURE0 );

			gl().glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			gl().glDisableClientState( GL_NORMAL_ARRAY );
			gl().glDisableClientState( GL_COLOR_ARRAY );

			gl().glDisable( GL_TEXTURE_2D );
			gl().glDisable( GL_LIGHTING );
			gl().glDisable( GL_COLOR_MATERIAL );
			gl().glDisable( GL_DEPTH_TEST );
			gl().glLineWidth( 1 );
		}
		{	// render model file names
			CellPos cellPos = g_ModelBrowser.constructCellPos();
			for( const CopiedString& string : g_ModelBrowser.m_currentFolder->m_files ){
				const Vector3 pos = cellPos.getTextPos();
				if( m_view.TestPoint( pos ) ){
					gl().glRasterPos3f( pos.x(), pos.y(), pos.z() );
					GlobalOpenGL().drawString( string.c_str() );
				}
				++cellPos;
			}
		}
	}

	// bind back to the default texture so that we don't have problems
	// elsewhere using/modifying texture maps between contexts
	gl().glBindTexture( GL_TEXTURE_2D, 0 );
}


class ModelBrowserGLWidget : public QOpenGLWidget
{
	ModelBrowser& m_modBro;
	FBO *m_fbo{};
	qreal m_scale;
	MousePresses m_mouse;
public:
	ModelBrowserGLWidget( ModelBrowser& modelBrowser ) : QOpenGLWidget(), m_modBro( modelBrowser )
	{
	}

	~ModelBrowserGLWidget() override {
		delete m_fbo;
		glwidget_context_destroyed();
	}
protected:
	void initializeGL() override
	{
		glwidget_context_created( *this );
	}
	void resizeGL( int w, int h ) override
	{
		m_scale = devicePixelRatioF();
		m_modBro.m_width = float_to_integer( w * m_scale );
		m_modBro.m_height = float_to_integer( h * m_scale );
		m_modBro.m_originInvalid = true;
		m_modBro.forEachModelInstance( models_set_transforms() );

		delete m_fbo;
		m_fbo = new FBO( m_modBro.m_width, m_modBro.m_height, true, m_modBro.m_MSAA );
	}
	void paintGL() override
	{
		if( ScreenUpdates_Enabled() && m_fbo->bind() ){
			GlobalOpenGL_debugAssertNoErrors();
			ModelBrowser_render();
			GlobalOpenGL_debugAssertNoErrors();
			m_fbo->blit();
			m_fbo->release();
		}
	}

	void mousePressEvent( QMouseEvent *event ) override {
		setFocus();
		const auto press = m_mouse.press( event );
		if( press == MousePresses::Left2x ){
			mouseDoubleClick();
		}
		else if ( press == MousePresses::Left || press == MousePresses::Right ) {
			m_modBro.tracking_MouseDown();
			if ( press == MousePresses::Left ) {
				m_modBro.testSelect( event->x() * m_scale, event->y() * m_scale );
			}
		}
	}
	void mouseDoubleClick(){
		/* create misc_model */
		if ( m_modBro.m_currentFolder != nullptr && m_modBro.m_currentModelId >= 0 ) {
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

			const auto sstream = StringStream<128>( m_modBro.m_currentFolderPath, std::next( m_modBro.m_currentFolder->m_files.begin(), m_modBro.m_currentModelId )->c_str() );
			Node_getEntity( node )->setKeyValue( entityClass->miscmodel_key(), sstream );
		}
	}
	void mouseReleaseEvent( QMouseEvent *event ) override {
		const auto release = m_mouse.release( event );
		if ( release == MousePresses::Left || release == MousePresses::Right ) {
			m_modBro.tracking_MouseUp();
		}
		if ( release == MousePresses::Left && m_modBro.m_move_amount < 16 && m_modBro.m_currentFolder != nullptr && m_modBro.m_currentModelId >= 0 ) { // assign model to selected entity nodes
			const auto sstream = StringStream<128>( m_modBro.m_currentFolderPath, std::next( m_modBro.m_currentFolder->m_files.begin(), m_modBro.m_currentModelId )->c_str() );
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
			} visitor( sstream );
			UndoableCommand undo( "entityAssignModel" );
			GlobalSelectionSystem().foreachSelected( visitor );
		}
		else if( release == MousePresses::Right && m_modBro.m_move_amount < 16 && m_modBro.m_currentFolder != nullptr ){
			m_modBro.forEachModelInstance( models_set_transforms() );
			m_modBro.queueDraw();
		}
	}
	void wheelEvent( QWheelEvent *event ) override {
		setFocus();
		if( !m_modBro.m_parent->isActiveWindow() ){
			m_modBro.m_parent->activateWindow();
			m_modBro.m_parent->raise();
		}

		m_modBro.setOriginZ( m_modBro.m_originZ + std::copysign( 64, event->angleDelta().y() ) );
	}
};



static void TreeView_onRowActivated( const QModelIndex& index ){
	StringOutputStream sstream( 64 );
	const ModelFS *modelFS = &g_ModelBrowser.m_modelFS;
	{
		std::deque<QModelIndex> iters;
		iters.push_front( index );
		while( iters.front().parent().isValid() )
			iters.push_front( iters.front().parent() );
		for( const QModelIndex& i : iters ){
			const auto dir = i.data( Qt::ItemDataRole::DisplayRole ).toByteArray();
			const auto found = modelFS->m_folders.find( ModelFS( StringRange( dir.constData(), strlen( dir.constData() ) ) ) );
			if( found != modelFS->m_folders.end() ){ // ok to not find, while loading root
				modelFS = &( *found );
				sstream << dir.constData() << '/';
			}
		}
	}

//%						globalOutputStream() << sstream << " sstream\n";

	ModelGraph_clear(); // this goes 1st: resets m_currentFolder

	g_ModelBrowser.m_currentFolder = modelFS;
	g_ModelBrowser.m_currentFolderPath = sstream;

	{
		ScopeDisableScreenUpdates disableScreenUpdates( g_ModelBrowser.m_currentFolderPath.c_str(), "Loading Models" );

		for( const CopiedString& filename : g_ModelBrowser.m_currentFolder->m_files ){
			sstream( g_ModelBrowser.m_currentFolderPath, filename );
			ModelNode *modelNode = new ModelNode;
			modelNode->setModel( sstream );
			NodeSmartReference node( modelNode->node() );
			Node_getTraversable( g_modelGraph->root() )->insert( node );
		}
		g_ModelBrowser.forEachModelInstance( models_set_transforms() );
	}

	g_ModelBrowser.queueDraw();

	//deactivate, so SPACE and RETURN wont be broken for 2d
	g_ModelBrowser.m_treeView->clearFocus();
}



#if 0
void modelFS_traverse( const ModelFS& modelFS ){
	static int depth = -1;
	++depth;
	for( int i = 0; i < depth; ++i ){
		globalOutputStream() << '\t';
	}
	globalOutputStream() << modelFS.m_folderName << '\n';
	for( const ModelFS& m : modelFS.m_folders )
		modelFS_traverse( m );

	--depth;
}
#endif
void ModelBrowser_constructTreeModel( const ModelFS& modelFS, QStandardItemModel* model, QStandardItem* parent ){
	auto item = new QStandardItem( modelFS.m_folderName.c_str() );
	parent->appendRow( item );
	for( const ModelFS& m : modelFS.m_folders )
		ModelBrowser_constructTreeModel( m, model, item ); //recursion
}

typedef std::map<CopiedString, std::size_t> ModelFoldersMap;

class ModelFolders
{
public:
	ModelFoldersMap m_modelFoldersMap;
	// parse string of format *pathToLoad/depth*path2ToLoad/depth*
	// */depth* for root path
	ModelFolders( const char* pathsString ){
		const auto str = StringStream<128>( PathCleaned( pathsString ) );

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


using StringSetNoCase = std::set<CopiedString, StringLessNoCase>;

class ModelPaths_ArchiveVisitor : public Archive::Visitor
{
	const StringSetNoCase& m_modelExtensions;
	ModelFS& m_modelFS;
public:
	const ModelFoldersMap& m_modelFoldersMap;
	ModelPaths_ArchiveVisitor( const StringSetNoCase& modelExtensions, ModelFS& modelFS, const ModelFoldersMap& modelFoldersMap )
		: m_modelExtensions( modelExtensions ),	m_modelFS( modelFS ), m_modelFoldersMap( modelFoldersMap ){
	}
	void visit( const char* name ) override {
		if( m_modelExtensions.contains( path_get_extension( name ) ) ){
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
			archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eFiles, folder.second ), folder.first.c_str() );
		}
	}
}
typedef ReferenceCaller<ModelPaths_ArchiveVisitor, void(const char*), ModelPaths_addFromArchive> ModelPaths_addFromArchiveCaller;

void ModelBrowser_constructTree(){
	g_ModelBrowser.m_modelFS.m_folders.clear();
	g_ModelBrowser.m_modelFS.m_files.clear();
	ModelGraph_clear();
	g_ModelBrowser.queueDraw();

	class : public IFileTypeList
	{
	public:
		StringSetNoCase m_modelExtensions;
		void addType( const char* moduleName, filetype_t type ) override {
			m_modelExtensions.emplace( moduleName );
		}
	} typelist;
	GlobalFiletypes().getTypeList( ModelLoader::Name, &typelist, true, false, false );

	ModelFolders modelFolders( g_ModelBrowser.m_prefFoldersToLoad.c_str() );

	ModelPaths_ArchiveVisitor visitor( typelist.m_modelExtensions, g_ModelBrowser.m_modelFS, modelFolders.m_modelFoldersMap );
	GlobalFileSystem().forEachArchive( ModelPaths_addFromArchiveCaller( visitor ), false, false );

//%	modelFS_traverse( g_ModelBrowser.m_modelFS );


	auto model = new QStandardItemModel( g_ModelBrowser.m_treeView ); //. ? delete old or clear() & reuse

	{
		if( !g_ModelBrowser.m_modelFS.m_files.empty() ){ // models in the root: add blank item for access
			model->invisibleRootItem()->appendRow( new QStandardItem( "" ) );
		}

		for( const ModelFS& m : g_ModelBrowser.m_modelFS.m_folders )
			ModelBrowser_constructTreeModel( m, model, model->invisibleRootItem() );
	}

	g_ModelBrowser.m_treeView->setModel( model );
}

class TexBro_QTreeView : public QTreeView
{
protected:
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride ){
			event->accept();
			return true;
		}
		return QTreeView::event( event );
	}
};

QWidget* ModelBrowser_constructWindow( QWidget* toplevel ){
	g_ModelBrowser.m_parent = toplevel;

	QSplitter *splitter = new QSplitter;
	QWidget *containerWidgetLeft = new QWidget; // Adding a QLayout to a QSplitter is not supported, use proxy widget
	QWidget *containerWidgetRight = new QWidget; // Adding a QLayout to a QSplitter is not supported, use proxy widget
	splitter->addWidget( containerWidgetLeft );
	splitter->addWidget( containerWidgetRight );
	QVBoxLayout *vbox = new QVBoxLayout( containerWidgetLeft );
	QHBoxLayout *hbox = new QHBoxLayout( containerWidgetRight );

	hbox->setContentsMargins( 0, 0, 0, 0 );
	vbox->setContentsMargins( 0, 0, 0, 0 );
	hbox->setSpacing( 0 );
	vbox->setSpacing( 0 );

	{	// menu bar
		QToolBar* toolbar = new QToolBar;
		vbox->addWidget( toolbar );

		toolbar_append_button( toolbar, "Reload Model Folders Tree View", "texbro_refresh.png", FreeCaller<void(), ModelBrowser_constructTree>() );
	}
	{	// TreeView
		g_ModelBrowser.m_treeView = new TexBro_QTreeView;
		g_ModelBrowser.m_treeView->setHeaderHidden( true );
		g_ModelBrowser.m_treeView->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
		g_ModelBrowser.m_treeView->setUniformRowHeights( true ); // optimization
		g_ModelBrowser.m_treeView->setFocusPolicy( Qt::FocusPolicy::ClickFocus );
		g_ModelBrowser.m_treeView->setExpandsOnDoubleClick( false );
		g_ModelBrowser.m_treeView->header()->setStretchLastSection( false ); // non greedy column sizing; + QHeaderView::ResizeMode::ResizeToContents = no text elision 🤷‍♀️
		g_ModelBrowser.m_treeView->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );


		QObject::connect( g_ModelBrowser.m_treeView, &QAbstractItemView::activated, TreeView_onRowActivated );

		ModelBrowser_constructTree();

		vbox->addWidget( g_ModelBrowser.m_treeView );
	}
	{	// gl_widget
		g_ModelBrowser.m_gl_widget = new ModelBrowserGLWidget( g_ModelBrowser );
		hbox->addWidget( g_ModelBrowser.m_gl_widget );
	}
	{	// gl_widget scrollbar
		auto scroll = g_ModelBrowser.m_gl_scroll = new QScrollBar;
		hbox->addWidget( scroll );

		QObject::connect( scroll, &QAbstractSlider::valueChanged, []( int value ){
			g_ModelBrowser.m_scrollAdjustment.value_changed( value );
		} );
	}

	splitter->setStretchFactor( 0, 0 ); // consistent treeview side sizing on resizes
	splitter->setStretchFactor( 1, 1 );
	g_guiSettings.addSplitter( splitter, "ModelBrowser/splitter", { 100, 500 } );
	return splitter;
}

void ModelBrowser_destroyWindow(){
	g_ModelBrowser.m_gl_widget = nullptr;
}


const Vector3& ModelBrowser_getBackgroundColour(){
	return g_ModelBrowser.m_background_color;
}

void ModelBrowser_setBackgroundColour( const Vector3& colour ){
	g_ModelBrowser.m_background_color = colour;
	g_ModelBrowser.queueDraw();
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
typedef ReferenceCaller<int, void(int), CellSizeImport> CellSizeImportCaller;

void FoldersToLoadImport( CopiedString& self, const char* value ){
	if( self != value ){
		self = value;
		ModelBrowser_constructTree();
	}
}
typedef ReferenceCaller<CopiedString, void(const char*), FoldersToLoadImport> FoldersToLoadImportCaller;

void ModelBrowser_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Model Browser", "Model Browser Preferences" ) );

	page.appendSpinner( "Model View Size", 16, 8192,
	                    IntImportCallback( CellSizeImportCaller( g_ModelBrowser.m_cellSize ) ),
	                    IntExportCallback( IntExportCaller( g_ModelBrowser.m_cellSize ) ) );
	page.appendEntry( "List of *folderToLoad/depth*",
	                  StringImportCallback( FoldersToLoadImportCaller( g_ModelBrowser.m_prefFoldersToLoad ) ),
	                  StringExportCallback( StringExportCaller( g_ModelBrowser.m_prefFoldersToLoad ) ) );
}
void ModelBrowser_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF( ModelBrowser_constructPage ) );
}

void ModelBrowser_Construct(){
	GlobalPreferenceSystem().registerPreference( "ModelBrowserFolders", CopiedStringImportStringCaller( g_ModelBrowser.m_prefFoldersToLoad ), CopiedStringExportStringCaller( g_ModelBrowser.m_prefFoldersToLoad ) );
	GlobalPreferenceSystem().registerPreference( "ModelBrowserCellSize", IntImportStringCaller( g_ModelBrowser.m_cellSize ), IntExportStringCaller( g_ModelBrowser.m_cellSize ) );
	GlobalPreferenceSystem().registerPreference( "ColorModBroBackground", Vector3ImportStringCaller( g_ModelBrowser.m_background_color ), Vector3ExportStringCaller( g_ModelBrowser.m_background_color ) );

	ModelBrowser_registerPreferencesPage();

	g_modelGraph = new ModelGraph( g_ModelBrowser );
	g_modelGraph->insert_root( ( new ModelGraphRoot )->node() );
}

void ModelBrowser_Destroy(){
	g_modelGraph->erase_root();
	delete g_modelGraph;
}

void ModelBrowser_flushReferences(){
	ModelGraph_clear();
	g_ModelBrowser.queueDraw();
}

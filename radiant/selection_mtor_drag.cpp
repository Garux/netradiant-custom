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

#include "selection_mtor_drag.h"

#include "selection_.h"
#include "selection_render.h"
#include "selection_volume.h"
#include "selection_selector.h"
#include "selection_mtable_translate.h"
#include "selection_mtable_brush.h"
#include "selection_mtable_extrude.h"
#include "selectionlib.h"
#include "brush.h"
#include "grid.h"


inline PlaneSelectable* Instance_getPlaneSelectable( scene::Instance& instance ){
	return InstanceTypeCast<PlaneSelectable>::cast( instance );
}

class PlaneSelectableSelectPlanes : public scene::Graph::Walker
{
	Selector& m_selector;
	SelectionTest& m_test;
	PlaneCallback m_selectedPlaneCallback;
public:
	PlaneSelectableSelectPlanes( Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback )
		: m_selector( selector ), m_test( test ), m_selectedPlaneCallback( selectedPlaneCallback ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if ( path.top().get().visible() && Instance_isSelected( instance ) ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->selectPlanes( m_selector, m_test, m_selectedPlaneCallback );
			}
		}
		return true;
	}
};

class PlaneSelectableSelectReversedPlanes : public scene::Graph::Walker
{
	Selector& m_selector;
	const SelectedPlanes& m_selectedPlanes;
public:
	PlaneSelectableSelectReversedPlanes( Selector& selector, const SelectedPlanes& selectedPlanes )
		: m_selector( selector ), m_selectedPlanes( selectedPlanes ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		if ( path.top().get().visible() && Instance_isSelected( instance ) ) {
			PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
			if ( planeSelectable != 0 ) {
				planeSelectable->selectReversedPlanes( m_selector, m_selectedPlanes );
			}
		}
		return true;
	}
};

void Scene_forEachPlaneSelectable_selectPlanes( scene::Graph& graph, Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback ){
	graph.traverse( PlaneSelectableSelectPlanes( selector, test, selectedPlaneCallback ) );
}

void Scene_forEachPlaneSelectable_selectReversedPlanes( scene::Graph& graph, Selector& selector, const SelectedPlanes& selectedPlanes ){
	graph.traverse( PlaneSelectableSelectReversedPlanes( selector, selectedPlanes ) );
}


class PlaneLess
{
public:
	bool operator()( const Plane3& plane, const Plane3& other ) const {
		return std::tie( plane.a, plane.b, plane.c, plane.d )
		     < std::tie( other.a, other.b, other.c, other.d );
	}
};

typedef std::set<Plane3, PlaneLess> PlaneSet;


class SelectedPlaneSet : public SelectedPlanes
{
	PlaneSet m_selectedPlanes;
public:
	bool empty() const {
		return m_selectedPlanes.empty();
	}

	void insert( const Plane3& plane ){
		m_selectedPlanes.insert( plane );
	}
	bool contains( const Plane3& plane ) const override {
		return m_selectedPlanes.contains( plane );
	}
	typedef MemberCaller<SelectedPlaneSet, void(const Plane3&), &SelectedPlaneSet::insert> InsertCaller;
};


bool Scene_forEachPlaneSelectable_selectPlanes( scene::Graph& graph, Selector& selector, SelectionTest& test ){
	SelectedPlaneSet selectedPlanes;

	Scene_forEachPlaneSelectable_selectPlanes( graph, selector, test, SelectedPlaneSet::InsertCaller( selectedPlanes ) );
	Scene_forEachPlaneSelectable_selectReversedPlanes( graph, selector, selectedPlanes );

	return !selectedPlanes.empty();
}



template<typename Functor>
class PlaneselectableVisibleSelectedVisitor : public SelectionSystem::Visitor
{
	const Functor& m_functor;
public:
	PlaneselectableVisibleSelectedVisitor( const Functor& functor ) : m_functor( functor ){
	}
	void visit( scene::Instance& instance ) const override {
		PlaneSelectable* planeSelectable = Instance_getPlaneSelectable( instance );
		if ( planeSelectable != 0
		     && instance.path().top().get().visible() ) {
			m_functor( *planeSelectable );
		}
	}
};

template<typename Functor>
inline const Functor& Scene_forEachVisibleSelectedPlaneselectable( const Functor& functor ){
	GlobalSelectionSystem().foreachSelected( PlaneselectableVisibleSelectedVisitor<Functor>( functor ) );
	return functor;
}

PlaneSelectable::BestPlaneData Scene_forEachPlaneSelectable_bestPlane( SelectionTest& test ){
	PlaneSelectable::BestPlaneData planeData;
	auto bestPlaneDirect = [&test, &planeData]( PlaneSelectable& planeSelectable ){
		planeSelectable.bestPlaneDirect( test, planeData );
	};
	Scene_forEachVisibleSelectedPlaneselectable( bestPlaneDirect );
	if( !planeData.valid() ){
		auto bestPlaneIndirect = [&test, &planeData]( PlaneSelectable& planeSelectable ){
			planeSelectable.bestPlaneIndirect( test, planeData );
		};
		Scene_forEachVisibleSelectedPlaneselectable( bestPlaneIndirect );
	}
	return planeData;
}

bool Scene_forEachPlaneSelectable_selectPlanes2( SelectionTest& test, TranslateAxis2& translateAxis ){
	const auto planeData = Scene_forEachPlaneSelectable_bestPlane( test );

	if( planeData.valid() ){
		const Plane3 plane = planeData.m_plane;
		if( planeData.direct() ){ // direct
			translateAxis.set0( point_on_plane( plane, test.getVolume().GetViewMatrix(), DeviceVector( 0, 0 ) ), plane );
		}
		else{ // indirect
			test.BeginMesh( g_matrix4_identity );
			/* may introduce some screen space offset in manipulatable to handle far-from-edge clicks perfectly; thought clicking not so far isn't too nasty, right? */
			translateAxis.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( planeData.m_closestPoint, 1 ) ) ), plane );
		}

		auto selectByPlane = [plane]( PlaneSelectable& planeSelectable ){
			planeSelectable.selectByPlane( plane );
		};
		Scene_forEachVisibleSelectedPlaneselectable( selectByPlane );
	}

	return planeData.valid();
}


PlaneSelectable::BestPlaneData Scene_forEachSelectedBrush_bestPlane( SelectionTest& test ){
	PlaneSelectable::BestPlaneData planeData;
	auto bestPlaneDirect = [&test, &planeData]( BrushInstance& brushInstance ){
		brushInstance.bestPlaneDirect( test, planeData );
	};
	Scene_forEachVisibleSelectedBrush( bestPlaneDirect );
	if( !planeData.valid() ){
		auto bestPlaneIndirect = [&test, &planeData]( BrushInstance& brushInstance ){
			brushInstance.bestPlaneIndirect( test, planeData );
		};
		Scene_forEachVisibleSelectedBrush( bestPlaneIndirect );
	}
	return planeData;
}

PlaneSelectable::BestPlaneData Scene_forEachBrush_bestPlane( SelectionTest& test ){
	if( g_SelectedFaceInstances.empty() ){
		return Scene_forEachSelectedBrush_bestPlane( test );
	}
	else{
		PlaneSelectable::BestPlaneData planeData;
		auto bestPlaneDirect = [&test, &planeData]( BrushInstance& brushInstance ){
			if( brushInstance.isSelected() || brushInstance.isSelectedComponents() )
				brushInstance.bestPlaneDirect( test, planeData );
		};

		Scene_forEachVisibleBrush( GlobalSceneGraph(), bestPlaneDirect );
		if( !planeData.valid() ){
			auto bestPlaneIndirect = [&test, &planeData]( BrushInstance& brushInstance ){
				if( brushInstance.isSelected() || brushInstance.isSelectedComponents() )
					brushInstance.bestPlaneIndirect( test, planeData );
			};
			Scene_forEachVisibleBrush( GlobalSceneGraph(), bestPlaneIndirect );
		}
		return planeData;
	}
}

bool Scene_forEachBrush_setupExtrude( SelectionTest& test, DragExtrudeFaces& extrudeFaces ){
	const auto planeData = Scene_forEachBrush_bestPlane( test );

	if( planeData.valid() ){
		const Plane3 plane = planeData.m_plane;
		if( planeData.direct() ){ // direct
			extrudeFaces.set0( point_on_plane( plane, test.getVolume().GetViewMatrix(), DeviceVector( 0, 0 ) ), plane );
		}
		else{ // indirect
			test.BeginMesh( g_matrix4_identity );
			/* may introduce some screen space offset in manipulatable to handle far-from-edge clicks perfectly; thought clicking not so far isn't too nasty, right? */
			extrudeFaces.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( planeData.m_closestPoint, 1 ) ) ), plane );
		}
		extrudeFaces.m_extrudeSources.clear();
		auto gatherExtrude = [plane, &extrudeFaces]( BrushInstance& brushInstance ){
			if( brushInstance.isSelected() || brushInstance.isSelectedComponents() ){
				bool m_pushed = false;
				auto gatherFaceInstances = [plane, &extrudeFaces, &brushInstance, &m_pushed]( FaceInstance& face ){
					if( face.isSelected() || plane3_equal( plane, face.getFace().plane3() ) ){
						if( !m_pushed ){
							extrudeFaces.m_extrudeSources.emplace_back();
							extrudeFaces.m_extrudeSources.back().m_brushInstance = &brushInstance;
							m_pushed = true;
						}
						extrudeFaces.m_extrudeSources.back().m_faces.emplace_back();
						extrudeFaces.m_extrudeSources.back().m_faces.back().m_face = &face.getFace();
						extrudeFaces.m_extrudeSources.back().m_faces.back().m_planepoints = face.getFace().getPlane().getPlanePoints();
					}
				};
				Brush_ForEachFaceInstance( brushInstance, gatherFaceInstances );

				brushInstance.setSelectedComponents( false, SelectionSystem::eFace );
				brushInstance.setSelected( false );
			}
		};
		Scene_forEachVisibleBrush( GlobalSceneGraph(), gatherExtrude );
	}

	return planeData.valid();
}


bool selection_selectVerticesOrFaceVertices( SelectionTest& test ){
	{	/* try to hit vertices */
		DeepBestSelector deepSelector;
		Scene_TestSelect_Component_Selected( deepSelector, test, test.getVolume(), SelectionSystem::eVertex );
		if( !deepSelector.best().empty() ){
			for ( Selectable* s : deepSelector.best() )
				s->setSelected( true );
			return true;
		}
	}
	/* otherwise select vertices of brush faces, which lay on best plane */
	const auto planeData = Scene_forEachSelectedBrush_bestPlane( test );

	if( planeData.valid() ){
		auto selectVerticesOnPlane = [plane = planeData.m_plane]( BrushInstance& brushInstance ){
			brushInstance.selectVerticesOnPlane( plane );
		};
		Scene_forEachVisibleSelectedBrush( selectVerticesOnPlane );
	}
	return planeData.valid();
}


bool scene_insert_brush_vertices( const View& view, TranslateFreeXY_Z& freeDragXY_Z ){
	SelectionVolume test( view );
	ScenePointSelector selector;
	if( view.fill() )
		Scene_forEachVisible_testselect_scene_point( view, selector, test );
	else
		Scene_forEachVisible_testselect_scene_point_selected_brushes( view, selector, test );
	test.BeginMesh( g_matrix4_identity, true );
	if( selector.isSelected() ){
		freeDragXY_Z.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, selector.best().depth(), 1 ) ) ) );
		DoubleVector3 point = testSelected_scene_snapped_point( test, selector );
		if( !view.fill() ){
			point -= view.getViewDir() * GetGridSize();
		}
		Brush::VertexModeVertices vertexModeVertices;
		vertexModeVertices.push_back( Brush::VertexModeVertex( point, true ) );
		if( selector.face() )
			vertexModeVertices.back().m_faces.push_back( selector.face() );

		UndoableCommand undo( "InsertBrushVertices" );
		Scene_forEachSelectedBrush( [&vertexModeVertices]( BrushInstance& brush ){ brush.insert_vertices( vertexModeVertices ); } );
		return true;
	}
	else if( !view.fill() ){ //+two points
		freeDragXY_Z.set0( g_vector3_identity );
		const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
		if( aabb_valid( bounds ) ){
			DoubleVector3 xy = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, 0, 1 ) ) );
			vector3_snap( xy, GetSnapGridSize() );
			DoubleVector3 a( xy ), b( xy );
			const std::size_t max = vector3_max_abs_component_index( view.getViewDir() );
			a[max] = bounds.origin[max] + bounds.extents[max];
			b[max] = bounds.origin[max] - bounds.extents[max];
			Brush::VertexModeVertices vertexModeVertices;
			vertexModeVertices.push_back( Brush::VertexModeVertex( a, true ) );
			vertexModeVertices.push_back( Brush::VertexModeVertex( b, true ) );

			UndoableCommand undo( "InsertBrushVertices" );
			Scene_forEachSelectedBrush( [&vertexModeVertices]( BrushInstance& brush ){ brush.insert_vertices( vertexModeVertices ); } );
			return true;
		}
	}
	return false;
}


template<typename Functor>
class ComponentSelectionTestableVisibleSelectedVisitor : public SelectionSystem::Visitor
{
	const Functor& m_functor;
public:
	ComponentSelectionTestableVisibleSelectedVisitor( const Functor& functor ) : m_functor( functor ){
	}
	void visit( scene::Instance& instance ) const override {
		ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( instance );
		if ( componentSelectionTestable != 0
		  && instance.path().top().get().visible() ) {
			m_functor( *componentSelectionTestable );
		}
	}
};

template<typename Functor>
inline const Functor& Scene_forEachVisibleSelectedComponentSelectionTestable( const Functor& functor ){
	GlobalSelectionSystem().foreachSelected( ComponentSelectionTestableVisibleSelectedVisitor<Functor>( functor ) );
	return functor;
}


class ResizeTranslatable : public Translatable
{
	void translate( const Vector3& translation ) override {
		Scene_Translate_Component_Selected( GlobalSceneGraph(), translation );
	}
};

class DragManipulatorImpl final : public DragManipulator
{
	ResizeTranslatable m_resize;
	TranslateFree m_freeResize;
	TranslateAxis2 m_axisResize;
	TranslateFreeXY_Z m_freeDragXY_Z;
	DragNewBrush m_dragNewBrush;
	DragExtrudeFaces m_dragExtrudeFaces;
	bool m_dragSelected; //drag selected primitives or components
	bool m_selected; //components selected temporally for drag
	bool m_selected2; //planeselectables in cam with alt
	bool m_newBrush;
	bool m_extrudeFaces;
public:
	DragManipulatorImpl( Translatable& translatable, AllTransformable& transformable ) :
		m_resize(), m_freeResize( m_resize ), m_axisResize( m_resize ), m_freeDragXY_Z( translatable, transformable ), m_renderCircle( 2 << 3 ){
		setSelected( false );
		draw_circle( m_renderCircle.m_vertices.size() >> 3, 5, m_renderCircle.m_vertices.data(), RemapXYZ() );
	}

	Manipulatable* GetManipulatable() override {
		if( m_newBrush )
			return &m_dragNewBrush;
		else if( m_extrudeFaces )
			return &m_dragExtrudeFaces;
		else if( m_selected )
			return &m_freeResize;
		else if( m_selected2 )
			return &m_axisResize;
		else
			return &m_freeDragXY_Z;
	}

	void testSelect( const View& view, const Matrix4& pivot2world ) override {
		SelectionPool selector;
		SelectionVolume test( view );

		if( g_modifiers == ( c_modifierAlt | c_modifierControl )
		 && GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive
		 && ( GlobalSelectionSystem().countSelected() != 0 || !g_SelectedFaceInstances.empty() ) ){ // extrude
			m_extrudeFaces = Scene_forEachBrush_setupExtrude( test, m_dragExtrudeFaces );
		}
		else if( GlobalSelectionSystem().countSelected() != 0 ){
			if ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ){
				if( g_modifiers == c_modifierAlt ){
					if( view.fill() ){ // alt resize
						m_selected2 = Scene_forEachPlaneSelectable_selectPlanes2( test, m_axisResize );
					}
					else{ // alt vertices drag
						m_selected = selection_selectVerticesOrFaceVertices( test );
					}
				}
				else if( g_modifiers == c_modifierNone ){
					BooleanSelector booleanSelector;
					Scene_TestSelect_Primitive( booleanSelector, test, view );

					if ( booleanSelector.isSelected() ) { /* hit a primitive */
						m_dragSelected = true; /* drag a primitive */
						test.BeginMesh( g_matrix4_identity, true );
						m_freeDragXY_Z.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, booleanSelector.bestIntersection().depth(), 1 ) ) ) );
					}
					else{ /* haven't hit a primitive */
						m_selected = Scene_forEachPlaneSelectable_selectPlanes( GlobalSceneGraph(), selector, test ); /* select faces on planeSelectables */
					}
				}
			}
			else if( g_modifiers == c_modifierNone ){ // components
				BestSelector bestSelector;
				Scene_TestSelect_Component_Selected( bestSelector, test, view, GlobalSelectionSystem().ComponentMode() ); /* drag components */
				for ( Selectable* s : bestSelector.best() ){
					if ( !s->isSelected() )
						GlobalSelectionSystem().setSelectedAllComponents( false );
					selector.addSelectable( SelectionIntersection( 0, 0 ), s );
					m_dragSelected = true;
				}
				if( bestSelector.bestIntersection().valid() ){
					test.BeginMesh( g_matrix4_identity, true );
					m_freeDragXY_Z.set0( vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, bestSelector.bestIntersection().depth(), 1 ) ) ) );
				}
				else{
					if( GlobalSelectionSystem().countSelectedComponents() != 0 ){ /* drag, even if hit nothing, but got selected */
						m_dragSelected = true;
						m_freeDragXY_Z.set0( g_vector3_identity );
					}
					else if( GlobalSelectionSystem().ComponentMode() == SelectionSystem::eVertex ){ /* otherwise insert */
						m_dragSelected = g_bTmpComponentMode = scene_insert_brush_vertices( view, m_freeDragXY_Z ); //hack: indicating not a tmp mode
						return;
					}
				}
			}

			for ( SelectableSortedSet::value_type& value : selector )
				value.second->setSelected( true );
			g_bTmpComponentMode = m_selected | m_selected2;
		}
		else if( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive && g_3DCreateBrushes && g_modifiers == c_modifierNone ){
			m_newBrush = true;
			BestPointSelector bestPointSelector;
			Scene_TestSelect_Primitive( bestPointSelector, test, view );
			Vector3 start;
			test.BeginMesh( g_matrix4_identity, true );
			if( bestPointSelector.isSelected() ){
				start = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, bestPointSelector.best().depth(), 1 ) ) );
			}
			else{
				const Vector3 pnear = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, -1, 1 ) ) );
				const Vector3 pfar = vector4_projected( matrix4_transformed_vector4( test.getScreen2world(), Vector4( 0, 0, 1, 1 ) ) );
				start = vector3_normalised( pfar - pnear ) * ( 256.f + GetGridSize() * sqrt( 3.0 ) ) + pnear;
			}
			vector3_snap( start, GetSnapGridSize() );
			m_dragNewBrush.set0( start );
		}
	}

	void setSelected( bool select ) override {
		m_dragSelected = select;
		m_selected = select;
		m_selected2 = select;
		m_newBrush = select;
		m_extrudeFaces = select;
	}
	bool isSelected() const override {
		return m_dragSelected || m_selected || m_selected2 || m_newBrush || m_extrudeFaces;
	}

	void render( Renderer& renderer, const VolumeTest& volume, const Matrix4& pivot2world ) override {
		if( !m_polygons.empty() ){
			renderer.SetState( m_state_wire, Renderer::eWireframeOnly );
			renderer.SetState( m_state_wire, Renderer::eFullMaterials );
			if( m_polygons.back().size() == 1 ){
				Pivot2World_viewplaneSpace( m_renderCircle.m_viewplaneSpace, matrix4_translation_for_vec3( m_polygons.back()[0] ), volume.GetModelview(), volume.GetProjection(), volume.GetViewport() );
				renderer.addRenderable( m_renderCircle, m_renderCircle.m_viewplaneSpace );
			}
			else{
				renderer.addRenderable( m_renderPoly, g_matrix4_identity );
			}
		}
	}
	void highlight( const View& view, const Matrix4& pivot2world ) override {
		SelectionVolume test( view );
		std::vector<std::vector<Vector3>> polygons;
		/* conditions structure respects one in testSelect() */
		if( g_modifiers == ( c_modifierAlt | c_modifierControl )
		 && GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive
		 && ( GlobalSelectionSystem().countSelected() != 0 || !g_SelectedFaceInstances.empty() ) ){ // extrude
			if( const auto planeData = Scene_forEachBrush_bestPlane( test ); planeData.valid() ){
				auto gatherPolygonsByPlane = [plane = planeData.m_plane, &polygons]( BrushInstance& brushInstance ){
					if( brushInstance.isSelected() || brushInstance.isSelectedComponents() )
						brushInstance.gatherPolygonsByPlane( plane, polygons, false );
				};
				Scene_forEachVisibleBrush( GlobalSceneGraph(), gatherPolygonsByPlane );
			}
		}
		else if( GlobalSelectionSystem().countSelected() != 0 ){
			if ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive ){
				if( g_modifiers == c_modifierAlt ){
					if( view.fill() ){ // alt resize
						if( const auto planeData = Scene_forEachPlaneSelectable_bestPlane( test ); planeData.valid() ){
							auto gatherPolygonsByPlane = [plane = planeData.m_plane, &polygons]( PlaneSelectable& planeSelectable ){
								planeSelectable.gatherPolygonsByPlane( plane, polygons );
							};
							Scene_forEachVisibleSelectedPlaneselectable( gatherPolygonsByPlane );
						}
					}
					else{ // alt vertices drag
						SelectionIntersection intersection;
						const SelectionSystem::EComponentMode mode = SelectionSystem::eVertex;
						auto gatherComponentsHighlight = [&polygons, &intersection, &test, mode]( const ComponentSelectionTestable& componentSelectionTestable ){
							componentSelectionTestable.gatherComponentsHighlight( polygons, intersection, test, mode );
						};
						Scene_forEachVisibleSelectedComponentSelectionTestable( gatherComponentsHighlight );

						if( polygons.empty() ){
							if( const auto planeData = Scene_forEachSelectedBrush_bestPlane( test ); planeData.valid() ){
								auto gatherPolygonsByPlane = [plane = planeData.m_plane, &polygons]( BrushInstance& brushInstance ){
									brushInstance.gatherPolygonsByPlane( plane, polygons );
								};
								Scene_forEachVisibleSelectedBrush( gatherPolygonsByPlane );
							}
						}
					}
				}
			}
			else if( g_modifiers == c_modifierNone // components
				|| g_modifiers == c_modifierShift // hack: these respect to the RadiantSelectionSystem::SelectPoint
				|| ( g_modifiers == c_modifierControl && GlobalSelectionSystem().ComponentMode() == SelectionSystem::EComponentMode::eFace ) ){
				SelectionIntersection intersection;
				const SelectionSystem::EComponentMode mode = GlobalSelectionSystem().ComponentMode();
				auto gatherComponentsHighlight = [&polygons, &intersection, &test, mode]( const ComponentSelectionTestable& componentSelectionTestable ){
					componentSelectionTestable.gatherComponentsHighlight( polygons, intersection, test, mode );
				};
				Scene_forEachVisibleSelectedComponentSelectionTestable( gatherComponentsHighlight );
			}
		}

		if( m_polygons != polygons ){
			m_polygons.swap( polygons );
			SceneChangeNotify();
		}
	}
private:
	std::vector<std::vector<Vector3>> m_polygons;
	struct RenderablePoly: public OpenGLRenderable
	{
		const std::vector<std::vector<Vector3>>& m_polygons;

		RenderablePoly( const std::vector<std::vector<Vector3>>& polygons ) : m_polygons( polygons ){
		}
		void render( RenderStateFlags state ) const override {
			gl().glPolygonOffset( -2, -2 );
			for( const auto& poly : m_polygons ){
				gl().glVertexPointer( 3, GL_FLOAT, sizeof( m_polygons[0][0] ), poly[0].data() );
				gl().glDrawArrays( GL_POLYGON, 0, GLsizei( poly.size() ) );
			}
			gl().glPolygonOffset( -1, 1 ); // restore default
		}
	};
	RenderablePoly m_renderPoly{ m_polygons };
	struct RenderableCircle : public OpenGLRenderable
	{
		Array<PointVertex> m_vertices;
		Matrix4 m_viewplaneSpace;

		RenderableCircle( std::size_t size ) : m_vertices( size ){
		}
		void render( RenderStateFlags state ) const override {
			gl().glVertexPointer( 3, GL_FLOAT, sizeof( PointVertex ), &m_vertices.data()->vertex );
			gl().glDrawArrays( GL_LINE_LOOP, 0, GLsizei( m_vertices.size() ) );
		}
	};
	RenderableCircle m_renderCircle;
};


DragManipulator* New_DragManipulator( Translatable& translatable, AllTransformable& transformable ){
	return new DragManipulatorImpl( translatable, transformable );
}

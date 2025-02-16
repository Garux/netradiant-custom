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

#include "csg.h"

#include "debugging/debugging.h"

#include <list>

#include "map.h"
#include "brushmanip.h"
#include "brushnode.h"
#include "grid.h"
/*
void Face_makeBrush( Face& face, const Brush& brush, brush_vector_t& out, float offset ){
	if ( face.contributes() ) {
		out.push_back( new Brush( brush ) );
		Face* newFace = out.back()->addFace( face );
		face.getPlane().offset( -offset );
		face.planeChanged();
		if ( newFace != 0 ) {
			newFace->flipWinding();
			newFace->getPlane().offset( offset );
			newFace->planeChanged();
		}
	}
}

void Face_extrude( Face& face, const Brush& brush, brush_vector_t& out, float offset ){
	if ( face.contributes() ) {
		face.getPlane().offset( offset );
		out.push_back( new Brush( brush ) );
		face.getPlane().offset( -offset );
		Face* newFace = out.back()->addFace( face );
		if ( newFace != 0 ) {
			newFace->flipWinding();
			newFace->planeChanged();
		}
	}
}
*/
#include "preferences.h"
#include "texwindow.h"
#include "filterbar.h"

enum EHollowType
{
	eDiag = 0,
	eWrap = 1,
	eExtrude = 2,
	ePull = 3,
};

class HollowSettings
{
public:
	EHollowType m_hollowType;
	float m_offset;
	// true = exclude by axis strategy, false = m_excludeSelectedFaces by 'selected' property
	bool m_excludeByAxis;
	Vector3 m_exclusionAxis;
	mutable double m_mindot;
	mutable double m_maxdot;
	// true = exclude selected faces, false = exclude unselected faces
	bool m_excludeSelectedFaces;
	mutable std::vector<DoubleVector3> m_exclude_vec; // list of excluded normals per brush

	bool m_caulk;
	bool m_removeInner;
	bool faceExcluded( const Face& face ) const {
		if( m_excludeByAxis ){
			const double dot = vector3_dot( face.getPlane().plane3().normal(), m_exclusionAxis );
			return dot < m_mindot + 0.001 || dot > m_maxdot - 0.001;
		}
		else{ // note: straight equality check: may explode, when used with modified faces (e.g. ePull tmpbrush offset faces forth and back) (works so far)
			return std::find( m_exclude_vec.begin(), m_exclude_vec.end(), face.getPlane().plane3().normal() ) != m_exclude_vec.end();
		}
	}
	void excludeFaces( BrushInstance& brushInstance ){
		if( m_excludeByAxis ) {
			m_mindot = m_maxdot = 0;
			Brush_forEachFace( brushInstance.getBrush(), *this );
		}
		else{
			m_exclude_vec.clear();
			Brush_ForEachFaceInstance( brushInstance, *this );
		}
	}
	void operator()( Face& face ) const {
		const double dot = vector3_dot( face.getPlane().plane3().normal(), m_exclusionAxis );
		if( dot < m_mindot ) {
			m_mindot = dot;
		}
		else if( dot > m_maxdot ) {
			m_maxdot = dot;
		}
	}
	void operator()( FaceInstance& face ) const {
		if( m_excludeSelectedFaces == face.isSelected() ) {
			m_exclude_vec.push_back( face.getFace().getPlane().plane3().normal() );
		}
	}
};


class CaulkFace {
	const HollowSettings& m_settings;
public:
	CaulkFace( const HollowSettings& settings ):
		m_settings( settings ) {
	}
	void operator()( Face& face ) const {
		if( !m_settings.faceExcluded( face ) ) {
			face.SetShader( GetCaulkShader() );
		}
	}
};

class FaceMakeBrush {
	const Brush& m_brush;
	brush_vector_t& m_out;
	const HollowSettings& m_settings;
public:
	FaceMakeBrush( const Brush& brush, brush_vector_t& out, const HollowSettings& settings )
		: m_brush( brush ),
		  m_out( out ),
		  m_settings( settings ) {
	}
	void operator()( Face& face ) const {
		if( !m_settings.faceExcluded( face ) ) {
			if( m_settings.m_hollowType == ePull ) {
				if( face.contributes() ) {
					face.getPlane().offset( m_settings.m_offset );
					face.planeChanged();
					m_out.push_back( new Brush( m_brush ) );
					face.getPlane().offset( -m_settings.m_offset );
					face.planeChanged();

					if( m_settings.m_caulk ) {
						Brush_forEachFace( *m_out.back(), CaulkFace( m_settings ) );
					}
					Face* newFace = m_out.back()->addFace( face );
					if( newFace != 0 ) {
						newFace->flipWinding();
					}
				}
			}
			else if( m_settings.m_hollowType == eWrap ) {
				//Face_makeBrush( face, brush, m_out, offset );
				if( face.contributes() ) {
					face.undoSave();
					m_out.push_back( new Brush( m_brush ) );
					if( !m_settings.m_removeInner && m_settings.m_caulk )
						face.SetShader( GetCaulkShader() );
					Face* newFace = m_out.back()->addFace( face );
					face.getPlane().offset( -m_settings.m_offset );
					face.planeChanged();
					if( m_settings.m_caulk )
						face.SetShader( GetCaulkShader() );
					if( newFace != 0 ) {
						newFace->flipWinding();
						newFace->getPlane().offset( m_settings.m_offset );
						newFace->planeChanged();
					}
				}
			}
			else if( m_settings.m_hollowType == eExtrude ) {
				if( face.contributes() ) {
					//face.undoSave();
					m_out.push_back( new Brush( m_brush ) );
					m_out.back()->clear();

					Face* newFace = m_out.back()->addFace( face );
					if( newFace != 0 ) {
						newFace->getPlane().offset( m_settings.m_offset );
						newFace->planeChanged();
					}

					if( !m_settings.m_removeInner && m_settings.m_caulk )
						face.SetShader( GetCaulkShader() );
					newFace = m_out.back()->addFace( face );
					if( newFace != 0 ) {
						newFace->flipWinding();
					}
					Winding& winding = face.getWinding();
					TextureProjection projection;
					TexDef_Construct_Default( projection );
					for( std::size_t index = 0; index < winding.numpoints; ++index ){
						const std::size_t next = Winding_next( winding, index );
						m_out.back()->addPlane( winding[index].vertex,
						                        winding[next].vertex,
						                        winding[next].vertex + face.getPlane().plane3().normal() * m_settings.m_offset,
						                        TextureBrowser_GetSelectedShader(),
						                        projection );
					}
				}
			}
		}
	}
};
/* brush0, brush2 are supposed to have same amount of faces in the same order; brush2 bigger than brush0 */
void brush_extrudeDiag( const Brush& brush0, const Brush& brush2, brush_vector_t& m_out, const HollowSettings& m_settings ){
	TextureProjection projection;
	TexDef_Construct_Default( projection );
	const char* shader = m_settings.m_caulk ? GetCaulkShader() : TextureBrowser_GetSelectedShader();

	for( Brush::const_iterator i0 = brush0.begin(); i0 != brush0.end(); ++i0 ){
		const Face& face0 = *( *i0 );
		const Face& face2 = *( *std::next( brush2.begin(), std::distance( brush0.begin(), i0 ) ) );
		if( !m_settings.faceExcluded( face0 ) ) {
			if( face0.contributes() ) {
				m_out.push_back( new Brush( brush0 ) );
				m_out.back()->clear();

				if( Face* newFace = m_out.back()->addFace( face0 ) ) {
					if( !m_settings.m_removeInner && m_settings.m_caulk ){
						newFace->SetShader( shader );
					}
					newFace->flipWinding();
				}

				if( face2.contributes() ){ //sew two valid windings
					m_out.back()->addFace( face2 );

					const auto addSidePlanes = [&m_out, shader, &projection]( const Winding& winding0, const Winding& winding2, const DoubleVector3 normal, const bool swap ){
						for( std::size_t index0 = 0; index0 < winding0.numpoints; ++index0 ){
							const std::size_t next = Winding_next( winding0, index0 );
							DoubleVector3 BestPoint;
							double bestdot = -1;
							for( std::size_t index2 = 0; index2 < winding2.numpoints; ++index2 ){
								const double dot = vector3_dot(
								                       vector3_normalised(
								                           vector3_cross(
								                               winding0[index0].vertex - winding0[next].vertex,
								                               winding0[index0].vertex - winding2[index2].vertex
								                           )
								                       ),
								                       normal
								                   );
								if( dot > bestdot ) {
									bestdot = dot;
									BestPoint = winding2[index2].vertex;
								}
							}
							m_out.back()->addPlane( winding0[swap? next : index0].vertex,
							                        winding0[swap? index0 : next].vertex,
							                        BestPoint,
							                        shader,
							                        projection );
						}
					};
					//insert side planes from each winding perspective, as their form may change after brush expansion
					addSidePlanes( face0.getWinding(), face2.getWinding(), face0.getPlane().plane3().normal(), false );
					addSidePlanes( face2.getWinding(), face0.getWinding(), face0.getPlane().plane3().normal(), true );
				}
				else{ //one valid winding: this way may produce garbage with complex brushes, extruded partially, but does preferred result with simple ones
					const Winding& winding0 = face0.getWinding();
					for( std::size_t index0 = 0; index0 < winding0.numpoints; ++index0 ){
						const std::size_t next = Winding_next( winding0, index0 );
						DoubleVector3 BestPoint;
						double bestdist = 999999;
						for( const Face* f : brush2 ) {
							const Winding& winding2 = f->getWinding();
							for( std::size_t index2 = 0; index2 < winding2.numpoints; ++index2 ){
								const double testdist = vector3_length( winding0[index0].vertex - winding2[index2].vertex );
								if( testdist < bestdist && plane3_distance_to_point( face0.getPlane().plane3(), winding2[index2].vertex ) > .05 ) {
									bestdist = testdist;
									BestPoint = winding2[index2].vertex;
								}
							}
						}
						m_out.back()->addPlane( winding0[index0].vertex,
						                        winding0[next].vertex,
						                        BestPoint,
						                        shader,
						                        projection );
					}
				}
			}
		}
	}
}

class FaceOffset {
	const HollowSettings& m_settings;
public:
	FaceOffset( const HollowSettings& settings )
		: m_settings( settings ) {
	}
	void operator()( Face& face ) const {
		if( !m_settings.faceExcluded( face ) ) {
			face.undoSave();
			face.getPlane().offset( m_settings.m_offset );
			face.planeChanged();
		}
	}
};


class BrushHollowSelectedWalker : public scene::Graph::Walker {
	HollowSettings& m_settings;
public:
	BrushHollowSelectedWalker( HollowSettings& settings )
		: m_settings( settings ) {
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if( path.top().get().visible() ) {
			Brush* brush = Node_getBrush( path.top() );
			if( brush != 0
			 && ( Instance_isSelected( instance ) || Instance_isSelectedComponents( instance ) ) ) {
				m_settings.excludeFaces( *Instance_getBrush( instance ) );
				brush_vector_t out;

				if( m_settings.m_hollowType == ePull ) {
					if( !m_settings.m_removeInner && m_settings.m_caulk ) {
						Brush_forEachFace( *brush, CaulkFace( m_settings ) );
					}
					Brush* tmpbrush = new Brush( *brush );
					tmpbrush->removeEmptyFaces();
					Brush_forEachFace( *tmpbrush, FaceMakeBrush( *tmpbrush, out, m_settings ) );
					delete tmpbrush;
				}
				else if( m_settings.m_hollowType == eDiag ) {
					Brush* tmpbrush = new Brush( *brush );
					Brush_forEachFace( *tmpbrush, FaceOffset( m_settings ) );
					tmpbrush->evaluateBRep();
					brush_extrudeDiag( *brush, *tmpbrush, out, m_settings );
					delete tmpbrush;
					if( !m_settings.m_removeInner && m_settings.m_caulk ) {
						Brush_forEachFace( *brush, CaulkFace( m_settings ) );
					}
				}
				else {
					Brush_forEachFace( *brush, FaceMakeBrush( *brush, out, m_settings ) );
				}
				for( Brush* b : out ) {
					b->removeEmptyFaces();
					if( b->hasContributingFaces() ) {
						NodeSmartReference node( ( new BrushNode() )->node() );
						Node_getBrush( node )->copy( *b );
						Node_getTraversable( path.parent() )->insert( node );
						//path.push( makeReference( node.get() ) );
						//selectPath( path, true );
						//Instance_getSelectable( *GlobalSceneGraph().find( path ) )->setSelected( true );
						//Path_deleteTop( path );
					}
					delete b;
				}
			}
		}
		return true;
	}
};

typedef std::list<Brush*> brushlist_t;

class BrushGatherSelected : public scene::Graph::Walker
{
	brush_vector_t& m_brushlist;
public:
	BrushGatherSelected( brush_vector_t& brushlist )
		: m_brushlist( brushlist ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.top().get().visible() && Instance_isSelected( instance ) )
			if ( Brush* brush = Node_getBrush( path.top() ) )
				m_brushlist.push_back( brush );
		return true;
	}
};
/*
class BrushDeleteSelected : public scene::Graph::Walker
{
public:
bool pre( const scene::Path& path, scene::Instance& instance ) const {
	return true;
}
void post( const scene::Path& path, scene::Instance& instance ) const {
	if ( path.top().get().visible() ) {
		Brush* brush = Node_getBrush( path.top() );
		if ( brush != 0
			 && Instance_isSelected( instance )
			 && path.size() > 1 ) {
			Path_deleteTop( path );
		}
	}
}
};
*/
#include "ientity.h"

class BrushDeleteSelected : public scene::Graph::Walker
{
	scene::Node* m_keepNode;
	scene::Node* m_world = Map_FindWorldspawn( g_map );
	mutable bool m_eraseParent = false;
public:
	BrushDeleteSelected( scene::Node* keepNode = nullptr ): m_keepNode( keepNode ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		if ( Node_isBrush( path.top() ) ) {
			if ( path.top().get().visible()
			  && Instance_isSelected( instance )
			  && path.top().get_pointer() != m_keepNode ) {
				scene::Node& parent = path.parent();
				Path_deleteTop( path );
				m_eraseParent = Node_getTraversable( parent )->empty();
			}
		}
		else if( m_eraseParent ){
			m_eraseParent = false;
			if ( path.top().get_pointer() != m_world && path.top().get_pointer() != m_keepNode )
				Path_deleteTop( path );
		}
	}
};


template<typename Type>
class RemoveReference
{
public:
	typedef Type type;
};

template<typename Type>
class RemoveReference<Type&>
{
public:
	typedef Type type;
};

template<typename Functor>
class Dereference
{
	const Functor& functor;
public:
	Dereference( const Functor& functor ) : functor( functor ){
	}
	get_result_type<Functor> operator()( typename RemoveReference<get_argument<Functor, 0>>::type *firstArgument ) const {
		return functor( *firstArgument );
	}
};

template<typename Functor>
inline Dereference<Functor> makeDereference( const Functor& functor ){
	return Dereference<Functor>( functor );
}

typedef Face* FacePointer;
const FacePointer c_nullFacePointer = 0;

template<typename Predicate>
Face* Brush_findIf( const Brush& brush, const Predicate& predicate ){
	Brush::const_iterator i = std::find_if( brush.begin(), brush.end(), makeDereference( predicate ) );
	return i == brush.end() ? c_nullFacePointer : *i; // uses c_nullFacePointer instead of 0 because otherwise gcc 4.1 attempts conversion to int
}

template<typename Caller>
class BindArguments1
{
	typedef get_argument<Caller, 1> FirstBound;
	FirstBound firstBound;
public:
	BindArguments1( FirstBound firstBound )
		: firstBound( firstBound ){
	}
	get_result_type<Caller> operator()( get_argument<Caller, 0> firstArgument ) const {
		return Caller::call( firstArgument, firstBound );
	}
};

template<typename Caller>
class BindArguments2
{
	typedef get_argument<Caller, 1> FirstBound;
	typedef get_argument<Caller, 2> SecondBound;
	FirstBound firstBound;
	SecondBound secondBound;
public:
	BindArguments2( FirstBound firstBound, SecondBound secondBound )
		: firstBound( firstBound ), secondBound( secondBound ){
	}
	get_result_type<Caller> operator()( get_argument<Caller, 0> firstArgument ) const {
		return Caller::call( firstArgument, firstBound, secondBound );
	}
};

template<typename Caller, typename FirstBound, typename SecondBound>
BindArguments2<Caller> bindArguments( const Caller& caller, FirstBound firstBound, SecondBound secondBound ){
	return BindArguments2<Caller>( firstBound, secondBound );
}

inline bool Face_testPlane( const Face& face, const Plane3& plane, bool flipped ){
	return face.contributes() && !Winding_TestPlane( face.getWinding(), plane, flipped );
}
typedef Function<bool(const Face&, const Plane3&, bool), Face_testPlane> FaceTestPlane;



/// \brief Returns true if
/// \li !flipped && brush is BACK or ON
/// \li flipped && brush is FRONT or ON
bool Brush_testPlane( const Brush& brush, const Plane3& plane, bool flipped ){
	brush.evaluateBRep();
#if 1
	for ( Brush::const_iterator i( brush.begin() ); i != brush.end(); ++i )
	{
		if ( Face_testPlane( *( *i ), plane, flipped ) ) {
			return false;
		}
	}
	return true;
#else
	return Brush_findIf( brush, bindArguments( FaceTestPlane(), makeReference( plane ), flipped ) ) == 0;
#endif
}

brushsplit_t Brush_classifyPlane( const Brush& brush, const Plane3& plane ){
	brush.evaluateBRep();
	brushsplit_t split;
	for ( Brush::const_iterator i( brush.begin() ); i != brush.end(); ++i )
	{
		if ( ( *i )->contributes() ) {
			split += Winding_ClassifyPlane( ( *i )->getWinding(), plane );
		}
	}
	return split;
}

bool Brush_subtract( const Brush& brush, const Brush& other, const std::vector<const Face *>& otherfaces, brush_vector_t& ret_fragments ){
	if ( aabb_intersects_aabb( brush.localAABB(), other.localAABB() ) ) {
		brush_vector_t fragments;
		fragments.reserve( other.size() );
		Brush back( brush );

		for ( const Face* face : otherfaces )
		{
			brushsplit_t split = Brush_classifyPlane( back, face->plane3() );
			if ( split.counts[ePlaneFront] != 0
			  && split.counts[ePlaneBack] != 0 ) {
				fragments.push_back( new Brush( back ) );
				if ( Face* newFace = fragments.back()->addFace( *face ) ) {
					newFace->flipWinding();
				}
				back.addFace( *face );
			}
			else if ( split.counts[ePlaneBack] == 0 ) {
				for ( Brush* frag : fragments )
					delete( frag );
				return false;
			}
		}
		ret_fragments.insert( ret_fragments.end(), fragments.begin(), fragments.end() );
		return true;
	}
	return false;
}

class SubtractBrushesFromUnselected : public scene::Graph::Walker
{
	const brush_vector_t& m_brushlist;
	std::vector<std::vector<const Face *>> m_brushfaces; // m_brushfaces.size() == m_brushlist.size()
	std::size_t& m_before;
	std::size_t& m_after;
	mutable bool m_eraseParent = false;
	scene::Node* m_world = Map_FindWorldspawn( g_map );
public:
	SubtractBrushesFromUnselected( const brush_vector_t& brushlist, std::size_t& before, std::size_t& after )
		: m_brushlist( brushlist ), m_before( before ), m_after( after )
	{
		std::vector<ProjectionAxis> doneProjections;
		doneProjections.reserve( 3 );
		m_brushfaces.reserve( m_brushlist.size() );
		for( const Brush* brush : m_brushlist )
		{
			doneProjections.clear();
			auto& faces = m_brushfaces.emplace_back();
			faces.reserve( brush->size() );
			for( const Face* face : *brush )
				if( face->contributes() )
					faces.push_back( face );

			std::ranges::sort( faces, []( const Face* a, const Face* b ){
				const DoubleVector3& n1 = a->getPlane().plane3().normal();
				const DoubleVector3& n2 = b->getPlane().plane3().normal();
				const ProjectionAxis p1 = projectionaxis_for_normal( n1 );
				const ProjectionAxis p2 = projectionaxis_for_normal( n2 );
				return float_equal_epsilon( fabs( n1[ p1 ] ), fabs( n2[ p2 ] ), c_PLANE_NORMAL_EPSILON )
				? p1 > p2 // Z > Y > X
				: fabs( n1[ p1 ] ) > fabs( n2[ p2 ] ); // or most axial
			} );

			auto it = faces.cbegin(), found = it; // traverse projections and craft more fortunate splits order in non trivial cases
			while( ( found = std::ranges::find_if( faces, [&doneProjections]( const Face* face ){
				return std::ranges::find( doneProjections, projectionaxis_for_normal( face->getPlane().plane3().normal() ) ) == doneProjections.cend();
			} ) ) != faces.cend() ){
				auto moveit = [&]( decltype( it ) move ){ // moves face forward to prioritize
					const Face* face = *move;
					faces.erase( move );
					faces.insert( it++, face ); // postincrement: 'it' ends up right after the insertion
				};
				moveit( found );
				const DoubleVector3& n1 = ( *( it - 1 ) )->getPlane().plane3().normal();
				const ProjectionAxis p1 = projectionaxis_for_normal( n1 );
				doneProjections.push_back( p1 );
				// find opposite face
				auto more = faces.cend();
				double bestmax = 0;
				double bestdot = 1;
				for( auto face = it; face != faces.cend(); ++face ){
					const DoubleVector3& n2 = ( *face )->getPlane().plane3().normal();
					const ProjectionAxis p2 = projectionaxis_for_normal( n2 );
					if( p1 == p2 // same projection
					&& n1[p1] * n2[p2] < 0 // opposite projection facing
					&& ( fabs( n2[p2] ) > bestmax + c_PLANE_NORMAL_EPSILON // definitely better proj direction
					   || ( fabs( n2[p2] ) > bestmax - c_PLANE_NORMAL_EPSILON // or similar proj direction
					     && vector3_dot( n1, n2 ) < bestdot ) ) ){ // + more opposing normal direction
						bestmax = fabs( n2[p2] );
						bestdot = vector3_dot( n1, n2 );
						more = face;
					}
				}
				if( more != faces.end() ){
					moveit( more );
				}
			}
		}
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		return path.top().get().visible();
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		if ( Brush* thebrush = Node_getBrush( path.top() ) ) {
			if ( path.top().get().visible() && !Instance_isSelected( instance )
			&& std::any_of( m_brushlist.cbegin(), m_brushlist.cend(),
			[thebrush]( const Brush *b ){ return aabb_intersects_aabb( thebrush->localAABB(), b->localAABB() ); } ) ) {
				brush_vector_t buffer[2];
				bool swap = false;
				Brush* original = new Brush( *thebrush );
				buffer[swap].push_back( original );

				for ( size_t i = 0; i < m_brushlist.size(); ++i )
				{
					for ( Brush *brush : buffer[swap] )
					{
						if ( Brush_subtract( *brush, *m_brushlist[i], m_brushfaces[i], buffer[!swap] ) ) {
							delete brush;
						}
						else
						{
							buffer[!swap].push_back( brush );
						}
					}
					buffer[swap].clear();
					swap = !swap;
				}

				brush_vector_t& out = buffer[swap];

				if ( out.size() == 1 && out.back() == original ) {
					delete original;
				}
				else
				{
					++m_before;
					for ( Brush *brush : out )
					{
						++m_after;
						brush->removeEmptyFaces();
						if ( !brush->empty() ) {
							NodeSmartReference node( ( new BrushNode() )->node() );
							Node_getBrush( node )->copy( *brush );
							delete brush;
							Node_getTraversable( path.parent() )->insert( node );
						}
						else{
							delete brush;
						}
					}
					scene::Node& parent = path.parent();
					Path_deleteTop( path );
					m_eraseParent = Node_getTraversable( parent )->empty();
				}
			}
		}
		else if( m_eraseParent ){
			m_eraseParent = false;
			if ( path.top().get_pointer() != m_world )
				Path_deleteTop( path );
		}
	}
};

void CSG_Subtract(){
	brush_vector_t selected_brushes;
	GlobalSceneGraph().traverse( BrushGatherSelected( selected_brushes ) );

	if ( selected_brushes.empty() ) {
		globalWarningStream() << "CSG Subtract: No brushes selected.\n";
	}
	else
	{
		globalOutputStream() << "CSG Subtract: Subtracting " << selected_brushes.size() << " brushes.\n";

		UndoableCommand undo( "brushSubtract" );

		// subtract selected from unselected
		std::size_t before = 0;
		std::size_t after = 0;
		GlobalSceneGraph().traverse( SubtractBrushesFromUnselected( selected_brushes, before, after ) );
		globalOutputStream() << "CSG Subtract: Result: "
		                     << after << ( after == 1 ? " fragment" : " fragments" )
		                     << " from " << before << ( before == 1 ? " brush.\n" : " brushes.\n" );

		SceneChangeNotify();
	}
}

#include "clippertool.h"
class BrushSplitByPlaneSelected : public scene::Graph::Walker
{
	const ClipperPoints m_points;
	const Plane3 m_plane; /* plane to insert */
	const char* m_shader;
	const TextureProjection& m_projection;
	const bool m_split; /* split or clip */
public:
	mutable bool m_gj;
	BrushSplitByPlaneSelected( const ClipperPoints& points, bool flip, const char* shader, const TextureProjection& projection, bool split ) :
		m_points( flip? ClipperPoints( points[0], points[2], points[1], points._count ) : points ),
		m_plane( plane3_for_points( m_points[0], m_points[1], m_points[2] ) ),
		m_shader( shader ), m_projection( projection ), m_split( split ), m_gj( false ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		return true;
	}
	void post( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.top().get().visible() ) {
			Brush* brush = Node_getBrush( path.top() );
			if ( brush != 0
			  && Instance_isSelected( instance ) ) {
				const brushsplit_t split = Brush_classifyPlane( *brush, m_plane );
				if ( split.counts[ePlaneBack] && split.counts[ePlaneFront] ) {
					// the plane intersects this brush
					m_gj = true;
					if ( m_split ) {
						NodeSmartReference node( ( new BrushNode() )->node() );
						Brush* fragment = Node_getBrush( node );
						fragment->copy( *brush );
						fragment->addPlane( m_points[0], m_points[2], m_points[1], m_shader, m_projection );
						fragment->removeEmptyFaces();
						ASSERT_MESSAGE( !fragment->empty(), "brush left with no faces after split" );

						Node_getTraversable( path.parent() )->insert( node );
						{
							scene::Path fragmentPath = path;
							fragmentPath.top() = makeReference( node.get() );
							selectPath( fragmentPath, true );
						}
					}

					brush->addPlane( m_points[0], m_points[1], m_points[2], m_shader, m_projection );
					brush->removeEmptyFaces();
					ASSERT_MESSAGE( !brush->empty(), "brush left with no faces after split" );
				}
				else
					// the plane does not intersect this brush and the brush is in front of the plane
					if ( !m_split && split.counts[ePlaneFront] != 0 ) {
						m_gj = true;
						Path_deleteTop( path );
					}
			}
		}
	}
};

void CSG_WrapMerge( const ClipperPoints& clipperPoints );

void Scene_BrushSplitByPlane( scene::Graph& graph, const ClipperPoints& points, bool flip, bool caulk, bool split ){
	const char* shader = caulk? GetCaulkShader() : TextureBrowser_GetSelectedShader();
	TextureProjection projection;
	TexDef_Construct_Default( projection );
	BrushSplitByPlaneSelected dosplit( points, flip, shader, projection, split );
	if( points._count > 1 && plane3_valid( plane3_for_points( points._points ) ) )
		graph.traverse( dosplit );
	if( !dosplit.m_gj ){
		CSG_WrapMerge( points );
	}
}


class BrushInstanceSetClipPlane : public scene::Graph::Walker
{
	const Plane3 m_plane;
public:
	BrushInstanceSetClipPlane( const ClipperPoints& points, bool flip )
		: m_plane( points._count < 2
		           ? Plane3( 0, 0, 0, 0 )
		           : flip
		             ? plane3_for_points( points[0], points[2], points[1] )
		             : plane3_for_points( points[0], points[1], points[2] ) ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		BrushInstance* brush = Instance_getBrush( instance );
		if ( brush != 0
		     && path.top().get().visible()
		     && brush->isSelected() ) {
			BrushInstance& brushInstance = *brush;
			brushInstance.setClipPlane( m_plane );
		}
		return true;
	}
};

void Scene_BrushSetClipPlane( scene::Graph& graph, const ClipperPoints& points, bool flip ){
	graph.traverse( BrushInstanceSetClipPlane( points, flip ) );
}

/*
   =============
   CSG_Merge
   =============
 */
bool Brush_merge( Brush& brush, const brush_vector_t& in, bool onlyshape ){
	// gather potential outer faces

	{
		typedef std::vector<const Face*> Faces;
		Faces faces;
		for ( brush_vector_t::const_iterator i( in.begin() ); i != in.end(); ++i )
		{
			( *i )->evaluateBRep();
			for ( Brush::const_iterator j( ( *i )->begin() ); j != ( *i )->end(); ++j )
			{
				if ( !( *j )->contributes() ) {
					continue;
				}

				const Face& face1 = *( *j );

				bool skip = false;
				// test faces of all input brushes
				//!\todo SPEEDUP: Flag already-skip faces and only test brushes from i+1 upwards.
				for ( brush_vector_t::const_iterator k( in.begin() ); !skip && k != in.end(); ++k )
				{
					if ( k != i ) { // don't test a brush against itself
						for ( Brush::const_iterator l( ( *k )->begin() ); !skip && l != ( *k )->end(); ++l )
						{
							const Face& face2 = *( *l );

							// face opposes another face
							if ( plane3_opposing( face1.plane3(), face2.plane3() ) ) {
								// skip opposing planes
								skip  = true;
								break;
							}
						}
					}
				}

				// check faces already stored
				for ( Faces::const_iterator m = faces.begin(); !skip && m != faces.end(); ++m )
				{
					const Face& face2 = *( *m );

					// face equals another face
					if ( plane3_equal( face1.plane3(), face2.plane3() ) ) {
						//if the texture/shader references should be the same but are not
						if ( !onlyshape && !shader_equal( face1.getShader().getShader(), face2.getShader().getShader() ) ) {
							return false;
						}
						// skip duplicate planes
						skip = true;
						break;
					}

					// face1 plane intersects face2 winding or vice versa
					if ( Winding_PlanesConcave( face1.getWinding(), face2.getWinding(), face1.plane3(), face2.plane3() ) ) {
						// result would not be convex
						return false;
					}
				}

				if ( !skip ) {
					faces.push_back( &face1 );
				}
			}
		}
		for ( Faces::const_iterator i = faces.begin(); i != faces.end(); ++i )
		{
			if ( !brush.addFace( *( *i ) ) ) {
				// result would have too many sides
				return false;
			}
		}
	}

	brush.removeEmptyFaces();

	return true;
}

scene::Path ultimate_group_path(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		scene::Path path = GlobalSelectionSystem().ultimateSelected().path();
		scene::Node& node = path.top();
		if( Node_isPrimitive( node ) ){
			path.pop();
			return path;
		}
		if ( Node_isEntity( node ) && node_is_group( node ) ){
			return path;
		}
	}
	scene::Path path;
	path.push( makeReference( GlobalSceneGraph().root() ) );
	path.push( makeReference( Map_FindOrInsertWorldspawn( g_map ) ) );
	return path;
}

void CSG_Merge(){
	brush_vector_t selected_brushes;

	// remove selected
	GlobalSceneGraph().traverse( BrushGatherSelected( selected_brushes ) );

	if ( selected_brushes.empty() ) {
		globalWarningStream() << "CSG Merge: No brushes selected.\n";
		return;
	}

	if ( selected_brushes.size() < 2 ) {
		globalWarningStream() << "CSG Merge: At least two brushes have to be selected.\n";
		return;
	}

	globalOutputStream() << "CSG Merge: Merging " << selected_brushes.size() << " brushes.\n";

	UndoableCommand undo( "brushMerge" );

	NodeSmartReference node( ( new BrushNode() )->node() );
	Brush* brush = Node_getBrush( node );
	// if the new brush would not be convex
	if ( !Brush_merge( *brush, selected_brushes, true ) ) {
		globalWarningStream() << "CSG Merge: Failed - result would not be convex.\n";
	}
	else
	{
		ASSERT_MESSAGE( !brush->empty(), "brush left with no faces after merge" );

		scene::Path path = ultimate_group_path();

		// free the original brushes
		GlobalSceneGraph().traverse( BrushDeleteSelected( path.top().get_pointer() ) );

		Node_getTraversable( path.top() )->insert( node );
		path.push( makeReference( node.get() ) );

		selectPath( path, true );

		globalOutputStream() << "CSG Merge: Succeeded.\n";
	}
}



class MergeVertices
{
	typedef std::vector<DoubleVector3> Vertices;
	Vertices m_vertices;
public:
	typedef Vertices::const_iterator const_iterator;
	void insert( const DoubleVector3& vertex ){
		if( !contains( vertex ) )
			m_vertices.push_back( vertex );
	}
	bool contains( const DoubleVector3& vertex ) const {
		return std::any_of( begin(), end(), [&vertex]( const DoubleVector3& v ){ return Edge_isDegenerate( vertex, v ); } );
	}
	const_iterator begin() const {
		return m_vertices.begin();
	}
	const_iterator end() const {
		return m_vertices.end();
	}
	std::size_t size() const {
		return m_vertices.size();
	}
	const DoubleVector3& operator[]( std::size_t i ) const {
		return m_vertices[i];
	}
	brushsplit_t classify_plane( const Plane3& plane ) const {
		brushsplit_t split;
		for( const DoubleVector3& vertex : m_vertices ){
			WindingVertex_ClassifyPlane( vertex, plane, split );
			if( ( split.counts[ePlaneFront] != 0 ) && ( split.counts[ePlaneBack] != 0 ) ) // optimized to break, if plane is inside
				break;
		}
		return split;
	}
};

class Scene_gatherSelectedComponents : public scene::Graph::Walker
{
	MergeVertices& m_mergeVertices;
	const Vector3Callback m_callback;
public:
	Scene_gatherSelectedComponents( MergeVertices& mergeVertices )
		: m_mergeVertices( mergeVertices ), m_callback( [this]( const DoubleVector3& value ){ m_mergeVertices.insert( value ); } ){
	}
	bool pre( const scene::Path& path, scene::Instance& instance ) const {
		if ( path.top().get().visible() ) {
			ComponentEditable* componentEditable = Instance_getComponentEditable( instance );
			if ( componentEditable ) {
				componentEditable->gatherSelectedComponents( m_callback );
			}
			return true;
		}
		return false;
	}
};

struct MergePlane
{
	const Plane3 m_plane;
	const Face *const m_face;
	const DoubleVector3 m_verts[3];
	MergePlane( const Plane3& plane, const Face* face ) : m_plane( plane ), m_face( face ){
	}
	MergePlane( const Plane3& plane, const DoubleVector3 verts[3] ) : m_plane( plane ), m_face( 0 ), m_verts{ verts[0], verts[1], verts[2] } {
	}
};

class MergePlanes
{
	typedef std::vector<MergePlane> Planes;
	Planes m_planes;
public:
	typedef Planes::const_iterator const_iterator;
	void insert( const MergePlane& plane ){
		if( std::none_of( begin(), end(), [&plane]( const MergePlane& pla ){ return plane3_equal( plane.m_plane, pla.m_plane ); } ) )
			m_planes.push_back( plane );
	}
	const_iterator begin() const {
		return m_planes.begin();
	}
	const_iterator end() const {
		return m_planes.end();
	}
	std::size_t size() const {
		return m_planes.size();
	}
};

#include "quickhull/QuickHull.hpp"
void CSG_build_hull( const MergeVertices& mergeVertices, MergePlanes& mergePlanes ){
#if 0
	if( mergeVertices.size() < 130 ){ // use reliable bruteforce path, when possible
		/* bruteforce new planes */
		for( MergeVertices::const_iterator i = mergeVertices.begin() + 0; i != mergeVertices.end() - 2; ++i )
			for( MergeVertices::const_iterator j = i + 1; j != mergeVertices.end() - 1; ++j )
				for( MergeVertices::const_iterator k = j + 1; k != mergeVertices.end() - 0; ++k ){
					const Plane3 plane = plane3_for_points( *i, *j, *k );
					if( plane3_valid( plane ) ){
						const brushsplit_t split = mergeVertices.classify_plane( plane );
						if( ( split.counts[ePlaneFront] == 0 ) != ( split.counts[ePlaneBack] == 0 ) ){
							if( split.counts[ePlaneFront] != 0 )
								mergePlanes.insert( MergePlane( plane3_flipped( plane ), *i, *k, *j ) );
							else
								mergePlanes.insert( MergePlane( plane, *i, *j, *k ) );
						}
					}
				}
	}
	else
#endif
	{
		quickhull::QuickHull<double> quickhull;
		std::vector<quickhull::Vector3<double>> pointCloud;
		pointCloud.reserve( mergeVertices.size() );
		for( const DoubleVector3& v : mergeVertices ){
			pointCloud.push_back( quickhull::Vector3<double>( v.x(), v.y(), v.z() ) );
		}
		auto hull = quickhull.getConvexHull( pointCloud, true, true );
		const auto& indexBuffer = hull.getIndexBuffer();
		const size_t triangleCount = indexBuffer.size() / 3;
		for( size_t i = 0; i < triangleCount; ++i ) {
			DoubleVector3 points[3];
			for( size_t j = 0; j < 3; ++j ){
				points[j] = mergeVertices[indexBuffer[i * 3 + j]];
			}
			const Plane3 plane = plane3_for_points( points );
			if( plane3_valid( plane ) ){
				mergePlanes.insert( MergePlane( plane, points ) );
			}
		}
	}
}

void CSG_WrapMerge( const ClipperPoints& clipperPoints ){
	if ( !GlobalSelectionSystem().countSelected() && !GlobalSelectionSystem().countSelectedComponents() ) {
		globalWarningStream() << "CSG Wrap Merge: No brushes or components selected.\n";
		return;
	}

	const bool primit = ( GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive );
	brush_vector_t selected_brushes;
	if( primit )
		GlobalSceneGraph().traverse( BrushGatherSelected( selected_brushes ) );

	MergeVertices mergeVertices;
	/* gather unique vertices */
	for ( const Brush *brush : selected_brushes )
		for ( const FaceSmartPointer& face : *brush )
			if ( face->contributes() )
				for ( const WindingVertex& v : face->getWinding() )
					mergeVertices.insert( v.vertex );

	GlobalSceneGraph().traverse( Scene_gatherSelectedComponents( mergeVertices ) );

	for( std::size_t i = 0; i < clipperPoints._count; ++i )
		mergeVertices.insert( clipperPoints[i] );

//globalOutputStream() << mergeVertices.size() << " mergeVertices.size()\n";
	if( mergeVertices.size() < 4 ){
		globalWarningStream() << "CSG Wrap Merge: Too few vertices: " << mergeVertices.size() << ".\n";
		return;
	}

	MergePlanes mergePlanes;
	/* gather unique && worthy planes */
	for ( const Brush *brush : selected_brushes )
		for ( const FaceSmartPointer& face : *brush )
			if ( face->contributes() ){
				const brushsplit_t split = mergeVertices.classify_plane( face->getPlane().plane3() );
				if( ( split.counts[ePlaneFront] == 0 ) != ( split.counts[ePlaneBack] == 0 ) )
					mergePlanes.insert( MergePlane( face->getPlane().plane3(), face.get() ) );
			}

	CSG_build_hull( mergeVertices, mergePlanes );

//globalOutputStream() << mergePlanes.size() << " mergePlanes.size()\n";
	if( mergePlanes.size() < 4 ){
		globalWarningStream() << "CSG Wrap Merge: Too few planes: " << mergePlanes.size() << ".\n";
		return;
	}

	NodeSmartReference node( ( new BrushNode() )->node() );
	Brush* brush = GlobalSelectionSystem().countSelected() > 0? Node_getBrush( GlobalSelectionSystem().ultimateSelected().path().top() ) : 0;
	const bool oldbrush = brush && primit;
	if( oldbrush )
		brush->clear();
	else{
		brush = Node_getBrush( node );
	}

	{
		const char* shader = TextureBrowser_GetSelectedShader();
		TextureProjection projection;
		TexDef_Construct_Default( projection );
		for( const MergePlane& p : mergePlanes ){
			if( p.m_face )
				brush->addFace( *( p.m_face ) );
			else
				brush->addPlane( p.m_verts[0], p.m_verts[1], p.m_verts[2], shader, projection );
//			globalOutputStream() << p.m_plane.normal() << ' ' << p.m_plane.dist() << " p.m_plane\n";
		}
		brush->removeEmptyFaces();
	}

	// if the new brush would not be convex
	if ( !brush->hasContributingFaces() ) {
		globalWarningStream() << "CSG Wrap Merge: Failed - result would not be convex.\n";
	}
	else
	{
		if( oldbrush ){
			GlobalSceneGraph().traverse( BrushDeleteSelected( GlobalSelectionSystem().ultimateSelected().path().top().get_pointer() ) );
		}
		else{
			scene::Path path = ultimate_group_path();

			// free the original brushes
			if( primit )
				GlobalSceneGraph().traverse( BrushDeleteSelected( path.top().get_pointer() ) );

			Node_getTraversable( path.top() )->insert( node );
			path.push( makeReference( node.get() ) );

			selectPath( path, true );
		}
	}
}

void CSG_WrapMerge(){
	UndoableCommand undo( "brushWrapMerge" );
	CSG_WrapMerge( Clipper_getPlanePoints() );
}


void CSG_Intersect(){
	brush_vector_t selected_brushes;
	scene::Node *ultimate_brush_node;

	Scene_forEachVisibleSelectedBrush( [&selected_brushes, &ultimate_brush_node]( BrushInstance& brush ){
		selected_brushes.push_back( &brush.getBrush() );
		ultimate_brush_node = brush.path().top().get_pointer();
	} );

	if ( selected_brushes.empty() ) {
		globalWarningStream() << "CSG Intersect: No brushes selected.\n";
		return;
	}

	if ( selected_brushes.size() < 2 ) {
		globalWarningStream() << "CSG Intersect: At least two brushes have to be selected.\n";
		return;
	}

	UndoableCommand undo( "brushIntersect" );

	Brush* ultimate_brush = selected_brushes.back();
	selected_brushes.pop_back();

	for ( const Brush *brush : selected_brushes )
		for ( const FaceSmartPointer& face : *brush )
			if ( face->contributes() ){
				const brushsplit_t split = Brush_classifyPlane( *ultimate_brush, face->getPlane().plane3() );
				if( split.counts[ePlaneFront] != 0 ){
					ultimate_brush->addFace( *face.get() );
					ultimate_brush->removeEmptyFaces();
				}
			}

	if ( !ultimate_brush->hasContributingFaces() ) {
		globalWarningStream() << "CSG Intesect: Failed - result would not be convex.\n";
		GlobalSceneGraph().traverse( BrushDeleteSelected() );
	}
	else
	{
		GlobalSceneGraph().traverse( BrushDeleteSelected( ultimate_brush_node ) );
	}
}


#if 0
class find_instance_to_DeleteComponents : public SelectionSystem::Visitor
{
public:
	mutable scene::Instance* m_instance;
	find_instance_to_DeleteComponents()
		: m_instance( 0 ) {
	}
	void visit( scene::Instance& instance ) const {
		if( Instance_getBrush( instance ) ){
			if( Instance_isSelectedComponents( instance ) )
				m_instance = &instance;
		}
	}
};
/* deletes one brush components */
void CSG_DeleteComponents(){
	find_instance_to_DeleteComponents find_instance;
	GlobalSelectionSystem().foreachSelected( find_instance );
	scene::Instance* instance = find_instance.m_instance;
	if( instance ){
		MergeVertices deleteVertices;
		{
			Scene_gatherSelectedComponents get_deleteVertices( deleteVertices );
			get_deleteVertices.pre( instance->path(), *instance );
		}
		Brush* brush = Node_getBrush( instance->path().top() );
		/* gather vertices to keep */
		MergeVertices mergeVertices;
		for ( Brush::const_iterator f = brush->begin(); f != brush->end(); ++f )
			if ( ( *f )->contributes() ){
				const Winding& winding = ( *f )->getWinding();
				for ( std::size_t w = 0; w != winding.numpoints; ++w )
					if( !deleteVertices.contains( winding[w].vertex ) )
						mergeVertices.insert( winding[w].vertex );
			}
		if( mergeVertices.size() < 4 ){
			globalWarningStream() << "CSG_DeleteComponents: Too few vertices left: " << mergeVertices.size() << ".\n";
			return;
		}
		/* gather original planes */
		MergePlanes mergePlanes;
		for ( Brush::const_iterator f = brush->begin(); f != brush->end(); ++f ){
			const Face& face = *( *f );
			if ( face.contributes() ){
				mergePlanes.insert( MergePlane( face.getPlane().plane3(), &face ) );
			}
		}

		CSG_build_hull( mergeVertices, mergePlanes );

		if( mergePlanes.size() < 4 ){
			globalWarningStream() << "CSG_DeleteComponents: Too few planes left: " << mergePlanes.size() << ".\n";
			return;
		}

		{
			ComponentSelectionTestable* componentSelectionTestable = Instance_getComponentSelectionTestable( *instance );
			componentSelectionTestable->setSelectedComponents( false, SelectionSystem::eVertex );
			componentSelectionTestable->setSelectedComponents( false, SelectionSystem::eEdge );
			componentSelectionTestable->setSelectedComponents( false, SelectionSystem::eFace );

			const char* shader = TextureBrowser_GetSelectedShader();
			TextureProjection projection;
			TexDef_Construct_Default( projection );
			for( MergePlanes::const_iterator i = mergePlanes.begin(); i != mergePlanes.end(); ++i ){
				if( !i->m_face )
					brush->addPlane( i->m_v1, i->m_v2, i->m_v3, shader, projection );
			}
			brush->removeEmptyFaces();
		}

		// if the new brush would not be convex
		if ( !brush->hasContributingFaces() ) {
			globalWarningStream() << "CSG_DeleteComponents: Failed - result would not be convex.\n";
		}
	}
}
#else

void CSG_DeleteComponents(){
	Scene_forEachSelectedBrush( []( BrushInstance& brush ){ brush.remove_vertices(); } );
}
#endif






/*
   =============
   CSG_Tool
   =============
 */
#include "mainframe.h"
#include "gtkutil/dialog.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/image.h"
#include "gtkutil/spinbox.h"
#include "gtkutil/guisettings.h"
#include "xywindow.h"
#include "camwindow.h"

#include <QWidget>
#include <QToolButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGridLayout>
#include <QFrame>

struct CSGToolDialog
{
	QDoubleSpinBox* spin;
	QWidget *window{};
	QRadioButton *radFaces, *radPlusFaces, *radProj, *radCam;
	QToolButton *caulk, *removeInner;
};

CSGToolDialog g_csgtool_dialog;

class BrushFaceOffset {
	HollowSettings& m_settings;
public:
	BrushFaceOffset( HollowSettings& settings )
		: m_settings( settings ) {
	}
	void operator()( BrushInstance& brush ) const {
		if( brush.isSelected() || brush.isSelectedComponents() ){
			m_settings.excludeFaces( brush );
			Brush_forEachFace( brush, FaceOffset( m_settings ) );
		}
	}
};

/*
   =============
   CSG_MakeRoom
   =============
 */
void CSG_MakeRoom(){
	HollowSettings settings;
	settings.m_hollowType = ePull;
	settings.m_offset = GetGridSize();
	settings.m_excludeByAxis = false;
	settings.m_excludeSelectedFaces = true;
	settings.m_caulk = true;
	settings.m_removeInner = true;

	UndoableCommand undo( "makeRoom" );
	GlobalSceneGraph().traverse( BrushHollowSelectedWalker( settings ) );
	GlobalSceneGraph().traverse( BrushDeleteSelected() );
	SceneChangeNotify();
}

HollowSettings CSGdlg_getSettings( const CSGToolDialog& dialog ){
	HollowSettings settings;

	settings.m_offset = dialog.spin->value();
	if( dialog.radProj->isChecked() ){
		settings.m_excludeByAxis = true;
		settings.m_exclusionAxis = g_vector3_axes[GlobalXYWnd_getCurrentViewType()];
	}
	else if( dialog.radCam->isChecked() ){
		settings.m_excludeByAxis = true;
		settings.m_exclusionAxis = Camera_getViewVector( *g_pParentWnd->GetCamWnd() );
	}
	else{ // either + or - faces
		settings.m_excludeByAxis = false;
		settings.m_excludeSelectedFaces = dialog.radFaces->isChecked();
	}
	settings.m_caulk = dialog.caulk->isChecked();
	settings.m_removeInner = dialog.removeInner->isChecked();

	return settings;
}

void CSG_Hollow( EHollowType type, const char* undoString, const CSGToolDialog& dialog ){
	HollowSettings settings = CSGdlg_getSettings( dialog );
	settings.m_hollowType = type;
	UndoableCommand undo( undoString );
	GlobalSceneGraph().traverse( BrushHollowSelectedWalker( settings ) );
	if( settings.m_removeInner ){
		GlobalSceneGraph().traverse( BrushDeleteSelected() );
	}
	SceneChangeNotify();
}

//=================DLG

static void CSGdlg_BrushShrink(){
	HollowSettings settings = CSGdlg_getSettings( g_csgtool_dialog );
	settings.m_offset *= -1;
	UndoableCommand undo( "Shrink brush" );
//	Scene_forEachSelectedBrush( BrushFaceOffset( settings ) );
	Scene_forEachVisibleBrush( GlobalSceneGraph(), BrushFaceOffset( settings ) );
	SceneChangeNotify();
}

static void CSGdlg_BrushExpand(){
	HollowSettings settings = CSGdlg_getSettings( g_csgtool_dialog );
	UndoableCommand undo( "Expand brush" );
//	Scene_forEachSelectedBrush( BrushFaceOffset( settings ) );
	Scene_forEachVisibleBrush( GlobalSceneGraph(), BrushFaceOffset( settings ) );
	SceneChangeNotify();
}


class CSG_SpinBoxLabel : public SpinBoxLabel<QDoubleSpinBox>
{
	using SpinBoxLabel<QDoubleSpinBox>::SpinBoxLabel;
protected:
	void mouseReleaseEvent( QMouseEvent* event ) override {
		if( !m_dragOccured ){
			m_spin->setValue( GetGridSize() );
			m_spin->setSingleStep( GetGridSize() );
		}
		SpinBoxLabel::mouseReleaseEvent( event );
    }
};


void CSG_Tool(){
	if ( g_csgtool_dialog.window == nullptr ) {
		g_csgtool_dialog.window = new QWidget( MainFrame_getWindow(), Qt::Dialog | Qt::WindowCloseButtonHint );
		g_csgtool_dialog.window->setWindowTitle( "CSG Tool" );
		g_guiSettings.addWindow( g_csgtool_dialog.window, "CSGTool/geometry" );

		{
			auto grid = new QGridLayout( g_csgtool_dialog.window ); // 3 x 8
			grid->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
			{
				auto spin = g_csgtool_dialog.spin = new DoubleSpinBox( 0, 9999, 16, 3, 16 );
				spin->setToolTip( "Thickness" );
				grid->addWidget( spin, 0, 1 );
			}
			{
				auto label = new CSG_SpinBoxLabel( "Grid->", g_csgtool_dialog.spin );
				grid->addWidget( label, 0, 0 );
			}
			{
				//radio button group for choosing the exclude axis
				auto radFaces = g_csgtool_dialog.radFaces = new QRadioButton( "-faces" );
				radFaces->setToolTip( "Exclude selected faces" );
				grid->addWidget( radFaces, 0, 2 );

				auto radPlusFaces = g_csgtool_dialog.radPlusFaces = new QRadioButton( "+faces" );
				radPlusFaces->setToolTip( "Only process selected faces" );
				grid->addWidget( radPlusFaces, 0, 3 );

				auto radProj = g_csgtool_dialog.radProj = new QRadioButton( "-proj" );
				radProj->setToolTip( "Exclude faces, most orthogonal to active projection" );
				grid->addWidget( radProj, 0, 4 );

				auto radCam = g_csgtool_dialog.radCam = new QRadioButton( "-cam" );
				radCam->setToolTip( "Exclude faces, most orthogonal to camera view" );
				grid->addWidget( radCam, 0, 5 );

				radFaces->setChecked( true );
			}
			{
				auto button = g_csgtool_dialog.caulk = new QToolButton;
				auto pix = new_local_image( "f-caulk.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Caulk some faces" );
				button->setCheckable( true );
				button->setChecked( true );
				grid->addWidget( button, 0, 6 );
			}
			{
				auto button = g_csgtool_dialog.removeInner = new QToolButton;
				auto pix = new_local_image( "csgtool_removeinner.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Remove inner brush" );
				button->setCheckable( true );
				button->setChecked( true );
				grid->addWidget( button, 0, 7 );
			}
			{
				auto line = new QFrame;
				line->setFrameShape( QFrame::Shape::HLine );
				line->setFrameShadow( QFrame::Shadow::Raised );
				grid->addWidget( line, 1, 0, 1, 8 );
			}
			{
				auto button = new QToolButton;
				auto pix = new_local_image( "csgtool_shrink.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Shrink brush" );
				grid->addWidget( button, 2, 0 );
				QObject::connect( button, &QAbstractButton::clicked, CSGdlg_BrushShrink );
			}
			{
				auto button = new QToolButton;
				auto pix = new_local_image( "csgtool_expand.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Expand brush" );
				grid->addWidget( button, 2, 1 );
				QObject::connect( button, &QAbstractButton::clicked, CSGdlg_BrushExpand );
			}
			{
				auto button = new QToolButton;
				auto pix = new_local_image( "csgtool_diagonal.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Hollow::diagonal joints" );
				grid->addWidget( button, 2, 3 );
				QObject::connect( button, &QAbstractButton::clicked, [](){ CSG_Hollow( eDiag, "brushHollow::Diag", g_csgtool_dialog ); } );
			}
			{
				auto button = new QToolButton;
				auto pix = new_local_image( "csgtool_wrap.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Hollow::wrap" );
				grid->addWidget( button, 2, 4 );
				QObject::connect( button, &QAbstractButton::clicked, [](){ CSG_Hollow( eWrap, "brushHollow::Wrap", g_csgtool_dialog ); } );
			}
			{
				auto button = new QToolButton;
				auto pix = new_local_image( "csgtool_extrude.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Hollow::extrude faces" );
				grid->addWidget( button, 2, 5 );
				QObject::connect( button, &QAbstractButton::clicked, [](){ CSG_Hollow( eExtrude, "brushHollow::Extrude", g_csgtool_dialog ); } );
			}
			{
				auto button = new QToolButton;
				auto pix = new_local_image( "csgtool_pull.png" );
				button->setIcon( pix );
				button->setIconSize( pix.size() + QSize( 8, 8 ) );
				button->setToolTip( "Hollow::pull faces" );
				grid->addWidget( button, 2, 6 );
				QObject::connect( button, &QAbstractButton::clicked, [](){ CSG_Hollow( ePull, "brushHollow::Pull", g_csgtool_dialog ); } );
			}
		}
	}

	g_csgtool_dialog.window->show();
}

#include "commands.h"

void CSG_registerCommands(){
	GlobalCommands_insert( "CSGSubtract", makeCallbackF( CSG_Subtract ), QKeySequence( "Shift+U" ) );
	GlobalCommands_insert( "CSGMerge", makeCallbackF( CSG_Merge ) );
	GlobalCommands_insert( "CSGWrapMerge", FreeCaller<void(), CSG_WrapMerge>(), QKeySequence( "Ctrl+U" ) );
	GlobalCommands_insert( "CSGIntersect", makeCallbackF( CSG_Intersect ), QKeySequence( "Ctrl+Shift+U" ) );
	GlobalCommands_insert( "CSGroom", makeCallbackF( CSG_MakeRoom ) );
	GlobalCommands_insert( "CSGTool", makeCallbackF( CSG_Tool ) );
}
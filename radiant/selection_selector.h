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

#pragma once

#include "selection_.h"

typedef std::multimap<SelectionIntersection, Selectable*> SelectableSortedSet;

class SelectionPool : public Selector
{
	SelectableSortedSet m_pool;
	SelectionIntersection m_intersection;
	Selectable* m_selectable;

public:
	void pushSelectable( Selectable& selectable ) override {
		m_intersection = SelectionIntersection();
		m_selectable = &selectable;
	}
	void popSelectable() override {
		addSelectable( m_intersection, m_selectable );
		m_intersection = SelectionIntersection();
	}
	void addIntersection( const SelectionIntersection& intersection ) override {
		assign_if_closer( m_intersection, intersection );
	}
	void addSelectable( const SelectionIntersection& intersection, Selectable* selectable ){
		if ( intersection.valid() ) {
			m_pool.insert( SelectableSortedSet::value_type( intersection, selectable ) );
		}
	}

	typedef SelectableSortedSet::iterator iterator;

	iterator begin(){
		return m_pool.begin();
	}
	iterator end(){
		return m_pool.end();
	}

	bool failed(){
		return m_pool.empty();
	}
};


class ManipulatorSelectionChangeable
{
	const Selectable* m_selectable_prev_ptr = nullptr;
public:
	void selectionChange( const Selectable *se ){
		if( m_selectable_prev_ptr != se ){
			m_selectable_prev_ptr = se;
			SceneChangeNotify();
		}
	}
	void selectionChange( SelectionPool& selector ){
		Selectable *se = nullptr;
		if ( !selector.failed() ) {
			se = selector.begin()->second;
			se->setSelected( true );
		}
		selectionChange( se );
	}
};


class BooleanSelector : public Selector
{
	SelectionIntersection m_bestIntersection;
	Selectable* m_selectable;
public:
	BooleanSelector() : m_bestIntersection( SelectionIntersection() ){
	}

	void pushSelectable( Selectable& selectable ) override {
		m_selectable = &selectable;
	}
	void popSelectable() override {
	}
	void addIntersection( const SelectionIntersection& intersection ) override {
		if ( m_selectable->isSelected() ) {
			assign_if_closer( m_bestIntersection, intersection );
		}
	}

	bool isSelected(){
		return m_bestIntersection.valid();
	}
	const SelectionIntersection& bestIntersection() const {
		return m_bestIntersection;
	}
};


template<float DEPTH_EPSILON>
class BestSelector___ : public Selector
{
protected:
	SelectionIntersection m_intersection;
	Selectable* m_selectable;
	SelectionIntersection m_bestIntersection;
	std::list<Selectable*> m_bestSelectable;
public:
	BestSelector___() : m_bestIntersection( SelectionIntersection() ), m_bestSelectable( 0 ){
	}

	void pushSelectable( Selectable& selectable ) override {
		m_intersection = SelectionIntersection();
		m_selectable = &selectable;
	}
	void popSelectable() override {
		if ( m_intersection.equalEpsilon( m_bestIntersection, 0.25f, DEPTH_EPSILON ) ) {
			m_bestSelectable.push_back( m_selectable );
			m_bestIntersection = m_intersection;
		}
		else if ( m_intersection < m_bestIntersection ) {
			m_bestSelectable.clear();
			m_bestSelectable.push_back( m_selectable );
			m_bestIntersection = m_intersection;
		}
		m_intersection = SelectionIntersection();
	}
	void addIntersection( const SelectionIntersection& intersection ) override {
		assign_if_closer( m_intersection, intersection );
	}

	std::list<Selectable*>& best(){
		return m_bestSelectable;
	}
	const SelectionIntersection& bestIntersection() const {
		return m_bestIntersection;
	}
};
using BestSelector = BestSelector___<2e-6f>;
using DeepBestSelector = BestSelector___<2.f>;


class BestPointSelector : public Selector
{
	SelectionIntersection m_bestIntersection;
public:
	BestPointSelector() : m_bestIntersection( SelectionIntersection() ){
	}

	void pushSelectable( Selectable& selectable ) override {
	}
	void popSelectable() override {
	}
	void addIntersection( const SelectionIntersection& intersection ) override {
		assign_if_closer( m_bestIntersection, intersection );
	}

	bool isSelected(){
		return m_bestIntersection.valid();
	}
	const SelectionIntersection& best() const {
		return m_bestIntersection;
	}
};


class ScenePointSelector : public Selector
{
	SelectionIntersection m_bestIntersection;
	class Face* m_face;
public:
	ScenePointSelector() : m_bestIntersection( SelectionIntersection() ), m_face( 0 ) {
	}

	void pushSelectable( Selectable& selectable ) override {
	}
	void popSelectable() override {
	}
	void addIntersection( const SelectionIntersection& intersection ) override {
		if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
			m_bestIntersection = intersection;
			m_face = 0;
		}
	}

	void addIntersection( const SelectionIntersection& intersection, Face* face ) {
		if( SelectionIntersection_closer( intersection, m_bestIntersection ) ) {
			m_bestIntersection = intersection;
			m_face = face;
		}
	}
	bool isSelected() {
		return m_bestIntersection.valid();
	}
	const SelectionIntersection& best() {
		return m_bestIntersection;
	}
	const Face* face() {
		return m_face;
	}
};

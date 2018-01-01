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

#include "iselection.h"
#include "generic/callback.h"
#include "scenelib.h"
#include <cstdlib>

class SelectableBool : public Selectable
{
	bool m_selected;
public:
	SelectableBool()
		: m_selected( false )
	{}

	void setSelected( bool select = true ) override {
		m_selected = select;
	}
	bool isSelected() const override {
		return m_selected;
	}
};

class ObservedSelectable final : public Selectable
{
	SelectionChangeCallback m_onchanged;
	bool m_selected;
public:
	ObservedSelectable( const SelectionChangeCallback& onchanged ) : m_onchanged( onchanged ), m_selected( false ){
	}
	ObservedSelectable( const ObservedSelectable& other ) : Selectable( other ), m_onchanged( other.m_onchanged ), m_selected( false ){
		setSelected( other.isSelected() );
	}
	ObservedSelectable& operator=( const ObservedSelectable& other ){
		setSelected( other.isSelected() );
		return *this;
	}
	~ObservedSelectable(){
		setSelected( false );
	}

	void setSelected( bool select ) override {
		if ( select ^ m_selected ) {
			m_selected = select;

			m_onchanged( *this );
		}
	}
	bool isSelected() const override {
		return m_selected;
	}
};

class SelectableInstance : public scene::Instance
{
	class TypeCasts
	{
		InstanceTypeCastTable m_casts;
	public:
		TypeCasts(){
			InstanceContainedCast<SelectableInstance, Selectable>::install( m_casts );
		}
		InstanceTypeCastTable& get(){
			return m_casts;
		}
	};

	ObservedSelectable m_selectable;
public:

	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	SelectableInstance( const scene::Path& path, scene::Instance* parent, void* instance = 0, InstanceTypeCastTable& casts = StaticTypeCasts::instance().get() ) :
		Instance( path, parent, instance != 0 ? instance : this, casts ),
		m_selectable( SelectedChangedCaller( *this ) ){
	}

	Selectable& get( NullType<Selectable>){
		return m_selectable;
	}

	Selectable& getSelectable(){
		return m_selectable;
	}
	const Selectable& getSelectable() const {
		return m_selectable;
	}

	void selectedChanged( const Selectable& selectable ){
		GlobalSelectionSystem().getObserver ( SelectionSystem::ePrimitive )( selectable );
		GlobalSelectionSystem().onSelectedChanged( *this, selectable );

		Instance::selectedChanged();
	}
	typedef MemberCaller<SelectableInstance, void(const Selectable&), &SelectableInstance::selectedChanged> SelectedChangedCaller;
};


#include <list>
#include <set>

/// It's illegal to modify inserted values directly!
template<typename Selected>
class SelectionList
{
	typedef std::list<Selected*> List;
	List m_selection;
public:
	typedef typename List::iterator iterator;
	typedef typename List::const_iterator const_iterator;
private:
	struct Compare{
		using is_transparent = void;

		bool operator()( const const_iterator& one, const const_iterator& other ) const {
			return *one < *other;
		}
		bool operator()( const Selected* va, const const_iterator& it ) const {
			return va < *it;
		}
		bool operator()( const const_iterator& it, const Selected* va ) const {
			return *it < va;
		}
	};
	std::multiset<const_iterator, Compare> m_set;
public:

	SelectionList() = default;
	SelectionList( SelectionList&& ) noexcept = delete;

	iterator begin(){
		return m_selection.begin();
	}
	const_iterator begin() const {
		return m_selection.begin();
	}
	iterator end(){
		return m_selection.end();
	}
	const_iterator end() const {
		return m_selection.end();
	}
	bool empty() const {
		return m_selection.empty();
	}
	std::size_t size() const {
		return m_selection.size();
	}
	Selected& back(){
		return *m_selection.back();
	}
	Selected& back() const {
		return *m_selection.back();
	}
	void append( Selected& selected ){
		m_selection.push_back( &selected );
		m_set.emplace( --end() );
	}
	void erase( Selected& selected ){
		const auto it = m_set.find( &selected );
		ASSERT_MESSAGE( it != m_set.cend(), "selection-tracking error" );
		m_selection.erase( *it );
		m_set.erase( it );
	}
};

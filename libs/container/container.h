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

#include <list>
#include <set>

#include "generic/static.h"
#include "debugging/debugging.h"

/// \brief A single-value container, which can either be empty or full.
template<typename Type>
class Single
{
	Type* m_value;
public:
	Single() : m_value( 0 ){
	}
	bool empty(){
		return m_value == 0;
	}
	Type* insert( const Type& other ){
		m_value = new Type( other );
		return m_value;
	}
	void clear(){
		delete m_value;
		m_value = 0;
	}
	Type& get(){
		//ASSERT_MESSAGE( !empty(), "Single: must be initialised before being accessed" );
		return *m_value;
	}
	const Type& get() const {
		//ASSERT_MESSAGE( !empty(), "Single: must be initialised before being accessed" );
		return *m_value;
	}
};


/// \brief An adaptor to make std::set or std::multiset into a SequenceContainer.
/// It's illegal to modify inserted values directly!
/// \param Value Uniquely identifies itself. Must provide a copy-constructor and an equality operator.
template<typename Value, bool UniqueValues>
class UnsortedSet
{
	struct Node
	{
		Node *m_prev;
		Node *m_next;
		Value m_value;
		Node( const Value& value ) : m_value( value ){
		}
		static void link( Node *prev, Node *next ){
			prev->m_next = next;
			next->m_prev = prev;
		}
	};
	/// special thin sentinel node to avoid DefaultConstructible \param Value requirement
	struct SentinelNode
	{
		Node *m_prev;
		Node *m_next;
		SentinelNode(){
			selfLink();
		}
		void selfLink(){
			m_prev = m_next = asNode();
		}
		Node* asNode(){
			return reinterpret_cast<Node*>( this );
		}
		const Node* asNode() const {
			return reinterpret_cast<const Node*>( this );
		}
	};
	static_assert( offsetof( SentinelNode, m_next ) == offsetof( Node, m_next ) &&
	               offsetof( SentinelNode, m_prev ) == offsetof( Node, m_prev ),
	               "Node layouts must be compatible for reinterpret_cast" );
	SentinelNode m_end;

	template<bool IsConst, bool IsReverse>
	class Iterator
	{
	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type        = Value;
		using difference_type   = std::ptrdiff_t;
		using pointer           = std::conditional_t<IsConst, const Value*, Value*>;
		using reference         = std::conditional_t<IsConst, const Value&, Value&>;
		using node_ptr          = std::conditional_t<IsConst, const Node*, Node*>;
	private:
		node_ptr m_node;
	public:
		Iterator( node_ptr node = nullptr ) : m_node( node ){
		}
		reference operator*() const {
			return m_node->m_value;
		}
		pointer operator->() const {
			return &m_node->m_value;
		}
		Iterator& operator++(){
			if constexpr ( IsReverse )
				m_node = m_node->m_prev;
			else
				m_node = m_node->m_next;
			return *this;
		}
		Iterator& operator--(){
			if constexpr ( IsReverse )
				m_node = m_node->m_next;
			else
				m_node = m_node->m_prev;
			return *this;
		}
		Iterator operator++( int ){
			auto ret = *this;
			++( *this );
			return ret;
		}
		Iterator operator--( int ){
			auto ret = *this;
			--( *this );
			return ret;
		}

		friend bool operator==( const Iterator& lhs, const Iterator& rhs ) {
			return lhs.m_node == rhs.m_node;
		}

		// Conversion from non-const to const iterator
		template <bool OtherIsConst, bool OtherIsReverse>
			requires ( OtherIsConst && !IsConst && ( OtherIsReverse == IsReverse ) )
		operator Iterator<OtherIsConst, OtherIsReverse>() const {
			return Iterator<OtherIsConst, OtherIsReverse>( m_node );
		}
	};

public:
	using iterator = Iterator<false, false>;
	using const_iterator = Iterator<true, false>;
	using reverse_iterator = Iterator<false, true>;
	using const_reverse_iterator = Iterator<true, true>;

	iterator               begin()        { return m_end.m_next; }
	const_iterator         begin()  const { return m_end.m_next; }
	iterator               end()          { return m_end.asNode(); }
	const_iterator         end()    const { return m_end.asNode(); }
	reverse_iterator       rbegin()       { return m_end.m_prev; }
	const_reverse_iterator rbegin() const { return m_end.m_prev; }
	reverse_iterator       rend()         { return m_end.asNode(); }
	const_reverse_iterator rend()   const { return m_end.asNode(); }
private:
	struct Compare{
		using is_transparent = void;

		bool operator()( const Node& one, const Node& other ) const {
			return one.m_value < other.m_value;
		}
		bool operator()( const Value& va, const Node& node ) const {
			return va < node.m_value;
		}
		bool operator()( const Node& node, const Value& va ) const {
			return node.m_value < va;
		}
	};
	std::conditional_t<UniqueValues, std::set<Node, Compare>, std::multiset<Node, Compare>> m_set;
public:
	UnsortedSet() = default;
	UnsortedSet( const UnsortedSet& other ) = delete;
	UnsortedSet( UnsortedSet&& ) noexcept = delete;
	UnsortedSet& operator=( const UnsortedSet& other ){
		clear();
		for( const auto& value : other )
			push_back( value );
		return *this;
	};
	UnsortedSet& operator=( UnsortedSet&& ) noexcept = delete;

	bool empty() const {
		return m_set.empty();
	}
	std::size_t size() const {
		return m_set.size();
	}
	void clear(){
		m_end.selfLink();
		m_set.clear();
	}

	void swap( UnsortedSet& other ){
		std::swap( m_set, other.m_set );
		std::swap( m_end.m_next, other.m_end.m_next );
		std::swap( m_end.m_prev, other.m_end.m_prev );
		for( auto *set : { this, &other } ){ // note: would be trivial swap with allocated end node; unused function
			if( set->empty() )
				set->m_end.selfLink();
			else
				set->m_end.m_prev->m_next = set->m_end.m_next->m_prev = set->m_end.asNode();
		}
	}

	iterator push_back( const Value& value ){
		std::tuple tuple = m_set.emplace( value );
		if constexpr ( UniqueValues ){
			ASSERT_MESSAGE( std::get<1>( tuple ), "UnsortedSet::insert: already added" );
		}
		Node *newNode = &const_cast<Node&>( *std::get<0>( tuple ) );
		Node::link( m_end.m_prev, newNode );
		Node::link( newNode, m_end.asNode() );
		return iterator( newNode );
	}
	void erase( const Value& value ){
		const auto it = m_set.find( value ); // note: multiset finds w/e value from equals
		ASSERT_MESSAGE( it != m_set.cend(), "UnsortedSet::erase: not found" );
		Node::link( it->m_prev, it->m_next );
		m_set.erase( it );
	}
	const_iterator find( const Value& value ) const { // note: multiset finds w/e value from equals
		const auto it = m_set.find( value );
		return ( it == m_set.cend() )? end() : const_iterator( &( *it ) );
	}
	Value& back(){
		return m_end.m_prev->m_value;
	}
	const Value& back() const {
		return m_end.m_prev->m_value;
	}
};

namespace std
{
/// \brief Swaps the values of \p self and \p other.
/// Overloads std::swap.
template<typename Value, bool UniqueValues>
inline void swap( UnsortedSet<Value, UniqueValues>& self, UnsortedSet<Value, UniqueValues>& other ){
	self.swap( other );
}
}

/// An adaptor to make std::list into a Unique Associative Sequence - which cannot contain the same value more than once.
/// Key: Uniquely identifies a value. Must provide a copy-constructor and an equality operator.
/// Value: Must provide a copy-constructor.
template<typename Key, typename Value>
class UnsortedMap
{
	typedef typename std::list< std::pair<Key, Value> > Values;
	Values m_values;
public:
	typedef typename Values::value_type value_type;
	typedef typename Values::iterator iterator;
	typedef typename Values::const_iterator const_iterator;

	iterator begin(){
		return m_values.begin();
	}
	const_iterator begin() const {
		return m_values.begin();
	}
	iterator end(){
		return m_values.end();
	}
	const_iterator end() const {
		return m_values.end();
	}

	bool empty() const {
		return m_values.empty();
	}
	std::size_t size() const {
		return m_values.size();
	}
	void clear(){
		m_values.clear();
	}

	iterator insert( const value_type& value ){
		ASSERT_MESSAGE( find( value.first ) == end(), "UnsortedMap::insert: already added" );
		m_values.push_back( value );
		return --m_values.end();
	}
	void erase( const Key& key ){
		iterator i = find( key );
		ASSERT_MESSAGE( i != end(), "UnsortedMap::erase: not found" );
		erase( i );
	}
	void erase( iterator i ){
		m_values.erase( i );
	}
	iterator find( const Key& key ){
		return std::ranges::find( m_values, key, &value_type::first );
	}
	const_iterator find( const Key& key ) const {
		return std::ranges::find( m_values, key, &value_type::first );
	}

	Value& operator[]( const Key& key ){
		iterator i = find( key );
		if ( i != end() ) {
			return ( *i ).second;
		}

		m_values.push_back( Values::value_type( key, Value() ) );
		return m_values.back().second;
	}
};

/// An adaptor to assert when duplicate values are added, or non-existent values removed from a std::set.
template<typename Value>
class UniqueSet
{
	typedef std::set<Value> Values;
	Values m_values;
public:
	typedef typename Values::iterator iterator;
	typedef typename Values::const_iterator const_iterator;
	typedef typename Values::reverse_iterator reverse_iterator;
	typedef typename Values::const_reverse_iterator const_reverse_iterator;


	iterator begin(){
		return m_values.begin();
	}
	const_iterator begin() const {
		return m_values.begin();
	}
	iterator end(){
		return m_values.end();
	}
	const_iterator end() const {
		return m_values.end();
	}
	reverse_iterator rbegin(){
		return m_values.rbegin();
	}
	const_reverse_iterator rbegin() const {
		return m_values.rbegin();
	}
	reverse_iterator rend(){
		return m_values.rend();
	}
	const_reverse_iterator rend() const {
		return m_values.rend();
	}

	bool empty() const {
		return m_values.empty();
	}
	std::size_t size() const {
		return m_values.size();
	}
	void clear(){
		m_values.clear();
	}

	void swap( UniqueSet& other ){
		std::swap( m_values, other.m_values );
	}
	iterator insert( const Value& value ){
		std::pair<iterator, bool> result = m_values.insert( value );
		ASSERT_MESSAGE( result.second, "UniqueSet::insert: already added" );
		return result.first;
	}
	void erase( const Value& value ){
		iterator i = find( value );
		ASSERT_MESSAGE( i != end(), "UniqueSet::erase: not found" );
		m_values.erase( i );
	}
	iterator find( const Value& value ){
		return m_values.find( value );
	}
};

namespace std
{
/// \brief Swaps the values of \p self and \p other.
/// Overloads std::swap.
template<typename Value>
inline void swap( UniqueSet<Value>& self, UniqueSet<Value>& other ){
	self.swap( other );
}
}

template<typename Type>
class ReferencePair
{
	Type* m_first;
	Type* m_second;
public:
	ReferencePair() : m_first( 0 ), m_second( 0 ){
	}
	void attach( Type& t ){
		ASSERT_MESSAGE( m_first == 0 || m_second == 0, "ReferencePair::insert: pointer already exists" );
		if ( m_first == 0 ) {
			m_first = &t;
		}
		else if ( m_second == 0 ) {
			m_second = &t;
		}
	}
	void detach( Type& t ){
		ASSERT_MESSAGE( m_first == &t || m_second == &t, "ReferencePair::erase: pointer not found" );
		if ( m_first == &t ) {
			m_first = 0;
		}
		else if ( m_second == &t ) {
			m_second = 0;
		}
	}
	template<typename Functor>
	void forEach( const Functor& functor ){
		if ( m_second != 0 ) {
			functor( *m_second );
		}
		if ( m_first != 0 ) {
			functor( *m_first );
		}
	}
};

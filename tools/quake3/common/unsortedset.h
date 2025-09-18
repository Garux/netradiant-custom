#pragma once

#include <set>

//variant of container from container/container.h for more heavy objects
//provides CompareValue param, efficient construction and search

/// \brief An adaptor to make std::set or std::multiset into a SequenceContainer.
/// It's illegal to modify inserted values directly!
/// \param Value Uniquely identifies itself. Must provide a copy-constructor and an equality operator.
template<typename Value, bool UniqueValues, typename CompareValue = std::less<>>
class UnsortedSet
{
	struct Node
	{
		Node *m_prev;
		Node *m_next;
		Value m_value;
		Node( const Value& value ) : m_value( value ){
		}
		Node( Value&& value ) : m_value( std::move( value ) ){
		}
		template <class... Args>
		Node( Args&&...args ) : m_value( std::forward<Args>( args )... ){
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
			return CompareValue()( one.m_value, other.m_value );
		}
		template <typename K> // Transparent comparison: Value with key type
		bool operator()( const Node& node, const K& key ) const {
			return CompareValue()( node.m_value, key );
		}
		template <typename K> // Transparent comparison: key type with Value
		bool operator()( const K& key, const Node& node ) const {
			return CompareValue()( key, node.m_value );
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
private:
	iterator link_back( auto tuple ){
		if constexpr ( UniqueValues ){
			if( !std::get<1>( tuple ) ) // not inserted //!assert or do not link
				return iterator( &const_cast<Node&>( *std::get<0>( tuple ) ) );
		}
		Node *newNode = &const_cast<Node&>( *std::get<0>( tuple ) );
		Node::link( m_end.m_prev, newNode );
		Node::link( newNode, m_end.asNode() );
		return iterator( newNode );
	}
public:
	iterator push_back( const Value& value ){
		return link_back( std::tuple( m_set.emplace( value ) ) );
	}
	iterator push_back( Value&& value ){
		return link_back( std::tuple( m_set.emplace( std::move( value ) ) ) );
	}
	template <class... Args>
	iterator emplace_back( Args&&...args ) {
		return link_back( std::tuple( m_set.emplace( std::forward<Args>( args )... ) ) );
	}
	void erase( const Value& value ){
		const auto it = m_set.find( value ); // note: multiset finds w/e value from equals
		if( it != m_set.cend() ){ //!assert or do not erase
			Node::link( it->m_prev, it->m_next );
			m_set.erase( it );
		}
	}
	template<typename K>
	const_iterator find( const K& key ) const { // note: multiset finds w/e value from equals
		const auto it = m_set.find( key );
		return ( it == m_set.cend() )? end() : const_iterator( &( *it ) );
	}
	template<typename K>
	iterator find( const K& key ) { // note: multiset finds w/e value from equals
		const auto it = m_set.find( key );
		return ( it == m_set.cend() )? end() : iterator( &const_cast<Node&>( *it ) );
	}
	template<typename K>
		requires ( !UniqueValues )
	iterator find_first( const K& key ) {
		auto it = m_set.find( key ); // multiset finds w/e value from equals, find 1st one
		if( it == m_set.cend() )
			return end();
		while( it != m_set.cbegin() && !Compare()( *std::prev( it ), *it ) )
			--it;
		return iterator( &const_cast<Node&>( *it ) );
	}
	Value& back(){
		return m_end.m_prev->m_value;
	}
	const Value& back() const {
		return m_end.m_prev->m_value;
	}
};

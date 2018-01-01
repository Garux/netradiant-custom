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

/// \file
/// \brief Type-safe techniques for binding the first argument of an opaque callback.

#include <cstddef>
#include "functional.h"

template<typename Type>
inline void* convertToOpaque( Type* t ){
	return t;
}
template<typename Type>
inline void* convertToOpaque( const Type* t ){
	return const_cast<Type*>( t );
}
template<typename Type>
inline void* convertToOpaque( Type& t ){
	return &t;
}
template<typename Type>
inline void* convertToOpaque( const Type& t ){
	return const_cast<Type*>( &t );
}


template<typename Type>
class ConvertFromOpaque
{
};

template<typename Type>
class ConvertFromOpaque<Type&>
{
public:
	static Type& apply( void* p ){
		return *static_cast<Type*>( p );
	}
};

template<typename Type>
class ConvertFromOpaque<const Type&>
{
public:
	static const Type& apply( void* p ){
		return *static_cast<Type*>( p );
	}
};


template<typename Type>
class ConvertFromOpaque<Type*>
{
public:
	static Type* apply( void* p ){
		return static_cast<Type*>( p );
	}
};

template<typename Type>
class ConvertFromOpaque<const Type*>
{
public:
	static const Type* apply( void* p ){
		return static_cast<Type*>( p );
	}
};

template<typename Thunk_>
class CallbackBase
{
	void* m_environment;
	Thunk_ m_thunk;
public:
	typedef Thunk_ Thunk;
	CallbackBase( void* environment, Thunk function ) : m_environment( environment ), m_thunk( function ){
	}
	void* getEnvironment() const {
		return m_environment;
	}
	Thunk getThunk() const {
		return m_thunk;
	}
};

template<typename Thunk>
inline bool operator==( const CallbackBase<Thunk>& self, const CallbackBase<Thunk>& other ){
	return self.getEnvironment() == other.getEnvironment() && self.getThunk() == other.getThunk();
}
template<typename Thunk>
inline bool operator!=( const CallbackBase<Thunk>& self, const CallbackBase<Thunk>& other ){
	return !( self == other );
}
template<typename Thunk>
inline bool operator<( const CallbackBase<Thunk>& self, const CallbackBase<Thunk>& other ){
	return self.getEnvironment() < other.getEnvironment() ||
	       ( !( other.getEnvironment() < self.getEnvironment() ) && self.getThunk() < other.getThunk() );
}

template<class Caller, class F>
class BindFirstOpaqueN;

template<class Caller, class R, class FirstBound, class... Ts>
class BindFirstOpaqueN<Caller, R(FirstBound, Ts...)>
{
	FirstBound firstBound;
public:
	explicit BindFirstOpaqueN( FirstBound firstBound ) : firstBound( firstBound ){
	}

	R operator()( Ts... args ) const {
		return Caller::call( firstBound, args... );
	}

	FirstBound getBound() const {
		return firstBound;
	}

	static R thunk( void *environment, Ts... args ){
		return Caller::call( ConvertFromOpaque<FirstBound>::apply( environment ), args... );
	}

	void *getEnvironment() const {
		return convertToOpaque( firstBound );
	}
};

template<class Caller>
using BindFirstOpaque = BindFirstOpaqueN<Caller, get_func<Caller>>;

/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer.
///
/// Use with the callback constructors MemberCaller0, ConstMemberCaller0, ReferenceCaller0, ConstReferenceCaller0, PointerCaller0, ConstPointerCaller0 and FreeCaller0.
template<class F>
class Callback;

template<class R, class... Ts>
class Callback<R(Ts...)> : public CallbackBase<R(*)(void *, Ts...)>
{
	using Base = CallbackBase<R (*)(void *, Ts...)>;

	static R nullThunk( void *, Ts... ){
	}

public:
	using func = R(Ts...);

	Callback() : Base( 0, nullThunk ){
	}

	template<typename Caller>
	Callback( const BindFirstOpaque<Caller>& caller ) : Base( caller.getEnvironment(), BindFirstOpaque<Caller>::thunk ){
	}

	Callback( void *environment, typename Base::Thunk function ) : Base( environment, function ){
	}

	R operator()( Ts... args ) const {
		return Base::getThunk()( Base::getEnvironment(), args... );
	}
};

namespace detail {
	template<class F>
	struct Arglist;

	template<class R, class Head, class... Ts>
	struct Arglist<R(Head, Ts...)>
	{
		using type = R(Head, Ts...);

		template <class Unshift>
		using unshift = Arglist<R(Unshift, Head, Ts...)>;

		using shift = Arglist<R(Ts...)>;
	};

	template<class R, class... Ts>
	struct Arglist<R(Ts...)>
	{
		using type = R(Ts...);

		template <class Unshift>
		using unshift = Arglist<R(Unshift, Ts...)>;
	};
}

template<typename Caller>
inline Callback<typename detail::Arglist<get_func<Caller>>::shift::type> makeCallback( const Caller& caller, get_argument<Caller, 0> callee ){
	return Callback<typename detail::Arglist<get_func<Caller>>::shift::type>( BindFirstOpaque<Caller>( callee ) );
}

template<typename Caller>
inline Callback<get_func<Caller>> makeStatelessCallback( const Caller& caller ){
	return makeCallback( CallerShiftFirst<Caller, typename detail::Arglist<get_func<Caller>>::template unshift<void *>::type>(), nullptr );
}

/// \brief Forms a Callback from a non-const Environment reference and a non-const Environment member-function.
///
/// \dontinclude generic/callback.cpp
/// \skipline MemberCaller0 example
/// \until end example

template<class Environment, class F, MemberFunction<Environment, F> member>
using MemberCaller = BindFirstOpaque<typename MemberN<Environment, F>::template instance<member>>;

/// \brief Forms a Callback from a const Environment reference and a const Environment member-function.
///
/// \dontinclude generic/callback.cpp
/// \skipline MemberCaller0 example
/// \until end example
template<class Environment, class F, ConstMemberFunction<Environment, F> member>
using ConstMemberCaller = BindFirstOpaque<typename ConstMemberN<Environment, F>::template instance<member>>;

/// \brief Forms a Callback from a non-const Environment reference and a free function which operates on a non-const Environment reference.
///
/// \dontinclude generic/callback.cpp
/// \skipline ReferenceCaller0 example
/// \until end example
template<class Environment, class F, typename detail::Arglist<F>::template unshift<Environment &>::type *func>
using ReferenceCaller = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<Environment &>::type>::template instance<func>>;

/// \brief Forms a Callback from a const Environment reference and a free function which operates on a const Environment reference.
///
/// \dontinclude generic/callback.cpp
/// \skipline ReferenceCaller0 example
/// \until end example
template<class Environment, class F, typename detail::Arglist<F>::template unshift<const Environment &>::type *func>
using ConstReferenceCaller = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<const Environment &>::type>::template instance<func>>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer.
template<class Environment, class F, typename detail::Arglist<F>::template unshift<Environment *>::type *func>
using PointerCaller = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<Environment *>::type>::template instance<func>>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer.
template<class Environment, class F, typename detail::Arglist<F>::template unshift<const Environment *>::type *func>
using ConstPointerCaller = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<const Environment *>::type>::template instance<func>>;

/// \brief Forms a Callback from a free function
template<class F, F *func>
class FreeCaller : public BindFirstOpaque<CallerShiftFirst<
        typename FunctionN<F>::template instance<func>,
        typename detail::Arglist<F>::template unshift<void *>::type
>> {
public:
	FreeCaller()
	        : BindFirstOpaque<CallerShiftFirst<
	        typename FunctionN<F>::template instance<func>,
	        typename detail::Arglist<F>::template unshift<void *>::type
	>>( nullptr ) {
	}
};

/// \brief  Constructs a Callback1 from a non-const \p functor
///
/// \param Functor Must define \c operator()(argument) and its signature as \c func.
template<typename Functor>
inline Callback<get_func<Functor>> makeCallback( Functor& functor ){
	return Callback<get_func<Functor>>( MemberCaller<Functor, get_func<Functor>, &Functor::operator()>( functor ) );
}

/// \brief  Constructs a Callback1 from a const \p functor
///
/// \param Functor Must define const \c operator()(argument) and its signature as \c func.
template<typename Functor>
inline Callback<get_func<Functor>> makeCallback( const Functor& functor ){
	return Callback<get_func<Functor>>( ConstMemberCaller<Functor, get_func<Functor>, &Functor::operator()>( functor ) );
}

using BoolImportCallback = Callback<void(bool)>;
using BoolExportCallback = Callback<void(const BoolImportCallback&)>;

using IntImportCallback = Callback<void(int)>;
using IntExportCallback = Callback<void(const IntImportCallback&)>;

using FloatImportCallback = Callback<void(float)>;
using FloatExportCallback = Callback<void(const FloatImportCallback&)>;

using StringImportCallback = Callback<void(const char*)>;
using StringExportCallback = Callback<void(const StringImportCallback&)>;

using SizeImportCallback = Callback<void(std::size_t)>;
using SizeExportCallback = Callback<void(const SizeImportCallback&)>;
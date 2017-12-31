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
#include "callbackfwd.h"

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
using BindFirstOpaque = BindFirstOpaqueN<Caller, typename Caller::func>;

template<class F>
class CallbackN;

template<class R, class... Ts>
class CallbackN<R(Ts...)> : public CallbackBase<R(*)(void *, Ts...)>
{
	using Base = CallbackBase<R (*)(void *, Ts...)>;

	static R nullThunk( void *, Ts... ){
	}

public:
	using func = R(Ts...);

	CallbackN() : Base( 0, nullThunk ){
	}

	template<typename Caller>
	CallbackN( const BindFirstOpaque<Caller>& caller ) : Base( caller.getEnvironment(), BindFirstOpaque<Caller>::thunk ){
	}

	CallbackN( void *environment, typename Base::Thunk function ) : Base( environment, function ){
	}

	R operator()( Ts... args ) const {
		return Base::getThunk()( Base::getEnvironment(), args... );
	}
};

/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer.
///
/// Use with the callback constructors MemberCaller, ConstMemberCaller, ReferenceCaller, ConstReferenceCaller, PointerCaller, ConstPointerCaller and FreeCaller.
template<class Result>
class Callback0 : public CallbackN<Result()>
{
public:
	using CallbackN<Result()>::CallbackN;
};

template<typename Caller>
inline Callback0<get_result_type<Caller>> makeCallback0( const Caller& caller, get_argument<Caller, 0> callee ){
	return Callback0<get_result_type<Caller>>( BindFirstOpaque<Caller>( callee ) );
}
template<typename Caller>
inline Callback0<get_result_type<Caller>> makeStatelessCallback0( const Caller& caller ){
	return makeCallback0( Caller0To1<Caller>(), 0 );
}

typedef Callback0<void> Callback;



/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer and one other argument.
///
/// Use with the callback constructors MemberCaller1, ConstMemberCaller1, ReferenceCaller1, ConstReferenceCaller1, PointerCaller1, ConstPointerCaller1 and FreeCaller1.
template<class FirstArgument, class Result>
class Callback1 : public CallbackN<Result(FirstArgument)>
{
public:
	using CallbackN<Result(FirstArgument)>::CallbackN;
};

template<typename Caller>
inline Callback1<get_argument<Caller, 1>, get_result_type<Caller>>
makeCallback1( const Caller& caller, get_argument<Caller, 0> callee ){
	return Callback1<get_argument<Caller, 1>, get_result_type<Caller>>( BindFirstOpaque<Caller>( callee ) );
}
template<typename Caller>
inline Callback1<get_argument<Caller, 1>, get_result_type<Caller>> makeStatelessCallback1( const Caller& caller ){
	return makeCallback1( Caller1To2<Caller>(), 0 );
}


/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer and two other arguments.
///
template<typename FirstArgument, typename SecondArgument, typename Result>
class Callback2 : public CallbackN<Result(FirstArgument, SecondArgument)>
{
public:
	using CallbackN<Result(FirstArgument, SecondArgument)>::CallbackN;
};

template<typename Caller>
inline Callback2<
         get_argument<Caller, 1>,
         get_argument<Caller, 2>,
         get_result_type<Caller>
> makeCallback2( const Caller& caller, get_argument<Caller, 0> callee ){
	return Callback2<
	           get_argument<Caller, 1>,
	           get_argument<Caller, 2>,
	           get_result_type<Caller>
	       >( BindFirstOpaque<Caller>( callee ) );
}
template<typename Caller>
inline Callback2<
         get_argument<Caller, 0>,
         get_argument<Caller, 1>,
         get_result_type<Caller>
> makeStatelessCallback2( const Caller& caller){
	return makeCallback2( Caller2To3<Caller>(), 0 );
}


/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer and three other arguments.
///
template<typename FirstArgument, typename SecondArgument, typename ThirdArgument, typename Result>
class Callback3 : public CallbackN<Result(FirstArgument, SecondArgument, ThirdArgument)>
{
public:
	using CallbackN<Result(FirstArgument, SecondArgument, ThirdArgument)>::CallbackN;
};

template<typename Caller>
inline Callback3<
         get_argument<Caller, 1>,
         get_argument<Caller, 2>,
         get_argument<Caller, 3>,
         get_result_type<Caller>
> makeCallback3( const Caller& caller, get_argument<Caller, 0> callee ){
	return Callback3<
	            get_argument<Caller, 1>,
	            get_argument<Caller, 2>,
	            get_argument<Caller, 3>,
	            get_result_type<Caller>
	       >( BindFirstOpaque<Caller>( callee ) );
}
template<typename Caller>
inline Callback3<
         get_argument<Caller, 0>,
         get_argument<Caller, 1>,
         get_argument<Caller, 2>,
         get_result_type<Caller>
> makeStatelessCallback3( const Caller& caller ){
	return makeCallback3( Caller3To4<Caller>(), 0 );
}


/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer and four other arguments.
///
template<typename FirstArgument, typename SecondArgument, typename ThirdArgument, typename FourthArgument, typename Result>
class Callback4 : public CallbackN<Result(FirstArgument, SecondArgument, ThirdArgument, FourthArgument)>
{
public:
	using CallbackN<Result(FirstArgument, SecondArgument, ThirdArgument, FourthArgument)>::CallbackN;
};

template<typename Caller>
inline Callback4<
         get_argument<Caller, 1>,
         get_argument<Caller, 2>,
         get_argument<Caller, 3>,
         get_argument<Caller, 4>,
         get_result_type<Caller>
> makeCallback4( const Caller& caller, get_argument<Caller, 0> callee ){
	return Callback4<
	            get_argument<Caller, 1>,
	            get_argument<Caller, 2>,
	            get_argument<Caller, 3>,
	            get_argument<Caller, 4>,
	            get_result_type<Caller>
	       >( BindFirstOpaque<Caller>( callee ) );
}
template<typename Caller>
inline Callback4<
         get_argument<Caller, 0>,
         get_argument<Caller, 1>,
         get_argument<Caller, 2>,
         get_argument<Caller, 3>,
         get_result_type<Caller>
> makeStatelessCallback4( const Caller& caller ){
	return makeCallback4( Caller4To5<Caller>(), 0 );
}


/// \brief Forms a Callback from a non-const Environment reference and a non-const Environment member-function.
///
/// \dontinclude generic/callback.cpp
/// \skipline MemberCaller example
/// \until end example
template<class Environment, void(Environment::*member)()>
using MemberCaller = BindFirstOpaque<Member<Environment, void, member>>;

/// \brief Forms a Callback from a const Environment reference and a const Environment member-function.
///
/// \dontinclude generic/callback.cpp
/// \skipline MemberCaller example
/// \until end example
template<class Environment, void(Environment::*member)() const>
using ConstMemberCaller = BindFirstOpaque<ConstMember<Environment, void, member>>;

/// \brief Forms a Callback from a non-const Environment reference and a const Environment member-function which takes one argument.
template<class Environment, class FirstArgument, void(Environment::*member)(FirstArgument)>
using MemberCaller1 = BindFirstOpaque<Member1<Environment, FirstArgument, void, member>>;

/// \brief Forms a Callback from a const Environment reference and a const Environment member-function which takes one argument.
template<class Environment, class FirstArgument, void(Environment::*member)(FirstArgument) const>
using ConstMemberCaller1 = BindFirstOpaque<ConstMember1<Environment, FirstArgument, void, member>>;

/// \brief Forms a Callback from a non-const Environment reference and a free function which operates on a non-const Environment reference.
///
/// \dontinclude generic/callback.cpp
/// \skipline ReferenceCaller example
/// \until end example
template<class Environment, void(*func)(Environment &)>
using ReferenceCaller = BindFirstOpaque<Function1<Environment &, void, func>>;

/// \brief Forms a Callback from a const Environment reference and a free function which operates on a const Environment reference.
///
/// \dontinclude generic/callback.cpp
/// \skipline ReferenceCaller example
/// \until end example
template<class Environment, void(*func)(const Environment &)>
using ConstReferenceCaller = BindFirstOpaque<Function1<const Environment &, void, func>>;

/// \brief Forms a Callback from a non-const Environment reference and a free function which operates on a non-const Environment reference and one other argument.
template<class Environment, class FirstArgument, void(*func)(Environment &, FirstArgument)>
using ReferenceCaller1 = BindFirstOpaque<Function2<Environment &, FirstArgument, void, func>>;

/// \brief Forms a Callback from a const Environment reference and a free function which operates on a const Environment reference and one other argument.
template<class Environment, class FirstArgument, void(*func)(const Environment &, FirstArgument)>
using ConstReferenceCaller1 = BindFirstOpaque<Function2<const Environment &, FirstArgument, void, func>>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer.
template<class Environment, void(*func)(Environment *)>
using PointerCaller = BindFirstOpaque<Function1<Environment *, void, func>>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer.
template<class Environment, void(*func)(const Environment *)>
using ConstPointerCaller = BindFirstOpaque<Function1<const Environment *, void, func>>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer and one other argument.
template<class Environment, class FirstArgument, void(*func)(Environment *, FirstArgument)>
using PointerCaller1 = BindFirstOpaque<Function2<Environment *, FirstArgument, void, func>>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer and one other argument.
template<class Environment, class FirstArgument, void(*func)(const Environment *, FirstArgument)>
using ConstPointerCaller1 = BindFirstOpaque<Function2<const Environment *, FirstArgument, void, func>>;

/// \brief Forms a Callback from a free function which takes no arguments.
template<void(*func)()>
class FreeCaller : public BindFirstOpaque<Caller0To1<Function0<void, func>>>
{
public:
	FreeCaller() : BindFirstOpaque<Caller0To1<Function0<void, func>>>( 0 ){
	}
};

/// \brief Forms a Callback from a free function which takes a single argument.
template<class FirstArgument, void(*func)(FirstArgument)>
class FreeCaller1 : public BindFirstOpaque<Caller1To2<Function1<FirstArgument, void, func>>>
{
public:
	FreeCaller1() : BindFirstOpaque<Caller1To2<Function1<FirstArgument, void, func>>>( 0 ){
	}
};


/// \brief Constructs a Callback from a non-const \p functor with zero arguments.
///
/// \param Functor Must define \c operator()().
template<typename Functor>
inline Callback makeCallback( Functor& functor ){
	return Callback( MemberCaller<Functor, &Functor::operator()>( functor ) );
}

/// \brief  Constructs a Callback from a const \p functor with zero arguments.
///
/// \param Functor Must define const \c operator()().
template<typename Functor>
inline Callback makeCallback( const Functor& functor ){
	return Callback( ConstMemberCaller<Functor, &Functor::operator()>( functor ) );
}

/// \brief  Constructs a Callback1 from a non-const \p functor with one argument.
///
/// \param Functor Must define \c operator()(argument) and its signature as \c func.
template<typename Functor>
inline Callback1<get_argument<Functor, 0>> makeCallback1( Functor& functor ){
	typedef get_argument<Functor, 0> FirstArgument;
	return Callback1<FirstArgument>( MemberCaller1<Functor, FirstArgument, &Functor::operator()>( functor ) );
}

/// \brief  Constructs a Callback1 from a const \p functor with one argument.
///
/// \param Functor Must define const \c operator()(argument) and its signature as \c func.
template<typename Functor>
inline Callback1<get_argument<Functor, 0>> makeCallback1( const Functor& functor ){
	typedef get_argument<Functor, 0> FirstArgument;
	return Callback1<FirstArgument>( ConstMemberCaller1<Functor, FirstArgument, &Functor::operator()>( functor ) );
}


typedef Callback1<bool> BoolImportCallback;
typedef Callback1<const BoolImportCallback&> BoolExportCallback;

typedef Callback1<int> IntImportCallback;
typedef Callback1<const IntImportCallback&> IntExportCallback;

typedef Callback1<float> FloatImportCallback;
typedef Callback1<const FloatImportCallback&> FloatExportCallback;

typedef Callback1<const char*> StringImportCallback;
typedef Callback1<const StringImportCallback&> StringExportCallback;

typedef Callback1<std::size_t> SizeImportCallback;
typedef Callback1<const SizeImportCallback&> SizeExportCallback;

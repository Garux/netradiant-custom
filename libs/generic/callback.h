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
/// Use with the callback constructors MemberCaller, ConstMemberCaller, ReferenceCaller, ConstReferenceCaller, PointerCaller, ConstPointerCaller and FreeCaller.
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
inline CallbackN<typename detail::Arglist<get_func<Caller>>::shift::type> makeCallbackN( const Caller& caller, get_argument<Caller, 0> callee ){
	return CallbackN<typename detail::Arglist<get_func<Caller>>::shift::type>( BindFirstOpaque<Caller>( callee ) );
}

template<typename Caller>
inline CallbackN<get_func<Caller>> makeStatelessCallbackN( const Caller& caller ){
	return makeCallbackN( CallerShiftFirst<Caller, typename detail::Arglist<get_func<Caller>>::template unshift<void *>::type>(), nullptr );
}

namespace detail {
	template<class Object, class F>
	struct MemberFunction;

	template<class Object, class R, class... Ts>
	struct MemberFunction<Object, R(Ts...)>
	{
		using type = R(Object::*)(Ts...);
		using type_const = R(Object::*)(Ts...) const;
	};
}

template<class Object, class F>
using MemberFunction = typename detail::MemberFunction<Object, F>::type;

template<class Object, class F>
using ConstMemberFunction = typename detail::MemberFunction<Object, F>::type_const;

/// \brief Forms a Callback from a non-const Environment reference and a non-const Environment member-function.
///
/// \dontinclude generic/callback.cpp
/// \skipline MemberCaller example
/// \until end example

template<class Environment, class F, MemberFunction<Environment, F> member>
using MemberCallerN = BindFirstOpaque<typename MemberN<Environment, F>::template instance<member>>;

/// \brief Forms a Callback from a const Environment reference and a const Environment member-function.
///
/// \dontinclude generic/callback.cpp
/// \skipline MemberCaller example
/// \until end example
template<class Environment, class F, ConstMemberFunction<Environment, F> member>
using ConstMemberCallerN = BindFirstOpaque<typename ConstMemberN<Environment, F>::template instance<member>>;

/// \brief Forms a Callback from a non-const Environment reference and a free function which operates on a non-const Environment reference.
///
/// \dontinclude generic/callback.cpp
/// \skipline ReferenceCaller example
/// \until end example
template<class Environment, class F, typename detail::Arglist<F>::template unshift<Environment &>::type *func>
using ReferenceCallerN = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<Environment &>::type>::template instance<func>>;

/// \brief Forms a Callback from a const Environment reference and a free function which operates on a const Environment reference.
///
/// \dontinclude generic/callback.cpp
/// \skipline ReferenceCaller example
/// \until end example
template<class Environment, class F, typename detail::Arglist<F>::template unshift<const Environment &>::type *func>
using ConstReferenceCallerN = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<const Environment &>::type>::template instance<func>>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer.
template<class Environment, class F, typename detail::Arglist<F>::template unshift<Environment *>::type *func>
using PointerCallerN = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<Environment *>::type>::template instance<func>>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer.
template<class Environment, class F, typename detail::Arglist<F>::template unshift<const Environment *>::type *func>
using ConstPointerCallerN = BindFirstOpaque<typename FunctionN<typename detail::Arglist<F>::template unshift<const Environment *>::type>::template instance<func>>;

/// \brief Forms a Callback from a free function
template<class F, F *func>
class FreeCallerN : public BindFirstOpaque<CallerShiftFirst<
        typename FunctionN<F>::template instance<func>,
        typename detail::Arglist<F>::template unshift<void *>::type
>> {
public:
	FreeCallerN()
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
inline CallbackN<get_func<Functor>> makeCallbackN( Functor& functor ){
	return CallbackN<get_func<Functor>>( MemberCallerN<Functor, get_func<Functor>, &Functor::operator()>( functor ) );
}

/// \brief  Constructs a Callback1 from a const \p functor
///
/// \param Functor Must define const \c operator()(argument) and its signature as \c func.
template<typename Functor>
inline CallbackN<get_func<Functor>> makeCallbackN( const Functor& functor ){
	return CallbackN<get_func<Functor>>( ConstMemberCallerN<Functor, get_func<Functor>, &Functor::operator()>( functor ) );
}

// todo: inline

#define makeCallback makeCallbackN

using Callback = CallbackN<void()>;

template<class Result>
using Callback0 = CallbackN<Result()>;

template<class FirstArgument, class Result = void>
using Callback1 = CallbackN<Result(FirstArgument)>;

template<typename FirstArgument, typename SecondArgument, typename Result = void>
using Callback2 = CallbackN<Result(FirstArgument, SecondArgument)>;

template<typename FirstArgument, typename SecondArgument, typename ThirdArgument, typename Result = void>
using Callback3 = CallbackN<Result(FirstArgument, SecondArgument, ThirdArgument)>;

template<typename FirstArgument, typename SecondArgument, typename ThirdArgument, typename FourthArgument, typename Result = void>
using Callback4 = CallbackN<Result(FirstArgument, SecondArgument, ThirdArgument, FourthArgument)>;

#define makeCallback0 makeCallbackN
#define makeStatelessCallback0 makeStatelessCallbackN

#define makeCallback1 makeCallbackN
#define makeStatelessCallback1 makeStatelessCallbackN

#define makeCallback2 makeCallbackN
#define makeStatelessCallback2 makeStatelessCallbackN

#define makeCallback3 makeCallbackN
#define makeStatelessCallback3 makeStatelessCallbackN

#define makeCallback4 makeCallbackN
#define makeStatelessCallback4 makeStatelessCallbackN

template<class Environment, void(Environment::*member)()>
using MemberCaller = MemberCallerN<Environment, void(), member>;

template<class Environment, void(Environment::*member)() const>
using ConstMemberCaller = ConstMemberCallerN<Environment, void(), member>;

template<class Environment, class FirstArgument, void(Environment::*member)(FirstArgument)>
using MemberCaller1 = MemberCallerN<Environment, void(FirstArgument), member>;

template<class Environment, class FirstArgument, void(Environment::*member)(FirstArgument) const>
using ConstMemberCaller1 = ConstMemberCallerN<Environment, void(FirstArgument), member>;

template<class Environment, void(*func)(Environment &)>
using ReferenceCaller = ReferenceCallerN<Environment, void(), func>;

template<class Environment, void(*func)(const Environment &)>
using ConstReferenceCaller = ConstReferenceCallerN<Environment, void(), func>;

/// \brief Forms a Callback from a non-const Environment reference and a free function which operates on a non-const Environment reference and one other argument.
template<class Environment, class FirstArgument, void(*func)(Environment &, FirstArgument)>
using ReferenceCaller1 = ReferenceCallerN<Environment, void(FirstArgument), func>;

/// \brief Forms a Callback from a const Environment reference and a free function which operates on a const Environment reference and one other argument.
template<class Environment, class FirstArgument, void(*func)(const Environment &, FirstArgument)>
using ConstReferenceCaller1 = ConstReferenceCallerN<Environment, void(FirstArgument), func>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer.
template<class Environment, void(*func)(Environment *)>
using PointerCaller = PointerCallerN<Environment, void(), func>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer.
template<class Environment, void(*func)(const Environment *)>
using ConstPointerCaller = ConstPointerCallerN<Environment, void(), func>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer and one other argument.
template<class Environment, class FirstArgument, void(*func)(Environment *, FirstArgument)>
using PointerCaller1 = PointerCallerN<Environment, void(FirstArgument), func>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer and one other argument.
template<class Environment, class FirstArgument, void(*func)(const Environment *, FirstArgument)>
using ConstPointerCaller1 = ConstPointerCallerN<Environment, void(FirstArgument), func>;

template<void(*func)()>
using FreeCaller = FreeCallerN<void(), func>;

template<class FirstArgument, void(*func)(FirstArgument)>
using FreeCaller1 = FreeCallerN<void(FirstArgument), func>;

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

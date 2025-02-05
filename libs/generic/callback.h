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

namespace detail {

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

}

namespace detail {

	template<class Type>
	struct ConvertFromOpaque
	{
	};

	// reference

	template<class T>
	inline const void *convertToOpaque( const T& t ){
		return &t;
	}

	template<class T>
	struct ConvertFromOpaque<const T &>
	{
		static T const &apply( void *p ){
			return *static_cast<const T *>( p );
		}
	};

	template<class T>
	inline void *convertToOpaque( T &t ){
		return &t;
	}

	template<class T>
	struct ConvertFromOpaque<T &>
	{
		static T &apply( void *p ){
			return *static_cast<T *>( p );
		}
	};

	// pointer

	template<class T, class U = typename std::enable_if<!std::is_function<T>::value>::type>
	inline const void *convertToOpaque( const T *t ){
		return t;
	}

	template<class T>
	struct ConvertFromOpaque<const T *>
	{
		static const T *apply( void *p ){
			return static_cast<const T *>( p );
		}
	};

	template<class T, class U = typename std::enable_if<!std::is_function<T>::value>::type>
	inline void *convertToOpaque( T *t ){
		return t;
	}

	template<class T>
	struct ConvertFromOpaque<T *>
	{
		static T *apply( void *p ){
			return static_cast<T *>( p );
		}
	};

	// function pointer

	template<class R, class... Ts>
	inline const void *convertToOpaque( R(*const &t)(Ts...) ){
		return &t;
	}

	template<class R, class... Ts>
	struct ConvertFromOpaque<R(*const &)(Ts...)>
	{
		using Type = R(*)(Ts...);

		static Type const &apply( void *p ){
			return *static_cast<Type *>( p );
		}
	};

	template<class R, class... Ts>
	inline void *convertToOpaque( R(*&t)(Ts...) ){
		return &t;
	}

	template<class R, class... Ts>
	struct ConvertFromOpaque<R(*&)(Ts...)>
	{
		using Type = R(*)(Ts...);

		static Type &apply( void *p ){
			return *static_cast<Type *>( p );
		}
	};

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
			return thunk_( detail::ConvertFromOpaque<FirstBound>::apply( environment ), args... );
		}

		static R thunk_( FirstBound environment, Ts... args ){
			return Caller::call( environment, args... );
		}

		void *getEnvironment() const {
			return const_cast<void *>( detail::convertToOpaque( firstBound ) );
		}
	};

}

template<class Caller>
using BindFirstOpaque = detail::BindFirstOpaqueN<Caller, get_func<Caller>>;

/// \brief Combines a void pointer with a pointer to a function which operates on a void pointer.
///
/// Use with the callback constructors MemberCaller0, ConstMemberCaller0, ReferenceCaller0, ConstReferenceCaller0, PointerCaller0, ConstPointerCaller0 and FreeCaller0.
template<class F>
class Callback;

template<class R, class... Ts>
class Callback<R(Ts...)> : public detail::CallbackBase<R(*)(void *, Ts...)>
{
	using Base = detail::CallbackBase<R (*)(void *, Ts...)>;

	static R nullThunk( void *, Ts... ){
		return R{};
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

	template<class F>
	using ArgShift = typename detail::Arglist<F>::shift::type;

	template<class F, class T>
	using ArgUnshift = typename detail::Arglist<F>::template unshift<T>::type;
}

template<typename Caller>
inline Callback<detail::ArgShift<get_func<Caller>>> makeCallback( const Caller& caller, get_argument<Caller, 0> callee ){
	return BindFirstOpaque<Caller>( callee );
}

template<class Caller, class F>
class CallerShiftFirst;

template<class Caller, class R, class FirstArgument, class... Ts>
class CallerShiftFirst<Caller, R(FirstArgument, Ts...)>
{
public:
	using func = R(FirstArgument, Ts...);

	static R call( FirstArgument, Ts... args ){
		return Caller::call( args... );
	}
};

template<typename Caller>
inline Callback<get_func<Caller>> makeStatelessCallback( const Caller& caller ){
	return makeCallback( CallerShiftFirst<Caller, detail::ArgUnshift<get_func<Caller>, void *>>(), nullptr );
}

/// \brief Forms a Callback from a non-const Environment reference and a non-const Environment member-function.
template<class Environment, class F, MemberFunction<Environment, F> member>
using MemberCaller = BindFirstOpaque<Member<Environment, F, member>>;

/// \brief  Constructs a Callback1 from a non-const \p functor
///
/// \param Functor Must define \c operator()(arguments) and its signature as \c func.
template<typename Functor>
inline Callback<get_func<Functor>> makeCallback( Functor& functor ){
	return MemberCaller<Functor, get_func<Functor>, &Functor::operator()>( functor );
}

/// \brief Forms a Callback from a const Environment reference and a const Environment member-function.
template<class Environment, class F, ConstMemberFunction<Environment, F> member>
using ConstMemberCaller = BindFirstOpaque<ConstMember<Environment, F, member>>;

/// \brief  Constructs a Callback1 from a const \p functor
///
/// \param Functor Must define const \c operator()(arguments) and its signature as \c func.
template<typename Functor>
inline Callback<get_func<Functor>> makeCallback( const Functor& functor ){
	return ConstMemberCaller<Functor, get_func<Functor>, &Functor::operator()>( functor );
}

/// \brief Forms a Callback from a non-const Environment reference and a free function which operates on a non-const Environment reference.
template<class Environment, class F, detail::ArgUnshift<F, Environment &> *func>
using ReferenceCaller = BindFirstOpaque<Function<detail::ArgUnshift<F, Environment &>, func>>;

/// \brief Forms a Callback from a const Environment reference and a free function which operates on a const Environment reference.
template<class Environment, class F, detail::ArgUnshift<F, const Environment &> *func>
using ConstReferenceCaller = BindFirstOpaque<Function<detail::ArgUnshift<F, const Environment &>, func>>;

/// \brief Forms a Callback from a non-const Environment pointer and a free function which operates on a non-const Environment pointer.
template<class Environment, class F, detail::ArgUnshift<F, Environment *> *func>
using PointerCaller = BindFirstOpaque<Function<detail::ArgUnshift<F, Environment *>, func>>;

/// \brief Forms a Callback from a const Environment pointer and a free function which operates on a const Environment pointer.
template<class Environment, class F, detail::ArgUnshift<F, const Environment *> *func>
using ConstPointerCaller = BindFirstOpaque<Function<detail::ArgUnshift<F, const Environment *>, func>>;

namespace detail {
	template<class Caller, class F>
	class FreeCaller : public BindFirstOpaque<CallerShiftFirst<Caller, detail::ArgUnshift<F, void *>>>
	{
	public:
		FreeCaller() : BindFirstOpaque<CallerShiftFirst<Caller, detail::ArgUnshift<F, void *>>>( nullptr ){
		}
	};

	template <class F>
	struct FreeCallerWrapper;

	template <class R, class... Ts>
	struct FreeCallerWrapper<R(Ts...)>
	{
		using func = R(void *, Ts...);

		static R call( void *f, Ts... args ){
			// ideally, we'd get the implementation of the function type directly. Instead, it's passed in
			return reinterpret_cast<R(*)(Ts...)>( f )( args... );
		}
	};
}

/// \brief Forms a Callback from a free function
template<class F, F *func>
using FreeCaller = detail::FreeCaller<Function<F, func>, F>;

template<class R, class... Ts>
inline Callback<R(Ts...)> makeCallbackF( R(*func)(Ts...) ){
	void *pVoid = reinterpret_cast<void *>( func );
	return BindFirstOpaque<detail::FreeCallerWrapper<R(Ts...)>>( pVoid );
}

// todo: remove

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
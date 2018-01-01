
#pragma once

#include <functional>
#include <tuple>

namespace detail {

	template<int N>
	struct rank : rank<N - 1> {
	};

	template<>
	struct rank<0> {
	};

	struct get_func {

		template<class T>
		struct wrapper {
			using type = T;
		};

		template<class F>
		using func_member = wrapper<typename F::func>;

		template<class F>
		static wrapper<func_member<F>> test( rank<2> ) { return {}; }

		template<class F>
		struct func_lambda {
			using type = typename func_lambda<decltype(&F::operator())>::type;
		};

		template<class R, class... Ts>
		struct func_lambda<R(*)(Ts...)> {
			using type = R(Ts...);
		};

		template<class Object, class R, class... Ts>
		struct func_lambda<R(Object::*)(Ts...) const> {
			using type = R(Ts...);
		};

		template<class Object, class R, class... Ts>
		struct func_lambda<R(Object::*)(Ts...)> {
			using type = R(Ts...);
		};

		template<class F, class = func_lambda<F>>
		static wrapper<func_lambda<F>> test( rank<1> ) { return {}; }
	};

	template<class F>
	struct Fn;

	template<class R, class... Ts>
	struct Fn<R(Ts...)>
	{
		using result_type = R;

		template<int N>
		using get = typename std::tuple_element<N, std::tuple<Ts...>>::type;
	};
}

template<class Caller>
using get_func = typename decltype(detail::get_func::test<Caller>( detail::rank<2>{} ))::type::type;

template<class Caller>
using get_result_type = typename detail::Fn<get_func<Caller>>::result_type;

template<class Caller, int N>
using get_argument = typename detail::Fn<get_func<Caller>>::template get<N>;

namespace detail {

	template<class F>
	class FunctionN;

	template<class R, class... Ts>
	class FunctionN<R(Ts...)>
	{
	public:
		template<R(*f)(Ts...)>
		class instance
		{
		public:
			using func = R(Ts...);

			static R call( Ts... args ){
				return ( f )( args... );
			}
		};
	};

}

template<class F, F *func>
using Function = typename detail::FunctionN<F>::template instance<func>;

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

namespace detail {
	template<class Object, class F>
	class MemberN;

	template<class Object, class R, class... Ts>
	class MemberN<Object, R(Ts...)>
	{
	public:
		template<R(Object::*f)(Ts...)>
		class instance
		{
		public:
			using func = R(Object &, Ts...);

			static R call( Object& object, Ts... args ){
				return ( object.*f )( args... );
			}
		};
	};
}

template<class Object, class F>
using MemberFunction = typename detail::MemberFunction<Object, F>::type;

template<class Object, class F, MemberFunction<Object, F> func>
using Member = typename detail::MemberN<Object, F>::template instance<func>;

namespace detail {
	template<class Object, class F>
	class ConstMemberN;

	template<class Object, class R, class... Ts>
	class ConstMemberN<Object, R(Ts...)>
	{
	public:
		template<R(Object::*f)(Ts...) const>
		class instance
		{
		public:
			using func = R(const Object &, Ts...);

			static R call( const Object& object, Ts... args ){
				return ( object.*f )( args... );
			}
		};
	};
}

template<class Object, class F>
using ConstMemberFunction = typename detail::MemberFunction<Object, F>::type_const;

template<class Object, class F, ConstMemberFunction<Object, F> func>
using ConstMember = typename detail::ConstMemberN<Object, F>::template instance<func>;

// misc

namespace detail {
	template<int ...>
	struct seq
	{
	};

	template<int N, int... S>
	struct gens : gens<N - 1, N - 1, S...>
	{
	};

	template<int... S>
	struct gens<0, S...>
	{
		using type = seq<S...>;
	};

	template<int N>
	using seq_new = typename gens<N>::type;

	template<class Functor, class F>
	class FunctorNInvoke;

	template<class Functor, class R, class... Ts>
	class FunctorNInvoke<Functor, R(Ts...)>
	{
		std::tuple<Ts...> args;

		template<class T>
		struct caller;

		template<int ...I>
		struct caller<seq<I...>>
		{
			static inline R call( FunctorNInvoke<Functor, R(Ts...)> *self, Functor functor ){
				(void) self;
				return functor( std::get<I>( self->args )... );
			}
		};

	public:
		FunctorNInvoke( Ts... args ) : args( args... ){
		}

		inline R operator()( Functor functor ) {
			return caller<seq_new<sizeof...(Ts)>>::call( this, functor );
		}
	};
}

template<class Functor>
using FunctorInvoke = detail::FunctorNInvoke<Functor, get_func<Functor>>;

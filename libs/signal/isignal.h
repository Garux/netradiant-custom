
#pragma once

#include "generic/callback.h"
#include "signal/signalfwd.h"

class SignalHandlerResult
{
	bool value;
public:
	explicit SignalHandlerResult( bool value ) : value( value ){
	}
	bool operator==( SignalHandlerResult other ) const {
		return value == other.value;
	}
	bool operator!=( SignalHandlerResult other ) const {
		return !operator==( other );
	}
};

const SignalHandlerResult SIGNAL_CONTINUE_EMISSION = SignalHandlerResult( false );
const SignalHandlerResult SIGNAL_STOP_EMISSION = SignalHandlerResult( true );

template<class Caller, class F>
class SignalHandlerCallerN;

template<class Caller, class R, class... Ts>
class SignalHandlerCallerN<Caller, R(Ts...)>
{
public:
	using func = SignalHandlerResult(Ts...);

	static SignalHandlerResult call( Ts... args ) {
		Caller::call( args... );
		return SIGNAL_CONTINUE_EMISSION;
	}
};

template<class Caller>
using SignalHandlerCaller = SignalHandlerCallerN<Caller, get_func<Caller>>;

template<class Caller>
using SignalHandlerCaller1 = SignalHandlerCaller<Caller>;

template<class Caller>
using SignalHandlerCaller2 = SignalHandlerCaller<Caller>;

template<typename Caller>
using SignalHandlerCaller3 = SignalHandlerCaller<Caller>;

template<typename Caller>
using SignalHandlerCaller4 = SignalHandlerCaller<Caller>;

template<typename Other, typename True, typename False, typename Type>
class TypeEqual
{
public:
	using type = False;
};

template<typename Other, typename True, typename False>
class TypeEqual<Other, True, False, Other>
{
public:
	using type = True;
};

template<class CB, template<class T> class Wrapper>
class SignalHandlerN : public CB
{
public:
	template<typename Caller>
	SignalHandlerN( const BindFirstOpaque<Caller>& caller )
		: CB( BindFirstOpaque<typename TypeEqual<
			SignalHandlerResult,
			Caller,
			Wrapper<Caller>,
			get_result_type<Caller>
			>::type>( caller.getBound() ) ){
	}
};

class SignalHandler : public SignalHandlerN<Callback<SignalHandlerResult()>, SignalHandlerCaller1>
{
	using SignalHandlerN<Callback<SignalHandlerResult()>, SignalHandlerCaller1>::SignalHandlerN;
};

template<typename Caller>
inline SignalHandler makeSignalHandler( const BindFirstOpaque<Caller>& caller ){
	return SignalHandler( caller );
}
template<typename Caller>
inline SignalHandler makeSignalHandler( const Caller& caller, get_argument<Caller, 0> callee ){
	return SignalHandler( BindFirstOpaque<Caller>( callee ) );
}

template<typename FirstArgument>
class SignalHandler1 : public SignalHandlerN<Callback<SignalHandlerResult(FirstArgument)>, SignalHandlerCaller2>
{
    using SignalHandlerN<Callback<SignalHandlerResult(FirstArgument)>, SignalHandlerCaller2>::SignalHandlerN;
};

template<typename Caller>
inline SignalHandler1<get_argument<Caller, 1>> makeSignalHandler1( const BindFirstOpaque<Caller>& caller ){
	return SignalHandler1<get_argument<Caller, 1>>( caller );
}
template<typename Caller>
inline SignalHandler1<get_argument<Caller, 1>>
makeSignalHandler1( const Caller &caller, get_argument<Caller, 0> callee ) {
    return SignalHandler1<get_argument<Caller, 1>>( BindFirstOpaque<Caller>( callee ) );
}

template<typename FirstArgument, typename SecondArgument>
class SignalHandler2 : public SignalHandlerN<Callback<SignalHandlerResult(FirstArgument, SecondArgument)>, SignalHandlerCaller3>
{
	using SignalHandlerN<Callback<SignalHandlerResult(FirstArgument, SecondArgument)>, SignalHandlerCaller3>::SignalHandlerN;
};

template<typename Caller>
inline SignalHandler2<
            get_argument<Caller, 1>,
            get_argument<Caller, 2>
> makeSignalHandler2( const BindFirstOpaque<Caller>& caller ){
	return SignalHandler2<
	            get_argument<Caller, 1>,
	            get_argument<Caller, 2>
	       >( caller );
}
template<typename Caller>
inline SignalHandler2<
            get_argument<Caller, 1>,
            get_argument<Caller, 2>
> makeSignalHandler2( const Caller& caller, get_argument<Caller, 0> callee ){
	return SignalHandler2<
	            get_argument<Caller, 1>,
	            get_argument<Caller, 2>
	       >( BindFirstOpaque<Caller>( callee ) );
}

template<typename FirstArgument, typename SecondArgument, typename ThirdArgument>
class SignalHandler3 : public SignalHandlerN<Callback<SignalHandlerResult(FirstArgument, SecondArgument, ThirdArgument)>, SignalHandlerCaller4>
{
	using SignalHandlerN<Callback<SignalHandlerResult(FirstArgument, SecondArgument, ThirdArgument)>, SignalHandlerCaller4>::SignalHandlerN;
};

template<typename Caller>
inline SignalHandler3<
         get_argument<Caller, 1>,
         get_argument<Caller, 2>,
         get_argument<Caller, 3>
> makeSignalHandler3( const BindFirstOpaque<Caller>& caller ){
	return SignalHandler3<
	            get_argument<Caller, 1>,
	            get_argument<Caller, 2>,
	            get_argument<Caller, 3>
	       >( caller );
}
template<typename Caller>
inline SignalHandler3<
        get_argument<Caller, 1>,
        get_argument<Caller, 2>,
        get_argument<Caller, 3>
> makeSignalHandler3( const Caller& caller, get_argument<Caller, 0> callee ){
	return SignalHandler3<
	            get_argument<Caller, 1>,
	            get_argument<Caller, 2>,
	            get_argument<Caller, 3>
	       >( BindFirstOpaque<Caller>( callee ) );
}

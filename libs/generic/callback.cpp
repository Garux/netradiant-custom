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

#include "callback.h"

#if defined( _DEBUG ) || defined( DOXYGEN )

namespace ExampleMemberCaller
{
// MemberCaller example
class Integer
{
public:
	int value;

	void printValue() const {
		// print this->value here;
	}

	void setValue(){
		value = 3;
	}
// a typedef to make things more readable
	typedef MemberCaller<Integer, void(), &Integer::setValue> SetValueCaller;
};

void example(){
	Integer foo = { 0 };

	{
		Callback<void()> bar = ConstMemberCaller<Integer, void(), &Integer::printValue>( foo );

		// invoke the callback
		bar(); // foo.printValue()
	}


	{
		// use the typedef to improve readability
		Callback<void()> bar = Integer::SetValueCaller( foo );

		// invoke the callback
		bar(); // foo.setValue()
	}
}
// end example
}

namespace ExampleReferenceCaller
{
// ReferenceCaller example
void Int_printValue( const int& value ){
	// print value here;
}

void Int_setValue( int& value ){
	value = 3;
}

// a typedef to make things more readable
typedef ReferenceCaller<int, void(), Int_setValue> IntSetValueCaller;

void example(){
	int foo = 0;

	{
		Callback<void()> bar = ConstReferenceCaller<int, void(), Int_printValue>( foo );

		// invoke the callback
		bar(); // Int_printValue( foo )
	}


	{
		// use the typedef to improve readability
		Callback<void()> bar = IntSetValueCaller( foo );

		// invoke the callback
		bar(); // Int_setValue( foo )
	}
}
// end example
}

#endif

namespace
{
class A1
{
};
class A2
{
};
class A3
{
};
class A4
{
};

class Test
{
public:
	void test0(){
	}
	typedef Member<Test, void(), &Test::test0> Test0;
	typedef MemberCaller<Test, void(), &Test::test0> Test0Caller;
	void test0const() const {
	}
	typedef ConstMember<Test, void(), &Test::test0const> Test0Const;
	typedef ConstMemberCaller<Test, void(), &Test::test0const> Test0ConstCaller;
	void test1( A1 ){
	}
	typedef Member<Test, void(A1), &Test::test1> Test1;
	typedef MemberCaller<Test, void(A1), &Test::test1> Test1Caller;
	void test1const( A1 ) const {
	}
	typedef ConstMember<Test, void(A1), &Test::test1const> Test1Const;
	typedef ConstMemberCaller<Test, void(A1), &Test::test1const> Test1ConstCaller;
	void test2( A1, A2 ){
	}
	typedef Member<Test, void(A1, A2), &Test::test2> Test2;
	void test2const( A1, A2 ) const {
	}
	typedef ConstMember<Test, void(A1, A2), &Test::test2const> Test2Const;
	void test3( A1, A2, A3 ){
	}
	typedef Member<Test, void(A1, A2, A3), &Test::test3> Test3;
	void test3const( A1, A2, A3 ) const {
	}
	typedef ConstMember<Test, void(A1, A2, A3), &Test::test3const> Test3Const;
};

void test0free(){
}
void test1free( A1 ){
}
void test2free( A1, A2 ){
}
typedef Function<void(A1, A2), &test2free> Test2Free;
void test3free( A1, A2, A3 ){
}
typedef Function<void(A1, A2, A3), &test3free> Test3Free;


void test0( Test& test ){
}
typedef ReferenceCaller<Test, void(), &test0> Test0Caller;

void test0const( const Test& test ){
}
typedef ConstReferenceCaller<Test, void(), &test0const> Test0ConstCaller;

void test0p( Test* test ){
}
typedef PointerCaller<Test, void(), &test0p> Test0PCaller;

void test0constp( const Test* test ){
}
typedef ConstPointerCaller<Test, void(), &test0constp> Test0ConstPCaller;

void test1( Test& test, A1 ){
}
typedef ReferenceCaller<Test, void(A1), &test1> Test1Caller;

void test1const( const Test& test, A1 ){
}
typedef ConstReferenceCaller<Test, void(A1), &test1const> Test1ConstCaller;

void test1p( Test* test, A1 ){
}
typedef PointerCaller<Test, void(A1), &test1p> Test1PCaller;

void test1constp( const Test* test, A1 ){
}
typedef ConstPointerCaller<Test, void(A1), &test1constp> Test1ConstPCaller;

void test2( Test& test, A1, A2 ){
}
typedef Function<void(Test&, A1, A2), &test2> Test2;

void test3( Test& test, A1, A2, A3 ){
}
typedef Function<void(Test&, A1, A2, A3), &test3> Test3;

void instantiate(){
	Test test;
	const Test& testconst = test;
	{
		Callback<void()> a = makeCallbackF( &test0free );
		Callback<void()> b = Test::Test0Caller( test );
		b = makeCallback( Test::Test0(), test );
		Callback<void()> c = Test::Test0ConstCaller( testconst );
		c = makeCallback( Test::Test0Const(), test );
		Test0Caller{ test };
		Test0ConstCaller{ testconst };
		Test0PCaller{ &test };
		Test0ConstPCaller{ &testconst };
		a();
		bool u = a != b;
	}
	{
		typedef Callback<void(A1)> TestCallback1;
		TestCallback1 a = makeCallbackF( &test1free );
		TestCallback1 b = Test::Test1Caller( test );
		b = makeCallback( Test::Test1(), test );
		TestCallback1 c = Test::Test1ConstCaller( testconst );
		c = makeCallback( Test::Test1Const(), test );
		Test1Caller{ test };
		Test1ConstCaller{ testconst };
		Test1PCaller{ &test };
		Test1ConstPCaller{ &testconst };
		a( A1() );
		bool u = a != b;
	}
	{
		typedef Callback<void(A1, A2)> TestCallback2;
		TestCallback2 a = makeStatelessCallback( Test2Free() );
		TestCallback2 b = makeCallback( Test2(), test );
		makeCallback( Test::Test2(), test );
		makeCallback( Test::Test2Const(), test );
		a( A1(), A2() );
		bool u = a != b;
	}
	{
		typedef Callback<void(A1, A2, A3)> TestCallback3;
		TestCallback3 a = makeStatelessCallback( Test3Free() );
		TestCallback3 b = makeCallback( Test3(), test );
		makeCallback( Test::Test3(), test );
		makeCallback( Test::Test3Const(), test );
		a( A1(), A2(), A3() );
		bool u = a != b;
	}
}
}

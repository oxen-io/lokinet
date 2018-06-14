/* Copyright (c) 2016, Pollard Banknote Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <iostream>
#include <typeinfo>
#include <string>
#include <cstdlib>
#include <cxxabi.h>
#include "type_traits.h"

template< typename T >
struct meta
{
	static std::string name()
	{
		const std::type_info& first = typeid( T );

		const char* name = first.name();

		int status;

		std::string s;

		if ( char* realname = abi::__cxa_demangle(name, 0, 0, &status) )
		{
			s = realname;
			free(realname);
		}

		return s;
	}

};

template< typename T >
struct meta< T& >
{
	static std::string name()
	{
		return meta< T >::name() + "&";
	}

};

template< typename T >
struct meta< const T >
{
	static std::string name()
	{
		return meta< T >::name() + " const";
	}

};

template< typename T >
struct meta< volatile T >
{
	static std::string name()
	{
		return meta< T >::name() + " volatile";
	}

};

template< typename T >
struct meta< const volatile T >
{
	static std::string name()
	{
		return meta< T >::name() + " const volatile";
	}

};

template< template< class, class > class T, typename U, typename V >
void print_relation()
{
	std::cout << "\t(" << meta< U >::name() << ", " << meta< V >::name() << "): " << T< U, V >::value << std::endl;
}

/// @todo rename derived, pod, etc.
class Base
{
};

class Derived
	: public Base
{
};

class C
{
public:
	void do_nothing() const
	{
	}

};

class ConvertsToC
{
public: operator C() const
	{
		C x;

		return x;
	}

};

class Explicit
{
public:
	explicit Explicit(const Base&)
	{
	}

};

class ConvertsFromDerived
{
public:
	ConvertsFromDerived(const Derived&)
	{
	}

};

union Union
{
	double d;
	unsigned long l;
};

enum fruit {APPLE, ORANGE, BANANA};

/// @todo pointer to members
/// @todo For each type, also check pointer to that type, ref, cv-qualified,...
template< template< class > class T >
struct trait_test
{
	template< typename U >
	struct apply_trait
	{
		template< typename V >
		static void print_variation()
		{
			if ( !cpp::is_same< U, V >::value )
			{
				std::cout << "\t" << meta< V >::name() << ": " << T< V >::value << std::endl;
			}
		}

		static void print()
		{
			std::cout << "\t" << meta< U >::name() << ": " << T< U >::value << std::endl;

			print_variation< typename cpp::add_pointer< U >::type >();
			print_variation< typename cpp::add_const< U >::type >();
			print_variation< typename cpp::add_volatile< U >::type >();
			print_variation< typename cpp::add_cv< U >::type >();
			print_variation< typename cpp::add_lvalue_reference< U >::type >();
			print_variation< typename cpp::remove_pointer< U >::type >();
			print_variation< typename cpp::remove_const< U >::type >();
			print_variation< typename cpp::remove_volatile< U >::type >();
			print_variation< typename cpp::remove_cv< U >::type >();
			print_variation< typename cpp::remove_reference< U >::type >();
			print_variation< typename cpp::remove_extent< U >::type >();
			print_variation< typename cpp::remove_all_extents< U >::type >();
		}

	};


	static void test_all()
	{
		std::cout << "Void and variations" << std::endl;

		apply_trait< void >::print();

		std::cout << "Integral types and variations:" << std::endl;
		apply_trait< char >::print();
		apply_trait< signed char >::print();
		apply_trait< unsigned char >::print();
		apply_trait< bool >::print();
		apply_trait< short >::print();
		apply_trait< unsigned short >::print();
		apply_trait< int >::print();
		apply_trait< unsigned int >::print();
		apply_trait< long >::print();
		apply_trait< unsigned long >::print();
		apply_trait< bool >::print();

		std::cout << "Floating point types and variations:" << std::endl;
		apply_trait< float >::print();
		apply_trait< double >::print();
		apply_trait< long double >::print();

		std::cout << "Enum types and variations: " << std::endl;
		apply_trait< fruit >::print();

		std::cout << "Pointer types and variations:" << std::endl;
		apply_trait< void* >::print();
		apply_trait< int* >::print();
		apply_trait< const int* const* >::print();
		apply_trait< Base* >::print();
		apply_trait< Derived* >::print();

		std::cout << "Reference types and variations:" << std::endl;
		apply_trait< int& >::print();
		apply_trait< const int& >::print();

		std::cout << "Array types and variations:" << std::endl;
		apply_trait< int[2] >::print();

		std::cout << "Function types and variations:" << std::endl;
		apply_trait< void() >::print();
		apply_trait< int() >::print();
		apply_trait< int ( * )() >::print();
		apply_trait< int(&) ( ) >::print();
		apply_trait< int(...) >::print();
		apply_trait< int ( * )(...) >::print();
		apply_trait< int(&) ( ... ) >::print();
		apply_trait< int(int) >::print();
		apply_trait< int ( * )(int) >::print();
		apply_trait< int(&) (int) >::print();
		apply_trait< int(int, ...) >::print();
		apply_trait< int ( * )(int, ...) >::print();
		apply_trait< int(&) ( int, ... ) >::print();
		apply_trait< int(char, int) >::print();
		apply_trait< int ( * )(char, int) >::print();
		apply_trait< int(&) ( char, int ) >::print();
		apply_trait< int(char, int, ...) >::print();
		apply_trait< int ( * )(char, int, ...) >::print();
		apply_trait< int(&) ( char, int, ... ) >::print();

		std::cout << "Class types and variations: " << std::endl;
		apply_trait< Base >::print();
		apply_trait< Derived >::print();
		apply_trait< C >::print();
		apply_trait< ConvertsToC >::print();
		apply_trait< Explicit >::print();
		apply_trait< Union >::print();

		std::cout << "Function pointer types and variations:" << std::endl;
		apply_trait< int ( * )(int) >::print();
		std::cout << std::endl;

	}

};

template< template< class, class > class T >
void relation_test()
{
	std::cout << "CV qualification:" << std::endl;

	print_relation< T, int, const int >();
	std::cout << "\tint, const int: " << T< int, const int >::value << std::endl;

	std::cout << "Builtins:" << std::endl;
	print_relation< T, int, float >();
	print_relation< T, long, void >();
	print_relation< T, void, long >();

	std::cout << "Pointers:" << std::endl;
	print_relation< T, void*, Base* >();
	print_relation< T, Base*, void* >();
	print_relation< T, Derived*, Base* >();
	print_relation< T, Base*, Derived* >();
	print_relation< T, Derived*, C* >();
	print_relation< T, ConvertsToC*, bool >();

	std::cout << "References:" << std::endl;
	print_relation< T, Derived&, Base& >();
	print_relation< T, Base&, Derived& >();
	print_relation< T, Derived&, C& >();

	std::cout << "Arrays:" << std::endl;
	print_relation< T, int[2], const int* >();
	print_relation< T, int[2], float* >();

	std::cout << "Conversion via operator:" << std::endl;
	print_relation< T, ConvertsToC, C >();

	std::cout << "Conversion via constructor:" << std::endl;
	print_relation< T, Base, Explicit >();
	print_relation< T, Derived, ConvertsFromDerived >();
	std::cout << std::endl;
}

template< typename T >
struct trivially_true
	: cpp::true_type
{
};

template< typename T >
struct is_signed_integral
	: cpp::bool_constant< cpp::is_integral< T >::value&& !cpp::is_same< typename cpp::remove_cv< T >::type, bool >::value&& !cpp::is_enum< T >::value >
{
};

template< template< class > class T, template< class > class Predicate = trivially_true >
struct test_transform
{
	template< bool >
	struct tag {};

	template< typename U >
	static void print_variation(tag< true >)
	{
		std::cout << "\t" << meta< U >::name() << " -> " << meta< typename T< U >::type >::name() << std::endl;
	}

	template< typename U >
	static void print_variation(tag< false >)
	{
		// do nothing
	}

	template< typename U >
	struct apply_transform
	{
		static void print()
		{
			tag< Predicate< U >::value > t;
			print_variation< U >(t);

			print_maybe< typename cpp::add_pointer< U >::type >();
			print_maybe< typename cpp::add_const< U >::type >();
			print_maybe< typename cpp::add_volatile< U >::type >();
			print_maybe< typename cpp::add_cv< U >::type >();
			print_maybe< typename cpp::add_lvalue_reference< U >::type >();
			print_maybe< typename cpp::remove_pointer< U >::type >();
			print_maybe< typename cpp::remove_const< U >::type >();
			print_maybe< typename cpp::remove_volatile< U >::type >();
			print_maybe< typename cpp::remove_cv< U >::type >();
			print_maybe< typename cpp::remove_reference< U >::type >();
			print_maybe< typename cpp::remove_extent< U >::type >();
			print_maybe< typename cpp::remove_all_extents< U >::type >();
		}

		template< typename V >
		static void print_maybe()
		{
			if ( !cpp::is_same< U, V >::value )
			{
				tag< Predicate< V >::value > t;
				print_variation< V >(t);
			}
		}

	};

	static void test_all()
	{
		std::cout << "Void" << std::endl;

		apply_transform< void >::print();

		std::cout << "Integral types:" << std::endl;
		apply_transform< char >::print();
		apply_transform< signed char >::print();
		apply_transform< unsigned char >::print();
		apply_transform< short >::print();
		apply_transform< unsigned short >::print();
		apply_transform< int >::print();
		apply_transform< unsigned int >::print();
		apply_transform< long >::print();
		apply_transform< unsigned long >::print();
		apply_transform< bool >::print();

		std::cout << "Floating point types:" << std::endl;
		apply_transform< float >::print();
		apply_transform< double >::print();
		apply_transform< long double >::print();

		std::cout << "Enum types:" << std::endl;
		apply_transform< fruit >::print();

		std::cout << "Pointer types:" << std::endl;
		apply_transform< void* >::print();
		apply_transform< int* >::print();
		apply_transform< int const* const* >::print();
		apply_transform< Base* >::print();
		apply_transform< Derived* >::print();

		std::cout << "Reference types:" << std::endl;
		apply_transform< int& >::print();
		apply_transform< const int& >::print();

		std::cout << "Array types:" << std::endl;
		apply_transform< int[2] >::print();

		std::cout << "Function types:" << std::endl;
		apply_transform< void() >::print();
		apply_transform< int() >::print();
		apply_transform< int(int) >::print();

		std::cout << "Class types: " << std::endl;
		apply_transform< Base >::print();
		apply_transform< Derived >::print();
		apply_transform< C >::print();
		apply_transform< ConvertsToC >::print();
		apply_transform< Explicit >::print();
		apply_transform< Union >::print();

		std::cout << "Function pointer types:" << std::endl;
		apply_transform< int ( * )(int) >::print();
		std::cout << std::endl;
	}

};

/* Test each type trait with a variety of types
 */
int main()
{
	std::cout << std::boolalpha;

	std::cout << "== is_arithmetic =============================================" << std::endl;

	trait_test< cpp::is_arithmetic >::test_all();

	std::cout << "== is_array ==================================================" << std::endl;
	trait_test< cpp::is_array >::test_all();

	std::cout << "== is_compound ===============================================" << std::endl;
	trait_test< cpp::is_compound >::test_all();

	std::cout << "== is_const ==================================================" << std::endl;
	trait_test< cpp::is_const >::test_all();

	std::cout << "== is_enum ===================================================" << std::endl;
	trait_test< cpp::is_enum >::test_all();

	std::cout << "== is_floating_point =========================================" << std::endl;
	trait_test< cpp::is_floating_point >::test_all();

	std::cout << "== is_function ===============================================" << std::endl;
	trait_test< cpp::is_function >::test_all();

	std::cout << "== is_fundamental ============================================" << std::endl;
	trait_test< cpp::is_fundamental >::test_all();

	std::cout << "== is_integral ===============================================" << std::endl;
	trait_test< cpp::is_integral >::test_all();

	std::cout << "== is_member_pointer =========================================" << std::endl;
	trait_test< cpp::is_member_pointer >::test_all();

	std::cout << "== is_member_function_pointer ================================" << std::endl;
	trait_test< cpp::is_member_function_pointer >::test_all();

	std::cout << "== is_member_object_pointer ==================================" << std::endl;
	trait_test< cpp::is_member_object_pointer >::test_all();

	std::cout << "== is_null_pointer ===========================================" << std::endl;
	trait_test< cpp::is_null_pointer >::test_all();

	std::cout << "== is_object =================================================" << std::endl;
	trait_test< cpp::is_object >::test_all();

	std::cout << "== is_pointer ================================================" << std::endl;
	trait_test< cpp::is_pointer >::test_all();

	std::cout << "== is_reference ==============================================" << std::endl;
	trait_test< cpp::is_reference >::test_all();

	std::cout << "== is_scalar =================================================" << std::endl;
	trait_test< cpp::is_scalar >::test_all();

	std::cout << "== is_signed =================================================" << std::endl;
	trait_test< cpp::is_signed >::test_all();

	std::cout << "== is_unsigned ===============================================" << std::endl;
	trait_test< cpp::is_unsigned >::test_all();

	std::cout << "== is_void ===================================================" << std::endl;
	trait_test< cpp::is_void >::test_all();

	std::cout << "== is_volatile ===============================================" << std::endl;
	trait_test< cpp::is_volatile >::test_all();

	std::cout << "== is_base_of ================================================" << std::endl;
	relation_test< cpp::is_base_of >();

	std::cout << "== is_convertible ============================================" << std::endl;
	relation_test< cpp::is_convertible >();

	std::cout << "== add_const =================================================" << std::endl;
	test_transform< cpp::add_const >::test_all();

	std::cout << "== add_volatile ==============================================" << std::endl;
	test_transform< cpp::add_volatile >::test_all();

	std::cout << "== add_cv ====================================================" << std::endl;
	test_transform< cpp::add_cv >::test_all();

	std::cout << "== add_pointer ===============================================" << std::endl;
	test_transform< cpp::add_pointer >::test_all();

	std::cout << "== add_lvalue_reference ======================================" << std::endl;
	test_transform< cpp::add_lvalue_reference >::test_all();

	std::cout << "== decay =====================================================" << std::endl;
	test_transform< cpp::decay >::test_all();

	std::cout << "== make_signed ===============================================" << std::endl;
	test_transform< cpp::make_signed, is_signed_integral >::test_all();

	std::cout << "== make_unsigned =============================================" << std::endl;
	test_transform< cpp::make_unsigned, is_signed_integral >::test_all();

	std::cout << "== remove_cv =================================================" << std::endl;
	test_transform< cpp::remove_cv >::test_all();

	std::cout << "== remove_const ==============================================" << std::endl;
	test_transform< cpp::remove_const >::test_all();

	std::cout << "== remove_volatile ===========================================" << std::endl;
	test_transform< cpp::remove_volatile >::test_all();

	std::cout << "== remove_extent =============================================" << std::endl;
	test_transform< cpp::remove_extent >::test_all();

	std::cout << "== remove_all_extents ========================================" << std::endl;
	test_transform< cpp::remove_all_extents >::test_all();

	std::cout << "== remove_pointer ============================================" << std::endl;
	test_transform< cpp::remove_pointer >::test_all();

	std::cout << "== remove_reference ==========================================" << std::endl;
	test_transform< cpp::remove_reference >::test_all();
}

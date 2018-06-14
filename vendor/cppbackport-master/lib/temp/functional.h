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
#ifndef PBL_CPP_FUNCTIONAL_H
#define PBL_CPP_FUNCTIONAL_H

#include "version.h"

#include <functional>

#ifndef CPP11
#include <exception>

#include "cstdint.h"
#include "rvalueref.h"
#include "traits/enable_if.h"
#include "traits/remove_reference.h"
#include "traits/is_unsigned.h"
#include "traits/is_signed.h"
#include "traits/make_unsigned.h"
#include "memory.h"

namespace cpp11
{
namespace detail
{
template< typename R >
class callable
{
public:
	virtual ~callable()
	{
	}

	virtual callable* clone() const = 0;
	virtual R operator()() const    = 0;
};

template< typename F >
class binder;

template< typename R, typename A1 >
class binder< R(A1) >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R (* fn)(A1);

	template< typename B1 >
	binder(
		fn f_,
		B1 b1
	)
		: f(f_), a1(b1)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return f(a1);
	}

private:
	fn                                         f;
	typename cpp::remove_reference< A1 >::type a1;
};

template< typename R, typename A1, typename A2 >
class binder< R(A1, A2) >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R (* fn)(A1, A2);

	template< typename B1, typename B2 >
	binder(
		fn f_,
		B1 b1,
		B2 b2
	)
		: f(f_), a1(b1), a2(b2)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return f(a1, a2);
	}

private:
	fn                                         f;
	typename cpp::remove_reference< A1 >::type a1;
	typename cpp::remove_reference< A2 >::type a2;
};

template< typename R, typename A1, typename A2, typename A3 >
class binder< R(A1, A2, A3) >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R (* fn)(A1, A2, A3);

	template< typename B1, typename B2, typename B3 >
	binder(
		fn f_,
		B1 b1,
		B2 b2,
		B3 b3
	)
		: f(f_), a1(b1), a2(b2), a3(b3)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return f(a1, a2, a3);
	}

private:
	fn                                         f;
	typename cpp::remove_reference< A1 >::type a1;
	typename cpp::remove_reference< A2 >::type a2;
	typename cpp::remove_reference< A3 >::type a3;
};

template< typename R, typename A1, typename A2, typename A3, typename A4 >
class binder< R(A1, A2, A3, A4) >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R (* fn)(A1, A2, A3, A4);

	template< typename B1, typename B2, typename B3, typename B4 >
	binder(
		fn f_,
		B1 b1,
		B2 b2,
		B3 b3,
		B4 b4
	)
		: f(f_), a1(b1), a2(b2), a3(b3), a4(b4)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return f(a1, a2, a3, a4);
	}

private:
	fn                                         f;
	typename cpp::remove_reference< A1 >::type a1;
	typename cpp::remove_reference< A2 >::type a2;
	typename cpp::remove_reference< A3 >::type a3;
	typename cpp::remove_reference< A4 >::type a4;
};

template< typename R, typename A1, typename A2, typename A3, typename A4, typename A5 >
class binder< R(A1, A2, A3, A4, A5) >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R (* fn)(A1, A2, A3, A4, A5);

	template< typename B1, typename B2, typename B3, typename B4, typename B5 >
	binder(
		fn f_,
		B1 b1,
		B2 b2,
		B3 b3,
		B4 b4,
		B5 b5
	)
		: f(f_), a1(b1), a2(b2), a3(b3), a4(b4), a5(b5)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return f(a1, a2, a3, a4, a5);
	}

private:
	fn                                         f;
	typename cpp::remove_reference< A1 >::type a1;
	typename cpp::remove_reference< A2 >::type a2;
	typename cpp::remove_reference< A3 >::type a3;
	typename cpp::remove_reference< A4 >::type a4;
	typename cpp::remove_reference< A5 >::type a5;
};

template< typename R, typename T >
class binder< R ( T::* )() >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R (T::* fn)();

	binder(
		fn f_,
		T* p_
	)
		: f(f_), p(p_)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return ( p->*f )();
	}

private:
	fn f;
	T* p;
};

template< typename R, typename T >
class binder< R ( T::* )() const >
	: public callable< R >
{
public:
	typedef R result_type;
	typedef R ( T::* fn )() const;

	binder(
		fn f_,
		T* p_
	)
		: f(f_), p(p_)
	{
	}

	binder* clone() const
	{
		return new binder(*this);
	}

	R operator()() const
	{
		return ( p->*f )();
	}

private:
	fn f;
	T* p;
};

template< typename T >
struct to_function_type
{
	typedef T type;
};

template< typename T >
struct to_function_type< T* >
{
	typedef T type;
};

}

template< typename F, typename T1 >
detail::binder< typename detail::to_function_type< F >::type > bind(
	F  f,
	T1 t1
)
{
	return detail::binder< typename detail::to_function_type< F >::type >(f, t1);
}

template< typename F, typename T1, typename T2 >
detail::binder< typename detail::to_function_type< F >::type > bind(
	F  f,
	T1 t1,
	T2 t2
)
{
	return detail::binder< typename detail::to_function_type< F >::type >(f, t1, t2);
}

template< typename F, typename T1, typename T2, typename T3 >
detail::binder< typename detail::to_function_type< F >::type > bind(
	F  f,
	T1 t1,
	T2 t2,
	T3 t3
)
{
	return detail::binder< typename detail::to_function_type< F >::type >(f, t1, t2, t3);
}

template< typename F, typename T1, typename T2, typename T3, typename T4 >
detail::binder< typename detail::to_function_type< F >::type > bind(
	F  f,
	T1 t1,
	T2 t2,
	T3 t3,
	T4 t4
)
{
	return detail::binder< typename detail::to_function_type< F >::type >(f, t1, t2, t3, t4);
}

template< typename F, typename T1, typename T2, typename T3, typename T4, typename T5 >
detail::binder< typename detail::to_function_type< F >::type > bind(
	F  f,
	T1 t1,
	T2 t2,
	T3 t3,
	T4 t4,
	T5 t5
)
{
	return detail::binder< typename detail::to_function_type< F >::type >(f, t1, t2, t3, t4, t5);
}

class bad_function_call
	: public std::exception
{
public:
	bad_function_call()
		: exception()
	{
	}

};

template< typename F >
class function;

template< typename R >
class function< R() >
{
	typedef R (* function_pointer)();
public:
	function()
		: type(EMPTY)
	{
	}

	function(function_pointer f)
		: type(FUNCTION)
	{
		s.f = f;
	}

	function(const detail::callable< R >& c)
		: type(BINDER)
	{
		s.callable = c.clone();
	}

	function(rvalue_reference< function > r)
		: type(r.ref.type), s(r.ref.s)
	{
		r.ref.type = EMPTY;
	}

	function(const function& other)
		: type(other.type)
	{
		switch ( other.type )
		{
		case EMPTY:
			break;
		case FUNCTION:
			s.f = other.s.f;
			break;
		case BINDER:
			s.callable = other.s.callable->clone();
			break;
		}
	}

	~function()
	{
		if ( type == BINDER )
		{
			delete s.callable;
		}
	}

	function& operator=(const function& other)
	{
		function x(other);

		swap(x);

		return *this;
	}

	R operator()() const
	{
		switch ( type )
		{
		case EMPTY:
			throw bad_function_call();
		case FUNCTION:

			return s.f();

		case BINDER:

			return ( *s.callable )();
		}
	}

	void swap(function& other)
	{
		{
			callable_type t = other.type;
			other.type = type;
			type       = t;
		}

		{
			function_storage t = other.s;
			other.s = s;
			s       = t;
		}

	}

	typedef void ( function::* safe_bool )() const;

	operator safe_bool() const
	{
		return type != EMPTY ? &function::safe_bool_function : 0;
	}
private:
	void safe_bool_function() const
	{
	}

	union function_storage
	{
		function_pointer f;
		detail::callable< R >* callable;
	};

	enum callable_type {EMPTY, FUNCTION, BINDER};

	callable_type    type;
	function_storage s;
};

template< typename T >
class reference_wrapper
{
public:
	typedef T type;

	reference_wrapper(T& x)
		: p( addressof(x) )
	{
	}

	reference_wrapper(const reference_wrapper& o)
		: p(o.p)
	{
	}

	reference_wrapper& operator=(const reference_wrapper& o)
	{
		p = o.p;

		return *this;
	}

	operator T&() const { return *p; }
	T& get() const
	{
		return *p;
	}

private:

	T* p;
};

template< typename T >
reference_wrapper< T > ref(T& t)
{
	return reference_wrapper< T >(t);
}

template< typename T >
reference_wrapper< T > ref(reference_wrapper< T > t)
{
	return ref( t.get() );
}

template< typename T >
reference_wrapper< const T > cref(const T& t)
{
	return reference_wrapper< const T >(t);
}

template< typename T >
reference_wrapper< const T > cref(reference_wrapper< T > t)
{
	return cref( t.get() );
}

namespace details
{
template< std::size_t >
struct uinthash;

template< >
struct uinthash< 32 >
{
	std::size_t operator()(uint32_t x) const
	{
		x = ( x + 0x7ed55d16 ) + ( x << 12 );
		x = ( x ^ 0xc761c23c ) ^ ( x >> 19 );
		x = ( x + 0x165667b1 ) + ( x << 5 );
		x = ( x + 0xd3a2646c ) ^ ( x << 9 );
		x = ( x + 0xfd7046c5 ) + ( x << 3 );
		x = ( x ^ 0xb55a4f09 ) ^ ( x >> 16 );
		return x;
	}

};

template< >
struct uinthash< 64 >
{
	std::size_t operator()(uint64_t x) const
	{
		x = ( ~x ) + ( x << 21 );
		x = x ^ ( x >> 24 );
		x = ( x + ( x << 3 ) ) + ( x << 8 );
		x = x ^ ( x >> 14 );
		x = ( x + ( x << 2 ) ) + ( x << 4 );
		x = x ^ ( x >> 28 );
		x = x + ( x << 31 );
		return x;
	}

};
}

template< class T, class Enable = void >
struct hash;

// partial specialization for pointers
template< typename T >
struct hash< T*, void >
{
	std::size_t operator()(T* p) const
	{
		details::uinthash< sizeof( uintptr_t ) * CHAR_BIT > h;
		return h( reinterpret_cast< uintptr_t >( p ) );
	}

};

template< typename T >
struct hash< T, typename enable_if< is_unsigned< T >::value >::type >
{
	std::size_t operator()(T x) const
	{
		details::uinthash< sizeof( T ) * CHAR_BIT > h;
		return h(x);
	}

};

template< typename T >
struct hash< T, typename enable_if< is_signed< T >::value >::type >
{
	std::size_t operator()(T x) const
	{
		details::uinthash< sizeof( T ) * CHAR_BIT > h;
		return h( static_cast< typename make_unsigned< T >::type >( x ) );
	}

};

}
#endif // ifndef CPP11

#endif // PBL_CPP_FUNCTIONAL_H

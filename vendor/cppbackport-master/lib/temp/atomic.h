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
/** Just enough std::atomic to get by
 *
 * Only implementing integral types (for now).
 */
#ifndef PBL_CPP_ATOMIC_H
#define PBL_CPP_ATOMIC_H

#include "version.h"

#ifdef CPP11
#include <atomic>
#else
#include "config/arch.h"
#include "cstdint.h"
#include "traits/enable_if.h"
#include "traits/is_integral.h"

namespace cpp11
{
template< typename T, class Enable = void >
class atomic;

template< typename T >
class atomic< T, typename ::cpp::enable_if< ::cpp::is_integral< T >::value >::type >
{
public:
	atomic()
	{
	}

	atomic(T x)
		: value(x)
	{
	}

	T operator=(T x)
	{
		value = x;

		return x;
	}

	operator T() const
	{
		return value;
	}

	T operator++()
	{
		return __sync_add_and_fetch(&value, 1);
	}

	T operator++(int)
	{
		return __sync_fetch_and_add(&value, 1);
	}

	T operator--()
	{
		return __sync_sub_and_fetch(&value, 1);
	}

	T operator--(int)
	{
		return __sync_fetch_and_sub(&value, 1);
	}

	T operator+=(T arg)
	{
		return __sync_add_and_fetch(&value, arg);
	}

	T operator-=(T arg)
	{
		return __sync_sub_and_fetch(&value, arg);
	}

	T operator&=(T arg)
	{
		return __sync_and_and_fetch(&value, arg);
	}

	T operator|=(T arg)
	{
		return __sync_or_and_fetch(&value, arg);
	}

	T operator^=(T arg)
	{
		return __sync_xor_and_fetch(&value, arg);
	}

	T fetch_add(T arg)
	{
		return __sync_fetch_and_add(&value, arg);
	}

	T fetch_sub(T arg)
	{
		return __sync_fetch_and_sub(&value, arg);
	}

	T fetch_and(T arg)
	{
		return __sync_fetch_and_and(&value, arg);
	}

	T fetch_or(T arg)
	{
		return __sync_fetch_and_or(&value, arg);
	}

	T fetch_xor(T arg)
	{
		return __sync_fetch_and_xor(&value, arg);
	}

private:
	atomic(const atomic&);            // non-copyable
	atomic& operator=(const atomic&); // non-copyable

	T value;
};

typedef atomic< bool > atomic_bool;
typedef atomic< char > atomic_char;
typedef atomic< signed char > atomic_schar;
typedef atomic< unsigned char > atomic_uchar;
typedef atomic< short > atomic_short;
typedef atomic< unsigned short > atomic_ushort;
typedef atomic< int > atomic_int;
typedef atomic< unsigned int > atomic_uint;
typedef atomic< long > atomic_long;
typedef atomic< unsigned long > atomic_ulong;
#ifdef HAS_LONG_LONG
typedef atomic< long long > atomic_llong;
typedef atomic< unsigned long long > atomic_ullong;
#endif
typedef atomic< ::cpp::int_least8_t > atomic_int_least8_t;
typedef atomic< ::cpp::uint_least8_t > atomic_uint_least8_t;
typedef atomic< ::cpp::int_least16_t > atomic_int_least16_t;
typedef atomic< ::cpp::uint_least16_t > atomic_uint_least16_t;
typedef atomic< ::cpp::int_least32_t > atomic_int_least32_t;
typedef atomic< ::cpp::uint_least32_t > atomic_uint_least32_t;
typedef atomic< ::cpp::int_least64_t > atomic_int_least64_t;
typedef atomic< ::cpp::uint_least64_t > atomic_uint_least64_t;
typedef atomic< ::cpp::int_fast8_t > atomic_int_fast8_t;
typedef atomic< ::cpp::uint_fast8_t > atomic_uint_fast8_t;
typedef atomic< ::cpp::int_fast16_t > atomic_int_fast16_t;
typedef atomic< ::cpp::uint_fast16_t > atomic_uint_fast16_t;
typedef atomic< ::cpp::int_fast32_t > atomic_int_fast32_t;
typedef atomic< ::cpp::uint_fast32_t > atomic_uint_fast32_t;
typedef atomic< ::cpp::int_fast64_t > atomic_int_fast64_t;
typedef atomic< ::cpp::uint_fast64_t > atomic_uint_fast64_t;
typedef atomic< ::cpp::intptr_t > atomic_intptr_t;
typedef atomic< ::cpp::uintptr_t > atomic_uintptr_t;
typedef atomic< ::std::size_t > atomic_size_t;
typedef atomic< ::std::ptrdiff_t > atomic_ptrdiff_t;
typedef atomic< ::cpp::intmax_t > atomic_intmax_t;
typedef atomic< ::cpp::uintmax_t > atomic_uintmax_t;
}

#endif // ifndef CPP11

#endif // PBL_CPP_ATOMIC_H

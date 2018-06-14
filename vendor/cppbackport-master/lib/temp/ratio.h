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
#ifndef PBL_CPP_RATIO_H
#define PBL_CPP_RATIO_H

#include "version.h"

#ifdef CPP11
#include <ratio>
#else
#include "cstdint.h"
#include "traits/integral_constant.h"

namespace cpp11
{
namespace detail
{
template< cpp::intmax_t X >
struct sign
{
	static const cpp::intmax_t value = ( X >= 0 ? 1 : -1 );
};

template< cpp::intmax_t X >
struct abs
{
	static const cpp::intmax_t value = ( X >= 0 ? X : -X );
};

template< cpp::intmax_t P, cpp::intmax_t Q >
struct gcd
{
	static const cpp::intmax_t value = gcd< abs< Q >::value, abs< P >::value % abs< Q >::value >::value;
};

template< cpp::intmax_t P >
struct gcd< P, 0 >
{
	static const cpp::intmax_t value = abs< P >::value;
};

template< cpp::intmax_t Q >
struct gcd< 0, Q >
{
	static const cpp::intmax_t value = abs< Q >::value;
};

template< cpp::intmax_t P, cpp::intmax_t Q >
struct lcm
{
	static const cpp::intmax_t value = P * ( Q / gcd< P, Q >::value );
};

}

template< cpp::intmax_t Num, cpp::intmax_t Denom = 1 >
class ratio
{
public:

	static const intmax_t num = detail::sign< Num >::value * detail::sign< Denom >::value
	                            * detail::abs< Num >::value / detail::gcd< Num, Denom >::value;

	static const intmax_t den = detail::abs< Denom >::value / detail::gcd< Num, Denom >::value;

	typedef ratio< num, den > type;
};

typedef ratio< 1000000000L, 1 > giga;
typedef ratio< 1000000L, 1 > mega;
typedef ratio< 1000, 1 > kilo;
typedef ratio< 100, 1 > hecto;
typedef ratio< 10, 1 > deca;
typedef ratio< 1, 10 > deci;
typedef ratio< 1, 100 > centi;
typedef ratio< 1, 1000 > milli;
typedef ratio< 1, 1000000L > micro;
typedef ratio< 1, 1000000000L > nano;

template< class R1, class R2 >
struct ratio_equal
	: public integral_constant< bool, R1::num == R2::num&& R1::den == R2::den >
{
};

template< class R1, class R2 >
struct ratio_not_equal
	: public integral_constant< bool, R1::num != R2::num || R1::den != R2::den >
{
};

}
#endif // ifndef CPP11
#endif // PBL_CPP_RATIO_H

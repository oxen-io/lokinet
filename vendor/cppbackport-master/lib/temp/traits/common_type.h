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
#ifndef PBL_CPP_TRAITS_COMMON_TYPE_H
#define PBL_CPP_TRAITS_COMMON_TYPE_H

#ifndef CPP11
#include <climits>
#include "is_signed.h"
#include "conditional.h"
#include "make_unsigned.h"
#include "decay.h"

namespace cpp11
{
namespace detail
{
// in arithmetic, small types are promoted to int/unsigned int
template< typename T >
struct arithmetic_promotion
{
	typedef T type;
};

template< >
struct arithmetic_promotion< bool >
{
	typedef int type;
};

template< >
struct arithmetic_promotion< signed char >
{
	typedef int type;
};

template< >
struct arithmetic_promotion< signed short >
{
	typedef int type;
};

template< >
struct arithmetic_promotion< unsigned char >
{
	#if INT_MAX >= UCHAR_MAX
	typedef int type;
	#else
	typedef unsigned type;
	#endif
};

template< >
struct arithmetic_promotion< char >
{
	typedef arithmetic_promotion< underlying_char_type >::type type;
};

// The conversion rank of a type
template< typename T >
struct conversion_rank;

template< >
struct conversion_rank< bool >
	: integral_constant< unsigned, 1 >
{
};

template< >
struct conversion_rank< signed char >
	: integral_constant< unsigned, 2 >
{
};

template< >
struct conversion_rank< unsigned char >
	: integral_constant< unsigned, 2 >
{
};
template< >
struct conversion_rank< char >
	: integral_constant< unsigned, 2 >
{
};

template< >
struct conversion_rank< short >
	: integral_constant< unsigned, 3 >
{
};

template< >
struct conversion_rank< unsigned short >
	: integral_constant< unsigned, 3 >
{
};

template< >
struct conversion_rank< int >
	: integral_constant< unsigned, 4 >
{
};

template< >
struct conversion_rank< unsigned >
	: integral_constant< unsigned, 4 >
{
};

template< >
struct conversion_rank< long >
	: integral_constant< unsigned, 5 >
{
};

template< >
struct conversion_rank< unsigned long >
	: integral_constant< unsigned, 5 >
{
};

#ifdef HAS_LONG_LONG
template< >
struct conversion_rank< long long >
	: integral_constant< unsigned, 6 >
{
};
template< >
struct conversion_rank< unsigned long long >
	: integral_constant< unsigned, 6 >
{
};
#endif

template< typename T >
struct umax;

template< >
struct umax< short >
	: integral_constant< unsigned short, SHRT_MAX >
{};

template< >
struct umax< unsigned short >
	: integral_constant< unsigned short, USHRT_MAX >
{};

template< >
struct umax< int >
	: integral_constant< unsigned, INT_MAX >
{};

template< >
struct umax< unsigned >
	: integral_constant< unsigned, UINT_MAX >
{};

template< >
struct umax< long >
	: integral_constant< unsigned long, LONG_MAX >
{};

template< >
struct umax< unsigned long >
	: integral_constant< unsigned long, ULONG_MAX >
{};

#ifdef HAS_LONG_LONG
template< >
struct umax< long long >
	: integral_constant< unsigned long long, LLONG_MAX >
{};

template< >
struct umax< unsigned long long >
	: integral_constant< unsigned long long, ULLONG_MAX >
{};
#endif

template< typename U, typename S, bool = ( umax< U >::value <= umax< S >::value ) >
struct sign_conversion
{
	// both types are converted to unsigned version of S
	typedef typename make_unsigned< S >::type type;
};

template< typename U, typename S >
struct sign_conversion< U, S, true >
{
	// signed version is large enough to hold unsigned
	typedef S type;
};

// Chooses the "wider" type
template< typename T, typename U, bool = cpp11::is_signed< T >::value, bool = cpp11::is_signed< U >::value >
struct rank_conversion
{
	// return the wider of the two types for the fallback case when they have the same sign
	typedef typename conditional< ( conversion_rank< T >::value > conversion_rank< U >::value ), T, U >::type type;
};

template< typename S, typename U >
struct rank_conversion< S, U, true, false >
{
	typedef typename conditional< ( conversion_rank< U >::value > conversion_rank< S >::value ), U, typename sign_conversion< U, S >::type >::type type;
};

template< typename U, typename S >
struct rank_conversion< U, S, false, true >
{
	typedef typename conditional< ( conversion_rank< U >::value > conversion_rank< S >::value ), U, typename sign_conversion< U, S >::type >::type type;
};

// Handles float with integer conversions
template< typename T, typename U >
struct arithmetic_conversion
{
	typedef typename rank_conversion< T, U >::type type;
};

template< typename T >
struct arithmetic_conversion< long double, T >
{
	typedef long double type;
};

template< typename T >
struct arithmetic_conversion< T, long double >
{
	typedef long double type;
};

template< >
struct arithmetic_conversion< long double, long double >
{
	typedef long double type;
};

template< typename T >
struct arithmetic_conversion< double, T >
{
	typedef double type;
};

template< typename T >
struct arithmetic_conversion< T, double >
{
	typedef double type;
};

template< >
struct arithmetic_conversion< double, double >
{
	typedef double type;
};

template< >
struct arithmetic_conversion< double, long double >
{
	typedef long double type;
};

template< >
struct arithmetic_conversion< long double, double >
{
	typedef long double type;
};

template< typename T >
struct arithmetic_conversion< float, T >
{
	typedef float type;
};

template< typename T >
struct arithmetic_conversion< T, float >
{
	typedef float type;
};

template< >
struct arithmetic_conversion< float, float >
{
	typedef float type;
};

template< >
struct arithmetic_conversion< float, long double >
{
	typedef long double type;
};

template< >
struct arithmetic_conversion< long double, float >
{
	typedef long double type;
};

template< >
struct arithmetic_conversion< double, float >
{
	typedef double type;
};

template< >
struct arithmetic_conversion< float, double >
{
	typedef double type;
};

template< typename T, typename U >
struct common_type_helper
{
	typedef typename arithmetic_conversion< T, U >::type type;
};

template< typename T >
struct common_type_helper< T, T >
{
	typedef T type;
};
}

// The common type of T and U after usual arithmetic conversions are applied
/// @todo This is incomplete. Should follow ternary operator, rather than
/// standard integral promotion
template< typename T, typename U >
struct common_type
{
	typedef typename decay< typename detail::common_type_helper< typename detail::arithmetic_promotion< T >::type, typename detail::arithmetic_promotion< U >::type >::type >::type type;
};

template< typename T >
struct common_type< T, T >
{
	typedef typename decay< T >::type type;
};

template< typename T >
struct common_type< T, void >
{
	typedef typename decay< T >::type type;
};

template< typename T >
struct common_type< void, T >
{
	typedef typename decay< T >::type type;
};

template< >
struct common_type< void, void >
{
};
}
#else
#ifndef CPP14
namespace cpp14
{
template< class... T >
using common_type_t = typename std::common_type< T... >::type;
}
#endif
#endif // ifndef CPP11

#endif // PBL_CPP_TRAITS_COMMON_TYPE_H

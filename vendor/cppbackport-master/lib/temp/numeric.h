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
#ifndef PBL_CPP_NUMERIC_H
#define PBL_CPP_NUMERIC_H

#include <numeric>

#include "version.h"
#ifndef CPP11

namespace cpp11
{
template< class ForwardIterator, class T >
void iota(
	ForwardIterator first,
	ForwardIterator last,
	T               value
)
{
	while ( first != last )
	{
		*first++ = value;
		++value;
	}
}

}
#endif

#ifndef CPP17
#include "type_traits.h"
#include "cmath.h"

namespace cpp17
{
namespace detail
{
template< class T >
typename ::cpp::make_unsigned< T >::type uabs(T x)
{
	typedef typename ::cpp::make_unsigned< T >::type U;

	return x >= 0 ? static_cast< U >( x ) : -static_cast< U >( x );
}

template< class T, class U >
typename ::cpp::common_type< T, U >::type ugcd(
	T a_,
	U b_
)
{
	typedef typename ::cpp::common_type< T, U >::type V;

	V a = a_, b = b_;

	while ( b != 0 )
	{
		V t = b;
		b = a % b;
		a = t;
	}

	return a;
}

template< class T, class U >
typename ::cpp::common_type< T, U >::type ulcm(
	T a,
	U b
)
{
	typedef typename ::cpp::common_type< T, U >::type V;

	const V d = ugcd(a, b);

	return d == 0 ? 0 : a* ( b / d );
}

}

template< class T, class U >
typename ::cpp::common_type< T, U >::type gcd(
	T a,
	U b
)
{
	return detail::ugcd( detail::uabs(a), detail::uabs(b) );
}

template< class T, class U >
typename ::cpp::common_type< T, U >::type lcm(
	T a,
	U b
)
{
	return detail::ulcm( detail::uabs(a), detail::uabs(b) );
}

}
#endif // ifndef CPP17

#endif // PBL_CPP_NUMERIC_H

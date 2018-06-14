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
#ifndef PBL_CPP_TRAITS_IS_CONVERTIBLE_H
#define PBL_CPP_TRAITS_IS_CONVERTIBLE_H

#ifndef CPP11
#include "declval.h"
#include "integral_constant.h"
#include "yesno.h"

namespace cpp11
{
namespace detail
{
// If a type U is convertible to T, it will match against yes f(T)
template< typename T >
struct is_convertible_helper
{
	static no f(...);
	static yes f(T);
};

}

// Can From convert to To
template< typename From, typename To >
struct is_convertible
	: cpp17::bool_constant< ( sizeof( detail::is_convertible_helper< To >::f( declval< From >( ) ) ) == sizeof( detail::yes ) ) >
{
};

template< typename From >
struct is_convertible< From, void >
	: false_type
{
};

template< typename To >
struct is_convertible< void, To >
	: false_type
{
};

template< >
struct is_convertible< void, void >
	: false_type
{
};
}
#else
#ifndef CPP17
#ifdef CPP14
namespace cpp17
{
template< class From, class To >
constexpr bool is_convertible_v = std::is_convertible< From, To >::value;
}
#endif
#endif
#endif // ifndef CPP11

#endif // PBL_CPP_TRAITS_IS_CONVERTIBLE_H

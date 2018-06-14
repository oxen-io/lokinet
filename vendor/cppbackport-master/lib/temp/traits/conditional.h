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
#ifndef PBL_CPP_TRAITS_CONDITIONAL_H
#define PBL_CPP_TRAITS_CONDITIONAL_H

#include "integral_constant.h"

#ifndef CPP11
namespace cpp11
{

template< bool B, class T, class F >
struct conditional {typedef T type;};

template< class T, class F >
struct conditional< false, T, F >{typedef F type;};

}
#else
#ifndef CPP14
namespace cpp14
{
template< bool B, class T, class F >
using conditional_t = typename std::conditional< B, T, F >::type;
}
#endif
#endif

#ifndef CPP11
namespace cpp17
{
template< class B1 = void, class B2 = void >
struct conjunction
	: bool_constant< B1::value&& B2::value >
{
};

template< >
struct conjunction< void, void >
	: cpp11::true_type
{
};

template< class B1 >
struct conjunction< B1, void >
	: bool_constant< B1::value >
{
};

template< class B1 = void, class B2 = void >
struct disjunction
	: bool_constant< B1::value || B2::value >
{
};

template< >
struct disjunction< void, void >
	: cpp11::false_type
{
};

template< class B1 >
struct disjunction< B1, void >
	: bool_constant< B1::value >
{
};
}

#else
#ifndef CPP17
namespace cpp17
{
template< class... >
struct conjunction
	: std::true_type
{};

template< class B >
struct conjunction< B >
	: B
{};

template< class B1, class... Bi >
struct conjunction< B1, Bi... >
	: std::conditional< B1::value, conjunction< Bi... >, B1 >::type
{};

template< class... >
struct disjunction
	: std::false_type
{};

template< class B >
struct disjunction< B >
	: B
{};

template< class B1, class... Bi >
struct disjunction< B1, Bi... >
	: std::conditional< B1::value, B1, disjunction< Bi... > >::type
{};

#ifdef CPP14
template< class... Bi >
constexpr bool conjunction_v = conjunction< Bi... >::value;

template< class... Bi >
constexpr bool disjunction_v = disjunction< Bi... >::value;
#endif
}
#endif // ifndef CPP17
#endif // ifndef CPP11

#ifndef CPP17
namespace cpp17
{
template< class B >
struct negation
	: bool_constant< !bool(B::value) >
{
};

#ifdef CPP14
template< class B >
constexpr bool negation_v = negation< B >::value;
#endif
}
#endif


#endif // PBL_CPP_TRAITS_CONDITIONAL_H

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
#ifndef PBL_CPP_TRAITS_IS_MEMBER_POINTER_H
#define PBL_CPP_TRAITS_IS_MEMBER_POINTER_H

#ifndef CPP11
#include "integral_constant.h"
#include "remove_cv.h"
#include "is_function.h"

namespace cpp11
{
namespace detail
{
template< class T >
struct is_member_pointer_helper
	: false_type
{
};

template< class T, class U >
struct is_member_pointer_helper< T U::* >
	: true_type
{
};

template< class T >
struct is_member_function_pointer_helper
	: false_type {};

template< class T, class U >
struct is_member_function_pointer_helper< T U::* >
	: is_function< T >{};
}

template< class T >
struct is_member_pointer
	: detail::is_member_pointer_helper< typename remove_cv< T >::type >
{
};

template< class T >
struct is_member_function_pointer
	: detail::is_member_function_pointer_helper< typename remove_cv< T >::type >
{
};

template< class T >
struct is_member_object_pointer
	: cpp17::bool_constant< is_member_pointer< T >::value&& !is_member_function_pointer< T >::value >
{};
}
#else
#ifndef CPP17
#ifdef CPP14
namespace cpp17
{
template< class T >
constexpr bool is_member_pointer_v = std::is_member_pointer< T >::value;

template< class T >
constexpr bool is_member_function_pointer_v = std::is_member_function_pointer< T >::value;

template< class T >
constexpr bool is_member_object_pointer_v = std::is_member_object_pointer< T >::value;
}
#endif
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_IS_MEMBER_POINTER_H

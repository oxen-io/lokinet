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
#ifndef PBL_CPP_TRAITS_IS_BASE_OF_H
#define PBL_CPP_TRAITS_IS_BASE_OF_H

#ifndef CPP11
#include "integral_constant.h"
#include "is_class_or_union.h"
#include "yesno.h"

namespace cpp11
{
namespace detail
{
template< typename B, typename D >
struct host
{
	operator B*() const;
	operator D*();
};

template< typename B, typename D >
struct is_base_of_impl
{
	template< typename T >
	static yes check(D*, T);

	static no check(B*, int);
};

template< typename B, typename D, bool = detail::is_class_or_union< B >::value&& detail::is_class_or_union< D >::value >
struct is_base_of_helper
	: cpp17::bool_constant< ( sizeof is_base_of_impl< B, D >::check( host< B, D >( ), int() ) ) == sizeof( detail::yes ) >
{};

/* Need some specializations to handle references because the detail structs
   form a pointer to reference. Might be able to work around it if the pointer
   to reference occurs in a SFINAE context.
 */
template< typename B, typename D >
struct is_base_of_helper< B, D, false >
	: false_type
{};

}

template< typename B, typename D >
struct is_base_of
	: cpp17::bool_constant< detail::is_base_of_helper< B, D >::value >
{};

}
#else
#ifndef CPP17
#ifdef CPP14
namespace cpp17
{
template< class Base, class Derived >
constexpr bool is_base_of_v = std::is_base_of< Base, Derived >::value;
}
#endif
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_IS_BASE_OF_H

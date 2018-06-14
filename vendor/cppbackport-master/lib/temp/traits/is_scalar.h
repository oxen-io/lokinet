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
#ifndef PBL_CPP_TRAITS_IS_SCALAR_H
#define PBL_CPP_TRAITS_IS_SCALAR_H

#ifndef CPP11
#include "integral_constant.h"
#include "is_arithmetic.h"
#include "is_enum.h"
#include "is_pointer.h"
#include "is_member_pointer.h"
#include "is_null_pointer.h"

namespace cpp11
{
template< class T >
struct is_scalar
	: cpp::bool_constant<
		cpp::is_arithmetic< T >::value
		|| cpp::is_enum< T >::value
		|| cpp::is_pointer< T >::value
		|| cpp::is_member_pointer< T >::value
		|| cpp::is_null_pointer< T >::value >
{};
}
#else
#ifndef CPP17
#ifdef CPP14
namespace cpp17
{
template< class T >
constexpr bool is_scalar_v = std::is_scalar< T >::value;
}
#endif
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_IS_SCALAR_H

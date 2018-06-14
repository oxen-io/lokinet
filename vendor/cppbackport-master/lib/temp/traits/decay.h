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
#ifndef PBL_CPP_TRAITS_DECAY_H
#define PBL_CPP_TRAITS_DECAY_H

#ifndef CPP11
#include "remove_reference.h"
#include "conditional.h"
#include "is_array.h"
#include "remove_extent.h"
#include "add_pointer.h"
#include "remove_cv.h"
#include "is_function.h"

namespace cpp11
{
namespace detail
{
template< typename T, bool Function = is_function< T >::value >
struct decay_function
{
	typedef typename remove_cv< T >::type type;
};

template< typename T >
struct decay_function< T, true >
{
	typedef typename add_pointer< T >::type type;
};

template< typename T, bool Array = is_array< T >::value >
struct decay_helper
{
	typedef typename decay_function< T >::type type;
};

// decay array types
template< typename T >
struct decay_helper< T, true >
{
	typedef typename remove_extent< T >::type* type;
};

}

template< class T >
struct decay
{
	typedef typename detail::decay_helper< typename remove_reference< T >::type >::type type;
};

}
#else
#ifndef CPP14
namespace cpp14
{
template< class T >
using decay_t = typename std::decay< T >::type;
}
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_DECAY_H

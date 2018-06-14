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
#ifndef PBL_CPP_UTILITY_H
#define PBL_CPP_UTILITY_H

#include "version.h"

#include <utility>
#include <cstddef>

#ifndef CPP11
#include "rvalueref.h"

namespace cpp11
{
/// @todo If we cannot take T by reference, we should take by value
template< typename T >
rvalue_reference< T > move(T& value)
{
	return rvalue_reference< T >(value);
}

}

#endif // ifndef CPP11

#ifndef CPP14
namespace cpp14
{
template< std::size_t... >
struct index_sequence {};

namespace detail
{
template< std::size_t N, std::size_t... S >
struct gens
    : gens< N - 1, N - 1, S... >{};

template< std::size_t... S >
struct gens< 0, S... >
{
	typedef cpp14::index_sequence< S... > type;
};
}

template< std::size_t N >
using make_index_sequence = typename detail::gens< N >::type;

template< class... T >
using index_sequence_for = make_index_sequence< sizeof...(T) >;
}
#endif // ifndef CPP14

#endif // PBL_CPP_UTILITY_H

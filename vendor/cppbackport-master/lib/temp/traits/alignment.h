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
#ifndef PBL_CPP_TRAITS_ALIGNMENT_H
#define PBL_CPP_TRAITS_ALIGNMENT_H

#include <cstddef>

namespace cpp11
{
#if !( __cplusplus >= 201103L )
template< class T >
class alignment_of
{
	struct dummy {char a;T b;};
public:
	static const std::size_t value = offsetof(dummy, b);
};

template< std::size_t N, std::size_t A >
struct aligned_storage
{
	struct type
	{
		char data[N];
	} __attribute((aligned (A)));
};
#endif

#if !( __cplusplus >= 201103L ) || ( !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5 )
template< std::size_t Len, typename T >
struct aligned_union
{
	static const std::size_t alignment_value = cpp::alignment_of< T >::value;

	// This is actually over-aligned, but since we expect to completely remove
	// this in favour of C++11, we don't really care.
    #ifdef __GNUG__
	struct type
	{
		char data[Len > sizeof( T ) ? Len : sizeof( T )];
	} __attribute__(( aligned ));
    #elif defined(_MSC_VER)
    struct __declspec(align(16)) type
    {
        char data[Len > sizeof( T ) ? Len : sizeof( T )];
    };
    #endif
};
#endif
}

#endif // PBL_CPP_TRAITS_ALIGNMENT_H

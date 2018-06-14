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
/// @todo Function pointer should be implemented in terms of
/// result_of< remove_pointer<F>::type >
#ifndef PBL_CPP_TRAITS_RESULT_OF_H
#define PBL_CPP_TRAITS_RESULT_OF_H

#ifndef CPP11
#include "is_convertible.h"
#include "enable_if.h"

namespace cpp11
{
template< typename F, typename = void >
struct result_of;

template< typename R >
struct result_of< R( &( ) ) ( ) >
{
	typedef R type;
};

// Can I call F=R(F1) with argument A1
template< typename R, typename F1, typename A1 >
struct result_of< R( &( A1 ) ) ( F1 ), typename enable_if< is_convertible< A1, F1 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename A1, typename A2 >
struct result_of< R( &( A1, A2 ) ) ( F1, F2 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename F3, typename A1, typename A2, typename A3 >
struct result_of< R( &( A1, A2, A3 ) ) ( F1, F2, F3 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value&& is_convertible< A3, F3 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename F3, typename F4, typename A1, typename A2, typename A3, typename A4 >
struct result_of< R( &( A1, A2, A3, A4 ) ) ( F1, F2, F3, F4 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value&& is_convertible< A3, F3 >::value&& is_convertible< A4, F4 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename F3, typename F4, typename F5, typename A1, typename A2, typename A3, typename A4, typename A5 >
struct result_of< R( &( A1, A2, A3, A4, A5 ) ) ( F1, F2, F3, F4, F5 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value&& is_convertible< A3, F3 >::value&& is_convertible< A4, F4 >::value&& is_convertible< A5, F5 >::value >::type >
{
	typedef R type;
};

template< typename R >
struct result_of< R( *( ) ) ( ) >
{
	typedef R type;
};

// Can I call F=R(F1) with argument A1
template< typename R, typename F1, typename A1 >
struct result_of< R( *( A1 ) ) ( F1 ), typename enable_if< is_convertible< A1, F1 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename A1, typename A2 >
struct result_of< R( *( A1, A2 ) ) ( F1, F2 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename F3, typename A1, typename A2, typename A3 >
struct result_of< R( *( A1, A2, A3 ) ) ( F1, F2, F3 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value&& is_convertible< A3, F3 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename F3, typename F4, typename A1, typename A2, typename A3, typename A4 >
struct result_of< R( *( A1, A2, A3, A4 ) ) ( F1, F2, F3, F4 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value&& is_convertible< A3, F3 >::value&& is_convertible< A4, F4 >::value >::type >
{
	typedef R type;
};

template< typename R, typename F1, typename F2, typename F3, typename F4, typename F5, typename A1, typename A2, typename A3, typename A4, typename A5 >
struct result_of< R( *( A1, A2, A3, A4, A5 ) ) ( F1, F2, F3, F4, F5 ), typename enable_if< is_convertible< A1, F1 >::value&& is_convertible< A2, F2 >::value&& is_convertible< A3, F3 >::value&& is_convertible< A4, F4 >::value&& is_convertible< A5, F5 >::value >::type >
{
	typedef R type;
};

}
#else
#ifndef CPP14
namespace cpp14
{
template< class T >
using result_of_t = typename std::result_of< T >::type;
}
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_RESULT_OF_H

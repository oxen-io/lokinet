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
#ifndef PBL_CPP_TRAITS_MAKE_SIGNED_H
#define PBL_CPP_TRAITS_MAKE_SIGNED_H

#ifndef CPP11
#include "config/arch.h"

namespace cpp11
{
// std::make_signed
template< typename T >
struct make_signed
{
};

template< >
struct make_signed< char >
{
	typedef signed char type;
};

template< >
struct make_signed< signed char >
{
	typedef signed char type;
};

template< >
struct make_signed< unsigned char >
{
	typedef signed char type;
};

template< >
struct make_signed< signed short int >
{
	typedef signed short int type;
};

template< >
struct make_signed< unsigned short int >
{
	typedef signed short int type;
};

template< >
struct make_signed< signed int >
{
	typedef signed int type;
};

template< >
struct make_signed< unsigned int >
{
	typedef signed int type;
};

template< >
struct make_signed< signed long int >
{
	typedef signed long int type;
};

template< >
struct make_signed< unsigned long int >
{
	typedef signed long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_signed< signed long long int >
{
	typedef signed long long int type;
};

template< >
struct make_signed< unsigned long long int >
{
	typedef signed long long int type;
};
#endif

// const -----------------------------------------------------------------------
template< >
struct make_signed< const char >
{
	typedef const signed char type;
};

template< >
struct make_signed< const signed char >
{
	typedef const signed char type;
};

template< >
struct make_signed< const unsigned char >
{
	typedef const signed char type;
};

template< >
struct make_signed< const signed short int >
{
	typedef const signed short int type;
};

template< >
struct make_signed< const unsigned short int >
{
	typedef const signed short int type;
};

template< >
struct make_signed< const signed int >
{
	typedef const signed int type;
};

template< >
struct make_signed< const unsigned int >
{
	typedef const signed int type;
};

template< >
struct make_signed< const signed long int >
{
	typedef const signed long int type;
};

template< >
struct make_signed< const unsigned long int >
{
	typedef const signed long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_signed< const signed long long int >
{
	typedef const signed long long int type;
};

template< >
struct make_signed< const unsigned long long int >
{
	typedef const signed long long int type;
};
#endif

// volatile --------------------------------------------------------------------
template< >
struct make_signed< volatile char >
{
	typedef volatile signed char type;
};

template< >
struct make_signed< volatile signed char >
{
	typedef volatile signed char type;
};

template< >
struct make_signed< volatile unsigned char >
{
	typedef volatile signed char type;
};

template< >
struct make_signed< volatile signed short int >
{
	typedef volatile signed short int type;
};

template< >
struct make_signed< volatile unsigned short int >
{
	typedef volatile signed short int type;
};

template< >
struct make_signed< volatile signed int >
{
	typedef volatile signed int type;
};

template< >
struct make_signed< volatile unsigned int >
{
	typedef volatile signed int type;
};

template< >
struct make_signed< volatile signed long int >
{
	typedef volatile signed long int type;
};

template< >
struct make_signed< volatile unsigned long int >
{
	typedef volatile signed long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_signed< volatile signed long long int >
{
	typedef volatile signed long long int type;
};

template< >
struct make_signed< volatile unsigned long long int >
{
	typedef volatile signed long long int type;
};
#endif

// const volatile --------------------------------------------------------------
template< >
struct make_signed< const volatile char >
{
	typedef const volatile signed char type;
};

template< >
struct make_signed< const volatile signed char >
{
	typedef const volatile signed char type;
};

template< >
struct make_signed< const volatile unsigned char >
{
	typedef const volatile signed char type;
};

template< >
struct make_signed< const volatile signed short int >
{
	typedef const volatile signed short int type;
};

template< >
struct make_signed< const volatile unsigned short int >
{
	typedef const volatile signed short int type;
};

template< >
struct make_signed< const volatile signed int >
{
	typedef const volatile signed int type;
};

template< >
struct make_signed< const volatile unsigned int >
{
	typedef const volatile signed int type;
};

template< >
struct make_signed< const volatile signed long int >
{
	typedef const volatile signed long int type;
};

template< >
struct make_signed< const volatile unsigned long int >
{
	typedef const volatile signed long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_signed< const volatile signed long long int >
{
	typedef const volatile signed long long int type;
};

template< >
struct make_signed< const volatile unsigned long long int >
{
	typedef const volatile signed long long int type;
};
#endif
}

#else
#ifndef CPP14
namespace cpp14
{
template< class T >
using make_signed_t = typename std::make_signed< T >::type;
}
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_MAKE_SIGNED_H

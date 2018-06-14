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
#ifndef PBL_CPP_TRAITS_MAKE_UNSIGNED_H
#define PBL_CPP_TRAITS_MAKE_UNSIGNED_H

#ifndef CPP11
#include "../config/arch.h"

namespace cpp11
{
// std::make_unsigned
template< typename T >
struct make_unsigned
{
};

template< >
struct make_unsigned< char >
{
	typedef unsigned char type;
};

template< >
struct make_unsigned< signed char >
{
	typedef unsigned char type;
};

template< >
struct make_unsigned< unsigned char >
{
	typedef unsigned char type;
};

template< >
struct make_unsigned< signed short int >
{
	typedef unsigned short int type;
};

template< >
struct make_unsigned< unsigned short int >
{
	typedef unsigned short int type;
};

template< >
struct make_unsigned< signed int >
{
	typedef unsigned int type;
};

template< >
struct make_unsigned< unsigned int >
{
	typedef unsigned int type;
};

template< >
struct make_unsigned< signed long int >
{
	typedef unsigned long int type;
};

template< >
struct make_unsigned< unsigned long int >
{
	typedef unsigned long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_unsigned< signed long long int >
{
	typedef unsigned long long int type;
};

template< >
struct make_unsigned< unsigned long long int >
{
	typedef unsigned long long int type;
};
#endif

// const -----------------------------------------------------------------------
template< >
struct make_unsigned< const char >
{
	typedef const unsigned char type;
};

template< >
struct make_unsigned< const signed char >
{
	typedef const unsigned char type;
};

template< >
struct make_unsigned< const unsigned char >
{
	typedef const unsigned char type;
};

template< >
struct make_unsigned< const signed short int >
{
	typedef const unsigned short int type;
};

template< >
struct make_unsigned< const unsigned short int >
{
	typedef const unsigned short int type;
};

template< >
struct make_unsigned< const signed int >
{
	typedef const unsigned int type;
};

template< >
struct make_unsigned< const unsigned int >
{
	typedef const unsigned int type;
};

template< >
struct make_unsigned< const signed long int >
{
	typedef const unsigned long int type;
};

template< >
struct make_unsigned< const unsigned long int >
{
	typedef const unsigned long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_unsigned< const signed long long int >
{
	typedef const unsigned long long int type;
};

template< >
struct make_unsigned< const unsigned long long int >
{
	typedef const unsigned long long int type;
};
#endif
// volatile --------------------------------------------------------------------
template< >
struct make_unsigned< volatile char >
{
	typedef volatile unsigned char type;
};

template< >
struct make_unsigned< volatile signed char >
{
	typedef volatile unsigned char type;
};

template< >
struct make_unsigned< volatile unsigned char >
{
	typedef volatile unsigned char type;
};

template< >
struct make_unsigned< volatile signed short int >
{
	typedef volatile unsigned short int type;
};

template< >
struct make_unsigned< volatile unsigned short int >
{
	typedef volatile unsigned short int type;
};

template< >
struct make_unsigned< volatile signed int >
{
	typedef volatile unsigned int type;
};

template< >
struct make_unsigned< volatile unsigned int >
{
	typedef volatile unsigned int type;
};

template< >
struct make_unsigned< volatile signed long int >
{
	typedef volatile unsigned long int type;
};

template< >
struct make_unsigned< volatile unsigned long int >
{
	typedef volatile unsigned long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_unsigned< volatile signed long long int >
{
	typedef volatile unsigned long long int type;
};

template< >
struct make_unsigned< volatile unsigned long long int >
{
	typedef volatile unsigned long long int type;
};
#endif

// const volatile --------------------------------------------------------------
template< >
struct make_unsigned< const volatile char >
{
	typedef const volatile unsigned char type;
};

template< >
struct make_unsigned< const volatile signed char >
{
	typedef const volatile unsigned char type;
};

template< >
struct make_unsigned< const volatile unsigned char >
{
	typedef const volatile unsigned char type;
};

template< >
struct make_unsigned< const volatile signed short int >
{
	typedef const volatile unsigned short int type;
};

template< >
struct make_unsigned< const volatile unsigned short int >
{
	typedef const volatile unsigned short int type;
};

template< >
struct make_unsigned< const volatile signed int >
{
	typedef const volatile unsigned int type;
};

template< >
struct make_unsigned< const volatile unsigned int >
{
	typedef const volatile unsigned int type;
};

template< >
struct make_unsigned< const volatile signed long int >
{
	typedef const volatile unsigned long int type;
};

template< >
struct make_unsigned< const volatile unsigned long int >
{
	typedef const volatile unsigned long int type;
};

#ifdef HAS_LONG_LONG
template< >
struct make_unsigned< const volatile signed long long int >
{
	typedef const volatile unsigned long long int type;
};

template< >
struct make_unsigned< const volatile unsigned long long int >
{
	typedef const volatile unsigned long long int type;
};
#endif
}
#else
#ifndef CPP14
namespace cpp14
{
template< class T >
using make_unsigned_t = typename std::make_unsigned< T >::type;
}
#endif
#endif // ifndef CPP11
#endif // PBL_CPP_TRAITS_MAKE_UNSIGNED_H

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
/** @file cstdint.h
 * @brief Implementation of C++11 cstdint header
 */
#ifndef PBL_CPP_CSTDINT_H
#define PBL_CPP_CSTDINT_H

#include "version.h"

#ifdef CPP11
// Provided by C++11
#include <cstdint>
#else

#include <climits>

// Steal some types from the implementation, if we can
#if !defined( _WIN32 ) && ( defined( __unix__ ) || defined( __unix ) || ( defined( __APPLE__ ) && defined( __MACH__ )))
#include <unistd.h>
#if ( _POSIX_C_SOURCE >= 200112L )
// Provided by POSIX
#include <stdint.h>
namespace cpp11
{
using ::intptr_t;
using ::uintptr_t;
}
#endif // POSIX
#endif // UNIX-LIKE

namespace cpp11
{
#define AT_LEAST_16_BITS_S(x) ((x) >= 32767)
#define AT_LEAST_32_BITS_S(x) ((x) >= 2147483647)
#define AT_LEAST_64_BITS_S(x) ((((((x) >> 15) >> 15) >> 15) >> 15) >= 7)
#define AT_LEAST_16_BITS_U(x) ((x) >= 65535)
#define AT_LEAST_32_BITS_U(x) (((x) >> 1) >= 2147483647)
#define AT_LEAST_64_BITS_U(x) ((((((x) >> 15) >> 15) >> 15) >> 15) >= 15)

#define EXACTLY_8_BITS_S(x) ((x) == 127)
#define EXACTLY_16_BITS_S(x) ((x) == 32767)
#define EXACTLY_32_BITS_S(x) ((x) == 2147483647)
#define EXACTLY_64_BITS_S(x) ((((((x) >> 15) >> 15) >> 15) >> 15) == 7)
#define EXACTLY_8_BITS_U(x) ((x) == 255)
#define EXACTLY_16_BITS_U(x) ((x) == 65535)
#define EXACTLY_32_BITS_U(x) (((x) >> 1) == 2147483647)
#define EXACTLY_64_BITS_U(x) ((((((x) >> 15) >> 15) >> 15) >> 15) == 15)

// int_least8_t
typedef signed char int_least8_t;
#ifndef INT8_C
#define INT8_C(x) x
#endif
#ifndef INT_LEAST8_MIN
#define INT_LEAST8_MIN SCHAR_MIN
#endif
#ifndef INT_LEAST8_MAX
#define INT_LEAST8_MAX SCHAR_MAX
#endif

// int8_t
#if EXACTLY_8_BITS_S(INT_LEAST8_MAX)
#ifndef INT8_MIN
#define INT8_MIN INT_LEAST8_MIN
#endif
#ifndef INT8_MAX
#define INT8_MAX INT_LEAST8_MAX
#endif
typedef int_least8_t int8_t;
#endif

// int_least16_t
#if AT_LEAST_16_BITS_S(SCHAR_MAX)
typedef signed char int_least16_t;
#ifndef INT_LEAST16_MIN
#define INT_LEAST16_MIN SCHAR_MIN
#endif
#ifndef INT_LEAST16_MAX
#define INT_LEAST16_MAX SCHAR_MAX
#endif
#else
typedef short int int_least16_t;
#ifndef INT_LEAST16_MIN
#define INT_LEAST16_MIN SHRT_MIN
#endif
#ifndef INT_LEAST16_MAX
#define INT_LEAST16_MAX SHRT_MAX
#endif
#endif
#ifndef INT16_C
#define INT16_C(x) x
#endif

// int16_t
#if EXACTLY_16_BITS_S(INT_LEAST8_MAX)
#ifndef INT16_MIN
#define INT16_MIN INT_LEAST16_MIN
#endif
#ifndef INT16_MAX
#define INT16_MAX INT_LEAST16_MAX
#endif
typedef int_least16_t int16_t;
#endif

// int_least32_t
#if AT_LEAST_32_BITS_S(SCHAR_MAX)
typedef signed char int_least32_t
#ifndef INT_LEAST32_MIN
#define INT_LEAST32_MIN SCHAR_MIN
#endif
#ifndef INT_LEAST32_MAX
#define INT_LEAST32_MAX SCHAR_MAX
#endif
#ifndef INT32_C
#define INT32_C(x) x
#endif
#elif AT_LEAST_32_BITS_S(SHRT_MAX)
typedef short int int_least32_t
#ifndef INT_LEAST32_MIN
#define INT_LEAST32_MIN SHRT_MIN
#endif
#ifndef INT_LEAST32_MAX
#define INT_LEAST32_MAX SHRT_MAX
#endif
#ifndef INT32_C
#define INT32_C(x) x
#endif
#elif AT_LEAST_32_BITS_S(INT_MAX)
typedef int int_least32_t;
#ifndef INT_LEAST32_MIN
#define INT_LEAST32_MIN INT_MIN
#endif
#ifndef INT_LEAST32_MAX
#define INT_LEAST32_MAX INT_MAX
#endif
#ifndef INT32_C
#define INT32_C(x) x
#endif
#else
typedef long int_least32_t;
#ifndef INT_LEAST32_MIN
#define INT_LEAST32_MIN LONG_MIN
#endif
#ifndef INT_LEAST32_MAX
#define INT_LEAST32_MAX LONG_MAX
#endif
#ifndef INT32_C
#define INT32_C(x) x##l
#endif
#endif // if AT_LEAST_32_BITS_S(SCHAR_MAX)

// int32_t
#if EXACTLY_32_BITS_S(INT_LEAST32_MAX)
#ifndef INT32_MIN
#define INT32_MIN INT_LEAST32_MIN
#endif
#ifndef INT32_MAX
#define INT32_MAX INT_LEAST32_MAX
#endif
typedef int_least32_t int32_t;
#endif

// int_least64_t
#if AT_LEAST_64_BITS_S(SCHAR_MAX)
typedef signed char int_least64_t
#ifndef INT_LEAST64_MIN
#define INT_LEAST64_MIN SCHAR_MIN
#endif
#ifndef INT_LEAST64_MAX
#define INT_LEAST64_MAX SCHAR_MAX
#endif
#ifndef INT64_C
#define INT64_C(x) x
#endif
#elif AT_LEAST_64_BITS_S(SHRT_MAX)
typedef short int_least64_t
#ifndef INT_LEAST64_MIN
#define INT_LEAST64_MIN SHRT_MIN
#endif
#ifndef INT_LEAST64_MAX
#define INT_LEAST64_MAX SHRT_MAX
#endif
#ifndef INT64_C
#define INT64_C(x) x
#endif
#elif AT_LEAST_64_BITS_S(INT_MAX)
typedef int int_least64_t;
#ifndef INT_LEAST64_MIN
#define INT_LEAST64_MIN INT_MIN
#endif
#ifndef INT_LEAST64_MAX
#define INT_LEAST64_MAX INT_MAX
#endif
#ifndef INT64_C
#define INT64_C(x) x
#endif
#elif AT_LEAST_64_BITS_S(LONG_MAX)
typedef long int_least64_t;
#ifndef INT_LEAST64_MIN
#define INT_LEAST64_MIN LONG_MIN
#endif
#ifndef INT_LEAST64_MAX
#define INT_LEAST64_MAX LONG_MAX
#endif
#ifndef INT64_C
#define INT64_C(x) x##l
#endif
#else
#ifdef LLONG_MAX
typedef long long int_least64_t;
#ifndef INT_LEAST64_MIN
#define INT_LEAST64_MIN LLONG_MIN
#endif
#ifndef INT_LEAST64_MAX
#define INT_LEAST64_MAX LLONG_MAX
#endif
#ifndef INT64_C
#define INT64_C(x) x##ll
#endif
#endif
#endif // if AT_LEAST_64_BITS_S(SCHAR_MAX)

// int64_t
#if EXACTLY_64_BITS_S(INT_LEAST64_MAX)
#ifndef INT64_MIN
#define INT64_MIN INT_LEAST64_MIN
#endif
#ifndef INT64_MAX
#define INT64_MAX INT_LEAST64_MAX
#endif
typedef int_least64_t int64_t;
#endif

// uint_least8_t
typedef unsigned char uint_least8_t;
#ifndef UINT8_C
#define UINT8_C(x) x
#endif
#ifndef UINT_LEAST8_MAX
#define UINT_LEAST8_MAX UCHAR_MAX
#endif

// uint8_t
#if EXACTLY_8_BITS_U(UINT_LEAST8_MAX)
#ifndef UINT8_MAX
#define UINT8_MAX UINT_LEAST8_MAX
#endif
typedef uint_least8_t uint8_t;
#endif

// uint_least16_t
#if AT_LEAST_16_BITS_U(UCHAR_MAX)
typedef unsigned char uint_least16_t;
#ifndef UINT_LEAST16_MAX
#define UINT_LEAST16_MAX UCHAR_MAX
#endif
#else
typedef unsigned short uint_least16_t;
#ifndef UINT_LEAST16_MAX
#define UINT_LEAST16_MAX USHRT_MAX
#endif
#endif
#ifndef UINT16_C
#define UINT16_C(x) x
#endif

// uint16_t
#if EXACTLY_16_BITS_U(UINT_LEAST16_MAX)
#ifndef UINT16_MAX
#define UINT16_MAX UINT_LEAST16_MAX
#endif
typedef uint_least16_t uint16_t;
#endif

// uint_least32_t
#if AT_LEAST_32_BITS_U(UCHAR_MAX)
typedef unsigned char uint_least32_t
#ifndef UINT_LEAST32_MAX
#define UINT_LEAST32_MAX UCHAR_MAX
#endif
#ifndef UINT32_C
#define UINT32_C(x) x
#endif
#elif AT_LEAST_32_BITS_U(USHRT_MAX)
typedef unsigned short uint_least32_t
#ifndef UINT_LEAST32_MAX
#define UINT_LEAST32_MAX USHRT_MAX
#endif
#ifndef UINT32_C
#define UINT32_C(x) x
#endif
#elif AT_LEAST_32_BITS_U(UINT_MAX)
typedef unsigned uint_least32_t;
#ifndef UINT_LEAST32_MAX
#define UINT_LEAST32_MAX UINT_MAX
#endif
#ifndef UINT32_C
#define UINT32_C(x) x
#endif
#else
typedef unsigned long uint_least32_t;
#ifndef UINT_LEAST32_MAX
#define UINT_LEAST32_MAX ULONG_MAX
#endif
#ifndef UINT32_C
#define UINT32_C(x) x##ul
#endif
#endif // if AT_LEAST_32_BITS_U(UCHAR_MAX)

// uint32_t
#if EXACTLY_32_BITS_U(UINT_LEAST32_MAX)
#ifndef UINT32_MAX
#define UINT32_MAX UINT_LEAST32_MAX
#endif
typedef uint_least32_t uint32_t;
#endif

// uint_least64_t
#if AT_LEAST_64_BITS_U(UCHAR_MAX)
typedef unsigned char uint_least64_t
#ifndef UINT64_C
#define UINT64_C(x) x
#endif
#ifndef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX UCHAR_MAX
#endif
#elif AT_LEAST_64_BITS_U(USHRT_MAX)
typedef unsigned short uint_least64_t
#ifndef UINT64_C
#define UINT64_C(x) x
#endif
#ifndef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX USHRT_MAX
#endif
#elif AT_LEAST_64_BITS_U(UINT_MAX)
typedef unsigned uint_least64_t;
#ifndef UINT64_C
#define UINT64_C(x) x
#endif
#ifndef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX UINT_MAX
#endif
#elif AT_LEAST_64_BITS_U(LONG_MAX)
typedef unsigned long uint_least64_t;
#ifndef UINT64_C
#define UINT64_C(x) x##ul
#endif
#ifndef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX ULONG_MAX
#endif
#else
#ifdef ULLONG_MAX
typedef unsigned long long uint_least64_t;
#ifndef UINT64_C
#define UINT64_C(x) x##ull
#endif
#ifndef UINT_LEAST64_MAX
#define UINT_LEAST64_MAX ULLONG_MAX
#endif
#endif
#endif // if AT_LEAST_64_BITS_U(UCHAR_MAX)

// uint64_t
#if EXACTLY_64_BITS_U(UINT_LEAST64_MAX)
#ifndef UINT64_MAX
#define UINT64_MAX UINT_LEAST64_MAX
#endif
typedef uint_least64_t uint64_t;
#endif

typedef signed char int_fast8_t;
typedef int int_fast16_t;
typedef long int_fast32_t;
typedef int_least64_t int_fast64_t;

typedef unsigned char uint_fast8_t;
typedef unsigned uint_fast16_t;
typedef unsigned long uint_fast32_t;
typedef uint_least64_t uint_fast64_t;

#ifdef LLONG_MAX
typedef long long intmax_t;
#ifndef INTMAX_C
#define INTMAX_C(x) x##ll
#endif
#ifndef INTMAX_MAX
#define INTMAX_MAX LLONG_MAX
#endif
#else
typedef long intmax_t;
#ifndef INTMAX_C
#define INTMAX_C(x) x##l
#endif
#ifndef INTMAX_MAX
#define INTMAX_MAX LONG_MAX
#endif
#endif

#ifdef ULLONG_MAX
typedef unsigned long long uintmax_t;
#ifndef UINTMAX_C
#define UINTMAX_C(x) x##ull
#endif
#ifndef UINTMAX_MAX
#define UINTMAX_MAX ULLONG_MAX
#endif
#else
typedef unsigned long uintmax_t;
#ifndef UINTMAX_C
#define UINTMAX_C(x) x##ul
#endif
#ifndef UINTMAX_MAX
#define UINTMAX_MAX ULONG_MAX
#endif
#endif

#undef EXACTLY_8_BITS_S
#undef EXACTLY_16_BITS_S
#undef EXACTLY_32_BITS_S
#undef EXACTLY_64_BITS_S
#undef EXACTLY_8_BITS_U
#undef EXACTLY_16_BITS_U
#undef EXACTLY_32_BITS_U
#undef EXACTLY_64_BITS_U

#undef AT_LEAST_16_BITS_S
#undef AT_LEAST_32_BITS_S
#undef AT_LEAST_64_BITS_S
#undef AT_LEAST_16_BITS_U
#undef AT_LEAST_32_BITS_U
#undef AT_LEAST_64_BITS_U
}

#endif // CPP11
#endif // PBL_CPP_CSTDINT_H


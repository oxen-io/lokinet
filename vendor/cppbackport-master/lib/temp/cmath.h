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
/** Implementation of C++11 cmath header
 *
 * Pretty much borrows everything from C99/POSIX.
 *
 * @todo Use the appropriate feature test macros for each function
 */
#ifndef PBL_CPP_CMATH_H
#define PBL_CPP_CMATH_H

#include <cmath>

#include "version.h"
#ifndef CPP11
#include "config/os.h"
#endif
#ifndef CPP17
#include "type_traits.h"

namespace cpp11
{
namespace detail
{
// Used to promote arithmetic arguments to double or long double
template< typename Arithmetic1, typename Arithmetic2 = void, typename Arithmetic3 = void, bool has_long_double = ( cpp::is_same< long double, Arithmetic1 >::value || cpp::is_same< long double, Arithmetic2 >::value || cpp::is_same< long double, Arithmetic3 >::value ) >
struct promoted
{
	typedef double type;
};

template< typename Arithmetic1, typename Arithmetic2, typename Arithmetic3 >
struct promoted< Arithmetic1, Arithmetic2, Arithmetic3, true >
{
	typedef long double type;
};

}
}
#endif // ifndef CPP17
#ifndef CPP11
namespace cpp11
{
#ifdef POSIX_ISSUE_6
inline float remainder(
	float x,
	float y
)
{
	return ::remainderf(x, y);
}

inline double remainder(
	double x,
	double y
)
{
	return ::remainder(x, y);
}

inline long double remainder(
	long double x,
	long double y
)
{
	return ::remainderl(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type remainder(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::remainder( static_cast< real >( x ), static_cast< real >( y ) );
}

inline float remquo(
	float x,
	float y,
	int*  quo
)
{
	return ::remquof(x, y, quo);
}

inline double remquo(
	double x,
	double y,
	int*   quo
)
{
	return ::remquo(x, y, quo);
}

inline long double remquo(
	long double x,
	long double y,
	int*        quo
)
{
	return ::remquol(x, y, quo);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type remquo(
	Arithmetic1 x,
	Arithmetic2 y,
	int*        quo
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::remquo(static_cast< real >( x ), static_cast< real >( y ), quo);
}

inline float fma(
	float x,
	float y,
	float z
)
{
	return ::fmaf(x, y, z);
}

inline double fma(
	double x,
	double y,
	double z
)
{
	return ::fma(x, y, z);
}

inline long double fma(
	long double x,
	long double y,
	long double z
)
{
	return ::fmal(x, y, z);
}

template< typename Arithmetic1, typename Arithmetic2, typename Arithmetic3 >
typename detail::promoted< Arithmetic1, Arithmetic2, Arithmetic3 >::type fma(
	Arithmetic1 x,
	Arithmetic2 y,
	Arithmetic3 z
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2, Arithmetic3 >::type real;

	return ::cpp11::fma( static_cast< real >( x ), static_cast< real >( y ), static_cast< real >( z ) );
}

inline float fmax(
	float x,
	float y
)
{
	return ::fmaxf(x, y);
}

inline double fmax(
	double x,
	double y
)
{
	return ::fmax(x, y);
}

inline long double fmax(
	long double x,
	long double y
)
{
	return ::fmaxl(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type fmax(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::fmax( static_cast< real >( x ), static_cast< real >( y ) );
}

inline float fmin(
	float x,
	float y
)
{
	return ::fminf(x, y);
}

inline double fmin(
	double x,
	double y
)
{
	return ::fmin(x, y);
}

inline long double fmin(
	long double x,
	long double y
)
{
	return ::fminl(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type fmin(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::fmin( static_cast< real >( x ), static_cast< real >( y ) );
}

inline float fdim(
	float x,
	float y
)
{
	return ::fdimf(x, y);
}

inline double fdim(
	double x,
	double y
)
{
	return ::fdim(x, y);
}

inline long double fdim(
	long double x,
	long double y
)
{
	return ::fdiml(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type fdim(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::fdim( static_cast< real >( x ), static_cast< real >( y ) );
}

inline float exp2(float n)
{
	return ::exp2f(n);
}

inline double exp2(double n)
{
	return ::exp2(n);
}

inline long double exp2(long double n)
{
	return ::exp2l(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type exp2(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::exp2( static_cast< real >( x ) );
}

inline float expm1(float n)
{
	return ::expm1f(n);
}

inline double expm1(double n)
{
	return ::expm1(n);
}

inline long double expm1(long double n)
{
	return ::expm1l(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type expm1(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::expm1( static_cast< real >( x ) );
}

inline float log2(float n)
{
	return ::log2f(n);
}

inline double log2(double n)
{
	return ::log2(n);
}

inline long double log2(long double n)
{
	return ::log2l(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type log2(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::log2( static_cast< real >( x ) );
}

inline float log1p(float n)
{
	return ::log1pf(n);
}

inline double log1p(double n)
{
	return ::log1p(n);
}

inline long double log1p(long double n)
{
	return ::log1pl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type log1p(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::log1p( static_cast< real >( x ) );
}

inline float cbrt(float n)
{
	return ::cbrtf(n);
}

inline double cbrt(double n)
{
	return ::cbrt(n);
}

inline long double cbrt(long double n)
{
	return ::cbrtl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type cbrt(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::cbrt( static_cast< real >( x ) );
}

inline float hypot(
	float x,
	float y
)
{
	return ::hypotf(x, y);
}

inline double hypot(
	double x,
	double y
)
{
	return ::hypot(x, y);
}

inline long double hypot(
	long double x,
	long double y
)
{
	return ::hypotl(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type hypot(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::hypot( static_cast< real >( x ), static_cast< real >( y ) );
}

inline float asinh(float n)
{
	return ::asinhf(n);
}

inline double asinh(double n)
{
	return ::asinh(n);
}

inline long double asinh(long double n)
{
	return ::asinhl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type asinh(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::asinh( static_cast< real >( x ) );
}

inline float acosh(float n)
{
	return ::acoshf(n);
}

inline double acosh(double n)
{
	return ::acosh(n);
}

inline long double acosh(long double n)
{
	return ::acoshl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type acosh(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::acosh( static_cast< real >( x ) );
}

inline float atanh(float n)
{
	return ::atanhf(n);
}

inline double atanh(double n)
{
	return ::atanh(n);
}

inline long double atanh(long double n)
{
	return ::atanhl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type atanh(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::atanh( static_cast< real >( x ) );
}

inline float erf(float n)
{
	return ::erff(n);
}

inline double erf(double n)
{
	return ::erf(n);
}

inline long double erf(long double n)
{
	return ::erfl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type erf(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::erf( static_cast< real >( x ) );
}

inline float erfc(float n)
{
	return ::erfcf(n);
}

inline double erfc(double n)
{
	return ::erfc(n);
}

inline long double erfc(long double n)
{
	return ::erfcl(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type erfc(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::erfc( static_cast< real >( x ) );
}

inline float tgamma(float n)
{
	return ::tgammaf(n);
}

inline double tgamma(double n)
{
	return ::tgamma(n);
}

inline long double tgamma(long double n)
{
	return ::tgammal(n);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type tgamma(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::tgamma( static_cast< real >( x ) );
}

// We can reuse a lot of the functions defined by POSIX
inline float round(float arg)
{
	return ::roundf(arg);
}

inline double round(double arg)
{
	return ::round(arg);
}

inline long double round(long double arg)
{
	return ::roundl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type round(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::round( static_cast< real >( x ) );
}

inline long lround(float arg)
{
	return ::lroundf(arg);
}

inline long lround(double arg)
{
	return ::lround(arg);
}

inline long lround(long double arg)
{
	return ::lroundl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type lround(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::lround( static_cast< real >( x ) );
}

inline float trunc(float arg)
{
	return ::truncf(arg);
}

inline double trunc(double arg)
{
	return ::trunc(arg);
}

inline long double trunc(long double arg)
{
	return ::truncl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type trunc(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::trunc( static_cast< real >( x ) );
}

inline float rint(float arg)
{
	return ::rintf(arg);
}

inline double rint(double arg)
{
	return ::rint(arg);
}

inline long double rint(long double arg)
{
	return ::rintl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type rint(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::rint( static_cast< real >( x ) );
}

inline long lrint(float arg)
{
	return ::lrintf(arg);
}

inline long lrint(double arg)
{
	return ::lrint(arg);
}

inline long lrint(long double arg)
{
	return ::lrintl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type lrint(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::lrint( static_cast< real >( x ) );
}

inline float scalbn(
	float x,
	int   exp
)
{
	return ::scalbnf(x, exp);
}

inline double scalbn(
	double x,
	int    exp
)
{
	return ::scalbn(x, exp);
}

inline long double scalbn(
	long double x,
	int         exp
)
{
	return ::scalbnl(x, exp);
}

inline float scalbln(
	float x,
	long  exp
)
{
	return ::scalblnf(x, exp);
}

inline double scalbln(
	double x,
	long   exp
)
{
	return ::scalbln(x, exp);
}

inline long double scalbln(
	long double x,
	long        exp
)
{
	return ::scalblnl(x, exp);
}

inline int ilogb(float arg)
{
	return ::ilogbf(arg);
}

inline int ilogb(double arg)
{
	return ::ilogb(arg);
}

inline int ilogb(long double arg)
{
	return ::ilogbl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type ilogb(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::ilogb( static_cast< real >( x ) );
}

inline float logb(float arg)
{
	return ::logbf(arg);
}

inline double logb(double arg)
{
	return ::logb(arg);
}

inline long double logb(long double arg)
{
	return ::logbl(arg);
}

template< typename Arithmetic1 >
typename detail::promoted< Arithmetic1 >::type logb(Arithmetic1 x)
{
	typedef typename detail::promoted< Arithmetic1 >::type real;

	return ::cpp11::logb( static_cast< real >( x ) );
}

inline float nextafter(
	float x,
	float y
)
{
	return ::nextafterf(x, y);
}

inline double nextafter(
	double x,
	double y
)
{
	return ::nextafter(x, y);
}

inline long double nextafter(
	long double x,
	long double y
)
{
	return ::nextafterl(x, y);
}

template< typename Arithmetic >
typename detail::promoted< Arithmetic >::type nextafter(
	Arithmetic x,
	Arithmetic y
)
{
	typedef typename detail::promoted< Arithmetic >::type real;

	return ::cpp11::nextafter( static_cast< real >( x ), static_cast< real >( y ) );
}

inline float nexttoward(
	float x,
	float y
)
{
	return ::nexttowardf(x, y);
}

inline double nexttoward(
	double x,
	double y
)
{
	return ::nexttoward(x, y);
}

inline long double nexttoward(
	long double x,
	long double y
)
{
	return ::nexttowardl(x, y);
}

inline float copysign(
	float x,
	float y
)
{
	return ::copysignf(x, y);
}

inline double copysign(
	double x,
	double y
)
{
	return ::copysign(x, y);
}

inline long double copysign(
	long double x,
	long double y
)
{
	return ::copysignl(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename detail::promoted< Arithmetic1, Arithmetic2 >::type copysign(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return ::cpp11::copysign( static_cast< real >( x ), static_cast< real >( y ) );
}

#endif // ifdef POSIX_ISSUE_6

#if ( defined( _BSD_SOURCE ) || defined( _SVID_SOURCE ) \
    || defined( _XOPEN_SOURCE ) || defined( _ISOC99_SOURCE ) || \
    ( defined( _POSIX_C_SOURCE ) || _POSIX_C_SOURCE >= 200112L ) )
inline float lgamma(float n)
{
	return ::lgammaf(n);
}

inline double lgamma(double n)
{
	return ::lgamma(n);
}

inline long double lgamma(long double n)
{
	return ::lgammal(n);
}

#else
#define PBL_CPP_CMATH_LGAMMA
namespace detail
{
long double lgamma_implementation(long double);
}

inline float lgamma(float z)
{
	return static_cast< float >( detail::lgamma_implementation(z) );
}

inline double lgamma(double z)
{
	return static_cast< double >( detail::lgamma_implementation(z) );
}

inline long double lgamma(long double z)
{
	return detail::lgamma_implementation(z);
}

#endif // if ( defined( _BSD_SOURCE ) || defined( _SVID_SOURCE ) || defined( _XOPEN_SOURCE ) || defined( _ISOC99_SOURCE ) || ( defined( _POSIX_C_SOURCE ) || _POSIX_C_SOURCE >= 200112L ) )

template< typename Arithmetic1 >
double lgamma(Arithmetic1 x)
{
	return ::cpp11::lgamma( static_cast< double >( x ) );
}

}
#endif // ifndef CPP11

#ifndef CPP17
#define PBL_CPP_CMATH_BETA
namespace cpp17
{
namespace detail
{
long double beta_implementation(long double, long double);
}

inline double beta(
	double x,
	double y
)
{
	return static_cast< double >( detail::beta_implementation(x, y) );
}

inline float betaf(
	float x,
	float y
)
{
	return static_cast< float >( detail::beta_implementation(x, y) );
}

inline long double betal(
	long double x,
	long double y
)
{
	return detail::beta_implementation(x, y);
}

template< typename Arithmetic1, typename Arithmetic2 >
typename ::cpp11::detail::promoted< Arithmetic1, Arithmetic2 >::type beta(
	Arithmetic1 x,
	Arithmetic2 y
)
{
	typedef typename ::cpp11::detail::promoted< Arithmetic1, Arithmetic2 >::type real;

	return static_cast< real >( detail::beta_implementation( static_cast< real >( x ), static_cast< real >( y ) ) );
}

}
#endif // ifndef CPP17
#endif // PBL_CPP_CMATH_H

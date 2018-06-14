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
#include "cmath.h"

#ifdef PBL_CPP_CMATH_LGAMMA
namespace cpp11
{
namespace detail
{
/* Fallback implementation for lgamma, if it isn't available from environment
 *
 * See "Numerical Recipes 3e", Press et al., p 257
 */
long double lgamma_implementation(long double z)
{
	static const long double cof[14] =
	{
		57.1562356658629235l,
		-59.5979603554754912l,
		14.1360979747417471l,
		-0.491913816097620199l,
		0.339946499848118887e-4l,
		0.465236289270485756e-4l,
		-0.983744753048795646e-4l,
		0.158088703224912494e-3l,
		-0.210264441724104883e-3l,
		0.217439618115212643e-3l,
		-0.164318106536763890e-3l,
		0.844182239838527433e-4l,
		-0.261908384015814087e-4l,
		0.368991826595316234e-5l
	};

	long double x   = z;
	long double y   = z;
	long double tmp = x + ( 671.0L / 128 );

	tmp = ( x + 0.5 ) * std::log(tmp) - tmp;
	long double ser = 0.999999999999997092L;

	for ( std::size_t j = 0; j < 14; ++j )
	{
		y   += 1;
		ser += cof[j] / y;
	}

	return tmp + std::log(2.5066282746310005L * ser / x);
}

}
}
#endif // ifdef PBL_CPP_CMATH_LGAMMA

#ifdef PBL_CPP_CMATH_BETA
namespace cpp17
{
namespace detail
{
long double beta_implementation(
	long double z,
	long double w
)
{
	return std::exp( cpp::lgamma(z) + cpp::lgamma(w) - cpp::lgamma(z + w) );
}

}
}
#endif

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
#include "chrono.h"

#ifndef CPP11

#include <ctime>
#include "cstdint.h"

namespace cpp11
{
namespace chrono
{
time_point< system_clock > system_clock::now()
{
	timespec ts;

	if ( ::clock_gettime(CLOCK_REALTIME, &ts) == 0 )
	{
		#if 0
		seconds     s(ts.tv_sec);
		nanoseconds ns(ts.tv_nsec);

		return time_point( duration(s + ms) );

		#else

		return time_point( duration( ts.tv_sec * 1000 + ( ts.tv_nsec / 1000000L ) ) );

		#endif
	}

	return time_point();
}

std::time_t system_clock::to_time_t(const time_point& t)
{
	return static_cast< std::time_t >( t.time_since_epoch().count() / 1000 );
}

time_point< system_clock > system_clock::from_time_t(std::time_t t)
{
	return time_point( duration(t * 1000) );
}

time_point< steady_clock > steady_clock::now()
{
	timespec ts;

	if ( ::clock_gettime(CLOCK_MONOTONIC, &ts) == 0 )
	{
		return time_point( duration(ts.tv_sec * INT32_C(1000000000) + ts.tv_nsec) );
	}

	return time_point();
}

}
}
#endif // ifndef CPP11

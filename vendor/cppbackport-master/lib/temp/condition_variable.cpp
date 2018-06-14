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
#include "condition_variable.h"

#ifndef CPP11

namespace cpp11
{
condition_variable::condition_variable()
{
	pthread_cond_init(&cond, 0);
}

condition_variable::~condition_variable()
{
	pthread_cond_destroy(&cond);
}

void condition_variable::notify_one()
{
	pthread_cond_signal(&cond);
}

void condition_variable::notify_all()
{
	pthread_cond_broadcast(&cond);
}

void condition_variable::wait(unique_lock< mutex >& lock)
{
	pthread_cond_wait( &cond, lock.mutex()->native_handle() );
}

condition_variable::native_handle_type condition_variable::native_handle()
{
	return &cond;
}

cv_status::cv_status condition_variable::wait_until(
	unique_lock< mutex >&                   lock,
	const chrono::system_clock::time_point& tp
)
{
	timespec ts;

	chrono::nanoseconds      d     = chrono::duration_cast< chrono::nanoseconds >( tp.time_since_epoch() );
	chrono::nanoseconds::rep r     = d.count();
	long                     scale = 1000000000L;

	ts.tv_sec  = r / scale;
	ts.tv_nsec = r % scale;

	if ( pthread_cond_timedwait(&cond, lock.mutex()->native_handle(), &ts) == 0 )
	{
		return cv_status::no_timeout;
	}

	return cv_status::timeout;
}

}

#endif // ifndef CPP11

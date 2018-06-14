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
#include "thread.h"

#ifndef CPP11
#include <stdexcept>
#include <algorithm>

#include <unistd.h>

#if ( !defined( POSIX_THREADS ) && !defined( OS_WINDOWS ) )
#error "Threads are not supported on this platform"
#endif

namespace
{
#ifdef POSIX_THREADS
static void* start(void* arg)
#elif defined( OS_WINDOWS )
static DWORD start(void* arg) // to match LPTHREAD_START_ROUTINE
#endif
{
	cpp11::details::runnable* func = static_cast< cpp11::details::runnable* >( arg );

	func->operator()();
	delete func;

	return 0;
}

}

namespace cpp11
{

thread::thread()
	: tid()
{
}

thread::~thread()
{
	if ( joinable() )
	{
		std::terminate();
	}
}

thread::id thread::get_id() const
{
	return tid;
}

void thread::swap(thread& t)
{
	std::swap(tid, t.tid);
}

void thread::run(details::runnable* c)
{
	#ifdef POSIX_THREADS
	native_handle_type tid_;
	int                res = pthread_create( &tid_, 0, start, static_cast< void* >( c ) );

	if ( res != 0 )
	{
		throw std::runtime_error("Could not create thread");
	}

	tid = id(tid_);

	#elif defined( OS_WINDOWS )
	native_handle_type res = CreateThread(NULL, 0, reinterpret_cast< LPTHREAD_START_ROUTINE >( start ), static_cast< void* >( c ), 0, 0);

	if ( res == NULL )
	{
		throw std::runtime_error("Could not create thread");
	}

	tid = id(res);
	#endif
}

bool thread::joinable() const
{
	return tid != id();
}

void thread::join()
{
	if ( joinable() )
	{
		#ifdef POSIX_THREADS
		void* arg;
		int   res = pthread_join(tid.tid, &arg);

		if ( res != 0 )
		{
			throw std::runtime_error("Thread could not be joined.");
		}

		#elif defined( OS_WINDOWS )
		DWORD res = WaitForSingleObject(tid, INFINITE);
		#endif
		tid = id();
	}
}

void thread::detach()
{
	if ( joinable() )
	{
		#ifdef POSIX_THREADS
		pthread_detach(tid.tid);
		#elif defined( OS_WINDOWS )
		/// @todo Anything to be done here? Doesn't seem so.
		#endif
		tid = id();
	}
}

unsigned thread::hardware_concurrancy()
{
	return 0;
}

// =============================================================================
thread::id::id()
	: valid(false), tid()
{
}

thread::id::id(const id& x)
	: valid(x.valid), tid(x.tid)
{
}

thread::id::id(thread::native_handle_type t)
	: valid(true), tid(t)
{
}

thread::id& thread::id::operator=(const id& x)
{
	valid = x.valid;
	tid   = x.tid;

	return *this;
}

bool thread::id::operator==(const id& y) const
{
	return ( valid && y.valid && tid == y.tid ) || ( !valid && !y.valid );
}

bool thread::id::operator!=(const id& y) const
{
	return ( valid && y.valid && tid != y.tid ) || ( valid != y.valid );
}

bool thread::id::operator<(const id& y) const
{
	return ( valid && y.valid && tid < y.tid ) || ( !valid && y.valid );
}

bool thread::id::operator<=(const id& y) const
{
	return ( valid && y.valid && tid <= y.tid ) || ( !valid );
}

bool thread::id::operator>(const id& y) const
{
	return ( valid && y.valid && tid > y.tid ) || ( valid && !y.valid );
}

bool thread::id::operator>=(const id& y) const
{
	return ( valid && y.valid && tid >= y.tid ) || ( !y.valid );
}

template< class charT, class traits >
std::basic_ostream< charT, traits >& operator<<(
	std::basic_ostream< charT, traits >& os,
	thread::id i
)
{
	if ( i.valid )
	{
		os << i.tid;
	}
	else
	{
		os << "(null)";
	}

	return os;
}

namespace details
{
void microsleep(const ::cpp11::chrono::microseconds& dt)
{
	::usleep( static_cast< useconds_t >( dt.count() ) );
}

}

} // end namespace cpp11
#endif // ifndef CPP11

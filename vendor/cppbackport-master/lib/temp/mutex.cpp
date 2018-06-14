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
#include "mutex.h"

#ifndef CPP11

#include <stdexcept>

namespace cpp11
{

#ifdef POSIX_THREADS

mutex::mutex()
{
	int r = pthread_mutex_init(&mut, 0);

	if ( r != 0 )
	{
		throw std::runtime_error("Could not construct POSIX mutex.");
	}
}

mutex::~mutex()
{
	pthread_mutex_destroy(&mut);
}

void mutex::lock()
{
	pthread_mutex_lock(&mut);
}

void mutex::unlock()
{
	pthread_mutex_unlock(&mut);
}

bool mutex::trylock()
{
	return pthread_mutex_trylock(&mut) == 0;
}

mutex::native_handle_type mutex::native_handle()
{
	return &mut;
}

#elif defined( OS_WINDOWS )

mutex::mutex()
{
	InitializeCriticalSection(&mut);
}

mutex::~mutex()
{
	DeleteCriticalSection(&mut);
}

void mutex::lock()
{
	EnterCriticalSection(&mut);
}

void mutex::unlock()
{
	LeaveCriticalSection(&mut);
}

bool mutex::trylock()
{
	return TryEnterCriticalSection(&mut);
}

mutex::native_handle_type mutex::native_handle()
{
	return &mut;
}

#endif // ifdef POSIX_THREADS
}
#endif // ifndef CPP11

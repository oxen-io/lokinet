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
#ifndef PBL_CPP_MUTEX_H
#define PBL_CPP_MUTEX_H

#include "version.h"

#ifdef CPP11
#include <mutex>
#else
#include "config/os.h"

namespace cpp11
{

struct defer_lock_t {};
struct try_to_lock_t {};
struct adopt_lock_t {};

extern const defer_lock_t  defer_lock;
extern const try_to_lock_t try_to_lock;
extern const adopt_lock_t  adopt_lock;

class mutex
{
public:
	typedef ::pbl::os::mutex_type* native_handle_type;

	mutex();
	~mutex();
	void lock();
	void unlock();
	bool trylock();
	native_handle_type native_handle();
private:
	mutex(const mutex&);            // non-copyable
	mutex& operator=(const mutex&); // non-copyable

	::pbl::os::mutex_type mut;
};

template< class Mutex >
class lock_guard
{
public:
	typedef Mutex mutex_type;

	explicit lock_guard(mutex_type& m)
		: pm(m)
	{
		m.lock();
	}

	lock_guard(
		mutex_type& m,
		adopt_lock_t
	)
		: pm(m)
	{
	}

	~lock_guard()
	{
		pm.unlock();
	}

private:
	mutex_type& pm;
	lock_guard(const lock_guard&);
	lock_guard& operator=(const lock_guard&);
};

template< class Mutex >
class unique_lock
{
public:
	typedef Mutex mutex_type;

	unique_lock()
		: pm(0), owns(false)
	{
	}

	explicit unique_lock(mutex_type& m)
		: pm(&m), owns(true)
	{
		m.lock();
	}

	unique_lock(
		mutex_type& m,
		defer_lock_t
	)
		: pm(&m), owns(false)
	{
	}

	unique_lock(
		mutex_type& m,
		try_to_lock_t
	)
		: pm(&m), owns( m.try_lock() )
	{
	}

	unique_lock(
		mutex_type& m,
		adopt_lock_t
	)
		: pm(&m), owns(true)
	{
	}

	~unique_lock()
	{
		if ( owns )
		{
			pm->unlock();
		}
	}

	void lock()
	{
		if ( pm )
		{
			pm->lock();
			owns = true;
		}
	}

	bool try_lock()
	{
		if ( pm )
		{
			owns = pm->try_lock();
		}

		return owns;
	}

	void unlock()
	{
		if ( pm )
		{
			pm->unlock();
		}

		owns = false;
	}

	void swap(unique_lock& m)
	{
		bool t1 = owns;

		owns   = m.owns;
		m.owns = t1;

		mutex_type* t2 = pm;
		pm   = m.pm;
		m.pm = t2;
	}

	mutex_type* release()
	{
		mutex_type* p = pm;

		pm   = 0;
		owns = false;

		return p;
	}

	bool owns_lock() const
	{
		return owns;
	}

	mutex_type* mutex() const
	{
		return pm;
	}

private:
	unique_lock(const unique_lock&);            // non-copyable
	unique_lock& operator=(const unique_lock&); // non-copyable
	mutex_type* pm;
	bool        owns;
};

}
#endif // ifdef CPP11
#endif // ifndef PBL_CPP_MUTEX_H

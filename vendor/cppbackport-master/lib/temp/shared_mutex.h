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
#ifndef PBL_CPP_SHARED_MUTEX_H
#define PBL_CPP_SHARED_MUTEX_H

#include "version.h"

#ifdef CPP17
#include <shared_mutex>
#else
#include "config/os.h"
#include "mutex.h"

namespace cpp17
{
/** @todo See N3569 re: upgrade_mutex. Also https://github.com/HowardHinnant/upgrade_mutex
 * has a public domain implementation
 */
class shared_mutex
{
public:
	typedef ::pbl::os::shared_mutex_type* native_handle_type;

	shared_mutex();
	~shared_mutex();

	void lock();
	bool try_lock();
	void unlock();

	void lock_shared();
	bool try_lock_shared();
	void unlock_shared();

	native_handle_type native_handle();
private:
	shared_mutex(const shared_mutex&);
	shared_mutex& operator=(const shared_mutex&);

	::pbl::os::shared_mutex_type mut;
};

template< class Mutex >
class shared_lock
{
public:
	typedef Mutex mutex_type;

	shared_lock()
		: pm(0), owns(false)
	{
	}

	explicit shared_lock(mutex_type& m)
		: pm(&m), owns(true)
	{
		m.lock_shared();
	}

	shared_lock(
		mutex_type& m,
		cpp::defer_lock_t
	)
		: pm(&m), owns(false)
	{
	}

	shared_lock(
		mutex_type& m,
		cpp::try_to_lock_t
	)
		: pm(&m), owns( m.try_lock_shared() )
	{
	}

	shared_lock(
		mutex_type& m,
		cpp::adopt_lock_t
	)
		: pm(&m), owns(true)
	{
	}

	~shared_lock()
	{
		if ( owns )
		{
			pm->unlock_shared();
		}
	}

	void lock()
	{
		if ( pm )
		{
			pm->lock_shared();
			owns = true;
		}
	}

	bool try_lock()
	{
		if ( pm )
		{
			owns = pm->try_lock_shared();
		}

		return owns;
	}

	void unlock()
	{
		if ( pm && owns )
		{
			pm->unlock_shared();
		}

		owns = false;
	}

	void swap(shared_lock& m)
	{
		bool t1 = owns;

		owns   = m.owns;
		m.owns = t1;

		mutex_type* t2 = pm;
		pm   = m.pm;
		m.pm = t2;
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
	shared_lock(const shared_lock&);            // non-copyable
	shared_lock& operator=(const shared_lock&); // non-copyable

	mutex_type* pm;
	bool        owns;
};

}
#endif // ifdef CPP17

#endif // PBL_CPP_SHARED_MUTEX_H

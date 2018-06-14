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
#ifndef PBL_CPP_THREAD_H
#define PBL_CPP_THREAD_H

#include "version.h"

#ifdef CPP11
#include <thread>
#else

#include <iosfwd>
#include "rvalueref.h"
#include "chrono.h"

#include "config/os.h"

namespace cpp11
{
namespace details
{
class runnable
{
public:
	virtual ~runnable()
	{
	}

	virtual void operator()() = 0;
};

}

/// @todo Use C++17 invoke on the backend
class thread
{
	template< typename F, typename Arg1 = void, typename Arg2 = void, typename Arg3 = void >
	class runnable_wrapper
		: public details::runnable
	{
public:
		runnable_wrapper(
			F           f_,
			const Arg1& a1_,
			const Arg2& a2_,
			const Arg3& a3_
		)
			: f(f_), a1(a1_), a2(a2_), a3(a3_)
		{
		}

		void operator()()
		{
			f(a1, a2, a3);
		}

private:
		F    f;
		Arg1 a1;
		Arg2 a2;
		Arg3 a3;
	};

	template< typename F, typename Arg1 >
	class runnable_wrapper< F, Arg1, void, void >
		: public details::runnable
	{
public:
		runnable_wrapper(
			F           f_,
			const Arg1& a1_
		)
			: f(f_), a1(a1_)
		{
		}

		void operator()()
		{
			f(a1);
		}

private:
		F    f;
		Arg1 a1;
	};

	template< typename R, typename C >
	class runnable_wrapper< R ( C::* )(), C*, void, void >
		: public details::runnable
	{
public:
		typedef R (C::* F)();

		runnable_wrapper(
			F  f_,
			C* a1_
		)
			: f(f_), a1(a1_)
		{
		}

		void operator()()
		{
			( a1->*f )();
		}

private:
		F  f;
		C* a1;
	};

	template< typename F >
	class runnable_wrapper< F, void, void, void >
		: public details::runnable
	{
public:
		explicit runnable_wrapper(F f_)
			: f(f_)
		{
		}

		runnable_wrapper(rvalue_reference< F > f_)
			: f(f_)
		{
		}

		void operator()()
		{
			f();
		}

private:
		F f;
	};
public:
	typedef ::pbl::os::thread_type native_handle_type;

	class id
	{
		friend class thread;

		bool               valid;
		native_handle_type tid;

		explicit id(native_handle_type);
public:
		id();
		id(const id&);

		id& operator=(const id&);

		bool operator==(const id&) const;
		bool operator!=(const id&) const;
		bool operator<(const id&) const;
		bool operator<=(const id&) const;
		bool operator>(const id&) const;
		bool operator>=(const id&) const;

		template< class charT, class traits >
		friend std::basic_ostream< charT, traits >& operator<<(std::basic_ostream< charT, traits >&, id);
	};

	thread();

	/// @note func must be copyable. thread::start<F> takes ownership of the copy
	/// @todo Move the platform specific code to thread.cpp
	template< class F >
	explicit thread(F func)
		: tid()
	{
		run( new runnable_wrapper< F >(func) );
	}

	/// @bug Should be copying decayed value
	template< class F, class Arg1 >
	thread(
		F           func,
		const Arg1& arg1
	)
		: tid()
	{
		run( new runnable_wrapper< F, Arg1 >(func, arg1) );
	}

	template< class F >
	explicit thread(rvalue_reference< F > func)
		: tid()
	{
		run( new runnable_wrapper< F >(func) );
	}

	template< class F, class Arg1, class Arg2, class Arg3 >
	thread(
		F           func,
		const Arg1& a1,
		const Arg2& a2,
		const Arg3& a3
	)
		: tid()
	{
		run( new runnable_wrapper< F, Arg1, Arg2, Arg3 >(func, a1, a2, a3) );
	}

	~thread();

	void swap(thread&);
	bool joinable() const;
	void join();
	void detach();

	id get_id() const;
	native_handle_type native_handle();

	static unsigned hardware_concurrancy();
private:
	thread(const thread&);            // non-copyable
	thread& operator=(const thread&); // non-copyable

	void run(details::runnable* c);

	id tid;
};


template< class charT, class traits >
std::basic_ostream< charT, traits >& operator<<(std::basic_ostream< charT, traits >&, thread::id);

namespace details
{
void microsleep(const ::cpp11::chrono::microseconds& dt);
}

namespace this_thread
{

template< class Rep, class Period >
void sleep_for(const ::cpp11::chrono::duration< Rep, Period >& dt)
{
	details::microsleep( ::cpp11::chrono::duration_cast< ::cpp11::chrono::microseconds >(dt) );
}

}
}
#endif // ifdef CPP11
#endif // ifndef PBL_CPP_THREAD_H

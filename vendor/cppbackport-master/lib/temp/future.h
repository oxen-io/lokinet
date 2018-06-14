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
#ifndef PBL_CPP_FUTURE_H
#define PBL_CPP_FUTURE_H

#include "version.h"

#ifdef CPP11
#include <future>
#else
#include <stdexcept>
#include "thread.h"
#include "mutex.h"
#include "condition_variable.h"
#include "system_error.h"
#include "utility.h"
#include "functional.h"

namespace cpp11
{
namespace future_errc
{
enum future_errc
{
	no_error                  = 0,
	broken_promise            = 1,
	future_already_retrieved  = 2,
	promise_already_satisfied = 3,
	no_state                  = 4
};
}

namespace future_status
{
enum future_status
{
	ready,
	timeout,
	deferred
};
}

namespace launch
{
enum launch_type
{
	async             = 1,
	deferred          = 2,
	async_or_deferred = 3
};
}

namespace details
{
template< typename T >
struct future_shared_state
{
	future_shared_state()
		: result(0), owners(0), error(future_errc::no_error)
	{
	}

	/** All access to this object should be done with mutual exclusion
	 */
	::cpp11::mutex lock;

	/** For waiting for the promise
	 */
	::cpp11::condition_variable cond;

	/** pointer to result. If 0, the promise is still going to provide
	 * the calculation
	 */
	T* result;

	unsigned owners;

	future_errc::future_errc error;
};

template< >
struct future_shared_state< void >
{
	future_shared_state()
		: result(false), owners(0), error(future_errc::no_error)
	{
	}

	::cpp11::mutex lock;
	::cpp11::condition_variable cond;
	bool result;
	unsigned owners;
	future_errc::future_errc error;
};
}

class future_error
	: public std::logic_error
{
public:
	future_error(error_code ec)
		: logic_error("future error"), code_(ec)
	{
	}

	const error_code& code() const
	{
		return code_;
	}

private:
	error_code code_;
};

template< class T >
class promise;

template< class T >
class future;

template< typename T >
void swap(future< T >& f, future< T >& g);

/// @bug future does not have a swap member
template< class T >
class future
{
	friend class promise< T >;

	typedef details::future_shared_state< T > shared_state;

	friend void::cpp11::swap< >(future< T >&, future< T >&);
public:
	future()
		: state(0)
	{
	}

	future(rvalue_reference< future > other)
		: state(0)
	{
		swap(other.ref);
	}

	~future()
	{
		if ( state )
		{
			unique_lock< mutex > lk(state->lock);

			if ( state->owners == 1 )
			{
				// ownership has transferred to future
				delete state->result;
				lk.unlock();
				delete state;
			}
			else
			{
				--( state->owners );
			}
		}
	}

	future& operator=(rvalue_reference< future > other)
	{
		swap(other.ref);

		return *this;
	}

	bool valid() const
	{
		return state != 0;
	}

	T get()
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		unique_lock< mutex > lk(state->lock);

		if ( state->error != future_errc::no_error )
		{
			throw future_error(state->error);
		}

		if ( !state->result )
		{
			state->cond.wait(lk);
		}

		// Take the result so we can return it
		T* y = state->result;

		// release the state
		shared_state* t = state;
		state = 0;

		if ( t->owners == 1 )
		{
			lk.unlock();
			delete t;
		}
		else
		{
			--( t->owners );
			lk.unlock();
		}

		T x(*y);
		delete y;

		return x;
	}

	void wait()
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		::cpp11::unique_lock< ::cpp11::mutex > l(state->lock);

		if ( !state->result )
		{
			state->cond.wait(l);
		}
	}

	bool is_ready() const
	{
		bool ready = false;

		if ( state )
		{
			unique_lock< mutex > lk(state->lock);
			ready = ( state->result != 0 );
		}

		return ready;
	}

	future(const future&);
private:
	future& operator=(const future&);

	future(shared_state* p)
		: state(p)
	{
		unique_lock< mutex > lk(state->lock);
		state->owners += 1;
	}

	void swap(future& f)
	{
		/// @todo Should we lock in case one of the objects is destroyed?
		shared_state* t = f.state;

		f.state = state;
		state   = t;
	}

	shared_state* state;
};

template< >
class future< void >
{
	friend class promise< void >;
	typedef details::future_shared_state< void > shared_state;
	friend void::cpp11::swap< >(future< void >&, future< void >&);
public:
	future()
		: state(0)
	{
	}

	future(rvalue_reference< future > other)
		: state(0)
	{
		swap(other.ref);
	}

	~future()
	{
		if ( state )
		{
			unique_lock< mutex > lk(state->lock);

			if ( state->owners == 1 )
			{
				// ownership has transferred to future
				lk.unlock();
				delete state;
			}
			else
			{
				--( state->owners );
			}
		}
	}

	future& operator=(rvalue_reference< future > other)
	{
		swap(other.ref);

		return *this;
	}

	bool valid() const
	{
		return state != 0;
	}

	void get()
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		unique_lock< mutex > lk(state->lock);

		if ( state->error != future_errc::no_error )
		{
			throw future_error(state->error);
		}

		if ( !state->result )
		{
			state->cond.wait(lk);
		}

		// release the state
		shared_state* t = state;
		state = 0;

		if ( t->owners == 1 )
		{
			lk.unlock();
			delete t;
		}
		else
		{
			--( t->owners );
			lk.unlock();
		}
	}

	void wait() const
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		::cpp11::unique_lock< ::cpp11::mutex > l(state->lock);

		if ( !state->result )
		{
			state->cond.wait(l);
		}
	}

	template< class Duration >
	future_status::future_status wait_for(const Duration& dt) const
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		::cpp11::unique_lock< ::cpp11::mutex > l(state->lock);

		if ( state->result )
		{
			return future_status::ready;
		}

		if ( state->cond.wait_for(l, dt) == cv_status::timeout )
		{
			return future_status::timeout;
		}

		return future_status::ready;
	}

	template< class Clock, class Duration >
	future_status::future_status wait_until(const chrono::time_point< Clock, Duration >& timeout_time) const
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		::cpp11::unique_lock< ::cpp11::mutex > l(state->lock);

		if ( state->result )
		{
			return future_status::ready;
		}

		if ( state->cond.wait_until(l, timeout_time) == cv_status::timeout )
		{
			return future_status::timeout;
		}

		return future_status::ready;
	}

	bool is_ready() const
	{
		bool ready = false;

		if ( state )
		{
			unique_lock< mutex > lk(state->lock);
			ready = state->result;
		}

		return ready;
	}

	future(const future&);
private:
	future& operator=(const future&);

	future(shared_state* p)
		: state(p)
	{
		unique_lock< mutex > pk(state->lock);
		state->owners += 1;
	}

	void swap(future& f)
	{
		/// @todo Should we lock in case one of the objects is destroyed?
		shared_state* t = f.state;

		f.state = state;
		state   = t;
	}

	shared_state* state;
};

template< typename T >
void swap(
	future< T >& f,
	future< T >& g
)
{
	f.swap(g);
}

template< class T >
class promise
{
	typedef details::future_shared_state< T > shared_state;
public:
	promise()
		: state( new shared_state() )
	{
		state->owners = 1;
	}

	promise(rvalue_reference< promise > other)
		: state(0)
	{
		swap(other.ref);
	}

	~promise()
	{
		if ( state )
		{
			unique_lock< mutex > lk(state->lock);

			if ( state->owners == 1 )
			{
				// last owner
				delete state->result;
				lk.unlock();
				delete state;
			}
			else
			{
				--( state->owners );

				if ( !state->result )
				{
					state->error = future_errc::broken_promise;
				}
			}
		}
	}

	promise& operator=(rvalue_reference< promise > other)
	{
		swap(other.ref);

		return *this;
	}

	void swap(promise& other)
	{
		shared_state* t = state;

		state       = other.state;
		other.state = t;
	}

	void set_value(const T& value)
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		unique_lock< mutex > lk(state->lock);

		if ( state->result || state->error != future_errc::no_error )
		{
			throw future_error(future_errc::promise_already_satisfied);
		}

		state->result = new T(value);
		lk.unlock();
		state->cond.notify_all();
	}

	future< T > get_future()
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		unique_lock< mutex > lk(state->lock);

		if ( state->owners != 1 )
		{
			throw future_error(future_errc::future_already_retrieved);
		}

		lk.unlock();

		return future< T >(state);
	}

	promise(const promise&);
private:
	promise& operator=(promise&);

	shared_state* state;
};

template< >
class promise< void >
{
	typedef details::future_shared_state< void > shared_state;
public:
	promise()
		: state( new shared_state() )
	{
		state->owners = 1;
	}

	promise(rvalue_reference< promise > other)
		: state(0)
	{
		swap(other.ref);
	}

	~promise()
	{
		if ( state )
		{
			unique_lock< mutex > lk(state->lock);

			if ( state->owners == 1 )
			{
				// last owner
				lk.unlock();
				delete state;
			}
			else
			{
				--( state->owners );

				if ( !state->result )
				{
					state->error = future_errc::broken_promise;
				}
			}
		}
	}

	promise& operator=(rvalue_reference< promise > other)
	{
		swap(other.ref);

		return *this;
	}

	void swap(promise& other)
	{
		shared_state* t = state;

		state       = other.state;
		other.state = t;
	}

	void set_value()
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		unique_lock< mutex > lk(state->lock);

		if ( state->result || state->error != future_errc::no_error )
		{
			throw future_error(future_errc::promise_already_satisfied);
		}

		state->result = true;
		lk.unlock();
		state->cond.notify_all();
	}

	future< void > get_future()
	{
		if ( !state )
		{
			throw future_error(future_errc::no_state);
		}

		unique_lock< mutex > lk(state->lock);

		if ( state->owners != 1 )
		{
			throw future_error(future_errc::future_already_retrieved);
		}

		lk.unlock();

		return future< void >(state);
	}

	promise(const promise&);
private:
	promise& operator=(promise&);

	shared_state* state;
};

template< typename T >
class packaged_task;

template< typename R >
class packaged_task< R() >
{
public:
	typedef R result_type;

	template< typename F >
	explicit packaged_task(F f_)
		: f(f_)
	{
	}

	packaged_task(rvalue_reference< packaged_task > w)
		: f( move(w.ref.f) ), p( move(w.ref.p) )
	{
	}

	void operator()()
	{
		p.set_value( f() );
	}

	future< result_type > get_future()
	{
		return p.get_future();
	}

private:
	packaged_task(const packaged_task& w);
	packaged_task& operator=(packaged_task&);

	function< R() >        f;
	promise< result_type > p;
};

// Only difference between this and T() is how operator() is executed
template< >
class packaged_task< void() >
{
public:
	typedef void result_type;

	template< typename F >
	explicit packaged_task(F f_)
		: f(f_)
	{
	}

	packaged_task(rvalue_reference< packaged_task > w)
		: f( move(w.ref.f) ), p( move(w.ref.p) )
	{
	}

	void operator()()
	{
		f();
		p.set_value();
	}

	future< result_type > get_future()
	{
		return p.get_future();
	}

private:
	packaged_task(const packaged_task& w);
	packaged_task& operator=(packaged_task&);

	function< void() >     f;
	promise< result_type > p;
};

namespace detail
{
template< typename B >
future< typename B::result_type > async_inner(
	launch::launch_type policy,
	const B&            bound_function
)
{
	if ( (policy& launch::async) == 0 )
	{
		throw std::runtime_error("Deferred execution is not supported yet");
	}

	typedef typename B::result_type R;

	packaged_task< R() > w(bound_function);
	future< R >          fu = w.get_future();

	thread t( move(w) );
	t.detach();

	return move(fu);
}

}

template< typename F, typename Arg1 >
future< typename result_of< F(const Arg1&) >::type > async(
	F           f,
	const Arg1& a1
)
{
	return detail::async_inner( launch::async_or_deferred, bind(f, a1) );
}

template< typename F, typename Arg1, typename Arg2 >
future< typename result_of< F(const Arg1&, const Arg2&) >::type > async(
	F           f,
	const Arg1& a1,
	const Arg2& a2
)
{
	return detail::async_inner( launch::async_or_deferred, bind(f, a1, a2) );
}

template< typename F, typename Arg1, typename Arg2, typename Arg3 >
future< typename result_of< F(const Arg1&, const Arg2&, const Arg3&) >::type > async(
	F           f,
	const Arg1& a1,
	const Arg2& a2,
	const Arg3& a3
)
{
	return detail::async_inner( launch::async_or_deferred, bind(f, a1, a2, a3) );
}

template< typename F, typename Arg1, typename Arg2, typename Arg3, typename Arg4 >
future< typename result_of< F(const Arg1&, const Arg2&, const Arg3&, const Arg4&) >::type > async(
	F           f,
	const Arg1& a1,
	const Arg2& a2,
	const Arg3& a3,
	const Arg4& a4
)
{
	return detail::async_inner( launch::async_or_deferred, bind(f, a1, a2, a3, a4) );
}

template< typename F, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5 >
future< typename result_of< F(const Arg1&, const Arg2&, const Arg3&, const Arg4&, const Arg5&) >::type > async(
	F           f,
	const Arg1& a1,
	const Arg2& a2,
	const Arg3& a3,
	const Arg4& a4,
	const Arg5& a5
)
{
	return detail::async_inner( launch::async_or_deferred, bind(f, a1, a2, a3, a4, a5) );
}

template< typename F, typename Arg1 >
future< typename result_of< F(const Arg1&) >::type > async(
	launch::launch_type policy,
	F                   f,
	const Arg1&         a1
)
{
	return detail::async_inner( policy, bind(f, a1) );
}

template< typename F, typename Arg1, typename Arg2 >
future< typename result_of< F(const Arg1&, const Arg2&) >::type > async(
	launch::launch_type policy,
	F                   f,
	const Arg1&         a1,
	const Arg2&         a2
)
{
	return detail::async_inner( policy, bind(f, a1, a2) );
}

template< typename F, typename Arg1, typename Arg2, typename Arg3 >
future< typename result_of< F(const Arg1&, const Arg2&, const Arg3&) >::type > async(
	launch::launch_type policy,
	F                   f,
	const Arg1&         a1,
	const Arg2&         a2,
	const Arg3&         a3
)
{
	return detail::async_inner( policy, bind(f, a1, a2, a3) );
}

template< typename F, typename Arg1, typename Arg2, typename Arg3, typename Arg4 >
future< typename result_of< F(const Arg1&, const Arg2&, const Arg3&, const Arg4&) >::type > async(
	launch::launch_type policy,
	F                   f,
	const Arg1&         a1,
	const Arg2&         a2,
	const Arg3&         a3,
	const Arg4&         a4
)
{
	return detail::async_inner( policy, bind(f, a1, a2, a3, a4) );
}

template< typename F, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5 >
future< typename result_of< F(const Arg1&, const Arg2&, const Arg3&, const Arg4&, const Arg5&) >::type > async(
	launch::launch_type policy,
	F                   f,
	const Arg1&         a1,
	const Arg2&         a2,
	const Arg3&         a3,
	const Arg4&         a4,
	const Arg5&         a5
)
{
	return detail::async_inner( policy, bind(f, a1, a2, a3, a4, a5) );
}

}
#endif // ifdef CPP11
#endif // PBL_CPP_FUTURE_H

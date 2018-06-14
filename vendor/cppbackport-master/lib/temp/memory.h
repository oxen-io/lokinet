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
#ifndef PBL_CPP_MEMORY_H
#define PBL_CPP_MEMORY_H

#include "version.h"

#include <memory>

#ifndef CPP11
#include "atomic.h"

namespace cpp11
{
namespace detail
{
struct deleter_base
{
	explicit deleter_base(void* p)
		: refcount(1), value(p)
	{
	}

	virtual ~deleter_base()
	{
	}

	deleter_base(const deleter_base&);            // non-copyable
	deleter_base& operator=(const deleter_base&); // non-copyable

	cpp::atomic_long refcount;
	void* value;
};

template< typename U, typename D = void >
class deleter
	: public deleter_base
{
public:
	deleter(
		U*       value_,
		const D& del_
	)
		: deleter_base(value_), orig(value_), del(del_)
	{
	}

	~deleter()
	{
		del(orig);
	}

private:
	U* orig;
	D  del;
};

template< typename U >
class deleter< U, void >
	: public deleter_base
{
public:
	explicit deleter(U* value_)
		: deleter_base(value_), orig(value_)
	{
	}

	~deleter()
	{
		delete orig;
	}

private:
	U* orig;
};

}

/// @todo There seems to be some overlap with threadsafe reference counting and
/// semaphores
template< typename T >
class shared_ptr
{
	void safe_bool_function() const
	{
	}

	typedef void ( shared_ptr::* safe_bool )() const;
public:
	shared_ptr()
		: share(0)
	{
	}

	shared_ptr(const shared_ptr& p)
		: share(p.share)
	{
		if ( share )
		{
			++share->refcount;
		}
	}

	// U must be convertible to T
	template< typename U >
	explicit shared_ptr(U* p)
		: share(p ? new detail::deleter< U >(p) : 0)
	{
	}

	template< typename U, typename Deleter >
	shared_ptr(
		U*      p,
		Deleter d
	)
		: share(p ? new detail::deleter< U, Deleter >(p, d) : 0)
	{
	}

	~shared_ptr()
	{
		if ( share )
		{
			if ( --share->refcount == 0 )
			{
				delete share;
			}
		}
	}

	shared_ptr& operator=(const shared_ptr& p)
	{
		shared_ptr q(p);

		swap(q);

		return *this;
	}

	void swap(shared_ptr& p)
	{
		detail::deleter_base* t = p.share;

		p.share = share;
		share   = t;
	}

	void reset()
	{
		shared_ptr().swap(*this);
	}

	template< typename U >
	void reset(U* p)
	{
		shared_ptr(p).swap(*this);
	}

	T* get() const
	{
		return share ? static_cast< T* >( share->value ) : 0;
	}

	T& operator*() const
	{
		return *( static_cast< T* >( share->value ) );
	}

	T* operator->() const
	{
		return static_cast< T* >( share->value );
	}

	long use_count()
	{
		return share ? static_cast< long >( share->refcount ) : 0l;
	}

	bool unique()
	{
		return use_count() == 1;
	}

	operator safe_bool() const
	{
		return share ? &shared_ptr::safe_bool_function : 0;
	}
private:
	detail::deleter_base* share;
};

template< typename T, typename Arg1 >
shared_ptr< T > make_shared(const Arg1& a)
{
	return shared_ptr< T >( new T(a) );
}

template< typename T, typename Arg1, typename Arg2 >
shared_ptr< T > make_shared(
	const Arg1& a1,
	const Arg2& a2
)
{
	return shared_ptr< T >( new T(a1, a2) );
}

template< typename T >
class default_delete
{
public:
	void operator()(T* ptr) const
	{
		delete ptr;
	}

};

template< typename T >
class default_delete< T[] >
{
public:
	void operator()(T* ptr) const
	{
		delete[] ptr;
	}

};

template< typename T, typename Deleter = default_delete< T > >
class unique_ptr
{
public:
	typedef void ( unique_ptr::* bool_type )() const;

	unique_ptr()
		: p(0), lock(0)
	{
	}

	unique_ptr(const unique_ptr& o)
		: p(0), lock(0)
	{
		take(o);
	}

	explicit unique_ptr(T* q)
		: p(0), lock(0)
	{
		if ( q )
		{
			lock = new lock_t(this);
			p    = q;
		}
	}

	~unique_ptr()
	{
		reset();
	}

	unique_ptr& operator=(const unique_ptr& o)
	{
		reset();
		take(o);

		return *this;
	}

	void reset()
	{
		if ( lock )
		{
			if ( lock->owner == this )
			{
				// Free the resource we've been managing
				deleter(p);

				// now nobody owns it
				lock->owner = 0;
			}

			// Free the share object
			if ( --lock->refcount == 0 )
			{
				delete lock;
			}

			lock = 0;
			p    = 0;
		}
	}

	operator bool_type() const
	{
		return owner() ? &unique_ptr::safe_bool() : 0;
	}

	T& operator*() const
	{
		return *p;
	}

	T* operator->() const
	{
		return p;
	}

	T* get() const
	{
		return owner() ? p : 0;
	}

	T* release()
	{
		T* q = get();

		p = 0;
		reset();

		return q;
	}

	void swap(unique_ptr& o)
	{
		if ( owner() )
		{
			if ( o.owner() )
			{
				// both owners
				T* t1 = p;
				p   = o.p;
				o.p = t1;

				lock_t* t2 = lock;
				lock   = o.lock;
				o.lock = t2;

				unique_ptr* t3 = lock->owner;
				lock->owner   = o.lock->owner;
				o.lock->owner = t3;
			}
			else
			{
				give(o);
			}
		}
		else
		{
			take(o);
		}
	}

private:
	struct lock_t
	{
		explicit lock_t(unique_ptr* o)
			: owner(o), refcount(1)
		{
		}

		unique_ptr* owner;
		unsigned long refcount;
	};


	void safe_bool() const
	{
	}

	bool owner() const
	{
		return lock && ( lock->owner == this );
	}

	// we own, and o does not
	void give(unique_ptr& o)
	{
		// transfer ownership to o
		o.p = p;
		lock_t* t = o.lock;
		o.lock      = lock;
		lock->owner = &o;

		// maintain refcount
		lock = t;
	}

	// prerequisite: owner() == false
	void take(const unique_ptr& o)
	{
		if ( o.owner() )
		{
			p           = o.p;
			lock        = o.lock;
			lock->owner = this;
			++lock->refcount;
		}
	}

	T*      p;
	lock_t* lock;

	/// @todo EBCO
	Deleter deleter;
};

/// @todo Combine common functionality with unique_ptr<T>
template< typename T, typename Deleter >
class unique_ptr< T[], Deleter >
{
public:
	typedef void ( unique_ptr::* bool_type )() const;

	unique_ptr()
		: p(0), lock(0)
	{
	}

	unique_ptr(const unique_ptr& o)
		: p(0), lock(0)
	{
		take(o);
	}

	explicit unique_ptr(T* q)
		: p(0), lock(0)
	{
		if ( q )
		{
			lock = new lock_t(this);
			p    = q;
		}
	}

	~unique_ptr()
	{
		reset();
	}

	unique_ptr& operator=(const unique_ptr& o)
	{
		reset();
		take(o);

		return *this;
	}

	void reset()
	{
		if ( lock )
		{
			if ( lock->owner == this )
			{
				// Free the resource we've been managing
				deleter(p);

				// now nobody owns it
				lock->owner = 0;
			}

			// Free the share object
			if ( --lock->refcount == 0 )
			{
				delete lock;
			}

			lock = 0;
			p    = 0;
		}
	}

	operator bool_type() const
	{
		return owner() ? &unique_ptr::safe_bool() : 0;
	}

	T& operator[](std::size_t i) const
	{
		return p[i];
	}

	T* get() const
	{
		return owner() ? p : 0;
	}

	T* release()
	{
		T* q = get();

		p = 0;
		reset();

		return q;
	}

	void swap(unique_ptr& o)
	{
		if ( owner() )
		{
			if ( o.owner() )
			{
				// both owners
				T* t1 = p;
				p   = o.p;
				o.p = t1;

				lock_t* t2 = lock;
				lock   = o.lock;
				o.lock = t2;

				unique_ptr* t3 = lock->owner;
				lock->owner   = o.lock->owner;
				o.lock->owner = t3;
			}
			else
			{
				give(o);
			}
		}
		else
		{
			take(o);
		}
	}

private:
	struct lock_t
	{
		explicit lock_t(unique_ptr* o)
			: owner(o), refcount(1)
		{
		}

		unique_ptr* owner;
		unsigned long refcount;
	};


	void safe_bool() const
	{
	}

	bool owner() const
	{
		return lock && ( lock->owner == this );
	}

	// we own, and o does not
	void give(unique_ptr& o)
	{
		// transfer ownership to o
		o.p = p;
		lock_t* t = o.lock;
		o.lock      = lock;
		lock->owner = &o;

		// maintain refcount
		lock = t;
	}

	// prerequisite: owner() == false
	void take(const unique_ptr& o)
	{
		if ( o.owner() )
		{
			p           = o.p;
			lock        = o.lock;
			lock->owner = this;
			++lock->refcount;
		}
	}

	T*      p;
	lock_t* lock;

	/// @todo EBCO
	Deleter deleter;
};

namespace detail
{
template< class T >
struct addr_impl_ref
{
	T& v_;

	inline addr_impl_ref(T& v)
		: v_(v)
	{
	}

	inline operator T&() const
	{
		return v_;
	}

private:
	addr_impl_ref& operator=(const addr_impl_ref&);
};

template< class T >
struct addressof_impl
{
	static inline T* f(
		T& v,
		long
	)
	{
		return reinterpret_cast< T* >(
			&const_cast< char& >( reinterpret_cast< const volatile char& >( v ) ) );
	}

	static inline T* f(
		T* v,
		int
	)
	{
		return v;
	}

};
}

template< class T >
T* addressof(T& v)
{
	return detail::addressof_impl< T >::f(detail::addr_impl_ref< T >(v), 0);
}

template< class InputIt, class Size, class ForwardIt >
ForwardIt uninitialized_copy_n(
	InputIt   first,
	Size      count,
	ForwardIt d_first
)
{
	typedef typename std::iterator_traits< ForwardIt >::value_type Value;

	ForwardIt current = d_first;
	try
	{
		for (; count > 0; ++first, ++current, --count )
		{
			::new( static_cast< void* >( addressof(*current) ) )Value(*first);
		}
	}
	catch ( ... )
	{
		for (; d_first != current; ++d_first )
		{
			d_first->~Value();
		}

		throw;
	}

	return current;
}

}
#endif // ifndef CPP11

#ifndef CPP14
namespace cpp14
{
#ifdef CPP11
template< typename T, typename... Ts >
std::unique_ptr< T > make_unique(Ts&& ... params)
{
	return std::unique_ptr< T >( new T(std::forward< Ts >(params) ...) );
}

#else
template< typename T, typename Arg1, typename Arg2 >
cpp::unique_ptr< T > make_unique(
	const Arg1& a1,
	const Arg2& a2
)
{
	return cpp::unique_ptr< T >( new T(a1, a2) );
}

#endif
}
#endif // ifndef CPP14


#endif // PBL_CPP_MEMORY_H

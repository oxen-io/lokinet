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
#ifndef PBL_CPP_VARIANT_H
#define PBL_CPP_VARIANT_H

#include "version.h"

#ifdef CPP17
#include <variant>
#else
#include <exception>
#include "type_traits.h"

namespace cpp17
{
const std::size_t variant_npos = -1;

class bad_variant_access
	: public std::exception
{

};

namespace detail
{
template< std::size_t X, std::size_t Y >
struct max
{
	static const std::size_t value = X > Y ? X : Y;
};

class ivariant_helper
{
public:
	virtual ~ivariant_helper()
	{
	}

	virtual void init(void*, const void*) const = 0;
	virtual void destroy(void*) const           = 0;
	virtual void copy(void*, const void*) const = 0;
	virtual void swapem(void*, void*) const     = 0;
};

template< typename T >
class variant_helper
	: public ivariant_helper
{
public:
	void copy(
		void*       p,
		const void* q
	) const
	{
		*( static_cast< T* >( p ) ) = *static_cast< const T* >( q );
	}

	void init(
		void*       p,
		const void* q
	) const
	{
		new(p) T( *static_cast< const T* >( q ) );
	}

	void destroy(void* p) const
	{
		static_cast< T* >( p )->~T();
	}

	void swapem(
		void* p,
		void* q
	) const
	{
		using std::swap;

		swap( *static_cast< T* >( p ), *static_cast< T* >( q ) );
	}

	static const variant_helper* instance()
	{
		static const variant_helper h;

		return &h;
	}

};

}
}

#ifdef CPP11
namespace cpp17
{
namespace detail
{
template< typename T, typename... Us >
struct max_size
{
	static const std::size_t value = max< sizeof( T ), max_size< Us... >::value >::value;
};

template< typename T >
struct max_size< T >
{
	static const std::size_t value = sizeof( T );
};

template< typename T, typename... Us >
struct max_align
{
	static const std::size_t value = max< alignof( T ), max_align< Us... >::value >::value;
};

template< typename T >
struct max_align< T >
{
	static const std::size_t value = alignof( T );
};


template< typename U, typename Ti, bool = cpp::is_convertible< U, Ti >::value >
struct convert_helper
{
};

template< typename U, typename Ti >
struct convert_helper< U, Ti, true >
{
	typedef Ti type;
};

template< typename U, typename... Ts >
struct convert_type
	: convert_helper< U, Ts >...
{
};

template< std::size_t I, typename T, typename... Us >
struct select_type
{
	typedef typename select_type< I - 1, Us... >::type type;
};

template< typename T, typename... Us >
struct select_type< 0, T, Us... >
{
	typedef T type;
};

template< std::size_t I, typename Ti, typename Tj, typename... Ts >
struct select_index_helper
{
	static const std::size_t value = select_index_helper< I + 1, Ti, Ts... >::value;
};

template< std::size_t I, typename Ti, typename... Ts >
struct select_index_helper< I, Ti, Ti, Ts... >
{
	static const std::size_t value = I;
};

template< typename Ti, typename... Ts >
struct select_index
{
	static const std::size_t value = select_index_helper< 0, Ti, Ts... >::value;
};

}

template< typename... Ts >
class variant;

template< std::size_t, class >
struct variant_alternative;

template< std::size_t I, class... Ts >
struct variant_alternative< I, variant< Ts... > >
{
	typedef typename detail::select_type< I, Ts... >::type type;
};

template< typename... Ts >
class variant
{
	template< typename T, typename... Us >
	friend T& get(variant< Us... >&);

	template< typename T, typename... Us >
	friend T && get(const variant< Us... >&&);

	template< typename T, typename... Us >
	friend const T& get(const variant< Us... >&);

	template< typename T, typename... Us >
	friend const T && get(const variant< Us... >&&);

	template< std::size_t I, typename... Us >
	friend typename variant_alternative< I, variant< Us... > >::type & get(variant< Us... >&);

	template< std::size_t I, typename... Us >
	friend typename variant_alternative< I, variant< Us... > >::type && get(variant< Us... >&&);

	template< std::size_t I, typename... Us >
	friend typename variant_alternative< I, variant< Us... > >::type const & get(const variant< Us... >&);

	template< std::size_t I, typename... Us >
	friend typename variant_alternative< I, variant< Us... > >::type const && get(const variant< Us... >&&);
public:
	variant()
		: index_(0)
	{
		typedef typename variant_alternative< 0, variant >::type T1;

		new( &storage )T1();
	}

	variant(const variant& o)
		: index_(o.index_)
	{
		if ( o.index_ != variant_npos )
		{
			o.helper()->init(&storage, &o.storage);
		}
	}

	template< typename U, typename Ti = typename detail::convert_type< U, Ts... >::type >
	variant(const U& value)
	{
		index_ = detail::select_index< Ti, Ts... >::value;
		new( &storage )Ti(value);
	}

	~variant()
	{
		if ( index_ != variant_npos )
		{
			helper()->destroy(&storage);
		}
	}

	variant& operator=(const variant& v)
	{
		if ( index_ != variant_npos || v.index_ != variant_npos )
		{
			if ( v.index_ == variant_npos )
			{
				helper()->destroy(&storage);
				index_ = variant_npos;
			}
			else if ( index_ == v.index_ )
			{
				helper()->copy(&storage, &v.storage);
			}
			else
			{
				helper()->destroy(&storage);
				index_ = variant_npos;
				v.helper()->init(&storage, &v.storage);
				index_ = v.index_;
			}
		}

		return *this;
	}

	template< typename U, typename Ti = typename detail::convert_type< U, Ts... >::type >
	variant& operator=(const U& value)
	{
		const std::size_t i = detail::select_index< Ti, Ts... >::value;

		if ( index_ == i )
		{
			void* p = &storage;
			*static_cast< Ti* >( p ) = value;
		}
		else
		{
			helper()->destroy(&storage);
			index_ = variant_npos;
			new( &storage )Ti(value);
			index_ = i;
		}

		return *this;
	}

	std::size_t index() const
	{
		return index_;
	}

	bool valueless_by_exception() const
	{
		return index_ == variant_npos;
	}

	void swap(variant& o)
	{
		if ( index_ != variant_npos || o.index_ != variant_npos )
		{
			if ( index_ == o.index_ )
			{
				helper()->swapem(&storage, &o.storage);
			}
			else
			{
				variant t(o);
				o = *this;
				this->operator=(t);
			}
		}
	}

private:
	typedef typename cpp::aligned_storage< detail::max_size< Ts... >::value, detail::max_align< Ts... >::value >::type storage_type;

	const detail::ivariant_helper* helper() const
	{
		static const detail::ivariant_helper* helpers[] =
		{
			detail::variant_helper< Ts >::instance() ...
		};

		return helpers[index_];
	}

	std::size_t  index_;
	storage_type storage;
};

template< typename T, typename... Us >
T& get(variant< Us... >& v)
{
	if ( v.index_ != detail::select_index< T, Us... >::value )
	{
		throw bad_variant_access();
	}

	return *static_cast< T* >( static_cast< void* >( &v.storage ) );
}

template< typename T, typename... Us >
T && get(const variant< Us... >&& v)
{
	if ( v.index_ != detail::select_index< T, Us... >::value )
	{
		throw bad_variant_access();
	}

	return std::move( *static_cast< T* >( static_cast< void* >( &v.storage ) ) );
}


template< typename T, typename... Us >
const T& get(const variant< Us... >& v)
{
	if ( v.index_ != detail::select_index< T, Us... >::value )
	{
		throw bad_variant_access();
	}

	return *static_cast< const T* >( static_cast< const void* >( &v.storage ) );
}

template< typename T, typename... Us >
const T && get(const variant< Us... >&& v)
{
	if ( v.index_ != detail::select_index< T, Us... >::value )
	{
		throw bad_variant_access();
	}

	return std::move( *static_cast< const T* >( static_cast< const void* >( &v.storage ) ) );
}

template< std::size_t I, typename... Us >
typename variant_alternative< I, variant< Us... > >::type & get(variant< Us... >&v)
{
	if ( v.index_ != I )
	{
		throw bad_variant_access();
	}

	return *static_cast< typename variant_alternative< I, variant< Us... > >::type* >( static_cast< void* >( &v.storage ) );
}


template< std::size_t I, typename... Us >
typename variant_alternative< I, variant< Us... > >::type && get(variant< Us... >&& v)
{
	if ( v.index_ != I )
	{
		throw bad_variant_access();
	}

	return std::move( *static_cast< typename variant_alternative< I, variant< Us... > >::type* >( static_cast< void* >( &v.storage ) ) );
}


template< std::size_t I, typename... Us >
typename variant_alternative< I, variant< Us... > >::type const & get(const variant< Us... >&v)
{
	if ( v.index_ != I )
	{
		throw bad_variant_access();
	}

	return *static_cast< typename variant_alternative< I, variant< Us... > >::type const* >( static_cast< const void* >( &v.storage ) );
}


template< std::size_t I, typename... Us >
typename variant_alternative< I, variant< Us... > >::type const && get(const variant< Us... >&& v)
{
	if ( v.index_ != I )
	{
		throw bad_variant_access();
	}

	return std::move( *static_cast< typename variant_alternative< I, variant< Us... > >::type const* >( static_cast< const void* >( &v.storage ) ) );
}


template< typename T, typename... Us >
bool holds_alternative(const variant< Us... >& v)
{
	return v.index() == detail::select_index< T, Us... >::value;
}

}
#else
namespace cpp17
{
namespace detail
{
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct max_size
{
	static const std::size_t value = max< sizeof( T1 ), max_size< T2, T3, T4, T5, T6, T7, T8, T9, T10, void >::value >::value;
};

template< typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct max_size< void, T2, T3, T4, T5, T6, T7, T8, T9, T10 >
{
	static const std::size_t value = 0;
};

template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct max_align
{
	static const std::size_t value = max< cpp::alignment_of< T1 >::value, max_align< T2, T3, T4, T5, T6, T7, T8, T9, T10, void >::value >::value;
};

template< typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct max_align< void, T2, T3, T4, T5, T6, T7, T8, T9, T10 >
{
	static const std::size_t value = 0;
};

template< >
class variant_helper< void >
	: public ivariant_helper
{
public:
	void copy(
		void*,
		const void*
	) const
	{
	}

	void init(
		void*,
		const void*
	) const
	{
	}

	void destroy(void* p) const
	{
	}

	void swapem(
		void*,
		void*
	) const
	{

	}

	static const variant_helper* instance()
	{
		return 0;
	}

};

template< typename U, typename T, std::size_t I, bool = cpp::is_convertible< U, T >::value >
struct convert_choice
{
	typedef T type;
	static const std::size_t value = I;
};

template< typename U, typename T, std::size_t I >
struct convert_choice< U, T, I, false >
{
};

template< typename U, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct convert_type
	: convert_choice< U, T1, 0 >,
	  convert_choice< U, T2, 1 >,
	  convert_choice< U, T3, 2 >,
	  convert_choice< U, T4, 3 >,
	  convert_choice< U, T5, 4 >,
	  convert_choice< U, T6, 5 >,
	  convert_choice< U, T7, 6 >,
	  convert_choice< U, T8, 7 >,
	  convert_choice< U, T9, 8 >,
	  convert_choice< U, T10, 9 >
{

};

template< typename U, typename T, std::size_t I >
struct select_choice
{
};

template< typename U, std::size_t I >
struct select_choice< U, U, I >
{
	typedef U type;
	static const std::size_t value = I;
};

template< typename U, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct select_type
	: select_choice< U, T1, 0 >,
	  select_choice< U, T2, 1 >,
	  select_choice< U, T3, 2 >,
	  select_choice< U, T4, 3 >,
	  select_choice< U, T5, 4 >,
	  select_choice< U, T6, 5 >,
	  select_choice< U, T7, 6 >,
	  select_choice< U, T8, 7 >,
	  select_choice< U, T9, 8 >,
	  select_choice< U, T10, 9 >
{

};
template< typename, typename T >
struct rhs
{
	typedef T type;
};
}

template< typename T1, typename T2 = void, typename T3 = void, typename T4 = void, typename T5 = void, typename T6 = void, typename T7 = void, typename T8 = void, typename T9 = void, typename T10 = void >
class variant;

template< std::size_t, typename >
struct variant_alternative;

template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 0, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T1 type;
};

template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 1, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T2 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 2, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T3 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 3, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T4 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 4, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T5 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 5, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T6 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 6, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T7 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 7, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T8 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 8, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T9 type;
};
template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
struct variant_alternative< 9, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >
{
	typedef T10 type;
};

template< typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
class variant
{
	template< typename T, typename U1, typename U2, typename U3, typename U4, typename U5, typename U6, typename U7, typename U8, typename U9, typename U10 >
	friend typename detail::select_type< T, U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 >::type & get(variant< U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 >&);

	template< typename T, typename U1, typename U2, typename U3, typename U4, typename U5, typename U6, typename U7, typename U8, typename U9, typename U10 >
	friend typename detail::select_type< T, U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 >::type const & get(const variant< U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 >&);

	template< std::size_t I, typename U1, typename U2, typename U3, typename U4, typename U5, typename U6, typename U7, typename U8, typename U9, typename U10 >
	friend typename variant_alternative< I, variant< U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 > >::type & get(variant< U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 >&);

	template< std::size_t I, typename U1, typename U2, typename U3, typename U4, typename U5, typename U6, typename U7, typename U8, typename U9, typename U10 >
	friend typename variant_alternative< I, variant< U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 > >::type const & get(const variant< U1, U2, U3, U4, U5, U6, U7, U8, U9, U10 >&);
public:
	variant()
		: index_(0)
	{
		new( &storage )T1();
	}

	variant(const variant& o)
		: index_(o.index_)
	{
		if ( o.index_ != variant_npos )
		{
			o.helper()->init(&storage, &o.storage);
		}
	}

	template< typename U >
	variant(
		const U& value,
		typename detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::type* = 0
	)
	{
		typedef typename detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::type T;

		index_ = detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value;
		new( &storage )T(value);
	}

	~variant()
	{
		if ( index_ != variant_npos )
		{
			helper()->destroy(&storage);
		}
	}

	variant& operator=(const variant& v)
	{
		if ( index_ != variant_npos || v.index_ != variant_npos )
		{
			if ( v.index_ == variant_npos )
			{
				helper()->destroy(&storage);
				index_ = variant_npos;
			}
			else if ( index_ == v.index_ )
			{
				helper()->copy(&storage, &v.storage);
			}
			else
			{
				helper()->destroy(&storage);
				index_ = variant_npos;
				v.helper()->init(&storage, &v.storage);
				index_ = v.index_;
			}
		}

		return *this;
	}

	template< typename U >
	typename detail::rhs< typename detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::type, variant& >::type operator=(const U& value)
	{
		typedef typename detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::type T;

		if ( index_ == detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value )
		{
			void* p = &storage;
			*static_cast< T* >( p ) = value;
		}
		else
		{
			helper()->destroy(&storage);
			index_ = variant_npos;
			new( &storage )T(value);
			index_ = detail::convert_type< U, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value;
		}

		return *this;
	}

	std::size_t index() const
	{
		return index_;
	}

	bool valueless_by_exception() const
	{
		return index_ == variant_npos;
	}

	void swap(variant& o)
	{
		if ( index_ != variant_npos || o.index_ != variant_npos )
		{
			if ( index_ == o.index_ )
			{
				helper()->swapem(&storage, &o.storage);
			}
			else
			{
				variant t(o);
				o = *this;
				this->operator=(t);
			}
		}
	}

private:
	typedef typename cpp::aligned_storage< detail::max_size< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value, detail::max_align< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value >::type storage_type;

	const detail::ivariant_helper* helper() const
	{
		static const detail::ivariant_helper* helpers[] =
		{
			detail::variant_helper< T1 >::instance(),
			detail::variant_helper< T2 >::instance(),
			detail::variant_helper< T3 >::instance(),
			detail::variant_helper< T4 >::instance(),
			detail::variant_helper< T5 >::instance(),
			detail::variant_helper< T6 >::instance(),
			detail::variant_helper< T7 >::instance(),
			detail::variant_helper< T8 >::instance(),
			detail::variant_helper< T9 >::instance(),
			detail::variant_helper< T10 >::instance(),
		};

		return helpers[index_];
	}

	std::size_t  index_;
	storage_type storage;
};

template< typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
typename detail::select_type< T, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::type & get(variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >&v)
{
	if ( v.index_ != detail::select_type< T, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value )
	{
		throw bad_variant_access();
	}

	return *static_cast< T* >( static_cast< void* >( &v.storage ) );
}

template< typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
typename detail::select_type< T, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::type const & get(const variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >&v)
{
	if ( v.index_ != detail::select_type< T, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value )
	{
		throw bad_variant_access();
	}

	return *static_cast< const T* >( static_cast< const void* >( &v.storage ) );
}

template< std::size_t I, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
typename variant_alternative< I, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >::type & get(variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >&v)
{
	if ( v.index_ != I )
	{
		throw bad_variant_access();
	}

	return *static_cast< typename variant_alternative< I, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >::type* >( static_cast< void* >( &v.storage ) );
}

template< std::size_t I, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
typename variant_alternative< I, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >::type const & get(const variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >&v)
{
	if ( v.index_ != I )
	{
		throw bad_variant_access();
	}

	return *static_cast< typename variant_alternative< I, variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 > >::type const* >( static_cast< const void* >( &v.storage ) );
}

template< typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10 >
bool holds_alternative(const variant< T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >& v)
{
	return v.index() == detail::select_type< T, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 >::value;
}

}
#endif // ifdef CPP11

namespace cpp17
{
struct monostate {};

inline bool operator<(
	monostate,
	monostate
)
{
	return false;
}

inline bool operator>(
	monostate,
	monostate
)
{
	return false;
}

inline bool operator<=(
	monostate,
	monostate
)
{
	return true;
}

inline bool operator>=(
	monostate,
	monostate
)
{
	return true;
}

inline bool operator==(
	monostate,
	monostate
)
{
	return true;
}

inline bool operator!=(
	monostate,
	monostate
)
{
	return false;
}

}
#endif // ifdef CPP17

#endif // VARIANT_H

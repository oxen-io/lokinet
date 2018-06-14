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
#ifndef PBL_CPP_OPTIONAL_H
#define PBL_CPP_OPTIONAL_H

#include "version.h"

#ifdef CPP17
#include <optional>
#else
#include <stdexcept>
#include <new>
#include "type_traits.h"

namespace cpp17
{
struct none_t
{

};

const none_t none = {};

/** @brief A class that represents an uninitialized value
 *
 * This is a simple wrapper for a (T, bool) pair where the bool
 * indicates whether or not the T value contains anything
 * meaningful (i.e., whether it has been "set"). For example,
 *
 * @code
 * optional<int> num_near_wins;
 *
 * // num_near_wins = 6;
 *
 * if (num_near_wins)
 *   generate_near_wins(num_near_wins.get());
 * @endcode
 *
 * Equivalent to get() is the use of the dereference or pointer
 * operator. This is sometimes more useful for classes.
 *
 * @code
 * optional< Symbol > multiplier_sym;
 *
 * ... // somewhere multiplier_sym gets set
 *
 * const int id = multiplier_sym->getId()
 * @endcode
 *
 * @todo Partial specialization for POD since we don't have to bother with
 * placement new and all that.
 */
template< typename T >
class optional
{
	typedef void ( optional::* bool_type )() const;
public:
	typedef T value_type;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* pointer;
	typedef const T* const_pointer;

	/// @brief An uninitialized T
	optional()
		: init(false)
	{
	}

	optional(none_t)
		: init(false)
	{
	}

	/** @brief Copy constructor
	 * @param x The optional to copy
	 */
	optional(const optional& x)
		: init(false)
	{
		if ( x.init )
		{
			acquire( x.get() );
		}
	}

	/** @brief Construct an initialized T, copied from x
	 * @param x The value to set *this to
	 */
	optional(const T& x)
		: init(false)
	{
		acquire(x);
	}

	template< typename U >
	explicit optional(const optional< U >& x)
		: init(false)
	{
		if ( x )
		{
			acquire( x.get() );
		}
	}

	~optional()
	{
		release();
	}

	optional& operator=(none_t)
	{
		release();

		return *this;
	}

	/** @brief Copy assignment
	 * @param x The optional to copy
	 */
	optional& operator=(const optional& x)
	{
		release();

		if ( x.init )
		{
			acquire( x.get() );
		}

		return *this;
	}

	/** @brief Set the value of this to x
	 * @param x The value to set *this to
	 *
	 * @note This is considered initilized after this function returns
	 */
	optional& operator=(const T& x)
	{
		release();
		acquire(x);

		return *this;
	}

	template< typename U >
	optional& operator=(const optional< U >& x)
	{
		release();

		if ( x )
		{
			acquire( x.get() );
		}

		return *this;
	}

	const_reference value() const
	{
		if ( !init )
		{
			throw std::runtime_error("Attempting to get from an uninitialized optional<T>");
		}

		return *data();
	}

	reference value()
	{
		if ( !init )
		{
			throw std::runtime_error("Attempting to get from an uninitialized optional<T>");
		}

		return *data();
	}

	template< typename U >
	value_type value_or(const U& u) const
	{
		if ( init )
		{
			return get();
		}

		return u;
	}

	/** @brief Get a reference to the (initialized) value. Const version.
	 * @throws std::runtime_error if this is not initialized
	 */
	const_reference get() const
	{
		return *data();
	}

	/** @brief Get a reference to the (initialized) value
	 * @throws std::runtime_error if this is not initialized
	 */
	reference get()
	{
		return *data();
	}

	/// @brief Dereference notation. Same as get()
	const_reference operator*() const
	{
		return get();
	}

	/// @brief Dereference notation. Same as get()
	reference operator*()
	{
		return get();
	}

	const_pointer get_ptr() const
	{
		return init ? data() : 0;
	}

	pointer get_ptr()
	{
		return init ? data() : 0;
	}

	/// @brief Pointer notation. Like get(), but returns a pointer
	const_pointer operator->() const
	{
		return data();
	}

	/// @brief Pointer notation. Like get(), but returns a pointer
	pointer operator->()
	{
		return data();
	}

	/** @brief Conversion to bool
	 * @returns true iff object is initialized
	 */
	operator bool_type() const
	{
		return init ? &optional::safe_bool_fn : NULL;
	}

	/** @brief Conversion to bool (negation)
	 * @returns true iff the object is @em not initialized
	 */
	bool operator!() const
	{
		return !init;
	}

private:
	// Safe bool idiom
	void safe_bool_fn() const
	{
	}

	T* data()
	{
		return static_cast< T* >( static_cast< void* >( &value_ ) );
	}

	const T* data() const
	{
		return static_cast< const T* >( static_cast< const void* >( &value_ ) );
	}

	// Should only ever be called when init == false
	void acquire(const T& x)
	{
		new( static_cast< void* >( &value_ ) )T(x);
		init = true;
	}

	// Calls the destructor for an allocated value
	void release()
	{
		if ( init )
		{
			get().~T();
			init = false;
		}
	}

	bool init; ///< True iff object has been initialized with a value

	/** Reserved space to hold an instance of T
	 *
	 * value is managed with placement new/delete. It is cast wherever it's used.
	 */
	typename ::cpp::aligned_union< 0, T >::type value_;
};
}

#endif // ifdef CPP17
#endif // ifndef PBL_CPP_OPTIONAL_H

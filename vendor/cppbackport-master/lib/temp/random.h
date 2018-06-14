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
/** @file random.h
 * @brief Implementation of C++11 random header
 */

#ifndef PBL_CPP_RANDOM_H
#define PBL_CPP_RANDOM_H

#include "version.h"

#ifdef CPP11
#include <random>
#else

#include <stdexcept>
#include <limits>
#include <cstddef>

namespace cpp11
{
class random_device
{
public:
	typedef unsigned int result_type;

	random_device();

	result_type operator()();
private:
	random_device(const random_device&);            /* noncopyable */
	random_device& operator=(const random_device&); /* noncopyable */

	int fd;
};

template< class UIntType, std::size_t W, std::size_t N, std::size_t M, std::size_t R, UIntType A, std::size_t U, UIntType D, std::size_t S, UIntType B, std::size_t T, UIntType C, std::size_t L, UIntType F >
class mersenne_twister_engine
{
public:
	typedef UIntType result_type;

	static const UIntType default_seed = 5489u;

	/** \brief Construct a pseudo-random number generator
	 * \param value The seed to initialize the PRNG with
	 */
	explicit mersenne_twister_engine(result_type value = default_seed)
		: state(), k(0)
	{
		seed(value);
	}

	/** \brief Seed the object with a 32-bit value
	 * @param value The seed to use
	 */
	void seed(result_type value = default_seed)
	{
		const result_type x = max_;

		state[0] = static_cast< result_type >( value ) & x;

		// Expand the state
		for ( k = 1; k < N; k++ )
		{
			state[k] = ( ( state[k - 1] ^ ( state[k - 1] >> ( W - 2 ) ) ) * F + k ) & x;
		}
	}

	/// \brief Generate a random value
	result_type operator()()
	{
		return get();
	}

	/// \brief Move ahead by z calls
	void discard(unsigned long z)
	{
		while ( z-- )
		{
			get();
		}
	}

	/// \brief Return the minimum value that this rng can produce
	static result_type min()
	{
		return 0;
	}

	/// \brief Return the maximum value that this rng can produce
	static result_type max()
	{
		return max_;
	}

	/// \brief Get a random value, increment the internal counter
	result_type get()
	{
		if ( k >= N )
		{
			generate_state();
		}

		// Temper and mix the result
		UIntType y = state[k++];

		y ^= ( y >> U ) & D;
		y ^= ( y << S ) & B;
		y ^= ( y << T ) & C;
		y ^= ( y >> L );

		return y;
	}

private:
	/// \brief Regenerate the state of the mersenne twister
	void generate_state()
	{
		k = 0;

		const UIntType x = ( ~UIntType(0) ) << R;

		for ( unsigned j = 0; j < N; j++ )
		{
			UIntType y = state[( j + 1 ) % N] ^ ( ( state[j] ^ state[( j + 1 ) % N] ) & x );
			y        = ( y >> 1 ) ^ ( -( y % 2 ) & A );
			state[j] = state[( j + M ) % N] ^ y;
		}
	}

	static const result_type max_ = ~( ( ( ~UIntType(0) ) << 1 ) << ( W - 1 ) );

	// mersenne twister state
	UIntType state[N];
	unsigned k;
};

typedef mersenne_twister_engine< unsigned long, 32, 624, 397, 31, 0x9908b0dful, 11, 0xfffffffful, 7, 0x9d2c5680ul, 15, 0xefc60000ul, 18, 1812433253ul > mt19937;
typedef mt19937 default_random_engine;

template< typename T >
class uniform_int_distribution
{
public:
	typedef T result_type;

	explicit uniform_int_distribution(
		T a = 0,
		T b = std::numeric_limits< T >::max()
	)
		: min(a), max(b)
	{
	}

	template< typename Generator >
	result_type operator()(Generator& g)
	{
		typedef typename Generator::result_type gtype;

		const gtype max_ = g.max();

		const gtype n = max - min;

		if ( n > max_ )
		{
			throw std::logic_error("Not implemented");
		}

		gtype x = g();

		if ( n < max_ )
		{
			gtype rem = max_ % ( n + 1 );

			if ( rem != n )
			{
				while ( x >= max_ - rem )
				{
					x = g();
				}

				x %= ( n + 1 );
			}
			else if ( n != 0 )
			{
				x /= ( ( max_ / ( n + 1 ) ) + 1 );
			}
			else
			{
				x = 0;
			}
		}

		return min + static_cast< T >( x );
	}

private:
	T min;
	T max;
};

template< typename R >
class uniform_real_distribution
{
public:
	typedef R result_type;

	explicit uniform_real_distribution(
		R a = 0,
		R b = 1
	)
		: min(a), max(b)
	{
	}

	/** @todo If Generator::max() is 2**n-1 then we should get u by simply
	 * shifting bits into R(0)
	 * @todo Alternatively, determine the number of uniformly distributed
	 * points between min and max. Treat this as n int, and (effectively)
	 * call uniform_int_distribution.
	 */
	template< typename Generator >
	result_type operator()(Generator& g)
	{
		/// @bug Can be 1, but should never be 1
		const R u = static_cast< R >( g() ) / static_cast< R >( g.max() );

		/// @bug Rounding can cause "max" to be returned
		return ( max - min ) * u + min;
	}

private:
	R min;
	R max;
};
}
#endif
#endif // PBL_CPP_RANDOM_H

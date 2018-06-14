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
#ifndef PBL_CPP_ARRAY_H
#define PBL_CPP_ARRAY_H

#include "version.h"

#ifdef CPP11
#include <array>
#else
#include <cstddef>
#include <stdexcept>
#include "algorithm.h"
#include "iterator.h"

namespace cpp11
{
template< class T, std::size_t N >
struct array
{
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef std::reverse_iterator< iterator > reverse_iterator;
	typedef std::reverse_iterator< const_iterator > const_reverse_iterator;

	reference at(size_type n)
	{
		if ( n >= N )
		{
			throw std::out_of_range("");
		}

		return elems[n];
	}

	const_reference at(size_type n) const
	{
		if ( n >= N )
		{
			throw std::out_of_range("");
		}

		return elems[n];
	}

	reference operator[](size_type n)
	{
		return elems[n];
	}

	const_reference operator[](size_type n) const
	{
		return elems[n];
	}

	reference front()
	{
		return elems[0];
	}

	const_reference front() const
	{
		return elems[0];
	}

	reference back()
	{
		return elems[N - 1];
	}

	const_reference back() const
	{
		return elems[N - 1];
	}

	T* data()
	{
		return elems;
	}

	const T* data() const
	{
		return elems;
	}

	iterator begin()
	{
		return elems;
	}

	const_iterator begin() const
	{
		return elems;
	}

	const_iterator cbegin() const
	{
		return elems;
	}

	iterator end()
	{
		return elems + N;
	}

	const_iterator end() const
	{
		return elems + N;
	}

	const_iterator cend() const
	{
		return elems + N;
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator( elems + ( N - 1 ) );
	}

	const_reverse_iterator rbegin() const
	{
		return reverse_iterator( elems + ( N - 1 ) );
	}

	const_reverse_iterator crbegin() const
	{
		return reverse_iterator( elems + ( N - 1 ) );
	}

	reverse_iterator rend()
	{
		return reverse_iterator(elems - 1);
	}

	const_reverse_iterator rend() const
	{
		return reverse_iterator(elems - 1);
	}

	const_reverse_iterator crend() const
	{
		return reverse_iterator(elems - 1);
	}

	bool empty() const
	{
		return false;
	}

	size_type size() const
	{
		return N;
	}

	size_type max_size() const
	{
		return N;
	}

	void fill(const T& t)
	{
		std::fill_n(elems, N, t);
	}

	void swap(array< T, N >& a)
	{
		std::swap_ranges(elems, elems + N, a.elems);
	}

	T elems[N];
};


template< class T >
struct array< T, 0 >
{
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef std::reverse_iterator< iterator > reverse_iterator;
	typedef std::reverse_iterator< const_iterator > const_reverse_iterator;

	T elems[1];

	void fill(const T& t)
	{
	}

	void swap(array< T, 0 >&)
	{
	}

	iterator begin()
	{
		return elems;
	}

	const_iterator begin() const
	{
		return elems;
	}

	const_iterator cbegin() const
	{
		return elems;
	}

	iterator end()
	{
		return elems;
	}

	const_iterator end() const
	{
		return elems;
	}

	const_iterator cend() const
	{
		return elems;
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator();
	}

	const_reverse_iterator rbegin() const
	{
		return reverse_iterator();
	}

	const_reverse_iterator crbegin() const
	{
		return reverse_iterator();
	}

	reverse_iterator rend()
	{
		return reverse_iterator();
	}

	const_reverse_iterator rend() const
	{
		return reverse_iterator();
	}

	const_reverse_iterator crend() const
	{
		return reverse_iterator();
	}

	size_type size() const
	{
		return 0;
	}

	size_type max_size() const
	{
		return 0;
	}

	bool empty() const
	{
		return true;
	}

	reference operator[](size_type n)
	{
		return elems[n];
	}

	const_reference operator[](size_type n) const
	{
		return elems[n];
	}

	reference at(size_type n)
	{
		throw std::out_of_range("");

		return elems[n];
	}

	const_reference at(size_type n) const
	{
		throw std::out_of_range("");

		return elems[n];
	}

	reference front()
	{
		return elems[0];
	}

	const_reference front() const
	{
		return elems[0];
	}

	reference back()
	{
		return elems[0];
	}

	const_reference back() const
	{
		return elems[0];
	}

	T* data()
	{
		return elems;
	}

	const T* data() const
	{
		return elems;
	}

};

template< class T, std::size_t N >
void swap(
	array< T, N >& a,
	array< T, N >& b
)
{
	a.swap(b);
}

template< class T, std::size_t N >
bool operator==(
	const array< T, N >& a,
	const array< T, N >& b
)
{
	return std::equal( a.begin(), a.end(), b.begin() );
}

template< class T, std::size_t N >
bool operator!=(
	const array< T, N >& a,
	const array< T, N >& b
)
{
	return !std::equal( a.begin(), a.end(), b.begin() );
}

template< class T, std::size_t N >
bool operator<(
	const array< T, N >& a,
	const array< T, N >& b
)
{
	return std::lexicographical_compare( a.begin(), a.end(), b.begin(), b.end() );
}

template< class T, std::size_t N >
bool operator<=(
	const array< T, N >& a,
	const array< T, N >& b
)
{
	return !std::lexicographical_compare( b.begin(), b.end(), a.begin(), a.end() );
}

template< class T, std::size_t N >
bool operator>(
	const array< T, N >& a,
	const array< T, N >& b
)
{
	return std::lexicographical_compare( b.begin(), b.end(), a.begin(), a.end() );
}

template< class T, std::size_t N >
bool operator>=(
	const array< T, N >& a,
	const array< T, N >& b
)
{
	return !std::lexicographical_compare( a.begin(), a.end(), b.begin(), b.end() );
}

}

#endif // ifdef CPP11
#endif // PBL_CPP_ARRAY_H

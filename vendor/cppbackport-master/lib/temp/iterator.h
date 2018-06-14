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
/** @file iterator.h
 * @brief Implementation of C++11 iterator header
 */
#ifndef PBL_CPP_ITERATOR_H
#define PBL_CPP_ITERATOR_H

#include "version.h"

#include <iterator>

#ifndef CPP11
namespace cpp11
{
template< typename T >
typename T::iterator begin(T& c)
{
	return c.begin();
}

/** @brief Get a beginning iterator to a container
 * @param c A container
 * @returns c.begin()
 */
template< typename T >
typename T::const_iterator begin(const T& c)
{
	return c.begin();
}

template< typename T >
typename T::iterator end(T& c)
{
	return c.end();
}

/** @brief Get an end iterator to a container
 * @param c A container
 * @returns c.end()
 */
template< typename T >
typename T::const_iterator end(const T& c)
{
	return c.end();
}

template< typename T, std::size_t N >
T* begin(T(&a)[N])
{
	return a;
}

/** @brief Get a beginning iterator to an array
 * @param a An array
 * @returns Pointer to the first element of a
 */
template< typename T, std::size_t N >
const T* begin(const T(&a)[N])
{
	return a;
}

template< typename T, std::size_t N >
T* end(T(&a)[N])
{
	return a + N;
}

/** @brief Get an end iterator to an array
 * @param a An array
 * @returns Pointer to the past-the-last elemnt of a
 */
template< typename T, std::size_t N >
const T* end(const T(&a)[N])
{
	return a + N;
}

template< typename InputIterator >
InputIterator next(
	InputIterator                                                   it,
	typename std::iterator_traits< InputIterator >::difference_type n = 1
)
{
	std::advance(it, n);

	return it;
}

template< typename BidirectionalIterator >
BidirectionalIterator prev(
	BidirectionalIterator                                                   it,
	typename std::iterator_traits< BidirectionalIterator >::difference_type n = 1
)
{
	std::advance(it, -n);

	return it;
}

}
#endif // ifndef CPP11

#ifndef CPP14
namespace cpp14
{

template< typename T >
typename T::const_iterator cbegin(const T& c)
{
	return c.begin();
}

template< typename T, std::size_t N >
const T* cbegin(const T(&a)[N])
{
	return a;
}

template< typename T >
typename T::const_iterator cend(const T& c)
{
	return c.end();
}

template< typename T, std::size_t N >
const T* cend(const T(&a)[N])
{
	return a + N;
}

}
#endif // ifndef CPP14

#ifndef CPP17
namespace cpp17
{
template< typename T >
typename T::size_type size(const T& c)
{
	return c.size();
}

template< typename T, std::size_t N >
std::size_t size(const T(&)[N])
{
	return N;
}

template< typename T >
bool empty(const T& c)
{
	return c.empty();
}

template< typename T, std::size_t N >
bool empty(const T(&)[N])
{
	return false;
}

}
#endif // ifndef CPP17

#endif // PBL_CPP__ITERATOR_H

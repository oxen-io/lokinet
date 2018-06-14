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
#ifndef PBL_CPP_ALGORITHM_H
#define PBL_CPP_ALGORITHM_H

#include <algorithm>

#include "version.h"

#ifndef CPP11
#include <functional>

namespace cpp11
{

template< class InputIterator, class Predicate >
InputIterator find_if_not(
	InputIterator first,
	InputIterator last,
	Predicate     q
)
{
	for (; first != last; ++first )
	{
		if ( !q(*first) )
		{
			return first;
		}
	}

	return last;
}

template< class InputIterator, class Predicate >
bool all_of(
	InputIterator first,
	InputIterator last,
	Predicate     p
)
{
	return ::cpp11::find_if_not(first, last, p) == last;
}

template< class InputIterator, class Predicate >
bool any_of(
	InputIterator first,
	InputIterator last,
	Predicate     p
)
{
	return std::find_if(first, last, p) != last;
}

template< class InputIterator, class Predicate >
bool none_of(
	InputIterator first,
	InputIterator last,
	Predicate     p
)
{
	return std::find_if(first, last, p) == last;
}

template< class InputIterator, class OutputIterator, class Predicate >
OutputIterator copy_if(
	InputIterator  first,
	InputIterator  last,
	OutputIterator d_first,
	Predicate      pred
)
{
	while ( first != last )
	{
		if ( pred(*first) )
		{
			*d_first++ = *first;
		}

		first++;
	}

	return d_first;
}

template< class InputIterator, class Size, class OutputIterator >
OutputIterator copy_n(
	InputIterator  first,
	Size           count,
	OutputIterator result
)
{
	if ( count > 0 )
	{
		*result++ = *first;

		for ( Size i = 1; i < count; ++i )
		{
			*result++ = *++first;
		}
	}

	return result;
}

template< class InputIterator, class Predicate >
bool is_partitioned(
	InputIterator first,
	InputIterator last,
	Predicate     p
)
{
	for (; first != last; ++first )
	{
		if ( !p(*first) )
		{
			break;
		}
	}

	for (; first != last; ++first )
	{
		if ( p(*first) )
		{
			return false;
		}
	}

	return true;
}

template< class InputIterator, class OutputIt1,
          class OutputIt2, class Predicate >
std::pair< OutputIt1, OutputIt2 > partition_copy(
	InputIterator first,
	InputIterator last,
	OutputIt1     d_first_true,
	OutputIt2     d_first_false,
	Predicate     p
)
{
	while ( first != last )
	{
		if ( p(*first) )
		{
			*d_first_true = *first;
			++d_first_true;
		}
		else
		{
			*d_first_false = *first;
			++d_first_false;
		}

		++first;
	}

	return std::pair< OutputIt1, OutputIt2 >(d_first_true, d_first_false);
}

template< class ForwardIterator, class Compare >
ForwardIterator is_sorted_until(
	ForwardIterator first,
	ForwardIterator last,
	Compare         comp
)
{
	if ( first != last )
	{
		ForwardIterator next = first;

		while ( ++next != last )
		{
			if ( comp(*next, *first) )
			{
				return next;
			}

			first = next;
		}
	}

	return last;
}

template< class ForwardIterator >
ForwardIterator is_sorted_until(
	ForwardIterator first,
	ForwardIterator last
)
{
	return ::cpp11::is_sorted_until( first, last, std::less< typename std::iterator_traits< ForwardIterator >::value_type >() );
}

template< class ForwardIterator >
bool is_sorted(
	ForwardIterator first,
	ForwardIterator last
)
{
	return ::cpp11::is_sorted_until(first, last) == last;
}

template< class ForwardIterator, class Compare >
bool is_sorted(
	ForwardIterator first,
	ForwardIterator last,
	Compare         comp
)
{
	return ::cpp11::is_sorted_until(first, last, comp) == last;
}

template< class T >
std::pair< const T&, const T& > minmax(
	const T& a,
	const T& b
)
{
	return ( b < a ) ? std::pair< const T&, const T& >(b, a)
		   : std::pair< const T&, const T& >(a, b);
}

template< class T, class Compare >
std::pair< const T&, const T& > minmax(
	const T& a,
	const T& b,
	Compare  comp
)
{
	return comp(b, a) ? std::pair< const T&, const T& >(b, a)
		   : std::pair< const T&, const T& >(a, b);
}

template< class ForwardIterator, class Compare >
std::pair< ForwardIterator, ForwardIterator > minmax_element(
	ForwardIterator first,
	ForwardIterator last,
	Compare         comp
)
{
	std::pair< ForwardIterator, ForwardIterator > result(first, first);

	if ( first == last )
	{
		return result;
	}

	if ( ++first == last )
	{
		return result;
	}

	if ( comp(*first, *result.first) )
	{
		result.first = first;
	}
	else
	{
		result.second = first;
	}

	while ( ++first != last )
	{
		ForwardIterator i = first;

		if ( ++first == last )
		{
			if ( comp(*i, *result.first) )
			{
				result.first = i;
			}
			else if ( !( comp(*i, *result.second) ) )
			{
				result.second = i;
			}

			break;
		}
		else
		{
			if ( comp(*first, *i) )
			{
				if ( comp(*first, *result.first) )
				{
					result.first = first;
				}

				if ( !( comp(*i, *result.second) ) )
				{
					result.second = i;
				}
			}
			else
			{
				if ( comp(*i, *result.first) )
				{
					result.first = i;
				}

				if ( !( comp(*first, *result.second) ) )
				{
					result.second = first;
				}
			}
		}
	}

	return result;
}

template< class ForwardIterator >
std::pair< ForwardIterator, ForwardIterator > minmax_element(
	ForwardIterator first,
	ForwardIterator last
)
{
	return ::cpp11::minmax_element( first, last, std::less< typename std::iterator_traits< ForwardIterator >::value_type >() );
}

template< class ForwardIterator, class T >
void iota(
	ForwardIterator first,
	ForwardIterator last,
	T               value
)
{
	while ( first != last )
	{
		*first++ = value;
		++value;
	}
}

template< class InputIterator, class OutputIterator >
OutputIterator move(
	InputIterator  first,
	InputIterator  last,
	OutputIterator dest
)
{
	while ( first != last )
	{
		*dest++ = *first++;
	}

	return dest;
}

}
#endif // ifndef CPP11

#ifndef CPP14
namespace cpp14
{
}
#endif // ifndef CPP14

#ifndef CPP17
namespace cpp17
{
template< typename T, class Compare >
const T& clamp(const T& value, const T& lo, const T& hi, Compare comp)
{
    if (comp(value, lo))
        return lo;
    if (comp(hi, value))
        return hi;
    return value;
}

template< typename T >
const T& clamp(const T& value, const T& lo, const T& hi)
{
    return ::cpp17::clamp(value, lo, hi, std::less<>());
}
}
#endif


#endif // PBL_CPP_ALGORITHM_H

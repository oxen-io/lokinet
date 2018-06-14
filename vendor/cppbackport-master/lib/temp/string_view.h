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
#ifndef PBL_CPP_STRING_VIEW_H
#define PBL_CPP_STRING_VIEW_H

#include "version.h"

#ifdef CPP17
#include <string_view>
#else
#include <stdexcept>
#include <string>
#include <limits>

namespace cpp17
{
template< typename CharT, typename Traits = std::char_traits< CharT > >
class basic_string_view
{
public:
	typedef Traits traits_type;
	typedef CharT value_type;
	typedef CharT* pointer;
	typedef const CharT* const_pointer;
	typedef CharT& reference;
	typedef const CharT& const_reference;
	typedef const CharT* const_iterator;
	typedef const_iterator iterator;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

	static const size_type npos = size_type(-1);

	basic_string_view()
		: first(0), len(0)
	{
	}

	basic_string_view(const basic_string_view&) = default;
	basic_string_view(
		const CharT* s,
		size_type    count
	)
		: first(s), len(count)
	{
	}

	basic_string_view(const CharT* s)
		: first(s), len( Traits::length(s) )
	{
	}

	basic_string_view& operator=(const basic_string_view&) = default;

	const_iterator begin() const
	{
		return first;
	}

	const_iterator cbegin() const
	{
		return first;
	}

	const_iterator end() const
	{
		return first + len;
	}

	const_iterator cend() const
	{
		return first + len;
	}

	const_reference operator[](size_type i) const
	{
		return first[i];
	}

	const_reference at(size_type i) const
	{
		if ( i >= len )
		{
			throw std::out_of_range("Index is out of range for string view");
		}

		return first[i];
	}

	const_reference front() const
	{
		return first[0];
	}

	const_reference back() const
	{
		return first + ( len - 1 );
	}

	const_pointer data() const
	{
		return first;
	}

	size_type size() const
	{
		return len;
	}

	size_type length() const
	{
		return size();
	}

	size_type max_size() const
	{
		return static_cast< size_type >( std::numeric_limits< difference_type >::max() );
	}

	bool empty() const
	{
		return len == 0;
	}

	void remove_prefix(size_type n)
	{
		first += n;
	}

	void remove_suffix(size_type n)
	{
		len -= n;
	}

	void swap(basic_string_view& v)
	{
		const CharT* t = first;

		first   = v.first;
		v.first = t;

		size_type n = len;
		len   = v.len;
		v.len = n;
	}

	size_type copy(
		CharT*    dest,
		size_type count,
		size_type pos = 0
	) const
	{
		if ( pos < len )
		{
			const char*     p = first + pos;
			const size_type m = len - pos;
			const size_type n = count < m ? count : m;

			for ( size_t i = 0; i < n; ++i )
			{
				dest[i] = p[i];
			}

			return n;
		}

		return 0;
	}

	basic_string_view substr(
		size_type pos = 0,
		size_type count = npos
	) const
	{
		const size_type n = ( pos < len )
		                    ? ( count <= len - pos ? count : len - pos )
							: 0;

		return basic_string_view(first + pos, n);
	}

private:
	const CharT* first;
	size_type    len;
};

typedef basic_string_view< char > string_view;
typedef basic_string_view< wchar_t > wstring_view;
#ifdef CPP11
typedef basic_string_view< char16_t > u16string_view;
typedef basic_string_view< char32_t > u32string_view;
#endif
}
#endif // ifdef CPP17

#endif // STRING_VIEW_H

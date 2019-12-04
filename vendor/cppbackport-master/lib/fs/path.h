/* Copyright (c) 2015, Pollard Banknote Limited
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
#ifndef PBL_CPP_FS_PATH_H
#define PBL_CPP_FS_PATH_H
#include <string>
#include <iosfwd>
#include <iterator>
#include <iostream>

namespace cpp17
{
namespace filesystem
{
/** A file path
 *
 * A partial implementation of std::experimental::filesystem::path
 * @todo Construct from the other basic_string types and pointer-to-char16_t, etc.
 * @bug Throughout, preferred_separator is used incorrectly
 */
class path
{
public:
	typedef char value_type;
	typedef std::basic_string< value_type > string_type;

	static const value_type preferred_separator = '/';

	class const_iterator;

	typedef const_iterator iterator;

	/// An empty path
	path();

	/// Construct a path form the given string
	path(const string_type&);

	path(const char*);

	template< typename Iterator >
	path(Iterator first, Iterator last) : s(first, last)
    {
    }

	/// Copy constructor
	path(const path&);

	/// Copy assignment
	path& operator=(const path&);

	/// Append oeprator
	path& operator+=(const path&);

	template< typename Source >
	path& operator=(const Source& source)
	{
		return assign(source);
	}

	template< typename Source >
	path& assign(const Source& source)
	{
		s = string_type(source);
		return *this;
	}

	template< typename Iterator >
	path& assign(
		Iterator first,
		Iterator last
	)
	{
		s = string_type(first, last);
		return *this;
	}

	/// Clear the path
	void clear();

	/// Swap the two paths
	void swap(path&);

	std::string string() const;
	const char* c_str() const;
	path& operator/=(const path&);
	path& append(const path&);

	/// Is the path empty (i.e., "")
	bool empty() const;
	const std::string& native() const;

	path extension() const;
	path filename() const;
	path parent_path() const;
	path& remove_filename();
	path& replace_filename(const path&);

	operator string_type() const;

	bool is_absolute() const;

	path lexically_normal() const;
	path lexically_relative(const path&) const;

	int compare(const path&) const;

	const_iterator begin() const;
	const_iterator end() const;
private:
	struct begin_iterator_tag {};
	struct end_iterator_tag {};

	std::string s;
};

class path::const_iterator
{
public:
	typedef const path value_type;
	typedef const path& reference;
	typedef const path* pointer;
	typedef std::ptrdiff_t difference_type;

	const_iterator();
	const_iterator(const path*, begin_iterator_tag);
	const_iterator(const path*, end_iterator_tag);

	const_iterator& operator++();
	const_iterator operator++(int);
	const_iterator& operator--();
	const_iterator operator--(int);
	bool operator==(const const_iterator&) const;
	bool operator!=(const const_iterator&) const;
	const path& operator*() const;
	const path* operator->() const;
private:
	const path* parent;
	std::size_t first;
	std::size_t last;
	path        value;
};


path operator/(const path& lhs, const path& rhs);

std::ostream& operator<<(std::ostream&, const path&);

bool operator==(const path&, const path&);
bool operator!=(const path&, const path&);
bool operator<(const path&, const path&);
bool operator<=(const path&, const path&);
bool operator>(const path&, const path&);
bool operator>=(const path&, const path&);
}
}

namespace std
{
template< >
struct iterator_traits< ::cpp17::filesystem::path::const_iterator >
{
	typedef std::ptrdiff_t difference_type;
	typedef const ::cpp17::filesystem::path value_type;
	typedef const ::cpp17::filesystem::path* pointer;
	typedef const ::cpp17::filesystem::path& reference;
	typedef std::bidirectional_iterator_tag iterator_category;
};
}
#endif // PBL_FS_PATH_H

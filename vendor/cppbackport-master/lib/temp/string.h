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
/** @file string.h
 * @brief Implementation of C++11 string header
 * @todo Might be able to use std::num_put
 */
#ifndef PBL_CPP_STRING_H
#define PBL_CPP_STRING_H

#include <string>

#include "version.h"

#ifndef CPP11
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "config/arch.h"
namespace cpp11
{
namespace detail
{
template< typename I >
std::string arithmetic_to_string(I x)
{
	std::ostringstream ss;

	ss << x;

	return ss.str();
}

template< typename I >
std::wstring arithmetic_to_wstring(I x)
{
    std::wostringstream ss;
    ss << x;
    return ss.str();
}
}

inline unsigned long stoul(
	const std::string& s,
	std::size_t*       pos = 0,
	int                base = 10
)
{
	char*       ptr;
	const char* s_  = s.c_str();
	const long  res = std::strtoul(s_, &ptr, base);

	if ( pos )
	{
		*pos = ptr - s_;
	}

	return res;
}

/// @todo Should throw if not convertible, or result is too large
inline long stol(
	const std::string& s,
	std::size_t*       pos = 0,
	int                base = 10
)
{
	char*       ptr;
	const char* s_  = s.c_str();
	const long  res = std::strtol(s_, &ptr, base);

	if ( pos )
	{
		*pos = ptr - s_;
	}

	return res;
}

/// @todo Should throw if not convertible, or result is too large
inline int stoi(
	const std::string& s,
	std::size_t*       pos = 0,
	int                base = 10
)
{
	return static_cast< int >( stol(s, pos, base) );
}

/** @brief Get the string representation of an int
 * @param n An integer
 * @returns lexical_cast<std::string>(n)
 */
inline std::string to_string(int n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(long n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(unsigned n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(unsigned long n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(float n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(double n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(long double n)
{
	return detail::arithmetic_to_string(n);
}

#ifdef HAS_LONG_LONG
inline std::string to_string(long long n)
{
	return detail::arithmetic_to_string(n);
}

inline std::string to_string(unsigned long long n)
{
	return detail::arithmetic_to_string(n);
}

#endif

inline std::wstring to_wstring(int n)
{
    return detail::arithmetic_to_wstring(n);
}

inline std::wstring to_wstring(unsigned n)
{
    return detail::arithmetic_to_wstring(n);
}

inline std::wstring to_wstring(long n)
{
    return detail::arithmetic_to_wstring(n);
}

inline std::wstring to_wstring(unsigned long n)
{
    return detail::arithmetic_to_wstring(n);
}

#ifdef HAS_LONG_LONG
inline std::wstring to_wstring(long long n)
{
    return detail::arithmetic_to_wstring(n);
}

inline std::wstring to_wstring(unsigned long long n)
{
    return detail::arithmetic_to_wstring(n);
}
#endif

inline std::wstring to_wstring(float n)
{
    return detail::arithmetic_to_wstring(n);
}

inline std::wstring to_wstring(double n)
{
    return detail::arithmetic_to_wstring(n);
}

inline std::wstring to_wstring(long double n)
{
    return detail::arithmetic_to_wstring(n);
}
}
#endif // ifndef CPP11

#endif // PBL_CPP_STRING_H

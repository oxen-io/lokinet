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
/** @file version.h
 * @brief namespaces for C++11 and C++14
 */
#ifndef PBL_CPP_VERSION_H
#define PBL_CPP_VERSION_H

#include <cstddef>

/// @todo Value may not be correct, because as of writing C++17 isn't official
#if __cplusplus >= 201703L
#define CPP17
#endif
#if __cplusplus >= 201402L
#define CPP14
#endif
#if __cplusplus >= 201103L
#define CPP11
#endif

/* Namespace macros. It's sometimes necessary to put something into the "std"
 * namespace. For example, a specialization of std::hash. These macros can be
 * used to put the code in the correct namespace.
 */
#ifdef CPP17
#define CPP17_NAMESPACE std
#else
#define CPP17_NAMESPACE cpp17
#endif
#ifdef CPP14
#define CPP14_NAMESPACE std
#else
#define CPP14_NAMESPACE cpp14
#endif
#ifdef CPP11
#define CPP11_NAMESPACE std
#else
#define CPP11_NAMESPACE cpp11
#endif

namespace cpp11
{
}

#ifndef CPP11
namespace cpp11
{
class nullptr_t
{
public:
	template< class T >
	operator T*() const
	{
		return 0;
	}

	template< class C, class T >
	operator T C::*( ) const
	{
		return 0;
	}
private:
	void operator&() const;
};

}

const cpp11::nullptr_t nullptr = {};
#endif // ifndef CPP11

namespace cpp14
{
}

namespace cpp17
{
}

/* cpp points to either std or cpp11/14/17 as appropriate
 */
namespace cpp
{
using namespace ::std;
using namespace ::cpp11;
using namespace ::cpp14;
using namespace ::cpp17;
}

#endif // PBL_CPP_VERSION_H

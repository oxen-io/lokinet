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
/* Allocate raw storage for a type
 */
#include <iostream>
#include "type_traits.h"

// A POD with high alignment
union maxalign
{
	unsigned long long x;
	long double y;
	void* z;
};

// a type that has 16 byte alignment
typedef int v4si __attribute__(( vector_size(16) ));

int main()
{
	// The type we are checking
	typedef int test_type;

	// Various ways to check alignment.
	std::cout << "Alignment" << std::endl;

	// The GCC way. Non-portable, but definitive
	std::cout << "GCC:           " << __alignof__(test_type) << std::endl;

	// Our implementation of C++11's alignment_of
	std::cout << "alignment_of:  " << cpp::alignment_of< test_type >::value << std::endl;

	// Our implementaiton of C++11's aligned_union
	std::cout << "aligned_union: " << __alignof__(cpp::aligned_union< 1, test_type >::type) << std::endl;
	std::cout << std::endl;

	std::cout << "sizeof" << std::endl;
	std::cout << "type:        " << sizeof( test_type ) << std::endl;
	std::cout << "raw storage: " << sizeof( cpp::aligned_union< 1, test_type >::type ) << std::endl;

}

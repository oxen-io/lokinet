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
#include <vector>
#include <iostream>
#include "optional.h"

typedef std::vector< int > test_type;

const test_type value(3, 5);

// set a value
void example1()
{
	std::cout << "Example 1" << std::endl;
	cpp::optional< test_type > y;

	if ( y )
	{
		std::cout << "y is set" << std::endl;
	}
	else
	{
		std::cout << "y is not set" << std::endl;
	}

	y = value;

	if ( y )
	{
		std::cout << "y is set" << std::endl;
		std::cout << "Size of vector is " << y->size() << std::endl;
	}
	else
	{
		std::cout << "y is not set" << std::endl;
	}

	std::cout << std::endl;
}

// clear a value
void example2()
{
	std::cout << "Example 2" << std::endl;
	cpp::optional< test_type > y(value);

	if ( y )
	{
		std::cout << "y is set" << std::endl;
	}
	else
	{
		std::cout << "y is not set" << std::endl;
	}

	y = cpp::none;

	if ( y )
	{
		std::cout << "y is set" << std::endl;
	}
	else
	{
		std::cout << "y is not set" << std::endl;
	}

	std::cout << std::endl;
}

int main()
{
	example1();
	example2();
}

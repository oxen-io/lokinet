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
#include <iostream>

#include "filesystem.h"
#include "iterator.h"

namespace fs = cpp::filesystem;

void print_ancestors(const fs::path& p)
{
	for ( fs::path::const_iterator it = p.begin(); it != p.end(); ++it )
	{
		std::cout << "\t" << "'" << *it << "'";
	}

	std::cout << std::endl;
}

void print_reverse(const fs::path& p)
{
	fs::path::const_iterator first = p.begin();
	fs::path::const_iterator it    = p.end();

	while ( it != first )
	{
		--it;
		std::cout << "\t" << "'" << *it << "'";
	}

	std::cout << std::endl;
}

const fs::path test[] =
{
	"",
	"/",
	"///",
	"/absolute",
	"/absolute/",
	"/absolute/file",
	"/absolute/dir/",
	"relative",
	"relative/",
	"relative/file",
	"relative/dir/"
};

int main()
{
	for ( const fs::path* it = cpp::begin(test), * last = cpp::end(test); it != last; ++it )
	{
		std::cout << "'" << *it << "'" << std::endl;
		print_ancestors(*it);
		print_reverse(*it);
	}
}

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
#include "direntry.h"

namespace cpp17
{
namespace filesystem
{
directory_entry::directory_entry()
{
}

directory_entry::directory_entry(const directory_entry& e)
	: path_(e.path_)
{

}

directory_entry::directory_entry(const ::cpp17::filesystem::path& p)
	: path_(p)
{

}

directory_entry& directory_entry::operator=(const directory_entry& e)
{
	path_ = e.path_;

	return *this;
}

void directory_entry::assign(const ::cpp17::filesystem::path& p)
{
	path_ = p;
}

void directory_entry::replace_filename(const ::cpp17::filesystem::path& p)
{
	path_ = path_.parent_path() / p;
}

const path& directory_entry::path() const
{
	return path_;
}

directory_entry::operator const ::cpp17::filesystem::path&() const
{
	return path_;
}

file_status directory_entry::status() const
{
	return ::cpp17::filesystem::status(path_);
}

file_status directory_entry::symlink_status() const
{
	return ::cpp17::filesystem::symlink_status(path_);
}

bool operator==(
	const directory_entry& a,
	const directory_entry& b
)
{
	return a.path() == b.path();
}

bool operator!=(
	const directory_entry& a,
	const directory_entry& b
)
{
	return a.path() != b.path();
}

bool operator<(
	const directory_entry& a,
	const directory_entry& b
)
{
	return a.path() < b.path();
}

bool operator<=(
	const directory_entry& a,
	const directory_entry& b
)
{
	return a.path() <= b.path();
}

bool operator>(
	const directory_entry& a,
	const directory_entry& b
)
{
	return a.path() > b.path();
}

bool operator>=(
	const directory_entry& a,
	const directory_entry& b
)
{
	return a.path() >= b.path();
}

}
}

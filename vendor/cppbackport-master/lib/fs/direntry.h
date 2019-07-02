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
#ifndef PBL_CPP_FS_DIRENTRY_H
#define PBL_CPP_FS_DIRENTRY_H

#include <iosfwd>
#include "path.h"
#include "filestatus.h"

namespace cpp17
{
namespace filesystem
{
/** Reperesents a file system object
 */
class directory_entry
{
public:
	directory_entry();
	directory_entry(const directory_entry&);
	explicit directory_entry(const path&);

	directory_entry& operator=(const directory_entry&);
	void assign(const path&);
	void replace_filename(const path&);

	const ::cpp17::filesystem::path& path() const;
	operator const ::cpp17::filesystem::path&() const;

	file_status status() const;

	file_status symlink_status() const;
private:
	::cpp17::filesystem::path path_;
};

bool operator==(const directory_entry&, const directory_entry&);
bool operator!=(const directory_entry&, const directory_entry&);
bool operator<(const directory_entry&, const directory_entry&);
bool operator<=(const directory_entry&, const directory_entry&);
bool operator>(const directory_entry&, const directory_entry&);
bool operator>=(const directory_entry&, const directory_entry&);
}
}
#endif // PBL_FS_DIRENTRY_H

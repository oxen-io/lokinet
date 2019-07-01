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
#ifndef PBL_CPP_FS_COPYFILE_H
#define PBL_CPP_FS_COPYFILE_H

#include "path.h"

namespace copy_options
{
enum copy_options
{
	none               = 0,
	skip_existing      = 1,
	overwrite_existing = 2,
	update_existing    = 4,
	recursive          = 8,
	copy_symlinks      = 16,
	skip_symlinks      = 32,
	directories_only   = 64,
	create_symlinks    = 128,
	create_hard_links  = 256
};
}

namespace cpp17
{
namespace filesystem
{
typedef ::copy_options::copy_options copy_options;

/** Copy the file at source to dest
 *
 * @param source A file to copy
 * @param dest A file (including name) of the file to create
 * @returns true iff dest exists and is a copy of source
 *
 * If source does not exist or is not a file, the copy will fail.
 *
 * If dest exists, it will be overwritten. Dest will have the same file
 * permissions of source, if possible (subject to umask).
 *
 * This function copies the source file "safely". That is, in the event of an
 * error, dest is unaltered (or, if it didn't exist, continues to not exist).
 *
 * @todo The std::experimental::fs namespace defines copy and copy_file. This
 * function should be renamed accordingly.
 */
bool copy_file(const path &source, const path &dest, copy_options);

bool copy_file(const path& source, const path& dest);

}
}

#endif // PBL_CPP_FS_COPYFILE_H

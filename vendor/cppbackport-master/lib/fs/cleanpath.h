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
#ifndef PBL_CPP_FS_CLEANPATH_H
#define PBL_CPP_FS_CLEANPATH_H

#include <string>

namespace cpp17
{
namespace filesystem
{
/** Return a simplified version of path
 *
 * @param path A file system path
 *
 * Collapses multiple path separators (ex., as in "/home//user"); replaces
 * "name/." with "name" and "parent/child/.." with "parent"; removes trailing
 * slashes.
 *
 * The returned path is equivalent to the original path in the sense that it
 * identifies the same file system object. Specifically, relative paths are
 * preserved (ex., no simplification is done to the dot-dot in "../here").
 *
 * If the path is malformed, the empty string is returned. For this function,
 * really only the empty string itself is "malformed".
 *
 * @note This is a textual operation. It does not validate the string against
 * the file system or otherwise touch the file system.
 * @bug foo/bar/../baz may not point to /foo/baz if bar symlinks to flip/flop
 */
std::string cleanpath(const std::string& path);
}
}

#endif // PBL_FS_CLEANPATH_H

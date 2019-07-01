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
#ifndef PBL_CPP_FS_BASENAME_H
#define PBL_CPP_FS_BASENAME_H

#include <string>

namespace cpp17
{
namespace filesystem
{

/** Return the name of the file system component indicated by path
 *
 * @param path A file path
 * @returns The name of the directory or file that the path points to, or the
 * empty string if there's an error
 *
 * The exact behavior of this function is platform dependent, because paths are.
 * However, whatever the platform, it should return the name of the file or
 * directory pathed. Ex., on POSIX platforms it should return "readme.txt" for
 * "/home/user/documents/readme.txt"; similarly it should return "readme.txt"
 * for "C:\Windows\readme.txt" on Windows platforms.
 *
 * This function does not access the file system. It merely parses the string.
 * In particular, it does not check if the path is valid and/or accessible. It
 * also does not check that the component before "." or ".." is, in fact, a
 * directory.
 *
 * If the last component cannot be determined for whatever reason, the string is
 * considered malformed and the empty string is returned.
 *
 * @note Although the empty string is considered a malformed path, "." (and
 * anything equivalent) will return ".".
 */
std::string basename(const std::string& path);

/** Return the directory which has the last path component
 *
 * @param path A file path
 *
 * Determines the last path component as in basename, then returns directory
 * which contains that file system object. Ex., the dirname of
 * "/home/user/documents/readme.txt" is "/home/user/documents".
 *
 * As special cases, the dirname of a bare filename is "."; the dirname of
 * "." (or equivalent) is ".."; the dirname of the root directory is still the
 * root.
 *
 * If the path is malformed, the empty string is returned.
 *
 * @sa basename
 */
std::string dirname(const std::string& path);

}
}

#endif // PBL_FS_BASENAME_H

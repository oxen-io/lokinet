/* Copyright (c) 2014, Pollard Banknote Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
 */
#include "absolute.h"

#include <climits>
#include <cstdlib>

#include "../config/os.h"

namespace cpp17
{
  namespace filesystem
  {
    path
    absolute(const path& filename)
    {
      if(!filename.empty())
      {
#if((defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809l) \
    || defined(__GLIBC__))                                 \
    || defined(ANDROID)
        // Preferred - POSIX-2008 and glibc will allocate the path buffer
        char* res = ::realpath(filename.c_str(), NULL);

        if(res)
        {
          path s = res;
          ::free(res);

          return s;
        }

#else
#ifdef _GNU_SOURCE
        // Maybe we can rely on the GNU extension
        char* res = ::canonicalize_file_name(filename.c_str());

        if(res)
        {
          std::string s = res;
          ::free(res);

          return s;
        }

#elif(((defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) \
       || (defined(_XOPEN_VERSION) && _XOPEN_VERSION >= 500)) \
      && defined(PATH_MAX))
        /// @todo PATH_MAX may be huge or -1, according to man pages for
        /// realpath
        char resolved[PATH_MAX + 1];
        char* res = ::realpath(filename.c_str(), resolved);

        if(res)
        {
          return resolved;
        }

#else
#error "No way to get absolute file path!"
#endif  // if 1
#endif  // if ( defined( _POSIX_VERSION ) && _POSIX_VERSION >= 200809l )
      }

      return path();
    }

  }  // namespace filesystem
}  // namespace cpp17

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
#include "cleanpath.h"
#include <algorithm>

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

namespace cpp17
{
  namespace filesystem
  {
    /// @todo Probably belongs in path.cpp
    std::string
    cleanpath(const std::string& s)
    {
      std::string t;

      if(s.empty())
      {
        // error
        return t;
      }

      const std::size_t n = s.length();

      for(std::size_t i = 0; i < n;)
      {
        if(s[i] == '/')
        {
          // directory separator
          t.push_back('/');
          i = s.find_first_not_of('/', i);
        }
        else
        {
          // path component
          const std::size_t j = std::min(s.find('/', i), n);

          if(s.compare(i, j - i, ".", 1) == 0)
          {
            // handle dot
            if(j < n)
            {
              i = s.find_first_not_of('/', j);
            }
            else
            {
              i = n;
            }
          }
          else if(s.compare(i, j - i, "..", 2) == 0)
          {
            // handle dot-dot
            const std::size_t l = t.length();

            if(l == 0)
            {
              // no previous component (ex., "../src")
              t.assign("..", 2);
              i = j;
            }
            else
            {
              // remove previously copied component (unless root)
              if(l >= 2)
              {
                const std::size_t k = t.find_last_of('/', l - 2);

                if(k == std::string::npos)
                {
                  t.clear();
                }
                else
                {
                  t.resize(k + 1);
                }
              }

              if(j < n)
              {
                i = s.find_first_not_of('/', j);
              }
              else
              {
                i = n;
              }
            }
          }
          else
          {
            // append path component
            t.append(s, i, j - i);
            i = j;
          }
        }
      }

      if(t.empty())
      {
        return ".";
      }

      // drop trailing slashes
      const std::size_t i = t.find_last_not_of('/');

      if(i != std::string::npos)
      {
        t.resize(i + 1);
      }

      return t;
    }

  }  // namespace filesystem
}  // namespace cpp17

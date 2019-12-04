/* Copyright (c) 2015, Pollard Banknote Limited
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
#include "path.h"
#include <algorithm>
namespace cpp17
{
  namespace filesystem
  {
    path::path()
    {
    }

    path::path(const string_type& s_) : s(s_)
    {
    }

    path::path(const char* s_) : s(s_)
    {
    }

    path::path(const path& p) : s(p.s)
    {
    }

    path&
    path::operator+=(const path& p)
    {
      s += p.s;

      return *this;
    }

    path&
    path::operator=(const path& p)
    {
      s = p.s;

      return *this;
    }

    void
    path::clear()
    {
      s.clear();
    }

    void
    path::swap(path& p)
    {
      std::swap(p.s, s);
    }

    std::string
    path::string() const
    {
      return s;
    }

    const char*
    path::c_str() const
    {
      return s.c_str();
    }

    bool
    path::empty() const
    {
      return s.empty();
    }

    path
    path::extension() const
    {
      const std::size_t i = s.find_last_of(preferred_separator);

      const std::size_t j = (i == std::string::npos ? 0 : i + 1);

      if(j < s.length())
      {
        if(s.compare(j, std::string::npos, ".", 1) != 0
           && s.compare(j, std::string::npos, "..", 2) != 0)
        {
          const std::size_t k = s.find_last_of('.');

          if(k != std::string::npos && j <= k)
          {
            return s.substr(k);
          }
        }
      }

      return path();
    }

    path
    path::filename() const
    {
      const std::size_t i = s.find_last_of(preferred_separator);

      if(i == std::string::npos)
      {
        return *this;
      }

      if(i + 1 < s.length())
      {
        return path(s.substr(i + 1));
      }
      else
      {
        return ".";
      }
    }

    path
    path::parent_path() const
    {
      const std::size_t i = s.find_last_of(preferred_separator);

      if(i == std::string::npos)
      {
        return path();
      }

      return path(s.substr(0, i));
    }

    path&
    path::remove_filename()
    {
      const std::size_t i = s.find_last_of(preferred_separator);

      if(i != std::string::npos)
      {
        s.resize(i);
      }

      return *this;
    }

    path&
    path::replace_filename(const path& p)
    {
      const std::size_t i = s.find_last_of(preferred_separator);

      if(i != std::string::npos)
      {
        s.resize(i);
        append(p);
      }
      else
      {
        assign(p);
      }

      return *this;
    }

    path::operator string_type() const
    {
      return s;
    }

    path&
    path::append(const path& p)
    {
      bool sep = true;

      if(!s.empty() && s[s.length() - 1] == preferred_separator)
      {
        sep = false;
      }
      else if(s.empty())
      {
        sep = false;
      }
      else if(p.empty() || p.s[0] == preferred_separator)
      {
        sep = false;
      }

      if(sep)
      {
        s += preferred_separator;
      }

      s.append(p.native());

      return *this;
    }

    path&
    path::operator/=(const path& p)
    {
      return append(p);
    }

    const std::string&
    path::native() const
    {
      return s;
    }

    path
    path::lexically_relative(const path& p) const
    {
      const_iterator first1 = begin(), last1 = end();
      const_iterator first2 = p.begin(), last2 = p.end();

      const_iterator it = first1;
      const_iterator jt = first2;

      while(it != last1 && jt != last2 && *it == *jt)
      {
        ++it, ++jt;
      }

      if(it == first1 || jt == first2)
      {
        return path();
      }

      if(it == last1 && jt == last2)
      {
        return path(".");
      }

      path r;

      for(; jt != last2; ++jt)
      {
        r /= "..";
      }

      for(; it != last1; ++it)
      {
        r /= *it;
      }

      return r;
    }

    bool
    path::is_absolute() const
    {
      return !s.empty() && s[0] == preferred_separator;
    }

    path
    path::lexically_normal() const
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

    int
    path::compare(const path& p) const
    {
      const_iterator first1 = begin(), last1 = end();
      const_iterator first2 = p.begin(), last2 = p.end();

      while(first1 != last1 && first2 != last2)
      {
        if(first1->native() < first2->native())
        {
          return -1;
        }

        if(first2->native() < first1->native())
        {
          return 1;
        }

        ++first1;
        ++first2;
      }

      if(first1 != last1)
      {
        return 1;
      }

      if(first2 != last2)
      {
        return -1;
      }

      return 0;
    }

    path::const_iterator
    path::begin() const
    {
      begin_iterator_tag tag;

      return const_iterator(this, tag);
    }

    path::const_iterator
    path::end() const
    {
      end_iterator_tag tag;

      return const_iterator(this, tag);
    }

    path::const_iterator::const_iterator()
        : parent(0), first(std::string::npos), last(std::string::npos), value()
    {
    }

    path::const_iterator::const_iterator(const path* parent_,
                                         begin_iterator_tag)
        : parent(parent_), first(0)
    {
      if(parent->s.empty())
      {
        first = std::string::npos;
        last  = std::string::npos;
      }
      else
      {
        const std::size_t k = parent->s.find_first_of(preferred_separator);

        if(k == std::string::npos)
        {
          last = parent->s.length();
        }
        else if(k == 0)
        {
          last = 1;
        }
        else
        {
          last = k;
        }

        value = parent->s.substr(0, last);
      }
    }

    path::const_iterator::const_iterator(const path* p, end_iterator_tag)
        : parent(p), first(std::string::npos), last(std::string::npos), value()
    {
    }

    path::const_iterator&
    path::const_iterator::operator++()
    {
      if(parent)
      {
        if(last < parent->s.length())
        {
          // find next component
          const std::size_t j =
              parent->s.find_first_not_of(preferred_separator, last);

          if(j == std::string::npos)
          {
            if(parent->s[first] == preferred_separator)
            {
              // path to root
              first = std::string::npos;
              last  = std::string::npos;
              value.clear();
            }
            else
            {
              // ends with directory
              first = parent->s.length();
              last  = std::string::npos;
              value = ".";
            }
          }
          else
          {
            first = j;

            // next path component
            const std::size_t k =
                parent->s.find_first_of(preferred_separator, j);

            if(k == std::string::npos)
            {
              last = parent->s.length();
            }
            else
            {
              last = k;
            }

            value = parent->s.substr(first, last - first);
          }
        }
        else
        {
          // go to end iterator
          first = std::string::npos;
          last  = std::string::npos;
          value.clear();
        }
      }

      return *this;
    }

    path::const_iterator
    path::const_iterator::operator++(int)
    {
      const_iterator t = *this;

      operator++();
      return t;
    }

    path::const_iterator&
    path::const_iterator::operator--()
    {
      if(parent && first != 0)
      {
        if(first == std::string::npos)
        {
          // end iterator
          if(!parent->s.empty())
          {
            const std::size_t j = parent->s.find_last_of(preferred_separator);

            if(j == std::string::npos)
            {
              // single path component
              first = 0;
              last  = parent->s.length();
              value = parent->s;
            }
            else
            {
              if(j + 1 < parent->s.length())
              {
                // trailing filename
                first = j + 1;
                last  = parent->s.length();
                value = parent->s.substr(first, last - first);
              }
              else
              {
                // trailing directory, or root
                const std::size_t k =
                    parent->s.find_last_not_of(preferred_separator, j);

                if(k == std::string::npos)
                {
                  first = 0;
                  last  = 1;
                  value = parent->s.substr(first, last - first);
                }
                else
                {
                  first = parent->s.length();
                  last  = std::string::npos;
                  value = ".";
                }
              }
            }
          }
        }
        else
        {
          // on an actual path component, or the fake "."
          const std::size_t j =
              parent->s.find_last_not_of(preferred_separator, first - 1);

          if(j == std::string::npos)
          {
            // beginning of absolute path
            first = 0;
            last  = 1;
          }
          else
          {
            last = j + 1;
            const std::size_t k =
                parent->s.find_last_of(preferred_separator, j);

            if(k == std::string::npos)
            {
              first = 0;
            }
            else
            {
              first = k + 1;
            }
          }

          value = parent->s.substr(first, last - first);
        }
      }

      return *this;
    }

    path::const_iterator
    path::const_iterator::operator--(int)
    {
      const_iterator t = *this;

      operator--();
      return t;
    }

    bool
    path::const_iterator::operator==(const const_iterator& o) const
    {
      return parent == o.parent && first == o.first;
    }

    bool
    path::const_iterator::operator!=(const const_iterator& o) const
    {
      return parent != o.parent || first != o.first;
    }

    const path& path::const_iterator::operator*() const
    {
      return value;
    }

    const path* path::const_iterator::operator->() const
    {
      return &value;
    }

    path
    operator/(const path& lhs, const path& rhs)
    {
      path res = lhs;

      res.append(rhs);

      return res;
    }

    std::ostream&
    operator<<(std::ostream& os, const path& p)
    {
      return os << p.string();
    }

    bool
    operator==(const path& l, const path& r)
    {
      return l.compare(r) == 0;
    }

    bool
    operator!=(const path& l, const path& r)
    {
      return l.compare(r) != 0;
    }

    bool
    operator<(const path& l, const path& r)
    {
      return l.compare(r) < 0;
    }

    bool
    operator<=(const path& l, const path& r)
    {
      return l.compare(r) <= 0;
    }

    bool
    operator>(const path& l, const path& r)
    {
      return l.compare(r) > 0;
    }

    bool
    operator>=(const path& l, const path& r)
    {
      return l.compare(r) >= 0;
    }

  }  // namespace filesystem
}  // namespace cpp17

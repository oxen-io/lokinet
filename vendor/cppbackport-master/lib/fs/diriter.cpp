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
#include "diriter.h"

#include <cstdlib>
#include <cstring>
#include <climits>

#include <dirent.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "direntry.h"
#include "path.h"

namespace cpp17
{
  namespace filesystem
  {
    /// @todo Could save the malloc/free (of e) if we mark end-of-directory
    class directory_iterator::impl
    {
     public:
      impl() : p(), d(0), e(0), info(), valid_info(false)
      {
        // printf("directory_iterator::impl::cstr\n");
        pos = 0;
      }

      explicit impl(const path& path_)
          : p(path_), d(0), e(0), info(), valid_info(false)
      {
        // printf("directory_iterator::impl::cstr path[%s]\n", path_.c_str());
        this->cPath = path_;
        d           = ::opendir(path_.c_str());

        if(d)
        {
          e = acquire();
          next();
        }
      }

      ~impl()
      {
        // printf("directory_iterator::impl::dstr [%s]\n", this->cPath.c_str());
        if(d)
        {
          release();  // free e
          ::closedir(d);
          d = 0;
        }
      }

      void
      rewind()
      {
        // printf("directory_iterator::impl::rewind\n");
        if(d)
        {
          release();
          ::closedir(d);
        }
        d   = ::opendir(this->cPath.c_str());
        pos = 0;
        if(d)
        {
          e = acquire();
          next();
        }
      }

      void
      seek(size_t seekPos)
      {
        // printf("directory_iterator::impl::seek\n");
        this->rewind();
        for(size_t i = 0; i < seekPos; i++)
        {
          this->next();
        }
      }

      /** Test if this is an end iterator
       */
      bool
      is_end() const
      {
        // printf("directory_iterator::impl::is_end\n");
        return !e;
      }

      bool
      next()
      {
        // printf("directory_iterator::impl::next\n");
        if(d)
        {
          valid_info = false;

          do
          {
            dirent* ptr = 0;

            const int res = ::readdir_r(d, e, &ptr);

            if(res != 0 || ptr == 0)
            {
              // error, or end of directory
              pos = 0;
              release();

              return false;
            }
          } while(std::strcmp(e->d_name, ".") == 0
                  || std::strcmp(e->d_name, "..") == 0);

          pos++;

          return true;
        }

        return false;
      }

      path
      get_path() const
      {
        // printf("directory_iterator::impl::get_path\n");
        return e ? (p / e->d_name) : path();
      }

      file_type
      type() const
      {
        // printf("directory_iterator::impl::type\n");
        if(e)
        {
          switch(e->d_type)
          {
            case DT_FIFO:

              return file_type::fifo;

            case DT_CHR:

              return file_type::character;

            case DT_DIR:

              return file_type::directory;

            case DT_BLK:

              return file_type::block;

            case DT_REG:

              return file_type::regular;

            case DT_LNK:

              return file_type::symlink;

            case DT_SOCK:

              return file_type::socket;

            default:
              break;
          }  // switch
        }

        return file_type::unknown;
      }

      const directory_entry&
      get_reference()
      {
        // printf("directory_iterator::impl::get_reference\n");
        update();

        return info;
      }

      const directory_entry*
      get_pointer()
      {
        // printf("directory_iterator::impl::get_pointer\n");
        update();

        return &info;
      }

      /// Position
      size_t pos;

      // Path to directory
      path p;

     private:
      void
      update()
      {
        // printf("directory_iterator::impl::update\n");
        if(!valid_info && e)
        {
          const path q = p / e->d_name;

          info.assign(q);
          valid_info = true;
        }
      }

      static dirent*
      acquire()
      {
        // printf("directory_iterator::impl::acquire\n");
        /* dirent::d_name is required to be at least NAME_MAX + 1 bytes.
         * However, some implementations use the struct hack and make d_name
         * 1 byte. Watch for this scenario and adjust accordingly.
         */
        if(sizeof(static_cast< dirent* >(0)->d_name) < NAME_MAX + 1)
        {
          return static_cast< dirent* >(
              ::malloc(sizeof(dirent) + NAME_MAX + 1));
        }
        else
        {
          return static_cast< dirent* >(::malloc(sizeof(dirent)));
        }
      }

      void
      release()
      {
        // printf("directory_iterator::impl::release\n");
        ::free(e);
        e = 0;
      }

      // constructor path
      path cPath;

      /// Platform information
      DIR* d;

      /// Platform information
      dirent* e;

      directory_entry info;

      bool valid_info;  // info has been populated
    };

    directory_iterator::directory_iterator() : pimpl(new impl)
    {
    }

    directory_iterator::directory_iterator(const path& path_)
        : pimpl(new impl(path_))
    {
    }

    // is this right
    directory_iterator::directory_iterator(directory_iterator const& src)
        : pimpl(new impl(src.pimpl->p.c_str()))
    {
      pimpl->seek(src.pimpl->pos);
    }

    directory_iterator::~directory_iterator()
    {
      delete pimpl;
    }

    directory_iterator::directory_iterator(directory_iterator const& src,
                                           int pos)
    {
      // printf("directory_iterator::directory_iterator copy at pos - pimpl[%x]
      // from[%s]\n", pimpl, src->path().c_str());
      pimpl = new impl(src.pimpl->p.c_str());
      pimpl->seek(pos);
    }

    bool
    directory_iterator::operator==(const directory_iterator& i) const
    {
      // only equal if both are end iterators
      return pimpl->is_end() && i.pimpl->is_end();
    }

    bool
    directory_iterator::operator!=(const directory_iterator& i) const
    {
      return !(pimpl->is_end() && i.pimpl->is_end());
    }

    directory_iterator&
    directory_iterator::operator++()
    {
      pimpl->next();

      return *this;
    }

    const directory_entry& directory_iterator::operator*() const
    {
      return pimpl->get_reference();
    }

    const directory_entry* directory_iterator::operator->() const
    {
      return pimpl->get_pointer();
    }

    file_type
    directory_iterator::type() const
    {
      return pimpl->type();
    }

    void
    directory_iterator::swap(directory_iterator& d)
    {
      std::swap(pimpl, d.pimpl);
    }

    // FIXME: this isn't right
    directory_iterator
    directory_iterator::begin()
    {
      return directory_iterator(*this, 0);
      /*
      printf("directory_iterator::directory_iterator::begin - start\n");
      size_t cur = pimpl->pos; // backup state
      printf("directory_iterator::directory_iterator::begin - was at
      [%zu][%s]\n", cur, this->pimpl->get_path().c_str()); pimpl->rewind();
      directory_iterator *ret = new directory_iterator(*this); // copy ourself
      pimpl->seek(cur); // restore state
      printf("directory_iterator::directory_iterator::begin - end\n");
      return *ret;
      */
    }
    directory_iterator
    directory_iterator::end()
    {
      // return *(directory_iterator *)endPtr;
      // printf("directory_iterator::directory_iterator::end - start\n");
      size_t cur = pimpl->pos;  // backup state
      // printf("directory_iterator::directory_iterator::end - was at
      // [%zu][%s]\n", cur, this->pimpl->get_path().c_str());
      // spool to the end
      size_t finalPos = pimpl->pos;
      while(!pimpl->is_end())
      {
        if(pimpl->next())
        {
          finalPos = pimpl->pos;
        }
      }
      // directory_iterator *ret = new directory_iterator(*this); // copy
      // ourself printf("directory_iterator::directory_iterator::end - now at
      // [%zu][%s]\n", pimpl->pos, this->pimpl->get_path().c_str());
      pimpl->seek(cur);  // restore state
      return directory_iterator(*this, finalPos);
      // printf("directory_iterator::directory_iterator::end - end\n");
      // return *ret;
    }

    //
    // recursive_directory_iterator
    //

    recursive_directory_iterator::recursive_directory_iterator()
    {
    }

    recursive_directory_iterator::recursive_directory_iterator(const path& p)
    {
      descend(p);
    }

    recursive_directory_iterator::~recursive_directory_iterator()
    {
      while(!stack.empty())
      {
        ascend();
      }
    }

    bool
    recursive_directory_iterator::descend(const path& p)
    {
      directory_iterator it(p), end;

      if(it != end)
      {
        directory_iterator* jt = new directory_iterator();
        jt->swap(it);
        stack.push(jt);

        return true;
      }

      return false;
    }

    void
    recursive_directory_iterator::ascend()
    {
      delete stack.top();
      stack.pop();
    }

    const directory_entry& recursive_directory_iterator::operator*() const
    {
      return *(*stack.top());
    }

    const directory_entry* recursive_directory_iterator::operator->() const
    {
      return stack.top()->operator->();
    }

    recursive_directory_iterator&
    recursive_directory_iterator::operator++()
    {
      if(stack.top()->type() == file_type::directory)
      {
        // go to directory's first child (if any)
        if(descend((*stack.top())->path()))
        {
          return *this;
        }
      }

      do
      {
        // move to sibling
        stack.top()->operator++();

        directory_iterator end;

        if(*(stack.top()) != end)
        {
          return *this;
        }

        // move to parent
        ascend();
      } while(!stack.empty());

      return *this;
    }

    bool
    recursive_directory_iterator::operator==(
        const recursive_directory_iterator& i) const
    {
      return stack.empty() && i.stack.empty();
    }

    bool
    recursive_directory_iterator::operator!=(
        const recursive_directory_iterator& i) const
    {
      return !(stack.empty() && i.stack.empty());
    }

  }  // namespace filesystem
}  // namespace cpp17

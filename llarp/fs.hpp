#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP
#include <functional>
#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#include "filesystem.h"
#if defined(CPP17) && defined(USE_CXX17_FILESYSTEM)
// win32 is the only one that doesn't use cpp17::filesystem
// because cpp17::filesystem is unimplemented for Windows
// -despair86
#if defined(__MINGW32__) || defined(_MSC_VER) || defined(__sun)
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif  // end win32
#else
// not CPP17 needs this
// openbsd needs this
// linux gcc 7.2 needs this
namespace fs = cpp17::filesystem;
#include <dirent.h>
#endif

namespace llarp
{
  namespace util
  {
    typedef std::function< bool(const fs::path &) > PathVisitor;
    typedef std::function< void(const fs::path &, PathVisitor) > PathIter;
#if defined(USE_CXX17_FILESYSTEM)
    static PathIter IterDir = [](const fs::path &path, PathVisitor visit) {
      fs::directory_iterator i(path);
      auto itr = fs::begin(i);
      while(itr != fs::end(i))
      {
        fs::path p = *itr;
        if(!visit(p))
          return;
        ++itr;
      }
    };
#else
    static PathIter IterDir = [](const fs::path &path, PathVisitor visit) {
      DIR *d = opendir(path.c_str());
      if(d == nullptr)
        return;
      struct dirent *ent = nullptr;
      do
      {
        ent = readdir(d);
        if(!ent)
          break;
        if(ent->d_name[0] == '.')
          continue;
        fs::path p = path / fs::path(ent->d_name);
        if(!visit(p))
          break;
      } while(ent);
      closedir(d);
    };
#endif
  }  // namespace util
}  // namespace llarp
#endif  // end LLARP_FS_HPP

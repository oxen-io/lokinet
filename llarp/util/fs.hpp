#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP
#include <functional>

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#ifdef _WIN32
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include "filesystem.h"
namespace fs = cpp17::filesystem;
#endif

#ifndef _MSC_VER
#include <dirent.h>
#endif

namespace llarp
{
  namespace util
  {
    using PathVisitor = std::function< bool(const fs::path &) >;
    using PathIter    = std::function< void(const fs::path &, PathVisitor) >;

    static PathIter IterDir = [](const fs::path &path, PathVisitor visit) {
#ifdef _MSC_VER
      for(auto &p : fs::directory_iterator(path))
      {
        if(!visit(p.path()))
        {
          break;
        }
      }
#else
      DIR *d = opendir(path.string().c_str());
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
#endif
    };
  }  // namespace util
}  // namespace llarp
#endif  // end LLARP_FS_HPP

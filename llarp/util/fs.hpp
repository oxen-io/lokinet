#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#include <functional>

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#if defined(LOKINET_USE_GHC_FILESYSTEM)
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#elif __cplusplus >= 201703L && !defined(_MSC_VER)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#ifndef _MSC_VER
#include <dirent.h>
#endif

#include <absl/types/optional.h>

namespace llarp
{
  namespace util
  {
    using error_code_t = std::error_code;

    /// Ensure that a file exists and has correct permissions
    /// return any error code or success
    error_code_t
    EnsurePrivateFile(fs::path pathname);

    /// open a stream to a file and ensure it exists before open
    /// sets any permissions on creation
    template < typename T >
    absl::optional< T >
    OpenFileStream(fs::path pathname, std::ios::openmode mode)
    {
      if(EnsurePrivateFile(pathname))
        return {};

      std::string f = pathname.string();
      return T{pathname, mode};
    }

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

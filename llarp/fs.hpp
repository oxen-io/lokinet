#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#if defined(WIN32) || defined(_WIN32)
#  define PATH_SEP "\\"
#else
#  define PATH_SEP "/"
#endif

#if(__cplusplus >= 201703L)
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
  #ifdef __clang__
    #include "filesystem.h"
    namespace fs = cpp17::filesystem;
  #else
    #error "fs support unimplemented"

    #include <string>

    namespace fs
    {
      static std::string Sep = "/";
      struct path
      {
      };
    }

  #endif
#endif

#endif

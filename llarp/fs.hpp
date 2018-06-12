#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#if(__cplusplus >= 201703L)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
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

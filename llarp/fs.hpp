#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#ifndef PATH_SEP
#define PATH_SEP "/"
#endif

#if(__cplusplus >= 201703L)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "fs support unimplemented"
#endif
#endif

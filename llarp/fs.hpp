#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#include "filesystem.h"
namespace fs = cpp17::filesystem;

#endif

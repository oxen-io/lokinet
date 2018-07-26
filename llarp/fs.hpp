#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#include "filesystem.h"
#if !defined(CPP17) || defined(__OpenBSD__)
namespace fs = cpp17::filesystem;
#else
#ifndef __MINGW32__
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif
#endif

#endif

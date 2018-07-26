#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#include "filesystem.h"

// mingw32 in the only one that doesn't use cpp17::filesystem
#if defined(__MINGW32__)
namespace fs = std::experimental::filesystem;
#else
// not CPP17 needs this
// openbsd needs this
// linux gcc 7.2 needs this
namespace fs = cpp17::filesystem;
#endif // end mingw32

#endif // end LLARP_FS_HPP

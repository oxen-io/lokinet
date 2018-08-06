#ifndef LLARP_FS_HPP
#define LLARP_FS_HPP

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
#if defined(__MINGW32__) || defined(_MSC_VER)
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif  // end win32
#else
// not CPP17 needs this
// openbsd needs this
// linux gcc 7.2 needs this
namespace fs = cpp17::filesystem;
#endif

#endif  // end LLARP_FS_HPP

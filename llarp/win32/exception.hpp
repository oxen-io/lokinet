#pragma once

#ifdef _WIN32

#include <stdexcept>
#include <windows.h>

namespace llarp::win32
{
  class error : public std::runtime_error
  {
   public:
    /// construct with error code
    explicit error(DWORD err, std::string msg);
    /// construct with last error
    explicit error(std::string msg = "");
  };

}  // namespace llarp::win32
#endif

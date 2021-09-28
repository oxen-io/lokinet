#pragma once

#ifdef _WIN32

#include <stdexcept>
#include <windows.h>

namespace llarp::win32
{
  class error : public std::runtime_error
  {
   public:
    explicit error(DWORD err, std::string msg);
  };

  /// @brief throws an exception with the last error from win32 api
  class last_error : public error
  {
   public:
    explicit last_error(std::string msg = "");
  };

}  // namespace llarp::win32
#endif

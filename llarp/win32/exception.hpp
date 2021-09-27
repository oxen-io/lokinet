#pragma once

#ifdef _WIN32

#include <stdexcept>

namespace llarp::win32
{
  /// @brief throws an exception with the last error from win32 api
  class last_error : public std::runtime_error
  {
   public:
    last_error();
  };

}  // namespace llarp::win32
#endif

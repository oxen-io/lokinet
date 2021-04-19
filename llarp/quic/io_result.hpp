#pragma once

#include <cstring>
#include <cerrno>
#include <string_view>

namespace llarp::quic
{
  // Return result from a read or write operation that wraps an errno value. It is implicitly
  // convertible to bool to test for "is not an error" (which is the inverse of casting a plain
  // integer error code value to bool).
  struct io_result
  {
    // An error code, typically an errno value
    int error_code{0};
    // Returns true if this represent a successful result, i.e. an error_code of 0.
    operator bool() const
    {
      return error_code == 0;
    }

    // Returns true if this is an error value indicating a failure to write without blocking (only
    // applied to io_result's capturing an errno).
    bool
    blocked() const
    {
      return error_code == EAGAIN || error_code == EWOULDBLOCK;
    }

    // Returns the errno string for the given error code.
    std::string_view
    str() const
    {
      return strerror(error_code);
    }
  };

}  // namespace llarp::quic

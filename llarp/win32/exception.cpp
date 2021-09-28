#ifdef _WIN32
#include "exception.hpp"
#include <windows.h>
#include <string>

namespace llarp::win32
{
  static std::string
  errorMessage(DWORD error)
  {
    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us
    // (because we don't yet know how long the message string will be).
    const size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        nullptr);

    // Copy the error message into a std::string.
    std::string message{messageBuffer, size};

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
  }

  error::error(DWORD code, std::string msg) : std::runtime_error{msg + errorMessage(code)}
  {}

  last_error::last_error(std::string msg) : error{GetLastError(), msg}
  {}

}  // namespace llarp::win32
#endif

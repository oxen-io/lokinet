#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>

namespace llarp::win32
{
  std::string
  error_to_string(DWORD err);

  class error : public std::runtime_error
  {
   public:
    error(std::string msg);
    virtual ~error() = default;
    explicit error(DWORD err, std::string msg);
  };
}  // namespace llarp::win32

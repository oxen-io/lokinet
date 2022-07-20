#pragma once

#include <array>
#include <string>
#include <windows.h>
#include <stdexcept>

#include <llarp/util/str.hpp>

namespace llarp::win32
{
  namespace
  {
    inline std::string
    error_to_string(DWORD err)
    {
      std::array<CHAR, 512> buffer{};

      FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, &err, 0, 0, buffer.data(), buffer.size(), nullptr);
      return std::string{buffer.data()};
    }
  }  // namespace

  class error : public std::runtime_error
  {
   public:
    error(DWORD err, std::string msg)
        : std::runtime_error{fmt::format("{}: {}", msg, error_to_string(err))}
    {}
  };
}  // namespace llarp::win32

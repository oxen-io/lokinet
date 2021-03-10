#pragma once

#include <cstdarg>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <type_traits>

// Temporary logging code to be replaced with lokinet logging

#include <oxenmq/hex.h>

#ifdef __cpp_lib_source_location
#include <source_location>
namespace slns = std;
#else
#include <experimental/source_location>
namespace slns = std::experimental;
#endif

namespace llarp::quic
{
  struct buffer_printer
  {
    std::basic_string_view<std::byte> buf;

    template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
    explicit buffer_printer(std::basic_string_view<T> buf)
        : buf{reinterpret_cast<const std::byte*>(buf.data()), buf.size()}
    {}

    template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
    explicit buffer_printer(const std::basic_string<T>& buf)
        : buffer_printer(std::basic_string_view<T>{buf})
    {}

    template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
    explicit buffer_printer(std::basic_string<T>&& buf) = delete;

    template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
    explicit buffer_printer(const T* data, size_t size)
        : buffer_printer(std::basic_string_view<T>{data, size})
    {}
  };
  std::ostream&
  operator<<(std::ostream& o, const buffer_printer& bp);

  namespace detail
  {
    template <typename T, typename... V>
    constexpr bool is_same_any_v = (std::is_same_v<T, V> || ...);

    template <typename T, typename... More>
    void
    log_print_vals(T&& val, More&&... more)
    {
      using PlainT = std::remove_reference_t<T>;
      if constexpr (is_same_any_v<PlainT, char, unsigned char, signed char, uint8_t, std::byte>)
        std::cerr
            << +val;  // Promote chars to int so that they get printed as numbers, not literal chars
      else
        std::cerr << val;
      if constexpr (sizeof...(More))
        log_print_vals(std::forward<More>(more)...);
    }

    template <typename... T>
    void
    log_print(const slns::source_location& location, T&&... args)
    {
      std::string_view filename{location.file_name()};
      if (auto pos = filename.rfind('/'); pos != std::string::npos
          && (pos = filename.substr(0, pos).rfind('/')) != std::string::npos)
      {
        filename.remove_prefix(pos + 1);
      }
      std::cerr << "\e[3m[" << filename << ':' << location.line() << "]\e[23m";
      if constexpr (sizeof...(T))
      {
        std::cerr << ": ";
        detail::log_print_vals(std::forward<T>(args)...);
      }
      std::cerr << '\n';
    }

  }  // namespace detail

#ifndef NDEBUG
  template <typename... T>
  struct Debug
  {
    Debug(T&&... args, const slns::source_location& location = slns::source_location::current())
    {
      std::cerr << "DBG";
      detail::log_print(location, std::forward<T>(args)...);
    }
  };
  template <typename... T>
  Debug(T&&...) -> Debug<T...>;
#else
  template <typename... T>
  void
  Debug(T&&...)
  {}
#endif

  template <typename... T>
  struct Info
  {
    Info(T&&... args, const slns::source_location& location = slns::source_location::current())
    {
      std::cerr << "\e[32mNFO";
      detail::log_print(location, std::forward<T>(args)...);
      std::cerr << "\e[0m";
    }
  };
  template <typename... T>
  Info(T&&...) -> Info<T...>;

  template <typename... T>
  struct Warn
  {
    Warn(T&&... args, const slns::source_location& location = slns::source_location::current())
    {
      std::cerr << "\e[33;1mWRN";
      detail::log_print(location, std::forward<T>(args)...);
      std::cerr << "\e[0m";
    }
  };
  template <typename... T>
  Warn(T&&...) -> Warn<T...>;

  template <typename... T>
  struct Error
  {
    Error(T&&... args, const slns::source_location& location = slns::source_location::current())
    {
      std::cerr << "\e[31;1mWRN";
      detail::log_print(location, std::forward<T>(args)...);
      std::cerr << "\e[0m";
    }
  };
  template <typename... T>
  Error(T&&...) -> Error<T...>;

}  // namespace llarp::quic

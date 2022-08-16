#pragma once

/// namespace for platform feature detection constexprs
namespace llarp::platform
{
  ///  are we linux  ?
  inline constexpr bool is_linux =
#ifdef __linux__
      true
#else
      false
#endif
      ;

  ///  are we freebsd ?
  inline constexpr bool is_freebsd =
#ifdef __FreeBSD__
      true
#else
      false
#endif
      ;

  /// are we windows ?
  inline constexpr bool is_windows =
#ifdef _WIN32
      true
#else
      false
#endif
      ;

  /// are we an apple platform ?
  inline constexpr bool is_apple =
#ifdef __APPLE__
      true
#else
      false
#endif
      ;

  /// are we an android platform ?
  inline constexpr bool is_android =
#ifdef ANDROID
      true
#else
      false
#endif
      ;

  /// are we an iphone ?
  inline constexpr bool is_iphone =
#ifdef IOS
      true
#else
      false
#endif
      ;

  /// are we a mobile phone ?
  inline constexpr bool is_mobile = is_android or is_iphone;

  /// does this platform support native ipv6 ?
  inline constexpr bool supports_ipv6 = not is_windows;
}  // namespace llarp::platform

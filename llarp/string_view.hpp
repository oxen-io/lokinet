#ifndef LLARP_STRING_VIEW_HPP
#define LLARP_STRING_VIEW_HPP

#if __cplusplus >= 201703L
#include <string_view>
#include <string>
namespace llarp
{
  using string_view = std::string_view;
  static std::string
  string_view_string(const string_view& v)
  {
    return std::string(v.data(), v.size());
  }
}  // namespace llarp
#else
#include <string>
namespace llarp
{
  using string_view = std::string;

  static std::string
  string_view_string(const string_view& v)
  {
    return v;
  };
}  // namespace llarp
#endif
#endif

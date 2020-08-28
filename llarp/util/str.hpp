#ifndef LLARP_STR_HPP
#define LLARP_STR_HPP

#include <string_view>
#include <sstream>
#include <vector>

namespace llarp
{
  bool
  IsFalseValue(std::string_view str);

  struct CaselessLessThan
  {
    bool
    operator()(std::string_view lhs, std::string_view rhs) const;
  };

  bool
  IsTrueValue(std::string_view str);

  /// Trim leading and trailing (ascii) whitespace from the given string;
  /// returns a std::string_view of the trimmed part of the string.
  [[nodiscard]] std::string_view
  TrimWhitespace(std::string_view str);

  template <typename... T>
  std::string
  stringify(T&&... stuff)
  {
    std::ostringstream o;
    (o << ... << std::forward<T>(stuff));
    return o.str();
  }

  /// Split a string on a given delimiter
  //
  /// @param str is the string to split
  /// @param delimiter is the character to split on
  /// @return a vector of std::string_views with the split words, excluding the delimeter
  std::vector<std::string_view>
  split(const std::string_view str, char delimiter);

}  // namespace llarp

#endif

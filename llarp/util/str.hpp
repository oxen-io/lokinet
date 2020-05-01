#ifndef LLARP_STR_HPP
#define LLARP_STR_HPP

#include <string_view>
#include <sstream>
#include <vector>

namespace llarp
{
  bool
  StrEq(const char* s1, const char* s2);

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
#ifdef __GNUG__
  [[gnu::warn_unused_result]]
#endif
  std::string_view
  TrimWhitespace(std::string_view str);

  template <typename... T>
  std::string
  stringify(T&&... stuff)
  {
    std::ostringstream o;
#ifdef __cpp_fold_expressions
    (o << ... << std::forward<T>(stuff));
#else
    (void)std::initializer_list<int>{(o << std::forward<T>(stuff), 0)...};
#endif
    return o.str();
  }

  // Shortcut for explicitly casting a string_view to a string.  Saves 8 characters compared to
  // `std::string(view)`.
  inline std::string str(std::string_view s) {
    return std::string{s};
  }

  /// Split a string on a given delimiter
  //
  /// @param str is the string to split
  /// @param delimiter is the character to split on
  /// @return a vector of std::string_views with the split words, excluding the delimeter
  std::vector<std::string_view>
  split(std::string_view str, char delimiter);

}  // namespace llarp

#endif

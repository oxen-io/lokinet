#ifndef LLARP_STR_HPP
#define LLARP_STR_HPP

#include <util/string_view.hpp>
#include <sstream>
#include <vector>

namespace llarp
{
  bool
  StrEq(const char* s1, const char* s2);

  bool
  IsFalseValue(string_view str);

  struct CaselessLessThan
  {
    bool
    operator()(string_view lhs, string_view rhs) const;
  };

  bool
  IsTrueValue(string_view str);

  /// Trim leading and trailing (ascii) whitespace from the given string;
  /// returns a string_view of the trimmed part of the string.
#ifdef __GNUG__
  [[gnu::warn_unused_result]]
#endif
  string_view
  TrimWhitespace(string_view str);

  template<typename... T>
  std::string stringify(T&&... stuff)
  {
    std::ostringstream o;
#ifdef __cpp_fold_expressions 
    (o << ... << std::forward<T>(stuff));
#else
    (void) std::initializer_list<int>{(o << std::forward<T>(stuff), 0)...};
#endif
    return o.str();
  }

  /// Split a string on a given delimiter
  //
  /// @param str is the string to split
  /// @param delimiter is the character to split on
  /// @return a vector of string_views with the split words, excluding the delimeter
  std::vector<string_view>
  split(string_view str, char delimiter);

}  // namespace llarp

#endif

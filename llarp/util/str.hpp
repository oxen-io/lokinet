#ifndef LLARP_STR_HPP
#define LLARP_STR_HPP

#include <util/string_view.hpp>

namespace llarp
{
  bool
  StrEq(const char *s1, const char *s2);

  bool
  IsFalseValue(string_view str);

  struct CaselessLessThan
  {
    bool
    operator()(string_view lhs, string_view rhs) const;
  };

  bool
  IsTrueValue(string_view str);

  /// Trim leading and trailing (ascii) whitespace from the given string; the
  /// string is modified in-place.
  void
  TrimWhiteSpace(std::string &str);

}  // namespace llarp

#endif

#ifndef LLARP_STR_HPP
#define LLARP_STR_HPP

namespace llarp
{
  bool
  StrEq(const char *s1, const char *s2);

  bool
  IsFalseValue(const char *str);

  bool
  IsTrueValue(const char *str);

}  // namespace llarp

#endif

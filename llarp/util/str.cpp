#include <util/str.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <set>

namespace llarp
{
  bool
  CaselessLessThan::operator()(std::string_view lhs, std::string_view rhs) const
  {
    const size_t s = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < s; ++i)
    {
      auto l = std::tolower(lhs[i]);
      auto r = std::tolower(rhs[i]);

      if (l < r)
      {
        return true;
      }
      if (l > r)
      {
        return false;
      }
    }

    return lhs.size() < rhs.size();
  }

  bool
  IsFalseValue(std::string_view str)
  {
    static const std::set<std::string_view, CaselessLessThan> vals{"no", "false", "0", "off"};

    return vals.count(str) > 0;
  }

  bool
  IsTrueValue(std::string_view str)
  {
    static const std::set<std::string_view, CaselessLessThan> vals{"yes", "true", "1", "on"};

    return vals.count(str) > 0;
  }

  bool
  StrEq(const char* s1, const char* s2)
  {
    size_t sz1 = strlen(s1);
    size_t sz2 = strlen(s2);
    if (sz1 == sz2)
    {
      return strncmp(s1, s2, sz1) == 0;
    }

    return false;
  }

  constexpr static char whitespace[] = " \t\n\r\f\v";

  std::string_view
  TrimWhitespace(std::string_view str)
  {
    size_t begin = str.find_first_not_of(whitespace);
    if (begin == std::string_view::npos)
    {
      str.remove_prefix(str.size());
      return str;
    }
    str.remove_prefix(begin);

    size_t end = str.find_last_not_of(whitespace);
    if (end != std::string_view::npos)
      str.remove_suffix(str.size() - end - 1);

    return str;
  }

  std::vector<std::string_view>
  split(const std::string_view str, char delimiter)
  {
    std::vector<std::string_view> splits;
    const auto str_size = str.size();
    size_t last = 0;
    size_t next = 0;
    while (last < str_size and next < std::string_view::npos)
    {
      next = str.find_first_of(delimiter, last);
      if (next > last)
      {
        splits.push_back(str.substr(last, next - last));

        last = next;

        // advance to next non-delimiter
        while (str[last] == delimiter and last < str_size)
          last++;
      }
      else
      {
        break;
      }
    }

    return splits;
  }

}  // namespace llarp

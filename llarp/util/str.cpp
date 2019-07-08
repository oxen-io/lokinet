#include <util/str.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <set>

namespace llarp
{
  bool
  CaselessCmp::operator()(string_view lhs, string_view rhs) const
  {
    if(lhs.size() < rhs.size())
    {
      return true;
    }
    else if(lhs.size() > rhs.size())
    {
      return false;
    }
    else
    {
      for(size_t i = 0; i < lhs.size(); ++i)
      {
        auto l = std::tolower(lhs[i]);
        auto r = std::tolower(rhs[i]);

        if(l < r)
        {
          return true;
        }
        else if(l > r)
        {
          return false;
        }
      }
      return false;
    }
  }

  bool
  IsFalseValue(string_view str)
  {
    static const std::set< string_view, CaselessCmp > vals{"no", "false", "0",
                                                           "off"};

    return vals.count(str) > 0;
  }

  bool
  IsTrueValue(string_view str)
  {
    static const std::set< string_view, CaselessCmp > vals{"yes", "true", "1",
                                                           "on"};

    return vals.count(str) > 0;
  }

  bool
  StrEq(const char* s1, const char* s2)
  {
    size_t sz1 = strlen(s1);
    size_t sz2 = strlen(s2);
    if(sz1 == sz2)
    {
      return strncmp(s1, s2, sz1) == 0;
    }
    else
      return false;
  }
}  // namespace llarp

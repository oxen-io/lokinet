#include "str.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cassert>
#include <string>
#include <set>

#ifdef _WIN32
#include <windows.h>
#include <stringapiset.h>
#include <llarp/win32/exception.hpp>
#endif

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

  using namespace std::literals;

  std::vector<std::string_view>
  split(std::string_view str, const std::string_view delim, bool trim)
  {
    std::vector<std::string_view> results;
    // Special case for empty delimiter: splits on each character boundary:
    if (delim.empty())
    {
      results.reserve(str.size());
      for (size_t i = 0; i < str.size(); i++)
        results.emplace_back(str.data() + i, 1);
      return results;
    }

    for (size_t pos = str.find(delim); pos != std::string_view::npos; pos = str.find(delim))
    {
      if (!trim || !results.empty() || pos > 0)
        results.push_back(str.substr(0, pos));
      str.remove_prefix(pos + delim.size());
    }
    if (!trim || str.size())
      results.push_back(str);
    else
      while (!results.empty() && results.back().empty())
        results.pop_back();
    return results;
  }

  std::vector<std::string_view>
  split_any(std::string_view str, const std::string_view delims, bool trim)
  {
    if (delims.empty())
      return split(str, delims, trim);
    std::vector<std::string_view> results;
    for (size_t pos = str.find_first_of(delims); pos != std::string_view::npos;
         pos = str.find_first_of(delims))
    {
      if (!trim || !results.empty() || pos > 0)
        results.push_back(str.substr(0, pos));
      size_t until = str.find_first_not_of(delims, pos + 1);
      if (until == std::string_view::npos)
        str.remove_prefix(str.size());
      else
        str.remove_prefix(until);
    }
    if (!trim || str.size())
      results.push_back(str);
    else
      while (!results.empty() && results.back().empty())
        results.pop_back();
    return results;
  }

  void
  trim(std::string_view& s)
  {
    constexpr auto simple_whitespace = " \t\r\n"sv;
    auto pos = s.find_first_not_of(simple_whitespace);
    if (pos == std::string_view::npos)
    {  // whole string is whitespace
      s.remove_prefix(s.size());
      return;
    }
    s.remove_prefix(pos);
    pos = s.find_last_not_of(simple_whitespace);
    assert(pos != std::string_view::npos);
    s.remove_suffix(s.size() - (pos + 1));
  }

  std::string
  lowercase_ascii_string(std::string src)
  {
    for (char& ch : src)
      if (ch >= 'A' && ch <= 'Z')
        ch = ch + ('a' - 'A');
    return src;
  }

  std::string
  friendly_duration(std::chrono::nanoseconds dur)
  {
    const double dsecs = std::chrono::duration<double>(dur).count();
    return fmt::format(
        dur >= 24h        ? "{0}d{1}h{2}m{3}s"
            : dur >= 1h   ? "{1}h{2}m{3}s"
            : dur >= 1min ? "{2}m{3}s"
            : dur >= 1s   ? "{4:.3f}s"
            : dur >= 1ms  ? "{5:.3f}s"
            : dur >= 1us  ? u8"{6:.3f}µs"
                          : "{7}ns",
        dur / 24h,
        dur / 1h,
        dur / 1min,
        dur / 1s,
        dsecs,
        dsecs * 1'000,
        dsecs * 1'000'000,
        dur.count());
  }

  std::wstring
  to_wide(std::string data)
  {
    std::wstring buf;
    buf.resize(data.size());
#ifdef _WIN32
    // win32 specific codepath because balmer made windows so that man may suffer
    if (MultiByteToWideChar(CP_UTF8, 0, data.c_str(), data.size(), buf.data(), buf.size()) == 0)
      throw win32::error{GetLastError(), "failed to convert string to wide string"};

#else
    // this dumb but probably works (i guess?)
    std::transform(
        data.begin(), data.end(), buf.begin(), [](const auto& ch) -> wchar_t { return ch; });
#endif
    return buf;
  }

}  // namespace llarp

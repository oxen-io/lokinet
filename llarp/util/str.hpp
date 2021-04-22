#pragma once

#include <string_view>
#include <sstream>
#include <vector>
#include <chrono>
#include <iterator>
#include <charconv>

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

  using namespace std::literals;

  /// Returns true if the first string is equal to the second string, compared case-insensitively.
  inline bool
  string_iequal(std::string_view s1, std::string_view s2)
  {
    return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(), [](char a, char b) {
      return std::tolower(static_cast<unsigned char>(a))
          == std::tolower(static_cast<unsigned char>(b));
    });
  }

  /// Returns true if the first string matches any of the given strings case-insensitively.
  /// Arguments must be string literals, std::string, or std::string_views
  template <typename S1, typename... S>
  bool
  string_iequal_any(const S1& s1, const S&... s)
  {
    return (... || string_iequal(s1, s));
  }

  /// Returns true if the first argument begins with the second argument
  inline bool
  starts_with(std::string_view str, std::string_view prefix)
  {
    return str.substr(0, prefix.size()) == prefix;
  }

  /// Returns true if the first argument ends with the second argument
  inline bool
  ends_with(std::string_view str, std::string_view suffix)
  {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
  }

  /// Splits a string on some delimiter string and returns a vector of string_view's pointing into
  /// the pieces of the original string.  The pieces are valid only as long as the original string
  /// remains valid.  Leading and trailing empty substrings are not removed.  If delim is empty you
  /// get back a vector of string_views each viewing one character.  If `trim` is true then leading
  /// and trailing empty values will be suppressed.
  ///
  ///     auto v = split("ab--c----de", "--"); // v is {"ab", "c", "", "de"}
  ///     auto v = split("abc", ""); // v is {"a", "b", "c"}
  ///     auto v = split("abc", "c"); // v is {"ab", ""}
  ///     auto v = split("abc", "c", true); // v is {"ab"}
  ///     auto v = split("-a--b--", "-"); // v is {"", "a", "", "b", "", ""}
  ///     auto v = split("-a--b--", "-", true); // v is {"a", "", "b"}
  ///
  std::vector<std::string_view>
  split(std::string_view str, std::string_view delim, bool trim = false);

  /// Splits a string on any 1 or more of the given delimiter characters and returns a vector of
  /// string_view's pointing into the pieces of the original string.  If delims is empty this works
  /// the same as split().  `trim` works like split (suppresses leading and trailing empty string
  /// pieces).
  ///
  ///     auto v = split_any("abcdedf", "dcx"); // v is {"ab", "e", "f"}
  std::vector<std::string_view>
  split_any(std::string_view str, std::string_view delims, bool trim = false);

  /// Joins [begin, end) with a delimiter and returns the resulting string.  Elements can be
  /// anything that can be sent to an ostream via `<<`.
  template <typename It>
  std::string
  join(std::string_view delimiter, It begin, It end)
  {
    std::ostringstream o;
    if (begin != end)
      o << *begin++;
    while (begin != end)
      o << delimiter << *begin++;
    return o.str();
  }

  /// Wrapper around the above that takes a container and passes c.begin(), c.end() to the above.
  template <typename Container>
  std::string
  join(std::string_view delimiter, const Container& c)
  {
    return join(delimiter, c.begin(), c.end());
  }

  /// Simple version of whitespace trimming: mutates the given string view to remove leading
  /// space, \t, \r, \n.  (More exotic and locale-dependent whitespace is not removed).
  void
  trim(std::string_view& s);

  /// Parses an integer of some sort from a string, requiring that the entire string be consumed
  /// during parsing.  Return false if parsing failed, sets `value` and returns true if the entire
  /// string was consumed.
  template <typename T>
  bool
  parse_int(const std::string_view str, T& value, int base = 10)
  {
    T tmp;
    auto* strend = str.data() + str.size();
    auto [p, ec] = std::from_chars(str.data(), strend, tmp, base);
    if (ec != std::errc() || p != strend)
      return false;
    value = tmp;
    return true;
  }

  std::string
  lowercase_ascii_string(std::string src);

  /// Converts a duration into a human friendlier string.
  std::string
  friendly_duration(std::chrono::nanoseconds dur);

}  // namespace llarp

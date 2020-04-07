#ifndef ABYSS_HTTP_HPP
#define ABYSS_HTTP_HPP
#include <util/json.hpp>
#include <util/string_view.hpp>

#include <string>
#include <unordered_map>

namespace abyss
{
  namespace http
  {
    struct RequestHeader
    {
      using Headers_t = std::unordered_multimap<std::string, std::string>;
      Headers_t Headers;
      std::string Method;
      std::string Path;
    };

    struct HeaderReader
    {
      using string_view = llarp::string_view;

      RequestHeader Header;
      virtual ~HeaderReader()
      {
      }

      bool
      ProcessHeaderLine(string_view line, bool& done);

      virtual bool
      ShouldProcessHeader(const string_view& line) const = 0;
    };

  }  // namespace http
}  // namespace abyss
#endif

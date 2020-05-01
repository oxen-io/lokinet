#ifndef ABYSS_HTTP_HPP
#define ABYSS_HTTP_HPP
#include <util/json.hpp>

#include <string>
#include <string_view>
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
      RequestHeader Header;
      virtual ~HeaderReader()
      {
      }

      bool
      ProcessHeaderLine(std::string_view line, bool& done);

      virtual bool
      ShouldProcessHeader(std::string_view line) const = 0;
    };

  }  // namespace http
}  // namespace abyss
#endif

#ifndef __ABYSS_HTTP_HPP__
#define __ABYSS_HTTP_HPP__
#include <string>
#include <unordered_map>
#include <llarp/string_view.hpp>
#include <abyss/json.hpp>

namespace abyss
{
  namespace http
  {
    struct RequestHeader
    {
      typedef std::unordered_multimap< std::string, std::string > Headers_t;
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
      ProcessHeaderLine(abyss::string_view line, bool& done);

      virtual bool
      ShouldProcessHeader(const abyss::string_view& line) const
      {
        return true;
      }
    };

  }  // namespace http
}  // namespace abyss
#endif
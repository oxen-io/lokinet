#include <abyss/http.hpp>
#include <algorithm>
namespace abyss
{
  namespace http
  {
    bool
    HeaderReader::ProcessHeaderLine(string_view line, bool& done)
    {
      if (line.size() == 0)
      {
        done = true;
        return true;
      }
      auto idx = line.find_first_of(':');
      if (idx == string_view::npos)
        return false;
      string_view header = line.substr(0, idx);
      string_view val = line.substr(1 + idx);
      // to lowercase
      std::string lowerHeader;
      lowerHeader.reserve(header.size());
      auto itr = header.begin();
      while (itr != header.end())
      {
        lowerHeader += std::tolower(*itr);
        ++itr;
      }
      if (ShouldProcessHeader(lowerHeader))
      {
        val = val.substr(val.find_first_not_of(' '));
        // llarp::str() here for gcc 5 compat
        Header.Headers.emplace(std::move(lowerHeader), llarp::str(val));
      }
      return true;
    }
  }  // namespace http
}  // namespace abyss

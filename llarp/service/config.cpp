#include <service/config.hpp>

#include <ini.hpp>

namespace llarp
{
  namespace service
  {
    bool
    Config::Load(const std::string& fname)
    {
      ini::Parser parser(fname);
      for(const auto& sec : parser.top().ordered_sections)
      {
        services.push_back({sec->first, sec->second.values});
      }
      return services.size() > 0;
    }

  }  // namespace service
}  // namespace llarp

#include <service/config.hpp>

#include <config/ini.hpp>

namespace llarp
{
  namespace service
  {
    bool
    Config::Load(string_view fname)
    {
      ConfigParser parser;
      if (!parser.LoadFile(fname))
        return false;
      parser.IterAll([&](string_view name, const ConfigParser::Section_t& section) {
        Config::section_t values;
        values.first.assign(name.begin(), name.end());
        for (const auto& item : section)
          values.second.emplace_back(item.first, item.second);
        services.emplace_back(values);
      });
      return services.size() > 0;
    }

  }  // namespace service
}  // namespace llarp

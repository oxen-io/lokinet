#include <service/config.hpp>

#include <util/ini.hpp>

namespace llarp
{
  namespace service
  {
    bool
    Config::Load(const std::string& fname)
    {
      llarp::ConfigParser parser;
      if(!parser.LoadFile(fname.c_str()))
        return false;
      parser.IterAll([&](const llarp::ConfigParser::String_t& name,
                         const llarp::ConfigParser::Section_t& section) {
        llarp::service::Config::section_t values;
        values.first.assign(name.begin(), name.end());
        for(const auto& item : section)
          values.second.emplace_back(string_view_string(item.first), string_view_string(item.second));
        services.emplace_back(values);
      });
      return services.size() > 0;
    }

  }  // namespace service
}  // namespace llarp

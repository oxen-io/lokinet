#ifndef LOKINET_BOOTSERV_CONFIG_HPP
#define LOKINET_BOOTSERV_CONFIG_HPP
#include <unordered_map>
#include <util/string_view.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace llarp
{
  struct ConfigParser
  {
    using String_t      = llarp::string_view;
    using Section_t     = std::unordered_multimap< String_t, String_t >;
    using Config_impl_t = std::unordered_map< String_t, Section_t >;
    /// clear parser
    void
    Clear();

    /// load config file for bootserv
    /// return true on success
    /// return false on error
    bool
    LoadFile(const char* fname);

    /// load from string
    /// return true on success
    /// return false on error
    bool
    LoadString(const std::string& str);

    /// iterate all sections and thier values
    void
    IterAll(std::function< void(const String_t&, const Section_t&) > visit);

    /// visit a section in config read only by name
    /// return false if no section or value propagated from visitor
    bool
    VisitSection(const char* name,
                 std::function< bool(const Section_t&) > visit) const;

   private:
    bool
    Parse();

    std::vector< char > m_Data;
    Config_impl_t m_Config;
  };

}  // namespace llarp

#endif

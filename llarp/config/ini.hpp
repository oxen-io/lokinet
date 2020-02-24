#ifndef LOKINET_BOOTSERV_CONFIG_HPP
#define LOKINET_BOOTSERV_CONFIG_HPP

#include <util/string_view.hpp>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace llarp
{
  struct ConfigParser
  {
    using Section_t     = std::unordered_multimap< std::string, std::string >;
    using Config_impl_t = std::unordered_map< std::string, Section_t >;
    /// clear parser
    void
    Clear();

    /// load config file for bootserv
    /// return true on success
    /// return false on error
    bool
    LoadFile(string_view fname);

    /// load from string
    /// return true on success
    /// return false on error
    bool
    LoadFromStr(string_view str);

    /// iterate all sections and thier values
    void
    IterAll(std::function< void(string_view, const Section_t&) > visit);

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
    std::string m_FileName;
  };

}  // namespace llarp

#endif

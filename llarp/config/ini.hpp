#ifndef LOKINET_BOOTSERV_CONFIG_HPP
#define LOKINET_BOOTSERV_CONFIG_HPP

#include <string_view>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace llarp
{
  struct ConfigParser
  {
    using SectionValues_t = std::unordered_multimap<std::string, std::string>;
    using Config_impl_t = std::unordered_map<std::string, SectionValues_t>;
    /// clear parser
    void
    Clear();

    /// load config file for bootserv
    /// return true on success
    /// return false on error
    bool
    LoadFile(std::string_view fname);

    /// load from string
    /// return true on success
    /// return false on error
    bool
    LoadFromStr(std::string_view str);

    /// iterate all sections and thier values
    void
    IterAll(std::function<void(std::string_view, const SectionValues_t&)> visit);

    /// visit a section in config read only by name
    /// return false if no section or value propagated from visitor
    bool
    VisitSection(const char* name, std::function<bool(const SectionValues_t&)> visit) const;

   private:
    bool
    Parse();

    std::vector<char> m_Data;
    Config_impl_t m_Config;
    std::string m_FileName;
  };

}  // namespace llarp

#endif

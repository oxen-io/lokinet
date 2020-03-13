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
    using SectionValues_t = std::unordered_multimap< std::string, std::string >;
    using Config_impl_t = std::unordered_map< std::string, SectionValues_t >;
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
    IterAll(std::function< void(string_view, const SectionValues_t&) > visit);

    /// visit a section in config read only by name
    /// return false if no section or value propagated from visitor
    bool
    VisitSection(const char* name,
                 std::function< bool(const SectionValues_t&) > visit) const;

    /// Obtain a section value for the given key, additionally imposing the
    /// provided constraints. an `invalid_argument` will be thrown if the
    /// constraints aren't met.
    /// 
    /// The `section` parameter is redundant and added for readability, but a call to
    /// m_Config[section] should result in the same object as `values`.
    /// 
    /// @param values is the SectionValues map in which to search for values
    /// @param section should correspond to INI section tag related to this config
    /// @param key is the key to look up
    /// @param bool constrains whether this key must exist
    /// @param tolerateMultiples constrains whether multiples are allowed
    /// @return the first matching entry if found or empty string if not found
    /// @throws std::invalid_argument if constrants aren't met or if `section` is not found
    const std::string&
    getSingleSectionValue(
        const SectionValues_t& values,
        const std::string& section,
        const std::string& key,
        bool required,
        bool tolerateMultiples = false) const;

   private:
    bool
    Parse();

    std::vector<char> m_Data;
    Config_impl_t m_Config;
    std::string m_FileName;
  };

}  // namespace llarp

#endif

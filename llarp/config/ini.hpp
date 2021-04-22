#pragma once

#include <string_view>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <llarp/util/fs.hpp>

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
    LoadFile(const fs::path fname);

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

    /// add a config option that is appended in another file
    void
    AddOverride(fs::path file, std::string section, std::string key, std::string value);

    /// save config overrides
    void
    Save();

   private:
    bool
    Parse();

    std::vector<char> m_Data;
    Config_impl_t m_Config;
    std::unordered_map<fs::path, Config_impl_t, util::FileHash> m_Overrides;
    fs::path m_FileName;
  };

}  // namespace llarp

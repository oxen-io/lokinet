#pragma once

#include <llarp/util/file.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
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
    LoadFile(const fs::path& fname);

    /// load new .ini file from string (calls ParseAll() rather than Parse())
    /// return true on success
    /// return false on error
    bool
    LoadNewFromStr(std::string_view str);

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

    /// save new .ini config file to path
    void
    SaveNew() const;

    inline void
    Filename(fs::path f)
    {
      m_FileName = f;
    };

   private:
    bool
    ParseAll();

    bool
    Parse();

    std::string m_Data;
    Config_impl_t m_Config;
    std::unordered_map<fs::path, Config_impl_t, util::FileHash> m_Overrides;
    fs::path m_FileName;
  };

}  // namespace llarp

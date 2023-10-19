#include "ini.hpp"

#include <llarp/util/logging.hpp>
#include <llarp/util/formattable.hpp>

#include <cctype>
#include <fstream>
#include <list>
#include <stdexcept>

namespace llarp
{
  bool
  ConfigParser::LoadFile(const fs::path& fname)
  {
    try
    {
      m_Data = util::slurp_file(fname);
    }
    catch (const std::exception& e)
    {
      return false;
    }
    if (m_Data.empty())
      return false;

    m_FileName = fname;
    return Parse();
  }

  bool
  ConfigParser::LoadNewFromStr(std::string_view str)
  {
    m_Data.resize(str.size());
    std::copy(str.begin(), str.end(), m_Data.begin());
    return ParseAll();
  }

  bool
  ConfigParser::LoadFromStr(std::string_view str)
  {
    m_Data.resize(str.size());
    std::copy(str.begin(), str.end(), m_Data.begin());
    return Parse();
  }

  void
  ConfigParser::Clear()
  {
    m_Overrides.clear();
    m_Config.clear();
    m_Data.clear();
  }

  static bool
  whitespace(char ch)
  {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
  }

  /// Differs from Parse() as ParseAll() does NOT skip comments
  /// ParseAll() is only used by RPC endpoint 'config' for
  /// reading new .ini files from string and writing them
  bool
  ConfigParser::ParseAll()
  {
    std::list<std::string_view> lines;
    {
      auto itr = m_Data.begin();
      // split into lines
      while (itr != m_Data.end())
      {
        auto beg = itr;
        while (itr != m_Data.end() && *itr != '\n' && *itr != '\r')
          ++itr;
        lines.emplace_back(std::addressof(*beg), std::distance(beg, itr));
        if (itr == m_Data.end())
          break;
        ++itr;
      }
    }

    std::string_view sectName;
    size_t lineno = 0;
    for (auto line : lines)
    {
      lineno++;
      // Trim whitespace
      while (!line.empty() && whitespace(line.front()))
        line.remove_prefix(1);
      while (!line.empty() && whitespace(line.back()))
        line.remove_suffix(1);

      // Skip blank lines but NOT comments
      if (line.empty())
        continue;

      if (line.front() == '[' && line.back() == ']')
      {
        // section header
        line.remove_prefix(1);
        line.remove_suffix(1);
        sectName = line;
      }
      else if (auto kvDelim = line.find('='); kvDelim != std::string_view::npos)
      {
        // key value pair
        std::string_view k = line.substr(0, kvDelim);
        std::string_view v = line.substr(kvDelim + 1);
        // Trim inner whitespace
        while (!k.empty() && whitespace(k.back()))
          k.remove_suffix(1);
        while (!v.empty() && whitespace(v.front()))
          v.remove_prefix(1);

        if (k.empty())
        {
          throw std::runtime_error(
              fmt::format("{} invalid line ({}): '{}'", m_FileName, lineno, line));
        }
        LogDebug(m_FileName, ": [", sectName, "]:", k, "=", v);
        m_Config[std::string{sectName}].emplace(k, v);
      }
      else  // malformed?
      {
        throw std::runtime_error(
            fmt::format("{} invalid line ({}): '{}'", m_FileName, lineno, line));
      }
    }
    return true;
  }

  bool
  ConfigParser::Parse()
  {
    std::list<std::string_view> lines;
    {
      auto itr = m_Data.begin();
      // split into lines
      while (itr != m_Data.end())
      {
        auto beg = itr;
        while (itr != m_Data.end() && *itr != '\n' && *itr != '\r')
          ++itr;
        lines.emplace_back(std::addressof(*beg), std::distance(beg, itr));
        if (itr == m_Data.end())
          break;
        ++itr;
      }
    }

    std::string_view sectName;
    size_t lineno = 0;
    for (auto line : lines)
    {
      lineno++;
      // Trim whitespace
      while (!line.empty() && whitespace(line.front()))
        line.remove_prefix(1);
      while (!line.empty() && whitespace(line.back()))
        line.remove_suffix(1);

      // Skip blank lines
      if (line.empty() or line.front() == ';' or line.front() == '#')
        continue;

      if (line.front() == '[' && line.back() == ']')
      {
        // section header
        line.remove_prefix(1);
        line.remove_suffix(1);
        sectName = line;
      }
      else if (auto kvDelim = line.find('='); kvDelim != std::string_view::npos)
      {
        // key value pair
        std::string_view k = line.substr(0, kvDelim);
        std::string_view v = line.substr(kvDelim + 1);
        // Trim inner whitespace
        while (!k.empty() && whitespace(k.back()))
          k.remove_suffix(1);
        while (!v.empty() && whitespace(v.front()))
          v.remove_prefix(1);

        if (k.empty())
        {
          throw std::runtime_error(
              fmt::format("{} invalid line ({}): '{}'", m_FileName, lineno, line));
        }
        LogDebug(m_FileName, ": [", sectName, "]:", k, "=", v);
        m_Config[std::string{sectName}].emplace(k, v);
      }
      else  // malformed?
      {
        throw std::runtime_error(
            fmt::format("{} invalid line ({}): '{}'", m_FileName, lineno, line));
      }
    }
    return true;
  }

  void
  ConfigParser::IterAll(std::function<void(std::string_view, const SectionValues_t&)> visit)
  {
    for (const auto& item : m_Config)
      visit(item.first, item.second);
  }

  bool
  ConfigParser::VisitSection(
      const char* name, std::function<bool(const SectionValues_t& sect)> visit) const
  {
    // m_Config is effectively:
    // unordered_map< string, unordered_multimap< string, string  >>
    // in human terms: a map of of sections
    //                 where a section is a multimap of k:v pairs
    auto itr = m_Config.find(name);
    if (itr == m_Config.end())
      return false;
    return visit(itr->second);
  }

  void
  ConfigParser::AddOverride(fs::path fpath, std::string section, std::string key, std::string value)
  {
    auto& data = m_Overrides[fpath];
    data[section].emplace(key, value);
  }

  void
  ConfigParser::Save()
  {
    // write overrides
    for (const auto& [fname, overrides] : m_Overrides)
    {
      std::ofstream ofs(fname);
      for (const auto& [section, values] : overrides)
      {
        ofs << std::endl << "[" << section << "]" << std::endl;
        for (const auto& [key, value] : values)
        {
          ofs << key << "=" << value << std::endl;
        }
      }
    }
    m_Overrides.clear();
  }

  void
  ConfigParser::SaveNew() const
  {
    if (not m_Overrides.empty())
    {
      throw std::invalid_argument("Override specified when attempting new .ini save");
    }
    if (m_Config.empty())
    {
      throw std::invalid_argument("New config not loaded when attempting new .ini save");
    }
    if (m_FileName.empty())
    {
      throw std::invalid_argument("New config cannot be saved with filepath specified");
    }

    std::ofstream ofs(m_FileName);
    for (const auto& [section, values] : m_Config)
    {
      ofs << std::endl << "[" << section << "]" << std::endl;
      for (const auto& [key, value] : values)
      {
        ofs << key << "=" << value << std::endl;
      }
    }
  }

}  // namespace llarp

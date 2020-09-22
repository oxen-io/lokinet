#include <config/ini.hpp>

#include <util/logging/logger.hpp>
#include <util/str.hpp>

#include <cctype>
#include <fstream>
#include <list>
#include <iostream>
#include <cassert>

namespace llarp
{
  bool
  ConfigParser::LoadFile(const fs::path fname)
  {
    {
      std::ifstream f(fname, std::ios::in | std::ios::binary);
      if (!f.is_open())
        return false;
      f.seekg(0, std::ios::end);
      m_Data.resize(f.tellg());
      f.seekg(0, std::ios::beg);
      if (m_Data.size() == 0)
        return false;
      f.read(m_Data.data(), m_Data.size());
    }
    m_FileName = fname;
    return Parse();
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
    for (const auto& line : lines)
    {
      lineno++;
      std::string_view realLine;
      auto comment = line.find_first_of(';');
      if (comment == std::string_view::npos)
        comment = line.find_first_of('#');
      if (comment == std::string_view::npos)
        realLine = line;
      else
        realLine = line.substr(0, comment);
      // blank or commented line?
      if (realLine.size() == 0)
        continue;
      // find delimiters
      auto sectOpenPos = realLine.find_first_of('[');
      auto sectClosPos = realLine.find_first_of(']');
      auto kvDelim = realLine.find_first_of('=');
      if (sectOpenPos != std::string_view::npos && sectClosPos != std::string_view::npos
          && kvDelim == std::string_view::npos)
      {
        // section header

        // clamp whitespaces
        ++sectOpenPos;
        while (whitespace(realLine[sectOpenPos]) && sectOpenPos != sectClosPos)
          ++sectOpenPos;
        --sectClosPos;
        while (whitespace(realLine[sectClosPos]) && sectClosPos != sectOpenPos)
          --sectClosPos;
        // set section name
        sectName = realLine.substr(sectOpenPos, sectClosPos);
      }
      else if (kvDelim != std::string_view::npos)
      {
        // key value pair
        std::string_view::size_type k_start = 0;
        std::string_view::size_type k_end = kvDelim;
        std::string_view::size_type v_start = kvDelim + 1;
        std::string_view::size_type v_end = realLine.size() - 1;
        // clamp whitespaces
        while (whitespace(realLine[k_start]) && k_start != kvDelim)
          ++k_start;
        while (whitespace(realLine[k_end - 1]) && k_end != k_start)
          --k_end;
        while (whitespace(realLine[v_start]) && v_start != v_end)
          ++v_start;
        while (whitespace(realLine[v_end]))
          --v_end;

        // sect.k = v
        std::string_view k = realLine.substr(k_start, k_end - k_start);
        std::string_view v = realLine.substr(v_start, 1 + (v_end - v_start));
        if (k.size() == 0 || v.size() == 0)
        {
          LogError(m_FileName, " invalid line (", lineno, "): '", line, "'");
          return false;
        }
        SectionValues_t& sect = m_Config[std::string{sectName}];
        LogDebug(m_FileName, ": ", sectName, ".", k, "=", v);
        sect.emplace(k, v);
      }
      else  // malformed?
      {
        LogError(m_FileName, " invalid line (", lineno, "): '", line, "'");
        return false;
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
  ConfigParser::AddOverride(std::string section, std::string key, std::string value)
  {
    m_Overrides[section].emplace(key, value);
  }

  void
  ConfigParser::Save() const
  {
    // if we have no overrides keep the config the same on disk
    if (m_Overrides.empty())
      return;
    std::ofstream ofs(m_FileName);
    // write existing config data
    ofs.write(m_Data.data(), m_Data.size());
    // write overrides
    ofs << std::endl << std::endl << "# overrides" << std::endl;
    for (const auto& [section, values] : m_Overrides)
    {
      ofs << std::endl << "[" << section << "]" << std::endl;
      for (const auto& [key, value] : values)
      {
        ofs << key << "=" << value << std::endl;
      }
    }
  }

}  // namespace llarp

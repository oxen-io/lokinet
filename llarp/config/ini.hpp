#pragma once

#include <llarp/util/file.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace llarp
{
    struct ConfigParser
    {
        using SectionValues = std::unordered_multimap<std::string, std::string>;
        using ConfigMap = std::unordered_map<std::string, SectionValues>;
        /// clear parser
        void clear();

        /// Load config file.  Throws on error.
        void load_file(const fs::path& fname);

        /// Load new .ini data from string (calls ParseAll() rather than Parse())
        /// Throws on error.
        void load_new_from_str(std::string str);

        /// Load from string. Throws on error.
        void load_from_str(std::string str);

        /// iterate all sections and thier values
        void iter_all_sections(std::function<void(std::string_view, const SectionValues&)> visit);

        /// visit a section in config read only by name
        /// return false if no section or value propagated from visitor
        bool visit_section(const char* name, std::function<bool(const SectionValues&)> visit) const;

        /// add a config option that is appended in another file
        void add_override(fs::path file, std::string section, std::string key, std::string value);

        /// save config overrides
        void save();

        /// save new .ini config file to path
        void save_new() const;

        void set_filename(const fs::path& f) { _filename = f; }

      private:
        void parse_all();

        void parse();

        std::string _data;
        ConfigMap _config;
        std::unordered_map<fs::path, ConfigMap, util::FileHash> _overrides;
        fs::path _filename;
    };

}  // namespace llarp

#include "definition.hpp"

#include <llarp/util/logging.hpp>

#include <cassert>
#include <iterator>
#include <stdexcept>

namespace llarp
{
  template <>
  bool
  OptionDefinition<bool>::fromString(const std::string& input)
  {
    if (input == "false" || input == "off" || input == "0" || input == "no")
      return false;
    else if (input == "true" || input == "on" || input == "1" || input == "yes")
      return true;
    else
      throw std::invalid_argument{fmt::format("{} is not a valid bool", input)};
  }

  ConfigDefinition&
  ConfigDefinition::defineOption(OptionDefinition_ptr def)
  {
    using namespace config;
    // If explicitly deprecated or is a {client,relay} option in a {relay,client} config then add a
    // dummy, warning option instead of this one.
    if (def->deprecated || (relay ? def->clientOnly : def->relayOnly))
    {
      return defineOption<std::string>(
          def->section,
          def->name,
          MultiValue,
          Hidden,
          [deprecated = def->deprecated,
           relay = relay,
           opt = "[" + def->section + "]:" + def->name](std::string_view) {
            LogWarn(
                "*** WARNING: The config option ",
                opt,
                (deprecated  ? " is deprecated"
                     : relay ? " is not valid in service node configuration files"
                             : " is not valid in client configuration files"),
                " and has been ignored.");
          });
    }

    auto [sectionItr, newSect] = m_definitions.try_emplace(def->section);
    if (newSect)
      m_sectionOrdering.push_back(def->section);
    auto& section = sectionItr->first;

    auto [it, added] = m_definitions[section].try_emplace(std::string{def->name}, std::move(def));
    if (!added)
      throw std::invalid_argument{
          fmt::format("definition for [{}]:{} already exists", def->section, def->name)};

    m_definitionOrdering[section].push_back(it->first);

    if (!it->second->comments.empty())
      addOptionComments(section, it->first, std::move(it->second->comments));

    return *this;
  }

  ConfigDefinition&
  ConfigDefinition::addConfigValue(
      std::string_view section, std::string_view name, std::string_view value)
  {
    // see if we have an undeclared handler to fall back to in case section or section:name is
    // absent
    auto undItr = m_undeclaredHandlers.find(std::string(section));
    bool haveUndeclaredHandler = (undItr != m_undeclaredHandlers.end());

    // get section, falling back to undeclared handler if needed
    auto secItr = m_definitions.find(std::string(section));
    if (secItr == m_definitions.end())
    {
      // fallback to undeclared handler if available
      if (not haveUndeclaredHandler)
        throw std::invalid_argument{fmt::format("unrecognized section [{}]", section)};
      auto& handler = undItr->second;
      handler(section, name, value);
      return *this;
    }

    // section was valid, get definition by name
    // fall back to undeclared handler if needed
    auto& sectionDefinitions = secItr->second;
    auto defItr = sectionDefinitions.find(std::string(name));
    if (defItr != sectionDefinitions.end())
    {
      OptionDefinition_ptr& definition = defItr->second;
      definition->parseValue(std::string(value));
      return *this;
    }

    if (not haveUndeclaredHandler)
      throw std::invalid_argument{fmt::format("unrecognized option [{}]: {}", section, name)};

    auto& handler = undItr->second;
    handler(section, name, value);
    return *this;
  }

  void
  ConfigDefinition::addUndeclaredHandler(const std::string& section, UndeclaredValueHandler handler)
  {
    auto itr = m_undeclaredHandlers.find(section);
    if (itr != m_undeclaredHandlers.end())
      throw std::logic_error{fmt::format("section {} already has a handler", section)};

    m_undeclaredHandlers[section] = std::move(handler);
  }

  void
  ConfigDefinition::removeUndeclaredHandler(const std::string& section)
  {
    auto itr = m_undeclaredHandlers.find(section);
    if (itr != m_undeclaredHandlers.end())
      m_undeclaredHandlers.erase(itr);
  }

  void
  ConfigDefinition::validateRequiredFields()
  {
    visitSections([&](const std::string& section, const DefinitionMap&) {
      visitDefinitions(section, [&](const std::string&, const OptionDefinition_ptr& def) {
        if (def->required and def->getNumberFound() < 1)
        {
          throw std::invalid_argument{
              fmt::format("[{}]:{} is required but missing", section, def->name)};
        }

        // should be handled earlier in OptionDefinition::parseValue()
        assert(def->getNumberFound() <= 1 or def->multiValued);
      });
    });
  }

  void
  ConfigDefinition::acceptAllOptions()
  {
    visitSections([this](const std::string& section, const DefinitionMap&) {
      visitDefinitions(
          section, [](const std::string&, const OptionDefinition_ptr& def) { def->tryAccept(); });
    });
  }

  void
  ConfigDefinition::addSectionComments(
      const std::string& section, std::vector<std::string> comments)
  {
    auto& sectionComments = m_sectionComments[section];
    for (auto& c : comments)
      sectionComments.emplace_back(std::move(c));
  }

  void
  ConfigDefinition::addOptionComments(
      const std::string& section, const std::string& name, std::vector<std::string> comments)
  {
    auto& defComments = m_definitionComments[section][name];
    if (defComments.empty())
      defComments = std::move(comments);
    else
      defComments.insert(
          defComments.end(),
          std::make_move_iterator(comments.begin()),
          std::make_move_iterator(comments.end()));
  }

  std::string
  ConfigDefinition::generateINIConfig(bool useValues)
  {
    std::string ini;
    auto ini_append = std::back_inserter(ini);

    int sectionsVisited = 0;

    visitSections([&](const std::string& section, const DefinitionMap&) {
      std::string sect_str;
      auto sect_append = std::back_inserter(sect_str);

      visitDefinitions(section, [&](const std::string& name, const OptionDefinition_ptr& def) {
        bool has_comment = false;
        // TODO: as above, this will create empty objects
        // TODO: as above (but more important): this won't handle definitions with no entries
        //       (i.e. those handled by UndeclaredValueHandler's)
        for (const std::string& comment : m_definitionComments[section][name])
        {
          fmt::format_to(sect_append, "\n# {}", comment);
          has_comment = true;
        }

        if (useValues and def->getNumberFound() > 0)
        {
          for (const auto& val : def->valuesAsString())
            fmt::format_to(sect_append, "\n{}={}", name, val);
          *sect_append = '\n';
        }
        else if (not def->hidden)
        {
          if (auto defaults = def->defaultValuesAsString(); not defaults.empty())
            for (const auto& val : defaults)
              fmt::format_to(sect_append, "\n{}{}={}", def->required ? "" : "#", name, val);
          else
            // We have no defaults so we append it as "#opt-name=" so that we show the option name,
            // and make it simple to uncomment and edit to the desired value.
            fmt::format_to(sect_append, "\n#{}=", name);
          *sect_append = '\n';
        }
        else if (has_comment)
          *sect_append = '\n';
      });

      if (sect_str.empty())
        return;  // Skip sections with no options

      if (sectionsVisited > 0)
        ini += "\n\n";

      fmt::format_to(ini_append, "[{}]\n", section);

      // TODO: this will create empty objects as a side effect of map's operator[]
      // TODO: this also won't handle sections which have no definition
      for (const std::string& comment : m_sectionComments[section])
      {
        fmt::format_to(ini_append, "# {}\n", comment);
      }
      ini += "\n";
      ini += sect_str;

      sectionsVisited++;
    });

    return ini;
  }

  const OptionDefinition_ptr&
  ConfigDefinition::lookupDefinitionOrThrow(std::string_view section, std::string_view name) const
  {
    const auto sectionItr = m_definitions.find(std::string(section));
    if (sectionItr == m_definitions.end())
      throw std::invalid_argument{fmt::format("No config section [{}]", section)};

    auto& sectionDefinitions = sectionItr->second;
    const auto definitionItr = sectionDefinitions.find(std::string(name));
    if (definitionItr == sectionDefinitions.end())
      throw std::invalid_argument{
          fmt::format("No config item {} within section {}", name, section)};

    return definitionItr->second;
  }

  OptionDefinition_ptr&
  ConfigDefinition::lookupDefinitionOrThrow(std::string_view section, std::string_view name)
  {
    return const_cast<OptionDefinition_ptr&>(
        const_cast<const ConfigDefinition*>(this)->lookupDefinitionOrThrow(section, name));
  }

  void
  ConfigDefinition::visitSections(SectionVisitor visitor) const
  {
    for (const std::string& section : m_sectionOrdering)
    {
      const auto itr = m_definitions.find(section);
      assert(itr != m_definitions.end());
      visitor(section, itr->second);
    }
  };
  void
  ConfigDefinition::visitDefinitions(const std::string& section, DefVisitor visitor) const
  {
    const auto& defs = m_definitions.at(section);
    const auto& defOrdering = m_definitionOrdering.at(section);
    for (const std::string& name : defOrdering)
    {
      const auto itr = defs.find(name);
      assert(itr != defs.end());
      visitor(name, itr->second);
    }
  };

}  // namespace llarp

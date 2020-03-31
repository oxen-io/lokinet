#include <config/definition.hpp>

#include <sstream>
#include <stdexcept>

namespace llarp
{

OptionDefinitionBase::OptionDefinitionBase(std::string section_,
                                           std::string name_,
                                           bool required_)
  : section(section_)
  , name(name_)
  , required(required_)
{
}
OptionDefinitionBase::OptionDefinitionBase(std::string section_,
                                           std::string name_,
                                           bool required_,
                                           bool multiValued_)
  : section(section_)
  , name(name_)
  , required(required_)
  , multiValued(multiValued_)
{
}

ConfigDefinition&
ConfigDefinition::defineOption(OptionDefinition_ptr def)
{
  auto sectionItr = m_definitions.find(def->section);
  if (sectionItr == m_definitions.end())
    m_sectionOrdering.push_back(def->section);

  auto& sectionDefinitions = m_definitions[def->section];
  if (sectionDefinitions.find(def->name) != sectionDefinitions.end())
    throw std::invalid_argument(stringify("definition for [",
          def->section, "]:", def->name, " already exists"));

  m_definitionOrdering[def->section].push_back(def->name);
  sectionDefinitions[def->name] = std::move(def);

  return *this;
}

ConfigDefinition&
ConfigDefinition::addConfigValue(string_view section, string_view name, string_view value)
{
  auto secItr = m_definitions.find(std::string(section));
  if (secItr == m_definitions.end())
  {
    // fallback to undeclared handler if available
    auto undItr = m_undeclaredHandlers.find(std::string(section));
    if (undItr == m_undeclaredHandlers.end())
      throw std::invalid_argument(stringify("no declared section [", section, "]"));
    else
    {
      auto& handler = undItr->second;
      handler(section, name, value);
      return *this;
    }
  }

  // section was valid, get definition by name
  auto& sectionDefinitions = secItr->second;
  auto defItr = sectionDefinitions.find(std::string(name));
  if (defItr == sectionDefinitions.end())
    throw std::invalid_argument(stringify("no declared option [", section, "]:", name));

  OptionDefinition_ptr& definition = defItr->second;
  definition->parseValue(std::string(value));

  return *this;
}

void
ConfigDefinition::addUndeclaredHandler(const std::string& section, UndeclaredValueHandler handler)
{
  auto itr = m_undeclaredHandlers.find(section);
  if (itr != m_undeclaredHandlers.end())
    throw std::logic_error(stringify("section ", section, " already has a handler"));

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
  visitSections([&](const std::string& section, const DefinitionMap&)
  {
    visitDefinitions(section, [&](const std::string&, const OptionDefinition_ptr& def)
    {
      if (def->required and def->numFound < 1)
      {
        throw std::invalid_argument(stringify(
              "[", section, "]:", def->name, " is required but missing"));
      }

      // should be handled earlier in OptionDefinition::parseValue()
      assert(def->numFound <= 1 or def->multiValued);
    });
  });
}

void
ConfigDefinition::acceptAllOptions()
{
  visitSections([&](const std::string& section, const DefinitionMap&)
  {
    visitDefinitions(section, [&](const std::string&, const OptionDefinition_ptr& def)
    {
      def->tryAccept();
    });
  });
}

void
ConfigDefinition::addSectionComments(const std::string& section,
                                     std::vector<std::string> comments)
{
  auto& sectionComments = m_sectionComments[section];
  for (size_t i=0; i<comments.size(); ++i)
  {
    sectionComments.emplace_back(std::move(comments[i]));
  }
}

void
ConfigDefinition::addOptionComments(const std::string& section,
                                    const std::string& name,
                                    std::vector<std::string> comments)
{
  auto& defComments = m_definitionComments[section][name];
  for (size_t i=0; i<comments.size(); ++i)
  {
    defComments.emplace_back(std::move(comments[i]));
  }
}

std::string
ConfigDefinition::generateINIConfig(bool useValues)
{
  std::ostringstream oss;

  int sectionsVisited = 0;

  visitSections([&](const std::string& section, const DefinitionMap&) {
    if (sectionsVisited > 0)
      oss << "\n\n";

    // TODO: this will create empty objects as a side effect of map's operator[]
    // TODO: this also won't handle sections which have no definition
    for (const std::string& comment : m_sectionComments[section])
    {
      oss << "# " << comment << "\n";
    }

    oss << "[" << section << "]\n";

    visitDefinitions(section, [&](const std::string& name, const OptionDefinition_ptr& def) {
      oss << "\n";

      // TODO: as above, this will create empty objects
      // TODO: as above (but more important): this won't handle definitions with no entries
      //       (i.e. those handled by UndeclaredValueHandler's)
      for (const std::string& comment : m_definitionComments[section][name])
      {
        oss << "# " << comment << "\n";
      }

      if (useValues and def->numFound > 0)
      {
        oss << name << "=" << def->valueAsString(false) << "\n";
      }
      else
      {
        if (not def->required)
          oss << "#";
        oss << name << "=" << def->defaultValueAsString() << "\n";
      }

    });

    sectionsVisited++;
  });

  return oss.str();
}

const OptionDefinition_ptr&
ConfigDefinition::lookupDefinitionOrThrow(string_view section, string_view name) const
{
  const auto sectionItr = m_definitions.find(std::string(section));
  if (sectionItr == m_definitions.end())
    throw std::invalid_argument(stringify("No config section [", section, "]"));

  auto& sectionDefinitions = sectionItr->second;
  const auto definitionItr = sectionDefinitions.find(std::string(name));
  if (definitionItr == sectionDefinitions.end())
    throw std::invalid_argument(stringify("No config item ", name, " within section ", section));

  return definitionItr->second;
}

OptionDefinition_ptr&
ConfigDefinition::lookupDefinitionOrThrow(string_view section, string_view name)
{
  return const_cast<OptionDefinition_ptr&>(
      const_cast<const ConfigDefinition*>(this)->lookupDefinitionOrThrow(section, name));
}

void ConfigDefinition::visitSections(SectionVisitor visitor) const
{
  for (const std::string& section : m_sectionOrdering)
  {
    const auto itr = m_definitions.find(section);
    assert(itr != m_definitions.end());
    visitor(section, itr->second);
  }
};
void ConfigDefinition::visitDefinitions(const std::string& section, DefVisitor visitor) const
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

} // namespace llarp


#include <config/definition.hpp>

#include <sstream>
#include <stdexcept>

namespace llarp
{

ConfigDefinitionBase::ConfigDefinitionBase(std::string section_,
                                           std::string name_,
                                           bool required_)
  : section(section_)
  , name(name_)
  , required(required_)
{
}

Configuration&
Configuration::defineOption(ConfigDefinition_ptr def)
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

Configuration&
Configuration::addConfigValue(string_view section, string_view name, string_view value)
{
  ConfigDefinition_ptr& definition = lookupDefinitionOrThrow(section, name);
  definition->parseValue(std::string(value));

  return *this;
}

void
Configuration::validateRequiredFields()
{
  visitSections([&](const std::string& section, const DefinitionMap&)
  {
    visitDefinitions(section, [&](const std::string&, const ConfigDefinition_ptr& def)
    {
      if (def->required and def->numFound < 1)
      {
        throw std::invalid_argument(stringify(
              "[", section, "]:", def->name, " is required but missing"));
      }

      // should be handled earlier in ConfigDefinition::parseValue()
      assert(def->numFound <= 1 or def->multiValued);
    });
  });
}

void
Configuration::acceptAllOptions()
{
  visitSections([&](const std::string& section, const DefinitionMap&)
  {
    visitDefinitions(section, [&](const std::string&, const ConfigDefinition_ptr& def)
    {
      def->tryAccept();
    });
  });
}

std::string
Configuration::generateINIConfig(bool useValues)
{
  std::ostringstream oss;

  int sectionsVisited = 0;

  visitSections([&](const std::string& section, const DefinitionMap&) {
    if (sectionsVisited > 0)
      oss << "\n";

    oss << "[" << section << "]\n";

    visitDefinitions(section, [&](const std::string& name, const ConfigDefinition_ptr& def) {
      if (useValues and def->numFound > 0)
      {
        oss << name << "=" << def->valueAsString(false) << "\n";
      }
      else
      {
        if (not def->required)
          oss << "# ";
        oss << name << "=" << def->defaultValueAsString() << "\n";
      }
    });

    sectionsVisited++;
  });

  return oss.str();
}

const ConfigDefinition_ptr&
Configuration::lookupDefinitionOrThrow(string_view section, string_view name) const
{
  const auto sectionItr = m_definitions.find(std::string(section));
  if (sectionItr == m_definitions.end())
    throw std::invalid_argument(stringify("No config section ", section));

  auto& sectionDefinitions = sectionItr->second;
  const auto definitionItr = sectionDefinitions.find(std::string(name));
  if (definitionItr == sectionDefinitions.end())
    throw std::invalid_argument(stringify("No config item ", name, " within section ", section));

  return definitionItr->second;
}

ConfigDefinition_ptr&
Configuration::lookupDefinitionOrThrow(string_view section, string_view name)
{
  return const_cast<ConfigDefinition_ptr&>(
      const_cast<const Configuration*>(this)->lookupDefinitionOrThrow(section, name));
}

void Configuration::visitSections(SectionVisitor visitor) const
{
  for (const std::string& section : m_sectionOrdering)
  {
    const auto itr = m_definitions.find(section);
    assert(itr != m_definitions.end());
    visitor(section, itr->second);
  }
};
void Configuration::visitDefinitions(const std::string& section, DefVisitor visitor) const
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


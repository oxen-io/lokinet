#include <config/definition.hpp>

#include <stdexcept>

namespace llarp
{

ConfigDefinitionBase::ConfigDefinitionBase(std::string section_,
                                           std::string name_,
                                           bool required_,
                                           bool multiValued_)
  : section(section_)
  , name(name_)
  , required(required_)
  , multiValued(multiValued_)
{
}

Configuration&
Configuration::addDefinition(ConfigDefinition_ptr def)
{
  auto& sectionDefinitions = m_definitions[def->section];
  if (sectionDefinitions.find(def->name) != sectionDefinitions.end())
    throw std::invalid_argument(stringify("definition for [",
          def->section, "]:", def->name, " already exists"));

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

void
Configuration::validate()
{
  for (const auto& pair : m_definitions)
  {
    const std::string& section = pair.first;

    const auto& sectionDefinitions = pair.second;
    for (const auto& defPair : sectionDefinitions)
    {
      const auto& def = defPair.second;
      if (def->required and def->numFound < 1)
      {
        throw std::invalid_argument(stringify(
              "[", section, "]:", def->name, " is required but missing"));
      }

      // should be handled earlier in ConfigDefinition::parseValue()
      assert(def->numFound == 1 or def->multiValued);
    }
  }
}

std::string
Configuration::generateDefaultConfig()
{
  return "Implement me!";
}

std::string
Configuration::generateOverridenConfig()
{
  return "Implement me!";
}

} // namespace llarp


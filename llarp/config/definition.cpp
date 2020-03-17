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

configuration&
Configuration::addconfigvalue(string_view section, string_view name, string_view value)
{
  configdefinition_ptr& definition = lookupdefinitionorthrow(section, name);
  definition->parsevalue(std::string(value));

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


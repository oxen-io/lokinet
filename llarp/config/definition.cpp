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

Configuration& Configuration::addDefinition(ConfigDefinition_ptr def)
{
  auto& sectionDefinitions = m_definitions[def->section];
  if (sectionDefinitions.find(def->name) != sectionDefinitions.end())
    throw std::invalid_argument(stringify("definition for [",
          def->section, "]:", def->name, " already exists"));

  sectionDefinitions[def->name] = std::move(def);

  return *this;
}

} // namespace llarp


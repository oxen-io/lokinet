#pragma once

#include <util/str.hpp>
#include <nonstd/optional.hpp>

#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace llarp
{

  /// non-templated base class for all config definition types.
  struct ConfigDefinitionBase 
  {
    ConfigDefinitionBase(std::string section_,
                         std::string name_,
                         bool required_,
                         bool multiValued_);

    virtual ~ConfigDefinitionBase() {}

    /// subclasses should provide their default value as a string
    virtual std::string defaultValueAsString() = 0;

    /// subclasses should parse and store the provided input
    virtual void parseValue(const std::string& input) = 0;

    /// subclasess should write their parsed value (not default value) as a string
    virtual std::string writeValue(bool useDefault) = 0;

    std::string section;
    std::string name;
    bool required = false;
    bool multiValued = false;
    size_t numFound = 0;
  };

  template<typename T>
  struct ConfigDefinition : public ConfigDefinitionBase
  {
    ConfigDefinition(std::string section_,
                           std::string name_,
                           bool required_,
                           bool multiValued_,
                           nonstd::optional<T> defaultValue_)
      : ConfigDefinitionBase(section_, name_, required_, multiValued_)
      , defaultValue(defaultValue_)
    {
    }

    nonstd::optional<T> getValue() const
    {
      if (parsedValue)
        return parsedValue.value();
      else if (not required)
        return defaultValue.value();
      else
        return {};
    }

    std::string defaultValueAsString()
    {
      if (defaultValue)
        return std::to_string(defaultValue.value());
      else
        return "";
    }

    void parseValue(const std::string& input)
    {
      if (not multiValued and parsedValue)
      {
        throw std::invalid_argument(stringify("duplicate value for ", name,
              ", previous value: ", parsedValue.value()));
      }
      
      std::istringstream iss(input);
      T t;
      iss >> t;
      if (iss.fail())
        throw std::invalid_argument(stringify(input, " is not a valid" , typeid(T).name()));
      else
        parsedValue = t;
    }

    std::string writeValue(bool useDefault)
    {
      if (parsedValue)
        return std::to_string(parsedValue.value());
      else if (useDefault and defaultValue.has_value())
        return std::to_string(defaultValue.value());
      else
        return {};
    }

    nonstd::optional<T> defaultValue;
    nonstd::optional<T> parsedValue; // needs to be set when parseValue() called
  };


  using ConfigDefinition_ptr = std::unique_ptr<ConfigDefinitionBase>;

  /// A configuration holds an ordered set of ConfigDefinitions defining the allowable values and
  /// their constraints and an optional set defining overrides of those values (e.g. the results
  /// of a parsed config file).
  struct Configuration {
    // the first std::string template parameter is the section
    std::unordered_map<std::string, std::unordered_map<std::string, ConfigDefinition_ptr>> m_definitions;

    Configuration& addDefinition(ConfigDefinition_ptr def);

    Configuration& addConfigValue(string_view section,
                                  string_view name,
                                  string_view value)
    {
      ConfigDefinition_ptr& definition = lookupDefinitionOrThrow(section, name);
      definition->parseValue(std::string(value));

      return *this;
    }

    template<typename T>
    nonstd::optional<T> getConfigValue(string_view section, string_view name)
    {
      ConfigDefinition_ptr& definition = lookupDefinitionOrThrow(section, name);

      auto derived = dynamic_cast<const ConfigDefinition<T>*>(definition.get());
      if (not derived)
        throw std::invalid_argument(stringify("", typeid(T).name(),
            " is the incorrect type for [", section, "]:", name));

      return derived->getValue();
    }

    /// Validate the config, presumably called after parsing. This will throw an exception if the
    /// parsed values do not meet the provided definition. 
    ///
    /// Note that this will only handle a subset of errors that may occur. Specifically, this will
    /// handle errors about missing required fields, whereas errors about incorrect type,
    /// duplicates, etc. are handled during parsing.
    ///
    /// @throws std::invalid_argument if configuration constraints are not met
    void validate();

    std::string
    generateDefaultConfig();

    std::string
    generateOverridenConfig();

   private:

    const ConfigDefinition_ptr& lookupDefinitionOrThrow(string_view section, string_view name) const
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

    ConfigDefinition_ptr& lookupDefinitionOrThrow(string_view section, string_view name)
    {
      return const_cast<ConfigDefinition_ptr&>(
          const_cast<const Configuration*>(this)->lookupDefinitionOrThrow(section, name));
    }
  };

} // namespace llarp


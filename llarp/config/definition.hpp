#pragma once

#include <util/str.hpp>
#include <nonstd/optional.hpp>

#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace llarp
{

  /// non-templated base class for all config definition types.
  struct ConfigDefinitionBase 
  {
    ConfigDefinitionBase(std::string section_,
                         std::string name_,
                         bool required_,
                         bool multiValued_);

    virtual
    ~ConfigDefinitionBase() {}

    /// subclasses should provide their default value as a string
    virtual std::string
    defaultValueAsString() = 0;

    /// subclasses should parse and store the provided input
    virtual void
    parseValue(const std::string& input) = 0;

    /// subclasess should write their parsed value (not default value) as a string
    virtual std::string
    writeValue(bool useDefault) = 0;

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

    nonstd::optional<T>
    getValue() const
    {
      if (parsedValue)
        return parsedValue.value();
      else if (not required)
        return defaultValue.value();
      else
        return {};
    }

    std::string
    defaultValueAsString()
    {
      std::ostringstream oss;
      if (defaultValue.has_value())
        oss << defaultValue.value();

      return oss.str();
    }

    void
    parseValue(const std::string& input)
    {
      if (not multiValued and parsedValue.has_value())
      {
        throw std::invalid_argument(stringify("duplicate value for ", name,
              ", previous value: ", parsedValue.value()));
      }
      
      std::istringstream iss(input);
      T t;
      iss >> t;
      if (iss.fail())
      {
        throw std::invalid_argument(stringify(input, " is not a valid ", typeid(T).name()));
      }
      else
      {
        parsedValue = t;
        numFound++;
      }

    }

    std::string
    writeValue(bool useDefault)
    {
      std::ostringstream oss;
      if (parsedValue.has_value())
        oss << parsedValue.value();
      else if (useDefault and defaultValue.has_value())
        oss << defaultValue.value();

      return oss.str();
    }

    nonstd::optional<T> defaultValue;
    nonstd::optional<T> parsedValue; // needs to be set when parseValue() called
  };


  using ConfigDefinition_ptr = std::unique_ptr<ConfigDefinitionBase>;

  // map of k:v pairs
  using DefinitionMap = std::unordered_map<std::string, ConfigDefinition_ptr>;

  // map of section-name to map-of-definitions
  using SectionMap = std::unordered_map<std::string, DefinitionMap>;

  /// A configuration holds an ordered set of ConfigDefinitions defining the allowable values and
  /// their constraints and an optional set defining overrides of those values (e.g. the results
  /// of a parsed config file).
  struct Configuration {
    SectionMap m_definitions;

    Configuration&
    addDefinition(ConfigDefinition_ptr def);

    Configuration&
    addConfigValue(string_view section,
                                  string_view name,
                                  string_view value);

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
    void
    validate();

    /// Generate a config string from the current config definition, optionally using overridden
    /// values. The generated config will preserve insertion order of both sections and their
    /// definitions.
    ///
    /// Definitions which are required or have an overriden value (and useValues == true) will be
    /// written normally. Otherwise, they will be written commented-out in order to provide a
    /// complete documentation of the configuration file.
    ///
    /// @param useValues specifies whether we use specified values (e.g. those from calls to
    ///        addConfigValue()) or only definitions
    /// @return a string containing the config in INI format
    std::string
    generateINIConfig(bool useValues = false);

   private:

    ConfigDefinition_ptr& lookupDefinitionOrThrow(string_view section, string_view name);
    const ConfigDefinition_ptr& lookupDefinitionOrThrow(string_view section, string_view name) const;

    using SectionVisitor = std::function<void(const std::string&, const DefinitionMap&)>;
    void visitSections(SectionVisitor visitor) const;

    using DefVisitor = std::function<void(const std::string&, const ConfigDefinition_ptr&)>;
    void visitDefinitions(const std::string& section, DefVisitor visitor) const;

    // track insertion order
    std::vector<std::string> m_sectionOrdering;
    std::unordered_map<std::string, std::vector<std::string>> m_definitionOrdering;
  };

} // namespace llarp


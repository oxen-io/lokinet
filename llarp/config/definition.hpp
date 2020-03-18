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

  /// A base class for specifying config options and their constraints. The basic to/from string
  /// type functions are provided pure-virtual. The type-aware implementations which implement these
  /// functions are templated classes. One reason for providing a non-templated base class is so
  /// that they can all be mixed into the same containers (albiet as pointers).
  struct ConfigDefinitionBase 
  {
    ConfigDefinitionBase(std::string section_,
                         std::string name_,
                         bool required_,
                         bool multiValued_);

    virtual
    ~ConfigDefinitionBase() {}

    /// Subclasses should provide their default value as a string
    ///
    /// @return the option's default value represented as a string
    virtual std::string
    defaultValueAsString() = 0;

    /// Subclasses should parse and store the provided input
    ///
    /// @param input is the string input to interpret
    virtual void
    parseValue(const std::string& input) = 0;

    /// Subclasess should write their parsed value as a string, optionally falling back to any
    /// specified default if `useDefault` is true.
    ///
    /// @param useDefault should specify whether to fallback to default when possible
    /// @return the option's value as a string
    virtual std::string
    valueAsString(bool useDefault) = 0;

    std::string section;
    std::string name;
    bool required = false;
    bool multiValued = false;
    size_t numFound = 0;
  };

  /// The primary type-aware implementation of ConfigDefinitionBase, this templated class allows
  /// for implementations which can use the std::ostringstream and std::istringstream for to/from
  /// string functionality.
  /// 
  /// Note that types (T) used as template parameters here must be used verbatim when calling 
  /// Configuration::getConfigValue(). Similar types such as uint32_t and int32_t cannot be mixed.
  template<typename T>
  struct ConfigDefinition : public ConfigDefinitionBase
  {
    /// Constructor. Arguments are passed directly to ConfigDefinitionBase. Also accepts a default
    /// value, which is used in the following situations:
    ///
    /// 1) as the return value for getValue() if there is no parsed value and required==false
    /// 2) as the output in defaultValueAsString(), used to generate config files
    /// 3) as the output in valueAsString(), used to generate config files
    ConfigDefinition(std::string section_,
                           std::string name_,
                           bool required_,
                           bool multiValued_,
                           nonstd::optional<T> defaultValue_)
      : ConfigDefinitionBase(section_, name_, required_, multiValued_)
      , defaultValue(defaultValue_)
    {
    }

    /// Returns the parsed value, if available. Otherwise, provides the default value if the option
    /// is not required. Otherwise, returns an empty optional.
    ///
    /// @return an optional with the parsed value, the default value, or no value.
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
    defaultValueAsString() override
    {
      std::ostringstream oss;
      if (defaultValue.has_value())
        oss << defaultValue.value();

      return oss.str();
    }

    void
    parseValue(const std::string& input) override
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
    valueAsString(bool useDefault) override
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

  /// A Configuration holds an ordered set of ConfigDefinitions defining the allowable values and
  /// their constraints (specified through calls to addConfigOption()).
  ///
  /// The layout and grouping of the config options are modelled after the INI file format; each
  /// option has a name and is grouped under a section. Duplicate option names are allowed only if
  /// they exist in a different section. The configuration can be serialized in the INI file format
  /// using the generateINIConfig() function.
  ///
  /// Configured values (e.g. those encountered when parsing a file) can be provided through calls
  /// to addConfigValue(). These take a std::string as a value, which is automatically parsed.
  /// 
  /// The Configuration can be used to print out a full config string (or file), including fields
  /// with defaults and optionally fields which have a specified value (values provided through
  /// calls to addConfigValue()).
  struct Configuration {
    SectionMap m_definitions;

    /// Spefify the parameters and type of a configuration option. The parameters are members of
    /// ConfigDefinitionBase; the type is inferred from ConfigDefinition's template parameter T.
    ///
    /// This function should be called for every option that this Configuration supports, and should
    /// be done before any other interractions involving that option.
    ///
    /// @param def should be a unique_ptr to a valid subclass of ConfigDefinitionBase
    /// @return `*this` for chaining calls
    /// @throws std::invalid_argument if the option already exists
    Configuration&
    addConfigOption(ConfigDefinition_ptr def);

    /// Specify a config value for the given section and name. The value should be a valid string
    /// representing the type used by the option (e.g. the type provided when addConfigOption() was
    /// called).
    ///
    /// If the specified option doesn't exist, an exception will be thrown. Otherwise, the option's
    /// parsedValue() will be invoked, and should throw an exception if the string can't be parsed.
    ///
    /// @param section is the section this value resides in
    /// @param name is the name of the value
    /// @return `*this` for chaining calls
    /// @throws if the option doesn't exist or the provided string isn't parseable
    Configuration&
    addConfigValue(string_view section,
                   string_view name,
                   string_view value);

    /// Get a config value. If the value hasn't been provided but a default has, the default will
    /// be returned. If no value and no default is provided, an empty optional will be returned.
    ///
    /// The type T should exactly match that provided by the definition; it is not sufficient for
    /// one type to be a valid substitution for the other.
    ///
    /// @param section is the section this value resides in
    /// @param name is the name of the value
    /// @return an optional providing the configured value, the default, or empty
    /// @throws std::invalid_argument if there is no such config option or the wrong type T was
    //          provided
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

    /// Validate that all required fields are present.
    ///
    /// @throws std::invalid_argument if configuration constraints are not met
    void
    validateRequiredFields();

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


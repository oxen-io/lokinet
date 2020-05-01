#pragma once

#include <util/str.hpp>

#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <functional>
#include <optional>
#include <cassert>

namespace llarp
{
  /// A base class for specifying config options and their constraints. The basic to/from string
  /// type functions are provided pure-virtual. The type-aware implementations which implement these
  /// functions are templated classes. One reason for providing a non-templated base class is so
  /// that they can all be mixed into the same containers (albiet as pointers).
  struct OptionDefinitionBase
  {
    OptionDefinitionBase(std::string section_, std::string name_, bool required_);
    OptionDefinitionBase(
        std::string section_, std::string name_, bool required_, bool multiValued_);

    virtual ~OptionDefinitionBase()
    {
    }

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

    /// Subclasses should provide the number of values found.
    ///
    /// @return number of values found
    virtual size_t
    getNumberFound() const = 0;

    /// Subclasess should write their parsed value as a string, optionally falling back to any
    /// specified default if `useDefault` is true.
    ///
    /// @param useDefault should specify whether to fallback to default when possible
    /// @return the option's value as a string
    virtual std::string
    valueAsString(bool useDefault) = 0;

    /// Subclassess should call their acceptor, if present. See OptionDefinition for more details.
    ///
    /// @throws if the acceptor throws or the option is required but missing
    virtual void
    tryAccept() const = 0;

    std::string section;
    std::string name;
    bool required = false;
    bool multiValued = false;
  };

  /// The primary type-aware implementation of OptionDefinitionBase, this templated class allows
  /// for implementations which can use the std::ostringstream and std::istringstream for to/from
  /// string functionality.
  ///
  /// Note that types (T) used as template parameters here must be used verbatim when calling
  /// ConfigDefinition::getConfigValue(). Similar types such as uint32_t and int32_t cannot be
  /// mixed.
  template <typename T>
  struct OptionDefinition : public OptionDefinitionBase
  {
    /// Constructor. Arguments are passed directly to OptionDefinitionBase.
    ///
    /// @param defaultValue_ is used in the following situations:
    /// 1) as the return value for getValue() if there is no parsed value and required==false
    /// 2) as the output in defaultValueAsString(), used to generate config files
    /// 3) as the output in valueAsString(), used to generate config files
    ///
    /// @param acceptor_ is an optional function whose purpose is to both validate the parsed
    ///        input and internalize it (e.g. copy it for runtime use). The acceptor should throw
    ///        an exception with a useful message if it is not acceptable.
    OptionDefinition(
        std::string section_,
        std::string name_,
        bool required_,
        std::optional<T> defaultValue_,
        std::function<void(T)> acceptor_ = nullptr)
        : OptionDefinitionBase(section_, name_, required_)
        , defaultValue(defaultValue_)
        , acceptor(acceptor_)
    {
    }

    /// As above, but also takes a bool value for multiValued.
    OptionDefinition(
        std::string section_,
        std::string name_,
        bool required_,
        bool multiValued_,
        std::optional<T> defaultValue_,
        std::function<void(T)> acceptor_ = nullptr)
        : OptionDefinitionBase(section_, name_, required_, multiValued_)
        , defaultValue(defaultValue_)
        , acceptor(acceptor_)
    {
    }

    /// Returns the first parsed value, if available. Otherwise, provides the default value if the
    /// option is not required. Otherwise, returns an empty optional.
    ///
    /// @return an optional with the parsed value, the default value, or no value.
    std::optional<T>
    getValue() const
    {
      if (parsedValues.size())
        return parsedValues[0];
      else if (not required and not multiValued)
        return defaultValue.value();
      else
        return {};
    }

    /// Returns the value at the given index.
    ///
    /// @param index
    /// @return the value at the given index, if it exists
    /// @throws range_error exception if index >= size
    T
    getValueAt(size_t index) const
    {
      if (index >= parsedValues.size())
        throw std::range_error(
            stringify("no value at index ", index, ", size: ", parsedValues.size()));

      return parsedValues[index];
    }

    /// Returns the number of values found.
    ///
    /// @return number of values found
    size_t
    getNumberFound() const override
    {
      return parsedValues.size();
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
      if (not multiValued and parsedValues.size() > 0)
      {
        throw std::invalid_argument(
            stringify("duplicate value for ", name, ", previous value: ", parsedValues[0]));
      }

      parsedValues.emplace_back(fromString(input));
    }

    T
    fromString(const std::string& input)
    {
      std::istringstream iss(input);
      T t;
      iss >> t;
      if (iss.fail())
        throw std::invalid_argument(stringify(input, " is not a valid ", typeid(T).name()));
      else
        return t;
    }

    std::string
    valueAsString(bool useDefault) override
    {
      std::ostringstream oss;
      if (parsedValues.size() > 0)
        oss << parsedValues[0];
      else if (useDefault and defaultValue.has_value())
        oss << defaultValue.value();

      return oss.str();
    }

    /// Attempts to call the acceptor function, if present. This function may throw if the value is
    /// not acceptable. Additionally, tryAccept should not be called if the option is required and
    /// no value has been provided.
    ///
    /// @throws if required and no value present or if the acceptor throws
    void
    tryAccept() const override
    {
      if (required and parsedValues.size() == 0)
      {
        throw std::runtime_error(stringify(
            "cannot call tryAccept() on [",
            section,
            "]:",
            name,
            " when required but no value available"));
      }

      // don't use default value if we are multi-valued and have no value
      if (multiValued && parsedValues.size() == 0)
        return;

      if (acceptor)
      {
        if (multiValued)
        {
          for (const auto& value : parsedValues)
          {
            acceptor(value);
          }
        }
        else
        {
          auto maybe = getValue();
          assert(maybe.has_value());  // should be guaranteed by our earlier checks
          // TODO: avoid copies here if possible
          acceptor(maybe.value());
        }
      }
    }

    std::optional<T> defaultValue;
    std::vector<T> parsedValues;
    std::function<void(T)> acceptor;
  };

  /// Specialization for bool types. We don't want to use stringstream parsing in this
  /// case because we want to accept "truthy" and "falsy" string values (e.g. "off" == false)
  template <>
  bool
  OptionDefinition<bool>::fromString(const std::string& input);

  using UndeclaredValueHandler =
      std::function<void(std::string_view section, std::string_view name, std::string_view value)>;

  using OptionDefinition_ptr = std::unique_ptr<OptionDefinitionBase>;

  // map of k:v pairs
  using DefinitionMap = std::unordered_map<std::string, OptionDefinition_ptr>;

  // map of section-name to map-of-definitions
  using SectionMap = std::unordered_map<std::string, DefinitionMap>;

  /// A ConfigDefinition holds an ordered set of OptionDefinitions defining the allowable values and
  /// their constraints (specified through calls to defineOption()).
  ///
  /// The layout and grouping of the config options are modelled after the INI file format; each
  /// option has a name and is grouped under a section. Duplicate option names are allowed only if
  /// they exist in a different section. The ConfigDefinition can be serialized in the INI file
  /// format using the generateINIConfig() function.
  ///
  /// Configured values (e.g. those encountered when parsing a file) can be provided through calls
  /// to addConfigValue(). These take a std::string as a value, which is automatically parsed.
  ///
  /// The ConfigDefinition can be used to print out a full config string (or file), including fields
  /// with defaults and optionally fields which have a specified value (values provided through
  /// calls to addConfigValue()).
  struct ConfigDefinition
  {
    /// Spefify the parameters and type of a configuration option. The parameters are members of
    /// OptionDefinitionBase; the type is inferred from OptionDefinition's template parameter T.
    ///
    /// This function should be called for every option that this Configuration supports, and should
    /// be done before any other interractions involving that option.
    ///
    /// @param def should be a unique_ptr to a valid subclass of OptionDefinitionBase
    /// @return `*this` for chaining calls
    /// @throws std::invalid_argument if the option already exists
    ConfigDefinition&
    defineOption(OptionDefinition_ptr def);

    /// Convenience function which calls defineOption with a OptionDefinition of the specified type
    /// and with parameters passed through to OptionDefinition's constructor.
    template <typename T, typename... Params>
    ConfigDefinition&
    defineOption(Params&&... args)
    {
      return defineOption(std::make_unique<OptionDefinition<T>>(args...));
    }

    /// Specify a config value for the given section and name. The value should be a valid string
    /// representing the type used by the option (e.g. the type provided when defineOption() was
    /// called).
    ///
    /// If the specified option doesn't exist, an exception will be thrown. Otherwise, the option's
    /// parseValue() will be invoked, and should throw an exception if the string can't be parsed.
    ///
    /// @param section is the section this value resides in
    /// @param name is the name of the value
    /// @return `*this` for chaining calls
    /// @throws if the option doesn't exist or the provided string isn't parseable
    ConfigDefinition&
    addConfigValue(std::string_view section, std::string_view name, std::string_view value);

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
    template <typename T>
    std::optional<T>
    getConfigValue(std::string_view section, std::string_view name)
    {
      OptionDefinition_ptr& definition = lookupDefinitionOrThrow(section, name);

      auto derived = dynamic_cast<const OptionDefinition<T>*>(definition.get());
      if (not derived)
        throw std::invalid_argument(
            stringify("", typeid(T).name(), " is the incorrect type for [", section, "]:", name));

      return derived->getValue();
    }

    /// Add an "undeclared" handler for the given section. This is a handler that will be called
    /// whenever a k:v pair is found that doesn't match a provided definition.
    ///
    /// Any exception thrown by the handler will progagate back through the call to
    /// addConfigValue().
    ///
    /// @param section is the section for which any undeclared values will invoke the provided
    ///        handler
    /// @param handler
    /// @throws if there is already a handler for this section
    void
    addUndeclaredHandler(const std::string& section, UndeclaredValueHandler handler);

    /// Removes an "undeclared" handler for the given section.
    ///
    /// @param section is the section which we want to remove the handler for
    void
    removeUndeclaredHandler(const std::string& section);

    /// Validate that all required fields are present.
    ///
    /// @throws std::invalid_argument if configuration constraints are not met
    void
    validateRequiredFields();

    /// Accept all options. This will call the acceptor (if present) on each option. Note that this
    /// should only be called if all required fields are present (that is, validateRequiredFields()
    /// has been or could be called without throwing).
    ///
    /// @throws if any option's acceptor throws
    void
    acceptAllOptions();

    /// Add comments for a given section. Comments are replayed in-order during config file
    /// generation. A proper comment prefix will automatically be applied, and the entire comment
    /// will otherwise be used verbatim (no automatic line separation, etc.).
    ///
    /// @param section
    /// @param comment
    void
    addSectionComments(const std::string& section, std::vector<std::string> comments);

    /// Add comments for a given option. Similar to addSectionComment, but applies to a specific
    /// [section]:name pair.
    ///
    /// @param section
    /// @param name
    /// @param comment
    void
    addOptionComments(
        const std::string& section, const std::string& name, std::vector<std::string> comments);

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
    OptionDefinition_ptr&
    lookupDefinitionOrThrow(std::string_view section, std::string_view name);
    const OptionDefinition_ptr&
    lookupDefinitionOrThrow(std::string_view section, std::string_view name) const;

    using SectionVisitor = std::function<void(const std::string&, const DefinitionMap&)>;
    void
    visitSections(SectionVisitor visitor) const;

    using DefVisitor = std::function<void(const std::string&, const OptionDefinition_ptr&)>;
    void
    visitDefinitions(const std::string& section, DefVisitor visitor) const;

    SectionMap m_definitions;

    std::unordered_map<std::string, UndeclaredValueHandler> m_undeclaredHandlers;

    // track insertion order. the vector<string>s are ordered list of section/option names.
    std::vector<std::string> m_sectionOrdering;
    std::unordered_map<std::string, std::vector<std::string>> m_definitionOrdering;

    // comments for config file generation
    using CommentList = std::vector<std::string>;
    using CommentsMap = std::unordered_map<std::string, CommentList>;
    CommentsMap m_sectionComments;
    std::unordered_map<std::string, CommentsMap> m_definitionComments;
  };

  /// A convenience acceptor which takes a reference and later assigns it in its acceptor call.
  ///
  /// Note that this holds on to a reference; it must only be used when this is safe to do. In
  /// particular, a reference to a local variable may be problematic.
  template <typename T>
  std::function<void(T)>
  AssignmentAcceptor(T& ref)
  {
    return [&](T arg) mutable { ref = std::move(arg); };
  }

}  // namespace llarp

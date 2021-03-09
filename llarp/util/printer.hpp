#pragma once

#include <llarp/util/meta/traits.hpp>

#include <functional>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <string_view>

namespace llarp
{
  /// simple guard class to restore stream flags.
  struct FormatFlagsGuard
  {
    std::ios_base& m_base;
    std::ios_base::fmtflags m_flags;

    FormatFlagsGuard(std::ios_base& base) : m_base(base), m_flags(base.flags())
    {}

    ~FormatFlagsGuard()
    {
      m_base.flags(m_flags);
    }
  };

  /// A general-purpose, stateful printer class.
  class Printer
  {
   private:
    std::ostream& m_stream;
    const int m_level;
    const int m_levelPlusOne;
    const bool m_suppressIndent;
    const int m_spaces;

   public:
    template <typename Type>
    using PrintFunction = std::function<std::ostream&(std::ostream&, const Type&, int, int)>;

    /// Create a printer.
    /// - level: the indentation level to use. If negative, suppress indentation
    /// on the first line.
    /// - spaces: the number of spaces to indent. If negative, put all output on
    /// a single line
    Printer(std::ostream& stream, int level, int spaces);

    ~Printer();

    /// Print the given `data` to the stream, using the following strategies:
    /// - If `Type` is fundamental, print to stream
    /// - If `Type` is a C-style array (and not a char array), print each
    /// element to the stream
    /// - If `Type` is a `void *`, `const void *` or function pointer, and not
    /// null, print in hex format or print "null".
    /// - If `Type` is a `char *`, a `const char *`, a C-style char array, a
    /// `std::string` or `std::string_view` print the string wrapped in `"`.
    /// - If `Type` is a pointer type, print the pointer, followed by the value
    /// if not-null.
    /// - If `Type` is a pair/tuple type, print the elements of the tuple.
    /// - If `Type` has STL-style iterators, print all elements in the
    /// container.
    /// - If `Type` is any other type, call the `print` method on that type.
    template <typename Type>
    void
    printAttribute(std::string_view name, const Type& value) const;

    template <typename Type>
    void
    printAttributeAsHex(std::string_view name, const Type& value) const;

    template <typename InputIt>
    void
    printAttribute(std::string_view name, const InputIt& begin, const InputIt& end) const;

    template <typename Type>
    void
    printValue(const Type& value) const;

    template <typename InputIt>
    void
    printValue(const InputIt& begin, const InputIt& end) const;

    template <typename Type>
    void
    printForeignAttribute(
        std::string_view name, const Type& value, const PrintFunction<Type>& printFunction) const;

    template <typename Type>
    void
    printForeignValue(const Type& value, const PrintFunction<Type>& printFunction) const;

    void
    printHexAddr(std::string_view name, const void* address) const;
    void
    printHexAddr(const void* address) const;

    template <class Type>
    void
    printOrNull(std::string_view name, const Type& address) const;
    template <class Type>
    void
    printOrNull(const Type& address) const;

   private:
    void
    printIndent() const;
  };

  /// helper struct
  struct PrintHelper
  {
    template <typename Type>
    static void
    print(std::ostream& stream, const Type& value, int level, int spaces);

    template <typename InputIt>
    static void
    print(std::ostream& stream, const InputIt& begin, const InputIt& end, int level, int spaces);

    // Specialisations

    // Fundamental types
    static void
    printType(
        std::ostream& stream,
        char value,
        int level,
        int spaces,
        traits::select::Case<std::is_fundamental>);

    static void
    printType(
        std::ostream& stream,
        bool value,
        int level,
        int spaces,
        traits::select::Case<std::is_fundamental>);

    template <typename Type>
    static void
    printType(
        std::ostream& stream,
        Type value,
        int level,
        int spaces,
        traits::select::Case<std::is_fundamental>);

    template <typename Type>
    static void
    printType(
        std::ostream& stream,
        Type value,
        int level,
        int spaces,
        traits::select::Case<std::is_enum>);

    // Function types
    template <typename Type>
    static void
    printType(
        std::ostream& stream,
        Type value,
        int level,
        int spaces,
        traits::select::Case<std::is_function>);

    // Pointer types
    static void
    printType(
        std::ostream& stream,
        const char* value,
        int level,
        int spaces,
        traits::select::Case<std::is_pointer>);

    static void
    printType(
        std::ostream& stream,
        const void* value,
        int level,
        int spaces,
        traits::select::Case<std::is_pointer>);

    template <typename Type>
    static void
    printType(
        std::ostream& stream,
        const Type* value,
        int level,
        int spaces,
        traits::select::Case<std::is_pointer>);

    template <typename Type>
    static void
    printType(
        std::ostream& stream,
        const Type* value,
        int level,
        int spaces,
        traits::select::Case<std::is_array>);

    // Container types
    static void
    printType(
        std::ostream& stream,
        const std::string& value,
        int level,
        int spaces,
        traits::select::Case<traits::is_container>);

    static void
    printType(
        std::ostream& stream,
        const std::string_view& value,
        int level,
        int spaces,
        traits::select::Case<traits::is_container>);

    template <typename Type>
    static void
    printType(
        std::ostream& stream,
        const Type& value,
        int level,
        int spaces,
        traits::select::Case<traits::is_container>);

    // Utility types
    template <typename Type1, typename Type2>
    static void
    printType(
        std::ostream& stream,
        const std::pair<Type1, Type2>& value,
        int level,
        int spaces,
        traits::select::Case<>);

    template <typename... Types>
    static void
    printType(
        std::ostream& stream,
        const std::tuple<Types...>& value,
        int level,
        int spaces,
        traits::select::Case<>);

    // Default type
    template <typename Type>
    static void
    printType(
        std::ostream& stream, const Type& value, int level, int spaces, traits::select::Case<>);
  };

  template <typename Type>
  inline void
  Printer::printAttribute(std::string_view name, const Type& value) const
  {
    assert(!name.empty());
    printIndent();

    m_stream << name << " = ";

    PrintHelper::print(m_stream, value, -m_levelPlusOne, m_spaces);
  }

  template <typename Type>
  inline void
  Printer::printAttributeAsHex(std::string_view name, const Type& value) const
  {
    static_assert(std::is_integral<Type>::value, "type should be integral");
    assert(!name.empty());
    printIndent();

    m_stream << name << " = ";
    {
      FormatFlagsGuard guard(m_stream);
      m_stream << std::hex << value;
    }

    if (m_spaces >= 0)
    {
      m_stream << '\n';
    }
  }

  template <typename InputIt>
  inline void
  Printer::printAttribute(std::string_view name, const InputIt& begin, const InputIt& end) const
  {
    assert(!name.empty());
    printIndent();

    m_stream << name << " = ";

    PrintHelper::print(m_stream, begin, end, -m_levelPlusOne, m_spaces);
  }

  template <typename Type>
  inline void
  Printer::printValue(const Type& value) const
  {
    printIndent();

    PrintHelper::print(m_stream, value, -m_levelPlusOne, m_spaces);
  }

  template <typename InputIt>
  inline void
  Printer::printValue(const InputIt& begin, const InputIt& end) const
  {
    printIndent();

    PrintHelper::print(m_stream, begin, end, -m_levelPlusOne, m_spaces);
  }

  template <typename Type>
  inline void
  Printer::printForeignAttribute(
      std::string_view name, const Type& value, const PrintFunction<Type>& printFunction) const
  {
    assert(!name.empty());
    printIndent();

    m_stream << name << " = ";

    printFunction(m_stream, value, -m_levelPlusOne, m_spaces);
  }

  template <typename Type>
  inline void
  Printer::printForeignValue(const Type& value, const PrintFunction<Type>& printFunction) const
  {
    printIndent();

    printFunction(m_stream, value, -m_levelPlusOne, m_spaces);
  }

  template <typename Type>
  inline void
  Printer::printOrNull(std::string_view name, const Type& address) const
  {
    assert(!name.empty());
    printIndent();

    m_stream << name << " = ";

    if (address == nullptr)
    {
      m_stream << "null";

      if (m_spaces >= 0)
      {
        m_stream << '\n';
      }
    }
    else
    {
      PrintHelper::print(m_stream, *address, -m_levelPlusOne, m_spaces);
    }
  }
  template <typename Type>
  inline void
  Printer::printOrNull(const Type& address) const
  {
    printIndent();

    if (address == nullptr)
    {
      m_stream << "null";

      if (m_spaces >= 0)
      {
        m_stream << '\n';
      }
    }
    else
    {
      PrintHelper::print(m_stream, *address, -m_levelPlusOne, m_spaces);
    }
  }

  template <>
  inline void
  Printer::printOrNull<const void*>(std::string_view name, const void* const& address) const
  {
    assert(!name.empty());
    printIndent();

    m_stream << name << " = ";
    const void* temp = address;

    PrintHelper::print(m_stream, temp, -m_levelPlusOne, m_spaces);
  }
  template <>
  inline void
  Printer::printOrNull<void*>(std::string_view name, void* const& address) const
  {
    const void* const& temp = address;

    printOrNull(name, temp);
  }

  template <>
  inline void
  Printer::printOrNull<const void*>(const void* const& address) const
  {
    printIndent();

    const void* temp = address;

    PrintHelper::print(m_stream, temp, -m_levelPlusOne, m_spaces);
  }

  template <>
  inline void
  Printer::printOrNull<void*>(void* const& address) const
  {
    const void* const& temp = address;

    printOrNull(temp);
  }

  // Print Helper methods

  template <typename InputIt>
  inline void
  PrintHelper::print(
      std::ostream& stream, const InputIt& begin, const InputIt& end, int level, int spaces)
  {
    Printer printer(stream, level, spaces);
    std::for_each(begin, end, [&](const auto& x) { printer.printValue(x); });
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream, Type value, int, int spaces, traits::select::Case<std::is_fundamental>)
  {
    stream << value;
    if (spaces >= 0)
    {
      stream << '\n';
    }
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream, Type value, int, int spaces, traits::select::Case<std::is_enum>)
  {
    printType(stream, value, 0, spaces, traits::select::Case<std::is_fundamental>());
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream,
      Type value,
      int level,
      int spaces,
      traits::select::Case<std::is_function>)
  {
    PrintHelper::print(stream, reinterpret_cast<const void*>(value), level, spaces);
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream,
      const Type* value,
      int level,
      int spaces,
      traits::select::Case<std::is_pointer>)
  {
    printType(
        stream,
        static_cast<const void*>(value),
        level,
        -1,
        traits::select::Case<std::is_pointer>());
    if (value == nullptr)
    {
      if (spaces >= 0)
      {
        stream << '\n';
      }
    }
    else
    {
      stream << ' ';
      PrintHelper::print(stream, *value, level, spaces);
    }
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream,
      const Type* value,
      int level,
      int spaces,
      traits::select::Case<std::is_array>)
  {
    printType(stream, value, level, spaces, traits::select::Case<std::is_pointer>());
  }

  inline void
  PrintHelper::printType(
      std::ostream& stream,
      const std::string& value,
      int level,
      int spaces,
      traits::select::Case<traits::is_container>)
  {
    printType(stream, value.c_str(), level, spaces, traits::select::Case<std::is_pointer>());
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream,
      const Type& value,
      int level,
      int spaces,
      traits::select::Case<traits::is_container>)
  {
    print(stream, value.begin(), value.end(), level, spaces);
  }

  template <typename Type1, typename Type2>
  inline void
  PrintHelper::printType(
      std::ostream& stream,
      const std::pair<Type1, Type2>& value,
      int level,
      int spaces,
      traits::select::Case<>)
  {
    Printer print(stream, level, spaces);
    print.printValue(value.first);
    print.printValue(value.second);
  }

  template <typename... Types>
  inline void
  PrintHelper::printType(
      std::ostream& stream,
      const std::tuple<Types...>& value,
      int level,
      int spaces,
      traits::select::Case<>)
  {
    Printer print(stream, level, spaces);
    traits::for_each_in_tuple(value, [&](const auto& x) { print.printValue(x); });
  }

  template <typename Type>
  inline void
  PrintHelper::printType(
      std::ostream& stream, const Type& value, int level, int spaces, traits::select::Case<>)
  {
    value.print(stream, level, spaces);
  }

  template <typename Type>
  inline void
  PrintHelper::print(std::ostream& stream, const Type& value, int level, int spaces)
  {
    using Selection = traits::select::Select<
        Type,
        std::is_fundamental,
        std::is_enum,
        std::is_function,
        std::is_pointer,
        std::is_array,
        traits::is_container>;

    PrintHelper::printType(stream, value, level, spaces, Selection());
  }

}  // namespace llarp

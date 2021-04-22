#include "printer.hpp"

#include <cctype>

namespace llarp
{
  namespace
  {
    static void
    putSpaces(std::ostream& stream, size_t count)
    {
      // chunk n write
      static const char spaces[] = "                                      ";
      static constexpr size_t size = sizeof(spaces) - 1;

      while (size < count)
      {
        stream.write(spaces, size);
        count -= size;
      }

      if (count > 0)
      {
        stream.write(spaces, count);
      }
    }
  }  // namespace

  Printer::Printer(std::ostream& stream, int level, int spacesPerLevel)
      : m_stream(stream)
      , m_level(level < 0 ? -level : level)
      , m_levelPlusOne(m_level + 1)
      , m_suppressIndent(level < 0)
      , m_spaces(spacesPerLevel)
  {
    if (!m_suppressIndent)
    {
      const int absSpaces = m_spaces < 0 ? -m_spaces : m_spaces;
      putSpaces(m_stream, absSpaces * m_level);
    }

    m_stream << '[';
    if (m_spaces >= 0)
    {
      m_stream << '\n';
    }
  }

  Printer::~Printer()
  {
    putSpaces(m_stream, m_spaces < 0 ? 1 : m_spaces * m_level);
    m_stream << ']';
  }

  void
  Printer::printIndent() const
  {
    putSpaces(m_stream, m_spaces < 0 ? 1 : m_spaces * m_levelPlusOne);
  }

  void
  Printer::printHexAddr(std::string_view name, const void* address) const
  {
    printIndent();
    m_stream << name << " = ";

    PrintHelper::print(m_stream, address, -m_levelPlusOne, m_spaces);
  }
  void
  Printer::printHexAddr(const void* address) const
  {
    printIndent();
    PrintHelper::print(m_stream, address, -m_levelPlusOne, m_spaces);
  }

  void
  PrintHelper::printType(
      std::ostream& stream,
      char value,
      int,
      int spacesPerLevel,
      traits::select::Case<std::is_fundamental>)
  {
    if (std::isprint(static_cast<unsigned char>(value)))
    {
      stream << "'" << value << "'";
    }
    else
    {
#define PRINT_CONTROL_CHAR(x) \
  case x:                     \
    stream << #x;             \
    break;

      switch (value)
      {
        PRINT_CONTROL_CHAR('\n');
        PRINT_CONTROL_CHAR('\t');
        PRINT_CONTROL_CHAR('\0');
        default:
        {
          // Print as hex
          FormatFlagsGuard guard(stream);
          stream << std::hex << std::showbase
                 << static_cast<std::uintptr_t>(static_cast<unsigned char>(value));
        }
      }
    }

    if (spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(
      std::ostream& stream,
      bool value,
      int,
      int spacesPerLevel,
      traits::select::Case<std::is_fundamental>)
  {
    {
      FormatFlagsGuard guard(stream);
      stream << std::boolalpha << value;
    }

    if (spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(
      std::ostream& stream,
      const char* value,
      int,
      int spacesPerLevel,
      traits::select::Case<std::is_pointer>)
  {
    if (value == nullptr)
    {
      stream << "null";
    }
    else
    {
      stream << '"' << value << '"';
    }

    if (spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(
      std::ostream& stream,
      const void* value,
      int,
      int spacesPerLevel,
      traits::select::Case<std::is_pointer>)
  {
    if (value == nullptr)
    {
      stream << "null";
    }
    else
    {
      FormatFlagsGuard guard(stream);
      stream << std::hex << std::showbase << reinterpret_cast<std::uintptr_t>(value);
    }

    if (spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(
      std::ostream& stream,
      const std::string_view& value,
      int,
      int spacesPerLevel,
      traits::select::Case<traits::is_container>)
  {
    stream << '"' << value << '"';
    if (spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

}  // namespace llarp

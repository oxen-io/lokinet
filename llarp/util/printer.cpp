#include <util/printer.hpp>

namespace llarp
{
  namespace
  {
    void
    putSpaces(std::ostream& stream, size_t count)
    {
      // chunk n write
      // NOLINTNEXTLINE
      static const char spaces[]   = "                                      ";
      static constexpr size_t size = sizeof(spaces) - 1;

      while(size < count)
      {
        stream.write(spaces, size);
        count -= size;
      }

      if(count > 0)
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
    if(!m_suppressIndent)
    {
      const int absSpaces = m_spaces < 0 ? -m_spaces : m_spaces;
      putSpaces(m_stream, absSpaces * m_level);
    }

    m_stream << '[';
    if(m_spaces >= 0)
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
  Printer::printHexAddr(string_view name, const void* address) const
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
  PrintHelper::printType(std::ostream& stream, char value, int /*unused*/,
                         int spacesPerLevel,
                         traits::select::Case< std::is_fundamental > /*unused*/)
  {
    if(std::isprint(static_cast< unsigned char >(value)) != 0)
    {
      stream << "'" << value << "'";
    }
    else
    {
      switch(value)
      {
        case '\n':
          stream << '\n';
          break;
        case '\t':
          stream << '\t';
          break;
        case '\0':
          stream << '\0';
          break;
        default:
        {
          // Print as hex
          FormatFlagsGuard guard(stream);
          stream << std::hex << std::showbase
                 << static_cast< std::uintptr_t >(
                        static_cast< unsigned char >(value));
        }
      }
    }

    if(spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(std::ostream& stream, bool value, int /*unused*/,
                         int spacesPerLevel,
                         traits::select::Case< std::is_fundamental > /*unused*/)
  {
    {
      FormatFlagsGuard guard(stream);
      stream << std::boolalpha << value;
    }

    if(spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(std::ostream& stream, const char* value,
                         int /*unused*/, int spacesPerLevel,
                         traits::select::Case< std::is_pointer > /*unused*/)
  {
    if(value == nullptr)
    {
      stream << "null";
    }
    else
    {
      stream << '"' << value << '"';
    }

    if(spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(std::ostream& stream, const void* value,
                         int /*unused*/, int spacesPerLevel,
                         traits::select::Case< std::is_pointer > /*unused*/)
  {
    if(value == nullptr)
    {
      stream << "null";
    }
    else
    {
      FormatFlagsGuard guard(stream);
      // NOLINTNEXTLINE
      auto val = reinterpret_cast< std::uintptr_t >(value);
      stream << std::hex << std::showbase << val;
    }

    if(spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

  void
  PrintHelper::printType(
      std::ostream& stream, const string_view& value, int /*unused*/,
      int spacesPerLevel,
      traits::select::Case< traits::is_container > /*unused*/)
  {
    stream << '"' << value << '"';
    if(spacesPerLevel >= 0)
    {
      stream << '\n';
    }
  }

}  // namespace llarp

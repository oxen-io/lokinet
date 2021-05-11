#pragma once

#ifdef __cpp_lib_source_location
#include <source_location>
namespace slns = std;
#else
namespace slns
{
  struct source_location
  {
   public:
    static constexpr source_location
    current(
        const char* fileName = __builtin_FILE(),
        const char* functionName = __builtin_FUNCTION(),
        const uint_least32_t lineNumber = __builtin_LINE(),
#if defined(__GNUC__) and (__GNUC__ > 4 or (__GNUC__ == 4 and __GNUC_MINOR__ >= 8))
        const uint_least32_t columnOffset = 0)
#else
        const uint_least32_t columnOffset = __builtin_COLUMN())
#endif
        noexcept
    {
      return source_location{fileName, functionName, lineNumber, columnOffset};
    }

    source_location(const source_location&) = default;
    source_location(source_location&&) = default;

    constexpr const char*
    file_name() const noexcept
    {
      return fileName;
    }

    constexpr const char*
    function_name() const noexcept
    {
      return functionName;
    }

    constexpr uint_least32_t
    line() const noexcept
    {
      return lineNumber;
    }

    constexpr std::uint_least32_t
    column() const noexcept
    {
      return columnOffset;
    }

   private:
    constexpr explicit source_location(
        const char* fileName,
        const char* functionName,
        const uint_least32_t lineNumber,
        const uint_least32_t columnOffset) noexcept
        : fileName(fileName)
        , functionName(functionName)
        , lineNumber(lineNumber)
        , columnOffset(columnOffset)
    {}

    const char* fileName;
    const char* functionName;
    const std::uint_least32_t lineNumber;
    const std::uint_least32_t columnOffset;
  };
}  // namespace slns
#endif

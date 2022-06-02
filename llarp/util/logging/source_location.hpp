#pragma once

#ifdef __cpp_lib_source_location
#include <source_location>
namespace slns = std;
#else
#include <cstdint>
namespace slns
{
  struct source_location
  {
   public:
    static constexpr source_location
    current(
        const char* fileName = __builtin_FILE(),
        const char* functionName = __builtin_FUNCTION(),
        const uint_least32_t lineNumber = __builtin_LINE()) noexcept
    {
      return source_location{fileName, functionName, lineNumber};
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

   private:
    constexpr explicit source_location(
        const char* fileName, const char* functionName, const uint_least32_t lineNumber) noexcept
        : fileName(fileName), functionName(functionName), lineNumber(lineNumber)
    {}

    const char* const fileName;
    const char* const functionName;
    const uint_least32_t lineNumber;
  };
}  // namespace slns
#endif

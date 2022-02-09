#pragma once
#include <string>
#include <type_traits>
#include <windows.h>

namespace
{
  static std::string
  utf8_encode(const std::wstring& wstr)
  {
    if (wstr.empty())
      return std::string();
    int size_needed =
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
  }

  // Convert an UTF8 string to a wide Unicode String
  static std::wstring
  utf8_decode(const std::string& str)
  {
    if (str.empty())
      return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
  }

  /// convert between std::string or std::wstring
  template <typename OutStr, typename InStr>
  OutStr
  to_width(const InStr& str)
  {
    if constexpr (std::is_same<typename InStr::value_type, wchar_t>::value)
    {
      return utf8_encode(str);
    }
    else
    {
      return utf8_decode(str);
    }
  }
}  // namespace

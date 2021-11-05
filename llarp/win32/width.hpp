#pragma once

namespace
{
  /// convert between std::string or std::wstring
  /// XXX: this function is shit
  template <typename OutStr, typename InStr>
  OutStr
  to_width(const InStr& str)
  {
    OutStr ostr{};
    typename OutStr::value_type buf[2] = {};

    for (const auto& ch : str)
    {
      buf[0] = static_cast<typename OutStr::value_type>(ch);
      ostr.append(buf);
    }
    return ostr;
  }
}  // namespace

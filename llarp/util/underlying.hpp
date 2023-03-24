#pragma once
#include <utility>

namespace llarp
{

#ifdef __cpp_lib_to_underlying
  using to_underlying = std::to_underlying;
#else
  template <typename T>
  constexpr auto
  to_underlying(T t)
  {
    return static_cast<std::underlying_type_t<T>>(t);
  }
#endif
}  // namespace llarp

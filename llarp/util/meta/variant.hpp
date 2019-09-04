#ifndef LLARP_VARIANT_HPP
#define LLARP_VARIANT_HPP

#include <absl/types/variant.h>

namespace llarp
{
  namespace util
  {
    template < typename... Ts >
    struct _overloaded;

    template < typename T, typename... Ts >
    struct _overloaded< T, Ts... > : T, _overloaded< Ts... >
    {
      _overloaded(T&& t, Ts&&... ts)
          : T(t), _overloaded< Ts... >(std::forward< Ts >(ts)...)
      {
      }
      using T::operator();

      using _overloaded< Ts... >::operator();
    };

    template < typename T >
    struct _overloaded< T > : T
    {
      _overloaded(T&& t) : T(t)
      {
      }

      using T::operator();
    };

    template < typename... Ts >
    constexpr auto
    overloaded(Ts&&... ts) -> _overloaded< Ts... >
    {
      return _overloaded< Ts... >(std::forward< Ts >(ts)...);
    }

  }  // namespace util
}  // namespace llarp

#endif

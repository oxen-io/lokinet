#ifndef LLARP_UTIL_MEMFN
#define LLARP_UTIL_MEMFN

#include <type_traits>
#include <utility>
#include <memory>

namespace llarp
{
  namespace util
  {
    // Wraps a member function and instance into a callable object that invokes
    // the method (non-const overload).
    template <
        typename Return,
        typename Class,
        typename Derived,
        typename... Arg,
        typename = std::enable_if_t<std::is_base_of<Class, Derived>::value>>
    auto
    memFn(Return (Class::*f)(Arg...), Derived* self)
    {
      return [f, self](Arg... args) -> Return { return (self->*f)(std::forward<Arg>(args)...); };
    }

    // Wraps a member function and instance into a lambda that invokes the
    // method (const overload).
    template <
        typename Return,
        typename Class,
        typename Derived,
        typename... Arg,
        typename = std::enable_if_t<std::is_base_of<Class, Derived>::value>>
    auto
    memFn(Return (Class::*f)(Arg...) const, const Derived* self)
    {
      return [f, self](Arg... args) -> Return { return (self->*f)(std::forward<Arg>(args)...); };
    }

    // Wraps a member function and shared pointer to an instance into a lambda
    // that invokes the method.
    template <
        typename Return,
        typename Class,
        typename Derived,
        typename... Arg,
        typename = std::enable_if_t<std::is_base_of<Class, Derived>::value>>
    auto
    memFn(Return (Class::*f)(Arg...), std::shared_ptr<Derived> self)
    {
      return [f, self = std::move(self)](Arg... args) -> Return {
        return (self.get()->*f)(std::forward<Arg>(args)...);
      };
    }

    // Wraps a member function and shared pointer to an instance into a lambda
    // that invokes the method (const method overload).
    template <
        typename Return,
        typename Class,
        typename Derived,
        typename... Arg,
        typename = std::enable_if_t<std::is_base_of<Class, Derived>::value>>
    auto
    memFn(Return (Class::*f)(Arg...) const, std::shared_ptr<Derived> self)
    {
      return [f, self = std::move(self)](Arg... args) -> Return {
        return (self.get()->*f)(std::forward<Arg>(args)...);
      };
    }

  }  // namespace util
}  // namespace llarp

#endif

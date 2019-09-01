#ifndef LLARP_UTIL_MEMFN
#define LLARP_UTIL_MEMFN

#include <util/meta/memfn_traits.hpp>
#include <util/meta/object.hpp>
#include <util/meta/traits.hpp>

#include <functional>
#include <utility>

namespace llarp
{
  namespace util
  {
    template < typename Obj >
    struct MemFnDereference
    {
      // clang-format off
      static inline Obj& derefImp(Obj& obj, std::false_type)
      {
        return obj;
      }

      template < typename Type >
      static inline Obj& derefImp(Type& obj, std::true_type)
      {
        return *obj;
      }

      template < typename Type >
      static inline Obj& derefImp(const Type& obj, std::true_type)
      {
        return *obj;
      }

      template < typename Type >
      static inline Obj& deref(Type& obj)
      {
        return derefImp(obj, traits::is_pointy< Type >());
      }

      template < typename Type >
      static inline Obj& deref(const Type& obj)
      {
        return derefImp(obj, traits::is_pointy< Type >());
      }
      // clang-format on
    };

    template < typename Prototype, typename Instance >
    class MemFn
    {
      using traits = MemFnTraits< Prototype >;

      static_assert(traits::IsMemFn, "");

     public:
      using result_type = typename traits::result_type;

     private:
      Prototype m_func;
      object::Proxy< Instance > m_instance;

      using Deref = MemFnDereference< typename traits::class_type >;

     public:
      MemFn(Prototype prototype, const Instance& instance)
          : m_func(prototype), m_instance(instance)
      {
      }

      template < typename... Args >
      result_type
      operator()(Args&&... args) const
      {
        return (Deref::deref(m_instance.value())
                .*m_func)(std::forward< Args >(args)...);
      }
    };

    template < typename Prototype, typename Instance >
    MemFn< Prototype, Instance >
    memFn(Prototype prototype, const Instance& instance)
    {
      return MemFn< Prototype, Instance >(prototype, instance);
    }

  }  // namespace util
}  // namespace llarp

#endif

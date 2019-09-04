#ifndef LLARP_UTIL_MEMFN_TRAITS
#define LLARP_UTIL_MEMFN_TRAITS

#include <util/meta/object.hpp>
#include <util/meta/traits.hpp>

#include <functional>
#include <utility>

namespace llarp
{
  namespace util
  {
    template < typename Prototype, typename TestPrototype >
    struct MemFnTraitsImpl;

    template < typename Prototype >
    struct MemFnTraits : public MemFnTraitsImpl< Prototype, Prototype >
    {
    };

    template < typename Prototype, typename Return, typename Type,
               typename... Args >
    class MemFnTraitsClass
    {
      using NonCVTag    = traits::Tag< 0 >;
      using ConstTag    = traits::Tag< 1 >;
      using VolTag      = traits::Tag< 2 >;
      using ConstVolTag = traits::Tag< 3 >;

      // clang-format off
      static constexpr NonCVTag    test(Return (Type::*)(Args...));
      static constexpr ConstTag    test(Return (Type::*)(Args...) const);
      static constexpr VolTag      test(Return (Type::*)(Args...) volatile);
      static constexpr ConstVolTag test(Return (Type::*)(Args...) const volatile);
      // clang-format on
     public:
      static constexpr bool IsConst =
          ((sizeof((test)((Prototype)0)) - 1) & 1) != 0;
      static constexpr bool IsVolatile =
          ((sizeof((test)((Prototype)0)) - 1) & 2) != 0;

      using ctype = std::conditional_t< IsConst, const Type, Type >;
      using type  = std::conditional_t< IsVolatile, volatile ctype, ctype >;
    };

    template < typename Prototype, typename Return, typename Type,
               typename... Args >
    struct MemFnTraitsImpl< Prototype, Return (Type::*)(Args...) >
    {
      static constexpr bool IsMemFn = true;

      using class_type =
          typename MemFnTraitsClass< Prototype, Return, Type, Args... >::type;

      using result_type = Return;
    };

    template < typename Prototype, typename Return, typename Type,
               typename... Args >
    struct MemFnTraitsImpl< Prototype, Return (Type::*)(Args...) const >
    {
      static constexpr bool IsMemFn = true;

      using class_type =
          typename MemFnTraitsClass< Prototype, Return, Type, Args... >::type;

      using result_type = Return;
    };

    template < typename Prototype, typename Return, typename Type,
               typename... Args >
    struct MemFnTraitsImpl< Prototype, Return (Type::*)(Args...) volatile >
    {
      static constexpr bool IsMemFn = true;

      using class_type =
          typename MemFnTraitsClass< Prototype, Return, Type, Args... >::type;

      using result_type = Return;
    };

    template < typename Prototype, typename Return, typename Type,
               typename... Args >
    struct MemFnTraitsImpl< Prototype,
                            Return (Type::*)(Args...) const volatile >
    {
      static constexpr bool IsMemFn = true;

      using class_type =
          typename MemFnTraitsClass< Prototype, Return, Type, Args... >::type;

      using result_type = Return;
    };

    template < typename Prototype, typename TestPrototype >
    struct MemFnTraitsImpl
    {
      static constexpr bool IsMemFn = false;

      using result_type = void;
      using class_type  = void;
    };
  }  // namespace util
}  // namespace llarp

#endif

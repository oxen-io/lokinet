#ifndef LLARP_TRAITS_HPP
#define LLARP_TRAITS_HPP

#include <type_traits>

namespace llarp
{
  namespace traits
  {
    /// Represents the empty type
    struct Bottom
    {
    };

    /// C++17 compatibility. template pack
    template < class... >
    using void_t = void;

    /// Type trait representing whether a type is an STL-style container
    template < typename T, typename _ = void >
    struct is_container : public std::false_type
    {
    };

    // We take that the container has begin, end and size methods to be a
    // container.
    // clang-format off
    template < typename T >
    struct is_container<
        T,
        std::conditional_t<
            false,
            void_t< typename T::value_type,
                    typename T::size_type,
                    typename T::iterator,
                    typename T::const_iterator,
                    decltype(std::declval<T>().size()),
                    decltype(std::declval<T>().begin()),
                    decltype(std::declval<T>().end()),
                    decltype(std::declval<T>().cbegin()),
                    decltype(std::declval<T>().cend()) >,
            void > > : public std::true_type
    {
    };
    // clang-format on

    namespace Switch
    {
      template < size_t Selector, typename... Types >
      struct Switch
      {
        using Type = Bottom;
      };

      template < typename T, typename... Types >
      struct Switch< 0u, T, Types... >
      {
        using Type = T;
      };

      template < size_t Selector, typename Tn, typename... Types >
      struct Switch< Selector, Tn, Types... >
      {
        using Type = typename Switch< Selector - 1, Types... >::Type;
      };

    }  // namespace Switch

    namespace select
    {
      /// This provides a way to do a compile-type dispatch based on type traits

      /// meta function which always returns false
      template < typename >
      class False : public std::false_type
      {
      };

      /// a case in the selection
      template < template < typename... > class Trait = False >
      class Case
      {
       public:
        template < typename Type >
        struct Selector : public Trait< Type >::type
        {
        };

        using Type = Case;
      };

      // clang-format off

      /// implementation helper
      template < typename T,
                 template < typename > class Trait1,
                 template < typename > class Trait2,
                 template < typename > class Trait3,
                 template < typename > class Trait4,
                 template < typename > class Trait5,
                 template < typename > class Trait6,
                 template < typename > class Trait7,
                 template < typename > class Trait8,
                 template < typename > class Trait9 >
      struct SelectHelper {
          enum {
              Selector = (
                  Trait1<T>::value ? 1 :
                  Trait2<T>::value ? 2 :
                  Trait3<T>::value ? 3 :
                  Trait4<T>::value ? 4 :
                  Trait5<T>::value ? 5 :
                  Trait6<T>::value ? 6 :
                  Trait7<T>::value ? 7 :
                  Trait8<T>::value ? 8 :
                  Trait9<T>::value ? 9 : 0
              )
          };

          using Type = typename Switch::Switch<
                Selector,
                Case<>,
                Case<Trait1>,
                Case<Trait2>,
                Case<Trait3>,
                Case<Trait4>,
                Case<Trait5>,
                Case<Trait6>,
                Case<Trait7>,
                Case<Trait8>,
                Case<Trait9>
          >::Type;
      };

      template< typename Type,
                template < typename > class Trait1,
                template < typename > class Trait2 = False,
                template < typename > class Trait3 = False,
                template < typename > class Trait4 = False,
                template < typename > class Trait5 = False,
                template < typename > class Trait6 = False,
                template < typename > class Trait7 = False,
                template < typename > class Trait8 = False,
                template < typename > class Trait9 = False >
      struct Select : public SelectHelper< Type,
                                           Trait1,
                                           Trait2,
                                           Trait3,
                                           Trait4,
                                           Trait5,
                                           Trait6,
                                           Trait7,
                                           Trait8,
                                           Trait9 >::Type
      {
          enum {
              Selector = SelectHelper< Type,
                                       Trait1,
                                       Trait2,
                                       Trait3,
                                       Trait4,
                                       Trait5,
                                       Trait6,
                                       Trait7,
                                       Trait8,
                                       Trait9 >::Selector
          };

          using SelectorType = std::integral_constant<int, Selector>;
      };

      // clang-format on
    }  // namespace select
  }    // namespace traits
}  // namespace llarp

#endif

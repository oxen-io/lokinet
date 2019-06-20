#ifndef LLARP_UTIL_STATUS_HPP
#define LLARP_UTIL_STATUS_HPP

#include <util/string_view.hpp>

#include <nlohmann/json.hpp>

#include <vector>
#include <string>
#include <algorithm>
#include <absl/types/variant.h>

namespace llarp
{
  namespace util
  {
    struct StatusVisitor;
    struct StatusObject
    {
      using String_t = string_view;
      using Variant  = absl::variant< uint64_t, std::string, bool, StatusObject,
                                     std::vector< std::string >,
                                     std::vector< StatusObject > >;
      using value_type = std::pair< String_t, Variant >;

      StatusObject(std::initializer_list< value_type > vals)
      {
        std::for_each(vals.begin(), vals.end(),
                      [&](const value_type& item) { Put(item); });
      }

      void
      Put(String_t name, const Variant& data);

      void
      Put(const value_type& value);

      template < typename Container >
      void
      PutContainer(String_t keyname, const Container& container)
      {
        std::vector< util::StatusObject > objs;
        std::transform(container.begin(), container.end(),
                       std::back_inserter(objs),
                       [](const auto& item) -> util::StatusObject {
                         return item.second->ExtractStatus();
                       });
        Put(keyname, objs);
      }

      nlohmann::json
      get() const
      {
        return Impl;
      }

     private:
      friend struct StatusVisitor;
      nlohmann::json Impl;
    };
  }  // namespace util
}  // namespace llarp

#endif

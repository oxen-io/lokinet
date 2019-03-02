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
      Put(String_t name, const Variant& value);

      void
      Put(const value_type& value);

      nlohmann::json
      get() const
      {
        return Impl;
      }

     private:
      friend struct StatusVisitor;
      nlohmann::json Impl;
    };

    /// an entity that has a status that can be extracted
    struct IStateful
    {
      virtual ~IStateful(){};

      virtual StatusObject
      ExtractStatus() const = 0;
    };

  }  // namespace util
}  // namespace llarp

#endif

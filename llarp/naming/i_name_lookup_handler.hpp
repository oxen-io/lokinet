#ifndef LLARP_NAMING_I_NAME_LOOKUP_HANDLER_HPP
#define LLARP_NAMING_I_NAME_LOOKUP_HANDLER_HPP
#include <absl/types/optional.h>
#include <service/address.hpp>

namespace llarp
{
  namespace naming
  {
    using NameLookupResultHandler =
        std::function< void(absl::optional< llarp::service::Address >) >;

    struct INameLookupHandler
    {
      virtual ~INameLookupHandler() = default;

      virtual bool
      LookupNameAsync(const std::string, NameLookupResultHandler h) = 0;
    };

  }  // namespace naming
}  // namespace llarp

#endif

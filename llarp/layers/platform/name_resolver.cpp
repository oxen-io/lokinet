#include "name_resolver.hpp"
#include <functional>
#include <memory>

#include "name_cache.hpp"

#include "deprecated_pimpl.hpp"

#include <llarp/layers/flow/deprecated_flow_layer_pimpl.hpp>

namespace llarp::layers::platform
{

  NameResolver::NameResolver(NameCache& name_cache, flow::FlowLayer& flow_layer)
      : _name_cache{name_cache}, _flow_layer{flow_layer}
  {}

  namespace
  {
    /// converts an optional address variant to an optional flow addr in a callback.
    struct name_resolver_lookup_handler
    {
      std::function<void(std::optional<flow::FlowAddr>)> result_handler;
      void
      operator()(std::optional<EndpointBase::AddressVariant_t> result) const
      {
        if (result)

          result_handler(flow::FlowAddr{*result});
        else
          result_handler(std::nullopt);
      }
    };
  }  // namespace

  void
  NameResolver::resolve_flow_addr_async(
      std::string name, std::function<void(std::optional<flow::FlowAddr>)> result_handler)
  {
    // todo: use flow layer
    if (const auto& endpoint_ptr = _flow_layer.pimpl->endpoint)
    {
      endpoint_ptr->LookupNameAsync(
          std::move(name), name_resolver_lookup_handler{std::move(result_handler)});
    }
    else
      result_handler(std::nullopt);
  }

}  // namespace llarp::layers::platform

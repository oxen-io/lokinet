#include "name_resolver.hpp"
#include "flow_layer.hpp"
#include <llarp/service/endpoint.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace llarp::layers::flow
{

  NameResolver::NameResolver(NameCache& name_cache, FlowLayer& parent)
      : _name_cache{name_cache}, _parent{parent}
  {}

  void
  NameResolver::resolve_flow_addr_async(
      std::string name, std::function<void(std::optional<FlowAddr>)> result_handler)
  {
    _parent.local_deprecated_loki_endpoint()->LookupNameAsync(
        name, [result_handler = std::move(result_handler)](auto maybe_addr) {
          if (not maybe_addr)
          {
            result_handler(std::nullopt);
            return;
          }
          result_handler(FlowAddr{*maybe_addr});
        });
  }

}  // namespace llarp::layers::flow

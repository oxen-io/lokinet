#include "name_resolver.hpp"
#include "flow_addr.hpp"
#include "flow_layer.hpp"
#include <llarp/service/endpoint.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace llarp::layers::flow
{

  static auto logcat = log::Cat("flow-layer");

  NameResolver::NameResolver(NameCache& name_cache, FlowLayer& parent)
      : _name_cache{name_cache}, _parent{parent}
  {}

  void
  NameResolver::resolve_flow_addr_async(
      std::string name, std::function<void(std::optional<FlowAddr>)> result_handler)
  {
    _parent.local_deprecated_loki_endpoint()->LookupNameAsync(
        name, [result_handler = std::move(result_handler), name](auto maybe_addr) {
          if (not maybe_addr)
          {
            result_handler(std::nullopt);
            return;
          }
          result_handler(to_flow_addr(*maybe_addr));
        });
  }

  bool
  NameResolver::name_well_formed(std::string_view str)
  {
    // todo
    return true;
  }

}  // namespace llarp::layers::flow

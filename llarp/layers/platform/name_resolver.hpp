#pragma once

#include <llarp/layers/flow/flow_layer.hpp>

#include "platform_addr.hpp"
#include "name_cache.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include <oxenc/variant.h>

#include <llarp/util/types.hpp>
#include <llarp/dns/question.hpp>
#include <variant>
#include <vector>

namespace llarp::layers::platform
{
  class PlatformLayer;
  class NameCache;

  class NameResolver
  {
    NameCache& _name_cache;
    flow::FlowLayer& _flow_layer;

   public:
    NameResolver(NameCache& name_cache, flow::FlowLayer& flow_layer);

    /// resolve name using in network resolution
    void
    resolve_flow_addr_async(
        std::string name, std::function<void(std::optional<flow::FlowAddr>)> handler);
  };

}  // namespace llarp::layers::platform

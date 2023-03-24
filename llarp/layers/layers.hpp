#pragma once

#include <llarp/layers/platform/platform_layer.hpp>
#include <llarp/layers/flow/flow_layer.hpp>
#include <llarp/layers/route/route_layer.hpp>
#include <llarp/layers/onion/onion_layer.hpp>
#include <memory>
namespace llarp
{
  // forward declare
  struct AbstractRouter;
  struct Config;
}  // namespace llarp

namespace llarp::layers
{
  /// a holder type that abstracts out how lokinet works at each layer.
  /// this is owned by AbstractRouter, and in time all the members on this will be moved to
  /// AbstractRouter.
  struct Layers
  {
    std::unique_ptr<platform::PlatformLayer> platform;
    std::unique_ptr<flow::FlowLayer> flow;
    std::unique_ptr<route::RouteLayer> route;
    std::unique_ptr<onion::OnionLayer> onion;
    // TODO: add more layers as they are wired up

    /// move all owned members to a const version.
    std::unique_ptr<const Layers>
    freeze();

    void
    start_all() const;

    void
    stop_all() const;
  };

  /// this lets us hide which implementation we are using durring the refactor
  std::unique_ptr<Layers>
  make_layers(AbstractRouter& router, const Config& conf);

}  // namespace llarp::layers

#include "layers.hpp"
#include <memory>
#include <llarp/config/config.hpp>

namespace llarp::layers
{

  std::unique_ptr<Layers>
  make_layers(AbstractRouter& router, const Config& conf)
  {
    auto layers = std::make_unique<Layers>();
    // TODO: placeholders
    layers->onion = std::make_unique<onion::OnionLayer>(router);
    layers->route = std::make_unique<route::RouteLayer>(router);

    layers->flow = std::make_unique<flow::FlowLayer>(router, conf.network);
    layers->platform =
        std::make_unique<platform::PlatformLayer>(router, *layers->flow, conf.network);
    // TODO: add each layer as they are wired up

    return layers;
  }

  std::unique_ptr<const Layers>
  Layers::freeze()
  {
    return std::unique_ptr<const Layers>(
        new const Layers{std::move(platform), std::move(flow), std::move(route), std::move(onion)});
  }

  void
  Layers::start_all() const
  {
    platform->start();
    flow->start();
    onion->start();
  }

  void
  Layers::stop_all() const
  {
    platform->stop();
    flow->stop();
    onion->stop();
  }
}  // namespace llarp::layers

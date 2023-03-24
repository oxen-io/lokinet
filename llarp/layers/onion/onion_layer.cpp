
#include "onion_layer.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/ev/ev.hpp>

namespace llarp::layers::onion
{
  OnionStats
  OnionLayer::current_stats() const
  {
    // todo: implement
    return OnionStats{};
  }

  void
  OnionLayer::start()
  {}

  void
  OnionLayer::stop()
  {}

  OnionLayer::OnionLayer(AbstractRouter& router) : _router{router}
  {}
}  // namespace llarp::layers::onion

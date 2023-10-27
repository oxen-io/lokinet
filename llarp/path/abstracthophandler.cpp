#include "abstracthophandler.hpp"

#include <llarp/router/router.hpp>

namespace llarp::path
{
  // handle data in upstream direction
  bool
  AbstractHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    auto& pkt = m_UpstreamQueue.emplace_back();
    pkt.first.resize(X.sz);
    std::copy_n(X.base, X.sz, pkt.first.begin());
    pkt.second = Y;
    r->TriggerPump();
    return true;
  }

  // handle data in downstream direction
  bool
  AbstractHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router* r)
  {
    auto& pkt = m_DownstreamQueue.emplace_back();
    pkt.first.resize(X.sz);
    std::copy_n(X.base, X.sz, pkt.first.begin());
    pkt.second = Y;
    r->TriggerPump();
    return true;
  }

  void
  AbstractHopHandler::DecayFilters(llarp_time_t now)
  {
    m_UpstreamReplayFilter.Decay(now);
    m_DownstreamReplayFilter.Decay(now);
  }
}  // namespace llarp::path

#include "ihophandler.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/path/path_context.hpp>

namespace llarp
{
  namespace path
  {
    // handle data in upstream direction
    bool
    IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter* r)
    {
      auto& pkt = m_UpstreamQueue.emplace_back();
      pkt.first.resize(X.sz);
      std::copy_n(X.base, X.sz, pkt.first.begin());
      pkt.second = Y;
      r->pathContext().trigger_upstream_flush();
      return true;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter* r)
    {
      auto& pkt = m_DownstreamQueue.emplace_back();
      pkt.first.resize(X.sz);
      std::copy_n(X.base, X.sz, pkt.first.begin());
      pkt.second = Y;
      r->pathContext().trigger_downstream_flush();
      return true;
    }

    void
    IHopHandler::DecayFilters(llarp_time_t now)
    {
      m_UpstreamReplayFilter.Decay(now);
      m_DownstreamReplayFilter.Decay(now);
    }

    RouterID
    IHopHandler::upstream() const
    {
      auto info = hop_info();
      return info.upstream;
    }

    RouterID
    IHopHandler::downstream() const
    {
      auto info = hop_info();
      return info.downstream;
    }

    PathID_t
    IHopHandler::txID() const
    {
      auto info = hop_info();
      return info.txID;
    }

    PathID_t
    IHopHandler::rxID() const
    {
      auto info = hop_info();
      return info.rxID;
    }

  }  // namespace path
}  // namespace llarp

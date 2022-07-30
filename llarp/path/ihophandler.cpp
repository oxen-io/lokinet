#include "ihophandler.hpp"
#include <llarp/router/abstractrouter.hpp>

namespace llarp
{
  namespace path
  {
    // handle data in upstream direction
    bool
    IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter* r)
    {
      if (r->replayFilter().Insert(Y, r->Now()))
      {
        auto& pkt = m_UpstreamQueue.emplace_back();
        pkt.first.resize(X.sz);
        std::copy_n(X.base, X.sz, pkt.first.begin());
        pkt.second = Y;
      }
      r->TriggerPump();
      return true;
    }

    // handle data in downstream direction
    bool
    IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter* r)
    {
      if (r->replayFilter().Insert(Y, r->Now()))
      {
        auto& pkt = m_DownstreamQueue.emplace_back();
        pkt.first.resize(X.sz);
        std::copy_n(X.base, X.sz, pkt.first.begin());
        pkt.second = Y;
      }
      r->TriggerPump();
      return true;
    }

  }  // namespace path
}  // namespace llarp

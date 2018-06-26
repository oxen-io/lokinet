#include <llarp/path.hpp>
#include "buffer.hpp"
#include "router.hpp"

namespace llarp
{
  namespace path
  {
    bool
    TransitHop::Expired(llarp_time_t now) const
    {
      return now - started > lifetime;
    }

    TransitHopInfo::TransitHopInfo(const TransitHopInfo& other)
        : txID(other.txID)
        , rxID(other.rxID)
        , upstream(other.upstream)
        , downstream(other.downstream)
    {
    }

    TransitHopInfo::TransitHopInfo(const RouterID& down,
                                   const LR_CommitRecord& record)
        : txID(record.txid)
        , rxID(record.rxid)
        , upstream(record.nextHop)
        , downstream(down)
    {
    }

    TransitHop::TransitHop(const TransitHop& other)
        : info(other.info)
        , pathKey(other.pathKey)
        , started(other.started)
        , lifetime(other.lifetime)
        , version(other.version)
    {
    }

    bool
    TransitHop::SendRoutingMessage(const llarp::routing::IMessage* msg,
                                   llarp_router* r)
    {
      byte_t tmp[MAX_LINK_MSG_SIZE / 2];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!msg->BEncode(&buf))
      {
        llarp::Error("failed to encode routing message");
        return false;
      }
      TunnelNonce N;
      N.Randomize();
      // rewind
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      return HandleDownstream(buf, N, r);
    }

    bool
    TransitHop::HandleDownstream(llarp_buffer_t buf, const TunnelNonce& Y,
                                 llarp_router* r)
    {
      RelayDownstreamMessage* msg = new RelayDownstreamMessage;
      msg->pathid                 = info.txID;
      msg->Y                      = Y;

      r->crypto.xchacha20(buf, pathKey, Y);
      msg->X = buf;
      llarp::Info("relay ", msg->X.size(), " bytes downstream from ",
                  info.upstream, " to ", info.downstream);
      return r->SendToOrQueue(info.downstream, msg);
    }

    bool
    TransitHop::HandleUpstream(llarp_buffer_t buf, const TunnelNonce& Y,
                               llarp_router* r)
    {
      RelayUpstreamMessage* msg = new RelayUpstreamMessage;
      msg->pathid               = info.rxID;
      msg->Y                    = Y;

      r->crypto.xchacha20(buf, pathKey, Y);
      msg->X = buf;
      llarp::Info("relay ", msg->X.size(), " bytes upstream from ",
                  info.downstream, " to ", info.upstream);
      return r->SendToOrQueue(info.upstream, msg);
    }
  }  // namespace path
}  // namespace llarp

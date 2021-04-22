#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/util/types.hpp>
#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/messages/relay.hpp>
#include <vector>

#include <memory>

struct llarp_buffer_t;

namespace llarp
{
  struct AbstractRouter;

  namespace routing
  {
    struct IMessage;
  }

  namespace path
  {
    struct IHopHandler
    {
      using TrafficEvent_t = std::pair<std::vector<byte_t>, TunnelNonce>;
      using TrafficQueue_t = std::list<TrafficEvent_t>;
      using TrafficQueue_ptr = std::shared_ptr<TrafficQueue_t>;

      virtual ~IHopHandler() = default;

      virtual PathID_t
      RXID() const = 0;

      void
      DecayFilters(llarp_time_t now);

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*);
      // handle data in downstream direction
      virtual bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*);

      /// return timestamp last remote activity happened at
      virtual llarp_time_t
      LastRemoteActivityAt() const = 0;

      virtual bool
      HandleLRSM(uint64_t status, std::array<EncryptedFrame, 8>& frames, AbstractRouter* r) = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

      virtual void
      FlushUpstream(AbstractRouter* r) = 0;

      virtual void
      FlushDownstream(AbstractRouter* r) = 0;

     protected:
      uint64_t m_SequenceNum = 0;
      TrafficQueue_ptr m_UpstreamQueue;
      TrafficQueue_ptr m_DownstreamQueue;
      util::DecayingHashSet<TunnelNonce> m_UpstreamReplayFilter;
      util::DecayingHashSet<TunnelNonce> m_DownstreamReplayFilter;

      virtual void
      UpstreamWork(TrafficQueue_ptr queue, AbstractRouter* r) = 0;

      virtual void
      DownstreamWork(TrafficQueue_ptr queue, AbstractRouter* r) = 0;

      virtual void
      HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, AbstractRouter* r) = 0;
      virtual void
      HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, AbstractRouter* r) = 0;
    };

    using HopHandler_ptr = std::shared_ptr<IHopHandler>;
  }  // namespace path
}  // namespace llarp

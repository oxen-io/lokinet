#pragma once

#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/messages/relay.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/types.hpp>

#include <memory>
#include <vector>

struct llarp_buffer_t;

namespace llarp
{
  struct Router;

  namespace routing
  {
    struct AbstractRoutingMessage;
  }

  namespace path
  {
    struct AbstractHopHandler
    {
      using TrafficEvent_t = std::pair<std::vector<byte_t>, TunnelNonce>;
      using TrafficQueue_t = std::list<TrafficEvent_t>;

      virtual ~AbstractHopHandler() = default;

      virtual PathID_t
      RXID() const = 0;

      void
      DecayFilters(llarp_time_t now);

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      virtual bool
      send_path_control_message(
          std::string method,
          std::string body,
          std::function<void(oxen::quic::message m)> func) = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(std::string payload, Router* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router*);
      // handle data in downstream direction
      virtual bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router*);

      /// return timestamp last remote activity happened at
      virtual llarp_time_t
      LastRemoteActivityAt() const = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

      virtual void
      FlushUpstream(Router* r) = 0;

      virtual void
      FlushDownstream(Router* r) = 0;

     protected:
      uint64_t m_SequenceNum = 0;
      TrafficQueue_t m_UpstreamQueue;
      TrafficQueue_t m_DownstreamQueue;
      util::DecayingHashSet<TunnelNonce> m_UpstreamReplayFilter;
      util::DecayingHashSet<TunnelNonce> m_DownstreamReplayFilter;

      virtual void
      UpstreamWork(TrafficQueue_t queue, Router* r) = 0;

      virtual void
      DownstreamWork(TrafficQueue_t queue, Router* r) = 0;

      virtual void
      HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, Router* r) = 0;
      virtual void
      HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, Router* r) = 0;
    };

    using HopHandler_ptr = std::shared_ptr<AbstractHopHandler>;
  }  // namespace path
}  // namespace llarp

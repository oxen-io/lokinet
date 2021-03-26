#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/path/path.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/util/time.hpp>

#include <queue>

namespace llarp
{
  namespace handlers
  {
    // forward declare
    struct ExitEndpoint;
  }  // namespace handlers

  namespace exit
  {
    /// persistant exit state for 1 identity on the exit node
    struct Endpoint
    {
      static constexpr size_t MaxUpstreamQueueSize = 256;

      Endpoint(
          const llarp::PubKey& remoteIdent,
          const llarp::PathID_t& beginPath,
          bool rewriteIP,
          huint128_t ip,
          llarp::handlers::ExitEndpoint* parent);

      ~Endpoint();

      /// close ourselves
      void
      Close();

      /// implement istateful
      util::StatusObject
      ExtractStatus() const;

      /// return true if we are expired right now
      bool
      IsExpired(llarp_time_t now) const;

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 5s) const;

      /// return true if this endpoint looks dead right now
      bool
      LooksDead(llarp_time_t now, llarp_time_t timeout = 10s) const;

      /// tick ourself, reset tx/rx rates
      void
      Tick(llarp_time_t now);

      /// queue traffic from service node / internet to be transmitted
      bool
      QueueInboundTraffic(ManagedBuffer buff);

      /// flush inbound and outbound traffic queues
      bool
      Flush();

      /// queue outbound traffic
      /// does ip rewrite here
      bool
      QueueOutboundTraffic(ManagedBuffer pkt, uint64_t counter, service::ProtocolType t);

      /// update local path id and cascade information to parent
      /// return true if success
      bool
      UpdateLocalPath(const llarp::PathID_t& nextPath);

      llarp::path::HopHandler_ptr
      GetCurrentPath() const;

      const llarp::PubKey&
      PubKey() const
      {
        return m_remoteSignKey;
      }

      const llarp::PathID_t&
      LocalPath() const
      {
        return m_CurrentPath;
      }

      uint64_t
      TxRate() const
      {
        return m_TxRate;
      }

      uint64_t
      RxRate() const
      {
        return m_RxRate;
      }

      huint128_t
      LocalIP() const
      {
        return m_IP;
      }

      const llarp_time_t createdAt;

     private:
      llarp::handlers::ExitEndpoint* m_Parent;
      llarp::PubKey m_remoteSignKey;
      llarp::PathID_t m_CurrentPath;
      llarp::huint128_t m_IP;
      uint64_t m_TxRate, m_RxRate;
      llarp_time_t m_LastActive;
      bool m_RewriteSource;
      using InboundTrafficQueue_t = std::deque<llarp::routing::TransferTrafficMessage>;
      using TieredQueue = std::map<uint8_t, InboundTrafficQueue_t>;
      // maps number of fragments the message will fit in to the queue for it
      TieredQueue m_DownstreamQueues;

      struct UpstreamBuffer
      {
        UpstreamBuffer(const llarp::net::IPPacket& p, uint64_t c) : pkt(p), counter(c)
        {}

        llarp::net::IPPacket pkt;
        uint64_t counter;

        bool
        operator<(const UpstreamBuffer& other) const
        {
          return counter < other.counter;
        }
      };

      using UpstreamQueue_t = std::priority_queue<UpstreamBuffer>;
      UpstreamQueue_t m_UpstreamQueue;
      uint64_t m_Counter;
    };
  }  // namespace exit
}  // namespace llarp

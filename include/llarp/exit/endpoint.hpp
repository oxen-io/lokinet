#ifndef LLARP_EXIT_ENDPOINT_HPP
#define LLARP_EXIT_ENDPOINT_HPP
#include <llarp/time.hpp>
#include <llarp/crypto.hpp>
#include <llarp/router.h>
#include <llarp/path.hpp>

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
      Endpoint(const llarp::PubKey& remoteIdent,
               const llarp::PathID_t& beginPath, bool rewriteIP, huint32_t ip,
               llarp::handlers::ExitEndpoint* parent);

      ~Endpoint();

      /// close ourselves
      void
      Close();

      /// return true if we are expired right now
      bool
      IsExpired(llarp_time_t now) const;

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 5000) const;

      /// return true if this endpoint looks dead right now
      bool 
      LooksDead(llarp_time_t now, llarp_time_t timeout = 10000) const;

      /// tick ourself, reset tx/rx rates
      void
      Tick(llarp_time_t now);

      /// queue traffic from service node / internet to be transmitted
      bool
      QueueInboundTraffic(llarp_buffer_t buff);

      /// flush inbound traffic queue
      bool
      FlushInboundTraffic();

      /// send traffic to service node / internet
      /// does ip rewrite here
      bool
      SendOutboundTraffic(llarp_buffer_t buf);

      /// update local path id and cascade information to parent
      /// return true if success
      bool
      UpdateLocalPath(const llarp::PathID_t& nextPath);

      llarp::path::IHopHandler*
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

      huint32_t
      LocalIP() const
      {
        return m_IP;
      }

     private:
      llarp::handlers::ExitEndpoint* m_Parent;
      llarp::PubKey m_remoteSignKey;
      llarp::PathID_t m_CurrentPath;
      llarp::huint32_t m_IP;
      uint64_t m_TxRate, m_RxRate;
      llarp_time_t m_LastActive;
      bool m_RewriteSource;
      using InboundTrafficQueue_t = std::deque<llarp::routing::TransferTrafficMessage>;
      InboundTrafficQueue_t m_DownstreamQueue;
    };
  }  // namespace exit
}  // namespace llarp

#endif

#ifndef LLARP_EXIT_ENDPOINT_HPP
#define LLARP_EXIT_ENDPOINT_HPP
#include <llarp/time.h>
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
               const llarp::PathID_t& beginPath, bool rewriteDst, huint32_t ip,
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

      /// tick ourself, reset tx/rx rates
      void
      Tick(llarp_time_t now);

      /// handle traffic from service node / internet
      bool
      SendInboundTraffic(llarp_buffer_t buff);

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

     private:
      llarp::handlers::ExitEndpoint* m_Parent;
      llarp::PubKey m_remoteSignKey;
      llarp::PathID_t m_CurrentPath;
      llarp::huint32_t m_IP;
      uint64_t m_TxRate, m_RxRate;
      bool m_RewriteSource;
    };
  }  // namespace exit
}  // namespace llarp

#endif
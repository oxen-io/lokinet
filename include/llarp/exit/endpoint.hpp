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
               const llarp::PathID_t& beginPath,
               llarp::handlers::ExitEndpoint* parent);

      ~Endpoint();

      /// return true if we are expired right now
      bool
      IsExpired(llarp_time_t now) const;

      /// handle traffic from service node / internet
      bool
      SendInboundTraffic(llarp_buffer_t buff);

      /// send traffic to service node / internet
      /// does ip rewrite
      bool
      SendOutboundTraffic(llarp_buffer_t buf);

      void
      UpdateLocalPath(const llarp::PathID_t& nextPath);

      llarp::path::IHopHandler*
      GetCurrentPath() const;

     private:
      llarp::handlers::ExitEndpoint* m_Parent;
      llarp::PubKey m_remoteSignKey;
      llarp::PathID_t m_CurrentPath;
    };
  }  // namespace exit
}  // namespace llarp

#endif
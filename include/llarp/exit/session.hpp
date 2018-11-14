#ifndef LLARP_EXIT_SESSION_HPP
#define LLARP_EXIT_SESSION_HPP
#include <llarp/pathbuilder.hpp>
#include <llarp/ip.hpp>

namespace llarp
{
  namespace exit
  {
    /// a persisiting exit session with an exit router
    struct BaseSession : public llarp::path::Builder
    {
      BaseSession(const llarp::RouterID& exitRouter,
                  std::function< bool(llarp_buffer_t) > writepkt,
                  llarp_router* r, size_t numpaths, size_t hoplen);

      ~BaseSession();

      bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop, llarp::path::PathRole roles) override;

      void
      HandlePathBuilt(llarp::path::Path* p) override;

      bool
      SendUpstreamTraffic(llarp::net::IPv4Packet pkt);

     protected:
      llarp::RouterID m_ExitRouter;
      std::function< bool(llarp_buffer_t) > m_WritePacket;

      bool
      HandleTrafficDrop(llarp::path::Path* p, const llarp::PathID_t& path,
                        uint64_t s);

      bool
      HandleGotExit(llarp::path::Path* p, llarp_time_t b);

      bool
      HandleTraffic(llarp::path::Path* p, llarp_buffer_t buf);

     private:
      llarp::SecretKey m_ExitIdentity;
    };

  }  // namespace exit
}  // namespace llarp

#endif
#ifndef LLARP_EXIT_SESSION_HPP
#define LLARP_EXIT_SESSION_HPP

#include <ip.hpp>
#include <messages/exit.hpp>
#include <messages/transfer_traffic.hpp>
#include <pathbuilder.hpp>

#include <deque>

namespace llarp
{
  namespace exit
  {
    /// a persisiting exit session with an exit router
    struct BaseSession : public llarp::path::Builder
    {
      static constexpr size_t MaxUpstreamQueueLength = 256;
      static constexpr llarp_time_t LifeSpan         = 60 * 10 * 1000;

      BaseSession(const llarp::RouterID& exitRouter,
                  std::function< bool(llarp_buffer_t) > writepkt,
                  llarp::Router* r, size_t numpaths, size_t hoplen);

      virtual ~BaseSession();

      bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop, llarp::path::PathRole roles) override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      void
      HandlePathBuilt(llarp::path::Path* p) override;

      bool
      QueueUpstreamTraffic(llarp::net::IPv4Packet pkt, const size_t packSize);

      bool
      FlushUpstreamTraffic();

      bool
      IsReady() const;

      const llarp::RouterID
      Endpoint() const
      {
        return m_ExitRouter;
      }

      bool
      IsExpired(llarp_time_t now) const;

     protected:
      llarp::RouterID m_ExitRouter;
      std::function< bool(llarp_buffer_t) > m_WritePacket;

      virtual void
      PopulateRequest(llarp::routing::ObtainExitMessage& msg) const = 0;

      bool
      HandleTrafficDrop(llarp::path::Path* p, const llarp::PathID_t& path,
                        uint64_t s);

      bool
      HandleGotExit(llarp::path::Path* p, llarp_time_t b);

      bool
      HandleTraffic(llarp::path::Path* p, llarp_buffer_t buf);

     private:
      using UpstreamTrafficQueue_t =
          std::deque< llarp::routing::TransferTrafficMessage >;
      using TieredQueue_t = std::map< uint8_t, UpstreamTrafficQueue_t >;
      TieredQueue_t m_Upstream;
      uint64_t m_Counter;
      llarp::SecretKey m_ExitIdentity;
      llarp_time_t m_LastUse;
    };

    struct ExitSession final : public BaseSession
    {
      ExitSession(const llarp::RouterID& snodeRouter,
                  std::function< bool(llarp_buffer_t) > writepkt,
                  llarp::Router* r, size_t numpaths, size_t hoplen)
          : BaseSession(snodeRouter, writepkt, r, numpaths, hoplen){};

      ~ExitSession(){};

     protected:
      virtual void
      PopulateRequest(llarp::routing::ObtainExitMessage& msg) const override
      {
        // TODO: set expiration time
        msg.X = 0;
        msg.E = 1;
      }
    };

    struct SNodeSession final : public BaseSession
    {
      SNodeSession(const llarp::RouterID& snodeRouter,
                   std::function< bool(llarp_buffer_t) > writepkt,
                   llarp::Router* r, size_t numpaths, size_t hoplen)
          : BaseSession(snodeRouter, writepkt, r, numpaths, hoplen){};

      ~SNodeSession(){};

     protected:
      void
      PopulateRequest(llarp::routing::ObtainExitMessage& msg) const override
      {
        // TODO: set expiration time
        msg.X = 0;
        msg.E = 0;
      }
    };

  }  // namespace exit
}  // namespace llarp

#endif

#ifndef LLARP_HANDLERS_TUN_HPP
#define LLARP_HANDLERS_TUN_HPP
#include <llarp/ev.h>
#include <llarp/service/endpoint.hpp>
#include <llarp/threading.hpp>

namespace llarp
{
  namespace handlers
  {
    struct TunEndpoint : public service::Endpoint
    {
      static constexpr int DefaultNetmask    = 16;
      static constexpr char DefaultIfname[]  = "lokinet0";
      static constexpr char DefaultDstAddr[] = "10.10.0.1";
      static constexpr char DefaultSrcAddr[] = "10.10.0.2";

      TunEndpoint(const std::string& nickname, llarp_router* r);
      ~TunEndpoint();

      bool
      SetOption(const std::string& k, const std::string& v);

      void
      Tick(llarp_time_t now);

      void
      TickTun(llarp_time_t now);

      bool
      Start();

      /// set up tun interface, blocking
      bool
      SetupTun();

      /// overrides Endpoint
      bool
      SetupNetworking();

      llarp_tun_io tunif;

      static void
      tunifTick(llarp_tun_io* t);

      static void
      tunifRecvPkt(llarp_tun_io* t, const void* pkt, ssize_t sz);

      static void
      handleTickTun(void* u);

     private:
      std::promise< bool > m_TunSetupResult;
    };
  }  // namespace handlers
}  // namespace llarp

#endif

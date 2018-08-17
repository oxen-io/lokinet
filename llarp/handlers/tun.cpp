#include <llarp/handlers/tun.hpp>
#include "router.hpp"

namespace llarp
{
  namespace handlers
  {
    TunEndpoint::TunEndpoint(const std::string &nickname, llarp_router *r)
        : service::Endpoint(nickname, r)
    {
      tunif.user    = this;
      tunif.netmask = TunEndpoint::DefaultNetmask;
      strncpy(tunif.ifaddr, TunEndpoint::DefaultSrcAddr,
              sizeof(tunif.ifaddr) - 1);
      strncpy(tunif.ifname, TunEndpoint::DefaultIfname,
              sizeof(tunif.ifname) - 1);
      tunif.tick    = &tunifTick;
      tunif.recvpkt = &tunifRecvPkt;
    }

    bool
    TunEndpoint::SetOption(const std::string &k, const std::string &v)
    {
      if(k == "ifname")
      {
        strncpy(tunif.ifname, v.c_str(), sizeof(tunif.ifname) - 1);
        return true;
      }
      if(k == "ifaddr")
      {
        std::string addr;
        auto pos = v.find("/");
        if(pos != std::string::npos)
        {
          auto num = std::stoi(v.substr(pos + 1));
          if(num > 0)
          {
            tunif.netmask = num;
            addr          = v.substr(0, pos);
          }
          else
          {
            llarp::LogError("bad ifaddr value: ", v);
            return false;
          }
        }
        else
        {
          tunif.netmask = 32;
          addr          = v;
        }
        strncpy(tunif.ifaddr, addr.c_str(), sizeof(tunif.ifaddr) - 1);
        return true;
      }
      return Endpoint::SetOption(k, v);
    }

    bool
    TunEndpoint::Start()
    {
      // do network isolation first
      if(!Endpoint::Start())
        return false;

      if(!NetworkIsIsolated())
      {
        // set up networking in currrent thread if we are not isolated
        if(!SetupNetworking())
          return false;
      }
      // wait for result for network setup
      llarp::LogInfo("waiting for tun interface...");
      return m_TunSetupResult.get_future().get();
    }

    bool
    TunEndpoint::SetupTun()
    {
      auto evloop = Router()->netloop;
      return llarp_ev_add_tun(evloop, &tunif);
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      bool result = SetupTun();
      m_TunSetupResult.set_value(result);
      return result;
    }

    void
    TunEndpoint::Tick(llarp_time_t now)
    {
      // call tun code in endpoint logic in case of network isolation
      llarp_logic_queue_job(EndpointLogic(), {this, handleTickTun});
      Endpoint::Tick(now);
    }

    void
    TunEndpoint::handleTickTun(void *u)
    {
      auto now          = llarp_time_now_ms();
      TunEndpoint *self = static_cast< TunEndpoint * >(u);
      self->TickTun(now);
    }

    TunEndpoint::~TunEndpoint()
    {
    }

  }  // namespace handlers
}  // namespace llarp

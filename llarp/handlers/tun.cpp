#include <llarp/handlers/tun.hpp>
#include "router.hpp"

namespace llarp
{
  namespace handlers
  {
    TunEndpoint::TunEndpoint(const std::string &nickname, llarp_router *r)
        : service::Endpoint(nickname, r)
        , m_UserToNetworkPktQueue(nickname + "_sendq")
        , m_NetworkToUserPktQueue(nickname + "_recvq")
    {
      tunif.user    = this;
      tunif.netmask = DefaultTunNetmask;
      strncpy(tunif.ifaddr, DefaultTunSrcAddr, sizeof(tunif.ifaddr) - 1);
      strncpy(tunif.ifname, DefaultTunIfname, sizeof(tunif.ifname) - 1);
      tunif.tick         = nullptr;
      tunif.before_write = &tunifBeforeWrite;
      tunif.recvpkt      = &tunifRecvPkt;
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
      return llarp_ev_add_tun(EndpointNetLoop(), &tunif);
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      llarp::LogInfo("Set Up networking for ", Name());
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

    void
    TunEndpoint::TickTun(llarp_time_t now)
    {
      // called in the isolated thread
    }

    void
    TunEndpoint::tunifBeforeWrite(llarp_tun_io *tun)
    {
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      self->m_NetworkToUserPktQueue.Process(
          [tun](const std::unique_ptr< net::IPv4Packet > &pkt) {
            if(!llarp_ev_tun_async_write(tun, pkt->buf, pkt->sz))
              llarp::LogWarn("packet dropped");
          });
    }

    void
    TunEndpoint::tunifRecvPkt(llarp_tun_io *tun, const void *buf, ssize_t sz)
    {
      // called for every packet read from user in isolated network thread
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);

      std::unique_ptr< net::IPv4Packet > pkt = net::ParseIPv4Packet(buf, sz);
      if(pkt)
        self->m_UserToNetworkPktQueue.Put(pkt);
    }

    TunEndpoint::~TunEndpoint()
    {
    }

  }  // namespace handlers
}  // namespace llarp

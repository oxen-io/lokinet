// harmless on other platforms
#define __USE_MINGW_ANSI_STDIO 1
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
      if(k == "mapaddr")
      {
        auto pos = v.find(":");
        if(pos == std::string::npos)
        {
          llarp::LogError("Cannot map address ", v,
                          " invalid format, expects "
                          "address.loki:ip.address.goes.here");
          return false;
        }
        service::Address addr;
        auto addr_str = v.substr(0, pos);
        if(!addr.FromString(addr_str))
        {
          llarp::LogError(Name() + " cannot map invalid address ", addr_str);
          return false;
        }
        auto ip_str = v.substr(pos + 1);
        uint32_t ip;
        if(inet_pton(AF_INET, ip_str.c_str(), &ip) != 1)
        {
          llarp::LogError("cannot map to invalid ip ", ip_str);
          return false;
        }
        return MapAddress(addr, ip);
      }
      if(k == "ifname")
      {
        strncpy(tunif.ifname, v.c_str(), sizeof(tunif.ifname) - 1);
        llarp::LogInfo(Name() + " setting ifname to ", tunif.ifname);
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
        llarp::LogInfo(Name() + " set ifaddr to ", addr, " with netmask ",
                       tunif.netmask);
        strncpy(tunif.ifaddr, addr.c_str(), sizeof(tunif.ifaddr) - 1);
        return true;
      }
      return Endpoint::SetOption(k, v);
    }

    bool
    TunEndpoint::MapAddress(const service::Address &addr, uint32_t ip)
    {
      char buf[128] = {0};
      inet_ntop(AF_INET, &ip, buf, sizeof(buf));
      auto itr = m_IPToAddr.find(ip);
      if(itr != m_IPToAddr.end())
      {
        llarp::LogWarn(buf, " already mapped to ", itr->second.ToString());
        return false;
      }
      llarp::LogInfo(Name() + " map ", addr.ToString(), " to ", buf);
      m_IPToAddr.insert(std::make_pair(ip, addr));
      m_AddrToIP.insert(std::make_pair(addr, ip));
      // TODO: make ip mapping persist forever
      MarkIPActiveForever(ip);
      return true;
    }

    bool
    TunEndpoint::Start()
    {
      // do network isolation first
      if(!Endpoint::Start())
        return false;
#ifdef _MINGW32_NO_THREADS
      return SetupNetworking();
#else
      if(!NetworkIsIsolated())
      {
        // set up networking in currrent thread if we are not isolated
        if(!SetupNetworking())
          return false;
      }
      // wait for result for network setup
      llarp::LogInfo("waiting for tun interface...");
      return m_TunSetupResult.get_future().get();
#endif
    }

    constexpr uint32_t
    netmask_ipv4_bits(uint32_t netmask)
    {
      return (32 - netmask)
          ? (1 << (32 - (netmask + 1))) | netmask_ipv4_bits(netmask + 1)
          : 0;
    }

    bool
    TunEndpoint::SetupTun()
    {
      if(!llarp_ev_add_tun(EndpointNetLoop(), &tunif))
      {
        llarp::LogError(Name(), " failed to set up tun interface");
        return false;
      }
      m_OurIP       = inet_addr(tunif.ifaddr);
      m_NextIP      = m_OurIP;
      uint32_t mask = tunif.netmask;

      uint32_t baseaddr = (ntohs(m_OurIP) & netmask_ipv4_bits(mask));
      m_MaxIP           = (ntohs(baseaddr) | ~ntohs(netmask_ipv4_bits(mask)));
      char buf[128]     = {0};
      llarp::LogInfo(Name(), " set ", tunif.ifname, " to have address ",
                     inet_ntop(AF_INET, &m_OurIP, buf, sizeof(buf)));

      llarp::LogInfo(Name(), " allocated up to ",
                     inet_ntop(AF_INET, &m_MaxIP, buf, sizeof(buf)));
      return true;
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      llarp::LogInfo("Set Up networking for ", Name());
      bool result = SetupTun();
#ifndef _WIN32
      m_TunSetupResult.set_value(result);
#endif
      return result;
    }

    void
    TunEndpoint::Tick(llarp_time_t now)
    {
      // call tun code in endpoint logic in case of network isolation
      llarp_logic_queue_job(EndpointLogic(), {this, handleTickTun});
      FlushSend();
      Endpoint::Tick(now);
    }

    void
    TunEndpoint::FlushSend()
    {
      m_UserToNetworkPktQueue.Process([&](net::IPv4Packet &pkt) {
        auto itr = m_IPToAddr.find(pkt.dst());
        if(itr == m_IPToAddr.end())
        {
          in_addr a;
          a.s_addr = pkt.dst();
          llarp::LogWarn("drop packet to ", inet_ntoa(a));
          llarp::DumpBuffer(pkt.Buffer());
          return true;
        }
        return SendToOrQueue(itr->second, pkt.Buffer(),
                             service::eProtocolTraffic);
      });
    }

    void
    TunEndpoint::HandleDataMessage(service::ProtocolMessage *msg)
    {
      if(msg->proto != service::eProtocolTraffic)
      {
        llarp::LogWarn(Name(),
                       " dropping unwarrented message, not ip traffic, proto=",
                       msg->proto);
        return;
      }
      llarp::LogInfo(Name(), " handle data message ", msg->payload.size(),
                     " bytes");
      uint32_t themIP = ObtainIPForAddr(msg->sender.Addr());
      uint32_t usIP   = m_OurIP;
      auto buf        = llarp::Buffer(msg->payload);
      if(!m_NetworkToUserPktQueue.EmplaceIf(
             [buf, themIP, usIP](net::IPv4Packet &pkt) -> bool {
               // do packet info rewrite here
               // TODO: don't truncate packet here
               memcpy(pkt.buf, buf.base, std::min(buf.sz, sizeof(pkt.buf)));
               pkt.src(themIP);
               pkt.dst(usIP);
               pkt.UpdateChecksum();
               return true;
             }))
      {
        llarp::LogWarn("failed to parse buffer for ip traffic");
        llarp::DumpBuffer(buf);
      }
    }

    uint32_t
    TunEndpoint::ObtainIPForAddr(const service::Address &addr)
    {
      llarp_time_t now = llarp_time_now_ms();
      uint32_t nextIP;
      {
        // previously allocated address
        auto itr = m_AddrToIP.find(addr);
        if(itr != m_AddrToIP.end())
        {
          // mark ip active
          m_IPActivity[itr->second] = now;
          return itr->second;
        }
      }
      if(m_NextIP < m_MaxIP)
      {
        nextIP = ++m_NextIP;
        m_AddrToIP.insert(std::make_pair(addr, nextIP));
        m_IPToAddr.insert(std::make_pair(nextIP, addr));
      }
      else
      {
        // we are full
        // expire least active ip
        // TODO: prevent DoS
        std::pair< uint32_t, llarp_time_t > oldest = {0, 0};

        // find oldest entry
        auto itr = m_IPActivity.begin();
        while(itr != m_IPActivity.end())
        {
          if(itr->second <= now)
          {
            if((now - itr->second) > oldest.second)
            {
              oldest.first  = itr->first;
              oldest.second = itr->second;
            }
          }
          ++itr;
        }
        // remap address
        m_IPToAddr[oldest.first] = addr;
        m_AddrToIP[addr]         = oldest.first;
        nextIP                   = oldest.first;
      }

      // mark ip active
      m_IPActivity[nextIP] = now;

      return nextIP;
    }

    bool
    TunEndpoint::HasRemoteForIP(const uint32_t &ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    void
    TunEndpoint::MarkIPActive(uint32_t ip)
    {
      m_IPActivity[ip] = llarp_time_now_ms();
    }

    void
    TunEndpoint::MarkIPActiveForever(uint32_t ip)
    {
      m_IPActivity[ip] = std::numeric_limits< uint64_t >::max();
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
      // called in the isolated network thread
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      self->m_NetworkToUserPktQueue.Process([self, tun](net::IPv4Packet &pkt) {
        if(!llarp_ev_tun_async_write(tun, pkt.buf, pkt.sz))
          llarp::LogWarn("packet dropped");
      });
      if(self->m_UserToNetworkPktQueue.Size())
        llarp_logic_queue_job(self->RouterLogic(), {self, &handleNetSend});
    }

    void
    TunEndpoint::handleNetSend(void *user)
    {
      TunEndpoint *self = static_cast< TunEndpoint * >(user);
      self->FlushSend();
    }

    void
    TunEndpoint::tunifRecvPkt(llarp_tun_io *tun, const void *buf, ssize_t sz)
    {
      // called for every packet read from user in isolated network thread
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      llarp::LogDebug("got pkt ", sz, " bytes");
      if(!self->m_UserToNetworkPktQueue.EmplaceIf(
             [self, buf, sz](net::IPv4Packet &pkt) -> bool {
               return pkt.Load(llarp::InitBuffer(buf, sz))
                   && pkt.Header()->ip_version == 4;
             }))
        llarp::LogError("Failed to parse ipv4 packet");
    }

    TunEndpoint::~TunEndpoint()
    {
    }

  }  // namespace handlers
}  // namespace llarp

#include <algorithm>
// harmless on other platforms
#define __USE_MINGW_ANSI_STDIO 1
#include <llarp/handlers/tun.hpp>
#include "router.hpp"
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#endif

#ifndef DNS_PORT
#define DNS_PORT (53)
#endif

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
      tunif.tick           = nullptr;
      tunif.before_write   = &tunifBeforeWrite;
      tunif.recvpkt        = &tunifRecvPkt;
      this->dll.ip_tracker = nullptr;
      this->dll.user       = &r->hiddenServiceContext;
      // this->dll.callback = std::bind(&TunEndpoint::MapAddress, this);
    }

    bool
    TunEndpoint::SetOption(const std::string &k, const std::string &v)
    {
      if(k == "nameresolver")
      {
        // we probably can set the property since the config will load before
        // the relay is set up
        // strncpy(tunif.ifname, v.c_str(), sizeof(tunif.ifname) - 1);
        llarp::LogInfo(Name() + " would be setting DNS resolver to ", v);
        return true;
      }
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
        in_addr ip;
        if(inet_pton(AF_INET, ip_str.c_str(), &ip) != 1)
        {
          llarp::LogError("cannot map to invalid ip ", ip_str);
          return false;
        }
        return MapAddress(addr, huint32_t{ntohl(ip.s_addr)});
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

        // set up address in dotLokiLookup
        struct sockaddr_in source_addr;
        source_addr.sin_addr.s_addr = inet_addr(tunif.ifaddr);
        source_addr.sin_family      = AF_INET;

        llarp::Addr tunIp(source_addr);
        // related to dns_iptracker_setup_dotLokiLookup(&this->dll, tunIp);
        dns_iptracker_setup(this->dll.ip_tracker,
                            tunIp);  // claim GW IP to make sure it's not inuse
        return true;
      }
      return Endpoint::SetOption(k, v);
    }

    /// ip should be in host byte order
    bool
    TunEndpoint::MapAddress(const service::Address &addr, huint32_t ip)
    {
      nuint32_t nip = xhtonl(ip);

      auto itr = m_IPToAddr.find(ip);
      if(itr != m_IPToAddr.end())
      {
        // XXX is calling inet_ntoa safe in this context? it's MP-unsafe
        llarp::LogWarn(inet_ntoa({nip.n}), " already mapped to ",
                       itr->second.ToString());
        return false;
      }
      llarp::LogInfo(Name() + " map ", addr.ToString(), " to ",
                     inet_ntoa({nip.n}));
      m_IPToAddr.insert(std::make_pair(ip, addr));
      m_AddrToIP.insert(std::make_pair(addr, ip));
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
        llarp::LogInfo("Setting up global DNS IP tracker");
        llarp::Addr tunIp(tunif.ifaddr);
        dns_iptracker_setup_dotLokiLookup(
            &this->dll, tunIp);  // just set ups dll to use global iptracker
        dns_iptracker_setup(this->dll.ip_tracker,
                            tunIp);  // claim GW IP to make sure it's not inuse

        // set up networking in currrent thread if we are not isolated
        if(!SetupNetworking())
          return false;
      }
      else
      {
        llarp::LogInfo("Setting up per netns DNS IP tracker");
        llarp::Addr tunIp(tunif.ifaddr);
        this->dll.ip_tracker = new dns_iptracker;
        dns_iptracker_setup_dotLokiLookup(
            &this->dll, tunIp);  // just set ups dll to use global iptracker
        dns_iptracker_setup(this->dll.ip_tracker,
                            tunIp);  // claim GW IP to make sure it's not inuse
      }
      // wait for result for network setup
      llarp::LogInfo("waiting for tun interface...");
      return m_TunSetupResult.get_future().get();
#endif
    }

    bool
    TunEndpoint::SetupTun()
    {
      if(!llarp_ev_add_tun(EndpointNetLoop(), &tunif))
      {
        llarp::LogError(Name(), " failed to set up tun interface");
        return false;
      }

      struct addrinfo hint, *res = NULL;
      int ret;

      memset(&hint, '\0', sizeof hint);

      hint.ai_family = PF_UNSPEC;
      hint.ai_flags  = AI_NUMERICHOST;

      ret = getaddrinfo(tunif.ifaddr, NULL, &hint, &res);
      if(ret)
      {
        llarp::LogError(Name(),
                        " failed to set up tun interface, cant determine "
                        "family from ",
                        tunif.ifaddr);
        return false;
      }

      /*
      // output is in network byte order
      unsigned char buf[sizeof(struct in6_addr)];
      int s = inet_pton(res->ai_family, tunif.ifaddr, buf);
      if (s <= 0)
      {
        llarp::LogError(Name(), " failed to set up tun interface, cant parse ",
      tunif.ifaddr); return false;
      }
      */
      if(res->ai_family == AF_INET6)
      {
        llarp::LogError(Name(),
                        " failed to set up tun interface, we don't support "
                        "IPv6 format");
        return false;
      }
      freeaddrinfo(res);

      struct in_addr addr;  // network byte order
      if(inet_aton(tunif.ifaddr, &addr) == 0)
      {
        llarp::LogError(Name(), " failed to set up tun interface, cant parse ",
                        tunif.ifaddr);
        return false;
      }

      llarp::Addr lAddr(tunif.ifaddr);

      m_OurIP    = lAddr.xtohl();
      m_NextIP   = m_OurIP;
      auto xmask = netmask_ipv4_bits(tunif.netmask);

      auto baseaddr = m_OurIP & xmask;
      m_MaxIP       = baseaddr | ~xmask;
      char buf[128] = {0};
      llarp::LogInfo(Name(), " set ", tunif.ifname, " to have address ", lAddr);

      llarp::LogInfo(Name(), " allocated up to ",
                     inet_ntop(AF_INET, &m_MaxIP, buf, sizeof(buf)));
      return true;
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      llarp::LogInfo("Set Up networking for ", Name());
      bool result = SetupTun();
      m_TunSetupResult.set_value(
          result);  // now that NT has tun, we don't need the CPP guard
      if(!NetworkIsIsolated())
      {
        // need to check to see if we have more than one hidden service
        // well we'll only use the primary
        // FIXME: detect number of hidden services
        llarp::LogWarn(
            "Only utilizing first hidden service for .loki look ups");
        // because we can't find to the tun interface because we don't want it
        // accessible on lokinet we can only bind one to loopback, and we can't
        // really utilize anything other than port 53 we can't bind to our
        // public interface, don't want it exploitable maybe we could detect if
        // you have a private interface
      }
      llarp::Addr dnsd_sockaddr(127, 0, 0, 1, DNS_PORT);
      llarp::Addr dnsc_sockaddr(8, 8, 8, 8, 53);
      llarp::LogInfo("TunDNS set up ", dnsd_sockaddr, " to ", dnsc_sockaddr);
      if(!llarp_dnsd_init(&this->dnsd, EndpointLogic(), EndpointNetLoop(),
                          dnsd_sockaddr, dnsc_sockaddr))
      {
        llarp::LogError("Couldnt init dns daemon");
      }
      // configure hook for .loki lookup
      dnsd.intercept = &llarp_dotlokilookup_handler;
      // set dotLokiLookup (this->dll) configuration
      dnsd.user = &this->dll;
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
          llarp::LogWarn(Name(), " has no endpoint for ",
                         inet_ntoa({xhtonl(pkt.dst()).n}));
          return true;
        }

        // prepare packet for insertion into network
        // this includes clearing IP addresses, recalculating checksums, etc
        pkt.UpdatePacketOnSrc();

        if(!SendToOrQueue(itr->second, pkt.Buffer(), service::eProtocolTraffic))
        {
          llarp::LogWarn(Name(), " did not flush packets");
        }
        return true;
      });
    }

    bool
    TunEndpoint::ProcessDataMessage(service::ProtocolMessage *msg)
    {
      auto themIP = ObtainIPForAddr(msg->sender.Addr());
      auto usIP   = m_OurIP;
      auto buf    = llarp::Buffer(msg->payload);
      if(m_NetworkToUserPktQueue.EmplaceIf(
             [buf, themIP, usIP](net::IPv4Packet &pkt) -> bool {
               // load
               if(!pkt.Load(buf))
                 return false;
               // filter out:
               // - packets smaller than minimal IPv4 header
               // - non-IPv4 packets
               // - packets with weird src/dst addresses
               //   (0.0.0.0/8 but not 0.0.0.0)
               // - packets with 0 src but non-0 dst and oposite
               auto hdr = pkt.Header();
               if(pkt.sz < sizeof(*hdr) || hdr->version != 4
                  || (hdr->saddr != 0 && *(byte_t *)&(hdr->saddr) == 0)
                  || (hdr->daddr != 0 && *(byte_t *)&(hdr->daddr) == 0)
                  || ((hdr->saddr == 0) != (hdr->daddr == 0)))
               {
                 return false;
               }
               // update packet to use proper addresses, recalc checksums
               pkt.UpdatePacketOnDst(themIP, usIP);
               return true;
             }))

        llarp::LogDebug(Name(), " handle data message ", msg->payload.size(),
                        " bytes from ", inet_ntoa({xhtonl(themIP).n}));
      return true;
    }

    huint32_t
    TunEndpoint::ObtainIPForAddr(const service::Address &addr)
    {
      llarp_time_t now = llarp_time_now_ms();
      huint32_t nextIP = {0};

      {
        // previously allocated address
        auto itr = m_AddrToIP.find(addr);
        if(itr != m_AddrToIP.end())
        {
          // mark ip active
          MarkIPActive(itr->second);
          return itr->second;
        }
      }
      // allocate new address
      if(m_NextIP < m_MaxIP)
      {
        do
        {
          nextIP = ++m_NextIP;
        } while(m_IPToAddr.find(nextIP) != m_IPToAddr.end()
                && m_NextIP < m_MaxIP);
        if(nextIP < m_MaxIP)
        {
          m_AddrToIP.insert(std::make_pair(addr, nextIP));
          m_IPToAddr.insert(std::make_pair(nextIP, addr));
          llarp::LogInfo(Name(), " mapped ", addr, " to ",
                         inet_ntoa({xhtonl(nextIP).n}));
          MarkIPActive(nextIP);
          return nextIP;
        }
      }

      // we are full
      // expire least active ip
      // TODO: prevent DoS
      std::pair< huint32_t, llarp_time_t > oldest = {huint32_t{0}, 0};

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

      // mark ip active
      m_IPActivity[nextIP] = std::max(m_IPActivity[nextIP], now);

      return nextIP;
    }

    bool
    TunEndpoint::HasRemoteForIP(huint32_t ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    void
    TunEndpoint::MarkIPActive(huint32_t ip)
    {
      m_IPActivity[ip] = std::max(llarp_time_now_ms(), m_IPActivity[ip]);
    }

    void
    TunEndpoint::MarkIPActiveForever(huint32_t ip)
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
                   && pkt.Header()->version == 4;
             }))
      {
        llarp::LogInfo("Failed to parse ipv4 packet");
        llarp::DumpBuffer(llarp::InitBuffer(buf, sz));
      }
    }

    TunEndpoint::~TunEndpoint()
    {
    }

  }  // namespace handlers
}  // namespace llarp

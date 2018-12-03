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
#include "ev.hpp"

namespace llarp
{
  namespace handlers
  {
    static llarp_fd_promise *
    get_tun_fd_promise(llarp_tun_io *tun)
    {
      return static_cast< TunEndpoint * >(tun->user)->Promise.get();
    }

    TunEndpoint::TunEndpoint(const std::string &nickname, llarp_router *r)
        : service::Endpoint(nickname, r)
        , m_UserToNetworkPktQueue(nickname + "_sendq", r->netloop, r->netloop)
        , m_NetworkToUserPktQueue(nickname + "_recvq", r->netloop, r->netloop)
    {
#ifdef ANDROID
      tunif.get_fd_promise = &get_tun_fd_promise;
      Promise.reset(new llarp_fd_promise(&m_VPNPromise));
#else
      tunif.get_fd_promise = nullptr;
#endif
      tunif.user    = this;
      tunif.netmask = DefaultTunNetmask;
#ifdef _WIN32
      llarp::Zero(tunif.ifaddr, sizeof(tunif.ifaddr));
      llarp::Zero(tunif.ifname, sizeof(tunif.ifname));
#else
      strncpy(tunif.ifaddr, DefaultTunSrcAddr, sizeof(tunif.ifaddr) - 1);
      strncpy(tunif.ifname, DefaultTunIfname, sizeof(tunif.ifname) - 1);
#endif
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
      if(k == "exit-node")
      {
        llarp::RouterID exitRouter;
        if(!HexDecode(v.c_str(), exitRouter, exitRouter.size()))
        {
          llarp::LogError(Name(), " bad exit router key: ", v);
          return false;
        }
        m_Exit.reset(new llarp::exit::ExitSession(
            exitRouter,
            std::bind(&TunEndpoint::QueueInboundPacketForExit, this,
                      std::placeholders::_1),
            router, m_NumPaths, numHops));
        llarp::LogInfo(Name(), " using exit at ", exitRouter);
      }
      if(k == "local-dns")
      {
        std::string resolverAddr = v;
        uint16_t dnsport         = 53;
        auto pos                 = v.find(":");
        if(pos != std::string::npos)
        {
          resolverAddr = v.substr(0, pos);
          dnsport      = std::atoi(v.substr(pos + 1).c_str());
        }
        m_LocalResolverAddr = llarp::Addr(resolverAddr, dnsport);
        llarp::LogInfo(Name(), " local dns set to ", m_LocalResolverAddr);
      }
      if(k == "upstream-dns")
      {
        std::string resolverAddr = v;
        uint16_t dnsport         = 53;
        auto pos                 = v.find(":");
        if(pos != std::string::npos)
        {
          resolverAddr = v.substr(0, pos);
          dnsport      = std::atoi(v.substr(pos + 1).c_str());
        }
        m_UpstreamDNSAddr = llarp::Addr(resolverAddr, dnsport);
        llarp::LogInfo(Name(), " upstream dns set to ", m_UpstreamDNSAddr);
      }
      if(k == "mapaddr")
      {
        auto pos = v.find(":");
        if(pos == std::string::npos)
        {
          llarp::LogError("Cannot map address ", v,
                          " invalid format, missing colon (:), expects "
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
        return MapAddress(addr, huint32_t{ntohl(ip.s_addr)}, false);
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
          int num;
          std::string part = v.substr(pos + 1);
#if defined(ANDROID) || defined(RPI)
          num = atoi(part.c_str());
#else
          num = std::stoi(part);
#endif
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
        // llarp::Addr tunIp(tunif.ifaddr);
        llarp::huint32_t tunIpV4;
        tunIpV4.h = inet_addr(tunif.ifaddr);
        // related to dns_iptracker_setup_dotLokiLookup(&this->dll, tunIp);
        dns_iptracker_setup(
            this->dll.ip_tracker,
            tunIpV4);  // claim GW IP to make sure it's not inuse
        return true;
      }
      return Endpoint::SetOption(k, v);
    }

    bool
    TunEndpoint::HasLocalIP(const huint32_t &ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    bool
    TunEndpoint::QueueOutboundTraffic(llarp::net::IPv4Packet &&pkt)
    {
      return m_NetworkToUserPktQueue.EmplaceIf(
          [](llarp::net::IPv4Packet &) -> bool { return true; },
          std::move(pkt));
    }

    bool
    TunEndpoint::MapAddress(const service::Address &addr, huint32_t ip,
                            bool SNode)
    {
      auto itr = m_IPToAddr.find(ip);
      if(itr != m_IPToAddr.end())
      {
        // XXX is calling inet_ntoa safe in this context? it's MP-unsafe
        llarp::LogWarn(ip, " already mapped to ",
                       service::Address(itr->second).ToString());
        return false;
      }
      llarp::LogInfo(Name() + " map ", addr.ToString(), " to ", ip);

      m_IPToAddr[ip]          = addr.data();
      m_AddrToIP[addr.data()] = ip;
      m_SNodes[addr.data()]   = SNode;
      MarkIPActiveForever(ip);
      return true;
    }

    bool
    TunEndpoint::Start()
    {
      // do network isolation first
      if(!Endpoint::Start())
        return false;
#ifdef WIN32
      return SetupNetworking();
#else
      if(!NetworkIsIsolated())
      {
        llarp::LogInfo("Setting up global DNS IP tracker");
        llarp::huint32_t tunIpV4;
        tunIpV4.h = inet_addr(tunif.ifaddr);
        dns_iptracker_setup_dotLokiLookup(
            &this->dll, tunIpV4);  // just set ups dll to use global iptracker
        dns_iptracker_setup(
            this->dll.ip_tracker,
            tunIpV4);  // claim GW IP to make sure it's not inuse

        // set up networking in currrent thread if we are not isolated
        if(!SetupNetworking())
          return false;
      }
      else
      {
        llarp::LogInfo("Setting up per netns DNS IP tracker");
        llarp::huint32_t tunIpV4;
        tunIpV4.h = inet_addr(tunif.ifaddr);
        llarp::Addr tunIp(tunif.ifaddr);
        this->dll.ip_tracker = new dns_iptracker;
        dns_iptracker_setup_dotLokiLookup(
            &this->dll, tunIpV4);  // just set ups dll to use global iptracker
        dns_iptracker_setup(
            this->dll.ip_tracker,
            tunIpV4);  // claim GW IP to make sure it's not inuse
      }
      // wait for result for network setup
      llarp::LogInfo("waiting for tun interface...");
      return m_TunSetupResult.get_future().get();
#endif
    }

    bool
    TunEndpoint::IsSNode() const
    {
      // TODO : implement me
      return false;
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

      memset(&hint, 0, sizeof hint);

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
      m_MaxIP    = m_OurIP ^ (~xmask);
      llarp::LogInfo(Name(), " set ", tunif.ifname, " to have address ", lAddr);

      llarp::LogInfo(Name(), " allocated up to ", m_MaxIP);
      MapAddress(m_Identity.pub.Addr(), m_OurIP, IsSNode());
      return true;
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      llarp::LogInfo("Set Up networking for ", Name());
      bool result = SetupTun();
#ifndef WIN32
      m_TunSetupResult.set_value(
          result);  // now that NT has tun, we don't need the CPP guard
#endif
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

      llarp::LogInfo("TunDNS set up ", m_LocalResolverAddr, " to ",
                     m_UpstreamDNSAddr);
      if(!llarp_dnsd_init(&this->dnsd, EndpointLogic(), EndpointNetLoop(),
                          m_LocalResolverAddr, m_UpstreamDNSAddr))
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
      // llarp_logic_queue_job(EndpointLogic(), {this, handleTickTun});
      FlushSend();
      Endpoint::Tick(now);
    }

    void
    TunEndpoint::FlushSend()
    {
      m_UserToNetworkPktQueue.Process([&](net::IPv4Packet &pkt) {
        std::function< bool(llarp_buffer_t) > sendFunc;
        auto itr = m_IPToAddr.find(pkt.dst());
        if(itr == m_IPToAddr.end())
        {
          if(m_Exit)
          {
            pkt.UpdateIPv4PacketOnDst({0}, pkt.dst());
            m_Exit->QueueUpstreamTraffic(std::move(pkt),
                                         llarp::routing::ExitPadSize);
            return true;
          }
          else
          {
            llarp::LogWarn(Name(), " has no endpoint for ", pkt.dst());
            return true;
          }
        }

        if(m_SNodes.at(itr->second))
        {
          sendFunc = std::bind(&TunEndpoint::SendToSNodeOrQueue, this,
                               itr->second.data(), std::placeholders::_1);
        }
        else
        {
          sendFunc = std::bind(&TunEndpoint::SendToServiceOrQueue, this,
                               itr->second.data(), std::placeholders::_1,
                               service::eProtocolTraffic);
        }
        // prepare packet for insertion into network
        // this includes clearing IP addresses, recalculating checksums, etc
        pkt.UpdateIPv4PacketOnSrc();

        if(sendFunc && sendFunc(pkt.Buffer()))
          return true;
        llarp::LogWarn(Name(), " did not flush packets");
        return true;
      });
      if(m_Exit)
        m_Exit->FlushUpstreamTraffic();
    }

    bool
    TunEndpoint::HandleWriteIPPacket(llarp_buffer_t buf,
                                     std::function< huint32_t(void) > getFromIP)
    {
      // llarp::LogInfo("got packet from ", msg->sender.Addr());
      auto themIP = getFromIP();
      // llarp::LogInfo("themIP ", themIP);
      auto usIP = m_OurIP;
      return m_NetworkToUserPktQueue.EmplaceIf(
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
            pkt.UpdateIPv4PacketOnDst(themIP, usIP);
            return true;
          });
    }

    huint32_t
    TunEndpoint::GetIfAddr() const
    {
      return m_OurIP;
    }

    huint32_t
    TunEndpoint::ObtainIPForAddr(const byte_t *a, bool snode)
    {
      llarp_time_t now = Now();
      huint32_t nextIP = {0};
      AlignedBuffer< 32 > ident(a);
      {
        // previously allocated address
        auto itr = m_AddrToIP.find(ident);
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
          m_AddrToIP[ident]  = nextIP;
          m_IPToAddr[nextIP] = ident;
          m_SNodes[ident]    = snode;
          llarp::LogInfo(Name(), " mapped ", ident, " to ", nextIP);
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
      m_IPToAddr[oldest.first] = ident;
      m_AddrToIP[ident]        = oldest.first;
      m_SNodes[ident]          = snode;
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
      m_IPActivity[ip] = std::max(Now(), m_IPActivity[ip]);
    }

    void
    TunEndpoint::MarkIPActiveForever(huint32_t ip)
    {
      m_IPActivity[ip] = std::numeric_limits< uint64_t >::max();
    }

    void
    TunEndpoint::handleTickTun(void *u)
    {
      TunEndpoint *self = static_cast< TunEndpoint * >(u);
      self->TickTun(self->Now());
    }

    void
    TunEndpoint::TickTun(__attribute__((unused)) llarp_time_t now)
    {
      // called in the isolated thread
    }

    void
    TunEndpoint::tunifBeforeWrite(llarp_tun_io *tun)
    {
      // called in the isolated network thread
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      self->m_NetworkToUserPktQueue.Process([tun](net::IPv4Packet &pkt) {
        if(!llarp_ev_tun_async_write(tun, pkt.Buffer()))
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
    TunEndpoint::tunifRecvPkt(llarp_tun_io *tun, llarp_buffer_t buf)
    {
      // called for every packet read from user in isolated network thread
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      if(!self->m_UserToNetworkPktQueue.EmplaceIf(
             [buf](net::IPv4Packet &pkt) -> bool {
               return pkt.Load(buf) && pkt.Header()->version == 4;
             }))
      {
        llarp::LogInfo("Failed to parse ipv4 packet");
        llarp::DumpBuffer(buf);
      }
    }

    TunEndpoint::~TunEndpoint()
    {
    }

  }  // namespace handlers
}  // namespace llarp

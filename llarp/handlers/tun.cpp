#include <algorithm>
// harmless on other platforms
#define __USE_MINGW_ANSI_STDIO 1
#include <handlers/tun.hpp>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <dns/dns.hpp>
#include <ev/ev.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <util/logic.hpp>
#include <util/memfn.hpp>
#include <nodedb.hpp>

#include <util/str.hpp>

namespace llarp
{
  namespace handlers
  {
    static llarp_fd_promise *
    get_tun_fd_promise(llarp_tun_io *tun)
    {
      return static_cast< TunEndpoint * >(tun->user)->Promise.get();
    }

    static void
    tunifTick(llarp_tun_io *tun)
    {
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      self->Flush();
    }

    TunEndpoint::TunEndpoint(const std::string &nickname, AbstractRouter *r,
                             service::Context *parent)
        : service::Endpoint(nickname, r, parent)
        , m_UserToNetworkPktQueue(nickname + "_sendq", r->netloop(),
                                  r->netloop())
        , m_NetworkToUserPktQueue(nickname + "_recvq", r->netloop(),
                                  r->netloop())
        , m_Resolver(std::make_shared< dns::Proxy >(
              r->netloop(), r->logic(), r->netloop(), r->logic(), this))
    {
      std::fill(tunif.ifaddr, tunif.ifaddr + sizeof(tunif.ifaddr), 0);
      std::fill(tunif.ifname, tunif.ifname + sizeof(tunif.ifname), 0);
      tunif.netmask = 0;

#ifdef ANDROID
      tunif.get_fd_promise = &get_tun_fd_promise;
      Promise.reset(new llarp_fd_promise(&m_VPNPromise));
#else
      tunif.get_fd_promise = nullptr;
#endif
      tunif.user = this;

      // eh this shouldn't do anything on windows anyway
      tunif.tick         = &tunifTick;
      tunif.before_write = &tunifBeforeWrite;
      tunif.recvpkt      = &tunifRecvPkt;
    }

    util::StatusObject
    TunEndpoint::ExtractStatus() const
    {
      auto obj = service::Endpoint::ExtractStatus();
      obj.Put("ifaddr", m_OurRange.ToString());

      std::vector< std::string > resolvers;
      for(const auto &addr : m_UpstreamResolvers)
        resolvers.emplace_back(addr.ToString());
      obj.Put("ustreamResolvers", resolvers);
      obj.Put("localResolver", m_LocalResolverAddr.ToString());
      util::StatusObject ips{};
      for(const auto &item : m_IPActivity)
      {
        util::StatusObject ipObj{{"lastActive", item.second}};
        std::string remoteStr;
        AlignedBuffer< 32 > addr = m_IPToAddr.at(item.first);
        if(m_SNodes.at(addr))
          remoteStr = RouterID(addr.as_array()).ToString();
        else
          remoteStr = service::Address(addr.as_array()).ToString();
        ipObj.Put("remote", remoteStr);
        std::string ipaddr = item.first.ToString();
        ips.Put(ipaddr.c_str(), ipObj);
      }
      obj.Put("addrs", ips);
      obj.Put("ourIP", m_OurIP.ToString());
      obj.Put("nextIP", m_NextIP.ToString());
      obj.Put("maxIP", m_MaxIP.ToString());
      return obj;
    }

    bool
    TunEndpoint::SetOption(const std::string &k, const std::string &v)
    {
      if(k == "isolate-network" && IsTrueValue(v.c_str()))
      {
#if defined(__linux__)
        LogInfo(Name(), " isolating network...");
        if(!SpawnIsolatedNetwork())
        {
          LogError(Name(), " failed to spawn isolated network");
          return false;
        }
        LogInfo(Name(), " booyeah network isolation succeeded");
        return true;
#else
        LogError(Name(),
                 " network isolation is not supported on your platform");
        return false;
#endif
      }
      if(k == "strict-connect")
      {
        RouterID connect;
        if(!connect.FromString(v))
        {
          LogError(Name(), " invalid snode for strict-connect: ", v);
          return false;
        }

        RouterContact rc;
        if(!m_router->nodedb()->Get(connect, rc))
        {
          LogError(Name(), " we don't have the RC for ", v,
                   " so we can't use it in strict-connect");
          return false;
        }
        for(const auto &ai : rc.addrs)
        {
          m_StrictConnectAddrs.emplace_back(ai);
          LogInfo(Name(), " added ", m_StrictConnectAddrs.back(),
                  " to strict connect");
        }
        return true;
      }
      // Name won't be set because we need to read the config before we can read
      // the keyfile
      if(k == "exit-node")
      {
        llarp::RouterID exitRouter;
        if(!(exitRouter.FromString(v)
             || HexDecode(v.c_str(), exitRouter.begin(), exitRouter.size())))
        {
          llarp::LogError(Name(), " bad exit router key: ", v);
          return false;
        }
        m_Exit = std::make_shared< llarp::exit::ExitSession >(
            exitRouter,
            util::memFn(&TunEndpoint::QueueInboundPacketForExit, this),
            m_router, numPaths, numHops, ShouldBundleRC());
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
        llarp::LogInfo(Name(), " binding DNS server to ", m_LocalResolverAddr);
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
        m_UpstreamResolvers.emplace_back(resolverAddr, dnsport);
        llarp::LogInfo(Name(), " adding upstream DNS server ", resolverAddr,
                       ":", dnsport);
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
        huint32_t ip;
        huint128_t ipv6;
        if(ip.FromString(ip_str))
        {
          ipv6 = net::IPPacket::ExpandV4(ip);
        }
        else if(ipv6.FromString(ip_str))
        {
        }
        else
        {
          llarp::LogError(Name(), "failed to map ", ip_str,
                          " failed to parse IP");
          return false;
        }
        return MapAddress(addr, ipv6, false);
      }
      if(k == "ifname")
      {
        if(v.length() >= sizeof(tunif.ifname))
        {
          llarp::LogError(Name() + " ifname '", v, "' is too long");
          return false;
        }
        strncpy(tunif.ifname, v.c_str(), sizeof(tunif.ifname) - 1);
        llarp::LogInfo(Name() + " setting ifname to ", tunif.ifname);
        return true;
      }
      if(k == "ifaddr")
      {
        std::string addr;
        m_UseV6  = addr.find(":") != std::string::npos;
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
          if(m_UseV6)
            tunif.netmask = 128;
          else
            tunif.netmask = 32;
          addr = v;
        }
        llarp::LogInfo(Name() + " set ifaddr to ", addr, " with netmask ",
                       tunif.netmask);
        strncpy(tunif.ifaddr, addr.c_str(), sizeof(tunif.ifaddr) - 1);
        return true;
      }
      return Endpoint::SetOption(k, v);
    }

    bool
    TunEndpoint::HasLocalIP(const huint128_t &ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    bool
    TunEndpoint::QueueOutboundTraffic(llarp::net::IPPacket &&pkt)
    {
      return m_NetworkToUserPktQueue.EmplaceIf(
          [](llarp::net::IPPacket &) -> bool { return true; }, std::move(pkt));
    }

    void
    TunEndpoint::Flush()
    {
      auto self = shared_from_this();
      FlushSend();
      if(m_Exit)
      {
        RouterLogic()->queue_func([=] {
          self->m_Exit->FlushUpstream();
          self->Router()->PumpLL();
        });
      }
      RouterLogic()->queue_func([=]() {
        self->Pump(self->Now());
        self->Router()->PumpLL();
      });
    }

    static bool
    is_random_snode(const dns::Message &msg)
    {
      return msg.questions[0].IsName("random.snode");
    }

    static bool
    is_localhost_loki(const dns::Message &msg)
    {
      return msg.questions[0].IsName("localhost.loki");
    }

    bool
    TunEndpoint::HandleHookedDNSMessage(
        dns::Message &&msg, std::function< void(dns::Message) > reply)
    {
      // llarp::LogInfo("Tun.HandleHookedDNSMessage ", msg.questions[0].qname, "
      // of type", msg.questions[0].qtype);
      if(msg.questions.size() != 1)
      {
        llarp::LogWarn("bad number of dns questions: ", msg.questions.size());
        return false;
      }
      const std::string qname = msg.questions[0].Name();
      if(msg.questions[0].qtype == dns::qTypeMX)
      {
        // mx record
        service::Address addr;
        if(addr.FromString(qname, ".loki") || addr.FromString(qname, ".snode")
           || is_random_snode(msg) || is_localhost_loki(msg))
          msg.AddMXReply(qname, 1);
        else
          msg.AddNXReply();
        reply(msg);
      }
      else if(msg.questions[0].qtype == dns::qTypeCNAME)
      {
        if(is_random_snode(msg))
        {
          RouterID random;
          if(Router()->GetRandomGoodRouter(random))
            msg.AddCNAMEReply(random.ToString(), 1);
          else
            msg.AddNXReply();
        }
        else if(is_localhost_loki(msg))
        {
          size_t counter = 0;
          context->ForEachService(
              [&](const std::string &,
                  const std::shared_ptr< service::Endpoint > &service) -> bool {
                const service::Address addr = service->GetIdentity().pub.Addr();
                msg.AddCNAMEReply(addr.ToString(), 1);
                ++counter;
                return true;
              });
          if(counter == 0)
            msg.AddNXReply();
        }
        else
          msg.AddNXReply();
        reply(msg);
      }
      else if(msg.questions[0].qtype == dns::qTypeA
              || msg.questions[0].qtype == dns::qTypeAAAA)
      {
        const bool isV6 =
            msg.questions[0].qtype == dns::qTypeAAAA && SupportsV6();
        const bool isV4 = msg.questions[0].qtype == dns::qTypeA;
        llarp::service::Address addr;
        // on MacOS this is a typeA query
        if(is_random_snode(msg))
        {
          RouterID random;
          if(Router()->GetRandomGoodRouter(random))
            msg.AddCNAMEReply(random.ToString(), 1);
          else
            msg.AddNXReply();
        }
        else if(is_localhost_loki(msg))
        {
          size_t counter = 0;
          context->ForEachService(
              [&](const std::string &,
                  const std::shared_ptr< service::Endpoint > &service) -> bool {
                if(!service->HasIfAddr())
                  return true;
                huint128_t ip = service->GetIfAddr();
                if(ip.h)
                {
                  msg.AddINReply(ip, isV6);
                  ++counter;
                }
                return true;
              });
          if(counter == 0)
            msg.AddNXReply();
        }
        else if(addr.FromString(qname, ".loki"))
        {
          if(isV4 && SupportsV6())
          {
            msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
          }
          else if(HasAddress(addr))
          {
            huint128_t ip = ObtainIPForAddr(addr, false);
            msg.AddINReply(ip, isV6);
          }
          else
          {
            dns::Message *replyMsg = new dns::Message(std::move(msg));
            using service::Address;
            using service::OutboundContext;
            return EnsurePathToService(
                addr,
                [=](const Address &, OutboundContext *ctx) {
                  SendDNSReply(addr, ctx, replyMsg, reply, false,
                               isV6 || !isV4);
                },
                2000);
          }
        }
        else if(addr.FromString(qname, ".snode"))
        {
          if(isV4 && SupportsV6())
          {
            msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
          }
          else
          {
            dns::Message *replyMsg = new dns::Message(std::move(msg));
            EnsurePathToSNode(addr.as_array(),
                              [=](const RouterID &, exit::BaseSession_ptr s) {
                                SendDNSReply(addr, s, replyMsg, reply, true,
                                             isV6);
                              });
            return true;
          }
        }
        else
          msg.AddNXReply();

        reply(msg);
      }
      else if(msg.questions[0].qtype == dns::qTypePTR)
      {
        // reverse dns
        huint128_t ip = {0};
        if(!dns::DecodePTR(msg.questions[0].qname, ip))
        {
          msg.AddNXReply();
          reply(msg);
          return true;
        }
        llarp::service::Address addr(
            ObtainAddrForIP< llarp::service::Address >(ip, true));
        if(!addr.IsZero())
        {
          msg.AddAReply(addr.ToString(".snode"));
          reply(msg);
          return true;
        }
        addr = ObtainAddrForIP< llarp::service::Address >(ip, false);
        if(!addr.IsZero())
        {
          msg.AddAReply(addr.ToString(".loki"));
          reply(msg);
          return true;
        }
        msg.AddNXReply();
        reply(msg);
        return true;
      }
      else
      {
        msg.AddNXReply();
        reply(msg);
      }
      return true;
    }

    void
    TunEndpoint::ResetInternalState()
    {
      service::Endpoint::ResetInternalState();
      if(m_Exit)
        m_Exit->ResetInternalState();
    }

    bool
    TunEndpoint::SupportsV6() const
    {
      return m_UseV6;
    }

    // FIXME: pass in which question it should be addressing
    bool
    TunEndpoint::ShouldHookDNSMessage(const dns::Message &msg) const
    {
      llarp::service::Address addr;
      if(msg.questions.size() == 1)
      {
        /// hook every .loki
        if(msg.questions[0].HasTLD(".loki"))
          return true;
        /// hook every .snode
        if(msg.questions[0].HasTLD(".snode"))
          return true;
        // hook any ranges we own
        if(msg.questions[0].qtype == llarp::dns::qTypePTR)
        {
          huint128_t ip = {0};
          if(!dns::DecodePTR(msg.questions[0].qname, ip))
            return false;
          return m_OurRange.Contains(ip);
        }
      }
      return false;
    }

    bool
    TunEndpoint::MapAddress(const service::Address &addr, huint128_t ip,
                            bool SNode)
    {
      auto itr = m_IPToAddr.find(ip);
      if(itr != m_IPToAddr.end())
      {
        llarp::LogWarn(ip, " already mapped to ",
                       service::Address(itr->second.as_array()).ToString());
        return false;
      }
      llarp::LogInfo(Name() + " map ", addr.ToString(), " to ", ip);

      m_IPToAddr[ip]   = addr;
      m_AddrToIP[addr] = ip;
      m_SNodes[addr]   = SNode;
      MarkIPActiveForever(ip);
      return true;
    }

    bool
    TunEndpoint::Start()
    {
      if(!Endpoint::Start())
      {
        llarp::LogWarn("Couldn't start endpoint");
        return false;
      }
      if(m_Exit)
      {
        for(const auto &snode : SnodeBlacklist())
          m_Exit->BlacklistSnode(snode);
      }
      return SetupNetworking();
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
      auto loop = EndpointNetLoop();
      if(!llarp_ev_add_tun(loop.get(), &tunif))
      {
        llarp::LogError(Name(),
                        " failed to set up tun interface: ", tunif.ifaddr,
                        " on ", tunif.ifname);
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
        llarp::LogError(Name(), " failed to set up tun interface, cant parse
      ", tunif.ifaddr); return false;
      }
      */
      if(res->ai_family == AF_INET6)
      {
        m_UseV6 = true;
      }

      freeaddrinfo(res);
      if(m_UseV6)
      {
        llarp::LogInfo(Name(), " using IPV6");
      }
      else
      {
        struct in_addr addr;  // network byte order
        if(inet_aton(tunif.ifaddr, &addr) == 0)
        {
          llarp::LogError(Name(),
                          " failed to set up tun interface, cant parse ",
                          tunif.ifaddr);
          return false;
        }
      }
      huint32_t ip;
      if(ip.FromString(tunif.ifaddr))
      {
        m_OurIP                 = net::IPPacket::ExpandV4(ip);
        m_OurRange.netmask_bits = netmask_ipv6_bits(tunif.netmask + 96);
      }
      else if(m_OurIP.FromString(tunif.ifaddr))
      {
        m_OurRange.netmask_bits = netmask_ipv6_bits(tunif.netmask);
      }

      m_NextIP        = m_OurIP;
      m_OurRange.addr = m_OurIP;
      m_MaxIP         = m_OurIP | (~m_OurRange.netmask_bits);
      llarp::LogInfo(Name(), " set ", tunif.ifname, " to have address ",
                     m_OurIP);
      llarp::LogInfo(Name(), " allocated up to ", m_MaxIP, " on range ",
                     m_OurRange);

      MapAddress(m_Identity.pub.Addr(), m_OurIP, IsSNode());
      if(m_OnUp)
      {
        m_OnUp->NotifyAsync(NotifyParams());
      }
      return true;
    }

    std::unordered_map< std::string, std::string >
    TunEndpoint::NotifyParams() const
    {
      auto env = Endpoint::NotifyParams();
      env.emplace("IP_ADDR", m_OurIP.ToString());
      env.emplace("IF_ADDR", m_OurRange.ToString());
      env.emplace("IF_NAME", tunif.ifname);
      std::string strictConnect;
      for(const auto &addr : m_StrictConnectAddrs)
        strictConnect += addr.ToString() + " ";
      env.emplace("STRICT_CONNECT_ADDRS", strictConnect);
      return env;
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      llarp::LogInfo("Set Up networking for ", Name());
      if(!SetupTun())
      {
        llarp::LogError(Name(), " failed to set up network interface");
        return false;
      }
      if(!m_Resolver->Start(m_LocalResolverAddr, m_UpstreamResolvers))
      {
        // downgrade DNS server failure to a warning
        llarp::LogWarn(Name(), " failed to start dns server");
        // return false;
      }
      return true;
    }

    void
    TunEndpoint::Tick(llarp_time_t now)
    {
      // call tun code in endpoint logic in case of network isolation
      // EndpointLogic()->queue_job({this, handleTickTun});
      if(m_Exit)
      {
        EnsureRouterIsKnown(m_Exit->Endpoint());
        m_Exit->Tick(now);
      }
      Endpoint::Tick(now);
    }

    bool
    TunEndpoint::Stop()
    {
      if(m_Exit)
        m_Exit->Stop();
      return llarp::service::Endpoint::Stop();
    }

    void
    TunEndpoint::FlushSend()
    {
      m_UserToNetworkPktQueue.Process([&](net::IPPacket &pkt) {
        std::function< bool(const llarp_buffer_t &) > sendFunc;

        huint128_t dst;
        if(pkt.IsV4())
          dst = net::IPPacket::ExpandV4(pkt.dstv4());
        else
          dst = pkt.dstv6();

        auto itr = m_IPToAddr.find(dst);
        if(itr == m_IPToAddr.end())
        {
          if(m_Exit && pkt.IsV4() && !llarp::IsIPv4Bogon(pkt.dstv4()))
          {
            pkt.UpdateIPv4Address({0}, xhtonl(pkt.dstv4()));
            m_Exit->QueueUpstreamTraffic(std::move(pkt),
                                         llarp::routing::ExitPadSize);
          }
          else if(m_Exit && pkt.IsV6())
          {
            pkt.UpdateIPv6Address({0}, pkt.dstv6());
            m_Exit->QueueUpstreamTraffic(std::move(pkt),
                                         llarp::routing::ExitPadSize);
          }
          else
          {
            llarp::LogWarn(Name(), " has no endpoint for ", dst);
            llarp::DumpBuffer(pkt.ConstBuffer());
          }
          return;
        }
        if(m_SNodes.at(itr->second))
        {
          sendFunc = std::bind(&TunEndpoint::SendToSNodeOrQueue, this,
                               itr->second.as_array(), std::placeholders::_1);
        }
        else
        {
          sendFunc = std::bind(&TunEndpoint::SendToServiceOrQueue, this,
                               service::Address(itr->second.as_array()),
                               std::placeholders::_1, pkt.ServiceProtocol());
        }
        // prepare packet for insertion into network
        // this includes clearing IP addresses, recalculating checksums, etc
        if(pkt.IsV4())
          pkt.UpdateIPv4Address({0}, {0});
        else
          pkt.UpdateIPv6Address({0}, {0});

        if(sendFunc && sendFunc(pkt.Buffer()))
        {
          MarkIPActive(dst);
          return;
        }
        llarp::LogWarn(Name(), " did not flush packets");
      });
    }

    bool
    TunEndpoint::HandleWriteIPPacket(
        const llarp_buffer_t &b, std::function< huint128_t(void) > getFromIP)
    {
      // llarp::LogInfo("got packet from ", msg->sender.Addr());
      auto themIP = getFromIP();
      // llarp::LogInfo("themIP ", themIP);
      auto usIP = m_OurIP;
      ManagedBuffer buf(b);
      return m_NetworkToUserPktQueue.EmplaceIf(
          [buf, themIP, usIP](net::IPPacket &pkt) -> bool {
            // load
            if(!pkt.Load(buf))
              return false;
            // filter out:
            // - packets smaller than minimal IPv4 header
            // - non-IPv4 packets
            // - packets with weird src/dst addresses
            //   (0.0.0.0/8 but not 0.0.0.0)
            // - packets with 0 src but non-0 dst and oposite
            if(pkt.IsV4())
            {
              auto hdr = pkt.Header();
              if(pkt.sz < sizeof(*hdr)
                 || (hdr->saddr != 0 && *(byte_t *)&(hdr->saddr) == 0)
                 || (hdr->daddr != 0 && *(byte_t *)&(hdr->daddr) == 0)
                 || ((hdr->saddr == 0) != (hdr->daddr == 0)))
              {
                return false;
              }
              pkt.UpdateIPv4Address(xhtonl(net::IPPacket::TruncateV6(themIP)),
                                    xhtonl(net::IPPacket::TruncateV6(usIP)));
            }
            else if(pkt.IsV6())
            {
              pkt.UpdateIPv6Address(themIP, usIP);
            }
            return true;
          });
    }

    huint128_t
    TunEndpoint::GetIfAddr() const
    {
      return m_OurIP;
    }

    huint128_t
    TunEndpoint::ObtainIPForAddr(const AlignedBuffer< 32 > &addr, bool snode)
    {
      llarp_time_t now  = Now();
      huint128_t nextIP = {0};
      AlignedBuffer< 32 > ident(addr);
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
      std::pair< huint128_t, llarp_time_t > oldest = {huint128_t{0}, 0};

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
    TunEndpoint::HasRemoteForIP(huint128_t ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    void
    TunEndpoint::MarkIPActive(huint128_t ip)
    {
      llarp::LogDebug(Name(), " address ", ip, " is active");
      m_IPActivity[ip] = std::max(Now(), m_IPActivity[ip]);
    }

    void
    TunEndpoint::MarkIPActiveForever(huint128_t ip)
    {
      m_IPActivity[ip] = std::numeric_limits< uint64_t >::max();
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
      // flush user to network
      self->FlushSend();
      // flush exit traffic queues if it's there
      if(self->m_Exit)
      {
        self->m_Exit->FlushDownstream();
      }
      // flush network to user
      self->m_NetworkToUserPktQueue.Process([tun](net::IPPacket &pkt) {
        if(!llarp_ev_tun_async_write(tun, pkt.Buffer()))
          llarp::LogWarn("packet dropped");
      });
    }

    void
    TunEndpoint::tunifRecvPkt(llarp_tun_io *tun, const llarp_buffer_t &b)
    {
      // called for every packet read from user in isolated network thread
      TunEndpoint *self = static_cast< TunEndpoint * >(tun->user);
      const ManagedBuffer buf(b);
      self->m_UserToNetworkPktQueue.EmplaceIf(
          [&buf](net::IPPacket &pkt) -> bool { return pkt.Load(buf); });
    }

    TunEndpoint::~TunEndpoint()
    {
    }

  }  // namespace handlers
}  // namespace llarp

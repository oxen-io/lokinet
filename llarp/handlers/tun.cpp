#include <algorithm>
#include "net/net.hpp"
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
#include <service/outbound_context.hpp>
#include <service/endpoint_state.hpp>
#include <service/outbound_context.hpp>
#include <service/name.hpp>
#include <util/meta/memfn.hpp>
#include <util/thread/logic.hpp>
#include <nodedb.hpp>
#include <rpc/endpoint_rpc.hpp>

#include <util/str.hpp>

#include <dns/srv_data.hpp>

namespace llarp
{
  namespace handlers
  {
    bool
    TunEndpoint::ShouldFlushNow(llarp_time_t now) const
    {
      static constexpr auto FlushInterval = 25ms;
      return now >= m_LastFlushAt + FlushInterval;
    }

    TunEndpoint::TunEndpoint(AbstractRouter* r, service::Context* parent)
        : service::Endpoint(r, parent)
        , m_UserToNetworkPktQueue("endpoint_sendq", r->netloop(), r->netloop())
        , m_Resolver(std::make_shared<dns::Proxy>(
              r->netloop(), r->logic(), r->netloop(), r->logic(), this))
    {}

    util::StatusObject
    TunEndpoint::ExtractStatus() const
    {
      auto obj = service::Endpoint::ExtractStatus();
      obj["ifaddr"] = m_OurRange.ToString();
      obj["ifname"] = m_IfName;
      std::vector<std::string> resolvers;
      for (const auto& addr : m_UpstreamResolvers)
        resolvers.emplace_back(addr.toString());
      obj["ustreamResolvers"] = resolvers;
      obj["localResolver"] = m_LocalResolverAddr.toString();
      util::StatusObject ips{};
      for (const auto& item : m_IPActivity)
      {
        util::StatusObject ipObj{{"lastActive", to_json(item.second)}};
        std::string remoteStr;
        AlignedBuffer<32> addr = m_IPToAddr.at(item.first);
        if (m_SNodes.at(addr))
          remoteStr = RouterID(addr.as_array()).ToString();
        else
          remoteStr = service::Address(addr.as_array()).ToString();
        ipObj["remote"] = remoteStr;
        std::string ipaddr = item.first.ToString();
        ips[ipaddr] = ipObj;
      }
      obj["addrs"] = ips;
      obj["ourIP"] = m_OurIP.ToString();
      obj["nextIP"] = m_NextIP.ToString();
      obj["maxIP"] = m_MaxIP.ToString();
      return obj;
    }

    bool
    TunEndpoint::Configure(const NetworkConfig& conf, const DnsConfig& dnsConf)
    {
      if (conf.m_reachable)
      {
        m_PublishIntroSet = true;
        LogInfo(Name(), " setting to be reachable by default");
      }
      else
      {
        m_PublishIntroSet = false;
        LogInfo(Name(), " setting to be not reachable by default");
      }

      if (conf.m_AuthType != service::AuthType::eAuthTypeNone)
      {
        std::string url, method;
        if (conf.m_AuthUrl.has_value() and conf.m_AuthMethod.has_value())
        {
          url = *conf.m_AuthUrl;
          method = *conf.m_AuthMethod;
        }
        auto auth = std::make_shared<rpc::EndpointAuthRPC>(
            url, method, conf.m_AuthWhitelist, Router()->lmq(), shared_from_this());
        auth->Start();
        m_AuthPolicy = std::move(auth);
      }

      /*
       * TODO: reinstate this option (it's not even clear what section this came from...)
       *
      if (k == "isolate-network" && IsTrueValue(v.c_str()))
      {
#if defined(__linux__)
        LogInfo(Name(), " isolating network...");
        if (!SpawnIsolatedNetwork())
        {
          LogError(Name(), " failed to spawn isolated network");
          return false;
        }
        LogInfo(Name(), " booyeah network isolation succeeded");
        return true;
#else
        LogError(Name(), " network isolation is not supported on your platform");
        return false;
#endif
      }
      */

      /*
       * TODO: this is currently defined for [router] / RouterConfig, but is clearly an [endpoint]
       *       option. either move it to [endpoint] or plumb RouterConfig through
       *
      if (k == "strict-connect")
      {
        RouterID connect;
        if (!connect.FromString(v))
        {
          LogError(Name(), " invalid snode for strict-connect: ", v);
          return false;
        }

        RouterContact rc;
        if (!m_router->nodedb()->Get(connect, rc))
        {
          LogError(
              Name(), " we don't have the RC for ", v, " so we can't use it in strict-connect");
          return false;
        }
        for (const auto& ai : rc.addrs)
        {
          m_StrictConnectAddrs.emplace_back(ai);
          LogInfo(Name(), " added ", m_StrictConnectAddrs.back(), " to strict connect");
        }
        return true;
      }
       */

      m_LocalResolverAddr = dnsConf.m_bind;
      m_UpstreamResolvers = dnsConf.m_upstreamDNS;

      for (const auto& item : conf.m_mapAddrs)
      {
        if (not MapAddress(item.second, item.first, false))
          return false;
      }

      m_IfName = conf.m_ifname;
      if (m_IfName.empty())
      {
        const auto maybe = llarp::FindFreeTun();
        if (not maybe.has_value())
          throw std::runtime_error("cannot find free interface name");
        m_IfName = *maybe;
      }

      m_OurRange = conf.m_ifaddr;
      if (!m_OurRange.addr.h)
      {
        const auto maybe = llarp::FindFreeRange();
        if (not maybe.has_value())
        {
          throw std::runtime_error("cannot find free address range");
        }
        m_OurRange = *maybe;
      }
      m_OurIP = m_OurRange.addr;
      m_UseV6 = not m_OurRange.IsV4();
      return Endpoint::Configure(conf, dnsConf);
    }

    bool
    TunEndpoint::HasLocalIP(const huint128_t& ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    void
    TunEndpoint::Flush()
    {
      FlushSend();
      Pump(Now());
      // flush network to user
      while (not m_NetworkToUserPktQueue.empty())
      {
        m_NetIf->WritePacket(m_NetworkToUserPktQueue.top().pkt);
        m_NetworkToUserPktQueue.pop();
      }
    }

    static bool
    is_random_snode(const dns::Message& msg)
    {
      return msg.questions[0].IsName("random.snode");
    }

    static bool
    is_localhost_loki(const dns::Message& msg)
    {
      return msg.questions[0].IsLocalhost();
    }

    template <>
    bool
    TunEndpoint::FindAddrForIP(service::Address& addr, huint128_t ip)
    {
      auto itr = m_IPToAddr.find(ip);
      if (itr != m_IPToAddr.end() and not m_SNodes[itr->second])
      {
        addr = service::Address(itr->second.as_array());
        return true;
      }
      return false;
    }

    template <>
    bool
    TunEndpoint::FindAddrForIP(RouterID& addr, huint128_t ip)
    {
      auto itr = m_IPToAddr.find(ip);
      if (itr != m_IPToAddr.end() and m_SNodes[itr->second])
      {
        addr = RouterID(itr->second.as_array());
        return true;
      }
      return false;
    }

    static dns::Message&
    clear_dns_message(dns::Message& msg)
    {
      msg.authorities.resize(0);
      msg.additional.resize(0);
      msg.answers.resize(0);
      msg.hdr_fields &= ~dns::flags_RCODENameError;
      return msg;
    }

    bool
    TunEndpoint::HandleHookedDNSMessage(dns::Message msg, std::function<void(dns::Message)> reply)
    {
      auto ReplyToSNodeDNSWhenReady = [self = this, reply = reply](
                                          RouterID snode, auto msg, bool isV6) -> bool {
        return self->EnsurePathToSNode(snode, [=](const RouterID&, exit::BaseSession_ptr s) {
          self->SendDNSReply(snode, s, msg, reply, true, isV6);
        });
      };
      auto ReplyToLokiDNSWhenReady = [self = this, reply = reply](
                                         service::Address addr, auto msg, bool isV6) -> bool {
        using service::Address;
        using service::OutboundContext;
        return self->EnsurePathToService(
            addr,
            [=](const Address&, OutboundContext* ctx) {
              self->SendDNSReply(addr, ctx, msg, reply, false, isV6);
            },
            2s);
      };

      auto ReplyToLokiSRVWhenReady = [self = this, reply = reply](
                                         service::Address addr, auto msg) -> bool {
        using service::Address;
        using service::OutboundContext;

        return self->EnsurePathToService(
            addr,
            [=](const Address&, OutboundContext* ctx) {
              if (ctx == nullptr)
                return;

              const auto& introset = ctx->GetCurrentIntroSet();
              msg->AddSRVReply(introset.GetMatchingSRVRecords(addr.subdomain));
              reply(*msg);
            },
            2s);
      };

      if (msg.answers.size() > 0)
      {
        const auto& answer = msg.answers[0];
        if (answer.HasCNameForTLD(".snode"))
        {
          dns::Name_t qname;
          llarp_buffer_t buf(answer.rData);
          if (not dns::DecodeName(&buf, qname, true))
            return false;
          RouterID addr;
          if (not addr.FromString(qname))
            return false;
          auto replyMsg = std::make_shared<dns::Message>(clear_dns_message(msg));
          return ReplyToSNodeDNSWhenReady(addr, std::move(replyMsg), false);
        }
        else if (answer.HasCNameForTLD(".loki"))
        {
          dns::Name_t qname;
          llarp_buffer_t buf(answer.rData);
          if (not dns::DecodeName(&buf, qname, true))
            return false;

          service::Address addr;
          if (not addr.FromString(qname))
            return false;

          auto replyMsg = std::make_shared<dns::Message>(clear_dns_message(msg));
          return ReplyToLokiDNSWhenReady(addr, replyMsg, false);
        }
      }
      if (msg.questions.size() != 1)
      {
        llarp::LogWarn("bad number of dns questions: ", msg.questions.size());
        return false;
      }
      std::string qname = msg.questions[0].Name();
      const auto nameparts = split(qname, ".");
      std::string lnsName;
      if (nameparts.size() >= 2 and ends_with(qname, ".loki"))
      {
        lnsName = nameparts[nameparts.size() - 2];
        lnsName += ".loki"sv;
      }
      if (msg.questions[0].qtype == dns::qTypeTXT)
      {
        RouterID snode;
        if (snode.FromString(qname))
        {
          m_router->LookupRouter(snode, [reply, msg = std::move(msg)](const auto& found) mutable {
            if (found.empty())
            {
              msg.AddNXReply();
            }
            else
            {
              std::stringstream ss;
              for (const auto& rc : found)
                rc.ToTXTRecord(ss);
              msg.AddTXTReply(ss.str());
            }
            reply(msg);
          });
          return true;
        }
        else if (msg.questions[0].IsLocalhost() and msg.questions[0].HasSubdomains())
        {
          const auto subdomain = msg.questions[0].Subdomains();
          if (subdomain == "exit")
          {
            if (HasExit())
            {
              std::stringstream ss;
              m_ExitMap.ForEachEntry([&ss](const auto& range, const auto& exit) {
                ss << range.ToString() << "=" << exit.ToString() << "; ";
              });
              msg.AddTXTReply(ss.str());
            }
            else
            {
              msg.AddNXReply();
            }
          }
          else if (subdomain == "netid")
          {
            std::stringstream ss;
            ss << "netid=" << m_router->rc().netID.ToString() << ";";
            msg.AddTXTReply(ss.str());
          }
          else
          {
            msg.AddNXReply();
          }
        }
        else
        {
          msg.AddNXReply();
        }

        reply(msg);
      }
      else if (msg.questions[0].qtype == dns::qTypeMX)
      {
        // mx record
        service::Address addr;
        if (addr.FromString(qname, ".loki") || addr.FromString(qname, ".snode")
            || is_random_snode(msg) || is_localhost_loki(msg))
          msg.AddMXReply(qname, 1);
        else
          msg.AddNXReply();
        reply(msg);
      }
      else if (msg.questions[0].qtype == dns::qTypeCNAME)
      {
        if (is_random_snode(msg))
        {
          RouterID random;
          if (Router()->GetRandomGoodRouter(random))
          {
            msg.AddCNAMEReply(random.ToString(), 1);
          }
          else
            msg.AddNXReply();
        }
        else if (msg.questions[0].IsLocalhost() and msg.questions[0].HasSubdomains())
        {
          const auto subdomain = msg.questions[0].Subdomains();
          if (subdomain == "exit" and HasExit())
          {
            m_ExitMap.ForEachEntry(
                [&msg](const auto&, const auto& exit) { msg.AddCNAMEReply(exit.ToString(), 1); });
          }
          else
          {
            msg.AddNXReply();
          }
        }
        else if (is_localhost_loki(msg))
        {
          size_t counter = 0;
          context->ForEachService(
              [&](const std::string&, const std::shared_ptr<service::Endpoint>& service) -> bool {
                const service::Address addr = service->GetIdentity().pub.Addr();
                msg.AddCNAMEReply(addr.ToString(), 1);
                ++counter;
                return true;
              });
          if (counter == 0)
            msg.AddNXReply();
        }
        else
          msg.AddNXReply();
        reply(msg);
      }
      else if (msg.questions[0].qtype == dns::qTypeA || msg.questions[0].qtype == dns::qTypeAAAA)
      {
        const bool isV6 = msg.questions[0].qtype == dns::qTypeAAAA;
        const bool isV4 = msg.questions[0].qtype == dns::qTypeA;
        llarp::service::Address addr;
        if (isV6 && !SupportsV6())
        {  // empty reply but not a NXDOMAIN so that client can retry IPv4
          msg.AddNSReply("localhost.loki.");
        }
        // on MacOS this is a typeA query
        else if (is_random_snode(msg))
        {
          RouterID random;
          if (Router()->GetRandomGoodRouter(random))
          {
            msg.AddCNAMEReply(random.ToString(), 1);
            return ReplyToSNodeDNSWhenReady(random, std::make_shared<dns::Message>(msg), isV6);
          }
          else
            msg.AddNXReply();
        }
        else if (is_localhost_loki(msg))
        {
          const bool lookingForExit = msg.questions[0].Subdomains() == "exit";
          huint128_t ip = GetIfAddr();
          if (ip.h)
          {
            if (lookingForExit)
            {
              if (HasExit())
              {
                m_ExitMap.ForEachEntry(
                    [&msg](const auto&, const auto& exit) { msg.AddCNAMEReply(exit.ToString()); });
                msg.AddINReply(ip, isV6);
              }
              else
              {
                msg.AddNXReply();
              }
            }
            else
            {
              msg.AddCNAMEReply(m_Identity.pub.Name(), 1);
              msg.AddINReply(ip, isV6);
            }
          }
          else
          {
            msg.AddNXReply();
          }
        }
        else if (addr.FromString(qname, ".loki"))
        {
          if (isV4 && SupportsV6())
          {
            msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
          }
          else
          {
            return ReplyToLokiDNSWhenReady(addr, std::make_shared<dns::Message>(msg), isV6);
          }
        }
        else if (addr.FromString(qname, ".snode"))
        {
          if (isV4 && SupportsV6())
          {
            msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
          }
          else
          {
            return ReplyToSNodeDNSWhenReady(
                addr.as_array(), std::make_shared<dns::Message>(msg), isV6);
          }
        }
        else if (service::NameIsValid(lnsName))
        {
          return LookupNameAsync(
              lnsName,
              [msg = std::make_shared<dns::Message>(msg),
               name = Name(),
               lnsName,
               isV6,
               reply,
               ReplyToLokiDNSWhenReady](auto maybe) {
                if (not maybe.has_value())
                {
                  LogWarn(name, " lns name ", lnsName, " not resolved");
                  msg->AddNXReply();
                  reply(*msg);
                  return;
                }
                LogInfo(name, " ", lnsName, " resolved to ", maybe->ToString());
                ReplyToLokiDNSWhenReady(*maybe, msg, isV6);
              });
        }
        else
          msg.AddNXReply();

        reply(msg);
      }
      else if (msg.questions[0].qtype == dns::qTypePTR)
      {
        // reverse dns
        huint128_t ip = {0};
        if (!dns::DecodePTR(msg.questions[0].qname, ip))
        {
          msg.AddNXReply();
          reply(msg);
          return true;
        }
        RouterID snodeAddr;
        if (FindAddrForIP(snodeAddr, ip))
        {
          msg.AddAReply(snodeAddr.ToString());
          reply(msg);
          return true;
        }
        service::Address lokiAddr;
        if (FindAddrForIP(lokiAddr, ip))
        {
          msg.AddAReply(lokiAddr.ToString());
          reply(msg);
          return true;
        }
        msg.AddNXReply();
        reply(msg);
        return true;
      }
      else if (msg.questions[0].qtype == dns::qTypeSRV)
      {
        llarp::service::Address addr;

        if (is_localhost_loki(msg))
        {
          msg.AddSRVReply(introSet().GetMatchingSRVRecords(msg.questions[0].Subdomains()));
          reply(msg);
          return true;
        }
        else if (addr.FromString(qname, ".loki"))
        {
          llarp::LogDebug("SRV request for: ", qname);

          return ReplyToLokiSRVWhenReady(addr, std::make_shared<dns::Message>(msg));
        }
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
    }

    bool
    TunEndpoint::SupportsV6() const
    {
      return m_UseV6;
    }

    // FIXME: pass in which question it should be addressing
    bool
    TunEndpoint::ShouldHookDNSMessage(const dns::Message& msg) const
    {
      llarp::service::Address addr;
      if (msg.questions.size() == 1)
      {
        /// hook every .loki
        if (msg.questions[0].HasTLD(".loki"))
          return true;
        /// hook every .snode
        if (msg.questions[0].HasTLD(".snode"))
          return true;
        // hook any ranges we own
        if (msg.questions[0].qtype == llarp::dns::qTypePTR)
        {
          huint128_t ip = {0};
          if (!dns::DecodePTR(msg.questions[0].qname, ip))
            return false;
          return m_OurRange.Contains(ip);
        }
      }
      for (const auto& answer : msg.answers)
      {
        if (answer.HasCNameForTLD(".loki"))
          return true;
        if (answer.HasCNameForTLD(".snode"))
          return true;
      }
      return false;
    }

    bool
    TunEndpoint::MapAddress(const service::Address& addr, huint128_t ip, bool SNode)
    {
      auto itr = m_IPToAddr.find(ip);
      if (itr != m_IPToAddr.end())
      {
        llarp::LogWarn(
            ip, " already mapped to ", service::Address(itr->second.as_array()).ToString());
        return false;
      }
      llarp::LogInfo(Name() + " map ", addr.ToString(), " to ", ip);

      m_IPToAddr[ip] = addr;
      m_AddrToIP[addr] = ip;
      m_SNodes[addr] = SNode;
      MarkIPActiveForever(ip);
      return true;
    }

    std::string
    TunEndpoint::GetIfName() const
    {
#ifdef _WIN32
      return net::TruncateV6(GetIfAddr()).ToString();
#else
      return m_IfName;
#endif
    }

    bool
    TunEndpoint::Start()
    {
      if (!Endpoint::Start())
      {
        llarp::LogWarn("Couldn't start endpoint");
        return false;
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
      m_NextIP = m_OurIP;
      m_MaxIP = m_OurRange.HighestAddr();
      llarp::LogInfo(Name(), " set ", m_IfName, " to have address ", m_OurIP);
      llarp::LogInfo(Name(), " allocated up to ", m_MaxIP, " on range ", m_OurRange);

      const service::Address ourAddr = m_Identity.pub.Addr();

      if (not MapAddress(ourAddr, GetIfAddr(), false))
      {
        return false;
      }

      vpn::InterfaceInfo info;
      info.addrs.emplace(m_OurRange);
      info.ifname = m_IfName;
      info.dnsaddr.FromString(m_LocalResolverAddr.toHost());

      LogInfo(Name(), " setting up network...");

      try
      {
        m_NetIf = Router()->GetVPNPlatform()->ObtainInterface(std::move(info));
      }
      catch (std::exception& ex)
      {
        LogError(Name(), " failed to set up network interface: ", ex.what());
      }
      if (not m_NetIf)
      {
        LogError(Name(), " failed to obtain network interface");
        return false;
      }
      m_IfName = m_NetIf->IfName();
      LogInfo(Name(), " got network interface ", m_IfName);

      auto netloop = Router()->netloop();
      if (not netloop->add_network_interface(
              m_NetIf, [&](net::IPPacket pkt) { HandleGotUserPacket(std::move(pkt)); }))
      {
        LogError(Name(), " failed to add network interface");
        return false;
      }

      netloop->add_ticker([&]() { Flush(); });

      if (m_OnUp)
      {
        m_OnUp->NotifyAsync(NotifyParams());
      }
      return HasAddress(ourAddr);
    }

    std::unordered_map<std::string, std::string>
    TunEndpoint::NotifyParams() const
    {
      auto env = Endpoint::NotifyParams();
      env.emplace("IP_ADDR", m_OurIP.ToString());
      env.emplace("IF_ADDR", m_OurRange.ToString());
      env.emplace("IF_NAME", m_IfName);
      std::string strictConnect;
      for (const auto& addr : m_StrictConnectAddrs)
        strictConnect += addr.toString() + " ";
      env.emplace("STRICT_CONNECT_ADDRS", strictConnect);
      return env;
    }

    bool
    TunEndpoint::SetupNetworking()
    {
      llarp::LogInfo("Set Up networking for ", Name());
      if (!SetupTun())
      {
        llarp::LogError(Name(), " failed to set up network interface");
        return false;
      }
      if (!m_Resolver->Start(m_LocalResolverAddr, m_UpstreamResolvers))
      {
        llarp::LogError(Name(), " failed to start DNS server");
        return false;
      }
      return true;
    }

    void
    TunEndpoint::Tick(llarp_time_t now)
    {
      Endpoint::Tick(now);
    }

    bool
    TunEndpoint::Stop()
    {
      if (m_Resolver)
        m_Resolver->Stop();
      return llarp::service::Endpoint::Stop();
    }

    void
    TunEndpoint::FlushSend()
    {
      m_UserToNetworkPktQueue.Process([&](net::IPPacket& pkt) {
        std::function<bool(const llarp_buffer_t&)> sendFunc;

        huint128_t dst, src;
        if (pkt.IsV4())
        {
          dst = pkt.dst4to6();
          src = pkt.src4to6();
        }
        else
        {
          dst = pkt.dstv6();
          src = pkt.srcv6();
        }
        /*
        constexpr huint128_t ipv6_multicast_all_nodes =
            huint128_t{uint128_t{0xff01'0000'0000'0000UL, 1UL}};
        constexpr huint128_t ipv6_multicast_all_routers =
            huint128_t{uint128_t{0xff01'0000'0000'0000UL, 2UL}};
        if (dst == ipv6_multicast_all_nodes and m_state->m_ExitEnabled)
        {
          // send ipv6 multicast
          for (const auto& [ip, addr] : m_IPToAddr)
          {
            (void)ip;
            SendToServiceOrQueue(
                service::Address{addr.as_array()}, pkt.ConstBuffer(), service::eProtocolExit);
          }
          return;
        }

        */

        auto itr = m_IPToAddr.find(dst);
        if (itr == m_IPToAddr.end())
        {
          const auto exits = m_ExitMap.FindAll(dst);
          if (IsBogon(dst) or exits.empty())
          {
            // send icmp unreachable
            const auto icmp = pkt.MakeICMPUnreachable();
            if (icmp.has_value())
            {
              HandleWriteIPPacket(icmp->ConstBuffer(), dst, src, 0);
            }
          }
          else
          {
            const auto addr = *exits.begin();
            if (pkt.IsV4())
              pkt.ZeroSourceAddress();
            MarkAddressOutbound(addr);
            EnsurePathToService(
                addr,
                [addr, pkt, self = this](service::Address, service::OutboundContext* ctx) {
                  if (ctx)
                  {
                    ctx->sendTimeout = 5s;
                  }
                  self->SendToServiceOrQueue(addr, pkt.ConstBuffer(), service::eProtocolExit);
                },
                1s);
          }
          return;
        }
        if (m_SNodes.at(itr->second))
        {
          sendFunc = std::bind(
              &TunEndpoint::SendToSNodeOrQueue,
              this,
              itr->second.as_array(),
              std::placeholders::_1);
        }
        else if (m_state->m_ExitEnabled)
        {
          sendFunc = std::bind(
              &TunEndpoint::SendToServiceOrQueue,
              this,
              service::Address(itr->second.as_array()),
              std::placeholders::_1,
              service::eProtocolExit);
        }
        else
        {
          sendFunc = std::bind(
              &TunEndpoint::SendToServiceOrQueue,
              this,
              service::Address(itr->second.as_array()),
              std::placeholders::_1,
              pkt.ServiceProtocol());
        }
        // prepare packet for insertion into network
        // this includes clearing IP addresses, recalculating checksums, etc
        if (not m_state->m_ExitEnabled)
        {
          if (pkt.IsV4())
            pkt.UpdateIPv4Address({0}, {0});
          else
            pkt.UpdateIPv6Address({0}, {0});
        }
        if (sendFunc && sendFunc(pkt.Buffer()))
        {
          MarkIPActive(dst);
          return;
        }
        llarp::LogWarn(Name(), " did not flush packets");
      });
    }

    bool
    TunEndpoint::HandleInboundPacket(
        const service::ConvoTag tag,
        const llarp_buffer_t& buf,
        service::ProtocolType t,
        uint64_t seqno)
    {
      if (t != service::eProtocolTrafficV4 && t != service::eProtocolTrafficV6
          && t != service::eProtocolExit)
        return false;
      AlignedBuffer<32> addr;
      bool snode = false;
      if (!GetEndpointWithConvoTag(tag, addr, snode))
        return false;
      huint128_t src, dst;

      net::IPPacket pkt;
      if (not pkt.Load(buf))
        return false;

      if (m_state->m_ExitEnabled)
      {
        // exit side from exit
        src = ObtainIPForAddr(addr, snode);
        if (pkt.IsV4())
          dst = pkt.dst4to6Lan();
        else if (pkt.IsV6())
          dst = pkt.dstv6();
      }
      else if (t == service::eProtocolExit)
      {
        // client side exit traffic from exit
        if (pkt.IsV4())
        {
          dst = m_OurIP;
          src = pkt.src4to6();
        }
        else if (pkt.IsV6())
        {
          dst = pkt.dstv6();
          src = pkt.srcv6();
        }
        // find what exit we think this should be for
        const auto mapped = m_ExitMap.FindAll(src);
        if (mapped.count(service::Address{addr}) == 0 or IsBogon(src))
        {
          // we got exit traffic from someone who we should not have gotten it from
          return false;
        }
      }
      else
      {
        // snapp traffic
        src = ObtainIPForAddr(addr, snode);
        dst = m_OurIP;
      }
      HandleWriteIPPacket(buf, src, dst, seqno);
      return true;
    }

    bool
    TunEndpoint::HandleWriteIPPacket(
        const llarp_buffer_t& b, huint128_t src, huint128_t dst, uint64_t seqno)
    {
      ManagedBuffer buf(b);
      WritePacket write;
      write.seqno = seqno;
      auto& pkt = write.pkt;
      // load
      if (!pkt.Load(buf))
        return false;
      if (pkt.IsV4())
      {
        pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(src)), xhtonl(net::TruncateV6(dst)));
      }
      else if (pkt.IsV6())
      {
        pkt.UpdateIPv6Address(src, dst);
      }
      m_NetworkToUserPktQueue.push(std::move(write));
      return true;
    }

    huint128_t
    TunEndpoint::GetIfAddr() const
    {
      return m_OurIP;
    }

    huint128_t
    TunEndpoint::ObtainIPForAddr(const AlignedBuffer<32>& ident, bool snode)
    {
      llarp_time_t now = Now();
      huint128_t nextIP = {0};
      {
        // previously allocated address
        auto itr = m_AddrToIP.find(ident);
        if (itr != m_AddrToIP.end())
        {
          // mark ip active
          MarkIPActive(itr->second);
          return itr->second;
        }
      }
      // allocate new address
      if (m_NextIP < m_MaxIP)
      {
        do
        {
          nextIP = ++m_NextIP;
        } while (m_IPToAddr.find(nextIP) != m_IPToAddr.end() && m_NextIP < m_MaxIP);
        if (nextIP < m_MaxIP)
        {
          m_AddrToIP[ident] = nextIP;
          m_IPToAddr[nextIP] = ident;
          m_SNodes[ident] = snode;
          llarp::LogInfo(Name(), " mapped ", ident, " to ", nextIP);
          MarkIPActive(nextIP);
          return nextIP;
        }
      }

      // we are full
      // expire least active ip
      // TODO: prevent DoS
      std::pair<huint128_t, llarp_time_t> oldest = {huint128_t{0}, 0s};

      // find oldest entry
      auto itr = m_IPActivity.begin();
      while (itr != m_IPActivity.end())
      {
        if (itr->second <= now)
        {
          if ((now - itr->second) > oldest.second)
          {
            oldest.first = itr->first;
            oldest.second = itr->second;
          }
        }
        ++itr;
      }
      // remap address
      m_IPToAddr[oldest.first] = ident;
      m_AddrToIP[ident] = oldest.first;
      m_SNodes[ident] = snode;
      nextIP = oldest.first;

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
      m_IPActivity[ip] = std::numeric_limits<llarp_time_t>::max();
    }

    void
    TunEndpoint::HandleGotUserPacket(net::IPPacket pkt)
    {
      m_UserToNetworkPktQueue.Emplace(std::move(pkt));
    }

    TunEndpoint::~TunEndpoint() = default;

  }  // namespace handlers
}  // namespace llarp

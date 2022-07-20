#include <algorithm>
#include <iterator>
#include <variant>
#include "tun.hpp"
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <llarp/dns/dns.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/net/net.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/systemd_resolved.hpp>
#include <llarp/service/context.hpp>
#include <llarp/service/outbound_context.hpp>
#include <llarp/service/endpoint_state.hpp>
#include <llarp/service/outbound_context.hpp>
#include <llarp/service/name.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/rpc/endpoint_rpc.hpp>
#include <llarp/util/str.hpp>
#include <llarp/dns/srv_data.hpp>

#include <oxenc/bt.h>

namespace llarp
{
  namespace handlers
  {
    // Intercepts DNS IP packets going to an IP on the tun interface; this is currently used on
    // Android and macOS where binding to a DNS port (i.e. via llarp::dns::Proxy) isn't possible
    // because of OS restrictions, but a tun interface *is* available.
    class DnsInterceptor : public dns::PacketHandler
    {
     public:
      TunEndpoint* const m_Endpoint;

      explicit DnsInterceptor(AbstractRouter* router, TunEndpoint* ep)
          : dns::PacketHandler{router->loop(), ep}, m_Endpoint{ep} {};

      void
      SendServerMessageBufferTo(
          const SockAddr& to, const SockAddr& from, llarp_buffer_t buf) override
      {
        const auto pkt = net::IPPacket::UDP(
            from.getIPv4(),
            ToNet(huint16_t{from.getPort()}),
            to.getIPv4(),
            ToNet(huint16_t{to.getPort()}),
            buf);

        if (pkt.sz == 0)
          return;
        m_Endpoint->HandleWriteIPPacket(
            pkt.ConstBuffer(), net::ExpandV4(from.asIPv4()), net::ExpandV4(to.asIPv4()), 0);
      }

#ifdef ANDROID
      bool
      IsUpstreamResolver(const SockAddr&, const SockAddr&) const override
      {
        return true;
      }
#endif

#ifdef __APPLE__
      // DNS on Apple is a bit weird because in order for the NetworkExtension itself to send data
      // through the tunnel we have to proxy DNS requests through Apple APIs (and so our actual
      // upstream DNS won't be set in our resolvers, which is why the vanilla IsUpstreamResolver
      // won't work for us.  However when active the mac also only queries the main tunnel IP for
      // DNS, so we consider anything else to be upstream-bound DNS to let it through the tunnel.
      bool
      IsUpstreamResolver(const SockAddr& to, const SockAddr& from) const override
      {
        return to.asIPv6() != m_Endpoint->GetIfAddr();
      }
#endif
    };

    TunEndpoint::TunEndpoint(AbstractRouter* r, service::Context* parent)
        : service::Endpoint(r, parent)
    {
      m_PacketRouter = std::make_unique<vpn::PacketRouter>(
          [this](net::IPPacket pkt) { HandleGotUserPacket(std::move(pkt)); });
#if defined(ANDROID) || defined(__APPLE__)
      m_Resolver = std::make_shared<DnsInterceptor>(r, this);
      m_PacketRouter->AddUDPHandler(huint16_t{53}, [&](net::IPPacket pkt) {
        const size_t ip_header_size = (pkt.Header()->ihl * 4);

        const uint8_t* ptr = pkt.buf + ip_header_size;
        const auto dst = ToNet(pkt.dstv4());
        const auto src = ToNet(pkt.srcv4());
        const SockAddr laddr{src, nuint16_t{*reinterpret_cast<const uint16_t*>(ptr)}};
        const SockAddr raddr{dst, nuint16_t{*reinterpret_cast<const uint16_t*>(ptr + 2)}};

        OwnedBuffer buf{pkt.sz - (8 + ip_header_size)};
        std::copy_n(ptr + 8, buf.sz, buf.buf.get());
        if (m_Resolver->ShouldHandlePacket(raddr, laddr, buf))
          m_Resolver->HandlePacket(raddr, laddr, buf);
        else
          HandleGotUserPacket(std::move(pkt));
      });
#else
      m_Resolver = std::make_shared<dns::Proxy>(r->loop(), this);
#endif
    }

    util::StatusObject
    TunEndpoint::ExtractStatus() const
    {
      auto obj = service::Endpoint::ExtractStatus();
      obj["ifaddr"] = m_OurRange.ToString();
      obj["ifname"] = m_IfName;
      std::vector<std::string> resolvers;
      for (const auto& addr : m_UpstreamResolvers)
        resolvers.emplace_back(addr.ToString());
      obj["ustreamResolvers"] = resolvers;
      obj["localResolver"] = m_LocalResolverAddr.ToString();
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

    void
    TunEndpoint::Thaw()
    {
      if (m_Resolver)
        m_Resolver->Restart();
    }

    std::vector<SockAddr>
    TunEndpoint::ReconfigureDNS(std::vector<SockAddr> servers)
    {
      std::swap(m_UpstreamResolvers, servers);
      m_Resolver->Stop();
      if (!m_Resolver->Start(
              m_LocalResolverAddr.createSockAddr(), m_UpstreamResolvers, m_hostfiles))
        llarp::LogError(Name(), " failed to reconfigure DNS server");
      return servers;
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

      if (conf.m_AuthType == service::AuthType::eAuthTypeFile)
      {
        m_AuthPolicy = service::MakeFileAuthPolicy(m_router, conf.m_AuthFiles, conf.m_AuthFileType);
      }
      else if (conf.m_AuthType != service::AuthType::eAuthTypeNone)
      {
        std::string url, method;
        if (conf.m_AuthUrl.has_value() and conf.m_AuthMethod.has_value())
        {
          url = *conf.m_AuthUrl;
          method = *conf.m_AuthMethod;
        }
        auto auth = std::make_shared<rpc::EndpointAuthRPC>(
            url,
            method,
            conf.m_AuthWhitelist,
            conf.m_AuthStaticTokens,
            Router()->lmq(),
            shared_from_this());
        auth->Start();
        m_AuthPolicy = std::move(auth);
      }

      m_TrafficPolicy = conf.m_TrafficPolicy;
      m_OwnedRanges = conf.m_OwnedRanges;

      m_LocalResolverAddr = dnsConf.m_bind;
      m_UpstreamResolvers = dnsConf.m_upstreamDNS;
      m_hostfiles = dnsConf.m_hostfiles;

      m_BaseV6Address = conf.m_baseV6Address;

      if (conf.m_PathAlignmentTimeout)
      {
        m_PathAlignmentTimeout = *conf.m_PathAlignmentTimeout;
      }
      else
        m_PathAlignmentTimeout = service::Endpoint::PathAlignmentTimeout();

      for (const auto& item : conf.m_mapAddrs)
      {
        if (not MapAddress(item.second, item.first, false))
          return false;
      }

      m_IfName = conf.m_ifname;
      if (m_IfName.empty())
      {
        const auto maybe = m_router->Net().FindFreeTun();
        if (not maybe.has_value())
          throw std::runtime_error("cannot find free interface name");
        m_IfName = *maybe;
      }

      m_OurRange = conf.m_ifaddr;
      if (!m_OurRange.addr.h)
      {
        const auto maybe = m_router->Net().FindFreeRange();
        if (not maybe.has_value())
        {
          throw std::runtime_error("cannot find free address range");
        }
        m_OurRange = *maybe;
      }

      m_OurIP = m_OurRange.addr;
      m_UseV6 = false;

      m_PersistAddrMapFile = conf.m_AddrMapPersistFile;
      if (m_PersistAddrMapFile)
      {
        const auto& file = *m_PersistAddrMapFile;
        if (fs::exists(file))
        {
          bool shouldLoadFile = true;
          {
            constexpr auto LastModifiedWindow = 1min;
            const auto lastmodified = fs::last_write_time(file);
            const auto now = decltype(lastmodified)::clock::now();
            if (now < lastmodified or now - lastmodified > LastModifiedWindow)
            {
              shouldLoadFile = false;
            }
          }
          std::vector<char> data;
          if (auto maybe = util::OpenFileStream<fs::ifstream>(file, std::ios_base::binary);
              maybe and shouldLoadFile)
          {
            LogInfo(Name(), " loading address map file from ", file);
            maybe->seekg(0, std::ios_base::end);
            const size_t len = maybe->tellg();
            maybe->seekg(0, std::ios_base::beg);
            data.resize(len);
            LogInfo(Name(), " reading ", len, " bytes");
            maybe->read(data.data(), data.size());
          }
          else
          {
            if (shouldLoadFile)
            {
              LogInfo(Name(), " address map file ", file, " does not exist, so we won't load it");
            }
            else
              LogInfo(Name(), " address map file ", file, " not loaded because it's stale");
          }
          if (not data.empty())
          {
            std::string_view bdata{data.data(), data.size()};
            LogDebug(Name(), " parsing address map data: ", bdata);
            const auto parsed = oxenc::bt_deserialize<oxenc::bt_dict>(bdata);
            for (const auto& [key, value] : parsed)
            {
              huint128_t ip{};
              if (not ip.FromString(key))
              {
                LogWarn(Name(), " malformed IP in addr map data: ", key);
                continue;
              }
              if (m_OurIP == ip)
                continue;
              if (not m_OurRange.Contains(ip))
              {
                LogWarn(Name(), " out of range IP in addr map data: ", ip);
                continue;
              }
              EndpointBase::AddressVariant_t addr;

              if (const auto* str = std::get_if<std::string>(&value))
              {
                if (auto maybe = service::ParseAddress(*str))
                {
                  addr = *maybe;
                }
                else
                {
                  LogWarn(Name(), " invalid address in addr map: ", *str);
                  continue;
                }
              }
              else
              {
                LogWarn(Name(), " invalid first entry in addr map, not a string");
                continue;
              }
              if (const auto* loki = std::get_if<service::Address>(&addr))
              {
                m_IPToAddr.emplace(ip, loki->data());
                m_AddrToIP.emplace(loki->data(), ip);
                m_SNodes[*loki] = false;
                LogInfo(Name(), " remapped ", ip, " to ", *loki);
              }
              if (const auto* snode = std::get_if<RouterID>(&addr))
              {
                m_IPToAddr.emplace(ip, snode->data());
                m_AddrToIP.emplace(snode->data(), ip);
                m_SNodes[*snode] = true;
                LogInfo(Name(), " remapped ", ip, " to ", *snode);
              }
              if (m_NextIP < ip)
                m_NextIP = ip;
              // make sure we dont unmap this guy
              MarkIPActive(ip);
            }
          }
        }
        else
        {
          LogInfo(
              Name(), " skipping loading addr map at ", file, " as it does not currently exist");
        }
      }

      if (auto* quic = GetQUICTunnel())
      {
        quic->listen([this](std::string_view, uint16_t port) {
          return llarp::SockAddr{net::TruncateV6(GetIfAddr()), huint16_t{port}};
        });
      }

      return Endpoint::Configure(conf, dnsConf);
    }

    bool
    TunEndpoint::HasLocalIP(const huint128_t& ip) const
    {
      return m_IPToAddr.find(ip) != m_IPToAddr.end();
    }

    void
    TunEndpoint::Pump(llarp_time_t now)
    {
      // flush network to user
      while (not m_NetworkToUserPktQueue.empty())
      {
        m_NetIf->WritePacket(m_NetworkToUserPktQueue.top().pkt);
        m_NetworkToUserPktQueue.pop();
      }

      service::Endpoint::Pump(now);
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

    static dns::Message&
    clear_dns_message(dns::Message& msg)
    {
      msg.authorities.resize(0);
      msg.additional.resize(0);
      msg.answers.resize(0);
      msg.hdr_fields &= ~dns::flags_RCODENameError;
      return msg;
    }

    std::optional<std::variant<service::Address, RouterID>>
    TunEndpoint::ObtainAddrForIP(huint128_t ip) const
    {
      auto itr = m_IPToAddr.find(ip);
      if (itr == m_IPToAddr.end())
        return std::nullopt;
      if (m_SNodes.at(itr->second))
        return RouterID{itr->second.as_array()};
      else
        return service::Address{itr->second.as_array()};
    }

    bool
    TunEndpoint::HandleHookedDNSMessage(dns::Message msg, std::function<void(dns::Message)> reply)
    {
      auto ReplyToSNodeDNSWhenReady = [this, reply](RouterID snode, auto msg, bool isV6) -> bool {
        return EnsurePathToSNode(
            snode,
            [this, snode, msg, reply, isV6](
                const RouterID&, exit::BaseSession_ptr s, [[maybe_unused]] service::ConvoTag tag) {
              SendDNSReply(snode, s, msg, reply, isV6);
            });
      };
      auto ReplyToLokiDNSWhenReady = [this, reply, timeout = PathAlignmentTimeout()](
                                         service::Address addr, auto msg, bool isV6) -> bool {
        using service::Address;
        using service::OutboundContext;
        if (HasInboundConvo(addr))
        {
          // if we have an inbound convo to this address don't mark as outbound so we don't have a
          // state race this codepath is hit when an application verifies that reverse and forward
          // dns records match for an inbound session
          SendDNSReply(addr, this, msg, reply, isV6);
          return true;
        }
        MarkAddressOutbound(addr);
        return EnsurePathToService(
            addr,
            [this, addr, msg, reply, isV6](const Address&, OutboundContext* ctx) {
              SendDNSReply(addr, ctx, msg, reply, isV6);
            },
            timeout);
      };

      auto ReplyToDNSWhenReady = [ReplyToLokiDNSWhenReady, ReplyToSNodeDNSWhenReady](
                                     auto addr, auto msg, bool isV6) {
        if (auto ptr = std::get_if<RouterID>(&addr))
        {
          ReplyToSNodeDNSWhenReady(*ptr, msg, isV6);
          return;
        }
        if (auto ptr = std::get_if<service::Address>(&addr))
        {
          ReplyToLokiDNSWhenReady(*ptr, msg, isV6);
          return;
        }
      };

      auto ReplyToLokiSRVWhenReady = [this, reply, timeout = PathAlignmentTimeout()](
                                         service::Address addr, auto msg) -> bool {
        using service::Address;
        using service::OutboundContext;
        // TODO: how do we handle SRV record lookups for inbound sessions?
        MarkAddressOutbound(addr);
        return EnsurePathToService(
            addr,
            [msg, addr, reply](const Address&, OutboundContext* ctx) {
              if (ctx == nullptr)
                return;

              const auto& introset = ctx->GetCurrentIntroSet();
              msg->AddSRVReply(introset.GetMatchingSRVRecords(addr.subdomain));
              reply(*msg);
            },
            timeout);
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
              std::string recs;
              for (const auto& rc : found)
                recs += rc.ToTXTRecord();
              msg.AddTXTReply(std::move(recs));
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
              std::string s;
              m_ExitMap.ForEachEntry([&s](const auto& range, const auto& exit) {
                fmt::format_to(std::back_inserter(s), "{}={}; ", range, exit);
              });
              msg.AddTXTReply(std::move(s));
            }
            else
            {
              msg.AddNXReply();
            }
          }
          else if (subdomain == "netid")
          {
            msg.AddTXTReply(fmt::format("netid={};", m_router->rc().netID));
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
        {
          msg.AddMXReply(qname, 1);
        }
        else if (service::NameIsValid(lnsName))
        {
          LookupNameAsync(lnsName, [msg, lnsName, reply](auto maybe) mutable {
            if (maybe.has_value())
            {
              var::visit([&](auto&& value) { msg.AddMXReply(value.ToString(), 1); }, *maybe);
            }
            else
            {
              msg.AddNXReply();
            }
            reply(msg);
          });
          return true;
        }
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
          LookupNameAsync(
              lnsName,
              [msg = std::make_shared<dns::Message>(msg),
               name = Name(),
               lnsName,
               isV6,
               reply,
               ReplyToDNSWhenReady](auto maybe) {
                if (not maybe.has_value())
                {
                  LogWarn(name, " lns name ", lnsName, " not resolved");
                  msg->AddNXReply();
                  reply(*msg);
                  return;
                }
                ReplyToDNSWhenReady(*maybe, msg, isV6);
              });
          return true;
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

        if (auto maybe = ObtainAddrForIP(ip))
        {
          var::visit([&msg](auto&& result) { msg.AddAReply(result.ToString()); }, *maybe);
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
      MarkAddressOutbound(addr);
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

      if (m_BaseV6Address)
      {
        IPRange v6range = m_OurRange;
        v6range.addr = (*m_BaseV6Address) | m_OurRange.addr;
        LogInfo(Name(), " using v6 range: ", v6range);
        info.addrs.emplace(v6range, AF_INET6);
      }

      info.ifname = m_IfName;
      info.dnsaddr.FromString(m_LocalResolverAddr.toHost());

      LogInfo(Name(), " setting up network...");

      try
      {
        m_NetIf = Router()->GetVPNPlatform()->ObtainInterface(std::move(info), Router());
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

      if (not Router()->loop()->add_network_interface(m_NetIf, [this](net::IPPacket pkt) {
            m_PacketRouter->HandleIPPacket(std::move(pkt));
          }))
      {
        LogError(Name(), " failed to add network interface");
        return false;
      }
#ifdef __APPLE__
      m_OurIPv6 = llarp::huint128_t{
          llarp::uint128_t{0xfd2e'6c6f'6b69'0000, llarp::net::TruncateV6(m_OurRange.addr).h}};
#else
      const auto maybe = m_router->Net().GetInterfaceIPv6Address(m_IfName);
      if (maybe.has_value())
      {
        m_OurIPv6 = *maybe;
        LogInfo(Name(), " has ipv6 address ", m_OurIPv6);
      }
#endif

      // Attempt to register DNS on the interface
      systemd_resolved_set_dns(
          m_IfName,
          m_LocalResolverAddr.createSockAddr(),
          false /* just .loki/.snode DNS initially */);

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
        strictConnect += addr.ToString() + " ";
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
      if (!m_Resolver->Start(
              m_LocalResolverAddr.createSockAddr(), m_UpstreamResolvers, m_hostfiles))
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
      // save address map if applicable
#ifndef ANDROID
      if (m_PersistAddrMapFile)
      {
        const auto& file = *m_PersistAddrMapFile;
        LogInfo(Name(), " saving address map to ", file);
        if (auto maybe = util::OpenFileStream<fs::ofstream>(file, std::ios_base::binary))
        {
          std::map<std::string, std::string> addrmap;
          for (const auto& [ip, addr] : m_IPToAddr)
          {
            if (not m_SNodes.at(addr))
            {
              const service::Address a{addr.as_array()};
              if (HasInboundConvo(a))
                addrmap[ip.ToString()] = a.ToString();
            }
          }
          const auto data = oxenc::bt_serialize(addrmap);
          maybe->write(data.data(), data.size());
        }
      }
#endif
      if (m_Resolver)
        m_Resolver->Stop();
      return llarp::service::Endpoint::Stop();
    }

    std::optional<service::Address>
    TunEndpoint::ObtainExitAddressFor(
        huint128_t ip,
        std::function<service::Address(std::unordered_set<service::Address>)> exitSelectionStrat)
    {
      // is it already mapped? return the mapping
      if (auto itr = m_ExitIPToExitAddress.find(ip); itr != m_ExitIPToExitAddress.end())
        return itr->second;
      // build up our candidates to choose
      std::unordered_set<service::Address> candidates;
      for (const auto& entry : m_ExitMap.FindAllEntries(ip))
      {
        // make sure it is allowed by the range if the ip is a bogon
        if (not IsBogon(ip) or entry.first.BogonContains(ip))
          candidates.emplace(entry.second);
      }
      // no candidates? bail.
      if (candidates.empty())
        return std::nullopt;
      if (not exitSelectionStrat)
      {
        // default strat to random choice
        exitSelectionStrat = [](auto candidates) {
          auto itr = candidates.begin();
          std::advance(itr, llarp::randint() % candidates.size());
          return *itr;
        };
      }
      // map the exit and return the endpoint we mapped it to
      return m_ExitIPToExitAddress.emplace(ip, exitSelectionStrat(candidates)).first->second;
    }

    void
    TunEndpoint::HandleGotUserPacket(net::IPPacket pkt)
    {
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

      if (m_state->m_ExitEnabled)
      {
        dst = net::ExpandV4(net::TruncateV6(dst));
      }
      auto itr = m_IPToAddr.find(dst);
      if (itr == m_IPToAddr.end())
      {
        service::Address addr{};

        if (auto maybe = ObtainExitAddressFor(dst))
          addr = *maybe;
        else
        {
          // send icmp unreachable as we dont have any exits for this ip
          if (const auto icmp = pkt.MakeICMPUnreachable())
            HandleWriteIPPacket(icmp->ConstBuffer(), dst, src, 0);

          return;
        }
        pkt.ZeroSourceAddress();
        MarkAddressOutbound(addr);
        EnsurePathToService(
            addr,
            [pkt, this](service::Address addr, service::OutboundContext* ctx) {
              if (ctx)
              {
                ctx->SendPacketToRemote(pkt.ConstBuffer(), service::ProtocolType::Exit);
                Router()->TriggerPump();
                return;
              }
              LogWarn("cannot ensure path to exit ", addr, " so we drop some packets");
            },
            PathAlignmentTimeout());
        return;
      }
      std::variant<service::Address, RouterID> to;
      service::ProtocolType type;
      if (m_SNodes.at(itr->second))
      {
        to = RouterID{itr->second.as_array()};
        type = service::ProtocolType::TrafficV4;
      }
      else
      {
        to = service::Address{itr->second.as_array()};
        type = m_state->m_ExitEnabled and src != m_OurIP ? service::ProtocolType::Exit
                                                         : pkt.ServiceProtocol();
      }

      // prepare packet for insertion into network
      // this includes clearing IP addresses, recalculating checksums, etc
      // this does not happen for exits because the point is they don't rewrite addresses
      if (type != service::ProtocolType::Exit)
      {
        if (pkt.IsV4())
          pkt.UpdateIPv4Address({0}, {0});
        else
          pkt.UpdateIPv6Address({0}, {0});
      }
      // try sending it on an existing convotag
      // this succeds for inbound convos, probably.
      if (auto maybe = GetBestConvoTagFor(to))
      {
        if (SendToOrQueue(*maybe, pkt.ConstBuffer(), type))
        {
          MarkIPActive(dst);
          Router()->TriggerPump();
          return;
        }
      }
      // try establishing a path to this guy
      // will fail if it's an inbound convo
      EnsurePathTo(
          to,
          [pkt, type, dst, to, this](auto maybe) {
            if (not maybe)
            {
              var::visit(
                  [this](auto&& addr) {
                    LogWarn(Name(), " failed to ensure path to ", addr, " no convo tag found");
                  },
                  to);
            }
            if (SendToOrQueue(*maybe, pkt.ConstBuffer(), type))
            {
              MarkIPActive(dst);
              Router()->TriggerPump();
            }
            else
            {
              var::visit(
                  [this](auto&& addr) {
                    LogWarn(Name(), " failed to send to ", addr, ", SendToOrQueue failed");
                  },
                  to);
            }
          },
          PathAlignmentTimeout());
    }

    bool
    TunEndpoint::ShouldAllowTraffic(const net::IPPacket& pkt) const
    {
      if (const auto exitPolicy = GetExitPolicy())
      {
        if (not exitPolicy->AllowsTraffic(pkt))
          return false;
      }

      return true;
    }

    bool
    TunEndpoint::HandleInboundPacket(
        const service::ConvoTag tag,
        const llarp_buffer_t& buf,
        service::ProtocolType t,
        uint64_t seqno)
    {
      LogTrace("Inbound ", t, " packet (", buf.sz, "B) on convo ", tag);
      if (t == service::ProtocolType::QUIC)
      {
        auto* quic = GetQUICTunnel();
        if (!quic)
        {
          LogWarn("incoming quic packet but this endpoint is not quic capable; dropping");
          return false;
        }
        if (buf.sz < 4)
        {
          LogWarn("invalid incoming quic packet, dropping");
          return false;
        }
        LogInfo("tag active T=", tag);
        quic->receive_packet(tag, buf);
        return true;
      }

      if (t != service::ProtocolType::TrafficV4 && t != service::ProtocolType::TrafficV6
          && t != service::ProtocolType::Exit)
        return false;
      std::variant<service::Address, RouterID> addr;
      if (auto maybe = GetEndpointWithConvoTag(tag))
      {
        addr = *maybe;
      }
      else
        return false;
      huint128_t src, dst;

      net::IPPacket pkt;
      if (not pkt.Load(buf))
        return false;

      if (m_state->m_ExitEnabled)
      {
        // exit side from exit

        // check packet against exit policy and if as needed
        if (not ShouldAllowTraffic(pkt))
          return false;

        src = ObtainIPForAddr(addr);
        if (t == service::ProtocolType::Exit)
        {
          if (pkt.IsV4())
            dst = pkt.dst4to6();
          else if (pkt.IsV6())
          {
            dst = pkt.dstv6();
            src = net::ExpandV4Lan(net::TruncateV6(src));
          }
        }
        else
        {
          // non exit traffic on exit
          dst = m_OurIP;
        }
      }
      else if (t == service::ProtocolType::Exit)
      {
        // client side exit traffic from exit
        if (pkt.IsV4())
        {
          dst = m_OurIP;
          src = pkt.src4to6();
        }
        else if (pkt.IsV6())
        {
          dst = m_OurIPv6;
          src = pkt.srcv6();
        }
        // find what exit we think this should be for
        service::Address fromAddr{};
        if (const auto* ptr = std::get_if<service::Address>(&addr))
        {
          fromAddr = *ptr;
        }
        else  // don't allow snode
          return false;
        // make sure the mapping matches
        if (auto itr = m_ExitIPToExitAddress.find(src); itr != m_ExitIPToExitAddress.end())
        {
          if (itr->second != fromAddr)
            return false;
        }
        else
          return false;
      }
      else
      {
        // snapp traffic
        src = ObtainIPForAddr(addr);
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
      {
        return false;
      }
      if (pkt.IsV4())
      {
        pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(src)), xhtonl(net::TruncateV6(dst)));
      }
      else if (pkt.IsV6())
      {
        pkt.UpdateIPv6Address(src, dst);
      }
      m_NetworkToUserPktQueue.push(std::move(write));
      // wake up so we ensure that all packets are written to user
      Router()->TriggerPump();
      return true;
    }

    huint128_t
    TunEndpoint::GetIfAddr() const
    {
      return m_OurIP;
    }

    huint128_t
    TunEndpoint::ObtainIPForAddr(std::variant<service::Address, RouterID> addr)
    {
      llarp_time_t now = Now();
      huint128_t nextIP = {0};
      AlignedBuffer<32> ident{};
      bool snode = false;

      var::visit([&ident](auto&& val) { ident = val.data(); }, addr);

      if (std::get_if<RouterID>(&addr))
      {
        snode = true;
      }

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
          var::visit(
              [&](auto&& remote) { llarp::LogInfo(Name(), " mapped ", remote, " to ", nextIP); },
              addr);
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

    TunEndpoint::~TunEndpoint() = default;

  }  // namespace handlers
}  // namespace llarp

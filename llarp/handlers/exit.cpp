#include "exit.hpp"

#include <llarp/dns/dns.hpp>
#include <llarp/net/net.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/bits.hpp>

#include <llarp/quic/tunnel.hpp>
#include <llarp/router/i_rc_lookup_handler.hpp>

#include <cassert>
#include "service/protocol_type.hpp"

namespace llarp
{
  namespace handlers
  {
    ExitEndpoint::ExitEndpoint(std::string name, AbstractRouter* r)
        : m_Router(r), m_Name(std::move(name)), m_QUIC{std::make_shared<quic::TunnelManager>(*this)}
    {
      m_ShouldInitTun = true;
      m_QUIC = std::make_shared<quic::TunnelManager>(*this);
    }

    ExitEndpoint::~ExitEndpoint() = default;

    void
    ExitEndpoint::LookupNameAsync(
        std::string, std::function<void(std::optional<AddressVariant_t>)> resultHandler)
    {
      // TODO: implement me
      resultHandler(std::nullopt);
    }

    void
    ExitEndpoint::LookupServiceAsync(
        std::string, std::string, std::function<void(std::vector<dns::SRVData>)> resultHandler)
    {
      // TODO: implement me
      resultHandler({});
    }

    std::optional<EndpointBase::AddressVariant_t>
    ExitEndpoint::GetEndpointWithConvoTag(service::ConvoTag tag) const
    {
      for (const auto& [pathID, pk] : m_Paths)
      {
        if (pathID.as_array() == tag.as_array())
          return RouterID{pk.as_array()};
      }
      for (const auto& [rid, session] : m_SNodeSessions)
      {
        PathID_t pathID{tag.as_array()};
        if (session->GetPathByID(pathID))
          return rid;
      }
      return std::nullopt;
    }

    std::optional<service::ConvoTag>
    ExitEndpoint::GetBestConvoTagFor(AddressVariant_t addr) const
    {
      if (auto* rid = std::get_if<RouterID>(&addr))
      {
        service::ConvoTag tag{};
        auto visit = [&tag](exit::Endpoint* const ep) -> bool {
          if (not ep)
            return false;
          if (auto path = ep->GetCurrentPath())
            tag = service::ConvoTag{path->RXID().as_array()};
          return true;
        };
        if (VisitEndpointsFor(PubKey{*rid}, visit) and not tag.IsZero())
          return tag;
        auto itr = m_SNodeSessions.find(*rid);
        if (itr == m_SNodeSessions.end())
        {
          return std::nullopt;
        }
        if (auto path = itr->second->GetPathByRouter(*rid))
        {
          tag = service::ConvoTag{path->RXID().as_array()};
          return tag;
        }
        return std::nullopt;
      }
      return std::nullopt;
    }

    const EventLoop_ptr&
    ExitEndpoint::Loop()
    {
      return m_Router->loop();
    }

    bool
    ExitEndpoint::SendToOrQueue(
        service::ConvoTag tag, const llarp_buffer_t& payload, service::ProtocolType type)
    {
      if (auto maybeAddr = GetEndpointWithConvoTag(tag))
      {
        if (std::holds_alternative<service::Address>(*maybeAddr))
          return false;
        if (auto* rid = std::get_if<RouterID>(&*maybeAddr))
        {
          for (auto [itr, end] = m_ActiveExits.equal_range(PubKey{*rid}); itr != end; ++itr)
          {
            if (not itr->second->LooksDead(Now()))
            {
              if (itr->second->QueueInboundTraffic(payload.copy(), type))
                return true;
            }
          }

          if (not m_Router->PathToRouterAllowed(*rid))
            return false;

          ObtainSNodeSession(*rid, [pkt = payload.copy(), type](auto session) mutable {
            if (session and session->IsReady())
            {
              session->SendPacketToRemote(std::move(pkt), type);
            }
          });
        }
        return true;
      }
      return false;
    }

    bool
    ExitEndpoint::EnsurePathTo(
        AddressVariant_t addr,
        std::function<void(std::optional<service::ConvoTag>)> hook,
        llarp_time_t)
    {
      if (std::holds_alternative<service::Address>(addr))
        return false;
      if (auto* rid = std::get_if<RouterID>(&addr))
      {
        if (m_SNodeKeys.count(PubKey{*rid}) or m_Router->PathToRouterAllowed(*rid))
        {
          ObtainSNodeSession(
              *rid, [hook, routerID = *rid](std::shared_ptr<exit::BaseSession> session) {
                if (session and session->IsReady())
                {
                  if (auto path = session->GetPathByRouter(routerID))
                  {
                    hook(service::ConvoTag{path->RXID().as_array()});
                  }
                  else
                    hook(std::nullopt);
                }
                else
                  hook(std::nullopt);
              });
        }
        else
        {
          // probably a client
          hook(GetBestConvoTagFor(addr));
        }
      }
      return true;
    }

    util::StatusObject
    ExitEndpoint::ExtractStatus() const
    {
      util::StatusObject obj{{"permitExit", m_PermitExit}, {"ip", m_IfAddr.ToString()}};
      util::StatusObject exitsObj{};
      for (const auto& item : m_ActiveExits)
      {
        exitsObj[item.first.ToString()] = item.second->ExtractStatus();
      }
      obj["exits"] = exitsObj;
      return obj;
    }

    bool
    ExitEndpoint::SupportsV6() const
    {
      return m_UseV6;
    }

    bool
    ExitEndpoint::ShouldHookDNSMessage(const dns::Message& msg) const
    {
      if (msg.questions.size() == 0)
        return false;
      // always hook ptr for ranges we own
      if (msg.questions[0].qtype == dns::qTypePTR)
      {
        if (auto ip = dns::DecodePTR(msg.questions[0].qname))
          return m_OurRange.Contains(*ip);
        return false;
      }
      if (msg.questions[0].qtype == dns::qTypeA || msg.questions[0].qtype == dns::qTypeCNAME
          || msg.questions[0].qtype == dns::qTypeAAAA)
      {
        if (msg.questions[0].IsName("localhost.loki"))
          return true;
        if (msg.questions[0].HasTLD(".snode"))
          return true;
      }
      return false;
    }

    bool
    ExitEndpoint::MaybeHookDNS(
        std::shared_ptr<dns::PacketSource_Base> source,
        const dns::Message& query,
        const SockAddr& to,
        const SockAddr& from)
    {
      if (not ShouldHookDNSMessage(query))
        return false;

      auto job = std::make_shared<dns::QueryJob>(source, query, to, from);
      if (not HandleHookedDNSMessage(query, [job](auto msg) { job->SendReply(msg.ToBuffer()); }))
        job->Cancel();
      return true;
    }

    bool
    ExitEndpoint::HandleHookedDNSMessage(dns::Message msg, std::function<void(dns::Message)> reply)
    {
      if (msg.questions[0].qtype == dns::qTypePTR)
      {
        auto ip = dns::DecodePTR(msg.questions[0].qname);
        if (not ip)
          return false;
        if (ip == m_IfAddr)
        {
          RouterID us = GetRouter()->pubkey();
          msg.AddAReply(us.ToString(), 300);
        }
        else
        {
          auto itr = m_IPToKey.find(*ip);
          if (itr != m_IPToKey.end() && m_SNodeKeys.find(itr->second) != m_SNodeKeys.end())
          {
            RouterID them = itr->second;
            msg.AddAReply(them.ToString());
          }
          else
            msg.AddNXReply();
        }
      }
      else if (msg.questions[0].qtype == dns::qTypeCNAME)
      {
        if (msg.questions[0].IsName("random.snode"))
        {
          RouterID random;
          if (GetRouter()->GetRandomGoodRouter(random))
            msg.AddCNAMEReply(random.ToString(), 1);
          else
            msg.AddNXReply();
        }
        else if (msg.questions[0].IsName("localhost.loki"))
        {
          RouterID us = m_Router->pubkey();
          msg.AddAReply(us.ToString(), 1);
        }
        else
          msg.AddNXReply();
      }
      else if (msg.questions[0].qtype == dns::qTypeA || msg.questions[0].qtype == dns::qTypeAAAA)
      {
        const bool isV6 = msg.questions[0].qtype == dns::qTypeAAAA;
        const bool isV4 = msg.questions[0].qtype == dns::qTypeA;
        if (msg.questions[0].IsName("random.snode"))
        {
          RouterID random;
          if (GetRouter()->GetRandomGoodRouter(random))
          {
            msg.AddCNAMEReply(random.ToString(), 1);
            auto ip = ObtainServiceNodeIP(random);
            msg.AddINReply(ip, false);
          }
          else
            msg.AddNXReply();
          reply(msg);
          return true;
        }
        if (msg.questions[0].IsName("localhost.loki"))
        {
          msg.AddINReply(GetIfAddr(), isV6);
          reply(msg);
          return true;
        }
        // forward dns for snode
        RouterID r;
        if (r.FromString(msg.questions[0].Name()))
        {
          huint128_t ip;
          PubKey pubKey(r);
          if (isV4 && SupportsV6())
          {
            msg.hdr_fields |= dns::flags_QR | dns::flags_AA | dns::flags_RA;
          }
          else if (m_SNodeKeys.find(pubKey) == m_SNodeKeys.end())
          {
            // we do not have it mapped, async obtain it
            ObtainSNodeSession(
                r,
                [&, msg = std::make_shared<dns::Message>(msg), reply](
                    std::shared_ptr<exit::BaseSession> session) {
                  if (session && session->IsReady())
                  {
                    msg->AddINReply(m_KeyToIP[pubKey], isV6);
                  }
                  else
                  {
                    msg->AddNXReply();
                  }
                  reply(*msg);
                });
            return true;
          }
          else
          {
            // we have it mapped already as a service node
            auto itr = m_KeyToIP.find(pubKey);
            if (itr != m_KeyToIP.end())
            {
              ip = itr->second;
              msg.AddINReply(ip, isV6);
            }
            else  // fallback case that should never happen (probably)
              msg.AddNXReply();
          }
        }
        else
          msg.AddNXReply();
      }
      reply(msg);
      return true;
    }

    void
    ExitEndpoint::ObtainSNodeSession(const RouterID& router, exit::SessionReadyFunc obtainCb)
    {
      if (not m_Router->rcLookupHandler().SessionIsAllowed(router))
      {
        obtainCb(nullptr);
        return;
      }
      ObtainServiceNodeIP(router);
      m_SNodeSessions[router]->AddReadyHook(obtainCb);
    }

    llarp_time_t
    ExitEndpoint::Now() const
    {
      return m_Router->Now();
    }

    bool
    ExitEndpoint::VisitEndpointsFor(
        const PubKey& pk, std::function<bool(exit::Endpoint* const)> visit) const
    {
      for (auto [itr, end] = m_ActiveExits.equal_range(pk); itr != end; ++itr)
      {
        if (not visit(itr->second.get()))
          return true;
      }
      return false;
    }

    void
    ExitEndpoint::Flush()
    {
      while (not m_InetToNetwork.empty())
      {
        auto& top = m_InetToNetwork.top();

        // get a session by public key
        std::optional<PubKey> maybe_pk;
        {
          auto itr = m_IPToKey.find(top.dstv6());
          if (itr != m_IPToKey.end())
            maybe_pk = itr->second;
        }

        auto buf = const_cast<net::IPPacket&>(top).steal();
        m_InetToNetwork.pop();
        // we have no session for public key so drop
        if (not maybe_pk)
          continue;  // we are in a while loop

        const auto& pk = *maybe_pk;

        // check if this key is a service node
        if (m_SNodeKeys.count(pk))
        {
          // check if it's a service node session we made and queue it via our
          // snode session that we made otherwise use an inbound session that
          // was made by the other service node
          auto itr = m_SNodeSessions.find(pk);
          if (itr != m_SNodeSessions.end())
          {
            itr->second->SendPacketToRemote(std::move(buf), service::ProtocolType::TrafficV4);
            // we are in a while loop
            continue;
          }
        }
        auto tryFlushingTraffic =
            [this, buf = std::move(buf), pk](exit::Endpoint* const ep) -> bool {
          if (!ep->QueueInboundTraffic(buf, service::ProtocolType::TrafficV4))
          {
            LogWarn(
                Name(),
                " dropped inbound traffic for session ",
                pk,
                " as we are overloaded (probably)");
            // continue iteration
            return true;
          }
          // break iteration
          return false;
        };
        if (!VisitEndpointsFor(pk, tryFlushingTraffic))
        {
          // we may have all dead sessions, wtf now?
          LogWarn(
              Name(),
              " dropped inbound traffic for session ",
              pk,
              " as we have no working endpoints");
        }
      }

      for (auto& [pubkey, endpoint] : m_ActiveExits)
      {
        if (!endpoint->Flush())
        {
          LogWarn("exit session with ", pubkey, " dropped packets");
        }
      }
      for (auto& [id, session] : m_SNodeSessions)
      {
        session->FlushUpstream();
        session->FlushDownstream();
      }
    }

    bool
    ExitEndpoint::Start()
    {
      // map our address
      const PubKey us(m_Router->pubkey());
      const huint128_t ip = GetIfAddr();
      m_KeyToIP[us] = ip;
      m_IPToKey[ip] = us;
      m_IPActivity[ip] = std::numeric_limits<llarp_time_t>::max();
      m_SNodeKeys.insert(us);

      if (m_ShouldInitTun)
      {
        vpn::InterfaceInfo info;
        info.ifname = m_ifname;
        info.addrs.emplace_back(m_OurRange);

        m_NetIf = GetRouter()->GetVPNPlatform()->CreateInterface(std::move(info), m_Router);
        if (not m_NetIf)
        {
          llarp::LogError("Could not create interface");
          return false;
        }
        if (not GetRouter()->loop()->add_network_interface(
                m_NetIf, [this](net::IPPacket pkt) { OnInetPacket(std::move(pkt)); }))
        {
          llarp::LogWarn("Could not create tunnel for exit endpoint");
          return false;
        }

        GetRouter()->loop()->add_ticker([this] { Flush(); });
#ifndef _WIN32
        m_Resolver = std::make_shared<dns::Server>(
            m_Router->loop(), m_DNSConf, if_nametoindex(m_ifname.c_str()));
        m_Resolver->AddResolver(weak_from_this());
        m_Resolver->Start();
#endif
      }
      return true;
    }

    AbstractRouter*
    ExitEndpoint::GetRouter()
    {
      return m_Router;
    }

    huint128_t
    ExitEndpoint::GetIfAddr() const
    {
      return m_IfAddr;
    }

    bool
    ExitEndpoint::Stop()
    {
      for (auto& item : m_SNodeSessions)
        item.second->Stop();
      return true;
    }

    bool
    ExitEndpoint::ShouldRemove() const
    {
      for (auto& item : m_SNodeSessions)
        if (!item.second->ShouldRemove())
          return false;
      return true;
    }

    bool
    ExitEndpoint::HasLocalMappedAddrFor(const PubKey& pk) const
    {
      return m_KeyToIP.find(pk) != m_KeyToIP.end();
    }

    huint128_t
    ExitEndpoint::GetIPForIdent(const PubKey pk)
    {
      huint128_t found{};
      if (!HasLocalMappedAddrFor(pk))
      {
        // allocate and map
        found = AllocateNewAddress();
        if (!m_KeyToIP.emplace(pk, found).second)
        {
          LogError(Name(), "failed to map ", pk, " to ", found);
          return found;
        }
        if (!m_IPToKey.emplace(found, pk).second)
        {
          LogError(Name(), "failed to map ", found, " to ", pk);
          return found;
        }
        if (HasLocalMappedAddrFor(pk))
          LogInfo(Name(), " mapping ", pk, " to ", found);
        else
          LogError(Name(), "failed to map ", pk, " to ", found);
      }
      else
        found = m_KeyToIP[pk];

      MarkIPActive(found);
      m_KeyToIP.rehash(0);
      assert(HasLocalMappedAddrFor(pk));
      return found;
    }

    huint128_t
    ExitEndpoint::AllocateNewAddress()
    {
      if (m_NextAddr < m_HigestAddr)
        return ++m_NextAddr;

      // find oldest activity ip address
      huint128_t found = {0};
      llarp_time_t min = std::numeric_limits<llarp_time_t>::max();
      for (const auto& [addr, time] : m_IPActivity)
      {
        if (time < min)
        {
          found.h = addr.h;
          min = time;
        }
      }
      // kick old ident off exit
      // TODO: DoS
      PubKey pk = m_IPToKey[found];
      KickIdentOffExit(pk);

      return found;
    }

    EndpointBase::AddressVariant_t
    ExitEndpoint::LocalAddress() const
    {
      return RouterID{m_Router->pubkey()};
    }

    void
    ExitEndpoint::SRVRecordsChanged()
    {
      m_Router->ModifyOurRC(
          [srvRecords = SRVRecords()](RouterContact rc) -> std::optional<RouterContact> {
            // check if there are any new srv records
            bool shouldUpdate = false;

            for (const auto& rcSrv : rc.srvRecords)
            {
              if (srvRecords.count(rcSrv) == 0)
                shouldUpdate = true;
            }

            // no new records so don't modify
            if (not shouldUpdate)
              return std::nullopt;

            // we got new entries so we clear the whole vector on the rc and recreate it
            rc.srvRecords.clear();
            for (auto& record : srvRecords)
              rc.srvRecords.emplace_back(record);
            // set the verssion to 1 because we have srv records
            rc.version = 1;
            return rc;
          });
    }

    std::optional<EndpointBase::SendStat> ExitEndpoint::GetStatFor(AddressVariant_t) const
    {
      /// TODO: implement me
      return std::nullopt;
    }

    std::unordered_set<EndpointBase::AddressVariant_t>
    ExitEndpoint::AllRemoteEndpoints() const
    {
      std::unordered_set<AddressVariant_t> remote;
      for (const auto& [path, pubkey] : m_Paths)
      {
        remote.insert(RouterID{pubkey});
      }
      return remote;
    }

    bool
    ExitEndpoint::QueueOutboundTraffic(net::IPPacket pkt)
    {
      return m_NetIf && m_NetIf->WritePacket(std::move(pkt));
    }

    void
    ExitEndpoint::KickIdentOffExit(const PubKey& pk)
    {
      LogInfo(Name(), " kicking ", pk, " off exit");
      huint128_t ip = m_KeyToIP[pk];
      m_KeyToIP.erase(pk);
      m_IPToKey.erase(ip);
      for (auto [exit_itr, end] = m_ActiveExits.equal_range(pk); exit_itr != end;)
        exit_itr = m_ActiveExits.erase(exit_itr);
    }

    void
    ExitEndpoint::MarkIPActive(huint128_t ip)
    {
      m_IPActivity[ip] = GetRouter()->Now();
    }

    void
    ExitEndpoint::OnInetPacket(net::IPPacket pkt)
    {
      m_InetToNetwork.emplace(std::move(pkt));
    }

    bool
    ExitEndpoint::QueueSNodePacket(const llarp_buffer_t& buf, huint128_t from)
    {
      net::IPPacket pkt{buf.view_all()};
      if (pkt.empty())
        return false;
      // rewrite ip
      if (m_UseV6)
        pkt.UpdateIPv6Address(from, m_IfAddr);
      else
        pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(from)), xhtonl(net::TruncateV6(m_IfAddr)));
      return m_NetIf and m_NetIf->WritePacket(std::move(pkt));
    }

    exit::Endpoint*
    ExitEndpoint::FindEndpointByPath(const PathID_t& path)
    {
      exit::Endpoint* endpoint = nullptr;
      PubKey pk;
      if (auto itr = m_Paths.find(path); itr != m_Paths.end())
        pk = itr->second;
      else
        return nullptr;
      if (auto itr = m_ActiveExits.find(pk); itr != m_ActiveExits.end())
      {
        if (itr->second->PubKey() == pk)
          endpoint = itr->second.get();
      }
      return endpoint;
    }

    bool
    ExitEndpoint::UpdateEndpointPath(const PubKey& remote, const PathID_t& next)
    {
      // check if already mapped
      if (auto itr = m_Paths.find(next); itr != m_Paths.end())
        return false;
      m_Paths.emplace(next, remote);
      return true;
    }

    void
    ExitEndpoint::Configure(const NetworkConfig& networkConfig, const DnsConfig& dnsConfig)
    {
      /*
       * TODO: pre-config refactor, this was checking a couple things that were extremely vague
       *       these could have appeared on either [dns] or [network], but they weren't documented
       *       anywhere
       *
      if (k == "type" && v == "null")
      {
        m_ShouldInitTun = false;
        return true;
      }
      if (k == "exit")
      {
        m_PermitExit = IsTrueValue(v.c_str());
        return true;
      }
       */

      m_DNSConf = dnsConfig;

      if (networkConfig.m_endpointType == "null")
      {
        m_ShouldInitTun = false;
      }

      m_OurRange = networkConfig.m_ifaddr;
      if (!m_OurRange.addr.h)
      {
        const auto maybe = m_Router->Net().FindFreeRange();
        if (not maybe.has_value())
          throw std::runtime_error("cannot find free interface range");
        m_OurRange = *maybe;
      }
      const auto host_str = m_OurRange.BaseAddressString();
      // string, or just a plain char array?
      m_IfAddr = m_OurRange.addr;
      m_NextAddr = m_IfAddr;
      m_HigestAddr = m_OurRange.HighestAddr();
      m_UseV6 = not m_OurRange.IsV4();

      m_ifname = networkConfig.m_ifname;
      if (m_ifname.empty())
      {
        const auto maybe = m_Router->Net().FindFreeTun();
        if (not maybe.has_value())
          throw std::runtime_error("cannot find free interface name");
        m_ifname = *maybe;
      }
      LogInfo(Name(), " set ifname to ", m_ifname);
      if (auto* quic = GetQUICTunnel())
      {
        quic->listen([ifaddr = net::TruncateV6(m_IfAddr)](std::string_view, uint16_t port) {
          return llarp::SockAddr{ifaddr, huint16_t{port}};
        });
      }
    }

    huint128_t
    ExitEndpoint::ObtainServiceNodeIP(const RouterID& other)
    {
      const PubKey pubKey{other};
      const PubKey us{m_Router->pubkey()};
      // just in case
      if (pubKey == us)
        return m_IfAddr;

      huint128_t ip = GetIPForIdent(pubKey);
      if (m_SNodeKeys.emplace(pubKey).second)
      {
        auto session = std::make_shared<exit::SNodeSession>(
            other,
            [this, ip](const auto& buf) { return QueueSNodePacket(buf, ip); },
            GetRouter(),
            2,
            1,
            true,
            this);
        // this is a new service node make an outbound session to them
        m_SNodeSessions[other] = session;
      }
      return ip;
    }

    quic::TunnelManager*
    ExitEndpoint::GetQUICTunnel()
    {
      return m_QUIC.get();
    }

    bool
    ExitEndpoint::AllocateNewExit(const PubKey pk, const PathID_t& path, bool wantInternet)
    {
      if (wantInternet && !m_PermitExit)
        return false;
      path::HopHandler_ptr handler =
          m_Router->pathContext().GetByUpstream(m_Router->pubkey(), path);
      if (handler == nullptr)
        return false;
      auto ip = GetIPForIdent(pk);
      if (GetRouter()->pathContext().TransitHopPreviousIsRouter(path, pk.as_array()))
      {
        // we think this path belongs to a service node
        // mark it as such so we don't make an outbound session to them
        m_SNodeKeys.emplace(pk.as_array());
      }
      m_ActiveExits.emplace(
          pk, std::make_unique<exit::Endpoint>(pk, handler, !wantInternet, ip, this));

      m_Paths[path] = pk;

      return HasLocalMappedAddrFor(pk);
    }

    std::string
    ExitEndpoint::Name() const
    {
      return m_Name;
    }

    void
    ExitEndpoint::DelEndpointInfo(const PathID_t& path)
    {
      m_Paths.erase(path);
    }

    void
    ExitEndpoint::RemoveExit(const exit::Endpoint* ep)
    {
      for (auto [itr, end] = m_ActiveExits.equal_range(ep->PubKey()); itr != end; ++itr)
      {
        if (itr->second->GetCurrentPath() == ep->GetCurrentPath())
        {
          m_ActiveExits.erase(itr);
          // now ep is gone af
          return;
        }
      }
    }

    void
    ExitEndpoint::Tick(llarp_time_t now)
    {
      {
        auto itr = m_SNodeSessions.begin();
        while (itr != m_SNodeSessions.end())
        {
          if (itr->second->IsExpired(now))
            itr = m_SNodeSessions.erase(itr);
          else
          {
            itr->second->Tick(now);
            ++itr;
          }
        }
      }
      {
        // expire
        auto itr = m_ActiveExits.begin();
        while (itr != m_ActiveExits.end())
        {
          if (itr->second->IsExpired(now))
            itr = m_ActiveExits.erase(itr);
          else
            ++itr;
        }
        // pick chosen exits and tick
        m_ChosenExits.clear();
        itr = m_ActiveExits.begin();
        while (itr != m_ActiveExits.end())
        {
          // do we have an exit set for this key?
          if (m_ChosenExits.find(itr->first) != m_ChosenExits.end())
          {
            // yes
            if (m_ChosenExits[itr->first]->createdAt < itr->second->createdAt)
            {
              // if the iterators's exit is newer use it for the chosen exit for
              // key
              if (!itr->second->LooksDead(now))
                m_ChosenExits[itr->first] = itr->second.get();
            }
          }
          else if (!itr->second->LooksDead(now))  // set chosen exit if not dead for key that
                                                  // doesn't have one yet
            m_ChosenExits[itr->first] = itr->second.get();
          // tick which clears the tx rx counters
          itr->second->Tick(now);
          ++itr;
        }
      }
    }
  }  // namespace handlers
}  // namespace llarp

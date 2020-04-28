#include <handlers/exit.hpp>

#include <dns/dns.hpp>
#include <net/net.hpp>
#include <path/path_context.hpp>
#include <router/abstractrouter.hpp>
#include <util/str.hpp>
#include <util/bits.hpp>

#include <cassert>

namespace llarp
{
  namespace handlers
  {
    static void
    ExitHandlerRecvPkt(llarp_tun_io* tun, const llarp_buffer_t& buf)
    {
      std::vector<byte_t> pkt;
      pkt.resize(buf.sz);
      std::copy_n(buf.base, buf.sz, pkt.data());
      auto self = static_cast<ExitEndpoint*>(tun->user);
      LogicCall(self->GetRouter()->logic(), [self, pktbuf = std::move(pkt)]() {
        self->OnInetPacket(std::move(pktbuf));
      });
    }

    static void
    ExitHandlerFlush(llarp_tun_io* tun)
    {
      auto* ep = static_cast<ExitEndpoint*>(tun->user);
      LogicCall(ep->GetRouter()->logic(), std::bind(&ExitEndpoint::Flush, ep));
    }

    ExitEndpoint::ExitEndpoint(const std::string& name, AbstractRouter* r)
        : m_Router(r)
        , m_Resolver(std::make_shared<dns::Proxy>(
              r->netloop(), r->logic(), r->netloop(), r->logic(), this))
        , m_Name(name)
        , m_Tun{{0},
                0,
                0,
                {0},
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr}
        , m_LocalResolverAddr("127.0.0.1", 53)
        , m_InetToNetwork(name + "_exit_rx", r->netloop(), r->netloop())

    {
      m_Tun.user = this;
      m_Tun.recvpkt = &ExitHandlerRecvPkt;
      m_Tun.tick = &ExitHandlerFlush;
      m_ShouldInitTun = true;
    }

    ExitEndpoint::~ExitEndpoint() = default;

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
        huint128_t ip;
        if (!dns::DecodePTR(msg.questions[0].qname, ip))
          return false;
        return m_OurRange.Contains(ip);
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
    ExitEndpoint::HandleHookedDNSMessage(dns::Message msg, std::function<void(dns::Message)> reply)
    {
      if (msg.questions[0].qtype == dns::qTypePTR)
      {
        huint128_t ip;
        if (!dns::DecodePTR(msg.questions[0].qname, ip))
          return false;
        if (ip == m_IfAddr)
        {
          RouterID us = GetRouter()->pubkey();
          msg.AddAReply(us.ToString(), 300);
        }
        else
        {
          auto itr = m_IPToKey.find(ip);
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
            ObtainSNodeSession(r, [&](std::shared_ptr<exit::BaseSession> session) {
              if (session && session->IsReady())
              {
                msg.AddINReply(m_KeyToIP[pubKey], isV6);
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
        const PubKey& pk, std::function<bool(exit::Endpoint* const)> visit)
    {
      auto range = m_ActiveExits.equal_range(pk);
      auto itr = range.first;
      while (itr != range.second)
      {
        if (visit(itr->second.get()))
          ++itr;
        else
          return true;
      }
      return false;
    }

    void
    ExitEndpoint::Flush()
    {
      m_InetToNetwork.Process([&](Pkt_t& pkt) {
        PubKey pk;
        {
          auto itr = m_IPToKey.find(pkt.dstv6());
          if (itr == m_IPToKey.end())
          {
            // drop
            LogWarn(Name(), " dropping packet, has no session at ", pkt.dstv6());
            return;
          }
          pk = itr->second;
        }
        // check if this key is a service node
        if (m_SNodeKeys.find(pk) != m_SNodeKeys.end())
        {
          // check if it's a service node session we made and queue it via our
          // snode session that we made otherwise use an inbound session that
          // was made by the other service node
          auto itr = m_SNodeSessions.find(pk);
          if (itr != m_SNodeSessions.end())
          {
            if (itr->second->QueueUpstreamTraffic(pkt, routing::ExitPadSize))
              return;
          }
        }
        auto tryFlushingTraffic = [&](exit::Endpoint* const ep) -> bool {
          if (!ep->QueueInboundTraffic(ManagedBuffer{pkt.Buffer()}))
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
      });
      {
        auto itr = m_ActiveExits.begin();
        while (itr != m_ActiveExits.end())
        {
          if (!itr->second->Flush())
          {
            LogWarn("exit session with ", itr->first, " dropped packets");
          }
          ++itr;
        }
      }
      {
        auto itr = m_SNodeSessions.begin();
        while (itr != m_SNodeSessions.end())
        {
          // TODO: move flush upstream to router event loop
          if (!itr->second->FlushUpstream())
          {
            LogWarn("failed to flush snode traffic to ", itr->first, " via outbound session");
          }
          itr->second->FlushDownstream();
          ++itr;
        }
      }
      m_Router->PumpLL();
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
        auto loop = GetRouter()->netloop();
        if (!llarp_ev_add_tun(loop.get(), &m_Tun))
        {
          llarp::LogWarn("Could not create tunnel for exit endpoint");
          return false;
        }
        llarp::LogInfo("Trying to start resolver ", m_LocalResolverAddr.ToString());
        return m_Resolver->Start(m_LocalResolverAddr, m_UpstreamResolvers);
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
      huint128_t found = {0};
      if (!HasLocalMappedAddrFor(pk))
      {
        // allocate and map
        found.h = AllocateNewAddress().h;
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
        found.h = m_KeyToIP[pk].h;

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
      auto itr = m_IPActivity.begin();
      while (itr != m_IPActivity.end())
      {
        if (itr->second < min)
        {
          found.h = itr->first.h;
          min = itr->second;
        }
        ++itr;
      }
      // kick old ident off exit
      // TODO: DoS
      PubKey pk = m_IPToKey[found];
      KickIdentOffExit(pk);

      return found;
    }

    bool
    ExitEndpoint::QueueOutboundTraffic(const llarp_buffer_t& buf)
    {
      return llarp_ev_tun_async_write(&m_Tun, buf);
    }

    void
    ExitEndpoint::KickIdentOffExit(const PubKey& pk)
    {
      LogInfo(Name(), " kicking ", pk, " off exit");
      huint128_t ip = m_KeyToIP[pk];
      m_KeyToIP.erase(pk);
      m_IPToKey.erase(ip);
      auto range = m_ActiveExits.equal_range(pk);
      auto exit_itr = range.first;
      while (exit_itr != range.second)
        exit_itr = m_ActiveExits.erase(exit_itr);
    }

    void
    ExitEndpoint::MarkIPActive(huint128_t ip)
    {
      m_IPActivity[ip] = GetRouter()->Now();
    }

    void
    ExitEndpoint::OnInetPacket(std::vector<byte_t> buf)
    {
      const llarp_buffer_t buffer(buf);
      m_InetToNetwork.EmplaceIf(
          [b = ManagedBuffer(buffer)](Pkt_t& pkt) -> bool { return pkt.Load(b); });
    }

    bool
    ExitEndpoint::QueueSNodePacket(const llarp_buffer_t& buf, huint128_t from)
    {
      net::IPPacket pkt;
      if (!pkt.Load(buf))
        return false;
      // rewrite ip
      if (m_UseV6)
        pkt.UpdateIPv6Address(from, m_IfAddr);
      else
        pkt.UpdateIPv4Address(
            xhtonl(net::IPPacket::TruncateV6(from)), xhtonl(net::IPPacket::TruncateV6(m_IfAddr)));
      return llarp_ev_tun_async_write(&m_Tun, pkt.Buffer());
    }

    exit::Endpoint*
    ExitEndpoint::FindEndpointByPath(const PathID_t& path)
    {
      exit::Endpoint* endpoint = nullptr;
      PubKey pk;
      {
        auto itr = m_Paths.find(path);
        if (itr == m_Paths.end())
          return nullptr;
        pk = itr->second;
      }
      {
        auto itr = m_ActiveExits.find(pk);
        if (itr != m_ActiveExits.end())
        {
          if (itr->second->PubKey() == pk)
            endpoint = itr->second.get();
        }
      }
      return endpoint;
    }

    bool
    ExitEndpoint::UpdateEndpointPath(const PubKey& remote, const PathID_t& next)
    {
      // check if already mapped
      auto itr = m_Paths.find(next);
      if (itr != m_Paths.end())
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

      m_LocalResolverAddr = dnsConfig.m_bind;
      m_UpstreamResolvers = dnsConfig.m_upstreamDNS;

      if (!m_OurRange.FromString(networkConfig.m_ifaddr))
      {
        throw std::invalid_argument(
            stringify(Name(), " has invalid address range: ", networkConfig.m_ifaddr));
      }
      // TODO: clean this up (make a util function for handling CIDR, etc.)
      auto pos = networkConfig.m_ifaddr.find("/");
      if (pos == std::string::npos)
      {
        throw std::invalid_argument(
            stringify(Name(), " ifaddr is not a cidr: ", networkConfig.m_ifaddr));
      }
      std::string nmask_str = networkConfig.m_ifaddr.substr(1 + pos);
      std::string host_str = networkConfig.m_ifaddr.substr(0, pos);
      // string, or just a plain char array?
      strncpy(m_Tun.ifaddr, host_str.c_str(), sizeof(m_Tun.ifaddr) - 1);
      m_Tun.netmask = std::atoi(nmask_str.c_str());
      m_IfAddr = m_OurRange.addr;
      m_NextAddr = m_IfAddr;
      m_HigestAddr = m_OurRange.HighestAddr();
      LogInfo(
          Name(),
          " set ifaddr range to ",
          m_Tun.ifaddr,
          "/",
          m_Tun.netmask,
          " lo=",
          m_IfAddr,
          " hi=",
          m_HigestAddr);
      m_UseV6 = false;

      if (networkConfig.m_ifname.length() >= sizeof(m_Tun.ifname))
      {
        throw std::invalid_argument(
            stringify(Name() + " ifname '", networkConfig.m_ifname, "' is too long"));
      }
      strncpy(m_Tun.ifname, networkConfig.m_ifname.c_str(), sizeof(m_Tun.ifname) - 1);
      LogInfo(Name(), " set ifname to ", m_Tun.ifname);

      // TODO: "exit-whitelist" and "exit-blacklist"
      //       (which weren't originally implemented)
    }

    huint128_t
    ExitEndpoint::ObtainServiceNodeIP(const RouterID& other)
    {
      const PubKey pubKey(other);
      const PubKey us(m_Router->pubkey());
      // just in case
      if (pubKey == us)
        return m_IfAddr;

      huint128_t ip = GetIPForIdent(pubKey);
      if (m_SNodeKeys.emplace(pubKey).second)
      {
        auto session = std::make_shared<exit::SNodeSession>(
            other,
            std::bind(&ExitEndpoint::QueueSNodePacket, this, std::placeholders::_1, ip),
            GetRouter(),
            2,
            1,
            true,
            false);
        // this is a new service node make an outbound session to them
        m_SNodeSessions.emplace(other, session);
      }
      return ip;
    }

    bool
    ExitEndpoint::AllocateNewExit(const PubKey pk, const PathID_t& path, bool wantInternet)
    {
      if (wantInternet && !m_PermitExit)
        return false;
      auto ip = GetIPForIdent(pk);
      if (GetRouter()->pathContext().TransitHopPreviousIsRouter(path, pk.as_array()))
      {
        // we think this path belongs to a service node
        // mark it as such so we don't make an outbound session to them
        m_SNodeKeys.emplace(pk.as_array());
      }
      m_ActiveExits.emplace(
          pk, std::make_unique<exit::Endpoint>(pk, path, !wantInternet, ip, this));

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
      auto range = m_ActiveExits.equal_range(ep->PubKey());
      auto itr = range.first;
      while (itr != range.second)
      {
        if (itr->second->LocalPath() == ep->LocalPath())
        {
          itr = m_ActiveExits.erase(itr);
          // now ep is gone af
          return;
        }
        ++itr;
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

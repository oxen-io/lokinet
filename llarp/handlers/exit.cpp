#include <handlers/exit.hpp>

#include <dns/dns.hpp>
#include <net/net.hpp>
#include <router/router.hpp>
#include <util/str.hpp>

#include <cassert>

namespace llarp
{
  namespace handlers
  {
    static void
    ExitHandlerRecvPkt(llarp_tun_io *tun, const llarp_buffer_t &buf)
    {
      static_cast< ExitEndpoint * >(tun->user)->OnInetPacket(buf);
    }

    static void
    ExitHandlerFlush(llarp_tun_io *tun)
    {
      static_cast< ExitEndpoint * >(tun->user)->Flush();
    }

    ExitEndpoint::ExitEndpoint(const std::string &name, Router *r)
        : m_Router(r)
        , m_Resolver(r->netloop, this)
        , m_Name(name)
        , m_Tun{{0}, 0, {0}, 0, 0, 0, 0, 0, 0, 0}
        , m_LocalResolverAddr("127.0.0.1", 53)
        , m_InetToNetwork(name + "_exit_rx", r->netloop, r->netloop)

    {
      m_Tun.user      = this;
      m_Tun.recvpkt   = &ExitHandlerRecvPkt;
      m_Tun.tick      = &ExitHandlerFlush;
      m_ShouldInitTun = true;
    }

    ExitEndpoint::~ExitEndpoint()
    {
    }

    bool
    ExitEndpoint::ShouldHookDNSMessage(const dns::Message &msg) const
    {
      if(msg.questions.size() == 0)
        return false;
      // always hook ptr for ranges we own
      if(msg.questions[0].qtype == dns::qTypePTR)
      {
        huint32_t ip;
        if(!dns::DecodePTR(msg.questions[0].qname, ip))
          return false;
        return m_OurRange.Contains(ip);
      }
      else if(msg.questions[0].qtype == dns::qTypeA
              || msg.questions[0].qtype == dns::qTypeCNAME)
      {
        // hook for forward dns or cname when using snode tld
        return msg.questions[0].qname.find(".snode.")
            == (msg.questions[0].qname.size() - 7);
      }
      else
        return false;
    }

    bool
    ExitEndpoint::HandleHookedDNSMessage(
        dns::Message &&msg, std::function< void(dns::Message) > reply)
    {
      if(msg.questions[0].qtype == dns::qTypePTR)
      {
        huint32_t ip;
        if(!dns::DecodePTR(msg.questions[0].qname, ip))
          return false;
        if(ip == m_IfAddr)
        {
          RouterID us = GetRouter()->pubkey();
          msg.AddAReply(us.ToString(), 300);
        }
        else
        {
          auto itr = m_IPToKey.find(ip);
          if(itr != m_IPToKey.end()
             && m_SNodeKeys.find(itr->second) != m_SNodeKeys.end())
          {
            RouterID them = itr->second;
            msg.AddAReply(them.ToString());
          }
          else
            msg.AddNXReply();
        }
      }
      else if(msg.questions[0].qtype == dns::qTypeCNAME)
      {
        if(msg.questions[0].qname == "random.snode"
           || msg.questions[0].qname == "random.snode.")
        {
          RouterID random;
          if(GetRouter()->GetRandomGoodRouter(random))
            msg.AddCNAMEReply(random.ToString(), 1);
          else
            msg.AddNXReply();
        }
        else
          msg.AddNXReply();
      }
      else if(msg.questions[0].qtype == dns::qTypeA)
      {
        // forward dns for snode
        RouterID r;
        if(r.FromString(msg.questions[0].qname))
        {
          huint32_t ip;
          PubKey pubKey(r);
          if(m_SNodeKeys.find(pubKey) == m_SNodeKeys.end())
          {
            // we do not have it mapped
            // map it
            ip = ObtainServiceNodeIP(r);
            msg.AddINReply(ip);
          }
          else
          {
            // we have it mapped already as a service node
            auto itr = m_KeyToIP.find(pubKey);
            if(itr != m_KeyToIP.end())
            {
              ip = itr->second;
              msg.AddINReply(ip);
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

    llarp_time_t
    ExitEndpoint::Now() const
    {
      return m_Router->Now();
    }

    void
    ExitEndpoint::Flush()
    {
      m_InetToNetwork.Process([&](Pkt_t &pkt) {
        PubKey pk;
        {
          auto itr = m_IPToKey.find(pkt.dst());
          if(itr == m_IPToKey.end())
          {
            // drop
            LogWarn(Name(), " dropping packet, has no session at ", pkt.dst());
            return;
          }
          pk = itr->second;
        }
        // check if this key is a service node
        if(m_SNodeKeys.find(pk) != m_SNodeKeys.end())
        {
          // check if it's a service node session we made and queue it via our
          // snode session that we made otherwise use an inbound session that
          // was made by the other service node
          auto itr = m_SNodeSessions.find(pk);
          if(itr != m_SNodeSessions.end())
          {
            if(itr->second->QueueUpstreamTraffic(pkt, routing::ExitPadSize))
              return;
          }
        }
        exit::Endpoint *ep = m_ChosenExits[pk];

        if(ep == nullptr)
        {
          // we may have all dead sessions, wtf now?
          LogWarn(Name(), " dropped inbound traffic for session ", pk,
                  " as we have no working endpoints");
        }
        else
        {
          if(!ep->QueueInboundTraffic(ManagedBuffer{pkt.Buffer()}))
          {
            LogWarn(Name(), " dropped inbound traffic for session ", pk,
                    " as we are overloaded (probably)");
          }
        }
      });
      {
        auto itr = m_ActiveExits.begin();
        while(itr != m_ActiveExits.end())
        {
          if(!itr->second->Flush())
          {
            LogWarn("exit session with ", itr->first, " dropped packets");
          }
          ++itr;
        }
      }
      {
        auto itr = m_SNodeSessions.begin();
        while(itr != m_SNodeSessions.end())
        {
          if(!itr->second->Flush())
          {
            LogWarn("failed to flush snode traffic to ", itr->first,
                    " via outbound session");
          }
          ++itr;
        }
      }
    }

    bool
    ExitEndpoint::Start()
    {
      if(m_ShouldInitTun)
      {
        if(!llarp_ev_add_tun(GetRouter()->netloop, &m_Tun))
        {
          llarp::LogWarn("Could not create tunnel for exit endpoint");
          return false;
        }
        if(m_UpstreamResolvers.size() == 0)
          m_UpstreamResolvers.emplace_back("8.8.8.8", 53);
        llarp::LogInfo("Trying to start resolver ",
                       m_LocalResolverAddr.ToString());
        return m_Resolver.Start(m_LocalResolverAddr, m_UpstreamResolvers);
      }
      return true;
    }

    Router *
    ExitEndpoint::GetRouter()
    {
      return m_Router;
    }

    Crypto *
    ExitEndpoint::GetCrypto()
    {
      return m_Router->crypto();
    }

    huint32_t
    ExitEndpoint::GetIfAddr() const
    {
      return m_IfAddr;
    }

    bool
    ExitEndpoint::Stop()
    {
      for(auto &item : m_SNodeSessions)
        item.second->Stop();
      return true;
    }

    bool
    ExitEndpoint::ShouldRemove() const
    {
      for(auto &item : m_SNodeSessions)
        if(!item.second->ShouldRemove())
          return false;
      return true;
    }

    bool
    ExitEndpoint::HasLocalMappedAddrFor(const PubKey &pk) const
    {
      return m_KeyToIP.find(pk) != m_KeyToIP.end();
    }

    huint32_t
    ExitEndpoint::GetIPForIdent(const PubKey pk)
    {
      huint32_t found = {0};
      if(!HasLocalMappedAddrFor(pk))
      {
        // allocate and map
        found.h = AllocateNewAddress().h;
        if(!m_KeyToIP.emplace(pk, found).second)
        {
          LogError(Name(), "failed to map ", pk, " to ", found);
          return found;
        }
        if(!m_IPToKey.emplace(found, pk).second)
        {
          LogError(Name(), "failed to map ", found, " to ", pk);
          return found;
        }
        if(HasLocalMappedAddrFor(pk))
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

    huint32_t
    ExitEndpoint::AllocateNewAddress()
    {
      if(m_NextAddr < m_HigestAddr)
        return ++m_NextAddr;

      // find oldest activity ip address
      huint32_t found  = {0};
      llarp_time_t min = std::numeric_limits< llarp_time_t >::max();
      auto itr         = m_IPActivity.begin();
      while(itr != m_IPActivity.end())
      {
        if(itr->second < min)
        {
          found.h = itr->first.h;
          min     = itr->second;
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
    ExitEndpoint::QueueOutboundTraffic(const llarp_buffer_t &buf)
    {
      return llarp_ev_tun_async_write(&m_Tun, buf);
    }

    void
    ExitEndpoint::KickIdentOffExit(const PubKey &pk)
    {
      LogInfo(Name(), " kicking ", pk, " off exit");
      huint32_t ip = m_KeyToIP[pk];
      m_KeyToIP.erase(pk);
      m_IPToKey.erase(ip);
      auto range    = m_ActiveExits.equal_range(pk);
      auto exit_itr = range.first;
      while(exit_itr != range.second)
        exit_itr = m_ActiveExits.erase(exit_itr);
    }

    void
    ExitEndpoint::MarkIPActive(huint32_t ip)
    {
      m_IPActivity[ip] = GetRouter()->Now();
    }

    void
    ExitEndpoint::OnInetPacket(const llarp_buffer_t &buf)
    {
      m_InetToNetwork.EmplaceIf(
          [b = ManagedBuffer(buf)](Pkt_t &pkt) -> bool { return pkt.Load(b); });
    }

    bool
    ExitEndpoint::QueueSNodePacket(const llarp_buffer_t &buf, huint32_t from)
    {
      net::IPv4Packet pkt;
      if(!pkt.Load(buf))
        return false;
      // rewrite ip
      pkt.UpdateIPv4PacketOnDst(from, m_IfAddr);
      return llarp_ev_tun_async_write(&m_Tun, pkt.Buffer());
    }

    exit::Endpoint *
    ExitEndpoint::FindEndpointByPath(const PathID_t &path)
    {
      exit::Endpoint *endpoint = nullptr;
      PubKey pk;
      {
        auto itr = m_Paths.find(path);
        if(itr == m_Paths.end())
          return nullptr;
        pk = itr->second;
      }
      {
        auto itr = m_ActiveExits.find(pk);
        if(itr != m_ActiveExits.end())
        {
          if(itr->second->PubKey() == pk)
            endpoint = itr->second.get();
        }
      }
      return endpoint;
    }

    bool
    ExitEndpoint::UpdateEndpointPath(const PubKey &remote, const PathID_t &next)
    {
      // check if already mapped
      auto itr = m_Paths.find(next);
      if(itr != m_Paths.end())
        return false;
      m_Paths.insert(std::make_pair(next, remote));
      return true;
    }

    bool
    ExitEndpoint::SetOption(const std::string &k, const std::string &v)
    {
      if(k == "type" && v == "null")
      {
        m_ShouldInitTun = false;
        return true;
      }
      if(k == "exit")
      {
        m_PermitExit = IsTrueValue(v.c_str());
        return true;
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
        m_LocalResolverAddr = Addr(resolverAddr, dnsport);
        LogInfo(Name(), " local dns set to ", m_LocalResolverAddr);
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
        LogInfo(Name(), " adding upstream dns set to ", resolverAddr, ":",
                dnsport);
      }
      if(k == "ifaddr")
      {
        auto pos = v.find("/");
        if(pos == std::string::npos)
        {
          LogError(Name(), " ifaddr is not a cidr: ", v);
          return false;
        }
        std::string nmask_str = v.substr(1 + pos);
        std::string host_str  = v.substr(0, pos);
        // string, or just a plain char array?
        strncpy(m_Tun.ifaddr, host_str.c_str(), sizeof(m_Tun.ifaddr) - 1);
        m_Tun.netmask = std::atoi(nmask_str.c_str());

        Addr ifaddr(host_str);
        m_IfAddr                = ifaddr.xtohl();
        m_OurRange.netmask_bits = netmask_ipv4_bits(m_Tun.netmask);
        m_OurRange.addr         = m_IfAddr;
        m_NextAddr              = m_IfAddr;
        m_HigestAddr            = m_IfAddr | (~m_OurRange.netmask_bits);
        LogInfo(Name(), " set ifaddr range to ", m_Tun.ifaddr, "/",
                m_Tun.netmask, " lo=", m_IfAddr, " hi=", m_HigestAddr);
      }
      if(k == "ifname")
      {
        if(v.length() >= sizeof(m_Tun.ifname))
        {
          LogError(Name() + " ifname '", v, "' is too long");
          return false;
        }
        strncpy(m_Tun.ifname, v.c_str(), sizeof(m_Tun.ifname) - 1);
        LogInfo(Name(), " set ifname to ", m_Tun.ifname);
      }
      if(k == "exit-whitelist")
      {
        // add exit policy whitelist rule
        // TODO: implement me
        return true;
      }
      if(k == "exit-blacklist")
      {
        // add exit policy blacklist rule
        // TODO: implement me
        return true;
      }

      return true;
    }

    huint32_t
    ExitEndpoint::ObtainServiceNodeIP(const RouterID &other)
    {
      PubKey pubKey(other);
      huint32_t ip = GetIPForIdent(pubKey);
      if(m_SNodeKeys.emplace(pubKey).second)
      {
        // this is a new service node make an outbound session to them
        m_SNodeSessions.emplace(
            other,
            std::unique_ptr< exit::SNodeSession >(new exit::SNodeSession(
                other,
                std::bind(&ExitEndpoint::QueueSNodePacket, this,
                          std::placeholders::_1, ip),
                GetRouter(), 2, 1, true)));
      }
      return ip;
    }

    bool
    ExitEndpoint::AllocateNewExit(const PubKey pk, const PathID_t &path,
                                  bool wantInternet)
    {
      if(wantInternet && !m_PermitExit)
        return false;
      huint32_t ip = GetIPForIdent(pk);
      if(GetRouter()->paths.TransitHopPreviousIsRouter(path, pk.as_array()))
      {
        // we think this path belongs to a service node
        // mark it as such so we don't make an outbound session to them
        m_SNodeKeys.emplace(pk.as_array());
      }
      m_ActiveExits.emplace(pk,
                            std::make_unique< exit::Endpoint >(
                                pk, path, !wantInternet, ip, this));

      m_Paths[path] = pk;
      return HasLocalMappedAddrFor(pk);
    }

    std::string
    ExitEndpoint::Name() const
    {
      return m_Name;
    }

    void
    ExitEndpoint::DelEndpointInfo(const PathID_t &path)
    {
      m_Paths.erase(path);
    }

    void
    ExitEndpoint::RemoveExit(const exit::Endpoint *ep)
    {
      auto range = m_ActiveExits.equal_range(ep->PubKey());
      auto itr   = range.first;
      while(itr != range.second)
      {
        if(itr->second->LocalPath() == ep->LocalPath())
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
        while(itr != m_SNodeSessions.end())
        {
          if(itr->second->IsExpired(now))
            itr = m_SNodeSessions.erase(itr);
          else
            ++itr;
        }
      }
      {
        // expire
        auto itr = m_ActiveExits.begin();
        while(itr != m_ActiveExits.end())
        {
          if(itr->second->IsExpired(now))
            itr = m_ActiveExits.erase(itr);
          else
            ++itr;
        }
        // pick chosen exits and tick
        m_ChosenExits.clear();
        itr = m_ActiveExits.begin();
        while(itr != m_ActiveExits.end())
        {
          // do we have an exit set for this key?
          if(m_ChosenExits.find(itr->first) != m_ChosenExits.end())
          {
            // yes
            if(m_ChosenExits[itr->first]->createdAt < itr->second->createdAt)
            {
              // if the iterators's exit is newer use it for the chosen exit for
              // key
              if(!itr->second->LooksDead(now))
                m_ChosenExits[itr->first] = itr->second.get();
            }
          }
          else if(!itr->second->LooksDead(
                      now))  // set chosen exit if not dead for key that doesn't
                             // have one yet
            m_ChosenExits[itr->first] = itr->second.get();
          // tick which clears the tx rx counters
          itr->second->Tick(now);
          ++itr;
        }
      }
    }
  }  // namespace handlers
}  // namespace llarp

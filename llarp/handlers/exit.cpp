#include <llarp/handlers/exit.hpp>
#include "../str.hpp"
#include "../router.hpp"
#include <llarp/net.hpp>
#include <cassert>

namespace llarp
{
  namespace handlers
  {
    static void
    ExitHandlerRecvPkt(llarp_tun_io *tun, const void *pkt, ssize_t sz)
    {
      static_cast< ExitEndpoint * >(tun->user)->OnInetPacket(
          llarp::InitBuffer(pkt, sz));
    }
    static void
    ExitHandlerFlushInbound(llarp_tun_io *tun)
    {
      static_cast< ExitEndpoint * >(tun->user)->FlushInbound();
    }

    ExitEndpoint::ExitEndpoint(const std::string &name, llarp_router *r)
        : m_Router(r)
        , m_Name(name)
        , m_Tun{{0}, 0, {0}, 0, 0, 0, 0, 0, 0}
        , m_InetToNetwork(name + "_exit_rx", r->netloop, r->netloop)

    {
      m_Tun.user      = this;
      m_Tun.recvpkt   = &ExitHandlerRecvPkt;
      m_Tun.tick      = &ExitHandlerFlushInbound;
      m_ShouldInitTun = true;
    }

    ExitEndpoint::~ExitEndpoint()
    {
    }

    void
    ExitEndpoint::FlushInbound()
    {
      auto now = Router()->Now();
      m_InetToNetwork.ProcessN(256, [&](Pkt_t &pkt) {
        llarp::PubKey pk;
        {
          auto itr = m_IPToKey.find(pkt.dst());
          if(itr == m_IPToKey.end())
          {
            // drop
            llarp::LogWarn(Name(), " dropping packet, has no session at ",
                           pkt.dst());
            return;
          }
          pk = itr->second;
        }
        llarp::exit::Endpoint *ep = nullptr;
        auto range                = m_ActiveExits.equal_range(pk);
        auto itr                  = range.first;
        uint64_t min              = std::numeric_limits< uint64_t >::max();
        /// pick path with lowest rx rate
        while(itr != range.second)
        {
          if(ep == nullptr)
            ep = itr->second.get();
          else if(itr->second->RxRate() < min && !itr->second->ExpiresSoon(now))
          {
            min = ep->RxRate();
            ep  = itr->second.get();
          }
          ++itr;
        }
        if(!ep->SendInboundTraffic(pkt.Buffer()))
        {
          llarp::LogWarn(Name(), " dropped inbound traffic for session ", pk);
        }
      });
    }

    bool
    ExitEndpoint::Start()
    {
      if(m_ShouldInitTun)
        return llarp_ev_add_tun(Router()->netloop, &m_Tun);
      return true;
    }

    llarp_router *
    ExitEndpoint::Router()
    {
      return m_Router;
    }

    llarp_crypto *
    ExitEndpoint::Crypto()
    {
      return &m_Router->crypto;
    }

    huint32_t
    ExitEndpoint::GetIfAddr() const
    {
      return m_IfAddr;
    }

    bool
    ExitEndpoint::HasLocalMappedAddrFor(const llarp::PubKey &pk) const
    {
      return m_KeyToIP.find(pk) != m_KeyToIP.end();
    }

    huint32_t
    ExitEndpoint::GetIPForIdent(const llarp::PubKey pk)
    {
      huint32_t found = {0};
      if(!HasLocalMappedAddrFor(pk))
      {
        // allocate and map
        found.h = AllocateNewAddress().h;
        if(!m_KeyToIP.emplace(pk, found).second)
        {
          llarp::LogError(Name(), "failed to map ", pk, " to ", found);
          return found;
        }
        if(!m_IPToKey.emplace(found, pk).second)
        {
          llarp::LogError(Name(), "failed to map ", found, " to ", pk);
          return found;
        }
        if(HasLocalMappedAddrFor(pk))
          llarp::LogInfo(Name(), " mapping ", pk, " to ", found);
        else
          llarp::LogError(Name(), "failed to map ", pk, " to ", found);
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
      llarp::PubKey pk = m_IPToKey[found];
      KickIdentOffExit(pk);

      return found;
    }

    bool
    ExitEndpoint::QueueOutboundTraffic(llarp_buffer_t buf)
    {
      return llarp_ev_tun_async_write(&m_Tun, buf.base, buf.sz);
    }

    void
    ExitEndpoint::KickIdentOffExit(const llarp::PubKey &pk)
    {
      llarp::LogInfo(Name(), " kicking ", pk, " off exit");
      huint32_t ip = m_KeyToIP[pk];
      m_KeyToIP.erase(pk);
      m_IPToKey.erase(ip);
      auto range    = m_ActiveExits.equal_range(pk);
      auto exit_itr = range.first;
      while(exit_itr != range.second)
        exit_itr = m_ActiveExits.erase(exit_itr);
    }

    void
    ExitEndpoint::MarkIPActive(llarp::huint32_t ip)
    {
      m_IPActivity[ip] = Router()->Now();
    }

    void
    ExitEndpoint::OnInetPacket(llarp_buffer_t buf)
    {
      m_InetToNetwork.EmplaceIf(
          [buf](Pkt_t &pkt) -> bool { return pkt.Load(buf); });
    }

    llarp::exit::Endpoint *
    ExitEndpoint::FindEndpointByPath(const llarp::PathID_t &path)
    {
      llarp::exit::Endpoint *endpoint = nullptr;
      llarp::PubKey pk;
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
    ExitEndpoint::UpdateEndpointPath(const llarp::PubKey &remote,
                                     const llarp::PathID_t &next)
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
      if(k == "ifaddr")
      {
        auto pos = v.find("/");
        if(pos == std::string::npos)
        {
          llarp::LogError(Name(), " ifaddr is not a cidr: ", v);
          return false;
        }
        std::string nmask_str = v.substr(1 + pos);
        std::string host_str  = v.substr(0, pos);
        strncpy(m_Tun.ifaddr, host_str.c_str(), sizeof(m_Tun.ifaddr));
        m_Tun.netmask = std::atoi(nmask_str.c_str());

        llarp::Addr ifaddr(host_str);
        m_IfAddr     = ifaddr.xtohl();
        m_NextAddr   = m_IfAddr;
        m_HigestAddr = m_IfAddr ^ (~llarp::netmask_ipv4_bits(m_Tun.netmask));
        llarp::LogInfo(Name(), " set ifaddr range to ", m_Tun.ifaddr, "/",
                       m_Tun.netmask, " lo=", m_IfAddr, " hi=", m_HigestAddr);
      }
      if(k == "ifname")
      {
        strncpy(m_Tun.ifname, v.c_str(), sizeof(m_Tun.ifname));
        llarp::LogInfo(Name(), " set ifname to ", m_Tun.ifname);
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

    bool
    ExitEndpoint::AllocateNewExit(const llarp::PubKey pk,
                                  const llarp::PathID_t &path,
                                  bool wantInternet)
    {
      if(wantInternet && !m_PermitExit)
        return false;
      huint32_t ip = GetIPForIdent(pk);
      m_ActiveExits.insert(std::make_pair(
          pk, new llarp::exit::Endpoint(pk, path, !wantInternet, ip, this)));
      m_Paths[path] = pk;
      return HasLocalMappedAddrFor(pk);
    }

    std::string
    ExitEndpoint::Name() const
    {
      return m_Name;
    }

    void
    ExitEndpoint::DelEndpointInfo(const llarp::PathID_t &path,
                                  const huint32_t &ip, const llarp::PubKey &pk)
    {
      m_Paths.erase(path);
      m_IPToKey.erase(ip);
      m_KeyToIP.erase(pk);
    }

    void
    ExitEndpoint::RemoveExit(const llarp::exit::Endpoint *ep)
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
      auto itr = m_ActiveExits.begin();
      while(itr != m_ActiveExits.end())
      {
        if(itr->second->IsExpired(now))
        {
          itr = m_ActiveExits.erase(itr);
        }
        else
        {
          itr->second->Tick(now);
          ++itr;
        }
      }
    }
  }  // namespace handlers
}  // namespace llarp
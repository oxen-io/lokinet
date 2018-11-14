#include <llarp/handlers/exit.hpp>
#include "../str.hpp"

namespace llarp
{
  namespace handlers
  {
    ExitEndpoint::ExitEndpoint(const std::string &name, llarp_router *r)
        : TunEndpoint(name, r), m_Name(name)
    {
    }

    ExitEndpoint::~ExitEndpoint()
    {
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
          if(itr->second.PubKey() == pk)
            endpoint = &itr->second;
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
      if(k == "exit")
      {
        m_PermitExit = IsTrueValue(v.c_str());
        // TODO: implement me
        return true;
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
      return TunEndpoint::SetOption(k, v);
    }

    bool
    ExitEndpoint::AllocateNewExit(const llarp::PubKey &pk,
                                  const llarp::PathID_t &path,
                                  bool permitInternet)
    {
      m_ActiveExits.insert(std::make_pair(
          pk, llarp::exit::Endpoint(pk, path, !permitInternet, this)));
      return true;
    }

    void
    ExitEndpoint::FlushSend()
    {
      m_UserToNetworkPktQueue.Process([&](net::IPv4Packet &pkt) {
        // find pubkey for addr
        if(!HasLocalIP(pkt.dst()))
        {
          llarp::LogWarn(Name(), " has no endpoint for ", pkt.dst());
          return true;
        }
        llarp::PubKey pk = ObtainAddrForIP< llarp::PubKey >(pkt.dst());
        pkt.UpdateIPv4PacketOnDst(pkt.src(), {0});
        auto range    = m_ActiveExits.equal_range(pk);
        auto exit_itr = range.first;
        while(exit_itr != range.second)
        {
          if(exit_itr->second.SendInboundTraffic(pkt.Buffer()))
            return true;
          ++exit_itr;
        }
        // dropped
        llarp::LogWarn(Name(), " dropped traffic to ", pk);
        return true;
      });
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
      m_IPToAddr.erase(ip);
      m_AddrToIP.erase(pk);
    }

    void
    ExitEndpoint::Tick(llarp_time_t now)
    {
      auto itr = m_ActiveExits.begin();
      while(itr != m_ActiveExits.end())
      {
        if(itr->second.IsExpired(now))
        {
          itr = m_ActiveExits.erase(itr);
        }
        else
          ++itr;
      }
      // call parent
      TunEndpoint::Tick(now);
    }
  }  // namespace handlers
}  // namespace llarp
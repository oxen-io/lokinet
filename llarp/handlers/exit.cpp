#include <llarp/handlers/exit.hpp>

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

    bool
    ExitEndpoint::SetOption(const std::string &k, const std::string &v)
    {
      if(k == "exit")
      {
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

    void
    ExitEndpoint::FlushSend()
    {
      m_UserToNetworkPktQueue.Process([&](net::IPv4Packet &pkt) {
        // find pubkey for addr
        auto itr = m_AddrsToPubKey.find(pkt.dst());
        if(itr == m_AddrsToPubKey.end())
        {
          llarp::LogWarn(Name(), " has no endpoint for ", pkt.dst());
          return true;
        }
        pkt.UpdateIPv4PacketOnSrc();
        auto range    = m_ActiveExits.equal_range(itr->second);
        auto exit_itr = range.first;
        while(exit_itr != range.second)
        {
          if(exit_itr->second.SendInboundTraffic(pkt.Buffer()))
            return true;
          ++exit_itr;
        }
        // dropped
        llarp::LogWarn(Name(), " dropped traffic to ", itr->second);
        return true;
      });
    }

    std::string
    ExitEndpoint::Name() const
    {
      return m_Name;
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
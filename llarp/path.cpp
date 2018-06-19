#include <deque>
#include <llarp/encrypted_frame.hpp>
#include <llarp/path.hpp>
#include "router.hpp"

namespace llarp
{
  Path::Path(llarp_path_hops* h)
  {
    for(size_t idx = 0; idx < h->numHops; ++idx)
    {
      hops.emplace_back();
      llarp_rc_copy(&hops[idx].router, &h->hops[idx].router);
    }
  }

  PathContext::PathContext(llarp_router* router)
      : m_Router(router), m_AllowTransit(false)
  {
  }

  PathContext::~PathContext()
  {
  }

  void
  PathContext::AllowTransit()
  {
    m_AllowTransit = true;
  }

  bool
  PathContext::AllowingTransit() const
  {
    return m_AllowTransit;
  }

  llarp_threadpool*
  PathContext::Worker()
  {
    return m_Router->tp;
  }

  llarp_crypto*
  PathContext::Crypto()
  {
    return &m_Router->crypto;
  }

  llarp_logic*
  PathContext::Logic()
  {
    return m_Router->logic;
  }

  byte_t*
  PathContext::EncryptionSecretKey()
  {
    return m_Router->encryption;
  }

  bool
  PathContext::HopIsUs(const PubKey& k) const
  {
    return memcmp(k, m_Router->pubkey(), PUBKEYSIZE) == 0;
  }

  bool
  PathContext::ForwardLRCM(const RouterID& nextHop,
                           std::deque< EncryptedFrame >& frames)
  {
    LR_CommitMessage* msg = new LR_CommitMessage;
    while(frames.size())
    {
      msg->frames.push_back(frames.back());
      frames.pop_back();
    }
    return m_Router->SendToOrQueue(nextHop, {msg});
  }

  template < typename Map_t, typename Key_t, typename CheckValue_t >
  bool
  MapHas(Map_t& map, const Key_t& k, CheckValue_t check)
  {
    std::unique_lock< std::mutex > lock(map.first);
    auto itr = map.second.find(k);
    while(itr != map.second.end())
    {
      if(check(itr->second))
        return true;
      ++itr;
    }
    return false;
  }

  template < typename Map_t, typename Key_t, typename Value_t >
  void
  MapPut(Map_t& map, const Key_t& k, const Value_t& v)
  {
    std::unique_lock< std::mutex > lock(map.first);
    map.second.emplace(k, v);
  }

  void
  PathContext::AddOwnPath(Path* path)
  {
    MapPut(m_OurPaths, path->PathID(), path);
  }

  bool
  PathContext::HasTransitHop(const TransitHopInfo& info)
  {
    return MapHas(
        m_TransitPaths, info.pathID,
        [info](const TransitHop& hop) -> bool { return info == hop.info; });
  }

  const byte_t*
  PathContext::OurRouterID() const
  {
    return m_Router->pubkey();
  }

  void
  PathContext::PutTransitHop(const TransitHop& hop)
  {
    MapPut(m_TransitPaths, hop.info.pathID, hop);
  }

  void
  PathContext::ExpirePaths()
  {
    std::unique_lock< std::mutex > lock(m_TransitPaths.first);
    auto now  = llarp_time_now_ms();
    auto& map = m_TransitPaths.second;
    auto itr  = map.begin();
    while(itr != map.end())
    {
      if(itr->second.Expired(now))
        itr = map.erase(itr);
      else
        ++itr;
    }
  }

  bool
  TransitHop::Expired(llarp_time_t now) const
  {
    return now - started > lifetime;
  }

  TransitHopInfo::TransitHopInfo(const RouterID& down,
                                 const LR_CommitRecord& record)
      : pathID(record.pathid), upstream(record.nextHop), downstream(down)
  {
  }

  const PathID_t&
  Path::PathID() const
  {
    return hops[0].pathID;
  }

  RouterID
  Path::Upstream()
  {
    return hops[0].router.pubkey;
  }

}  // namespace llarp
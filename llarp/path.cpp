#include <deque>
#include <llarp/encrypted_frame.hpp>
#include <llarp/path.hpp>
#include "buffer.hpp"
#include "router.hpp"

namespace llarp
{
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
    llarp::Info("fowarding LRCM to ", nextHop);
    LR_CommitMessage* msg = new LR_CommitMessage;
    while(frames.size())
    {
      msg->frames.push_back(frames.front());
      frames.pop_front();
    }
    return m_Router->SendToOrQueue(nextHop, msg);
  }
  template < typename Map_t, typename Key_t, typename CheckValue_t >
  IHopHandler*
  MapGet(Map_t& map, const Key_t& k, CheckValue_t check)
  {
    std::unique_lock< std::mutex > lock(map.first);
    auto itr = map.second.find(k);
    while(itr != map.second.end())
    {
      if(check(itr->second))
        return itr->second;
      ++itr;
    }
    return nullptr;
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

  template < typename Map_t, typename Key_t, typename Check_t >
  void
  MapDel(Map_t& map, const Key_t& k, Check_t check)
  {
    std::unique_lock< std::mutex > lock(map.first);
    auto itr = map.second.find(k);
    while(itr != map.second.end())
    {
      if(check(itr->second))
        itr = map.second.erase(itr);
      else
        ++itr;
    }
  }

  void
  PathContext::AddOwnPath(Path* path)
  {
    MapPut(m_OurPaths, path->PathID(), path);
  }

  bool
  PathContext::HasTransitHop(const TransitHopInfo& info)
  {
    return MapHas(m_TransitPaths, info.pathID, [info](TransitHop* hop) -> bool {
      return info == hop->info;
    });
  }

  IHopHandler*
  PathContext::GetByUpstream(const RouterID& remote, const PathID_t& id)
  {
    auto own = MapGet(m_OurPaths, id, [remote](const Path* p) -> bool {
      return p->Upstream() == remote;
    });
    if(own)
      return own;
    return MapGet(m_TransitPaths, id, [remote](const TransitHop* hop) -> bool {
      return hop->info.upstream == remote;
    });
  }

  const byte_t*
  PathContext::OurRouterID() const
  {
    return m_Router->pubkey();
  }

  llarp_router*
  PathContext::Router()
  {
    return m_Router;
  }

  void
  PathContext::PutTransitHop(TransitHop* hop)
  {
    MapPut(m_TransitPaths, hop->info.txID, hop);
    MapPut(m_TransitPaths, hop->info.rxID, hop);
  }

  void
  PathContext::ExpirePaths()
  {
    std::unique_lock< std::mutex > lock(m_TransitPaths.first);
    auto now  = llarp_time_now_ms();
    auto& map = m_TransitPaths.second;
    auto itr  = map.begin();
    std::set< TransitHop* > removePaths;
    while(itr != map.end())
    {
      if(itr->second->Expired(now))
      {
        TransitHop* path = itr->second;
        llarp::Info("transit path expired ", path);
        removePaths.insert(path);
      }
      ++itr;
    }
    for(auto& p : removePaths)
    {
      map.erase(p->info.txID);
      map.erase(p->info.rxID);
      delete p;
    }
  }

  Path::Path(llarp_path_hops* h) : hops(h->numHops)
  {
    for(size_t idx = 0; idx < h->numHops; ++idx)
    {
      llarp_rc_copy(&hops[idx].router, &h->hops[idx].router);
    }
  }

  const PathID_t&
  Path::TXID() const
  {
    return hops[0].pathTX;
  }

  const PathID_t&
  Path::RXID() const
  {
    return hops[0].pathRX;
  }

  RouterID
  Path::Upstream() const
  {
    return hops[0].router.pubkey;
  }

  bool
  Path::HandleUpstream(llarp_buffer_t buf, const TunnelNonce& Y,
                       llarp_router* r)
  {
    for(const auto& hop : hops)
    {
      r->crypto.xchacha20(buf, hop.shared, Y);
    }
    RelayUpstreamMessage* msg = new RelayUpstreamMessage;
    msg->X                    = buf;
    msg->Y                    = Y;
    msg->pathid               = PathID();
    msg->pathid.data_l()[1]   = 0;
    return r->SendToOrQueue(Upstream(), msg);
  }

  bool
  Path::Expired(llarp_time_t now) const
  {
    return now - buildStarted > hops[0].lifetime;
  }

  bool
  Path::HandleDownstream(llarp_buffer_t buf, const TunnelNonce& Y,
                         llarp_router* r)
  {
    size_t idx = hops.size() - 1;
    while(idx >= 0)
    {
      r->crypto.xchacha20(buf, hops[idx].shared, Y);
      if(idx)
        idx--;
      else
        break;
    }
    return HandleRoutingMessage(buf, r);
  }

  bool
  Path::HandleRoutingMessage(llarp_buffer_t buf, llarp_router* r)
  {
    // TODO: implement me
    return true;
  }

  bool
  Path::SendRoutingMessage(const llarp::routing::IMessage* msg, llarp_router* r)
  {
    byte_t tmp[MAX_LINK_MSG_SIZE / 2];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    if(!msg->BEncode(&buf))
      return false;
    // rewind
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    // make nonce
    TunnelNonce N;
    N.Randomize();
    return HandleUpstream(buf, N, r);
  }

}  // namespace llarp
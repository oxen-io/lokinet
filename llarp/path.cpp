#include <deque>
#include <llarp/encrypted_frame.hpp>
#include <llarp/path.hpp>
#include "buffer.hpp"
#include "pathbuilder.hpp"
#include "router.hpp"

namespace llarp
{
  namespace path
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
    template < typename Map_t, typename Key_t, typename CheckValue_t,
               typename GetFunc_t >
    IHopHandler*
    MapGet(Map_t& map, const Key_t& k, CheckValue_t check, GetFunc_t get)
    {
      std::unique_lock< std::mutex > lock(map.first);
      auto range = map.second.equal_range(k);
      for(auto i = range.first; i != range.second; ++i)
      {
        if(check(i->second))
          return get(i->second);
      }
      return nullptr;
    }

    template < typename Map_t, typename Key_t, typename CheckValue_t >
    bool
    MapHas(Map_t& map, const Key_t& k, CheckValue_t check)
    {
      std::unique_lock< std::mutex > lock(map.first);
      auto range = map.second.equal_range(k);
      for(auto i = range.first; i != range.second; ++i)
      {
        if(check(i->second))
          return true;
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

    template < typename Map_t, typename Visit_t >
    void
    MapIter(Map_t& map, Visit_t v)
    {
      std::unique_lock< std::mutex > lock(map.first);
      for(const auto& item : map.second)
        v(item);
    }

    template < typename Map_t, typename Key_t, typename Check_t >
    void
    MapDel(Map_t& map, const Key_t& k, Check_t check)
    {
      std::unique_lock< std::mutex > lock(map.first);
      auto range = map.second.equal_range(k);
      for(auto i = range.first; i != range.second;)
      {
        if(check(i->second))
          i = map.second.erase(i);
        else
          ++i;
      }
    }

    void
    PathContext::AddOwnPath(PathSet* set, Path* path)
    {
      set->AddPath(path);
      MapPut(m_OurPaths, path->TXID(), set);
      MapPut(m_OurPaths, path->RXID(), set);
    }

    bool
    PathContext::HasTransitHop(const TransitHopInfo& info)
    {
      return MapHas(m_TransitPaths, info.txID, [info](TransitHop* hop) -> bool {
        return info == hop->info;
      });
    }

    IHopHandler*
    PathContext::GetByUpstream(const RouterID& remote, const PathID_t& id)
    {
      auto own = MapGet(m_OurPaths, id,
                        [](const PathSet* s) -> bool {
                          // TODO: is this right?
                          return true;
                        },
                        [remote, id](PathSet* p) -> IHopHandler* {
                          return p->GetByUpstream(remote, id);
                        });
      if(own)
        return own;

      return MapGet(m_TransitPaths, id,
                    [remote](const TransitHop* hop) -> bool {
                      return hop->info.upstream == remote;
                    },
                    [](TransitHop* h) -> IHopHandler* { return h; });
    }

    IHopHandler*
    PathContext::GetByDownstream(const RouterID& remote, const PathID_t& id)
    {
      return MapGet(m_TransitPaths, id,
                    [remote](const TransitHop* hop) -> bool {
                      return hop->info.downstream == remote;
                    },
                    [](TransitHop* h) -> IHopHandler* { return h; });
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
      for(auto& builder : m_PathBuilders)
      {
        builder->ExpirePaths(now);
      }
    }

    void
    PathContext::BuildPaths()
    {
      for(auto& builder : m_PathBuilders)
      {
        if(builder->ShouldBuildMore())
        {
          builder->BuildOne();
        }
      }
    }

    void
    PathContext::AddPathBuilder(llarp_pathbuilder_context* ctx)
    {
      m_PathBuilders.push_back(ctx);
    }

    PathHopConfig::PathHopConfig()
    {
      llarp_rc_clear(&router);
    }

    PathHopConfig::~PathHopConfig()
    {
      llarp_rc_free(&router);
    }

    Path::Path(llarp_path_hops* h) : hops(h->numHops)
    {
      for(size_t idx = 0; idx < h->numHops; ++idx)
      {
        llarp_rc_copy(&hops[idx].router, &h->hops[idx].router);
        hops[idx].txID.Randomize();
        hops[idx].rxID.Randomize();
      }
      /*
      for(size_t idx = (h->numHops - 1); idx > 0; --idx)
      {
        hops[idx].txID = hops[idx - 1].rxID;
      }
      */
      for(size_t idx = 0; idx < h->numHops - 1; ++idx)
      {
        hops[idx].txID = hops[idx + 1].rxID;
      }
    }

    void
    Path::SetBuildResultHook(BuildResultHookFunc func)
    {
      m_BuiltHook = func;
    }

    const PathID_t&
    Path::TXID() const
    {
      return hops[0].txID;
    }

    const PathID_t&
    Path::RXID() const
    {
      return hops[0].rxID;
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
      msg->pathid               = TXID();
      return r->SendToOrQueue(Upstream(), msg);
    }

    bool
    Path::Expired(llarp_time_t now) const
    {
      if(status == ePathEstablished)
        return now - buildStarted > hops[0].lifetime;
      else if(status == ePathBuilding)
        return now - buildStarted > PATH_BUILD_TIMEOUT;
      else
        return true;
    }

    bool
    Path::HandleDownstream(llarp_buffer_t buf, const TunnelNonce& Y,
                           llarp_router* r)
    {
      for(const auto& hop : hops)
      {
        r->crypto.xchacha20(buf, hop.shared, Y);
      }
      return HandleRoutingMessage(buf, r);
    }

    bool
    Path::HandleHiddenServiceData(llarp_buffer_t buf)
    {
      // TODO: implement me
      return false;
    }

    bool
    Path::HandleRoutingMessage(llarp_buffer_t buf, llarp_router* r)
    {
      if(!m_InboundMessageParser.ParseMessageBuffer(buf, this))
      {
        llarp::Warn("Failed to parse inbound routing message");
        return false;
      }
      return true;
    }

    bool
    Path::SendRoutingMessage(const llarp::routing::IMessage* msg,
                             llarp_router* r)
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

    bool
    Path::HandlePathConfirmMessage(
        const llarp::routing::PathConfirmMessage* msg)
    {
      if(status == ePathBuilding)
      {
        // confirm that we build the path
        status = ePathEstablished;
        llarp::Info("path is confirmed rx=", RXID(), " tx=", TXID());
        if(m_BuiltHook)
          m_BuiltHook(this);
        m_BuiltHook = nullptr;
        return true;
      }
      llarp::Warn("got unwarrented path confirm message on rx=", RXID(),
                  " tx=", TXID());
      return false;
    }

    bool
    Path::HandlePathLatencyMessage(
        const llarp::routing::PathLatencyMessage* msg)
    {
      // TODO: implement me
      return false;
    }

    bool
    Path::HandleDHTMessage(const llarp::dht::IMessage* msg)
    {
      // TODO: implement me
      return false;
    }
  }  // namespace path
}  // namespace llarp

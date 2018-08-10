#include <deque>
#include <llarp/encrypted_frame.hpp>
#include <llarp/path.hpp>
#include <llarp/pathbuilder.hpp>
#include "buffer.hpp"
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
      llarp::LogDebug("fowarding LRCM to ", nextHop);
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
      map.second.insert(std::make_pair(k, v));
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

    PathSet*
    PathContext::GetLocalPathSet(const PathID_t& id)
    {
      auto& map = m_OurPaths;
      std::unique_lock< std::mutex > lock(map.first);
      auto itr = map.second.find(id);
      if(itr != map.second.end())
      {
        return itr->second;
      }
      return nullptr;
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
          llarp::LogDebug("transit path expired ", path->info);
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
    PathContext::TickPaths()
    {
      auto now = llarp_time_now_ms();
      for(auto& builder : m_PathBuilders)
        builder->Tick(now, m_Router);
    }

    routing::IMessageHandler*
    PathContext::GetHandler(const PathID_t& id)
    {
      routing::IMessageHandler* h = nullptr;
      auto pathset                = GetLocalPathSet(id);
      if(pathset)
      {
        h = pathset->GetPathByID(id);
      }
      if(h)
        return h;
      RouterID us(OurRouterID());
      auto& map = m_TransitPaths;
      std::unique_lock< std::mutex > lock(map.first);
      auto range = map.second.equal_range(id);
      for(auto i = range.first; i != range.second; ++i)
      {
        if(i->second->info.upstream == us)
          return i->second;
      }
      return nullptr;
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

      for(size_t idx = 0; idx < h->numHops - 1; ++idx)
      {
        hops[idx].txID = hops[idx + 1].rxID;
      }
      // initialize parts of the introduction
      intro.router = hops[h->numHops - 1].router.pubkey;
      // TODO: or is it rxid ?
      intro.pathID = hops[h->numHops - 1].txID;
    }

    void
    Path::SetBuildResultHook(BuildResultHookFunc func)
    {
      m_BuiltHook = func;
    }

    RouterID
    Path::Endpoint() const
    {
      return hops[hops.size() - 1].router.pubkey;
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

    bool
    Path::IsReady() const
    {
      return intro.latency > 0 && status == ePathEstablished;
    }

    RouterID
    Path::Upstream() const
    {
      return hops[0].router.pubkey;
    }

    void
    Path::Tick(llarp_time_t now, llarp_router* r)
    {
      if(Expired(now))
        return;
      if(now < m_LastLatencyTestTime)
        return;
      auto dlt = now - m_LastLatencyTestTime;
      if(dlt > 5000 && m_LastLatencyTestID == 0)
      {
        llarp::routing::PathLatencyMessage latency;
        latency.T             = llarp_randint();
        m_LastLatencyTestID   = latency.T;
        m_LastLatencyTestTime = now;
        SendRoutingMessage(&latency, r);
      }
    }

    bool
    Path::HandleUpstream(llarp_buffer_t buf, const TunnelNonce& Y,
                         llarp_router* r)
    {
      TunnelNonce n = Y;
      for(const auto& hop : hops)
      {
        r->crypto.xchacha20(buf, hop.shared, n);
        n ^= hop.nonceXOR;
      }
      RelayUpstreamMessage* msg = new RelayUpstreamMessage;
      msg->X                    = buf;
      msg->Y                    = Y;
      msg->pathid               = TXID();
      if(r->SendToOrQueue(Upstream(), msg))
        return true;
      llarp::LogError("send to ", Upstream(), " failed");
      return false;
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
      TunnelNonce n = Y;
      for(const auto& hop : hops)
      {
        n ^= hop.nonceXOR;
        r->crypto.xchacha20(buf, hop.shared, n);
      }
      return HandleRoutingMessage(buf, r);
    }

    bool
    Path::HandleRoutingMessage(llarp_buffer_t buf, llarp_router* r)
    {
      if(!m_InboundMessageParser.ParseMessageBuffer(buf, this, RXID(), r))
      {
        llarp::LogWarn("Failed to parse inbound routing message");
        return false;
      }
      return true;
    }

    bool
    Path::SendRoutingMessage(llarp::routing::IMessage* msg, llarp_router* r)
    {
      msg->S = m_SequenceNum++;
      byte_t tmp[MAX_LINK_MSG_SIZE / 2];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!msg->BEncode(&buf))
      {
        llarp::LogError("Bencode failed");
        llarp::DumpBuffer(buf);
        return false;
      }
      // make nonce
      TunnelNonce N;
      N.Randomize();
      buf.sz = buf.cur - buf.base;
      // pad smaller messages
      if(buf.sz < MESSAGE_PAD_SIZE)
      {
        // randomize padding
        r->crypto.randbytes(buf.cur, MESSAGE_PAD_SIZE - buf.sz);
        buf.sz = MESSAGE_PAD_SIZE;
      }
      buf.cur = buf.base;
      return HandleUpstream(buf, N, r);
    }

    bool
    Path::HandlePathTransferMessage(
        const llarp::routing::PathTransferMessage* msg, llarp_router* r)
    {
      llarp::LogWarn("unwarrented path transfer message on tx=", TXID(),
                     " rx=", RXID());
      return false;
    }

    bool
    Path::HandlePathConfirmMessage(
        const llarp::routing::PathConfirmMessage* msg, llarp_router* r)
    {
      if(status == ePathBuilding)
      {
        // finish initializing introduction
        intro.expiresAt = buildStarted + hops[0].lifetime;
        // confirm that we build the path
        status = ePathEstablished;
        llarp::LogInfo("path is confirmed tx=", TXID(), " rx=", RXID());
        if(m_BuiltHook)
          m_BuiltHook(this);
        m_BuiltHook = nullptr;

        llarp::routing::PathLatencyMessage latency;
        latency.T             = llarp_randint();
        m_LastLatencyTestID   = latency.T;
        m_LastLatencyTestTime = llarp_time_now_ms();
        return SendRoutingMessage(&latency, r);
      }
      llarp::LogWarn("got unwarrented path confirm message on tx=", RXID(),
                     " rx=", RXID());
      return false;
    }

    bool
    Path::HandleHiddenServiceFrame(const llarp::service::ProtocolFrame* frame)
    {
      if(m_DataHandler)
        return m_DataHandler(frame);
      return false;
    }

    bool
    Path::HandlePathLatencyMessage(
        const llarp::routing::PathLatencyMessage* msg, llarp_router* r)
    {
      if(msg->L == m_LastLatencyTestID && status == ePathEstablished)
      {
        intro.latency = llarp_time_now_ms() - m_LastLatencyTestTime;
        llarp::LogInfo("path latency is ", intro.latency, " ms for tx=", TXID(),
                       " rx=", RXID());
        m_LastLatencyTestID = 0;
        return true;
      }
      else
      {
        llarp::LogWarn("unwarrented path latency message via ", Upstream());
        return false;
      }
    }

    bool
    Path::HandleDHTMessage(const llarp::dht::IMessage* msg, llarp_router* r)
    {
      std::vector< llarp::dht::IMessage* > discard;
      auto result = msg->HandleMessage(r->dht, discard);
      for(auto& msg : discard)
        delete msg;
      return result;
    }

  }  // namespace path
}  // namespace llarp

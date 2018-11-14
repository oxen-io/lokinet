#include <deque>
#include <llarp/encrypted_frame.hpp>
#include <llarp/path.hpp>
#include <llarp/pathbuilder.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/discard.hpp>
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
                             const std::array< EncryptedFrame, 8 >& frames)
    {
      llarp::LogDebug("fowarding LRCM to ", nextHop);
      LR_CommitMessage msg;
      msg.frames = frames;
      return m_Router->SendToOrQueue(nextHop, &msg);
    }
    template < typename Map_t, typename Key_t, typename CheckValue_t,
               typename GetFunc_t >
    IHopHandler*
    MapGet(Map_t& map, const Key_t& k, CheckValue_t check, GetFunc_t get)
    {
      util::Lock lock(map.first);
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
      util::Lock lock(map.first);
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
      util::Lock lock(map.first);
      map.second.insert(std::make_pair(k, v));
    }

    template < typename Map_t, typename Visit_t >
    void
    MapIter(Map_t& map, Visit_t v)
    {
      util::Lock lock(map.first);
      for(const auto& item : map.second)
        v(item);
    }

    template < typename Map_t, typename Key_t, typename Check_t >
    void
    MapDel(Map_t& map, const Key_t& k, Check_t check)
    {
      util::Lock lock(map.first);
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
      return MapHas(m_TransitPaths, info.txID,
                    [info](const std::shared_ptr< TransitHop >& hop) -> bool {
                      return info == hop->info;
                    });
    }

    IHopHandler*
    PathContext::GetByUpstream(const RouterID& remote, const PathID_t& id)
    {
      auto own = MapGet(m_OurPaths, id,
                        [](__attribute__((unused)) const PathSet* s) -> bool {
                          // TODO: is this right?
                          return true;
                        },
                        [remote, id](PathSet* p) -> IHopHandler* {
                          return p->GetByUpstream(remote, id);
                        });
      if(own)
        return own;

      return MapGet(m_TransitPaths, id,
                    [remote](const std::shared_ptr< TransitHop >& hop) -> bool {
                      return hop->info.upstream == remote;
                    },
                    [](const std::shared_ptr< TransitHop >& h) -> IHopHandler* {
                      return h.get();
                    });
    }

    IHopHandler*
    PathContext::GetByDownstream(const RouterID& remote, const PathID_t& id)
    {
      return MapGet(m_TransitPaths, id,
                    [remote](const std::shared_ptr< TransitHop >& hop) -> bool {
                      return hop->info.downstream == remote;
                    },
                    [](const std::shared_ptr< TransitHop >& h) -> IHopHandler* {
                      return h.get();
                    });
    }

    PathSet*
    PathContext::GetLocalPathSet(const PathID_t& id)
    {
      auto& map = m_OurPaths;
      util::Lock lock(map.first);
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

    IHopHandler*
    PathContext::GetPathForTransfer(const PathID_t& id)
    {
      RouterID us(OurRouterID());
      auto& map = m_TransitPaths;
      {
        util::Lock lock(map.first);
        auto range = map.second.equal_range(id);
        for(auto i = range.first; i != range.second; ++i)
        {
          if(i->second->info.upstream == us)
            return i->second.get();
        }
      }
      return nullptr;
    }

    void
    PathContext::PutTransitHop(std::shared_ptr< TransitHop > hop)
    {
      MapPut(m_TransitPaths, hop->info.txID, hop);
      MapPut(m_TransitPaths, hop->info.rxID, hop);
    }

    void
    PathContext::ExpirePaths(llarp_time_t now)
    {
      util::Lock lock(m_TransitPaths.first);
      auto& map = m_TransitPaths.second;
      auto itr  = map.begin();
      while(itr != map.end())
      {
        if(itr->second->Expired(now))
        {
          itr = map.erase(itr);
        }
        else
          ++itr;
      }

      for(auto& builder : m_PathBuilders)
      {
        if(builder)
          builder->ExpirePaths(now);
      }
    }

    void
    PathContext::BuildPaths(llarp_time_t now)
    {
      for(auto& builder : m_PathBuilders)
      {
        if(builder->ShouldBuildMore(now))
        {
          builder->BuildOne(ePathRoleAny);
        }
        if(builder->ShouldBuildMoreForRoles(now, ePathRoleExit))
        {
          builder->BuildOne(ePathRoleExit);
        }
      }
    }

    void
    PathContext::TickPaths(llarp_time_t now)
    {
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
      {
        util::Lock lock(map.first);
        auto range = map.second.equal_range(id);
        for(auto i = range.first; i != range.second; ++i)
        {
          if(i->second->info.upstream == us)
            return i->second.get();
        }
      }
      return nullptr;
    }

    void
    PathContext::AddPathBuilder(Builder* ctx)
    {
      m_PathBuilders.push_back(ctx);
    }

    void
    PathContext::RemovePathSet(PathSet* set)
    {
      util::Lock lock(m_OurPaths.first);
      auto& map = m_OurPaths.second;
      auto itr  = map.begin();
      while(itr != map.end())
      {
        if(itr->second == set)
          itr = map.erase(itr);
        else
          ++itr;
      }
    }

    void
    PathContext::RemovePathBuilder(Builder* ctx)
    {
      m_PathBuilders.remove(ctx);
      RemovePathSet(ctx);
    }

    PathHopConfig::PathHopConfig()
    {
    }

    PathHopConfig::~PathHopConfig()
    {
    }

    Path::Path(const std::vector< RouterContact >& h, PathSet* parent,
               PathRole startingRoles)
        : m_PathSet(parent), _role(startingRoles)
    {
      hops.resize(h.size());
      size_t hsz = h.size();
      for(size_t idx = 0; idx < hsz; ++idx)
      {
        hops[idx].rc = h[idx];
        hops[idx].txID.Randomize();
        hops[idx].rxID.Randomize();
      }

      for(size_t idx = 0; idx < hsz - 1; ++idx)
      {
        hops[idx].txID = hops[idx + 1].rxID;
      }
      // initialize parts of the introduction
      intro.router = hops[hsz - 1].rc.pubkey;
      intro.pathID = hops[hsz - 1].txID;
      EnterState(ePathBuilding, parent->Now());
    }

    void
    Path::SetBuildResultHook(BuildResultHookFunc func)
    {
      m_BuiltHook = func;
    }

    RouterID
    Path::Endpoint() const
    {
      return hops[hops.size() - 1].rc.pubkey;
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
      return intro.latency > 0 && _status == ePathEstablished;
    }

    RouterID
    Path::Upstream() const
    {
      return hops[0].rc.pubkey;
    }

    void
    Path::EnterState(PathStatus st, llarp_time_t now)
    {
      if(st == ePathTimeout)
      {
        m_PathSet->HandlePathBuildTimeout(this);
      }
      else if(st == ePathBuilding)
      {
        llarp::LogInfo("path ", Name(), " is building");
        buildStarted = now;
      }
      _status = st;
    }

    void
    Path::Tick(llarp_time_t now, llarp_router* r)
    {
      if(Expired(now))
        return;

      if(_status == ePathBuilding)
      {
        if(now < buildStarted)
          return;
        auto dlt = now - buildStarted;
        if(dlt >= PATH_BUILD_TIMEOUT)
        {
          r->routerProfiling.MarkPathFail(this);
          EnterState(ePathTimeout, now);
          return;
        }
      }

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
      // check to see if this path is dead
      if(_status == ePathEstablished)
      {
        if(m_LastRecvMessage && now > m_LastRecvMessage
           && now - m_LastRecvMessage > PATH_ALIVE_TIMEOUT)
        {
          if(m_CheckForDead)
          {
            if(m_CheckForDead(this, dlt))
            {
              r->routerProfiling.MarkPathFail(this);
              EnterState(ePathTimeout, now);
            }
          }
          else
          {
            r->routerProfiling.MarkPathFail(this);
            EnterState(ePathTimeout, now);
          }
        }
        else if(dlt >= 10000 && m_LastRecvMessage == 0)
        {
          r->routerProfiling.MarkPathFail(this);
          EnterState(ePathTimeout, now);
        }
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
      RelayUpstreamMessage msg;
      msg.X      = buf;
      msg.Y      = Y;
      msg.pathid = TXID();
      if(r->SendToOrQueue(Upstream(), &msg))
        return true;
      llarp::LogError("send to ", Upstream(), " failed");
      return false;
    }

    bool
    Path::Expired(llarp_time_t now) const
    {
      if(_status == ePathEstablished)
        return now - buildStarted > hops[0].lifetime;
      else if(_status == ePathBuilding)
        return false;
      else
        return true;
    }

    std::string
    Path::Name() const
    {
      std::stringstream ss;
      ss << "TX=" << TXID() << " RX=" << RXID();
      return ss.str();
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
    Path::HandleUpdateExitVerifyMessage(
        const llarp::routing::UpdateExitVerifyMessage* msg, llarp_router* r)
    {
      (void)r;
      if(m_UpdateExitTX && msg->T == m_UpdateExitTX)
      {
        if(m_ExitUpdated)
          return m_ExitUpdated(this);
      }
      if(m_CloseExitTX && msg->T == m_CloseExitTX)
      {
        if(m_ExitClosed)
          return m_ExitClosed(this);
      }
      return false;
    }

    bool
    Path::SendRoutingMessage(const llarp::routing::IMessage* msg,
                             llarp_router* r)
    {
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
        __attribute__((unused)) const llarp::routing::PathTransferMessage* msg,
        __attribute__((unused)) llarp_router* r)
    {
      llarp::LogWarn("unwarranted path transfer message on tx=", TXID(),
                     " rx=", RXID());
      return false;
    }

    bool
    Path::HandleDataDiscardMessage(
        const llarp::routing::DataDiscardMessage* msg,
        __attribute__((unused)) llarp_router* r)
    {
      if(m_DropHandler)
        return m_DropHandler(this, msg->P, msg->S);
      return true;
    }

    bool
    Path::HandlePathConfirmMessage(
        __attribute__((unused)) const llarp::routing::PathConfirmMessage* msg,
        llarp_router* r)
    {
      auto now = r->Now();
      if(_status == ePathBuilding)
      {
        // finish initializing introduction
        intro.expiresAt = buildStarted + hops[0].lifetime;
        // confirm that we build the path
        EnterState(ePathEstablished, now);
        llarp::LogInfo("path is confirmed tx=", TXID(), " rx=", RXID(),
                       " took ", now - buildStarted, " ms");
        if(m_BuiltHook)
          m_BuiltHook(this);
        m_BuiltHook = nullptr;

        r->routerProfiling.MarkPathSuccess(this);

        // persist session with upstream router until the path is done
        r->PersistSessionUntil(Upstream(), intro.expiresAt);

        // send path latency test
        llarp::routing::PathLatencyMessage latency;
        latency.T             = llarp_randint();
        m_LastLatencyTestID   = latency.T;
        m_LastLatencyTestTime = now;
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
      {
        if(m_DataHandler(this, frame))
        {
          m_LastRecvMessage = m_PathSet->Now();
          return true;
        }
      }
      return false;
    }

    bool
    Path::HandlePathLatencyMessage(
        const llarp::routing::PathLatencyMessage* msg, llarp_router* r)
    {
      auto now = r->Now();
      if(msg->L == m_LastLatencyTestID)
      {
        intro.latency = now - m_LastLatencyTestTime;
        llarp::LogDebug("path latency is ", intro.latency,
                        " ms for tx=", TXID(), " rx=", RXID());
        m_LastLatencyTestID = 0;
        _status             = ePathEstablished;
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
      llarp::routing::DHTMessage reply;
      if(!msg->HandleMessage(r->dht, reply.M))
        return false;
      if(reply.M.size())
        return SendRoutingMessage(&reply, r);
      return true;
    }

    bool
    Path::HandleCloseExitMessage(const llarp::routing::CloseExitMessage* msg,
                                 llarp_router* r)
    {
      /// allows exits to close from their end
      if(SupportsRoles(ePathRoleExit))
      {
        if(msg->Verify(&r->crypto, Endpoint()))
        {
          llarp::LogInfo(Name(), " had its exit closed");
          _role &= ~ePathRoleExit;
          return true;
        }
        else
          llarp::LogError(Name(), " CXM from exit with bad signature");
      }
      else
        llarp::LogError(Name(), " unwarrented CXM");
      return false;
    }

    bool
    Path::SendExitRequest(const llarp::routing::ObtainExitMessage* msg,
                          llarp_router* r)
    {
      llarp::LogInfo(Name(), " sending exit request to ", Endpoint());
      m_ExitObtainTX = msg->T;
      return SendRoutingMessage(msg, r);
    }

    bool
    Path::HandleObtainExitMessage(const llarp::routing::ObtainExitMessage* msg,
                                  llarp_router* r)
    {
      (void)msg;
      (void)r;
      llarp::LogError(Name(), " got unwarrented OXM");
      return false;
    }

    bool
    Path::HandleUpdateExitMessage(const llarp::routing::UpdateExitMessage* msg,
                                  llarp_router* r)
    {
      (void)msg;
      (void)r;
      llarp::LogError(Name(), " got unwarrented UXM");
      return false;
    }

    bool
    Path::HandleRejectExitMessage(const llarp::routing::RejectExitMessage* msg,
                                  llarp_router* r)
    {
      if(m_ExitObtainTX && msg->T == m_ExitObtainTX)
      {
        if(!msg->Verify(&r->crypto, Endpoint()))
        {
          llarp::LogError(Name(), "RXM invalid signature");
          return false;
        }
        llarp::LogInfo(Name(), " ", Endpoint(), " Rejected exit");
        return InformExitResult(msg->B);
      }
      llarp::LogError(Name(), " got unwarrented RXM");
      return false;
    }

    bool
    Path::HandleGrantExitMessage(const llarp::routing::GrantExitMessage* msg,
                                 llarp_router* r)
    {
      if(m_ExitObtainTX && msg->T == m_ExitObtainTX)
      {
        if(!msg->Verify(&r->crypto, Endpoint()))
        {
          llarp::LogError(Name(), " GXM signature failed");
          return false;
        }
        // we now can send exit traffic
        _role |= ePathRoleExit;
        llarp::LogInfo(Name(), " ", Endpoint(), " Granted exit");
        return InformExitResult(0);
      }
      llarp::LogError(Name(), " got unwarrented GXM");
      return false;
    }

    bool
    Path::InformExitResult(llarp_time_t B)
    {
      bool result = true;
      for(const auto& hook : m_ObtainedExitHooks)
        result &= hook(this, B);
      m_ObtainedExitHooks.clear();
      return result;
    }

    bool
    Path::HandleTransferTrafficMessage(
        const llarp::routing::TransferTrafficMessage* msg, llarp_router* r)
    {
      // check if we can handle exit data
      if(!SupportsRoles(ePathRoleExit))
        return false;
      // verify sig
      if(!msg->Verify(&r->crypto, Endpoint()))
      {
        llarp::LogError(Name(), " bad signature on inbound traffic");
        return false;
      }
      // handle traffic if we have a handler
      if(m_ExitTrafficHandler)
        return m_ExitTrafficHandler(this, llarp::ConstBuffer(msg->X));
      // fail if no handler
      return false;
    }

  }  // namespace path
}  // namespace llarp

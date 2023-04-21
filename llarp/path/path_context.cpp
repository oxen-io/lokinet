#include "path_context.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <llarp/messages/relay_commit.hpp>
#include "llarp/messages/relay.hpp"
#include "llarp/path/ihophandler.hpp"
#include "llarp/path/path_types.hpp"
#include "llarp/path/pathset.hpp"
#include "llarp/path/transit_hop.hpp"
#include "llarp/router_id.hpp"
#include "llarp/util/compare_ptr.hpp"
#include "llarp/util/thread/annotations.hpp"
#include "oxen/log.hpp"
#include "path.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/i_outbound_message_handler.hpp>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_set>

namespace llarp
{
  namespace path
  {
    static auto logcat = log::Cat("path-context");

    static constexpr auto DefaultPathBuildLimit = 500ms;

    PathContext::PathContext(AbstractRouter* router)
        : m_Router(router), m_AllowTransit(false), m_PathLimits(DefaultPathBuildLimit)
    {
      m_upstream_flush = router->loop()->make_waker([this]() { PumpUpstream(); });
      m_downstream_flush = router->loop()->make_waker([this]() { PumpDownstream(); });
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

    bool
    PathContext::CheckPathLimitHitByIP(const IpAddress& ip)
    {
#ifdef TESTNET
      return false;
#else
      IpAddress remote = ip;
      // null out the port -- we don't care about it for path limiting purposes
      remote.setPort(0);
      // try inserting remote address by ip into decaying hash set
      // if it cannot insert it has hit a limit
      return not m_PathLimits.Insert(remote);
#endif
    }

    const EventLoop_ptr&
    PathContext::loop()
    {
      return m_Router->loop();
    }

    const SecretKey&
    PathContext::EncryptionSecretKey()
    {
      return m_Router->encryption();
    }

    bool
    PathContext::HopIsUs(const RouterID& k) const
    {
      return std::equal(m_Router->pubkey(), m_Router->pubkey() + PUBKEYSIZE, k.begin());
    }

    PathContext::EndpointPathPtrSet
    PathContext::FindOwnedPathsWithEndpoint(const RouterID& r)
    {
      EndpointPathPtrSet found;
      m_OurPaths.ForEach([&](const Path_ptr& p) {
        if (p->Endpoint() == r && p->IsReady())
          found.insert(p);
      });
      return found;
    }

    bool
    PathContext::ForwardLRCM(
        const RouterID& nextHop,
        const std::array<EncryptedFrame, 8>& frames,
        SendStatusHandler handler)
    {
      if (handler == nullptr)
      {
        LogError("Calling ForwardLRCM without passing result handler");
        return false;
      }

      const LR_CommitMessage msg{frames};

      LogDebug("forwarding LRCM to ", nextHop);

      return m_Router->SendToOrQueue(nextHop, msg, handler);
    }

    template <
        typename Lock_t,
        typename Map_t,
        typename Key_t,
        typename CheckValue_t,
        typename GetFunc_t,
        typename Return_ptr = HopHandler_ptr>
    Return_ptr
    MapGet(Map_t& map, const Key_t& k, CheckValue_t check, GetFunc_t get)
    {
      auto lock = map.acquire();
      auto range = map.second.equal_range(k);
      for (auto i = range.first; i != range.second; ++i)
      {
        if (check(i->second))
          return get(i->second);
      }
      return nullptr;
    }

    template <typename Lock_t, typename Map_t, typename Key_t, typename CheckValue_t>
    bool
    MapHas(Map_t& map, const Key_t& k, CheckValue_t check)
    {
      auto lock = map.acquire();
      auto range = map.second.equal_range(k);
      for (auto i = range.first; i != range.second; ++i)
      {
        if (check(i->second))
          return true;
      }
      return false;
    }

    template <typename Lock_t, typename Map_t, typename Key_t, typename Value_t>
    void
    MapPut(Map_t& map, const Key_t& k, const Value_t& v)
    {
      auto lock = map.acquire();
      map.second.emplace(k, v);
    }

    template <typename Lock_t, typename Map_t, typename Visit_t>
    void
    MapIter(Map_t& map, Visit_t v)
    {
      auto lock = map.acquire();
      for (const auto& item : map.second)
        v(item);
    }

    template <typename Lock_t, typename Map_t, typename Key_t, typename Check_t>
    void
    MapDel(Map_t& map, const Key_t& k, Check_t check)
    {
      auto lock = map.acquire();
      auto range = map.second.equal_range(k);
      for (auto i = range.first; i != range.second;)
      {
        if (check(i->second))
          i = map.second.erase(i);
        else
          ++i;
      }
    }

    void
    PathContext::AddOwnPath(PathSet_ptr set, Path_ptr path)
    {
      {
        auto lock = m_OurPaths.acquire();
        auto& map = m_OurPaths.second;
        const auto info = path->hop_info();
        map[info.rxID] = path;
        map[info.txID] = path;
      }
      set->AddPath(path);
    }

    bool
    PathContext::HasTransitHop(const TransitHopInfo& info)
    {
      return TransitHopByInfo(info) != std::nullopt;
    }

    std::optional<std::weak_ptr<TransitHop>>
    PathContext::TransitHopByInfo(const TransitHopInfo& info)
    {
      auto lock = m_TransitPaths.acquire();
      {
        auto& map = m_TransitPaths.second;
        // both rx and tx should be registered so we only use rxid for lookup
        auto range = map.equal_range(info.rxID);
        for (auto itr = range.first; itr != range.second; ++itr)
        {
          if (itr->second->info == info)
            return std::weak_ptr<TransitHop>{itr->second};
        }
      }
      return std::nullopt;
    }

    std::optional<std::weak_ptr<TransitHop>>
    PathContext::TransitHopByUpstream(const RouterID& upstream, const PathID_t& id)
    {
      auto lock = m_TransitPaths.acquire();
      {
        auto& map = m_TransitPaths.second;

        auto range = map.equal_range(id);
        for (auto itr = range.first; itr != range.second; ++itr)
        {
          const auto& info = itr->second->info;
          if (info.upstream == upstream)
            return std::weak_ptr<TransitHop>{itr->second};
        }
      }
      return std::nullopt;
    }

    HopHandler_ptr
    PathContext::GetByUpstream(const RouterID& remote, const PathID_t& id)
    {
      auto lock = m_OurPaths.acquire();
      {
        auto& map = m_OurPaths.second;

        auto range = map.equal_range(id);
        for (auto itr = range.first; itr != range.second; ++itr)
        {
          auto info = itr->second->hop_info();
          if (info.upstream == remote)
            return itr->second;
        }
      }
      return nullptr;
    }

    bool
    PathContext::TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& otherRouter)
    {
      auto lock = m_TransitPaths.acquire();
      auto itr = m_TransitPaths.second.find(path);
      if (itr == m_TransitPaths.second.end())
        return false;
      return itr->second->info.downstream == otherRouter;
    }

    HopHandler_ptr
    PathContext::GetByDownstream(const RouterID& remote, const PathID_t& id)
    {
      auto lock = m_TransitPaths.acquire();
      {
        auto& map = m_TransitPaths.second;

        auto range = map.equal_range(id);
        for (auto itr = range.first; itr != range.second; ++itr)
        {
          auto info = itr->second->hop_info();
          if (info.downstream == remote)
            return itr->second;
        }
      }
      return nullptr;
    }

    PathSet_ptr
    PathContext::GetLocalPathSet(const PathID_t& id)
    {
      auto lock = m_OurPaths.acquire();
      {
        auto& map = m_OurPaths.second;
        if (auto itr = map.find(id); itr != map.end())
          return itr->second->m_PathSet.lock();
      }
      return nullptr;
    }

    const byte_t*
    PathContext::OurRouterID() const
    {
      return m_Router->pubkey();
    }

    AbstractRouter*
    PathContext::Router()
    {
      return m_Router;
    }

    TransitHop_ptr
    PathContext::GetPathForTransfer(const PathID_t& id)
    {
      if (auto ptr = GetByUpstream(m_Router->pubkey(), id))
        return std::static_pointer_cast<TransitHop>(ptr);
      return nullptr;
    }

    void
    PathContext::PumpUpstream()
    {
      m_TransitPaths.ForEach([this](const auto& ptr) { ptr->FlushUpstream(m_Router); });
      m_OurPaths.ForEach([this](const auto& ptr) { ptr->FlushUpstream(m_Router); });
    }

    void
    PathContext::PumpDownstream()
    {
      m_TransitPaths.ForEach([this](const auto& ptr) { ptr->FlushDownstream(m_Router); });
      m_OurPaths.ForEach([this](const auto& ptr) { ptr->FlushDownstream(m_Router); });
    }

    uint64_t
    PathContext::CurrentTransitPaths()
    {
      auto lock = m_TransitPaths.acquire();
      return m_TransitPaths.second.size() / 2;
    }

    uint64_t
    PathContext::CurrentOwnedPaths(path::PathStatus st)
    {
      uint64_t num{};
      auto lock = m_OurPaths.acquire();
      auto& map = m_OurPaths.second;
      for (auto itr = map.cbegin(); itr != map.cend(); ++itr)
      {
        if (itr->second->Status() == st)
          num++;
      }
      return num / 2;
    }

    void
    PathContext::PutTransitHop(std::shared_ptr<TransitHop> hop)
    {
      const auto& info = hop->info;
      auto lock = m_TransitPaths.acquire();
      {
        auto& map = m_TransitPaths.second;
        map.emplace(info.txID, hop);
        map.emplace(info.rxID, hop);
      }
    }

    void
    PathContext::ExpirePaths(llarp_time_t now)
    {
      // decay limits
      m_PathLimits.Decay(now);
      std::vector<PathID_t> removed;
      {
        auto lock = m_TransitPaths.acquire();
        auto& map = m_TransitPaths.second;
        auto itr = map.begin();
        while (itr != map.end())
        {
          if (itr->second->Expired(now))
          {
            removed.push_back(itr->first);
            itr = map.erase(itr);
          }
          else
          {
            itr->second->DecayFilters(now);
            ++itr;
          }
        }
      }
      {
        auto lock = m_OurPaths.acquire();
        auto& map = m_OurPaths.second;
        auto itr = map.begin();
        while (itr != map.end())
        {
          if (itr->second->Expired(now))
          {
            removed.push_back(itr->first);
            itr = map.erase(itr);
          }
          else
          {
            itr->second->DecayFilters(now);
            ++itr;
          }
        }
      }
      for (const auto& id : removed)
        m_Router->outboundMessageHandler().RemovePath(id);
    }

    routing::MessageHandler_ptr
    PathContext::GetHandler(const PathID_t& id)
    {
      routing::MessageHandler_ptr h = nullptr;
      auto pathset = GetLocalPathSet(id);
      if (pathset)
      {
        h = pathset->GetPathByID(id);
      }
      if (h)
        return h;
      const RouterID us(OurRouterID());
      auto& map = m_TransitPaths;
      {
        SyncTransitMap_t::Lock_t lock(map.first);
        auto range = map.second.equal_range(id);
        for (auto i = range.first; i != range.second; ++i)
        {
          if (i->second->info.upstream == us)
            return i->second;
        }
      }
      return nullptr;
    }

    void
    PathContext::periodic_tick()
    {
      const auto now = m_Router->Now();
      ExpirePaths(now);
    }

    template <typename Map_t, typename Msg_t>
    std::shared_ptr<IHopHandler>
    get_handler_for(Map_t& map, const Msg_t& msg)
    {
      RouterID from_pk{msg.session->GetPubKey()};
      std::shared_ptr<IHopHandler> handler{nullptr};
      auto lock = map.acquire();
      auto range = map.second.equal_range(msg.pathid);
      for (auto itr = range.first; handler != nullptr and itr != range.second; ++itr)
      {
        const auto hop_info = itr->second->hop_info();
        if (hop_info.downstream == from_pk or hop_info.upstream == from_pk)
          handler = itr->second;
      }
      return handler;
    }

    bool
    PathContext::HandleLRUM(const RelayUpstreamMessage& msg)
    {
      RouterID from_pk{msg.session->GetPubKey()};
      std::shared_ptr<IHopHandler> handler;

      if (m_AllowTransit)
        handler = get_handler_for(m_TransitPaths, msg);
      if (handler != nullptr)
        handler = get_handler_for(m_OurPaths, msg);

      if (not handler)
      {
        log::info(
            logcat,
            "no handler for upstream message from {} for pathid={}",
            msg.session->GetPubKey(),
            msg.Y);
        return false;
      }
      trigger_upstream_flush();
      return true;
    }

    bool
    PathContext::HandleLRDM(const RelayDownstreamMessage& msg)
    {
      RouterID from_pk{msg.session->GetPubKey()};
      std::shared_ptr<IHopHandler> handler;

      if (m_AllowTransit)
        handler = get_handler_for(m_TransitPaths, msg);
      if (handler != nullptr)
        handler = get_handler_for(m_OurPaths, msg);

      if (not handler)
      {
        log::info(
            logcat,
            "no handler for downstream message from {} for pathid={}",
            msg.session->GetPubKey(),
            msg.Y);
        return false;
      }
      trigger_downstream_flush();
      return true;
    }

    void
    PathContext::trigger_upstream_flush()
    {
      m_upstream_flush->Trigger();
    }

    void
    PathContext::trigger_downstream_flush()
    {
      m_downstream_flush->Trigger();
    }
  }  // namespace path
}  // namespace llarp

#pragma once

#include <llarp/constants/path.hpp>
#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/messages/relay.hpp>
#include "abstracthophandler.hpp"
#include "path_types.hpp"
#include "pathbuilder.hpp"
#include "pathset.hpp"
#include <llarp/router_id.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/intro.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace llarp
{
  struct Router;
  struct LR_CommitMessage;

  namespace path
  {
    struct TransitHop;
    struct TransitHopInfo;
    struct PathHopConfig;

    using TransitHop_ptr = std::shared_ptr<TransitHop>;

    /// A path we made
    struct Path final : public AbstractHopHandler,
                        public routing::AbstractRoutingMessageHandler,
                        public std::enable_shared_from_this<Path>
    {
      using BuildResultHookFunc = std::function<void(Path_ptr)>;
      using CheckForDeadFunc = std::function<bool(Path_ptr, llarp_time_t)>;
      using DropHandlerFunc = std::function<bool(Path_ptr, const PathID_t&, uint64_t)>;
      using HopList = std::vector<PathHopConfig>;
      using DataHandlerFunc = std::function<bool(Path_ptr, const service::ProtocolFrameMessage&)>;
      using ExitUpdatedFunc = std::function<bool(Path_ptr)>;
      using ExitClosedFunc = std::function<bool(Path_ptr)>;
      using ExitTrafficHandlerFunc =
          std::function<bool(Path_ptr, const llarp_buffer_t&, uint64_t, service::ProtocolType)>;
      /// (path, backoff) backoff is 0 on success
      using ObtainedExitHandler = std::function<bool(Path_ptr, llarp_time_t)>;

      HopList hops;

      std::weak_ptr<PathSet> m_PathSet;

      service::Introduction intro;

      llarp_time_t buildStarted = 0s;

      Path(
          Router* rtr,
          const std::vector<RouterContact>& routers,
          std::weak_ptr<PathSet> parent,
          PathRole startingRoles,
          std::string shortName);

      util::StatusObject
      ExtractStatus() const;

      PathRole
      Role() const
      {
        return _role;
      }

      struct Hash
      {
        size_t
        operator()(const Path& p) const
        {
          const auto& tx = p.hops[0].txID;
          const auto& rx = p.hops[0].rxID;
          const auto& r = p.hops[0].upstream;
          const size_t rhash = std::accumulate(r.begin(), r.end(), 0, std::bit_xor{});
          return std::accumulate(
              rx.begin(),
              rx.begin(),
              std::accumulate(tx.begin(), tx.end(), rhash, std::bit_xor{}),
              std::bit_xor{});
        }
      };

      /// hash for std::shared_ptr<Path>
      struct Ptr_Hash
      {
        size_t
        operator()(const Path_ptr& p) const
        {
          if (p == nullptr)
            return 0;
          return Hash{}(*p);
        }
      };

      /// hash for std::shared_ptr<Path> by path endpoint
      struct Endpoint_Hash
      {
        size_t
        operator()(const Path_ptr& p) const
        {
          if (p == nullptr)
            return 0;
          return std::hash<RouterID>{}(p->Endpoint());
        }
      };

      /// comparision for equal endpoints
      struct Endpoint_Equals
      {
        bool
        operator()(const Path_ptr& left, const Path_ptr& right) const
        {
          return left && right && left->Endpoint() == left->Endpoint();
        }
      };

      /// unordered set of paths with unique endpoints
      using UniqueEndpointSet_t = std::unordered_set<Path_ptr, Endpoint_Hash, Endpoint_Equals>;

      bool
      operator<(const Path& other) const
      {
        return hops < other.hops;
      }

      void
      MarkActive(llarp_time_t now)
      {
        m_LastRecvMessage = std::max(now, m_LastRecvMessage);
      }

      /// return true if ALL of the specified roles are supported
      bool
      SupportsAllRoles(PathRole roles) const
      {
        return (_role & roles) == roles;
      }

      /// return true if ANY of the specified roles are supported
      bool
      SupportsAnyRoles(PathRole roles) const
      {
        return roles == ePathRoleAny || (_role | roles) != 0;
      }

      /// clear role bits
      void
      ClearRoles(PathRole roles)
      {
        _role &= ~roles;
      }

      PathStatus
      Status() const
      {
        return _status;
      }

      // handle data in upstream direction
      bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router*) override;
      // handle data in downstream direction

      bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, Router*) override;

      const std::string&
      ShortName() const;

      std::string
      HopsString() const;

      llarp_time_t
      LastRemoteActivityAt() const override
      {
        return m_LastRecvMessage;
      }

      bool
      HandleLRSM(uint64_t status, std::array<EncryptedFrame, 8>& frames, Router* r) override;

      void
      SetBuildResultHook(BuildResultHookFunc func);

      void
      SetExitTrafficHandler(ExitTrafficHandlerFunc handler)
      {
        m_ExitTrafficHandler = handler;
      }

      void
      SetCloseExitFunc(ExitClosedFunc handler)
      {
        m_ExitClosed = handler;
      }

      void
      SetUpdateExitFunc(ExitUpdatedFunc handler)
      {
        m_ExitUpdated = handler;
      }

      void
      SetDataHandler(DataHandlerFunc func)
      {
        m_DataHandler = func;
      }

      void
      SetDropHandler(DropHandlerFunc func)
      {
        m_DropHandler = func;
      }

      void
      SetDeadChecker(CheckForDeadFunc func)
      {
        m_CheckForDead = func;
      }

      void
      EnterState(PathStatus st, llarp_time_t now);

      llarp_time_t
      ExpireTime() const
      {
        return buildStarted + hops[0].lifetime;
      }

      bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 5s) const override
      {
        return now >= (ExpireTime() - dlt);
      }

      void
      enable_exit_traffic()
      {
        log::info(path_cat, "{} {} granted exit", name(), Endpoint());
        _role |= ePathRoleExit;
      }

      void
      close_exit()
      {
        log::info(path_cat, "{} hd its exit closed", name());
        _role &= ePathRoleExit;
      }

      bool
      update_exit(uint64_t tx_id);

      bool
      Expired(llarp_time_t now) const override;

      /// build a new path on the same set of hops as us
      /// regenerates keys
      void
      Rebuild();

      void
      Tick(llarp_time_t now, Router* r);

      void
      find_name(std::string name, std::function<void(oxen::quic::message m)> func = nullptr);

      void
      find_router(std::string rid, std::function<void(oxen::quic::message m)> func = nullptr);

      void
      send_path_control_message(
          std::string method,
          std::string body,
          std::function<void(oxen::quic::message m)> func = nullptr) override;

      bool
      SendRoutingMessage(const routing::AbstractRoutingMessage& msg, Router* r) override;

      bool
      HandleObtainExitMessage(const routing::ObtainExitMessage& msg, Router* r) override;

      bool
      HandleUpdateExitVerifyMessage(
          const routing::UpdateExitVerifyMessage& msg, Router* r) override;

      bool
      HandleTransferTrafficMessage(const routing::TransferTrafficMessage& msg, Router* r) override;

      bool
      HandleUpdateExitMessage(const routing::UpdateExitMessage& msg, Router* r) override;

      bool
      HandleCloseExitMessage(const routing::CloseExitMessage& msg, Router* r) override;

      bool
      HandleGrantExitMessage(const routing::GrantExitMessage& msg, Router* r) override;

      bool
      HandleRejectExitMessage(const routing::RejectExitMessage& msg, Router* r) override;

      bool
      HandleDataDiscardMessage(const routing::DataDiscardMessage& msg, Router* r) override;

      bool
      HandlePathConfirmMessage(Router* r);

      bool
      HandlePathConfirmMessage(const routing::PathConfirmMessage& msg, Router* r) override;

      bool
      HandlePathLatencyMessage(const routing::PathLatencyMessage& msg, Router* r) override;

      bool
      HandlePathTransferMessage(const routing::PathTransferMessage& msg, Router* r) override;

      bool
      HandleHiddenServiceFrame(const service::ProtocolFrameMessage& frame) override;

      bool
      HandleGotIntroMessage(const dht::GotIntroMessage& msg);

      bool
      HandleDHTMessage(const dht::AbstractDHTMessage& msg, Router* r) override;

      bool
      HandleRoutingMessage(const llarp_buffer_t& buf, Router* r);

      bool
      IsReady() const;

      // Is this deprecated?
      // nope not deprecated :^DDDD
      PathID_t
      TXID() const;

      RouterID
      Endpoint() const;

      PubKey
      EndpointPubKey() const;

      bool
      is_endpoint(const RouterID& router, const PathID_t& path) const;

      PathID_t
      RXID() const override;

      RouterID
      upstream() const;

      std::string
      name() const;

      void
      AddObtainExitHandler(ObtainedExitHandler handler)
      {
        m_ObtainedExitHooks.push_back(handler);
      }

      bool
      SendExitRequest(const routing::ObtainExitMessage& msg, Router* r);

      bool
      SendExitClose(const routing::CloseExitMessage& msg, Router* r);

      void
      FlushUpstream(Router* r) override;

      void
      FlushDownstream(Router* r) override;

     protected:
      void
      UpstreamWork(TrafficQueue_t queue, Router* r) override;

      void
      DownstreamWork(TrafficQueue_t queue, Router* r) override;

      void
      HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, Router* r) override;

      void
      HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, Router* r) override;

     private:
      bool
      SendLatencyMessage(Router* r);

      /// call obtained exit hooks
      bool
      InformExitResult(llarp_time_t b);

      Router& router;
      BuildResultHookFunc m_BuiltHook;
      DataHandlerFunc m_DataHandler;
      DropHandlerFunc m_DropHandler;
      CheckForDeadFunc m_CheckForDead;
      ExitUpdatedFunc m_ExitUpdated;
      ExitClosedFunc m_ExitClosed;
      ExitTrafficHandlerFunc m_ExitTrafficHandler;
      std::vector<ObtainedExitHandler> m_ObtainedExitHooks;
      llarp_time_t m_LastRecvMessage = 0s;
      llarp_time_t m_LastLatencyTestTime = 0s;
      uint64_t m_LastLatencyTestID = 0;
      uint64_t m_UpdateExitTX = 0;
      uint64_t m_CloseExitTX = 0;
      uint64_t m_ExitObtainTX = 0;
      PathStatus _status;
      PathRole _role;
      uint64_t m_LastRXRate = 0;
      uint64_t m_RXRate = 0;
      uint64_t m_LastTXRate = 0;
      uint64_t m_TXRate = 0;
      std::deque<llarp_time_t> m_LatencySamples;
      const std::string m_shortName;
    };
  }  // namespace path
}  // namespace llarp

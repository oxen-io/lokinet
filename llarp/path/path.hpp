#pragma once

#include <llarp/constants/path.hpp>
#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/messages/relay.hpp>
#include "ihophandler.hpp"
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
  struct AbstractRouter;
  struct LR_CommitMessage;

  namespace path
  {
    struct TransitHop;
    struct TransitHopInfo;

    using TransitHop_ptr = std::shared_ptr<TransitHop>;

    /// configuration for a single hop when building a path
    struct PathHopConfig
    {
      /// path id
      PathID_t txID, rxID;
      // router contact of router
      RouterContact rc;
      // temp public encryption key
      SecretKey commkey;
      /// shared secret at this hop
      SharedSecret shared;
      /// hash of shared secret used for nonce mutation
      ShortHash nonceXOR;
      /// next hop's router id
      RouterID upstream;
      /// nonce for key exchange
      TunnelNonce nonce;
      // lifetime
      llarp_time_t lifetime = default_lifetime;

      util::StatusObject
      ExtractStatus() const;
    };

    inline bool
    operator<(const PathHopConfig& lhs, const PathHopConfig& rhs)
    {
      return std::tie(lhs.txID, lhs.rxID, lhs.rc, lhs.upstream, lhs.lifetime)
          < std::tie(rhs.txID, rhs.rxID, rhs.rc, rhs.upstream, rhs.lifetime);
    }

    /// A path we made
    struct Path final : public IHopHandler,
                        public routing::IMessageHandler,
                        public std::enable_shared_from_this<Path>
    {
      using BuildResultHookFunc = std::function<void(Path_ptr)>;
      using CheckForDeadFunc = std::function<bool(Path_ptr, llarp_time_t)>;
      using DropHandlerFunc = std::function<bool(Path_ptr, const PathID_t&, uint64_t)>;
      using HopList = std::vector<PathHopConfig>;
      using DataHandlerFunc = std::function<bool(Path_ptr, const service::ProtocolFrame&)>;
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
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*) override;
      // handle data in downstream direction

      bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*) override;

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
      HandleLRSM(
          uint64_t status, std::array<EncryptedFrame, 8>& frames, AbstractRouter* r) override;

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

      bool
      Expired(llarp_time_t now) const override;

      /// build a new path on the same set of hops as us
      /// regenerates keys
      void
      Rebuild();

      void
      Tick(llarp_time_t now, AbstractRouter* r);

      bool
      SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r) override;

      bool
      HandleObtainExitMessage(const routing::ObtainExitMessage& msg, AbstractRouter* r) override;

      bool
      HandleUpdateExitVerifyMessage(
          const routing::UpdateExitVerifyMessage& msg, AbstractRouter* r) override;

      bool
      HandleTransferTrafficMessage(
          const routing::TransferTrafficMessage& msg, AbstractRouter* r) override;

      bool
      HandleUpdateExitMessage(const routing::UpdateExitMessage& msg, AbstractRouter* r) override;

      bool
      HandleCloseExitMessage(const routing::CloseExitMessage& msg, AbstractRouter* r) override;
      bool
      HandleGrantExitMessage(const routing::GrantExitMessage& msg, AbstractRouter* r) override;
      bool
      HandleRejectExitMessage(const routing::RejectExitMessage& msg, AbstractRouter* r) override;

      bool
      HandleDataDiscardMessage(const routing::DataDiscardMessage& msg, AbstractRouter* r) override;

      bool
      HandlePathConfirmMessage(AbstractRouter* r);

      bool
      HandlePathConfirmMessage(const routing::PathConfirmMessage& msg, AbstractRouter* r) override;

      bool
      HandlePathLatencyMessage(const routing::PathLatencyMessage& msg, AbstractRouter* r) override;

      bool
      HandlePathTransferMessage(
          const routing::PathTransferMessage& msg, AbstractRouter* r) override;

      bool
      HandleHiddenServiceFrame(const service::ProtocolFrame& frame) override;

      bool
      HandleGotIntroMessage(const dht::GotIntroMessage& msg);

      bool
      HandleDHTMessage(const dht::IMessage& msg, AbstractRouter* r) override;

      bool
      HandleRoutingMessage(const llarp_buffer_t& buf, AbstractRouter* r);

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
      IsEndpoint(const RouterID& router, const PathID_t& path) const;

      PathID_t
      RXID() const override;

      RouterID
      Upstream() const;

      std::string
      Name() const;

      void
      AddObtainExitHandler(ObtainedExitHandler handler)
      {
        m_ObtainedExitHooks.push_back(handler);
      }

      bool
      SendExitRequest(const routing::ObtainExitMessage& msg, AbstractRouter* r);

      bool
      SendExitClose(const routing::CloseExitMessage& msg, AbstractRouter* r);

      void
      FlushUpstream(AbstractRouter* r) override;

      void
      FlushDownstream(AbstractRouter* r) override;

     protected:
      void
      UpstreamWork(TrafficQueue_ptr queue, AbstractRouter* r) override;

      void
      DownstreamWork(TrafficQueue_ptr queue, AbstractRouter* r) override;

      void
      HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, AbstractRouter* r) override;

      void
      HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, AbstractRouter* r) override;

     private:
      bool
      SendLatencyMessage(AbstractRouter* r);

      /// call obtained exit hooks
      bool
      InformExitResult(llarp_time_t b);

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

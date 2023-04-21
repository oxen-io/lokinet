#pragma once
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/exit/session.hpp>
#include <llarp/net/ip_range_map.hpp>
#include <llarp/net/net.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/pathbuilder.hpp>
#include <llarp/util/compare_ptr.hpp>

// --- begin kitchen sink headers ----
#include <llarp/service/address.hpp>
#include <llarp/service/handler.hpp>
#include <llarp/service/identity.hpp>
#include <llarp/service/pendingbuffer.hpp>
#include <llarp/service/protocol.hpp>
#include <llarp/service/sendcontext.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/service/session.hpp>
#include <llarp/service/lookup.hpp>
#include <llarp/service/endpoint_types.hpp>
#include <llarp/endpoint_base.hpp>
#include <llarp/service/auth.hpp>
// ----- end kitchen sink headers -----

#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <oxenc/variant.h>

#include <llarp/vpn/egres_packet_router.hpp>
#include <llarp/dns/server.hpp>
#include "llarp/path/ihophandler.hpp"
#include "llarp/router_id.hpp"
// minimum time between introset shifts
#ifndef MIN_SHIFT_INTERVAL
#define MIN_SHIFT_INTERVAL 5s
#endif

namespace llarp
{
  namespace quic
  {
    class TunnelManager;
  }

  namespace service
  {
    struct AsyncKeyExchange;
    struct Context;
    struct EndpointState;
    struct OutboundContext;

    /// minimum interval for publishing introsets
    inline constexpr auto IntrosetPublishInterval = path::intro_path_spread / 2;

    /// how agressively should we retry publishing introset on failure
    inline constexpr auto IntrosetPublishRetryCooldown = 1s;

    /// how aggressively should we retry looking up introsets
    inline constexpr auto IntrosetLookupCooldown = 250ms;

    /// number of unique snodes we want to talk to do to ons lookups
    inline constexpr size_t MIN_ENDPOINTS_FOR_LNS_LOOKUP = 2;

    struct Endpoint : public path::Builder,
                      public ILookupHolder,
                      public IDataHandler,
                      public EndpointBase
    {
      explicit Endpoint(AbstractRouter& r);
      ~Endpoint() override;

      /// return true if we are ready to recv packets from the void.
      /// really should be ReadyForInboundTraffic() but the diff is HUGE and we need to rewrite this
      /// component anyways.
      bool
      IsReady() const;

      void
      QueueRecvData(RecvDataEvent ev) override;

      /// return true if our introset has expired intros
      bool
      IntrosetIsStale() const;

      /// construct parameters for notify hooks
      virtual std::unordered_map<std::string, std::string>
      NotifyParams() const;

      virtual util::StatusObject
      ExtractStatus() const;

      void
      SetHandler(IDataHandler* h);

      bool
      Configure(const NetworkConfig& conf, const DnsConfig&) override;

      std::string_view
      endpoint_name() const override;

      void
      Tick(llarp_time_t now) override;

      /// return true if we have a resolvable ip address
      virtual bool
      HasIfAddr() const
      {
        return false;
      }

      std::optional<ConvoTag>
      GetBestConvoTagFor(std::variant<Address, RouterID> addr) const override;

      /// get our ifaddr if it is set
      virtual huint128_t
      GetIfAddr() const
      {
        return {0};
      }

      /// get the exit policy for our exit if we have one
      /// override me
      virtual std::optional<net::TrafficPolicy>
      GetExitPolicy() const
      {
        return std::nullopt;
      };

      /// get the ip ranges we claim to own
      /// override me
      virtual std::set<IPRange>
      GetOwnedRanges() const
      {
        return {};
      };

      virtual void
      Thaw(){};

      void
      ResetInternalState() override;

      /// loop (via router)
      /// use when sending any data on a path
      const EventLoop_ptr&
      Loop() override;

      AbstractRouter*
      Router();

      virtual bool
      LoadKeyFile();

      virtual bool
      Start();

      std::string
      Name() const override;

      AddressVariant_t
      LocalAddress() const override;

      std::optional<SendStat>
      GetStatFor(AddressVariant_t remote) const override;

      std::unordered_set<AddressVariant_t>
      AllRemoteEndpoints() const override;

      bool
      ShouldPublishDescriptors(llarp_time_t now) const override;

      void
      SRVRecordsChanged() override;

      void
      HandlePathDied(path::Path_ptr p) override;

      virtual vpn::EgresPacketRouter*
      EgresPacketRouter()
      {
        return nullptr;
      };

      virtual vpn::NetworkInterface*
      GetVPNInterface()
      {
        return nullptr;
      }

      void
      SendPacketToRemote(const llarp_buffer_t&, ProtocolType) override{};

      bool
      PublishIntroSet(const EncryptedIntroSet& i, AbstractRouter* r) override;

      bool
      PublishIntroSetVia(
          const EncryptedIntroSet& i, AbstractRouter* r, path::Path_ptr p, uint64_t relayOrder);

      bool
      HandleGotIntroMessage(std::shared_ptr<const dht::GotIntroMessage> msg) override;

      bool
      HandleGotRouterMessage(std::shared_ptr<const dht::GotRouterMessage> msg) override;

      bool
      HandleGotNameMessage(std::shared_ptr<const dht::GotNameMessage> msg) override;

      bool
      HandleHiddenServiceFrame(path::Path_ptr p, const service::ProtocolFrame& msg);

      void
      SetEndpointAuth(std::shared_ptr<IAuthPolicy> policy);

      /// sets how we authenticate with remote address
      void
      SetAuthInfoForEndpoint(Address remote, AuthInfo info);

      // virtual bool
      // HasServiceAddress(const AlignedBuffer< 32 >& addr) const = 0;

      /// return true if we have a pending job to build to a hidden service but
      /// it's not done yet
      bool
      HasPendingPathToService(const Address& remote) const;

      bool
      HandleDataMessage(
          path::Path_ptr path, const PathID_t from, std::shared_ptr<ProtocolMessage> msg) override;

      // virtual bool
      // HandleWriteIPPacket(const llarp_buffer_t& pkt,
      //                    std::function< huint128_t(void) > getFromIP) = 0;

      bool
      ProcessDataMessage(std::shared_ptr<ProtocolMessage> msg);

      /// ensure that we know a router, looks up if it doesn't
      void
      EnsureRouterIsKnown(const RouterID& router);

      /// lookup a router via closest path
      bool
      LookupRC(RouterID router, RouterLookupHandler handler) override;

      void
      LookupNameAsync(
          std::string name,
          std::function<void(std::optional<std::variant<Address, RouterID>>)> resultHandler)
          override;

      void
      LookupServiceAsync(
          std::string name,
          std::string service,
          std::function<void(std::vector<dns::SRVData>)> resultHandler) override;

      /// called on event loop pump
      virtual void
      Pump(llarp_time_t now);

      /// stop this endpoint
      bool
      Stop() override;

      const Identity&
      GetIdentity() const
      {
        return m_Identity;
      }

      void
      MapExitRange(IPRange range, service::Address exit);

      void
      UnmapExitRange(IPRange range);

      void
      UnmapRangeByExit(IPRange range, std::string exit);

      void
      map_exit(
          std::string name,
          std::string token,
          std::vector<IPRange> ranges,
          std::function<void(bool, std::string)> result);

      void
      PutLookup(IServiceLookup* lookup, uint64_t txid) override;

      void
      HandlePathBuilt(path::Path_ptr path) override;

      bool
      HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t s);

      bool
      CheckPathIsDead(path::Path_ptr p, llarp_time_t latency);

      using PendingBufferQueue = std::deque<PendingBuffer>;

      size_t
      RemoveAllConvoTagsFor(service::Address remote);

      bool
      WantsOutboundSession(const Address&) const override;

      /// this MUST be called if you want to call EnsurePathTo on the given address
      void MarkAddressOutbound(AddressVariant_t) override;

      bool
      ShouldBundleRC() const override
      {
        return false;
      }

      void
      BlacklistSNode(const RouterID snode) override;

      /// maybe get an endpoint variant given its convo tag
      std::optional<std::variant<Address, RouterID>>
      GetEndpointWithConvoTag(ConvoTag t) const override;

      bool
      HasConvoTag(const ConvoTag& t) const override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      constexpr llarp_time_t
      PathAlignmentTimeout() const
      {
        constexpr auto DefaultPathAlignmentTimeout = 10s;
        return DefaultPathAlignmentTimeout;
      }

      bool
      EnsurePathTo(
          std::variant<Address, RouterID> addr,
          std::function<void(std::optional<ConvoTag>)> hook,
          llarp_time_t timeout) override;

      // passed a sendto context when we have a path established otherwise
      // nullptr if the path was not made before the timeout
      using PathEnsureHook = std::function<void(Address, OutboundContext*)>;

      static constexpr auto DefaultPathEnsureTimeout = 2s;

      /// return false if we have already called this function before for this
      /// address
      bool
      EnsurePathToService(
          const Address remote,
          PathEnsureHook h,
          llarp_time_t timeoutMS = DefaultPathEnsureTimeout);

      using SNodeEnsureHook = std::function<void(const RouterID, exit::BaseSession_ptr, ConvoTag)>;

      void
      InformPathToService(const Address remote, OutboundContext* ctx);

      /// ensure a path to a service node by public key
      bool
      EnsurePathToSNode(const RouterID remote, SNodeEnsureHook h);

      /// return true if this endpoint is trying to lookup this router right now
      bool
      HasPendingRouterLookup(const RouterID remote) const;

      bool
      HasPathToSNode(const RouterID remote) const;

      bool
      HasFlowToService(const Address remote) const;

      void
      PutSenderFor(const ConvoTag& tag, const ServiceInfo& info, bool inbound) override;

      bool
      HasInboundConvo(const Address& addr) const override;

      bool
      HasOutboundConvo(const Address& addr) const override;

      bool
      GetCachedSessionKeyFor(const ConvoTag& remote, SharedSecret& secret) const override;
      void
      PutCachedSessionKeyFor(const ConvoTag& remote, const SharedSecret& secret) override;

      bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const override;

      void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro) override;

      bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const override;

      void
      RemoveConvoTag(const ConvoTag& remote) override;

      void
      ConvoTagTX(const ConvoTag& remote) override;

      void
      ConvoTagRX(const ConvoTag& remote) override;

      void
      PutReplyIntroFor(const ConvoTag& remote, const Introduction& intro) override;

      bool
      GetReplyIntroFor(const ConvoTag& remote, Introduction& intro) const override;

      bool
      GetConvoTagsForService(const Address& si, std::set<ConvoTag>& tag) const override;

      void
      PutNewOutboundContext(const IntroSet& introset, llarp_time_t timeLeftToAlign);

      std::optional<uint64_t>
      GetSeqNoForConvo(const ConvoTag& tag);

      /// count unique endpoints we are talking to
      size_t
      UniqueEndpoints() const;

      bool
      HasExit() const;

      std::optional<std::vector<RouterContact>>
      GetHopsForBuild() override;

      std::optional<std::vector<RouterContact>>
      GetHopsForBuildWithEndpoint(RouterID endpoint);

      virtual void
      PathBuildStarted(path::Path_ptr path) override;

      virtual void
      IntroSetPublishFail();
      virtual void
      IntroSetPublished();

      void
      AsyncProcessAuthMessage(
          std::shared_ptr<ProtocolMessage> msg, std::function<void(AuthResult)> hook);

      void
      SendAuthResult(path::Path_ptr path, PathID_t replyPath, ConvoTag tag, AuthResult st);

      uint64_t
      GenTXID();

      void
      ResetConvoTag(ConvoTag tag, path::Path_ptr path, PathID_t from);

      const std::set<RouterID>&
      SnodeBlacklist() const;

      // Looks up the ConvoTag and, if it exists, calls SendToOrQueue to send it to a remote client
      // or a snode (or nothing, if the convo tag is unknown).
      bool
      SendToOrQueue(ConvoTag tag, std::vector<byte_t> data, ProtocolType t) override;

      // Send a to (or queues for sending) to either an address or router id
      bool
      send_to_or_queue(
          const std::variant<Address, RouterID>& addr, std::vector<byte_t>&& data, ProtocolType t);

      // Sends to (or queues for sending) to a remote client
      bool
      send_to_loki(const Address& addr, std::vector<byte_t>&& payload, ProtocolType t);

      // Sends to (or queues for sending) to a router
      bool
      send_to_snode(const RouterID& addr, std::vector<byte_t>&& payload, ProtocolType t);

      std::optional<AuthInfo>
      MaybeGetAuthInfoForEndpoint(service::Address addr);

      /// Returns a pointer to the quic::Tunnel object handling quic connections for this endpoint.
      /// Returns nullptr if quic is not supported.
      quic::TunnelManager*
      GetQUICTunnel() override;

     protected:
      void
      RegenAndPublishIntroSet();

      IServiceLookup*
      GenerateLookupByTag(const Tag& tag);

      void
      PrefetchServicesByTag(const Tag& tag);

     private:
      void
      HandleVerifyGotRouter(dht::GotRouterMessage_constptr msg, RouterID id, bool valid);

      bool
      OnLookup(
          const service::Address& addr,
          std::optional<IntroSet> i,
          const RouterID& endpoint,
          llarp_time_t timeLeft,
          uint64_t relayOrder);

      bool
      DoNetworkIsolation(bool failed);

      virtual bool
      SetupNetworking()
      {
        // XXX: override me
        return true;
      }

      virtual bool
      IsolationFailed()
      {
        // XXX: override me
        return false;
      }

      /// return true if we are ready to do outbound and inbound traffic
      bool
      ReadyForNetwork() const;

     protected:
      bool
      ReadyToDoLookup(size_t num_paths) const;
      path::Path::UniqueEndpointSet_t
      GetUniqueEndpointsForLookup() const;

      IDataHandler* m_DataHandler = nullptr;
      Identity m_Identity;
      net::IPRangeMap<service::Address> m_ExitMap;
      bool m_PublishIntroSet = true;
      std::unique_ptr<EndpointState> m_state;
      std::shared_ptr<IAuthPolicy> m_AuthPolicy;
      std::unordered_map<Address, AuthInfo> m_RemoteAuthInfos;
      std::unique_ptr<quic::TunnelManager> m_quic;

      /// (lns name, optional exit range, optional auth info) for looking up on startup
      std::unordered_map<std::string, std::pair<std::optional<IPRange>, std::optional<AuthInfo>>>
          m_StartupLNSMappings;

      RecvPacketQueue_t m_InboundTrafficQueue;

     public:
      SendMessageQueue_t m_SendQueue;

     private:
      llarp_time_t m_LastIntrosetRegenAttempt = 0s;

     protected:
      void
      FlushRecvData();

      friend struct EndpointUtil;

      // clang-format off
      const IntroSet& introSet() const;
      IntroSet&       introSet();

      using ConvoMap = std::unordered_map<ConvoTag, Session>;
      const ConvoMap& Sessions() const;
      ConvoMap&       Sessions();
      // clang-format on
      thread::Queue<RecvDataEvent> m_RecvQueue;

      /// for rate limiting introset lookups
      util::DecayingHashSet<Address> m_IntrosetLookupFilter;
    };

    using Endpoint_ptr = std::shared_ptr<Endpoint>;

  }  // namespace service
}  // namespace llarp

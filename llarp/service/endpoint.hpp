#pragma once
#include <llarp/endpoint_base.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/exit/session.hpp>
#include <llarp/net/ip_range_map.hpp>
#include <llarp/net/net.hpp>
#include <llarp/path/pathbuilder.hpp>
#include <llarp/util/compare_ptr.hpp>

// --- begin kitchen sink headers ----
#include <llarp/service/address.hpp>
#include <llarp/service/identity.hpp>
#include <llarp/service/pendingbuffer.hpp>
#include <llarp/service/protocol.hpp>
#include <llarp/service/sendcontext.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/service/session.hpp>
#include <llarp/service/endpoint_types.hpp>
#include <llarp/endpoint_base.hpp>
#include <llarp/service/auth.hpp>
// ----- end kitchen sink headers -----

#include <optional>
#include <unordered_map>
#include <variant>
#include <oxenc/variant.h>

#include <llarp/vpn/egres_packet_router.hpp>
#include <llarp/dns/server.hpp>

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
    inline constexpr auto IntrosetPublishInterval = path::INTRO_PATH_SPREAD / 2;

    /// how agressively should we retry publishing introset on failure
    inline constexpr auto IntrosetPublishRetryCooldown = 1s;

    /// how aggressively should we retry looking up introsets
    inline constexpr auto IntrosetLookupCooldown = 250ms;

    /// number of unique snodes we want to talk to do to ons lookups
    inline constexpr size_t MIN_ONS_LOOKUP_ENDPOINTS = 2;

    inline constexpr size_t MAX_ONS_LOOKUP_ENDPOINTS = 7;

    // TODO: delete this, it is copied from the late llarp/service/handler.hpp
    struct RecvDataEvent
    {
      path::Path_ptr fromPath;
      PathID_t pathid;
      std::shared_ptr<ProtocolMessage> msg;
    };

    struct Endpoint : public path::Builder,
                      public EndpointBase,
                      public std::enable_shared_from_this<Endpoint>
    {
      Endpoint(Router* r, Context* parent);
      ~Endpoint() override;

      /// return true if we are ready to recv packets from the void.
      /// really should be ReadyForInboundTraffic() but the diff is HUGE and we need to rewrite this
      /// component anyways.
      bool
      is_ready() const;

      void
      QueueRecvData(RecvDataEvent ev);

      /// return true if our introset has expired intros
      bool
      IntrosetIsStale() const;

      /// construct parameters for notify hooks
      virtual std::unordered_map<std::string, std::string>
      NotifyParams() const;

      virtual util::StatusObject
      ExtractStatus() const;

      virtual bool
      Configure(const NetworkConfig& conf, const DnsConfig& dnsConf);

      void
      Tick(llarp_time_t now) override;

      /// return true if we have a resolvable ip address
      virtual bool
      HasIfAddr() const
      {
        return false;
      }

      virtual std::string
      GetIfName() const = 0;

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

      Router*
      router();

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

      bool
      publish_introset(const EncryptedIntroSet& i);

      bool
      HandleHiddenServiceFrame(path::Path_ptr p, const service::ProtocolFrameMessage& msg);

      void
      SetEndpointAuth(std::shared_ptr<IAuthPolicy> policy);

      /// sets how we authenticate with remote address
      void
      SetAuthInfoForEndpoint(Address remote, AuthInfo info);

      virtual huint128_t ObtainIPForAddr(std::variant<Address, RouterID>) = 0;

      /// get a key for ip address
      virtual std::optional<std::variant<service::Address, RouterID>>
      ObtainAddrForIP(huint128_t ip) const = 0;

      // virtual bool
      // HasServiceAddress(const AlignedBuffer< 32 >& addr) const = 0;

      /// return true if we have a pending job to build to a hidden service but
      /// it's not done yet
      bool
      HasPendingPathToService(const Address& remote) const;

      bool
      HandleDataMessage(
          path::Path_ptr path, const PathID_t from, std::shared_ptr<ProtocolMessage> msg);

      /// handle packet io from service node or hidden service to frontend
      virtual bool
      HandleInboundPacket(
          const ConvoTag tag, const llarp_buffer_t& pkt, ProtocolType t, uint64_t seqno) = 0;

      // virtual bool
      // HandleWriteIPPacket(const llarp_buffer_t& pkt,
      //                    std::function< huint128_t(void) > getFromIP) = 0;

      bool
      ProcessDataMessage(std::shared_ptr<ProtocolMessage> msg);

      /// ensure that we know a router, looks up if it doesn't
      void
      EnsureRouterIsKnown(const RouterID& router);

      // "find router" via closest path
      bool
      lookup_router(RouterID router, std::function<void(oxen::quic::message)> func = nullptr);

      // "find name"
      void
      lookup_name(
          std::string name, std::function<void(oxen::quic::message)> func = nullptr) override;

      // "find introset?"
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
        return _identity;
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
      HandlePathBuilt(path::Path_ptr path) override;

      bool
      HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t s);

      bool
      CheckPathIsDead(path::Path_ptr p, llarp_time_t latency);

      using PendingBufferQueue = std::deque<PendingBuffer>;

      size_t
      RemoveAllConvoTagsFor(service::Address remote);

      bool
      WantsOutboundSession(const Address&) const;

      /// this MUST be called if you want to call EnsurePathTo on the given address
      void MarkAddressOutbound(service::Address) override;

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
      HasConvoTag(const ConvoTag& t) const;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      virtual llarp_time_t
      PathAlignmentTimeout() const
      {
        constexpr auto DefaultPathAlignmentTimeout = 30s;
        return DefaultPathAlignmentTimeout;
      }

      bool
      EnsurePathTo(
          std::variant<Address, RouterID> addr,
          std::function<void(std::optional<ConvoTag>)> hook,
          llarp_time_t timeout) override;

      static constexpr auto DefaultPathEnsureTimeout = 2s;

      /// return false if we have already called this function before for this
      /// address
      bool
      EnsurePathToService(
          const Address remote,
          std::function<void(Address, OutboundContext*)> h,
          llarp_time_t timeoutMS = DefaultPathEnsureTimeout);

      void
      InformPathToService(const Address remote, OutboundContext* ctx);

      /// ensure a path to a service node by public key
      bool
      EnsurePathToSNode(
          const RouterID remote,
          std::function<void(const RouterID, exit::BaseSession_ptr, ConvoTag)> h);

      /// return true if this endpoint is trying to lookup this router right now
      bool
      HasPendingRouterLookup(const RouterID remote) const;

      bool
      HasPathToSNode(const RouterID remote) const;

      bool
      HasFlowToService(const Address remote) const;

      void
      PutSenderFor(const ConvoTag& tag, const ServiceInfo& info, bool inbound);

      bool
      HasInboundConvo(const Address& addr) const;

      bool
      HasOutboundConvo(const Address& addr) const;

      bool
      GetCachedSessionKeyFor(const ConvoTag& remote, SharedSecret& secret) const;

      void
      PutCachedSessionKeyFor(const ConvoTag& remote, const SharedSecret& secret);

      bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const;

      void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro);

      bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const;

      void
      RemoveConvoTag(const ConvoTag& remote);

      void
      ConvoTagTX(const ConvoTag& remote);

      void
      ConvoTagRX(const ConvoTag& remote);

      void
      PutReplyIntroFor(const ConvoTag& remote, const Introduction& intro);

      bool
      GetReplyIntroFor(const ConvoTag& remote, Introduction& intro) const;

      bool
      GetConvoTagsForService(const Address& si, std::set<ConvoTag>& tag) const;

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

      void
      PathBuildStarted(path::Path_ptr path) override;

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
      send_to(ConvoTag tag, std::string payload) override;

      std::optional<AuthInfo>
      MaybeGetAuthInfoForEndpoint(service::Address addr);

      /// Returns a pointer to the quic::Tunnel object handling quic connections for this endpoint.
      /// Returns nullptr if quic is not supported.
      quic::TunnelManager*
      GetQUICTunnel() override;

     protected:
      /// parent context that owns this endpoint
      Context* const context;

      virtual bool
      SupportsV6() const = 0;

      void
      regen_and_publish_introset();

      IServiceLookup*
      GenerateLookupByTag(const Tag& tag);

      void
      PrefetchServicesByTag(const Tag& tag);

     private:
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

      auto
      GetUniqueEndpointsForLookup() const;

      Identity _identity;
      net::IPRangeMap<service::Address> _exit_map;
      bool _publish_introset = true;
      std::unique_ptr<EndpointState> _state;
      std::shared_ptr<IAuthPolicy> _auth_policy;
      std::unordered_map<Address, AuthInfo> _remote_auth_infos;
      std::unique_ptr<quic::TunnelManager> _tunnel_manager;

      /// (ons name, optional exit range, optional auth info) for looking up on startup
      std::unordered_map<std::string, std::pair<std::optional<IPRange>, std::optional<AuthInfo>>>
          _startup_ons_mappings;

      RecvPacketQueue_t _inbound_queue;

     public:
      SendMessageEventQueue _send_queue;

     private:
      llarp_time_t _last_introset_regen_attempt = 0s;

     protected:
      void
      FlushRecvData();

      friend struct EndpointUtil;

      const IntroSet&
      intro_set() const;
      IntroSet&
      intro_set();

      const std::unordered_map<ConvoTag, Session>&
      Sessions() const;
      std::unordered_map<ConvoTag, Session>&
      Sessions();

      thread::Queue<RecvDataEvent> _recv_event_queue;

      /// for rate limiting introset lookups
      util::DecayingHashSet<Address> _introset_lookup_filter;
    };

    using Endpoint_ptr = std::shared_ptr<Endpoint>;

  }  // namespace service
}  // namespace llarp

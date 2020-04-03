#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP
#include <llarp.h>
#include <dht/messages/gotrouter.hpp>
#include <ev/ev.h>
#include <exit/session.hpp>
#include <net/ip_range_map.hpp>
#include <net/net.hpp>
#include <path/path.hpp>
#include <path/pathbuilder.hpp>
#include <service/address.hpp>
#include <service/handler.hpp>
#include <service/identity.hpp>
#include <service/pendingbuffer.hpp>
#include <service/protocol.hpp>
#include <service/sendcontext.hpp>
#include <service/session.hpp>
#include <service/tag_lookup_job.hpp>
#include <hook/ihook.hpp>
#include <util/compare_ptr.hpp>
#include <util/thread/logic.hpp>

// minimum time between introset shifts
#ifndef MIN_SHIFT_INTERVAL
#define MIN_SHIFT_INTERVAL 5s
#endif

struct llarp_async_verify_rc;

namespace llarp
{
  namespace service
  {
    struct AsyncKeyExchange;
    struct Context;
    struct EndpointState;
    struct OutboundContext;

    struct IConvoEventListener
    {
      ~IConvoEventListener() = default;

      /// called when we have obtained the introset
      /// called with nullptr on not found or when we
      /// talking to a snode
      virtual void
      FoundIntroSet(const IntroSet*) = 0;

      /// called when we found the RC we need for alignment
      virtual void
      FoundRC(const RouterContact) = 0;

      /// called when we have successfully built an aligned path
      virtual void GotAlignedPath(path::Path_ptr) = 0;

      /// called when we have established a session or conversation
      virtual void
      MadeConvo(const ConvoTag) = 0;
    };
    using ConvoEventListener_ptr = std::shared_ptr<IConvoEventListener>;

    /// minimum interval for publishing introsets
    static constexpr auto INTROSET_PUBLISH_INTERVAL =
        std::chrono::milliseconds(path::default_lifetime) / 4;

    static constexpr auto INTROSET_PUBLISH_RETRY_INTERVAL = 5s;

    struct Endpoint : public path::Builder, public ILookupHolder, public IDataHandler
    {
      static const size_t MAX_OUTBOUND_CONTEXT_COUNT = 4;

      Endpoint(const std::string& nickname, AbstractRouter* r, Context* parent);
      ~Endpoint() override;

      /// return true if we are ready to recv packets from the void
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

      util::StatusObject
      ExtractStatus() const;

      void
      SetHandler(IDataHandler* h);

      virtual bool
      SetOption(const std::string& k, const std::string& v);

      void
      Tick(llarp_time_t now) override;

      /// return true if we have a resolvable ip address
      virtual bool
      HasIfAddr() const
      {
        return false;
      }

      /// inject vpn io
      /// return false if not supported
      virtual bool
      InjectVPN(llarp_vpn_io*, llarp_vpn_ifaddr_info)
      {
        return false;
      }

      /// get our ifaddr if it is set
      virtual huint128_t
      GetIfAddr() const
      {
        return {0};
      }

      void
      ResetInternalState() override;

      /// router's logic
      /// use when sending any data on a path
      std::shared_ptr<Logic>
      RouterLogic();

      /// endpoint's logic
      /// use when writing any data to local network interfaces
      std::shared_ptr<Logic>
      EndpointLogic();

      /// borrow endpoint's net loop for sending data to user on local network
      /// interface
      llarp_ev_loop_ptr
      EndpointNetLoop();

      /// crypto worker threadpool
      std::shared_ptr<llarp::thread::ThreadPool>
      CryptoWorker();

      AbstractRouter*
      Router();

      virtual bool
      LoadKeyFile();

      virtual bool
      Start();

      std::string
      Name() const override;

      /// get a set of all the routers we use as exit node
      std::set<RouterID>
      GetExitRouters() const;

      bool
      ShouldPublishDescriptors(llarp_time_t now) const override;

      void
      HandlePathDied(path::Path_ptr p) override;

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
      HandleHiddenServiceFrame(path::Path_ptr p, const service::ProtocolFrame& msg);

      // virtual huint128_t
      // ObtainIPForAddr(const AlignedBuffer< 32 >& addr, bool serviceNode) = 0;

      // virtual bool
      // HasServiceAddress(const AlignedBuffer< 32 >& addr) const = 0;

      /// return true if we have a pending job to build to a hidden service but
      /// it's not done yet
      bool
      HasPendingPathToService(const Address& remote) const;

      bool
      HandleDataMessage(
          path::Path_ptr path, const PathID_t from, std::shared_ptr<ProtocolMessage> msg) override;

      /// handle packet io from service node or hidden service to frontend
      virtual bool
      HandleInboundPacket(const ConvoTag tag, const llarp_buffer_t& pkt, ProtocolType t) = 0;

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
      LookupRouterAnon(RouterID router, RouterLookupHandler handler);

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
      PutLookup(IServiceLookup* lookup, uint64_t txid) override;

      void
      HandlePathBuilt(path::Path_ptr path) override;

      bool
      EnsureConvo(const AlignedBuffer<32> addr, bool snode, ConvoEventListener_ptr ev);

      bool
      SendTo(const ConvoTag tag, const llarp_buffer_t& pkt, ProtocolType t);

      bool
      HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t s);

      bool
      CheckPathIsDead(path::Path_ptr p, llarp_time_t latency);

      using PendingBufferQueue = std::deque<PendingBuffer>;

      bool
      WantsOutboundSession(const Address&) const override;

      void
      MarkAddressOutbound(const Address&) override;

      bool
      ShouldBundleRC() const override;

      /// return true if we have a convotag as an exit session
      /// or as a hidden service session
      /// set addr and issnode
      ///
      /// return false if we don't have either
      bool
      GetEndpointWithConvoTag(const ConvoTag t, AlignedBuffer<32>& addr, bool& issnode) const;

      bool
      HasConvoTag(const ConvoTag& t) const override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      // passed a sendto context when we have a path established otherwise
      // nullptr if the path was not made before the timeout
      using PathEnsureHook = std::function<void(Address, OutboundContext*)>;

      /// return false if we have already called this function before for this
      /// address
      bool
      EnsurePathToService(const Address remote, PathEnsureHook h, llarp_time_t timeoutMS);

      using SNodeEnsureHook = std::function<void(const RouterID, exit::BaseSession_ptr)>;

      /// ensure a path to a service node by public key
      bool
      EnsurePathToSNode(const RouterID remote, SNodeEnsureHook h);

      /// return true if this endpoint is trying to lookup this router right now
      bool
      HasPendingRouterLookup(const RouterID remote) const;

      bool
      HasPathToSNode(const RouterID remote) const;

      void
      PutSenderFor(const ConvoTag& tag, const ServiceInfo& info, bool inbound) override;

      bool
      HasInboundConvo(const Address& addr) const override;

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
      MarkConvoTagActive(const ConvoTag& remote) override;

      void
      PutReplyIntroFor(const ConvoTag& remote, const Introduction& intro) override;

      bool
      GetReplyIntroFor(const ConvoTag& remote, Introduction& intro) const override;

      bool
      GetConvoTagsForService(const Address& si, std::set<ConvoTag>& tag) const override;

      void
      PutNewOutboundContext(const IntroSet& introset);

      uint64_t
      GetSeqNoForConvo(const ConvoTag& tag);

      bool
      SelectHop(
          llarp_nodedb* db,
          const std::set<RouterID>& prev,
          RouterContact& cur,
          size_t hop,
          path::PathRole roles) override;

      virtual void
      PathBuildStarted(path::Path_ptr path) override;

      virtual void
      IntroSetPublishFail();
      virtual void
      IntroSetPublished();

      uint64_t
      GenTXID();

      const std::set<RouterID>&
      SnodeBlacklist() const;

     protected:
      bool
      SendToServiceOrQueue(
          const service::Address& addr, const llarp_buffer_t& payload, ProtocolType t);
      bool
      SendToSNodeOrQueue(const RouterID& addr, const llarp_buffer_t& payload);

      /// parent context that owns this endpoint
      Context* const context;

      virtual bool
      SupportsV6() const = 0;

      void
      RegenAndPublishIntroSet(bool forceRebuild = false);

      IServiceLookup*
      GenerateLookupByTag(const Tag& tag);

      void
      PrefetchServicesByTag(const Tag& tag);

      /// spawn a new process that contains a network isolated process
      /// return true if we set up isolation and the event loop is up
      /// otherwise return false
      virtual bool
      SpawnIsolatedNetwork()
      {
        return false;
      }

      bool
      NetworkIsIsolated() const;

      /// this runs in the isolated network process
      void
      IsolatedNetworkMainLoop();

     private:
      void
      HandleVerifyGotRouter(dht::GotRouterMessage_constptr msg, llarp_async_verify_rc* j);

      bool
      OnLookup(
          const service::Address& addr,
          nonstd::optional<IntroSet> i,
          const RouterID& endpoint); /*  */

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

     protected:
      IDataHandler* m_DataHandler = nullptr;
      Identity m_Identity;
      net::IPRangeMap<exit::BaseSession_ptr> m_ExitMap;
      hooks::Backend_ptr m_OnUp;
      hooks::Backend_ptr m_OnDown;
      hooks::Backend_ptr m_OnReady;
      bool m_PublishIntroSet = true;

     private:
      void
      FlushRecvData();

      friend struct EndpointUtil;

      // clang-format off
      const IntroSet& introSet() const;
      IntroSet&       introSet();

      using ConvoMap = std::unordered_map< ConvoTag, Session, ConvoTag::Hash >;
      const ConvoMap& Sessions() const;
      ConvoMap&       Sessions();
      // clang-format on

      std::unique_ptr<EndpointState> m_state;
      thread::Queue<RecvDataEvent> m_RecvQueue;
    };

    using Endpoint_ptr = std::shared_ptr<Endpoint>;

  }  // namespace service
}  // namespace llarp

#endif

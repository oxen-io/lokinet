#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP

#include <dht/messages/gotrouter.hpp>
#include <ev/ev.h>
#include <exit/session.hpp>
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
#include <util/logic.hpp>

// minimum time between introset shifts
#ifndef MIN_SHIFT_INTERVAL
#define MIN_SHIFT_INTERVAL (5 * 1000)
#endif

struct llarp_async_verify_rc;

namespace llarp
{
  namespace service
  {
    struct AsyncKeyExchange;
    struct Context;
    struct OutboundContext;

    struct Endpoint : public path::Builder,
                      public ILookupHolder,
                      public IDataHandler
    {
      /// minimum interval for publishing introsets
      static const llarp_time_t INTROSET_PUBLISH_INTERVAL =
          path::default_lifetime / 8;

      static const llarp_time_t INTROSET_PUBLISH_RETRY_INTERVAL = 5000;

      static const size_t MAX_OUTBOUND_CONTEXT_COUNT = 4;

      Endpoint(const std::string& nickname, AbstractRouter* r, Context* parent);
      ~Endpoint();

      /// return true if we are ready to recv packets from the void
      bool
      IsReady() const;

      /// return true if our introset has expired intros
      bool
      IntrosetIsStale() const;

      /// construct parameters for notify hooks
      virtual std::unordered_map< std::string, std::string >
      NotifyParams() const;

      util::StatusObject
      ExtractStatus() const;

      void
      SetHandler(IDataHandler* h);

      virtual bool
      SetOption(const std::string& k, const std::string& v);

      virtual void
      Tick(llarp_time_t now) override;

      /// return true if we have a resolvable ip address
      virtual bool
      HasIfAddr() const
      {
        return false;
      }

      /// get our ifaddr if it is set
      virtual huint32_t
      GetIfAddr() const
      {
        return huint32_t{0};
      }

      virtual void
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

      Crypto*
      crypto();

      /// crypto worker threadpool
      llarp_threadpool*
      CryptoWorker();

      AbstractRouter*
      Router()
      {
        return m_Router;
      }

      virtual bool
      LoadKeyFile();

      virtual bool
      Start();

      virtual std::string
      Name() const override;

      bool
      ShouldPublishDescriptors(llarp_time_t now) const override;

      void
      HandlePathDied(path::Path_ptr p) override;

      void
      EnsureReplyPath(const ServiceInfo& addr);

      bool
      PublishIntroSet(AbstractRouter* r) override;

      bool
      PublishIntroSetVia(AbstractRouter* r, path::Path_ptr p);

      bool
      HandleGotIntroMessage(
          std::shared_ptr< const dht::GotIntroMessage > msg) override;

      bool
      HandleGotRouterMessage(
          std::shared_ptr< const dht::GotRouterMessage > msg) override;

      bool
      HandleHiddenServiceFrame(path::Path_ptr p,
                               const service::ProtocolFrame& msg);

      virtual huint32_t
      ObtainIPForAddr(const AlignedBuffer< 32 >& addr, bool serviceNode) = 0;

      virtual bool
      HasAddress(const AlignedBuffer< 32 >& addr) const = 0;

      /// return true if we have a pending job to build to a hidden service but
      /// it's not done yet
      bool
      HasPendingPathToService(const Address& remote) const;

      /// return false if we don't have a path to the service
      /// return true if we did and we removed it
      bool
      ForgetPathToService(const Address& remote);

      bool
      HandleDataMessage(const PathID_t&,
                        std::shared_ptr< ProtocolMessage > msg) override;

      virtual bool
      HandleWriteIPPacket(const llarp_buffer_t& pkt,
                          std::function< huint32_t(void) > getFromIP) = 0;

      bool
      ProcessDataMessage(std::shared_ptr< ProtocolMessage > msg);

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
      SendToServiceOrQueue(const RouterID& addr, const llarp_buffer_t& payload,
                           ProtocolType t);

      bool
      SendToSNodeOrQueue(const RouterID& addr, const llarp_buffer_t& payload);

      bool
      HandleDataDrop(path::Path_ptr p, const PathID_t& dst, uint64_t s);

      bool
      CheckPathIsDead(path::Path_ptr p, llarp_time_t latency);

      using PendingBufferQueue = std::deque< PendingBuffer >;

      bool
      ShouldBundleRC() const override;

      static void
      HandlePathDead(void*);

      bool
      HasConvoTag(const ConvoTag& t) const override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      // passed a sendto context when we have a path established otherwise
      // nullptr if the path was not made before the timeout
      using PathEnsureHook = std::function< void(Address, OutboundContext*) >;

      /// return false if we have already called this function before for this
      /// address
      bool
      EnsurePathToService(const Address& remote, PathEnsureHook h,
                          uint64_t timeoutMS, bool lookupOnRandomPath = false);

      using SNodeEnsureHook =
          std::function< void(RouterID, exit::BaseSession_ptr) >;

      /// ensure a path to a service node by public key
      void
      EnsurePathToSNode(const RouterID& remote, SNodeEnsureHook h);

      bool
      HasPathToSNode(const RouterID& remote) const;

      void
      PutSenderFor(const ConvoTag& tag, const ServiceInfo& info) override;

      bool
      GetCachedSessionKeyFor(const ConvoTag& remote,
                             SharedSecret& secret) const override;
      void
      PutCachedSessionKeyFor(const ConvoTag& remote,
                             const SharedSecret& secret) override;

      bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const override;

      void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro) override;

      bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const override;

      void
      RemoveConvoTag(const ConvoTag& remote) override;

      void
      PutReplyIntroFor(const ConvoTag& remote,
                       const Introduction& intro) override;

      bool
      GetReplyIntroFor(const ConvoTag& remote,
                       Introduction& intro) const override;

      bool
      GetConvoTagsForService(const ServiceInfo& si,
                             std::set< ConvoTag >& tag) const override;

      void
      PutNewOutboundContext(const IntroSet& introset);

      uint64_t
      GetSeqNoForConvo(const ConvoTag& tag);

      virtual bool
      SelectHop(llarp_nodedb* db, const std::set< RouterID >& prev,
                RouterContact& cur, size_t hop, path::PathRole roles) override;

      virtual void
      IntroSetPublishFail();
      virtual void
      IntroSetPublished();

      uint64_t
      GenTXID();

     protected:
      /// parent context that owns this endpoint
      Context* const context;

      void
      RegenAndPublishIntroSet(llarp_time_t now, bool forceRebuild = false);

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
      HandleVerifyGotRouter(dht::GotRouterMessage_constptr msg,
                            llarp_async_verify_rc* j);

      bool
      OnLookup(const service::Address& addr, const IntroSet* i,
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

     public:
      std::set< RouterID > m_SnodeBlacklist;

     protected:
      IDataHandler* m_DataHandler = nullptr;
      Identity m_Identity;
      std::shared_ptr< exit::BaseSession > m_Exit;
      hooks::Backend_ptr m_OnUp;
      hooks::Backend_ptr m_OnDown;
      hooks::Backend_ptr m_OnReady;

     private:
      friend struct EndpointUtil;

      struct RouterLookupJob
      {
        RouterLookupJob(Endpoint* p, RouterLookupHandler h) : handler(h)
        {
          started = p->Now();
          txid    = p->GenTXID();
        }

        RouterLookupHandler handler;
        uint64_t txid;
        llarp_time_t started;

        bool
        IsExpired(llarp_time_t now) const
        {
          if(now < started)
            return false;
          return now - started > 30000;
        }

        void
        InformResult(std::vector< RouterContact > result)
        {
          if(handler)
            handler(result);
        }
      };

      using Msg_ptr     = std::shared_ptr< const routing::PathTransferMessage >;
      using SendEvent_t = std::pair< Msg_ptr, path::Path_ptr >;
      using PendingTraffic =
          std::unordered_map< Address, PendingBufferQueue, Address::Hash >;

      using ProtocolMessagePtr = std::shared_ptr<ProtocolMessage>;
      using RecvPacketQueue_t = std::priority_queue<ProtocolMessagePtr, std::vector<ProtocolMessagePtr>, ComparePtr<ProtocolMessagePtr> >;

      util::Mutex m_InboundTrafficQueueMutex;
      /// ordered queue for inbound hidden service traffic
      RecvPacketQueue_t m_InboundTrafficQueue GUARDED_BY(m_InboundTrafficQueueMutex);

      using PendingRouters =
          std::unordered_map< RouterID, RouterLookupJob, RouterID::Hash >;

      using PendingLookups =
          std::unordered_map< uint64_t,
                              std::unique_ptr< service::IServiceLookup > >;

      using Sessions =
          std::unordered_multimap< Address, std::shared_ptr< OutboundContext >,
                                   Address::Hash >;

      using SNodeSessions = std::unordered_multimap<
          RouterID, std::shared_ptr< exit::BaseSession >, RouterID::Hash >;

      using ConvoMap = std::unordered_map< ConvoTag, Session, ConvoTag::Hash >;

      AbstractRouter* m_Router;
      std::shared_ptr<Logic> m_IsolatedLogic              = nullptr;
      llarp_ev_loop_ptr m_IsolatedNetLoop = nullptr;
      std::string m_Keyfile;
      std::string m_Name;
      std::string m_NetNS;
      bool m_BundleRC = false;

      util::Mutex m_SendQueueMutex;
      std::deque< SendEvent_t > m_SendQueue GUARDED_BY(m_SendQueueMutex);

      PendingTraffic m_PendingTraffic;

      Sessions m_RemoteSessions;
      Sessions m_DeadSessions;

      SNodeSessions m_SNodeSessions;

      std::unordered_map< Address, ServiceInfo, Address::Hash >
          m_AddressToService;

      std::unordered_multimap< Address, PathEnsureHook, Address::Hash >
          m_PendingServiceLookups;

      std::unordered_map< RouterID, uint32_t, RouterID::Hash >
          m_ServiceLookupFails;

      PendingRouters m_PendingRouters;

      uint64_t m_CurrentPublishTX       = 0;
      llarp_time_t m_LastPublish        = 0;
      llarp_time_t m_LastPublishAttempt = 0;
      llarp_time_t m_MinPathLatency     = (5 * 1000);
      /// our introset
      service::IntroSet m_IntroSet;
      /// pending remote service lookups by id
      PendingLookups m_PendingLookups;
      /// prefetch remote address list
      std::set< Address > m_PrefetchAddrs;
      /// hidden service tag
      Tag m_Tag;
      /// prefetch descriptors for these hidden service tags
      std::set< Tag > m_PrefetchTags;
      /// on initialize functions
      std::list< std::function< bool(void) > > m_OnInit;

      /// conversations
      ConvoMap m_Sessions;

      std::unordered_map< Tag, CachedTagResult, Tag::Hash > m_PrefetchedTags;
    };

    using Endpoint_ptr = std::shared_ptr< Endpoint >;

  }  // namespace service
}  // namespace llarp

#endif

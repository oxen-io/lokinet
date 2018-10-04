#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP
#include <llarp/codel.hpp>
#include <llarp/pathbuilder.hpp>
#include <llarp/service/Identity.hpp>
#include <llarp/service/handler.hpp>
#include <llarp/service/protocol.hpp>
#include <llarp/path.hpp>

// minimum time between interoset shifts
#ifndef MIN_SHIFT_INTERVAL
#define MIN_SHIFT_INTERVAL (5 * 1000)
#endif

namespace llarp
{
  namespace service
  {
    // foward declare
    struct AsyncKeyExchange;

    struct Endpoint : public path::Builder,
                      public ILookupHolder,
                      public IDataHandler
    {
      /// minimum interval for publishing introsets
      static const llarp_time_t INTROSET_PUBLISH_INTERVAL =
          DEFAULT_PATH_LIFETIME / 4;

      static const llarp_time_t INTROSET_PUBLISH_RETRY_INTERVAL = 5000;

      static const size_t MAX_OUTBOUND_CONTEXT_COUNT = 4;

      Endpoint(const std::string& nickname, llarp_router* r);
      ~Endpoint();

      void
      SetHandler(IDataHandler* h);

      virtual bool
      SetOption(const std::string& k, const std::string& v);

      virtual void
      Tick(llarp_time_t now);

      /// router's logic
      llarp_logic*
      RouterLogic();

      /// endpoint's logic
      llarp_logic*
      EndpointLogic();

      /// endpoint's net loop for sending data to user
      llarp_ev_loop*
      EndpointNetLoop();

      llarp_crypto*
      Crypto();

      llarp_threadpool*
      Worker();

      llarp_router*
      Router()
      {
        return m_Router;
      }

      virtual bool
      Start();

      virtual bool
      Stop()
      {
        // TODO: implement me
        return false;
      }

      std::string
      Name() const;

      bool
      ShouldPublishDescriptors(llarp_time_t now) const;

      void
      EnsureReplyPath(const ServiceInfo& addr);

      bool
      PublishIntroSet(llarp_router* r);

      bool
      PublishIntroSetVia(llarp_router* r, path::Path* p);

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleGotRouterMessage(const llarp::dht::GotRouterMessage* msg);

      bool
      HandleHiddenServiceFrame(path::Path* p,
                               const llarp::service::ProtocolFrame* msg);

      /// return true if we have an established path to a hidden service
      bool
      HasPathToService(const Address& remote) const;

      /// return true if we have a pending job to build to a hidden service but
      /// it's not done yet
      bool
      HasPendingPathToService(const Address& remote) const;

      /// return false if we don't have a path to the service
      /// return true if we did and we removed it
      bool
      ForgetPathToService(const Address& remote);

      bool
      HandleDataMessage(const PathID_t&, ProtocolMessage* msg);

      virtual bool
      ProcessDataMessage(ProtocolMessage* msg)
      {
        return true;
      }

      /// ensure that we know a router, looks up if it doesn't
      void
      EnsureRouterIsKnown(const RouterID& router);

      const Identity&
      GetIdentity()
      {
        return m_Identity;
      }

      void
      PutLookup(IServiceLookup* lookup, uint64_t txid);

      void
      HandlePathBuilt(path::Path* path);

      bool
      SendToOrQueue(const Address& remote, llarp_buffer_t payload,
                    ProtocolType t);

      struct PendingBuffer
      {
        std::vector< byte_t > payload;
        ProtocolType protocol;

        PendingBuffer(llarp_buffer_t buf, ProtocolType t)
            : payload(buf.sz), protocol(t)
        {
          memcpy(payload.data(), buf.base, buf.sz);
        }

        llarp_buffer_t
        Buffer()
        {
          return llarp::InitBuffer(payload.data(), payload.size());
        }
      };

      bool
      HandleDataDrop(path::Path* p, const PathID_t& dst, uint64_t s);

      bool
      CheckPathIsDead(path::Path* p, llarp_time_t latency);

      typedef std::queue< PendingBuffer > PendingBufferQueue;

      struct SendContext
      {
        SendContext(const ServiceInfo& ident, const Introduction& intro,
                    PathSet* send, Endpoint* ep);

        void
        AsyncEncryptAndSendTo(llarp_buffer_t payload, ProtocolType t);

        /// send a fully encrypted hidden service frame
        /// via a path on our pathset with path id p
        void
        Send(ProtocolFrame& f);

        llarp::SharedSecret sharedKey;
        ServiceInfo remoteIdent;
        Introduction remoteIntro;
        ConvoTag currentConvoTag;
        PathSet* m_PathSet;
        IDataHandler* m_DataHandler;
        Endpoint* m_Endpoint;
        uint64_t sequenceNo       = 0;
        llarp_time_t lastGoodSend = 0;
        llarp_time_t createdAt;
        llarp_time_t sendTimeout    = 40 * 1000;
        llarp_time_t connectTimeout = 60 * 1000;
        bool markedBad              = false;

        virtual bool
        ShiftIntroduction()
        {
          return true;
        };

        virtual void
        UpdateIntroSet(){};
        virtual bool
        MarkCurrentIntroBad(llarp_time_t now)
        {
          return true;
        };

       private:
        void
        EncryptAndSendTo(llarp_buffer_t payload, ProtocolType t);

        virtual void
        AsyncGenIntro(llarp_buffer_t payload, ProtocolType t)
        {
        }
      };

      static void
      HandlePathDead(void*);

      /// context needed to initiate an outbound hidden service session
      struct OutboundContext : public path::Builder, public SendContext
      {
        OutboundContext(const IntroSet& introSet, Endpoint* parent);
        ~OutboundContext();

        bool
        HandleDataDrop(path::Path* p, const PathID_t& dst, uint64_t s);

        /// set to true if we are updating the remote introset right now
        bool updatingIntroSet;

        /// update the current selected intro to be a new best introduction
        /// return true if we have changed intros
        bool
        ShiftIntroduction();

        /// mark the current remote intro as bad
        bool
        MarkCurrentIntroBad(llarp_time_t now);

        /// return true if we are ready to send
        bool
        ReadyToSend() const;

        bool
        ShouldBuildMore() const;

        /// tick internal state
        /// return true to mark as dead
        bool
        Tick(llarp_time_t now);

        /// return true if it's safe to remove ourselves
        bool
        IsDone(llarp_time_t now) const;

        bool
        CheckPathIsDead(path::Path* p, llarp_time_t dlt);

        void
        AsyncGenIntro(llarp_buffer_t payload, ProtocolType t);

        /// issues a lookup to find the current intro set of the remote service
        void
        UpdateIntroSet();

        bool
        BuildOneAlignedTo(const RouterID& remote);

        void
        HandlePathBuilt(path::Path* path);

        bool
        SelectHop(llarp_nodedb* db, const RouterContact& prev,
                  RouterContact& cur, size_t hop);

        bool
        HandleHiddenServiceFrame(path::Path* p, const ProtocolFrame* frame);

        std::string
        Name() const;

       private:
        /// swap remoteIntro with next intro
        void
        SwapIntros();

        void
        OnGeneratedIntroFrame(AsyncKeyExchange* k, PathID_t p);

        bool
        OnIntroSetUpdate(const Address& addr, const IntroSet* i);

        uint64_t m_UpdateIntrosetTX = 0;
        IntroSet currentIntroSet;
        Introduction m_NextIntro;
        std::unordered_map< Introduction, llarp_time_t, Introduction::Hash >
            m_BadIntros;
        llarp_time_t lastShift = 0;
      };

      // passed a sendto context when we have a path established otherwise
      // nullptr if the path was not made before the timeout
      typedef std::function< void(Address, OutboundContext*) > PathEnsureHook;

      /// return false if we have already called this function before for this
      /// address
      bool
      EnsurePathToService(const Address& remote, PathEnsureHook h,
                          uint64_t timeoutMS);

      virtual bool
      HandleAuthenticatedDataFrom(const Address& remote, llarp_buffer_t data)
      {
        /// TODO: imlement me
        return true;
      }

      void
      PutSenderFor(const ConvoTag& tag, const ServiceInfo& info);

      bool
      GetCachedSessionKeyFor(const ConvoTag& remote,
                             const byte_t*& secret) const;
      void
      PutCachedSessionKeyFor(const ConvoTag& remote,
                             const SharedSecret& secret);

      bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const;

      void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro);

      bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const;

      bool
      GetConvoTagsForService(const ServiceInfo& si,
                             std::set< ConvoTag >& tag) const;

      void
      PutNewOutboundContext(const IntroSet& introset);

      virtual void
      IntroSetPublishFail();
      virtual void
      IntroSetPublished();

     protected:
      void
      RegenAndPublishIntroSet(llarp_time_t now, bool forceRebuild = false);

      IServiceLookup*
      GenerateLookupByTag(const Tag& tag);

      void
      PrefetchServicesByTag(const Tag& tag);

      uint64_t
      GetSeqNoForConvo(const ConvoTag& tag);

      bool
      IsolateNetwork();

      bool
      NetworkIsIsolated() const;

      static void
      RunIsolatedMainLoop(void*);

     private:
      bool
      OnOutboundLookup(const Address&, const IntroSet* i); /*  */

      static bool
      SetupIsolatedNetwork(void* user, bool success);

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

      uint64_t
      GenTXID();

     protected:
      IDataHandler* m_DataHandler = nullptr;
      Identity m_Identity;

     private:
      llarp_router* m_Router;
      llarp_threadpool* m_IsolatedWorker = nullptr;
      llarp_logic* m_IsolatedLogic       = nullptr;
      llarp_ev_loop* m_IsolatedNetLoop   = nullptr;
      std::string m_Keyfile;
      std::string m_Name;
      std::string m_NetNS;

      std::unordered_map< Address, PendingBufferQueue, Address::Hash >
          m_PendingTraffic;

      std::unordered_multimap< Address, std::unique_ptr< OutboundContext >,
                               Address::Hash >
          m_RemoteSessions;

      std::unordered_multimap< Address, std::unique_ptr< OutboundContext >,
                               Address::Hash >
          m_DeadSessions;

      std::unordered_map< Address, ServiceInfo, Address::Hash >
          m_AddressToService;

      std::unordered_map< Address, PathEnsureHook, Address::Hash >
          m_PendingServiceLookups;

      struct RouterLookupJob
      {
        RouterLookupJob(Endpoint* p)
        {
          started = llarp_time_now_ms();
          txid    = p->GenTXID();
        }

        uint64_t txid;
        llarp_time_t started;

        bool
        IsExpired(llarp_time_t now) const
        {
          if(now < started)
            return false;
          return now - started > 5000;
        }
      };

      std::unordered_map< RouterID, RouterLookupJob, RouterID::Hash >
          m_PendingRouters;

      uint64_t m_CurrentPublishTX       = 0;
      llarp_time_t m_LastPublish        = 0;
      llarp_time_t m_LastPublishAttempt = 0;
      llarp_time_t m_MinPathLatency     = (5 * 1000);
      /// our introset
      service::IntroSet m_IntroSet;
      /// pending remote service lookups by id
      std::unordered_map< uint64_t, std::unique_ptr< service::IServiceLookup > >
          m_PendingLookups;
      /// prefetch remote address list
      std::set< Address > m_PrefetchAddrs;
      /// hidden service tag
      Tag m_Tag;
      /// prefetch descriptors for these hidden service tags
      std::set< Tag > m_PrefetchTags;
      /// on initialize functions
      std::list< std::function< bool(void) > > m_OnInit;

      struct Session
      {
        SharedSecret sharedKey;
        ServiceInfo remote;
        Introduction intro;
        llarp_time_t lastUsed = 0;
        uint64_t seqno        = 0;
      };

      /// sessions
      std::unordered_map< ConvoTag, Session, ConvoTag::Hash > m_Sessions;

      struct CachedTagResult
      {
        const static llarp_time_t TTL = 10000;
        llarp_time_t lastRequest      = 0;
        llarp_time_t lastModified     = 0;
        std::set< IntroSet > result;
        Tag tag;

        CachedTagResult(const Tag& t) : tag(t)
        {
        }

        ~CachedTagResult()
        {
        }

        void
        Expire(llarp_time_t now);

        bool
        ShouldRefresh(llarp_time_t now) const
        {
          if(now <= lastRequest)
            return false;
          return (now - lastRequest) > TTL;
        }

        llarp::routing::IMessage*
        BuildRequestMessage(uint64_t txid);

        bool
        HandleResponse(const std::set< IntroSet >& results);
      };

      struct TagLookupJob : public IServiceLookup
      {
        TagLookupJob(Endpoint* parent, CachedTagResult* result)
            : IServiceLookup(parent, parent->GenTXID(), "taglookup")
            , m_result(result)
        {
        }

        ~TagLookupJob()
        {
        }

        llarp::routing::IMessage*
        BuildRequestMessage()
        {
          return m_result->BuildRequestMessage(txid);
        }

        bool
        HandleResponse(const std::set< IntroSet >& results)
        {
          return m_result->HandleResponse(results);
        }

        CachedTagResult* m_result;
      };

      std::unordered_map< Tag, CachedTagResult, Tag::Hash > m_PrefetchedTags;
    };
  }  // namespace service
}  // namespace llarp

#endif

#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP
#include <llarp/codel.hpp>
#include <llarp/pathbuilder.hpp>
#include <llarp/service/Identity.hpp>
#include <llarp/service/protocol.hpp>

namespace llarp
{
  namespace service
  {
    struct Endpoint : public llarp_pathbuilder_context
    {
      /// minimum interval for publishing introsets
      static const llarp_time_t INTROSET_PUBLISH_INTERVAL =
          DEFAULT_PATH_LIFETIME / 4;

      static const llarp_time_t INTROSET_PUBLISH_RETRY_INTERVAL = 5000;

      Endpoint(const std::string& nickname, llarp_router* r);
      ~Endpoint();

      bool
      SetOption(const std::string& k, const std::string& v);

      void
      Tick(llarp_time_t now);

      llarp_logic*
      Logic();

      llarp_crypto*
      Crypto();

      llarp_threadpool*
      Worker();

      llarp_router*
      Router()
      {
        return m_Router;
      }

      bool
      Start();

      std::string
      Name() const;

      bool
      ShouldPublishDescriptors(llarp_time_t now) const;

      bool
      PublishIntroSet(llarp_router* r);

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleHiddenServiceFrame(const llarp::service::ProtocolFrame* msg);

      /// return true if we have an established path to a hidden service
      bool
      HasPathToService(const Address& remote) const;

      /// return false if we don't have a path to the service
      /// return true if we did and we removed it
      bool
      ForgetPathToService(const Address& remote);

      Identity*
      GetIdentity()
      {
        return &m_Identity;
      }

      /// context needed to initiate an outbound hidden service session
      struct OutboundContext : public llarp_pathbuilder_context
      {
        OutboundContext(const IntroSet& introSet, Endpoint* parent);
        ~OutboundContext();

        /// the remote hidden service's curren intro set
        IntroSet currentIntroSet;
        /// the current selected intro
        Introduction selectedIntro;

        /// update the current selected intro to be a new best introduction
        void
        ShiftIntroduction();

        /// tick internal state
        /// return true to remove otherwise don't remove
        bool
        Tick(llarp_time_t now);

        /// encrypt asynchronously and send to remote endpoint from us
        void
        AsyncEncryptAndSendTo(llarp_buffer_t D, ProtocolType protocol);

        /// issues a lookup to find the current intro set of the remote service
        void
        UpdateIntroSet();

        bool
        SelectHop(llarp_nodedb* db, llarp_rc* prev, llarp_rc* cur, size_t hop);

        bool
        HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

       private:
        void
        AsyncEncrypt(llarp_buffer_t payload);

        void
        AsyncGenIntro(llarp_buffer_t payload);

        /// send a fully encrypted hidden service frame
        void
        Send(ProtocolFrame& f);

        uint64_t sequenceNo = 0;
        llarp::SharedSecret sharedKey;
        Endpoint* m_Parent;
      };

      // passed a sendto context when we have a path established otherwise
      // nullptr if the path was not made before the timeout
      typedef std::function< void(OutboundContext*) > PathEnsureHook;

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
      PutNewOutboundContext(const IntroSet& introset);

     protected:
      virtual void
      IntroSetPublishFail();
      virtual void
      IntroSetPublished();

      IServiceLookup*
      GenerateLookupByTag(const Tag& tag);

      void
      PrefetchServicesByTag(const Tag& tag);

     private:
      uint64_t
      GenTXID();

     private:
      llarp_router* m_Router;
      std::string m_Keyfile;
      std::string m_Name;
      Identity m_Identity;
      std::unordered_map< Address, OutboundContext*, Address::Hash >
          m_RemoteSessions;
      std::unordered_map< Address, PathEnsureHook, Address::Hash >
          m_PendingServiceLookups;
      uint64_t m_CurrentPublishTX       = 0;
      llarp_time_t m_LastPublish        = 0;
      llarp_time_t m_LastPublishAttempt = 0;
      /// our introset
      service::IntroSet m_IntroSet;
      /// pending remote service lookups by id
      std::unordered_map< uint64_t, service::IServiceLookup* > m_PendingLookups;
      /// hidden service tag
      Tag m_Tag;
      /// prefetch descriptors for these hidden service tags
      std::set< Tag > m_PrefetchTags;

      struct CachedTagResult : public IServiceLookup
      {
        const static llarp_time_t TTL = 10000;
        llarp_time_t lastRequest      = 0;
        llarp_time_t lastModified;
        uint64_t pendingTX = 0;
        std::set< IntroSet > result;
        Tag tag;

        CachedTagResult(const Tag& t, llarp_time_t now)
            : lastModified(now), tag(t)
        {
        }

        ~CachedTagResult();

        void
        Expire(llarp_time_t now);

        bool
        ShouldRefresh(llarp_time_t now) const
        {
          if(now <= lastRequest)
            return false;
          return (now - lastRequest) > TTL && pendingTX == 0;
        }

        llarp::routing::IMessage*
        BuildRequestMessage();

        bool
        HandleResponse(const std::set< IntroSet >& results);
      };

      std::unordered_map< Tag, CachedTagResult, Tag::Hash > m_PrefetchedTags;
    };
  }  // namespace service
}  // namespace llarp

#endif
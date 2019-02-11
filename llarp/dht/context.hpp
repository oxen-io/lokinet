#ifndef LLARP_DHT_CONTEXT
#define LLARP_DHT_CONTEXT

#include <dht/bucket.hpp>
#include <dht/dht.h>
#include <dht/key.hpp>
#include <dht/message.hpp>
#include <dht/messages/findintro.hpp>
#include <dht/node.hpp>
#include <dht/tx.hpp>
#include <dht/txholder.hpp>
#include <dht/txowner.hpp>
#include <service/IntroSet.hpp>
#include <util/time.hpp>
#include <util/status.hpp>

#include <set>

namespace llarp
{
  struct AbstractRouter;

  namespace dht
  {
    struct AbstractContext
    {
      virtual ~AbstractContext() = 0;

      virtual bool
      LookupRouter(const RouterID& target, RouterLookupHandler result) = 0;

      /// on behalf of whoasked request introset for target from dht router with
      /// key askpeer
      virtual void
      LookupIntroSetRecursive(const service::Address& target,
                              const Key_t& whoasked, uint64_t whoaskedTX,
                              const Key_t& askpeer, uint64_t R,
                              service::IntroSetLookupHandler result =
                                  service::IntroSetLookupHandler()) = 0;

      virtual void
      LookupIntroSetIterative(const service::Address& target,
                              const Key_t& whoasked, uint64_t whoaskedTX,
                              const Key_t& askpeer,
                              service::IntroSetLookupHandler result =
                                  service::IntroSetLookupHandler()) = 0;

      virtual std::set< service::IntroSet >
      FindRandomIntroSetsWithTagExcluding(
          const service::Tag& tag, size_t max = 2,
          const std::set< service::IntroSet >& excludes = {}) = 0;

      virtual void
      DHTSendTo(const RouterID& peer, IMessage* msg, bool keepalive = true) = 0;

      virtual llarp_time_t
      Now() const = 0;

      virtual llarp::Crypto*
      Crypto() const = 0;

      virtual llarp::AbstractRouter*
      GetRouter() const = 0;

      virtual const Key_t&
      OurKey() const = 0;

      virtual Bucket< RCNode >*
      Nodes() const = 0;
    };

    struct Context final : public AbstractContext, public util::IStateful
    {
      Context();

      ~Context()
      {
      }

      util::StatusObject
      ExtractStatus() const override;

      llarp::Crypto*
      Crypto() const override;

      /// on behalf of whoasked request introset for target from dht router with
      /// key askpeer
      void
      LookupIntroSetRecursive(
          const service::Address& target, const Key_t& whoasked,
          uint64_t whoaskedTX, const Key_t& askpeer, uint64_t R,
          service::IntroSetLookupHandler result = nullptr) override;

      void
      LookupIntroSetIterative(
          const service::Address& target, const Key_t& whoasked,
          uint64_t whoaskedTX, const Key_t& askpeer,
          service::IntroSetLookupHandler result = nullptr) override;

      /// on behalf of whoasked request router with public key target from dht
      /// router with key askpeer
      void
      LookupRouterRecursive(const RouterID& target, const Key_t& whoasked,
                            uint64_t whoaskedTX, const Key_t& askpeer,
                            RouterLookupHandler result = nullptr);

      bool
      LookupRouter(const RouterID& target, RouterLookupHandler result) override
      {
        Key_t askpeer;
        if(!nodes->FindClosest(Key_t(target), askpeer))
        {
          return false;
        }
        LookupRouterRecursive(target, OurKey(), 0, askpeer, result);
        return true;
      }

      bool
      HasRouterLookup(const RouterID& target) const
      {
        return pendingRouterLookups.HasLookupFor(target);
      }

      /// on behalf of whoasked request introsets with tag from dht router with
      /// key askpeer with Recursion depth R
      void
      LookupTagRecursive(const service::Tag& tag, const Key_t& whoasked,
                         uint64_t whoaskedTX, const Key_t& askpeer, uint64_t R);

      /// issue dht lookup for tag via askpeer and send reply to local path
      void
      LookupTagForPath(const service::Tag& tag, uint64_t txid,
                       const llarp::PathID_t& path, const Key_t& askpeer);

      /// issue dht lookup for router via askpeer and send reply to local path
      void
      LookupRouterForPath(const RouterID& target, uint64_t txid,
                          const llarp::PathID_t& path, const Key_t& askpeer);

      /// issue dht lookup for introset for addr via askpeer and send reply to
      /// local path
      void
      LookupIntroSetForPath(const service::Address& addr, uint64_t txid,
                            const llarp::PathID_t& path, const Key_t& askpeer);

      /// send a dht message to peer, if keepalive is true then keep the session
      /// with that peer alive for 10 seconds
      void
      DHTSendTo(const RouterID& peer, IMessage* msg,
                bool keepalive = true) override;

      /// get routers closest to target excluding requester
      bool
      HandleExploritoryRouterLookup(
          const Key_t& requester, uint64_t txid, const RouterID& target,
          std::vector< std::unique_ptr< IMessage > >& reply);

      std::set< service::IntroSet >
      FindRandomIntroSetsWithTagExcluding(
          const service::Tag& tag, size_t max = 2,
          const std::set< service::IntroSet >& excludes = {}) override;

      /// handle rc lookup from requester for target
      void
      LookupRouterRelayed(const Key_t& requester, uint64_t txid,
                          const Key_t& target, bool recursive,
                          std::vector< std::unique_ptr< IMessage > >& replies);

      /// relay a dht message from a local path to the main network
      bool
      RelayRequestForPath(const llarp::PathID_t& localPath,
                          const IMessage* msg);

      /// send introset to peer from source with S counter and excluding peers
      void
      PropagateIntroSetTo(const Key_t& source, uint64_t sourceTX,
                          const service::IntroSet& introset, const Key_t& peer,
                          uint64_t S, const std::set< Key_t >& exclude);

      /// initialize dht context and explore every exploreInterval milliseconds
      void
      Init(const Key_t& us, llarp::Router* router,
           llarp_time_t exploreInterval);

      /// get localally stored introset by service address
      const llarp::service::IntroSet*
      GetIntroSetByServiceAddress(const llarp::service::Address& addr) const;

      static void
      handle_cleaner_timer(void* user, uint64_t orig, uint64_t left);

      static void
      handle_explore_timer(void* user, uint64_t orig, uint64_t left);

      /// explore dht for new routers
      void
      Explore(size_t N = 3);

      llarp::AbstractRouter* router;
      // for router contacts
      std::unique_ptr< Bucket< RCNode > > nodes;

      // for introduction sets
      std::unique_ptr< Bucket< ISNode > > services;
      bool allowTransit;

      Bucket< RCNode >*
      Nodes() const override
      {
        return nodes.get();
      }

      const Key_t&
      OurKey() const override
      {
        return ourKey;
      }

      llarp::AbstractRouter*
      GetRouter() const override
      {
        return router;
      }

      TXHolder< service::Address, service::IntroSet, service::Address::Hash >
          pendingIntrosetLookups;

      TXHolder< service::Tag, service::IntroSet, service::Tag::Hash >
          pendingTagLookups;

      TXHolder< RouterID, RouterContact, RouterID::Hash > pendingRouterLookups;

      TXHolder< RouterID, RouterID, RouterID::Hash > pendingExploreLookups;

      uint64_t
      NextID()
      {
        return ++ids;
      }

      llarp_time_t
      Now() const override;

      void
      ExploreNetworkVia(const Key_t& peer);

     private:
      void
      ScheduleCleanupTimer();

      void
      CleanupTX();

      uint64_t ids;

      Key_t ourKey;
    };  // namespace llarp
  }     // namespace dht
}  // namespace llarp

struct llarp_dht_context
{
  llarp::dht::Context impl;
  llarp::Router* parent;
  llarp_dht_context(llarp::Router* router);
};

#endif

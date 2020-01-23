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
#include <service/intro_set.hpp>
#include <util/time.hpp>
#include <util/status.hpp>

#include <memory>
#include <set>

namespace llarp
{
  struct AbstractRouter;

  namespace dht
  {
    struct AbstractContext
    {
      using PendingIntrosetLookups =
          TXHolder< service::Address, service::IntroSet,
                    service::Address::Hash >;
      using PendingTagLookups =
          TXHolder< service::Tag, service::IntroSet, service::Tag::Hash >;
      using PendingRouterLookups =
          TXHolder< RouterID, RouterContact, RouterID::Hash >;
      using PendingExploreLookups =
          TXHolder< RouterID, RouterID, RouterID::Hash >;

      virtual ~AbstractContext() = 0;

      virtual bool
      LookupRouter(const RouterID& target, RouterLookupHandler result) = 0;

      virtual void
      LookupRouterRecursive(const RouterID& target, const Key_t& whoasked,
                            uint64_t whoaskedTX, const Key_t& askpeer,
                            RouterLookupHandler result = nullptr) = 0;

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

      virtual bool
      HasRouterLookup(const RouterID& target) const = 0;

      /// on behalf of whoasked request introsets with tag from dht router with
      /// key askpeer with Recursion depth R
      virtual void
      LookupTagRecursive(const service::Tag& tag, const Key_t& whoasked,
                         uint64_t whoaskedTX, const Key_t& askpeer,
                         uint64_t R) = 0;

      /// issue dht lookup for tag via askpeer and send reply to local path
      virtual void
      LookupTagForPath(const service::Tag& tag, uint64_t txid,
                       const PathID_t& path, const Key_t& askpeer) = 0;

      /// issue dht lookup for router via askpeer and send reply to local path
      virtual void
      LookupRouterForPath(const RouterID& target, uint64_t txid,
                          const PathID_t& path, const Key_t& askpeer) = 0;

      virtual void
      LookupIntroSetForPath(const service::Address& addr, uint64_t txid,
                            const PathID_t& path, const Key_t& askpeer,
                            uint64_t R) = 0;

      virtual void
      DHTSendTo(const RouterID& peer, IMessage* msg, bool keepalive = true) = 0;

      /// get routers closest to target excluding requester
      virtual bool
      HandleExploritoryRouterLookup(
          const Key_t& requester, uint64_t txid, const RouterID& target,
          std::vector< std::unique_ptr< IMessage > >& reply) = 0;

      /// handle rc lookup from requester for target
      virtual void
      LookupRouterRelayed(
          const Key_t& requester, uint64_t txid, const Key_t& target,
          bool recursive,
          std::vector< std::unique_ptr< IMessage > >& replies) = 0;

      virtual bool
      RelayRequestForPath(const PathID_t& localPath, const IMessage& msg) = 0;

      /// send introset to peer from source with S counter and excluding peers
      virtual void
      PropagateIntroSetTo(const Key_t& source, uint64_t sourceTX,
                          const service::IntroSet& introset, const Key_t& peer,
                          uint64_t S, const std::set< Key_t >& exclude) = 0;

      virtual void
      Init(const Key_t& us, AbstractRouter* router,
           llarp_time_t exploreInterval) = 0;

      virtual const llarp::service::IntroSet*
      GetIntroSetByServiceAddress(
          const llarp::service::Address& addr) const = 0;

      virtual llarp_time_t
      Now() const = 0;

      virtual void
      ExploreNetworkVia(const Key_t& peer) = 0;

      virtual llarp::AbstractRouter*
      GetRouter() const = 0;

      virtual bool
      GetRCFromNodeDB(const Key_t& k, llarp::RouterContact& rc) const = 0;

      virtual const Key_t&
      OurKey() const = 0;

      virtual PendingIntrosetLookups&
      pendingIntrosetLookups() = 0;

      virtual const PendingIntrosetLookups&
      pendingIntrosetLookups() const = 0;

      virtual PendingTagLookups&
      pendingTagLookups() = 0;

      virtual const PendingTagLookups&
      pendingTagLookups() const = 0;

      virtual PendingRouterLookups&
      pendingRouterLookups() = 0;

      virtual const PendingRouterLookups&
      pendingRouterLookups() const = 0;

      virtual PendingExploreLookups&
      pendingExploreLookups() = 0;

      virtual const PendingExploreLookups&
      pendingExploreLookups() const = 0;

      virtual Bucket< ISNode >*
      services() = 0;

      virtual bool&
      AllowTransit() = 0;
      virtual const bool&
      AllowTransit() const = 0;

      virtual Bucket< RCNode >*
      Nodes() const = 0;

      virtual void
      PutRCNodeAsync(const RCNode& val) = 0;

      virtual void
      DelRCNodeAsync(const Key_t& val) = 0;

      virtual util::StatusObject
      ExtractStatus() const = 0;

      virtual void
      StoreRC(const RouterContact rc) const = 0;
    };

    std::unique_ptr< AbstractContext >
    makeContext();
  }  // namespace dht
}  // namespace llarp

struct llarp_dht_context
{
  std::unique_ptr< llarp::dht::AbstractContext > impl;
  llarp::AbstractRouter* parent;
  llarp_dht_context(llarp::AbstractRouter* router);
};

#endif

#ifndef LLARP_DHT_CONTEXT
#define LLARP_DHT_CONTEXT

#include "bucket.hpp"
#include "dht.h"
#include "key.hpp"
#include "message.hpp"
#include <llarp/dht/messages/findintro.hpp>
#include "node.hpp"
#include "tx.hpp"
#include "txholder.hpp"
#include "txowner.hpp"
#include <llarp/service/intro_set.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/status.hpp>

#include <memory>
#include <set>

namespace llarp
{
  struct Router;

  namespace dht
  {
    /// number of routers to publish to
    static constexpr size_t IntroSetRelayRedundancy = 2;

    /// number of dht locations handled per relay
    static constexpr size_t IntroSetRequestsPerRelay = 2;

    static constexpr size_t IntroSetStorageRedundancy =
        (IntroSetRelayRedundancy * IntroSetRequestsPerRelay);

    struct AbstractDHTMessageHandler /* : public AbstractMessageHandler */
    {
      using PendingIntrosetLookups = TXHolder<TXOwner, service::EncryptedIntroSet>;
      using PendingRouterLookups = TXHolder<RouterID, RouterContact>;
      using PendingExploreLookups = TXHolder<RouterID, RouterID>;

      virtual ~AbstractDHTMessageHandler() = 0;

      virtual bool
      LookupRouter(const RouterID& target, RouterLookupHandler result) = 0;

      virtual void
      LookupRouterRecursive(
          const RouterID& target,
          const Key_t& whoasked,
          uint64_t whoaskedTX,
          const Key_t& askpeer,
          RouterLookupHandler result = nullptr) = 0;

      /// Ask a Service Node to perform an Introset lookup for us
      virtual void
      LookupIntroSetRelayed(
          const Key_t& target,
          const Key_t& whoasked,
          uint64_t whoaskedTX,
          const Key_t& askpeer,
          uint64_t relayOrder,
          service::EncryptedIntroSetLookupHandler result =
              service::EncryptedIntroSetLookupHandler()) = 0;

      /// Directly as a Service Node for an Introset
      virtual void
      LookupIntroSetDirect(
          const Key_t& target,
          const Key_t& whoasked,
          uint64_t whoaskedTX,
          const Key_t& askpeer,
          service::EncryptedIntroSetLookupHandler result =
              service::EncryptedIntroSetLookupHandler()) = 0;

      virtual bool
      HasRouterLookup(const RouterID& target) const = 0;

      /// issue dht lookup for router via askpeer and send reply to local path
      virtual void
      LookupRouterForPath(
          const RouterID& target, uint64_t txid, const PathID_t& path, const Key_t& askpeer) = 0;

      virtual void
      LookupIntroSetForPath(
          const Key_t& addr,
          uint64_t txid,
          const PathID_t& path,
          const Key_t& askpeer,
          uint64_t relayOrder) = 0;

      virtual void
      DHTSendTo(const RouterID& peer, AbstractDHTMessage* msg, bool keepalive = true) = 0;

      /// get routers closest to target excluding requester
      virtual bool
      HandleExploritoryRouterLookup(
          const Key_t& requester,
          uint64_t txid,
          const RouterID& target,
          std::vector<std::unique_ptr<AbstractDHTMessage>>& reply) = 0;

      /// handle rc lookup from requester for target
      virtual void
      LookupRouterRelayed(
          const Key_t& requester,
          uint64_t txid,
          const Key_t& target,
          bool recursive,
          std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) = 0;

      virtual bool
      RelayRequestForPath(const PathID_t& localPath, const AbstractDHTMessage& msg) = 0;

      /// send introset to peer from source with S counter and excluding peers
      virtual void
      PropagateLocalIntroSet(
          const PathID_t& path,
          uint64_t sourceTX,
          const service::EncryptedIntroSet& introset,
          const Key_t& peer,
          uint64_t relayOrder) = 0;

      /// send introset to peer from source with S counter and excluding peers
      virtual void
      PropagateIntroSetTo(
          const Key_t& source,
          uint64_t sourceTX,
          const service::EncryptedIntroSet& introset,
          const Key_t& peer,
          uint64_t relayOrder) = 0;

      virtual void
      Init(const Key_t& us, Router* router) = 0;

      virtual std::optional<llarp::service::EncryptedIntroSet>
      GetIntroSetByLocation(const Key_t& location) const = 0;

      virtual llarp_time_t
      Now() const = 0;

      virtual void
      ExploreNetworkVia(const Key_t& peer) = 0;

      virtual llarp::Router*
      GetRouter() const = 0;

      virtual bool
      GetRCFromNodeDB(const Key_t& k, llarp::RouterContact& rc) const = 0;

      virtual const Key_t&
      OurKey() const = 0;

      virtual PendingIntrosetLookups&
      pendingIntrosetLookups() = 0;

      virtual const PendingIntrosetLookups&
      pendingIntrosetLookups() const = 0;

      virtual PendingRouterLookups&
      pendingRouterLookups() = 0;

      virtual const PendingRouterLookups&
      pendingRouterLookups() const = 0;

      virtual PendingExploreLookups&
      pendingExploreLookups() = 0;

      virtual const PendingExploreLookups&
      pendingExploreLookups() const = 0;

      virtual Bucket<ISNode>*
      services() = 0;

      virtual bool&
      AllowTransit() = 0;
      virtual const bool&
      AllowTransit() const = 0;

      virtual Bucket<RCNode>*
      Nodes() const = 0;

      virtual void
      PutRCNodeAsync(const RCNode& val) = 0;

      virtual void
      DelRCNodeAsync(const Key_t& val) = 0;

      virtual util::StatusObject
      ExtractStatus() const = 0;

      virtual void
      StoreRC(const RouterContact rc) const = 0;

      virtual bool
      handle_message(
          const AbstractDHTMessage&, std::vector<std::unique_ptr<dht::AbstractDHTMessage>>&) = 0;
    };

    std::unique_ptr<AbstractDHTMessageHandler>
    make_handler();
  }  // namespace dht
}  // namespace llarp

struct llarp_dht_context
{
  std::unique_ptr<llarp::dht::AbstractDHTMessageHandler> impl;
  llarp::Router* parent;
  llarp_dht_context(llarp::Router* router);
};

#endif

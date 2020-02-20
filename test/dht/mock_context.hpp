#ifndef TEST_LLARP_DHT_MOCK_CONTEXT
#define TEST_LLARP_DHT_MOCK_CONTEXT

#include <dht/context.hpp>

#include <gmock/gmock.h>

namespace llarp
{
  namespace test
  {
    struct MockContext final : public dht::AbstractContext
    {
      MOCK_CONST_METHOD1(StoreRC, void(const RouterContact));

      MOCK_METHOD2(LookupRouter, bool(const RouterID&, RouterLookupHandler));

      MOCK_METHOD5(LookupRouterRecursive,
                   void(const RouterID&, const dht::Key_t&, uint64_t,
                        const dht::Key_t&, RouterLookupHandler));

      MOCK_METHOD6(LookupIntroSetRelayed,
                   void(const dht::Key_t&, const dht::Key_t&, uint64_t,
                        const dht::Key_t&, uint64_t,
                        service::EncryptedIntroSetLookupHandler));

      MOCK_METHOD5(LookupIntroSetDirect,
                   void(const dht::Key_t&, const dht::Key_t&, uint64_t,
                        const dht::Key_t&,
                        service::EncryptedIntroSetLookupHandler));

      MOCK_CONST_METHOD1(HasRouterLookup, bool(const RouterID& target));

      MOCK_METHOD4(LookupRouterForPath,
                   void(const RouterID& target, uint64_t txid,
                        const PathID_t& path, const dht::Key_t& askpeer));

      MOCK_METHOD5(LookupIntroSetForPath,
                   void(const dht::Key_t&, uint64_t, const PathID_t&,
                        const dht::Key_t&, uint64_t));

      MOCK_METHOD3(DHTSendTo, void(const RouterID&, dht::IMessage*, bool));

      MOCK_METHOD4(
          HandleExploritoryRouterLookup,
          bool(const dht::Key_t& requester, uint64_t txid,
               const RouterID& target,
               std::vector< std::unique_ptr< dht::IMessage > >& reply));

      MOCK_METHOD5(
          LookupRouterRelayed,
          void(const dht::Key_t& requester, uint64_t txid,
               const dht::Key_t& target, bool recursive,
               std::vector< std::unique_ptr< dht::IMessage > >& replies));

      MOCK_METHOD2(RelayRequestForPath,
                   bool(const PathID_t& localPath, const dht::IMessage& msg));

      MOCK_CONST_METHOD2(GetRCFromNodeDB,
                         bool(const dht::Key_t& k, RouterContact& rc));

      MOCK_METHOD6(PropagateIntroSetTo,
                   void(const dht::Key_t& source, uint64_t sourceTX,
                        const service::EncryptedIntroSet& introset,
                        const dht::Key_t& peer, bool relayed, uint64_t relayOrder));

      MOCK_METHOD3(Init,
                   void(const dht::Key_t&, AbstractRouter*, llarp_time_t));

      MOCK_CONST_METHOD1(GetIntroSetByLocation,
                         absl::optional< llarp::service::EncryptedIntroSet >(
                             const llarp::dht::Key_t&));

      MOCK_CONST_METHOD0(ExtractStatus, util::StatusObject());

      MOCK_CONST_METHOD0(Now, llarp_time_t());

      MOCK_METHOD1(ExploreNetworkVia, void(const dht::Key_t& peer));

      MOCK_CONST_METHOD0(GetRouter, llarp::AbstractRouter*());

      MOCK_CONST_METHOD0(OurKey, const dht::Key_t&());

      MOCK_CONST_METHOD0(pendingIntrosetLookups,
                         const PendingIntrosetLookups&());
      MOCK_METHOD0(pendingIntrosetLookups, PendingIntrosetLookups&());

      MOCK_METHOD0(pendingRouterLookups, PendingRouterLookups&());

      MOCK_CONST_METHOD0(pendingRouterLookups, const PendingRouterLookups&());

      MOCK_METHOD0(pendingExploreLookups, PendingExploreLookups&());

      MOCK_CONST_METHOD0(pendingExploreLookups, const PendingExploreLookups&());

      MOCK_METHOD0(services, dht::Bucket< dht::ISNode >*());

      MOCK_CONST_METHOD0(AllowTransit, const bool&());
      MOCK_METHOD0(AllowTransit, bool&());

      MOCK_CONST_METHOD0(Nodes, dht::Bucket< dht::RCNode >*());
      MOCK_METHOD1(PutRCNodeAsync, void(const dht::RCNode& val));
      MOCK_METHOD1(DelRCNodeAsync, void(const dht::Key_t& val));

      MOCK_METHOD2(FloodRCLater, void(const dht::Key_t, const RouterContact));
    };

  }  // namespace test
}  // namespace llarp

#endif

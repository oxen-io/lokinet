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
      MOCK_METHOD2(LookupRouter, bool(const RouterID&, RouterLookupHandler));

      MOCK_METHOD6(LookupIntroSetRecursive,
                   void(const service::Address&, const dht::Key_t&, uint64_t,
                        const dht::Key_t&, uint64_t,
                        service::IntroSetLookupHandler));

      MOCK_METHOD5(LookupIntroSetIterative,
                   void(const service::Address&, const dht::Key_t&, uint64_t,
                        const dht::Key_t&, service::IntroSetLookupHandler));

      MOCK_METHOD3(
          FindRandomIntroSetsWithTagExcluding,
          std::set< service::IntroSet >(const service::Tag&, size_t,
                                        const std::set< service::IntroSet >&));

      MOCK_METHOD3(DHTSendTo, void(const RouterID&, dht::IMessage*, bool));

      MOCK_CONST_METHOD0(Now, llarp_time_t());

      MOCK_CONST_METHOD0(Crypto, llarp::Crypto*());

      MOCK_CONST_METHOD0(GetRouter, llarp::AbstractRouter*());

      MOCK_CONST_METHOD0(OurKey, const dht::Key_t&());

      MOCK_CONST_METHOD0(Nodes, dht::Bucket< dht::RCNode >*());
    };

  }  // namespace test
}  // namespace llarp

#endif

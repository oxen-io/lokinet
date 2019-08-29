#ifndef LLARP_DHT_PUBLISHSERVICEJOB
#define LLARP_DHT_PUBLISHSERVICEJOB

#include <dht/tx.hpp>
#include <dht/txowner.hpp>
#include <service/address.hpp>
#include <service/intro_set.hpp>

#include <set>

namespace llarp
{
  namespace dht
  {
    struct PublishServiceJob : public TX< service::Address, service::IntroSet >
    {
      uint64_t S;
      std::set< Key_t > dontTell;
      service::IntroSet I;

      PublishServiceJob(const TXOwner &asker, const service::IntroSet &introset,
                        AbstractContext *ctx, uint64_t s,
                        std::set< Key_t > exclude);

      bool
      Validate(const service::IntroSet &introset) const override;

      void
      Start(const TXOwner &peer) override;

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      SendReply() override
      {
        // don't need this
      }
    };
  }  // namespace dht
}  // namespace llarp

#endif

#include <dht/publishservicejob.hpp>

#include <dht/context.hpp>
#include <dht/messages/pubintro.hpp>
#include <utility>

namespace llarp
{
  namespace dht
  {
    PublishServiceJob::PublishServiceJob(const TXOwner &asker,
                      const service::EncryptedIntroSet &introset_,
                      AbstractContext *ctx, bool relayed_, uint64_t relayOrder_)
        : TX< Key_t, service::EncryptedIntroSet >(
            asker, Key_t{introset_.derivedSigningKey}, ctx)
        , relayed(relayed_)
        , relayOrder(relayOrder_)
        , introset(introset_)
    {
    }

    bool
    PublishServiceJob::Validate(const service::EncryptedIntroSet &value) const
    {
      if(value.derivedSigningKey != introset.derivedSigningKey)
      {
        llarp::LogWarn(
            "publish introset acknowledgement acked a different service");
        return false;
      }
      const llarp_time_t now = llarp::time_now_ms();
      return value.Verify(now);
    }

    void
    PublishServiceJob::Start(const TXOwner &peer)
    {
      parent->DHTSendTo(
          peer.node.as_array(),
          new PublishIntroMessage(introset, peer.txid, relayed, relayOrder));
    }
  }  // namespace dht
}  // namespace llarp

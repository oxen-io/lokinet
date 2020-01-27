#include <dht/publishservicejob.hpp>

#include <dht/context.hpp>
#include <dht/messages/pubintro.hpp>
#include <utility>

namespace llarp
{
  namespace dht
  {
    PublishServiceJob::PublishServiceJob(const TXOwner &asker,
                                         const service::EncryptedIntroSet &I,
                                         AbstractContext *ctx, uint64_t s,
                                         std::set< Key_t > exclude)
        : TX< Key_t, service::EncryptedIntroSet >(
            asker, Key_t{I.derivedSigningKey}, ctx)
        , S(s)
        , dontTell(std::move(exclude))
        , introset(I)
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
      std::vector< Key_t > exclude;
      for(const auto &router : dontTell)
      {
        exclude.push_back(router);
      }
      parent->DHTSendTo(
          peer.node.as_array(),
          new PublishIntroMessage(introset, peer.txid, S, exclude));
    }
  }  // namespace dht
}  // namespace llarp

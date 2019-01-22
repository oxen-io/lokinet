#include <dht/publishservicejob.hpp>

#include <dht/context.hpp>
#include <dht/messages/pubintro.hpp>

namespace llarp
{
  namespace dht
  {
    PublishServiceJob::PublishServiceJob(const TXOwner &asker,
                                         const service::IntroSet &introset,
                                         Context *ctx, uint64_t s,
                                         const std::set< Key_t > &exclude)
        : TX< service::Address, service::IntroSet >(asker, introset.A.Addr(),
                                                    ctx)
        , S(s)
        , dontTell(exclude)
        , I(introset)
    {
    }

    bool
    PublishServiceJob::Validate(const service::IntroSet &introset) const
    {
      if(I.A != introset.A)
      {
        llarp::LogWarn(
            "publish introset acknowledgement acked a different service");
        return false;
      }
      return true;
    }

    void
    PublishServiceJob::Start(const TXOwner &peer)
    {
      std::vector< Key_t > exclude;
      for(const auto &router : dontTell)
      {
        exclude.push_back(router);
      }
      parent->DHTSendTo(peer.node.as_array(),
                        new PublishIntroMessage(I, peer.txid, S, exclude));
    }
  }  // namespace dht
}  // namespace llarp

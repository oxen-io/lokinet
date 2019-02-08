#include <dht/publishservicejob.hpp>

#include <dht/context.hpp>
#include <dht/messages/pubintro.hpp>

namespace llarp
{
  namespace dht
  {
    PublishServiceJob::PublishServiceJob(const TXOwner &asker,
                                         const service::IntroSet &introset,
                                         AbstractContext *ctx, uint64_t s,
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
    PublishServiceJob::ExtractStatus(util::StatusObject &obj) const
    {
      obj.PutString("target", I.A.Name());
      obj.PutInt("S", S);
      util::StatusObject introsetObj;
      I.ExtractStatus(introsetObj);
      obj.PutObject("introset", introsetObj);
      std::vector< std::string > dontTellObj;
      for(const auto &key : dontTell)
        dontTellObj.emplace_back(key.ToHex());
      obj.PutStringArray("exclude", dontTellObj);

      util::StatusObject whoaskedTx;
      whoaskedTx.PutInt("txid", whoasked.txid);
      whoaskedTx.PutString("node", whoasked.node.ToHex());

      obj.PutObject("whoasked", whoaskedTx);
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

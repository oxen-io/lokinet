#include <dht/messages/pubintro.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotintro.hpp>
#include <messages/dht.hpp>
#include <messages/dht_immediate.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace dht
  {
    PublishIntroMessage::~PublishIntroMessage()
    {
    }

    bool
    PublishIntroMessage::DecodeKey(const llarp_buffer_t &key,
                                   llarp_buffer_t *val)
    {
      bool read = false;
      if(llarp_buffer_eq(key, "E"))
      {
        return BEncodeReadList(E, val);
      }
      if(!BEncodeMaybeReadDictEntry("I", I, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("R", R, read, key, val))
        return false;
      if(llarp_buffer_eq(key, "S"))
      {
        read = true;
        hasS = true;
        if(!bencode_read_integer(val, &S))
          return false;
      }
      if(!BEncodeMaybeReadDictInt("T", txID, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, key, val))
        return false;
      return read;
    }

    bool
    PublishIntroMessage::HandleMessage(
        llarp_dht_context *ctx,
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      auto now = ctx->impl.Now();
      if(S > 5)
      {
        llarp::LogWarn("invalid S value ", S, " > 5");
        return false;
      }
      auto &dht = ctx->impl;
      if(!I.Verify(dht.router->crypto(), now))
      {
        llarp::LogWarn("invalid introset: ", I);
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      using namespace std::placeholders;
      shorthash_func shorthash =
          std::bind(&Crypto::shorthash, dht.router->crypto(), _1, _2);
      if(I.W && !I.W->IsValid(shorthash, now))
      {
        llarp::LogWarn("proof of work not good enough for IntroSet");
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }
      llarp::dht::Key_t addr;
      if(!I.A.CalculateAddress(addr.as_array()))
      {
        llarp::LogWarn(
            "failed to calculate hidden service address for PubIntro message");
        return false;
      }

      now += llarp::service::MAX_INTROSET_TIME_DELTA;
      if(I.IsExpired(now))
      {
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }
      dht.services->PutNode(I);
      replies.emplace_back(new GotIntroMessage({I}, txID));
      Key_t peer;
      std::set< Key_t > exclude;
      for(const auto &e : E)
        exclude.insert(e);
      exclude.insert(From);
      exclude.insert(dht.OurKey());
      if(S && dht.nodes->FindCloseExcluding(addr, peer, exclude))
      {
        dht.PropagateIntroSetTo(From, txID, I, peer, S - 1, exclude);
      }
      return true;
    }

    bool
    PublishIntroMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if(!BEncodeWriteDictList("E", E, buf))
        return false;
      if(!BEncodeWriteDictEntry("I", I, buf))
        return false;
      if(!BEncodeWriteDictInt("R", R, buf))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("T", txID, buf))
        return false;
      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;
      return bencode_end(buf);
    }
  }  // namespace dht
}  // namespace llarp

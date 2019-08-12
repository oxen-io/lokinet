#include <dht/messages/pubintro.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotintro.hpp>
#include <messages/dht_immediate.hpp>
#include <router/abstractrouter.hpp>
#include <routing/dht_message.hpp>

namespace llarp
{
  namespace dht
  {
    PublishIntroMessage::~PublishIntroMessage() = default;

    bool
    PublishIntroMessage::DecodeKey(const llarp_buffer_t &key,
                                   llarp_buffer_t *val)
    {
      bool read = false;
      if(key == "E")
      {
        return BEncodeReadList(E, val);
      }
      if(!BEncodeMaybeReadDictEntry("I", I, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("R", R, read, key, val))
        return false;
      if(key == "S")
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
      auto now = ctx->impl->Now();
      if(S > 5)
      {
        llarp::LogWarn("invalid S value ", S, " > 5");
        return false;
      }
      auto &dht = *ctx->impl;
      if(!I.Verify(now))
      {
        llarp::LogWarn("invalid introset: ", I);
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      if(I.W && !I.W->IsValid(now))
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
      dht.services()->PutNode(I);
      replies.emplace_back(new GotIntroMessage({I}, txID));
      Key_t peer;
      std::set< Key_t > exclude;
      for(const auto &e : E)
        exclude.insert(e);
      exclude.insert(From);
      exclude.insert(dht.OurKey());
      if(S && dht.Nodes()->FindCloseExcluding(addr, peer, exclude))
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

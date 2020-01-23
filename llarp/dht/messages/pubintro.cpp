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
    const uint64_t PublishIntroMessage::MaxPropagationDepth = 5;
    PublishIntroMessage::~PublishIntroMessage()             = default;

    bool
    PublishIntroMessage::DecodeKey(const llarp_buffer_t &key,
                                   llarp_buffer_t *val)
    {
      bool read = false;
      if(key == "E")
      {
        return BEncodeReadList(exclude, val);
      }
      if(!BEncodeMaybeReadDictEntry("I", introset, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("S", depth, read, key, val))
        return false;
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

      if(depth > MaxPropagationDepth)
      {
        llarp::LogWarn("invalid propgagation depth value ", depth, " > ",
                       MaxPropagationDepth);
        return false;
      }
      auto &dht = *ctx->impl;
      if(!introset.Verify(now))
      {
        llarp::LogWarn("invalid introset: ", introset);
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      if(introset.W && !introset.W->IsValid(now))
      {
        llarp::LogWarn("proof of work not good enough for IntroSet");
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }
      llarp::dht::Key_t addr;
      if(not introset.A.CalculateAddress(addr.as_array()))
      {
        llarp::LogWarn(
            "failed to calculate hidden service address for PubIntro message");
        return false;
      }

      now += llarp::service::MAX_INTROSET_TIME_DELTA;
      if(introset.IsExpired(now))
      {
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }
      dht.services()->PutNode(introset);
      replies.emplace_back(new GotIntroMessage({introset}, txID));
      Key_t peer;
      std::set< Key_t > exclude_propagate;
      for(const auto &e : exclude)
        exclude_propagate.insert(e);
      exclude_propagate.insert(From);
      exclude_propagate.insert(dht.OurKey());
      if(depth > 0
         && dht.Nodes()->FindCloseExcluding(addr, peer, exclude_propagate))
      {
        dht.PropagateIntroSetTo(From, txID, introset, peer, depth - 1,
                                exclude_propagate);
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
      if(!BEncodeWriteDictList("E", exclude, buf))
        return false;
      if(!BEncodeWriteDictEntry("I", introset, buf))
        return false;
      if(!BEncodeWriteDictInt("S", depth, buf))
        return false;
      if(!BEncodeWriteDictInt("T", txID, buf))
        return false;
      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;
      return bencode_end(buf);
    }
  }  // namespace dht
}  // namespace llarp

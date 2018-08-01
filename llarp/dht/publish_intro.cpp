
#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/pubintro.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    PublishIntroMessage::~PublishIntroMessage()
    {
    }

    bool
    PublishIntroMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
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
    PublishIntroMessage::HandleMessage(llarp_dht_context *ctx,
                                       std::vector< IMessage * > &replies) const
    {
      if(S > 5)
      {
        llarp::LogWarn("invalid S value ", S, " > 5");
        return false;
      }
      auto &dht = ctx->impl;
      if(!I.VerifySignature(&dht.router->crypto))
      {
        llarp::LogWarn("invalid introset signature, ", I);
        return false;
      }
      if(I.W && !I.W->IsValid(dht.router->crypto.shorthash))
      {
        llarp::LogWarn("proof of work not good enough for IntroSet");
        return false;
      }
      llarp::dht::Key_t addr;
      if(!I.A.CalculateAddress(addr))
      {
        llarp::LogWarn(
            "failed to calculate hidden service address for PubIntro message");
        return false;
      }
      dht.services->PutNode(I);
      replies.push_back(new GotIntroMessage({I}, txID));
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
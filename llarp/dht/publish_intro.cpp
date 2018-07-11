
#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/pubintro.hpp>
#include <llarp/messages/dht.hpp>
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
      if(!BEncodeMaybeReadDictInt("V", version, read, key, val))
        return false;
      return read;
    }

    bool
    PublishIntroMessage::HandleMessage(llarp_dht_context *ctx,
                                       std::vector< IMessage * > &replies) const
    {
      auto &dht = ctx->impl;
      if(!I.VerifySignature(&dht.router->crypto))
      {
        llarp::LogWarn("invalid introset signature");
        return false;
      }
      if(I.W && !I.W->IsValid(dht.router->crypto.shorthash))
      {
        llarp::LogWarn("proof of work not good enough for IntroSet");
        return false;
      }
      // TODO: make this smarter (?)
      dht.services->PutNode(I);
      replies.push_back(new GotIntroMessage(txID, &I));
      return true;
    }

    bool
    PublishIntroMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if(!BEncodeWriteDictEntry("I", I, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "R", R))
        return false;
      if(hasS)
      {
        if(!BEncodeWriteDictInt(buf, "S", S))
          return false;
      }
      if(!BEncodeWriteDictInt(buf, "V", LLARP_PROTO_VERSION))
        return false;
      return bencode_end(buf);
    }
  }
}
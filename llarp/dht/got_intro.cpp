
#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/messages/dht.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    GotIntroMessage::GotIntroMessage(uint64_t tx,
                                     const llarp::service::IntroSet *i)
        : IMessage({}), T(tx)
    {
      if(i)
      {
        I.push_back(*i);
      }
    }

    GotIntroMessage::~GotIntroMessage()
    {
    }

    bool
    GotIntroMessage::HandleMessage(llarp_dht_context *ctx,
                                   std::vector< IMessage * > &replies) const
    {
      // TODO: implement me better?
      auto path = ctx->impl.router->paths.GetLocalPathSet(pathID);
      if(path)
      {
        return path->HandleGotIntroMessage(this);
      }
      return false;
    }

    bool
    GotIntroMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
    {
      if(llarp_buffer_eq(key, "I"))
      {
        return BEncodeReadList(I, buf);
      }
      bool read = false;
      if(!BEncodeMaybeReadDictInt("T", T, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, key, buf))
        return false;
      return read;
    }

    bool
    GotIntroMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "G"))
        return false;
      if(!BEncodeWriteDictList("I", I, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "T", T))
        return false;
      if(!BEncodeWriteDictInt(buf, "V", version))
        return false;
      return bencode_end(buf);
    }
  }
}
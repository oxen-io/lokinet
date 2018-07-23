
#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/messages/dht.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    GotIntroMessage::GotIntroMessage(
        const std::vector< llarp::service::IntroSet > &results, uint64_t tx)
        : IMessage({}), I(results), T(tx)
    {
    }

    GotIntroMessage::~GotIntroMessage()
    {
    }

    bool
    GotIntroMessage::HandleMessage(llarp_dht_context *ctx,
                                   std::vector< IMessage * > &replies) const
    {
      auto &dht   = ctx->impl;
      auto crypto = &dht.router->crypto;

      for(const auto &introset : I)
      {
        if(!introset.VerifySignature(crypto))
        {
          llarp::LogWarn(
              "Invalid introset signature while handling direct GotIntro "
              "from ",
              From);
          return false;
        }
      }
      auto pending = dht.FindPendingTX(From, T);
      if(pending)
      {
        pending->FoundIntros(I);
        dht.RemovePendingLookup(From, T);
        return true;
      }
      else
      {
        llarp::LogWarn("got GIM from ", From,
                       " with no previous pending transaction, txid=", T);
        return false;
      }
    }

    bool
    RelayedGotIntroMessage::HandleMessage(
        llarp_dht_context *ctx, std::vector< IMessage * > &replies) const
    {
      // TODO: implement me better?
      auto pathset = ctx->impl.router->paths.GetLocalPathSet(pathID);
      if(pathset)
      {
        return pathset->HandleGotIntroMessage(this);
      }
      llarp::LogWarn("No path for got intro message pathid=", pathID);
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
      if(!BEncodeWriteDictInt("T", T, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      return bencode_end(buf);
    }
  }  // namespace dht
}  // namespace llarp
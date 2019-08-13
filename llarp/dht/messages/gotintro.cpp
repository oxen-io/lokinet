#include <dht/messages/gotintro.hpp>

#include <dht/context.hpp>
#include <memory>
#include <path/path_context.hpp>
#include <router/abstractrouter.hpp>
#include <routing/dht_message.hpp>
#include <utility>

namespace llarp
{
  namespace dht
  {
    GotIntroMessage::GotIntroMessage(std::vector< service::IntroSet > results,
                                     uint64_t tx)
        : IMessage({}), I(std::move(results)), T(tx)
    {
    }

    bool
    GotIntroMessage::HandleMessage(
        llarp_dht_context *ctx,
        ABSL_ATTRIBUTE_UNUSED std::vector< std::unique_ptr< IMessage > >
            &replies) const
    {
      auto &dht = *ctx->impl;

      for(const auto &introset : I)
      {
        if(!introset.Verify(dht.Now()))
        {
          LogWarn(
              "Invalid introset while handling direct GotIntro "
              "from ",
              From);
          return false;
        }
      }
      TXOwner owner(From, T);
      auto tagLookup = dht.pendingTagLookups().GetPendingLookupFrom(owner);
      if(tagLookup)
      {
        dht.pendingTagLookups().Found(owner, tagLookup->target, I);
        return true;
      }
      auto serviceLookup =
          dht.pendingIntrosetLookups().GetPendingLookupFrom(owner);
      if(serviceLookup)
      {
        if(I.size())
        {
          dht.pendingIntrosetLookups().Found(owner, serviceLookup->target, I);
        }
        else
        {
          dht.pendingIntrosetLookups().NotFound(owner, K);
        }
        return true;
      }
      LogError("no pending TX for GIM from ", From, " txid=", T);
      return false;
    }

    bool
    RelayedGotIntroMessage::HandleMessage(
        llarp_dht_context *ctx,
        __attribute__((unused))
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      // TODO: implement me better?
      auto pathset =
          ctx->impl->GetRouter()->pathContext().GetLocalPathSet(pathID);
      if(pathset)
      {
        auto copy = std::make_shared< const RelayedGotIntroMessage >(*this);
        return pathset->HandleGotIntroMessage(copy);
      }
      LogWarn("No path for got intro message pathid=", pathID);
      return false;
    }

    bool
    GotIntroMessage::DecodeKey(const llarp_buffer_t &key, llarp_buffer_t *buf)
    {
      if(key == "I")
      {
        return BEncodeReadList(I, buf);
      }
      if(key == "K")
      {
        if(K)  // duplicate key?
          return false;
        K = std::make_unique< dht::Key_t >();
        return K->BDecode(buf);
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
      if(K)
      {
        if(!BEncodeWriteDictEntry("K", *K.get(), buf))
          return false;
      }
      if(!BEncodeWriteDictInt("T", T, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      return bencode_end(buf);
    }
  }  // namespace dht
}  // namespace llarp

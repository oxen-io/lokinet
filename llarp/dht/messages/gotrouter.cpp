#include <dht/context.hpp>
#include <dht/messages/gotrouter.hpp>

#include <memory>
#include <path/path_context.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace dht
  {
    GotRouterMessage::~GotRouterMessage() = default;

    bool
    GotRouterMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      // message type
      if(!BEncodeWriteDictMsgType(buf, "A", "S"))
        return false;

      if(K)
      {
        if(!BEncodeWriteDictEntry("K", *K.get(), buf))
          return false;
      }

      // near
      if(N.size())
      {
        if(!BEncodeWriteDictList("N", N, buf))
          return false;
      }

      if(!BEncodeWriteDictList("R", R, buf))
        return false;

      // txid
      if(!BEncodeWriteDictInt("T", txid, buf))
        return false;

      // version
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    GotRouterMessage::DecodeKey(const llarp_buffer_t &key, llarp_buffer_t *val)
    {
      if(key == "K")
      {
        if(K)  // duplicate key?
          return false;
        K = std::make_unique< dht::Key_t >();
        return K->BDecode(val);
      }
      if(key == "N")
      {
        return BEncodeReadList(N, val);
      }
      if(key == "R")
      {
        return BEncodeReadList(R, val);
      }
      if(key == "T")
      {
        return bencode_read_integer(val, &txid);
      }
      bool read = false;
      if(!BEncodeMaybeVerifyVersion("V", version, LLARP_PROTO_VERSION, read,
                                    key, val))
        return false;

      return read;
    }

    bool
    GotRouterMessage::HandleMessage(
        llarp_dht_context *ctx,
        __attribute__((unused))
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      auto &dht = *ctx->impl;
      if(relayed)
      {
        auto pathset =
            ctx->impl->GetRouter()->pathContext().GetLocalPathSet(pathID);
        auto copy = std::make_shared< const GotRouterMessage >(*this);
        return pathset && pathset->HandleGotRouterMessage(copy);
      }
      // not relayed
      const TXOwner owner(From, txid);

      if(dht.pendingExploreLookups().HasPendingLookupFrom(owner))
      {
        LogDebug("got ", N.size(), " results in GRM for explore");
        if(N.size() == 0)
          dht.pendingExploreLookups().NotFound(owner, K);
        else
        {
          dht.pendingExploreLookups().Found(owner, From.as_array(), N);
        }
        return true;
      }
      // not explore lookup
      if(dht.pendingRouterLookups().HasPendingLookupFrom(owner))
      {
        LogDebug("got ", R.size(), " results in GRM for lookup");
        if(R.size() == 0)
          dht.pendingRouterLookups().NotFound(owner, K);
        else
          dht.pendingRouterLookups().Found(owner, R[0].pubkey, R);
        return true;
      }
      // store if valid
      for(const auto &rc : R)
      {
        if(not dht.GetRouter().rcLookupHandler().CheckRC(rc))
          return false;
      }
      return true;
    }
  }  // namespace dht
}  // namespace llarp


#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    GotRouterMessage::~GotRouterMessage()
    {
      for(auto rc : R)
        llarp_rc_free(&rc);
      R.clear();
    }

    bool
    GotRouterMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      // message type
      if(!BEncodeWriteDictMsgType(buf, "A", "S"))
        return false;

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
    GotRouterMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
    {
      if(llarp_buffer_eq(key, "N"))
      {
        return BEncodeReadList(N, val);
      }
      if(llarp_buffer_eq(key, "R"))
      {
        return BEncodeReadList(R, val);
      }
      if(llarp_buffer_eq(key, "T"))
      {
        return bencode_read_integer(val, &txid);
      }
      bool read = false;
      if(!BEncodeMaybeReadVersion("V", version, LLARP_PROTO_VERSION, read, key,
                                  val))
        return false;

      return read;
    }

    bool
    GotRouterMessage::HandleMessage(llarp_dht_context *ctx,
                                    std::vector< IMessage * > &replies) const
    {
      auto &dht = ctx->impl;
      if(relayed)
      {
        auto pathset = ctx->impl.router->paths.GetLocalPathSet(pathID);
        if(pathset)
        {
          return pathset->HandleGotRouterMessage(this);
        }
      }
      TXOwner owner(From, txid);

      if(dht.pendingExploreLookups.HasPendingLookupFrom(owner))
      {
        if(N.size() == 0)
          dht.pendingExploreLookups.NotFound(owner);
        else
          dht.pendingExploreLookups.Found(owner, From, N);
        return true;
      }

      if(!dht.pendingRouterLookups.HasPendingLookupFrom(owner))
      {
        llarp::LogWarn("Unwarrented GRM from ", From, " txid=", txid);
        return false;
      }
      if(R.size() == 1)
        dht.pendingRouterLookups.Found(owner, R[0].pubkey, R);
      else
        dht.pendingRouterLookups.NotFound(owner);
      return true;
    }
  }  // namespace dht
}  // namespace llarp

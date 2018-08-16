
#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    GotRouterMessage::~GotRouterMessage()
    {
      for(auto &rc : R)
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
      SearchJob *pending = dht.FindPendingTX(From, txid);
      if(pending)
      {
        if(R.size())
        {
          pending->FoundRouter(&R[0]);
          if(pending->requester != dht.OurKey())
          {
            replies.push_back(new GotRouterMessage(
                pending->target, pending->requesterTX, &R[0], false));
          }
        }
        else
        {
          // iterate to next closest peer
          Key_t nextPeer;
          pending->exclude.insert(From);
          if(pending->exclude.size() < 3
             && dht.nodes->FindCloseExcluding(pending->target, nextPeer,
                                              pending->exclude))
          {
            llarp::LogInfo(pending->target, " was not found via ", From,
                           " iterating to next peer ", nextPeer,
                           " already asked ", pending->exclude.size(),
                           " other peers");
            // REVIEW: is this ok to relay the pending->job as the current job
            // (seems to make things work)
            dht.LookupRouter(pending->target, pending->requester,
                             pending->requesterTX, nextPeer, pending->job, true,
                             pending->exclude);
          }
          else
          {
            llarp::LogInfo(pending->target, " was not found via ", From,
                           " and we won't look it up");
            pending->FoundRouter(nullptr);
            if(pending->requester != dht.OurKey())
            {
              replies.push_back(new GotRouterMessage(
                  pending->target, pending->requesterTX, nullptr, false));
            }
          }
        }
        dht.RemovePendingTX(From, txid);
        return true;
      }
      llarp::LogWarn(
          "Got response for DHT transaction we are not tracking, txid=", txid);
      return false;
    }
  }  // namespace dht
}  // namespace llarp

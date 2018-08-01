
#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/findrouter.hpp>
#include <llarp/dht/messages/gotrouter.hpp>
#include <llarp/messages/dht.hpp>
#include "router.hpp"

namespace llarp
{
  namespace dht
  {
    struct PathLookupInformer
    {
      llarp_router *router;
      PathID_t pathID;
      uint64_t txid;

      PathLookupInformer(llarp_router *r, const PathID_t &id, uint64_t tx)
          : router(r), pathID(id), txid(tx)
      {
      }

      void
      SendReply(llarp::routing::IMessage *msg)
      {
        auto path = router->paths.GetByUpstream(router->pubkey(), pathID);
        if(path == nullptr)
        {
          llarp::LogWarn("Path not found for relayed DHT message txid=", txid,
                         " pathid=", pathID);
          return;
        }
        if(!path->SendRoutingMessage(msg, router))
          llarp::LogWarn("Failed to send reply for relayed DHT message txid=",
                         txid, "pathid=", pathID);
      }

      static void
      InformReply(llarp_router_lookup_job *job)
      {
        PathLookupInformer *self =
            static_cast< PathLookupInformer * >(job->user);
        llarp::routing::DHTMessage reply;
        if(job->found)
        {
          if(llarp_rc_verify_sig(&self->router->crypto, &job->result))
          {
            reply.M.push_back(
                new GotRouterMessage(job->target, self->txid, &job->result));
          }
          llarp_rc_free(&job->result);
          llarp_rc_clear(&job->result);
        }
        else
        {
          reply.M.push_back(
              new GotRouterMessage(job->target, self->txid, nullptr));
        }
        self->SendReply(&reply);
        // TODO: is this okay?
        delete self;
        delete job;
      }
    };

    bool
    RelayedFindRouterMessage::HandleMessage(
        llarp_dht_context *ctx, std::vector< IMessage * > &replies) const
    {
      auto &dht = ctx->impl;
      /// lookup for us, send an immeidate reply
      if(K == dht.OurKey())
      {
        auto path = dht.router->paths.GetByUpstream(K, pathID);
        if(path)
        {
          replies.push_back(new GotRouterMessage(K, txid, &dht.router->rc));
          return true;
        }
        return false;
      }
      llarp_router_lookup_job *job = new llarp_router_lookup_job;
      PathLookupInformer *informer =
          new PathLookupInformer(dht.router, pathID, txid);
      job->user  = informer;
      job->hook  = &PathLookupInformer::InformReply;
      job->found = false;
      llarp_rc_clear(&job->result);
      job->dht = ctx;
      memcpy(job->target, K, sizeof(job->target));
      Key_t peer;
      if(dht.nodes->FindClosest(K, peer))
        dht.LookupRouter(K, dht.OurKey(), txid, peer, job);
      return false;
    }

    FindRouterMessage::~FindRouterMessage()
    {
    }

    bool
    FindRouterMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      // message type
      if(!bencode_write_bytestring(buf, "A", 1))
        return false;
      if(!bencode_write_bytestring(buf, "R", 1))
        return false;

      // iterative or not?
      if(!bencode_write_bytestring(buf, "I", 1))
        return false;
      if(!bencode_write_uint64(buf, iterative ? 1 : 0))
        return false;

      // key
      if(!bencode_write_bytestring(buf, "K", 1))
        return false;
      if(!bencode_write_bytestring(buf, K.data(), K.size()))
        return false;

      // txid
      if(!bencode_write_bytestring(buf, "T", 1))
        return false;
      if(!bencode_write_uint64(buf, txid))
        return false;

      // version
      if(!bencode_write_bytestring(buf, "V", 1))
        return false;
      if(!bencode_write_uint64(buf, version))
        return false;

      return bencode_end(buf);
    }

    bool
    FindRouterMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
    {
      llarp_buffer_t strbuf;

      if(llarp_buffer_eq(key, "I"))
      {
        uint64_t result;
        if(!bencode_read_integer(val, &result))
          return false;

        iterative = result != 0;
        return true;
      }
      if(llarp_buffer_eq(key, "K"))
      {
        if(!bencode_read_string(val, &strbuf))
          return false;
        if(strbuf.sz != K.size())
          return false;

        memcpy(K.data(), strbuf.base, K.size());
        return true;
      }
      if(llarp_buffer_eq(key, "T"))
      {
        return bencode_read_integer(val, &txid);
      }
      if(llarp_buffer_eq(key, "V"))
      {
        return bencode_read_integer(val, &version);
      }
      return false;
    }

    bool
    FindRouterMessage::HandleMessage(llarp_dht_context *ctx,
                                     std::vector< IMessage * > &replies) const
    {
      auto &dht = ctx->impl;
      if(!dht.allowTransit)
      {
        llarp::LogWarn("Got DHT lookup from ", From,
                       " when we are not allowing dht transit");
        return false;
      }
      auto pending = dht.FindPendingTX(From, txid);
      if(pending)
      {
        llarp::LogWarn("Got duplicate DHT lookup from ", From, " txid=", txid);
        return false;
      }
      dht.LookupRouterRelayed(From, txid, K, !iterative, replies);
      return true;
    }
  }  // namespace dht
}  // namespace llarp
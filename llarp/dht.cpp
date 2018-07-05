#include <llarp/bencode.hpp>
#include <llarp/dht.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include "router.hpp"
#include "router_contact.hpp"

#include <sodium.h>

#include <algorithm>  // std::find
#include <set>

namespace llarp
{
  DHTImmeidateMessage::~DHTImmeidateMessage()
  {
    for(auto &msg : msgs)
      delete msg;
    msgs.clear();
  }

  bool
  DHTImmeidateMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    if(llarp_buffer_eq(key, "m"))
      return llarp::dht::DecodeMesssageList(remote.data(), buf, msgs);
    if(llarp_buffer_eq(key, "v"))
    {
      if(!bencode_read_integer(buf, &version))
        return false;
      return version == LLARP_PROTO_VERSION;
    }
    // bad key
    return false;
  }

  bool
  DHTImmeidateMessage::BEncode(llarp_buffer_t *buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    // message type
    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!bencode_write_bytestring(buf, "m", 1))
      return false;

    // dht messages
    if(!bencode_write_bytestring(buf, "m", 1))
      return false;
    // begin list
    if(!bencode_start_list(buf))
      return false;
    for(const auto &msg : msgs)
    {
      if(!msg->BEncode(buf))
        return false;
    }
    // end list
    if(!bencode_end(buf))
      return false;

    // protocol version
    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  DHTImmeidateMessage::HandleMessage(llarp_router *router) const
  {
    DHTImmeidateMessage *reply = new DHTImmeidateMessage(remote);
    bool result                = true;
    for(auto &msg : msgs)
    {
      result &= msg->HandleMessage(router->dht, reply->msgs);
    }
    return result && router->SendToOrQueue(remote.data(), reply);
  }

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
      SendReply(const llarp::routing::IMessage *msg)
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

    /// variant of FindRouterMessage relayed via path
    struct RelayedFindRouterMessage : public FindRouterMessage
    {
      RelayedFindRouterMessage(const Key_t &from) : FindRouterMessage(from)
      {
      }

      /// handle a relayed FindRouterMessage, do a lookup on the dht and inform
      /// the path of the result
      /// TODO: smart path expiration logic needs to be implemented
      virtual bool
      HandleMessage(llarp_dht_context *ctx, std::vector< IMessage * > &replies)
      {
        auto &dht = ctx->impl;
        /// lookup for us, send an immeidate reply
        if(K == dht.OurKey())
        {
          auto path = dht.router->paths.GetByUpstream(K, pathID);
          if(path)
          {
            llarp::routing::DHTMessage reply;
            reply.M.push_back(new GotRouterMessage(K, txid, &dht.router->rc));
            return path->SendRoutingMessage(&reply, dht.router);
          }
          return false;
        }
        llarp_router_lookup_job *job = new llarp_router_lookup_job;
        PathLookupInformer *informer =
            new PathLookupInformer(dht.router, pathID, txid);
        job->user  = informer;
        job->hook  = &PathLookupInformer::InformReply;
        job->found = false;
        job->dht   = ctx;
        memcpy(job->target, K, sizeof(job->target));
        Key_t peer;
        if(dht.nodes->FindClosest(K, peer))
          dht.LookupRouter(K, dht.OurKey(), txid, peer, job);
        return false;
      }
    };

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
      if(!BEncodeMaybeReadDictInt("V", V, read, key, val))
        return false;
      return read;
    }

    bool
    PublishIntroMessage::HandleMessage(llarp_dht_context *ctx,
                                       std::vector< IMessage * > &replies) const
    {
      // TODO: implement me
      return false;
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
      if(!BEncodeWriteDictInt(buf, "T", txid))
        return false;

      // version
      if(!BEncodeWriteDictInt(buf, "V", version))
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
      auto &dht    = ctx->impl;
      auto pending = dht.FindPendingTX(From, txid);
      if(pending)
      {
        if(R.size())
        {
          pending->Completed(&R[0]);
          if(pending->requester != dht.OurKey())
          {
            replies.push_back(new GotRouterMessage(
                pending->target, pending->requesterTX, &R[0]));
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
            dht.LookupRouter(pending->target, pending->requester,
                             pending->requesterTX, nextPeer, nullptr, true,
                             pending->exclude);
          }
          else
          {
            llarp::LogInfo(pending->target, " was not found via ", From,
                           " and we won't look it up");
            pending->Completed(nullptr);
            if(pending->requester != dht.OurKey())
            {
              replies.push_back(new GotRouterMessage(
                  pending->target, pending->requesterTX, nullptr));
            }
          }
        }
        dht.RemovePendingLookup(From, txid);
        return true;
      }
      llarp::LogWarn(
          "Got response for DHT transaction we are not tracking, txid=", txid);
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
      if(!bencode_write_int(buf, iterative ? 1 : 0))
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

    struct MessageDecoder
    {
      const Key_t &From;
      bool firstKey = true;
      IMessage *msg = nullptr;
      bool relayed  = false;

      MessageDecoder(const Key_t &from) : From(from)
      {
      }

      static bool
      on_key(dict_reader *r, llarp_buffer_t *key)
      {
        llarp_buffer_t strbuf;
        MessageDecoder *dec = static_cast< MessageDecoder * >(r->user);
        // check for empty dict
        if(!key)
          return !dec->firstKey;

        // first key
        if(dec->firstKey)
        {
          if(!llarp_buffer_eq(*key, "A"))
            return false;
          if(!bencode_read_string(r->buffer, &strbuf))
            return false;
          // bad msg size?
          if(strbuf.sz != 1)
            return false;
          switch(*strbuf.base)
          {
            case 'R':
              if(dec->relayed)
                dec->msg = new RelayedFindRouterMessage(dec->From);
              else
                dec->msg = new FindRouterMessage(dec->From);
              break;
            case 'S':
              if(dec->relayed)
              {
                llarp::LogWarn(
                    "GotRouterMessage found when parsing relayed DHT "
                    "message");
                return false;
              }
              else
                dec->msg = new GotRouterMessage(dec->From);
              break;
            case 'I':
              dec->msg = new PublishIntroMessage();
              break;
            default:
              llarp::LogWarn("unknown dht message type: ", (char)*strbuf.base);
              // bad msg type
              return false;
          }
          dec->firstKey = false;
          return dec->msg != nullptr;
        }
        else
          return dec->msg->DecodeKey(*key, r->buffer);
      }
    };

    IMessage *
    DecodeMesssage(const Key_t &from, llarp_buffer_t *buf, bool relayed)
    {
      MessageDecoder dec(from);
      dec.relayed = relayed;
      dict_reader r;
      r.user   = &dec;
      r.on_key = &MessageDecoder::on_key;
      if(bencode_read_dict(buf, &r))
        return dec.msg;
      else
      {
        if(dec.msg)
          delete dec.msg;
        return nullptr;
      }
    }

    struct ListDecoder
    {
      ListDecoder(const Key_t &from, std::vector< IMessage * > &list)
          : From(from), l(list){};

      bool relayed = false;
      const Key_t &From;
      std::vector< IMessage * > &l;

      static bool
      on_item(list_reader *r, bool has)
      {
        ListDecoder *dec = static_cast< ListDecoder * >(r->user);
        if(!has)
          return true;
        auto msg = DecodeMesssage(dec->From, r->buffer, dec->relayed);
        if(msg)
        {
          dec->l.push_back(msg);
          return true;
        }
        else
          return false;
      }
    };

    bool
    DecodeMesssageList(const Key_t &from, llarp_buffer_t *buf,
                       std::vector< IMessage * > &list, bool relayed)
    {
      ListDecoder dec(from, list);
      dec.relayed = relayed;
      list_reader r;
      r.user    = &dec;
      r.on_item = &ListDecoder::on_item;
      return bencode_read_list(buf, &r);
    }

    SearchJob::SearchJob()
    {
      started = 0;
      requester.Zero();
      target.Zero();
    }

    SearchJob::SearchJob(const Key_t &asker, uint64_t tx, const Key_t &key,
                         llarp_router_lookup_job *j,
                         const std::set< Key_t > &excludes)
        : job(j)
        , started(llarp_time_now_ms())
        , requester(asker)
        , requesterTX(tx)
        , target(key)
        , exclude(excludes)
    {
    }

    void
    SearchJob::Completed(const llarp_rc *router, bool timeout) const
    {
      if(job && job->hook)
      {
        if(router)
        {
          job->found = true;
          llarp_rc_copy(&job->result, router);
        }
        job->hook(job);
      }
    }

    bool
    SearchJob::IsExpired(llarp_time_t now) const
    {
      return now - started >= JobTimeout;
    }

    Context::Context()
    {
      randombytes((byte_t *)&ids, sizeof(uint64_t));
    }

    Context::~Context()
    {
      if(nodes)
        delete nodes;
      if(services)
        delete services;
    }

    void
    Context::handle_cleaner_timer(void *u, uint64_t orig, uint64_t left)
    {
      if(left)
        return;
      Context *ctx = static_cast< Context * >(u);

      ctx->CleanupTX();
      ctx->ScheduleCleanupTimer();
    }

    void
    Context::LookupRouterRelayed(const Key_t &requester, uint64_t txid,
                                 const Key_t &target, bool recursive,
                                 std::vector< IMessage * > &replies)
    {
      if(target == ourKey)
      {
        // we are the target, give them our RC
        replies.push_back(new GotRouterMessage(requester, txid, &router->rc));
        return;
      }
      Key_t next;
      std::set< Key_t > excluding = {requester, ourKey};
      if(nodes->FindCloseExcluding(target, next, excluding))
      {
        if(next == target)
        {
          // we know it
          replies.push_back(
              new GotRouterMessage(requester, txid, nodes->nodes[target].rc));
        }
        else if(recursive)  // are we doing a recursive lookup?
        {
          if((requester ^ target) < (ourKey ^ target))
          {
            // we aren't closer to the target than next hop
            // so we won't ask neighboor recursively, tell them we don't have it
            llarp::LogInfo("we aren't closer to ", target, " than ", next,
                           " so we end it here");
            replies.push_back(new GotRouterMessage(requester, txid, nullptr));
          }
          else
          {
            // yeah, ask neighboor recursively
            LookupRouter(target, requester, txid, next);
          }
        }
        else  // otherwise tell them we don't have it
        {
          llarp::LogInfo("we don't have ", target,
                         " and this was an iterative request so telling ",
                         requester, " that we don't have it");
          replies.push_back(new GotRouterMessage(requester, txid, nullptr));
        }
      }
      else
      {
        // we don't know it and have no closer peers
        llarp::LogInfo("we don't have ", target,
                       " and have no closer peers so telling ", requester,
                       " that we don't have it");
        replies.push_back(new GotRouterMessage(requester, txid, nullptr));
      }
    }

    void
    Context::RemovePendingLookup(const Key_t &owner, uint64_t id)
    {
      TXOwner search;
      search.node = owner;
      search.txid = id;
      auto itr    = pendingTX.find(search);
      if(itr == pendingTX.end())
        return;
      pendingTX.erase(itr);
    }

    SearchJob *
    Context::FindPendingTX(const Key_t &owner, uint64_t id)
    {
      TXOwner search;
      search.node = owner;
      search.txid = id;
      auto itr    = pendingTX.find(search);
      if(itr == pendingTX.end())
        return nullptr;
      else
        return &itr->second;
    }

    void
    Context::CleanupTX()
    {
      auto now = llarp_time_now_ms();
      llarp::LogDebug("DHT tick");

      auto itr = pendingTX.begin();
      while(itr != pendingTX.end())
      {
        if(itr->second.IsExpired(now))
        {
          itr->second.Completed(nullptr, true);
          itr = pendingTX.erase(itr);
        }
        else
          ++itr;
      }
    }

    void
    Context::Init(const Key_t &us, llarp_router *r)
    {
      router   = r;
      ourKey   = us;
      nodes    = new Bucket< RCNode >(ourKey);
      services = new Bucket< ISNode >(ourKey);
      llarp::LogDebug("intialize dht with key ", ourKey);
    }

    void
    Context::ScheduleCleanupTimer()
    {
      llarp_logic_call_later(router->logic,
                             {1000, this, &handle_cleaner_timer});
    }

    bool
    Context::RelayRequestForPath(const llarp::PathID_t &id, const IMessage *msg)
    {
      llarp::routing::DHTMessage reply;
      if(!msg->HandleMessage(router->dht, reply.M))
        return false;
      auto path = router->paths.GetByUpstream(router->pubkey(), id);
      return path && path->SendRoutingMessage(&reply, router);
    }

    void
    Context::LookupRouter(const Key_t &target, const Key_t &whoasked,
                          uint64_t txid, const Key_t &askpeer,
                          llarp_router_lookup_job *job, bool iterative,
                          std::set< Key_t > excludes)
    {
      if(target.IsZero() || whoasked.IsZero() || askpeer.IsZero())
      {
        return;
      }
      auto id = ++ids;
      TXOwner ownerKey;
      ownerKey.node = askpeer;
      ownerKey.txid = id;
      if(txid == 0)
        txid = id;

      pendingTX[ownerKey] = SearchJob(whoasked, txid, target, job, excludes);

      llarp::LogInfo("Asking ", askpeer, " for router ", target, " for ",
                     whoasked);
      auto msg          = new llarp::DHTImmeidateMessage(askpeer);
      auto dhtmsg       = new FindRouterMessage(askpeer, target, id);
      dhtmsg->iterative = iterative;
      msg->msgs.push_back(dhtmsg);
      router->SendToOrQueue(askpeer, msg);
    }

    void
    Context::LookupRouterViaJob(llarp_router_lookup_job *job)
    {
      Key_t peer;
      if(nodes->FindClosest(job->target, peer))
        LookupRouter(job->target, ourKey, 0, peer, job);
      else if(job->hook)
      {
        job->found = false;
        job->hook(job);
      }
    }

    void
    Context::queue_router_lookup(void *user)
    {
      llarp_router_lookup_job *job =
          static_cast< llarp_router_lookup_job * >(user);
      job->dht->impl.LookupRouterViaJob(job);
    }

  }  // namespace dht
}  // namespace llarp

llarp_dht_context::llarp_dht_context(llarp_router *router)
{
  parent = router;
}

extern "C"
{
  struct llarp_dht_context *
  llarp_dht_context_new(struct llarp_router *router)
  {
    return new llarp_dht_context(router);
  }

  void
  llarp_dht_context_free(struct llarp_dht_context *ctx)
  {
    delete ctx;
  }

  void
  llarp_dht_put_peer(struct llarp_dht_context *ctx, struct llarp_rc *rc)

  {
    llarp::dht::RCNode n(rc);
    ctx->impl.nodes->PutNode(n);
  }

  void
  llarp_dht_remove_peer(struct llarp_dht_context *ctx, const byte_t *id)
  {
    llarp::dht::Key_t k = id;
    ctx->impl.nodes->DelNode(k);
  }

  void
  llarp_dht_set_msg_handler(struct llarp_dht_context *ctx,
                            llarp_dht_msg_handler handler)
  {
    ctx->impl.custom_handler = handler;
  }

  void
  llarp_dht_allow_transit(llarp_dht_context *ctx)
  {
    ctx->impl.allowTransit = true;
  }

  void
  llarp_dht_context_start(struct llarp_dht_context *ctx, const byte_t *key)
  {
    ctx->impl.Init(key, ctx->parent);
  }

  void
  llarp_dht_lookup_router(struct llarp_dht_context *ctx,
                          struct llarp_router_lookup_job *job)
  {
    job->dht   = ctx;
    job->found = false;
    llarp_logic_queue_job(ctx->parent->logic,
                          {job, &llarp::dht::Context::queue_router_lookup});
  }
}

#include <llarp/bencode.h>
#include <llarp/dht.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include "router.hpp"

#include <sodium.h>

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
      result &= msg->HandleMessage(router, reply->msgs);
    }
    return result && router->SendToOrQueue(remote.data(), {reply});
  }

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
      return false;
    }

    bool
    GotRouterMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
    {
      return false;
    }

    bool
    GotRouterMessage::HandleMessage(llarp_router *router,
                                    std::vector< IMessage * > &replies) const
    {
      auto &dht    = router->dht->impl;
      auto pending = dht.FindPendingTX(From, txid);
      if(pending)
      {
        if(R.size())
          pending->Completed(&R[0]);
        else
          pending->Completed(nullptr);

        dht.RemovePendingLookup(From, txid);
        return true;
      }
      llarp::Warn("Got response for DHT transaction we are not tracking, txid=",
                  txid);
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
    FindRouterMessage::HandleMessage(llarp_router *router,
                                     std::vector< IMessage * > &replies) const
    {
      auto &dht    = router->dht->impl;
      auto pending = dht.FindPendingTX(From, txid);
      if(pending)
      {
        llarp::Warn("Got duplicate DHT lookup from ", From, " txid=", txid);
        return false;
      }
      dht.LookupRouterRelayed(From, txid, K, replies);
      return true;
    }

    struct MessageDecoder
    {
      const Key_t &From;
      bool firstKey = true;
      IMessage *msg = nullptr;

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
              dec->msg = new FindRouterMessage(dec->From);
              break;
            case 'S':
              dec->msg = new GotRouterMessage(dec->From);
              break;
            default:
              llarp::Warn("unknown dht message type: ", (char)*strbuf.base);
              // bad msg type
              return false;
          }
          dec->firstKey = false;
          return true;
        }
        else
          return dec->msg->DecodeKey(*key, r->buffer);
      }
    };

    IMessage *
    DecodeMesssage(const Key_t &from, llarp_buffer_t *buf)
    {
      MessageDecoder dec(from);
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

      const Key_t &From;
      std::vector< IMessage * > &l;

      static bool
      on_item(list_reader *r, bool has)
      {
        ListDecoder *dec = static_cast< ListDecoder * >(r->user);
        if(!has)
          return true;
        auto msg = DecodeMesssage(dec->From, r->buffer);
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
                       std::vector< IMessage * > &list)
    {
      ListDecoder dec(from, list);

      list_reader r;
      r.user    = &dec;
      r.on_item = &ListDecoder::on_item;
      return bencode_read_list(buf, &r);
    }

    SearchJob::SearchJob()
    {
      started = 0;
      requestor.Zero();
      target.Zero();
    }

    SearchJob::SearchJob(const Key_t &asker, const Key_t &key,
                         llarp_router_lookup_job *j)
        : started(llarp_time_now_ms()), requestor(asker), target(key), job(j)
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

    bool
    Bucket::FindClosest(const Key_t &target, Key_t &result) const
    {
      auto itr = nodes.lower_bound(target);
      if(itr == nodes.end())
        return false;

      result = itr->second.ID;
      return true;
    }

    bool
    Bucket::FindCloseExcluding(const Key_t &target, Key_t &result,
                               const Key_t &exclude) const
    {
      auto itr = nodes.lower_bound(target);
      if(itr == nodes.end())
        return false;
      if(itr->second.ID == exclude)
        ++itr;
      if(itr == nodes.end())
        return false;
      result = itr->second.ID;
      return true;
    }

    Context::Context()
    {
      randombytes((byte_t *)&ids, sizeof(uint64_t));
    }

    Context::~Context()
    {
      if(nodes)
        delete nodes;
    }

    void
    Context::handle_cleaner_timer(void *u, uint64_t orig, uint64_t left)
    {
      if(left)
        return;
      Context *ctx = static_cast< Context * >(u);

      ctx->CleanupTX();
    }

    void
    Context::LookupRouterRelayed(const Key_t &requester, uint64_t txid,
                                 const Key_t &target,
                                 std::vector< IMessage * > &replies)
    {
      if(target == ourKey)
      {
        // we are the target, give them our RC
        replies.push_back(new GotRouterMessage(requester, txid, &router->rc));
        return;
      }
      Key_t next = ourKey;
      nodes->FindClosest(target, next);
      if(next == ourKey)
      {
        // we are closest and don't have a match
        replies.push_back(new GotRouterMessage(requester, txid, nullptr));
        return;
      }
      if(next == target)
      {
        // we know it
        replies.push_back(
            new GotRouterMessage(requester, txid, nodes->nodes[target].rc));
        return;
      }

      // ask neighbor
      LookupRouter(target, requester, next);
    }

    void
    Context::RemovePendingLookup(const Key_t &owner, uint64_t id)
    {
      auto itr = pendingTX.find({owner, id});
      if(itr == pendingTX.end())
        return;
      pendingTX.erase(itr);
    }

    SearchJob *
    Context::FindPendingTX(const Key_t &owner, uint64_t id)
    {
      auto itr = pendingTX.find({owner, id});
      if(itr == pendingTX.end())
        return nullptr;
      else
        return &itr->second;
    }

    void
    Context::CleanupTX()
    {
      auto now = llarp_time_now_ms();
      llarp::Debug("DHT tick");
      std::set< TXOwner > expired;

      for(auto &item : pendingTX)
        if(item.second.IsExpired(now))
          expired.insert(item.first);

      for(const auto &e : expired)
      {
        pendingTX[e].Completed(nullptr, true);
        RemovePendingLookup(e.requester, e.txid);
        if(e.requester != ourKey)
        {
          // inform not found
          llarp::DHTImmeidateMessage msg(e.requester);
          msg.msgs.push_back(
              new GotRouterMessage(e.requester, e.txid, nullptr));
          llarp::Info("DHT reply to ", e.requester);
          router->SendTo(e.requester, &msg);
        }
      }

      ScheduleCleanupTimer();
    }

    void
    Context::Init(const Key_t &us, llarp_router *r)
    {
      router = r;
      ourKey = us;
      nodes  = new Bucket(ourKey);
      llarp::Debug("intialize dht with key ", ourKey);
    }

    void
    Context::ScheduleCleanupTimer()
    {
      llarp_logic_call_later(router->logic,
                             {1000, this, &handle_cleaner_timer});
    }

    void
    Context::LookupRouter(const Key_t &target, const Key_t &whoasked,
                          const Key_t &askpeer, llarp_router_lookup_job *job)
    {
      auto id                   = ++ids;
      pendingTX[{whoasked, id}] = SearchJob(whoasked, target, job);

      llarp::Info("Asking ", askpeer, " for router ", target, " for ",
                  whoasked);
      auto msg = new llarp::DHTImmeidateMessage(askpeer);
      msg->msgs.push_back(new FindRouterMessage(askpeer, target, id));
      router->SendToOrQueue(askpeer, {msg});
    }

    void
    Context::LookupRouterViaJob(llarp_router_lookup_job *job)
    {
      Key_t peer;
      if(nodes->FindCloseExcluding(job->target, peer, ourKey))
        LookupRouter(job->target, ourKey, peer, job);
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
  }
}

llarp_dht_context::llarp_dht_context(llarp_router *router)
{
  parent = router;
}

extern "C" {

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
llarp_dht_put_local_router(struct llarp_dht_context *ctx, struct llarp_rc *rc)

{
  llarp::dht::Key_t k = rc->pubkey;
  llarp::Debug("put router at ", k);
  ctx->impl.nodes->nodes[k] = rc;
}

void
llarp_dht_remove_local_router(struct llarp_dht_context *ctx, const byte_t *id)
{
  auto &nodes = ctx->impl.nodes->nodes;
  auto itr    = nodes.find(id);
  if(itr == nodes.end())
    return;
  nodes.erase(itr);
}

void
llarp_dht_set_msg_handler(struct llarp_dht_context *ctx,
                          llarp_dht_msg_handler handler)
{
  ctx->impl.custom_handler = handler;
}

void
llarp_dht_context_start(struct llarp_dht_context *ctx, const byte_t *key)
{
  ctx->impl.Init(key, ctx->parent);
}

void
llarp_dh_lookup_router(struct llarp_dht_context *ctx,
                       struct llarp_router_lookup_job *job)
{
  job->dht = ctx;
  llarp_logic_queue_job(ctx->parent->logic,
                        {job, &llarp::dht::Context::queue_router_lookup});
}
}

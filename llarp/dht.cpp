#include <llarp/bencode.h>
#include <llarp/dht.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include "router.hpp"

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
      if(!bdecode_read_integer(buf, &version))
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
      return false;
    }

    FindRouterMessage::~FindRouterMessage()
    {
    }

    bool
    FindRouterMessage::BEncode(llarp_buffer_t *buf) const
    {
      return false;
    }

    bool
    FindRouterMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
    {
      llarp_buffer_t strbuf;
      if(llarp_buffer_eq(key, "K"))
      {
        if(!bdecode_read_string(val, &strbuf))
          return false;
        if(strbuf.sz != K.size())
          return false;

        memcpy(K.data(), strbuf.base, K.size());
        return true;
      }
      if(llarp_buffer_eq(key, "T"))
      {
        return bdecode_read_integer(val, &txid);
      }
      if(llarp_buffer_eq(key, "V"))
      {
        if(!bdecode_read_integer(val, &version))
          return false;
        return version == LLARP_PROTO_VERSION;
      }
      return false;
    }

    bool
    FindRouterMessage::HandleMessage(llarp_router *router,
                                     std::vector< IMessage * > &replies) const
    {
      return false;
    }

    struct MessageDecoder
    {
      Key_t From;
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
          if(!bdecode_read_string(r->buffer, &strbuf))
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
      if(bdecode_read_dict(buf, &r))
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
      Key_t From;
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
      return bdecode_read_list(buf, &r);
    }

    Node::Node()
    {
      llarp_rc_clear(&rc);
    }

    Node::~Node()
    {
      llarp_rc_free(&rc);
    }

    Context::Context()
    {
    }

    Context::~Context()
    {
      if(nodes)
        delete nodes;
    }

    void
    Context::Init(const Key_t &us)
    {
      ourKey = us;
      nodes  = new Bucket(ourKey);
    }
  }
}

extern "C" {

struct llarp_dht_context *
llarp_dht_context_new()
{
  return new llarp_dht_context;
}

void
llarp_dht_context_free(struct llarp_dht_context *ctx)
{
  delete ctx;
}

void
llarp_dht_set_msg_handler(struct llarp_dht_context *ctx,
                          llarp_dht_msg_handler handler)
{
  ctx->impl.custom_handler = handler;
}

void
llarp_dht_context_set_our_key(struct llarp_dht_context *ctx, const byte_t *key)
{
  ctx->impl.Init(key);
}
}

#include <dht/messages/findrouter.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotrouter.hpp>
#include <nodedb.hpp>
#include <path/path_context.hpp>
#include <router/abstractrouter.hpp>
#include <routing/dht_message.hpp>

namespace llarp
{
  namespace dht
  {
    bool
    RelayedFindRouterMessage::HandleMessage(
        llarp_dht_context *ctx,
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      auto &dht = *ctx->impl;
      /// lookup for us, send an immeidate reply
      Key_t us = dht.OurKey();
      Key_t k{K};
      if(K == us)
      {
        auto path = dht.GetRouter()->pathContext().GetByUpstream(K, pathID);
        if(path)
        {
          replies.emplace_back(
              new GotRouterMessage(k, txid, {dht.GetRouter()->rc()}, false));
          return true;
        }
        return false;
      }

      Key_t peer;
      // check if we know this in our nodedb first
      RouterContact found;
      if(!dht.GetRouter()->ConnectionToRouterAllowed(K))
      {
        // explicitly disallowed by network
        replies.emplace_back(new GotRouterMessage(k, txid, {}, false));
        return true;
      }
      if(dht.GetRouter()->nodedb()->Get(K, found))
      {
        replies.emplace_back(new GotRouterMessage(k, txid, {found}, false));
        return true;
      }
      if((!dht.Nodes()->FindClosest(k, peer)) || peer == us)
      {
        // can't find any peers closer
        replies.emplace_back(new GotRouterMessage(k, txid, {}, false));
        return true;
      }
      // lookup if we don't have it in our nodedb
      dht.LookupRouterForPath(K, txid, pathID, peer);
      return true;
    }

    FindRouterMessage::~FindRouterMessage() = default;

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

      // exploritory or not?
      if(!bencode_write_bytestring(buf, "E", 1))
        return false;
      if(!bencode_write_uint64(buf, exploritory ? 1 : 0))
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
    FindRouterMessage::DecodeKey(const llarp_buffer_t &key, llarp_buffer_t *val)
    {
      llarp_buffer_t strbuf;

      if(key == "E")
      {
        uint64_t result;
        if(!bencode_read_integer(val, &result))
          return false;

        exploritory = result != 0;
        return true;
      }

      if(key == "I")
      {
        uint64_t result;
        if(!bencode_read_integer(val, &result))
          return false;

        iterative = result != 0;
        return true;
      }
      if(key == "K")
      {
        if(!bencode_read_string(val, &strbuf))
          return false;
        if(strbuf.sz != K.size())
          return false;

        std::copy(strbuf.base, strbuf.base + K.SIZE, K.begin());
        return true;
      }
      if(key == "T")
      {
        return bencode_read_integer(val, &txid);
      }
      if(key == "V")
      {
        return bencode_read_integer(val, &version);
      }
      return false;
    }

    bool
    FindRouterMessage::HandleMessage(
        llarp_dht_context *ctx,
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      auto &dht = *ctx->impl;
      if(!dht.AllowTransit())
      {
        llarp::LogWarn("Got DHT lookup from ", From,
                       " when we are not allowing dht transit");
        return false;
      }
      if(dht.pendingRouterLookups().HasPendingLookupFrom({From, txid}))
      {
        llarp::LogWarn("Duplicate FRM from ", From, " txid=", txid);
        return false;
      }
      RouterContact found;
      if(K.IsZero())
      {
        llarp::LogError("invalid FRM from ", From, "K is zero");
        return false;
      }
      const Key_t k(K);
      if(exploritory)
        return dht.HandleExploritoryRouterLookup(From, txid, K, replies);
      if(!dht.GetRouter()->ConnectionToRouterAllowed(K))
      {
        // explicitly disallowed by network
        replies.emplace_back(new GotRouterMessage(k, txid, {}, false));
        return true;
      }
      if(dht.GetRCFromNodeDB(k, found))
      {
        replies.emplace_back(new GotRouterMessage(k, txid, {found}, false));
        return true;
      }

      dht.LookupRouterRelayed(From, txid, k, !iterative, replies);
      return true;
    }
  }  // namespace dht
}  // namespace llarp

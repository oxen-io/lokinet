#include <dht/context.hpp>
#include <dht/messages/findintro.hpp>
#include <dht/messages/gotintro.hpp>
#include <routing/message.hpp>
#include <router/abstractrouter.hpp>
#include <nodedb.hpp>

namespace llarp
{
  namespace dht
  {
    /// 2 ** 12 which is 4096 nodes, after which this starts to fail "more"
    const uint64_t FindIntroMessage::MaxRecursionDepth = 12;
    FindIntroMessage::~FindIntroMessage()              = default;

    bool
    FindIntroMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val)
    {
      bool read = false;

      if(!BEncodeMaybeReadDictEntry("N", tagName, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictInt("R", recursionDepth, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictEntry("S", location, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictInt("T", txID, read, k, val))
        return false;

      if(!BEncodeMaybeVerifyVersion("V", version, LLARP_PROTO_VERSION, read, k,
                                    val))
        return false;

      return read;
    }

    bool
    FindIntroMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      // message id
      if(!BEncodeWriteDictMsgType(buf, "A", "F"))
        return false;
      if(tagName.Empty())
      {
        // recursion
        if(!BEncodeWriteDictInt("R", recursionDepth, buf))
          return false;
        // service address
        if(!BEncodeWriteDictEntry("S", location, buf))
          return false;
      }
      else
      {
        if(!BEncodeWriteDictEntry("N", tagName, buf))
          return false;
        // recursion
        if(!BEncodeWriteDictInt("R", recursionDepth, buf))
          return false;
      }
      // txid
      if(!BEncodeWriteDictInt("T", txID, buf))
        return false;
      // protocol version
      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    FindIntroMessage::HandleMessage(
        llarp_dht_context* ctx, std::vector< IMessage::Ptr_t >& replies) const
    {
      if(recursionDepth > MaxRecursionDepth)
      {
        llarp::LogError("recursion depth big, ", recursionDepth, "> ",
                        MaxRecursionDepth);
        return false;
      }
      auto& dht = *ctx->impl;
      if(dht.pendingIntrosetLookups().HasPendingLookupFrom(TXOwner{From, txID}))
      {
        llarp::LogWarn("duplicate FIM from ", From, " txid=", txID);
        return false;
      }

      std::set< Key_t > exclude = {dht.OurKey(), From};
      if(not tagName.Empty())
        return false;

      if(location.IsZero())
      {
        // we dont got it
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }
      const auto maybe = dht.GetIntroSetByLocation(location);
      if(maybe.has_value())
      {
        replies.emplace_back(new GotIntroMessage({maybe.value()}, txID));
        return true;
      }

      const Key_t us = dht.OurKey();

      if(recursionDepth == 0)
      {
        // we don't have it
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      // we are recursive
      const auto rc = dht.GetRouter()->nodedb()->FindClosestTo(location);

      Key_t peer = Key_t(rc.pubkey);

      if((us ^ location) < (peer ^ location) || peer == us)
      {
        // ask second closest as we are relayed
        if(not dht.Nodes()->FindCloseExcluding(location, peer, exclude))
        {
          // no second closeset
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }
      }
      if(relayed)
      {
        dht.LookupIntroSetForPath(location, txID, pathID, peer,
                                  recursionDepth - 1);
      }
      else
      {
        dht.LookupIntroSetRecursive(location, From, txID, peer,
                                    recursionDepth - 1);
      }
      return true;
    }
  }  // namespace dht
}  // namespace llarp

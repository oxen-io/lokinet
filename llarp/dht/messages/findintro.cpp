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
    FindIntroMessage::~FindIntroMessage() = default;

    bool
    FindIntroMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val)
    {
      bool read = false;

      if(!BEncodeMaybeReadDictEntry("N", tagName, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictInt("O", relayOrder, read, k, val))
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
        // relay order
        if(!BEncodeWriteDictInt("O", relayOrder, buf))
          return false;

        // service address
        if(!BEncodeWriteDictEntry("S", location, buf))
          return false;
      }
      else
      {
        if(!BEncodeWriteDictEntry("N", tagName, buf))
          return false;

        // relay order
        if(!BEncodeWriteDictInt("O", relayOrder, buf))
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
      auto& dht = *ctx->impl;
      if(dht.pendingIntrosetLookups().HasPendingLookupFrom(TXOwner{From, txID}))
      {
        llarp::LogWarn("duplicate FIM from ", From, " txid=", txID);
        return false;
      }

      if(not tagName.Empty())
        return false;

      // bad request (request for zero-key)
      if(location.IsZero())
      {
        // we dont got it
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      // we are relaying this message for e.g. a client
      if(relayed)
      {
        uint32_t numDesired = 0;
        if(relayOrder == 0)
          numDesired = 2;
        else if(relayOrder == 1)
          numDesired = 4;
        else
        {
          // TODO: consider forward-compatibility here
          LogError("Error: relayOrder must be 0 or 1");
          return false;
        }

        auto closestRCs =
            dht.GetRouter()->nodedb()->FindClosestTo(location, numDesired);

        // if relayOrder == 1, we want the 3rd and 4th closest, so remove the
        // 1st and 2nd closest
        if(relayOrder == 1)
        {
          auto itr = closestRCs.begin();
          std::advance(itr, 2);
          closestRCs.erase(closestRCs.begin(), itr);
        }

        for(const auto& entry : closestRCs)
        {
          Key_t peer = Key_t(entry.pubkey);
          dht.LookupIntroSetForPath(location, txID, pathID, peer, 0);
        }
      }
      else
      {
        // we should have this value if introset was propagated properly
        const auto maybe = dht.GetIntroSetByLocation(location);
        if(maybe.has_value())
        {
          replies.emplace_back(new GotIntroMessage({maybe.value()}, txID));
        }
        else
        {
          LogWarn("Got FIM with relayed == false and we don't have entry");
          replies.emplace_back(new GotIntroMessage({}, txID));
        }
      }
      return true;
    }
  }  // namespace dht
}  // namespace llarp

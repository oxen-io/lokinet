#include <llarp/dht/context.hpp>
#include "findintro.hpp"
#include "gotintro.hpp"
#include <llarp/routing/message.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/nodedb.hpp>

namespace llarp
{
  namespace dht
  {
    FindIntroMessage::~FindIntroMessage() = default;

    bool
    FindIntroMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val)
    {
      bool read = false;

      if (!BEncodeMaybeReadDictEntry("N", tagName, read, k, val))
        return false;

      if (!BEncodeMaybeReadDictInt("O", relayOrder, read, k, val))
        return false;

      if (!BEncodeMaybeReadDictEntry("S", location, read, k, val))
        return false;

      if (!BEncodeMaybeReadDictInt("T", txID, read, k, val))
        return false;

      if (!BEncodeMaybeVerifyVersion("V", version, LLARP_PROTO_VERSION, read, k, val))
        return false;

      return read;
    }

    bool
    FindIntroMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;

      // message id
      if (!BEncodeWriteDictMsgType(buf, "A", "F"))
        return false;
      if (tagName.Empty())
      {
        // relay order
        if (!BEncodeWriteDictInt("O", relayOrder, buf))
          return false;

        // service address
        if (!BEncodeWriteDictEntry("S", location, buf))
          return false;
      }
      else
      {
        if (!BEncodeWriteDictEntry("N", tagName, buf))
          return false;

        // relay order
        if (!BEncodeWriteDictInt("O", relayOrder, buf))
          return false;
      }
      // txid
      if (!BEncodeWriteDictInt("T", txID, buf))
        return false;
      // protocol version
      if (!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    FindIntroMessage::HandleMessage(
        llarp_dht_context* ctx, std::vector<IMessage::Ptr_t>& replies) const
    {
      auto& dht = *ctx->impl;
      if (dht.pendingIntrosetLookups().HasPendingLookupFrom(TXOwner{From, txID}))
      {
        llarp::LogWarn("duplicate FIM from ", From, " txid=", txID);
        return false;
      }

      if (not tagName.Empty())
      {
        return false;
      }
      // bad request (request for zero-key)
      if (location.IsZero())
      {
        // we dont got it
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      // we are relaying this message for e.g. a client
      if (relayed)
      {
        if (relayOrder >= IntroSetStorageRedundancy)
        {
          llarp::LogWarn("Invalid relayOrder received: ", relayOrder);
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }

        auto closestRCs =
            dht.GetRouter()->nodedb()->FindManyClosestTo(location, IntroSetStorageRedundancy);

        if (closestRCs.size() <= relayOrder)
        {
          llarp::LogWarn("Can't fulfill FindIntro for relayOrder: ", relayOrder);
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }

        const auto& entry = closestRCs[relayOrder];
        Key_t peer = Key_t(entry.pubkey);
        dht.LookupIntroSetForPath(location, txID, pathID, peer, 0);
      }
      else
      {
        // we should have this value if introset was propagated properly
        const auto maybe = dht.GetIntroSetByLocation(location);
        if (maybe)
        {
          replies.emplace_back(new GotIntroMessage({*maybe}, txID));
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

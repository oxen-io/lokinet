#include <dht/messages/pubintro.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotintro.hpp>
#include <messages/dht_immediate.hpp>
#include <router/abstractrouter.hpp>
#include <routing/dht_message.hpp>
#include <nodedb.hpp>

namespace llarp
{
  namespace dht
  {
    const uint64_t PublishIntroMessage::MaxPropagationDepth = 5;
    PublishIntroMessage::~PublishIntroMessage()             = default;

    bool
    PublishIntroMessage::DecodeKey(const llarp_buffer_t &key,
                                   llarp_buffer_t *val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("I", introset, read, key, val))
        return false;
      if(read)
        return true;

      if(!BEncodeMaybeReadDictInt("O", relayOrder, read, key, val))
        return false;
      if(read)
        return true;

      uint64_t relayedInt = (relayed ? 1 : 0);
      if(!BEncodeMaybeReadDictInt("R", relayedInt, read, key, val))
        return false;
      if(read)
      {
        relayed = relayedInt;
        return true;
      }

      if(!BEncodeMaybeReadDictInt("T", txID, read, key, val))
        return false;
      if(read)
        return true;

      if(!BEncodeMaybeReadDictInt("V", version, read, key, val))
        return false;
      if(read)
        return true;

      return false;
    }

    bool
    PublishIntroMessage::HandleMessage(
        llarp_dht_context *ctx,
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      auto now = ctx->impl->Now();
      const auto keyStr = introset.derivedSigningKey.ToHex();

      auto &dht = *ctx->impl;
      if(!introset.Verify(now))
      {
        llarp::LogWarn("Received PublishIntroMessage with invalid introset: ",
                       introset);
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      if(introset.IsExpired(now + llarp::service::MAX_INTROSET_TIME_DELTA))
      {
        // don't propogate or store
        llarp::LogWarn("Received PublishIntroMessage with expired Introset: ",
                       introset);
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      const llarp::dht::Key_t addr(introset.derivedSigningKey);

      // identify closest 4 routers
      static constexpr size_t StorageRedundancy = 4;
      auto closestRCs = dht.GetRouter()->nodedb()->FindClosestTo(addr, StorageRedundancy);
      if(closestRCs.size() != StorageRedundancy)
      {
        llarp::LogWarn("Received PublishIntroMessage but only know ",
                       closestRCs.size(), " nodes");
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      const auto &us = dht.OurKey();

      // function to identify the closest 4 routers we know of for this introset
      auto propagateIfNotUs = [&](size_t index) {
        assert(index < StorageRedundancy);

        const auto &rc = closestRCs[index];
        const Key_t peer{rc.pubkey};

        if(peer == us)
        {
          llarp::LogInfo("we are peer ", index,
                         " so storing instead of propagating");

          dht.services()->PutNode(introset);
          replies.emplace_back(new GotIntroMessage({introset}, txID));
        }
        else
        {
          llarp::LogInfo("propagating to peer ", index);

          dht.PropagateIntroSetTo(From, txID, introset, peer, false, 0);
        }
      };

      if(relayed)
      {
        if(relayOrder >= StorageRedundancy)
        {
          llarp::LogWarn(
              "Received PublishIntroMessage with invalid relayOrder: ",
              relayOrder);
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }

        llarp::LogInfo("Relaying PublishIntroMessage for ", keyStr,
                       ", txid=", txID);

        propagateIfNotUs(relayOrder);
      }
      else
      {
        bool found = false;
        for(const auto &rc : closestRCs)
        {
          if(rc.pubkey == dht.OurKey())
          {
            found = true;
            break;
          }
        }

        if(found)
        {
          dht.services()->PutNode(introset);
          replies.emplace_back(new GotIntroMessage({introset}, txID));
        }
        else
        {
          LogWarn(
              "!!! Received PubIntro with relayed==false but we aren't"
              " candidate, intro derived key: ",
              keyStr, ", txid=", txID, ", message from: ", From);
          for(size_t i = 0; i < StorageRedundancy; ++i)
          {
            propagateIfNotUs(i);
          }
        }
      }

      return true;
    }

    bool
    PublishIntroMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if(!BEncodeWriteDictEntry("I", introset, buf))
        return false;
      if(!BEncodeWriteDictInt("O", relayOrder, buf))
        return false;
      if(!BEncodeWriteDictInt("R", relayed, buf))
        return false;
      if(!BEncodeWriteDictInt("T", txID, buf))
        return false;
      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;
      return bencode_end(buf);
    }
  }  // namespace dht
}  // namespace llarp

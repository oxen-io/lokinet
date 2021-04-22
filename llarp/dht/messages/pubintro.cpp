#include "pubintro.hpp"

#include <llarp/dht/context.hpp>
#include "gotintro.hpp"
#include <llarp/messages/dht_immediate.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/dht_message.hpp>
#include <llarp/nodedb.hpp>

#include <llarp/tooling/dht_event.hpp>

namespace llarp
{
  namespace dht
  {
    const uint64_t PublishIntroMessage::MaxPropagationDepth = 5;
    PublishIntroMessage::~PublishIntroMessage() = default;

    bool
    PublishIntroMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictEntry("I", introset, read, key, val))
        return false;
      if (read)
        return true;

      if (!BEncodeMaybeReadDictInt("O", relayOrder, read, key, val))
        return false;
      if (read)
        return true;

      uint64_t relayedInt = (relayed ? 1 : 0);
      if (!BEncodeMaybeReadDictInt("R", relayedInt, read, key, val))
        return false;
      if (read)
      {
        relayed = relayedInt;
        return true;
      }

      if (!BEncodeMaybeReadDictInt("T", txID, read, key, val))
        return false;
      if (read)
        return true;

      if (!BEncodeMaybeReadDictInt("V", version, read, key, val))
        return false;
      if (read)
        return true;

      return false;
    }

    bool
    PublishIntroMessage::HandleMessage(
        llarp_dht_context* ctx, std::vector<std::unique_ptr<IMessage>>& replies) const
    {
      const auto now = ctx->impl->Now();
      const llarp::dht::Key_t addr(introset.derivedSigningKey);
      const auto keyStr = addr.ToHex();

      auto router = ctx->impl->GetRouter();
      router->NotifyRouterEvent<tooling::PubIntroReceivedEvent>(
          router->pubkey(),
          Key_t(relayed ? router->pubkey() : From.data()),
          addr,
          txID,
          relayOrder);

      auto& dht = *ctx->impl;
      if (!introset.Verify(now))
      {
        llarp::LogWarn("Received PublishIntroMessage with invalid introset: ", introset);
        // don't propogate or store
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      if (introset.IsExpired(now + llarp::service::MAX_INTROSET_TIME_DELTA))
      {
        // don't propogate or store
        llarp::LogWarn("Received PublishIntroMessage with expired Introset: ", introset);
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      // identify closest 4 routers
      auto closestRCs =
          dht.GetRouter()->nodedb()->FindManyClosestTo(addr, IntroSetStorageRedundancy);
      if (closestRCs.size() != IntroSetStorageRedundancy)
      {
        llarp::LogWarn("Received PublishIntroMessage but only know ", closestRCs.size(), " nodes");
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      const auto& us = dht.OurKey();

      // function to identify the closest 4 routers we know of for this introset
      auto propagateIfNotUs = [&](size_t index) {
        assert(index < IntroSetStorageRedundancy);

        const auto& rc = closestRCs[index];
        const Key_t peer{rc.pubkey};

        if (peer == us)
        {
          llarp::LogInfo("we are peer ", index, " so storing instead of propagating");

          dht.services()->PutNode(introset);
          replies.emplace_back(new GotIntroMessage({introset}, txID));
        }
        else
        {
          llarp::LogInfo("propagating to peer ", index);
          if (relayed)
          {
            dht.PropagateLocalIntroSet(pathID, txID, introset, peer, 0);
          }
          else
          {
            dht.PropagateIntroSetTo(From, txID, introset, peer, 0);
          }
        }
      };

      if (relayed)
      {
        if (relayOrder >= IntroSetStorageRedundancy)
        {
          llarp::LogWarn("Received PublishIntroMessage with invalid relayOrder: ", relayOrder);
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }

        llarp::LogInfo("Relaying PublishIntroMessage for ", keyStr, ", txid=", txID);

        propagateIfNotUs(relayOrder);
      }
      else
      {
        int candidateNumber = -1;
        int index = 0;
        for (const auto& rc : closestRCs)
        {
          if (rc.pubkey == dht.OurKey())
          {
            candidateNumber = index;
            break;
          }
          ++index;
        }

        if (candidateNumber >= 0)
        {
          LogInfo(
              "Received PubIntro for ",
              keyStr,
              ", txid=",
              txID,
              " and we are candidate ",
              candidateNumber);
          dht.services()->PutNode(introset);
          replies.emplace_back(new GotIntroMessage({introset}, txID));
        }
        else
        {
          LogWarn(
              "!!! Received PubIntro with relayed==false but we aren't"
              " candidate, intro derived key: ",
              keyStr,
              ", txid=",
              txID,
              ", message from: ",
              From);
        }
      }

      return true;
    }

    bool
    PublishIntroMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if (!BEncodeWriteDictEntry("I", introset, buf))
        return false;
      if (!BEncodeWriteDictInt("O", relayOrder, buf))
        return false;
      if (!BEncodeWriteDictInt("R", relayed, buf))
        return false;
      if (!BEncodeWriteDictInt("T", txID, buf))
        return false;
      if (!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;
      return bencode_end(buf);
    }
  }  // namespace dht
}  // namespace llarp

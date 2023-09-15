#include "gotintro.hpp"

#include <llarp/service/intro.hpp>
#include <llarp/dht/context.hpp>
#include <memory>
#include <llarp/path/path_context.hpp>
#include <llarp/router/router.hpp>
#include <llarp/routing/path_dht_message.hpp>
#include <llarp/tooling/dht_event.hpp>
#include <utility>

namespace llarp::dht
{
  GotIntroMessage::GotIntroMessage(std::vector<service::EncryptedIntroSet> results, uint64_t tx)
      : AbstractDHTMessage({}), found(std::move(results)), txid(tx)
  {}

  bool
  GotIntroMessage::handle_message(
      AbstractDHTMessageHandler& dht,
      std::vector<std::unique_ptr<AbstractDHTMessage>>& /*replies*/) const
  {
    auto* router = dht.GetRouter();

    router->notify_router_event<tooling::GotIntroReceivedEvent>(
        router->pubkey(),
        Key_t(From.data()),
        (found.size() > 0 ? found[0] : llarp::service::EncryptedIntroSet{}),
        txid);

    for (const auto& introset : found)
    {
      if (!introset.Verify(dht.Now()))
      {
        LogWarn(
            "Invalid introset while handling direct GotIntro "
            "from ",
            From);
        return false;
      }
    }
    TXOwner owner(From, txid);

    auto serviceLookup = dht.pendingIntrosetLookups().GetPendingLookupFrom(owner);
    if (serviceLookup)
    {
      if (not found.empty())
      {
        dht.pendingIntrosetLookups().Found(owner, serviceLookup->target, found);
      }
      else
      {
        dht.pendingIntrosetLookups().NotFound(owner, nullptr);
      }
      return true;
    }
    LogError("no pending TX for GIM from ", From, " txid=", txid);
    return false;
  }

  bool
  RelayedGotIntroMessage::handle_message(
      AbstractDHTMessageHandler& dht,
      [[maybe_unused]] std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const
  {
    // TODO: implement me better?
    auto pathset = dht.GetRouter()->path_context().GetLocalPathSet(pathID);
    if (pathset)
    {
      auto copy = std::make_shared<const RelayedGotIntroMessage>(*this);
      return pathset->HandleGotIntroMessage(copy);
    }
    LogWarn("No path for got intro message pathid=", pathID);
    return false;
  }

  bool
  GotIntroMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key.startswith("I"))
    {
      return BEncodeReadList(found, buf);
    }
    if (key.startswith("K"))
    {
      if (closer)  // duplicate key?
        return false;
      dht::Key_t K;
      if (not K.BDecode(buf))
        return false;
      closer = K;
      return true;
    }
    bool read = false;
    if (!BEncodeMaybeReadDictInt("T", txid, read, key, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, key, buf))
      return false;
    return read;
  }

  void
  GotIntroMessage::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("A", "G");

      {
        auto sublist = btdp.append_list("I");
        for (auto f : found)
          sublist.append(f.ToString());
      }

      if (closer)
        btdp.append("K", closer->ToView());

      btdp.append("T", txid);
      btdp.append("V", version);
    }
    catch (...)
    {
      log::error(dht_cat, "Error: GotIntroMessage failed to bt encode contents!");
    }
  }
}  // namespace llarp::dht
